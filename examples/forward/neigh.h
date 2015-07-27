#ifndef _IXMAPFWD_NEIGH_H
#define _IXMAPFWD_NEIGH_H

struct neigh_table {
	struct hash_root *hash_root;
};

struct neigh_entry {
	uint8_t mac_addr[6];
};

#endif /* _IXMAPFWD_NEIGH_H */
