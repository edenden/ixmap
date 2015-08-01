#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "main.h"
#include "hash.h"

static unsigned int hash_key(void *key, int key_len);
static void hash_delete_all(struct hash_table *table);

static unsigned int hash_key(void *key, int key_len)
{
	unsigned int i, sum;

	for(i = 0, sum = 0; i < key_len; i++){
		sum += ((uint8_t *)key)[i];
	}

	return sum % HASH_SIZE;
}

struct hash_table *hash_alloc()
{
	struct hash_table *table;

	table = malloc(sizeof(struct hash_table));
	if(!table)
		goto err_hash_alloc;
	memset(table, 0, sizeof(struct hash_table));

	return table;

err_hash_alloc:
	return NULL;
}

void hash_release(struct hash_table *table)
{
	free(table);
	return;
}

int hash_add(struct hash_table *table, void *key, int key_len,
	void **value)
{
	struct hash_entry *entry_new, *entry;
	unsigned int hash_key;
	int count = 0;
	
	entry_new = (struct hash_entry *)malloc(sizeof(struct hash_entry));
	if(!entry_new)
		goto err_alloc_entry;

	entry_new->key = malloc(key_len);
	if(!entry_new->key)
		goto err_alloc_key;

	memcpy(entry_new->key, key, key_len);
	entry_new->key_len = key_len;
	entry_new->value = *value;

	hash_key = hash_key(key, key_len);
	while(count++ < HASH_COLLISION){
		entry = table->entries[hash_key];

		if(entry){
			if(entry->key_len == key_len
			&& !memcmp(entry->key, key, key_len)){
				rcu_set_pointer(table->entries[hash_key], entry_new);
				synchronize_rcu();
				*value = entry->value;
				free(entry->key);
				free(entry);
				break;
			}
		}else{
			rcu_set_pointer(table->entries[hash_key], entry_new);
			break;
		}

		hash_key++;
		if(hash_key == HASH_SIZE){
			hash_key = 0;
		}
	}

	if(count > HASH_COLLISION){
		goto err_hash_full;
	}

	return 0;
	
err_hash_full:
	free(entry_new->key);
err_alloc_key:
	free(entry_new);
err_alloc_entry:
	return -1;
}

int hash_delete(struct hash_table *table, void *key, int key_len, void **value)
{
	struct hash_entry *entry;
	unsigned int hash_key;
	int ret = -1, count = 0;

	hash_key = hash_key(key, key_len);
	while(count++ < HASH_COLLISION){
		entry = table->entries[hash_key];
		if(entry){
			if(entry->key_len == key_len
			&& !memcmp(entry->key, key, key_len)){
				rcu_set_pointer(table->entries[hash_key], NULL);
				synchronize_rcu();
				*value = entry->value;
				free(entry->key);
				free(entry);
				ret = 0;
                        }
                }

		hash_key++;
		if(hash_key == HASH_SIZE){
			hash_key = 0;
		}
	}

	if(count > HASH_COLLISION){
		goto err_not_found;
	}

	return 0;

err_not_found:
	return -1;
}

int hash_delete_all(struct hash_table *table, struct hash_value_list **list_ret)
{
	struct hash_entry *entry;
	struct hash_value_list list_root, *list, *list_next;
	unsigned int i;

	for(i = 0; i < HASH_SIZE; i++){
		entry = table->entries[i];

		if(entry){
			list = malloc(sizeof(struct hash_value_list));
			if(!list)
				goto err_alloc_list;

			list->value = entry->value;
			list->next = NULL;

			list_root->last->next = list;
			list_root->last = list;
		}
	}

	for(i = 0; i < HASH_SIZE; i++){
		entry = table->entries[i];

		if(entry){
			rcu_set_pointer(table->entries[i], NULL);
			synchronize_rcu();
			free(entry->key);
			free(entry);
		}
	}

	*list_ret = list_root.next;
	return 0;

err_alloc_list:
	list = list_root.next;
	while(list){
		list_next = list->next;
		free(list);
		list = list_next;
	}
	*list_ret = NULL;
	return -1;
}

/* rcu_read_lock needs to be hold by caller from readside */
void *hash_lookup(struct hash_table *table, void *key, int key_len)
{
	struct hash_entry *entry, *entry_ret = NULL;
	unsigned int hash_key;
	int count = 0;

	hash_key = hash_key(key, key_len);
	while(count++ < HASH_COLLISION){
		entry = rcu_dereference(table->entries[hash_key]);

		if(entry){
			if(entry->key_len == key_len
			&& !memcmp(entry->key, key, key_len)){
				entry_ret = entry;
				break;
			}
		}

		hash_key++;
		if(hash_key == HASH_SIZE){
			hash_key = 0;
		}
	}

        return entry_ret;
}

