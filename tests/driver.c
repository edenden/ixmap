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

static void ixgbe_rx_alloc(struct ixgbe_ring *rx_ring, struct ixgbe_buf *buf,
	int port_index);
static void ixgbe_tx_xmit(struct ixgbe_ring *tx_ring, struct ixgbe_buf *buf,
	int port_index, struct ixgbe_bulk *bulk);
static void ixgbe_rx_clean(struct ixgbe_ring *rx_ring, struct ixgbe_buf *buf,
	int budget, struct ixgbe_bulk *bulk);
static void ixgbe_tx_clean(struct ixgbe_ring *tx_ring, struct ixgbe_buf *buf,
	int budget);
static inline int ixgbe_slot_assign(struct ixgbe_buf *buf);
static inline void ixgbe_slot_attach(struct ixgbe_ring *ring,
	uint16_t desc_index, int slot_index);
static inline int ixgbe_slot_detach(struct ixgbe_ring *ring,
	uint16_t desc_index);
static inline void ixgbe_slot_release(struct ixgbe_buf *buf,
	int slot_index);
static inline unsigned long ixgbe_slot_addr_dma(struct ixgbe_buf *buf,
	int slot_index, int port_index);
static inline void *ixgbe_slot_addr_virt(struct ixgbe_buf *buf,
	uint16_t slot_index);
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
	int max_budget = 0;
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
		if(thread->ports[i].budget > max_budget){
			max_budget = thread->ports[i].budget;
		}
	}
	bulk.count = 0;
	bulk.slot_index = malloc(sizeof(int) * max_budget);
	if(!bulk.slot_index)
		goto err_bulk_slot_index;
	bulk.size = malloc(sizeof(uint32_t) * max_budget);
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
		ixgbe_rx_alloc(thread->ports[i].rx_ring, thread->buf, i);
	}

	while(1){
		num_fd = epoll_wait(fd_ep, events, EPOLL_MAXEVENTS, -1);
		if(num_fd < 0){
			perror("epoll error");
			continue;
		}

		for(i = 0; i < num_fd; i++){
			irq_data = (struct ixgbe_irq_data *)events[i].data.ptr;
			ret = read(irq_data->fd, read_buf, read_size);
			if(ret < 0)
				continue;
			
			switch(irq_data->type){
			case IXGBE_IRQ_RX:
				/* Rx descripter cleaning */
				ixgbe_print("Rx: descripter cleaning on thread %d\n",
					thread->index);
				ixgbe_rx_clean(thread->ports[irq_data->port_index].rx_ring,
					thread->buf, thread->ports[irq_data->port_index].budget,
					&bulk);
				ixgbe_print("Rx: descripter cleaning completed\n\n");

				ixgbe_rx_alloc(thread->ports[irq_data->port_index].rx_ring,
					thread->buf, irq_data->port_index);
				ixgbe_irq_enable_queues(thread->ports[irq_data->port_index].ih,
					rx_qmask);

				ixgbe_print("Tx: xmit start on thread %d\n",
					thread->index);
				/* XXX: Following is 2ports specific code */
				ixgbe_tx_xmit(thread->ports[!irq_data->port_index].tx_ring,
					thread->buf, !irq_data->port_index, &bulk);
				ixgbe_print("Tx: xmit completed\n\n");
				break;
			case IXGBE_IRQ_TX:
				/* Tx descripter cleaning */
				ixgbe_print("Tx: descripter cleaning on thread %d\n",
					thread->index);
				ixgbe_tx_clean(thread->ports[irq_data->port_index].tx_ring,
					thread->buf, thread->ports[irq_data->port_index].budget);
				ixgbe_print("Tx: descripter cleaning completed\n\n");

				ixgbe_irq_enable_queues(thread->ports[irq_data->port_index].ih,
					tx_qmask);

				/* XXX: Following is 2ports specific code */
				ixgbe_rx_alloc(thread->ports[!irq_data->port_index].rx_ring,
					thread->buf, !irq_data->port_index);

				break;
			case IXGBE_SIGNAL:
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
	ixgbe_print("thread %d stopped\n", thread->index);
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

static void ixgbe_rx_alloc(struct ixgbe_ring *rx_ring, struct ixgbe_buf *buf,
	int port_index)
{
	unsigned int total_allocated = 0;
	uint16_t max_allocation;

	max_allocation = ixgbe_desc_unused(rx_ring);
	if (!max_allocation)
		return;

	do{
		union ixgbe_adv_rx_desc *rx_desc;
		uint16_t next_to_use;
		uint64_t addr_dma;
		int slot_index;

		rx_desc = IXGBE_RX_DESC(rx_ring, rx_ring->next_to_use);

		slot_index = ixgbe_slot_assign(buf);
		if(slot_index < 0)
			break;

		ixgbe_slot_attach(rx_ring, rx_ring->next_to_use, slot_index);
		addr_dma = (uint64_t)ixgbe_slot_addr_dma(buf,
				slot_index, port_index);

		rx_desc->read.pkt_addr = htole64(addr_dma);
		rx_desc->read.hdr_addr = 0;

		next_to_use = rx_ring->next_to_use + 1;
		rx_ring->next_to_use =
			(next_to_use < rx_ring->count) ? next_to_use : 0;

		total_allocated++;
	}while(likely(total_allocated < max_allocation));

	if(likely(total_allocated)){
		/*
		 * Force memory writes to complete before letting h/w
		 * know there are new descriptors to fetch.  (Only
		 * applicable for weak-ordered memory model archs,
		 * such as IA-64).
		 */
		/* XXX: Do we need this write memory barrier ? */
		wmb();
		ixgbe_write_tail(rx_ring, rx_ring->next_to_use);
	}
}

static void ixgbe_tx_xmit(struct ixgbe_ring *tx_ring, struct ixgbe_buf *buf,
	int port_index, struct ixgbe_bulk *bulk)
{
	unsigned int total_xmit = 0;
	uint16_t unused_count;
	uint32_t tx_flags;
	int i;

	/* Nothing to do */
	if(!bulk->count)
		return;

	/* set type for advanced descriptor with frame checksum insertion */
	tx_flags = IXGBE_ADVTXD_DTYP_DATA | IXGBE_ADVTXD_DCMD_DEXT
			| IXGBE_ADVTXD_DCMD_IFCS;
	unused_count = min(bulk->count, ixgbe_desc_unused(tx_ring));

	do{
		union ixgbe_adv_tx_desc *tx_desc;
		uint16_t next_to_use;
		int slot_index;
		uint64_t addr_dma;
		uint32_t size;
		uint32_t cmd_type;
		uint32_t olinfo_status;

		tx_desc = IXGBE_TX_DESC(tx_ring, tx_ring->next_to_use);

		slot_index = bulk->slot_index[total_xmit];
		size = bulk->size[total_xmit];
		if(unlikely(size > IXGBE_MAX_DATA_PER_TXD))
			continue;

		ixgbe_slot_attach(tx_ring, tx_ring->next_to_use, slot_index);
		addr_dma = (uint64_t)ixgbe_slot_addr_dma(buf,
					slot_index, port_index);
		ixgbe_print("Tx: packet sending DMAaddr = %p size = %d\n",
			(void *)addr_dma, size);

		cmd_type = size | IXGBE_TXD_CMD_EOP | IXGBE_TXD_CMD_RS | tx_flags;
		olinfo_status = size << IXGBE_ADVTXD_PAYLEN_SHIFT;

		tx_desc->read.buffer_addr = htole64(addr_dma);
		tx_desc->read.cmd_type_len = htole32(cmd_type);
		tx_desc->read.olinfo_status = htole32(olinfo_status);

		next_to_use = tx_ring->next_to_use + 1;
		tx_ring->next_to_use =
			(next_to_use < tx_ring->count) ? next_to_use : 0;

		total_xmit++;
	}while(likely(total_xmit < unused_count));

	if(likely(total_xmit)){
		/*
		 * Force memory writes to complete before letting h/w know there
		 * are new descriptors to fetch.  (Only applicable for weak-ordered
		 * memory model archs, such as IA-64).
		 *
		 * We also need this memory barrier to make certain all of the
		 * status bits have been updated before next_to_watch is written.
		 */
		wmb();

		/* notify HW of packet */
		ixgbe_write_tail(tx_ring, tx_ring->next_to_use);
	}

	/* drop overflowed frames */
	for(i = 0; i < bulk->count - total_xmit; i++){
		ixgbe_slot_release(buf, bulk->slot_index[total_xmit + i]);
	}

	return;
}

static void ixgbe_rx_clean(struct ixgbe_ring *rx_ring, struct ixgbe_buf *buf,
	int budget, struct ixgbe_bulk *bulk)
{
	unsigned int total_rx_packets = 0;

	do{
		union ixgbe_adv_rx_desc *rx_desc;
		uint16_t next_to_clean;
		int slot_index;
#ifdef DEBUG
		void *packet;
#endif

		if(unlikely(rx_ring->next_to_clean == rx_ring->next_to_use)){
			break;
		}

		rx_desc = IXGBE_RX_DESC(rx_ring, rx_ring->next_to_clean);

		if (!ixgbe_test_staterr(rx_desc, IXGBE_RXD_STAT_DD)){
			break;
		}

		/*
		 * This memory barrier is needed to keep us from reading
		 * any other fields out of the rx_desc until we know the
		 * RXD_STAT_DD bit is set
		 */
		rmb();

		/*
		 * Confirm: We have not to check IXGBE_RXD_STAT_EOP here
		 * because we have skipped to enable(= disabled) hardware RSC.
		 */

		/* XXX: ERR_MASK will only have valid bits if EOP set ? */
		if (unlikely(ixgbe_test_staterr(rx_desc,
			IXGBE_RXDADV_ERR_FRAME_ERR_MASK))) {
			printf("frame error detected\n");
		}

		/* retrieve a buffer address from the ring */
		slot_index = ixgbe_slot_detach(rx_ring, rx_ring->next_to_clean);
		bulk->slot_index[total_rx_packets] = slot_index;
		bulk->size[total_rx_packets] =
			le16toh(rx_desc->wb.upper.length);
		ixgbe_print("Rx: packet received size = %d\n",
			bulk->size[total_rx_packets]);

		/* XXX: Should we prefetch the packet buffer ? */
#ifdef DEBUG
		packet = ixgbe_slot_addr_virt(buf, slot_index);
		dump_packet(packet);
#endif

		next_to_clean = rx_ring->next_to_clean + 1;
		rx_ring->next_to_clean = 
			(next_to_clean < rx_ring->count) ? next_to_clean : 0;

		/* XXX: Should we prefetch the next_to_clean desc ? */

		total_rx_packets++;
	}while(likely(total_rx_packets < budget));

	bulk->count = total_rx_packets;
	return;
}

static void ixgbe_tx_clean(struct ixgbe_ring *tx_ring, struct ixgbe_buf *buf,
	int budget)
{
	unsigned int total_tx_packets = 0;

	do {
		union ixgbe_adv_tx_desc *tx_desc;
		uint16_t next_to_clean;
		int slot_index;

		if(unlikely(tx_ring->next_to_clean == tx_ring->next_to_use)){
			break;
		}

		tx_desc = IXGBE_TX_DESC(tx_ring, tx_ring->next_to_clean);

		if (!(tx_desc->wb.status & htole32(IXGBE_TXD_STAT_DD)))
			break;

		/* Release unused buffer */
		slot_index = ixgbe_slot_detach(tx_ring, tx_ring->next_to_clean);
		ixgbe_slot_release(buf, slot_index);

		next_to_clean = tx_ring->next_to_clean + 1;
		tx_ring->next_to_clean =
			(next_to_clean < tx_ring->count) ? next_to_clean : 0;

		total_tx_packets++;
	}while(likely(total_tx_packets < budget));

	return;
}

static inline int ixgbe_slot_assign(struct ixgbe_buf *buf)
{
	int slot_index = -1;

	if(!buf->free_count)
		goto out;

	slot_index = buf->free_index[buf->free_count - 1];
	buf->free_count--;
	
out:
	return slot_index;
}

static inline void ixgbe_slot_attach(struct ixgbe_ring *ring,
	uint16_t desc_index, int slot_index)
{
	ring->slot_index[desc_index] = slot_index;
	return;
}

static inline int ixgbe_slot_detach(struct ixgbe_ring *ring,
	uint16_t desc_index)
{
	int slot_index;

	slot_index = ring->slot_index[desc_index];
	return slot_index;
}

static inline void ixgbe_slot_release(struct ixgbe_buf *buf,
	int slot_index)
{
	buf->free_index[buf->free_count] = slot_index;
	buf->free_count++;

	return;
}

static inline unsigned long ixgbe_slot_addr_dma(struct ixgbe_buf *buf,
	int slot_index, int port_index)
{
	unsigned long addr_dma;

	addr_dma = buf->addr_dma[port_index] + (buf->buf_size * slot_index);
	return addr_dma;
}

static inline void *ixgbe_slot_addr_virt(struct ixgbe_buf *buf,
	uint16_t slot_index)
{
	void *addr_virtual;

	addr_virtual = buf->addr_virtual + (buf->buf_size * slot_index);
	return addr_virtual;
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

