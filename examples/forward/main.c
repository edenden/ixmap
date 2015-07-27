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
	struct ixmap_handle	**ih_list;
	struct ixmapfwd_thread	*threads;
	struct tun_handle	**th_list;
	unsigned int buf_size = 0;
	unsigned int num_cores = 4;
	unsigned int num_ports = 2;
	unsigned int budget = 1024;
	unsigned int promisc = 1;
	unsigned int mtu_frame = 0; /* MTU=1522 is used by default. */
	unsigned short intr_rate = IXGBE_20K_ITR;
	int ret = 0, i, fd_ep, num_fd, read_size;
	int cores_assigned = 0, ports_assigned = 0;
	struct epoll_desc *ep_desc_list, *ep_desc;
	struct epoll_event events[EPOLL_MAXEVENTS];
	uint8_t *read_buf;

	ixmap_interface_list[0] = "ixgbe0";
	ixmap_interface_list[1] = "ixgbe1";

	ih_list = malloc(sizeof(struct ixmap_handle *) * num_ports);
	if(!ih_list){
		ret = -1;
		goto err_ih_list;
	}

	th_list = malloc(sizeof(struct tun_handle *) * num_ports);
	if(!th_list){
		ret = -1;
		goto err_th_list;
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

		th_list[i] = tun_open(ixmap_interface_list[i],
			ixmap_macaddr_get(ih_list[i]),
			ixmap_mtu_get(ih_list[i]));
		if(!th_list[i]){
			printf("failed to tun_open\n");
			ixmap_desc_release(ih_list[i]);
			ixmap_close(ih_list[i]);
			ret = -1;
			goto err_assign_ports;
		}
	}

	rcu_init();
	rcu_register_thread();

	threads = malloc(sizeof(struct ixmapfwd_thread) * num_cores);
	if(!threads){
		ret = -1;
		goto err_alloc_threads;
	}

	/* Prepare read buffer */
	read_size = max(sysconf(_SC_PAGE_SIZE),
		sizeof(struct signalfd_siginfo));
	read_buf = malloc(read_size);
	if(!read_buf)
		goto err_alloc_read_buf;

	/* Prepare each fd in epoll */
	fd_ep = ixmapfwd_fd_prepare(&ep_desc_list);
	if(fd_ep < 0){
		printf("failed to epoll prepare\n");
		goto err_fd_prepare;
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

		threads[i].instance_tun = tun_instance_alloc(th_list, num_ports);
		if(!threads[i].instance_tun){
			printf("failed to tun_instance_alloc, idx = %d\n", i);
			ixmap_instance_release(threads[i].instance);
			ixmap_buf_release(threads[i].buf, ih_list, num_ports);
			ret = -1;
			goto err_assign_cores;
		}

		ret = ixmapfwd_thread_create(&threads[i], i, num_ports);
		if(ret < 0){
			tun_instance_release(threads[i].instance_tun);
			ixmap_instance_release(threads[i].instance);
			ixmap_buf_release(threads[i].buf, ih_list, num_ports);
			ret = -1;
			goto err_assign_cores;
		}
	}

	while(1){
		num_fd = epoll_wait(fd_ep, events, EPOLL_MAXEVENTS, -1);
		if(num_fd < 0){
			perror("epoll error");
			continue;
		}

		for(i = 0; i < num_fd; i++){
			ep_desc = (struct epoll_desc *)events[i].data.ptr;
			
			switch(ep_desc->type){
			case EPOLL_NETLINK:
				ret = read(ep_desc->fd, read_buf, read_size);
				if(ret < 0)
					continue;
				netlink_process(read_buf, ret);
				break;
			case EPOLL_SIGNAL:
				ret = read(ep_desc->fd, read_buf, read_size);
				if(ret < 0)
					continue;
				goto out;
				break;
			default:
				break;
			}
		}
	}

out:
	ret = 0;

