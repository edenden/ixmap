s32 ixgbe_init_hw_generic(struct ixgbe_hw *hw);
void ixgbe_start_hw_generic(struct ixgbe_hw *hw);
s32 ixgbe_get_mac_addr_generic(struct ixgbe_hw *hw, u8 *mac_addr);
s32 ixgbe_stop_adapter_generic(struct ixgbe_hw *hw);
s32 ixgbe_init_rx_addrs_generic(struct ixgbe_hw *hw);
s32 ixgbe_validate_mac_addr(u8 *mac_addr);
s32 ixgbe_set_fw_drv_ver_generic(struct ixgbe_hw *hw, u8 maj, u8 min, u8 build, u8 sub);
s32 ixgbe_check_mac_link_generic(struct ixgbe_hw *hw, u32 *speed,
				 bool *link_up, bool link_up_wait_to_complete);
u16 ixgbe_get_pcie_msix_count_generic(struct ixgbe_hw *hw);
void ixgbe_start_hw_gen2(struct ixgbe_hw *hw);
bool ixgbe_device_supports_autoneg_fc(struct ixgbe_hw *hw);
void ixgbe_clear_tx_pending(struct ixgbe_hw *hw);
s32 ixgbe_disable_pcie_master(struct ixgbe_hw *hw);
s32 ixgbe_init_uta_tables_generic(struct ixgbe_hw *hw);
u8 ixgbe_calculate_checksum(u8 *buffer, u32 length);
s32 ixgbe_host_interface_command(struct ixgbe_hw *hw, u32 *buffer,
				 u32 length);
s32 ixgbe_set_rar_generic(struct ixgbe_hw *hw, u32 index, u8 *addr, u32 vmdq,
			  u32 enable_addr);
s32 ixgbe_clear_vfta_generic(struct ixgbe_hw *hw);
s32 ixgbe_clear_hw_cntrs_generic(struct ixgbe_hw *hw);
void ixgbe_set_lan_id_multi_port_pcie(struct ixgbe_hw *hw);
void ixgbe_setup_fc(struct ixgbe_hw *hw);

static inline bool IXGBE_REMOVED(void __iomem *addr)
{
	return unlikely(!addr);
}

#define IXGBE_FAILED_READ_REG 0xffffffffU

static inline u32 IXGBE_READ_REG(struct ixgbe_hw *hw, u32 reg)
{
	u32 value;
	u8 __iomem *reg_addr;

	reg_addr = ACCESS_ONCE(hw->hw_addr);
	if (IXGBE_REMOVED(reg_addr))
		return IXGBE_FAILED_READ_REG;
	value = readl(reg_addr + reg);
	return value;
}

static inline void IXGBE_WRITE_REG(struct ixgbe_hw *hw, u32 reg, u32 value)
{
	u8 __iomem *reg_addr;

	reg_addr = ACCESS_ONCE(hw->hw_addr);
	if (IXGBE_REMOVED(reg_addr))
		return;

	writel(value, reg_addr + reg);
}

#define IXGBE_READ_REG_ARRAY(a, reg, offset) ( \
	IXGBE_READ_REG((a), (reg) + ((offset) << 2)))

#define IXGBE_WRITE_REG_ARRAY(a, reg, offset, value) \
	IXGBE_WRITE_REG((a), (reg) + ((offset) << 2), (value))

#define IXGBE_WRITE_FLUSH(a) IXGBE_READ_REG(a, IXGBE_STATUS)

#define IXGBE_CPU_TO_LE32(_i) cpu_to_le32(_i)
#define IXGBE_LE32_TO_CPUS(_i) le32_to_cpus(_i)
