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

#include "ixmap_main.h"
#include "ixmap_type.h"
#include "ixmap_common.h"
#include "ixmap_phy.h"
#include "ixmap_eeprom.h"

static uint32_t ixmap_pcie_timeout_poll(struct ixmap_hw *hw);

int32_t ixmap_init_hw(struct ixmap_hw *hw)
{
	int32_t status;

	/* Reset the hardware */
	status = hw->mac.ops.reset_hw(hw);

	if (status == 0) {
		/* Start the HW */
		status = hw->mac.ops.start_hw(hw);
	}

	return status;
}

void ixmap_start_hw(struct ixmap_hw *hw)
{
	uint32_t ctrl_ext;

	/* Set the media type */
	hw->phy.media_type = hw->mac.ops.get_media_type(hw);

	/* PHY ops initialization must be done in reset_hw() */

	/* Clear the VLAN filter table */
	hw->mac.ops.clear_vfta(hw);

	/* Clear statistics registers */
	hw->mac.ops.clear_hw_cntrs(hw);

	/* Set No Snoop Disable */
	ctrl_ext = IXGBE_READ_REG(hw, IXGBE_CTRL_EXT);
	ctrl_ext |= IXGBE_CTRL_EXT_NS_DIS;
	IXGBE_WRITE_REG(hw, IXGBE_CTRL_EXT, ctrl_ext);
	IXGBE_WRITE_FLUSH(hw);

	/* Clear adapter stopped flag */
	hw->adapter_stopped = false;

	return;
}

void ixmap_start_hw_gen2(struct ixmap_hw *hw)
{
	uint32_t i;
	uint32_t regval;

	/* Clear the rate limiters */
	for (i = 0; i < hw->mac.max_tx_queues; i++) {
		IXGBE_WRITE_REG(hw, IXGBE_RTTDQSEL, i);
		IXGBE_WRITE_REG(hw, IXGBE_RTTBCNRC, 0);
	}
	IXGBE_WRITE_FLUSH(hw);

	/* Disable relaxed ordering */
	for (i = 0; i < hw->mac.max_tx_queues; i++) {
		regval = IXGBE_READ_REG(hw, IXGBE_DCA_TXCTRL_82599(i));
		regval &= ~IXGBE_DCA_TXCTRL_DESC_WRO_EN;
		IXGBE_WRITE_REG(hw, IXGBE_DCA_TXCTRL_82599(i), regval);
	}

	for (i = 0; i < hw->mac.max_rx_queues; i++) {
		regval = IXGBE_READ_REG(hw, IXGBE_DCA_RXCTRL(i));
		regval &= ~(IXGBE_DCA_RXCTRL_DATA_WRO_EN |
			    IXGBE_DCA_RXCTRL_HEAD_WRO_EN);
		IXGBE_WRITE_REG(hw, IXGBE_DCA_RXCTRL(i), regval);
	}

	return;
}

int ixmap_device_supports_autoneg_fc(struct ixmap_hw *hw)
{
	int supported = false;
	ixmap_link_speed speed;
	int link_up;

	switch (hw->phy.media_type) {
	case ixmap_media_type_fiber:
		hw->mac.ops.check_link(hw, &speed, &link_up, false);
		/* if link is down, assume supported */
		if (link_up)
			supported = speed == IXGBE_LINK_SPEED_1GB_FULL ?
				true : false;
		else
			supported = true;
		break;
	default:
		break;
	}

	return supported;
}

void ixmap_setup_fc(struct ixmap_hw *hw)
{
	uint32_t mflcn_reg, fccfg_reg;

	/* Disable any previous flow control settings */
	mflcn_reg = IXGBE_READ_REG(hw, IXGBE_MFLCN);
	mflcn_reg &= ~(IXGBE_MFLCN_RPFCE_MASK | IXGBE_MFLCN_RFCE);

	fccfg_reg = IXGBE_READ_REG(hw, IXGBE_FCCFG);
	fccfg_reg &= ~(IXGBE_FCCFG_TFCE_802_3X | IXGBE_FCCFG_TFCE_PRIORITY);

	/*
	 * Flow control is disabled by software override or autoneg.
	 * The code below will actually disable it in the HW.
	 */

	/* Set 802.3x based flow control settings. */
	mflcn_reg |= IXGBE_MFLCN_DPF;
	IXGBE_WRITE_REG(hw, IXGBE_MFLCN, mflcn_reg);
	IXGBE_WRITE_REG(hw, IXGBE_FCCFG, fccfg_reg);

	/* XXX: Should we set IXGBE_FCRTL_82599 & IXGBE_FCRTH_82599 here ? */

	return;
}

