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

#include "main.h"
#include "driver.h"
#include "rxinit.h"
#include "txinit.h"

static int buf_count = 4096;
static char *ixgbe_interface_list[2];
static int budget = 1024;

static inline void ixgbe_irq_enable(struct ixgbe_handle *ih);
static int ixgbe_thread_create(struct ixgbe_handle **ih_list,
	struct ixgbe_thread *thread, int num_ports,
	int num_cores, int thread_index, struct ixgbe_buf *buf);
static void ixgbe_kill_thread(struct ixgbe_thread *thread);
static int ixgbe_alloc_descring(struct ixgbe_handle *ih,
	uint32_t num_rx_desc, uint32_t num_tx_desc);
static void ixgbe_release_descring(struct ixgbe_handle *ih);
static struct ixgbe_buf *ixgbe_alloc_buf(struct ixgbe_handle **ih_list,
	int num_ports, uint32_t count, uint32_t buf_size);
static void ixgbe_release_buf(struct ixgbe_handle **ih_list,
	int num_ports, struct ixgbe_buf *buf);
static int ixgbe_dma_map(struct ixgbe_handle *ih,
	void *addr_virtual, unsigned long *addr_dma, unsigned long size);
static int ixgbe_dma_unmap(struct ixgbe_handle *ih,
	unsigned long addr_dma);
static struct ixgbe_handle *ixgbe_open(char *interface_name,
	uint32_t num_core, int budget);
static void ixgbe_close(struct ixgbe_handle *ih);
static int ixgbe_set_signal(sigset_t *sigset);

int main(int argc, char **argv)
{
	struct ixgbe_handle **ih_list;
	struct ixgbe_thread *threads;
	uint32_t buf_size = 0;
	uint32_t num_cores = 4;
	uint32_t num_ports = 2;
	sigset_t sigset;
	int ret = 0, i, signal;
	int cores_assigned = 0, ports_assigned = 0;

	ixgbe_interface_list[0] = "ixgbe0";
	ixgbe_interface_list[1] = "ixgbe1";

	ih_list = malloc(sizeof(struct ixgbe_handle *) * num_ports);
	if(!ih_list){
		ret = -1;
		goto err_ih_list;
	}

	for(i = 0; i < num_ports; i++, ports_assigned++){
		ih_list[i] = ixgbe_open(ixgbe_interface_list[i], num_cores, budget);
		if(!ih_list[i]){
			printf("failed to ixgbe_open, idx = %d\n", i);
			ret = -1;
			goto err_assign_ports;
		}

		/*
		 * Configuration of frame MTU is supported.
		 * However, MTU=1522 is used by default.
		 * See ixgbe_set_rx_buffer_len().
		 */
		// ih_list[i]->mtu_frame = mtu_frame;

		/* Configuration of promiscuous mode is supported */
		ih_list[i]->promisc = 1;

		ret = ixgbe_alloc_descring(ih_list[i],
			4096, 4096);
			//IXGBE_DEFAULT_RXD, IXGBE_DEFAULT_TXD);
		if(ret < 0){
			printf("failed to ixgbe_alloc_descring, idx = %d\n", i);
			printf("please decrease descripter or enable iommu\n");
			ixgbe_close(ih_list[i]);
			ret = -1;
			goto err_assign_ports;
		}

		ixgbe_configure_rx(ih_list[i]);
		ixgbe_configure_tx(ih_list[i]);
		ixgbe_irq_enable(ih_list[i]);

		/* calclulate maximum buf_size we should prepare */
		if(ih_list[i]->buf_size > buf_size)
			buf_size = ih_list[i]->buf_size;
	}

	threads = malloc(sizeof(struct ixgbe_thread) * num_cores);
	if(!threads){
		perror("malloc");
		ret = -1;
		goto err_ixgbe_alloc_threads;
	}

	ret = ixgbe_set_signal(&sigset);
	if(ret != 0){
		printf("failed to ixgbe_set_signal\n");
		ret = -1;
		goto err_ixgbe_set_signal;
	}

	for(i = 0; i < num_cores; i++, cores_assigned++){
		struct ixgbe_buf *buf;

		buf = ixgbe_alloc_buf(ih_list, num_ports,
			buf_count, buf_size);
		if(!buf){
			printf("failed to ixgbe_alloc_buf, idx = %d\n", i);
			printf("please decrease buffer or enable iommu\n");
			ret = -1;
			goto err_assign_cores;
		}

		ret = ixgbe_thread_create(ih_list, &threads[i],
			num_ports, num_cores, i, buf);
		if(ret != 0){
			printf("failed to ixgbe_thread_create, idx = %d\n", i);
			ixgbe_release_buf(ih_list, num_ports, buf);
			ret = -1;
			goto err_assign_cores;
		}
	}

	while(1){
		if(sigwait(&sigset, &signal) == 0){
			break;
		}
	}
	ret = 0;

err_assign_cores:
	for(i = 0; i < cores_assigned; i++){
		ixgbe_kill_thread(&threads[i]);
		ixgbe_release_buf(ih_list, num_ports, threads[i].buf);
	}
err_ixgbe_set_signal:
	free(threads);
err_ixgbe_alloc_threads:
err_assign_ports:
	for(i = 0; i < ports_assigned; i++){
		ixgbe_release_descring(ih_list[i]);
		ixgbe_close(ih_list[i]);
	}
	free(ih_list);
err_ih_list:
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

	/* XXX: Currently we don't support misc interrupts */
	mask &= ~IXGBE_EIMS_LSC;
	mask &= ~IXGBE_EIMS_TCP_TIMER;
	mask &= ~IXGBE_EIMS_OTHER;

	IXGBE_WRITE_REG(ih, IXGBE_EIMS, mask);

	ixgbe_irq_enable_queues(ih, ~0);
	IXGBE_WRITE_FLUSH(ih);

	return;
}

