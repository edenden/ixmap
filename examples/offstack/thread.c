#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <net/ethernet.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <pthread.h>
#include <urcu.h>
#include <ixmap.h>

#include "main.h"
#include "thread.h"
#include "forward.h"
#include "epoll.h"

static int thread_wait(struct ixmapfwd_thread *thread,
	int fd_ep, uint8_t *read_buf, int read_size,
	struct ixmap_bulk **bulk_array);
static int thread_fd_prepare(struct list_head *ep_desc_head,
	struct ixmapfwd_thread *thread);
static void thread_fd_destroy(struct list_head *ep_desc_head,
	int fd_ep);
static int thread_irq_setmask(struct ixmap_irqdev_handle *irqh, int core_id);
static void thread_print_result(struct ixmapfwd_thread *thread);

void *thread_process_interrupt(void *data)
{
	struct ixmapfwd_thread	*thread = data;
	struct ixmap_bulk	**bulk_array;
	struct list_head	ep_desc_head;
	uint8_t			*read_buf;
	int			read_size, fd_ep, i, ret,
				bulk_assigned = 0;

	ixgbe_print("thread %d started\n", thread->index);
	INIT_LIST_HEAD(&ep_desc_head);

	/* Prepare read buffer */
	read_size = max(sizeof(uint32_t), sizeof(struct signalfd_siginfo));
	for(i = 0; i < thread->num_ports; i++){
		/* calclulate maximum buf_size we should prepare */
		if(thread->tun_plane->ports[i].mtu_frame > read_size)
			read_size = thread->tun_plane->ports[i].mtu_frame;
        }
	read_buf = malloc(read_size);
	if(!read_buf)
		goto err_alloc_read_buf;

	bulk_array = malloc(sizeof(struct ixmap_bulk *) * (thread->num_ports + 1));
	if(!bulk_array)
		goto err_bulk_array_alloc;

	for(i = 0; i < thread->num_ports + 1; i++, bulk_assigned++){
		bulk_array[i] = ixmap_bulk_alloc(thread->plane,
			thread->num_ports);
		if(!bulk_array[i])
			goto err_bulk_alloc;
	}

	/* Prepare each fd in epoll */
	fd_ep = thread_fd_prepare(&ep_desc_head, thread);
	if(fd_ep < 0){
		printf("failed to epoll prepare\n");
		goto err_ixgbe_epoll_prepare;
	}

	/* Prepare initial RX buffer */
	for(i = 0; i < thread->num_ports; i++){
		ixmap_rx_alloc(thread->plane, i, thread->buf);
	}

	rcu_register_thread();

	ret = thread_wait(thread, fd_ep, read_buf, read_size, bulk_array);
	if(ret < 0)
		goto err_wait;

	rcu_unregister_thread();
	thread_fd_destroy(&ep_desc_head, fd_ep);
	for(i = 0; i < bulk_assigned; i++){
		ixmap_bulk_release(bulk_array[i]);
	}
	free(bulk_array);
	free(read_buf);
	thread_print_result(thread);
	return NULL;

err_wait:
	rcu_unregister_thread();
	thread_fd_destroy(&ep_desc_head, fd_ep);
err_ixgbe_epoll_prepare:
err_bulk_alloc:
	for(i = 0; i < bulk_assigned; i++){
		ixmap_bulk_release(bulk_array[i]);
	}
	free(bulk_array);
err_bulk_array_alloc:
	free(read_buf);
err_alloc_read_buf:
	printf("thread execution failed\n");
	pthread_kill(thread->ptid, SIGINT);
	return NULL;
}

static int thread_wait(struct ixmapfwd_thread *thread,
	int fd_ep, uint8_t *read_buf, int read_size,
	struct ixmap_bulk **bulk_array)
{
        struct epoll_desc *ep_desc;
        struct ixmap_irqdev_handle *irqh;
        struct epoll_event events[EPOLL_MAXEVENTS];
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
				irqh = (struct ixmap_irqdev_handle *)ep_desc->data;
				port_index = ixmap_port_index(irqh);

				/* Rx descripter cleaning */
				ret = ixmap_rx_clean(thread->plane, port_index,
					thread->buf, bulk_array[thread->num_ports]);
				ixmap_rx_alloc(thread->plane, port_index, thread->buf);

				forward_process(thread, port_index, bulk_array);
				for(i = 0; i < thread->num_ports; i++){
					ixmap_tx_xmit(thread->plane, i, thread->buf,
						bulk_array[i]);
				}

