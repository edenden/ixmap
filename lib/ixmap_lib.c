#define _GNU_SOURCE
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <endian.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <net/ethernet.h>
#include <signal.h>
#include <pthread.h>

#include "ixmap_lib.h"

void ixmap_irq_enable_queues(struct ixmap_handle *ih, uint64_t qmask)
{
	uint32_t mask;

	mask = (qmask & 0xFFFFFFFF);
	if (mask)
		IXGBE_WRITE_REG(ih, IXGBE_EIMS_EX(0), mask);
	mask = (qmask >> 32);
	if (mask)
		IXGBE_WRITE_REG(ih, IXGBE_EIMS_EX(1), mask);

	return;
}

void ixmap_irq_enable(struct ixmap_handle *ih)
{
	uint32_t mask;

	mask = (IXGBE_EIMS_ENABLE_MASK & ~IXGBE_EIMS_RTX_QUEUE);

	/* XXX: Currently we don't support misc interrupts */
	mask &= ~IXGBE_EIMS_LSC;
	mask &= ~IXGBE_EIMS_TCP_TIMER;
	mask &= ~IXGBE_EIMS_OTHER;

	IXGBE_WRITE_REG(ih, IXGBE_EIMS, mask);

	ixmap_irq_enable_queues(ih, ~0);
	IXGBE_WRITE_FLUSH(ih);

	return;
}

struct ixmap_thread *ixmap_thread_alloc(struct ixmap_handle **ih_list,
	int num_ports, int num_cores, int thread_index, struct ixmap_buf *buf)
{
	struct ixmap_thread *thread;
	int i;

	thread = malloc(sizeof(struct ixmap_thread));
	if(!thread)
		goto err_thread_alloc;

	thread->index = thread_index;
	thread->num_threads = num_cores;
	thread->num_ports = num_ports;
	thread->buf = buf;
	thread->ptid = pthread_self();

	thread->ports = malloc(sizeof(struct ixmap_port) * num_ports);
	if(!thread->ports){
		printf("failed to allocate port for each thread\n");
		goto err_alloc_ports;
	}

	for(i = 0; i < num_ports; i++){
		thread->ports[i].ih = ih_list[i];
		thread->ports[i].interface_name = ih_list[i]->interface_name;
		thread->ports[i].rx_ring = &(ih_list[i]->rx_ring[thread_index]);
		thread->ports[i].tx_ring = &(ih_list[i]->tx_ring[thread_index]);
		thread->ports[i].num_rx_desc = ih_list[i]->num_rx_desc;
		thread->ports[i].num_tx_desc = ih_list[i]->num_tx_desc;
		thread->ports[i].budget = ih_list[i]->budget;
		thread->ports[i].mtu_frame = ih_list[i]->mtu_frame;
		thread->ports[i].count_rx_alloc_failed = 0;
		thread->ports[i].count_rx_clean_total = 0;
		thread->ports[i].count_tx_xmit_failed = 0;
		thread->ports[i].count_tx_clean_total = 0;
	}

	return thread;

err_alloc_ports:
	free(thread);
err_thread_alloc:
	return NULL;
}

void ixmap_thread_release(struct ixmap_thread *thread)
{
	free(thread->ports);
	free(thread);

	return;
}

