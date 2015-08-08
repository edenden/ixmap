#ifndef _IXMAPFWD_THREAD_H
#define _IXMAPFWD_THREAD_H

#include <pthread.h>
#include <ixmap.h>

#include "tun.h"
#include "neigh.h"
#include "fib.h"

struct ixmapfwd_thread {
	int			index;
	pthread_t		tid;
	pthread_t		ptid;
	unsigned int		num_ports;
	struct ixmap_buf	*buf;
	struct ixmap_plane	*plane;
	struct tun		**tun;
	struct neigh_table	**neigh;
	struct fib		*fib;
};

void *thread_process_interrupt(void *data);

#endif /* _IXMAPFWD_THREAD_H */
