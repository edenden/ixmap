#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <linux/if_ether.h>

#include "main.h"
#include "thread.h"
#include "netlink.h"
#include "fib.h"
#include "neigh.h"
#include "tun.h"

static void netlink_route(struct ixmapfwd_thread *thread, struct nlmsghdr *nlh);
static void netlink_neigh(struct ixmapfwd_thread *thread, struct nlmsghdr *nlh);

void netlink_process(struct ixmapfwd_thread *thread,
	uint8_t *read_buf, int read_size)
{
	struct nlmsghdr *nlh;

	nlh = (struct nlmsghdr *)read_buf;

	while(NLMSG_OK(nlh, read_size)){
		switch(nlh->nlmsg_type){
		case RTM_NEWROUTE:
		case RTM_DELROUTE:
			netlink_route(thread, nlh);
			break;
		case RTM_NEWNEIGH:
		case RTM_DELNEIGH:
			netlink_neigh(thread, nlh);
			break;
		default:
			printf("unknown type\n");
			break;
		}

		nlh = NLMSG_NEXT(nlh, read_size);
	}

	return;
}

static void netlink_route(struct ixmapfwd_thread *thread, struct nlmsghdr *nlh)
{
	struct rtmsg *route_entry;
	struct rtattr *route_attr;
	int route_attr_len, family;
	uint8_t prefix[16] = {};
	uint8_t nexthop[16] = {};
	unsigned int prefix_len;
	int ifindex, port_index, i;
	enum fib_type type;

	route_entry = (struct rtmsg *)NLMSG_DATA(nlh);
	family		= route_entry->rtm_family;
	prefix_len	= route_entry->rtm_dst_len;
	ifindex		= -1;
	port_index	= -1;
	type		= FIB_TYPE_LINK;

	route_attr = (struct rtattr *)RTM_RTA(route_entry);
	route_attr_len = RTM_PAYLOAD(nlh);

	while(RTA_OK(route_attr, route_attr_len)){
		switch(route_attr->rta_type){
		case RTA_DST:
			memcpy(prefix, RTA_DATA(route_attr),
				RTA_PAYLOAD(route_attr));
			break;
		case RTA_GATEWAY:
			memcpy(nexthop, RTA_DATA(route_attr),
				RTA_PAYLOAD(route_attr));
			type = FIB_TYPE_FORWARD;
			break;
		case RTA_OIF:
			ifindex = *(int *)RTA_DATA(route_attr);
			break;
		default:
			break;
		}

		route_attr = RTA_NEXT(route_attr, route_attr_len);
	}

	if(route_entry->rtm_table == RT_TABLE_LOCAL)
		type = FIB_TYPE_LOCAL;

	for(i = 0; i < thread->num_ports; i++){
		if(thread->tun_plane->ports[i].ifindex == ifindex){
			port_index = i;
			break;
		}
	}

	switch(nlh->nlmsg_type){
	case RTM_NEWROUTE:
		fib_route_update(thread->fib, family, type,
			prefix, prefix_len, nexthop, port_index, ifindex,
			thread->desc);
		break;
	case RTM_DELROUTE:
		fib_route_delete(thread->fib, family,
			prefix, prefix_len, ifindex);
		break;
	default:
		break;
	}

	return;
}

static void netlink_neigh(struct ixmapfwd_thread *thread, struct nlmsghdr *nlh)
{
	struct ndmsg *neigh_entry;
	struct rtattr *route_attr;
	int route_attr_len;
	int ifindex;
	int family;
	uint8_t dst_addr[16] = {};
	uint8_t dst_mac[ETH_ALEN] = {};
	int i, port_index = -1;

	neigh_entry = (struct ndmsg *)NLMSG_DATA(nlh);
	family		= neigh_entry->ndm_family;
	ifindex		= neigh_entry->ndm_ifindex;
	port_index 	= -1;

	for(i = 0; i < thread->num_ports; i++){
		if(thread->tun_plane->ports[i].ifindex == ifindex){
			port_index = i;
			break;
		}
	}

	if(port_index < 0)
		goto ign_ifindex;

	route_attr = (struct rtattr *)RTM_RTA(neigh_entry);
	route_attr_len = RTM_PAYLOAD(nlh);

	while(RTA_OK(route_attr, route_attr_len)){
		switch(route_attr->rta_type){
		case NDA_DST:
			memcpy(dst_addr, RTA_DATA(route_attr),
				RTA_PAYLOAD(route_attr));
			break;
		case NDA_LLADDR:
			memcpy(dst_mac, RTA_DATA(route_attr),
				RTA_PAYLOAD(route_attr));
			break;
		default:
			break;
		}

		route_attr = RTA_NEXT(route_attr, route_attr_len);
	}

	switch(nlh->nlmsg_type){
	case RTM_NEWNEIGH:
		neigh_add(thread->neigh[port_index],
			family, dst_addr, dst_mac,
			thread->desc);
		break;
	case RTM_DELNEIGH:
		neigh_delete(thread->neigh[port_index],
			family, dst_addr);
		break;
	default:
		break;
	}

ign_ifindex:
	return;
}
