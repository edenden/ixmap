#include <linux/init.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/mii.h>
#include <linux/vmalloc.h>
#include <asm/io.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>

#include "ixmap_type.h"
#include "ixmap_main.h"
#include "ixmap_82599.h"
#include "ixmap_common.h"
#include "ixmap_phy.h"
#include "ixmap_eeprom.h"

static s32 ixgbe_read_eeprom_82599(struct ixgbe_hw *hw, u16 offset, u16 *data);
static s32 ixgbe_verify_fw_version_82599(struct ixgbe_hw *hw);

s32 ixgbe_init_ops_82599(struct ixgbe_hw *hw){
	struct ixgbe_mac_info *mac = &hw->mac;
	struct ixgbe_phy_info *phy = &hw->phy;
	struct ixgbe_eeprom_info *eeprom = &hw->eeprom;

	/* PHY */
	phy->ops.identify = &ixgbe_identify_phy_82599;
	phy->ops.init = &ixgbe_init_phy_ops_82599;

	/* MAC */
	mac->ops.init_hw = &ixgbe_init_hw_generic;
	mac->ops.reset_hw = &ixgbe_reset_hw_82599;
	mac->ops.get_mac_addr = &ixgbe_get_mac_addr_generic;
	mac->ops.get_media_type = &ixgbe_get_media_type_82599;
	mac->ops.setup_link = &ixgbe_setup_mac_link_82599;
	mac->ops.check_link = &ixgbe_check_mac_link_generic;
	mac->ops.start_hw = &ixgbe_start_hw_82599;
	mac->ops.prot_autoc_write = &prot_autoc_write_82599;
	mac->ops.prot_autoc_read = &prot_autoc_read_82599;
	mac->ops.stop_adapter = &ixgbe_stop_adapter_generic;
	mac->ops.setup_sfp = &ixgbe_setup_sfp_modules_82599;
	mac->ops.acquire_swfw_sync = &ixgbe_acquire_swfw_sync;
	mac->ops.release_swfw_sync = &ixgbe_release_swfw_sync;
	mac->ops.clear_hw_cntrs = &ixgbe_clear_hw_cntrs_generic;
	mac->ops.set_lan_id = &ixgbe_set_lan_id_multi_port_pcie;

	/* RAR, Multicast, VLAN */
	mac->ops.set_rar = &ixgbe_set_rar_generic;
	mac->ops.init_rx_addrs = &ixgbe_init_rx_addrs_generic;
	mac->ops.clear_vfta = &ixgbe_clear_vfta_generic;

	/* Manageability interface */
	mac->ops.set_fw_drv_ver = &ixgbe_set_fw_drv_ver_generic;

	mac->mcft_size	  = IXGBE_82599_MC_TBL_SIZE;
	mac->vft_size	   = IXGBE_82599_VFT_TBL_SIZE;
	mac->num_rar_entries    = IXGBE_82599_RAR_ENTRIES;
	mac->rx_pb_size	 = IXGBE_82599_RX_PB_SIZE;
	mac->max_rx_queues	= IXGBE_82599_MAX_RX_QUEUES;
	mac->max_tx_queues	= IXGBE_82599_MAX_TX_QUEUES;
	mac->max_msix_vectors   = ixgbe_get_pcie_msix_count_generic(hw);

	mac->arc_subsystem_valid = (IXGBE_READ_REG(hw, IXGBE_FWSM) &
					IXGBE_FWSM_MODE_MASK) ? true : false;

	/* EEPROM */
	eeprom->ops.init_params = &ixgbe_init_eeprom_params_generic;
	eeprom->ops.read = &ixgbe_read_eeprom_82599;
	eeprom->ops.validate_checksum = &ixgbe_validate_eeprom_checksum_generic;
	eeprom->ops.calc_checksum = &ixgbe_calc_eeprom_checksum_generic;

	return 0;
}

