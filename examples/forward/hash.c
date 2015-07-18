#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "main.h"
#include "hash.h"

unsigned int hash_key(void *key, int key_len)
{

}

unsigned int hash_add(struct hash_root *root, void *key, int key_len,
	void *value, int value_len)
{
	struct hash_entry *entry, *entry_list_current;
	unsigned int collision;
	unsigned int hash_key;
	
	entry = (struct hash_entry *)malloc(sizeof(struct hash_entry));
	if(!entry)
		goto err_alloc_entry;

	entry->key = malloc(key_len);
	if(!entry->key)
		goto err_alloc_key;

	entry->value = malloc(value_len);
	if(!entry->value)
		goto err_alloc_value;

	memcpy(entry->key, key, key_len);
	entry->key_len = key_len;
	memcpy(entry->value, value, value_len); 
	entry->next = NULL;

	hash_key = hash_key(key, key_len);
	collision = 0;

	if(!root->entries[hash_key]){
		root->entries[hash_key] = entry;
		return collision;
	}

	entry_list_current = root->entries[hash_key];
	collision++;

	while(entry_list_current->next){
		entry_list_current = entry_list_current->next;
		collision++;
	}
	entry_list_current->next = entry;

	return count_collision;

err_alloc_value:
	free(entry->key);
err_alloc_key:
	free(entry);
err_alloc_entry:
	return -1;
}

int hash_delete(struct hash_root *root, void *key, int key_len)
{

}

int hash_delete_walk(struct hash_root *root)
{

}

void *hash_lookup(void *key, int key_len)
{

}
