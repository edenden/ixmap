#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <ixmap.h>

#include "main.h"
#include "forward.h"
#include "thread.h"

static int forward_arp_process(struct ixmapfwd_thread *thread,
	unsigned int port_index, void *slot_buf, unsigned int slot_size);
static int forward_ip_process(struct ixmapfwd_thread *thread,
	unsigned int port_index, void *slot_buf, unsigned int slot_size);
static int forward_ip6_process(struct ixmapfwd_thread *thread,
	unsigned int port_index, void *slot_buf, unsigned int slot_size);

#ifdef DEBUG
void forward_dump(void *slot_buf, unsigned int slot_size)
{
	struct ethhdr *eth;

	eth = (struct ethhdr *)slot_buf;

	printf("packet dump:\n");
	printf("\tsrc %02x:%02x:%02x:%02x:%02x:%02x\n",
		eth->h_source[0], eth->h_source[1], eth->h_source[2],
		eth->h_source[3], eth->h_source[4], eth->h_source[5]);
	printf("\tdst  %02x:%02x:%02x:%02x:%02x:%02x\n",
		eth->h_dest[0], eth->h_dest[1], eth->h_dest[2],
		eth->h_dest[3], eth->h_dest[4], eth->h_dest[5]);
	printf("\ttype 0x%x\n", eth->h_proto);
	printf("\tsize %d bytes\n", slot_size);
}
#endif

void forward_process(struct ixmapfwd_thread *thread, unsigned int port_index,
	struct ixmap_bulk **bulk_array)
{
	unsigned short count;
	int slot_index, i, ret;
	unsigned int slot_size;
	void *slot_buf;
	struct ethhdr *eth;

	count = ixmap_bulk_slot_count(bulk_array[thread->num_ports]);
	for(i = 0; i < count; i++){
		ixmap_bulk_slot_get(bulk_array[thread->num_ports], i,
			&slot_index, &slot_size);
		slot_buf = ixmap_slot_addr_virt(thread->buf, slot_index);

#ifdef DEBUG
		forward_dump(slot_buf, slot_size);
#endif

		eth = (struct ethhdr *)slot_buf;
		switch(ntohs(eth->h_proto)){
		case ETH_P_ARP:
			ret = forward_arp_process(thread, port_index, slot_buf,
				slot_size);
			break;
		case ETH_P_IP:
			ret = forward_ip_process(thread, port_index, slot_buf,
				slot_size);
			break;
		case ETH_P_IPV6:
			ret = forward_ip6_process(thread, port_index, slot_buf,
				slot_size);
			break;
		default:
			ret = -1;
			break;
		}

		if(ret < 0){
			goto packet_drop;
		}

		ret = ixmap_bulk_slot_push(bulk_array[ret],
			slot_index, slot_size);
		if(ret < 0)
			goto packet_drop;

		continue;
packet_drop:
		ixmap_slot_release(thread->buf, slot_index);
	}

	return;
}

void forward_process_tun(struct ixmapfwd_thread *thread, unsigned int port_index,
	struct ixmap_bulk **bulk_array, uint8_t *read_buf, int read_size)
{
	int slot_index, ret;
	void *slot_buf;
	unsigned int slot_size;

	slot_index = ixmap_slot_assign(thread->buf);
	if(slot_index < 0){
		goto err_slot_assign;
	}

	slot_buf = ixmap_slot_addr_virt(thread->buf, slot_index);
	slot_size = ixmap_slot_size(thread->buf);

	if(read_size > slot_size)
		goto err_slot_size;

	memcpy(slot_buf, read_buf, read_size);

	ret = ixmap_bulk_slot_push(bulk_array[port_index],
		slot_index, read_size);
	if(ret < 0){
		goto err_slot_push;
	}

	return;

err_slot_push:
err_slot_size:
	ixmap_slot_release(thread->buf, slot_index);
err_slot_assign:
	return;
}

static int forward_arp_process(struct ixmapfwd_thread *thread,
	unsigned int port_index, void *slot_buf, unsigned int slot_size)
{
	int fd, ret;

	fd = thread->tun_plane->ports[port_index].fd;
	ret = write(fd, slot_buf, slot_size);
	if(ret < 0)
		goto err_write_tun;

	return -1;

err_write_tun:
	return -1;
}

