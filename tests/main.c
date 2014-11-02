#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <net/ethernet.h>
#include <pthread.h>

#include "main.h"
#include "forward.h"
#include "descring.h"

static int mtu_frame = 1518;
static int buf_count = 1024;
static char *ixgbe_interface = "ixgbe1";
static int huge_page = 2 * 1024 * 1024;
static int budget = 1024;

static int ixgbe_alloc_descring(struct ixgbe_handle *ih,
	uint32_t num_rx_desc, uint32_t num_tx_desc);
static int ixgbe_alloc_buf(struct ixgbe_handle *ih, uint32_t count);
static int ixgbe_malloc(struct ixgbe_handle *ih,
	uint64_t *offset, uint64_t *paddr, uint32_t size, uint16_t numa_node);
static struct ixgbe_handle *ixgbe_open(char *int_name);
static void ixgbe_close(struct ixgbe_handle *ih);

int main(int argc, char **argv)
{
	struct ixgbe_handle *ih;
	struct ixgbe_thread *threads;
	int ret, i;

	ih = ixgbe_open(ixgbe_interface);
	if(!ih){
		perror("ixgbe_open");
		return -1;
	}

	/*
	 * Configuration of frame MTU is supported.
	 * However, MTU=1522 is used by default.
	 * See ixgbe_set_rx_buffer_len().
	 */
	// ih->mtu_frame = mtu_frame;

	ret = ixgbe_alloc_descring(ih, IXGBE_DEFAULT_RXD, IXGBE_DEFAULT_TXD);
	if(ret < 0){
		perror("ixgbe_alloc_descring");
		return -1;
	}

        ixgbe_configure_rx(ih);
        ixgbe_configure_tx(ih);

	ret = ixgbe_alloc_buf(ih, buf_count);
	if(ret < 0){
		perror("ixgbe_alloc_buf");
		return -1;
	}

	threads = malloc(sizeof(struct ixgbe_thread) * ih->num_queues);
	if(!threads){
		perror("malloc");
		return -1;
	}

	for(i = 0; i < ih->num_queues; i++){
		threads[i].index = i;
		threads[i].int_name = ixgbe_interface;
		threads[i].rx_ring = &ih->rx_ring[i];
		threads[i].tx_ring = &ih->tx_ring[i];
		threads[i].buf = &ih->buf[i];
		threads[i].badget = badget;
		if(pthread_create(&threads[i].tid,
			NULL, process_interrupt, &threads[i]) < 0){
			perror("failed to create thread");
			return -1;
		}
	}

	while(1){
		sleep(30);
	}

	/* TBD: release mapped DMA areas */

	ixgbe_close(ih);

	return 0;
}

static int ixgbe_alloc_descring(struct ixgbe_handle *ih,
	uint32_t num_rx_desc, uint32_t num_tx_desc)
{
	int ret, i;

	ih->rx_ring = malloc(sizeof(struct ixgbe_ring) * ih->num_queues);
	ih->tx_ring = malloc(sizeof(struct ixgbe_ring) * ih->num_queues);

	/* Rx descripter ring allocation */
	for(i = 0; i < ih->num_queues; i++){
		uint32_t size;
		uint64_t addr_virtual;
		uint64_t addr_dma;
		int *buffer_index;

		size = sizeof(union ixgbe_adv_rx_desc) * num_rx_desc;
		if(size > huge_page_size){
			printf("rx descripter size is larger than hugetlb\n");
			return -1;
		}

		addr_virtual = mmap(NULL, size,
			PROT_READ | PROT_WRITE, MAP_HUGETLB, 0, 0);
		if(addr_virtual == MAP_FAILED)
			return 1;

		ret = ixgbe_dma_map(ih, addr_virtual, &addr_dma, size);
		if(ret < 0)
			return -1;

		slot_index = malloc(sizeof(int) * num_rx_desc);
		if(!slot_index)
			return -1;
#ifdef DEBUG
		for(int j = 0; j < num_rx_desc; j++)
			slot_index[j] = -1;
#endif

		ih->rx_ring[i].addr_dma = addr_dma;
		ih->rx_ring[i].addr_virtual = addr_virtual;
		ih->rx_ring[i].count = num_rx_desc;

		ih->rx_ring[i].next_to_use = 0;
		ih->rx_ring[i].next_to_clean = 0;
		ih->rx_ring[i].slot_index = slot_index;
	}

	/* Tx descripter ring allocation */
	for(i = 0; i < ih->num_queues; i++){
                uint32_t size;
                uint64_t addr_virtual;
                uint64_t addr_dma;
		uint16_t *slot_index;

                size = sizeof(union ixgbe_adv_tx_desc) * num_tx_desc;
		if(size > huge_page_size){
			printf("tx descripter size is larger than hugetlb\n");
			return -1;
		}

                addr_virtual = mmap(NULL, size,
                        PROT_READ | PROT_WRITE, MAP_HUGETLB, 0, 0);
                if(addr_virtual == MAP_FAILED)
                        return 1;

                ret = ixgbe_dma_map(ih, addr_virtual, &addr_dma, size);
                if(ret < 0)
                        return -1;

                slot_index = malloc(sizeof(int) * num_tx_desc);
                if(!slot_index)
                        return -1;
#ifdef DEBUG
		for(int j = 0; j < num_rx_desc; j++)
			slot_index[j] = -1;
#endif

                ih->tx_ring[i].addr_dma = addr_dma;
                ih->tx_ring[i].addr_virtual = addr_virtual;
                ih->tx_ring[i].count = num_tx_desc;

		ih->tx_ring[i].next_to_use = 0;
		ih->tx_ring[i].next_to_clean = 0;
		ih->tx_ring[i].slot_index = slot_index;
        }

	return 0;
}

