#ifndef _IXMAPFWD_THREAD_H
#define _IXMAPFWD_THREAD_H

#define EPOLL_MAXEVENTS 16

enum {
	IXMAPFWD_IRQ_RX = 0,
	IXMAPFWD_IRQ_TX,
	IXMAPFWD_SIGNAL
};

struct ixmapfwd_fd_desc {
	int				fd;
	int				type;
	struct ixmap_irqdev_handle	*irqh;
};

void *process_interrupt(void *data);

#endif /* _IXMAPFWD_THREAD_H */
