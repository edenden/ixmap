#define ALIGN(x,a)		__ALIGN_MASK(x,(typeof(x))(a)-1)
#define __ALIGN_MASK(x,mask)	(((x)+(mask))&~(mask))

#define FILENAME_SIZE 256

#define min(x, y) ({			\
	typeof(x) _min1 = (x);		\
	typeof(y) _min2 = (y);		\
	(void) (&_min1 == &_min2);		\
	_min1 < _min2 ? _min1 : _min2; })

struct ixgbe_thread {
	pthread_t		tid;
	uint32_t		index;
	char			*int_name;
	struct ixgbe_ring	*rx_ring;
	struct ixgbe_ring	*tx_ring;
	struct ixgbe_buf	*buf;
};

/* MAC and PHY info */
struct uio_ixgbe_info {
	uint32_t	irq;
	uint64_t	mmio_base;
	uint32_t	mmio_size;

	uint16_t	mac_type;
	uint8_t		mac_addr[ETH_ALEN];
	uint16_t	phy_type;

	uint32_t        num_rx_queues;
	uint32_t        num_tx_queues;
	uint32_t        max_rx_queues;
	uint32_t        max_tx_queues;
	uint32_t        max_msix_vectors;
};

struct ixgbe_ring {
	void		*vaddr;
	uint32_t	count;
	uint64_t	paddr;

	uint8_t __iomem	*tail;
	uint16_t	next_to_use;
	uint16_t	next_to_clean;
};

struct ixgbe_buf {
	void		*vaddr;
	uint32_t	mtu_frame;
	uint32_t	buf_size;
	uint32_t	count;
	uint64_t	paddr;
};

struct ixgbe_handle {
 	int			fd;
	void			*bar;
	uint32_t		bar_size;
	uint64_t		mmapped_offset;
	uint32_t		num_queues;

	struct ixgbe_ring	*tx_ring;
	struct ixgbe_ring	*rx_ring;
	struct ixgbe_buf	*buf;

	uint32_t		promisc;
	uint32_t		mtu_frame;
	uint32_t		buf_size;
	struct uio_ixgbe_info info;
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

#define UIO_IXGBE_MALLOC _IOW('U', 208, int)
enum {
	IXGBE_DMA_CACHE_DEFAULT = 0,
	IXGBE_DMA_CACHE_DISABLE,
	IXGBE_DMA_CACHE_WRITECOMBINE
};
struct uio_ixgbe_malloc_req {
        uint64_t mmap_offset;
	uint64_t physical_addr;
        uint32_t size;
        uint16_t numa_node;
        uint16_t cache;
};

#define UIO_IXGBE_MFREE  _IOW('U', 209, int)
struct uio_ixgbe_mfree_req {
        uint64_t mmap_offset;
};

