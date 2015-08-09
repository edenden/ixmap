#ifndef _IXMAPFWD_NEIGH_H
#define _IXMAPFWD_NEIGH_H

#include <linux/if_ether.h>
#include <pthread.h>
#include "hash.h"

struct neigh_table {
	struct hash_table	table;
	pthread_mutex_t		mutex;
};

struct neigh_entry {
	uint8_t			dst_mac[ETH_ALEN];
	struct hash_entry	hash;
};

struct neigh_table *neigh_alloc();
void neigh_release(struct neigh_table *neigh);
int neigh_add(struct neigh_table *neigh, int family,
	uint32_t *dst_addr, uint8_t *mac_addr);
int neigh_delete(struct neigh_table *neigh, int family,
	uint32_t *dst_addr);
struct neigh_entry *neigh_lookup(struct neigh_table *neigh, int family,
	uint32_t *dst_addr);

#endif /* _IXMAPFWD_NEIGH_H */
