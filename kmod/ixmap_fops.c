#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/ioport.h>
#include <linux/poll.h>
#include <linux/sysctl.h>
#include <linux/wait.h>
#include <linux/miscdevice.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/pci.h>

#include "ixmap_type.h"
#include "ixmap_main.h"
#include "ixmap_dma.h"
#include "ixmap_fops.h"

static int ixmap_cmd_up(struct ixmap_adapter *adapter, void __user *argp);
static int ixmap_cmd_down(struct ixmap_adapter *adapter, unsigned long arg);
static int ixmap_cmd_reset(struct ixmap_adapter *adapter, unsigned long arg);
static int ixmap_cmd_check_link(struct ixmap_adapter *adapter, void __user *argp);
static int ixmap_cmd_info(struct ixmap_adapter *adapter, void __user *argp);
static int ixmap_cmd_map(struct ixmap_adapter *adapter, void __user *argp);
static int ixmap_cmd_unmap(struct ixmap_adapter *adapter, void __user *argp);
static ssize_t ixmap_read(struct file *file, char __user *buf,
	size_t count, loff_t *pos);
static ssize_t ixmap_write(struct file *file, const char __user *buf,
	size_t count, loff_t *pos);
static int ixmap_open(struct inode *inode, struct file * file);
static int ixmap_close(struct inode *inode, struct file *file);
static void ixmap_vm_open(struct vm_area_struct *vma);
static void ixmap_vm_close(struct vm_area_struct *vma);
static int ixmap_vm_fault(struct vm_area_struct *area, struct vm_fault *fdata);
static int ixmap_mmap(struct file *file, struct vm_area_struct *vma);
static long ixmap_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static unsigned int ixmap_irqdev_poll(struct file *file, poll_table *wait);
static ssize_t ixmap_irqdev_read(struct file * file, char __user * buf,
	size_t count, loff_t *pos);
static int ixmap_irqdev_open(struct inode *inode, struct file *file);
static int ixmap_irqdev_close(struct inode *inode, struct file *file);
static int ixmap_irqdev_cmd_info(struct ixmap_irqdev *irqdev,
	void __user *argp);
static long ixmap_irqdev_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg);

static struct file_operations ixmap_fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.read		= ixmap_read,
	.write		= ixmap_write,
	.open		= ixmap_open,
	.release	= ixmap_close,
	.mmap		= ixmap_mmap,
	.unlocked_ioctl	= ixmap_ioctl,
};

static struct file_operations ixmap_irqdev_fops = {
	.owner		= THIS_MODULE,
	.llseek		= no_llseek,
	.read		= ixmap_irqdev_read,
	.write		= ixmap_write,
	.open		= ixmap_irqdev_open,
	.release	= ixmap_irqdev_close,
	.poll		= ixmap_irqdev_poll,
	.unlocked_ioctl	= ixmap_irqdev_ioctl,
};

static struct vm_operations_struct ixmap_mmap_ops = {
	.open		= ixmap_vm_open,
	.close		= ixmap_vm_close,
	.fault		= ixmap_vm_fault
};

int ixmap_miscdev_register(struct ixmap_adapter *adapter)
{
	char *miscdev_name;
	int err;

	miscdev_name = kmalloc(MISCDEV_NAME_SIZE, GFP_KERNEL);
	if(!miscdev_name){
		goto err_alloc_name;
	}
	snprintf(miscdev_name, MISCDEV_NAME_SIZE, "ixgbe%d", adapter->id);

	adapter->miscdev.minor = MISC_DYNAMIC_MINOR;
	adapter->miscdev.name = miscdev_name;
	adapter->miscdev.fops = &ixmap_fops;
	err = misc_register(&adapter->miscdev);
	if (err) {
		pr_err("failed to register misc device\n");
		goto err_misc_register;
	}

	pr_info("misc device registered as %s\n", adapter->miscdev.name);
	return 0;

err_misc_register:
        kfree(adapter->miscdev.name);
err_alloc_name:
	return -1;
}

void ixmap_miscdev_deregister(struct ixmap_adapter *adapter)
{
	misc_deregister(&adapter->miscdev);

	pr_info("misc device %s unregistered\n", adapter->miscdev.name);
	kfree(adapter->miscdev.name);

	return;
}

