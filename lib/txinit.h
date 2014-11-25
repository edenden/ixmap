/* Transmit registers */
#define IXGBE_DMATXCTL		0x04A80
#define IXGBE_RTTDCS		0x04900
#define IXGBE_TDBAL(_i)		(0x06000 + ((_i) * 0x40)) /* 32 of them (0-31)*/
#define IXGBE_TDBAH(_i)		(0x06004 + ((_i) * 0x40))
#define IXGBE_TDLEN(_i)		(0x06008 + ((_i) * 0x40))
#define IXGBE_TDH(_i)		(0x06010 + ((_i) * 0x40))
#define IXGBE_TDT(_i)		(0x06018 + ((_i) * 0x40))
#define IXGBE_TXDCTL(_i)	(0x06028 + ((_i) * 0x40))
#define IXGBE_TDWBAL(_i)	(0x06038 + ((_i) * 0x40))
#define IXGBE_TDWBAH(_i)	(0x0603C + ((_i) * 0x40))
#define IXGBE_MTQC		0x08120

/* Transmit DMA Config masks */
#define IXGBE_DMATXCTL_TE	0x1 /* Transmit Enable */

/* DCB registers */
#define IXGBE_RTTDCS_ARBDIS	0x00000040 /* DCB arbiter disable */

/* Transmit Config masks */
#define IXGBE_TXDCTL_ENABLE	0x02000000 /* Ena specific Tx Queue */
#define IXGBE_TXDCTL_SWFLSH	0x04000000 /* Tx Desc. wr-bk flushing */

/* Multiple Transmit Queue Command Register */
#define IXGBE_MTQC_64Q_1PB	0x0 /* 64 queues 1 pack buffer */

