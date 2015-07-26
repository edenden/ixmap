#ifndef _IXMAPFWD_MAIN_H
#define _IXMAPFWD_MAIN_H

//#define DEBUG

#define ALIGN(x,a)		__ALIGN_MASK(x,(typeof(x))(a)-1)
#define __ALIGN_MASK(x,mask)	(((x)+(mask))&~(mask))

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

#ifdef DEBUG
#define ixgbe_print(args...) printf("ixgbe: " args)
#else
#define ixgbe_print(args...)
#endif

struct tun_port {
	int			fd;
	unsigned int		ifindex;
};

struct tun_instance {
	struct tun_port		*port;
};

struct ixmapfwd_thread {
	int			index;
	pthread_t		tid;
	pthread_t		ptid;
	int			num_ports;
	struct ixmap_buf	*buf;
	struct ixmap_instance	*instance;
	struct tun_instance	*instance_tun;
};

#endif /* _IXMAPFWD_MAIN_H */