int32_t ixmap_get_mac_addr(struct ixmap_hw *hw, uint8_t *mac_addr)
{
	uint32_t rar_high;
	uint32_t rar_low;
	uint16_t i;

	rar_high = IXGBE_READ_REG(hw, IXGBE_RAH(0));
	rar_low = IXGBE_READ_REG(hw, IXGBE_RAL(0));

	for (i = 0; i < 4; i++)
		mac_addr[i] = (uint8_t)(rar_low >> (i*8));

	for (i = 0; i < 2; i++)
		mac_addr[i+4] = (uint8_t)(rar_high >> (i*8));

	return 0;
}

void ixmap_set_lan_id_multi_port_pcie(struct ixmap_hw *hw)
{
	struct ixmap_bus_info *bus = &hw->bus;
	uint32_t reg;

	reg = IXGBE_READ_REG(hw, IXGBE_STATUS);
	bus->func = (reg & IXGBE_STATUS_LAN_ID) >> IXGBE_STATUS_LAN_ID_SHIFT;
	bus->lan_id = bus->func;

	/* check for a port swap */
	reg = IXGBE_READ_REG(hw, IXGBE_FACTPS);
	if (reg & IXGBE_FACTPS_LFS)
		bus->func ^= 0x1;
}

int32_t ixmap_stop_adapter(struct ixmap_hw *hw)
{
	uint32_t reg_val;
	uint16_t i;

	/*
	 * Set the adapter_stopped flag so other driver functions stop touching
	 * the hardware
	 */
	hw->adapter_stopped = true;

	/* Disable the receive unit */
	IXGBE_WRITE_REG(hw, IXGBE_RXCTRL, 0);

	/* Clear interrupt mask to stop interrupts from being generated */
	IXGBE_WRITE_REG(hw, IXGBE_EIMC, IXGBE_IRQ_CLEAR_MASK);

	/* Clear any pending interrupts, flush previous writes */
	IXGBE_READ_REG(hw, IXGBE_EICR);

	/* Disable the transmit unit.  Each queue must be disabled. */
	for (i = 0; i < hw->mac.max_tx_queues; i++)
		IXGBE_WRITE_REG(hw, IXGBE_TXDCTL(i), IXGBE_TXDCTL_SWFLSH);

	/* Disable the receive unit by stopping each queue */
	for (i = 0; i < hw->mac.max_rx_queues; i++) {
		reg_val = IXGBE_READ_REG(hw, IXGBE_RXDCTL(i));
		reg_val &= ~IXGBE_RXDCTL_ENABLE;
		reg_val |= IXGBE_RXDCTL_SWFLSH;
		IXGBE_WRITE_REG(hw, IXGBE_RXDCTL(i), reg_val);
	}

	/* flush all queues disables */
	IXGBE_WRITE_FLUSH(hw);
	msleep(2);

	/*
	 * Prevent the PCI-E bus from from hanging by disabling PCI-E master
	 * access and verify no pending requests
	 */
	return ixmap_disable_pcie_master(hw);
}

int32_t ixmap_init_rx_addrs(struct ixmap_hw *hw)
{
	uint32_t i;
	uint32_t rar_entries = hw->mac.num_rar_entries;

	/*
	 * If the current mac address is valid, assume it is a software override
	 * to the permanent address.
	 * Otherwise, use the permanent address from the eeprom.
	 */
	if (ixmap_validate_mac_addr(hw->mac.addr) ==
	    IXGBE_ERR_INVALID_MAC_ADDR) {
		/* Get the MAC address from the RAR0 for later reference */
		hw->mac.ops.get_mac_addr(hw, hw->mac.addr);
	} else {
		/* Setup the receive address. */
		hw->mac.ops.set_rar(hw, 0, hw->mac.addr, 0, IXGBE_RAH_AV);
	}

	/* Zero out the other receive addresses. */
	for (i = 1; i < rar_entries; i++) {
		IXGBE_WRITE_REG(hw, IXGBE_RAL(i), 0);
		IXGBE_WRITE_REG(hw, IXGBE_RAH(i), 0);
	}

	/* Clear the MTA */
	IXGBE_WRITE_REG(hw, IXGBE_MCSTCTRL, hw->mac.mc_filter_type);

	for (i = 0; i < hw->mac.mcft_size; i++)
		IXGBE_WRITE_REG(hw, IXGBE_MTA(i), 0);

	ixmap_init_uta_tables(hw);

	return 0;
}

