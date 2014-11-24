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

#include "main.h"
#include "driver.h"

static int ixgbe_epoll_prepare(struct ixgbe_irq_data **_irq_data_list,
	struct ixgbe_port *ports, uint32_t num_ports, uint32_t thread_index);
static void ixgbe_epoll_destroy(struct ixgbe_irq_data *irq_data_list,
	int fd_ep, int num_ports);
static int epoll_add(int fd_ep, void *ptr, int fd);
static int ixgbe_irq_setmask(struct ixgbe_irq_data *irq_data_list,
	int num_ports, int thread_index);
static int signalfd_create();

#ifdef DEBUG
static void dump_packet(void *buffer)
{
	struct ether_header *eth;
	eth = (struct ether_header *)buffer;

	printf("\tsrc %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n",
		eth->ether_shost[0], eth->ether_shost[1], eth->ether_shost[2],
		eth->ether_shost[3], eth->ether_shost[4], eth->ether_shost[5]);
	printf("\tdst %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n",
		eth->ether_dhost[0], eth->ether_dhost[1], eth->ether_dhost[2],
		eth->ether_dhost[3], eth->ether_dhost[4], eth->ether_dhost[5]);
	printf("\ttype 0x%x\n", eth->ether_type);
	return;
}
#endif

void *process_interrupt(void *data)
{
	struct ixgbe_thread *thread = data;
	struct ixgbe_irq_data *irq_data_list, *irq_data;
	struct ixgbe_bulk bulk;
	struct epoll_event events[EPOLL_MAXEVENTS];
	uint64_t tx_qmask, rx_qmask;
	int read_size, fd_ep, i, ret, num_fd;
	int max_bulk_count = 0;
	uint8_t *read_buf;

	ixgbe_print("thread %d started\n", thread->index);

	/* Prepare TX/RX interrupt mask */
	rx_qmask = 1 << thread->index;
	tx_qmask = 1 << (thread->index + thread->num_threads);

	/* Prepare read buffer */
	read_size = max(sizeof(uint32_t),
			sizeof(struct signalfd_siginfo));
	read_buf = malloc(read_size);
	if(!read_buf)
		goto err_alloc_read_buf;

	/* Prepare bulk array */
	for(i = 0; i < thread->num_ports; i++){
		if(thread->ports[i].budget > max_bulk_count){
			max_bulk_count = thread->ports[i].budget;
		}
	}
	bulk.count = 0;
	bulk.slot_index = malloc(sizeof(int) * max_bulk_count);
	if(!bulk.slot_index)
		goto err_bulk_slot_index;
	bulk.size = malloc(sizeof(uint32_t) * max_bulk_count);
	if(!bulk.size)
		goto err_bulk_size;

	/* Prepare TX-irq/RX-irq/signalfd in epoll */
	fd_ep = ixgbe_epoll_prepare(&irq_data_list,
		thread->ports, thread->num_ports, thread->index);
	if(fd_ep < 0){
		printf("failed to epoll prepare\n");
		goto err_ixgbe_epoll_prepare;
	}

	/* Set interrupt masks */
	ret = ixgbe_irq_setmask(irq_data_list,
		thread->num_ports, thread->index);
	if(ret < 0){
		printf("failed to set irq affinity mask\n");
		goto err_ixgbe_irq_setmask;
	}

	/* Prepare initial RX buffer */
	for(i = 0; i < thread->num_ports; i++){
		ixgbe_rx_alloc(&thread->ports[i], thread->buf, i);
	}

	while(1){
		num_fd = epoll_wait(fd_ep, events, EPOLL_MAXEVENTS, -1);
		if(num_fd < 0){
			perror("epoll error");
			continue;
		}

		for(i = 0; i < num_fd; i++){
			irq_data = (struct ixgbe_irq_data *)events[i].data.ptr;
			
			switch(irq_data->type){
			case IXGBE_IRQ_RX:
				/* Rx descripter cleaning */
				ixgbe_print("Rx: descripter cleaning on thread %d\n",
					thread->index);
				ret = ixgbe_rx_clean(&thread->ports[irq_data->port_index],
					thread->buf, &bulk);
				ixgbe_print("Rx: descripter cleaning completed\n\n");

				ixgbe_rx_alloc(&thread->ports[irq_data->port_index],
					thread->buf, irq_data->port_index);

				ixgbe_print("Tx: xmit start on thread %d\n",
					thread->index);
				/* XXX: Following is 2ports specific code */
				ixgbe_tx_xmit(&thread->ports[!irq_data->port_index],
					thread->buf, !irq_data->port_index, &bulk);
				ixgbe_print("Tx: xmit completed\n\n");

				if(ret < thread->ports[irq_data->port_index].budget){
					ret = read(irq_data->fd, read_buf, read_size);
					if(ret < 0)
						goto out;
					ixgbe_irq_enable_queues(
						thread->ports[irq_data->port_index].ih,
						rx_qmask);
				}
				break;
			case IXGBE_IRQ_TX:
				/* Tx descripter cleaning */
				ixgbe_print("Tx: descripter cleaning on thread %d\n",
					thread->index);
				ret = ixgbe_tx_clean(&thread->ports[irq_data->port_index],
					thread->buf);
				ixgbe_print("Tx: descripter cleaning completed\n\n");

				/* XXX: Following is 2ports specific code */
				ixgbe_rx_alloc(&thread->ports[!irq_data->port_index],
					thread->buf, !irq_data->port_index);

				if(ret < thread->ports[irq_data->port_index].budget){
					ret = read(irq_data->fd, read_buf, read_size);
					if(ret < 0)
						goto out;
					ixgbe_irq_enable_queues(
						thread->ports[irq_data->port_index].ih,
						tx_qmask);
				}
				break;
			case IXGBE_SIGNAL:
				ret = read(irq_data->fd, read_buf, read_size);
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
	ixgbe_epoll_destroy(irq_data_list,
		fd_ep, thread->num_ports);
	free(bulk.size);
	free(bulk.slot_index);
	free(read_buf);
	for(i = 0; i < thread->num_ports; i++){
		printf("thread %d port %d statictis:\n", thread->index, i);
		printf("\tRx allocation failed = %lu\n",
			thread->ports[i].count_rx_alloc_failed);
		printf("\tRx packetes received = %lu\n",
			thread->ports[i].count_rx_clean_total);
		printf("\tTx xmit failed = %lu\n",
			thread->ports[i].count_tx_xmit_failed);
		printf("\tTx packetes transmitted = %lu\n",
			thread->ports[i].count_tx_clean_total);
	}
	return NULL;

err_ixgbe_irq_setmask:
	ixgbe_epoll_destroy(irq_data_list,
		fd_ep, thread->num_ports);
err_ixgbe_epoll_prepare:
	free(bulk.size);
err_bulk_size:
	free(bulk.slot_index);
err_bulk_slot_index:
	free(read_buf);
err_alloc_read_buf:
	printf("thread execution failed\n");
	pthread_kill(thread->ptid, SIGINT);
	return NULL;
}

static int ixgbe_epoll_prepare(struct ixgbe_irq_data **_irq_data_list,
	struct ixgbe_port *ports, uint32_t num_ports, uint32_t thread_index)
{
	char filename[FILENAME_SIZE];
	struct ixgbe_irq_data *irq_data_list;
	int fd_ep, i, ret;
	int assigned_ports = 0;

	/* epoll fd preparing */
	fd_ep = epoll_create(EPOLL_MAXEVENTS);
	if(fd_ep < 0){
		perror("failed to make epoll fd");
		goto err_epoll_create;
	}

	/* TX/RX interrupt data preparing */
	irq_data_list =
		malloc(sizeof(struct ixgbe_irq_data) * (num_ports * 2 + 1));
	if(!irq_data_list)
		goto err_alloc_irq_data_list;

	for(i = 0; i < num_ports; i++, assigned_ports++){
		/* Rx interrupt fd preparing */
		snprintf(filename, sizeof(filename), "/dev/%s-irqrx%d",
			ports[i].interface_name, thread_index);
		irq_data_list[i].fd = open(filename, O_RDWR);
		if(irq_data_list[i].fd < 0){
			perror("failed to open");
			goto err_assign_port;
		}

		irq_data_list[i].type = IXGBE_IRQ_RX;
		irq_data_list[i].port_index = i;

		/* Tx interrupt fd preparing */
		snprintf(filename, sizeof(filename), "/dev/%s-irqtx%d",
			ports[i].interface_name, thread_index);
		irq_data_list[i + num_ports].fd = open(filename, O_RDWR);
		if(irq_data_list[i + num_ports].fd < 0){
			perror("failed to open");
			close(irq_data_list[i].fd);
			goto err_assign_port;
		}

		irq_data_list[i + num_ports].type = IXGBE_IRQ_TX;
		irq_data_list[i + num_ports].port_index = i;

		ret = epoll_add(fd_ep, &irq_data_list[i],
			irq_data_list[i].fd);
		if(ret < 0){
			perror("failed to add fd in epoll");
			close(irq_data_list[i].fd);
			close(irq_data_list[i + num_ports].fd);
			goto err_assign_port;
		}

		ret = epoll_add(fd_ep, &irq_data_list[i + num_ports],
			irq_data_list[i + num_ports].fd);
		if(ret < 0){
			perror("failed to add fd in epoll");
			close(irq_data_list[i].fd);
			close(irq_data_list[i + num_ports].fd);
			goto err_assign_port;
		}
	}

	/* signalfd preparing */
	irq_data_list[num_ports * 2].fd = signalfd_create();
	if(irq_data_list[num_ports * 2].fd < 0){
		perror("failed to open signalfd");
		goto err_signalfd_create;
        }
	irq_data_list[num_ports * 2].type = IXGBE_SIGNAL;
	irq_data_list[num_ports * 2].port_index = -1;

	ret = epoll_add(fd_ep, &irq_data_list[num_ports * 2],
		irq_data_list[num_ports * 2].fd);
	if(ret < 0){
		perror("failed to add fd in epoll");
		goto err_epoll_add_signal_fd;
	}

	*_irq_data_list = irq_data_list;
	return fd_ep;

err_epoll_add_signal_fd:
	close(irq_data_list[num_ports * 2].fd);
err_signalfd_create:
err_assign_port:
	for(i = 0; i < assigned_ports; i++){
		close(irq_data_list[i].fd);
		close(irq_data_list[i + num_ports].fd);
	}
	free(irq_data_list);
err_alloc_irq_data_list:
	close(fd_ep);
err_epoll_create:
	return -1;
}

static void ixgbe_epoll_destroy(struct ixgbe_irq_data *irq_data_list,
	int fd_ep, int num_ports)
{
	int i;

	close(irq_data_list[num_ports * 2].fd);

	for(i = 0; i < num_ports; i++){
		close(irq_data_list[i].fd);
		close(irq_data_list[i + num_ports].fd);
	}

	free(irq_data_list);
	close(fd_ep);
	return;
}

static int epoll_add(int fd_ep, void *ptr, int fd)
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

static int ixgbe_irq_setmask(struct ixgbe_irq_data *irq_data_list,
	int num_ports, int thread_index)
{
	struct uio_irq_info_req req_info;
	FILE *file;
	char filename[FILENAME_SIZE];
	uint32_t mask_low, mask_high;
	int i, ret;

	mask_low = thread_index <= 31 ? 1 << thread_index : 0;
	mask_high = thread_index <= 31 ? 0 : 1 << (thread_index - 31);

	for(i = 0; i < num_ports * 2; i++){
		ret = ioctl(irq_data_list[i].fd, UIO_IRQ_INFO,
			(unsigned long)&req_info);
		if(ret < 0){
			printf("failed to UIO_IRQ_INFO\n");
			goto err_set_affinity;
		}

		snprintf(filename, sizeof(filename),
			"/proc/irq/%d/smp_affinity", req_info.vector);
		file = fopen(filename, "w");
		if(!file){
			printf("failed to open smp_affinity\n");
			goto err_set_affinity;
		}

		ixgbe_print("irq affinity mask: %08x,%08x\n", mask_high, mask_low);
		ret = fprintf(file, "%08x,%08x", mask_high, mask_low);
		if(ret < 0){
			fclose(file);
			goto err_set_affinity;
		}

		fclose(file);
	}

	return 0;

err_set_affinity:
	return -1;
}

static int signalfd_create(){
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

