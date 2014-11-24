#ifndef _IXMAP_FOPS_H
#define _IXMAP_FOPS_H

#define MISCDEV_NAME_SIZE	32

enum {
	IRQDEV_RX = 0,
	IRQDEV_TX,
};

#define IXMAP_INFO		_IOW('E', 201, int)
/* MAC and PHY info */
struct ixmap_info_req {
	unsigned long		mmio_base;
	unsigned long		mmio_size;

	uint16_t		mac_type;
	uint8_t			mac_addr[ETH_ALEN];
	uint16_t		phy_type;

	uint16_t		max_interrupt_rate;
	uint16_t		num_interrupt_rate;
	uint32_t		num_rx_queues;
	uint32_t		num_tx_queues;
	uint32_t		max_rx_queues;
	uint32_t		max_tx_queues;
	uint32_t		max_msix_vectors;
};

#define IXMAP_UP		_IOW('E', 202, int)
struct ixmap_up_req {
	uint16_t		num_interrupt_rate;
	uint32_t		num_rx_queues;
	uint32_t		num_tx_queues;
};

#define IXMAP_DOWN		_IOW('E', 203, int)
#define IXMAP_RESET		_IOW('E', 204, int)
#define IXMAP_CHECK_LINK	_IOW('E', 205, int)

struct ixmap_link_req {
	uint16_t		speed;
	uint16_t		duplex;
	/*
	 * Indicates that TX/RX flush is necessary
	 * after link state changed
	 */
	uint16_t		flush;
};

#define IXMAP_MAP		_IOW('U', 210, int)
struct ixmap_map_req {
	unsigned long		addr_virtual;
	unsigned long		addr_dma;
	unsigned long		size;
	uint8_t			cache;
};

#define IXMAP_UNMAP		_IOW('U', 211, int)
struct ixmap_unmap_req {
	unsigned long		addr_dma;
};

#define IXMAP_IRQDEV_INFO	_IOW('E', 201, int)
struct ixmap_irqdev_info_req {
	uint32_t		vector;
	uint16_t		entry;
};

int ixmap_miscdev_register(struct ixmap_adapter *adapter);
void ixmap_miscdev_deregister(struct ixmap_adapter *adapter);
int ixmap_irqdev_misc_register(struct ixmap_irqdev *irqdev,
	unsigned int id, int direction, int queue_idx);
void ixmap_irqdev_misc_deregister(struct ixmap_irqdev *irqdev);

#endif /* _IXMAP_FOPS_H */
