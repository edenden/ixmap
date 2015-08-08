#ifndef _TRIE_H
#define _TRIE_H

struct trie_node {
	void			*parent;
	void			*child[2];
	struct list_node	head;
	int			index;
};

struct trie_tree {
	struct trie_node	node;
	void			(*trie_entry_insert)();
	void			(*trie_entry_delete)();
	void			(*trie_entry_delete_all)();
	void			(*trie_entry_dump)();
};

#endif /* _TRIE_H */