s32 ixgbe_reset_hw_82599(struct ixgbe_hw *hw){
	u32 link_speed;
	s32 status;
	u32 ctrl = 0;
	u32 i, autoc, autoc2;
	bool link_up = false;

	/* Call adapter stop to disable tx/rx and clear interrupts */
	status = hw->mac.ops.stop_adapter(hw);
	if (status != 0)
		goto reset_hw_out;

	/* flush pending Tx transactions */
	ixgbe_clear_tx_pending(hw);

	/* Identify PHY and related function pointers */
	status = hw->phy.ops.init(hw);
	if(status != 0){
		goto reset_hw_out;
	}

	/* Setup SFP module if there is one present. */
	if (hw->phy.sfp_setup_needed) {
		status = hw->mac.ops.setup_sfp(hw);
		hw->phy.sfp_setup_needed = false;
		if(status != 0){
			goto reset_hw_out;
		}
	}

mac_reset_top:
	/*
	 * Issue global reset to the MAC.  Needs to be SW reset if link is up.
	 * If link reset is used when link is up, it might reset the PHY when
	 * mng is using it.  If link is down or the flag to force full link
	 * reset is set, then perform link reset.
	 */
	ctrl = IXGBE_CTRL_LNK_RST;
	if (!hw->force_full_reset) {
		hw->mac.ops.check_link(hw, &link_speed, &link_up, false);
		if (link_up)
			ctrl = IXGBE_CTRL_RST;
	}

	ctrl |= IXGBE_READ_REG(hw, IXGBE_CTRL);
	IXGBE_WRITE_REG(hw, IXGBE_CTRL, ctrl);
	IXGBE_WRITE_FLUSH(hw);

	/* Poll for reset bit to self-clear meaning reset is complete */
	for (i = 0; i < 10; i++) {
		udelay(1);
		ctrl = IXGBE_READ_REG(hw, IXGBE_CTRL);
		if (!(ctrl & IXGBE_CTRL_RST_MASK))
			break;
	}

	if (ctrl & IXGBE_CTRL_RST_MASK) {
		status = IXGBE_ERR_RESET_FAILED;
	}

	msleep(50);

	/*
	 * Double resets are required for recovery from certain error
	 * conditions.  Between resets, it is necessary to stall to
	 * allow time for any pending HW events to complete.
	 */
	if (hw->mac.flags & IXGBE_FLAGS_DOUBLE_RESET_REQUIRED) {
		hw->mac.flags &= ~IXGBE_FLAGS_DOUBLE_RESET_REQUIRED;
		goto mac_reset_top;
	}

	/*
	 * Store the original AUTOC/AUTOC2 values if they have not been
	 * stored off yet.  Otherwise restore the stored original
	 * values since the reset operation sets back to defaults.
	 */
	autoc = IXGBE_READ_REG(hw, IXGBE_AUTOC);
	autoc2 = IXGBE_READ_REG(hw, IXGBE_AUTOC2);

	/* Enable link if disabled in NVM */
	if (autoc2 & IXGBE_AUTOC2_LINK_DISABLE_MASK) {
		autoc2 &= ~IXGBE_AUTOC2_LINK_DISABLE_MASK;
		IXGBE_WRITE_REG(hw, IXGBE_AUTOC2, autoc2);
		IXGBE_WRITE_FLUSH(hw);
	}

	if (hw->mac.orig_link_settings_stored == false) {
		hw->mac.orig_autoc = autoc;
		hw->mac.orig_autoc2 = autoc2;
		hw->mac.orig_link_settings_stored = true;
	} else {
		if (autoc != hw->mac.orig_autoc) {
			status = hw->mac.ops.prot_autoc_write(hw,
							hw->mac.orig_autoc,
							false);
			if (status != 0)
				goto reset_hw_out;
		}

		if ((autoc2 & IXGBE_AUTOC2_UPPER_MASK) !=
		    (hw->mac.orig_autoc2 & IXGBE_AUTOC2_UPPER_MASK)) {
			autoc2 &= ~IXGBE_AUTOC2_UPPER_MASK;
			autoc2 |= (hw->mac.orig_autoc2 &
				   IXGBE_AUTOC2_UPPER_MASK);
			IXGBE_WRITE_REG(hw, IXGBE_AUTOC2, autoc2);
		}
	}

	/* Store the permanent mac address */
	hw->mac.ops.get_mac_addr(hw, hw->mac.perm_addr);

	/*
	 * Store MAC address from RAR0, clear receive address registers, and
	 * clear the multicast table.  Also reset num_rar_entries to 128,
	 * since we modify this value when programming the SAN MAC address.
	 */
	hw->mac.num_rar_entries = 128;
	hw->mac.ops.init_rx_addrs(hw);

reset_hw_out:
	return status;
}

