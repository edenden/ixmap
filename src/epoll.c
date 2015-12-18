#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <numa.h>

#include "main.h"
#include "epoll.h"

int epoll_add(int fd_ep, void *ptr, int fd)
{
	struct epoll_event event;
	int ret;

	memset(&event, 0, sizeof(struct epoll_event));
	event.events = EPOLLIN;
	event.data.ptr = ptr;
	ret = epoll_ctl(fd_ep, EPOLL_CTL_ADD, fd, &event);
	if(ret < 0)
		return -1;

	return 0;
}

int epoll_del(int fd_ep, int fd)
{
	int ret;

	ret = epoll_ctl(fd_ep, EPOLL_CTL_DEL, fd, NULL);
	if(ret < 0)
		return -1;

	return 0;
}

struct epoll_desc *epoll_desc_alloc_irq(struct ixmap_plane *plane,
	unsigned int port_index, unsigned int core_id,
	enum ixmap_irq_type type)
{
	struct epoll_desc *ep_desc;
	int ep_type;

	ep_desc = numa_alloc_onnode(sizeof(struct epoll_desc),
		numa_node_of_cpu(core_id));
	if(!ep_desc)
		goto err_alloc_ep_desc;

	switch(type){
	case IXMAP_IRQ_RX:
		ep_type = EPOLL_IRQ_RX;
		break;
	case IXMAP_IRQ_TX:
		ep_type = EPOLL_IRQ_TX;
		break;
	default:
		goto err_invalid_type;
		break;
	}

	ep_desc->fd		= ixmap_irq_fd(plane, port_index, type);
	ep_desc->type		= ep_type;
	ep_desc->port_index	= port_index;
	ep_desc->data		= ixmap_irq_handle(plane, port_index, type);

	return ep_desc;

err_invalid_type:
	numa_free(ep_desc, sizeof(struct epoll_desc));
err_alloc_ep_desc:
	return NULL;
}

void epoll_desc_release_irq(struct epoll_desc *ep_desc)
{
	numa_free(ep_desc, sizeof(struct epoll_desc));
	return;
}

struct epoll_desc *epoll_desc_alloc_signalfd(sigset_t *sigset,
	unsigned int core_id)
{
	struct epoll_desc *ep_desc;
	int fd;

	ep_desc = numa_alloc_onnode(sizeof(struct epoll_desc),
		numa_node_of_cpu(core_id));
	if(!ep_desc)
		goto err_alloc_ep_desc;

	fd = signalfd(-1, sigset, 0);
	if(fd < 0){
		perror("failed to open signalfd");
		goto err_open_signalfd;
	}

	ep_desc->fd = fd;
	ep_desc->type = EPOLL_SIGNAL;

	return ep_desc;

err_open_signalfd:
	numa_free(ep_desc, sizeof(struct epoll_desc));
err_alloc_ep_desc:
	return NULL;
}

void epoll_desc_release_signalfd(struct epoll_desc *ep_desc)
{
	close(ep_desc->fd);
	numa_free(ep_desc, sizeof(struct epoll_desc));
	return;
}

struct epoll_desc *epoll_desc_alloc_tun(struct tun_plane *tun_plane,
	unsigned int port_index, unsigned int core_id)
{
	struct epoll_desc *ep_desc;

	ep_desc = numa_alloc_onnode(sizeof(struct epoll_desc),
		numa_node_of_cpu(core_id));
	if(!ep_desc)
		goto err_alloc_ep_desc;

	ep_desc->fd		= tun_plane->ports[port_index].fd;
	ep_desc->type		= EPOLL_TUN;
	ep_desc->port_index	= port_index;

	return ep_desc;

err_alloc_ep_desc:
	return NULL;
}

void epoll_desc_release_tun(struct epoll_desc *ep_desc)
{
	numa_free(ep_desc, sizeof(struct epoll_desc));
	return;
}

struct epoll_desc *epoll_desc_alloc_netlink(struct sockaddr_nl *addr,
	unsigned int core_id)
{
	struct epoll_desc *ep_desc;
	int fd, ret;
	
	ep_desc = numa_alloc_onnode(sizeof(struct epoll_desc),
		numa_node_of_cpu(core_id));
	if(!ep_desc)
		goto err_alloc_ep_desc;

	fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if(fd < 0)
		goto err_open_netlink;

	ret = bind(fd, (struct sockaddr *)addr,
		sizeof(struct sockaddr_nl));
	if(ret < 0)
		goto err_bind_netlink;

	ep_desc->fd = fd;
	ep_desc->type = EPOLL_NETLINK;

	return ep_desc;

err_bind_netlink:
	close(fd);
err_open_netlink:
	numa_free(ep_desc, sizeof(struct epoll_desc));
err_alloc_ep_desc:
	return NULL;
}

	
void epoll_desc_release_netlink(struct epoll_desc *ep_desc)
{
	close(ep_desc->fd);
	numa_free(ep_desc, sizeof(struct epoll_desc));
	return;
}
