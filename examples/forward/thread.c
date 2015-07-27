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

static int thread_fd_prepare(struct epoll_desc **_ep_desc_list,
	struct ixmap_instance *instance, uint32_t num_ports, uint32_t queue_index);
static void thread_fd_destroy(struct epoll_desc *ep_desc_list,
	int fd_ep, int num_ports);
static int thread_irq_setmask(struct ixmap_irqdev_handle *irqh, int core_id);
static void thread_print_result(struct ixmapfwd_thread *thread);

void *thread_process_interrupt(void *data)
{
	struct ixmapfwd_thread *thread = data;
	struct ixmap_instance *instance;
	struct ixmap_buf *buf;
	struct ixmap_bulk *bulk_rx, **bulk_tx_list;
	struct epoll_desc *ep_desc_list, *ep_desc;
	struct ixmap_irqdev_handle *irqh;
	struct tun_instance *instance_tun;
	struct epoll_event events[EPOLL_MAXEVENTS];
	int read_size, fd_ep, i, ret, num_fd;
	unsigned int port_index;
	uint8_t read_buf[READ_BUF];
	int bulk_tx_assigned = 0;

	ixgbe_print("thread %d started\n", thread->index);
	instance = thread->instance;
	buf = thread->buf;
	instance_tun = thread->instance_tun;

	/* Prepare read buffer */
	read_size = max(sizeof(uint32_t),
		sizeof(struct signalfd_siginfo));
	for(i = 0; i < thread->num_ports; i++){
		/* calclulate maximum buf_size we should prepare */
		if(instance_tun->ports[i].mtu > read_size)
			read_size = instance_tun->ports[i].mtu;
        }
	read_buf = malloc(read_size);
	if(!read_buf)
		goto err_alloc_read_buf;

	/* Prepare bulk array */
	bulk_rx = ixmap_bulk_alloc(instance, thread->num_ports);
	if(!bulk_rx)
		goto err_bulk_rx_alloc;

	bulk_tx_list = malloc(sizeof(struct ixmap_bulk *) * thread->num_ports);
	if(!bulk_tx_list)
		goto err_bulk_tx_alloc_list;

	for(i = 0; i < thread->num_ports; i++, bulk_tx_assigned++){
		bulk_tx_list[i] = ixmap_bulk_alloc(instance, thread->num_ports);
		if(!bulk_tx_list[i])
			goto err_bulk_tx_alloc;
	}

	/* Prepare each fd in epoll */
	fd_ep = thread_fd_prepare(&ep_desc_list,
		instance, instance_tun, thread->num_ports, thread->index);
	if(fd_ep < 0){
		printf("failed to epoll prepare\n");
		goto err_ixgbe_epoll_prepare;
	}

	/* Prepare initial RX buffer */
	for(i = 0; i < thread->num_ports; i++){
		ixmap_rx_alloc(instance, i, buf);
	}

	rcu_register_thread();

	while(1){
		num_fd = epoll_wait(fd_ep, events, EPOLL_MAXEVENTS, -1);
		if(num_fd < 0){
			perror("epoll error");
			continue;
		}

		for(i = 0; i < num_fd; i++){
			ep_desc = (struct epoll_desc *)events[i].data.ptr;
			irqh = (struct ixmap_irqdev_handle *)ep_desc->data;
			
			switch(ep_desc->type){
			case EPOLL_IRQ_RX:
				port_index = ixmap_port_index(irqh);

				/* Rx descripter cleaning */
				ret = ixmap_rx_clean(instance, port_index, buf, bulk_rx);
				ixmap_rx_alloc(instance, port_index, buf);

#ifdef DEBUG
				forward_dump(buf, bulk_rx);
#endif

				forward_process(buf, port_index,
					bulk_rx, bulk_tx_list, instance_tun);
				for(i = 0; i < thread->num_ports; i++){
					ixmap_tx_xmit(instance, i, buf, bulk_tx_list[i]);
				}

				if(ret < ixmap_budget(instance, port_index)){
					ret = read(ep_desc->fd, read_buf, read_size);
					if(ret < 0)
						goto out;
					ixmap_irq_unmask_queues(instance, irqh);
				}
				break;
			case EPOLL_IRQ_TX:
				port_index = ixmap_port_index(irqh);

				/* Tx descripter cleaning */
				ret = ixmap_tx_clean(instance, port_index, buf);

				if(ret < ixmap_budget(instance, port_index)){
					ret = read(ep_desc->fd, read_buf, read_size);
					if(ret < 0)
						goto out;
					ixmap_irq_unmask_queues(instance, irqh);
				}
				break;
			case EPOLL_TUN:
				port_index = *(unsigned int *)ep_desc->data;

				ret = read(ep_desc->fd, read_buf, read_size);
				if(ret < 0)
					continue;

				forward_process_tun(port_index,
					read_buf, read_size, bulk_tx_list);
				for(i = 0; i < thread->num_ports; i++){
					ixmap_tx_xmit(instance, i, buf, bulk_tx_list[i]);
				}
				break;
			case EPOLL_SIGNAL:
				ret = read(ep_desc->fd, read_buf, read_size);
				if(ret < 0)
					continue;
				goto out;
				break;
			default:
				break;
			}
		}
	}

out:
	rcu_unregister_thread();
	thread_epoll_destroy(ep_desc_list, fd_ep, thread->num_ports);
	for(i = 0; i < bulk_tx_assigned; i++){
		ixmap_bulk_release(bulk_tx_list[i]);
	}
	free(bulk_tx_list);
	ixmap_bulk_release(bulk_rx);
	free(read_buf);
	thread_print_result(thread);
	return NULL;

err_ixgbe_epoll_prepare:
err_bulk_tx_alloc:
	for(i = 0; i < bulk_tx_assigned; i++){
		ixmap_bulk_release(bulk_tx_list[i]);
	}
	free(bulk_tx_list);
err_bulk_tx_alloc_list:
	ixmap_bulk_release(bulk_rx);
err_bulk_rx_alloc:
	free(read_buf);
err_alloc_read_buf:
	printf("thread execution failed\n");
	pthread_kill(thread->ptid, SIGINT);
	return NULL;
}