int32_t ixmap_validate_mac_addr(uint8_t *mac_addr)
{
	int32_t status = 0;

	/* Make sure it is not a multicast address */
	if (IXGBE_IS_MULTICAST(mac_addr)) {
		status = IXGBE_ERR_INVALID_MAC_ADDR;
	/* Not a broadcast address */
	} else if (IXGBE_IS_BROADCAST(mac_addr)) {
		status = IXGBE_ERR_INVALID_MAC_ADDR;
	/* Reject the zero address */
	} else if (mac_addr[0] == 0 && mac_addr[1] == 0 && mac_addr[2] == 0 &&
		   mac_addr[3] == 0 && mac_addr[4] == 0 && mac_addr[5] == 0) {
		status = IXGBE_ERR_INVALID_MAC_ADDR;
	}
	return status;
}

uint8_t ixmap_calculate_checksum(uint8_t *buffer, uint32_t length)
{
	uint32_t i;
	uint8_t sum = 0;

	if (!buffer)
		return 0;
	for (i = 0; i < length; i++)
		sum += buffer[i];

	return (uint8_t) (0 - sum);
}

int32_t ixmap_host_interface_command(struct ixmap_hw *hw,
	uint32_t *buffer, uint32_t length)
{
	uint32_t hicr, i, bi;
	uint32_t hdr_size = sizeof(struct ixmap_hic_hdr);
	uint8_t buf_len, dword_len;

	int32_t ret_val = 0;

	if (length == 0 || length & 0x3 ||
	    length > IXGBE_HI_MAX_BLOCK_BYTE_LENGTH) {
		ret_val = IXGBE_ERR_HOST_INTERFACE_COMMAND;
		goto out;
	}

	/* Check that the host interface is enabled. */
	hicr = IXGBE_READ_REG(hw, IXGBE_HICR);
	if ((hicr & IXGBE_HICR_EN) == 0) {
		ret_val = IXGBE_ERR_HOST_INTERFACE_COMMAND;
		goto out;
	}

	/* Calculate length in DWORDs */
	dword_len = length >> 2;

	/*
	 * The device driver writes the relevant command block
	 * into the ram area.
	 */
	for (i = 0; i < dword_len; i++)
		IXGBE_WRITE_REG_ARRAY(hw, IXGBE_FLEX_MNG,
				      i, IXGBE_CPU_TO_LE32(buffer[i]));

	/* Setting this bit tells the ARC that a new command is pending. */
	IXGBE_WRITE_REG(hw, IXGBE_HICR, hicr | IXGBE_HICR_C);

	for (i = 0; i < IXGBE_HI_COMMAND_TIMEOUT; i++) {
		hicr = IXGBE_READ_REG(hw, IXGBE_HICR);
		if (!(hicr & IXGBE_HICR_C))
			break;
		msleep(1);
	}

	/* Check command successful completion. */
	if (i == IXGBE_HI_COMMAND_TIMEOUT ||
	    (!(IXGBE_READ_REG(hw, IXGBE_HICR) & IXGBE_HICR_SV))) {
		ret_val = IXGBE_ERR_HOST_INTERFACE_COMMAND;
		goto out;
	}

	/* Calculate length in DWORDs */
	dword_len = hdr_size >> 2;

	/* first pull in the header so we know the buffer length */
	for (bi = 0; bi < dword_len; bi++) {
		buffer[bi] = IXGBE_READ_REG_ARRAY(hw, IXGBE_FLEX_MNG, bi);
		IXGBE_LE32_TO_CPUS(&buffer[bi]);
	}

	/* If there is any thing in data position pull it in */
	buf_len = ((struct ixmap_hic_hdr *)buffer)->buf_len;
	if (buf_len == 0)
		goto out;

	if (length < (buf_len + hdr_size)) {
		ret_val = IXGBE_ERR_HOST_INTERFACE_COMMAND;
		goto out;
	}

	/* Calculate length in DWORDs, add 3 for odd lengths */
	dword_len = (buf_len + 3) >> 2;

	/* Pull in the rest of the buffer (bi is where we left off)*/
	for (; bi <= dword_len; bi++) {
		buffer[bi] = IXGBE_READ_REG_ARRAY(hw, IXGBE_FLEX_MNG, bi);
		IXGBE_LE32_TO_CPUS(&buffer[bi]);
	}

out:
	return ret_val;
}

