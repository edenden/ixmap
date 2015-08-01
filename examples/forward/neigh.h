#ifndef _IXMAPFWD_NEIGH_H
#define _IXMAPFWD_NEIGH_H

struct neigh_table {
	struct hash_table	*table;
	pthread_mutex_t		mutex;
};

struct neigh_entry {
	uint8_t dst_mac[ETH_ALEN];
};

#endif /* _IXMAPFWD_NEIGH_H */
