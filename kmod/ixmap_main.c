#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/miscdevice.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <asm/io.h>

#include "ixmap_main.h"
#include "ixmap_type.h"
#include "ixmap_common.h"
#include "ixmap_82599.h"
#include "ixmap_eeprom.h"
#include "ixmap_dma.h"
#include "ixmap_fops.h"

static int ixmap_alloc_enid(void);
static struct ixmap_adapter *ixmap_adapter_alloc(void);
static void ixmap_adapter_free(struct ixmap_adapter *adapter);
static struct ixmap_irqdev *ixmap_irqdev_alloc(struct ixmap_adapter *adapter,
	struct msix_entry *entry);
static void ixmap_irqdev_free(struct ixmap_irqdev *irqdev);
static void ixmap_release_hw_control(struct ixmap_adapter *adapter);
static void ixmap_take_hw_control(struct ixmap_adapter *adapter);
static int ixmap_sw_init(struct ixmap_adapter *adapter);
static int ixmap_probe(struct pci_dev *pdev, const struct pci_device_id *ent);
static void ixmap_remove(struct pci_dev *pdev);
static pci_ers_result_t ixmap_io_error_detected(struct pci_dev *pdev,
	pci_channel_state_t state);
static pci_ers_result_t ixmap_io_slot_reset(struct pci_dev *pdev);
static void ixmap_io_resume(struct pci_dev *pdev);
static irqreturn_t ixmap_interrupt(int irq, void *data);
static void ixmap_write_eitr(struct ixmap_adapter *adapter, int vector);
static void ixmap_set_ivar(struct ixmap_adapter *adapter,
	s8 direction, u8 queue, u8 msix_vector);
static void ixmap_free_msix(struct ixmap_adapter *adapter);
static int ixmap_configure_msix(struct ixmap_adapter *adapter);
static void ixmap_setup_gpie(struct ixmap_adapter *adapter);
static int __init ixmap_module_init(void);
static void __exit ixmap_module_exit(void);

const char ixmap_driver_name[]	= "ixmap";
const char ixmap_driver_desc[]	= "Direct access to ixgbe device register";
const char ixmap_driver_ver[]	= "1.0";
const char *ixmap_copyright[]	= {
	"Copyright (c) 1999-2014 Intel Corporation.",
	"Copyright (c) 2009 Qualcomm Inc.",
	"Copyright (c) 2014 by Yukito Ueno <eden@sfc.wide.ad.jp>.",
};

static DEFINE_PCI_DEVICE_TABLE(ixmap_pci_tbl) = {
	{PCI_VDEVICE(INTEL, IXGBE_DEV_ID_82599_SFP)},
	/* required last entry */
	{0, }
};

MODULE_DEVICE_TABLE(pci, ixmap_pci_tbl);
static LIST_HEAD(dev_list);
static DEFINE_SEMAPHORE(dev_sem);

static struct pci_error_handlers ixmap_err_handler = {
	.error_detected	= ixmap_io_error_detected,
	.slot_reset	= ixmap_io_slot_reset,
	.resume		= ixmap_io_resume,
};

static struct pci_driver ixmap_driver = {
	.name		= ixmap_driver_name,
	.id_table	= ixmap_pci_tbl,
	.probe		= ixmap_probe,
	.remove		= ixmap_remove,
	.err_handler	= &ixmap_err_handler,
};

uint16_t ixmap_read_pci_cfg_word(struct ixgbe_hw *hw, uint32_t reg)
{
	struct ixmap_adapter *adapter = hw->back;
	uint16_t value;

	if (unlikely(!hw->hw_addr))
		return IXGBE_FAILED_READ_CFG_WORD;

	pci_read_config_word(adapter->pdev, reg, &value);

	return value;
}

int ixmap_adapter_inuse(struct ixmap_adapter *adapter)
{
	unsigned ref = atomic_read(&adapter->refcount);
	if (ref == 1)
		return 0;
	return 1;
}

void ixmap_adapter_get(struct ixmap_adapter *adapter)
{
	atomic_inc(&adapter->refcount);
	return;
}

