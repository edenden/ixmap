#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <stddef.h>
#include <ixmap.h>

#include "linux/list.h"
#include "main.h"
#include "fib.h"

static int fib_entry_identify(void *ptr, unsigned int id,
	unsigned int prefix_len);
static int fib_entry_compare(void *ptr, unsigned int prefix_len);
static void fib_entry_pull(void *ptr);
static void fib_entry_put(void *ptr);

#ifdef DEBUG
static void fib_update_print(int family, enum fib_type type,
	void *prefix, unsigned int prefix_len, void *nexthop,
	int port_index, int id);
static void fib_delete_print(int family, void *prefix,
	unsigned int prefix_len, int id);
#endif

#ifdef DEBUG
static void fib_update_print(int family, enum fib_type type,
	void *prefix, unsigned int prefix_len, void *nexthop,
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
		strcpy(family_a, "UNKNOWN");
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

static void fib_delete_print(int family, void *prefix,
	unsigned int prefix_len, int id)
{
	char prefix_a[128];
	char family_a[128];

	printf("fib delete:\n");

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

	inet_ntop(family, prefix, prefix_a, sizeof(prefix_a));
	printf("\tPREFIX: %s/%d\n", prefix_a, prefix_len);

	printf("\tID: %d\n", id);

	return;
}
#endif

struct fib *fib_alloc(struct ixmap_desc *desc, int family)
{
        struct fib *fib;
	struct ixmap_marea *area;

	area = ixmap_mem_alloc(desc, sizeof(struct fib));
	if(!area)
		goto err_fib_alloc;

	fib = area->ptr;
	fib->area = area;

	switch(family){
	case AF_INET:
		lpm_init(&fib->table, 32);
		break;
	case AF_INET6:
		lpm_init(&fib->table, 128);
		break;
	default:
		goto err_invalid_family;
		break;
	}

	fib->table.entry_identify	= fib_entry_identify;
	fib->table.entry_compare	= fib_entry_compare;
	fib->table.entry_pull		= fib_entry_pull;
	fib->table.entry_put		= fib_entry_put;

	return fib;

err_invalid_family:
	ixmap_mem_free(fib->area);
err_fib_alloc:
	return NULL;
}

void fib_release(struct fib *fib)
{
	lpm_delete_all(&fib->table);
	ixmap_mem_free(fib->area);
	return;
}

int fib_route_update(struct fib *fib, int family, enum fib_type type,
	void *prefix, unsigned int prefix_len, void *nexthop,
	int port_index, int id, struct ixmap_desc *desc)
{
	struct fib_entry *entry;
	struct ixmap_marea *area;
	int ret;

	area = ixmap_mem_alloc(desc, sizeof(struct fib_entry));
	if(!area)
		goto err_alloc_entry;

	entry = area->ptr;
	entry->area = area;

	switch(family){
	case AF_INET:
		memcpy(entry->nexthop, nexthop, 4);
		memcpy(entry->prefix, prefix, 4);
		break;
	case AF_INET6:
		memcpy(entry->nexthop, nexthop, 16);
		memcpy(entry->prefix, prefix, 16);
		break;
	default:
		goto err_invalid_family;
		break;
	}

	entry->prefix_len	= prefix_len;
	entry->port_index	= port_index;
	entry->type		= type;
	entry->id		= id;
	entry->refcount		= 0;

#ifdef DEBUG
	fib_update_print(family, type, prefix, prefix_len,
		nexthop, port_index, id);
#endif

	ret = lpm_add(&fib->table, prefix, prefix_len,
		id, entry, desc);
	if(ret < 0)
		goto err_lpm_add;

	return 0;

err_lpm_add:
err_invalid_family:
	ixmap_mem_free(entry->area);
err_alloc_entry:
	return -1;
}

int fib_route_delete(struct fib *fib, int family,
	void *prefix, unsigned int prefix_len,
	int id)
{
	int ret;

#ifdef DEBUG
	fib_delete_print(family, prefix, prefix_len, id);
#endif

	ret = lpm_delete(&fib->table, prefix, prefix_len, id);
	if(ret < 0)
		goto err_lpm_delete;

	return 0;

err_lpm_delete:
	return -1;
}

struct fib_entry *fib_lookup(struct fib *fib, void *destination)
{
	struct lpm_entry *entry;

	entry = lpm_lookup(&fib->table, destination);
	if(!entry)
		goto err_lpm_lookup;

	return entry->ptr;

err_lpm_lookup:
	return NULL;
}

static int fib_entry_identify(void *ptr, unsigned int id,
	unsigned int prefix_len)
{
	struct fib_entry *entry;

	entry = ptr;

	if(entry->id == id
	&& entry->prefix_len == prefix_len){
		return 0;
	}else{
		return 1;
	}
}

static int fib_entry_compare(void *ptr, unsigned int prefix_len)
{
	struct fib_entry *entry;

	entry = ptr;

	return entry->prefix_len > prefix_len ?
		1 : 0;
}

static void fib_entry_pull(void *ptr)
{
	struct fib_entry *entry;

	entry = ptr;
	entry->refcount++;

	return;
}

static void fib_entry_put(void *ptr)
{
	struct fib_entry *entry;

	entry = ptr;
	entry->refcount--;

	if(!entry->refcount){
		ixmap_mem_free(entry->area);
	}
}
