#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct fib *fib_alloc()
{
        struct fib *fib;

	fib = malloc(sizeof(struct fib));
	if(!fib)
		goto err_fib_alloc;

	fib->tree = trie_alloc();
	if(!fib->tree)
		goto err_trie_alloc;

	pthread_mutex_init(&fib->mutex, NULL);
	return fib;

err_trie_alloc:
	free(fib);
err_fib_alloc:
	return NULL;
}

void fib_release(struct fib *fib)
{
	struct node_data_list *list = NULL, *list_next;
	struct fib_entry *entry, *entry_next;
	int ret = -1;

	while(ret < 0){
		ret = trie_delete_all(fib->tree, &list);
	}

	while(list){
		list_next = list->next;
		entry = list->data;

		while(entry){
			entry_next = rcu_dereference(entry->next);
			free(entry);
			entry = entry_next;
		}

		list = list_next;
	}

	trie_release(fib->tree);
	free(fib);
	return;
}

int fib_route_update(struct fib *fib, int family,
	uint32_t *prefix, unsigned int prefix_len,
	uint32_t *nexthop, unsigned int port_index,
	enum fib_type type, unsigned int id)
{
	struct fib_entry *entry, *entry_exist;
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
	entry_exist = trie_lookup(fib->trie_root, family_len, prefix);
	if(entry_exist){
		struct fib_entry *entry_prev;

		while(entry_exist){
			entry_prev = entry_exist;
			entry_exist = rcu_dereference(entry_exist->next);

			if(entry_prev->id == entry->id)
				goto err_route_exist;
		}
		rcu_set_pointer(entry_prev->next, entry);
	}else{
		struct fib_entry *entry_next;

		ret = trie_add(fib->trie_root, family_len,
			prefix, prefix_len, &entry);
		if(ret < 0)
			goto err_trie_add;

		while(entry){
			entry_next = rcu_dereference(entry->next);
			free(entry);
			entry = entry_next;
		}
	}
	ixmapfwd_mutex_unlock(&fib->mutex);

	return 0;

err_route_exist:
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
	struct fib_entry *entry = NULL;
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
	/* TBD: only delete the entry its id has match */
	ret = trie_delete(fib->trie_root, family_len, prefix, prefix_len, &entry);
	if(ret < 0)
		goto err_trie_delete;

	if(entry){
		struct fib_entry *entry_next;

		while(entry){
			entry_next = rcu_dereference(entry->next);
			free(entry);
			entry = entry_next;
		}
	}
	ixmapfwd_mutex_unlock(&fib->mutex);

	return 0;

err_trie_delete:
	ixmapfwd_mutex_unlock(&fib->mutex);
err_invalid_family:
	return -1;
}

struct fib_entry *fib_lookup(struct fib *fib, int family,
	uint32_t *destination)
{
	struct fib_entry *entry;
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

	entry = trie_lookup(fib->trie_root, family_len, destination);
	if(!entry)
		goto err_trie_lookup;

err_trie_lookup:
err_invalid_family:
	return NULL;
}
