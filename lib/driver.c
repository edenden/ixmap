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

#include "ixmap.h"
#include "driver.h"

static inline uint16_t ixmap_desc_unused(struct ixmap_ring *ring,
	uint16_t num_desc);
static inline uint32_t ixmap_test_staterr(union ixmap_adv_rx_desc *rx_desc,
	const uint32_t stat_err_bits);
static inline void ixmap_write_tail(struct ixmap_ring *ring, uint32_t value);
inline int ixmap_slot_assign(struct ixmap_buf *buf);
static inline void ixmap_slot_attach(struct ixmap_ring *ring,
	uint16_t desc_index, int slot_index);
static inline int ixmap_slot_detach(struct ixmap_ring *ring,
	uint16_t desc_index);
inline void ixmap_slot_release(struct ixmap_buf *buf,
	int slot_index);
static inline unsigned long ixmap_slot_addr_dma(struct ixmap_buf *buf,
	int slot_index, int port_index);
inline void *ixmap_slot_addr_virt(struct ixmap_buf *buf,
	uint16_t slot_index);

static inline uint16_t ixmap_desc_unused(struct ixmap_ring *ring,
	uint16_t num_desc)
{
        uint16_t next_to_clean = ring->next_to_clean;
        uint16_t next_to_use = ring->next_to_use;

	return next_to_clean > next_to_use
		? next_to_clean - next_to_use - 1
		: (num_desc - next_to_use) + next_to_clean - 1;
}

static inline uint32_t ixmap_test_staterr(union ixmap_adv_rx_desc *rx_desc,
	const uint32_t stat_err_bits)
{
	return rx_desc->wb.upper.status_error & htole32(stat_err_bits);
}

static inline void ixmap_write_tail(struct ixmap_ring *ring, uint32_t value)
{
	ixmap_writel(value, ring->tail);
	return;
}

inline void ixmap_irq_unmask_queues(struct ixmap_plane *plane,
	struct ixmap_irqdev_handle *irqh)
{
	struct ixmap_port *port;
	uint32_t mask;

	port = &plane->ports[irqh->port_index];

	mask = (irqh->qmask & 0xFFFFFFFF);
	if (mask)
		ixmap_writel(mask, port->irqreg[0]);
	mask = (irqh->qmask >> 32);
	if (mask)
		ixmap_writel(mask, port->irqreg[1]);

	return;
}

inline unsigned int ixmap_port_index(struct ixmap_irqdev_handle *irqh)
{
	unsigned int port_index;

	port_index = irqh->port_index;
	return port_index;
}

void ixmap_rx_assign(struct ixmap_plane *plane, unsigned int port_index,
	struct ixmap_buf *buf)
{
	struct ixmap_port *port;
	struct ixmap_ring *rx_ring;
	union ixmap_adv_rx_desc *rx_desc;
	unsigned int total_allocated;
	uint16_t max_allocation;

	port = &plane->ports[port_index];
	rx_ring = port->rx_ring;

	max_allocation = ixmap_desc_unused(rx_ring, port->num_rx_desc);
	if (!max_allocation)
		return;

	total_allocated = 0;
	while(likely(total_allocated < max_allocation)){
		uint16_t next_to_use;
		uint64_t addr_dma;
		int slot_index;

		slot_index = ixmap_slot_assign(buf);
		if(slot_index < 0){
			port->count_rx_alloc_failed +=
				(max_allocation - total_allocated);
			break;
		}

		ixmap_slot_attach(rx_ring, rx_ring->next_to_use, slot_index);
		addr_dma = (uint64_t)ixmap_slot_addr_dma(buf,
				slot_index, port_index);

		rx_desc = IXGBE_RX_DESC(rx_ring, rx_ring->next_to_use);

		rx_desc->read.pkt_addr = htole64(addr_dma);
		rx_desc->read.hdr_addr = 0;

		next_to_use = rx_ring->next_to_use + 1;
		rx_ring->next_to_use =
			(next_to_use < port->num_rx_desc) ? next_to_use : 0;

		total_allocated++;
	}

	if(likely(total_allocated)){
		/*
		 * Force memory writes to complete before letting h/w
		 * know there are new descriptors to fetch.  (Only
		 * applicable for weak-ordered memory model archs,
		 * such as IA-64).
		 */
		/* XXX: Do we need this write memory barrier ? */
		wmb();
		ixmap_write_tail(rx_ring, rx_ring->next_to_use);
	}
}

