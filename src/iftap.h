#ifndef _IXMAPFWD_TUN_H
#define _IXMAPFWD_TUN_H

#include "main.h"

#define TAP_IFNAME "ixmap"

struct tun_handle {
	int		*queues;
        unsigned int	ifindex;
	unsigned int	mtu_frame;
};

struct tun_port {
	int		fd;
	unsigned int	ifindex;
	unsigned int	mtu_frame;
};

struct tun_plane {
	struct tun_port	*ports;
};

struct tun_handle *tun_open(struct ixmapfwd *ixmapfwd,
	unsigned int port_index);
void tun_close(struct ixmapfwd *ixmapfwd, unsigned int port_index);
struct tun_plane *tun_plane_alloc(struct ixmapfwd *ixmapfwd,
	int queue_index);
void tun_plane_release(struct tun_plane *plane);

#endif /* _IXMAPFWD_TUN_H */
