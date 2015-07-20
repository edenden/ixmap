struct arp_table {
	struct hash_root *hash_root;
};

struct arp_entry {
	uint8_t mac_addr[6];
};
