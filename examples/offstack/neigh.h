#ifndef _IXMAPFWD_NEIGH_H
#define _IXMAPFWD_NEIGH_H

#include <linux/if_ether.h>
#include <pthread.h>
#include "hash.h"

#define GOLDEN_RATIO_PRIME_32 0x9e370001UL
#define GOLDEN_RATIO_PRIME_64 0x9e37fffffffc0001UL

struct neigh_table {
	struct hash_table	table;
	struct ixmap_marea	*area;
};

struct neigh_entry {
	struct hash_entry	hash;
	uint8_t			dst_mac[ETH_ALEN];
	uint32_t		dst_addr[4];
	struct ixmap_marea	*area;
};

struct neigh_table *neigh_alloc(struct ixmap_desc *desc, int family);
void neigh_release(struct neigh_table *neigh);
int neigh_add(struct neigh_table *neigh, int family,
	void *dst_addr, void *mac_addr, struct ixmap_desc *desc);
int neigh_delete(struct neigh_table *neigh, int family,
	void *dst_addr);
struct neigh_entry *neigh_lookup(struct neigh_table *neigh,
	void *dst_addr);

#endif /* _IXMAPFWD_NEIGH_H */
