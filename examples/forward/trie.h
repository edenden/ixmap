#ifndef _TRIE_H
#define _TRIE_H

struct trie_node {
	void			*parent;
	void			*child[2];
	struct list_node	head;
};

struct trie_tree {
	struct trie_node	node;
	void			(*trie_release)();
	void			(*trie_entry_insert)();
	void			(*trie_entry_delete)();
};

#endif /* _TRIE_H */
