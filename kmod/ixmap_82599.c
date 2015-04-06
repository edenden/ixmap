#include <linux/init.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/miscdevice.h>
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

static int32_t ixmap_read_eeprom_82599(struct ixmap_hw *hw,
	uint16_t offset, uint16_t *data);
static int32_t ixmap_verify_fw_version_82599(struct ixmap_hw *hw);

int32_t ixmap_init_ops_82599(struct ixmap_hw *hw)
{
	struct ixmap_mac_info *mac	= &hw->mac;
	struct ixmap_phy_info *phy	= &hw->phy;
	struct ixmap_eeprom_info *eeprom 
					= &hw->eeprom;

	/* PHY */
	phy->ops.identify		= &ixmap_identify_phy_82599;
	phy->ops.init			= &ixmap_init_phy_ops_82599;

	/* MAC */
	mac->ops.init_hw		= &ixmap_init_hw;
	mac->ops.reset_hw		= &ixmap_reset_hw_82599;
	mac->ops.get_mac_addr		= &ixmap_get_mac_addr;
	mac->ops.get_media_type		= &ixmap_get_media_type_82599;
	mac->ops.setup_link		= &ixmap_setup_mac_link_82599;
	mac->ops.check_link		= &ixmap_check_mac_link;
	mac->ops.start_hw		= &ixmap_start_hw_82599;
	mac->ops.prot_autoc_write	= &prot_autoc_write_82599;
	mac->ops.prot_autoc_read	= &prot_autoc_read_82599;
	mac->ops.stop_adapter		= &ixmap_stop_adapter;
	mac->ops.setup_sfp		= &ixmap_setup_sfp_modules_82599;
	mac->ops.acquire_swfw_sync	= &ixmap_acquire_swfw_sync;
	mac->ops.release_swfw_sync	= &ixmap_release_swfw_sync;
	mac->ops.clear_hw_cntrs		= &ixmap_clear_hw_cntrs;
	mac->ops.set_lan_id		= &ixmap_set_lan_id_multi_port_pcie;

	/* RAR, Multicast, VLAN */
	mac->ops.set_rar		= &ixmap_set_rar;
	mac->ops.init_rx_addrs		= &ixmap_init_rx_addrs;
	mac->ops.clear_vfta		= &ixmap_clear_vfta;

	/* Manageability interface */
	mac->ops.set_fw_drv_ver		= &ixmap_set_fw_drv_ver;

	mac->mcft_size			= IXGBE_82599_MC_TBL_SIZE;
	mac->vft_size			= IXGBE_82599_VFT_TBL_SIZE;
	mac->num_rar_entries		= IXGBE_82599_RAR_ENTRIES;
	mac->rx_pb_size			= IXGBE_82599_RX_PB_SIZE;
	mac->max_rx_queues		= IXGBE_82599_MAX_RX_QUEUES;
	mac->max_tx_queues		= IXGBE_82599_MAX_TX_QUEUES;
	mac->max_msix_vectors		= ixmap_get_pcie_msix_count(hw);

	mac->arc_subsystem_valid	= (IXGBE_READ_REG(hw, IXGBE_FWSM) &
					IXGBE_FWSM_MODE_MASK) ? true : false;

	/* EEPROM */
	eeprom->ops.init_params		= &ixmap_init_eeprom_params;
	eeprom->ops.read		= &ixmap_read_eeprom_82599;
	eeprom->ops.validate_checksum	= &ixmap_validate_eeprom_checksum;
	eeprom->ops.calc_checksum	= &ixmap_calc_eeprom_checksum;

	return 0;
}