int ixmap_desc_alloc(struct ixmap_handle *ih,
	uint32_t num_rx_desc, uint32_t num_tx_desc)
{
	int rx_assigned = 0, tx_assigned = 0, i, ret;
	unsigned long size_tx_desc, size_rx_desc;
	void *addr_virtual;
	unsigned long addr_dma;

	ih->rx_ring = malloc(sizeof(struct ixmap_ring) * ih->num_queues);
	if(!ih->rx_ring)
		goto err_alloc_rx_ring;

	ih->tx_ring = malloc(sizeof(struct ixmap_ring) * ih->num_queues);
	if(!ih->tx_ring)
		goto err_alloc_tx_ring;

	size_rx_desc = sizeof(union ixmap_adv_rx_desc) * num_rx_desc;
	size_rx_desc = ALIGN(size_rx_desc, 128); /* needs 128-byte alignment */
	size_tx_desc = sizeof(union ixmap_adv_tx_desc) * num_tx_desc;
	size_tx_desc = ALIGN(size_tx_desc, 128); /* needs 128-byte alignment */

	/*
	 * XXX: We don't support NUMA-aware memory allocation in userspace.
	 * To support, mbind() or set_mempolicy() will be useful.
	 */
	ih->addr_virtual = mmap(NULL,
		ih->num_queues * (size_rx_desc + size_tx_desc),
		PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, 0, 0);
	if(ih->addr_virtual == MAP_FAILED){
		goto err_mmap;
	}

	ret = ixmap_dma_map(ih, ih->addr_virtual, &ih->addr_dma,
		ih->num_queues * (size_rx_desc + size_tx_desc));
	if(ret < 0){
		goto err_dma_map;
	}

	addr_virtual	= ih->addr_virtual;
	addr_dma	= ih->addr_dma;

	/* Rx descripter ring allocation */
	for(i = 0; i < ih->num_queues; i++, rx_assigned++){
		int *slot_index;

		slot_index = malloc(sizeof(int) * num_rx_desc);
		if(!slot_index){
			goto err_rx_assign;
		}

		addr_virtual	+= (i * size_rx_desc);
		addr_dma	+= (i * size_rx_desc);

		ih->rx_ring[i].addr_dma = addr_dma;
		ih->rx_ring[i].addr_virtual = addr_virtual;
		ih->rx_ring[i].next_to_use = 0;
		ih->rx_ring[i].next_to_clean = 0;
		ih->rx_ring[i].slot_index = slot_index;
	}

	/* Tx descripter ring allocation */
	for(i = 0; i < ih->num_queues; i++, tx_assigned++){
		int *slot_index;

		slot_index = malloc(sizeof(int) * num_tx_desc);
		if(!slot_index){
			goto err_tx_assign;
		}

		addr_virtual	+= (i * size_tx_desc);
		addr_dma	+= (i * size_tx_desc);

		ih->tx_ring[i].addr_dma = addr_dma;
		ih->tx_ring[i].addr_virtual = addr_virtual;
		ih->tx_ring[i].next_to_use = 0;
		ih->tx_ring[i].next_to_clean = 0;
		ih->tx_ring[i].slot_index = slot_index;
	}

	ih->num_rx_desc = num_rx_desc;
	ih->num_tx_desc = num_tx_desc;

	return 0;

err_tx_assign:
	for(i = 0; i < tx_assigned; i++){
		free(ih->tx_ring[i].slot_index);
	}
err_rx_assign:
	for(i = 0; i < rx_assigned; i++){
		free(ih->rx_ring[i].slot_index);
	}
	ixmap_dma_unmap(ih, ih->addr_dma);
err_dma_map:
	munmap(ih->addr_virtual,
		ih->num_queues * (size_rx_desc + size_tx_desc));
err_mmap:
	free(ih->tx_ring);
err_alloc_tx_ring:
	free(ih->rx_ring);
err_alloc_rx_ring:

	return -1;
}

void ixmap_desc_release(struct ixmap_handle *ih)
{
	int i, ret;
	unsigned long size_rx_desc, size_tx_desc;

	for(i = 0; i < ih->num_queues; i++){
		free(ih->rx_ring[i].slot_index);
		free(ih->tx_ring[i].slot_index);
	}

	ret = ixmap_dma_unmap(ih, ih->addr_dma);
	if(ret < 0)
		perror("failed to unmap descring");

	size_rx_desc = sizeof(union ixmap_adv_rx_desc) * ih->num_rx_desc;
	size_rx_desc = ALIGN(size_rx_desc, 128);
	size_tx_desc = sizeof(union ixmap_adv_tx_desc) * ih->num_tx_desc;
	size_tx_desc = ALIGN(size_tx_desc, 128);
	munmap(ih->rx_ring[i].addr_virtual,
		ih->num_queues * (size_rx_desc + size_tx_desc));

	free(ih->tx_ring);
	free(ih->rx_ring);

	return;
}

