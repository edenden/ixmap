#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <ixmap.h>

#ifdef DEBUG
static void forward_dump(struct ixmap_buf *buf, struct ixmap_bulk *bulk)
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

void forward_process(struct ixmap_buf *buf, unsigned int port_index,
	struct ixmap_bulk *bulk_rx, struct ixmap_bulk **bulk_tx,
	struct tun_instance *instance_tun, struct neigh_table *neigh,
	struct fib *fib)
{
	unsigned short count;
	int slot_index, i, ret;
	unsigned int slot_size;
	void *slot_buf;
	struct ethhdr *eth;

	/* TEMP */
	uint8_t src_mac[6];
	struct in_addr src_ip;

	src_mac[0] = 0x9c; src_mac[1] = 0xb6; src_mac[2] = 0x54;
	src_mac[3] = 0xbb; src_mac[4] = 0xfb; src_mac[5] = 0xe8;
	inet_pton(AF_INET, "203.178.138.110", &src_ip);

	count = ixmap_bulk_slot_count(bulk_rx);
	for(i = 0; i < count; i++){
		ixmap_bulk_slot_get(bulk_rx, i, &slot_index, &slot_size);
		slot_buf = ixmap_slot_addr_virt(buf, slot_index);

		eth = (struct ethhdr *)slot_buf;
		switch(ntohs(eth->h_proto)){
		case ETH_P_ARP:
			ret = packet_arp_process(port_index, slot_buf,
				slot_size, instance_tun);
			break;
		case ETH_P_IP:
			ret = packet_ip_process(port_index, slot_buf,
				slot_size, instance_tun);
			break;
		case ETH_P_IPV6:
			packet_ip6_process();
			ret = -1;
			break;
		default:
			ret = -1;
			break;
		}

		if(ret < 0){
			goto packet_drop;
		}

		ret = ixmap_bulk_slot_push(bulk_tx[ret],
			slot_index, slot_size);
		if(ret < 0)
			goto packet_drop;

		continue;
packet_drop:
		ixmap_slot_release(buf, slot_index);
	}

	return;
}

void forward_process_tun(unsigned int port_index,
	uint8_t *read_buf, int read_size, struct ixmap_bulk **bulk_tx)
{
	int slot_index, ret;
	void *slot_buf;
	unsigned int slot_size;

	slot_index = ixmap_slot_assign(buf);
	if(slot_index < 0){
		goto err_slot_assign;
	}

	slot_buf = ixmap_slot_addr_virt(buf, slot_index);
	slot_size = ixmap_slot_size(buf);

	if(read_size > slot_size)
		goto err_slot_size;

	memcpy(slot_buf, read_buf, read_size);

	ret = ixmap_bulk_slot_push(bulk_tx[port_index],
		slot_index, read_size);
	if(ret < 0){
		goto err_slot_push;
	}

	return;

err_slot_push:
err_slot_size:
	ixmap_slot_release(buf, slot_index);
err_slot_assign:
	return;
}

int forward_arp_process(unsigned int port_index,
	void *slot_buf, unsigned int slot_size,
	struct tun_instance *instance_tun)
{
	int fd, ret;

	fd = instance_tun->ports[port_index].fd;
	ret = write(fd, slot_buf, slot_size);
	if(ret < 0)
		goto err_write_tun;

	return -1;

err_write_tun:
	return -1;
}

int forward_ip_process(unsigned int port_index,
	void *slot_buf, unsigned int slot_size,
	struct tun_instance *instance_tun,
	struct neigh_table *neigh, struct fib *fib)
{
	struct ethhdr *eth;
	struct iphdr *ip;
	struct fib_entry *fib_entry;
	struct neigh_entry *neigh_entry;
	int fd, ret;

	eth = (struct ethhdr *)slot_buf;
	ip = (struct iphdr *)(slot_buf + sizeof(struct ethhdr));

	rcu_read_lock();
	
	fib_entry = fib_lookup(fib, AF_INET, &ip->daddr);
	if(!fib_entry){
		goto packet_drop;
	}

	neigh_entry = neigh_lookup(neigh, AF_INET, fib_entry->nexthop);
	if(!neigh_entry){
		fd = instance_tun->ports[port_index].fd;
		ret = write(fd, slot_buf, slot_size);
		if(ret < 0)
			goto err_write_tun;

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

	ret = fib_entry->port_index;
	rcu_read_unlock();

	return ret;

err_write_tun:
packet_drop:
	rcu_read_unlock();
	return -1;
}

void forward_ip6_process()
{
	struct ethhdr *eth;
	struct ip6_hdr *ip6;
	struct fib_entry *fib_entry;
	struct arp_entry *arp_entry;
	int slot_index_new, ret;
	void *slot_buf_new;
	unsigned int slot_size_new;

	eth = (struct ethhdr *)slot_buf;
	ip6 = (struct ip6_hdr *)(slot_buf + sizeof(struct ethhdr));

	fib_entry = fib_lookup(fib, AF_INET6, &ip6->ip6_dst);
	if(!fib_entry){
		goto packet_drop;
	}

	return;
}