int32_t ixmap_reset_hw_82599(struct ixmap_hw *hw)
{
	uint32_t link_speed;
	int32_t status;
	uint32_t ctrl = 0;
	uint32_t i, autoc, autoc2;
	int link_up = false;

	/* Call adapter stop to disable tx/rx and clear interrupts */
	status = hw->mac.ops.stop_adapter(hw);
	if (status != 0)
		goto reset_hw_out;

	/* flush pending Tx transactions */
	ixmap_clear_tx_pending(hw);

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

int32_t ixmap_start_hw_82599(struct ixmap_hw *hw)
{
	int32_t ret_val = 0;

	ixmap_start_hw(hw);
	ixmap_start_hw_gen2(hw);

	/* We need to run link autotry after the driver loads */
	hw->mac.autotry_restart = true;

	ret_val = ixmap_verify_fw_version_82599(hw);

	return ret_val;
}

static int32_t ixmap_verify_fw_version_82599(struct ixmap_hw *hw)
{
	int32_t status = IXGBE_ERR_EEPROM_VERSION;
	uint16_t fw_offset, fw_ptp_cfg_offset;
	uint16_t fw_version;

	/* firmware check is only necessary for SFI devices */
	if (hw->phy.media_type != ixmap_media_type_fiber) {
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

int32_t ixmap_init_phy_ops_82599(struct ixmap_hw *hw)
{
	struct ixmap_phy_info *phy = &hw->phy;
	uint32_t ret_val = 0;

	/* Identify the PHY or SFP module */
	ret_val = phy->ops.identify(hw);

	/* Setup function pointers based on detected SFP module and speeds */
	ixmap_init_mac_link_ops_82599(hw);

	return ret_val;
}

enum ixmap_media_type ixmap_get_media_type_82599(struct ixmap_hw *hw)
{
	enum ixmap_media_type media_type;

	switch (hw->device_id) {
	case IXGBE_DEV_ID_82599_SFP:
	case IXGBE_DEV_ID_82599_SFP_FCOE:
	case IXGBE_DEV_ID_82599_SFP_EM:
	case IXGBE_DEV_ID_82599_SFP_SF2:
	case IXGBE_DEV_ID_82599_SFP_SF_QP:
	case IXGBE_DEV_ID_82599EN_SFP:
		media_type = ixmap_media_type_fiber;
		break;
	default:
		media_type = ixmap_media_type_unknown;
		break;
	}

	return media_type;
}

void ixmap_init_mac_link_ops_82599(struct ixmap_hw *hw)
{
	struct ixmap_mac_info *mac = &hw->mac;

	/* enable the laser control functions for SFP+ fiber */
	if (mac->ops.get_media_type(hw) == ixmap_media_type_fiber) {
		mac->ops.disable_tx_laser = &ixmap_disable_tx_laser_multispeed_fiber;
		mac->ops.enable_tx_laser = &ixmap_enable_tx_laser_multispeed_fiber;
		mac->ops.flap_tx_laser = &ixmap_flap_tx_laser_multispeed_fiber;
	} else {
		mac->ops.disable_tx_laser = NULL;
		mac->ops.enable_tx_laser = NULL;
		mac->ops.flap_tx_laser = NULL;
	}
}

void ixmap_disable_tx_laser_multispeed_fiber(struct ixmap_hw *hw)
{
	uint32_t esdp_reg = IXGBE_READ_REG(hw, IXGBE_ESDP);

	/* Disable tx laser; allow 100us to go dark per spec */
	esdp_reg |= IXGBE_ESDP_SDP3;
	IXGBE_WRITE_REG(hw, IXGBE_ESDP, esdp_reg);
	IXGBE_WRITE_FLUSH(hw);
	udelay(100);
}

void ixmap_enable_tx_laser_multispeed_fiber(struct ixmap_hw *hw)
{
	uint32_t esdp_reg = IXGBE_READ_REG(hw, IXGBE_ESDP);

	/* Enable tx laser; allow 100ms to light up */
	esdp_reg &= ~IXGBE_ESDP_SDP3;
	IXGBE_WRITE_REG(hw, IXGBE_ESDP, esdp_reg);
	IXGBE_WRITE_FLUSH(hw);
	msleep(100);
}

void ixmap_flap_tx_laser_multispeed_fiber(struct ixmap_hw *hw)
{
	if (hw->mac.autotry_restart) {
		ixmap_disable_tx_laser_multispeed_fiber(hw);
		ixmap_enable_tx_laser_multispeed_fiber(hw);
		hw->mac.autotry_restart = false;
	}
}

int32_t prot_autoc_read_82599(struct ixmap_hw *hw,
	int *locked, uint32_t *reg_val)
{
	int32_t ret_val;

	*locked = false;
	 /* If LESM is on then we need to hold the SW/FW semaphore. */
	if (ixmap_verify_lesm_fw_enabled_82599(hw)) {
		ret_val = hw->mac.ops.acquire_swfw_sync(hw,
					IXGBE_GSSR_MAC_CSR_SM);
		if (ret_val != 0)
			return IXGBE_ERR_SWFW_SYNC;

		*locked = true;
	}

	*reg_val = IXGBE_READ_REG(hw, IXGBE_AUTOC);
	return 0;
}

int32_t prot_autoc_write_82599(struct ixmap_hw *hw,
	uint32_t autoc, int locked)
{
	int32_t ret_val = 0;

	/* We only need to get the lock if:
	 *  - We didn't do it already (in the read part of a read-modify-write)
	 *  - LESM is enabled.
	 */
	if (!locked && ixmap_verify_lesm_fw_enabled_82599(hw)) {
		ret_val = hw->mac.ops.acquire_swfw_sync(hw,
					IXGBE_GSSR_MAC_CSR_SM);
		if (ret_val != 0)
			return IXGBE_ERR_SWFW_SYNC;

		locked = true;
	}

	IXGBE_WRITE_REG(hw, IXGBE_AUTOC, autoc);
	ret_val = ixmap_reset_pipeline_82599(hw);

	/* Free the SW/FW semaphore as we either grabbed it here or
	 * already had it when this function was called.
	 */
	if (locked)
		hw->mac.ops.release_swfw_sync(hw, IXGBE_GSSR_MAC_CSR_SM);

	return ret_val;
}

int ixmap_verify_lesm_fw_enabled_82599(struct ixmap_hw *hw)
{
	int lesm_enabled = false;
	uint16_t fw_offset, fw_lesm_param_offset, fw_lesm_state;
	int32_t status;

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

int32_t ixmap_setup_sfp_modules_82599(struct ixmap_hw *hw)
{
	int32_t ret_val = 0;
	uint16_t list_offset, data_offset, data_value;

	ixmap_init_mac_link_ops_82599(hw);

	ret_val = ixmap_get_sfp_init_sequence_offsets(hw, &list_offset, &data_offset);
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

int32_t ixmap_reset_pipeline_82599(struct ixmap_hw *hw)
{
	int32_t ret_val;
	uint32_t anlp1_reg = 0;
	uint32_t i, autoc_reg, autoc2_reg;

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

static int32_t ixmap_read_eeprom_82599(struct ixmap_hw *hw,
	uint16_t offset, uint16_t *data)
{
	struct ixmap_eeprom_info *eeprom = &hw->eeprom;
	int32_t ret_val = IXGBE_ERR_CONFIG;

	/*
	 * If EEPROM is detected and can be addressed using 14 bits,
	 * use EERD otherwise use bit bang
	 * (At first time, eeprom->type is ixmap_eeprom_uninitialized,
	 * but after that eeprom->type will be ixmap_eeprom_spi.)
	 */
	if ((eeprom->type == ixmap_eeprom_spi) &&
	    (offset <= IXGBE_EERD_MAX_ADDR))
		ret_val = ixmap_read_eerd(hw, offset, data);
	else
		ret_val = ixmap_read_eeprom_bit_bang(hw, offset, data);

	return ret_val;
}

int32_t ixmap_identify_phy_82599(struct ixmap_hw *hw)
{
	int32_t status;

	/* Detect PHY if not unknown - returns success if already detected. */
	status = ixmap_identify_module(hw);

	return status;
}

int32_t ixmap_setup_mac_link_82599(struct ixmap_hw *hw,
	uint32_t speed, int autoneg_wait_to_complete)
{
	int autoneg = false;
	int32_t status = 0;
	uint32_t pma_pmd_1g, link_mode;

	/* holds the value of AUTOC register at this curr ent point in time */
	uint32_t current_autoc = IXGBE_READ_REG(hw, IXGBE_AUTOC); 

	uint32_t orig_autoc = 0; /* holds the cached value of AUTOC register */
	uint32_t autoc = current_autoc; /* Temporary variable used for comparison purposes */
	uint32_t autoc2 = IXGBE_READ_REG(hw, IXGBE_AUTOC2);
	uint32_t pma_pmd_10g_serial = autoc2 & IXGBE_AUTOC2_10G_SERIAL_PMA_PMD_MASK;
	uint32_t links_reg;
	uint32_t i;
	uint32_t link_capabilities = IXGBE_LINK_SPEED_UNKNOWN;

	/* Check to see if speed passed in is supported. */
	status = ixmap_get_link_capabilities_82599(hw, &link_capabilities, &autoneg);
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

int32_t ixmap_get_link_capabilities_82599(struct ixmap_hw *hw,
	uint32_t *speed, int *autoneg)
{
	int32_t status = 0;
	uint32_t autoc = 0;

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