static int ixgbe_thread_create(struct ixgbe_handle **ih_list,
	struct ixgbe_thread *thread, int num_ports,
	int num_cores, int thread_index, struct ixgbe_buf *buf)
{
	cpu_set_t cpuset;
	int i, ret;

	thread->index = thread_index;
	thread->num_threads = num_cores;
	thread->num_ports = num_ports;
	thread->buf = buf;
	thread->ptid = pthread_self();

	thread->ports = malloc(sizeof(struct ixgbe_port) * num_ports);
	if(!thread->ports){
		printf("failed to allocate port for each thread\n");
		goto err_ixgbe_alloc_ports;
	}

	for(i = 0; i < num_ports; i++){
		thread->ports[i].ih = ih_list[i];
		thread->ports[i].interface_name = ih_list[i]->interface_name;
		thread->ports[i].rx_ring = &(ih_list[i]->rx_ring[thread_index]);
		thread->ports[i].tx_ring = &(ih_list[i]->tx_ring[thread_index]);
		thread->ports[i].mtu_frame = ih_list[i]->mtu_frame;
		thread->ports[i].budget = ih_list[i]->budget;
	}

	ret = pthread_create(&thread->tid, NULL, process_interrupt, thread);
	if(ret < 0){
		perror("failed to create thread");
		goto err_pthread_create;
	}

	CPU_ZERO(&cpuset);
	CPU_SET(thread_index, &cpuset);
	ret = pthread_setaffinity_np(thread->tid, sizeof(cpu_set_t), &cpuset);
	if(ret < 0){
		perror("failed to set affinity");
		goto err_set_affinity;
	}
	

	return 0;

err_pthread_create:
	free(thread->ports);
err_ixgbe_alloc_ports:
	return -1;

err_set_affinity:
	ixgbe_kill_thread(thread);
	return -1;
}

static void ixgbe_kill_thread(struct ixgbe_thread *thread)
{
	int ret;
	ret = pthread_kill(thread->tid, SIGUSR1);
	if(ret != 0)
		perror("failed to kill thread");

	ret = pthread_join(thread->tid, NULL);
	if(ret != 0)
		perror("failed to join thread");

	free(thread->ports);
	return;
}