static int ixmap_cmd_up(struct ixmap_adapter *adapter, void __user *argp)
{
	struct ixmap_up_req req;
	int err = 0;

	if(adapter->up){
		return -EALREADY;
	}

	if (copy_from_user(&req, argp, sizeof(req)))
		return -EFAULT;

	pr_info("open req\n");

	if(req.num_interrupt_rate > IXGBE_MAX_EITR){
		return -EINVAL;
	}

	if(req.num_rx_queues > IXGBE_MAX_RSS_INDICES){
		return -EINVAL;
	}

	if(req.num_tx_queues > IXGBE_MAX_RSS_INDICES){
		return -EINVAL;
	}

	adapter->num_interrupt_rate = req.num_interrupt_rate;
	adapter->num_rx_queues = req.num_rx_queues;
	adapter->num_tx_queues = req.num_tx_queues;

	err = ixmap_up(adapter);
	if (err){
		goto err_up_complete;
	}

	return 0;

err_up_complete:
	ixmap_down(adapter);

	return err;
}

static int ixmap_cmd_down(struct ixmap_adapter *adapter,
	unsigned long arg)
{
	if(!adapter->up){
		return -EALREADY;
	}

	pr_info("down req\n");
	ixmap_down(adapter);

	return 0;
}

static int ixmap_cmd_reset(struct ixmap_adapter *adapter,
	unsigned long arg)
{
	if(!adapter->up){
		return 0;
	}

	pr_info("reset req\n");
	ixmap_reset(adapter);

	return 0;
}

static int ixmap_cmd_check_link(struct ixmap_adapter *adapter,
	void __user *argp)
{
	struct ixmap_hw *hw = adapter->hw;
	struct ixmap_link_req req;
	int err = 0, flush = 0;
       	int link_up;
	u32 link_speed = 0;

	if (!adapter->up)
		return -EAGAIN;

	if (hw->mac.ops.check_link) {
		hw->mac.ops.check_link(hw, &link_speed, &link_up, false);
	} else {
		/* always assume link is up, if no check link function */
		link_speed = IXGBE_LINK_SPEED_10GB_FULL;
		link_up = true;
	}

	pr_info("check_link req speed %u up %u\n", link_speed, link_up);

	if (link_up) {
		if (!adapter->link_speed) {
			adapter->link_speed  = link_speed;
			adapter->link_duplex = 2;

			pr_info("link is up %d mbps %s\n", adapter->link_speed,
				adapter->link_duplex == 2 ?  "Full Duplex" : "Half Duplex");
		}
	} else {
		if (adapter->link_speed) {
			adapter->link_speed  = 0;
			adapter->link_duplex = 0;

			/* We've lost the link, so the controller stops DMA,
			 * but we've got queued Tx/Rx work that's never going
			 * to get done. Tell the app to flush RX */
			flush = 1;

			pr_info("link is down\n");
		}
	}

	req.speed   = adapter->link_speed;
	req.duplex  = adapter->link_duplex;
	req.flush   = flush;

	if (copy_to_user(argp, &req, sizeof(req)))
		err = -EFAULT;

	return err;
}

static int ixmap_cmd_info(struct ixmap_adapter *adapter,
	void __user *argp)
{
	struct ixmap_info_req req;
	struct ixmap_hw *hw = adapter->hw;
	int err = 0;

	req.mmio_base = adapter->iobase;
	req.mmio_size = adapter->iolen;

	memcpy(req.mac_addr, hw->mac.perm_addr, ETH_ALEN);
	req.mac_type = hw->mac.type;
	req.phy_type = hw->phy.type;

	req.num_interrupt_rate = adapter->num_interrupt_rate;
	req.max_interrupt_rate = IXGBE_MAX_EITR;

	req.num_rx_queues = adapter->num_rx_queues;
	req.num_tx_queues = adapter->num_tx_queues;

	/* Currently we support only RX/TX RSS mode */
	req.max_rx_queues = IXGBE_MAX_RSS_INDICES;
	req.max_tx_queues = IXGBE_MAX_RSS_INDICES;

	req.max_msix_vectors = hw->mac.max_msix_vectors;

	if (copy_to_user(argp, &req, sizeof(req))) {
		err = -EFAULT;
		goto out;
	}

out:
	return err;
}

static int ixmap_cmd_map(struct ixmap_adapter *adapter,
	void __user *argp)
{
	struct ixmap_map_req req;
	unsigned long addr_dma;

	if (copy_from_user(&req, argp, sizeof(req)))
		return -EFAULT;

	if (!req.size)
		return -EINVAL;

	addr_dma = ixmap_dma_map(adapter, req.addr_virtual,
			req.size, req.cache);
	if(!addr_dma)
		return -EFAULT;

	req.addr_dma = addr_dma;

	if (copy_to_user(argp, &req, sizeof(req))) {
		ixmap_dma_unmap(adapter, req.addr_dma);
		return -EFAULT;
	}

	return 0;
}

