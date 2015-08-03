#ifndef _IXMAPFWD_EPOLL_H
#define _IXMAPFWD_EPOLL_H

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

#endif /* _IXMAPFWD_EPOLL_H */
