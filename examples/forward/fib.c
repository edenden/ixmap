struct fib *fib_alloc()
{
        struct fib *fib;

	fib = malloc(sizeof(struct fib));
	if(!fib)
		goto err_fib_alloc;

	fib->tree = trie_alloc();
	if(!fib->tree)
		goto err_trie_alloc;

	return fib;

err_trie_alloc:
	free(fib);
err_fib_alloc:
	return NULL;
}

void fib_release(struct fib *fib)
{
	trie_release(fib->tree);
	free(fib);
	return;
}

int fib_route_update(struct fib *fib, int family,
	uint32_t *prefix, unsigned int prefix_len, uint32_t *nexthop)
{
	struct fib_entry entry;
	unsigned int port_index, family_len;

	switch(family){
	case AF_INET:
		family_len = 32;
		break;
	case AF_INET6:
		family_len = 128;
		break;
	default:
		goto err_invalid_family;
		break;
	}

	// TBD: Get the port index here

	memcpy(entry.nexthop, nexthop, ALIGN(family_len, 8) >> 3);
	memcpy(entry.prefix, prefix, ALIGN(family_len, 8) >> 3);
	entry.prefix_len = prefix_len;
	entry.port_index = port_index;

	trie_add(fib->trie_root, family_len, prefix, prefix_len,
		&entry, sizeof(struct fib_entry));

	return 0;

err_invalid_family:
	return -1;
}

int fib_route_delete(struct fib *fib, int family,
	uint32_t *prefix, unsigned int prefix_len)
{
	unsigned int family_len;

	switch(family){
	case AF_INET:
		family_len = 32;
		break;
	case AF_INET6:
		family_len = 128;
		break;
	default:
		goto err_invalid_family;
		break;
	}

	return trie_delete(fib->trie_root, family_len, prefix, prefix_len);

err_invalid_family:
	return -1;
}

struct fib_entry *fib_lookup(struct fib *fib, int family,
	uint32_t *destination)
{
	unsigned int family_len;

	switch(family){
	case AF_INET:
		family_len = 32;
		break;
	case AF_INET6:
		family_len = 128;
		break;
	default:
		goto err_invalid_family;
		break;
	}

	return trie_lookup(fib->trie_root, family_len, destination);

err_invalid_family:
	return -1;
}
