#ifndef _HASH_H
#define _HASH_H

#define HASH_SIZE 100000

#define hash_entry(ptr, type, member)	\
	container_of(ptr, type, member)

struct hash_entry {
	void			*key;
	unsigned int		key_len;
	struct hlist_node	list;
};

struct hash_table {
	struct hlist_head	head[HASH_SIZE];
	void			(*hash_entry_delete)();
};

int hash_add(struct hash *hash, void *key, int key_len,
	void *value, int value_len);
int hash_delete(struct hash *hash, void *key, int key_len);
void hash_delete_walk(struct hash *hash);
void *hash_lookup(struct hash *hash, void *key, int key_len);

#endif /* _HASH_H */