struct ixmap_buf *ixmap_buf_alloc(struct ixmap_handle **ih_list,
	int num_ports, uint32_t count, uint32_t buf_size)
{
	struct ixmap_buf *buf;
	void	*addr_virtual;
	unsigned long addr_dma, size;
	int *free_index;
	int ret, i, mapped_ports = 0;

	buf = malloc(sizeof(struct ixmap_buf));
	if(!buf)
		goto err_alloc_buf;

	buf->addr_dma = malloc(sizeof(unsigned long) * num_ports);
	if(!buf->addr_dma)
		goto err_alloc_buf_addr_dma;

	/*
	 * XXX: Should we add buffer padding for memory interleaving?
	 * DPDK does so in rte_mempool.c/optimize_object_size().
	 */
	size = buf_size * count;

	/*
	 * XXX: We don't support NUMA-aware memory allocation in userspace.
	 * To support, mbind() or set_mempolicy() will be useful.
	 */
	addr_virtual = mmap(NULL, size, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, 0, 0);
	if(addr_virtual == MAP_FAILED)
		goto err_mmap;

	for(i = 0; i < num_ports; i++, mapped_ports++){
		ret = ixmap_dma_map(ih_list[i], addr_virtual, &addr_dma, size);
		if(ret < 0)
			goto err_ixmap_dma_map;

		buf->addr_dma[i] = addr_dma;
	}

	free_index = malloc(sizeof(int) * count);
	if(!free_index)
		goto err_alloc_free_index;

	buf->addr_virtual = addr_virtual;
	buf->buf_size = buf_size;
	buf->count = count;
	buf->free_count = 0;
	buf->free_index = free_index;

	for(i = 0; i < buf->count; i++){
		buf->free_index[i] = i;
		buf->free_count++;
	}

	return buf;

err_alloc_free_index:
err_ixmap_dma_map:
	for(i = 0; i < mapped_ports; i++){
		ixmap_dma_unmap(ih_list[i], buf->addr_dma[i]);
	}
	munmap(addr_virtual, size);
err_mmap:
	free(buf->addr_dma);
err_alloc_buf_addr_dma:
	free(buf);
err_alloc_buf:
	return NULL;
}

void ixmap_buf_release(struct ixmap_handle **ih_list,
	int num_ports, struct ixmap_buf *buf)
{
	int i, ret;
	unsigned long size;

	free(buf->free_index);

	for(i = 0; i < num_ports; i++){
		ret = ixmap_dma_unmap(ih_list[i], buf->addr_dma[i]);
		if(ret < 0)
			perror("failed to unmap buf");
	}

	size = buf->buf_size * buf->count;
	munmap(buf->addr_virtual, size);
	free(buf->addr_dma);
	free(buf);

	return;
}

static int ixmap_dma_map(struct ixmap_handle *ih, void *addr_virtual,
	unsigned long *addr_dma, unsigned long size)
{
	struct uio_ixmap_map_req req_map;

	req_map.addr_virtual = (unsigned long)addr_virtual;
	req_map.addr_dma = 0;
	req_map.size = size;
	req_map.cache = IXGBE_DMA_CACHE_DISABLE;

	if(ioctl(ih->fd, UIO_IXGBE_MAP, (unsigned long)&req_map) < 0)
		return -1;

	*addr_dma = req_map.addr_dma;
	return 0;
}

