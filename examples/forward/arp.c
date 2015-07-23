#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>

int arp_generate(uint16_t opcode, void *buf, int buf_len,
	uint8_t *dest_mac, uint8_t *src_mac,
	uint32_t *dest_ip, uint32_t *src_ip)
{
	struct ethhdr *eth;
	struct arphdr *arp;
	int len = 0;

	eth = (struct ethhdr *)buf;
	len += sizeof(struct ethhdr);
	if(len > buf_len)
		goto err_buf_size;

	memset(eth->h_dest, 0xFF, ETH_ALEN);
	memcpy(eth->h_source, src_mac, ETH_ALEN);
	eth->h_proto = htons(ETH_P_ARP);

	arp = (struct arphdr *)(buf + len);
	len += sizeof(struct arphdr);
	if(len > buf_len)
		goto err_buf_size;

	arp->ar_hrd	= htons(ARPHRD_ETHER);
	arp->ar_pro	= htons(ETH_P_IP);
	arp->ar_hln	= ETH_ALEN;
	arp->ar_pln	= 4;
	arp->ar_op	= htons(opcode);

	if(len + 8 + ETH_ALEN * 2 > buf_len){
		goto err_buf_size;
	}

	memcpy(buf + len, src_mac, ETH_ALEN);
	len += ETH_ALEN;

	memcpy(buf + len, src_ip, 4);
	len += 4;

	if(opcode == ARPOP_REQUEST){
		memset(buf + len, 0, ETH_ALEN);
	}else if(opcode == ARPOP_REPLY){
		memcpy(buf + len, dest_mac, ETH_ALEN);
	}else{
		goto err_unsupp_opcode;
	}
	len += ETH_ALEN;

	memcpy(buf + len, dest_ip, 4);
	len += 4;

	return len;

err_unsupp_opcode:
err_buf_size:
	return -1;
}

int arp_learn(struct arp *arp, void *buf, int buf_len,
	uint8_t *src_mac, uint32_t *src_ip)
{
	struct arp_entry entry;
	struct ethhdr *eth;
	struct arphdr *arp;
	uint8_t *arp_dst_mac;
	uint8_t *arp_src_mac;
	uint32_t *arp_dst_ip;
	uint32_t *arp_src_ip;
	int ret, len = 0;

	eth = (struct ethhdr *)buf;
	len += sizeof(struct ethhdr);
	if(len > buf_len)
		goto err_buf_size;

	arp = (struct arphdr *)(buf + len);
	len += sizeof(struct arphdr);
	if(len > buf_len)
		goto err_buf_size;

	if(len + 8 + ETH_ALEN * 2 > buf_len)
		goto err_buf_size;

	arp_src_mac = (uint8_t *)(buf + len);
	len += ETH_ALEN;

	arp_src_ip = (uint32_t *)(buf + len);
	len += 4;

	arp_dst_mac = (uint8_t *)(buf + len);
	len += ETH_ALEN;

	arp_dst_ip = (uint32_t *)(buf + len);
	len += 4;

	memcpy(entry.mac_addr, arp_src_mac, ETH_ALEN);
	ret = hash_add(arp->root, arp_src_ip, sizeof(uint32_t),
		&entry, sizeof(struct arp_entry));
	if(ret < 0){
		goto err_hash_add;
	}
	
	if(arp->ar_op == htons(ARPOP_REQUEST)
	&& !memcmp(src_ip, arp_dst_ip, 4)){
		return 0;
	}

	return -1;

err_hash_add:
err_buf_size:
	return -1;
}

struct arp_entry *arp_lookup(struct arp *arp, uint32_t *dst_ip)
{
	struct arp_entry *arp_entry;

	arp_entry = (struct arp_entry *)hash_lookup(
		arp->hash_root, dst_ip, sizeof(uint32_t));
	return arp_entry;
}

