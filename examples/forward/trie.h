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

struct node_data_list {
	void			*data;
	struct node_data_list	*next;
	struct node_data_list	*last;
};

#endif /* _TRIE_H */