static int ixmap_cmd_unmap(struct ixmap_adapter *adapter,
	void __user *argp)
{
	struct ixmap_unmap_req req;
	int ret;

	if (copy_from_user(&req, argp, sizeof(req)))
		return -EFAULT;

	ret = ixmap_dma_unmap(adapter, req.addr_dma);
	if(ret != 0)
		return ret;

	return 0;
}

static ssize_t ixmap_read(struct file * file, char __user * buf,
	size_t count, loff_t *pos)
{
	return 0;
}

static ssize_t ixmap_write(struct file * file, const char __user * buf,
			     size_t count, loff_t *pos)
{
	return 0;
}

static int ixmap_open(struct inode *inode, struct file * file)
{
	struct ixmap_adapter *adapter;
	struct miscdevice *miscdev = file->private_data;
	int err;

	adapter = container_of(miscdev, struct ixmap_adapter, miscdev);
	pr_info("open req miscdev=%p\n", miscdev);

	down(&adapter->sem);

	// Only one process is alowed to open
	if (ixmap_adapter_inuse(adapter)) {
		err = -EBUSY;
		goto out;
	}

	ixmap_adapter_get(adapter);
	file->private_data = adapter;
	err = 0;

out:
	up(&adapter->sem);
	return err;
}

static int ixmap_close(struct inode *inode, struct file *file)
{
	struct ixmap_adapter *adapter = file->private_data;
	if (!adapter)
		return 0;

	pr_info("close adapter=%p\n", adapter);

	down(&adapter->sem);
	ixmap_cmd_down(adapter, 0);
	up(&adapter->sem);

	ixmap_adapter_put(adapter);
	return 0;
}

static void ixmap_vm_open(struct vm_area_struct *vma)
{
}

static void ixmap_vm_close(struct vm_area_struct *vma)
{
}

static int ixmap_vm_fault(struct vm_area_struct *area, struct vm_fault *fdata)
{
	return VM_FAULT_SIGBUS;
}

static int ixmap_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct ixmap_adapter *adapter = file->private_data;
	struct ixmap_dma_area *area;

	unsigned long start = vma->vm_start;
	unsigned long size  = vma->vm_end - vma->vm_start;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long pfn;

	if (!adapter)
		return -ENODEV;

	pr_info("mmap adapter %p start %lu size %lu\n", adapter, start, size);

	/* Currently no area used except offset=0 for pci registers */
	if(offset != 0)
		return -EINVAL;

	area = ixmap_dma_area_lookup(adapter, adapter->iobase);
	if (!area)
		return -ENOENT;

	// We do not do partial mappings, sorry
	if (area->size != size)
		return -EOVERFLOW;

	pfn = area->addr_dma >> PAGE_SHIFT;

	switch (area->cache) {
	case IXGBE_DMA_CACHE_DISABLE:
		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
		break;

	case IXGBE_DMA_CACHE_WRITECOMBINE:
		#ifdef pgprot_writecombine
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
		#endif
		break;

	default:
		/* Leave as is */
		break;
	}

	if (remap_pfn_range(vma, start, pfn, size, vma->vm_page_prot))
		return -EAGAIN;

	vma->vm_ops = &ixmap_mmap_ops;
	return 0;
}

static long ixmap_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct ixmap_adapter *adapter = file->private_data;
	void __user * argp = (void __user *) arg;
	int err;

	if(!adapter)
		return -EBADFD;

	down(&adapter->sem);

	switch (cmd) {
	case IXMAP_INFO:
		err = ixmap_cmd_info(adapter, argp);
		break;

	case IXMAP_UP:
		err = ixmap_cmd_up(adapter, argp);
		break;

	case IXMAP_MAP:
		err = ixmap_cmd_map(adapter, argp);
		break;

	case IXMAP_UNMAP:
		err = ixmap_cmd_unmap(adapter, argp);
		break;

	case IXMAP_DOWN:
		err = ixmap_cmd_down(adapter, arg);
		break;

	case IXMAP_RESET:
		err = ixmap_cmd_reset(adapter, arg);
		break;

	case IXMAP_CHECK_LINK:
		err = ixmap_cmd_check_link(adapter, argp);
		break;

	default:
		err = -EINVAL;
		break;
	};

	up(&adapter->sem);

	return err;
}

