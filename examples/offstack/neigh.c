#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <stddef.h>
#include <ixmap.h>

#include "main.h"
#include "neigh.h"

static void neigh_entry_delete(struct hash_entry *entry);

#ifdef DEBUG
static void neigh_add_print(int family,
	void *dst_addr, void *mac_addr);
static void neigh_delete_print(int family,
	void *dst_addr);
#endif

#ifdef DEBUG
static void neigh_add_print(int family,
	void *dst_addr, void *mac_addr)
{
	char dst_addr_a[128];
	char family_a[128];

	printf("neigh update:\n");

	switch(family){
	case AF_INET:
		strcpy(family_a, "AF_INET");
		break;
	case AF_INET6:
		strcpy(family_a, "AF_INET6");
		break;
	default:
		strcpy(family_a, "UNKNOWN");
		break;
	}
	printf("\tFAMILY: %s\n", family_a);

	inet_ntop(family, dst_addr, dst_addr_a, sizeof(dst_addr_a));
	printf("\tDST: %s\n", dst_addr_a);

	printf("\tMAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
		((uint8_t *)mac_addr)[0], ((uint8_t *)mac_addr)[1],
		((uint8_t *)mac_addr)[2], ((uint8_t *)mac_addr)[3],
		((uint8_t *)mac_addr)[4], ((uint8_t *)mac_addr)[5]);

	return;
}

static void neigh_delete_print(int family,
	void *dst_addr)
{
	char dst_addr_a[128];
	char family_a[128];

	printf("neigh delete:\n");

	switch(family){
	case AF_INET:
		strcpy(family_a, "AF_INET");
		break;
	case AF_INET6:
		strcpy(family_a, "AF_INET6");
		break;
	default:
		strcpy(family_a, "UNKNOWN");
		break;
	}
	printf("\tFAMILY: %s\n", family_a);

	inet_ntop(family, dst_addr, dst_addr_a, sizeof(dst_addr_a));
	printf("\tDST: %s\n", dst_addr_a);

	return;
}
#endif

struct neigh_table *neigh_alloc(struct ixmap_desc *desc)
{
	struct neigh_table *neigh;
	struct ixmap_marea *area;

	area = ixmap_mem_alloc(desc, sizeof(struct neigh_table));
	if(!area)
		goto err_neigh_alloc;

	neigh = area->ptr;

	hash_init(&neigh->table);
	neigh->table.hash_entry_delete = neigh_entry_delete;
	neigh->area = area;
	return neigh;

err_neigh_alloc:
	return NULL;
}

void neigh_release(struct neigh_table *neigh)
{
	hash_delete_all(&neigh->table);
	ixmap_mem_free(neigh->area);
	return;
}

static void neigh_entry_delete(struct hash_entry *entry)
{
	struct neigh_entry *neigh_entry;

	neigh_entry = hash_entry(entry, struct neigh_entry, hash);
	ixmap_mem_free(neigh_entry->area);
	return;
}

int neigh_add(struct neigh_table *neigh, int family,
	void *dst_addr, void *mac_addr, struct ixmap_desc *desc) 
{
	struct neigh_entry *neigh_entry;
	struct ixmap_marea *area;
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

	area = ixmap_mem_alloc(desc, sizeof(struct neigh_entry));
	if(!area)
		goto err_alloc_entry;

	neigh_entry = area->ptr;

	memcpy(neigh_entry->dst_mac, mac_addr, ETH_ALEN);
	neigh_entry->area = area;

#ifdef DEBUG
	neigh_add_print(family, dst_addr, mac_addr);
#endif

	ret = hash_add(&neigh->table, dst_addr, family_len, &neigh_entry->hash);
	if(ret < 0)
		goto err_hash_add;

	return 0;

err_hash_add:
	free(neigh_entry);
err_alloc_entry:
err_invalid_family:
	return -1;
}

int neigh_delete(struct neigh_table *neigh, int family,
	void *dst_addr)
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

#ifdef DEBUG
	neigh_delete_print(family, dst_addr);
#endif

	ret = hash_delete(&neigh->table, dst_addr, family_len);
	if(ret < 0)
		goto err_hash_delete;

	return 0;

err_hash_delete:
err_invalid_family:
	return -1;
}

struct neigh_entry *neigh_lookup(struct neigh_table *neigh, int family,
	void *dst_addr)
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

	hash_entry = hash_lookup(&neigh->table, dst_addr, family_len);
	if(!hash_entry)
		goto err_hash_lookup;

	neigh_entry = hash_entry(hash_entry, struct neigh_entry, hash);
	return neigh_entry;

err_hash_lookup:
err_invalid_family:
	return NULL;
}
