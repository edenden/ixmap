#ifndef _IXGBE_TYPE_H_
#define _IXGBE_TYPE_H_

/* forward declaration */
struct ixgbe_hw;

/* Device IDs */
#define IXGBE_DEV_ID_82599_SFP		  0x10FB
#define IXGBE_DEV_ID_82599_SFP_FCOE	     0x1529
#define IXGBE_DEV_ID_82599_SFP_EM	       0x1507
#define IXGBE_DEV_ID_82599_SFP_SF2	      0x154D
#define IXGBE_DEV_ID_82599_SFP_SF_QP	    0x154A
#define IXGBE_DEV_ID_82599EN_SFP		0x1557
#define IXGBE_DEV_ID_82599_T3_LOM	       0x151C

/* Part Number String Length */
#define IXGBE_PBANUM_LENGTH     11

/* General Registers */
#define IXGBE_CTRL	      0x00000
#define IXGBE_STATUS	    0x00008
#define IXGBE_CTRL_EXT	  0x00018
#define IXGBE_ESDP	      0x00020
#define IXGBE_EODSDP	    0x00028
#define IXGBE_I2CCTL	    0x00028
#define IXGBE_PHY_GPIO	  0x00028
#define IXGBE_MAC_GPIO	  0x00030
#define IXGBE_PHYINT_STATUS0    0x00100
#define IXGBE_PHYINT_STATUS1    0x00104
#define IXGBE_PHYINT_STATUS2    0x00108
#define IXGBE_LEDCTL	    0x00200
#define IXGBE_FRTIMER	   0x00048
#define IXGBE_TCPTIMER	  0x0004C
#define IXGBE_CORESPARE	 0x00600
#define IXGBE_EXVET	     0x05078

/* NVM Registers */
#define IXGBE_EEC       0x10010
#define IXGBE_EERD      0x10014
#define IXGBE_EEWR      0x10018
#define IXGBE_FLA       0x1001C
#define IXGBE_EEMNGCTL  0x10110
#define IXGBE_EEMNGDATA 0x10114
#define IXGBE_FLMNGCTL  0x10118
#define IXGBE_FLMNGDATA 0x1011C
#define IXGBE_FLMNGCNT  0x10120
#define IXGBE_FLOP      0x1013C
#define IXGBE_GRC       0x10200
#define IXGBE_SRAMREL   0x10210
#define IXGBE_PHYDBG    0x10218

/* EEPROM Commands - SPI */
#define IXGBE_EEPROM_MAX_RETRY_SPI      5000 /* Max wait 5ms for RDY signal */
#define IXGBE_EEPROM_STATUS_RDY_SPI     0x01
#define IXGBE_EEPROM_READ_OPCODE_SPI    0x03  /* EEPROM read opcode */
#define IXGBE_EEPROM_WRITE_OPCODE_SPI   0x02  /* EEPROM write opcode */
#define IXGBE_EEPROM_A8_OPCODE_SPI      0x08  /* opcode bit-3 = addr bit-8 */
#define IXGBE_EEPROM_WREN_OPCODE_SPI    0x06  /* EEPROM set Write Ena latch */
/* EEPROM reset Write Enable latch */
#define IXGBE_EEPROM_WRDI_OPCODE_SPI    0x04
#define IXGBE_EEPROM_RDSR_OPCODE_SPI    0x05  /* EEPROM read Status reg */
#define IXGBE_EEPROM_WRSR_OPCODE_SPI    0x01  /* EEPROM write Status reg */
#define IXGBE_EEPROM_ERASE4K_OPCODE_SPI 0x20  /* EEPROM ERASE 4KB */
#define IXGBE_EEPROM_ERASE64K_OPCODE_SPI	0xD8  /* EEPROM ERASE 64KB */
#define IXGBE_EEPROM_ERASE256_OPCODE_SPI	0xDB  /* EEPROM ERASE 256B */

#define IXGBE_EEPROM_GRANT_ATTEMPTS     1000 /* EEPROM attempts to gain grant */

/* Number of 5 microseconds we wait for EERD read and
 * EERW write to complete */
#define IXGBE_EERD_EEWR_ATTEMPTS	100000

/* # attempts we wait for flush update to complete */
#define IXGBE_FLUDONE_ATTEMPTS	  20000

/* EEPROM Read Register */
#define IXGBE_EEPROM_RW_REG_DATA	16 /* data offset in EEPROM read reg */
#define IXGBE_EEPROM_RW_REG_DONE	2 /* Offset to READ done bit */
#define IXGBE_EEPROM_RW_REG_START       1 /* First bit to start operation */
#define IXGBE_EEPROM_RW_ADDR_SHIFT      2 /* Shift to the address bits */
#define IXGBE_NVM_POLL_WRITE	    1 /* Flag for polling for wr complete */
#define IXGBE_NVM_POLL_READ	     0 /* Flag for polling for rd complete */
#define IXGBE_ETH_LENGTH_OF_ADDRESS     6

/* FACTPS */
#define IXGBE_FACTPS_MNGCG      0x20000000 /* Manageblility Clock Gated */
#define IXGBE_FACTPS_LFS	0x40000000 /* LAN Function Select */

/* PCI-E registers */
#define IXGBE_GCR		0x11000
#define IXGBE_GTV		0x11004
#define IXGBE_FUNCTAG		0x11008
#define IXGBE_GLT		0x1100C
#define IXGBE_PCIEPIPEADR	0x11004
#define IXGBE_PCIEPIPEDAT	0x11008
#define IXGBE_GSCL_1		0x11010
#define IXGBE_GSCL_2		0x11014
#define IXGBE_GSCL_3		0x11018
#define IXGBE_GSCL_4		0x1101C
#define IXGBE_GSCN_0		0x11020
#define IXGBE_GSCN_1		0x11024
#define IXGBE_GSCN_2		0x11028
#define IXGBE_GSCN_3		0x1102C
#define IXGBE_FACTPS		0x10150
#define IXGBE_PCIEANACTL	0x11040
#define IXGBE_SWSM		0x10140
#define IXGBE_FWSM		0x10148
#define IXGBE_GSSR		0x10160
#define IXGBE_MREVID		0x11064
#define IXGBE_DCA_ID		0x11070
#define IXGBE_DCA_CTRL		0x11074
#define IXGBE_SWFW_SYNC		IXGBE_GSSR

/* PCI-E registers 82599-Specific */
#define IXGBE_GCR_EXT	   0x11050
#define IXGBE_GSCL_5_82599      0x11030
#define IXGBE_GSCL_6_82599      0x11034
#define IXGBE_GSCL_7_82599      0x11038
#define IXGBE_GSCL_8_82599      0x1103C
#define IXGBE_PHYADR_82599      0x11040
#define IXGBE_PHYDAT_82599      0x11044
#define IXGBE_PHYCTL_82599      0x11048
#define IXGBE_PBACLR_82599      0x11068
#define IXGBE_CIAA_82599	0x11088
#define IXGBE_CIAD_82599	0x1108C
#define IXGBE_PICAUSE	   0x110B0
#define IXGBE_PIENA	     0x110B8
#define IXGBE_CDQ_MBR_82599     0x110B4
#define IXGBE_PCIESPARE	 0x110BC
#define IXGBE_MISC_REG_82599    0x110F0
#define IXGBE_ECC_CTRL_0_82599  0x11100
#define IXGBE_ECC_CTRL_1_82599  0x11104
#define IXGBE_ECC_STATUS_82599  0x110E0
#define IXGBE_BAR_CTRL_82599    0x110F4

/* PCI Express Control */
#define IXGBE_GCR_EXT_BUFFERS_CLEAR     0x40000000

/* Transmit DMA registers */
#define IXGBE_TDBAL(_i)	 (0x06000 + ((_i) * 0x40)) /* 32 of them (0-31)*/
#define IXGBE_TDBAH(_i)	 (0x06004 + ((_i) * 0x40))
#define IXGBE_TDLEN(_i)	 (0x06008 + ((_i) * 0x40))
#define IXGBE_TDH(_i)	   (0x06010 + ((_i) * 0x40))
#define IXGBE_TDT(_i)	   (0x06018 + ((_i) * 0x40))
#define IXGBE_TXDCTL(_i)	(0x06028 + ((_i) * 0x40))
#define IXGBE_TDWBAL(_i)	(0x06038 + ((_i) * 0x40))
#define IXGBE_TDWBAH(_i)	(0x0603C + ((_i) * 0x40))
#define IXGBE_DTXCTL	    0x07E00

