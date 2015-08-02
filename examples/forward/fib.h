#define FIB_FLAG_UNICAST 0x0001
#define FIB_FLAG_MULTICAST 0x0002

enum fib_type {
	FIB_TYPE_FORWARD = 0,
	FIB_TYPE_LINK
};

struct fib_entry {
	uint32_t		prefix[4];
	unsigned int		prefix_len;
	uint32_t		nexthop[4];
	int			port_index; /* -1 means not ixmap interface */
	enum fib_type		type;
	uint16_t		flag;
	struct list_node	node;
};

struct fib {
	struct trie_tree	tree;
	pthread_mutex_t		mutex;
};
