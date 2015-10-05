#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <stddef.h>
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
	printf("\tdst %02x:%02x:%02x:%02x:%02x:%02x\n",
		eth->h_dest[0], eth->h_dest[1], eth->h_dest[2],
		eth->h_dest[3], eth->h_dest[4], eth->h_dest[5]);
	printf("\ttype 0x%x\n", eth->h_proto);
	printf("\tsize %d bytes\n", slot_size);
}
#endif

void forward_process(int slot_index, unsigned int slot_size,
	unsigned int port_index, void *opaque)
{
	struct ixmapfwd_thread *thread;
	void *slot_buf;
	struct ethhdr *eth;
	int ret;

	thread = (struct ixmapfwd_thread *)opaque;
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

	ixmap_tx_assign(thread->plane, ret, thread->buf,
		slot_index, slot_size);
	return;

packet_drop:
	ixmap_slot_release(thread->buf, slot_index);
	return;
}

void forward_process_tun(struct ixmapfwd_thread *thread, unsigned int port_index,
	uint8_t *read_buf, unsigned int read_size)
{
	int slot_index;
	unsigned int slot_size;
	void *slot_buf;

	slot_index = ixmap_slot_assign(thread->buf);
	if(slot_index < 0){
		goto err_slot_assign;
	}

	slot_buf = ixmap_slot_addr_virt(thread->buf, slot_index);
	slot_size = ixmap_slot_size(thread->buf);

	if(read_size > slot_size)
		goto err_slot_size;

	memcpy(slot_buf, read_buf, read_size);

#ifdef DEBUG
	forward_dump(slot_buf, read_size);
#endif

	ixmap_tx_assign(thread->plane, port_index, thread->buf,
		slot_index, read_size);
	return;

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
	struct fib_entry	*fib_entry;
	struct neigh_entry	*neigh_entry;
	uint8_t			*dst_mac, *src_mac;
	uint32_t		check;
	int			fd, ret;

	eth = (struct ethhdr *)slot_buf;
	ip = (struct iphdr *)(slot_buf + sizeof(struct ethhdr));

	fib_entry = fib_lookup(thread->fib_inet, &ip->daddr);
	if(!fib_entry)
		goto packet_drop;

	if(unlikely(fib_entry->port_index < 0))
		goto packet_local;

	switch(fib_entry->type){
	case FIB_TYPE_LOCAL:
		goto packet_local;
		break;
	case FIB_TYPE_LINK:
		neigh_entry = neigh_lookup(
			thread->neigh_inet[fib_entry->port_index],
			&ip->daddr);
		break;
	case FIB_TYPE_FORWARD:
		neigh_entry = neigh_lookup(
			thread->neigh_inet[fib_entry->port_index],
			fib_entry->nexthop);
		break;
	default:
		neigh_entry = NULL;
		break;
	}

	if(!neigh_entry)
		goto packet_local;

	if(unlikely(ip->ttl == 1))
		goto packet_local;

	ip->ttl--;

	check = ip->check;
	check += htons(0x0100);
	ip->check = check + ((check >= 0xFFFF) ? 1 : 0);

	dst_mac = neigh_entry->dst_mac;
	src_mac = ixmap_macaddr(thread->plane, fib_entry->port_index);
	memcpy(eth->h_dest, dst_mac, ETH_ALEN);
	memcpy(eth->h_source, src_mac, ETH_ALEN);

	ret = fib_entry->port_index;
	return ret;

packet_local:
	fd = thread->tun_plane->ports[port_index].fd;
	write(fd, slot_buf, slot_size);
packet_drop:
	return -1;
}

static int forward_ip6_process(struct ixmapfwd_thread *thread,
	unsigned int port_index, void *slot_buf, unsigned int slot_size)
{
	struct ethhdr		*eth;
	struct ip6_hdr		*ip6;
	struct fib_entry	*fib_entry;
	struct neigh_entry	*neigh_entry;
	uint8_t			*dst_mac, *src_mac;
	int			fd, ret;

	eth = (struct ethhdr *)slot_buf;
	ip6 = (struct ip6_hdr *)(slot_buf + sizeof(struct ethhdr));

	if(unlikely(ip6->ip6_dst.s6_addr[0] == 0xfe
	&& (ip6->ip6_dst.s6_addr[1] & 0xc0) == 0x80))
		goto packet_local;

	fib_entry = fib_lookup(thread->fib_inet6, (uint32_t *)&ip6->ip6_dst);
	if(!fib_entry)
		goto packet_drop;

	if(unlikely(fib_entry->port_index < 0))
		goto packet_local;

	switch(fib_entry->type){
	case FIB_TYPE_LOCAL:
		goto packet_local;
		break;
	case FIB_TYPE_LINK:
		neigh_entry = neigh_lookup(
			thread->neigh_inet6[fib_entry->port_index],
			&ip6->ip6_dst);
		break;
	case FIB_TYPE_FORWARD:
		neigh_entry = neigh_lookup(
			thread->neigh_inet6[fib_entry->port_index],
			fib_entry->nexthop);
		break;
	default:
		neigh_entry = NULL;
		break;
	}

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
	return ret;

packet_local:
	fd = thread->tun_plane->ports[port_index].fd;
	write(fd, slot_buf, slot_size);
packet_drop:
	return -1;
}

