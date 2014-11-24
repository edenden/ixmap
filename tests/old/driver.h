#define EPOLL_MAXEVENTS 16

enum {
	IXGBE_IRQ_RX = 0,
	IXGBE_IRQ_TX,
	IXGBE_SIGNAL
};

struct ixgbe_irq_data {
	int	fd;
	int	type;
	int	port_index;
};

/* RX descriptor defines */
#define IXGBE_DEFAULT_RXD	512
#define IXGBE_MAX_RXD		4096
#define IXGBE_MIN_RXD		64

/* Receive Descriptor bit definitions */
#define IXGBE_RXD_STAT_DD	0x01 /* Descriptor Done */
#define IXGBE_RXDADV_ERR_CE     0x01000000 /* CRC Error */
#define IXGBE_RXDADV_ERR_LE     0x02000000 /* Length Error */
#define IXGBE_RXDADV_ERR_PE     0x08000000 /* Packet Error */
#define IXGBE_RXDADV_ERR_OSE    0x10000000 /* Oversize Error */
#define IXGBE_RXDADV_ERR_USE    0x20000000 /* Undersize Error */

#define IXGBE_RXDADV_ERR_FRAME_ERR_MASK ( \
				IXGBE_RXDADV_ERR_CE | \
				IXGBE_RXDADV_ERR_LE | \
				IXGBE_RXDADV_ERR_PE | \
				IXGBE_RXDADV_ERR_OSE | \
				IXGBE_RXDADV_ERR_USE)

/* TX descriptor defines */
#define IXGBE_DEFAULT_TXD	512
#define IXGBE_MAX_TXD		4096
#define IXGBE_MIN_TXD		64
#define IXGBE_MAX_TXD_PWR	14
#define IXGBE_MAX_DATA_PER_TXD	(1 << IXGBE_MAX_TXD_PWR)

/* Send Descriptor bit definitions */
#define IXGBE_TXD_STAT_DD	0x00000001 /* Descriptor Done */
#define IXGBE_TXD_CMD_EOP	0x01000000 /* End of Packet */
#define IXGBE_TXD_CMD_IFCS	0x02000000 /* Insert FCS (Ethernet CRC) */
#define IXGBE_TXD_CMD_RS	0x08000000 /* Report Status */
#define IXGBE_TXD_CMD_DEXT	0x20000000 /* Desc extension (0 = legacy) */

/* Adv Transmit Descriptor Config Masks */
#define IXGBE_ADVTXD_DTYP_DATA	0x00300000 /* Adv Data Descriptor */
#define IXGBE_ADVTXD_DCMD_IFCS	IXGBE_TXD_CMD_IFCS /* Insert FCS */
#define IXGBE_ADVTXD_DCMD_DEXT	IXGBE_TXD_CMD_DEXT /* Desc ext 1=Adv */
#define IXGBE_ADVTXD_PAYLEN_SHIFT \
				14 /* Adv desc PAYLEN shift */

/* Receive Descriptor - Advanced */
union ixgbe_adv_rx_desc {
	struct {
		uint64_t pkt_addr; /* Packet buffer address */
		uint64_t hdr_addr; /* Header buffer address */
	} read;
	struct {
		struct {
			union {
				uint32_t data;
				struct {
					uint16_t pkt_info; /* RSS, Pkt type */
					uint16_t hdr_info; /* Splithdr, hdrlen */
				} hs_rss;
			} lo_dword;
			union {
				uint32_t rss; /* RSS Hash */
				struct {
					uint16_t ip_id; /* IP id */
					uint16_t csum; /* Packet Checksum */
				} csum_ip;
			} hi_dword;
		} lower;
		struct {
			uint32_t status_error; /* ext status/error */
			uint16_t length; /* Packet length */
			uint16_t vlan; /* VLAN tag */
		} upper;
	} wb;  /* writeback */
};

/* Transmit Descriptor - Advanced */
union ixgbe_adv_tx_desc {
	struct {
		uint64_t buffer_addr; /* Address of descriptor's data buf */
		uint32_t cmd_type_len;
		uint32_t olinfo_status;
	} read;
	struct {
		uint64_t rsvd; /* Reserved */
		uint32_t nxtseq_seed;
		uint32_t status;
	} wb;
};

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

static inline uint32_t readl(const volatile void *addr)
{
	return htole32( *(volatile uint32_t *) addr );
}

static inline void writel(uint32_t b, volatile void *addr)
{
	*(volatile uint32_t *) addr = htole32(b);
}

static inline uint32_t IXGBE_READ_REG(struct ixgbe_handle *ih, uint32_t reg)
{
	uint32_t value = readl(ih->bar + reg);
	return value;
}

static inline void IXGBE_WRITE_REG(struct ixgbe_handle *ih, uint32_t reg, uint32_t value)
{
	writel(value, ih->bar + reg);
	return;
}

#define IXGBE_WRITE_FLUSH(a) IXGBE_READ_REG(a, IXGBE_STATUS)

/*
 * ixgbe_desc_unused - calculate if we have unused descriptors.
 * "-1" ensures next_to_clean does not overtake next_to_clean.
 */
static inline uint16_t ixgbe_desc_unused(struct ixgbe_ring *ring,
	uint16_t num_desc)
{
        uint16_t next_to_clean = ring->next_to_clean;
        uint16_t next_to_use = ring->next_to_use;

	return next_to_clean > next_to_use
		? next_to_clean - next_to_use - 1
		: (num_desc - next_to_use) + next_to_clean - 1;
}

/* ixgbe_test_staterr - tests bits in Rx descriptor status and error fields */
static inline uint32_t ixgbe_test_staterr(union ixgbe_adv_rx_desc *rx_desc,
                                        const uint32_t stat_err_bits)
{
	return rx_desc->wb.upper.status_error & htole32(stat_err_bits);
}

static inline void ixgbe_write_tail(struct ixgbe_ring *ring, uint32_t value)
{
	writel(value, ring->tail);
}

#if defined(__x86_64__) || defined(__i386__) || defined(__amd64__)
#define mb()  asm volatile("mfence" ::: "memory")
#define wmb() asm volatile("sfence" ::: "memory")
#define rmb() asm volatile("lfence" ::: "memory")
#else
#define mb()  asm volatile("" ::: "memory")
#define rmb() asm volatile("" ::: "memory")
#define wmb() asm volatile("" ::: "memory")
#endif

#define IXGBE_RX_DESC(R, i)	\
	(&(((union ixgbe_adv_rx_desc *)((R)->addr_virtual))[i]))
#define IXGBE_TX_DESC(R, i)	\
	(&(((union ixgbe_adv_tx_desc *)((R)->addr_virtual))[i]))

void *process_interrupt(void *data);
