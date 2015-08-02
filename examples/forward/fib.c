#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct fib *fib_alloc()
{
        struct fib *fib;

	fib = malloc(sizeof(struct fib));
	if(!fib)
		goto err_fib_alloc;

	trie_init(&fib->tree);
	pthread_mutex_init(&fib->mutex, NULL);
	return fib;

err_fib_alloc:
	return NULL;
}

void fib_release(struct fib *fib)
{
	struct list_node head, *list;
	struct fib_entry *entry;

	list_init(&head);
	trie_delete_all(fib->tree, &head);

	list_for_each_safe(list, &head){
		entry = list_entry(list, struct fib_entry, node);
		free(entry);
	}

	free(fib);
	return;
}

int fib_entry_insert(void *data, void *data_orig, void **data_ret)
{
	struct fib_entry *entry, *entry_orig;

	entry = data;
	entry_orig = data_orig;
}

int fib_entry_delete()
{

}

int fib_route_update(struct fib *fib, int family,
	uint32_t *prefix, unsigned int prefix_len,
	uint32_t *nexthop, unsigned int port_index,
	enum fib_type type, unsigned int id)
{
	struct fib_entry *entry, *entry_next;
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

	memcpy(entry->nexthop, nexthop, ALIGN(family_len, 8) >> 3);
	memcpy(entry->prefix, prefix, ALIGN(family_len, 8) >> 3);
	entry->prefix_len	= prefix_len;
	entry->port_index	= port_index;
	entry->type		= type;
	entry->flag		= FIB_FLAG_UNICAST;
	entry->next		= NULL;
	entry->id		= id;

	ixmapfwd_mutex_lock(&fib->mutex);
	ret = trie_add(fib->trie_root, family_len,
		prefix, prefix_len, &entry->node, fib_entry_insert);
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
	uint32_t *prefix, unsigned int prefix_len, unsigned int id)
{
	struct fib_entry *entry = NULL, *entry_next;
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
	ret = trie_delete(fib->trie_root, family_len,
		prefix, prefix_len, id, fib_entry_delete);
	if(ret < 0)
		goto err_trie_delete;
	ixmapfwd_mutex_unlock(&fib->mutex);

	return 0;

err_trie_delete:
	ixmapfwd_mutex_unlock(&fib->mutex);
err_invalid_family:
	return -1;
}

struct list_node *fib_lookup(struct fib *fib, int family,
	uint32_t *destination)
{
	struct list_node *head;
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

	head = trie_lookup(fib->trie_root, family_len, destination);
	if(!head)
		goto err_trie_lookup;

	return head;

err_trie_lookup:
err_invalid_family:
	return NULL;
}