void ixmap_adapter_put(struct ixmap_adapter *adapter)
{
	atomic_dec(&adapter->refcount);
	return;
}

int ixmap_irqdev_inuse(struct ixmap_irqdev *irqdev)
{
	unsigned ref = atomic_read(&irqdev->refcount);
	if (ref == 1)
		return 0;
	return 1;
}

void ixmap_irqdev_get(struct ixmap_irqdev *irqdev)
{
        atomic_inc(&irqdev->refcount);
	return;
}

void ixmap_irqdev_put(struct ixmap_irqdev *irqdev)
{
        atomic_dec(&irqdev->refcount);
	return;
}

static int ixmap_alloc_enid(void)
{
	struct ixmap_adapter *adapter;
	unsigned int id = 0;

	list_for_each_entry(adapter, &dev_list, list) {
		id++;
	}

	return id;
}

static struct ixmap_adapter *ixmap_adapter_alloc(void)
{
	struct ixmap_adapter *adapter;

	adapter = kzalloc(sizeof(struct ixmap_adapter), GFP_KERNEL);
	if (!adapter){
		return NULL;
	}

	adapter->hw = kzalloc(sizeof(struct ixgbe_hw), GFP_KERNEL);
	if(!adapter->hw){
		return NULL;
	}

	atomic_set(&adapter->refcount, 1);
	sema_init(&adapter->sem,1);

	/* Add to global list */
	down(&dev_sem);
	adapter->id = ixmap_alloc_enid();
	list_add(&adapter->list, &dev_list);
	up(&dev_sem);

	return adapter;
}

static void ixmap_adapter_free(struct ixmap_adapter *adapter)
{
	down(&dev_sem);
	list_del(&adapter->list);
	up(&dev_sem);

	kfree(adapter->hw);
	kfree(adapter);
	return;
}

static struct ixmap_irqdev *ixmap_irqdev_alloc(struct ixmap_adapter *adapter,
	struct msix_entry *entry)
{
	struct ixmap_irqdev *irqdev;

	/* XXX: To support NUMA-aware allocation, use kzalloc_node() */
	irqdev = kzalloc(sizeof(struct ixmap_irqdev), GFP_KERNEL);
	if(!irqdev){
		return NULL;
	}

	irqdev->adapter = adapter;
	irqdev->msix_entry = entry;
	atomic_set(&irqdev->refcount, 1);
	atomic_set(&irqdev->count_interrupt, 0);
	sema_init(&irqdev->sem, 1);
	init_waitqueue_head(&irqdev->read_wait);
	list_add(&irqdev->list, &adapter->irqdev_list);

	return irqdev;
}

static void ixmap_irqdev_free(struct ixmap_irqdev *irqdev)
{
	list_del(&irqdev->list);
	kfree(irqdev);
	return;
}

static void ixmap_release_hw_control(struct ixmap_adapter *adapter)
{
	struct ixgbe_hw *hw = adapter->hw;
	u32 ctrl_ext;

	/* Let firmware take over control of h/w */
	ctrl_ext = IXGBE_READ_REG(hw, IXGBE_CTRL_EXT);
	IXGBE_WRITE_REG(hw, IXGBE_CTRL_EXT,
			ctrl_ext & ~IXGBE_CTRL_EXT_DRV_LOAD);
}

static void ixmap_take_hw_control(struct ixmap_adapter *adapter)
{
	struct ixgbe_hw *hw = adapter->hw;
	u32 ctrl_ext;

	/* Let firmware know the driver has taken over */
	ctrl_ext = IXGBE_READ_REG(hw, IXGBE_CTRL_EXT);
	IXGBE_WRITE_REG(hw, IXGBE_CTRL_EXT,
			ctrl_ext | IXGBE_CTRL_EXT_DRV_LOAD);
}

