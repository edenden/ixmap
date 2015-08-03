#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <urcu.h>

#include "main.h"
#include "trie.h"

static uint32_t trie_bit(uint32_t *addr, int digit);
static struct trie_node *trie_alloc_node(struct trie_node *parent);
static int _trie_traverse(struct trie_node *node, struct list_head *head);

void trie_init(struct trie_tree *tree)
{
	struct trie_node *node;

	node = &tree->node;

	memset(node, 0, sizeof(struct trie_node));
	node->parent = NULL;
	INIT_LIST_HEAD(&node->head);

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
	INIT_LIST_HEAD(&node->head);

	return node;

err_alloc_node:
	return NULL;
}

static void _trie_traverse(struct trie_node *node, struct list_head *head)
{
	struct trie_node *child;
	int ret, i;

	if(!list_empty(&node->head)){
		/* TBD: search best way */
	}

	for(i = 0; i < 2; i++){
		child = rcu_dereference(node->child[i]);

		if(child != NULL){
			_trie_traverse(child, head);
		}
	}

	return;
}

/* rcu_read_lock needs to be hold by caller from readside */
int trie_traverse(struct trie_tree *tree, unsigned int family_len,
	uint32_t *prefix, unsigned int prefix_len, struct list_head *head)
{
	struct trie_node *node;
	int rest_len, ret;

	node = &tree->node;
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

	_trie_traverse(node, head);
	return 0;

err_noroute_found:
	return -1;
}

static void _trie_delete_all(struct trie_tree *tree, struct trie_node *node)
{
	struct list_head *list;
	struct trie_node *child;
	int i;

	if(!list_empty(&node->head)){
		tree->trie_entry_delete_all($node->head);
	}

	for(i = 0; i < 2; i++){
		child = rcu_dereference(node->child[i]);

		if(child != NULL){
			_trie_delete_all(tree, child);

			rcu_set_pointer(node->child[i], NULL);
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
	uint32_t *destination)
{
	struct trie_node *node;
	struct list_node *head;
	int rest_len, index, ret;

	node = &tree->node;
	head = NULL;
	rest_len = family_len;
	ret = 0;

	while(rest_len > 0){
		rest_len--;
		index = trie_bit(destination, rest_len);

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
	&& list_empty(&node->head)){
		return -1;
	}

	return 0;
}

int trie_add(struct trie_tree *tree, unsigned int family_len,
	uint32_t *prefix, unsigned int prefix_len, unsigned int id,
	struct list_head *node)
{
	struct trie_node *node, *node_parent;
	int rest_len, index, ret;

	node = &tree->node;
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

	ret = tree->trie_entry_insert(&node->head, id, node);
	if(ret < 0)
		goto err_insert;

	return 0;

err_insert:
err_alloc_child:
	_trie_cleanup(&tree->node);
	return -1;
}

int trie_delete(struct trie_tree *tree, unsigned int family_len,
	uint32_t *prefix, unsigned int prefix_len, unsigned int id)
{
	struct trie_node *node, *node_parent;
	int rest_len, index;

	node = &tree->node;
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

	ret = tree->trie_entry_delete(&node->head, id);
	if(ret < 0)
		goto err_delete;

	rest_len = prefix_len;
	while(rest_len > 0){
		index = trie_bit(prefix, (family_len - rest_len));
		rest_len--;

		if(rcu_dereference(node->child[0]) != NULL
		|| rcu_dereference(node->child[1]) != NULL
		|| !list_empty(&node->head)){
			break;
		}

		node_parent = node->parent;
		rcu_set_pointer(node_parent->child[index], NULL);
		synchronize_rcu();
		free(node);
		node = node_parent;
	}

	return 0;

err_delete:
err_noroute_found:
	return -1;
}