int32_t ixmap_set_fw_drv_ver(struct ixmap_hw *hw,
	uint8_t maj, uint8_t min, uint8_t build, uint8_t sub)
{
	struct ixmap_hic_drv_info fw_cmd;
	int i;
	int32_t ret_val = 0;

	if (hw->mac.ops.acquire_swfw_sync(hw, IXGBE_GSSR_SW_MNG_SM)
	    != 0) {
		ret_val = IXGBE_ERR_SWFW_SYNC;
		goto out;
	}

	fw_cmd.hdr.cmd = FW_CEM_CMD_DRIVER_INFO;
	fw_cmd.hdr.buf_len = FW_CEM_CMD_DRIVER_INFO_LEN;
	fw_cmd.hdr.cmd_or_resp.cmd_resv = FW_CEM_CMD_RESERVED;
	fw_cmd.port_num = (uint8_t)hw->bus.func;
	fw_cmd.ver_maj = maj;
	fw_cmd.ver_min = min;
	fw_cmd.ver_build = build;
	fw_cmd.ver_sub = sub;
	fw_cmd.hdr.checksum = 0;
	fw_cmd.hdr.checksum = ixmap_calculate_checksum((uint8_t *)&fw_cmd,
				(FW_CEM_HDR_LEN + fw_cmd.hdr.buf_len));
	fw_cmd.pad = 0;
	fw_cmd.pad2 = 0;

	for (i = 0; i <= FW_CEM_MAX_RETRIES; i++) {
		ret_val = ixmap_host_interface_command(hw, (uint32_t *)&fw_cmd,
						       sizeof(fw_cmd));
		if (ret_val != 0)
			continue;

		if (fw_cmd.hdr.cmd_or_resp.ret_status ==
		    FW_CEM_RESP_STATUS_SUCCESS)
			ret_val = 0;
		else
			ret_val = IXGBE_ERR_HOST_INTERFACE_COMMAND;

		break;
	}

	hw->mac.ops.release_swfw_sync(hw, IXGBE_GSSR_SW_MNG_SM);
out:
	return ret_val;
}

int32_t ixmap_check_mac_link(struct ixmap_hw *hw, uint32_t *speed,
	 int *link_up, int link_up_wait_to_complete)
{
	uint32_t links_reg, links_orig;
	uint32_t i;

	/* clear the old state */
	links_orig = IXGBE_READ_REG(hw, IXGBE_LINKS);

	links_reg = IXGBE_READ_REG(hw, IXGBE_LINKS);

	if (link_up_wait_to_complete) {
		for (i = 0; i < IXGBE_LINK_UP_TIME; i++) {
			if (links_reg & IXGBE_LINKS_UP) {
				*link_up = true;
				break;
			} else {
				*link_up = false;
			}
			msleep(100);
			links_reg = IXGBE_READ_REG(hw, IXGBE_LINKS);
		}
	} else {
		if (links_reg & IXGBE_LINKS_UP)
			*link_up = true;
		else
			*link_up = false;
	}

	if ((links_reg & IXGBE_LINKS_SPEED_82599) ==
	    IXGBE_LINKS_SPEED_10G_82599)
		*speed = IXGBE_LINK_SPEED_10GB_FULL;
	else if ((links_reg & IXGBE_LINKS_SPEED_82599) ==
		 IXGBE_LINKS_SPEED_1G_82599)
		*speed = IXGBE_LINK_SPEED_1GB_FULL;
	else if ((links_reg & IXGBE_LINKS_SPEED_82599) ==
		 IXGBE_LINKS_SPEED_100_82599)
		*speed = IXGBE_LINK_SPEED_100_FULL;
	else
		*speed = IXGBE_LINK_SPEED_UNKNOWN;

	return 0;
}

