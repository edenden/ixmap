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

static int buf_count = 16384;
static char *ixgbe_interface_list[2];

static int ixgbe_set_signal(sigset_t *sigset);

int main(int argc, char **argv)
{
	struct ixgbe_handle **ih_list;
	struct ixgbe_thread *threads;
	uint32_t buf_size = 0;
	uint32_t num_cores = 4;
	uint32_t num_ports = 2;
	uint32_t budget = 1024;
	uint16_t intr_rate = IXGBE_20K_ITR;
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
		ih_list[i] = ixgbe_open(ixgbe_interface_list[i],
			num_cores, budget, intr_rate);
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
			IXGBE_MAX_RXD, IXGBE_MAX_TXD);
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
