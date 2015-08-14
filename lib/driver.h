#ifndef _IXMAP_DRIVER_H
#define _IXMAP_DRIVER_H

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

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

#define prefetch(x)	__builtin_prefetch(x, 0)
#define prefetchw(x)	__builtin_prefetch(x, 1)

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
	(&(((union ixmap_adv_rx_desc *)((R)->addr_virtual))[i]))
#define IXGBE_TX_DESC(R, i)	\
	(&(((union ixmap_adv_tx_desc *)((R)->addr_virtual))[i]))

#endif /* _IXMAP_DRIVER_H */