static int forward_ip_process(struct ixmapfwd_thread *thread,
	unsigned int port_index, void *slot_buf, unsigned int slot_size)
{
	struct ethhdr		*eth;
	struct iphdr		*ip;
	struct list_head	*head;
	struct fib_entry	*fib_entry;
	struct neigh_entry	*neigh_entry;
	uint8_t			*dst_mac, *src_mac;
	int			fd, ret;

	eth = (struct ethhdr *)slot_buf;
	ip = (struct iphdr *)(slot_buf + sizeof(struct ethhdr));

	rcu_read_lock();

	head = fib_lookup(thread->fib, AF_INET, &ip->daddr);
	if(!head)
		goto packet_drop;

	fib_entry = list_first_or_null_rcu(head, struct fib_entry, list);
	if(!fib_entry)
		goto packet_drop;

	if(unlikely(fib_entry->type == FIB_TYPE_LOCAL
	|| fib_entry->port_index < 0))
		goto packet_local;

	neigh_entry = neigh_lookup(thread->neigh[fib_entry->port_index],
		AF_INET, fib_entry->nexthop);
	if(!neigh_entry)
		goto packet_local;

	if(unlikely(ip->ttl == 1))
		goto packet_local;

	ip->ttl--;
	ip->check--;

	dst_mac = neigh_entry->dst_mac;
	src_mac = ixmap_macaddr(thread->plane, fib_entry->port_index);
	memcpy(eth->h_dest, dst_mac, ETH_ALEN);
	memcpy(eth->h_source, src_mac, ETH_ALEN);

	ret = fib_entry->port_index;
	rcu_read_unlock();

	return ret;

packet_local:
	fd = thread->tun_plane->ports[port_index].fd;
	write(fd, slot_buf, slot_size);
packet_drop:
	rcu_read_unlock();
	return -1;
}

static int forward_ip6_process(struct ixmapfwd_thread *thread,
	unsigned int port_index, void *slot_buf, unsigned int slot_size)
{
	struct ethhdr		*eth;
	struct ip6_hdr		*ip6;
	struct list_head	*head;
	struct fib_entry	*fib_entry;
	struct neigh_entry	*neigh_entry;
	uint8_t			*dst_mac, *src_mac;
	int			fd, ret;

	eth = (struct ethhdr *)slot_buf;
	ip6 = (struct ip6_hdr *)(slot_buf + sizeof(struct ethhdr));

	if(unlikely(ip6->ip6_dst.s6_addr[0] == 0xfe
	&& (ip6->ip6_dst.s6_addr[1] & 0xc0) == 0x80))
		goto packet_local;

	rcu_read_lock();

	head = fib_lookup(thread->fib, AF_INET6, (uint32_t *)&ip6->ip6_dst);
	if(!head)
		goto packet_drop;

	fib_entry = list_first_or_null_rcu(head, struct fib_entry, list);
	if(!fib_entry)
		goto packet_drop;

	if(unlikely(fib_entry->type == FIB_TYPE_LOCAL
	|| fib_entry->port_index < 0))
		goto packet_local;

	neigh_entry = neigh_lookup(thread->neigh[fib_entry->port_index],
		AF_INET6, fib_entry->nexthop);
	if(!neigh_entry)
		goto packet_local;

	if(unlikely(ip6->ip6_hlim == 1))
		goto packet_local;

	ip6->ip6_hlim--;

	dst_mac = neigh_entry->dst_mac;
	src_mac = ixmap_macaddr(thread->plane, fib_entry->port_index);
	memcpy(eth->h_dest, dst_mac, ETH_ALEN);
	memcpy(eth->h_source, src_mac, ETH_ALEN);

	ret = fib_entry->port_index;
	rcu_read_unlock();

	return ret;

packet_local:
	fd = thread->tun_plane->ports[port_index].fd;
	write(fd, slot_buf, slot_size);
packet_drop:
	return -1;
}