int32_t ixmap_disable_pcie_master(struct ixmap_hw *hw)
{
	int32_t status = 0;
	uint32_t i, poll;
	uint16_t value;

	/* Always set this bit to ensure any future transactions are blocked */
	IXGBE_WRITE_REG(hw, IXGBE_CTRL, IXGBE_CTRL_GIO_DIS);

	/* Exit if master requests are blocked */
	if (!(IXGBE_READ_REG(hw, IXGBE_STATUS) & IXGBE_STATUS_GIO) ||
	    IXGBE_REMOVED(hw->hw_addr))
		goto out;

	/* Poll for master request bit to clear */
	for (i = 0; i < IXGBE_PCI_MASTER_DISABLE_TIMEOUT; i++) {
		udelay(100);
		if (!(IXGBE_READ_REG(hw, IXGBE_STATUS) & IXGBE_STATUS_GIO))
			goto out;
	}

	/*
	 * Two consecutive resets are required via CTRL.RST per datasheet
	 * 5.2.5.3.2 Master Disable.  We set a flag to inform the reset routine
	 * of this need.  The first reset prevents new master requests from
	 * being issued by our device.  We then must wait 1usec or more for any
	 * remaining completions from the PCIe bus to trickle in, and then reset
	 * again to clear out any effects they may have had on our device.
	 */
	hw->mac.flags |= IXGBE_FLAGS_DOUBLE_RESET_REQUIRED;

	/*
	 * Before proceeding, make sure that the PCIe block does not have
	 * transactions pending.
	 */
	poll = ixmap_pcie_timeout_poll(hw);
	for (i = 0; i < poll; i++) {
		udelay(100);
		value = ixmap_read_pci_cfg_word(hw, IXGBE_PCI_DEVICE_STATUS);
		if (IXGBE_REMOVED(hw->hw_addr))
			goto out;
		if (!(value & IXGBE_PCI_DEVICE_STATUS_TRANSACTION_PENDING))
			goto out;
	}

	status = IXGBE_ERR_MASTER_REQUESTS_PENDING;

out:
	return status;
}

uint16_t ixmap_get_pcie_msix_count(struct ixmap_hw *hw)
{
	uint16_t msix_count = 1;
	uint16_t max_msix_count;
	uint16_t pcie_offset;

	switch (hw->mac.type) {
	case ixmap_mac_82599EB:
		pcie_offset = IXGBE_PCIE_MSIX_82599_CAPS;
		max_msix_count = IXGBE_MAX_MSIX_VECTORS_82599;
		break;
	default:
		return msix_count;
	}

	msix_count = ixmap_read_pci_cfg_word(hw, pcie_offset);
	if (IXGBE_REMOVED(hw->hw_addr))
		msix_count = 0;
	msix_count &= IXGBE_PCIE_MSIX_TBL_SZ_MASK;

	/* MSI-X count is zero-based in HW */
	msix_count++;

	if (msix_count > max_msix_count)
		msix_count = max_msix_count;

	return msix_count;
}

void ixmap_clear_tx_pending(struct ixmap_hw *hw)
{
	uint32_t gcr_ext, hlreg0;

	/*
	 * If double reset is not requested then all transactions should
	 * already be clear and as such there is no work to do
	 */
	if (!(hw->mac.flags & IXGBE_FLAGS_DOUBLE_RESET_REQUIRED))
		return;

	/*
	 * Set loopback enable to prevent any transmits from being sent
	 * should the link come up.  This assumes that the RXCTRL.RXEN bit
	 * has already been cleared.
	 */
	hlreg0 = IXGBE_READ_REG(hw, IXGBE_HLREG0);
	IXGBE_WRITE_REG(hw, IXGBE_HLREG0, hlreg0 | IXGBE_HLREG0_LPBK);

	/* initiate cleaning flow for buffers in the PCIe transaction layer */
	gcr_ext = IXGBE_READ_REG(hw, IXGBE_GCR_EXT);
	IXGBE_WRITE_REG(hw, IXGBE_GCR_EXT,
			gcr_ext | IXGBE_GCR_EXT_BUFFERS_CLEAR);

	/* Flush all writes and allow 20usec for all transactions to clear */
	IXGBE_WRITE_FLUSH(hw);
	udelay(20);

	/* restore previous register values */
	IXGBE_WRITE_REG(hw, IXGBE_GCR_EXT, gcr_ext);
	IXGBE_WRITE_REG(hw, IXGBE_HLREG0, hlreg0);
}

