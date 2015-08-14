#define _GNU_SOURCE
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <endian.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <pthread.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <urcu.h>
#include <ixmap.h>

#include "linux/list.h"
#include "linux/list_rcu.h"
#include "main.h"
#include "thread.h"
#include "netlink.h"
#include "epoll.h"

static int ixmapfwd_wait(struct ixmapfwd *ixmapfwd, int fd_ep,
	uint8_t *read_buf, int read_size);
static int ixmapfwd_fd_prepare(struct list_head *ep_desc_head);
static void ixmapfwd_fd_destroy(struct list_head *ep_desc_head,
	int fd_ep);
static int ixmapfwd_thread_create(struct ixmapfwd *ixmapfwd,
	struct ixmapfwd_thread *thread, int thread_index);
static void ixmapfwd_thread_kill(struct ixmapfwd_thread *thread);
static int ixmapfwd_set_signal(sigset_t *sigset);

static int buf_count = 65536;
static char *ixmap_interface_array[2];

int main(int argc, char **argv)
{
	struct ixmapfwd		ixmapfwd;
	struct ixmapfwd_thread	*threads;
	struct list_head	ep_desc_head;
	uint8_t			*read_buf;
	int			ret, i, fd_ep, read_size;
	int			cores_assigned = 0,
				ports_assigned = 0;

	ixmap_interface_array[0] = "ixgbe0";
	ixmap_interface_array[1] = "ixgbe1";

	ixmapfwd.buf_size = 0;
	ixmapfwd.num_cores = 4;
	ixmapfwd.num_ports = 2;
	ixmapfwd.rx_budget = 1024;
	ixmapfwd.tx_budget = 4096;
	ixmapfwd.promisc = 1;
	ixmapfwd.mtu_frame = 0; /* MTU=1522 is used by default. */
	ixmapfwd.intr_rate = IXGBE_20K_ITR;
	INIT_LIST_HEAD(&ep_desc_head);

	ixmapfwd.ih_array = malloc(sizeof(struct ixmap_handle *) * ixmapfwd.num_ports);
	if(!ixmapfwd.ih_array){
		ret = -1;
		goto err_ih_array;
	}

	ixmapfwd.tunh_array = malloc(sizeof(struct tun_handle *) * ixmapfwd.num_ports);
	if(!ixmapfwd.tunh_array){
		ret = -1;
		goto err_tunh_array;
	}

	ixmapfwd.neigh = malloc(sizeof(struct neigh *) * ixmapfwd.num_ports);
	if(!ixmapfwd.neigh){
		ret = -1;
		goto err_neigh_table;
	}

	for(i = 0; i < ixmapfwd.num_ports; i++, ports_assigned++){
		ixmapfwd.ih_array[i] = ixmap_open(ixmap_interface_array[i],
			ixmapfwd.num_cores, ixmapfwd.intr_rate,
			ixmapfwd.rx_budget, ixmapfwd.tx_budget,
			ixmapfwd.mtu_frame, ixmapfwd.promisc);
		if(!ixmapfwd.ih_array[i]){
			printf("failed to ixmap_open, idx = %d\n", i);
			goto err_open;
		}

		ret = ixmap_desc_alloc(ixmapfwd.ih_array[i],
			IXGBE_MAX_RXD, IXGBE_MAX_TXD);
		if(ret < 0){
			printf("failed to ixmap_alloc_descring, idx = %d\n", i);
			printf("please decrease descripter or enable iommu\n");
			goto err_desc_alloc;
		}

		ixmap_configure_rx(ixmapfwd.ih_array[i]);
		ixmap_configure_tx(ixmapfwd.ih_array[i]);
		ixmap_irq_enable(ixmapfwd.ih_array[i]);

		/* calclulate maximum buf_size we should prepare */
		if(ixmap_bufsize_get(ixmapfwd.ih_array[i]) > ixmapfwd.buf_size)
			ixmapfwd.buf_size = ixmap_bufsize_get(ixmapfwd.ih_array[i]);

		ixmapfwd.tunh_array[i] = tun_open(&ixmapfwd, ixmap_interface_array[i], i);
		if(!ixmapfwd.tunh_array[i]){
			printf("failed to tun_open\n");
			goto err_tun_open;
		}

		ixmapfwd.neigh[i] = neigh_alloc();
		if(!ixmapfwd.neigh[i])
			goto err_neigh_alloc;

		continue;

err_neigh_alloc:
		tun_close(&ixmapfwd, i);
err_tun_open:
		ixmap_desc_release(ixmapfwd.ih_array[i]);
err_desc_alloc:
		ixmap_close(ixmapfwd.ih_array[i]);
err_open:
		ret = -1;
		goto err_assign_ports;
	}

	/* Prepare fib */
	ixmapfwd.fib = fib_alloc();
	if(!ixmapfwd.fib)
		goto err_fib_alloc;

	/* Prepare read buffer */
	read_size = max((size_t)getpagesize(), sizeof(struct signalfd_siginfo));
	read_buf = malloc(read_size);
	if(!read_buf)
		goto err_alloc_read_buf;

	/* Prepare each fd in epoll */
	fd_ep = ixmapfwd_fd_prepare(&ep_desc_head);
	if(fd_ep < 0){
		printf("failed to epoll prepare\n");
		goto err_fd_prepare;
	}

	rcu_init();
	rcu_register_thread();

	threads = malloc(sizeof(struct ixmapfwd_thread) * ixmapfwd.num_cores);
	if(!threads){
		ret = -1;
		goto err_alloc_threads;
	}

	for(i = 0; i < ixmapfwd.num_cores; i++, cores_assigned++){
		threads[i].buf = ixmap_buf_alloc(ixmapfwd.ih_array,
			ixmapfwd.num_ports, buf_count, ixmapfwd.buf_size);
		if(!threads[i].buf){
			printf("failed to ixmap_alloc_buf, idx = %d\n", i);
			printf("please decrease buffer or enable iommu\n");
			goto err_buf_alloc;
		}

		threads[i].plane = ixmap_plane_alloc(ixmapfwd.ih_array,
			ixmapfwd.num_ports, i);
		if(!threads[i].plane){
			printf("failed to ixmap_plane_alloc, idx = %d\n", i);
			goto err_plane_alloc;
		}

		threads[i].tun_plane = tun_plane_alloc(&ixmapfwd, i);
		if(!threads[i].tun_plane)
			goto err_tun_plane_alloc;

		ret = ixmapfwd_thread_create(&ixmapfwd, &threads[i], i);
		if(ret < 0){
			goto err_thread_create;
		}

		continue;

err_thread_create:
		tun_plane_release(threads[i].tun_plane);
err_tun_plane_alloc:
		ixmap_plane_release(threads[i].plane);
err_plane_alloc:
		ixmap_buf_release(threads[i].buf,
			ixmapfwd.ih_array, ixmapfwd.num_ports);
err_buf_alloc:
		ret = -1;
		goto err_assign_cores;
	}

	ret = ixmapfwd_wait(&ixmapfwd, fd_ep, read_buf, read_size);

err_assign_cores:
	for(i = 0; i < cores_assigned; i++){
		ixmapfwd_thread_kill(&threads[i]);
		tun_plane_release(threads[i].tun_plane);
		ixmap_plane_release(threads[i].plane);
		ixmap_buf_release(threads[i].buf,
			ixmapfwd.ih_array, ixmapfwd.num_ports);
	}
	free(threads);
err_alloc_threads:
	rcu_unregister_thread();
	ixmapfwd_fd_destroy(&ep_desc_head, fd_ep);
err_fd_prepare:
	free(read_buf);
err_alloc_read_buf:
	fib_release(ixmapfwd.fib);
err_fib_alloc:
err_assign_ports:
	for(i = 0; i < ports_assigned; i++){
		neigh_release(ixmapfwd.neigh[i]);
		tun_close(&ixmapfwd, i);
		ixmap_desc_release(ixmapfwd.ih_array[i]);
		ixmap_close(ixmapfwd.ih_array[i]);
	}
	free(ixmapfwd.neigh);
err_neigh_table:
	free(ixmapfwd.tunh_array);
err_tunh_array:
	free(ixmapfwd.ih_array);
err_ih_array:
	return ret;
}

