#ifndef _IXMAPFWD_TUN_H
#define _IXMAPFWD_TUN_H

struct tun {
	int		fd;
        unsigned int	ifindex;
	unsigned int	mtu_frame;
};

struct tun *tun_open(char *if_name, uint8_t *src_mac,
	unsigned int mtu_frame);
void tun_close(struct tun *tun);

#endif /* _IXMAPFWD_TUN_H */