int32_t ixmap_init_uta_tables(struct ixmap_hw *hw)
{
	int i;

	for (i = 0; i < 128; i++)
		IXGBE_WRITE_REG(hw, IXGBE_UTA(i), 0);

	return 0;
}

static uint32_t ixmap_pcie_timeout_poll(struct ixmap_hw *hw)
{
	s16 devctl2;
	uint32_t pollcnt;

	devctl2 = ixmap_read_pci_cfg_word(hw, IXGBE_PCI_DEVICE_CONTROL2);
	devctl2 &= IXGBE_PCIDEVCTRL2_TIMEO_MASK;

	switch (devctl2) {
	case IXGBE_PCIDEVCTRL2_65_130ms:
		pollcnt = 1300;	 /* 130 millisec */
		break;
	case IXGBE_PCIDEVCTRL2_260_520ms:
		pollcnt = 5200;	 /* 520 millisec */
		break;
	case IXGBE_PCIDEVCTRL2_1_2s:
		pollcnt = 20000;	/* 2 sec */
		break;
	case IXGBE_PCIDEVCTRL2_4_8s:
		pollcnt = 80000;	/* 8 sec */
		break;
	case IXGBE_PCIDEVCTRL2_17_34s:
		pollcnt = 34000;	/* 34 sec */
		break;
	case IXGBE_PCIDEVCTRL2_50_100us:	/* 100 microsecs */
	case IXGBE_PCIDEVCTRL2_1_2ms:	   /* 2 millisecs */
	case IXGBE_PCIDEVCTRL2_16_32ms:	 /* 32 millisec */
	case IXGBE_PCIDEVCTRL2_16_32ms_def:     /* 32 millisec default */
	default:
		pollcnt = 800;	  /* 80 millisec minimum */
		break;
	}

	/* add 10% to spec maximum */
	return (pollcnt * 11) / 10;
}

int32_t ixmap_set_rar(struct ixmap_hw *hw,
	uint32_t index, uint8_t *addr, uint32_t vmdq, uint32_t enable_addr)
{
	uint32_t rar_low, rar_high;
	uint32_t rar_entries = hw->mac.num_rar_entries;

	/* Make sure we are using a valid rar index range */
	if (index >= rar_entries) {
		return IXGBE_ERR_INVALID_ARGUMENT;
	}

	/*
	 * HW expects these in little endian so we reverse the byte
	 * order from network order (big endian) to little endian
	 */
	rar_low = ((uint32_t)addr[0] |
		   ((uint32_t)addr[1] << 8) |
		   ((uint32_t)addr[2] << 16) |
		   ((uint32_t)addr[3] << 24));
	/*
	 * Some parts put the VMDq setting in the extra RAH bits,
	 * so save everything except the lower 16 bits that hold part
	 * of the address and the address valid bit.
	 */
	rar_high = IXGBE_READ_REG(hw, IXGBE_RAH(index));
	rar_high &= ~(0x0000FFFF | IXGBE_RAH_AV);
	rar_high |= ((uint32_t)addr[4] | ((uint32_t)addr[5] << 8));

	if (enable_addr != 0)
		rar_high |= IXGBE_RAH_AV;

	IXGBE_WRITE_REG(hw, IXGBE_RAL(index), rar_low);
	IXGBE_WRITE_REG(hw, IXGBE_RAH(index), rar_high);

	return 0;
}

int32_t ixmap_clear_vfta(struct ixmap_hw *hw)
{
	uint32_t offset;

	for (offset = 0; offset < hw->mac.vft_size; offset++)
		IXGBE_WRITE_REG(hw, IXGBE_VFTA(offset), 0);

	for (offset = 0; offset < IXGBE_VLVF_ENTRIES; offset++) {
		IXGBE_WRITE_REG(hw, IXGBE_VLVF(offset), 0);
		IXGBE_WRITE_REG(hw, IXGBE_VLVFB(offset * 2), 0);
		IXGBE_WRITE_REG(hw, IXGBE_VLVFB((offset * 2) + 1), 0);
	}

	return 0;
}

