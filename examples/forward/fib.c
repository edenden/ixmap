
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
