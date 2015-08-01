#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <arpa/inet.h>
#include <unistd.h>

void netlink_process(uint8_t *buf, int read_size)
{
	struct nlmsghdr *nlh;
	int ret;

	nlh = (struct nlmsghdr *)buf;

	while(NLMSG_OK(nlh, read_size)){
		switch(nlh->nlmsg_type){
		case RTM_NEWROUTE:
		case RTM_DELROUTE:
			netlink_route(nlh);
			break;
		case RTM_NEWNEIGH:
		case RTM_DELNEIGH:
			netlink_neigh(nlh);
			break;
		default:
			printf("unknown type\n");
			break;
		}

		nlh = NLMSG_NEXT(nlh, read_size);
	}

	return;
}

void netlink_route(struct nlmsghdr *nlh, struct fib *fib)
{
	struct rtmsg *route_entry;
	struct rtattr *route_attr;
	int route_attr_len, family;
	uint32_t prefix[4], nexthop[4];
	unsigned int prefix_len, port_index;
	int type;

	route_entry = (struct rtmsg *)NLMSG_DATA(nlh);
	family		= route_entry->rtm_family;
	prefix_len	= route_entry->rtm_dst_len;

	/* if packet matches RT_TABLE_LOCAL, then inject it to kernel */
	switch(route_entry->rtm_table){
	case RT_TABLE_MAIN:
		type = FORWARD;
		break;
	case RT_TABLE_LOCAL:
		type = LOCAL;
		break;
	default:
		goto ign_route_table;
		break;
	}

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
			break;
		default:
			break;
		}

		route_attr = RTA_NEXT(route_attr, route_attr_len);
	}

	// TBD: get dest port index here

	switch(nlh->nlmsg_type){
	case RTM_NEWROUTE:
		fib_route_update(fib, family, prefix, prefix_len,
			nexthop, port_index, type);
		break;
	case RTM_DELROUTE:
		fib_route_delete(fib, family, prefix, prefix_len);
		break;
	default:
		break;
	}

ign_route_table:
	return;
}

void netlink_neigh(struct nlmsghdr *nlh, struct neigh_table *neigh)
{
	struct ndmsg *neigh_entry;
	struct rtattr *route_attr;
	int route_attr_len;
	int ifindex;
	int family;
	uint32_t dst_addr[4];
	uint8_t dst_mac[ETH_ALEN];

	neigh_entry = (struct ndmsg *)NLMSG_DATA(nlh);
	family		= neigh_entry->ndm_family;
	ifindex		= neigh_entry->ndm_ifindex;

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

	// TBD: support per port neigh table

	switch(nlh->nlmsg_type){
	case RTM_NEWNEIGH:
		neigh_add(neigh, family, dst_addr, dst_mac);
		break;
	case RTM_DELNEIGH:
		neigh_delete(neigh, family, dst_addr);
		break;
	default:
		break;
	}

	return;
}