static int ixmap_sw_init(struct ixmap_adapter *adapter)
{
	struct ixgbe_hw *hw = adapter->hw;
	struct pci_dev *pdev = adapter->pdev;

	/* PCI config space info */
	hw->vendor_id = pdev->vendor;
	hw->device_id = pdev->device;
	hw->revision_id = pdev->revision;
	hw->subsystem_vendor_id = pdev->subsystem_vendor;
	hw->subsystem_device_id = pdev->subsystem_device;

	/* Setup hw api */
	hw->mac.type  = ixgbe_mac_82599EB;
	ixgbe_init_ops_82599(hw);

	return 0;
}

static int ixmap_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct ixmap_adapter *adapter;
	struct ixgbe_hw *hw;
	u16 offset = 0, eeprom_verh = 0, eeprom_verl = 0;
	u16 eeprom_cfg_blkh = 0, eeprom_cfg_blkl = 0;
	u32 etrack_id;
	u16 build, major, patch;
	u8 part_str[IXGBE_PBANUM_LENGTH];
	int pci_using_dac, err;

	pr_info("probing device %s\n", pci_name(pdev));

	err = pci_enable_device_mem(pdev);
	if (err)
		return err;

	/*
	 * first argument of dma_set_mask(), &pdev->dev should be changed
	 * on Linux kernel version.
	 * Original ixgbe driver hides this issue with pci_dev_to_dev() macro.
	 */
	if (!dma_set_mask(&pdev->dev, DMA_BIT_MASK(64)) &&
		!dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(64))){
		pci_using_dac = 1;
	}else{
		err = dma_set_mask(&pdev->dev, DMA_BIT_MASK(32));
		if(err){
			err = dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(32));
			if(err){
				pr_err("No usable DMA configuration, aborting\n");
				goto err_dma;
			}
		}
		pci_using_dac = 0;
	}

	err = pci_request_selected_regions(pdev, pci_select_bars(pdev,
					   IORESOURCE_MEM), ixmap_driver_name);
	if (err) {
		pr_err("pci_request_selected_regions failed 0x%x\n", err);
		goto err_pci_reg;
	}

	adapter = ixmap_adapter_alloc();
	if (!adapter) {
		err = -ENOMEM;
		goto err_alloc;
	}
	hw = adapter->hw;

	pci_set_drvdata(pdev, adapter);
	adapter->pdev = pdev;
	hw->back = adapter;

	pci_set_master(pdev);
	pci_save_state(pdev);

	adapter->iobase = pci_resource_start(pdev, 0);
	adapter->iolen  = pci_resource_len(pdev, 0);

	if(pci_using_dac){
		adapter->dma_mask = DMA_BIT_MASK(64);
	}else{
		adapter->dma_mask = DMA_BIT_MASK(32);
	}

	/* setup for userland pci register access */
	INIT_LIST_HEAD(&adapter->areas);
	hw->hw_addr = ixmap_dma_map_iobase(adapter);
	if (!hw->hw_addr) {
		err = -EIO;
		goto err_ioremap;
	}

	/* setup for userland interrupt notification */
	INIT_LIST_HEAD(&adapter->irqdev_list);

	/* SOFTWARE INITIALIZATION */
	err = ixmap_sw_init(adapter);
	if (err)
		goto err_sw_init;

	/* reset_hw fills in the perm_addr as well */
	err = hw->mac.ops.reset_hw(hw);
	if (err) {
		pr_err("HW Init failed: %d\n", err);
		goto err_sw_init;
	}

	/* make sure the EEPROM is good */
	if (hw->eeprom.ops.validate_checksum &&
	    (hw->eeprom.ops.validate_checksum(hw, NULL) < 0)) {
		pr_err("The EEPROM Checksum Is Not Valid\n");
		err = -EIO;
		goto err_sw_init;
	}

	if (ixgbe_validate_mac_addr(hw->mac.perm_addr)) {
		pr_err("invalid MAC address\n");
		err = -EIO;
		goto err_sw_init;
	}

       /*
	 * Save off EEPROM version number and Option Rom version which
	 * together make a unique identify for the eeprom
	 */
	hw->eeprom.ops.read(hw, 0x2e, &eeprom_verh);
	hw->eeprom.ops.read(hw, 0x2d, &eeprom_verl);
	etrack_id = (eeprom_verh << 16) | eeprom_verl;

	hw->eeprom.ops.read(hw, 0x17, &offset);

	/* Make sure offset to SCSI block is valid */
	if (!(offset == 0x0) && !(offset == 0xffff)) {
		hw->eeprom.ops.read(hw, offset + 0x84, &eeprom_cfg_blkh);
		hw->eeprom.ops.read(hw, offset + 0x83, &eeprom_cfg_blkl);

		/* Only display Option Rom if exist */
		if (eeprom_cfg_blkl && eeprom_cfg_blkh) {
			major = eeprom_cfg_blkl >> 8;
			build = (eeprom_cfg_blkl << 8) | (eeprom_cfg_blkh >> 8);
			patch = eeprom_cfg_blkh & 0x00ff;

			snprintf(adapter->eeprom_id, sizeof(adapter->eeprom_id),
				 "0x%08x, %d.%d.%d", etrack_id, major, build,
				 patch);
		} else {
			snprintf(adapter->eeprom_id, sizeof(adapter->eeprom_id),
				 "0x%08x", etrack_id);
		}
	} else {
		snprintf(adapter->eeprom_id, sizeof(adapter->eeprom_id),
			 "0x%08x", etrack_id);
	}

	/* reset the hardware with the new settings */
	err = hw->mac.ops.start_hw(hw);
	if (err == IXGBE_ERR_EEPROM_VERSION) {
		/* We are running on a pre-production device, log a warning */
		pr_info("This device is a pre-production adapter/LOM.\n");
	}

	/* power down the optics for 82599 SFP+ fiber */
	if (hw->mac.ops.disable_tx_laser)
		hw->mac.ops.disable_tx_laser(hw);

	/* First try to read PBA as a string */
	err = ixgbe_read_pba_string_generic(hw, part_str, IXGBE_PBANUM_LENGTH);
	if (err)
		strncpy(part_str, "Unknown", IXGBE_PBANUM_LENGTH);
	if (hw->phy.sfp_type != ixgbe_sfp_type_not_present)
		pr_info("MAC: %d, PHY: %d, SFP+: %d, PBA No: %s\n",
		       hw->mac.type, hw->phy.type, hw->phy.sfp_type, part_str);

	pr_info("%02x:%02x:%02x:%02x:%02x:%02x\n",
		   hw->mac.perm_addr[0], hw->mac.perm_addr[1],
		   hw->mac.perm_addr[2], hw->mac.perm_addr[3],
		   hw->mac.perm_addr[4], hw->mac.perm_addr[5]);

	/* firmware requires blank driver version */
	if (hw->mac.ops.set_fw_drv_ver)
		hw->mac.ops.set_fw_drv_ver(hw, 0xFF, 0xFF, 0xFF, 0xFF);

	pr_info("device[%u] %s initialized\n", adapter->id, pci_name(pdev));
	pr_info("device[%u] %s dma mask %llx\n", adapter->id, pci_name(pdev),
		(unsigned long long) pdev->dma_mask);

	err = ixmap_miscdev_register(adapter);
	if(err < 0)
		goto err_miscdev_register;

	return 0;