#define IXGBE_DMATXCTL	  0x04A80
#define IXGBE_PFVFSPOOF(_i)     (0x08200 + ((_i) * 4)) /* 8 of these 0 - 7 */
#define IXGBE_PFDTXGSWC	 0x08220
#define IXGBE_DTXMXSZRQ	 0x08100
#define IXGBE_DTXTCPFLGL	0x04A88
#define IXGBE_DTXTCPFLGH	0x04A8C
#define IXGBE_LBDRPEN	   0x0CA00
#define IXGBE_TXPBTHRESH(_i)    (0x04950 + ((_i) * 4)) /* 8 of these 0 - 7 */

#define IXGBE_DMATXCTL_TE       0x1 /* Transmit Enable */
#define IXGBE_DMATXCTL_NS       0x2 /* No Snoop LSO hdr buffer */
#define IXGBE_DMATXCTL_GDV      0x8 /* Global Double VLAN */
#define IXGBE_DMATXCTL_VT_SHIFT 16  /* VLAN EtherType */

#define IXGBE_PFDTXGSWC_VT_LBEN 0x1 /* Local L2 VT switch enable */

/* Receive DMA Registers */
#define IXGBE_RDBAL(_i) (((_i) < 64) ? (0x01000 + ((_i) * 0x40)) : \
			 (0x0D000 + (((_i) - 64) * 0x40)))
#define IXGBE_RDBAH(_i) (((_i) < 64) ? (0x01004 + ((_i) * 0x40)) : \
			 (0x0D004 + (((_i) - 64) * 0x40)))
#define IXGBE_RDLEN(_i) (((_i) < 64) ? (0x01008 + ((_i) * 0x40)) : \
			 (0x0D008 + (((_i) - 64) * 0x40)))
#define IXGBE_RDH(_i)   (((_i) < 64) ? (0x01010 + ((_i) * 0x40)) : \
			 (0x0D010 + (((_i) - 64) * 0x40)))
#define IXGBE_RDT(_i)   (((_i) < 64) ? (0x01018 + ((_i) * 0x40)) : \
			 (0x0D018 + (((_i) - 64) * 0x40)))
#define IXGBE_RXDCTL(_i)	(((_i) < 64) ? (0x01028 + ((_i) * 0x40)) : \
				 (0x0D028 + (((_i) - 64) * 0x40)))
#define IXGBE_RSCCTL(_i)	(((_i) < 64) ? (0x0102C + ((_i) * 0x40)) : \
				 (0x0D02C + (((_i) - 64) * 0x40)))
#define IXGBE_RSCDBU    0x03028
#define IXGBE_RDDCC     0x02F20
#define IXGBE_RXMEMWRAP 0x03190
#define IXGBE_STARCTRL  0x03024

/* ANLP1 Bit Masks */
#define IXGBE_ANLP1_PAUSE	       0x0C00
#define IXGBE_ANLP1_SYM_PAUSE	   0x0400
#define IXGBE_ANLP1_ASM_PAUSE	   0x0800
#define IXGBE_ANLP1_AN_STATE_MASK       0x000f0000

/* Firmware Semaphore Register */
#define IXGBE_FWSM_MODE_MASK	0xE
#define IXGBE_FWSM_TS_ENABLED	0x1
#define IXGBE_FWSM_FW_MODE_PT	0x4

/* ARC Subsystem registers */
#define IXGBE_HICR	      0x15F00
#define IXGBE_FWSTS	     0x15F0C
#define IXGBE_HSMC0R	    0x15F04
#define IXGBE_HSMC1R	    0x15F08
#define IXGBE_SWSR	      0x15F10
#define IXGBE_HFDR	      0x15FE8
#define IXGBE_FLEX_MNG	  0x15800 /* 0x15800 - 0x15EFC */
#define IXGBE_HICR_EN	   0x01  /* Enable bit - RO */
/* Driver sets this bit when done to put command in RAM */
#define IXGBE_HICR_C	    0x02
#define IXGBE_HICR_SV	   0x04  /* Status Validity */
#define IXGBE_HICR_FW_RESET_ENABLE      0x40
#define IXGBE_HICR_FW_RESET     0x80

/* HLREG0 Bit Masks */
#define IXGBE_HLREG0_TXCRCEN	    0x00000001 /* bit  0 */
#define IXGBE_HLREG0_RXCRCSTRP	  0x00000002 /* bit  1 */
#define IXGBE_HLREG0_JUMBOEN	    0x00000004 /* bit  2 */
#define IXGBE_HLREG0_TXPADEN	    0x00000400 /* bit 10 */
#define IXGBE_HLREG0_TXPAUSEEN	  0x00001000 /* bit 12 */
#define IXGBE_HLREG0_RXPAUSEEN	  0x00004000 /* bit 14 */
#define IXGBE_HLREG0_LPBK	       0x00008000 /* bit 15 */
#define IXGBE_HLREG0_MDCSPD	     0x00010000 /* bit 16 */
#define IXGBE_HLREG0_CONTMDC	    0x00020000 /* bit 17 */
#define IXGBE_HLREG0_CTRLFLTR	   0x00040000 /* bit 18 */
#define IXGBE_HLREG0_PREPEND	    0x00F00000 /* bits 20-23 */
#define IXGBE_HLREG0_PRIPAUSEEN	 0x01000000 /* bit 24 */
#define IXGBE_HLREG0_RXPAUSERECDA       0x06000000 /* bits 25-26 */
#define IXGBE_HLREG0_RXLNGTHERREN       0x08000000 /* bit 27 */
#define IXGBE_HLREG0_RXPADSTRIPEN       0x10000000 /* bit 28 */

/* Omer CORECTL */
#define IXGBE_CORECTL		   0x014F00

/* CTRL Bit Masks */
#define IXGBE_CTRL_GIO_DIS      0x00000004 /* Global IO Master Disable bit */
#define IXGBE_CTRL_LNK_RST      0x00000008 /* Link Reset. Resets everything. */
#define IXGBE_CTRL_RST	  0x04000000 /* Reset (SW) */
#define IXGBE_CTRL_RST_MASK     (IXGBE_CTRL_LNK_RST | IXGBE_CTRL_RST)

/* MAC Registers */
#define IXGBE_HLREG0	    0x04240
#define IXGBE_HLREG1	    0x04244
#define IXGBE_PAP	       0x04248
#define IXGBE_MACA	      0x0424C
#define IXGBE_APAE	      0x04250
#define IXGBE_ARD	       0x04254
#define IXGBE_AIS	       0x04258
#define IXGBE_MSCA	      0x0425C
#define IXGBE_MSRWD	     0x04260
#define IXGBE_MLADD	     0x04264
#define IXGBE_MHADD	     0x04268
#define IXGBE_MAXFRS	    0x04268
#define IXGBE_TREG	      0x0426C
#define IXGBE_PCSS1	     0x04288
#define IXGBE_PCSS2	     0x0428C
#define IXGBE_XPCSS	     0x04290
#define IXGBE_MFLCN	     0x04294
#define IXGBE_SERDESC	   0x04298
#define IXGBE_MACS	      0x0429C
#define IXGBE_AUTOC	     0x042A0
#define IXGBE_LINKS	     0x042A4
#define IXGBE_LINKS2	    0x04324
#define IXGBE_AUTOC2	    0x042A8
#define IXGBE_AUTOC3	    0x042AC
#define IXGBE_ANLP1	     0x042B0
#define IXGBE_ANLP2	     0x042B4
#define IXGBE_MACC	      0x04330
#define IXGBE_ATLASCTL	  0x04800
#define IXGBE_MMNGC	     0x042D0
#define IXGBE_ANLPNP1	   0x042D4
#define IXGBE_ANLPNP2	   0x042D8
#define IXGBE_KRPCSFC	   0x042E0
#define IXGBE_KRPCSS	    0x042E4
#define IXGBE_FECS1	     0x042E8
#define IXGBE_FECS2	     0x042EC
#define IXGBE_SMADARCTL	 0x14F10
#define IXGBE_MPVC	      0x04318
#define IXGBE_SGMIIC	    0x04314

