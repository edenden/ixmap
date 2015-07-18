#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>

#include "main.h"
#include "trie.h"

int main()
{
	struct trie_node *root;
	uint32_t nexthop = 0;
	char nexthop_a[256];
	struct gamt_map *gamt;

	root = trie_alloc_node(NULL);

	trie_add_v4_ascii(root, "10.0.0.0", 24, "0.0.0.1");
	trie_add_v4_ascii(root, "10.1.0.0", 24, "0.0.0.2");
	trie_add_v4_ascii(root, "10.0.0.0", 8, "0.0.0.3");

	trie_lookup_v4_ascii(root, "10.1.0.1", &nexthop);
	nexthop = htonl(nexthop);
	inet_ntop(AF_INET, &nexthop, nexthop_a, 256);
	printf("nexthop = %s\n", nexthop_a);

	trie_delete_v4_ascii(root, "10.1.0.0", 24);

	trie_lookup_v4_ascii(root, "10.1.0.1", &nexthop);
	nexthop = htonl(nexthop);
	inet_ntop(AF_INET, &nexthop, nexthop_a, 256);
	printf("nexthop = %s\n", nexthop_a);

	gamt = gamt_alloc(root, 32, 6);

	return 0;
}

int trie_lookup_v4_ascii(struct trie_node *root,
	char *destination_a, uint32_t *nexthop)
{
	uint32_t destination;

	inet_pton(AF_INET, destination_a, &destination);
	destination = ntohl(destination);
	return trie_lookup_v4(root, destination, nexthop);
}

int trie_add_v4_ascii(struct trie_node *root,
	char *prefix_a, unsigned int prefix_len, char *nexthop_a)
{
	uint32_t prefix, nexthop;

	inet_pton(AF_INET, prefix_a, &prefix);
	prefix = ntohl(prefix);
	inet_pton(AF_INET, nexthop_a, &nexthop);
	nexthop = ntohl(nexthop);
	return trie_add_v4(root, prefix, prefix_len, nexthop);
}

int trie_delete_v4_ascii(struct trie_node *root,
	char *prefix_a, unsigned int prefix_len)
{
	uint32_t prefix;

	inet_pton(AF_INET, prefix_a, &prefix);
	prefix = ntohl(prefix);
	return trie_delete_v4(root, prefix, prefix_len);
}

struct trie_node *trie_alloc_node(struct trie_node *parent)
{
	struct trie_node *node;

	node = malloc(sizeof(struct trie_node));
	memset(node, 0, sizeof(struct trie_node));

	node->parent = parent;

	return node;
}

int trie_traverse_v4(struct trie_node *current, uint32_t prefix,
	unsigned int prefix_len, struct routes_list **list)
{
	struct route *route;
	struct routes_list *list_new, *list_current;
	int i;

	prefix_len++;

	for(i = 0; i < 2; i++){
		if(current->child[i] != NULL){
			prefix |= i << (32 - (prefix_len));
			trie_traverse_v4(current->child[i], prefix, prefix_len, list);
		}
	}

	if(current->data != NULL){
		route = malloc(sizeof(struct route));
		list_new = malloc(sizeof(struct routes_list));

		route->prefix = prefix;
		route->prefix_len = prefix_len;
		route->nexthop = *(uint32_t *)current->data;

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

int trie_lookup_v4(struct trie_node *root,
	uint32_t destination, uint32_t *nexthop)
{
	struct trie_node *current;
	uint32_t addr, nexthop_last;
	int len_rest, index, ret;

	current = root;
	addr = destination;
	nexthop_last = 0;
	len_rest = 32;
	ret = 0;

	while(len_rest > 0){
		len_rest--;
		index = (addr >> len_rest) & 0x1;

		if(current->child[index] == NULL){
			if(!nexthop_last){
				/* no route found */
				ret = -1;
				goto out;
			}

			break;
		}

		current = current->child[index];

		if(current->data != NULL){
			nexthop_last = *(uint32_t *)(current->data);
		}
	}

	*nexthop = nexthop_last;
out:
	return ret;
}

int trie_add_v4(struct trie_node *root,
	uint32_t prefix, unsigned int prefix_len, uint32_t nexthop)
{
	struct trie_node *current;
	uint32_t addr;
	int len_rest, index;

	current = root;
	addr = prefix;
	len_rest = prefix_len;

	while(len_rest > 0){
		len_rest--;
		index = (addr >> (32 - (prefix_len - len_rest))) & 0x1;
		
		if(current->child[index] == NULL){
			current->child[index] = trie_alloc_node(current);
		}

		current = current->child[index];
	}

	if(current->data == NULL){
		current->data = malloc(sizeof(uint32_t));
	}

	*(uint32_t *)(current->data) = nexthop;
	return 0;
}

int trie_delete_v4(struct trie_node *root,
	uint32_t prefix, unsigned int prefix_len)
{
	struct trie_node *current;
	uint32_t addr;
	int len_rest, index, ret;

	current = root;
	addr = prefix;
	len_rest = prefix_len;
	ret = 0;
	
	while(len_rest > 0){
		len_rest--;
		index = (addr >> (32 - (prefix_len - len_rest))) & 0x1;

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
		index = (addr >> (32 - len_rest)) & 0x1;
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