s32 ixgbe_start_hw_82599(struct ixgbe_hw *hw){
	s32 ret_val = 0;

	ixgbe_start_hw_generic(hw);
	ixgbe_start_hw_gen2(hw);

	/* We need to run link autotry after the driver loads */
	hw->mac.autotry_restart = true;

	ret_val = ixgbe_verify_fw_version_82599(hw);

	return ret_val;
}

static s32 ixgbe_verify_fw_version_82599(struct ixgbe_hw *hw)
{
	s32 status = IXGBE_ERR_EEPROM_VERSION;
	u16 fw_offset, fw_ptp_cfg_offset;
	u16 fw_version;

	/* firmware check is only necessary for SFI devices */
	if (hw->phy.media_type != ixgbe_media_type_fiber) {
		status = 0;
		goto fw_version_out;
	}

	/* get the offset to the Firmware Module block */
	if (hw->eeprom.ops.read(hw, IXGBE_FW_PTR, &fw_offset)) {
		return IXGBE_ERR_EEPROM_VERSION;
	}

	if ((fw_offset == 0) || (fw_offset == 0xFFFF))
		goto fw_version_out;

	/* get the offset to the Pass Through Patch Configuration block */
	if (hw->eeprom.ops.read(hw, (fw_offset +
				 IXGBE_FW_PASSTHROUGH_PATCH_CONFIG_PTR),
				 &fw_ptp_cfg_offset)) {
		return IXGBE_ERR_EEPROM_VERSION;
	}

	if ((fw_ptp_cfg_offset == 0) || (fw_ptp_cfg_offset == 0xFFFF))
		goto fw_version_out;

	/* get the firmware version */
	if (hw->eeprom.ops.read(hw, (fw_ptp_cfg_offset +
			    IXGBE_FW_PATCH_VERSION_4), &fw_version)) {
		return IXGBE_ERR_EEPROM_VERSION;
	}

	if (fw_version > 0x5)
		status = 0;

fw_version_out:
	return status;
}

s32 ixgbe_init_phy_ops_82599(struct ixgbe_hw *hw){
	struct ixgbe_phy_info *phy = &hw->phy;
	u32 ret_val = 0;

	/* Identify the PHY or SFP module */
	ret_val = phy->ops.identify(hw);

	/* Setup function pointers based on detected SFP module and speeds */
	ixgbe_init_mac_link_ops_82599(hw);

	return ret_val;
}

enum ixgbe_media_type ixgbe_get_media_type_82599(struct ixgbe_hw *hw){
	enum ixgbe_media_type media_type;

	switch (hw->device_id) {
	case IXGBE_DEV_ID_82599_SFP:
	case IXGBE_DEV_ID_82599_SFP_FCOE:
	case IXGBE_DEV_ID_82599_SFP_EM:
	case IXGBE_DEV_ID_82599_SFP_SF2:
	case IXGBE_DEV_ID_82599_SFP_SF_QP:
	case IXGBE_DEV_ID_82599EN_SFP:
		media_type = ixgbe_media_type_fiber;
		break;
	default:
		media_type = ixgbe_media_type_unknown;
		break;
	}

	return media_type;
}

void ixgbe_init_mac_link_ops_82599(struct ixgbe_hw *hw){
	struct ixgbe_mac_info *mac = &hw->mac;

	/* enable the laser control functions for SFP+ fiber */
	if (mac->ops.get_media_type(hw) == ixgbe_media_type_fiber) {
		mac->ops.disable_tx_laser = &ixgbe_disable_tx_laser_multispeed_fiber;
		mac->ops.enable_tx_laser = &ixgbe_enable_tx_laser_multispeed_fiber;
		mac->ops.flap_tx_laser = &ixgbe_flap_tx_laser_multispeed_fiber;
	} else {
		mac->ops.disable_tx_laser = NULL;
		mac->ops.enable_tx_laser = NULL;
		mac->ops.flap_tx_laser = NULL;
	}
}

void ixgbe_disable_tx_laser_multispeed_fiber(struct ixgbe_hw *hw){
	u32 esdp_reg = IXGBE_READ_REG(hw, IXGBE_ESDP);

	/* Disable tx laser; allow 100us to go dark per spec */
	esdp_reg |= IXGBE_ESDP_SDP3;
	IXGBE_WRITE_REG(hw, IXGBE_ESDP, esdp_reg);
	IXGBE_WRITE_FLUSH(hw);
	udelay(100);
}

