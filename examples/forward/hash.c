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

int hash_add(struct hash *hash, void *key, int key_len,
	void *value, int value_len)
{
	struct hash_entry *entry;
	unsigned int hash_key;
	int count = 0;
	
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

	hash_key = hash_key(key, key_len);
	while(count++ < HASH_COLLISION){
		if(hash->entries[hash_key]){
			if(hash->entries[hash_key]->key_len == key_len
			&& !memcmp(hash->entries[hash_key]->key, key, key_len)){
				free(hash->entries[hash_key]->value);
				free(hash->entries[hash_key]->key);
				free(hash->entries[hash_key]);
				break;
			}
		}else{
			break;
		}

		hash_key++;
		if(hash_key == HASH_SIZE){
			hash_key = 0;
		}
	}

	if(count > HASH_COLLISION){
		goto err_hash_collision;
	}

	hash->entries[hash_key] = entry;
	return 0;

err_hash_collision:
	free(entry->value);
err_alloc_value:
	free(entry->key);
err_alloc_key:
	free(entry);
err_alloc_entry:
	return -1;
}

int hash_delete(struct hash *hash, void *key, int key_len)
{
	struct hash_entry *entry;
	unsigned int hash_key;
	int count = 0;

	hash_key = hash_key(key, key_len);
	while(count++ < HASH_COLLISION){
		if(hash->entries[hash_key]){
			if(hash->entries[hash_key]->key_len == key_len
			&& !memcmp(hash->entries[hash_key]->key, key, key_len)){
                                break;
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

	entry = hash->entries[hash_key];
	hash->entries[hash_key] = NULL;
	free(entry->value);
	free(entry->key);
	free(entry);

	return 0;

err_not_found:
	return -1;
}

void hash_delete_walk(struct hash *hash)
{
	struct hash_entry *entry;
	unsigned int i;

	for(i = 0; i < HASH_SIZE; i++){
		if(hash->entries[i]){
			entry = hash->entries[i];
			hash->entries[i] = NULL;
			free(entry->value);
			free(entry->key);
			free(entry);
		}
	}

	return;
}

void *hash_lookup(struct hash *hash, void *key, int key_len)
{
	struct hash_entry *entry;
	unsigned int hash_key;
	int count = 0;

	hash_key = hash_key(key, key_len);
	while(count++ < HASH_COLLISION){
		if(hash->entries[hash_key]){
			if(hash->entries[hash_key]->key_len == key_len
			&& !memcmp(hash->entries[hash_key]->key, key, key_len)){
				break;
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

        entry = hash->entries[hash_key];
	return entry;

err_not_found:
        return NULL;
}

