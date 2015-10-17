# IXMAP
Wire-speed routing stack in user-space for Intel 10GbE NIC

## 1. Overview
IXMAP is a routing stack for Intel 82599 10GbE NIC in user-space.  
It supports Layer2/Layer3 processing at wire-speed(14.88Mpps).  
It has following entities:

* **IXMAP kmod**
	* interrupt notification by read()
	* device register mapping by mmap()
	* DMA mapping of user space memory by ioctl()
	* exports character devices used by IXMAP lib
* **IXMAP lib**
	* Rx/Tx multiqueue support (based on 5-tuple classification)
	* descripter ring driver for packet batching
	* buffer management with hugepages
* **IXMAP stack**
	* IPv4/IPv6 forwarding
	* Route/Neighbor lookup synchronized with kernel
	* Packet injection via TAP interfaces
	* Interrupt affinity configuration

**IXMAP kmod** is kernel backend module that provides a set of system call
to export what only kernel can access, such as device configuration register,
interrupt handling and conversion between virtual memory address and physical one.

**IXMAP lib** provides API to initialize device register with typical configuration,
manipulation of descripter rings and buffer allocation/deallocation in hugepages.
It uses character devices exported by IXMAP kernel module, such as `/dev/ixmap0`.

**IXMAP stack** processes IPv4/IPv6 lookup for each packet at wire-speed(14.88Mpps/core).
It also supports ARP/ND protocols by injecting the packets into kernel network stack
through TAP interfaces. After injection, The route/neighbor entries are automatically
synchronized with the kernel via NETLINK socket. Moreover, it also injects packets
destined to localhost so that you can use any existing network application on it.

## 2. Build and Install

    % cd ixmap
    % ./autogen.sh
    % ./configure && make && make install

## 3. Configuration

In advance, disable Hyper-Threading, VT-d and Power management(or select high performance mode) in the BIOS.
Enable hugepages and Disable IOMMU at /etc/default/grub:

    GRUB_CMDLINE_LINUX="default_hugepagesz=1G hugepagesz=1G hugepages=8 intel_iommu=off"

Enable kernel IPv4/IPv6 routing at /etc/sysctl.conf:

    net.ipv4.ip_forward=1
    net.ipv6.conf.all.forwarding=1

4. Add udev rule so that ixmap kernel module will be loaded automatically  
(the parameter is depending on your environment):

    % cp ./extra/99-ixmap.rules /etc/udev/rules.d/
    % vi /etc/udev/rules.d/99-ixmap.rules

5. Reboot:

    % reboot

