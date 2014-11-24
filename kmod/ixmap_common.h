int32_t ixmap_init_hw(struct ixmap_hw *hw);
void ixmap_start_hw(struct ixmap_hw *hw);
int32_t ixmap_get_mac_addr(struct ixmap_hw *hw,
	uint8_t *mac_addr);
int32_t ixmap_stop_adapter(struct ixmap_hw *hw);
int32_t ixmap_init_rx_addrs(struct ixmap_hw *hw);
int32_t ixmap_validate_mac_addr(uint8_t *mac_addr);
int32_t ixmap_set_fw_drv_ver(struct ixmap_hw *hw,
	uint8_t maj, uint8_t min, uint8_t build, uint8_t sub);
int32_t ixmap_check_mac_link(struct ixmap_hw *hw, uint32_t *speed,
	int *link_up, int link_up_wait_to_complete);
uint16_t ixmap_get_pcie_msix_count(struct ixmap_hw *hw);
void ixmap_start_hw_gen2(struct ixmap_hw *hw);
int ixmap_device_supports_autoneg_fc(struct ixmap_hw *hw);
void ixmap_clear_tx_pending(struct ixmap_hw *hw);
int32_t ixmap_disable_pcie_master(struct ixmap_hw *hw);
int32_t ixmap_init_uta_tables(struct ixmap_hw *hw);
uint8_t ixmap_calculate_checksum(uint8_t *buffer, uint32_t length);
int32_t ixmap_host_interface_command(struct ixmap_hw *hw, uint32_t *buffer,
	uint32_t length);
int32_t ixmap_set_rar(struct ixmap_hw *hw,
	uint32_t index, uint8_t *addr, uint32_t vmdq, uint32_t enable_addr);
int32_t ixmap_clear_vfta(struct ixmap_hw *hw);
int32_t ixmap_clear_hw_cntrs(struct ixmap_hw *hw);
void ixmap_set_lan_id_multi_port_pcie(struct ixmap_hw *hw);
void ixmap_setup_fc(struct ixmap_hw *hw);

static inline int IXGBE_REMOVED(void __iomem *addr)
{
	return unlikely(!addr);
}

#define IXGBE_FAILED_READ_REG 0xffffffffU

static inline uint32_t IXGBE_READ_REG(struct ixmap_hw *hw,
	uint32_t reg)
{
	uint32_t value;
	uint8_t __iomem *reg_addr;

	reg_addr = ACCESS_ONCE(hw->hw_addr);
	if (IXGBE_REMOVED(reg_addr))
		return IXGBE_FAILED_READ_REG;
	value = readl(reg_addr + reg);
	return value;
}

static inline void IXGBE_WRITE_REG(struct ixmap_hw *hw,
	uint32_t reg, uint32_t value)
{
	uint8_t __iomem *reg_addr;

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
