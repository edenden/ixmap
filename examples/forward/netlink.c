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

void netlink_route(struct nlmsghdr *nlh){
	struct rtmsg *route_entry;
	struct rtattr *route_attr;
	int route_attr_len;
	char gw_ip[256], dst_ip[256];
	int family;
	unsigned int prefix_len;

	route_entry = (struct rtmsg *)NLMSG_DATA(nlh);
	family		= route_entry->rtm_family;
	prefix_len	= route_entry->rtm_dst_len;

	/* if packet matches RT_TABLE_LOCAL, then inject it to kernel */
	if(route_entry->rtm_table != RT_TABLE_MAIN
	&& route_entry->rtm_table != RT_TABLE_LOCAL)
		goto ign_route_table;

	route_attr = (struct rtattr *)RTM_RTA(route_entry);
	route_attr_len = RTM_PAYLOAD(nlh);

	while(RTA_OK(route_attr, route_attr_len)){
		switch(route_attr->rta_type){
		case RTA_DST:
			inet_ntop(family, RTA_DATA(route_attr),
				dst_ip, sizeof(dst_ip));
			break;
		case RTA_GATEWAY:
			inet_ntop(family, RTA_DATA(route_attr),
				gw_ip, sizeof(gw_ip));
			break;
		default:
			break;
		}

		route_attr = RTA_NEXT(route_attr, route_attr_len);
	}

	switch(nlh->nlmsg_type){
	case RTM_NEWROUTE:
		printf("route add: %s/%d -> %s\n", dst_ip, prefix_len, gw_ip);
		break;
	case RTM_DELROUTE:
		printf("route del: %s/%d -> %s\n", dst_ip, prefix_len, gw_ip);
		break;
	default:
		break;
	}

ign_route_table:
	return;
}

void netlink_neigh(struct nlmsghdr *nlh)
{
	struct ndmsg *neigh_entry;
	struct rtattr *route_attr;
	int route_attr_len;
	int ifindex;
	int family;
	uint8_t *temp;
	char dst_mac[256], dst_ip[256];

	neigh_entry = (struct ndmsg *)NLMSG_DATA(nlh);
	family		= neigh_entry->ndm_family;
	ifindex		= neigh_entry->ndm_ifindex;

	route_attr = (struct rtattr *)RTM_RTA(neigh_entry);
	route_attr_len = RTM_PAYLOAD(nlh);

	while(RTA_OK(route_attr, route_attr_len)){
		switch(route_attr->rta_type){
		case NDA_DST:
			inet_ntop(family, RTA_DATA(route_attr),
				dst_ip, sizeof(dst_ip));
			break;
		case NDA_LLADDR:
			temp = RTA_DATA(route_attr);
			sprintf(dst_mac, "%02x:%02x:%02x:%02x:%02x:%02x",
				temp[0], temp[1], temp[2], temp[3], temp[4], temp[5]);
			break;
		default:
			break;
		}

		route_attr = RTA_NEXT(route_attr, route_attr_len);
	}

	switch(nlh->nlmsg_type){
	case RTM_NEWNEIGH:
		printf("neigh add: %s -> %s\n", dst_ip, dst_mac);
		break;
	case RTM_DELNEIGH:
		printf("neigh del: %s -> %s\n", dst_ip, dst_mac);
		break;
	default:
		break;
	}

	return;
}
