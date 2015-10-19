#ifndef _IXMAPFWD_THREAD_H
#define _IXMAPFWD_THREAD_H

#include <pthread.h>
#include <ixmap.h>

#include "iftap.h"
#include "neigh.h"
#include "fib.h"

struct ixmapfwd_thread {
	struct ixmap_plane	*plane;
	struct ixmap_buf	*buf;
	struct ixmap_desc	*desc;
	struct neigh_table	**neigh_inet;
	struct neigh_table	**neigh_inet6;
	struct fib		*fib_inet;
	struct fib		*fib_inet6;
	struct tun_plane	*tun_plane;
	int			index;
	pthread_t		tid;
	pthread_t		ptid;
	unsigned int		num_ports;
	struct ixmap_marea	*neigh_inet_area;
	struct ixmap_marea	*neigh_inet6_area;
};

void *thread_process_interrupt(void *data);

#endif /* _IXMAPFWD_THREAD_H */