static int ixmapfwd_wait(struct ixmapfwd *ixmapfwd, int fd_ep,
	uint8_t *read_buf, int read_size)
{
	struct epoll_event events[EPOLL_MAXEVENTS];
	struct epoll_desc *ep_desc;
	int ret, i, num_fd;

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
					goto err_read;

				netlink_process(ixmapfwd, read_buf, ret);
				break;
			case EPOLL_SIGNAL:
				ret = read(ep_desc->fd, read_buf, read_size);
				if(ret < 0)
					goto err_read;

				goto out;
				break;
			default:
				break;
			}
		}
	}

out:
	return 0;

err_read:
	return -1;
}

static int ixmapfwd_fd_prepare(struct list_head *ep_desc_head)
{
	struct epoll_desc	*ep_desc;
	sigset_t		sigset;
	struct sockaddr_nl	addr;
	int			fd_ep, ret;

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

	ep_desc = epoll_desc_alloc_signalfd(&sigset);
	if(!ep_desc)
		goto err_epoll_desc_signalfd;

	list_add(&ep_desc->list, ep_desc_head);

	ret = epoll_add(fd_ep, ep_desc, ep_desc->fd);
	if(ret < 0){
		perror("failed to add fd in epoll");
		goto err_epoll_add_signalfd;
	}

	/* netlink preparing */
	memset(&addr, 0, sizeof(struct sockaddr_nl));
	addr.nl_family = AF_NETLINK;
	addr.nl_groups = RTMGRP_NEIGH | RTMGRP_IPV4_ROUTE | RTMGRP_IPV6_ROUTE;

	ep_desc = epoll_desc_alloc_netlink(&addr);
	if(!ep_desc)
		goto err_epoll_desc_netlink;

	list_add(&ep_desc->list, ep_desc_head);

	ret = epoll_add(fd_ep, ep_desc, ep_desc->fd);
	if(ret < 0){
		perror("failed to add fd in epoll");
		goto err_epoll_add_netlink;
	}

	return fd_ep;

err_epoll_add_netlink:
err_epoll_desc_netlink:
err_epoll_add_signalfd:
err_epoll_desc_signalfd:
err_set_signal:
	ixmapfwd_fd_destroy(ep_desc_head, fd_ep);
err_epoll_open:
	return -1;
}

