#ifndef _TRIE_H
#define _TRIE_H

struct trie_node {
	void		*parent;
	void		*child[2];
	void		*data;
};

struct trie_tree {
	struct trie_node	*node;
};

struct trie_data_list {
	void			*data;
	struct trie_data_list	*next;
	struct trie_data_list	*last;
};

#endif /* _TRIE_H */
