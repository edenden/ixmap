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
#include <linux/scatterlist.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <linux/dma_remapping.h>

#include <linux/version.h>

#include "ixmap_main.h"
#include "ixmap_type.h"
#include "ixmap_dma.h"

static struct list_head *ixmap_dma_area_whereto(struct ixmap_adapter *adapter,
	unsigned long addr_dma, unsigned long size);
static void ixmap_dma_area_free(struct ixmap_adapter *adapter,
	struct ixmap_dma_area *area);

#if ( LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0) )
static int sg_alloc_table_from_pages(struct sg_table *sgt,
	struct page **pages, unsigned int n_pages,
	unsigned long offset, unsigned long size,
	gfp_t gfp_mask);
#endif /* < 3.6.0 */

uint8_t __iomem *ixmap_dma_map_iobase(struct ixmap_adapter *adapter)
{
	struct list_head *where;
	struct ixmap_dma_area *area;
	unsigned long addr_dma = adapter->iobase;
	uint8_t __iomem *hw_addr;

	hw_addr = ioremap(adapter->iobase, adapter->iolen);
	if (!hw_addr)
		goto err_ioremap;

	where = ixmap_dma_area_whereto(adapter, addr_dma, adapter->iolen);
	if(!where)
		goto err_area_whereto;

	area = kzalloc(sizeof(struct ixmap_dma_area), GFP_KERNEL);
	if (!area)
		goto err_alloc_area;

	atomic_set(&area->refcount, 1);
	area->size = adapter->iolen;
	area->cache = IXGBE_DMA_CACHE_DISABLE;
	area->addr_dma = addr_dma;
	area->direction = DMA_BIDIRECTIONAL;

	list_add(&area->list, where);

	return hw_addr;

err_alloc_area:
err_area_whereto:
	iounmap(hw_addr);
err_ioremap:
	return NULL;
}

dma_addr_t ixmap_dma_map(struct ixmap_adapter *adapter,
		unsigned long addr_virtual, unsigned long size, uint8_t cache)
{
	struct ixmap_dma_area *area;
	struct list_head *where;
	struct pci_dev *pdev = adapter->pdev;
	struct page **pages;
	struct sg_table *sgt;
	struct scatterlist *sg;
	unsigned long user_start, user_end, user_offset;
	unsigned int i, npages;
	int ret;
	dma_addr_t addr_dma;

	user_start = addr_virtual & PAGE_MASK;
	user_end = PAGE_ALIGN(addr_virtual + size);
	user_offset = addr_virtual & ~PAGE_MASK;
	npages = (user_end - user_start) >> PAGE_SHIFT;

	pages = kzalloc(sizeof(struct pages *) * npages, GFP_KERNEL);
	if(!pages)
		goto err_alloc_pages;

	down_read(&current->mm->mmap_sem);
	ret = get_user_pages(current, current->mm,
				user_start, npages, 1, 0, pages, NULL);
	up_read(&current->mm->mmap_sem);
	if(ret < 0)
		goto err_get_user_pages;

	sgt = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if(!sgt)
		goto err_alloc_sgt;

	/*
	 * Workaround for the problem that Intel IOMMU driver seems not to support
	 * mapping of sg_table returned by sg_alloc_table_from_pages() when we use
	 * HugeTLB pages taken from userspace. Is this our BUG?
	 * 
	 * Output when we were in for the problem:
	 * kernel: [ 2892.897731] DRHD: handling fault status reg 3
	 * kernel: [ 2892.897750] DMAR:[DMA Read] Request device [04:00.0] fault addr fde00000
	 * kernel: [ 2892.897751] DMAR:[fault reason 12] non-zero reserved fields in PTE
	 */
	if(intel_iommu_enabled){
		ret = sg_alloc_table(sgt, npages, GFP_KERNEL);
		if(ret < 0)
			goto err_alloc_sgt_from_pages;

		for_each_sg(sgt->sgl, sg, npages, i)
			sg_set_page(sg, pages[i], PAGE_SIZE, 0);
	}else{
		ret = sg_alloc_table_from_pages(sgt, pages, npages,
			user_offset, npages << PAGE_SHIFT, GFP_KERNEL);
		if(ret < 0)
			goto err_alloc_sgt_from_pages;
	}

	ret = dma_map_sg(&pdev->dev, sgt->sgl, sgt->orig_nents,
		DMA_BIDIRECTIONAL);
	if(!ret){
		pr_err("ERR: failed to dma_map_sg\n");
		goto err_dma_map_sg;
	}

	for_each_sg(sgt->sgl, sg, ret, i) {
		if(sg_next(sg) &&
		sg_dma_address(sg) + sg_dma_len(sg) != sg_dma_address(sg_next(sg))){
			pr_err("ERR: non-contiguous dma area\n");
			goto err_dma_map_sg_not_contiguous;
		}
	}

	addr_dma = sg_dma_address(sgt->sgl);
        where = ixmap_dma_area_whereto(adapter, addr_dma, size);
        if (!where)
		goto err_area_whereto;

	area = kzalloc(sizeof(struct ixmap_dma_area), GFP_KERNEL);
	if (!area)
		goto err_alloc_area;

	atomic_set(&area->refcount, 1);
	area->size = size;
	area->cache = IXGBE_DMA_CACHE_DISABLE;
	area->addr_dma = addr_dma;
	area->direction = DMA_BIDIRECTIONAL;

	area->sgt = sgt;
	area->pages = pages;
	area->npages = npages;

	list_add(&area->list, where);
	
	return addr_dma;

err_alloc_area:
err_area_whereto:
err_dma_map_sg_not_contiguous:
	dma_unmap_sg(&pdev->dev, sgt->sgl, sgt->nents, DMA_BIDIRECTIONAL);
err_dma_map_sg:
	sg_free_table(sgt);
err_alloc_sgt_from_pages:
	kfree(sgt);
err_alloc_sgt:
	for(i = 0; i < npages; i++){
		set_page_dirty_lock(pages[i]);
		put_page(pages[i]);
	}
err_get_user_pages:
	kfree(pages);
err_alloc_pages:
	return 0;
}

