#define IXGBE_82599_MAX_TX_QUEUES 128
#define IXGBE_82599_MAX_RX_QUEUES 128
#define IXGBE_82599_RAR_ENTRIES   128
#define IXGBE_82599_MC_TBL_SIZE   128
#define IXGBE_82599_VFT_TBL_SIZE  128
#define IXGBE_82599_RX_PB_SIZE    512

s32 ixgbe_init_ops_82599(struct ixgbe_hw *hw);
s32 ixgbe_reset_hw_82599(struct ixgbe_hw *hw);
s32 ixgbe_start_hw_82599(struct ixgbe_hw *hw);
s32 ixgbe_init_phy_ops_82599(struct ixgbe_hw *hw);
enum ixgbe_media_type ixgbe_get_media_type_82599(struct ixgbe_hw *hw);
void ixgbe_init_mac_link_ops_82599(struct ixgbe_hw *hw);
void ixgbe_disable_tx_laser_multispeed_fiber(struct ixgbe_hw *hw);
void ixgbe_enable_tx_laser_multispeed_fiber(struct ixgbe_hw *hw);
void ixgbe_flap_tx_laser_multispeed_fiber(struct ixgbe_hw *hw);
s32 prot_autoc_read_82599(struct ixgbe_hw *hw, bool *locked, u32 *reg_val);
s32 prot_autoc_write_82599(struct ixgbe_hw *hw, u32 autoc, bool locked);
s32 ixgbe_setup_sfp_modules_82599(struct ixgbe_hw *hw);
s32 ixgbe_identify_phy_82599(struct ixgbe_hw *hw);
s32 ixgbe_setup_mac_link_82599(struct ixgbe_hw *hw, u32 speed, bool autoneg_wait_to_complete);
s32 ixgbe_get_link_capabilities_82599(struct ixgbe_hw *hw, u32 *speed, bool *autoneg);
bool ixgbe_verify_lesm_fw_enabled_82599(struct ixgbe_hw *hw);
s32 ixgbe_reset_pipeline_82599(struct ixgbe_hw *hw);
