#define EPOLL_MAXEVENTS 16

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
#define ACCESS_ONCE(x) (*(volatile typeof(x) *)&(x))

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

void *process_interrupt(void *data);