static int thread_fd_prepare(struct epoll_desc **ep_desc_list,
	struct ixmap_instance *instance, struct tun_instance *instance_tun,
	uint32_t num_ports, uint32_t queue_index)
{
	struct epoll_desc ep_desc_root, *ep_desc_last;
	sigset_t sigset;
	int fd_ep, num_fd, i, ret;

	memset(&ep_desc_root, 0, sizeof(struct epoll_desc));
	ep_desc_last = &ep_desc_root;

	/* epoll fd preparing */
	fd_ep = epoll_open();
	if(fd_ep < 0){
		perror("failed to make epoll fd");
		goto err_epoll_open;
	}

	for(i = 0; i < num_ports; i++){
		/* Register RX interrupt fd */
		ep_desc_last->next = epoll_desc_alloc_irqdev(
			instance, i, queue_index, IXMAP_IRQ_RX);
		if(!ep_desc_last->next)
			goto err_assign_port;

		ret = thread_irq_setmask((struct ixmap_irqdev_handle *)
			ep_desc_last->next->data, queue_index);
		if(ret < 0)
			goto err_assign_port;

		ep_desc_last = ep_desc_last->next;
		ret = epoll_add(fd_ep, ep_desc_last, ep_desc_last->fd);
		if(ret < 0){
			perror("failed to add fd in epoll");
			goto err_assign_port;
		}

		/* Register TX interrupt fd */
		ep_desc_last->next = epoll_desc_alloc_irqdev(
			instance, i, queue_index, IXMAP_IRQ_TX);
		if(!ep_desc_last->next)
			goto err_assign_port;

		ret = thread_irq_setmask((struct ixmap_irqdev_handle *)
			ep_desc_last->next->data, queue_index);
		if(ret < 0)
			goto err_assign_port;

                ep_desc_last = ep_desc_last->next;
		ret = epoll_add(fd_ep, ep_desc_last, ep_desc_last->fd);
		if(ret < 0){
			perror("failed to add fd in epoll");
			goto err_assign_port;
		}

		/* Register Virtual Interface fd */
		ep_desc_last->next = epoll_desc_alloc_tun(instance_tun, i);
		if(!ep_desc_last->next)
			goto err_assign_port;

		ep_desc_last = ep_desc_last->next;
		ret = epoll_add(fd_ep, ep_desc_last, ep_desc_last->fd);
		if(ret < 0){
			perror("failed to add fd in epoll");
			goto err_assign_port;
		}
	}

	/* signalfd preparing */
	sigemptyset(&sigset);
	sigaddset(&sigset, SIGUSR1);
	ep_desc_last->next = epoll_desc_alloc_signalfd(&sigset);
	if(!ep_desc_last->next)
		goto err_epoll_desc_signalfd;

	ep_desc_last = ep_desc_last->next;
	ret = epoll_add(fd_ep, ep_desc_last, ep_desc_last->fd);
	if(ret < 0){
		perror("failed to add fd in epoll");
		goto err_epoll_add_signalfd;
	}

	*ep_desc_list = ep_desc_root.next;
	return fd_ep;

err_epoll_add_signalfd:
err_epoll_desc_signalfd:
err_assign_port:
	thread_fd_destroy(ep_desc_root.next, fd_ep, num_ports);
err_epoll_open:
	return -1;
}

static void thread_fd_destroy(struct epoll_desc *ep_desc_list,
	int fd_ep)
{
	struct epoll_desc *ep_desc, *ep_desc_next;

	ep_desc = ep_desc_list;
	while(ep_desc){
		ep_desc_next = ep_desc->next;

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

		ep_desc = ep_desc_next;
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
			ixmap_count_rx_alloc_failed(thread->instance, i));
		printf("\tRx packetes received = %lu\n",
			ixmap_count_rx_clean_total(thread->instance, i));
		printf("\tTx xmit failed = %lu\n",
			ixmap_count_tx_xmit_failed(thread->instance, i));
		printf("\tTx packetes transmitted = %lu\n",
			ixmap_count_tx_clean_total(thread->instance, i));
	}
	return;
}
