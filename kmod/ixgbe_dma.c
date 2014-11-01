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

#include <linux/version.h>

#include "ixgbe_uio.h"
#include "ixgbe_type.h"
#include "ixgbe_dma.h"

static struct list_head *ixgbe_dma_area_whereto(struct uio_ixgbe_udapter *ud,
	uint64_t addr_dma, unsigned int size);
static void ixgbe_dma_area_free(struct uio_ixgbe_udapter *ud,
	struct ixgbe_dma_area *area);

#if ( LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0) )
static int sg_alloc_table_from_pages(struct sg_table *sgt,
        struct page **pages, unsigned int n_pages,
        unsigned long offset, unsigned long size,
        gfp_t gfp_mask);
#endif /* < 3.6.0 */

u8 __iomem *ixgbe_dma_map_iobase(struct uio_ixgbe_udapter *ud)
{
	struct list_head *where;
	struct ixgbe_dma_area *area;
	uint64_t addr_dma = ud->iobase;
	u8 __iomem *hw_addr;

	hw_addr = ioremap(ud->iobase, ud->iolen);
	if (!hw_addr)
		goto err_ioremap;

	where = ixgbe_dma_area_whereto(ud, addr_dma, ud->iolen);
	if(!where)
		goto err_area_whereto;

	area = kzalloc(sizeof(struct ixgbe_dma_area), GFP_KERNEL);
	if (!area)
		goto err_alloc_area;

	atomic_set(&area->refcount, 1);
	area->size = ud->iolen;
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

dma_addr_t ixgbe_dma_map(struct uio_ixgbe_udapter *ud,
		unsigned long addr_virtual, unsigned int size, unsigned int cache)
{
	struct ixgbe_dma_area *area;
	struct list_head *where;
	struct pci_dev *pdev = ud->pdev;
	struct page **pages;
	struct sg_table *sgt;
	uint64_t user_start, user_end;
	unsigned int ret, i, npages, user_offset;
	dma_addr_t addr_dma;
	
	user_start = addr_virtual & PAGE_MASK;
	user_end = PAGE_ALIGN(addr_virtual + size);
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

	user_offset = addr_virtual & ~PAGE_MASK;
	ret = sg_alloc_table_from_pages(sgt, pages, npages,
			user_offset, size, GFP_KERNEL);
	if(ret < 0)
		goto err_alloc_sgt_from_pages;

	ret = dma_map_sg(&pdev->dev, sgt->sgl, sgt->nents, DMA_BIDIRECTIONAL);
	if(!ret)
		goto err_dma_map_sg;

	addr_dma = sgt->sgl[0].dma_address;
        where = ixgbe_dma_area_whereto(ud, addr_dma, size);
        if (!where)
		goto err_area_whereto;

	area = kzalloc(sizeof(struct ixgbe_dma_area), GFP_KERNEL);
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

int ixgbe_dma_unmap(struct uio_ixgbe_udapter *ud, uint64_t addr_dma)
{
	struct ixgbe_dma_area *area;

	area = ixgbe_dma_area_lookup(ud, addr_dma);
	if (!area)
		return -ENOENT;

	list_del(&area->list);
	ixgbe_dma_area_free(ud, area);

	return 0;
}

void ixgbe_dma_unmap_all(struct uio_ixgbe_udapter *ud)
{
	struct ixgbe_dma_area *area, *temp;

	list_for_each_entry_safe(area, temp, &ud->areas, list) {
		list_del(&area->list);
		ixgbe_dma_area_free(ud, area);
	}

	return;
}

struct ixgbe_dma_area *ixgbe_dma_area_lookup(struct uio_ixgbe_udapter *ud,
	uint64_t addr_dma)
{
	struct ixgbe_dma_area *area;

	IXGBE_DBG("area lookup. addr_dma %llu\n", addr_dma);

	list_for_each_entry(area, &ud->areas, list) {
		if (area->addr_dma == addr_dma)
			return area;
	}

	return NULL;
}

static struct list_head *ixgbe_dma_area_whereto(struct uio_ixgbe_udapter *ud,
	uint64_t addr_dma, unsigned int size)
{
	unsigned long start_new, end_new;
	unsigned long start_area, end_area;
	struct ixgbe_dma_area *area;
	struct list_head *last;

	start_new = addr_dma;
	end_new   = start_new + size;

	IXGBE_DBG("adding area. context %p start %lu end %lu\n", ud, start_new, end_new);

	last  = &ud->areas;

	list_for_each_entry(area, &ud->areas, list) {
		start_area = area->addr_dma;
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
			last = NULL;
			break;
		}
	}

	return last;
}

static void ixgbe_dma_area_free(struct uio_ixgbe_udapter *ud,
	struct ixgbe_dma_area *area)
{
	struct ixgbe_hw *hw = ud->hw;
	struct pci_dev *pdev = ud->pdev;
	struct page **pages;
	struct sg_table *sgt;
	unsigned int i, npages;

	if (atomic_dec_and_test(&area->refcount)){
		if(area->addr_dma == ud->iobase){
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
		if (pages[i] != pages[i - 1] + 1)
			++chunks;

	ret = sg_alloc_table(sgt, chunks, gfp_mask);
	if (unlikely(ret))
		return ret;

	/* merging chunks and putting them into the scatterlist */
	cur_page = 0;
	for_each_sg(sgt->sgl, s, sgt->orig_nents, i) {
		unsigned long chunk_size;
		unsigned int j;

		/* looking for the end of the current chunk */
		for (j = cur_page + 1; j < n_pages; ++j)
			if (pages[j] != pages[j - 1] + 1)
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
