#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/sysctl.h>
#include <linux/wait.h>
#include <linux/miscdevice.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/file.h>
#include <asm/io.h>

#include "ixgbe_uio.h"
#include "ixgbe_type.h"
#include "ixgbe_dma.h"

static struct list_head *ixgbe_dma_area_whereto(struct uio_ixgbe_udapter *ud,
				uint64_t *offset, unsigned int size);
static struct ixgbe_dma_area *ixgbe_dma_area_alloc(struct uio_ixgbe_udapter *ud,
				unsigned int size, unsigned int numa_node, unsigned int cache);
static void ixgbe_dma_area_free(struct uio_ixgbe_udapter *ud, struct ixgbe_dma_area *area);

int ixgbe_dma_iobase(struct uio_ixgbe_udapter *ud){
	struct ixgbe_hw *hw = ud->hw;
	struct list_head *where;
	struct ixgbe_dma_area *area;
	uint64_t offset = 0;

	where = ixgbe_dma_area_whereto(ud, &offset, ud->iolen);
	if(!where)
		return -EBUSY;

	area = kzalloc(sizeof(struct ixgbe_dma_area), GFP_KERNEL);
	if (!area)
		return -ENOMEM;

	atomic_set(&area->refcount, 1);
	area->vaddr = hw->hw_addr;
	area->paddr = ud->iobase >> PAGE_SHIFT;
	area->size = ud->iolen;
	area->cache = IXGBE_DMA_CACHE_DISABLE;
	area->mmap_offset = offset;

	list_add(&area->list, where);

	return 0;
}

int ixgbe_dma_malloc(struct uio_ixgbe_udapter *ud, struct uio_ixgbe_malloc_req *req){
	struct list_head *where;
	struct ixgbe_dma_area *area;

	where = ixgbe_dma_area_whereto(ud, &req->mmap_offset, req->size);
	if (!where)
		return -EBUSY;

	area = ixgbe_dma_area_alloc(ud, req->size, req->numa_node, req->cache);
	if (!area)
		return -ENOMEM;

	req->physical_addr = area->paddr;

	/* Add to the context */
	area->mmap_offset = req->mmap_offset;
	list_add(&area->list, where);

	return 0;
}

int ixgbe_dma_mfree(struct uio_ixgbe_udapter *ud, unsigned long mmap_offset){
	struct ixgbe_dma_area *area;

	area = ixgbe_dma_area_lookup(ud, mmap_offset);
	if (!area)
		return -ENOENT;

	list_del(&area->list);
	ixgbe_dma_area_free(ud, area);

	return 0;
}

void ixgbe_dma_mfree_all(struct uio_ixgbe_udapter *ud){
	struct ixgbe_dma_area *area, *temp;

	list_for_each_entry_safe(area, temp, &ud->areas, list) {
		list_del(&area->list);
		ixgbe_dma_area_free(ud, area);
	}

	return;
}

struct ixgbe_dma_area *ixgbe_dma_area_lookup(struct uio_ixgbe_udapter *ud, uint64_t offset)
{
	struct ixgbe_dma_area *area;

	IXGBE_DBG("area lookup. offset %llu\n", (unsigned long long) offset);

	list_for_each_entry(area, &ud->areas, list) {
		if (area->mmap_offset == offset)
			return area;
	}

	return NULL;
}

static struct list_head *ixgbe_dma_area_whereto(struct uio_ixgbe_udapter *ud,
						uint64_t *offset, unsigned int size){
	unsigned long start_new, end_new;
	unsigned long start_area, end_area;
	struct ixgbe_dma_area *area;
	struct list_head *last;

	start_new = *offset;
	end_new   = start_new + size;

	IXGBE_DBG("adding area. context %p start %lu end %lu\n", ud, start_new, end_new);

	last  = &ud->areas;

	list_for_each_entry(area, &ud->areas, list) {
		start_area = area->mmap_offset;
		end_area   = start_area + area->size;

		IXGBE_DBG("checking area. context %p start %lu end %lu\n",
			ud, start_area, end_area);

		/* Since the list is sorted we know at this point that
		 * new area goes before this one. */
		if (end_new <= start_area)
			break;

		last = &area->list;

		if ((start_new >= start_area && start_new < end_area) ||
				(end_new > start_area && end_new <= end_area)) {
			/* Found overlap. Set start to the end of the current
			 * area and keep looking. */
			start_new = end_area;
			end_new   = start_new + size;
			continue;
		}
	}

	*offset = start_new;
	return last;
}

static struct ixgbe_dma_area *ixgbe_dma_area_alloc(struct uio_ixgbe_udapter *ud,
		unsigned int size, unsigned int numa_node, unsigned int cache){
	struct ixgbe_dma_area *area;
	struct pci_dev *pdev = ud->pdev;
	int orig_node = dev_to_node(&pdev->dev);
	gfp_t gfp;

	area = kzalloc(sizeof(struct ixgbe_dma_area), GFP_KERNEL);
	if (!area)
		return NULL;

	gfp = GFP_KERNEL | __GFP_NOWARN;
	if (ud->dma_mask == DMA_BIT_MASK(64)) {
		gfp |= GFP_DMA;
	}else if(ud->dma_mask == DMA_BIT_MASK(32)){
		gfp |= GFP_DMA32;
	}

	atomic_set(&area->refcount, 1);
	area->size = size;
	area->cache = cache;

	set_dev_node(&pdev->dev, numa_node);
	area->vaddr = dma_alloc_coherent(&pdev->dev, size, &area->paddr, gfp);
	set_dev_node(&pdev->dev, orig_node);

	return area;
}

static void ixgbe_dma_area_free(struct uio_ixgbe_udapter *ud, struct ixgbe_dma_area *area){
	struct ixgbe_hw *hw = ud->hw;
	struct pci_dev *pdev = ud->pdev;

	if (atomic_dec_and_test(&area->refcount)){
		if(area->mmap_offset == 0){
			iounmap(hw->hw_addr);
		}else{
			dma_free_coherent(&pdev->dev, area->size, area->vaddr, area->paddr);
		}
	}

	kfree(area);
	return;
}