err_assign_cores:
	for(i = 0; i < cores_assigned; i++){
		ixmapfwd_thread_kill(&threads[i]);
		tun_instance_release(threads[i].instance_tun);
		ixmap_instance_release(threads[i].instance);
		ixmap_buf_release(threads[i].buf, ih_list, num_ports);
	}
	ixmapfwd_fd_destroy(ep_desc_list, fd_ep);
err_fd_prepare:
	free(read_buf);
err_alloc_read_buf:
	free(threads);
err_alloc_threads:
	rcu_unregister_thread();
	rcu_exit();
err_assign_ports:
	for(i = 0; i < ports_assigned; i++){
		tun_close(th_list[i]);
		ixmap_desc_release(ih_list[i]);
		ixmap_close(ih_list[i]);
	}
	free(th_list);
err_tun_list:
	free(ih_list);
err_ih_list:
	return ret;
}

static int ixmapfwd_fd_prepare(struct epoll_desc **ep_desc_list)
{
	struct epoll_desc ep_desc_root, *ep_desc_last;
	sigset_t sigset;
	struct sockaddr_nl addr;
	int fd_ep, num_fd, i, ret;

	memset(&ep_desc_root, 0, sizeof(struct epoll_desc));
	ep_desc_last = &ep_desc_root;

	/* epoll fd preparing */
	fd_ep = epoll_create(EPOLL_MAXEVENTS);
	if(fd_ep < 0){
		perror("failed to make epoll fd");
		goto err_epoll_open;
	}

	/* signalfd preparing */
	ret = ixmapfwd_set_signal(&sigset);
	if(ret != 0){
		goto err_set_signal;
	}

	ep_desc_last->next = epoll_desc_alloc_signalfd(&sigset);
	if(!ep_desc_last->next)
		goto err_epoll_desc_signalfd;

	ep_desc_last = ep_desc_last->next;
	ret = epoll_add(fd_ep, ep_desc_last, ep_desc_last->fd);
	if(ret < 0){
		perror("failed to add fd in epoll");
		goto err_epoll_add_signalfd;
	}

	/* netlink preparing */
	memset(&addr, 0, sizeof(struct sockaddr_nl));
	addr.nl_family = AF_NETLINK;
	addr.nl_groups = RTMGRP_NEIGH | RTMGRP_IPV4_ROUTE | RTMGRP_IPV6_ROUTE;

	ep_desc_last->next = epoll_desc_alloc_netlink(&addr);
	if(!ep_desc_last->next)
		goto err_epoll_desc_netlink;

	ep_desc_last = ep_desc_last->next;
	ret = epoll_add(fd_ep, ep_desc_last, ep_desc_last->fd);
	if(ret < 0){
		perror("failed to add fd in epoll");
		goto err_epoll_add_netlink;
	}

	*ep_desc_list = ep_desc_root.next;
	return fd_ep;

err_epoll_add_netlink:
err_epoll_desc_netlink:
err_epoll_add_signalfd:
err_epoll_desc_signalfd:
err_set_signal:
	thread_fd_destroy(ep_desc_root.next, fd_ep, num_ports);
err_epoll_open:
	return -1;
}

static void ixmapfwd_fd_destroy(struct epoll_desc *ep_desc_list,
	int fd_ep)
{
	struct epoll_desc *ep_desc, *ep_desc_next;

	ep_desc = ep_desc_list;
	while(ep_desc){
		ep_desc_next = ep_desc->next;

		switch(ep_desc->type){
		case EPOLL_SIGNAL:
			epoll_desc_release_signalfd(ep_desc);
			break;
		case EPOLL_NETLINK:
			epoll_desc_release_netlink(ep_desc);
			break;
		default:
			break;
		}

		ep_desc = ep_desc_next;
	}

	close(fd_ep);
	return;
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

void ixmapfwd_mutex_lock(pthread_mutex_t *mutex){
	int ret;

	ret = pthread_mutex_lock(mutex);
	if(ret)
		perror("failed to lock");
	}
}

void ixmapfwd_mutex_unlock(pthread_mutex_t *mutex){
	int ret;

	ret = pthread_mutex_unlock(mutex);
	if(ret){
		perror("failed to unlock");
	}
}
