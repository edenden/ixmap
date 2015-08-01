#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>

#include "main.h"
#include "trie.h"

static uint32_t trie_bit(uint32_t *addr, int digit);
static struct trie_node *trie_alloc_node(struct trie_node *parent);
static int _trie_traverse(struct trie_node *node, struct trie_data_list *list_root);

struct trie_tree *trie_alloc()
{
	struct trie_tree *tree;

	tree = malloc(sizeof(struct trie_tree));
	if(!tree)
		goto err_trie_alloc;

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
	free(tree->node);
	free(tree);

	return;
}

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

static int _trie_traverse(struct trie_node *node, struct trie_data_list *list_root)
{
	struct trie_data_list *list;
	struct trie_node *child;
	void *data;
	int ret, i;

	data = rcu_dereference(node->data);
	if(data){
		list = malloc(sizeof(struct trie_data_list));
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
int trie_traverse(struct trie_tree *tree, unsigned int family_len,
	uint32_t *prefix, unsigned int prefix_len, struct trie_data_list **list_ret)
{
	struct trie_node *node;
	struct trie_data_list list_root, *list, *list_next;
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
		goto err_traverse;
	}

	*list_ret = list_root.next;
	return 0;

err_traverse:
	list = list_root.next;
	while(list){
		list_next = list->next;
		free(list);
		list = list_next;
	}
err_noroute_found:
	*list_ret = NULL;
	return -1;
}

static int _trie_delete_all(struct trie_node *node, struct trie_data_list *list_root)
{
	struct trie_node *child;
	void *data;
	int i;

	data = rcu_dereference(node->data);
	if(data){
		list = malloc(sizeof(struct trie_data_list));
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
			ret = _trie_delete_all(child, list_root);
			if(ret < 0)
				goto err_recursive;

			rcu_set_pointer(node->child[i], NULL);
			synchronize_rcu();
			free(child);
		}
	}

	return 0;

err_recursive:
err_alloc_list:
	return -1;
}

int trie_delete_all(struct trie_tree *tree, struct trie_data_list **list_ret)
{
	struct trie_data_list list_root, *list, *list_next;
	int ret;

	list_root.node = NULL;
	list_root.next = NULL;
	list_root.last = &list_root;

	ret = _trie_delete_all(tree->node, &list_root);
	if(ret < 0){
		goto err_delete_all;
	}

	*list_ret = list_root.next;
	return 0;

err_delete_all:
	list = list_root.next;
	while(list){
		list_next = list->next;
		free(list);
		list = list_next;
	}

	*list_ret = NULL;
	return -1;
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

static int _trie_cleanup(struct trie_node *node)
{
	struct trie_node *child;
	int ret, i;

	for(i = 0; i < 2; i++){
		child = rcu_dereference(node->child[i]);

		if(child != NULL){
			ret = _trie_cleanup(child);
			if(ret < 0)
				rcu_set_pointer(node->child[i], NULL);
				synchronize_rcu();
				free(child);
			}
		}
	}

	/* Cleanup stub node */
	if(!rcu_dereference(node->child[0])
	&& !rcu_dereference(node->child[1])
	&& !rcu_dereference(node->data)){
		return -1;
	}

	return 0;
}

int trie_add(struct trie_tree *tree, unsigned int family_len,
	uint32_t *prefix, unsigned int prefix_len, void **data)
{
	struct trie_node *node, *node_parent;
	int rest_len, index;

	node = tree->node;
	rest_len = prefix_len;

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

	rcu_xchg_pointer(node->data, *data)
	if(*data){
		synchronize_rcu();
	}

	return 0;

err_alloc_child;
	_trie_cleanup(tree->node);
err_alloc_data:
	return -1;
}

int trie_delete(struct trie_tree *tree, unsigned int family_len,
	uint32_t *prefix, unsigned int prefix_len, void **data)
{
	struct trie_node *node, *node_parent;
	int rest_len, index;

	node = tree->node;
	rest_len = prefix_len;

	while(rest_len > 0){
		rest_len--;
		index = trie_bit(prefix, (family_len - (prefix_len - rest_len)));

		node = rcu_dereference(node->child[index]);
		if(node == NULL){
			/* no route found */
			goto err_noroute_found;
                }
	}
	
	rcu_xchg_pointer(node->data, *data);
	if(*data){
		synchronize_rcu();
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

	return 0;

err_noroute_found:
	return -1;
}

