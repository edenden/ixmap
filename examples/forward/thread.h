#ifndef _IXMAPFWD_THREAD_H
#define _IXMAPFWD_THREAD_H

struct ixmapfwd_thread {
	int			index;
	pthread_t		tid;
	pthread_t		ptid;
	int			num_ports;
	struct ixmap_buf	*buf;
	struct ixmap_instance	*instance;
	struct tun		**tun;
	struct neigh_table	**neigh;
	struct fib		*fib;
};

void *thread_process_interrupt(void *data);

#endif /* _IXMAPFWD_THREAD_H */
