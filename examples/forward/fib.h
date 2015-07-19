struct fib_entry {
	uint32_t		prefix[4];
	unsigned int		prefix_len;
	uint32_t		nexthop[4];

	struct fib_entry	default;
	int			default_exist;
};

struct fib {
	struct trie_node	*trie_node;
	struct hash_root	*hash_root;
	struct fib_entry	default;
	int			default_exist;
};
