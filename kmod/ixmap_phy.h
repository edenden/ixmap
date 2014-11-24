/* EEPROM byte offsets */
#define IXGBE_SFF_IDENTIFIER	    0x0
#define IXGBE_SFF_IDENTIFIER_SFP	0x3
#define IXGBE_SFF_10GBE_COMP_CODES      0x3
#define IXGBE_SFF_CABLE_TECHNOLOGY      0x8
#define IXGBE_SFF_CABLE_SPEC_COMP       0x3C

/* Bitmasks */
#define IXGBE_SFF_DA_PASSIVE_CABLE      0x4
#define IXGBE_SFF_DA_ACTIVE_CABLE       0x8
#define IXGBE_SFF_DA_SPEC_ACTIVE_LIMITING       0x4
#define IXGBE_SFF_10GBASESR_CAPABLE     0x10
#define IXGBE_SFF_10GBASELR_CAPABLE     0x20

/* Special PHY Init Routine */
#define IXGBE_PHY_INIT_OFFSET_NL	0x002B
#define IXGBE_PHY_INIT_END_NL	   0xFFFF

/* PHY IDs*/
#define TN1010_PHY_ID   0x00A19410
#define X540_PHY_ID     0x01540200
#define QT2022_PHY_ID   0x0043A400
#define ATH_PHY_ID      0x03429050

/* Flow control defines */
#define IXGBE_TAF_SYM_PAUSE	     0x400
#define IXGBE_TAF_ASM_PAUSE	     0x800

/* MSCA Bit Masks */
#define IXGBE_MSCA_NP_ADDR_MASK	 0x0000FFFF /* MDI Addr (new prot) */
#define IXGBE_MSCA_NP_ADDR_SHIFT	0
#define IXGBE_MSCA_DEV_TYPE_MASK	0x001F0000 /* Dev Type (new prot) */
#define IXGBE_MSCA_DEV_TYPE_SHIFT       16 /* Register Address (old prot */
#define IXGBE_MSCA_PHY_ADDR_MASK	0x03E00000 /* PHY Address mask */
#define IXGBE_MSCA_PHY_ADDR_SHIFT       21 /* PHY Address shift*/
#define IXGBE_MSCA_OP_CODE_MASK	 0x0C000000 /* OP CODE mask */
#define IXGBE_MSCA_OP_CODE_SHIFT	26 /* OP CODE shift */
#define IXGBE_MSCA_ADDR_CYCLE	   0x00000000 /* OP CODE 00 (addr cycle) */
#define IXGBE_MSCA_WRITE		0x04000000 /* OP CODE 01 (wr) */
#define IXGBE_MSCA_READ		 0x0C000000 /* OP CODE 11 (rd) */
#define IXGBE_MSCA_READ_AUTOINC	 0x08000000 /* OP CODE 10 (rd auto inc)*/
#define IXGBE_MSCA_ST_CODE_MASK	 0x30000000 /* ST Code mask */
#define IXGBE_MSCA_ST_CODE_SHIFT	28 /* ST Code shift */
#define IXGBE_MSCA_NEW_PROTOCOL	 0x00000000 /* ST CODE 00 (new prot) */
#define IXGBE_MSCA_OLD_PROTOCOL	 0x10000000 /* ST CODE 01 (old prot) */
#define IXGBE_MSCA_MDI_COMMAND	  0x40000000 /* Initiate MDI command */
#define IXGBE_MSCA_MDI_IN_PROG_EN       0x80000000 /* MDI in progress ena */

/* MSRWD bit masks */
#define IXGBE_MSRWD_WRITE_DATA_MASK     0x0000FFFF
#define IXGBE_MSRWD_WRITE_DATA_SHIFT    0
#define IXGBE_MSRWD_READ_DATA_MASK      0xFFFF0000
#define IXGBE_MSRWD_READ_DATA_SHIFT     16

int32_t ixmap_get_sfp_init_sequence_offsets(struct ixmap_hw *hw,
	uint16_t *list_offset, uint16_t *data_offset);
int32_t ixmap_identify_module(struct ixmap_hw *hw);
