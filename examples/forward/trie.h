#ifndef _TRIE_H
#define _TRIE_H

struct trie_node {
	void		*parent;
	void		*child[2];
	void		*data;
};

struct route {
	uint32_t	prefix;
	unsigned int	prefix_len;
	uint32_t	nexthop;
};

struct routes_list {
	struct route		*route;
	struct routes_list	*next;
};

int trie_lookup_v4_ascii(struct trie_node *root,
	char *destination_a, uint32_t *nexthop);
int trie_add_v4_ascii(struct trie_node *root,
	char *prefix_a, unsigned int prefix_len, char *nexthop_a);
int trie_delete_v4_ascii(struct trie_node *root,
	char *prefix_a, unsigned int prefix_len);
struct trie_node *trie_alloc_node(struct trie_node *parent);
int trie_traverse_v4(struct trie_node *current, uint32_t prefix,
	unsigned int prefix_len, struct routes_list **list);
int trie_lookup_v4(struct trie_node *root,
	uint32_t destination, uint32_t *nexthop);
int trie_add_v4(struct trie_node *root,
	uint32_t prefix, unsigned int prefix_len, uint32_t nexthop);
int trie_delete_v4(struct trie_node *root,
	uint32_t prefix, unsigned int prefix_len);

#endif /* _TRIE_H */
