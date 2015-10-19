#ifndef _IXMAPFWD_MAIN_H
#define _IXMAPFWD_MAIN_H

#include <pthread.h>
#include <ixmap.h>
#include <config.h>

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

#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)
#define prefetch(x)	__builtin_prefetch(x, 0)
#define prefetchw(x)	__builtin_prefetch(x, 1)

#define PROCESS_NAME "ixmap"
#define SYSLOG_FACILITY LOG_DAEMON
#define IXMAP_RX_BUDGET 1024
#define IXMAP_TX_BUDGET 4096

struct ixmapfwd {
	struct ixmap_handle	**ih_array;
	struct tun_handle	**tunh_array;
	unsigned int		buf_size;
	unsigned int		num_cores;
	unsigned int		num_ports;
	unsigned int		promisc;
	unsigned int		mtu_frame;
	unsigned int		buf_count;
	unsigned int		daemonize;
	unsigned short		intr_rate;
};

void ixmapfwd_log(int level, char *fmt, ...);
extern char *optarg;

#endif /* _IXMAPFWD_MAIN_H */
