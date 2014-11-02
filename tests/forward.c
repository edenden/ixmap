#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/epoll.h>

#include <net/ethernet.h>
#include <pthread.h>

#include "main.h"
#include "forward.h"
#include "descring.h"

static int epoll_add(int fd_ep, int fd);

void *process_interrupt(void *data)
{
	struct ixgbe_thread *thread = data;
	struct epoll_event events[EPOLL_MAXEVENTS];
	int fd_ep, fd_intrx, fd_inttx, i, num_fd;
	char filename[FILENAME_SIZE];
	uint64_t tx_qmask, rx_qmask;
	struct ixgbe_bulk bulk;

	/* Rx interrupt fd preparing */
	snprintf(filename, sizeof(filename),
		"/dev/%s-intrx%d", thread->int_name, thread->index);
	fd_intrx = open(filename, O_RDWR);
        if (fd_intrx < 0)
		return NULL;

        /* Tx interrupt fd preparing */
        snprintf(filename, sizeof(filename),
		"/dev/%s-inttx%d", thread->int_name, thread->index);
        fd_inttx = open(filename, O_RDWR);
        if (fd_inttx < 0)
                return NULL;

	/* epoll fd preparing */
	fd_ep = epoll_create(EPOLL_MAXEVENTS);
	if(fd_ep < 0){
		perror("failed to make epoll fd");
		return NULL;
	}

	if(epoll_add(fd_ep, fd_intrx) != 0){
		perror("failed to add fd in epoll");
		return NULL;
	}

	if(epoll_add(fd_ep, fd_inttx) != 0){
		perror("failed to add fd in epoll");
		return NULL;
	}

	bulk.count = 0;
	bulk.slot_index = malloc((sizeof int) * thread->budget);
	if(!bulk.slot_index)
		return NULL;
	bulk.size = malloc((sizeof uint32_t) * thread->budget);
	if(!bulk.size)
		return NULL;

	ixgbe_rx_alloc(thread->rx_ring, thread->buf,
		ixgbe_desc_unused(thread->rx_ring));

	rx_qmask = 1 << thread->index;
	tx_qmask = 1 << (thread->index + thread->num_threads);

	while(1){
		num_fd = epoll_wait(fd_ep, events, EPOLL_MAXEVENTS, -1);
		if(num_fd <= 0){
			perror("epoll_wait");
			break;
		}

		for(i = 0; i < num_fd; i++){
			if(events[i].data.fd == fd_intrx){
				/* Rx descripter cleaning */

				ixgbe_rx_clean(thread->rx_ring, thread->buf,
					&bulk, thread->budget);
				ixgbe_irq_enable_queues(ih, rx_qmask);

				ixgbe_tx_xmit(thread->rx_ring, thread->buf, &bulk);

			}else if(events[i].data.fd == fd_inttx){
				/* Tx descripter cleaning */

				ixgbe_tx_clean(thread->tx_ring, thread->buf,
					thread->budget);
				ixgbe_irq_enable_queues(ih, tx_qmask);

			}
		}
	}

	return NULL;
}

