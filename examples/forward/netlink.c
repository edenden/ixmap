#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <arpa/inet.h>
#include <unistd.h>

void netlink_process(struct ixmapfwd *ixmapfwd,
	uint8_t *read_buf, int read_size)
{
	struct nlmsghdr *nlh;
	int ret;

	nlh = (struct nlmsghdr *)read_buf;

	while(NLMSG_OK(nlh, read_size)){
		switch(nlh->nlmsg_type){
		case RTM_NEWROUTE:
		case RTM_DELROUTE:
			netlink_route(ixmapfwd, nlh);
			break;
		case RTM_NEWNEIGH:
		case RTM_DELNEIGH:
			netlink_neigh(ixmapfwd, nlh);
			break;
		default:
			printf("unknown type\n");
			break;
		}

		nlh = NLMSG_NEXT(nlh, read_size);
	}

	return;
}

void netlink_route(struct ixmapfwd *ixmapfwd, struct nlmsghdr *nlh)
{
	struct rtmsg *route_entry;
	struct rtattr *route_attr;
	int route_attr_len, family;
	uint32_t prefix[4] = {};
	uint32_t nexthop[4] = {};
	unsigned int prefix_len;
	int ifindex, port_index;
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

	for(i = 0; i < ixmapfwd->num_ports; i++){
		if(ixmapfwd->tun[i]->ifindex == ifindex){
			port_index = i;
			break;
		}
	}

	/* TBD: handle link local route */

	switch(nlh->nlmsg_type){
	case RTM_NEWROUTE:
		fib_route_update(fib, family, prefix, prefix_len,
			nexthop, port_index, type, ifindex);
		break;
	case RTM_DELROUTE:
		fib_route_delete(fib, family, prefix, prefix_len, ifindex);
		break;
	default:
		break;
	}

	return;
}

void netlink_neigh(struct ixmapfwd *ixmapfwd, struct nlmsghdr *nlh)
{
	struct ndmsg *neigh_entry;
	struct rtattr *route_attr;
	int route_attr_len;
	int ifindex;
	int family;
	uint32_t dst_addr[4] = {};
	uint8_t dst_mac[ETH_ALEN] = {};
	int i, port_index = -1;

	neigh_entry = (struct ndmsg *)NLMSG_DATA(nlh);
	family		= neigh_entry->ndm_family;
	ifindex		= neigh_entry->ndm_ifindex;
	port_index 	= -1;

	for(i = 0; i < ixmapfwd->num_ports; i++){
		if(ixmapfwd->tun[i]->ifindex == ifindex){
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
		neigh_add(ixmapfwd->neigh[port_index],
			family, dst_addr, dst_mac);
		break;
	case RTM_DELNEIGH:
		neigh_delete(ixmapfwd->neigh[port_index],
			family, dst_addr);
		break;
	default:
		break;
	}

ign_ifindex:
	return;
}
