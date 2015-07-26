#ifndef _IXMAP_H
#define _IXMAP_H

//#define DEBUG

#define ALIGN(x,a)		__ALIGN_MASK(x,(typeof(x))(a)-1)
#define __ALIGN_MASK(x,mask)	(((x)+(mask))&~(mask))

#define FILENAME_SIZE 256

#define min(x, y) ({				\
	typeof(x) _min1 = (x);			\
	typeof(y) _min2 = (y);			\
	(void) (&_min1 == &_min2);		\
	_min1 < _min2 ? _min1 : _min2; })

#define max(x, y) ({				\
	typeof(x) _max1 = (x);			\
	typeof(y) _max2 = (y);			\
	(void) (&_max1 == &_max2);		\
	_max1 > _max2 ? _max1 : _max2; })

#define CONFIG_X86_L1_CACHE_SHIFT \
				(6)
#define L1_CACHE_SHIFT		(CONFIG_X86_L1_CACHE_SHIFT)
#define L1_CACHE_BYTES		(1 << L1_CACHE_SHIFT)

#ifdef DEBUG
#define ixmap_print(args...) printf("ixgbe: " args)
#else
#define ixmap_print(args...)
#endif

#define DMA_64BIT_MASK		0xffffffffffffffffULL
#define DMA_BIT_MASK(n)		(((n) == 64) ? \
				DMA_64BIT_MASK : ((1ULL<<(n))-1))

/* General Registers */
#define IXGBE_STATUS		0x00008

/* Interrupt Registers */
#define IXGBE_EIMS		0x00880
#define IXGBE_EIMS_EX(_i)	(0x00AA0 + (_i) * 4)

#define IXGBE_EICR_RTX_QUEUE	0x0000FFFF /* RTx Queue Interrupt */
#define IXGBE_EICR_LSC		0x00100000 /* Link Status Change */
#define IXGBE_EICR_TCP_TIMER	0x40000000 /* TCP Timer */
#define IXGBE_EICR_OTHER	0x80000000 /* Interrupt Cause Active */
#define IXGBE_EIMS_RTX_QUEUE	IXGBE_EICR_RTX_QUEUE /* RTx Queue Interrupt */
#define IXGBE_EIMS_LSC		IXGBE_EICR_LSC /* Link Status Change */
#define IXGBE_EIMS_TCP_TIMER	IXGBE_EICR_TCP_TIMER /* TCP Timer */
#define IXGBE_EIMS_OTHER	IXGBE_EICR_OTHER /* INT Cause Active */
#define IXGBE_EIMS_ENABLE_MASK ( \
				IXGBE_EIMS_RTX_QUEUE    | \
				IXGBE_EIMS_LSC          | \
				IXGBE_EIMS_TCP_TIMER    | \
				IXGBE_EIMS_OTHER)

struct ixmap_ring {
	void		*addr_virtual;
	unsigned long	addr_dma;

	uint8_t		*tail;
	uint16_t	next_to_use;
	uint16_t	next_to_clean;
	int32_t		*slot_index;
};

struct ixmap_buf {
	void		*addr_virtual;
	unsigned long	*addr_dma;
	uint32_t	buf_size;
	uint32_t	count;

	uint32_t	free_count;
	int32_t		*free_index;
};

struct ixmap_bulk {
	uint16_t	count;
	uint16_t	max_count;
	int32_t		*slot_index;
	uint32_t	*slot_size;
};

struct ixmap_handle {
 	int			fd;
	void			*bar;
	unsigned long		bar_size;

	char			*interface_name;
	struct ixmap_ring	*tx_ring;
	struct ixmap_ring	*rx_ring;
	struct ixmap_buf	*buf;

	void			*addr_virtual;
	unsigned long		addr_dma;
	uint32_t		num_tx_desc;
	uint32_t		num_rx_desc;
	uint32_t		budget;

	uint32_t		num_queues;
	uint16_t		num_interrupt_rate;
	uint32_t		promisc;
	uint32_t		mtu_frame;
	uint32_t		buf_size;
	uint8_t			mac_addr[ETH_ALEN];
};

struct ixmap_irqdev_handle {
	int			fd;
	uint32_t		port_index;
	uint64_t		qmask;
};

struct ixmap_port {
	char			*interface_name;
	void			*irqreg[2];
	struct ixmap_ring	*rx_ring;
	struct ixmap_ring	*tx_ring;
	uint32_t		mtu_frame;
	uint32_t		num_tx_desc;
	uint32_t		num_rx_desc;
	uint32_t		num_queues;
	uint32_t		budget;
	uint8_t			mac_addr[ETH_ALEN];

	unsigned long		count_rx_alloc_failed;
	unsigned long		count_rx_clean_total;
	unsigned long		count_tx_xmit_failed;
	unsigned long		count_tx_clean_total;
};

struct ixmap_instance {
	struct ixmap_port 	*ports;
};

enum {
	IXGBE_DMA_CACHE_DEFAULT = 0,
	IXGBE_DMA_CACHE_DISABLE,
	IXGBE_DMA_CACHE_WRITECOMBINE
};

enum ixmap_irq_direction {
	IXMAP_IRQ_RX = 0,
	IXMAP_IRQ_TX,
};

/* Receive Descriptor - Advanced */
union ixmap_adv_rx_desc {
	struct {
		uint64_t pkt_addr; /* Packet buffer address */
		uint64_t hdr_addr; /* Header buffer address */
	} read;
	struct {
		struct {
			union {
				uint32_t data;
				struct {
					uint16_t pkt_info; /* RSS, Pkt type */
					uint16_t hdr_info; /* Splithdr, hdrlen */
				} hs_rss;
			} lo_dword;
			union {
				uint32_t rss; /* RSS Hash */
				struct {
					uint16_t ip_id; /* IP id */
					uint16_t csum; /* Packet Checksum */
				} csum_ip;
			} hi_dword;
		} lower;
		struct {
			uint32_t status_error; /* ext status/error */
			uint16_t length; /* Packet length */
			uint16_t vlan; /* VLAN tag */
		} upper;
	} wb;  /* writeback */
};

/* Transmit Descriptor - Advanced */
union ixmap_adv_tx_desc {
	struct {
		uint64_t buffer_addr; /* Address of descriptor's data buf */
		uint32_t cmd_type_len;
		uint32_t olinfo_status;
	} read;
	struct {
		uint64_t rsvd; /* Reserved */
		uint32_t nxtseq_seed;
		uint32_t status;
	} wb;
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

inline uint32_t ixmap_readl(const volatile void *addr);
inline void ixmap_writel(uint32_t b, volatile void *addr);
inline uint32_t ixmap_read_reg(struct ixmap_handle *ih, uint32_t reg);
inline void ixmap_write_reg(struct ixmap_handle *ih, uint32_t reg, uint32_t value);
inline void ixmap_write_flush(struct ixmap_handle *ih);

#endif /* _IXMAP_H */
