#ifndef _IXMAPFWD_EPOLL_H
#define _IXMAPFWD_EPOLL_H

#include <linux/netlink.h>
#include <signal.h>
#include <ixmap.h>
#include "linux/list.h"
#include "linux/list_rcu.h"

#define EPOLL_MAXEVENTS 16

enum {
	EPOLL_IRQ_RX = 0,
	EPOLL_IRQ_TX,
	EPOLL_TUN,
	EPOLL_SIGNAL,
	EPOLL_NETLINK
};

struct epoll_desc {
	int			fd;
	int			type;
	void			*data;
	struct list_head	list;
};

int epoll_add(int fd_ep, void *ptr, int fd);
struct epoll_desc *epoll_desc_alloc_irqdev(struct ixmap_plane *plane,
	unsigned int port_index, unsigned int queue_index,
	enum ixmap_irq_direction direction);
void epoll_desc_release_irqdev(struct epoll_desc *ep_desc);
struct epoll_desc *epoll_desc_alloc_signalfd(sigset_t *sigset);
void epoll_desc_release_signalfd(struct epoll_desc *ep_desc);
struct epoll_desc *epoll_desc_alloc_tun(struct tun **tun,
	unsigned int port_index);
void epoll_desc_release_tun(struct epoll_desc *ep_desc);
struct epoll_desc *epoll_desc_alloc_netlink(struct sockaddr_nl *addr);
void epoll_desc_release_netlink(struct epoll_desc *ep_desc);

#endif /* _IXMAPFWD_EPOLL_H */
