#ifndef _IXMAPFWD_FORWARD_H
#define _IXMAPFWD_FORWARD_H

#include "thread.h"

void forward_process(int slot_index, unsigned int slot_size,
	unsigned int port_index, void *opaque);
void forward_process_tun(struct ixmapfwd_thread *thread, unsigned int port_index,
	uint8_t *read_buf, unsigned int read_size);

#endif /* _IXMAPFWD_FORWARD_H */
