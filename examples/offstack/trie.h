#ifndef _IXMAPFWD_TRIE_H
#define _IXMAPFWD_TRIE_H

#include "linux/list.h"
#include "linux/list_rcu.h"

struct trie_node {
	void			*parent;
	void			*child[2];
	struct list_head	head;
	int			index;
};

struct trie_tree {
	struct trie_node	node;
	int			(*trie_entry_insert)(
				struct list_head *,
				unsigned int,
				struct list_head *
				);
	int			(*trie_entry_delete)(
				struct list_head *,
				unsigned int
				);
	void			(*trie_entry_delete_all)(
				struct list_head *	
				);
	void			(*trie_entry_dump)(
				struct list_head *
				);
};

void trie_init(struct trie_tree *tree);
int trie_traverse(struct trie_tree *tree, unsigned int family_len,
	uint32_t *prefix, unsigned int prefix_len);
void trie_delete_all(struct trie_tree *tree);
struct list_head *trie_lookup(struct trie_tree *tree, unsigned int family_len,
	uint32_t *destination);
int trie_add(struct trie_tree *tree, unsigned int family_len,
	uint32_t *prefix, unsigned int prefix_len, unsigned int id,
	struct list_head *list);
int trie_delete(struct trie_tree *tree, unsigned int family_len,
	uint32_t *prefix, unsigned int prefix_len, unsigned int id);

#endif /* _IXMAPFWD_TRIE_H */
