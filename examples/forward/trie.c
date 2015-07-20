#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>

#include "main.h"
#include "trie.h"

static uint32_t trie_bit(uint32_t *addr, int digit);
static void trie_bit_or(uint32_t *addr, int digit, int value);

static uint32_t trie_bit(uint32_t *addr, int digit)
{
	int index, shift;

	index = digit >> 5;
	shift = digit % 32;

	return (addr[index] >> shift) & 0x1;
}

static void trie_bit_or(uint32_t *addr, int digit, int value)
{
	int index, shift;

	index = digit >> 5;
	shift = digit % 32;

	addr[index] |= (value & 0x1) << shift;
	return;
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

int trie_traverse(struct trie_node *current, unsigned int family_len,
	uint32_t *prefix, unsigned int prefix_len, struct routes_list **list)
{
	struct route *route;
	struct routes_list *list_new, *list_current;
	uint32_t prefix_child[4];
	int i;

	memcpy(prefix_child, prefix, family_len >> 3);
	prefix_len++;
	for(i = 0; i < 2; i++){
		if(current->child[i] != NULL){
			trie_bit_or(prefix_child, family_len - prefix_len, i);
			trie_traverse(current->child[i], family, prefix_child, prefix_len, list);
		}
	}

	if(current->data != NULL){
		route = malloc(sizeof(struct route));
		list_new = malloc(sizeof(struct routes_list));

		memcpy(route->prefix, prefix, family_len >> 3);
		route->prefix_len = prefix_len;
		route->data = current->data;

		list_new->route = route;
		list_new->next = NULL;

		if(*list == NULL){
			*list = list_new;
		}else{
			list_current = *list;
			while(list_current->next != NULL){
				list_current = list_current->next;
			}

			list_current->next = list_new;
		}
	}

	return 0;
}

void *trie_lookup(struct trie_node *root, unsigned int family_len,
	uint32_t *destination)
{
	struct trie_node *current;
	void *data;
	int len_rest, index, ret;

	current = root;
	data = NULL;
	len_rest = family_len;
	ret = 0;

	while(len_rest > 0){
		len_rest--;
		index = trie_bit(destination, len_rest);

		if(current->child[index] == NULL){
			if(!data){
				/* no route found */
				goto err_noroute_found;
			}

			break;
		}

		current = current->child[index];

		if(current->data != NULL){
			data = current->data;
		}
	}

	return data;

err_noroute_found:
	return NULL;
}

int trie_add(struct trie_node *root, unsigned int family_len,
	uint32_t *prefix, unsigned int prefix_len,
	void *data, unsigned int data_len)
{
	struct trie_node *current;
	int len_rest, index;

	current = root;
	len_rest = prefix_len;

	while(len_rest > 0){
		len_rest--;
		index = trie_bit(prefix, (family_len - (prefix_len - len_rest)));
		
		if(current->child[index] == NULL){
			current->child[index] = trie_alloc_node(current);
			if(!current->child[index])
				goto err_alloc_child;
		}

		current = current->child[index];
	}

	if(current->data != NULL){
		free(current->data);
	}

	current->data = malloc(data_len);
	if(!current->data){
		goto err_alloc_data;
	}

	memcpy(current->data, data, data_len);

	return 0;

err_alloc_child;
	/* TBD: free allocated nodes */
err_alloc_data:
	return -1;
}

int trie_delete(struct trie_node *root, unsigned int family_len,
	uint32_t *prefix, unsigned int prefix_len)
{
	struct trie_node *current;
	int len_rest, index;

	current = root;
	len_rest = prefix_len;

	while(len_rest > 0){
		len_rest--;
		index = trie_bit(prefix, (family_len - (prefix_len - len_rest)));

		if(current->child[index] == NULL){
			/* no route found */
			goto err_noroute_found;
                }

		current = current->child[index];
	}
	
	free(current->data);
	current->data = NULL;
	len_rest = prefix_len;

	while(len_rest > 0){
		index = trie_bit(prefix, (family_len - len_rest));
		len_rest--;

		if(current->child[0] != NULL
		|| current->child[1] != NULL
		|| current->data != NULL){
			break;
		}

		current = current->parent;
		free(current->child[index]);
		current->child[index] = NULL;
	}

	return 0;

err_noroute_found:
	return -1;
}

