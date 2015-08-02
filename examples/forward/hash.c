#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <urcu.h>

#include "main.h"
#include "hash.h"

static unsigned int hash_key(void *key, int key_len);
static void hash_delete_all(struct hash_table *table);

/* TBD: use hlist */

static unsigned int hash_key(void *key, int key_len)
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

int hash_entry_init(struct hash_entry *entry,
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

void hash_entry_destroy(struct hash_entry *entry)
{
	free(entry->key);
}

int hash_add(struct hash_table *table, void *key, int key_len,
	struct hash_entry *entry_new)
{
	struct hlist_node *list, *head;
	struct hash_entry *entry_new, *entry;
	unsigned int hash_key;
	int ret;

	ret = hash_entry_init(entry_new, key, key_len);
	if(ret < 0)
		goto err_init_node;
	
	hash_key = hash_key(key, key_len);
	head = &table->head[hash_key];

	hlist_for_each_entry_rcu(entry, head, list){
		if(entry->key_len == key_len
		&& !memcmp(entry->key, key, key_len)){
			goto err_entry_exist;
		}
	}

	hlist_add_head_rcu(&entry_new->list, head);

	return 0;

err_entry_exist:
	hash_entry_destroy(entry_new);
err_init_node:
	return -1;
}

int hash_delete(struct hash_table *table,
	void *key, int key_len, struct hash_entry **entry_ret)
{
	struct hlist_node *list, *head;
	struct hash_entry *entry;
	unsigned int hash_key;

	*entry_ret = NULL;
	hash_key = hash_key(key, key_len);
	head = &table->head[hash_key];

	hlist_for_each_entry_rcu(entry, head, list){
		if(entry->key_len == key_len
		&& !memcmp(entry->key, key, key_len)){
			*entry_ret = entry;
			break;
		}
	}

	if(!*entry_ret)
		goto err_not_found;

	hlist_del_init_rcu(&(*entry_ret->list));
	hash_entry_destroy(*entry_ret);

	return 0;

err_not_found:
	return -1;
}

void hash_delete_all(struct hash_table *table, struct list_node *head)
{
	struct hash_entry *entry;
	unsigned int i;

	for(i = 0; i < HASH_SIZE; i++){
		if(!hlist_empty(&table->head[i])){
			list_splice(&table->head[i], head);
		}
	}

	synchronize_rcu();
	list_for_each(list, head){
		entry = list_entry(list, struct hash_entry, list);
		hash_entry_destroy(entry);
	}

	return;
}

/* rcu_read_lock needs to be hold by caller from readside */
struct hash_entry *hash_lookup(struct hash_table *table, void *key, int key_len)
{
	struct list_head *head, *list;
	struct hash_entry *entry, *entry_ret;
	unsigned int hash_key;
	int count = 0;

	entry_ret = NULL;
	hash_key = hash_key(key, key_len);
	head = &table->head[hash_key];

	list_for_each(list, head){
		entry = list_content(list, struct hash_entry, list);

		if(entry->key_len == key_len
		&& !memcmp(entry->key, key, key_len)){
			entry_ret = entry;
			break;
		}
	}

        return entry_ret;
}

