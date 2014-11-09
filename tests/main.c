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
static int huge_page_size = 2 * 1024 * 1024;
static int budget = 1024;

static inline void ixgbe_irq_enable(struct ixgbe_handle *ih);
static int ixgbe_alloc_descring(struct ixgbe_handle *ih,
	uint32_t num_rx_desc, uint32_t num_tx_desc);
static void ixgbe_release_descring(ixgbe_handle *ih);
static struct ixgbe_buf *ixgbe_alloc_buf(struct ixgbe_handle *ih,
	uint32_t count, uint32_t buf_size);
static int ixgbe_dma_unmap(struct ixgbe_handle *ih,
	uint64_t addr_dma);
static int ixgbe_dma_map(struct ixgbe_handle *ih,
	void *addr_virtual, uint64_t *addr_dma, uint32_t size);
static struct ixgbe_handle *ixgbe_open(char *int_name, uint32_t num_core);
static void ixgbe_close(struct ixgbe_handle *ih);
static int ixgbe_set_signal(sigset_t *sigset);

int main(int argc, char **argv)
{
	struct ixgbe_handle **ih_list;
	struct ixgbe_buf **ixgbe_buf_list;
	char **ixgbe_interface_list;
	struct ixgbe_thread *threads;
	uint32_t buf_size = 0;
	uint32_t num_cores = 4;
	uint32_t num_ports = 2;
	sigset_t sigset;
	int ret = 0, i, signal;
	int cores_assigned = 0, ports_assigned = 0;

	ixgbe_interface_list = malloc(sizeof(char *) * num_ports);
	if(!ixgbe_interface_list)
		goto err_interface_list;

	ixgbe_interface_list[0] = ixgbe_interface0;
	ixgbe_interface_list[1] = ixgbe_interface1;

	ih_list = malloc(sizeof(struct ixgbe_handle *) * num_ports);
	if(!ih_list)
		goto err_ih_list;

	for(i = 0; i < num_ports; i++, ports_assigned++){
		ih_list[i] = ixgbe_open(ixgbe_interface_list[i], num_cores);
		if(!ih_list[i]){
			printf("failed to ixgbe_open count = %d\n", i);
			goto err_ixgbe_open;
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
			ixgbe_close(ih_list[i]);
			goto err_ixgbe_alloc_descring;
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
		goto err_ixgbe_buf_list;
	}

	threads = malloc(sizeof(struct ixgbe_thread) * num_cores);
	if(!threads){
		perror("malloc");
		goto err_ixgbe_alloc_threads;
	}

	for(i = 0; i < num_cores; i++, cores_assigned++){
		int j;

		ixgbe_buf_list[i] =
			ixgbe_alloc_buf(ih_list[0], buf_count, buf_size);
		if(!ixgbe_buf_list[i]){
			printf("failed to ixgbe_alloc_buf count = %d\n", i);
			goto err_ixgbe_alloc_buf;
		}

		threads[i].index = i;
		threads[i].num_threads = num_cores;
		threads[i].num_ports = num_ports;
		threads[i].buf = ixgbe_buf_list[i];

		threads[i].ports = malloc(sizeof(struct ixgbe_port) * num_ports);
		if(!threads[i].ports){
			printf("failed to allocate port for each thread\n");
			free(ixgbe_buf_list[i]);
			goto err_ixgbe_alloc_ports;
		}

		for(j = 0; j < num_ports; j++){
			threads[i].ports[j].ih = ih_list[j];
			threads[i].ports[j].interface_name = ixgbe_interface_list[j];
			threads[i].ports[j].rx_ring = &(ih_list[j]->rx_ring[i]);
			threads[i].ports[j].tx_ring = &(ih_list[j]->tx_ring[i]);
			threads[i].ports[j].mtu_frame = ih_list[j]->mtu_frame;
			threads[i].ports[j].budget = budget;
		}

		if(pthread_create(&threads[i].tid,
			NULL, process_interrupt, &threads[i]) < 0){
			perror("failed to create thread");
			free(ixgbe_buf_list[i]);
			free(threads[i].ports);
			goto err_ixgbe_create_threads;
		}
	}

	ret = ixgbe_set_signal(&sigset);
	if(ret != 0)
		goto err_ixgbe_set_signal;

	while(1){
		if(sigwait(&sigset, &signal) == 0){
			break;
		}
	}

err_ixgbe_set_signal:
err_ixgbe_create_threads:
err_ixgbe_alloc_ports:
err_ixgbe_alloc_buf:
	for(i = 0; i < cores_assigned; i++){
		pthread_kill();
		free(threads[i].ports);
		ixgbe_release_buf();
	}
	free(threads);
err_ixgbe_alloc_threads:
	free(ixgbe_buf_list);
err_ixgbe_buf_list:
err_ixgbe_alloc_descring:
err_ixgbe_open:
	for(i = 0; i < ports_assigned; i++){
		ixgbe_release_descring(ih_list[i]);
		ixgbe_close(ih_list[i]);
	}
	free(ih_list);
err_ih_list:
	free(ixgbe_interface_list);
err_interface_list:

	return ret;
}

void ixgbe_irq_enable_queues(struct ixgbe_handle *ih, uint64_t qmask)
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
	int ret, i, rx_assigned = 0, tx_assigned = 0;
	int size_tx_desc, size_rx_desc;

	ih->rx_ring = malloc(sizeof(struct ixgbe_ring) * ih->num_queues);
	if(!ih->rx_ring)
		goto err_alloc_rx_ring;

	ih->tx_ring = malloc(sizeof(struct ixgbe_ring) * ih->num_queues);
	if(!ih->tx_ring)
		goto err_alloc_tx_ring;

	size_rx_desc = sizeof(union ixgbe_adv_rx_desc) * num_rx_desc;
	size_tx_desc = sizeof(union ixgbe_adv_tx_desc) * num_tx_desc;
	if(size_rx_desc > huge_page_size
	|| size_tx_desc > huge_page_size)
		goto err_desc_size;

	/* Rx descripter ring allocation */
	for(i = 0; i < ih->num_queues; i++, rx_assigned++){
		void *addr_virtual;
		uint64_t addr_dma;
		int *slot_index;

		addr_virtual = mmap(NULL, size_rx_desc,
			PROT_READ | PROT_WRITE, MAP_HUGETLB, 0, 0);
		if(addr_virtual == MAP_FAILED){
			goto err_rx_mmap;
		}

		ret = ixgbe_dma_map(ih,
			addr_virtual, &addr_dma, size_rx_desc);
		if(ret < 0){
			munmap(addr_virtual, size_rx_desc);
			goto err_rx_dma_map;
		}

		slot_index = malloc(sizeof(int) * num_rx_desc);
		if(!slot_index){
			ixgbe_dma_unmap(ih, addr_dma);
			munmap(addr_virtual, size_rx_desc);
			goto err_rx_alloc_slot_index;
		}
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
	for(i = 0; i < ih->num_queues; i++, tx_assigned++){
		void	*addr_virtual;
		uint64_t addr_dma;
		int *slot_index;

		addr_virtual = mmap(NULL, size_tx_desc,
			PROT_READ | PROT_WRITE, MAP_HUGETLB, 0, 0);
		if(addr_virtual == MAP_FAILED){
			goto err_tx_mmap;
		}

		ret = ixgbe_dma_map(ih,
			addr_virtual, &addr_dma, size_tx_desc);
		if(ret < 0){
			munmap(addr_virtual, size_tx_desc);
			goto err_tx_dma_map;
		}

		slot_index = malloc(sizeof(int) * num_tx_desc);
		if(!slot_index){
			ixgbe_dma_unmap(ih, addr_dma);
			munmap(addr_virtual, size_tx_desc);
			goto err_tx_alloc_slot_index;
		}
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

err_tx_alloc_slot_index:
err_tx_dma_map:
err_tx_mmap:
	for(i = 0; i < tx_assigned; i++){
		free(ih->tx_ring[i].slot_index);
		ixgbe_dma_unmap(ih, ih->tx_ring[i].addr_dma);
		munmap(ih->tx_ring[i].addr_virtual,
			sizeof(union ixgbe_adv_tx_desc) * ih->tx_ring[i].count);
	}
err_rx_alloc_slot_index:
err_rx_dma_map:
err_rx_mmap:
	for(i = 0; i < rx_assigned; i++){
		free(ih->rx_ring[i].slot_index);
		ixgbe_dma_unmap(ih, ih->rx_ring[i].addr_dma);
		munmap(ih->rx_ring[i].addr_virtual,
			sizeof(union ixgbe_adv_rx_desc) * ih->rx_ring[i].count);
	}
err_desc_size:
	free(ih->tx_ring);
err_alloc_tx_ring:
	free(ih->rx_ring);
err_alloc_rx_ring:

	return -1;
}

static void ixgbe_release_descring(ixgbe_handle *ih)
{
	int i, ret;

	for(i = 0; i < ih->num_queues; i++){
		free(ih->rx_ring[i].slot_index);

		ret = ixgbe_dma_unmap(ih, ih->rx_ring[i].addr_dma);
		if(ret < 0)
			perror("failed to unmap descring");

		munmap(ih->rx_ring[i].addr_virtual,
			sizeof(union ixgbe_adv_rx_desc) * ih->rx_ring[i].count);
	}

	for(i = 0; i < ih->num_queues; i++){
		free(ih->tx_ring[i].slot_index);

		ret = ixgbe_dma_unmap(ih, ih->tx_ring[i].addr_dma);
		if(ret < 0)
			perror("failed to unmap descring");

		munmap(ih->tx_ring[i].addr_virtual,
			sizeof(union ixgbe_adv_tx_desc) * ih->tx_ring[i].count);
	}

	return;
}

static struct ixgbe_buf *ixgbe_alloc_buf(struct ixgbe_handle *ih,
	uint32_t count, uint32_t buf_size)
{
	struct ixgbe_buf *buf;
	uint32_t size;
	void	*addr_virtual;
	uint64_t addr_dma;
	int *free_index;
	int ret, i;

	buf = malloc(sizeof(struct ixgbe_buf));
	if(!buf){
		return NULL;
	}

	size = buf_size * count;
	if(size > huge_page_size){
		printf("buffer size is larger than hugetlb\n");
		return NULL;
	}

	addr_virtual = mmap(NULL, size,
			PROT_READ | PROT_WRITE, MAP_HUGETLB, 0, 0);
	if(addr_virtual == MAP_FAILED)
		return NULL;

	ret = ixgbe_dma_map(ih, addr_virtual, &addr_dma, size);
	if(ret < 0)
		return NULL;

	free_index = malloc(sizeof(int) * count);
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

static int ixgbe_dma_unmap(struct ixgbe_handle *ih,
	uint64_t addr_dma)
{
	struct uio_ixgbe_unmap_req req_unmap;

	req_unmap.addr_dma = addr_dma;

	if(ioctl(ih->fd, UIO_IXGBE_UNMAP, (unsigned long)&req_unmap) < 0)
		return -1;

	return 0;
}

static int ixgbe_dma_map(struct ixgbe_handle *ih,
	void *addr_virtual, uint64_t *addr_dma, uint32_t size)
{
	struct uio_ixgbe_map_req req_map;

	req_map.addr_virtual = (uint64_t)addr_virtual;
	req_map.addr_dma = 0;
	req_map.size = size;
	req_map.cache = IXGBE_DMA_CACHE_DISABLE;

	if(ioctl(ih->fd, UIO_IXGBE_MAP, (unsigned long)&req_map) < 0)
		return -1;

	*addr_dma = req_map.addr_dma;
	return 0;
}

static struct ixgbe_handle *ixgbe_open(char *int_name, uint32_t num_core)
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

	ih->num_interrupt_rate = min((uint16_t)IXGBE_8K_ITR, req_info.info.max_interrupt_rate);
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

static int ixgbe_set_signal(sigset_t *sigset){
	sigemptyset(&sigset);
	ret = sigaddset(&sigset, SIGHUP);
	if(ret != 0)
		return -1;

	ret = sigaddset(&sigset, SIGINT);
	if(ret != 0)
		return -1;

	ret = sigaddset(&sigset, SIGTERM);
	if(ret != 0)
		return -1;

	ret = sigprocmask(SIG_BLOCK, &sigset, NULL);
	if(ret != 0)
		return -1;

	return 0;
}
