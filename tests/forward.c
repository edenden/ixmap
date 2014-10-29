#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/epoll.h>

#include <net/ethernet.h>
#include <pthread.h>

#include "main.h"
#include "forward.h"

static int epoll_add(int fd_ep, int fd);

void *process_interrupt(void *data)
{
	struct ixgbe_thread *thread = data;
	struct epoll_event events[EPOLL_MAXEVENTS];
	int fd_ep, fd_intrx, fd_inttx, i, num_fd;
	char filename[FILENAME_SIZE];

	/* Rx interrupt fd preparing */
	snprintf(filename, sizeof(filename),
		"/dev/%s-intrx%d", thread->int_name, thread->index);
	fd_intrx = open(filename, O_RDWR);
        if (fd_intrx < 0)
		return NULL;

        /* Tx interrupt fd preparing */
        snprintf(filename, sizeof(filename),
		"/dev/%s-inttx%d", thread->int_name, thread->index);
        fd_inttx = open(filename, O_RDWR);
        if (fd_inttx < 0)
                return NULL;

	/* epoll fd preparing */
	fd_ep = epoll_create(EPOLL_MAXEVENTS);
	if(fd_ep < 0){
		perror("failed to make epoll fd");
		return NULL;
	}

	if(epoll_add(fd_ep, fd_intrx) != 0){
		perror("failed to add fd in epoll");
		return NULL;
	}

	if(epoll_add(fd_ep, fd_inttx) != 0){
		perror("failed to add fd in epoll");
		return NULL;
	}

	while(1){
		num_fd = epoll_wait(fd_ep, events, EPOLL_MAXEVENTS, -1);
		if(num_fd <= 0){
			perror("epoll_wait");
			break;
		}

		for(i = 0; i < num_fd; i++){
			if(events[i].data.fd == fd_intrx){
				/* Rx descripter cleaning */
			}else if(events[i].data.fd == fd_inttx){
				/* Tx descripter cleaning */
			}
		}
	}

	return NULL;
}

static int epoll_add(int fd_ep, int fd)
{
	struct epoll_event event;

	memset(&event, 0, sizeof(struct epoll_event));
	event.events = EPOLLIN;
	event.data.fd = fd;
	return epoll_ctl(fd_ep, EPOLL_CTL_ADD, fd, &event);
}
