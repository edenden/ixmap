#ifndef _IXMAPFWD_FIB_H
#define _IXMAPFWD_FIB_H

#include <pthread.h>
#include "linux/list.h"
#include "linux/list_rcu.h"
#include "trie.h"

enum fib_type {
	FIB_TYPE_FORWARD = 0,
	FIB_TYPE_LINK,
	FIB_TYPE_LOCAL
};

struct fib_entry {
	uint8_t			prefix[16];
	unsigned int		prefix_len;
	uint8_t			nexthop[16];
	int			port_index; /* -1 means not ixmap interface */
	enum fib_type		type;
	struct list_head	list;
	int			id;
};

struct fib {
	struct trie_tree	tree;
	pthread_mutex_t		mutex;
};

struct fib *fib_alloc();
void fib_release(struct fib *fib);
int fib_route_update(struct fib *fib, int family, enum fib_type type,
	void *prefix, unsigned int prefix_len, void *nexthop,
	int port_index, int id);
int fib_route_delete(struct fib *fib, int family,
	void *prefix, unsigned int prefix_len,
	int id);
struct list_head *fib_lookup(struct fib *fib, int family,
	void *destination);

#endif /* _IXMAPFWD_FIB_H */
