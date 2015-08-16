#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#include "linux/list.h"
#include "main.h"
#include "hash.h"

static unsigned int hash_key_crc32(void *key, int key_len);
static int hash_entry_init(struct hash_entry *entry,
	void *key, int key_len);
static void hash_entry_destroy(struct hash_entry *entry);

static unsigned int hash_key_crc32(void *key, int key_len)
{
	unsigned int i, sum;

	for(i = 0, sum = 0; i < key_len; i++){
		sum += ((uint8_t *)key)[i];
	}

	return sum % HASH_SIZE;
}

void hash_init(struct hash_table *table)
{
	int i;

	for(i = 0; i < HASH_SIZE; i++){
		INIT_HLIST_HEAD(&table->head[i]);
	}
	
	return;
}

static int hash_entry_init(struct hash_entry *entry,
	void *key, int key_len)
{
	entry->key = malloc(key_len);
	if(!entry->key)
		goto err_alloc_key;

	memcpy(entry->key, key, key_len);
	entry->key_len = key_len;

	return 0;

err_alloc_key:
	return -1;
}

static void hash_entry_destroy(struct hash_entry *entry)
{
	free(entry->key);
}

int hash_add(struct hash_table *table, void *key, int key_len,
	struct hash_entry *entry_new)
{
	struct hlist_head *head;
	struct hash_entry *entry;
	unsigned int hash_key;
	int ret;

	ret = hash_entry_init(entry_new, key, key_len);
	if(ret < 0)
		goto err_init_node;
	
	hash_key = hash_key_crc32(key, key_len);
	head = &table->head[hash_key];

	hlist_for_each_entry(entry, head, list){
		if(entry->key_len == key_len
		&& !memcmp(entry->key, key, key_len)){
			goto err_entry_exist;
		}
	}

	hlist_add_head(&entry_new->list, head);

	return 0;

err_entry_exist:
	hash_entry_destroy(entry_new);
err_init_node:
	return -1;
}

int hash_delete(struct hash_table *table,
	void *key, int key_len)
{
	struct hlist_head *head;
	struct hash_entry *entry, *target;
	unsigned int hash_key;

	target = NULL;
	hash_key = hash_key_crc32(key, key_len);
	head = &table->head[hash_key];

	hlist_for_each_entry(entry, head, list){
		if(entry->key_len == key_len
		&& !memcmp(entry->key, key, key_len)){
			target = entry;
			break;
		}
	}

	if(!target)
		goto err_not_found;

	hlist_del_init(&target->list);
	hash_entry_destroy(target);
	table->hash_entry_delete(target);

	return 0;

err_not_found:
	return -1;
}

void hash_delete_all(struct hash_table *table)
{
	struct hlist_head *head;
	struct hlist_node *next;
	struct hash_entry *entry;
	unsigned int i;

	for(i = 0; i < HASH_SIZE; i++){
		head = &table->head[i];
		hlist_for_each_entry_safe(entry, next, head, list){
			hlist_del_init(&entry->list);
			hash_entry_destroy(entry);
			table->hash_entry_delete(entry);
		}
	}

	return;
}

struct hash_entry *hash_lookup(struct hash_table *table,
	void *key, int key_len)
{
	struct hlist_head *head;
	struct hash_entry *entry, *entry_ret;
	unsigned int hash_key;

	entry_ret = NULL;
	hash_key = hash_key_crc32(key, key_len);
	head = &table->head[hash_key];

	hlist_for_each_entry(entry, head, list){
		if(entry->key_len == key_len
		&& !memcmp(entry->key, key, key_len)){
			entry_ret = entry;
			break;
		}
	}

        return entry_ret;
}

