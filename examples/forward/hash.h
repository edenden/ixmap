#ifndef _HASH_H
#define _HASH_H

#define HASH_SIZE 100000

struct hash_entry {
	struct hash_entry	*next;
	void			*key;
	unsigned int		key_len;
	void			*value;
};

struct hash_root {
	struct hash_entry	*entries[HASH_SIZE];
};


unsigned int hash_add(struct hash_root *root, void *key, int key_len,
	void *value, int value_len);
int hash_delete(struct hash_root *root, void *key, int key_len);
void hash_delete_walk(struct hash_root *root);
void *hash_lookup(struct hash_root *root, void *key, int key_len);

#endif /* _HASH_H */
