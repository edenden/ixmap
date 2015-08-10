#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <urcu.h>

#include "main.h"
#include "trie.h"

static inline int trie_bit(void *addr, int digit);
static struct trie_node *trie_alloc_node(struct trie_node *parent, int index);
static void _trie_traverse(struct trie_tree *tree, struct trie_node *node);
static void _trie_delete_all(struct trie_tree *tree, struct trie_node *node);
static void _trie_cleanup(struct trie_node *node, int index);

void trie_init(struct trie_tree *tree)
{
	struct trie_node *node;

	node = &tree->node;

	memset(node, 0, sizeof(struct trie_node));
	node->parent = NULL;
	INIT_LIST_HEAD(&node->head);

	return;
}

static inline int trie_bit(void *addr, int digit)
{
	int index, shift;

	index = digit >> 3;
	shift = (8 - 1) - digit % 8;

	return (((uint8_t *)addr)[index] >> shift) & 0x1;
}

static struct trie_node *trie_alloc_node(struct trie_node *parent, int index)
{
	struct trie_node *node;

	node = malloc(sizeof(struct trie_node));
	if(!node)
		goto err_alloc_node;

	memset(node, 0, sizeof(struct trie_node));
	node->parent = parent;
	node->index = index;
	INIT_LIST_HEAD(&node->head);

	return node;

err_alloc_node:
	return NULL;
}

static void _trie_traverse(struct trie_tree *tree, struct trie_node *node)
{
	struct trie_node *child;
	int i;

	if(!list_empty(&node->head)){
		tree->trie_entry_dump(&node->head);
	}

	for(i = 0; i < 2; i++){
		child = rcu_dereference(node->child[i]);

		if(child != NULL){
			_trie_traverse(tree, child);
		}
	}

	return;
}

/* rcu_read_lock needs to be hold by caller from readside */
int trie_traverse(struct trie_tree *tree, unsigned int family_len,
	void *prefix, unsigned int prefix_len)
{
	struct trie_node *node;
	int index, i;

	node = &tree->node;

	for(i = 0; i < prefix_len; i++){
		index = trie_bit(prefix, i);

		node = rcu_dereference(node->child[index]);
		if(node == NULL){
			/* no route found */
			goto err_noroute_found;
		}
	}

	_trie_traverse(tree, node);
	return 0;

err_noroute_found:
	return -1;
}

static void _trie_delete_all(struct trie_tree *tree, struct trie_node *node)
{
	struct trie_node *child;
	int i;

	if(!list_empty(&node->head)){
		tree->trie_entry_delete_all(&node->head);
	}

	for(i = 0; i < 2; i++){
		child = rcu_dereference(node->child[i]);

		if(child != NULL){
			_trie_delete_all(tree, child);

			rcu_assign_pointer(node->child[i], NULL);
			synchronize_rcu();
			free(child);
		}
	}

	return;
}

void trie_delete_all(struct trie_tree *tree)
{
	_trie_delete_all(tree, &tree->node);
	return;
}

/* rcu_read_lock needs to be hold by caller from readside */
struct list_head *trie_lookup(struct trie_tree *tree, unsigned int family_len,
	void *destination)
{
	struct trie_node *node;
	struct list_head *head;
	int index, i;

	node = &tree->node;
	head = NULL;

	for(i = 0; i < family_len; i++){
		index = trie_bit(destination, i);

		node = rcu_dereference(node->child[index]);
		if(node == NULL){
			if(!head){
				/* no route found */
				goto err_noroute_found;
			}

			break;
		}

		if(!list_empty(&node->head)){
			head = &node->head;
		}
	}

	return head;

err_noroute_found:
	return NULL;
}

static void _trie_cleanup(struct trie_node *node, int index)
{
	struct trie_node *parent;

	parent = rcu_dereference(node->parent);
	if(parent != NULL){
		if(!rcu_dereference(node->child[!index])
		&& list_empty(&node->head)){
			_trie_cleanup(parent, node->index);
			free(node);
			return;
		}
	}

	rcu_assign_pointer(node->child[index], NULL);
	synchronize_rcu();
	return;
}

int trie_add(struct trie_tree *tree, unsigned int family_len,
	void *prefix, unsigned int prefix_len, unsigned int id,
	struct list_head *list)
{
	struct trie_node *node, *parent;
	int index, ret, i;

	node = &tree->node;

	for(i = 0; i < prefix_len; i++){
		index = trie_bit(prefix, i);
		parent = node;

		node = rcu_dereference(parent->child[index]);
		if(!node){
			node = trie_alloc_node(parent, index);
			if(!node)
				goto err_alloc_child;

			rcu_assign_pointer(parent->child[index], node);
		}
	}

	ret = tree->trie_entry_insert(&node->head, id, list);
	if(ret < 0)
		goto err_insert;

	return 0;

err_alloc_child:
	_trie_cleanup(parent, index);
err_insert:
	return -1;
}

int trie_delete(struct trie_tree *tree, unsigned int family_len,
	void *prefix, unsigned int prefix_len, unsigned int id)
{
	struct trie_node *node, *parent;
	int index, ret, i;

	node = &tree->node;

	for(i = 0; i < prefix_len; i++){
		index = trie_bit(prefix, i);
		parent = node;

		node = rcu_dereference(parent->child[index]);
		if(node == NULL){
			/* no route found */
			goto err_noroute_found;
                }
	}

	ret = tree->trie_entry_delete(&node->head, id);
	if(ret < 0)
		goto err_delete;

	rcu_assign_pointer(parent->child[index], NULL);
	synchronize_rcu();
	free(node);
	_trie_cleanup(parent, index);

	return 0;

err_delete:
err_noroute_found:
	return -1;
}

