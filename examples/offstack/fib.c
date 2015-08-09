#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <netinet/ip.h>

#include "linux/list.h"
#include "linux/list_rcu.h"
#include "main.h"
#include "fib.h"

static int fib_entry_insert(struct list_head *head, unsigned int id,
	struct list_head *list);
static int fib_entry_delete(struct list_head *head, unsigned int id);
static void fib_entry_delete_all(struct list_head *head);

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

int fib_route_update(struct fib *fib, int family,
	uint32_t *prefix, unsigned int prefix_len,
	uint32_t *nexthop, unsigned int port_index,
	enum fib_type type, unsigned int id)
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
	unsigned int id)
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