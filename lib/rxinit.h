#ifndef _IXMAP_RXINIT_H
#define _IXMAP_RXINIT_H

#define VLAN_HLEN		4
#define ETH_FCS_LEN		4
#define VLAN_ETH_FRAME_LEN	1518
#define MAXIMUM_ETHERNET_VLAN_SIZE \
				(VLAN_ETH_FRAME_LEN + ETH_FCS_LEN)
#define DMA_64BIT_MASK		0xffffffffffffffffULL
#define DMA_BIT_MASK(n)		(((n) == 64) ? \
				DMA_64BIT_MASK : ((1ULL<<(n))-1))
#define IXGBE_MAX_RX_DESC_POLL	10

/* Supported Rx Buffer Sizes */
#define IXGBE_MAX_RXBUFFER      16384  /* largest size for single descriptor */

/* Receive Registers */
#define IXGBE_RDBAL(_i)		(0x01000 + ((_i) * 0x40))
#define IXGBE_RDBAH(_i)		(0x01004 + ((_i) * 0x40))
#define IXGBE_RDLEN(_i)		(0x01008 + ((_i) * 0x40)) 
#define IXGBE_RDH(_i)		(0x01010 + ((_i) * 0x40))
#define IXGBE_RDT(_i)		(0x01018 + ((_i) * 0x40))
#define IXGBE_RXDCTL(_i)	(0x01028 + ((_i) * 0x40))
#define IXGBE_SRRCTL(_i)	(0x02100 + ((_i) * 4))
#define IXGBE_RDRXCTL		0x02F00
#define IXGBE_RXCTRL		0x03000
#define IXGBE_RSCDBU		0x03028
#define IXGBE_HLREG0		0x04240
#define IXGBE_MHADD		0x04268
#define IXGBE_RXCSUM		0x05000
#define IXGBE_RFCTL		0x05008
#define IXGBE_FCTRL		0x05080
#define IXGBE_VLNCTRL		0x05088
#define IXGBE_PSRTYPE		0x05480
#define IXGBE_MRQC		0x05818
#define IXGBE_RETA(_i)		(0x05C00 + ((_i) * 4))  /* 32 of these (0-31) */
#define IXGBE_RSSRK(_i)		(0x05C80 + ((_i) * 4))  /* 10 of these (0-9) */
#define IXGBE_PFDTXGSWC		0x08220
#define IXGBE_VMOLR		0x0F000

/* Receive Config masks */
#define IXGBE_RXCTRL_RXEN	0x00000001 /* Enable Receiver */
#define IXGBE_RXDCTL_ENABLE	0x02000000 /* Ena specific Rx Queue */

/* SRRCTL bit definitions */
#define IXGBE_SRRCTL_BSIZEPKT_SHIFT \
				10 /* so many KBs */
#define IXGBE_SRRCTL_DESCTYPE_ADV_ONEBUF \
				0x02000000
#define IXGBE_SRRCTL_DROP_EN	0x10000000

/* RDRXCTL Bit Masks */
#define IXGBE_RDRXCTL_CRCSTRIP	0x00000002 /* CRC Strip */
#define IXGBE_RDRXCTL_RSCFRSTSIZE \
				0x003E0000 /* RSC First packet size */

/* RSCDBU Bit Masks */
#define IXGBE_RSCDBU_RSCACKDIS	0x00000080

/* HLREG0 Bit Masks */
#define IXGBE_HLREG0_JUMBOEN	0x00000004 /* bit  2 */

/* MHADD Bit Masks */
#define IXGBE_MHADD_MFS_MASK	0xFFFF0000
#define IXGBE_MHADD_MFS_SHIFT	16

/* Receive Checksum Control */
#define IXGBE_RXCSUM_PCSD	0x00002000 /* packet checksum disabled */

/* Header split receive */
#define IXGBE_RFCTL_RSC_DIS	0x00000020

/* Flow Control Bit Masks */
#define IXGBE_FCTRL_MPE		0x00000100 /* Multicast Promiscuous Ena*/
#define IXGBE_FCTRL_UPE		0x00000200 /* Unicast Promiscuous Ena */
#define IXGBE_FCTRL_BAM		0x00000400 /* Broadcast Accept Mode */
#define IXGBE_FCTRL_PMCF	0x00001000 /* Pass MAC Control Frames */
#define IXGBE_FCTRL_DPF		0x00002000 /* Discard Pause Frame */

/* VLAN Control Bit Masks */
#define IXGBE_VLNCTRL_CFIEN	0x20000000  /* bit 29 */
#define IXGBE_VLNCTRL_VFE	0x40000000  /* bit 30 */

/* PSRTYPE bit definitions */
#define IXGBE_PSRTYPE_TCPHDR	0x00000010
#define IXGBE_PSRTYPE_UDPHDR	0x00000020
#define IXGBE_PSRTYPE_IPV4HDR	0x00000100
#define IXGBE_PSRTYPE_IPV6HDR	0x00000200
#define IXGBE_PSRTYPE_L2HDR	0x00001000

/* Multiple Receive Queue Control */
#define IXGBE_MRQC_RSSEN	0x00000001  /* RSS Enable */
#define IXGBE_MRQC_RSS_FIELD_IPV4_TCP \
				0x00010000
#define IXGBE_MRQC_RSS_FIELD_IPV4 \
				0x00020000
#define IXGBE_MRQC_RSS_FIELD_IPV6 \
				0x00100000
#define IXGBE_MRQC_RSS_FIELD_IPV6_TCP \
				0x00200000
#define IXGBE_MRQC_RSS_FIELD_IPV4_UDP \
				0x00400000
#define IXGBE_MRQC_RSS_FIELD_IPV6_UDP \
				0x00800000

/* VT switch Bit Masks */
#define IXGBE_PFDTXGSWC_VT_LBEN	0x1 /* Local L2 VT switch enable */

/* VMOLR bitmasks */
#define IXGBE_VMOLR_AUPE	0x01000000 /* accept untagged packets */
#define IXGBE_VMOLR_ROMPE	0x02000000 /* accept packets in MTA tbl */
#define IXGBE_VMOLR_ROPE	0x04000000 /* accept packets in UC tbl */
#define IXGBE_VMOLR_BAM		0x08000000 /* accept broadcast packets */
#define IXGBE_VMOLR_MPE		0x10000000 /* multicast promiscuous */

#endif /* _IXMAP_RXINIT_H */
