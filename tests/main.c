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
#include <pthread.h>

#include "main.h"
#include "driver.h"
#include "rxinit.h"
#include "txinit.h"

static int buf_count = 1024;
static char *ixgbe_interface0 = "ixgbe1";
static char *ixgbe_interface1 = "ixgbe3";
static int num_ports = 2;
static int huge_page = 2 * 1024 * 1024;
static int budget = 1024;

static void ixgbe_irq_enable_queues(struct ixgbe_handle *ih, uint64_t qmask);
static inline void ixgbe_irq_enable(struct ixgbe_handle *ih);
static int ixgbe_alloc_descring(struct ixgbe_handle *ih,
	uint32_t num_rx_desc, uint32_t num_tx_desc);
static struct ixgbe_buf *ixgbe_alloc_buf(uint32_t count,
	uint32_t buf_size);
static int ixgbe_dma_map(struct ixgbe_handle *ih,
	uint64_t addr_virtual, uint64_t *addr_dma, uint32_t size);
static struct ixgbe_handle *ixgbe_open(char *int_name, int num_core);
static void ixgbe_close(struct ixgbe_handle *ih);

int main(int argc, char **argv)
{
	struct ixgbe_handle **ih_list;
	struct ixgbe_buf **ixgbe_buf_list;
	char **ixgbe_interface_list;
	struct ixgbe_thread *threads;
	uint32_t buf_size = 0;
	int num_cores = 4, ret, i;

	ixgbe_interface_list = malloc(sizeof(char *) * num_ports);
	if(!ixgbe_interface_list)
		return -1;

	ixgbe_interface_list[0] = ixgbe_interface0;
	ixgbe_interface_list[1] = ixgbe_interface1;

	ih_list = malloc(sizeof(struct ixgbe_handle *) * num_ports);
	if(!ih_list)
		return -1;

	for(i = 0; i < num_ports; i++){

		ih_list[i] = ixgbe_open(ixgbe_interface_list[i], num_cores);
		if(!ih_list[i]){
			printf("failed to ixgbe_open count = %d\n", i);
			return -1;
		}

		/*
		 * Configuration of frame MTU is supported.
		 * However, MTU=1522 is used by default.
		 * See ixgbe_set_rx_buffer_len().
		 */
		// ih_list[i]->mtu_frame = mtu_frame;

		ret = ixgbe_alloc_descring(ih_list[i],
			IXGBE_DEFAULT_RXD, IXGBE_DEFAULT_TXD);
		if(ret < 0){
			printf("failed to ixgbe_alloc_descring count = %d\n", i);
			return -1;
		}

		ixgbe_configure_rx(ih_list[i]);
		ixgbe_configure_tx(ih_list[i]);
		ixgbe_irq_enable(ih_list[i]);

		/* calclulate maximum buf_size we should prepare */
		if(ih_list[i]->buf_size > buf_size)
			buf_size = ih_list[i]->buf_size;
	}

	ixgbe_buf_list = malloc(sizeof(struct ixgbe_buf *) * num_cores);
	if(!ixgbe_buf_list){
		perror("malloc");
		return -1;
	}

	threads = malloc(sizeof(struct ixgbe_thread) * num_cores);
	if(!threads){
		perror("malloc");
		return -1;
	}

	for(i = 0; i < num_cores; i++){
		int j;

                ixgbe_buf_list[i] =
			ixgbe_alloc_buf(buf_count, buf_size);
                if(!ixgbe_buf_list[i]){
                        printf("failed to ixgbe_alloc_buf count = %d\n", i);
                        return -1;
                }

		threads[i].index = i;
		threads[i].num_threads = num_cores;
		threads[i].num_ports = num_ports;
		threads[i].buf = ixgbe_buf_list[i];

		threads[i].ports = malloc(sizeof(struct ixgbe_port) * num_ports);
		if(!threads[i].ports){
			printf("failed to allocate port for each thread\n");
			return -1;
		}

		for(j = 0; j < num_ports; j++){
			threads[i].ports[j].interface_name = ixgbe_interface_list[j];
			threads[i].ports[j].rx_ring = &ih_list[j]->rx_ring[i];
			threads[i].ports[j].tx_ring = &ih_list[j]->tx_ring[i];
			threads[i].ports[j].mtu_frame = ih_list[j]->mtu_frame;
			threads[i].ports[j].budget = budget;
		}

		if(pthread_create(&threads[i].tid,
			NULL, process_interrupt, &threads[i]) < 0){
			perror("failed to create thread");
			return -1;
		}
	}

	while(1){
		sleep(30);
	}

	for(i = 0; i < num_cores; i++){
		/* TBD: release mapped DMA areas */
	}

	for(i = 0; i < num_ports; i++){
		ixgbe_close(ih_list[i]);
	}

	return 0;
}

static void ixgbe_irq_enable_queues(struct ixgbe_handle *ih, uint64_t qmask)
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

static inline void ixgbe_irq_enable(struct ixgbe_handle *ih)
{
        uint32_t mask;

	mask = (IXGBE_EIMS_ENABLE_MASK & ~IXGBE_EIMS_RTX_QUEUE);
        IXGBE_WRITE_REG(ih, IXGBE_EIMS, mask);

	ixgbe_irq_enable_queues(ih, ~0);
	IXGBE_WRITE_FLUSH(ih);

	return;
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

static struct ixgbe_buf *ixgbe_alloc_buf(uint32_t count,
	uint32_t buf_size)
{
	struct ixgbe_buf *buf;
	uint32_t size;
	uint64_t addr_virtual;
	uint64_t addr_dma;
	uint32_t *free_index;
	int ret, i;

	buf = malloc(sizeof(struct ixgbe_buf));
	if(!buf){
		return NULL;
	}

	size = buf_size * count;
	if(size > huge_page_size){
		printf("buffer size is larger than hugetlb\n");
		return -1;
	}

	addr_virtual = mmap(NULL, size,
			PROT_READ | PROT_WRITE, MAP_HUGETLB, 0, 0);
	if(addr_virtual == MAP_FAILED)
		return NULL;

	ret = ixgbe_dma_map(ih, addr_virtual, &addr_dma, size);
	if(ret < 0)
		return NULL;

	free_index = malloc(sizeof(uint32_t) * count);
	if(!free_index)
		return NULL;

	buf->addr_dma = addr_dma;
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

static struct ixgbe_handle *ixgbe_open(char *int_name, int num_core)
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
	ih->num_queues = min(num_core, ih->num_queues);
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
