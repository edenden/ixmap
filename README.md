# IXMAP - Wire-speed routing stack for Intel 10GbE NIC in user-space

## 1. Overview
IXMAP is a routing stack for Intel 82599 10GbE NIC in user-space.
It supports Layer2/Layer3 processing at wire-speed(14.88Mpps).

It has following modules:
* IXMAP kernel module
	* interrupt notification by read()
	* device register mapping by mmap()
	* DMA mapping of user space memory by ioctl()
	* exports character devices used by IXMAP lib
* IXMAP lib
	* Rx/Tx multiqueue support (based on 5-tuple classification)
	* descripter ring driver for packet batching
	* buffer management with hugepages
* IXMAP stack
	* IP/IPv6 lookup
	* Route/Neighbor entry synchronization with kernel
	* Packet injection via TAP interfaces
	* Interrupt affinity configuration

IXMAP kernel module provides a set of system call to access what only kernel
can access, such as device configuration register, interrupt handling and
conversion between virtual memory address and physical one.

IXMAP lib provides API to initialize device register with typical configuration,
manipulation of descripter rings and buffer allocation/deallocation in hugepages.
It uses character devices exported by IXMAP kernel module, such as /dev/ixmap0.

IXMAP stack processes IP/IPv6 lookup for each packet at wire-speed(14.88Mpps/core).
It also supports ARP/ND protocols injecting the packets into kernel network stack
through TAP interfaces. After injection, The route/neighbor entries are automatically
synchronized with the kernel via NETLINK socket. Moreover, it also injects packets
destined to localhost so that you can use any existing network application on it.

## 2. Build and Install
TBD

## 3. Usage
TBD

