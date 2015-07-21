#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <ixmap.h>

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

static void packet_process(struct ixmap_buf *buf, unsigned int port_index,
	struct ixmap_bulk *bulk_rx, struct ixmap_bulk **bulk_tx)
{
	unsigned short count;
	int slot_index, i, ret;
	unsigned int slot_size;
	void *slot_buf;
	struct ethhdr *eth;

	/* TEMP */
	uint8_t src_mac[6];
	uint32_t src_ip;

	src_mac[0] = 0x9c; src_mac[1] = 0xb6; src_mac[2] = 0x54;
	src_mac[3] = 0xbb; src_mac[4] = 0xfb; src_mac[5] = 0xe8;
	inet_pton(AF_INET, "203.178.138.110", &src_ip);

	count = ixmap_bulk_count_get(bulk_rx);
	for(i = 0; i < count; i++){
		slot_index = ixmap_bulk_slot_index_get(bulk_rx, i);
		slot_size = ixmap_bulk_slot_size_get(bulk_rx, i);
		slot_buf = ixmap_slot_addr_virt(buf, slot_index);

		eth = (struct ethhdr *)slot_buf;
		switch(ntohs(eth->h_proto)){
		case ETH_P_ARP:
			ret = packet_arp_process();
			if(ret < 0){
				goto packet_drop;
			}
			break;
		case ETH_P_IP:
			ret = packet_ip_process(buf, slot_buf, slot_index,
				slot_size, bulk_rx, bulk_tx);
			if(ret < 0){
				goto packet_drop;
			}
			break;
		case ETH_P_IPV6:
			packet_ip6_process();
			goto packet_drop;
			break;
		default:
			goto packet_drop;
			break;
		}

		continue;

packet_drop:
		ixmap_slot_release(buf, slot_index);
	}
}

int packet_arp_process(struct ixmap_buf *buf, unsigned int port_index,
	void *slot_buf, int slot_index, int slot_size,
	struct ixmap_bulk *bulk_rx, struct ixmap_bulk **bulk_tx)
{
	int index_arp, slot_index_arp, ret;
	void *slot_buf_arp;
	unsigned int slot_size_arp;

	ret = arp_learn(slot_buf, slot_size, src_mac, src_ip);
	if(ret < 0){
		goto packet_drop;
	}

	bulk_tx_count_max = ixmap_bulk_max_count_get(bulk_tx[fib_entry->port_index]);
	bulk_tx_count = ixmap_bulk_count_get(bulk_tx[fib_entry->port_index]);
	if(unlikely(bulk_tx_count == bulk_tx_count_max)){
		goto err_bulk_tx_full;
	}

	index_arp = ixmap_bulk_slot_append(bulk_tx[port_index], buf);
	if(index_arp < 0){
		goto err_arp_append;
	}

	slot_index_arp = ixmap_bulk_slot_index_get(
		bulk_tx[port_index], index_arp);
	slot_buf_arp = ixmap_slot_addr_virt(buf, slot_index_arp);
	slot_size_arp = ixmap_bulk_slot_size_get(
		bulk_tx[port_index], index_arp);

	ret = arp_generate(slot_buf_arp, slot_size_arp, ARPOP_REPLY,
		NULL, src_mac, entry->nexthop, src_ip);
	if(ret < 0){
		goto err_arp_generate;
	}

	ixmap_bulk_slot_size_set(bulk_tx[entry->port_index],
		index_arp, ret);

	return -1;

err_arp_generate:
	ixmap_bulk_slot_pop(bulk_tx[port_index], buf);
err_arp_append:
packet_drop:
	return -1;
}

int packet_ip_process(struct ixmap_buf *buf, unsigned int port_index,
	void *slot_buf, int slot_index, int slot_size,
	struct ixmap_bulk *bulk_rx, struct ixmap_bulk **bulk_tx)
{
	struct ethhdr *eth;
	struct iphdr *ip;
	struct fib_entry *fib_entry;
	struct arp_entry *arp_entry;
	unsigned short bulk_tx_count, bulk_tx_count_max;
	int index_arp, slot_index_arp, ret;
	void *slot_buf_arp;
	unsigned int slot_size_arp;

	eth = (struct ethhdr *)slot_buf;
	ip = (struct iphdr *)(slot_buf + sizeof(struct ethhdr));

	fib_entry = fib_lookup(fib, AF_INET, &ip->daddr);
	if(!fib_entry){
		goto packet_drop;
	}

	bulk_tx_count_max = ixmap_bulk_max_count_get(bulk_tx[fib_entry->port_index]);
	bulk_tx_count = ixmap_bulk_count_get(bulk_tx[fib_entry->port_index]);
	if(unlikely(bulk_tx_count == bulk_tx_count_max)){
		goto err_bulk_tx_full;
	}

	arp_entry = arp_lookup(arp, fib_entry->nexthop);
	if(!arp_entry){
		index_arp = ixmap_bulk_slot_append(bulk_tx[entry->port_index], buf);
		if(index_arp < 0){
			goto err_arp_append;
		}

		slot_index_arp = ixmap_bulk_slot_index_get(
					bulk_tx[entry->port_index], index_arp);
		slot_buf_arp = ixmap_slot_addr_virt(buf, slot_index_arp);
		slot_size_arp = ixmap_bulk_slot_size_get(
					bulk_tx[entry->port_index], index_arp);
		ret = arp_generate(slot_buf_arp, slot_size_arp, ARPOP_REQUEST,
			NULL, src_mac, entry->nexthop, src_ip);
		if(ret < 0){
			goto err_arp_generate;
		}

		ixmap_bulk_slot_size_set(bulk_tx[entry->port_index],
			index_arp, ret);
		goto packet_drop;
	}

	memcpy(eth->h_dest, entry->mac_addr, ETH_ALEN);
	memcpy(eth->h_source, src_mac, ETH_ALEN);

	ip->ttl--;
	if(!ip->ttl){
		/* TBD: return the ICMP packet */
		goto packet_drop;
	}
	ip->check--;

	ixmap_bulk_slot_index_set(bulk_tx[entry->port_index],
		bulk_tx_count, slot_index);
	ixmap_bulk_slot_size_set(bulk_tx[entry->port_index],
		bulk_tx_count, slot_size);
	ixmap_bulk_count_set(bulk_tx[entry->port_index], ++bulk_tx_count);

	return 0;

err_arp_generate:
	ixmap_bulk_slot_pop(bulk_tx[entry->port_index], buf);
err_arp_append:
err_bulk_tx_full:
packet_drop:
	return -1;
}

void packet_ip6_process()
{
	return;
}