static int ixgbe_alloc_buf(struct ixgbe_handle *ih, uint32_t count)
{
	int ret, i;

	ih->buf = malloc(sizeof(struct ixgbe_buf) * ih->num_queues);

	for(i = 0; i < ih->num_queues; i++){
                uint32_t size;
                uint64_t addr_virtual;
                uint64_t addr_dma;
		uint32_t *free_index;
		int j;

		size = ih->bufsize * count;
		if(size > huge_page_size){
			printf("buffer size is larger than hugetlb\n");
			return -1;
		}

                addr_virtual = mmap(NULL, size,
                        PROT_READ | PROT_WRITE, MAP_HUGETLB, 0, 0);
                if(addr_virtual == MAP_FAILED)
                        return 1;

                ret = ixgbe_dma_map(ih, addr_virtual, &addr_dma, size);
                if(ret < 0)
                        return -1;

                free_index = malloc(sizeof(uint32_t) * count);
                if(!free_index)
                        return -1;

                ih->buf[i].addr_dma = addr_dma;
                ih->buf[i].addr_virtual = addr_virtual;
		ih->buf[i].buf_size = ih->buf_size;
                ih->buf[i].mtu_frame = ih->mtu_frame;
                ih->buf[i].count = count;
		ih->buf[i].free_count = 0;
		ih->buf[i].free_index = free_index;

		for(j = 0; j < ih->buf[i].count; j++){
			ih->buf[i].free_index[j] = j;
			ih->buf[i].free_count++;
		}
	}

	return 0;
}

static int ixgbe_dma_map(struct ixgbe_handle *ih,
	uint64_t addr_virtual, uint64_t *addr_dma, uint32_t size)
{
	struct uio_ixgbe_dmamap_req req_dmamap;

	req_dmamap.addr_virtual = addr_virtual;
	req_dmamap.addr_dma = 0;
	req_dmamap.size = size;
	req_dmamap.cache = IXGBE_DMA_CACHE_DISABLE;

	if(ioctl(ih->fd, UIO_IXGBE_DMAMAP, (unsigned long)&req_dmamap) < 0)
		return -1;

	*addr_dma = req_dmamap.addr_dma;
	return 0;
}

static struct ixgbe_handle *ixgbe_open(char *int_name)
{
	struct uio_ixgbe_info_req req_info;
	struct uio_ixgbe_up_req req_up;
	struct ixgbe_handle *ih;
	char filename[FILENAME_SIZE];
	int err;

	ih = malloc(sizeof(struct ixgbe_handle));
	if (!ih)
		return NULL;
	memset(ih, 0, sizeof(struct ixgbe_handle));

	snprintf(filename, sizeof(filename), "/dev/%s", int_name);
	ih->fd = open(filename, O_RDWR);
	if (ih->fd < 0)
		goto failed;

	/* Get device information */
	memset(&req_info, 0, sizeof(struct uio_ixgbe_info_req));
	if(ioctl(ih->fd, UIO_IXGBE_INFO, (unsigned long)&req_info) < 0)
		goto failed;

	/* UP the device */
	memset(&req_up, 0, sizeof(struct uio_ixgbe_up_req));

	ih->num_interrupt_rate = min(IXGBE_8K_ITR, req_info.info.max_interrupt_rate);
	req_up.info.num_interrupt_rate = ih->num_interrupt_rate;

	ih->num_queues = min(req_info.info.max_rx_queues, req_info.info.max_tx_queues);
	req_up.info.num_rx_queues = ih->num_queues;
	req_up.info.num_tx_queues = ih->num_queues;

	if(ioctl(ih->fd, UIO_IXGBE_UP, (unsigned long)&req_up) < 0)
		goto failed;

	ih->info = req_up.info;

	/* Map PCI config register space */
	ih->bar = mmap(NULL, ih->info.mmio_size,
		PROT_READ | PROT_WRITE, MAP_SHARED, ih->fd, 0);
	if(ih->bar == MAP_FAILED)
		goto failed;

	ih->bar_size = req_up.info.mmio_size;
	ih->promisc = 0;

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

static void ixgbe_close(struct ixgbe_handle *ih)
{
	munmap(ih->bar, ih->bar_size);
	close(ih->fd);
	free(ih);
}
