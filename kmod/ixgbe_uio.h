#ifndef UIO_IXGBE_H
#define UIO_IXGBE_H

#include <linux/if_ether.h>
#include <linux/types.h>
#include <asm/page.h>

/* common prefix used by pr_<> macros */
#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define DEBUG
#ifdef DEBUG
#define IXGBE_DBG(args...) printk(KERN_DEBUG "uio-ixgbe: " args)
#else
#define IXGBE_DBG(args...)
#endif
#define IXGBE_INFO(args...) printk(KERN_INFO "uio-ixgbe: " args)
#define IXGBE_ERR(args...)  printk(KERN_ERR  "uio-ixgbe: " args)

#define MISCDEV_NAME_SIZE	32
#define IXGBE_10K_ITR		400
#define IXGBE_20K_ITR		200
#define MIN_MSIX_Q_VECTORS	1
#define IXGBE_MAX_RSS_INDICES	16
#define IXGBE_IVAR_ALLOC_VAL	    0x80 /* Interrupt Allocation valid */

/* General purpose Interrupt Enable */
#define IXGBE_GPIE_MSIX_MODE    0x00000010 /* MSI-X mode */
#define IXGBE_GPIE_OCD	  0x00000020 /* Other Clear Disable */
#define IXGBE_GPIE_EIAME	0x40000000
#define IXGBE_GPIE_PBA_SUPPORT  0x80000000

struct uio_ixgbe_udapter {
	struct list_head	list;
	struct list_head	areas;
	unsigned int		id;
	uint8_t			removed;
	uint8_t			up;

	struct miscdevice	miscdev;

	struct list_head	irqdev_rx;
	struct list_head	irqdev_tx;

	struct semaphore	sem;
	atomic_t		refcount;

	uint64_t		dma_mask;
	struct pci_dev		*pdev;
	unsigned long		iobase;
	unsigned long		iolen;
	struct ixgbe_hw		*hw;
	char			eeprom_id[32];
	struct msix_entry	*msix_entries;
	uint32_t		num_q_vectors;

	uint16_t		link_speed;
	uint16_t		link_duplex;

	uint32_t		num_rx_queues;
	uint32_t		num_tx_queues;
	uint16_t		num_interrupt_rate;
};

struct ixgbe_irqdev {
	struct uio_ixgbe_udapter	*ud;
	struct list_head		list;
	struct miscdevice		miscdev;
	struct semaphore		sem;
	atomic_t			refcount;

	struct msix_entry		*msix_entry;
	wait_queue_head_t		read_wait;
	uint32_t			eicr;
};

/* MAC and PHY info */
struct uio_ixgbe_info {
	uint32_t	irq;
	uint64_t	mmio_base;
	uint32_t	mmio_size;

	uint16_t	mac_type;
	uint8_t		mac_addr[ETH_ALEN];
	uint16_t	phy_type;

	uint16_t	max_interrupt_rate;
	uint16_t	num_interrupt_rate;
	uint32_t	num_rx_queues;
	uint32_t	num_tx_queues;
	uint32_t	max_rx_queues;
	uint32_t	max_tx_queues;
	uint32_t	max_msix_vectors;
};

/* Ioctl defines */

#define UIO_IXGBE_INFO       _IOW('E', 201, int)
struct uio_ixgbe_info_req {
	struct uio_ixgbe_info info;
};

#define UIO_IXGBE_UP       _IOW('E', 202, int)
struct uio_ixgbe_up_req {
	struct uio_ixgbe_info info;
};

#define UIO_IXGBE_DOWN      _IOW('E', 203, int)
#define UIO_IXGBE_RESET      _IOW('E', 204, int)
#define UIO_IXGBE_CHECK_LINK _IOW('E', 205, int)
#define UIO_IXGBE_GET_LINK   _IOW('E', 206, int)
#define UIO_IXGBE_SET_LINK   _IOW('E', 207, int)

struct uio_ixgbe_link_req {
	uint16_t  speed;
	uint16_t  duplex;
	uint16_t  flowctl;
	uint16_t  media_type;
	uint32_t  autoneg_advertised;
	uint8_t   autoneg_wait_to_complete;
	uint16_t  flush;  /* Indicates that TX/RX flush is necessary
			   * after link state changed */
};

#define UIO_IXGBE_DMAMAP _IOW('U', 210, int)
struct uio_ixgbe_dmamap_req {
	uint64_t addr_virtual;
	uint64_t addr_dma;
	uint32_t size;
	uint16_t cache;
};

#define UIO_IXGBE_DMAUNMAP  _IOW('U', 211, int)
struct uio_ixgbe_dmaunmap_req {
	uint64_t addr_dma;
};

u16 ixgbe_read_pci_cfg_word(struct ixgbe_hw *hw, u32 reg);

#endif /* IXGBE_IOCTL_H */