/* Checksum and EEPROM pointers */
#define IXGBE_PBANUM_PTR_GUARD	  0xFAFA
#define IXGBE_EEPROM_CHECKSUM	   0x3F
#define IXGBE_EEPROM_SUM		0xBABA
#define IXGBE_PCIE_ANALOG_PTR	   0x03
#define IXGBE_ATLAS0_CONFIG_PTR	 0x04
#define IXGBE_PHY_PTR		   0x04
#define IXGBE_ATLAS1_CONFIG_PTR	 0x05
#define IXGBE_OPTION_ROM_PTR	    0x05
#define IXGBE_PCIE_GENERAL_PTR	  0x06
#define IXGBE_PCIE_CONFIG0_PTR	  0x07
#define IXGBE_PCIE_CONFIG1_PTR	  0x08
#define IXGBE_CORE0_PTR		 0x09
#define IXGBE_CORE1_PTR		 0x0A
#define IXGBE_MAC0_PTR		  0x0B
#define IXGBE_MAC1_PTR		  0x0C
#define IXGBE_CSR0_CONFIG_PTR	   0x0D
#define IXGBE_CSR1_CONFIG_PTR	   0x0E
#define IXGBE_FW_PTR		    0x0F
#define IXGBE_PBANUM0_PTR	       0x15
#define IXGBE_PBANUM1_PTR	       0x16
#define IXGBE_ALT_MAC_ADDR_PTR	  0x37
#define IXGBE_FREE_SPACE_PTR	    0X3E

/* AUTOC Bit Masks */
#define IXGBE_AUTOC_KX4_KX_SUPP_MASK 0xC0000000
#define IXGBE_AUTOC_KX4_SUPP    0x80000000
#define IXGBE_AUTOC_KX_SUPP     0x40000000
#define IXGBE_AUTOC_PAUSE       0x30000000
#define IXGBE_AUTOC_ASM_PAUSE   0x20000000
#define IXGBE_AUTOC_SYM_PAUSE   0x10000000
#define IXGBE_AUTOC_RF	  0x08000000
#define IXGBE_AUTOC_PD_TMR      0x06000000
#define IXGBE_AUTOC_AN_RX_LOOSE 0x01000000
#define IXGBE_AUTOC_AN_RX_DRIFT 0x00800000
#define IXGBE_AUTOC_AN_RX_ALIGN 0x007C0000
#define IXGBE_AUTOC_FECA	0x00040000
#define IXGBE_AUTOC_FECR	0x00020000
#define IXGBE_AUTOC_KR_SUPP     0x00010000
#define IXGBE_AUTOC_AN_RESTART  0x00001000
#define IXGBE_AUTOC_FLU	 0x00000001
#define IXGBE_AUTOC_LMS_SHIFT   13
#define IXGBE_AUTOC_LMS_10G_SERIAL      (0x3 << IXGBE_AUTOC_LMS_SHIFT)
#define IXGBE_AUTOC_LMS_KX4_KX_KR       (0x4 << IXGBE_AUTOC_LMS_SHIFT)
#define IXGBE_AUTOC_LMS_SGMII_1G_100M   (0x5 << IXGBE_AUTOC_LMS_SHIFT)
#define IXGBE_AUTOC_LMS_KX4_KX_KR_1G_AN (0x6 << IXGBE_AUTOC_LMS_SHIFT)
#define IXGBE_AUTOC_LMS_KX4_KX_KR_SGMII (0x7 << IXGBE_AUTOC_LMS_SHIFT)
#define IXGBE_AUTOC_LMS_MASK	    (0x7 << IXGBE_AUTOC_LMS_SHIFT)
#define IXGBE_AUTOC_LMS_1G_LINK_NO_AN   (0x0 << IXGBE_AUTOC_LMS_SHIFT)
#define IXGBE_AUTOC_LMS_10G_LINK_NO_AN  (0x1 << IXGBE_AUTOC_LMS_SHIFT)
#define IXGBE_AUTOC_LMS_1G_AN	   (0x2 << IXGBE_AUTOC_LMS_SHIFT)
#define IXGBE_AUTOC_LMS_KX4_AN	  (0x4 << IXGBE_AUTOC_LMS_SHIFT)
#define IXGBE_AUTOC_LMS_KX4_AN_1G_AN    (0x6 << IXGBE_AUTOC_LMS_SHIFT)
#define IXGBE_AUTOC_LMS_ATTACH_TYPE     (0x7 << IXGBE_AUTOC_10G_PMA_PMD_SHIFT)

#define IXGBE_AUTOC_1G_PMA_PMD_MASK     0x00000200
#define IXGBE_AUTOC_1G_PMA_PMD_SHIFT    9
#define IXGBE_AUTOC_10G_PMA_PMD_MASK    0x00000180
#define IXGBE_AUTOC_10G_PMA_PMD_SHIFT   7
#define IXGBE_AUTOC_10G_XAUI    (0x0 << IXGBE_AUTOC_10G_PMA_PMD_SHIFT)
#define IXGBE_AUTOC_10G_KX4     (0x1 << IXGBE_AUTOC_10G_PMA_PMD_SHIFT)
#define IXGBE_AUTOC_10G_CX4     (0x2 << IXGBE_AUTOC_10G_PMA_PMD_SHIFT)
#define IXGBE_AUTOC_1G_BX       (0x0 << IXGBE_AUTOC_1G_PMA_PMD_SHIFT)
#define IXGBE_AUTOC_1G_KX       (0x1 << IXGBE_AUTOC_1G_PMA_PMD_SHIFT)
#define IXGBE_AUTOC_1G_SFI      (0x0 << IXGBE_AUTOC_1G_PMA_PMD_SHIFT)
#define IXGBE_AUTOC_1G_KX_BX    (0x1 << IXGBE_AUTOC_1G_PMA_PMD_SHIFT)

#define IXGBE_AUTOC2_UPPER_MASK 0xFFFF0000
#define IXGBE_AUTOC2_10G_SERIAL_PMA_PMD_MASK    0x00030000
#define IXGBE_AUTOC2_10G_SERIAL_PMA_PMD_SHIFT   16
#define IXGBE_AUTOC2_10G_KR     (0x0 << IXGBE_AUTOC2_10G_SERIAL_PMA_PMD_SHIFT)
#define IXGBE_AUTOC2_10G_XFI    (0x1 << IXGBE_AUTOC2_10G_SERIAL_PMA_PMD_SHIFT)
#define IXGBE_AUTOC2_10G_SFI    (0x2 << IXGBE_AUTOC2_10G_SERIAL_PMA_PMD_SHIFT)
#define IXGBE_AUTOC2_LINK_DISABLE_ON_D3_MASK    0x50000000
#define IXGBE_AUTOC2_LINK_DISABLE_MASK	  0x70000000

/* STATUS Bit Masks */
#define IXGBE_STATUS_LAN_ID	     0x0000000C /* LAN ID */
#define IXGBE_STATUS_LAN_ID_SHIFT       2 /* LAN ID Shift*/
#define IXGBE_STATUS_GIO		0x00080000 /* GIO Master Ena Status */

#define IXGBE_STATUS_LAN_ID_0   0x00000000 /* LAN ID 0 */
#define IXGBE_STATUS_LAN_ID_1   0x00000004 /* LAN ID 1 */