int32_t ixmap_clear_hw_cntrs(struct ixmap_hw *hw)
{
	uint16_t i = 0;

	IXGBE_READ_REG(hw, IXGBE_CRCERRS);
	IXGBE_READ_REG(hw, IXGBE_ILLERRC);
	IXGBE_READ_REG(hw, IXGBE_ERRBC);
	IXGBE_READ_REG(hw, IXGBE_MSPDC);

	for (i = 0; i < 8; i++)
		IXGBE_READ_REG(hw, IXGBE_MPC(i));

	IXGBE_READ_REG(hw, IXGBE_MLFC);
	IXGBE_READ_REG(hw, IXGBE_MRFC);
	IXGBE_READ_REG(hw, IXGBE_RLEC);
	IXGBE_READ_REG(hw, IXGBE_LXONTXC);
	IXGBE_READ_REG(hw, IXGBE_LXOFFTXC);
	IXGBE_READ_REG(hw, IXGBE_LXONRXCNT);
	IXGBE_READ_REG(hw, IXGBE_LXOFFRXCNT);

	for (i = 0; i < 8; i++) {
		IXGBE_READ_REG(hw, IXGBE_PXONTXC(i));
		IXGBE_READ_REG(hw, IXGBE_PXOFFTXC(i));
		IXGBE_READ_REG(hw, IXGBE_PXONRXCNT(i));
		IXGBE_READ_REG(hw, IXGBE_PXOFFRXCNT(i));
	}

	for (i = 0; i < 8; i++)
		IXGBE_READ_REG(hw, IXGBE_PXON2OFFCNT(i));

	IXGBE_READ_REG(hw, IXGBE_PRC64);
	IXGBE_READ_REG(hw, IXGBE_PRC127);
	IXGBE_READ_REG(hw, IXGBE_PRC255);
	IXGBE_READ_REG(hw, IXGBE_PRC511);
	IXGBE_READ_REG(hw, IXGBE_PRC1023);
	IXGBE_READ_REG(hw, IXGBE_PRC1522);
	IXGBE_READ_REG(hw, IXGBE_GPRC);
	IXGBE_READ_REG(hw, IXGBE_BPRC);
	IXGBE_READ_REG(hw, IXGBE_MPRC);
	IXGBE_READ_REG(hw, IXGBE_GPTC);
	IXGBE_READ_REG(hw, IXGBE_GORCL);
	IXGBE_READ_REG(hw, IXGBE_GORCH);
	IXGBE_READ_REG(hw, IXGBE_GOTCL);
	IXGBE_READ_REG(hw, IXGBE_GOTCH);
	IXGBE_READ_REG(hw, IXGBE_RUC);
	IXGBE_READ_REG(hw, IXGBE_RFC);
	IXGBE_READ_REG(hw, IXGBE_ROC);
	IXGBE_READ_REG(hw, IXGBE_RJC);
	IXGBE_READ_REG(hw, IXGBE_MNGPRC);
	IXGBE_READ_REG(hw, IXGBE_MNGPDC);
	IXGBE_READ_REG(hw, IXGBE_MNGPTC);
	IXGBE_READ_REG(hw, IXGBE_TORL);
	IXGBE_READ_REG(hw, IXGBE_TORH);
	IXGBE_READ_REG(hw, IXGBE_TPR);
	IXGBE_READ_REG(hw, IXGBE_TPT);
	IXGBE_READ_REG(hw, IXGBE_PTC64);
	IXGBE_READ_REG(hw, IXGBE_PTC127);
	IXGBE_READ_REG(hw, IXGBE_PTC255);
	IXGBE_READ_REG(hw, IXGBE_PTC511);
	IXGBE_READ_REG(hw, IXGBE_PTC1023);
	IXGBE_READ_REG(hw, IXGBE_PTC1522);
	IXGBE_READ_REG(hw, IXGBE_MPTC);
	IXGBE_READ_REG(hw, IXGBE_BPTC);
	for (i = 0; i < 16; i++) {
		IXGBE_READ_REG(hw, IXGBE_QPRC(i));
		IXGBE_READ_REG(hw, IXGBE_QPTC(i));
		IXGBE_READ_REG(hw, IXGBE_QBRC_L(i));
		IXGBE_READ_REG(hw, IXGBE_QBRC_H(i));
		IXGBE_READ_REG(hw, IXGBE_QBTC_L(i));
		IXGBE_READ_REG(hw, IXGBE_QBTC_H(i));
		IXGBE_READ_REG(hw, IXGBE_QPRDC(i));
	}

	return 0;
}