void ixmap_tx_assign(struct ixmap_plane *plane, unsigned int port_index,
	struct ixmap_buf *buf, int slot_index, unsigned int slot_size)
{
	struct ixmap_port *port;
	struct ixmap_ring *tx_ring;
	union ixmap_adv_tx_desc *tx_desc;
	uint16_t unused_count;
	uint32_t tx_flags;
	uint16_t next_to_use;
	uint64_t addr_dma;
	uint32_t cmd_type;
	uint32_t olinfo_status;

	port = &plane->ports[port_index];
	tx_ring = port->tx_ring;

	unused_count = ixmap_desc_unused(tx_ring, port->num_tx_desc);
	if(!unused_count){
		port->count_tx_xmit_failed++;
		ixmap_slot_release(buf, slot_index);
		return;
	}

	if(unlikely(slot_size > IXGBE_MAX_DATA_PER_TXD)){
		port->count_tx_xmit_failed++;
		ixmap_slot_release(buf, slot_index);
		return;
	}

	ixmap_slot_attach(tx_ring, tx_ring->next_to_use, slot_index);
	addr_dma = (uint64_t)ixmap_slot_addr_dma(buf, slot_index, port_index);
	ixmap_print("Tx: packet sending DMAaddr = %p size = %d\n",
		(void *)addr_dma, size);

	/* set type for advanced descriptor with frame checksum insertion */
	tx_desc = IXGBE_TX_DESC(tx_ring, tx_ring->next_to_use);
	tx_flags = IXGBE_ADVTXD_DTYP_DATA | IXGBE_ADVTXD_DCMD_DEXT
		| IXGBE_ADVTXD_DCMD_IFCS;
	cmd_type = slot_size | IXGBE_TXD_CMD_EOP | IXGBE_TXD_CMD_RS | tx_flags;
	olinfo_status = slot_size << IXGBE_ADVTXD_PAYLEN_SHIFT;

	tx_desc->read.buffer_addr = htole64(addr_dma);
	tx_desc->read.cmd_type_len = htole32(cmd_type);
	tx_desc->read.olinfo_status = htole32(olinfo_status);

	next_to_use = tx_ring->next_to_use + 1;
	tx_ring->next_to_use =
		(next_to_use < port->num_tx_desc) ? next_to_use : 0;

	port->tx_suspended++;
	return;
}

void ixmap_tx_xmit(struct ixmap_plane *plane, unsigned int port_index)
{
	struct ixmap_port *port;
	struct ixmap_ring *tx_ring;

	port = &plane->ports[port_index];
	tx_ring = port->tx_ring;

	if(port->tx_suspended){
		/*
		 * Force memory writes to complete before letting h/w know there
		 * are new descriptors to fetch.  (Only applicable for weak-ordered
		 * memory model archs, such as IA-64).
		 *
		 * We also need this memory barrier to make certain all of the
		 * status bits have been updated before next_to_watch is written.
		 */
		wmb();
		ixmap_write_tail(tx_ring, tx_ring->next_to_use);
		port->tx_suspended = 0;
	}

	return;
}