/* ESDP Bit Masks */
#define IXGBE_ESDP_SDP0	 0x00000001 /* SDP0 Data Value */
#define IXGBE_ESDP_SDP1	 0x00000002 /* SDP1 Data Value */
#define IXGBE_ESDP_SDP2	 0x00000004 /* SDP2 Data Value */
#define IXGBE_ESDP_SDP3	 0x00000008 /* SDP3 Data Value */
#define IXGBE_ESDP_SDP4	 0x00000010 /* SDP4 Data Value */
#define IXGBE_ESDP_SDP5	 0x00000020 /* SDP5 Data Value */
#define IXGBE_ESDP_SDP6	 0x00000040 /* SDP6 Data Value */
#define IXGBE_ESDP_SDP7	 0x00000080 /* SDP7 Data Value */
#define IXGBE_ESDP_SDP0_DIR     0x00000100 /* SDP0 IO direction */
#define IXGBE_ESDP_SDP1_DIR     0x00000200 /* SDP1 IO direction */
#define IXGBE_ESDP_SDP2_DIR     0x00000400 /* SDP1 IO direction */
#define IXGBE_ESDP_SDP3_DIR     0x00000800 /* SDP3 IO direction */
#define IXGBE_ESDP_SDP4_DIR     0x00001000 /* SDP4 IO direction */
#define IXGBE_ESDP_SDP5_DIR     0x00002000 /* SDP5 IO direction */
#define IXGBE_ESDP_SDP6_DIR     0x00004000 /* SDP6 IO direction */
#define IXGBE_ESDP_SDP7_DIR     0x00008000 /* SDP7 IO direction */
#define IXGBE_ESDP_SDP0_NATIVE  0x00010000 /* SDP0 IO mode */
#define IXGBE_ESDP_SDP1_NATIVE  0x00020000 /* SDP1 IO mode */

/* SW Semaphore Register bitmasks */
#define IXGBE_SWSM_SMBI	 0x00000001 /* Driver Semaphore bit */
#define IXGBE_SWSM_SWESMBI      0x00000002 /* FW Semaphore bit */
#define IXGBE_SWSM_WMNG	 0x00000004 /* Wake MNG Clock */
#define IXGBE_SWFW_REGSMP       0x80000000 /* Register Semaphore bit 31 */

/* SW_FW_SYNC/GSSR definitions */
#define IXGBE_GSSR_EEP_SM       0x0001
#define IXGBE_GSSR_PHY0_SM      0x0002
#define IXGBE_GSSR_PHY1_SM      0x0004
#define IXGBE_GSSR_MAC_CSR_SM   0x0008
#define IXGBE_GSSR_FLASH_SM     0x0010
#define IXGBE_GSSR_SW_MNG_SM    0x0400

/* EEC Register */
#define IXGBE_EEC_SK	    0x00000001 /* EEPROM Clock */
#define IXGBE_EEC_CS	    0x00000002 /* EEPROM Chip Select */
#define IXGBE_EEC_DI	    0x00000004 /* EEPROM Data In */
#define IXGBE_EEC_DO	    0x00000008 /* EEPROM Data Out */
#define IXGBE_EEC_FWE_MASK      0x00000030 /* FLASH Write Enable */
#define IXGBE_EEC_FWE_DIS       0x00000010 /* Disable FLASH writes */
#define IXGBE_EEC_FWE_EN	0x00000020 /* Enable FLASH writes */
#define IXGBE_EEC_FWE_SHIFT     4
#define IXGBE_EEC_REQ	   0x00000040 /* EEPROM Access Request */
#define IXGBE_EEC_GNT	   0x00000080 /* EEPROM Access Grant */
#define IXGBE_EEC_PRES	  0x00000100 /* EEPROM Present */
#define IXGBE_EEC_ARD	   0x00000200 /* EEPROM Auto Read Done */
#define IXGBE_EEC_FLUP	  0x00800000 /* Flash update command */
#define IXGBE_EEC_SEC1VAL       0x02000000 /* Sector 1 Valid */
#define IXGBE_EEC_FLUDONE       0x04000000 /* Flash update done */
/* EEPROM Addressing bits based on type (0-small, 1-large) */
#define IXGBE_EEC_ADDR_SIZE     0x00000400
#define IXGBE_EEC_SIZE	  0x00007800 /* EEPROM Size */
#define IXGBE_EERD_MAX_ADDR     0x00003FFF /* EERD alows 14 bits for addr. */

/* LINKS Bit Masks */
#define IXGBE_LINKS_KX_AN_COMP  0x80000000
#define IXGBE_LINKS_UP	  0x40000000
#define IXGBE_LINKS_SPEED       0x20000000
#define IXGBE_LINKS_MODE	0x18000000
#define IXGBE_LINKS_RX_MODE     0x06000000
#define IXGBE_LINKS_TX_MODE     0x01800000
#define IXGBE_LINKS_XGXS_EN     0x00400000
#define IXGBE_LINKS_SGMII_EN    0x02000000
#define IXGBE_LINKS_PCS_1G_EN   0x00200000
#define IXGBE_LINKS_1G_AN_EN    0x00100000
#define IXGBE_LINKS_KX_AN_IDLE  0x00080000
#define IXGBE_LINKS_1G_SYNC     0x00040000
#define IXGBE_LINKS_10G_ALIGN   0x00020000
#define IXGBE_LINKS_10G_LANE_SYNC       0x00017000
#define IXGBE_LINKS_TL_FAULT	    0x00001000
#define IXGBE_LINKS_SIGNAL	      0x00000F00

#define IXGBE_LINKS_SPEED_82599	 0x30000000
#define IXGBE_LINKS_SPEED_10G_82599     0x30000000
#define IXGBE_LINKS_SPEED_1G_82599      0x20000000
#define IXGBE_LINKS_SPEED_100_82599     0x10000000
#define IXGBE_LINK_UP_TIME	      90 /* 9.0 Seconds */
#define IXGBE_AUTO_NEG_TIME	     45 /* 4.5 Seconds */

#define IXGBE_LINKS2_AN_SUPPORTED       0x00000040

/* Link speed */
typedef u32 ixgbe_link_speed;
#define IXGBE_LINK_SPEED_UNKNOWN	0
#define IXGBE_LINK_SPEED_100_FULL       0x0008
#define IXGBE_LINK_SPEED_1GB_FULL       0x0020
#define IXGBE_LINK_SPEED_10GB_FULL      0x0080
#define IXGBE_LINK_SPEED_82598_AUTONEG  (IXGBE_LINK_SPEED_1GB_FULL | \
					 IXGBE_LINK_SPEED_10GB_FULL)
#define IXGBE_LINK_SPEED_82599_AUTONEG  (IXGBE_LINK_SPEED_100_FULL | \
					 IXGBE_LINK_SPEED_1GB_FULL | \
					 IXGBE_LINK_SPEED_10GB_FULL)

/* Multicast Table Array - 128 entries */
#define IXGBE_MTA(_i)	   (0x05200 + ((_i) * 4))
#define IXGBE_RAL(_i)	   (((_i) <= 15) ? (0x05400 + ((_i) * 8)) : \
				 (0x0A200 + ((_i) * 8)))
#define IXGBE_RAH(_i)	   (((_i) <= 15) ? (0x05404 + ((_i) * 8)) : \
				 (0x0A204 + ((_i) * 8)))

/* RAH */
#define IXGBE_RAH_VIND_MASK     0x003C0000
#define IXGBE_RAH_VIND_SHIFT    18
#define IXGBE_RAH_AV	    0x80000000
#define IXGBE_CLEAR_VMDQ_ALL    0xFFFFFFFF

/* Interrupt Registers */
#define IXGBE_EICR	      0x00800
#define IXGBE_EICS	      0x00808
#define IXGBE_EIMS	      0x00880
#define IXGBE_EIMC	      0x00888
#define IXGBE_EIAC	      0x00810
#define IXGBE_EIAM	      0x00890
#define IXGBE_EICS_EX(_i)       (0x00A90 + (_i) * 4)
#define IXGBE_EIMS_EX(_i)       (0x00AA0 + (_i) * 4)
#define IXGBE_EIMC_EX(_i)       (0x00AB0 + (_i) * 4)
#define IXGBE_EIAM_EX(_i)       (0x00AD0 + (_i) * 4)
/* 82599 EITR is only 12 bits, with the lower 3 always zero */
/*
 * 82598 EITR is 16 bits but set the limits based on the max
 * supported by all ixgbe hardware
 */
#define IXGBE_MAX_INT_RATE      488281
#define IXGBE_MIN_INT_RATE      956
#define IXGBE_MAX_EITR	  0x00000FF8
#define IXGBE_MIN_EITR	  8
#define IXGBE_EITR(_i)	  (((_i) <= 23) ? (0x00820 + ((_i) * 4)) : \
				 (0x012300 + (((_i) - 24) * 4)))