int ixmap_dma_unmap(struct ixmap_adapter *adapter, unsigned long addr_dma)
{
	struct ixmap_dma_area *area;

	area = ixmap_dma_area_lookup(adapter, addr_dma);
	if (!area)
		return -ENOENT;

	list_del(&area->list);
	ixmap_dma_area_free(adapter, area);

	return 0;
}

void ixmap_dma_unmap_all(struct ixmap_adapter *adapter)
{
	struct ixmap_dma_area *area, *temp;

	list_for_each_entry_safe(area, temp, &adapter->areas, list) {
		list_del(&area->list);
		ixmap_dma_area_free(adapter, area);
	}

	return;
}

struct ixmap_dma_area *ixmap_dma_area_lookup(struct ixmap_adapter *adapter,
	unsigned long addr_dma)
{
	struct ixmap_dma_area *area;

	list_for_each_entry(area, &adapter->areas, list) {
		if (area->addr_dma == addr_dma)
			return area;
	}

	return NULL;
}

static struct list_head *ixmap_dma_area_whereto(struct ixmap_adapter *adapter,
	unsigned long addr_dma, unsigned long size)
{
	unsigned long start_new, end_new;
	unsigned long start_area, end_area;
	struct ixmap_dma_area *area;
	struct list_head *last;

	pr_info("add area: start = %p end = %p size = %lu\n",
		(void *)addr_dma, (void *)(addr_dma + size), size);

	start_new = addr_dma;
	end_new   = start_new + size;
	last  = &adapter->areas;

	list_for_each_entry(area, &adapter->areas, list) {
		start_area = area->addr_dma;
		end_area   = start_area + area->size;

		/* Since the list is sorted we know at this point that
		 * new area goes before this one. */
		if (end_new <= start_area)
			break;

		last = &area->list;

		if ((start_new >= start_area && start_new < end_area) ||
				(end_new > start_area && end_new <= end_area)) {
			/* Found overlap. Set start to the end of the current
			 * area and keep looking. */
			last = NULL;
			break;
		}
	}

	return last;
}

static void ixmap_dma_area_free(struct ixmap_adapter *adapter,
	struct ixmap_dma_area *area)
{
	struct ixmap_hw *hw = adapter->hw;
	struct pci_dev *pdev = adapter->pdev;
	struct page **pages;
	struct sg_table *sgt;
	unsigned int i, npages;

	pr_info("delete area: start = %p end = %p size = %lu\n",
		(void *)area->addr_dma, (void *)(area->addr_dma + area->size), area->size);

	if (atomic_dec_and_test(&area->refcount)){
		if(area->addr_dma == adapter->iobase){
			iounmap(hw->hw_addr);
		}else{
			pages = area->pages;
			sgt = area->sgt;
			npages = area->npages;

			dma_unmap_sg(&pdev->dev, sgt->sgl, sgt->nents,
				area->direction);
			sg_free_table(sgt);
			kfree(sgt);
			for(i = 0; i < npages; i++){
				set_page_dirty_lock(pages[i]);
				put_page(pages[i]);
			}
			kfree(pages);
		}
	}

	kfree(area);
	return;
}

#if ( LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0) )
static int sg_alloc_table_from_pages(struct sg_table *sgt,
	struct page **pages, unsigned int n_pages,
	unsigned long offset, unsigned long size,
	gfp_t gfp_mask)
{
	unsigned int chunks;
	unsigned int i;
	unsigned int cur_page;
	int ret;
	struct scatterlist *s;

	/* compute number of contiguous chunks */
	chunks = 1;
	for (i = 1; i < n_pages; ++i)
		if (page_to_pfn(pages[i]) != page_to_pfn(pages[i - 1]) + 1)
			++chunks;

	ret = sg_alloc_table(sgt, chunks, gfp_mask);
	if (unlikely(ret))
		return ret;

	/* merging chunks and putting them into the scatterlist */
	cur_page = 0;
	for_each_sg(sgt->sgl, s, sgt->orig_nents, i) {
		unsigned long chunk_size;
		unsigned int j;

		/* look for the end of the current chunk */
		for (j = cur_page + 1; j < n_pages; ++j)
			if (page_to_pfn(pages[j]) !=
				page_to_pfn(pages[j - 1]) + 1)
				break;

		chunk_size = ((j - cur_page) << PAGE_SHIFT) - offset;
		sg_set_page(s, pages[cur_page], min(size, chunk_size), offset);
		size -= chunk_size;
		offset = 0;
		cur_page = j;
	}

	return 0;
}
#endif /* < 3.6.0 */
