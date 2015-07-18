#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "main.h"
#include "hash.h"

static unsigned int hash_key(void *key, int key_len);

static unsigned int hash_key(void *key, int key_len)
{
	unsigned int i, sum;

	for(i = 0, sum = 0; i < key_len; i++){
		sum += ((uint8_t *)key)[i];
	}

	return sum % HASH_SIZE;
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
	struct hash_entry *entry, **prev_ptr;
	unsigned int hash_key;

	hash_key = hash_key(key, key_len);
	entry = root->entries[hash_key];
	prev_ptr = &(root->entries[hash_key])

	if(!entry)
		goto err_not_found;

	while(memcmp(entry->key, key, key_len)){
		prev_ptr = &(entry->next);
		entry = entry->next;
		if(!entry)
			goto err_not_found;
	}

	*prev_ptr = entry->next;
	free(entry->value);
	free(entry->key);
	free(entry);

	return 0;

err_not_found:
	return -1;
}

void hash_delete_walk(struct hash_root *root)
{
	struct hash_entry *entry, *entry_next;
	unsigned int i;

	for(i = 0; i < HASH_SIZE; i++){
		entry = root->entries[i];
		if(!entry)
			continue;

		while(entry){
			entry_next = entry->next;

			free(entry->value);
			free(entry->key);
			free(entry);

			entry = entry_next;
		}
	}

	return;
}

void *hash_lookup(struct hash_root *root, void *key, int key_len)
{
	struct hash_entry *entry;
	unsigned int hash_key;

	hash_key = hash_key(key, key_len);
	entry = root->entries[hash_key];

	if(!entry)
		goto err_not_found;

	while(memcmp(entry->key, key, key_len)){
		entry = entry->next;
		if(!entry)
			goto err_not_found;
	}

	return entry->value;

err_not_found:
	return NULL;
}
