#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

int neigh_add(struct neigh_table *neigh, uint8_t *src_mac,
	struct in_addr *src_ip;)
{
	struct neigh_entry entry;
	int ret;

	memcpy(entry.mac_addr, src_mac, ETH_ALEN);
	ret = hash_add(neigh_table->hash_root, src_ip, sizeof(struct in_addr),
		&entry, sizeof(struct neigh_entry));
	if(ret < 0)
		goto err_hash_add;

	return 0;

err_hash_add:
	return -1;
}

int neigh_delete(struct neigh_table *neigh, struct in_addr *src_ip)
{
	int ret;

	ret = hash_delete(neigh_table->hash_root, src_ip, sizeof(struct in_addr));
	if(ret < 0)
		goto err_hash_del;

	return 0;

err_hash_del:
	return -1;
}

struct neigh_entry *neigh_lookup(struct neigh_table *neigh,
	struct in_addr *dst_ip)
{
	struct neigh_entry *neigh_entry;

	neigh_entry = (struct neigh_entry *)hash_lookup(
		neigh->hash_root, dst_ip, sizeof(struct in_addr));
	return neigh_entry;
}
