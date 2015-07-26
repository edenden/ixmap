#ifndef _IXMAPFWD_TUN_H
#define _IXMAPFWD_TUN_H

struct tun_handle {
	int		fd;
        unsigned int	ifindex;
};

struct tun_handle *tun_open(char *if_name, uint8_t *src_mac,
	unsigned int mtu_frame);
void tun_close(struct tun_handle *tun);
struct tun_instance *tun_instance_alloc(struct tun_handle **th_list,
	int tun_num);
void tun_instance_release(struct tun_instance *instance);

#endif /* _IXMAPFWD_TUN_H */
