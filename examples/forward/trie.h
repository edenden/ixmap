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

struct node_list {
	struct trie_node	*node;
	struct node_list	*next;
	struct node_list	*last;
};

#endif /* _TRIE_H */