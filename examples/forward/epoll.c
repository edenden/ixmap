
int epoll_open()
{
	int fd;

	fd = epoll_create(EPOLL_MAXEVENTS);
	if(fd < 0){
		perror("failed to make epoll fd");
		return -1;
	}

	return fd;
}

int epoll_open_signalfd(sigset_t *sigset)
{
	int fd;

	fd = signalfd(-1, sigset, 0);
	if(fd < 0){
		perror("signalfd");
		return -1;
	}

	return fd;
}

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

struct epoll_desc *epoll_desc_alloc_irqdev(struct ixmap_instance *instance,
	unsigned int port_index, unsigned int queue_index,
	enum ixmap_irq_direction direction)
{
	struct epoll_desc *ep_desc;
	struct ixmap_irqdev_handle *irqh;
	int type;

	ep_desc = malloc(sizeof(struct epoll_desc));
	if(!ep_desc)
		goto err_alloc_ep_desc;

	irqh = ixmap_irqdev_open(instance, port_index, queue_index, direction);
	if(!irqh_rx){
		perror("failed to open");
		goto err_open_irqdev;
	}

	switch(direction){
	case IXMAP_IRQ_RX:
		type = EPOLL_IRQ_RX;
		break;
	case IXMAP_IRQ_TX:
		type = EPOLL_IRQ_TX;
		break;
	default:
		goto err_invalid_type;
		break;
	}

	ep_desc->fd = ixmap_irqdev_fd(irqh);
	ep_desc->type = type;
	ep_desc->data = irqh;
	ep_desc->next = NULL;

	return ep_desc;

err_invalid_type:
	ixmap_irqdev_close(irqh);
err_open_irqdev:
	free(ep_desc);
err_alloc_ep_desc:
	return NULL;
}

void epoll_desc_release_irqdev(struct epoll_desc *ep_desc)
{
	ixmap_irqdev_close((struct ixmap_irqdev_handle *)ep_desc->data);
	free(ep_desc);
	return;
}

int epoll_desc_alloc_singalfd(sigset_t *sigset)
{
	struct epoll_desc *ep_desc;
	int fd;

	ep_desc = malloc(sizeof(struct epoll_desc));
	if(!ep_desc)
		goto err_alloc_ep_desc;

	fd = epoll_open_signalfd(&sigset);
	if(fd < 0){
		perror("failed to open signalfd");
		goto err_open_signalfd;
	}

	ep_desc->fd = fd;
	ep_desc->type = EPOLL_SIGNAL;
	ep_desc->data = NULL;
	ep_desc->next = NULL;

	return ep_desc;

err_open_signalfd:
	free(ep_desc);
err_alloc_ep_desc:
	return NULL;
}

void epoll_desc_release_signalfd(struct epoll_desc *ep_desc)
{
	close(ep_desc->fd);
	free(ep_desc);
	return;
}

int epoll_desc_alloc_tun()
{

}

int epoll_desc_release_tun()
{

}