err_miscdev_register:
err_sw_init:
	ixmap_dma_unmap_all(adapter);
err_ioremap:
	ixmap_adapter_free(adapter);
err_alloc:
	pci_release_selected_regions(pdev, pci_select_bars(pdev, IORESOURCE_MEM));
err_pci_reg:
err_dma:

	return err;
}

static void ixmap_remove(struct pci_dev *pdev)
{
	struct ixmap_adapter *adapter = pci_get_drvdata(pdev);

	if(adapter->up){
		ixmap_down(adapter);
	}

	ixmap_miscdev_deregister(adapter);
	ixmap_dma_unmap_all(adapter);
	pci_release_selected_regions(pdev, pci_select_bars(pdev, IORESOURCE_MEM));
	pci_disable_device(pdev);

	pr_info("device[%u] %s removed\n", adapter->id, pci_name(pdev));

	ixmap_adapter_put(adapter);
	ixmap_adapter_free(adapter);
}

static pci_ers_result_t ixmap_io_error_detected(struct pci_dev *pdev,
	pci_channel_state_t state)
{
	// FIXME: Do something
	return PCI_ERS_RESULT_NEED_RESET;
}

static pci_ers_result_t ixmap_io_slot_reset(struct pci_dev *pdev)
{
	// FIXME: Do something
	return PCI_ERS_RESULT_RECOVERED;
}

