#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <net/ethernet.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <pthread.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <stddef.h>
#include <syslog.h>
#include <numa.h>
#include <ixmap.h>

#include "main.h"
#include "thread.h"
#include "forward.h"
#include "epoll.h"
#include "netlink.h"

static int thread_wait(struct ixmapfwd_thread *thread,
	int fd_ep, uint8_t *read_buf, int read_size);
static int thread_fd_prepare(struct list_head *ep_desc_head,
	struct ixmapfwd_thread *thread);
static void thread_fd_destroy(struct list_head *ep_desc_head,
	int fd_ep);
static void thread_print_result(struct ixmapfwd_thread *thread);

void *thread_process_interrupt(void *data)
{
	struct ixmapfwd_thread	*thread = data;
	struct list_head	ep_desc_head;
	uint8_t			*read_buf;
	int			read_size, fd_ep, i, ret;
	int			ports_assigned = 0;

	ixmapfwd_log(LOG_INFO, "thread %d started", thread->index);
	read_size = getpagesize();
	INIT_LIST_HEAD(&ep_desc_head);

	/* Prepare fib */
	thread->fib_inet = fib_alloc(thread->desc);
	if(!thread->fib_inet)
		goto err_fib_inet_alloc;

	thread->fib_inet6 = fib_alloc(thread->desc);
	if(!thread->fib_inet6)
		goto err_fib_inet6_alloc;

	/* Prepare Neighbor table */
	thread->neigh_inet = ixmap_mem_alloc(thread->desc,
		sizeof(struct neigh *) * thread->num_ports);
	if(!thread->neigh_inet)
		goto err_neigh_table_inet;
	
	thread->neigh_inet6 = ixmap_mem_alloc(thread->desc,
		sizeof(struct neigh *) * thread->num_ports);
	if(!thread->neigh_inet6)
		goto err_neigh_table_inet6;

	for(i = 0; i < thread->num_ports; i++, ports_assigned++){
		thread->neigh_inet[i] = neigh_alloc(thread->desc, AF_INET);
		if(!thread->neigh_inet[i])
			goto err_neigh_inet_alloc;

		thread->neigh_inet6[i] = neigh_alloc(thread->desc, AF_INET6);
		if(!thread->neigh_inet6[i])
			goto err_neigh_inet6_alloc;

		/* calclulate maximum buf_size we should prepare */
		if(thread->tun_plane->ports[i].mtu_frame > read_size)
			read_size = thread->tun_plane->ports[i].mtu_frame;

		continue;

err_neigh_inet6_alloc:
		neigh_release(thread->neigh_inet[i]);
err_neigh_inet_alloc:
		goto err_assign_ports;
	}

	/* Prepare read buffer */
	read_buf = numa_alloc_onnode(read_size,
		numa_node_of_cpu(thread->index));
	if(!read_buf)
		goto err_alloc_read_buf;

	/* Prepare each fd in epoll */
	fd_ep = thread_fd_prepare(&ep_desc_head, thread);
	if(fd_ep < 0){
		ixmapfwd_log(LOG_ERR, "failed to epoll prepare");
		goto err_ixgbe_epoll_prepare;
	}

	/* Prepare initial RX buffer */
	for(i = 0; i < thread->num_ports; i++){
		ixmap_rx_assign(thread->plane, i, thread->buf);
	}

	ret = thread_wait(thread, fd_ep, read_buf, read_size);
	if(ret < 0)
		goto err_wait;

err_wait:
	thread_fd_destroy(&ep_desc_head, fd_ep);
err_ixgbe_epoll_prepare:
	numa_free(read_buf, read_size);
err_alloc_read_buf:
err_assign_ports:
	for(i = 0; i < ports_assigned; i++){
		neigh_release(thread->neigh_inet6[i]);
		neigh_release(thread->neigh_inet[i]);
	}
	ixmap_mem_free(thread->neigh_inet6);
err_neigh_table_inet6:
	ixmap_mem_free(thread->neigh_inet);
err_neigh_table_inet:
	fib_release(thread->fib_inet6);
err_fib_inet6_alloc:
	fib_release(thread->fib_inet);
err_fib_inet_alloc:
	thread_print_result(thread);
	pthread_kill(thread->ptid, SIGINT);
	return NULL;
}

