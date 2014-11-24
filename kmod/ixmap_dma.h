uint8_t __iomem *ixmap_dma_map_iobase(struct ixmap_adapter *ud);
dma_addr_t ixmap_dma_map(struct ixmap_adapter *ud,
	unsigned long addr_virtual, unsigned long size, uint8_t cache);
int ixmap_dma_unmap(struct ixmap_adapter *ud, unsigned long addr_dma);
void ixmap_dma_unmap_all(struct ixmap_adapter *ud);
struct ixmap_dma_area *ixmap_dma_area_lookup(struct ixmap_adapter *ud,
	unsigned long addr_dma);

enum {
	IXGBE_DMA_CACHE_DEFAULT = 0,
	IXGBE_DMA_CACHE_DISABLE,
	IXGBE_DMA_CACHE_WRITECOMBINE
};

struct ixmap_dma_area {
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
