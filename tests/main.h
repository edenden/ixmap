#define ALIGN(x,a)		__ALIGN_MASK(x,(typeof(x))(a)-1)
#define __ALIGN_MASK(x,mask)	(((x)+(mask))&~(mask))

#define FILENAME_SIZE 256

#define min(x, y) ({			\
	typeof(x) _min1 = (x);		\
	typeof(y) _min2 = (y);		\
	(void) (&_min1 == &_min2);		\
	_min1 < _min2 ? _min1 : _min2; })

/*
 * microsecond values for various ITR rates shifted by 2 to fit itr register
 * with the first 3 bits reserved 0
 */
#define IXGBE_MIN_RSC_ITR	24
#define IXGBE_100K_ITR		40
#define IXGBE_20K_ITR		200
#define IXGBE_16K_ITR		248
#define IXGBE_10K_ITR		400
#define IXGBE_8K_ITR		500

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
	uint32_t        num_rx_queues;
	uint32_t        num_tx_queues;
	uint32_t        max_rx_queues;
	uint32_t        max_tx_queues;
	uint32_t        max_msix_vectors;
};

struct ixgbe_ring {
	void		*addr_virtual;
	uint64_t	addr_dma;
	uint32_t	count;

	uint8_t		*tail;
	uint16_t	next_to_use;
	uint16_t	next_to_clean;
	int		*slot_index;
};

struct ixgbe_buf {
	void		*addr_virtual;
	uint64_t	addr_dma;
	uint32_t	buf_size;
	uint32_t	count;

	uint32_t	free_count;
	int		*free_index;
};

struct ixgbe_bulk {
	uint16_t	count;
	int		*slot_index;
	uint32_t	*size;
};

struct ixgbe_handle {
 	int			fd;
	void			*bar;
	uint32_t		bar_size;
	uint32_t		num_queues;
	uint16_t		num_interrupt_rate;

	struct ixgbe_ring	*tx_ring;
	struct ixgbe_ring	*rx_ring;
	struct ixgbe_buf	*buf;

	uint32_t		promisc;
	uint32_t		mtu_frame;
	uint32_t		buf_size;
	struct uio_ixgbe_info	info;
};

/* Per port parameter each thread takes */
struct ixgbe_port {
	struct ixgbe_handle	*ih;
	char                    *interface_name;
	struct ixgbe_ring       *rx_ring;
	struct ixgbe_ring       *tx_ring;
	uint32_t		mtu_frame;
	int                     budget;
};

/* Per thread parameter each thread takes */
struct ixgbe_thread {
	pthread_t		tid;
	uint32_t		num_threads;
	uint32_t		index;
	uint32_t		num_ports;
	struct ixgbe_buf	*buf;

	struct ixgbe_port 	*ports;
};

enum {
	IXGBE_DMA_CACHE_DEFAULT = 0,
	IXGBE_DMA_CACHE_DISABLE,
	IXGBE_DMA_CACHE_WRITECOMBINE
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

void ixgbe_irq_enable_queues(struct ixgbe_handle *ih, uint64_t qmask);