static int ixgbe_alloc_descring(struct ixgbe_handle *ih,
	uint32_t num_rx_desc, uint32_t num_tx_desc)
{
	int ret, i, rx_assigned = 0, tx_assigned = 0;
	unsigned long size_tx_desc, size_rx_desc;

	ih->rx_ring = malloc(sizeof(struct ixgbe_ring) * ih->num_queues);
	if(!ih->rx_ring)
		goto err_alloc_rx_ring;

	ih->tx_ring = malloc(sizeof(struct ixgbe_ring) * ih->num_queues);
	if(!ih->tx_ring)
		goto err_alloc_tx_ring;

	size_rx_desc = sizeof(union ixgbe_adv_rx_desc) * num_rx_desc;
	size_tx_desc = sizeof(union ixgbe_adv_tx_desc) * num_tx_desc;

	/* Rx descripter ring allocation */
	for(i = 0; i < ih->num_queues; i++, rx_assigned++){
		void *addr_virtual;
		unsigned long addr_dma;
		int *slot_index;

		/*
		 * XXX: We don't support NUMA-aware memory allocation in userspace.
		 * To support, mbind() or set_mempolicy() will be useful.
		 */
		addr_virtual = mmap(NULL, size_rx_desc, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, 0, 0);
		if(addr_virtual == MAP_FAILED){
			goto err_rx_assign;
		}

		ret = ixgbe_dma_map(ih,
			addr_virtual, &addr_dma, size_rx_desc);
		if(ret < 0){
			munmap(addr_virtual, size_rx_desc);
			goto err_rx_assign;
		}

		slot_index = malloc(sizeof(int) * num_rx_desc);
		if(!slot_index){
			ixgbe_dma_unmap(ih, addr_dma);
			munmap(addr_virtual, size_rx_desc);
			goto err_rx_assign;
		}

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
		unsigned long addr_dma;
		int *slot_index;

		/*
		 * XXX: We don't support NUMA-aware memory allocation in userspace.
		 * To support, mbind() or set_mempolicy() will be useful.
		 */
		addr_virtual = mmap(NULL, size_tx_desc, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, 0, 0);
		if(addr_virtual == MAP_FAILED){
			goto err_tx_assign;
		}

		ret = ixgbe_dma_map(ih,
			addr_virtual, &addr_dma, size_tx_desc);
		if(ret < 0){
			munmap(addr_virtual, size_tx_desc);
			goto err_tx_assign;
		}

		slot_index = malloc(sizeof(int) * num_tx_desc);
		if(!slot_index){
			ixgbe_dma_unmap(ih, addr_dma);
			munmap(addr_virtual, size_tx_desc);
			goto err_tx_assign;
		}

		ih->tx_ring[i].addr_dma = addr_dma;
		ih->tx_ring[i].addr_virtual = addr_virtual;
		ih->tx_ring[i].count = num_tx_desc;

		ih->tx_ring[i].next_to_use = 0;
		ih->tx_ring[i].next_to_clean = 0;
		ih->tx_ring[i].slot_index = slot_index;
	}

	return 0;

err_tx_assign:
	for(i = 0; i < tx_assigned; i++){
		free(ih->tx_ring[i].slot_index);
		ixgbe_dma_unmap(ih, ih->tx_ring[i].addr_dma);
		munmap(ih->tx_ring[i].addr_virtual, size_tx_desc);
	}
err_rx_assign:
	for(i = 0; i < rx_assigned; i++){
		free(ih->rx_ring[i].slot_index);
		ixgbe_dma_unmap(ih, ih->rx_ring[i].addr_dma);
		munmap(ih->rx_ring[i].addr_virtual, size_rx_desc);
	}
	free(ih->tx_ring);
err_alloc_tx_ring:
	free(ih->rx_ring);
err_alloc_rx_ring:

	return -1;
}

static void ixgbe_release_descring(struct ixgbe_handle *ih)
{
	int i, ret;
	unsigned long size_rx_desc, size_tx_desc;

	for(i = 0; i < ih->num_queues; i++){
		free(ih->rx_ring[i].slot_index);

		ret = ixgbe_dma_unmap(ih, ih->rx_ring[i].addr_dma);
		if(ret < 0)
			perror("failed to unmap descring");

		size_rx_desc = sizeof(union ixgbe_adv_rx_desc) * ih->rx_ring[i].count;
		munmap(ih->rx_ring[i].addr_virtual, size_rx_desc);
	}

	for(i = 0; i < ih->num_queues; i++){
		free(ih->tx_ring[i].slot_index);

		ret = ixgbe_dma_unmap(ih, ih->tx_ring[i].addr_dma);
		if(ret < 0)
			perror("failed to unmap descring");

		size_tx_desc = sizeof(union ixgbe_adv_tx_desc) * ih->tx_ring[i].count;
		munmap(ih->tx_ring[i].addr_virtual, size_tx_desc);
	}

	return;
}