void ixgbe_enable_tx_laser_multispeed_fiber(struct ixgbe_hw *hw){
	u32 esdp_reg = IXGBE_READ_REG(hw, IXGBE_ESDP);

	/* Enable tx laser; allow 100ms to light up */
	esdp_reg &= ~IXGBE_ESDP_SDP3;
	IXGBE_WRITE_REG(hw, IXGBE_ESDP, esdp_reg);
	IXGBE_WRITE_FLUSH(hw);
	msleep(100);
}

void ixgbe_flap_tx_laser_multispeed_fiber(struct ixgbe_hw *hw){

	if (hw->mac.autotry_restart) {
		ixgbe_disable_tx_laser_multispeed_fiber(hw);
		ixgbe_enable_tx_laser_multispeed_fiber(hw);
		hw->mac.autotry_restart = false;
	}
}

s32 prot_autoc_read_82599(struct ixgbe_hw *hw, bool *locked, u32 *reg_val)
{
	s32 ret_val;

	*locked = false;
	 /* If LESM is on then we need to hold the SW/FW semaphore. */
	if (ixgbe_verify_lesm_fw_enabled_82599(hw)) {
		ret_val = hw->mac.ops.acquire_swfw_sync(hw,
					IXGBE_GSSR_MAC_CSR_SM);
		if (ret_val != 0)
			return IXGBE_ERR_SWFW_SYNC;

		*locked = true;
	}

	*reg_val = IXGBE_READ_REG(hw, IXGBE_AUTOC);
	return 0;
}

s32 prot_autoc_write_82599(struct ixgbe_hw *hw, u32 autoc, bool locked){
	s32 ret_val = 0;

	/* We only need to get the lock if:
	 *  - We didn't do it already (in the read part of a read-modify-write)
	 *  - LESM is enabled.
	 */
	if (!locked && ixgbe_verify_lesm_fw_enabled_82599(hw)) {
		ret_val = hw->mac.ops.acquire_swfw_sync(hw,
					IXGBE_GSSR_MAC_CSR_SM);
		if (ret_val != 0)
			return IXGBE_ERR_SWFW_SYNC;

		locked = true;
	}

	IXGBE_WRITE_REG(hw, IXGBE_AUTOC, autoc);
	ret_val = ixgbe_reset_pipeline_82599(hw);

	/* Free the SW/FW semaphore as we either grabbed it here or
	 * already had it when this function was called.
	 */
	if (locked)
		hw->mac.ops.release_swfw_sync(hw, IXGBE_GSSR_MAC_CSR_SM);

	return ret_val;
}

bool ixgbe_verify_lesm_fw_enabled_82599(struct ixgbe_hw *hw)
{
	bool lesm_enabled = false;
	u16 fw_offset, fw_lesm_param_offset, fw_lesm_state;
	s32 status;

	/* get the offset to the Firmware Module block */
	status = hw->eeprom.ops.read(hw, IXGBE_FW_PTR, &fw_offset);

	if ((status != 0) ||
	    (fw_offset == 0) || (fw_offset == 0xFFFF))
		goto out;

	/* get the offset to the LESM Parameters block */
	status = hw->eeprom.ops.read(hw, (fw_offset +
				     IXGBE_FW_LESM_PARAMETERS_PTR),
				     &fw_lesm_param_offset);

	if ((status != 0) ||
	    (fw_lesm_param_offset == 0) || (fw_lesm_param_offset == 0xFFFF))
		goto out;

	/* get the lesm state word */
	status = hw->eeprom.ops.read(hw, (fw_lesm_param_offset +
				     IXGBE_FW_LESM_STATE_1),
				     &fw_lesm_state);

	if ((status == 0) &&
	    (fw_lesm_state & IXGBE_FW_LESM_STATE_ENABLED))
		lesm_enabled = true;

out:
	return lesm_enabled;
}

