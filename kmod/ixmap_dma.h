u8 __iomem *ixgbe_dma_map_iobase(struct uio_ixgbe_udapter *ud);
dma_addr_t ixgbe_dma_map(struct uio_ixgbe_udapter *ud,
	unsigned long addr_virtual, unsigned long size, uint8_t cache);
int ixgbe_dma_unmap(struct uio_ixgbe_udapter *ud, unsigned long addr_dma);
void ixgbe_dma_unmap_all(struct uio_ixgbe_udapter *ud);
struct ixgbe_dma_area *ixgbe_dma_area_lookup(struct uio_ixgbe_udapter *ud,
	unsigned long addr_dma);

enum {
	IXGBE_DMA_CACHE_DEFAULT = 0,
	IXGBE_DMA_CACHE_DISABLE,
	IXGBE_DMA_CACHE_WRITECOMBINE
};

struct ixgbe_dma_area {
	struct list_head	list;
	atomic_t		refcount;
	unsigned long		size;
	uint8_t			cache;
	dma_addr_t		addr_dma;
	enum dma_data_direction direction;

	struct page		**pages;
	struct sg_table		*sgt;
	unsigned int		npages;
};
