#ifndef _IXMAPFWD_HASH_H
#define _IXMAPFWD_HASH_H

#include "linux/list.h"
#include "main.h"

#define HASH_SIZE (1 << 16)

#define hash_entry(ptr, type, member)	\
	container_of(ptr, type, member)

struct hash_entry {
	struct hlist_node	list;
	void			*key;
	unsigned int		key_len;
};

struct hash_table {
	struct hlist_head	head[HASH_SIZE];
	void			(*hash_entry_delete)(
				struct hash_entry *
				);
};

void hash_init(struct hash_table *table);
int hash_add(struct hash_table *table, void *key, int key_len,
	struct hash_entry *entry_new);
int hash_delete(struct hash_table *table,
	void *key, int key_len);
void hash_delete_all(struct hash_table *table);
struct hash_entry *hash_lookup(struct hash_table *table,
	void *key, int key_len);

#endif /* _IXMAPFWD_HASH_H */