#define IXGBE_EITR_ITR_INT_MASK 0x00000FF8
#define IXGBE_EITR_LLI_MOD      0x00008000
#define IXGBE_EITR_CNT_WDIS     0x80000000
#define IXGBE_IVAR(_i)	  (0x00900 + ((_i) * 4)) /* 24 at 0x900-0x960 */
#define IXGBE_IVAR_MISC	 0x00A00 /* misc MSI-X interrupt causes */
#define IXGBE_EITRSEL	   0x00894
#define IXGBE_MSIXT	     0x00000 /* MSI-X Table. 0x0000 - 0x01C */
#define IXGBE_MSIXPBA	   0x02000 /* MSI-X Pending bit array */
#define IXGBE_PBACL(_i) (((_i) == 0) ? (0x11068) : (0x110C0 + ((_i) * 4)))
#define IXGBE_GPIE	      0x00898

/* Extended Interrupt Cause Read */
#define IXGBE_EICR_RTX_QUEUE	0x0000FFFF /* RTx Queue Interrupt */

/* Extended Interrupt Mask Set */
#define IXGBE_EIMS_RTX_QUEUE	IXGBE_EICR_RTX_QUEUE /* RTx Queue Interrupt */

/* Interrupt clear mask */
#define IXGBE_IRQ_CLEAR_MASK    0xFFFFFFFF

/* Extended Device Control */
#define IXGBE_CTRL_EXT_PFRSTD   0x00004000 /* Physical Function Reset Done */
#define IXGBE_CTRL_EXT_NS_DIS   0x00010000 /* No Snoop disable */
#define IXGBE_CTRL_EXT_RO_DIS   0x00020000 /* Relaxed Ordering disable */
#define IXGBE_CTRL_EXT_DRV_LOAD 0x10000000 /* Driver loaded bit for FW */

/* Device Type definitions for new protocol MDIO commands */
#define IXGBE_MDIO_PMA_PMD_DEV_TYPE	     0x1
#define IXGBE_MDIO_PCS_DEV_TYPE		 0x3
#define IXGBE_MDIO_PHY_XS_DEV_TYPE	      0x4
#define IXGBE_MDIO_AUTO_NEG_DEV_TYPE	    0x7
#define IXGBE_MDIO_VENDOR_SPECIFIC_1_DEV_TYPE   0x1E   /* Device 30 */
#define IXGBE_TWINAX_DEV			1

#define IXGBE_MDIO_COMMAND_TIMEOUT      100 /* PHY Timeout for 1 GB mode */

#define IXGBE_MDIO_VENDOR_SPECIFIC_1_CONTROL	    0x0 /* VS1 Ctrl Reg */
#define IXGBE_MDIO_VENDOR_SPECIFIC_1_STATUS	     0x1 /* VS1 Status Reg */
#define IXGBE_MDIO_VENDOR_SPECIFIC_1_LINK_STATUS	0x0008 /* 1 = Link Up */
#define IXGBE_MDIO_VENDOR_SPECIFIC_1_SPEED_STATUS       0x0010 /* 0-10G, 1-1G */
#define IXGBE_MDIO_VENDOR_SPECIFIC_1_10G_SPEED	  0x0018
#define IXGBE_MDIO_VENDOR_SPECIFIC_1_1G_SPEED	   0x0010

#define IXGBE_MDIO_AUTO_NEG_CONTROL     0x0 /* AUTO_NEG Control Reg */
#define IXGBE_MDIO_AUTO_NEG_STATUS      0x1 /* AUTO_NEG Status Reg */
#define IXGBE_MDIO_AUTO_NEG_ADVT	0x10 /* AUTO_NEG Advt Reg */
#define IXGBE_MDIO_AUTO_NEG_LP	  0x13 /* AUTO_NEG LP Status Reg */
#define IXGBE_MDIO_AUTO_NEG_EEE_ADVT    0x3C /* AUTO_NEG EEE Advt Reg */
#define IXGBE_AUTO_NEG_10GBASE_EEE_ADVT 0x8  /* AUTO NEG EEE 10GBaseT Advt */
#define IXGBE_AUTO_NEG_1000BASE_EEE_ADVT 0x4  /* AUTO NEG EEE 1000BaseT Advt */
#define IXGBE_AUTO_NEG_100BASE_EEE_ADVT 0x2  /* AUTO NEG EEE 100BaseT Advt */
#define IXGBE_MDIO_PHY_XS_CONTROL       0x0 /* PHY_XS Control Reg */
#define IXGBE_MDIO_PHY_XS_RESET	 0x8000 /* PHY_XS Reset */
#define IXGBE_MDIO_PHY_ID_HIGH	  0x2 /* PHY ID High Reg*/
#define IXGBE_MDIO_PHY_ID_LOW	   0x3 /* PHY ID Low Reg*/
#define IXGBE_MDIO_PHY_SPEED_ABILITY    0x4 /* Speed Ability Reg */
#define IXGBE_MDIO_PHY_SPEED_10G	0x0001 /* 10G capable */
#define IXGBE_MDIO_PHY_SPEED_1G	 0x0010 /* 1G capable */
#define IXGBE_MDIO_PHY_SPEED_100M       0x0020 /* 100M capable */
#define IXGBE_MDIO_PHY_EXT_ABILITY      0xB /* Ext Ability Reg */
#define IXGBE_MDIO_PHY_10GBASET_ABILITY	 0x0004 /* 10GBaseT capable */
#define IXGBE_MDIO_PHY_1000BASET_ABILITY	0x0020 /* 1000BaseT capable */
#define IXGBE_MDIO_PHY_100BASETX_ABILITY	0x0080 /* 100BaseTX capable */
#define IXGBE_MDIO_PHY_SET_LOW_POWER_MODE       0x0800 /* Set low power mode */

#define IXGBE_MDIO_PMA_PMD_CONTROL_ADDR 0x0000 /* PMA/PMD Control Reg */
#define IXGBE_MDIO_PMA_PMD_SDA_SCL_ADDR 0xC30A /* PHY_XS SDA/SCL Addr Reg */
#define IXGBE_MDIO_PMA_PMD_SDA_SCL_DATA 0xC30B /* PHY_XS SDA/SCL Data Reg */
#define IXGBE_MDIO_PMA_PMD_SDA_SCL_STAT 0xC30C /* PHY_XS SDA/SCL Status Reg */

#define IXGBE_PCRC8ECL	  0x0E810 /* PCR CRC-8 Error Count Lo */
#define IXGBE_PCRC8ECH	  0x0E811 /* PCR CRC-8 Error Count Hi */
#define IXGBE_PCRC8ECH_MASK     0x1F
#define IXGBE_LDPCECL	   0x0E820 /* PCR Uncorrected Error Count Lo */
#define IXGBE_LDPCECH	   0x0E821 /* PCR Uncorrected Error Count Hi */

/* MII clause 22/28 definitions */
#define IXGBE_MDIO_PHY_LOW_POWER_MODE   0x0800

#define IXGBE_MII_10GBASE_T_AUTONEG_CTRL_REG    0x20   /* 10G Control Reg */
#define IXGBE_MII_AUTONEG_VENDOR_PROVISION_1_REG 0xC400 /* 1G Provisioning 1 */
#define IXGBE_MII_AUTONEG_XNP_TX_REG	    0x17   /* 1G XNP Transmit */
#define IXGBE_MII_AUTONEG_ADVERTISE_REG	 0x10   /* 100M Advertisement */
#define IXGBE_MII_10GBASE_T_ADVERTISE	   0x1000 /* full duplex, bit:12*/
#define IXGBE_MII_1GBASE_T_ADVERTISE_XNP_TX     0x4000 /* full duplex, bit:14*/
#define IXGBE_MII_1GBASE_T_ADVERTISE	    0x8000 /* full duplex, bit:15*/
#define IXGBE_MII_100BASE_T_ADVERTISE	   0x0100 /* full duplex, bit:8 */
#define IXGBE_MII_100BASE_T_ADVERTISE_HALF      0x0080 /* half duplex, bit:7 */
#define IXGBE_MII_RESTART		       0x200
#define IXGBE_MII_AUTONEG_COMPLETE	      0x20
#define IXGBE_MII_AUTONEG_LINK_UP	       0x04
#define IXGBE_MII_AUTONEG_REG		   0x0

