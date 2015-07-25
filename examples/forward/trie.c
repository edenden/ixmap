#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>

#include "main.h"
#include "trie.h"

static uint32_t trie_bit(uint32_t *addr, int digit);
static int _trie_traverse(struct trie_node *node, struct node_list *list_root);

static uint32_t trie_bit(uint32_t *addr, int digit)
{
	int index, shift;

	index = digit >> 5;
	shift = digit % 32;

	return (addr[index] >> shift) & 0x1;
}

struct trie_node *trie_alloc_node(struct trie_node *parent)
{
	struct trie_node *node;

	node = malloc(sizeof(struct trie_node));
	if(!node)
		goto err_alloc_node;

	memset(node, 0, sizeof(struct trie_node));
	node->parent = parent;

	return node;

err_alloc_node:
	return NULL;
}

static int _trie_traverse(struct trie_node *node, struct node_list *list_root)
{
	struct node_list *list;
	struct trie_node *child;
	int ret, i;

	if(node->data != NULL){
		list = malloc(sizeof(struct node_list));
		if(!list)
			goto err_alloc_list;

		list->node = node;
		list->next = NULL;

		list_root->last->next = list;
		list_root->last = list;
	}

	for(i = 0; i < 2; i++){
		child = node->child[i];

		if(child != NULL){
			ret = _trie_traverse(child, list_root);
			if(ret < 0)
				goto err_recursive;
		}
	}

	return 0;

err_recursive:
err_alloc_list:
	return -1;
}

/* rcu_read_lock needs to be hold by caller from readside */
struct node_list *trie_traverse(struct trie *trie, unsigned int family_len,
	uint32_t *prefix, unsigned int prefix_len)
{
	struct trie_node *node;
	struct node_list list_root, *list, *list_next;
	int rest_len, ret;

	node = trie->node;
	rest_len = prefix_len;
	list_root.node = NULL;
	list_root.next = NULL;
	list_root.last = &list_root;

	while(rest_len > 0){
		rest_len--;
		index = trie_bit(prefix, (family_len - (prefix_len - rest_len)));

		node = node->child[index];
		if(node == NULL){
			/* no route found */
			goto err_noroute_found;
		}
	}

	ret = _trie_traverse(node, &list_root);
	if(ret < 0){
		list = list_root.next;
		while(list){
			list_next = list->next;
			free(list);
			list = list_next;
		}
		list_root.next = NULL;
	}

err_noroute_found:
	list = list_root.next;
	return list;
}

/* rcu_read_lock needs to be hold by caller from readside */
void *trie_lookup(struct trie *trie, unsigned int family_len,
	uint32_t *destination)
{
	struct trie_node *node;
	void *data, *data_ptr;
	int rest_len, index, ret;

	node = trie->node;
	data = NULL;
	rest_len = family_len;
	ret = 0;

	while(rest_len > 0){
		rest_len--;
		index = trie_bit(destination, rest_len);

		node = node->child[index];
		if(node == NULL){
			if(!data){
				/* no route found */
				goto err_noroute_found;
			}

			break;
		}

		data_ptr = node->data;
		if(data_ptr != NULL){
			data = data_ptr;
		}
	}

	return data;

err_noroute_found:
	return NULL;
}

int trie_add(struct trie *trie, unsigned int family_len,
	uint32_t *prefix, unsigned int prefix_len,
	void *data, unsigned int data_len)
{
	struct trie_node *node, *node_parent;
	void *data_new, *data_ptr;
	int rest_len, index;

	data_new = malloc(data_len);
	if(!data_new)
		goto err_alloc_data;
	memcpy(data_new, data, data_len);

	node = trie->node;
	rest_len = prefix_len;
	ixmapfwd_mutex_lock(&trie->mutex);

	while(rest_len > 0){
		rest_len--;
		index = trie_bit(prefix, (family_len - (prefix_len - rest_len)));
		node_parrent = node;
		node = node->child[index];

		if(node == NULL){
			node = trie_alloc_node(parrent);
			if(!node)
				goto err_alloc_child;

			node_parrent->child[index] = node;
		}

	}

	data_ptr = node->data;
	node->data = data_new;
	if(data_ptr){
		synchronize_rcu();
		free(data_ptr);
	}

	ixmapfwd_mutex_unlock(&trie->mutex);
	return 0;

err_alloc_child;
	/* TBD: free allocated nodes */
	ixmapfwd_mutex_unlock(&trie->mutex);
	free(data_new);
err_alloc_data:
	return -1;
}

int trie_delete(struct trie *trie, unsigned int family_len,
	uint32_t *prefix, unsigned int prefix_len)
{
	struct trie_node *node, *node_parent;
	void *data;
	int rest_len, index;

	node = trie->node;
	rest_len = prefix_len;
	ixmapfwd_mutex_lock(&trie->mutex);

	while(rest_len > 0){
		rest_len--;
		index = trie_bit(prefix, (family_len - (prefix_len - rest_len)));

		node = node->child[index];
		if(node == NULL){
			/* no route found */
			goto err_noroute_found;
                }
	}
	
	data = node->data;
	node->data = NULL;
	synchronize_rcu();
	if(data){
		free(data);
	}
	rest_len = prefix_len;

	while(rest_len > 0){
		index = trie_bit(prefix, (family_len - rest_len));
		rest_len--;

		if(node->child[0] != NULL
		|| node->child[1] != NULL
		|| node->data != NULL){
			break;
		}

		node_parent = node->parent;
		node_parent->child[index] = NULL;
		synchronize_rcu();
		free(node);

		node = node_parent;
	}

	ixmapfwd_mutex_unlock(&trie->mutex);
	return 0;

err_noroute_found:
	ixmapfwd_mutex_unlock(&trie->mutex);
	return -1;
}