s32 ixgbe_setup_sfp_modules_82599(struct ixgbe_hw *hw){
	s32 ret_val = 0;
	u16 list_offset, data_offset, data_value;

	ixgbe_init_mac_link_ops_82599(hw);

	ret_val = ixgbe_get_sfp_init_sequence_offsets(hw, &list_offset, &data_offset);
	if (ret_val != 0)
		goto setup_sfp_out;

	/* PHY config will finish before releasing the semaphore */
	ret_val = hw->mac.ops.acquire_swfw_sync(hw, IXGBE_GSSR_MAC_CSR_SM);
	if (ret_val != 0) {
		ret_val = IXGBE_ERR_SWFW_SYNC;
		goto setup_sfp_out;
	}

	if (hw->eeprom.ops.read(hw, ++data_offset, &data_value))
		goto setup_sfp_err;
	while (data_value != 0xffff) {
		IXGBE_WRITE_REG(hw, IXGBE_CORECTL, data_value);
		IXGBE_WRITE_FLUSH(hw);
		if (hw->eeprom.ops.read(hw, ++data_offset, &data_value))
			goto setup_sfp_err;
	}

	/* Release the semaphore */
	hw->mac.ops.release_swfw_sync(hw, IXGBE_GSSR_MAC_CSR_SM);
	/* Delay obtaining semaphore again to allow FW access
	* prot_autoc_write uses the semaphore too.
	*/
	msleep(hw->eeprom.semaphore_delay);

	/* Restart DSP and set SFI mode */
	ret_val = hw->mac.ops.prot_autoc_write(hw,
	hw->mac.orig_autoc | IXGBE_AUTOC_LMS_10G_SERIAL,
	false);

	if (ret_val) {
		ret_val = IXGBE_ERR_SFP_SETUP_NOT_COMPLETE;
		goto setup_sfp_out;
	}

setup_sfp_out:
	return ret_val;

setup_sfp_err:
	/* Release the semaphore */
	hw->mac.ops.release_swfw_sync(hw, IXGBE_GSSR_MAC_CSR_SM);
	/* Delay obtaining semaphore again to allow FW access */
	msleep(hw->eeprom.semaphore_delay);
	return IXGBE_ERR_PHY;
}

s32 ixgbe_reset_pipeline_82599(struct ixgbe_hw *hw)
{
	s32 ret_val;
	u32 anlp1_reg = 0;
	u32 i, autoc_reg, autoc2_reg;

	/* Enable link if disabled in NVM */
	autoc2_reg = IXGBE_READ_REG(hw, IXGBE_AUTOC2);
	if (autoc2_reg & IXGBE_AUTOC2_LINK_DISABLE_MASK) {
		autoc2_reg &= ~IXGBE_AUTOC2_LINK_DISABLE_MASK;
		IXGBE_WRITE_REG(hw, IXGBE_AUTOC2, autoc2_reg);
		IXGBE_WRITE_FLUSH(hw);
	}

	autoc_reg = IXGBE_READ_REG(hw, IXGBE_AUTOC);
	autoc_reg |= IXGBE_AUTOC_AN_RESTART;
	/* Write AUTOC register with toggled LMS[2] bit and Restart_AN */
	IXGBE_WRITE_REG(hw, IXGBE_AUTOC,
			autoc_reg ^ (0x4 << IXGBE_AUTOC_LMS_SHIFT));
	/* Wait for AN to leave state 0 */
	for (i = 0; i < 10; i++) {
		msleep(4);
		anlp1_reg = IXGBE_READ_REG(hw, IXGBE_ANLP1);
		if (anlp1_reg & IXGBE_ANLP1_AN_STATE_MASK)
			break;
	}

	if (!(anlp1_reg & IXGBE_ANLP1_AN_STATE_MASK)) {
		ret_val = IXGBE_ERR_RESET_FAILED;
		goto reset_pipeline_out;
	}

	ret_val = 0;

reset_pipeline_out:
	/* Write AUTOC register with original LMS field and Restart_AN */
	IXGBE_WRITE_REG(hw, IXGBE_AUTOC, autoc_reg);
	IXGBE_WRITE_FLUSH(hw);

	return ret_val;
}

static s32 ixgbe_read_eeprom_82599(struct ixgbe_hw *hw, u16 offset, u16 *data){
	struct ixgbe_eeprom_info *eeprom = &hw->eeprom;
	s32 ret_val = IXGBE_ERR_CONFIG;

	/*
	 * If EEPROM is detected and can be addressed using 14 bits,
	 * use EERD otherwise use bit bang
	 * (At first time, eeprom->type is ixgbe_eeprom_uninitialized,
	 * but after that eeprom->type will be ixgbe_eeprom_spi.)
	 */
	if ((eeprom->type == ixgbe_eeprom_spi) &&
	    (offset <= IXGBE_EERD_MAX_ADDR))
		ret_val = ixgbe_read_eerd_generic(hw, offset, data);
	else
		ret_val = ixgbe_read_eeprom_bit_bang_generic(hw, offset, data);

	return ret_val;
}

