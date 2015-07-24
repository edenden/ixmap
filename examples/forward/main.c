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
#include <urcu.h>
#include <ixmap.h>

#include "main.h"
#include "thread.h"

static int buf_count = 16384;
static char *ixmap_interface_list[2];

static int ixmapfwd_thread_create(struct ixmapfwd_thread *thread,
	int thread_index, unsigned int num_ports);
static void ixmapfwd_thread_kill(struct ixmapfwd_thread *thread);
static int ixmapfwd_set_signal(sigset_t *sigset);

int main(int argc, char **argv)
{
	struct ixmap_handle **ih_list;
	struct ixmapfwd_thread *threads;
	unsigned int buf_size = 0;
	unsigned int num_cores = 4;
	unsigned int num_ports = 2;
	unsigned int budget = 1024;
	unsigned int promisc = 1;
	unsigned int mtu_frame = 0; /* MTU=1522 is used by default. */
	unsigned short intr_rate = IXGBE_20K_ITR;
	sigset_t sigset;
	int ret = 0, i, signal;
	int cores_assigned = 0, ports_assigned = 0;

	ixmap_interface_list[0] = "ixgbe0";
	ixmap_interface_list[1] = "ixgbe1";

	ih_list = malloc(sizeof(struct ixmap_handle *) * num_ports);
	if(!ih_list){
		ret = -1;
		goto err_ih_list;
	}

	for(i = 0; i < num_ports; i++, ports_assigned++){
		ih_list[i] = ixmap_open(ixmap_interface_list[i],
			num_cores, budget, intr_rate, mtu_frame, promisc);
		if(!ih_list[i]){
			printf("failed to ixmap_open, idx = %d\n", i);
			ret = -1;
			goto err_assign_ports;
		}

		ret = ixmap_desc_alloc(ih_list[i],
			IXGBE_MAX_RXD, IXGBE_MAX_TXD);
		if(ret < 0){
			printf("failed to ixmap_alloc_descring, idx = %d\n", i);
			printf("please decrease descripter or enable iommu\n");
			ixmap_close(ih_list[i]);
			ret = -1;
			goto err_assign_ports;
		}

		ixmap_configure_rx(ih_list[i]);
		ixmap_configure_tx(ih_list[i]);
		ixmap_irq_enable(ih_list[i]);

		/* calclulate maximum buf_size we should prepare */
		if(ixmap_bufsize_get(ih_list[i]) > buf_size)
			buf_size = ixmap_bufsize_get(ih_list[i]);
	}

	rcu_init();
	rcu_register_thread();

	threads = malloc(sizeof(struct ixmapfwd_thread) * num_cores);
	if(!threads){
		ret = -1;
		goto err_alloc_threads;
	}

	ret = ixmapfwd_set_signal(&sigset);
	if(ret != 0){
		printf("failed to ixmap_set_signal\n");
		ret = -1;
		goto err_ixmap_set_signal;
	}

	for(i = 0; i < num_cores; i++, cores_assigned++){
		threads[i].buf = ixmap_buf_alloc(ih_list, num_ports,
					buf_count, buf_size);
		if(!threads[i].buf){
			printf("failed to ixmap_alloc_buf, idx = %d\n", i);
			printf("please decrease buffer or enable iommu\n");
			ret = -1;
			goto err_assign_cores;
		}

		threads[i].instance = ixmap_instance_alloc(ih_list, num_ports, i);
		if(!threads[i].instance){
			printf("failed to ixmap_instance_alloc, idx = %d\n", i);
			ixmap_buf_release(threads[i].buf, ih_list, num_ports);
			ret = -1;
			goto err_assign_cores;
		}

		ret = ixmapfwd_thread_create(&threads[i], i, num_ports);
		if(ret < 0){
			ixmap_instance_release(threads[i].instance);
			ixmap_buf_release(threads[i].buf, ih_list, num_ports);
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
		ixmapfwd_thread_kill(&threads[i]);
		ixmap_instance_release(threads[i].instance);
		ixmap_buf_release(threads[i].buf, ih_list, num_ports);
	}
err_ixmap_set_signal:
	free(threads);
err_alloc_threads:
	rcu_unregister_thread();
	rcu_exit();
err_assign_ports:
	for(i = 0; i < ports_assigned; i++){
		ixmap_desc_release(ih_list[i]);
		ixmap_close(ih_list[i]);
	}
	free(ih_list);
err_ih_list:
	return ret;
}

static int ixmapfwd_thread_create(struct ixmapfwd_thread *thread,
	int thread_index, unsigned int num_ports)
{
	cpu_set_t cpuset;
	int ret;

	thread->index = thread_index;
	thread->num_ports = num_ports;
	thread->ptid = pthread_self();

	ret = pthread_create(&thread->tid, NULL, thread_process_interrupt, thread);
	if(ret < 0){
		perror("failed to create thread");
		goto err_pthread_create;
	}

	CPU_ZERO(&cpuset);
	CPU_SET(thread->index, &cpuset);
	ret = pthread_setaffinity_np(thread->tid, sizeof(cpu_set_t), &cpuset);
	if(ret < 0){
		perror("failed to set affinity");
		goto err_set_affinity;
	}
	

	return 0;

err_set_affinity:
	ixmapfwd_thread_kill(thread);
err_pthread_create:
	return -1;
}

static void ixmapfwd_thread_kill(struct ixmapfwd_thread *thread)
{
	int ret;
	ret = pthread_kill(thread->tid, SIGUSR1);
	if(ret != 0)
		perror("failed to kill thread");

	ret = pthread_join(thread->tid, NULL);
	if(ret != 0)
		perror("failed to join thread");

	return;
}

static int ixmapfwd_set_signal(sigset_t *sigset)
{
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
