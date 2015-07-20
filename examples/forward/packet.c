#ifdef DEBUG
static void packet_dump(struct ixmap_buf *buf, struct ixmap_bulk *bulk)
{
	unsigned short count;
	int slot_index;
	unsigned int size;
	struct ether_header *eth;
	int i;

	count = ixmap_bulk_count_get(bulk);
	for(i = 0; i < count; i++){
		slot_index = ixmap_bulk_slot_index_get(bulk, i);
		size = ixmap_bulk_slot_size_get(bulk, i);
		eth = (struct ether_header *)ixmap_slot_addr_virt(buf,
			slot_index);

		printf("packet dump:\n");
		printf("\tsrc %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n",
			eth->ether_shost[0], eth->ether_shost[1], eth->ether_shost[2],
			eth->ether_shost[3], eth->ether_shost[4], eth->ether_shost[5]);
		printf("\tdst %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n",
			eth->ether_dhost[0], eth->ether_dhost[1], eth->ether_dhost[2],
			eth->ether_dhost[3], eth->ether_dhost[4], eth->ether_dhost[5]);
		printf("\ttype 0x%x\n", eth->ether_type);
		printf("\tsize %d bytes\n", size);
	}
}
#endif

static void packet_process(struct ixmap_buf *buf,
	struct ixmap_bulk *bulk_rx, struct ixmap_bulk **bulk_tx)
{
	unsigned short count;
	int slot_index, i, ret;
	unsigned int size;
	struct ethhdr *eth;
	struct iphdr *ip;
	struct fib_entry *entry;
	unsigned short bulk_count, bulk_count_max;

	/* TEMP */
	uint8_t src_mac[6];
	uint32_t src_ip;

	src_mac[0] = 0x9c; src_mac[1] = 0xb6; src_mac[2] = 0x54;
	src_mac[3] = 0xbb; src_mac[4] = 0xfb; src_mac[5] = 0xe8;
	inet_pton(AF_INET, "203.178.138.110", &src_ip);

	count = ixmap_bulk_count_get(bulk_rx);
	for(i = 0; i < count; i++){
		slot_index = ixmap_bulk_slot_index_get(bulk, i);
		size = ixmap_bulk_slot_size_get(bulk, i);

		eth = (struct ethhdr *)ixmap_slot_addr_virt(buf, slot_index);
		switch(ntohs(eth->h_proto)){
		case ETH_P_ARP:
			ret = arp_learn(eth, size, src_mac, src_ip);
			if(ret > 0){

			}
			goto drop;
			break;
		case ETH_P_IP:
			ip = (struct iphdr *)(eth + 1);
			entry = fib_lookup(fib, AF_INET, &ip->daddr);

			bulk_count_max =
				ixmap_bulk_max_count_get(bulk_tx[entry->port_index]);
			bulk_count =
				ixmap_bulk_count_get(bulk_tx[entry->port_index]);
			if(unlikely(bulk_count == bulk_count_max)){
				goto drop;
			}

			ret = arp_lookup(arp, entry->nexthop);
			if(ret < 0){
				int index_arp, slot_index_arp;
				void *buf_arp;
				unsigned int buf_size_arp;

				index_arp = ixmap_bulk_slot_append(bulk_tx[entry->port_index], buf);
				if(index_arp < 0){
					goto drop;
				}

				slot_index_arp =
					ixmap_bulk_slot_index_get(bulk_tx[entry->port_index], index);
				buf_arp = (void *)ixmap_slot_addr_virt(buf, slot_index_append);
				buf_size_arp =
					ixmap_bulk_slot_size_get(bulk_tx[entry->port_index], index);
				buf_size_arp = arp_generate(buf_arp, buf_size_arp, ARPOP_REQUEST,
					NULL, src_mac, entry->nexthop, src_ip);

				ixmap_bulk_count_set(bulk_tx[entry->port_index], ++bulk_count);
				ixmap_bulk_slot_index_set(bulk_tx[entry->port_index],
					index_arp, slot_index_arp);
				ixmap_bulk_slot_size_set(bulk_tx[entry->port_index],
					index_arp, buf_size_arp);
				goto drop;
			}
			break;
		case ETH_P_IPV6:
			goto drop;
			break;
		default:
			goto drop;
			break;
		}

		continue;
drop:
		ixmap_bulk_slot_release(bulk_rx, buf, i);
	}
}

packet_arp_process()
{

}

packet_ip_process()
{

}

packet_ip6_process()
{

}
