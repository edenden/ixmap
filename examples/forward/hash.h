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

#endif /* _HASH_H */