s32 ixgbe_identify_phy_82599(struct ixgbe_hw *hw){
	s32 status;

	/* Detect PHY if not unknown - returns success if already detected. */
	status = ixgbe_identify_module_generic(hw);

	return status;
}

s32 ixgbe_setup_mac_link_82599(struct ixgbe_hw *hw, u32 speed, bool autoneg_wait_to_complete){
	bool autoneg = false;
	s32 status = 0;
	u32 pma_pmd_1g, link_mode;

	/* holds the value of AUTOC register at this curr ent point in time */
	u32 current_autoc = IXGBE_READ_REG(hw, IXGBE_AUTOC); 

	u32 orig_autoc = 0; /* holds the cached value of AUTOC register */
	u32 autoc = current_autoc; /* Temporary variable used for comparison purposes */
	u32 autoc2 = IXGBE_READ_REG(hw, IXGBE_AUTOC2);
	u32 pma_pmd_10g_serial = autoc2 & IXGBE_AUTOC2_10G_SERIAL_PMA_PMD_MASK;
	u32 links_reg;
	u32 i;
	u32 link_capabilities = IXGBE_LINK_SPEED_UNKNOWN;

	/* Check to see if speed passed in is supported. */
	status = ixgbe_get_link_capabilities_82599(hw, &link_capabilities, &autoneg);
	if (status)
		goto out;

	speed &= link_capabilities;

	if (speed == IXGBE_LINK_SPEED_UNKNOWN) {
		status = IXGBE_ERR_LINK_SETUP;
		goto out;
	}

	/* Use stored value (EEPROM defaults) of AUTOC to find KR/KX4 support*/
	if (hw->mac.orig_link_settings_stored)
		orig_autoc = hw->mac.orig_autoc;
	else
		orig_autoc = autoc;

	link_mode = autoc & IXGBE_AUTOC_LMS_MASK;
	pma_pmd_1g = autoc & IXGBE_AUTOC_1G_PMA_PMD_MASK;

	if (link_mode == IXGBE_AUTOC_LMS_KX4_KX_KR ||
	    link_mode == IXGBE_AUTOC_LMS_KX4_KX_KR_1G_AN ||
	    link_mode == IXGBE_AUTOC_LMS_KX4_KX_KR_SGMII) {
		/* Set KX4/KX/KR support according to speed requested */
		autoc &= ~(IXGBE_AUTOC_KX4_KX_SUPP_MASK | IXGBE_AUTOC_KR_SUPP);
		if (speed & IXGBE_LINK_SPEED_10GB_FULL) {
			if (orig_autoc & IXGBE_AUTOC_KX4_SUPP)
				autoc |= IXGBE_AUTOC_KX4_SUPP;
			if (orig_autoc & IXGBE_AUTOC_KR_SUPP)
				autoc |= IXGBE_AUTOC_KR_SUPP;
		}
		if (speed & IXGBE_LINK_SPEED_1GB_FULL)
			autoc |= IXGBE_AUTOC_KX_SUPP;
	} else if ((pma_pmd_1g == IXGBE_AUTOC_1G_SFI) &&
		   (link_mode == IXGBE_AUTOC_LMS_1G_LINK_NO_AN ||
		    link_mode == IXGBE_AUTOC_LMS_1G_AN)) {
		/* Switch from 1G SFI to 10G SFI if requested */
		if ((speed == IXGBE_LINK_SPEED_10GB_FULL) &&
		    (pma_pmd_10g_serial == IXGBE_AUTOC2_10G_SFI)) {
			autoc &= ~IXGBE_AUTOC_LMS_MASK;
			autoc |= IXGBE_AUTOC_LMS_10G_SERIAL;
		}
	} else if ((pma_pmd_10g_serial == IXGBE_AUTOC2_10G_SFI) &&
		   (link_mode == IXGBE_AUTOC_LMS_10G_SERIAL)) {
		/* Switch from 10G SFI to 1G SFI if requested */
		if ((speed == IXGBE_LINK_SPEED_1GB_FULL) &&
		    (pma_pmd_1g == IXGBE_AUTOC_1G_SFI)) {
			autoc &= ~IXGBE_AUTOC_LMS_MASK;
			if (autoneg)
				autoc |= IXGBE_AUTOC_LMS_1G_AN;
			else
				autoc |= IXGBE_AUTOC_LMS_1G_LINK_NO_AN;
		}
	}

	if (autoc != current_autoc) {
		/* Restart link */
		status = hw->mac.ops.prot_autoc_write(hw, autoc, false);
		if (status != 0)
			goto out;

		/* Only poll for autoneg to complete if specified to do so */
		if (autoneg_wait_to_complete) {
			if (link_mode == IXGBE_AUTOC_LMS_KX4_KX_KR ||
			    link_mode == IXGBE_AUTOC_LMS_KX4_KX_KR_1G_AN ||
			    link_mode == IXGBE_AUTOC_LMS_KX4_KX_KR_SGMII) {
				links_reg = 0; /*Just in case Autoneg time=0*/
				for (i = 0; i < IXGBE_AUTO_NEG_TIME; i++) {
					links_reg =
					       IXGBE_READ_REG(hw, IXGBE_LINKS);
					if (links_reg & IXGBE_LINKS_KX_AN_COMP)
						break;
					msleep(100);
				}
				if (!(links_reg & IXGBE_LINKS_KX_AN_COMP)) {
					status =
						IXGBE_ERR_AUTONEG_NOT_COMPLETE;
				}
			}
		}

		/* Add delay to filter out noises during initial link setup */
		msleep(50);
	}

out:
	return status;
}

