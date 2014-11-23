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
#include "ixmap_common.h"
#include "ixmap_phy.h"

s32 ixgbe_identify_module_generic(struct ixgbe_hw *hw)
{
	s32 status = IXGBE_ERR_SFP_NOT_PRESENT;
	enum ixgbe_sfp_type stored_sfp_type = hw->phy.sfp_type;

	switch (hw->mac.ops.get_media_type(hw)) {
	case ixgbe_media_type_fiber:
		/* LAN ID is needed for sfp_type determination */
		hw->mac.ops.set_lan_id(hw);
		/* Currently we support only 10GbE SR/LR SFP+ module on the phy layer */
		if (hw->bus.lan_id == 0)
			hw->phy.sfp_type = ixgbe_sfp_type_srlr_core0;
		else
			hw->phy.sfp_type = ixgbe_sfp_type_srlr_core1;

		hw->phy.type = ixgbe_phy_generic;
		if (hw->phy.sfp_type != stored_sfp_type)
			hw->phy.sfp_setup_needed = true;
		status = 0;
		break;

	default:
		hw->phy.sfp_type = ixgbe_sfp_type_not_present;
		status = IXGBE_ERR_SFP_NOT_PRESENT;
		break;
	}

	return status;
}

s32 ixgbe_get_sfp_init_sequence_offsets(struct ixgbe_hw *hw,
					u16 *list_offset,
					u16 *data_offset)
{
	u16 sfp_id;
	u16 sfp_type = hw->phy.sfp_type;

	if (hw->phy.sfp_type == ixgbe_sfp_type_not_present)
		return IXGBE_ERR_SFP_NOT_PRESENT;

	/* Read offset to PHY init contents */
	if (hw->eeprom.ops.read(hw, IXGBE_PHY_INIT_OFFSET_NL, list_offset)) {
		return IXGBE_ERR_SFP_NO_INIT_SEQ_PRESENT;
	}

	if ((!*list_offset) || (*list_offset == 0xFFFF))
		return IXGBE_ERR_SFP_NO_INIT_SEQ_PRESENT;

	/* Shift offset to first ID word */
	(*list_offset)++;

	/*
	 * Find the matching SFP ID in the EEPROM
	 * and program the init sequence
	 */
	if (hw->eeprom.ops.read(hw, *list_offset, &sfp_id))
		goto err_phy;

	while (sfp_id != IXGBE_PHY_INIT_END_NL) {
		if (sfp_id == sfp_type) {
			(*list_offset)++;
			if (hw->eeprom.ops.read(hw, *list_offset, data_offset))
				goto err_phy;
			if ((!*data_offset) || (*data_offset == 0xFFFF)) {
				return IXGBE_ERR_SFP_NOT_SUPPORTED;
			} else {
				break;
			}
		} else {
			(*list_offset) += 2;
			if (hw->eeprom.ops.read(hw, *list_offset, &sfp_id))
				goto err_phy;
		}
	}

	if (sfp_id == IXGBE_PHY_INIT_END_NL) {
		return IXGBE_ERR_SFP_NOT_SUPPORTED;
	}

	return 0;

err_phy:
	return IXGBE_ERR_PHY;
}

