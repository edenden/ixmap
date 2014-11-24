#define IXGBE_82599_MAX_TX_QUEUES 128
#define IXGBE_82599_MAX_RX_QUEUES 128
#define IXGBE_82599_RAR_ENTRIES   128
#define IXGBE_82599_MC_TBL_SIZE   128
#define IXGBE_82599_VFT_TBL_SIZE  128
#define IXGBE_82599_RX_PB_SIZE    512

int32_t ixmap_init_ops_82599(struct ixmap_hw *hw);
int32_t ixmap_reset_hw_82599(struct ixmap_hw *hw);
int32_t ixmap_start_hw_82599(struct ixmap_hw *hw);
int32_t ixmap_init_phy_ops_82599(struct ixmap_hw *hw);
enum ixmap_media_type ixmap_get_media_type_82599(struct ixmap_hw *hw);
void ixmap_init_mac_link_ops_82599(struct ixmap_hw *hw);
void ixmap_disable_tx_laser_multispeed_fiber(struct ixmap_hw *hw);
void ixmap_enable_tx_laser_multispeed_fiber(struct ixmap_hw *hw);
void ixmap_flap_tx_laser_multispeed_fiber(struct ixmap_hw *hw);
int32_t prot_autoc_read_82599(struct ixmap_hw *hw,
	int *locked, uint32_t *reg_val);
int32_t prot_autoc_write_82599(struct ixmap_hw *hw,
	uint32_t autoc, int locked);
int32_t ixmap_setup_sfp_modules_82599(struct ixmap_hw *hw);
int32_t ixmap_identify_phy_82599(struct ixmap_hw *hw);
int32_t ixmap_setup_mac_link_82599(struct ixmap_hw *hw,
	uint32_t speed, int autoneg_wait_to_complete);
int32_t ixmap_get_link_capabilities_82599(struct ixmap_hw *hw,
	uint32_t *speed, int *autoneg);
int ixmap_verify_lesm_fw_enabled_82599(struct ixmap_hw *hw);
int32_t ixmap_reset_pipeline_82599(struct ixmap_hw *hw);
