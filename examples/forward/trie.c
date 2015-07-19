#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>

#include "main.h"
#include "trie.h"

static void trie_betole(int family, uint32_t *value);
static void trie_letobe(int family, uint32_t *value);
static uint32_t trie_bit(uint32_t *addr, int digit);
static void trie_bit_or(uint32_t *addr, int digit, int value);

int main()
{
	struct trie_node *root;
	char *nexthop;

	root = trie_alloc_node(NULL);

	trie_add_ascii(root, AF_INET, "10.0.0.0", 24, "0.0.0.1", strlen("0.0.0.1"));
	trie_add_ascii(root, AF_INET, "10.1.0.0", 24, "0.0.0.2", strlen("0.0.0.2"));
	trie_add_ascii(root, AF_INET, "10.0.0.0", 8, "0.0.0.3", strlen("0.0.0.3"));

	trie_lookup_ascii(root, AF_INET, "10.1.0.1", (void **)&nexthop);
	printf("nexthop = %s\n", nexthop);

	trie_delete_ascii(root, AF_INET, "10.1.0.0", 24);

	trie_lookup_ascii(root, AF_INET, "10.1.0.1", (void **)&nexthop);
	printf("nexthop = %s\n", nexthop);

	return 0;
}

static void trie_betole(int family, uint32_t *value)
{
	switch(family){
		case AF_INET:
			value[0] = ntohl(value[0]);
			break;
		case AF_INET6:
			value[0] = ntohl(value[0]);
			value[1] = ntohl(value[1]);
			value[2] = ntohl(value[2]);
			value[3] = ntohl(value[3]);
			break;
		default:
			break;
	}
	return;
}

static void trie_letobe(int family, uint32_t *value)
{
	trie_betole(family, value);
	return;
}

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

int trie_lookup_ascii(struct trie_node *root, int family,
	char *destination_a, void **data)
{
	uint32_t destination[4];

	inet_pton(family, destination_a, destination);
	trie_betole(family, destination);

	return trie_lookup(root, family, destination, data);
}

int trie_add_ascii(struct trie_node *root, int family,
	char *prefix_a, unsigned int prefix_len,
	void *data, unsigned int data_len)
{
	uint32_t prefix[4];

	inet_pton(family, prefix_a, prefix);
	trie_betole(family, prefix);

	return trie_add(root, family, prefix, prefix_len,
		data, data_len);
}

int trie_delete_ascii(struct trie_node *root, int family,
	char *prefix_a, unsigned int prefix_len)
{
	uint32_t prefix[4];

	inet_pton(family, prefix_a, &prefix);
	trie_betole(family, prefix);

	return trie_delete(root, family, prefix, prefix_len);
}

struct trie_node *trie_alloc_node(struct trie_node *parent)
{
	struct trie_node *node;

	node = malloc(sizeof(struct trie_node));
	memset(node, 0, sizeof(struct trie_node));

	node->parent = parent;

	return node;
}

int trie_traverse(struct trie_node *current, int family,
	uint32_t *prefix, unsigned int prefix_len,
	struct routes_list **list)
{
	struct route *route;
	struct routes_list *list_new, *list_current;
	uint32_t prefix_child[4];
	int i, len_family;

	switch(family){
		case AF_INET:
			len_family = 32;
			break;
		case AF_INET6:
			len_family = 128;
			break;
		default:
			break;
	}

	memcpy(prefix_child, prefix, len_family >> 3);
	prefix_len++;
	for(i = 0; i < 2; i++){
		if(current->child[i] != NULL){
			trie_bit_or(prefix_child, len_family - prefix_len, i);
			trie_traverse(current->child[i], family, prefix_child, prefix_len, list);
		}
	}

	if(current->data != NULL){
		route = malloc(sizeof(struct route));
		list_new = malloc(sizeof(struct routes_list));

		memcpy(route->prefix, prefix, len_family >> 3);
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

int trie_lookup(struct trie_node *root, int family,
	uint32_t *destination, void **data)
{
	struct trie_node *current;
	void *data_last;
	int len_rest, index, ret;

	current = root;
	data_last = NULL;
	ret = 0;

	switch(family){
		case AF_INET:
			len_rest = 32;
			break;
		case AF_INET6:
			len_rest = 128;
			break;
		default:
			break;
	}

	while(len_rest > 0){
		len_rest--;
		index = trie_bit(destination, len_rest);

		if(current->child[index] == NULL){
			if(!data_last){
				/* no route found */
				ret = -1;
				goto out;
			}

			break;
		}

		current = current->child[index];

		if(current->data != NULL){
			data_last = current->data;
		}
	}

	*data = data_last;
out:
	return ret;
}

int trie_add(struct trie_node *root, int family,
	uint32_t *prefix, unsigned int prefix_len,
	void *data, unsigned int data_len)
{
	struct trie_node *current;
	int len_rest, len_family, index;

	current = root;
	len_rest = prefix_len;

	switch(family){
		case AF_INET:
			len_family = 32;
			break;
		case AF_INET6:
			len_family = 128;
			break;
		default:
			break;
	}

	while(len_rest > 0){
		len_rest--;
		index = trie_bit(prefix, (len_family - (prefix_len - len_rest)));
		
		if(current->child[index] == NULL){
			current->child[index] = trie_alloc_node(current);
		}

		current = current->child[index];
	}

	if(current->data != NULL){
		free(current->data);
	}

	current->data = malloc(data_len);
	memcpy(current->data, data, data_len);
	return 0;
}

int trie_delete(struct trie_node *root, int family,
	uint32_t *prefix, unsigned int prefix_len)
{
	struct trie_node *current;
	int len_rest, len_family, index, ret;

	current = root;
	len_rest = prefix_len;
	ret = 0;

	switch(family){
		case AF_INET:
			len_family = 32;
			break;
		case AF_INET6:
			len_family = 128;
			break;
		default:
			break;
	}
	
	while(len_rest > 0){
		len_rest--;
		index = trie_bit(prefix, (len_family - (prefix_len - len_rest)));

		if(current->child[index] == NULL){
			/* no route found */
			ret = -1;
			goto out;
                }

		current = current->child[index];
	}
	
	free(current->data);
	current->data = NULL;
	len_rest = prefix_len;

	while(len_rest > 0){
		index = trie_bit(prefix, (len_family - len_rest));
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

out:
	return ret;
}

