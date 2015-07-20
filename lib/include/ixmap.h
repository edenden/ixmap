#ifndef _IXMAP_H
#define _IXMAP_H

/*
 * microsecond values for various ITR rates shifted by 2 to fit itr register
 * with the first 3 bits reserved 0
 */
#define IXGBE_MIN_RSC_ITR	24
#define IXGBE_100K_ITR		40
#define IXGBE_20K_ITR		200
#define IXGBE_16K_ITR		248
#define IXGBE_10K_ITR		400
#define IXGBE_8K_ITR		500

/* RX descriptor defines */
#define IXGBE_DEFAULT_RXD	512
#define IXGBE_MAX_RXD		4096
#define IXGBE_MIN_RXD		64

/* TX descriptor defines */
#define IXGBE_DEFAULT_TXD	512
#define IXGBE_MAX_TXD		4096
#define IXGBE_MIN_TXD		64

struct ixmap_handle;
struct ixmap_irqdev_handle;
struct ixmap_buf;
struct ixmap_instance;
struct ixmap_bulk;

enum ixmap_irq_direction {
	IXMAP_IRQ_RX = 0,
	IXMAP_IRQ_TX,
};

void ixmap_irq_enable(struct ixmap_handle *ih);
struct ixmap_instance *ixmap_instance_alloc(struct ixmap_handle **ih_list,
	int ih_num, int queue_index);
void ixmap_instance_release(struct ixmap_instance *instance);
int ixmap_desc_alloc(struct ixmap_handle *ih,
	uint32_t num_rx_desc, uint32_t num_tx_desc);
void ixmap_desc_release(struct ixmap_handle *ih);
struct ixmap_buf *ixmap_buf_alloc(struct ixmap_handle **ih_list,
	int ih_num, uint32_t count, uint32_t buf_size);
void ixmap_buf_release(struct ixmap_buf *buf,
	struct ixmap_handle **ih_list, int ih_num);
struct ixmap_handle *ixmap_open(char *interface_name,
	unsigned int num_queues_req, unsigned int budget, unsigned short intr_rate,
	unsigned int mtu_frame, unsigned int promisc);
void ixmap_close(struct ixmap_handle *ih);
unsigned int ixmap_bufsize_get(struct ixmap_handle *ih);
struct ixmap_irqdev_handle *ixmap_irqdev_open(struct ixmap_instance *instance,
	unsigned int port_index, unsigned int queue_index,
	enum ixmap_irq_direction direction);
void ixmap_irqdev_close(struct ixmap_irqdev_handle *irqh);
int ixmap_irqdev_setaffinity(struct ixmap_irqdev_handle *irqh,
	unsigned int core_id);
int ixmap_irqdev_fd(struct ixmap_irqdev_handle *irqh);

inline void ixmap_irq_unmask_queues(struct ixmap_instance *instance,
	struct ixmap_irqdev_handle *irqh);
inline unsigned int ixmap_budget(struct ixmap_instance *instance,
	unsigned int port_index);
inline unsigned int ixmap_port_index(struct ixmap_irqdev_handle *irqh);
struct ixmap_bulk *ixmap_bulk_alloc(struct ixmap_instance *instance,
	unsigned int num_ports);
void ixmap_bulk_release(struct ixmap_bulk *bulk);
unsigned short ixmap_bulk_count_get(struct ixmap_bulk *bulk);
void ixmap_bulk_count_set(struct ixmap_bulk *bulk, unsigned short count);
int ixmap_bulk_slot_index_get(struct ixmap_bulk *bulk,
	unsigned int index);
void ixmap_bulk_slot_index_set(struct ixmap_bulk *bulk,
	unsigned int index, int slot_index);
unsigned int ixmap_bulk_slot_size_get(struct ixmap_bulk *bulk,
	unsigned int index);
void ixmap_bulk_slot_size_set(struct ixmap_bulk *bulk,
	unsigned int index, unsigned int size);
void ixmap_rx_alloc(struct ixmap_instance *instance, unsigned int port_index,
	struct ixmap_buf *buf);
int ixmap_tx_alloc(struct ixmap_instance *instance, struct ixmap_buf *buf,
	uint16_t max_allocation, struct ixmap_bulk *bulk);
void ixmap_tx_xmit(struct ixmap_instance *instance, unsigned int port_index,
	struct ixmap_buf *buf, struct ixmap_bulk *bulk);
int ixmap_rx_clean(struct ixmap_instance *instance, unsigned int port_index,
	struct ixmap_buf *buf, struct ixmap_bulk *bulk);
int ixmap_tx_clean(struct ixmap_instance *instance, unsigned int port_index,
	struct ixmap_buf *buf);
inline void *ixmap_slot_addr_virt(struct ixmap_buf *buf,
	uint16_t slot_index);
inline unsigned long ixmap_count_rx_alloc_failed(struct ixmap_instance *instance,
	unsigned int port_index);
inline unsigned long ixmap_count_rx_clean_total(struct ixmap_instance *instance,
	unsigned int port_index);
inline unsigned long ixmap_count_tx_alloc_failed(struct ixmap_instance *instance);
inline unsigned long ixmap_count_tx_xmit_failed(struct ixmap_instance *instance,
	unsigned int port_index);
inline unsigned long ixmap_count_tx_clean_total(struct ixmap_instance *instance,
	unsigned int port_index);

void ixmap_configure_rx(struct ixmap_handle *ih);
void ixmap_configure_tx(struct ixmap_handle *ih);

inline uint32_t ixmap_read_reg(struct ixmap_handle *ih, uint32_t reg);
inline void ixmap_write_reg(struct ixmap_handle *ih, uint32_t reg, uint32_t value);

#endif /* _IXMAP_H */
