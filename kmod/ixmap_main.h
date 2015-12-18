#ifndef _IXMAP_MAIN_H
#define _IXMAP_MAIN_H

#include <linux/if_ether.h>
#include <linux/types.h>
#include <asm/page.h>

/* common prefix used by pr_<> macros */
#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define MIN_MSIX_Q_VECTORS	1
#define IXGBE_MAX_RSS_INDICES	16
#define IXGBE_IVAR_ALLOC_VAL    0x80 /* Interrupt Allocation valid */
#define EVENTFD_INCREMENT	1

/* General purpose Interrupt Enable */
#define IXGBE_GPIE_MSIX_MODE	0x00000010 /* MSI-X mode */
#define IXGBE_GPIE_OCD		0x00000020 /* Other Clear Disable */
#define IXGBE_GPIE_EIAME	0x40000000
#define IXGBE_GPIE_PBA_SUPPORT	0x80000000

enum ixmap_irq_type {
	IXMAP_IRQ_RX = 0,
	IXMAP_IRQ_TX,
};

struct ixmap_adapter {
	struct list_head	list;
	struct list_head	areas;
	unsigned int		id;
	uint8_t			up;

	struct miscdevice	miscdev;

	struct semaphore	sem;
	atomic_t		refcount;

	uint64_t		dma_mask;
	struct pci_dev		*pdev;
	unsigned long		iobase;
	unsigned long		iolen;
	struct ixmap_hw		*hw;
	char			eeprom_id[32];

	struct msix_entry	*msix_entries;
	uint32_t		num_q_vectors;
	struct ixmap_irq	**rx_irq;
	struct ixmap_irq	**tx_irq;

	uint16_t		link_speed;
	uint16_t		link_duplex;

	uint32_t		num_rx_queues;
	uint32_t		num_tx_queues;
	uint16_t		num_interrupt_rate;
};

struct ixmap_irq {
	struct eventfd_ctx	*efd_ctx;
	struct msix_entry	*msix_entry;
};

uint16_t ixmap_read_pci_cfg_word(struct ixmap_hw *hw, uint32_t reg);
int ixmap_adapter_inuse(struct ixmap_adapter *adapter);
void ixmap_adapter_get(struct ixmap_adapter *adapter);
void ixmap_adapter_put(struct ixmap_adapter *adapter);
int ixmap_irq_assign(struct ixmap_adapter *adapter, enum ixmap_irq_type type,
	uint32_t queue_idx, int event_fd, uint32_t *vector, uint16_t *entry);
int ixmap_up(struct ixmap_adapter *adapter);
int ixmap_down(struct ixmap_adapter *adapter);
void ixmap_reset(struct ixmap_adapter *adapter);
#endif /* _IXMAP_MAIN_H */