s32 ixgbe_get_link_capabilities_82599(struct ixgbe_hw *hw, u32 *speed, bool *autoneg){
	s32 status = 0;
	u32 autoc = 0;

	/*
	 * Determine link capabilities based on the stored value of AUTOC,
	 * which represents EEPROM defaults.  If AUTOC value has not
	 * been stored, use the current register values.
	 */
	if (hw->mac.orig_link_settings_stored)
		autoc = hw->mac.orig_autoc;
	else
		autoc = IXGBE_READ_REG(hw, IXGBE_AUTOC);

	switch (autoc & IXGBE_AUTOC_LMS_MASK) {
	case IXGBE_AUTOC_LMS_1G_LINK_NO_AN:
		*speed = IXGBE_LINK_SPEED_1GB_FULL;
		*autoneg = false;
		break;

	case IXGBE_AUTOC_LMS_10G_LINK_NO_AN:
		*speed = IXGBE_LINK_SPEED_10GB_FULL;
		*autoneg = false;
		break;

	case IXGBE_AUTOC_LMS_1G_AN:
		*speed = IXGBE_LINK_SPEED_1GB_FULL;
		*autoneg = true;
		break;

	case IXGBE_AUTOC_LMS_10G_SERIAL:
		*speed = IXGBE_LINK_SPEED_10GB_FULL;
		*autoneg = false;
		break;

	case IXGBE_AUTOC_LMS_KX4_KX_KR:
	case IXGBE_AUTOC_LMS_KX4_KX_KR_1G_AN:
		*speed = IXGBE_LINK_SPEED_UNKNOWN;
		if (autoc & IXGBE_AUTOC_KR_SUPP)
			*speed |= IXGBE_LINK_SPEED_10GB_FULL;
		if (autoc & IXGBE_AUTOC_KX4_SUPP)
			*speed |= IXGBE_LINK_SPEED_10GB_FULL;
		if (autoc & IXGBE_AUTOC_KX_SUPP)
			*speed |= IXGBE_LINK_SPEED_1GB_FULL;
		*autoneg = true;
		break;

	case IXGBE_AUTOC_LMS_KX4_KX_KR_SGMII:
		*speed = IXGBE_LINK_SPEED_100_FULL;
		if (autoc & IXGBE_AUTOC_KR_SUPP)
			*speed |= IXGBE_LINK_SPEED_10GB_FULL;
		if (autoc & IXGBE_AUTOC_KX4_SUPP)
			*speed |= IXGBE_LINK_SPEED_10GB_FULL;
		if (autoc & IXGBE_AUTOC_KX_SUPP)
			*speed |= IXGBE_LINK_SPEED_1GB_FULL;
		*autoneg = true;
		break;

	case IXGBE_AUTOC_LMS_SGMII_1G_100M:
		*speed = IXGBE_LINK_SPEED_1GB_FULL | IXGBE_LINK_SPEED_100_FULL;
		*autoneg = false;
		break;

	default:
		status = IXGBE_ERR_LINK_SETUP;
		goto out;
		break;
	}

out:
	return status;
}