				if(ret < ixmap_budget(thread->plane, port_index)){
					ret = read(ep_desc->fd, read_buf, read_size);
					if(ret < 0)
						goto err_read;

					ixmap_irq_unmask_queues(thread->plane, irqh);
				}
				break;
			case EPOLL_IRQ_TX:
				irqh = (struct ixmap_irqdev_handle *)ep_desc->data;
				port_index = ixmap_port_index(irqh);

				/* Tx descripter cleaning */
				ret = ixmap_tx_clean(thread->plane, port_index, thread->buf);

				if(ret < ixmap_budget(thread->plane, port_index)){
					ret = read(ep_desc->fd, read_buf, read_size);
					if(ret < 0)
						goto err_read;

					ixmap_irq_unmask_queues(thread->plane, irqh);
				}
				break;
			case EPOLL_TUN:
				port_index = *(unsigned int *)ep_desc->data;

				ret = read(ep_desc->fd, read_buf, read_size);
				if(ret < 0)
					goto err_read;

				forward_process_tun(thread, port_index, bulk_array,
					read_buf, ret);
				for(i = 0; i < thread->num_ports; i++){
					ixmap_tx_xmit(thread->plane, i, thread->buf,
						bulk_array[i]);
				}
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
	int			fd_ep, i, ret;

	/* epoll fd preparing */
	fd_ep = epoll_create(EPOLL_MAXEVENTS);
	if(fd_ep < 0){
		perror("failed to make epoll fd");
		goto err_epoll_open;
	}

	for(i = 0; i < thread->num_ports; i++){
		/* Register RX interrupt fd */
		ep_desc = epoll_desc_alloc_irqdev(
			thread->plane, i, thread->index, IXMAP_IRQ_RX);
		if(!ep_desc)
			goto err_assign_port;

		list_add(&ep_desc->list, ep_desc_head);

		ret = thread_irq_setmask((struct ixmap_irqdev_handle *)
			ep_desc->data, thread->index);
		if(ret < 0)
			goto err_assign_port;

		ret = epoll_add(fd_ep, ep_desc, ep_desc->fd);
		if(ret < 0){
			perror("failed to add fd in epoll");
			goto err_assign_port;
		}

		/* Register TX interrupt fd */
		ep_desc = epoll_desc_alloc_irqdev(
			thread->plane, i, thread->index, IXMAP_IRQ_TX);
		if(!ep_desc)
			goto err_assign_port;

		list_add(&ep_desc->list, ep_desc_head);

		ret = thread_irq_setmask((struct ixmap_irqdev_handle *)
			ep_desc->data, thread->index);
		if(ret < 0)
			goto err_assign_port;

		ret = epoll_add(fd_ep, ep_desc, ep_desc->fd);
		if(ret < 0){
			perror("failed to add fd in epoll");
			goto err_assign_port;
		}

		/* Register Virtual Interface fd */
		ep_desc = epoll_desc_alloc_tun(thread->tun_plane, i);
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
	ep_desc = epoll_desc_alloc_signalfd(&sigset);
	if(!ep_desc)
		goto err_epoll_desc_signalfd;

	list_add(&ep_desc->list, ep_desc_head);

	ret = epoll_add(fd_ep, ep_desc, ep_desc->fd);
	if(ret < 0){
		perror("failed to add fd in epoll");
		goto err_epoll_add_signalfd;
	}

	return fd_ep;

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
			epoll_desc_release_irqdev(ep_desc);
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

static int thread_irq_setmask(struct ixmap_irqdev_handle *irqh, int core_id)
{
	int ret;

	ret = ixmap_irqdev_setaffinity(irqh, core_id);
	if(ret < 0){
		printf("failed to set affinity\n");
		goto err_set_affinity;
	}

	return 0;

err_set_affinity:
	return -1;
}

static void thread_print_result(struct ixmapfwd_thread *thread)
{
	int i;

	for(i = 0; i < thread->num_ports; i++){
		printf("thread %d port %d statictis:\n", thread->index, i);
		printf("\tRx allocation failed = %lu\n",
			ixmap_count_rx_alloc_failed(thread->plane, i));
		printf("\tRx packetes received = %lu\n",
			ixmap_count_rx_clean_total(thread->plane, i));
		printf("\tTx xmit failed = %lu\n",
			ixmap_count_tx_xmit_failed(thread->plane, i));
		printf("\tTx packetes transmitted = %lu\n",
			ixmap_count_tx_clean_total(thread->plane, i));
	}
	return;
}