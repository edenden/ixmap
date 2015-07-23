#ifndef _HASH_H
#define _HASH_H

#define HASH_SIZE 100000
#define HASH_COLLISION 5

struct hash_entry {
	void			*key;
	unsigned int		key_len;
	void			*value;
};

struct hash {
	struct hash_entry	*entries[HASH_SIZE];
};

int hash_add(struct hash *hash, void *key, int key_len,
	void *value, int value_len);
int hash_delete(struct hash *hash, void *key, int key_len);
void hash_delete_walk(struct hash *hash);
void *hash_lookup(struct hash *hash, void *key, int key_len);

#endif /* _HASH_H */
