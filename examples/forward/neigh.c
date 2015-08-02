#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

struct neigh_table *neigh_alloc()
{
	struct neigh_table *neigh;

	neigh = malloc(sizeof(struct neigh_table));
	if(!neigh)
		goto err_neigh_alloc;

	hash_init(&neigh->table);
	neigh->table.hash_release = neigh_entry_delete;

	pthread_mutex_init(&neigh->mutex, NULL);
	return neigh;

err_neigh_alloc:
	return NULL;
}

void neigh_release(struct neigh_table *neigh)
{
	hash_delete_all(neigh->table);
	free(neigh);
	return;
}

void neigh_entry_delete(struct hash_entry *entry)
{
	struct neigh_entry *neigh_entry;

	neigh_entry = hlist_entry(entry, struct neigh_entry, hlist);
	free(neigh_entry);
	return;
}

int neigh_add(struct neigh_table *neigh, int family,
	uint32_t *dst_addr, uint8_t *mac_addr)
{
	struct neigh_entry *neigh_entry;
	int family_len, ret;

	switch(family){
	case AF_INET:
		family_len = 4;
		break;
	case AF_INET6:
		family_len = 16;
		break;
	default:
		goto err_invalid_family;
		break;
	}

	neigh_entry = malloc(sizeof(struct neigh_entry));
	if(!neigh_entry)
		goto err_alloc_entry;

	memcpy(neigh_entry->dst_mac, mac_addr, ETH_ALEN);

	ixmapfwd_mutex_lock(&neigh->mutex);
	ret = hash_add(neigh->table, dst_addr, family_len, &neigh_entry->hash);
	if(ret < 0)
		goto err_hash_add;
	ixmapfwd_mutex_unlock(&neigh->mutex);

	return 0;

err_hash_add:
	ixmapfwd_mutex_unlock(&neigh->mutex);
	free(neigh_entry);
err_alloc_entry;
err_invalid_family:
	return -1;
}

int neigh_delete(struct neigh_table *neigh, int family,
	uint32_t *dst_addr)
{
	struct hash_entry *hash_entry = NULL;
	struct neigh_entry *neigh_entry;
	int family_len, ret;

	switch(family){
	case AF_INET:
		family_len = 4;
		break;
	case AF_INET6:
		family_len = 16;
		break;
	default:
		goto err_invalid_family;
		break;
	}

	ixmapfwd_mutex_lock(&neigh->mutex);
	ret = hash_delete(neigh->table, dst_addr, family_len, &hash_entry);
	if(ret < 0)
		goto err_hash_delete;

	if(hash_entry){
		neigh_entry = hash_entry(hash_entry, struct neigh_entry, hash);
		free(neigh_entry);
	}

	ixmapfwd_mutex_unlock(&neigh->mutex);

	return 0;

err_hash_delete:
	ixmapfwd_mutex_unlock(&neigh->mutex);
err_invalid_family:
	return -1;
}

struct neigh_entry *neigh_lookup(struct neigh_table *neigh, int family,
	uint32_t *dst_addr)
{
	struct hash_entry *hash_entry;
	struct neigh_entry *neigh_entry;
	int family_len;

	switch(family){
	case AF_INET:
		family_len = 4;
		break;
	case AF_INET6:
		family_len = 16;
		break;
	default:
		goto err_invalid_family;
		break;
	}

	hash_entry = hash_lookup(neigh->table, dst_ip, sizeof(struct in_addr));
	if(!hash_entry)
		goto err_hash_lookup;

	neigh_entry = hash_entry(entry_hash, struct neigh_entry, hash);
	return neigh_entry;

err_hash_lookup:
err_invalid_family:
	return NULL;
}
