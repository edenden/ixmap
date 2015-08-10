#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include "linux/list.h"
#include "linux/list_rcu.h"
#include "main.h"
#include "fib.h"

static int fib_entry_insert(struct list_head *head, unsigned int id,
	struct list_head *list);
static int fib_entry_delete(struct list_head *head, unsigned int id);
static void fib_entry_delete_all(struct list_head *head);

#ifdef DEBUG
static void fib_update_print(int family, enum fib_type type,
	uint32_t *prefix, unsigned int prefix_len, uint32_t *nexthop,
	int port_index, int id);
static void fib_delete_print(int family, uint32_t *prefix,
	unsigned int prefix_len, int id);
#endif

#ifdef DEBUG
static void fib_update_print(int family, enum fib_type type,
	uint32_t *prefix, unsigned int prefix_len, uint32_t *nexthop,
	int port_index, int id)
{
	char prefix_a[128];
	char nexthop_a[128];
	char family_a[128];
	char type_a[128];

	printf("fib update:\n");

	switch(family){
	case AF_INET:
		strcpy(family_a, "AF_INET");
		break;
	case AF_INET6:
		strcpy(family_a, "AF_INET6");
		break;
	default:
		printf("UNKNOWN");
		break;
	}
	printf("\tFAMILY: %s\n", family_a);

	switch(type){
	case FIB_TYPE_FORWARD:
		strcpy(type_a, "FIB_TYPE_FORWARD");
		break;
	case FIB_TYPE_LINK:
		strcpy(type_a, "FIB_TYPE_LINK");
		break;
	case FIB_TYPE_LOCAL:
		strcpy(type_a, "FIB_TYPE_LOCAL");
		break;
	default:
		break;
	}
	printf("\tTYPE: %s\n", type_a);

	inet_ntop(family, prefix, prefix_a, sizeof(prefix_a));
	printf("\tPREFIX: %s/%d\n", prefix_a, prefix_len);

	inet_ntop(family, nexthop, nexthop_a, sizeof(nexthop_a));
	printf("\tNEXTHOP: %s\n", nexthop_a);

	printf("\tPORT: %d\n", port_index);
	printf("\tID: %d\n", id);

	return;
}

static void fib_delete_print(int family, uint32_t *prefix,
	unsigned int prefix_len, int id)
{
	char prefix_a[128];
	char family_a[128];

	printf("fib update:\n");

	switch(family){
	case AF_INET:
		strcpy(family_a, "AF_INET");
		break;
	case AF_INET6:
		strcpy(family_a, "AF_INET6");
		break;
	default:
		printf("UNKNOWN");
		break;
	}
	printf("\tFAMILY: %s\n", family_a);

	inet_ntop(family, prefix, prefix_a, sizeof(prefix_a));
	printf("\tPREFIX: %s/%d\n", prefix_a, prefix_len);

	printf("\tID: %d\n", id);

	return;
}
#endif

struct fib *fib_alloc()
{
        struct fib *fib;

	fib = malloc(sizeof(struct fib));
	if(!fib)
		goto err_fib_alloc;

	trie_init(&fib->tree);
	fib->tree.trie_entry_delete = fib_entry_delete;
	fib->tree.trie_entry_insert = fib_entry_insert;
	fib->tree.trie_entry_delete_all = fib_entry_delete_all;

	pthread_mutex_init(&fib->mutex, NULL);
	return fib;

err_fib_alloc:
	return NULL;
}

void fib_release(struct fib *fib)
{
	trie_delete_all(&fib->tree);
	free(fib);
	return;
}

static int fib_entry_insert(struct list_head *head, unsigned int id,
	struct list_head *list)
{
	struct fib_entry *entry;

	list_for_each_entry_rcu(entry, head, list){
		if(entry->id == id){
			goto err_entry_exist;
		}
	}

	list_add_rcu(list, head);
	return 0;

err_entry_exist:
	return -1;
}

static int fib_entry_delete(struct list_head *head, unsigned int id)
{
	struct fib_entry *entry;

	list_for_each_entry_rcu(entry, head, list){
		if(entry->id == id){
			list_del_rcu(&entry->list);
			free(entry);
			return 0;
		}
	}

	return -1;
}

static void fib_entry_delete_all(struct list_head *head)
{
	struct fib_entry *entry;

	list_for_each_entry_rcu(entry, head, list){
		list_del_rcu(&entry->list);
		free(entry);
	}

	return;
}

int fib_route_update(struct fib *fib, int family, enum fib_type type,
	uint32_t *prefix, unsigned int prefix_len, uint32_t *nexthop,
	int port_index, int id)
{
	struct fib_entry *entry;
	unsigned int family_len;
	int ret;

	switch(family){
	case AF_INET:
		family_len = 32;
		break;
	case AF_INET6:
		family_len = 128;
		break;
	default:
		goto err_invalid_family;
		break;
	}

	entry = malloc(sizeof(struct fib_entry));
	if(!entry)
		goto err_alloc_entry;

	memcpy(entry->nexthop, nexthop, family_len >> 3);
	memcpy(entry->prefix, prefix, family_len >> 3);
	entry->prefix_len	= prefix_len;
	entry->port_index	= port_index;
	entry->type		= type;
	entry->id		= id;

#ifdef DEBUG
	fib_update_print(family, type, prefix, prefix_len,
		nexthop, port_index, id);
#endif

	ixmapfwd_mutex_lock(&fib->mutex);
	ret = trie_add(&fib->tree, family_len,
		prefix, prefix_len, id, &entry->list);
	if(ret < 0)
		goto err_trie_add;
	ixmapfwd_mutex_unlock(&fib->mutex);

	return 0;

err_trie_add:
	ixmapfwd_mutex_unlock(&fib->mutex);
	free(entry);
err_alloc_entry:
err_invalid_family:
	return -1;
}

int fib_route_delete(struct fib *fib, int family,
	uint32_t *prefix, unsigned int prefix_len,
	int id)
{
	unsigned int family_len;
	int ret;

	switch(family){
	case AF_INET:
		family_len = 32;
		break;
	case AF_INET6:
		family_len = 128;
		break;
	default:
		goto err_invalid_family;
		break;
	}

#ifdef DEBUG
	fib_delete_print(family, prefix, prefix_len, id);
#endif

	ixmapfwd_mutex_lock(&fib->mutex);
	ret = trie_delete(&fib->tree, family_len,
		prefix, prefix_len, id);
	if(ret < 0)
		goto err_trie_delete;
	ixmapfwd_mutex_unlock(&fib->mutex);

	return 0;

err_trie_delete:
	ixmapfwd_mutex_unlock(&fib->mutex);
err_invalid_family:
	return -1;
}

struct list_head *fib_lookup(struct fib *fib, int family,
	uint32_t *destination)
{
	struct list_head *head;
	unsigned int family_len;

	switch(family){
	case AF_INET:
		family_len = 32;
		break;
	case AF_INET6:
		family_len = 128;
		break;
	default:
		goto err_invalid_family;
		break;
	}

	head = trie_lookup(&fib->tree, family_len, destination);
	if(!head)
		goto err_trie_lookup;

	return head;

err_trie_lookup:
err_invalid_family:
	return NULL;
}
