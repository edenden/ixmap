#ifndef _TRIE_H
#define _TRIE_H

struct trie_node {
	void			*parent;
	void			*child[2];
	struct list_node	head;
};

struct trie_tree {
	struct trie_node	node;
};

#endif /* _TRIE_H */