static int ixmap_dma_unmap(struct ixmap_handle *ih,
	unsigned long addr_dma)
{
	struct uio_ixmap_unmap_req req_unmap;

	req_unmap.addr_dma = addr_dma;

	if(ioctl(ih->fd, UIO_IXGBE_UNMAP, (unsigned long)&req_unmap) < 0)
		return -1;

	return 0;
}

struct ixmap_handle *ixmap_open(char *interface_name,
	uint32_t num_core, uint32_t budget, uint16_t intr_rate)
{
	struct ixmap_handle *ih;
	char filename[FILENAME_SIZE];
	struct uio_ixmap_info_req req_info;
	struct uio_ixmap_up_req req_up;

	ih = malloc(sizeof(struct ixmap_handle));
	if (!ih)
		goto err_alloc_ih;
	memset(ih, 0, sizeof(struct ixmap_handle));

	snprintf(filename, sizeof(filename), "/dev/%s", interface_name);
	ih->fd = open(filename, O_RDWR);
	if (ih->fd < 0)
		goto err_open;

	/* Get device information */
	memset(&req_info, 0, sizeof(struct uio_ixmap_info_req));
	if(ioctl(ih->fd, UIO_IXGBE_INFO, (unsigned long)&req_info) < 0)
		goto err_ioctl_info;

	/* UP the device */
	memset(&req_up, 0, sizeof(struct uio_ixmap_up_req));

	ih->num_interrupt_rate =
		min(intr_rate, req_info.max_interrupt_rate);
	req_up.num_interrupt_rate = ih->num_interrupt_rate;

	ih->num_queues =
		min(req_info.max_rx_queues, req_info.max_tx_queues);
	ih->num_queues = min(num_core, ih->num_queues);
	req_up.num_rx_queues = ih->num_queues;
	req_up.num_tx_queues = ih->num_queues;

	if(ioctl(ih->fd, UIO_IXGBE_UP, (unsigned long)&req_up) < 0)
		goto err_ioctl_up;

	/* Map PCI config register space */
	ih->bar = mmap(NULL, req_info.mmio_size,
		PROT_READ | PROT_WRITE, MAP_SHARED, ih->fd, 0);
	if(ih->bar == MAP_FAILED)
		goto err_mmap;

	ih->bar_size = req_info.mmio_size;
	ih->promisc = 0;
	ih->interface_name = interface_name;
	ih->budget = budget;

	return ih;

err_mmap:
err_ioctl_up:
err_ioctl_info:
	close(ih->fd);
err_open:
	free(ih);
err_alloc_ih:
	return NULL;
}

void ixmap_close(struct ixmap_handle *ih)
{
	munmap(ih->bar, ih->bar_size);
	close(ih->fd);
	free(ih);

	return;
}

struct ixmap_irqdev_handle *ixmap_irqdev_open(struct ixmap_thread *thread,
	unsigned int port_num, enum ixmap_irq_direction)
{
	struct ixmap_irqdev_handle *irqh;
	char filename[FILENAME_SIZE];

	switch(ixmap_irq_direction){
	case IXMAP_IRQ_RX:
		snprintf(filename, sizeof(filename), "/dev/%s-irqrx%d",
			ports[i].interface_name, thread_index);
		break;
	case IXMAP_IRQ_TX:
		snprintf(filename, sizeof(filename), "/dev/%s-irqtx%d",
			ports[i].interface_name, thread_index);
		break;
	case default:
		goto err_invalid_direction;
		break;
	}

	irqh = malloc(sizeof(struct ixmap_irqdev_handle));
	if(!irqh)
		goto err_alloc_handle;

	irqh->fd = open(filename, O_RDWR);
	if(irqh->fd < 0)
		goto err_open;

	return irqh;

err_open:
	free(irqh);
err_alloc_handle:
err_invalid_direction:
	return NULL;
}

void ixmap_irqdev_close(struct ixmap_irqdev_handle *irqh)
{
	close(irqh->fd);
	free(irqh);

	return;
}