static int thread_wait(struct ixmapfwd_thread *thread,
	int fd_ep, uint8_t *read_buf, int read_size)
{
        struct epoll_desc *ep_desc;
        struct epoll_event events[EPOLL_MAXEVENTS];
	struct ixmap_packet packet[IXMAP_RX_BUDGET];
        int i, ret, num_fd;
        unsigned int port_index;

	while(1){
		num_fd = epoll_wait(fd_ep, events, EPOLL_MAXEVENTS, -1);
		if(num_fd < 0){
			goto err_read;
		}

		for(i = 0; i < num_fd; i++){
			ep_desc = (struct epoll_desc *)events[i].data.ptr;
			
			switch(ep_desc->type){
			case EPOLL_IRQ_RX:
				port_index = ep_desc->port_index;

				/* Rx descripter cleaning */
				ret = ixmap_rx_clean(thread->plane, port_index,
					thread->buf, packet);
				ixmap_rx_assign(thread->plane, port_index, thread->buf);

				forward_process(thread, port_index, packet, ret);

				for(i = 0; i < thread->num_ports; i++){
					ixmap_tx_xmit(thread->plane, i);
				}

				ret = read(ep_desc->fd, read_buf, read_size);
				if(ret < 0)
					goto err_read;

				ixmap_irq_unmask_queues(thread->plane, port_index,
					(struct ixmap_irq_handle *)ep_desc->data);
				break;
			case EPOLL_IRQ_TX:
				port_index = ep_desc->port_index;

				/* Tx descripter cleaning */
				ixmap_tx_clean(thread->plane, port_index, thread->buf);

				ret = read(ep_desc->fd, read_buf, read_size);
				if(ret < 0)
					goto err_read;

				ixmap_irq_unmask_queues(thread->plane, port_index,
					(struct ixmap_irq_handle *)ep_desc->data);
				break;
			case EPOLL_TUN:
				port_index = ep_desc->port_index;

				ret = read(ep_desc->fd, read_buf, read_size);
				if(ret < 0)
					goto err_read;

				forward_process_tun(thread, port_index, read_buf, ret);
				for(i = 0; i < thread->num_ports; i++){
					ixmap_tx_xmit(thread->plane, i);
				}
				break;
			case EPOLL_NETLINK:
				ret = read(ep_desc->fd, read_buf, read_size);
				if(ret < 0)
					goto err_read;

				netlink_process(thread, read_buf, ret);
                                break;
			case EPOLL_SIGNAL:
				ret = read(ep_desc->fd, read_buf, read_size);
				if(ret < 0)
					goto err_read;

				goto out;
				break;
			default:
				break;
			}
		}
	}

out:
	return 0;

err_read:
	return -1;
}

static int thread_fd_prepare(struct list_head *ep_desc_head,
	struct ixmapfwd_thread *thread)
{
	struct epoll_desc 	*ep_desc;
	sigset_t		sigset;
	struct sockaddr_nl	addr;
	int			fd_ep, i, ret;

	/* epoll fd preparing */
	fd_ep = epoll_create(EPOLL_MAXEVENTS);
	if(fd_ep < 0){
		perror("failed to make epoll fd");
		goto err_epoll_open;
	}