#define IXGBE_PHY_REVISION_MASK	 0xFFFFFFF0
#define IXGBE_MAX_PHY_ADDR	      32

/* Transmit Config masks */
#define IXGBE_TXDCTL_ENABLE	     0x02000000 /* Ena specific Tx Queue */
#define IXGBE_TXDCTL_SWFLSH	     0x04000000 /* Tx Desc. wr-bk flushing */

/* Receive Config masks */
#define IXGBE_RXDCTL_ENABLE	     0x02000000 /* Ena specific Rx Queue */
#define IXGBE_RXDCTL_SWFLSH	     0x04000000 /* Rx Desc wr-bk flushing */

/* Manageablility Host Interface defines */
#define IXGBE_HI_MAX_BLOCK_BYTE_LENGTH  1792 /* Num of bytes in range */
#define IXGBE_HI_MAX_BLOCK_DWORD_LENGTH 448 /* Num of dwords in range */
#define IXGBE_HI_COMMAND_TIMEOUT	500 /* Process HI command limit */

/* CEM Support */
#define FW_CEM_HDR_LEN		  0x4
#define FW_CEM_CMD_DRIVER_INFO	  0xDD
#define FW_CEM_CMD_DRIVER_INFO_LEN      0x5
#define FW_CEM_CMD_RESERVED	     0X0
#define FW_CEM_UNUSED_VER	       0x0
#define FW_CEM_MAX_RETRIES	      3
#define FW_CEM_RESP_STATUS_SUCCESS      0x1

/* MSI-X capability fields masks */
#define IXGBE_PCIE_MSIX_TBL_SZ_MASK     0x7FF

/* PCI Bus Info */
#define IXGBE_PCI_DEVICE_STATUS	 0xAA
#define IXGBE_PCI_DEVICE_STATUS_TRANSACTION_PENDING     0x0020
#define IXGBE_PCI_LINK_STATUS	   0xB2
#define IXGBE_PCI_DEVICE_CONTROL2       0xC8
#define IXGBE_PCI_LINK_WIDTH	    0x3F0
#define IXGBE_PCI_LINK_WIDTH_1	  0x10
#define IXGBE_PCI_LINK_WIDTH_2	  0x20
#define IXGBE_PCI_LINK_WIDTH_4	  0x40
#define IXGBE_PCI_LINK_WIDTH_8	  0x80
#define IXGBE_PCI_LINK_SPEED	    0xF
#define IXGBE_PCI_LINK_SPEED_2500       0x1
#define IXGBE_PCI_LINK_SPEED_5000       0x2
#define IXGBE_PCI_LINK_SPEED_8000       0x3
#define IXGBE_PCI_HEADER_TYPE_REGISTER  0x0E
#define IXGBE_PCI_HEADER_TYPE_MULTIFUNC 0x80
#define IXGBE_PCI_DEVICE_CONTROL2_16ms  0x0005

#define IXGBE_PCIDEVCTRL2_TIMEO_MASK    0xf
#define IXGBE_PCIDEVCTRL2_16_32ms_def   0x0
#define IXGBE_PCIDEVCTRL2_50_100us      0x1
#define IXGBE_PCIDEVCTRL2_1_2ms	 0x2
#define IXGBE_PCIDEVCTRL2_16_32ms       0x5
#define IXGBE_PCIDEVCTRL2_65_130ms      0x6
#define IXGBE_PCIDEVCTRL2_260_520ms     0x9
#define IXGBE_PCIDEVCTRL2_1_2s	  0xa
#define IXGBE_PCIDEVCTRL2_4_8s	  0xd
#define IXGBE_PCIDEVCTRL2_17_34s	0xe

/* Number of 100 microseconds we wait for PCI Express master disable */
#define IXGBE_PCI_MASTER_DISABLE_TIMEOUT	800

/* Stats registers */
#define IXGBE_CRCERRS   0x04000
#define IXGBE_ILLERRC   0x04004
#define IXGBE_ERRBC     0x04008
#define IXGBE_MSPDC     0x04010
#define IXGBE_MPC(_i)   (0x03FA0 + ((_i) * 4)) /* 8 of these 3FA0-3FBC*/
#define IXGBE_MLFC      0x04034
#define IXGBE_MRFC      0x04038
#define IXGBE_RLEC      0x04040
#define IXGBE_LXONTXC   0x03F60
#define IXGBE_LXONRXC   0x0CF60
#define IXGBE_LXOFFTXC  0x03F68
#define IXGBE_LXOFFRXC  0x0CF68
#define IXGBE_LXONRXCNT	 0x041A4
#define IXGBE_LXOFFRXCNT	0x041A8
#define IXGBE_PXONRXCNT(_i)     (0x04140 + ((_i) * 4)) /* 8 of these */
#define IXGBE_PXOFFRXCNT(_i)    (0x04160 + ((_i) * 4)) /* 8 of these */
#define IXGBE_PXON2OFFCNT(_i)   (0x03240 + ((_i) * 4)) /* 8 of these */
#define IXGBE_PXONTXC(_i)       (0x03F00 + ((_i) * 4)) /* 8 of these 3F00-3F1C*/
#define IXGBE_PXONRXC(_i)       (0x0CF00 + ((_i) * 4)) /* 8 of these CF00-CF1C*/
#define IXGBE_PXOFFTXC(_i)      (0x03F20 + ((_i) * 4)) /* 8 of these 3F20-3F3C*/
#define IXGBE_PXOFFRXC(_i)      (0x0CF20 + ((_i) * 4)) /* 8 of these CF20-CF3C*/
#define IXGBE_PRC64	     0x0405C
#define IXGBE_PRC127	    0x04060
#define IXGBE_PRC255	    0x04064
#define IXGBE_PRC511	    0x04068
#define IXGBE_PRC1023	   0x0406C
#define IXGBE_PRC1522	   0x04070
#define IXGBE_GPRC	      0x04074
#define IXGBE_BPRC	      0x04078
#define IXGBE_MPRC	      0x0407C
#define IXGBE_GPTC	      0x04080
#define IXGBE_GORCL	     0x04088
#define IXGBE_GORCH	     0x0408C
#define IXGBE_GOTCL	     0x04090
#define IXGBE_GOTCH	     0x04094
#define IXGBE_RNBC(_i)	  (0x03FC0 + ((_i) * 4)) /* 8 of these 3FC0-3FDC*/
#define IXGBE_RUC	       0x040A4
#define IXGBE_RFC	       0x040A8
#define IXGBE_ROC	       0x040AC
#define IXGBE_RJC	       0x040B0
#define IXGBE_MNGPRC	    0x040B4
#define IXGBE_MNGPDC	    0x040B8
#define IXGBE_MNGPTC	    0x0CF90
#define IXGBE_TORL	      0x040C0
#define IXGBE_TORH	      0x040C4
#define IXGBE_TPR	       0x040D0
#define IXGBE_TPT	       0x040D4
#define IXGBE_PTC64	     0x040D8
#define IXGBE_PTC127	    0x040DC
#define IXGBE_PTC255	    0x040E0
#define IXGBE_PTC511	    0x040E4
#define IXGBE_PTC1023	   0x040E8
#define IXGBE_PTC1522	   0x040EC
#define IXGBE_MPTC	      0x040F0
#define IXGBE_BPTC	      0x040F4
#define IXGBE_XEC	       0x04120
#define IXGBE_SSVPC	     0x08780
#define IXGBE_RQSMR(_i) (0x02300 + ((_i) * 4))
#define IXGBE_TQSMR(_i) (((_i) <= 7) ? (0x07300 + ((_i) * 4)) : \
			 (0x08600 + ((_i) * 4)))