static void ixmap_io_resume(struct pci_dev *pdev)
{
	// FIXME: Do something
	return;
}

static irqreturn_t ixmap_interrupt(int irq, void *data)
{
	struct ixmap_irqdev *irqdev = data;

	/* 
	 * We setup EIAM such that interrupts are auto-masked (disabled).
	 * User-space will re-enable them.
	 */

	atomic_inc(&irqdev->count_interrupt);
	wake_up_interruptible(&irqdev->read_wait);
	return IRQ_HANDLED;
}

static void ixmap_write_eitr(struct ixmap_adapter *adapter, int vector)
{
	struct ixgbe_hw *hw = adapter->hw;
	u32 itr_reg = adapter->num_interrupt_rate & IXGBE_MAX_EITR;

	/*
	 * set the WDIS bit to not clear the timer bits and cause an
	 * immediate assertion of the interrupt
	 */
	itr_reg |= IXGBE_EITR_CNT_WDIS;
	IXGBE_WRITE_REG(hw, IXGBE_EITR(vector), itr_reg);
}

static void ixmap_set_ivar(struct ixmap_adapter *adapter,
	s8 direction, u8 queue, u8 msix_vector)
{
	u32 ivar, index;
	struct ixgbe_hw *hw = adapter->hw;

	/* tx or rx causes */
	msix_vector |= IXGBE_IVAR_ALLOC_VAL;
	index = ((16 * (queue & 1)) + (8 * direction));
	ivar = IXGBE_READ_REG(hw, IXGBE_IVAR(queue >> 1));
	ivar &= ~(0xFF << index);
	ivar |= (msix_vector << index);
	IXGBE_WRITE_REG(hw, IXGBE_IVAR(queue >> 1), ivar);
}

static void ixmap_free_msix(struct ixmap_adapter *adapter)
{
	struct ixmap_irqdev *irqdev, *next;

	list_for_each_entry_safe(irqdev, next, &adapter->irqdev_list, list) {
		free_irq(irqdev->msix_entry->vector, irqdev);
		wake_up_interruptible(&irqdev->read_wait);
		ixmap_irqdev_misc_deregister(irqdev);
		ixmap_irqdev_free(irqdev);
	}

	pci_disable_msix(adapter->pdev);
	kfree(adapter->msix_entries);
	adapter->msix_entries = NULL;
	adapter->num_q_vectors = 0;

	return;
}