	for(i = 0; i < thread->num_ports; i++){
		/* Register RX interrupt fd */
		ep_desc = epoll_desc_alloc_irq(thread->plane, i,
			thread->index, IXMAP_IRQ_RX);
		if(!ep_desc)
			goto err_assign_port;

		list_add(&ep_desc->list, ep_desc_head);

		ret = epoll_add(fd_ep, ep_desc, ep_desc->fd);
		if(ret < 0){
			perror("failed to add fd in epoll");
			goto err_assign_port;
		}

		/* Register TX interrupt fd */
		ep_desc = epoll_desc_alloc_irq(thread->plane, i,
			thread->index, IXMAP_IRQ_TX);
		if(!ep_desc)
			goto err_assign_port;

		list_add(&ep_desc->list, ep_desc_head);

		ret = epoll_add(fd_ep, ep_desc, ep_desc->fd);
		if(ret < 0){
			perror("failed to add fd in epoll");
			goto err_assign_port;
		}

		/* Register Virtual Interface fd */
		ep_desc = epoll_desc_alloc_tun(thread->tun_plane, i,
			thread->index);
		if(!ep_desc)
			goto err_assign_port;

		list_add(&ep_desc->list, ep_desc_head);

		ret = epoll_add(fd_ep, ep_desc, ep_desc->fd);
		if(ret < 0){
			perror("failed to add fd in epoll");
			goto err_assign_port;
		}
	}

	/* signalfd preparing */
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGUSR1);
	ep_desc = epoll_desc_alloc_signalfd(&sigset, thread->index);
	if(!ep_desc)
		goto err_epoll_desc_signalfd;

	list_add(&ep_desc->list, ep_desc_head);

	ret = epoll_add(fd_ep, ep_desc, ep_desc->fd);
	if(ret < 0){
		perror("failed to add fd in epoll");
		goto err_epoll_add_signalfd;
	}

	/* netlink preparing */
	memset(&addr, 0, sizeof(struct sockaddr_nl));
	addr.nl_family = AF_NETLINK;
	addr.nl_groups = RTMGRP_NEIGH | RTMGRP_IPV4_ROUTE | RTMGRP_IPV6_ROUTE;

	ep_desc = epoll_desc_alloc_netlink(&addr, thread->index);
	if(!ep_desc)
		goto err_epoll_desc_netlink;

	list_add(&ep_desc->list, ep_desc_head);

	ret = epoll_add(fd_ep, ep_desc, ep_desc->fd);
	if(ret < 0){
		perror("failed to add fd in epoll");
		goto err_epoll_add_netlink;
	}

	return fd_ep;

err_epoll_add_netlink:
err_epoll_desc_netlink:
err_epoll_add_signalfd:
err_epoll_desc_signalfd:
err_assign_port:
	thread_fd_destroy(ep_desc_head, fd_ep);
err_epoll_open:
	return -1;
}

static void thread_fd_destroy(struct list_head *ep_desc_head,
	int fd_ep)
{
	struct epoll_desc *ep_desc, *ep_next;

	list_for_each_entry_safe(ep_desc, ep_next, ep_desc_head, list){
		list_del(&ep_desc->list);
		epoll_del(fd_ep, ep_desc->fd);

		switch(ep_desc->type){
		case EPOLL_IRQ_RX:
		case EPOLL_IRQ_TX:
			epoll_desc_release_irq(ep_desc);
			break;
		case EPOLL_SIGNAL:
			epoll_desc_release_signalfd(ep_desc);
			break;
		case EPOLL_TUN:
			epoll_desc_release_tun(ep_desc);
			break;
		default:
			break;
		}
	}

	close(fd_ep);
	return;
}

static void thread_print_result(struct ixmapfwd_thread *thread)
{
	int i;

	for(i = 0; i < thread->num_ports; i++){
		ixmapfwd_log(LOG_INFO, "thread %d port %d statictis:", thread->index, i);
		ixmapfwd_log(LOG_INFO, "  Rx allocation failed = %lu",
			ixmap_count_rx_alloc_failed(thread->plane, i));
		ixmapfwd_log(LOG_INFO, "  Rx packetes received = %lu",
			ixmap_count_rx_clean_total(thread->plane, i));
		ixmapfwd_log(LOG_INFO, "  Tx xmit failed = %lu",
			ixmap_count_tx_xmit_failed(thread->plane, i));
		ixmapfwd_log(LOG_INFO, "  Tx packetes transmitted = %lu",
			ixmap_count_tx_clean_total(thread->plane, i));
	}
	return;
}
