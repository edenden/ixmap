#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#include "linux/list.h"
#include "main.h"
#include "hash.h"

static void hash_entry_init(struct hash_entry *entry,
	void *key);

void hash_init(struct hash_table *table)
{
	int i;

	for(i = 0; i < HASH_SIZE; i++){
		INIT_HLIST_HEAD(&table->head[i]);
	}
	
	return;
}

static void hash_entry_init(struct hash_entry *entry,
	void *key)
{
	entry->key = key;
	return;
}

int hash_add(struct hash_table *table, void *key,
	struct hash_entry *entry_new)
{
	struct hlist_head *head;
	struct hash_entry *entry;
	unsigned int hash_key;

	hash_entry_init(entry_new, key);
	hash_key = table->hash_key_generate(key, HASH_BIT);
	head = &table->head[hash_key];

	hlist_for_each_entry(entry, head, list){
		if(!table->hash_key_compare(key, entry->key)){
			goto err_entry_exist;
		}
	}

	hlist_add_head(&entry_new->list, head);

	return 0;

err_entry_exist:
	return -1;
}

int hash_delete(struct hash_table *table,
	void *key)
{
	struct hlist_head *head;
	struct hash_entry *entry, *target;
	unsigned int hash_key;

	target = NULL;
	hash_key = table->hash_key_generate(key, HASH_BIT);
	head = &table->head[hash_key];

	hlist_for_each_entry(entry, head, list){
		if(!table->hash_key_compare(key, entry->key)){
			target = entry;
			break;
		}
	}

	if(!target)
		goto err_not_found;

	hlist_del_init(&target->list);
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
			table->hash_entry_delete(entry);
		}
	}

	return;
}

struct hash_entry *hash_lookup(struct hash_table *table,
	void *key)
{
	struct hlist_head *head;
	struct hash_entry *entry, *entry_ret;
	unsigned int hash_key;

	entry_ret = NULL;
	hash_key = table->hash_key_generate(key, HASH_BIT);
	head = &table->head[hash_key];

	hlist_for_each_entry(entry, head, list){
		if(!table->hash_key_compare(key, entry->key)){
			entry_ret = entry;
			break;
		}
	}

        return entry_ret;
}

