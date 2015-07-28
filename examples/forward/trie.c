#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>

#include "main.h"
#include "trie.h"

static uint32_t trie_bit(uint32_t *addr, int digit);
static struct trie_node *trie_alloc_node(struct trie_node *parent);
static int _trie_traverse(struct trie_node *node, struct node_data_list *list_root);

static uint32_t trie_bit(uint32_t *addr, int digit)
{
	int index, shift;

	index = digit >> 5;
	shift = digit % 32;

	return (addr[index] >> shift) & 0x1;
}

static struct trie_node *trie_alloc_node(struct trie_node *parent)
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

static int _trie_traverse(struct trie_node *node, struct node_data_list *list_root)
{
	struct node_data_list *list;
	struct trie_node *child;
	void *data;
	int ret, i;

	data = rcu_dereference(node->data);
	if(data){
		list = malloc(sizeof(struct node_data_list));
		if(!list)
			goto err_alloc_list;

		list->data = data;
		list->next = NULL;

		list_root->last->next = list;
		list_root->last = list;
	}

	for(i = 0; i < 2; i++){
		child = rcu_dereference(node->child[i]);

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
struct node_data_list *trie_traverse(struct trie_tree *tree, unsigned int family_len,
	uint32_t *prefix, unsigned int prefix_len)
{
	struct trie_node *node;
	struct node_data_list list_root, *list, *list_next;
	int rest_len, ret;

	node = tree->node;
	rest_len = prefix_len;
	list_root.node = NULL;
	list_root.next = NULL;
	list_root.last = &list_root;

	while(rest_len > 0){
		rest_len--;
		index = trie_bit(prefix, (family_len - (prefix_len - rest_len)));

		node = rcu_dereference(node->child[index]);
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

static void _trie_delete_all(struct trie_node *node)
{
	struct trie_node *child;
	void *data;
	int i;

	data = rcu_dereference(node->data);
	if(data){
		free(data);
	}

	for(i = 0; i < 2; i++){
		child = rcu_dereference(node->child[i]);

		if(child != NULL){
			_trie_delete_all(child);
		}
	}

	/* Do not delete top node */
	if(node->parent)
		free(node);

	return;
}

static void trie_delete_all(struct trie_tree *tree)
{
	ixmapfwd_mutex_lock(&tree->mutex);
	_trie_delete_all(tree->node);
	ixmapfwd_mutex_unlock(&tree->mutex);
}

/* rcu_read_lock needs to be hold by caller from readside */
void *trie_lookup(struct trie_tree *tree, unsigned int family_len,
	uint32_t *destination)
{
	struct trie_node *node;
	void *data, *data_ptr;
	int rest_len, index, ret;

	node = tree->node;
	data = NULL;
	rest_len = family_len;
	ret = 0;

	while(rest_len > 0){
		rest_len--;
		index = trie_bit(destination, rest_len);

		node = rcu_dereference(node->child[index]);
		if(node == NULL){
			if(!data){
				/* no route found */
				goto err_noroute_found;
			}

			break;
		}

		data_ptr = rcu_dereference(node->data);
		if(data_ptr != NULL){
			data = data_ptr;
		}
	}

	return data;

err_noroute_found:
	return NULL;
}

int trie_add(struct trie_tree *tree, unsigned int family_len,
	uint32_t *prefix, unsigned int prefix_len,
	void *data, unsigned int data_len)
{
	struct trie_node *node, *node_parent;
	void *data_new;
	int rest_len, index;

	data_new = malloc(data_len);
	if(!data_new)
		goto err_alloc_data;
	memcpy(data_new, data, data_len);

	node = tree->node;
	rest_len = prefix_len;
	ixmapfwd_mutex_lock(&tree->mutex);

	while(rest_len > 0){
		rest_len--;
		index = trie_bit(prefix, (family_len - (prefix_len - rest_len)));
		node_parrent = node;

		node = rcu_dereference(node->child[index]);
		if(node == NULL){
			node = trie_alloc_node(parrent);
			if(!node)
				goto err_alloc_child;

			rcu_set_pointer(node_parrent->child[index], node);
		}

	}

	rcu_xchg_pointer(node->data, data_new)
	if(data_new){
		synchronize_rcu();
		free(data_new);
	}

	ixmapfwd_mutex_unlock(&trie->mutex);
	return 0;

err_alloc_child;
	/* TBD: free allocated nodes */
	ixmapfwd_mutex_unlock(&tree->mutex);
	free(data_new);
err_alloc_data:
	return -1;
}

int trie_delete(struct trie_tree *tree, unsigned int family_len,
	uint32_t *prefix, unsigned int prefix_len)
{
	struct trie_node *node, *node_parent;
	void *data = NULL;
	int rest_len, index;

	node = tree->node;
	rest_len = prefix_len;
	ixmapfwd_mutex_lock(&tree->mutex);

	while(rest_len > 0){
		rest_len--;
		index = trie_bit(prefix, (family_len - (prefix_len - rest_len)));

		node = rcu_dereference(node->child[index]);
		if(node == NULL){
			/* no route found */
			goto err_noroute_found;
                }
	}
	
	rcu_xchg_pointer(node->data, data);
	if(data){
		synchronize_rcu();
		free(data);
	}
	rest_len = prefix_len;

	while(rest_len > 0){
		index = trie_bit(prefix, (family_len - rest_len));
		rest_len--;

		if(rcu_dereference(node->child[0]) != NULL
		|| rcu_dereference(node->child[1]) != NULL
		|| rcu_dereference(node->data) != NULL){
			break;
		}

		node_parent = node->parent;
		rcu_set_pointer(node_parent->child[index], NULL);
		synchronize_rcu();
		free(node);
		node = node_parent;
	}

	ixmapfwd_mutex_unlock(&tree->mutex);
	return 0;

err_noroute_found:
	ixmapfwd_mutex_unlock(&tree->mutex);
	return -1;
}

struct trie_tree *trie_alloc()
{
	struct trie_tree *tree;

	tree = malloc(sizeof(struct trie_tree));
	if(!tree)
		goto err_trie_alloc;

	memset(tree, 0, sizeof(struct trie_tree));
	pthread_mutex_init(&tree->mutex, NULL);
	tree->node = trie_alloc_node(NULL);
	if(!tree->node)
		goto err_trie_alloc_node;

	return tree;

err_trie_alloc_node:
	free(tree);
err_trie_alloc:
	return NULL;
}

void trie_release(struct trie_tree *tree)
{
	trie_delete_all(tree);
	free(tree->node);
	free(tree);
	return;
}
