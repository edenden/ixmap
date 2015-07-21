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
#include <ixmap.h>

#include "main.h"
#include "thread.h"

static int thread_epoll_prepare(struct ixmapfwd_fd_desc **_fd_desc_list,
	struct ixmap_instance *instance, uint32_t num_ports, uint32_t queue_index);
static void thread_epoll_destroy(struct ixmapfwd_fd_desc *fd_desc_list,
	int fd_ep, int num_ports);
static int thread_epoll_add(int fd_ep, void *ptr, int fd);
static int thread_irq_setmask(struct ixmapfwd_fd_desc *fd_desc_list,
	int num_ports, int core_id);
static int thread_signalfd_create();
static void thread_print_result(struct ixmapfwd_thread *thread);

void *thread_process_interrupt(void *data)
{
	struct ixmapfwd_thread *thread = data;
	struct ixmap_instance *instance;
	struct ixmap_buf *buf;
	struct ixmap_bulk *bulk_rx, **bulk_tx_list;
	struct ixmapfwd_fd_desc *fd_desc_list, *fd_desc;
	struct epoll_event events[EPOLL_MAXEVENTS];
	int read_size, fd_ep, i, ret, num_fd;
	unsigned int port_index;
	uint8_t *read_buf;
	int bulk_tx_assigned = 0;

	ixgbe_print("thread %d started\n", thread->index);
	instance = thread->instance;
	buf = thread->buf;

	/* Prepare read buffer */
	read_size = max(sizeof(uint32_t),
			sizeof(struct signalfd_siginfo));
	read_buf = malloc(read_size);
	if(!read_buf)
		goto err_alloc_read_buf;

	/* Prepare bulk array */
	bulk_rx = ixmap_bulk_alloc(instance, thread->num_ports);
	if(!bulk_rx)
		goto err_bulk_rx_alloc;

	bulk_tx_list = malloc(sizeof(struct ixmap_bulk) * thread->num_ports);
	if(!bulk_tx_list)
		goto err_bulk_tx_list_alloc;

	for(i = 0; i < thread->num_ports; i++, bulk_tx_assigned++){
		bulk_tx_list[i] = ixmap_bulk_alloc(instance, thread->num_ports);
		if(!bulk_tx_list[i])
			goto err_bulk_tx_alloc;
	}

	/* Prepare each fd in epoll */
	fd_ep = thread_epoll_prepare(&fd_desc_list,
		instance, thread->num_ports, thread->index);
	if(fd_ep < 0){
		printf("failed to epoll prepare\n");
		goto err_ixgbe_epoll_prepare;
	}

	/* Set interrupt masks */
	ret = thread_irq_setmask(fd_desc_list, 
		thread->num_ports, thread->index);
	if(ret < 0){
		printf("failed to set irq affinity mask\n");
		goto err_ixgbe_irq_setmask;
	}

	/* Prepare initial RX buffer */
	for(i = 0; i < thread->num_ports; i++){
		ixmap_rx_alloc(instance, i, buf);
	}

	while(1){
		num_fd = epoll_wait(fd_ep, events, EPOLL_MAXEVENTS, -1);
		if(num_fd < 0){
			perror("epoll error");
			continue;
		}

		for(i = 0; i < num_fd; i++){
			fd_desc = (struct ixmapfwd_fd_desc *)events[i].data.ptr;
			
			switch(fd_desc->type){
			case IXMAPFWD_IRQ_RX:
				port_index = ixmap_port_index(fd_desc->irqh);

				/* Rx descripter cleaning */
				ret = ixmap_rx_clean(instance, port_index, buf, bulk_rx);
				ixmap_rx_alloc(instance, port_index, buf);

#ifdef DEBUG
				packet_dump(buf, bulk_rx);
#endif

				packet_process(buf, port_index, bulk_rx, bulk_tx_list);
				for(i = 0; i < thread->num_ports; i++){
					if(bulk > 0){
						ixmap_tx_xmit(instance, i, buf, bulk_tx_list[i]);
					}
				}

				if(ret < ixmap_budget(instance, port_index)){
					ret = read(fd_desc->fd, read_buf, read_size);
					if(ret < 0)
						goto out;
					ixmap_irq_unmask_queues(instance, fd_desc->irqh);
				}
				break;
			case IXMAPFWD_IRQ_TX:
				port_index = ixmap_port_index(fd_desc->irqh);

				/* Tx descripter cleaning */
				ret = ixmap_tx_clean(instance, port_index, buf);

				if(ret < ixmap_budget(instance, port_index)){
					ret = read(fd_desc->fd, read_buf, read_size);
					if(ret < 0)
						goto out;
					ixmap_irq_unmask_queues(instance, fd_desc->irqh);
				}
				break;
			case IXMAPFWD_SIGNAL:
				ret = read(fd_desc->fd, read_buf, read_size);
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
	thread_epoll_destroy(fd_desc_list, fd_ep, thread->num_ports);
	ixmap_bulk_release(bulk);
	free(read_buf);
	thread_print_result(thread);
	return NULL;

err_ixgbe_irq_setmask:
	thread_epoll_destroy(fd_desc_list, fd_ep, thread->num_ports);
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

static int thread_epoll_prepare(struct ixmapfwd_fd_desc **_fd_desc_list,
	struct ixmap_instance *instance, uint32_t num_ports, uint32_t queue_index)
{
	struct ixmapfwd_fd_desc *fd_desc_list;
	struct ixmapfwd_fd_desc *fd_desc_rx, *fd_desc_tx;
	struct ixmapfwd_fd_desc *fd_desc_sig;
	int fd_ep, num_fd, i, ret;
	int assigned_ports = 0;

	/* epoll fd preparing */
	fd_ep = epoll_create(EPOLL_MAXEVENTS);
	if(fd_ep < 0){
		perror("failed to make epoll fd");
		goto err_epoll_create;
	}

	/* TX/RX interrupt data preparing */
	num_fd = (num_ports * 2) + 1;
	fd_desc_list = malloc(sizeof(struct ixmapfwd_fd_desc) * num_fd);
	if(!fd_desc_list)
		goto err_alloc_fddesc;

	for(i = 0; i < num_ports; i++, assigned_ports++){
		fd_desc_rx = &fd_desc_list[i];
		fd_desc_tx = &fd_desc_list[i + num_ports];
		
		/* Rx interrupt fd preparing */
		fd_desc_rx->irqh = ixmap_irqdev_open(instance,
					i, queue_index, IXMAP_IRQ_RX);
		if(!fd_desc_rx->irqh){
			perror("failed to open");
			goto err_assign_port;
                }
		fd_desc_rx->fd = ixmap_irqdev_fd(fd_desc_rx->irqh);
		fd_desc_rx->type = IXMAPFWD_IRQ_RX;

		/* Tx interrupt fd preparing */
		fd_desc_tx->irqh = ixmap_irqdev_open(instance,
					i, queue_index, IXMAP_IRQ_TX);
		if(!fd_desc_tx->irqh){
			ixmap_irqdev_close(fd_desc_rx->irqh);
			perror("failed to open");
			goto err_assign_port;
		}
		fd_desc_tx->fd = ixmap_irqdev_fd(fd_desc_tx->irqh);
		fd_desc_tx->type = IXMAPFWD_IRQ_TX;

		ret = thread_epoll_add(fd_ep, fd_desc_rx, fd_desc_rx->fd);
		if(ret < 0){
			perror("failed to add fd in epoll");
			ixmap_irqdev_close(fd_desc_rx->irqh);
			ixmap_irqdev_close(fd_desc_tx->irqh);
			goto err_assign_port;
		}

		ret = thread_epoll_add(fd_ep, fd_desc_tx, fd_desc_tx->fd);
		if(ret < 0){
			perror("failed to add fd in epoll");
			ixmap_irqdev_close(fd_desc_rx->irqh);
			ixmap_irqdev_close(fd_desc_tx->irqh);
			goto err_assign_port;
		}
	}

	/* signalfd preparing */
	fd_desc_sig = &fd_desc_list[num_ports * 2];

	fd_desc_sig->fd = thread_signalfd_create();
	if(fd_desc_sig->fd < 0){
		perror("failed to open signalfd");
		goto err_signalfd_create;
        }

	fd_desc_sig->irqh = NULL;
	fd_desc_sig->type = IXMAPFWD_SIGNAL;

	ret = thread_epoll_add(fd_ep, fd_desc_sig,
		fd_desc_sig->fd);
	if(ret < 0){
		perror("failed to add fd in epoll");
		goto err_epoll_add_signal_fd;
	}

	*_fd_desc_list = fd_desc_list;
	return fd_ep;

err_epoll_add_signal_fd:
	close(fd_desc_sig->fd);
err_signalfd_create:
err_assign_port:
	for(i = 0; i < assigned_ports; i++){
		fd_desc_rx = &fd_desc_list[i];
		fd_desc_tx = &fd_desc_list[i + num_ports];

		ixmap_irqdev_close(fd_desc_rx->irqh);
		ixmap_irqdev_close(fd_desc_tx->irqh);
	}
	free(fd_desc_list);
err_alloc_fddesc:
	close(fd_ep);
err_epoll_create:
	return -1;
}

static void thread_epoll_destroy(struct ixmapfwd_fd_desc *fd_desc_list,
	int fd_ep, int num_ports)
{
	struct ixmapfwd_fd_desc *fd_desc_rx, *fd_desc_tx;
	struct ixmapfwd_fd_desc *fd_desc_sig;
	int i;

	fd_desc_sig = &fd_desc_list[num_ports * 2];
	close(fd_desc_sig->fd);

	for(i = 0; i < num_ports; i++){
		fd_desc_rx = &fd_desc_list[i];
		fd_desc_tx = &fd_desc_list[i + num_ports];

		ixmap_irqdev_close(fd_desc_rx->irqh);
		ixmap_irqdev_close(fd_desc_tx->irqh);
	}

	free(fd_desc_list);
	close(fd_ep);
	return;
}

static int thread_epoll_add(int fd_ep, void *ptr, int fd)
{
	struct epoll_event event;
	int ret;

	memset(&event, 0, sizeof(struct epoll_event));
	event.events = EPOLLIN;
	event.data.ptr = ptr;
	ret = epoll_ctl(fd_ep, EPOLL_CTL_ADD, fd, &event);
	if(ret < 0)
		return -1;

	return 0;
}

static int thread_irq_setmask(struct ixmapfwd_fd_desc *fd_desc_list,
	int num_ports, int core_id)
{
	int i, ret;

	for(i = 0; i < num_ports * 2; i++){
		ret = ixmap_irqdev_setaffinity(fd_desc_list[i].irqh, core_id);
		if(ret < 0){
			printf("failed to set affinity\n");
			goto err_set_affinity;
		}
	}

	return 0;

err_set_affinity:
	return -1;
}

static int thread_signalfd_create()
{
	sigset_t sigset;
	int signal_fd;

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGUSR1);

	signal_fd = signalfd(-1, &sigset, 0);
	if(signal_fd < 0){
		perror("signalfd");
		return -1;
	}

	return signal_fd;
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
