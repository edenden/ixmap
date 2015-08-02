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

#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})

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

#endif /* _IXMAPFWD_MAIN_H */
