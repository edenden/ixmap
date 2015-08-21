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
static unsigned int neigh_key_generate_v4(void *key,
	unsigned int bit_len);
static unsigned int neigh_key_generate_v6(void *key,
	unsigned int bit_len);
static int neigh_key_compare_v4(void *key_tgt, void *key_ent);
static int neigh_key_compare_v6(void *key_tgt, void *key_ent);

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

struct neigh_table *neigh_alloc(struct ixmap_desc *desc, int family)
{
	struct neigh_table *neigh;
	struct ixmap_marea *area;

	area = ixmap_mem_alloc(desc, sizeof(struct neigh_table));
	if(!area)
		goto err_neigh_alloc;

	neigh = area->ptr;
	neigh->area = area;

	hash_init(&neigh->table);
	neigh->table.hash_entry_delete = neigh_entry_delete;

	switch(family){
	case AF_INET:
		neigh->table.hash_key_generate =
			neigh_key_generate_v4;
		neigh->table.hash_key_compare =
			neigh_key_compare_v4;
		break;
	case AF_INET6:
		neigh->table.hash_key_generate =
			neigh_key_generate_v6;
		neigh->table.hash_key_compare =
			neigh_key_compare_v6;
		break;
	default:
		goto err_invalid_family;
		break;
	}

	return neigh;

err_invalid_family:
	ixmap_mem_free(neigh->area);
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

static unsigned int neigh_key_generate_v4(void *key, unsigned int bit_len)
{
	/* On some cpus multiply is faster, on others gcc will do shifts */
	uint32_t hash = *((uint32_t *)key) * GOLDEN_RATIO_PRIME_32;

	/* High bits are more random, so use them. */
	return hash >> (32 - bit_len);
}

static unsigned int neigh_key_generate_v6(void *key, unsigned int bit_len)
{
	uint64_t hash = ((uint64_t *)key)[0] * ((uint64_t *)key)[1];
	hash *= GOLDEN_RATIO_PRIME_64;

	return hash >> (64 - bit_len);
}

static int neigh_key_compare_v4(void *key_tgt, void *key_ent)
{
	return ((uint32_t *)key_tgt)[0] ^ ((uint32_t *)key_ent)[0] ?
		1 : 0;
}

static int neigh_key_compare_v6(void *key_tgt, void *key_ent)
{
	return ((uint64_t *)key_tgt)[0] ^ ((uint64_t *)key_ent)[0] ?
		(((uint64_t *)key_tgt)[1] ^ ((uint64_t *)key_ent)[1] ?
		1 : 0) : 0;
}

int neigh_add(struct neigh_table *neigh, int family,
	void *dst_addr, void *mac_addr, struct ixmap_desc *desc) 
{
	struct neigh_entry *neigh_entry;
	struct ixmap_marea *area;
	int ret;

	area = ixmap_mem_alloc(desc, sizeof(struct neigh_entry));
	if(!area)
		goto err_alloc_entry;

	neigh_entry = area->ptr;
	neigh_entry->area = area;

	memcpy(neigh_entry->dst_mac, mac_addr, ETH_ALEN);
	switch(family){
	case AF_INET:
		memcpy(neigh_entry->dst_addr, dst_addr, 4);
		break;
	case AF_INET6:
		memcpy(neigh_entry->dst_addr, dst_addr, 16);
		break;
	default:
		goto err_invalid_family;
		break;
	}

#ifdef DEBUG
	neigh_add_print(family, dst_addr, mac_addr);
#endif

	ret = hash_add(&neigh->table, neigh_entry->dst_addr, &neigh_entry->hash);
	if(ret < 0)
		goto err_hash_add;

	return 0;

err_hash_add:
err_invalid_family:
	ixmap_mem_free(neigh_entry->area);
err_alloc_entry:
	return -1;
}

int neigh_delete(struct neigh_table *neigh, int family,
	void *dst_addr)
{
	int ret;

#ifdef DEBUG
	neigh_delete_print(family, dst_addr);
#endif

	ret = hash_delete(&neigh->table, dst_addr);
	if(ret < 0)
		goto err_hash_delete;

	return 0;

err_hash_delete:
	return -1;
}

struct neigh_entry *neigh_lookup(struct neigh_table *neigh,
	void *dst_addr)
{
	struct hash_entry *hash_entry;
	struct neigh_entry *neigh_entry;

	hash_entry = hash_lookup(&neigh->table, dst_addr);
	if(!hash_entry)
		goto err_hash_lookup;

	neigh_entry = hash_entry(hash_entry, struct neigh_entry, hash);
	return neigh_entry;

err_hash_lookup:
	return NULL;
}