#define IXGBE_TQSM(_i)  (0x08600 + ((_i) * 4))
#define IXGBE_QPRC(_i)  (0x01030 + ((_i) * 0x40)) /* 16 of these */
#define IXGBE_QPTC(_i)  (0x06030 + ((_i) * 0x40)) /* 16 of these */
#define IXGBE_QBRC(_i)  (0x01034 + ((_i) * 0x40)) /* 16 of these */
#define IXGBE_QBTC(_i)  (0x06034 + ((_i) * 0x40)) /* 16 of these */
#define IXGBE_QBRC_L(_i)	(0x01034 + ((_i) * 0x40)) /* 16 of these */
#define IXGBE_QBRC_H(_i)	(0x01038 + ((_i) * 0x40)) /* 16 of these */
#define IXGBE_QPRDC(_i)	 (0x01430 + ((_i) * 0x40)) /* 16 of these */
#define IXGBE_QBTC_L(_i)	(0x08700 + ((_i) * 0x8)) /* 16 of these */
#define IXGBE_QBTC_H(_i)	(0x08704 + ((_i) * 0x8)) /* 16 of these */
#define IXGBE_FCCRC	     0x05118 /* Num of Good Eth CRC w/ Bad FC CRC */
#define IXGBE_FCOERPDC	  0x0241C /* FCoE Rx Packets Dropped Count */
#define IXGBE_FCLAST	    0x02424 /* FCoE Last Error Count */
#define IXGBE_FCOEPRC	   0x02428 /* Number of FCoE Packets Received */
#define IXGBE_FCOEDWRC	  0x0242C /* Number of FCoE DWords Received */
#define IXGBE_FCOEPTC	   0x08784 /* Number of FCoE Packets Transmitted */
#define IXGBE_FCOEDWTC	  0x08788 /* Number of FCoE DWords Transmitted */
#define IXGBE_FCCRC_CNT_MASK    0x0000FFFF /* CRC_CNT: bit 0 - 15 */
#define IXGBE_FCLAST_CNT_MASK   0x0000FFFF /* Last_CNT: bit 0 - 15 */
#define IXGBE_O2BGPTC	   0x041C4
#define IXGBE_O2BSPC	    0x087B0
#define IXGBE_B2OSPC	    0x041C0
#define IXGBE_B2OGPRC	   0x02F90
#define IXGBE_BUPRC	     0x04180
#define IXGBE_BMPRC	     0x04184
#define IXGBE_BBPRC	     0x04188
#define IXGBE_BUPTC	     0x0418C
#define IXGBE_BMPTC	     0x04190
#define IXGBE_BBPTC	     0x04194
#define IXGBE_BCRCERRS	  0x04198
#define IXGBE_BXONRXC	   0x0419C
#define IXGBE_BXOFFRXC	  0x041E0
#define IXGBE_BXONTXC	   0x041E4
#define IXGBE_BXOFFTXC	  0x041E8

/* To disable unused features */
#define IXGBE_RXCTRL	    0x03000
#define IXGBE_RTTDQSEL	  0x04904
#define IXGBE_RTTBCNRC		  0x04984
#define IXGBE_DCA_TXCTRL_82599(_i)      (0x0600C + ((_i) * 0x40))
#define IXGBE_DCA_TXCTRL_DESC_WRO_EN    (1 << 11) /* Tx Desc writeback RO bit */
#define IXGBE_DCA_RXCTRL(_i)    (((_i) <= 15) ? (0x02200 + ((_i) * 4)) : \
				 (((_i) < 64) ? (0x0100C + ((_i) * 0x40)) : \
				 (0x0D00C + (((_i) - 64) * 0x40))))
#define IXGBE_DCA_RXCTRL_DATA_WRO_EN    (1 << 13) /* Rx wr data Relax Order */
#define IXGBE_DCA_RXCTRL_HEAD_WRO_EN    (1 << 15) /* Rx wr header RO */
#define IXGBE_DCB_MAX_TRAFFIC_CLASS     8
#define IXGBE_MCSTCTRL	  0x05090
#define IXGBE_VFTA(_i)	  (0x0A000 + ((_i) * 4))
#define IXGBE_VLVF(_i)  (0x0F100 + ((_i) * 4))  /* 64 of these (0-63) */
#define IXGBE_VLVFB(_i) (0x0F200 + ((_i) * 4))  /* 128 of these (0-127) */
#define IXGBE_VLVF_ENTRIES	      64
#define IXGBE_FCCFG             0x03D00
#define IXGBE_MFLCN_DPF         0x00000002 /* Discard Pause Frame */
#define IXGBE_MFLCN_RFCE        0x00000008 /* Receive FC Enable */
#define IXGBE_MFLCN_RPFCE_MASK  0x00000FF4 /* Rx Priority FC bitmap mask */
#define IXGBE_FCCFG_TFCE_802_3X         0x00000008 /* Tx link FC enable */
#define IXGBE_FCCFG_TFCE_PRIORITY       0x00000010 /* Tx priority FC enable */

/* Error Codes */
#define IXGBE_ERR_EEPROM			-1
#define IXGBE_ERR_EEPROM_CHECKSUM	       -2
#define IXGBE_ERR_PHY			   -3
#define IXGBE_ERR_CONFIG			-4
#define IXGBE_ERR_PARAM			 -5
#define IXGBE_ERR_MAC_TYPE		      -6
#define IXGBE_ERR_UNKNOWN_PHY		   -7
#define IXGBE_ERR_LINK_SETUP		    -8
#define IXGBE_ERR_ADAPTER_STOPPED	       -9
#define IXGBE_ERR_INVALID_MAC_ADDR	      -10
#define IXGBE_ERR_DEVICE_NOT_SUPPORTED	  -11
#define IXGBE_ERR_MASTER_REQUESTS_PENDING       -12
#define IXGBE_ERR_INVALID_LINK_SETTINGS	 -13
#define IXGBE_ERR_AUTONEG_NOT_COMPLETE	  -14
#define IXGBE_ERR_RESET_FAILED		  -15
#define IXGBE_ERR_SWFW_SYNC		     -16
#define IXGBE_ERR_PHY_ADDR_INVALID	      -17
#define IXGBE_ERR_I2C			   -18
#define IXGBE_ERR_SFP_NOT_SUPPORTED	     -19
#define IXGBE_ERR_SFP_NOT_PRESENT	       -20
#define IXGBE_ERR_SFP_NO_INIT_SEQ_PRESENT       -21
#define IXGBE_ERR_NO_SAN_ADDR_PTR	       -22
#define IXGBE_ERR_FDIR_REINIT_FAILED	    -23
#define IXGBE_ERR_EEPROM_VERSION		-24
#define IXGBE_ERR_NO_SPACE		      -25
#define IXGBE_ERR_OVERTEMP		      -26
#define IXGBE_ERR_FC_NOT_NEGOTIATED	     -27
#define IXGBE_ERR_FC_NOT_SUPPORTED	      -28
#define IXGBE_ERR_SFP_SETUP_NOT_COMPLETE	-30
#define IXGBE_ERR_PBA_SECTION		   -31
#define IXGBE_ERR_INVALID_ARGUMENT	      -32
#define IXGBE_ERR_HOST_INTERFACE_COMMAND	-33
#define IXGBE_ERR_OUT_OF_MEM		    -34
#define IXGBE_ERR_FEATURE_NOT_SUPPORTED	 -36
#define IXGBE_ERR_EEPROM_PROTECTED_REGION       -37

/* Misc */
#define IXGBE_EEC_SIZE_SHIFT	    11
#define IXGBE_EEPROM_WORD_SIZE_SHIFT    6
#define IXGBE_EEPROM_OPCODE_BITS	8
#define IXGBE_FW_LESM_PARAMETERS_PTR	    0x2
#define IXGBE_FW_LESM_STATE_1		   0x1
#define IXGBE_FW_LESM_STATE_ENABLED	     0x8000 /* LESM Enable bit */
#define IXGBE_FW_PASSTHROUGH_PATCH_CONFIG_PTR   0x4
#define IXGBE_FW_PATCH_VERSION_4		0x7
#define IXGBE_PCIE_MSIX_82599_CAPS      0x72
#define IXGBE_MAX_MSIX_VECTORS_82599    0x40
#define IXGBE_UTA(_i)	   (0x0F400 + ((_i) * 4))
#define IXGBE_FAILED_READ_CFG_WORD 0xffffU

/* Check whether address is multicast. This is little-endian specific check.*/
#define IXGBE_IS_MULTICAST(Address) \
		(bool)(((u8 *)(Address))[0] & ((u8)0x01))

