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

	neigh->table = hash_alloc();
	if(!neigh->table)
		goto err_hash_alloc;

	return neigh;

err_hash_alloc:
	free(neigh);
err_neigh_alloc:
	return NULL;
}

void neigh_release(struct neigh_table *neigh)
{
	hash_release(neigh->table);
	free(neigh);
	return;
}

int neigh_add(struct neigh_table *neigh, int family,
	uint32_t *dst_addr, uint8_t *mac_addr)
{
	struct neigh_entry entry;
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

	memcpy(entry.dst_mac, mac_addr, ETH_ALEN);
	ret = hash_add(neigh->table, dst_addr, family_len,
		&entry, sizeof(struct neigh_entry));
	if(ret < 0)
		goto err_hash_add;

	return 0;

err_hash_add:
err_invalid_family:
	return -1;
}

int neigh_delete(struct neigh_table *neigh, int family,
	uint32_t *dst_addr)
{
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

	ret = hash_delete(neigh->table, dst_addr, family_len);
	if(ret < 0)
		goto err_hash_del;

	return 0;

err_hash_del:
err_invalid_family:
	return -1;
}

struct neigh_entry *neigh_lookup(struct neigh_table *neigh, int family,
	uint32_t *dst_addr)
{
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

	neigh_entry = (struct neigh_entry *)hash_lookup(
		neigh->table, dst_ip, sizeof(struct in_addr));
	if(!neigh_entry)
		goto err_hash_lookup;

	return neigh_entry;

err_hash_lookup:
err_invalid_family:
	return NULL;
}