void ixgbe_rx_alloc(struct ixgbe_ring *rx_ring, ixgbe_buf *buf,
	u16 max_allocation)
{
	unsigned int total_allocated = 0;

        /* nothing to do */
        if (!max_allocation)
                return;

        do{
		union ixgbe_adv_rx_desc *rx_desc;
		uint16_t next_to_use;
		dma_addr_t addr_dma;
		int slot_index;

		rx_desc = IXGBE_RX_DESC(rx_ring, rx_ring->next_to_use);

		slot_index = ixgbe_slot_assign(buf);
		if(slot_index < 0)
			break;

		ixgbe_slot_attach(ring, rx_ring->next_to_use, slot_index);
		addr_dma = ixgbe_slot_addr_dma(buf, slot_index);

                rx_desc->read.pkt_addr = cpu_to_le64(addr_dma);
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

static void ixgbe_tx_xmit(struct ixgbe_ring *tx_ring,
	struct ixgbe_buf *buf, struct ixgbe_bulk *bulk)
{
        uint32_t cmd_type;
	unsigned int total_xmit = 0;

	/* set type for advanced descriptor with frame checksum insertion */
	cmd_type = IXGBE_ADVTXD_DTYP_DATA |
			IXGBE_ADVTXD_DCMD_DEXT |
			IXGBE_ADVTXD_DCMD_IFCS;

	do{
		union ixgbe_adv_tx_desc *tx_desc;
		uint16_t next_to_use;
		int slot_index;
		uint32_t size;
		uint32_t olinfo_status = 0;

		tx_desc = IXGBE_TX_DESC(tx_ring, tx_ring->next_to_use);

		slot_index = bulk->slot_index[i];
		size = bulk->size[i];

		if(unlikely(size > IXGBE_MAX_DATA_PER_TXD))
			continue;

		ixgbe_slot_attach(tx_ring, tx_ring->next_to_use, slot_index);

		addr_dma = ixgbe_slot_addr_dma(buf, slot_index);
		tx_desc->read.buffer_addr = cpu_to_le64(addr_dma);

		cmd_type |= size | IXGBE_TXD_CMD_EOP | IXGBE_TXD_CMD_RS;
		tx_desc->read.cmd_type_len = cpu_to_le32(cmd_type);

		tx_desc->read.olinfo_status = cpu_to_le32(olinfo_status);

		next_to_use = tx_ring->next_to_use + 1;
		tx_ring->next_to_use =
			(next_to_use < tx_ring->count) ? next_to_use : 0;

		total_xmit++;
	}while(likely(total_xmit < bulk->count));

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

        return;
}

static int ixgbe_rx_clean(struct ixgbe_ring *rx_ring, struct ixgbe_buf *buf,
	struct ixgbe_bulk *bulk, int budget)
{
        unsigned int total_rx_packets = 0;
	uint16_t cleaned_count = ixgbe_desc_unused(rx_ring);

        do{
                union ixgbe_adv_rx_desc *rx_desc;
		uint16_t next_to_clean;
		int slot_index;
		//void *packet;

                rx_desc = IXGBE_RX_DESC(rx_ring, rx_ring->next_to_clean);

		if (!ixgbe_test_staterr(rx_desc, IXGBE_RXD_STAT_DD))
			break;

                /*
                 * This memory barrier is needed to keep us from reading
                 * any other fields out of the rx_desc until we know the
                 * RXD_STAT_DD bit is set
                 */
                rmb();

		/* retrieve a buffer address from the ring */
		slot_index = ixgbe_slot_detach(rx_ring, rx_ring->next_to_clean);
		bulk->slot_index[total_rx_packets] = slot_index;
		bulk->size[total_rx_packets] =
			le16_to_cpu(rx_desc->wb.upper.length);

		//packet = ixgbe_slot_addr_virt(buf, slot_index);

		/* XXX: Should we prefetch the packet buffer ? */

		/*
		 * Confirm: We have not to check IXGBE_RXD_STAT_EOP here
		 * because we have skipped to enable(= disabled) hardware RSC.
		 */

		/* XXX: ERR_MASK will only have valid bits if EOP set ? */
                if (unlikely(ixgbe_test_staterr(rx_desc,
                        IXGBE_RXDADV_ERR_FRAME_ERR_MASK))) {
			printf("frame error detected\n");
                }

                cleaned_count++;
		next_to_clean = rx_ring->next_to_clean + 1;
		rx_ring->next_to_clean = 
			(next_to_clean < rx_ring->count) ? next_to_clean : 0;

		/* XXX: Should we prefetch the next_to_clean desc ? */

                total_rx_packets++;
        }while(likely(total_rx_packets < budget));

        if (cleaned_count)
                ixgbe_rx_alloc(rx_ring, buf, cleaned_count);

	bulk->count = total_rx_packets;

        return total_rx_packets;
}

static int ixgbe_tx_clean(struct ixgbe_ring *tx_ring, struct ixgbe_buf *buf,
	int budget)
{
        unsigned int total_tx_packets = 0;

	do {
		union ixgbe_adv_tx_desc *tx_desc;
		uint16_t next_to_clean;
		int slot_index;

		tx_desc = IXGBE_TX_DESC(tx_ring, tx_ring->next_to_clean);

		if (!(tx_desc->wb.status & cpu_to_le32(IXGBE_TXD_STAT_DD)))
			break;

		/* Release unused buffer */
		slot_index = ixgbe_slot_detach(tx_ring, tx_ring->next_to_clean);
		ixgbe_slot_release(buf, slot_index);

		next_to_clean = tx_ring->next_to_clean + 1;
		tx_ring->next_to_clean =
			(next_to_clean < tx_ring->count) ? next_to_clean : 0;

		total_tx_packets++;
	}while(likely(total_tx_packets < budget));

        return total_tx_packets;
}

static inline int ixgbe_slot_assign(struct ixgbe_buf *buf)
{
	int slot_index = -1;

	if(!buf->free_count)
		goto out;

	slot_index = buf->free_index[buf->free_count - 1];
#ifdef DEBUG
	if(slot_index < 0)
		printf("BUG: assigned slot index is invalid\n");
	buf->free_index[buf->free_count - 1] = -1;
#endif
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
#ifdef DEBUG
	if(slot_index < 0)
		printf("BUG: retrieved slot index is invalid\n");
	ring->slot_index[desc_index] = -1;
#endif
	return slot_index;
}

static inline void ixgbe_slot_release(struct ixgbe_buf *buf,
	int slot_index)
{
	buf->free_index[buf->free_count] = slot_index;
	buf->free_count++;

	return;
}

static inline uint64_t ixgbe_slot_addr_dma(struct ixgbe_buf *buf,
        int slot_index)
{
	uint64_t addr_dma;

	addr_dma = buf->addr_dma + (buf->buf_size * slot_index);
	return addr_dma;
}

static inline void *ixgbe_slot_addr_virt(struct ixgbe_buf *buf,
	uint16_t slot_index)
{
	void *addr_virtual;

        addr_virtual = buf->addr_virtual + (buf->buf_size * slot_index);
        return addr_virtual;
}

static int epoll_add(int fd_ep, int fd)
{
	struct epoll_event event;

	memset(&event, 0, sizeof(struct epoll_event));
	event.events = EPOLLIN;
	event.data.fd = fd;
	return epoll_ctl(fd_ep, EPOLL_CTL_ADD, fd, &event);
}