int ixmap_irqdev_misc_register(struct ixmap_irqdev *irqdev,
	unsigned int id, int direction, int queue_idx)
{
	char *irqdev_name;
	int err;

	irqdev_name = kmalloc(MISCDEV_NAME_SIZE, GFP_KERNEL);
	if(!irqdev_name){
		goto err_alloc_irqdev_name;
	}

	switch(direction){
	case IRQDEV_RX:
		snprintf(irqdev_name, MISCDEV_NAME_SIZE,
			"ixgbe%d-irqrx%d", id, queue_idx);
		break;
	case IRQDEV_TX:
		snprintf(irqdev_name, MISCDEV_NAME_SIZE,
			"ixgbe%d-irqtx%d", id, queue_idx);
		break;
	default:
		break;
	}

	irqdev->miscdev.minor = MISC_DYNAMIC_MINOR;
	irqdev->miscdev.name = irqdev_name;
	irqdev->miscdev.fops = &ixmap_irqdev_fops;

	err = misc_register(&irqdev->miscdev);
	if (err) {
		pr_err("failed to register irq device\n");
		goto err_misc_register;
	}

	return 0;

err_misc_register:
        kfree(irqdev->miscdev.name);
err_alloc_irqdev_name:
	return -1;
}

void ixmap_irqdev_misc_deregister(struct ixmap_irqdev *irqdev)
{
	misc_deregister(&irqdev->miscdev);
	kfree(irqdev->miscdev.name);

	return;
}

static int ixmap_irqdev_cmd_info(struct ixmap_irqdev *irqdev,
	void __user *argp)
{
	struct ixmap_irqdev_info_req req;
	int err = 0;

	req.vector = irqdev->msix_entry->vector;
	req.entry = irqdev->msix_entry->entry;

	if (copy_to_user(argp, &req, sizeof(req))) {
		return -EFAULT;
	}

	return err;
}

static unsigned int ixmap_irqdev_poll(struct file *file, poll_table *wait)
{
	struct ixmap_irqdev *irqdev;
	unsigned int mask = 0;
	uint32_t count_interrupt;

	irqdev = file->private_data;
	if (!irqdev)
		return -EBADFD;

	poll_wait(file, &irqdev->read_wait, wait);

	count_interrupt = atomic_read(&irqdev->count_interrupt);
	if (count_interrupt)
		mask |= POLLIN | POLLRDNORM;
	mask |= POLLOUT | POLLWRNORM;

	return mask;
}

static ssize_t ixmap_irqdev_read(struct file * file, char __user * buf,
	size_t count, loff_t *pos)
{
	struct ixmap_irqdev *irqdev;
	DECLARE_WAITQUEUE(wait, current);
	uint32_t count_interrupt;
	int err;

	irqdev = file->private_data;
	if (!irqdev)
		return -EBADFD;

	if (count < sizeof(count_interrupt))
		return -EINVAL;

	add_wait_queue(&irqdev->read_wait, &wait);
	while (count) {
		set_current_state(TASK_INTERRUPTIBLE);

		count_interrupt = atomic_xchg(&irqdev->count_interrupt, 0);
		if (!count_interrupt) {
			if (file->f_flags & O_NONBLOCK) {
				err = -EAGAIN;
				break;
			}
			if (signal_pending(current)) {
				err = -ERESTARTSYS;
				break;
			}

			/* Nothing to read, let's sleep */
			schedule();
			continue;
		}
		if (copy_to_user(buf, &count_interrupt, sizeof(count_interrupt)))
			err = -EFAULT;
		else
			err = sizeof(count_interrupt);
		break;
	}

	__set_current_state(TASK_RUNNING);
	remove_wait_queue(&irqdev->read_wait, &wait);

	return err;
}

static int ixmap_irqdev_open(struct inode *inode, struct file *file)
{
	struct ixmap_irqdev *irqdev;
	struct miscdevice *miscdev = file->private_data;
	int err;

	pr_info("open req irqdev=%p\n", miscdev);
	irqdev = container_of(miscdev, struct ixmap_irqdev, miscdev);

	down(&irqdev->sem);

	if (ixmap_irqdev_inuse(irqdev)) {
		err = -EBUSY;
		goto out;
	}

	ixmap_irqdev_get(irqdev);
	file->private_data = irqdev;
	err = 0;

out:
	up(&irqdev->sem);
	return err;
}

static int ixmap_irqdev_close(struct inode *inode, struct file *file)
{
	struct ixmap_irqdev *irqdev = file->private_data;
	if (!irqdev)
		return 0;

	pr_info("close irqdev=%p\n", irqdev);

	down(&irqdev->sem);
	/* XXX: Should we do something here? */
	up(&irqdev->sem);

	ixmap_irqdev_put(irqdev);
	return 0;
}

static long ixmap_irqdev_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	struct ixmap_irqdev *irqdev = file->private_data;
	void __user * argp = (void __user *) arg;
	int err;

	if(!irqdev)
		return -EBADFD;

	down(&irqdev->sem);

	switch (cmd) {
	case IXMAP_IRQDEV_INFO:
		err = ixmap_irqdev_cmd_info(irqdev, argp);
		break;
	default:
		err = -EINVAL;
		break;
	};

	up(&irqdev->sem);

	return err;
}