/* Check whether an address is broadcast. */
#define IXGBE_IS_BROADCAST(Address) \
		((((u8 *)(Address))[0] == ((u8)0xff)) && \
		(((u8 *)(Address))[1] == ((u8)0xff)))

enum ixgbe_phy_type {
	ixgbe_phy_unknown = 0,
	ixgbe_phy_none,
	ixgbe_phy_generic
};

enum ixgbe_mac_type {
	ixgbe_mac_unknown = 0,
	ixgbe_mac_82598EB,
	ixgbe_mac_82599EB,
	ixgbe_mac_X540,
	ixgbe_num_macs
};

enum ixgbe_sfp_type {
	ixgbe_sfp_type_srlr_core0 = 5,
	ixgbe_sfp_type_srlr_core1 = 6,
	ixgbe_sfp_type_not_present = 0xFFFE,
	ixgbe_sfp_type_unknown = 0xFFFF
};

enum ixgbe_media_type {
	ixgbe_media_type_unknown = 0,
	ixgbe_media_type_fiber,
};

enum ixgbe_eeprom_type {
	ixgbe_eeprom_uninitialized = 0,
	ixgbe_eeprom_spi,
	ixgbe_flash,
	ixgbe_eeprom_none /* No NVM support */
};

/* Function pointer table */
struct ixgbe_mac_operations {
	s32 (*init_hw)(struct ixgbe_hw *);
	s32 (*reset_hw)(struct ixgbe_hw *);
	s32 (*start_hw)(struct ixgbe_hw *);
	s32 (*clear_hw_cntrs)(struct ixgbe_hw *);
	enum ixgbe_media_type (*get_media_type)(struct ixgbe_hw *);
	s32 (*get_mac_addr)(struct ixgbe_hw *, u8 *);
	s32 (*stop_adapter)(struct ixgbe_hw *);
	void (*set_lan_id)(struct ixgbe_hw *);
	s32 (*setup_sfp)(struct ixgbe_hw *);
	s32 (*acquire_swfw_sync)(struct ixgbe_hw *, u16);
	void (*release_swfw_sync)(struct ixgbe_hw *, u16);
	s32 (*prot_autoc_read)(struct ixgbe_hw *, bool *, u32 *);
	s32 (*prot_autoc_write)(struct ixgbe_hw *, u32, bool);

	/* Link */
	void (*disable_tx_laser)(struct ixgbe_hw *);
	void (*enable_tx_laser)(struct ixgbe_hw *);
	void (*flap_tx_laser)(struct ixgbe_hw *);
	s32 (*setup_link)(struct ixgbe_hw *, u32, bool);
	s32 (*check_link)(struct ixgbe_hw *, u32 *, bool *, bool);

	/* RAR, Multicast, VLAN */
	s32 (*set_rar)(struct ixgbe_hw *, u32, u8 *, u32, u32);
	s32 (*init_rx_addrs)(struct ixgbe_hw *);
	s32 (*clear_vfta)(struct ixgbe_hw *);

	/* Manageability interface */
	s32 (*set_fw_drv_ver)(struct ixgbe_hw *, u8, u8, u8, u8);
};

struct ixgbe_phy_operations {
	s32 (*identify)(struct ixgbe_hw *);
	s32 (*init)(struct ixgbe_hw *);
};

struct ixgbe_eeprom_operations {
	s32 (*init_params)(struct ixgbe_hw *);
	s32 (*read)(struct ixgbe_hw *, u16, u16 *);
	s32 (*validate_checksum)(struct ixgbe_hw *, u16 *);
	u16 (*calc_checksum)(struct ixgbe_hw *);
};

#define IXGBE_FLAGS_DOUBLE_RESET_REQUIRED       0x01
struct ixgbe_mac_info {
	struct ixgbe_mac_operations ops;
	enum ixgbe_mac_type type;
	u8 addr[IXGBE_ETH_LENGTH_OF_ADDRESS];
	u8 perm_addr[IXGBE_ETH_LENGTH_OF_ADDRESS];
	u8 san_addr[IXGBE_ETH_LENGTH_OF_ADDRESS];
	/* prefix for World Wide Node Name (WWNN) */
	u16 wwnn_prefix;
	/* prefix for World Wide Port Name (WWPN) */
	u16 wwpn_prefix;
#define IXGBE_MAX_MTA		   128
	u32 mta_shadow[IXGBE_MAX_MTA];
	s32 mc_filter_type;
	u32 mcft_size;
	u32 vft_size;
	u32 num_rar_entries;
	u32 rar_highwater;
	u32 rx_pb_size;
	u32 max_rx_queues;
	u32 max_tx_queues;
	u32 orig_autoc;
	u8  san_mac_rar_index;
	bool get_link_status;
	u32 orig_autoc2;
	u16 max_msix_vectors;
	bool arc_subsystem_valid;
	bool orig_link_settings_stored;
	bool autotry_restart;
	u8 flags;
	bool set_lben;
};

struct ixgbe_phy_info {
	struct ixgbe_phy_operations ops;
	enum ixgbe_phy_type type;
	enum ixgbe_sfp_type sfp_type;
	enum ixgbe_media_type media_type;
	bool sfp_setup_needed;
};

struct ixgbe_eeprom_info {
	struct ixgbe_eeprom_operations ops;
	enum ixgbe_eeprom_type type;
	u32 semaphore_delay;
	u16 word_size;
	u16 address_bits;
	u16 word_page_size;
};

/* PCI bus types */
enum ixgbe_bus_type {
	ixgbe_bus_type_unknown = 0,
	ixgbe_bus_type_pci,
	ixgbe_bus_type_pcix,
	ixgbe_bus_type_pci_express,
	ixgbe_bus_type_reserved
};

/* PCI bus speeds */
enum ixgbe_bus_speed {
	ixgbe_bus_speed_unknown = 0,
	ixgbe_bus_speed_33      = 33,
	ixgbe_bus_speed_66      = 66,
	ixgbe_bus_speed_100     = 100,
	ixgbe_bus_speed_120     = 120,
	ixgbe_bus_speed_133     = 133,
	ixgbe_bus_speed_2500    = 2500,
	ixgbe_bus_speed_5000    = 5000,
	ixgbe_bus_speed_8000    = 8000,
	ixgbe_bus_speed_reserved
};

/* PCI bus widths */
enum ixgbe_bus_width {
	ixgbe_bus_width_unknown = 0,
	ixgbe_bus_width_pcie_x1 = 1,
	ixgbe_bus_width_pcie_x2 = 2,
	ixgbe_bus_width_pcie_x4 = 4,
	ixgbe_bus_width_pcie_x8 = 8,
	ixgbe_bus_width_32      = 32,
	ixgbe_bus_width_64      = 64,
	ixgbe_bus_width_reserved
};

/* Bus parameters */
struct ixgbe_bus_info {
	enum ixgbe_bus_speed speed;
	enum ixgbe_bus_width width;
	enum ixgbe_bus_type type;

	u16 func;
	u16 lan_id;
};

struct ixgbe_hw {
	u8 __iomem *hw_addr;
	void *back;
	struct ixgbe_mac_info mac;
	struct ixgbe_phy_info phy;
	struct ixgbe_eeprom_info eeprom;
	struct ixgbe_bus_info bus;
	u16 device_id;
	u16 vendor_id;
	u16 subsystem_device_id;
	u16 subsystem_vendor_id;
	u8 revision_id;
	bool adapter_stopped;
	int api_version;
	bool force_full_reset;
	bool allow_unsupported_sfp;
	bool wol_enabled;
};

/* Host Interface Command Structures */

struct ixgbe_hic_hdr {
	u8 cmd;
	u8 buf_len;
	union {
		u8 cmd_resv;
		u8 ret_status;
	} cmd_or_resp;
	u8 checksum;
};

struct ixgbe_hic_drv_info {
	struct ixgbe_hic_hdr hdr;
	u8 port_num;
	u8 ver_sub;
	u8 ver_build;
	u8 ver_min;
	u8 ver_maj;
	u8 pad; /* end spacing to ensure length is mult. of dword */
	u16 pad2; /* end spacing to ensure length is mult. of dword2 */
};

#endif /* _IXGBE_TYPE_H_ */
