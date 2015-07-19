
int fib_import(struct fib *fib, char *command)
{
	int opcode, family;
	uint32_t prefix[4], nexthop[4];
	unsigned int prefix_len;

	switch(opcode){
		case FIB_ADD:
			fib_route_update(fib, family, prefix, prefix_len, nexthop);
			break;
		case FIB_DELETE:
			fib_route_delete(fib, family, prefix, prefix_len);
			break;
		default:
	}


        return 0;

}

int fib_route_update(struct fib *fib, int family,
	uint32_t *prefix, unsigned int prefix_len, uint32_t *nexthop)
{
	uint32_t destination[4];
	struct fib_entry entry, *entry_real;
	unsigned int port_index;
	int family_len;

	switch(family){
		case AF_INET:
			family_len = 32;
			break;
		case AF_INET6:
			family_len = 128;
			break;
		default:
	}

	// TBD: Get the port index here

	memcpy(entry->nexthop, nexthop, family_len >> 3);
	memcpy(entry->prefix, prefix, family_len >> 3);
	entry->prefix_len = prefix_len;

	trie_add_ascii(fib->trie_root, family, prefix, prefix_len,
		&entry, sizeof(struct fib_entry));

	if(!prefix_len){
		/* This is the default route */
		memcpy(&fib->default, &entry, sizeof(struct fib_entry));
		fib->default_exist = 1;
		return 0;
	}

	for(i = 0; i < 1 << (family_len - prefix_len); i++){
		destination = prefix + i;
		
		trie_lookup_ascii(fib->trie_root, family, destination,
			(void **)&entry_real);
		hash_add(fib->hash_root, destination, family_len >> 3,
			entry_real, sizeof(struct fib_entry));
	}

	return 0;
}

int fib_route_delete(struct fib *fib, int family,
	uint32_t *prefix, unsigned int prefix_len)
{
	uint32_t destination[4];
	struct fib_entry entry, *entry_real;
	int family_len, ret;

	switch(family){
		case AF_INET:
			family_len = 32;
			break;
		case AF_INET6:
			family_len = 128;
			break;
		default:
	}

	ret = trie_delete_ascii(fib->trie_root, family, prefix, prefix_len);
	if(ret < 0){
		return -1;
	}

	if(!prefix_len){
		/* This is the default route */
		memset(&fib->default, 0, sizeof(struct fib_entry));
		fib->default_exist = 0;
		return 0;
	}

	for(i = 0; i < 1 << (family_len - prefix_len); i++){
		destination = prefix + i;

		ret = trie_lookup_ascii(fib->trie_root, family, destination,
			(void **)&entry_real);
		if(!entry_real->prefix_len || ret < 0){
			hash_delete(fib->hash_root, destination, family_len >> 3);
		}else{
			hash_add(fib->hash_root, destination, family_len >> 3,
				entry_real, sizeof(struct fib_entry));
		}
	}

	return 0;
}

struct fib_entry *fib_lookup(struct fib *fib, int family,
	uint32_t *destination)
{
	struct fib_entry *entry;
	int family_len;

	switch(family){
		case AF_INET:
			family_len = 32;
			break;
		case AF_INET6:
			family_len = 128;
			break;
		default:
	}

	entry = hash_lookup(fib->hash_root, destination, family_len >> 3);
	if(!entry){
		if(fib->default_exist){
			return &fib->default;
		}else{
			return NULL;
		}
	}
	
	return entry;
}
