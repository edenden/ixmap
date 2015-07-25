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
	struct hash_entry *entry_new, *entry;
	unsigned int hash_key;
	int count = 0;
	
	entry_new = (struct hash_entry *)malloc(sizeof(struct hash_entry));
	if(!entry_new)
		goto err_alloc_entry;

	entry_new->key = malloc(key_len);
	if(!entry_new->key)
		goto err_alloc_key;

	entry_new->value = malloc(value_len);
	if(!entry_new->value)
		goto err_alloc_value;

	memcpy(entry_new->key, key, key_len);
	entry_new->key_len = key_len;
	memcpy(entry_new->value, value, value_len); 

	hash_key = hash_key(key, key_len);
	ixmapfwd_mutex_lock(&hash->mutex);

	while(count++ < HASH_COLLISION){
		entry = hash->entries[hash_key];

		if(entry){
			if(entry->key_len == key_len
			&& !memcmp(entry->key, key, key_len)){
				rcu_set_pointer(hash->entries[hash_key], entry_new);
				synchronize_rcu();
				free(entry->value);
				free(entry->key);
				free(entry);
				break;
			}
		}else{
			rcu_set_pointer(hash->entries[hash_key], entry_new);
			ret = 0;
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

	ixmapfwd_mutex_unlock(&hash->mutex);
	return 0;
	
err_hash_full:
	ixmapfwd_mutex_unlock(&hash->mutex);
	free(entry_new->value);
err_alloc_value:
	free(entry_new->key);
err_alloc_key:
	free(entry_new);
err_alloc_entry:
	return -1;
}

int hash_delete(struct hash *hash, void *key, int key_len)
{
	struct hash_entry *entry;
	unsigned int hash_key;
	int ret = -1, count = 0;

	hash_key = hash_key(key, key_len);
	ixmapfwd_mutex_lock(&hash->mutex);

	while(count++ < HASH_COLLISION){
		entry = hash->entries[hash_key];
		if(entry){
			if(entry->key_len == key_len
			&& !memcmp(entry->key, key, key_len)){
				rcu_set_pointer(hash->entries[hash_key], NULL);
				synchronize_rcu();
				free(entry->value);
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

	ixmapfwd_mutex_unlock(&hash->mutex);
	return 0;

err_not_found:
	ixmapfwd_mutex_unlock(&hash->mutex);
	return -1;
}

void hash_delete_walk(struct hash *hash)
{
	struct hash_entry *entry;
	unsigned int i;

	ixmapfwd_mutex_lock(&hash->mutex);

	for(i = 0; i < HASH_SIZE; i++){
		entry = hash->entries[i];

		if(entry){
			rcu_set_pointer(hash->entries[i], NULL);
			synchronize_rcu();
			free(entry->value);
			free(entry->key);
			free(entry);
		}
	}

	ixmapfwd_mutex_unlock(&hash->mutex);
	return;
}

/* rcu_read_lock needs to be hold by caller from readside */
void *hash_lookup(struct hash *hash, void *key, int key_len)
{
	struct hash_entry *entry, *entry_ret = NULL;
	unsigned int hash_key;
	int count = 0;

	hash_key = hash_key(key, key_len);
	while(count++ < HASH_COLLISION){
		entry = hash->entries[hash_key];

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