static struct ixgbe_buf *ixgbe_alloc_buf(struct ixgbe_handle **ih_list,
	int num_ports, uint32_t count, uint32_t buf_size)
{
	struct ixgbe_buf *buf;
	void	*addr_virtual;
	unsigned long addr_dma, size;
	int *free_index;
	int ret, i, mapped_ports = 0;

	buf = malloc(sizeof(struct ixgbe_buf));
	if(!buf)
		goto err_alloc_buf;

	buf->addr_dma = malloc(sizeof(unsigned long) * num_ports);
	if(!buf->addr_dma)
		goto err_alloc_buf_addr_dma;

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
		ret = ixgbe_dma_map(ih_list[i], addr_virtual, &addr_dma, size);
		if(ret < 0)
			goto err_ixgbe_dma_map;

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
err_ixgbe_dma_map:
	for(i = 0; i < mapped_ports; i++){
		ixgbe_dma_unmap(ih_list[i], buf->addr_dma[i]);
	}
	munmap(addr_virtual, size);
err_mmap:
	free(buf->addr_dma);
err_alloc_buf_addr_dma:
	free(buf);
err_alloc_buf:
	return NULL;
}

static void ixgbe_release_buf(struct ixgbe_handle **ih_list,
	int num_ports, struct ixgbe_buf *buf)
{
	int i, ret;
	unsigned long size;

	free(buf->free_index);

	for(i = 0; i < num_ports; i++){
		ret = ixgbe_dma_unmap(ih_list[i], buf->addr_dma[i]);
		if(ret < 0)
			perror("failed to unmap buf");
	}

	size = buf->buf_size * buf->count;
	munmap(buf->addr_virtual, size);
	free(buf->addr_dma);
	free(buf);

	return;
}

static int ixgbe_dma_map(struct ixgbe_handle *ih, void *addr_virtual,
	unsigned long *addr_dma, unsigned long size)
{
	struct uio_ixgbe_map_req req_map;

	req_map.addr_virtual = (unsigned long)addr_virtual;
	req_map.addr_dma = 0;
	req_map.size = size;
	req_map.cache = IXGBE_DMA_CACHE_DISABLE;

	if(ioctl(ih->fd, UIO_IXGBE_MAP, (unsigned long)&req_map) < 0)
		return -1;

	*addr_dma = req_map.addr_dma;
	return 0;
}

static int ixgbe_dma_unmap(struct ixgbe_handle *ih,
	unsigned long addr_dma)
{
	struct uio_ixgbe_unmap_req req_unmap;

	req_unmap.addr_dma = addr_dma;

	if(ioctl(ih->fd, UIO_IXGBE_UNMAP, (unsigned long)&req_unmap) < 0)
		return -1;

	return 0;
}

static struct ixgbe_handle *ixgbe_open(char *interface_name,
	uint32_t num_core, int budget)
{
	struct ixgbe_handle *ih;
	char filename[FILENAME_SIZE];
	struct uio_ixgbe_info_req req_info;
	struct uio_ixgbe_up_req req_up;

	ih = malloc(sizeof(struct ixgbe_handle));
	if (!ih)
		goto err_alloc_ih;
	memset(ih, 0, sizeof(struct ixgbe_handle));

	snprintf(filename, sizeof(filename), "/dev/%s", interface_name);
	ih->fd = open(filename, O_RDWR);
	if (ih->fd < 0)
		goto err_open;

	/* Get device information */
	memset(&req_info, 0, sizeof(struct uio_ixgbe_info_req));
	if(ioctl(ih->fd, UIO_IXGBE_INFO, (unsigned long)&req_info) < 0)
		goto err_ioctl_info;

	/* UP the device */
	memset(&req_up, 0, sizeof(struct uio_ixgbe_up_req));

	ih->num_interrupt_rate =
		min((uint16_t)IXGBE_8K_ITR, req_info.max_interrupt_rate);
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

static void ixgbe_close(struct ixgbe_handle *ih)
{
	munmap(ih->bar, ih->bar_size);
	close(ih->fd);
	free(ih);

	return;
}

static int ixgbe_set_signal(sigset_t *sigset){
	int ret;

	sigemptyset(sigset);
	ret = sigaddset(sigset, SIGUSR1);
	if(ret != 0)
		return -1;

	ret = sigaddset(sigset, SIGHUP);
	if(ret != 0)
		return -1;

	ret = sigaddset(sigset, SIGINT);
	if(ret != 0)
		return -1;

	ret = sigaddset(sigset, SIGTERM);
	if(ret != 0)
		return -1;

	ret = sigprocmask(SIG_BLOCK, sigset, NULL);
	if(ret != 0)
		return -1;

	return 0;
}
