#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>
#include <stdint.h>

#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/user.h>
#include <sys/types.h>

#include <net/ethernet.h>

#include "main.h"

static int mtu = 1518;
static int buf_count = 1024;

int main(int argc, char **argv)
{
	struct ixgbe_handle *ih;
	int ret;

	ih = ixgbe_open();
	if(!ih)
		return -1;

	ret = ixgbe_alloc_descring(ih, IXGBE_DEFAULT_RXD, IXGBE_DEFAULT_TXD);
	if(ret < 0)
		printf("error while ixgbe_alloc_descring\n");

	ret = ixgbe_alloc_buf(ih, mtu, buf_count);
	if(ret < 0)
		printf("error while ixgbe_alloc_buf\n");

	sleep(30);

	ixgbe_close(ih);

	return 0;
}

int ixgbe_alloc_descring(struct ixgbe_handle *ih,
	uint32_t num_rx_desc, uint32_t num_tx_desc)
{
	uint64_t offset = ih->mmapped_offset;
	int ret, i;

	ih->rx_ring = malloc(sizeof(struct ixgbe_ring) * ih->num_queues);
	ih->tx_ring = malloc(sizeof(struct ixgbe_ring) * ih->num_queues);

	/* Rx descripter ring allocation */
	for(i = 0; i < ih->num_queues; i++){
		uint32_t size;
		uint64_t paddr;

		size = sizeof(struct ixgbe_legacy_rx_desc) * num_rx_desc;
		size = ALIGN(size, 4096);
		ret = ixgbe_malloc(ih, &offset, &paddr, size, 0);
		if(ret < 0)
			return -1;

		ih->rx_ring[i].paddr = paddr;
		ih->rx_ring[i].vaddr = mmap(NULL, size,
				PROT_READ | PROT_WRITE, MAP_SHARED, ih->fd, offset);
		if(ih->rx_ring[i].vaddr == MAP_FAILED)
			return -1;

		ih->rx_ring[i].count = num_rx_desc;

		offset += size;
	}

	/* Tx descripter ring allocation */
	for(i = 0; i < ih->num_queues; i++){
		uint32_t size;
		uint64_t paddr;

		size = sizeof(struct ixgbe_legacy_tx_desc) * num_tx_desc;
		size = ALIGN(size, 4096);
		ret = ixgbe_malloc(ih, &offset, &paddr, size, 0);
		if(ret < 0)
			return -1;

		ih->tx_ring[i].paddr = paddr;
		ih->tx_ring[i].vaddr = mmap(NULL, size,
			PROT_READ | PROT_WRITE, MAP_SHARED, ih->fd, offset);
		if(ih->tx_ring[i].vaddr == MAP_FAILED)
			return -1;

		ih->tx_ring[i].count = num_tx_desc;

		offset += size;
        }

	ih->mmapped_offset = offset;
	return 0;
}

int ixgbe_alloc_buf(struct ixgbe_handle *ih, uint32_t mtu, uint32_t count)
{
	uint64_t offset = ih->mmapped_offset;
	int ret, i;

	ih->buf = malloc(sizeof(struct ixgbe_buf) * ih->num_queues);

	for(i = 0; i < ih->num_queues; i++){
		uint64_t paddr;
		uint32_t size = mtu * count;

		size = ALIGN(size, 4094);
		ret = ixgbe_malloc(ih, &offset, &paddr, size, 0);
		if(ret < 0)
			return -1;

		ih->buf[i].paddr = paddr;
		ih->buf[i].vaddr = mmap(NULL, size,
			PROT_READ | PROT_WRITE, MAP_SHARED, ih->fd, offset);
		if(ih->buf[i].vaddr == MAP_FAILED)
			return -1;

		ih->buf[i].mtu = mtu;
		ih->buf[i].count = count;

		offset += size;
	}

	ih->mmapped_offset = offset;
	return 0;
}

int ixgbe_malloc(struct ixgbe_handle *ih,
	uint64_t *offset, uint64_t *paddr, uint32_t size, uint16_t numa_node)
{
	struct uio_ixgbe_malloc_req req_malloc;

	
	req_malloc.mmap_offset = *offset;
	req_malloc.physical_addr = 0;
	req_malloc.size = size;
	req_malloc.numa_node = numa_node;
	req_malloc.cache = IXGBE_DMA_CACHE_DISABLE;

	if(ioctl(ih->fd, UIO_IXGBE_MALLOC, (unsigned long)&req_malloc) < 0)
		return -1;

	*offset = req_malloc.mmap_offset;
	*paddr = req_malloc.physical_addr;
	return 0;
}

struct ixgbe_handle *ixgbe_open()
{
	struct uio_ixgbe_info_req req_info;
	struct uio_ixgbe_up_req req_up;
	struct ixgbe_handle *ih;
	int err;

	ih = malloc(sizeof(struct ixgbe_handle));
	if (!ih)
		return NULL;
	memset(ih, 0, sizeof(struct ixgbe_handle));

	ih->fd = open("/dev/ixgbe1", O_RDWR);
	if (ih->fd < 0)
		goto failed;

	/* Get device information */
	memset(&req_info, 0, sizeof(struct uio_ixgbe_info_req));
	if(ioctl(ih->fd, UIO_IXGBE_INFO, (unsigned long)&req_info) < 0)
		goto failed;

	/* UP the device */
	memset(&req_up, 0, sizeof(struct uio_ixgbe_up_req));
	ih->num_queues = min(req_info.info.max_rx_queues, req_info.info.max_tx_queues);
	req_up.info.num_rx_queues = ih->num_queues;
	req_up.info.num_tx_queues = ih->num_queues;

	if(ioctl(ih->fd, UIO_IXGBE_UP, (unsigned long)&req_up) < 0)
		goto failed;

	ih->info = req_up.info;

	/* Map PCI config register space */
	ih->bar = mmap(NULL, ih->info.mmio_size, PROT_READ | PROT_WRITE, MAP_SHARED, ih->fd, 0);
	if(ih->bar == MAP_FAILED)
		goto failed;

	ih->bar_size = req_up.info.mmio_size;
	ih->mmapped_offset = ih->bar_size;

	return ih;

failed:
	err = errno;
	if (ih->bar)
		munmap(ih->bar, ih->bar_size);
	close(ih->fd);
	free(ih);
	errno = err;
	return NULL;
}

void ixgbe_close(struct ixgbe_handle *ih)
{
	munmap(ih->bar, ih->bar_size);
	close(ih->fd);
	free(ih);
}