static int ixmap_configure_msix(struct ixmap_adapter *adapter)
{
	int vector = 0, vector_num, queue_idx, err;
	struct ixgbe_hw *hw = adapter->hw;
	struct msix_entry *entry;
	struct ixmap_irqdev *irqdev;

	vector_num = adapter->num_rx_queues + adapter->num_tx_queues;
	if(vector_num > hw->mac.max_msix_vectors){
		goto err_num_msix_vectors;
	}
	pr_info("required vector num = %d\n", vector_num);

	adapter->msix_entries = kcalloc(vector_num, sizeof(struct msix_entry), GFP_KERNEL);
	if (!adapter->msix_entries) {
		goto err_allocate_msix_entries;
	}

	for (vector = 0; vector < vector_num; vector++){
		adapter->msix_entries[vector].entry = vector;
	}

	err = vector_num;
	while (err){
		/* err == number of vectors we should try again with */ 
	      	err = pci_enable_msix(adapter->pdev, adapter->msix_entries, err);

		if(err < 0){
		       	/* failed to allocate enough msix vector */
			goto err_pci_enable_msix;
	       	}
	}
	adapter->num_q_vectors = vector_num;
	
	for(queue_idx = 0, vector = 0; queue_idx < adapter->num_rx_queues;
	queue_idx++, vector++){
		entry = &adapter->msix_entries[vector];

		irqdev = ixmap_irqdev_alloc(adapter, entry);
		if(!irqdev){
			goto err_alloc_irqdev;
		}

		err = ixmap_irqdev_misc_register(irqdev, adapter->id,
			IRQDEV_RX, queue_idx);
		if(err < 0){
			goto err_misc_register;
		}

		err = request_irq(entry->vector, &ixmap_interrupt,
				0, pci_name(adapter->pdev), irqdev);
		if(err){
			goto err_request_irq;
		}

		/* set RX queue interrupt */
		ixmap_set_ivar(adapter, 0, queue_idx, vector);
		ixmap_write_eitr(adapter, vector);

		pr_info("irq device registered as %s\n", irqdev->miscdev.name);
	}

	for(queue_idx = 0; queue_idx < adapter->num_tx_queues;
	queue_idx++, vector++){
		entry = &adapter->msix_entries[vector];

		irqdev = ixmap_irqdev_alloc(adapter, entry);
		if(!irqdev){
			goto err_alloc_irqdev;
		}

		err = ixmap_irqdev_misc_register(irqdev, adapter->id,
			IRQDEV_TX, queue_idx);
		if(err < 0){
			goto err_misc_register;
		}

		err = request_irq(entry->vector, &ixmap_interrupt,
			0, pci_name(adapter->pdev), irqdev);
		if(err){
			goto err_request_irq;
		}

		/* set TX queue interrupt */
		ixmap_set_ivar(adapter, 1, queue_idx, vector);
		ixmap_write_eitr(adapter, vector);

		pr_info("irq device registered as %s\n", irqdev->miscdev.name);
	}

	return 0;

err_request_irq:
	wake_up_interruptible(&irqdev->read_wait);
	ixmap_irqdev_misc_deregister(irqdev);
err_misc_register:
	ixmap_irqdev_free(irqdev);
err_alloc_irqdev:
	ixmap_free_msix(adapter);
	return -1;

err_pci_enable_msix:
	pci_disable_msix(adapter->pdev);
	kfree(adapter->msix_entries);
	adapter->msix_entries = NULL;
err_allocate_msix_entries:
err_num_msix_vectors:
	return -1;
}

static void ixmap_setup_gpie(struct ixmap_adapter *adapter)
{
	struct ixgbe_hw *hw = adapter->hw;
	uint32_t eiac, gpie;

	/*
	 * use EIAM to auto-mask when MSI-X interrupt is asserted
	 * this saves a register write for every interrupt
	 */
	IXGBE_WRITE_REG(hw, IXGBE_EIAM_EX(0), 0xFFFFFFFF);
	IXGBE_WRITE_REG(hw, IXGBE_EIAM_EX(1), 0xFFFFFFFF);

	/* set up to autoclear timer, and the vectors */
	eiac = IXGBE_EIMS_RTX_QUEUE;
	IXGBE_WRITE_REG(hw, IXGBE_EIAC, eiac);

	gpie = IXGBE_GPIE_MSIX_MODE | IXGBE_GPIE_PBA_SUPPORT | IXGBE_GPIE_OCD;
	gpie |= IXGBE_GPIE_EIAME;
	IXGBE_WRITE_REG(hw, IXGBE_GPIE, gpie);

	return;
}

