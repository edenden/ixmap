#define EPOLL_MAXEVENTS 16

struct ixgbe_irq_data {
	int	fd;
	int	direction;
	int	port_index;
};

#define IXGBE_DEFAULT_TXD	512
#define IXGBE_MAX_TXD		4096
#define IXGBE_MIN_TXD		64

#define IXGBE_DEFAULT_RXD	512
#define IXGBE_MAX_RXD		4096
#define IXGBE_MIN_RXD		64

/* Receive Descriptor - Advanced */
union ixgbe_adv_rx_desc {
	struct {
		__le64 pkt_addr; /* Packet buffer address */
		__le64 hdr_addr; /* Header buffer address */
	} read;
	struct {
		struct {
			union {
				__le32 data;
				struct {
					__le16 pkt_info; /* RSS, Pkt type */
					__le16 hdr_info; /* Splithdr, hdrlen */
				} hs_rss;
			} lo_dword;
			union {
				__le32 rss; /* RSS Hash */
				struct {
					__le16 ip_id; /* IP id */
					__le16 csum; /* Packet Checksum */
				} csum_ip;
			} hi_dword;
		} lower;
		struct {
			__le32 status_error; /* ext status/error */
			__le16 length; /* Packet length */
			__le16 vlan; /* VLAN tag */
		} upper;
	} wb;  /* writeback */
};

/* Transmit Descriptor - Advanced */
union ixgbe_adv_tx_desc {
	struct {
		__le64 buffer_addr; /* Address of descriptor's data buf */
		__le32 cmd_type_len;
		__le32 olinfo_status;
	} read;
	struct {
		__le64 rsvd; /* Reserved */
		__le32 nxtseq_seed;
		__le32 status;
	} wb;
};

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

static inline uint32_t readl(const volatile void *addr)
{
	return cpu_to_le32( *(volatile uint32_t *) addr );
}

static inline void writel(uint32_t b, volatile void *addr)
{
	*(volatile uint32_t *) addr = cpu_to_le32(b);
}

static inline uint32_t IXGBE_READ_REG(struct ixgbe_handler *ih, uint32_t reg)
{
	uint32_t value = readl(ih->bar + reg);
	return value;
}

static inline void IXGBE_WRITE_REG(struct ixgbe_handler *ih, uint32_t reg, uint32_t value)
{
	writel(value, ih->bar + reg);
	return;
}

#define IXGBE_WRITE_FLUSH(a) IXGBE_READ_REG(a, IXGBE_STATUS)

/* ixgbe_desc_unused - calculate if we have unused descriptors */
static inline uint16_t ixgbe_desc_unused(struct ixgbe_ring *ring)
{
        uint16_t next_to_clean = ring->next_to_clean;
        uint16_t next_to_use = ring->next_to_use;

	return next_to_clean > next_to_use
		? next_to_clean - next_to_use - 1
		: (count - next_to_use) + next_to_clean - 1;
}

/* ixgbe_test_staterr - tests bits in Rx descriptor status and error fields */
static inline __le32 ixgbe_test_staterr(union ixgbe_adv_rx_desc *rx_desc,
                                        const u32 stat_err_bits)
{
        return rx_desc->wb.upper.status_error & cpu_to_le32(stat_err_bits);
}

static inline void ixgbe_write_tail(struct ixgbe_ring *ring, u32 value)
{
	writel(value, ring->tail);
}

#define IXGBE_RX_DESC(R, i)     \
	(&(((union ixgbe_adv_rx_desc *)((R)->desc))[i]))
#define IXGBE_TX_DESC(R, i)     \
	(&(((union ixgbe_adv_tx_desc *)((R)->desc))[i]))

void *process_interrupt(void *data);