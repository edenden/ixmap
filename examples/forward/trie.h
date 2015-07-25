#ifndef _TRIE_H
#define _TRIE_H

struct trie_node {
	void		*parent;
	void		*child[2];
	void		*data;
};

struct trie {
	pthread_mutex_t		mutex;
	struct trie_node	*node;
};

struct route {
	uint32_t	prefix[4];
	unsigned int	prefix_len;
	void		*data;
};

struct routes_list {
	struct route		*route;
	struct routes_list	*next;
};

void *trie_lookup_ascii(struct trie_node *root, int family,
	char *destination_a);
int trie_add_ascii(struct trie_node *root, int family,
	char *prefix_a, unsigned int prefix_len,
	void *data, unsigned int data_len);
int trie_delete_ascii(struct trie_node *root, int family,
	char *prefix_a, unsigned int prefix_len);
struct trie_node *trie_alloc_node(struct trie_node *parent);
int trie_traverse(struct trie_node *current, int family,
	uint32_t *prefix, unsigned int prefix_len,
	struct routes_list **list);
void *trie_lookup(struct trie_node *root, int family,
	uint32_t *destination);
int trie_add(struct trie_node *root, int family,
	uint32_t *prefix, unsigned int prefix_len,
	void *data, unsigned int data_len);
int trie_delete(struct trie_node *root, int family,
	uint32_t *prefix, unsigned int prefix_len);

#endif /* _TRIE_H */
