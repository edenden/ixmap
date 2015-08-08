#ifndef _IXMAPFWD_MAIN_H
#define _IXMAPFWD_MAIN_H

#include <pthread.h>
#include <ixmap.h>
#include "tun.h"
#include "neigh.h"
#include "fib.h"
#include "epoll.h"

//#define DEBUG

#define min(x, y) ({				\
	typeof(x) _min1 = (x);			\
	typeof(y) _min2 = (y);			\
	(void) (&_min1 == &_min2);		\
	_min1 < _min2 ? _min1 : _min2; })

#define max(x, y) ({				\
	typeof(x) _max1 = (x);			\
	typeof(y) _max2 = (y);			\
	(void) (&_max1 == &_max2);		\
	_max1 > _max2 ? _max1 : _max2; })

#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

#ifdef DEBUG
#define ixgbe_print(args...) printf("ixgbe: " args)
#else
#define ixgbe_print(args...)
#endif

struct ixmapfwd {
	struct ixmap_handle	**ih_array;
	struct tun		**tun;
	struct neigh_table	**neigh;
	struct fib		*fib;
	unsigned int		buf_size;
	unsigned int		num_cores;
	unsigned int		num_ports;
	unsigned int		budget;
	unsigned int		promisc;
	unsigned int		mtu_frame;
	unsigned short		intr_rate;
};

void ixmapfwd_mutex_lock(pthread_mutex_t *mutex);
void ixmapfwd_mutex_unlock(pthread_mutex_t *mutex);

#endif /* _IXMAPFWD_MAIN_H */
