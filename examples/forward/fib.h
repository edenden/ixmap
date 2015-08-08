#ifndef _IXMAPFWD_FIB_H
#define _IXMAPFWD_FIB_H

#include <pthread.h>
#include "linux/list.h"
#include "linux/list_rcu.h"

enum fib_type {
	FIB_TYPE_FORWARD = 0,
	FIB_TYPE_LINK,
	FIB_TYPE_LOCAL
};

struct fib_entry {
	uint32_t		prefix[4];
	unsigned int		prefix_len;
	uint32_t		nexthop[4];
	int			port_index; /* -1 means not ixmap interface */
	enum fib_type		type;
	struct list_head	list;
};

struct fib {
	struct trie_tree	tree;
	pthread_mutex_t		mutex;
};

struct fib *fib_alloc();
void fib_release(struct fib *fib);
int fib_route_update(struct fib *fib, int family,
	uint32_t *prefix, unsigned int prefix_len,
	uint32_t *nexthop, unsigned int port_index,
	enum fib_type type, unsigned int id);
int fib_route_delete(struct fib *fib, int family,
	uint32_t *prefix, unsigned int prefix_len,
	unsigned int id);
struct list_head *fib_lookup(struct fib *fib, int family,
	uint32_t *destination);

#endif /* _IXMAPFWD_FIB_H */
