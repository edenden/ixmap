#ifndef _IXMAPFWD_FORWARD_H
#define _IXMAPFWD_FORWARD_H

#include "thread.h"

void forward_process(struct ixmapfwd_thread *thread, unsigned int port_index,
	struct ixmap_bulk **bulk_array);
void forward_process_tun(struct ixmapfwd_thread *thread, unsigned int port_index,
	struct ixmap_bulk **bulk_array, uint8_t *read_buf, int read_size);

#endif /* _IXMAPFWD_FORWARD_H */
