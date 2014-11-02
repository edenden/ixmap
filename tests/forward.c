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
	uint64_t qmask;

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

	ixgbe_alloc_rx_buffers(thread->rx_ring, thread->buf,
		ixgbe_desc_unused(thread->rx_ring));
	qmask = 1 << thread->index;

	while(1){
		num_fd = epoll_wait(fd_ep, events, EPOLL_MAXEVENTS, -1);
		if(num_fd <= 0){
			perror("epoll_wait");
			break;
		}

		for(i = 0; i < num_fd; i++){
			if(events[i].data.fd == fd_intrx){
				/* Rx descripter cleaning */

				ixgbe_clean_rx_irq(thread->rx_ring, thread->buf, budget);
				ixgbe_irq_enable_queues(ih, qmask);

			}else if(events[i].data.fd == fd_inttx){
				/* Tx descripter cleaning */

				ixgbe_clean_tx_irq(thread->tx_ring, thread->buf, budget);
				ixgbe_irq_enable_queues(ih, qmask);

			}
		}
	}

	return NULL;
}

static void ixgbe_irq_enable_queues(struct ixgbe_handle *ih, uint64_t qmask)
{
        u32 mask;

	mask = (qmask & 0xFFFFFFFF);
	if (mask)
		IXGBE_WRITE_REG(hw, IXGBE_EIMS_EX(0), mask);
	mask = (qmask >> 32);
	if (mask)
		IXGBE_WRITE_REG(hw, IXGBE_EIMS_EX(1), mask);
	return;
}

void ixgbe_alloc_rx_buffers(struct ixgbe_ring *rx_ring, ixgbe_buf *buf,
	u16 max_allocation)
{
	unsigned int total_allocated = 0;
	uint16_t next_to_use;
	dma_addr_t addr_dma;

        /* nothing to do */
        if (!max_allocation)
                return;

        do {
		union ixgbe_adv_rx_desc *rx_desc;
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
        } while (total_allocated < max_allocation);

	if(total_allocated){
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

static int ixgbe_clean_rx_irq(struct ixgbe_ring *rx_ring, struct ixgbe_buf *buf,
	int budget)
{
        unsigned int total_rx_packets = 0;
	uint16_t next_to_clean;
	uint16_t cleaned_count = ixgbe_desc_unused(rx_ring);

        do {
                union ixgbe_adv_rx_desc *rx_desc;
		int slot_index;
		void *packet;

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
		packet = ixgbe_slot_addr_virt(buf, slot_index);

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
        } while (likely(total_rx_packets < budget));

        if (cleaned_count)
                ixgbe_alloc_rx_buffers(rx_ring, buf, cleaned_count);

        return total_rx_packets;
}

static int ixgbe_clean_tx_irq(struct ixgbe_ring *tx_ring, struct ixgbe_buf *buf,
	int budget)
{
        unsigned int total_tx_packets = 0;
	uint16_t next_to_clean;

	do {
		union ixgbe_adv_tx_desc *tx_desc;
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
	} while (likely(total_tx_packets < budget));

        return total_tx_packets;
}

/* TBD: Revise to reduce overhead of slot-assigning with O(1) method */
static inline int ixgbe_slot_assign(struct ixgbe_buf *buf)
{
	uint16_t next_to_use = buf->next_to_use;
	int slot_index = -1;
	int assigned = 0;

	do{
		if(!(buf->flag[next_to_use] & IXGBE_SLOT_USED)){
			slot_index = next_to_use;
			buf->flag[next_to_use] |= IXGBE_SLOT_USED;
			assigned = 1;
		}

		next_to_use++;
		next_to_use =
			(next_to_use < buf->count) ? next_to_use : 0;
	}while(next_to_use != buf->next_to_use && !assigned);

	if(assigned)
		buf->next_to_use = next_to_use;

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
	ring->slot_index[desc_index] = -1;

	return slot_index;
}

static inline void ixgbe_slot_release(struct ixgbe_buf *buf,
	int slot_index)
{
	buf->flag[slot_index] &= ~IXGBE_SLOT_USED;
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
	void *slot;

	if(unlikely(!(buf->flag[slot_index] & IXGBE_SLOT_USED))){
		printf("slot unassigned\n")
		return NULL;
	}

        slot = buf->addr_virtual + (buf->buf_size * slot_index);
        return slot;
}

static int epoll_add(int fd_ep, int fd)
{
	struct epoll_event event;

	memset(&event, 0, sizeof(struct epoll_event));
	event.events = EPOLLIN;
	event.data.fd = fd;
	return epoll_ctl(fd_ep, EPOLL_CTL_ADD, fd, &event);
}