void ixmap_rx_clean(struct ixmap_plane *plane, unsigned int port_index,
	struct ixmap_buf *buf, void *opaque,
	void (*process)(int, unsigned int, unsigned int, void *))
{
	struct ixmap_port *port;
	struct ixmap_ring *rx_ring;
	union ixmap_adv_rx_desc *rx_desc;
	unsigned int total_rx_packets;

	port = &plane->ports[port_index];
	rx_ring = port->rx_ring;

	total_rx_packets = 0;
	while(likely(total_rx_packets < port->rx_budget)){
		uint16_t next_to_clean;
		int slot_index;
		unsigned int slot_size;
		void *slot_buf;

		if(unlikely(rx_ring->next_to_clean == rx_ring->next_to_use)){
			break;
		}

		rx_desc = IXGBE_RX_DESC(rx_ring, rx_ring->next_to_clean);

		if (!ixmap_test_staterr(rx_desc, IXGBE_RXD_STAT_DD)){
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
		if (unlikely(ixmap_test_staterr(rx_desc,
			IXGBE_RXDADV_ERR_FRAME_ERR_MASK))) {
			printf("frame error detected\n");
		}

		/* retrieve a buffer address from the ring */
		slot_index = ixmap_slot_detach(rx_ring, rx_ring->next_to_clean);
		slot_size = le16toh(rx_desc->wb.upper.length);
		slot_buf = ixmap_slot_addr_virt(buf, slot_index);
		prefetchw(slot_buf);
		process(slot_index, slot_size, port_index, opaque);

		ixmap_print("Rx: packet received size = %d\n", slot_size);

		next_to_clean = rx_ring->next_to_clean + 1;
		rx_ring->next_to_clean = 
			(next_to_clean < port->num_rx_desc) ? next_to_clean : 0;

		total_rx_packets++;
	}

	port->count_rx_clean_total += total_rx_packets;
	return;
}

void ixmap_tx_clean(struct ixmap_plane *plane, unsigned int port_index,
	struct ixmap_buf *buf)
{
	struct ixmap_port *port;
	struct ixmap_ring *tx_ring;
	union ixmap_adv_tx_desc *tx_desc;
	unsigned int total_tx_packets;

	port = &plane->ports[port_index];
	tx_ring = port->tx_ring;

	total_tx_packets = 0;
	while(likely(total_tx_packets < port->tx_budget)){
		uint16_t next_to_clean;
		int slot_index;

		if(unlikely(tx_ring->next_to_clean == tx_ring->next_to_use)){
			break;
		}

		tx_desc = IXGBE_TX_DESC(tx_ring, tx_ring->next_to_clean);

		if (!(tx_desc->wb.status & htole32(IXGBE_TXD_STAT_DD)))
			break;

		/* Release unused buffer */
		slot_index = ixmap_slot_detach(tx_ring, tx_ring->next_to_clean);
		ixmap_slot_release(buf, slot_index);

		next_to_clean = tx_ring->next_to_clean + 1;
		tx_ring->next_to_clean =
			(next_to_clean < port->num_tx_desc) ? next_to_clean : 0;

		total_tx_packets++;
	}

	port->count_tx_clean_total += total_tx_packets;
	return;
}

uint8_t *ixmap_macaddr(struct ixmap_plane *plane,
	unsigned int port_index)
{
	return plane->ports[port_index].mac_addr;
}

inline int ixmap_slot_assign(struct ixmap_buf *buf)
{
	int slot_index = -1;

	if(!buf->free_count)
		goto out;

	slot_index = buf->free_index[buf->free_count - 1];
	buf->free_count--;
	
out:
	return slot_index;
}

static inline void ixmap_slot_attach(struct ixmap_ring *ring,
	uint16_t desc_index, int slot_index)
{
	ring->slot_index[desc_index] = slot_index;
	return;
}

static inline int ixmap_slot_detach(struct ixmap_ring *ring,
	uint16_t desc_index)
{
	return ring->slot_index[desc_index];
}

inline void ixmap_slot_release(struct ixmap_buf *buf,
	int slot_index)
{
	buf->free_index[buf->free_count] = slot_index;
	buf->free_count++;

	return;
}

static inline unsigned long ixmap_slot_addr_dma(struct ixmap_buf *buf,
	int slot_index, int port_index)
{
	return buf->addr_dma[port_index] + (buf->buf_size * slot_index);
}

inline void *ixmap_slot_addr_virt(struct ixmap_buf *buf,
	uint16_t slot_index)
{
	return buf->addr_virt + (buf->buf_size * slot_index);
}

inline unsigned int ixmap_slot_size(struct ixmap_buf *buf)
{
	return buf->buf_size;
}

inline unsigned long ixmap_count_rx_alloc_failed(struct ixmap_plane *plane,
	unsigned int port_index)
{
	return plane->ports[port_index].count_rx_alloc_failed;
}

inline unsigned long ixmap_count_rx_clean_total(struct ixmap_plane *plane,
	unsigned int port_index)
{
	return plane->ports[port_index].count_rx_clean_total;
}

inline unsigned long ixmap_count_tx_xmit_failed(struct ixmap_plane *plane,
	unsigned int port_index)
{
	return plane->ports[port_index].count_tx_xmit_failed;
}

inline unsigned long ixmap_count_tx_clean_total(struct ixmap_plane *plane,
	unsigned int port_index)
{
	return plane->ports[port_index].count_tx_clean_total;
}