int ixmap_up(struct ixmap_adapter *adapter)
{
	struct ixgbe_hw *hw = adapter->hw;

	ixmap_take_hw_control(adapter);
	ixmap_setup_gpie(adapter);

	/* set up msix interupt */
	if(ixmap_configure_msix(adapter) < 0){
		pr_err("failed to allocate MSI-X\n");
	}

	/* enable the optics for 82599 SFP+ fiber */
	if (hw->mac.ops.enable_tx_laser){
		hw->mac.ops.enable_tx_laser(hw);
		pr_info("enabled tx laser\n");
	}

	if (hw->mac.ops.setup_link){
		hw->mac.ops.setup_link(hw, IXGBE_LINK_SPEED_10GB_FULL, true);
		pr_info("link setup complete\n");
	}

	/* Setup flow control: Though we don't support flow control */
	ixgbe_setup_fc(hw);

	/* clear any pending interrupts, may auto mask */
	IXGBE_READ_REG(hw, IXGBE_EICR);

	/* User space application enables interrupts after */
	adapter->up = 1;

	return 0;
}

int ixmap_down(struct ixmap_adapter *adapter)
{
	struct ixgbe_hw *hw = adapter->hw;
	int vector;

	/* Disable Interrupts */
	IXGBE_WRITE_REG(hw, IXGBE_EIMC, 0xFFFF0000);
	IXGBE_WRITE_REG(hw, IXGBE_EIMC_EX(0), ~0);
	IXGBE_WRITE_REG(hw, IXGBE_EIMC_EX(1), ~0);

	IXGBE_WRITE_FLUSH(hw);

	for(vector = 0; vector < adapter->num_q_vectors; vector++){
		synchronize_irq(adapter->msix_entries[vector].vector);
	}

	/* Disable Transmits */
	IXGBE_WRITE_REG(hw, IXGBE_TXDCTL(0), IXGBE_TXDCTL_SWFLSH);

	/* Disable the Tx DMA engine on 82599 and X540 */
	IXGBE_WRITE_REG(hw, IXGBE_DMATXCTL, (IXGBE_READ_REG(hw, IXGBE_DMATXCTL) & ~IXGBE_DMATXCTL_TE));

	/* Reset hardware */
	if (!pci_channel_offline(adapter->pdev)){
		ixmap_reset(adapter);
	}

	/* power down the optics for 82599 SFP+ fiber */
	if (hw->mac.ops.disable_tx_laser){
		hw->mac.ops.disable_tx_laser(hw);
	}

	/* free irqs */
	ixmap_free_msix(adapter);

	adapter->num_interrupt_rate = 0;
	adapter->num_rx_queues = 0;
	adapter->num_tx_queues = 0;

	ixmap_release_hw_control(adapter);
	adapter->up = 0;
	return 0;
}

void ixmap_reset(struct ixmap_adapter *adapter)
{
	struct ixgbe_hw *hw = adapter->hw;
	int err;

	err = hw->mac.ops.init_hw(hw);
	switch (err) {
	case 0:
	case IXGBE_ERR_SFP_NOT_PRESENT:
	case IXGBE_ERR_SFP_NOT_SUPPORTED:
		break;
	case IXGBE_ERR_MASTER_REQUESTS_PENDING:
		pr_err("master disable timed out\n");
		break;
	case IXGBE_ERR_EEPROM_VERSION:
		/* We are running on a pre-production device, log a warning */
		pr_err("This device is a pre-production adapter/LOM. "
			   "Please be aware there may be issues associated "
			   "with your hardware.  If you are experiencing "
			   "problems please contact your Intel or hardware "
			   "representative who provided you with this "
			   "hardware.\n");
		break;
	default:
		pr_err("Hardware Error: %d\n", err);
	}

	return;
}

static int __init ixmap_module_init(void)
{
	int err;

	pr_info("%s - version %s\n",
		ixmap_driver_desc, ixmap_driver_ver);
	pr_info("%s\n", ixmap_copyright[0]);
	pr_info("%s\n", ixmap_copyright[1]);
	pr_info("%s\n", ixmap_copyright[2]);

	err = pci_register_driver(&ixmap_driver);
	return err;
}

static void __exit ixmap_module_exit(void)
{
	pci_unregister_driver(&ixmap_driver);
}

module_init(ixmap_module_init);
module_exit(ixmap_module_exit);
MODULE_AUTHOR("Yukito Ueno <eden@sfc.wide.ad.jp>");
MODULE_DESCRIPTION("Direct access to ixgbe device register");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