static void ixmapfwd_fd_destroy(struct list_head *ep_desc_head,
	int fd_ep)
{
	struct epoll_desc *ep_desc, *ep_next;

	list_for_each_entry_safe(ep_desc, ep_next, ep_desc_head, list){
		list_del(&ep_desc->list);
		epoll_del(fd_ep, ep_desc->fd);

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
	}

	close(fd_ep);
	return;
}

static int ixmapfwd_thread_create(struct ixmapfwd *ixmapfwd,
	struct ixmapfwd_thread *thread, int thread_index)
{
	cpu_set_t cpuset;
	int ret;

	thread->index		= thread_index;
	thread->num_ports	= ixmapfwd->num_ports;
	thread->ptid		= pthread_self();
	thread->neigh		= ixmapfwd->neigh;
	thread->fib		= ixmapfwd->fib;

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

	ret = pthread_sigmask(SIG_BLOCK, sigset, NULL);
	if(ret != 0)
		return -1;

	return 0;
}

void ixmapfwd_mutex_lock(pthread_mutex_t *mutex)
{
	int ret;

	ret = pthread_mutex_lock(mutex);
	if(ret){
		perror("failed to lock");
	}
}

void ixmapfwd_mutex_unlock(pthread_mutex_t *mutex)
{
	int ret;

	ret = pthread_mutex_unlock(mutex);
	if(ret){
		perror("failed to unlock");
	}
}
