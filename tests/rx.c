#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#include "main.h"
#include "forward.h"
#include "rx.h"

void ixgbe_configure_rx(struct ixgbe_handler *ih)
{
	u32 rxctrl, rfctl;

	ixgbe_set_rx_mode(ih);

        ixgbe_disable_rx(ih);
        ixgbe_setup_psrtype(adapter);
        ixgbe_setup_rdrxctl(adapter);

        /* We don't support RSC */
        rfctl = IXGBE_READ_REG(hw, IXGBE_RFCTL);
        rfctl &= ~IXGBE_RFCTL_RSC_DIS;
        IXGBE_WRITE_REG(hw, IXGBE_RFCTL, rfctl);

        /* Program registers for the distribution of queues */
        ixgbe_setup_mrqc(adapter);

        /* set_rx_buffer_len must be called before ring initialization */
        ixgbe_set_rx_buffer_len(adapter);

        /*
         * Setup the HW Rx Head and Tail Descriptor Pointers and
         * the Base and Length of the Rx Descriptor Ring
         */
        for (i = 0; i < adapter->num_rx_queues; i++)
                ixgbe_configure_rx_ring(adapter, adapter->rx_ring[i]);

        rxctrl = IXGBE_READ_REG(hw, IXGBE_RXCTRL);

        /* enable all receives */
	/* XXX: Do we need disable rx-sec-path before ixgbe_enable_rx? */
	ixgbe_enable_rx(hw);
}

void ixgbe_set_rx_mode(struct ixgbe_handle *ih)
{
        uint32_t fctrl;
        uint32_t vlnctrl;
	uint32_t vmolr = IXGBE_VMOLR_BAM | IXGBE_VMOLR_AUPE;

        /* Check for Promiscuous and All Multicast modes */
        fctrl = IXGBE_READ_REG(ih, IXGBE_FCTRL);
        vlnctrl = IXGBE_READ_REG(ih, IXGBE_VLNCTRL);

        /* set all bits that we expect to always be set */
        fctrl |= IXGBE_FCTRL_BAM;
        fctrl |= IXGBE_FCTRL_DPF; /* discard pause frames when FC enabled */
        fctrl |= IXGBE_FCTRL_PMCF;

        /* clear the bits we are changing the status of */
	fctrl &= ~(IXGBE_FCTRL_UPE | IXGBE_FCTRL_MPE);
	vlnctrl  &= ~(IXGBE_VLNCTRL_VFE | IXGBE_VLNCTRL_CFIEN);

	if (ih->promisc) {
		fctrl |= (IXGBE_FCTRL_UPE | IXGBE_FCTRL_MPE);
		vmolr |= IXGBE_VMOLR_MPE;
        } else {
		fctrl |= IXGBE_FCTRL_MPE;
		vmolr |= IXGBE_VMOLR_MPE;
        }

	/* XXX: Do we need to write VMOLR ? */
	vmolr |= IXGBE_READ_REG(ih, IXGBE_VMOLR(VMDQ_P(0))) &
			~(IXGBE_VMOLR_MPE | IXGBE_VMOLR_ROMPE |
			IXGBE_VMOLR_ROPE);
	IXGBE_WRITE_REG(ih, IXGBE_VMOLR(VMDQ_P(0)), vmolr);

        IXGBE_WRITE_REG(ih, IXGBE_VLNCTRL, vlnctrl);
        IXGBE_WRITE_REG(ih, IXGBE_FCTRL, fctrl);

	return;
}

void ixgbe_disable_rx_generic(struct ixgbe_hw *hw)
{
        u32 pfdtxgswc;
        u32 rxctrl;

        rxctrl = IXGBE_READ_REG(hw, IXGBE_RXCTRL);
        if (rxctrl & IXGBE_RXCTRL_RXEN) {
                if (hw->mac.type != ixgbe_mac_82598EB) {
                        pfdtxgswc = IXGBE_READ_REG(hw, IXGBE_PFDTXGSWC);
                        if (pfdtxgswc & IXGBE_PFDTXGSWC_VT_LBEN) {
                                pfdtxgswc &= ~IXGBE_PFDTXGSWC_VT_LBEN;
                                IXGBE_WRITE_REG(hw, IXGBE_PFDTXGSWC, pfdtxgswc);
                                hw->mac.set_lben = true;
                        } else {
                                hw->mac.set_lben = false;
                        }
                }
                rxctrl &= ~IXGBE_RXCTRL_RXEN;
                IXGBE_WRITE_REG(hw, IXGBE_RXCTRL, rxctrl);
        }
}

static void ixgbe_setup_psrtype(struct ixgbe_adapter *adapter)
{
        struct ixgbe_hw *hw = &adapter->hw;
        int rss_i = adapter->ring_feature[RING_F_RSS].indices;
        int p;

        /* PSRTYPE must be initialized in non 82598 adapters */
        u32 psrtype = IXGBE_PSRTYPE_TCPHDR |
                      IXGBE_PSRTYPE_UDPHDR |
                      IXGBE_PSRTYPE_IPV4HDR |
                      IXGBE_PSRTYPE_L2HDR |
                      IXGBE_PSRTYPE_IPV6HDR;

        if (hw->mac.type == ixgbe_mac_82598EB)
                return;

        if (rss_i > 3)
                psrtype |= 2 << 29;
        else if (rss_i > 1)
                psrtype |= 1 << 29;

        for (p = 0; p < adapter->num_rx_pools; p++)
                IXGBE_WRITE_REG(hw, IXGBE_PSRTYPE(VMDQ_P(p)), psrtype);
}

static void ixgbe_setup_rdrxctl(struct ixgbe_adapter *adapter)
{
        struct ixgbe_hw *hw = &adapter->hw;
        u32 rdrxctl = IXGBE_READ_REG(hw, IXGBE_RDRXCTL);

        /* Disable RSC for ACK packets */
        IXGBE_WRITE_REG(hw, IXGBE_RSCDBU,
		(IXGBE_RSCDBU_RSCACKDIS | IXGBE_READ_REG(hw, IXGBE_RSCDBU)));
	rdrxctl &= ~IXGBE_RDRXCTL_RSCFRSTSIZE;
	/* hardware requires some bits to be set by default */
	rdrxctl |= (IXGBE_RDRXCTL_RSCACKC | IXGBE_RDRXCTL_FCOE_WRFIX);
	rdrxctl |= IXGBE_RDRXCTL_CRCSTRIP;

	IXGBE_WRITE_REG(hw, IXGBE_RDRXCTL, rdrxctl);
}

static void ixgbe_setup_mrqc(struct ixgbe_adapter *adapter)
{
        struct ixgbe_hw *hw = &adapter->hw;
        static const u32 seed[10] = { 0xE291D73D, 0x1805EC6C, 0x2A94B30D,
                          0xA54F2BEC, 0xEA49AF7C, 0xE214AD3D, 0xB855AABE,
                          0x6A3E67EA, 0x14364D17, 0x3BED200D};
        u32 mrqc = 0, reta = 0;
        u32 rxcsum;
        int i, j, reta_entries = 128;
        int indices_multi;
        u16 rss_i = adapter->ring_feature[RING_F_RSS].indices;

        /*
         * Program table for at least 2 queues w/ SR-IOV so that VFs can
         * make full use of any rings they may have.  We will use the
         * PSRTYPE register to control how many rings we use within the PF.
         */
        if ((adapter->flags & IXGBE_FLAG_SRIOV_ENABLED) && (rss_i < 2))
                rss_i = 2;

        /* Fill out hash function seeds */
        for (i = 0; i < 10; i++)
                IXGBE_WRITE_REG(hw, IXGBE_RSSRK(i), seed[i]);

        /* Fill out the redirection table as follows:
         * 82598: 128 (8 bit wide) entries containing pair of 4 bit RSS indices
         * 82599/X540: 128 (8 bit wide) entries containing 4 bit RSS index
         */
        if (adapter->hw.mac.type == ixgbe_mac_82598EB)
                indices_multi = 0x11;
        else
                indices_multi = 0x1;

        for (i = 0, j = 0; i < reta_entries; i++, j++) {
                if (j == rss_i)
                        j = 0;
                reta = (reta << 8) | (j * indices_multi);
                if ((i & 3) == 3) {
                        if (i < 128)
                                IXGBE_WRITE_REG(hw, IXGBE_RETA(i >> 2), reta);
                }
        }

        /* Disable indicating checksum in descriptor, enables RSS hash */
        rxcsum = IXGBE_READ_REG(hw, IXGBE_RXCSUM);
        rxcsum |= IXGBE_RXCSUM_PCSD;
        IXGBE_WRITE_REG(hw, IXGBE_RXCSUM, rxcsum);

        if (adapter->hw.mac.type == ixgbe_mac_82598EB) {
                if (adapter->ring_feature[RING_F_RSS].mask)
                        mrqc = IXGBE_MRQC_RSSEN;
        } else {
                u8 tcs = netdev_get_num_tc(adapter->netdev);

                if (adapter->flags & IXGBE_FLAG_VMDQ_ENABLED) {
                        if (tcs > 4)
                                mrqc = IXGBE_MRQC_VMDQRT8TCEN;  /* 8 TCs */
                        else if (tcs > 1)
                                mrqc = IXGBE_MRQC_VMDQRT4TCEN;  /* 4 TCs */
                        else if (adapter->ring_feature[RING_F_RSS].indices == 4)
                                mrqc = IXGBE_MRQC_VMDQRSS32EN;
                        else
                                mrqc = IXGBE_MRQC_VMDQRSS64EN;
                } else {
                        if (tcs > 4)
                                mrqc = IXGBE_MRQC_RTRSS8TCEN;
                        else if (tcs > 1)
                                mrqc = IXGBE_MRQC_RTRSS4TCEN;
                        else
                                mrqc = IXGBE_MRQC_RSSEN;
                }
        }

        /* Perform hash on these packet types */
        mrqc |= IXGBE_MRQC_RSS_FIELD_IPV4 |
                IXGBE_MRQC_RSS_FIELD_IPV4_TCP |
                IXGBE_MRQC_RSS_FIELD_IPV6 |
                IXGBE_MRQC_RSS_FIELD_IPV6_TCP;

        if (adapter->flags2 & IXGBE_FLAG2_RSS_FIELD_IPV4_UDP)
                mrqc |= IXGBE_MRQC_RSS_FIELD_IPV4_UDP;
        if (adapter->flags2 & IXGBE_FLAG2_RSS_FIELD_IPV6_UDP)
                mrqc |= IXGBE_MRQC_RSS_FIELD_IPV6_UDP;

        IXGBE_WRITE_REG(hw, IXGBE_MRQC, mrqc);
}

static void ixgbe_set_rx_buffer_len(struct ixgbe_adapter *adapter)
{
        struct ixgbe_hw *hw = &adapter->hw;
        struct net_device *netdev = adapter->netdev;
        int max_frame = netdev->mtu + ETH_HLEN + ETH_FCS_LEN;
        struct ixgbe_ring *rx_ring;
        int i;
        u32 mhadd, hlreg0;
#ifdef CONFIG_IXGBE_DISABLE_PACKET_SPLIT
        int rx_buf_len;
#endif

#ifdef IXGBE_FCOE
        /* adjust max frame to be able to do baby jumbo for FCoE */
        if ((adapter->flags & IXGBE_FLAG_FCOE_ENABLED) &&
            (max_frame < IXGBE_FCOE_JUMBO_FRAME_SIZE))
                max_frame = IXGBE_FCOE_JUMBO_FRAME_SIZE;

#endif /* IXGBE_FCOE */

        /* adjust max frame to be at least the size of a standard frame */
        if (max_frame < (ETH_FRAME_LEN + ETH_FCS_LEN))
                max_frame = (ETH_FRAME_LEN + ETH_FCS_LEN);

        mhadd = IXGBE_READ_REG(hw, IXGBE_MHADD);
        if (max_frame != (mhadd >> IXGBE_MHADD_MFS_SHIFT)) {
                mhadd &= ~IXGBE_MHADD_MFS_MASK;
                mhadd |= max_frame << IXGBE_MHADD_MFS_SHIFT;

                IXGBE_WRITE_REG(hw, IXGBE_MHADD, mhadd);
        }

#ifdef CONFIG_IXGBE_DISABLE_PACKET_SPLIT
                /* MHADD will allow an extra 4 bytes past for vlan tagged frames */
                max_frame += VLAN_HLEN;

        if (!(adapter->flags2 & IXGBE_FLAG2_RSC_ENABLED) &&
            (max_frame <= MAXIMUM_ETHERNET_VLAN_SIZE)) {
                rx_buf_len = MAXIMUM_ETHERNET_VLAN_SIZE;
        /*
         * Make best use of allocation by using all but 1K of a
         * power of 2 allocation that will be used for skb->head.
         */
        } else if (max_frame <= IXGBE_RXBUFFER_3K) {
                rx_buf_len = IXGBE_RXBUFFER_3K;
        } else if (max_frame <= IXGBE_RXBUFFER_7K) {
                rx_buf_len = IXGBE_RXBUFFER_7K;
        } else if (max_frame <= IXGBE_RXBUFFER_15K) {
                rx_buf_len = IXGBE_RXBUFFER_15K;
        } else {
                rx_buf_len = IXGBE_MAX_RXBUFFER;
        }

#endif /* CONFIG_IXGBE_DISABLE_PACKET_SPLIT */
        hlreg0 = IXGBE_READ_REG(hw, IXGBE_HLREG0);
        /* set jumbo enable since MHADD.MFS is keeping size locked at
         * max_frame
         */
        hlreg0 |= IXGBE_HLREG0_JUMBOEN;
        IXGBE_WRITE_REG(hw, IXGBE_HLREG0, hlreg0);

        /*
         * Setup the HW Rx Head and Tail Descriptor Pointers and
         * the Base and Length of the Rx Descriptor Ring
         */
        for (i = 0; i < adapter->num_rx_queues; i++) {
                rx_ring = adapter->rx_ring[i];
                if (adapter->flags2 & IXGBE_FLAG2_RSC_ENABLED)
                        set_ring_rsc_enabled(rx_ring);
                else
                        clear_ring_rsc_enabled(rx_ring);
#ifdef CONFIG_IXGBE_DISABLE_PACKET_SPLIT

                rx_ring->rx_buf_len = rx_buf_len;

#ifdef IXGBE_FCOE
                if (test_bit(__IXGBE_RX_FCOE, &rx_ring->state) &&
                    (rx_buf_len < IXGBE_FCOE_JUMBO_FRAME_SIZE))
                        rx_ring->rx_buf_len = IXGBE_FCOE_JUMBO_FRAME_SIZE;
#endif /* IXGBE_FCOE */
#endif /* CONFIG_IXGBE_DISABLE_PACKET_SPLIT */
        }
}

void ixgbe_configure_rx_ring(struct ixgbe_adapter *adapter,
                             struct ixgbe_ring *ring)
{
        struct ixgbe_hw *hw = &adapter->hw;
        u64 rdba = ring->dma;
        u32 rxdctl;
        u8 reg_idx = ring->reg_idx;

        /* disable queue to avoid issues while updating state */
        rxdctl = IXGBE_READ_REG(hw, IXGBE_RXDCTL(reg_idx));
        ixgbe_disable_rx_queue(adapter, ring);

        IXGBE_WRITE_REG(hw, IXGBE_RDBAL(reg_idx), rdba & DMA_BIT_MASK(32));
        IXGBE_WRITE_REG(hw, IXGBE_RDBAH(reg_idx), rdba >> 32);
        IXGBE_WRITE_REG(hw, IXGBE_RDLEN(reg_idx),
                        ring->count * sizeof(union ixgbe_adv_rx_desc));

        /* reset head and tail pointers */
        IXGBE_WRITE_REG(hw, IXGBE_RDH(reg_idx), 0);
        IXGBE_WRITE_REG(hw, IXGBE_RDT(reg_idx), 0);
#ifndef NO_LER_WRITE_CHECKS
        ring->adapter_present = &hw->hw_addr;
#endif /* NO_LER_WRITE_CHECKS */
        ring->tail = adapter->io_addr + IXGBE_RDT(reg_idx);

        /* reset ntu and ntc to place SW in sync with hardwdare */
        ring->next_to_clean = 0;
        ring->next_to_use = 0;
#ifndef CONFIG_IXGBE_DISABLE_PACKET_SPLIT
        ring->next_to_alloc = 0;
#endif

        ixgbe_configure_srrctl(adapter, ring);
        /* In ESX, RSCCTL configuration is done by on demand */
        ixgbe_configure_rscctl(adapter, ring);

        /* enable receive descriptor ring */
        rxdctl |= IXGBE_RXDCTL_ENABLE;
        IXGBE_WRITE_REG(hw, IXGBE_RXDCTL(reg_idx), rxdctl);

        ixgbe_rx_desc_queue_enable(adapter, ring);
}

void ixgbe_disable_rx_queue(struct ixgbe_adapter *adapter,
                            struct ixgbe_ring *ring)
{
        struct ixgbe_hw *hw = &adapter->hw;
        int wait_loop = IXGBE_MAX_RX_DESC_POLL;
        u32 rxdctl;
        u8 reg_idx = ring->reg_idx;

        if (IXGBE_REMOVED(hw->hw_addr))
                return;
        rxdctl = IXGBE_READ_REG(hw, IXGBE_RXDCTL(reg_idx));
        rxdctl &= ~IXGBE_RXDCTL_ENABLE;

        /* write value back with RXDCTL.ENABLE bit cleared */
        IXGBE_WRITE_REG(hw, IXGBE_RXDCTL(reg_idx), rxdctl);

        if (hw->mac.type == ixgbe_mac_82598EB &&
            !(IXGBE_READ_REG(hw, IXGBE_LINKS) & IXGBE_LINKS_UP))
                return;

        /* the hardware may take up to 100us to really disable the rx queue */
        do {
                udelay(10);
                rxdctl = IXGBE_READ_REG(hw, IXGBE_RXDCTL(reg_idx));
        } while (--wait_loop && (rxdctl & IXGBE_RXDCTL_ENABLE));

        if (!wait_loop) {
                e_err(drv, "RXDCTL.ENABLE on Rx queue %d not cleared within "
                      "the polling period\n", reg_idx);
        }
}

static void ixgbe_configure_srrctl(struct ixgbe_adapter *adapter,
                                   struct ixgbe_ring *rx_ring)
{
        struct ixgbe_hw *hw = &adapter->hw;
        u32 srrctl;
        u8 reg_idx = rx_ring->reg_idx;

        if (hw->mac.type == ixgbe_mac_82598EB) {
                u16 mask = adapter->ring_feature[RING_F_RSS].mask;

                /* program one srrctl register per VMDq index */
                if (adapter->flags & IXGBE_FLAG_VMDQ_ENABLED)
                        mask = adapter->ring_feature[RING_F_VMDQ].mask;

                /*
                 * if VMDq is not active we must program one srrctl register
                 * per RSS queue since we have enabled RDRXCTL.MVMEN
                 */
                reg_idx &= mask;

                /* divide by the first bit of the mask to get the indices */
                if (reg_idx)
                        reg_idx /= ((~mask) + 1) & mask;
        }

        /* configure header buffer length, needed for RSC */
        srrctl = IXGBE_RX_HDR_SIZE << IXGBE_SRRCTL_BSIZEHDRSIZE_SHIFT;

        /* configure the packet buffer length */
#ifdef CONFIG_IXGBE_DISABLE_PACKET_SPLIT
        srrctl |= ALIGN(rx_ring->rx_buf_len, 1024) >>
                  IXGBE_SRRCTL_BSIZEPKT_SHIFT;
#else
        srrctl |= ixgbe_rx_bufsz(rx_ring) >> IXGBE_SRRCTL_BSIZEPKT_SHIFT;
#endif

        /* configure descriptor type */
        srrctl |= IXGBE_SRRCTL_DESCTYPE_ADV_ONEBUF;

        IXGBE_WRITE_REG(hw, IXGBE_SRRCTL(reg_idx), srrctl);
}

void ixgbe_configure_rscctl(struct ixgbe_adapter *adapter,
                            struct ixgbe_ring *ring)
{
        struct ixgbe_hw *hw = &adapter->hw;
        u32 rscctrl;
        u8 reg_idx = ring->reg_idx;

        if (!ring_is_rsc_enabled(ring))
                return;

        rscctrl = IXGBE_READ_REG(hw, IXGBE_RSCCTL(reg_idx));
        rscctrl |= IXGBE_RSCCTL_RSCEN;
        /*
         * we must limit the number of descriptors so that the
         * total size of max desc * buf_len is not greater
         * than 65536
         */
#ifndef CONFIG_IXGBE_DISABLE_PACKET_SPLIT
#if (MAX_SKB_FRAGS >= 16)
        rscctrl |= IXGBE_RSCCTL_MAXDESC_16;
#elif (MAX_SKB_FRAGS >= 8)
        rscctrl |= IXGBE_RSCCTL_MAXDESC_8;
#elif (MAX_SKB_FRAGS >= 4)
        rscctrl |= IXGBE_RSCCTL_MAXDESC_4;
#else
        rscctrl |= IXGBE_RSCCTL_MAXDESC_1;
#endif
#else /* CONFIG_IXGBE_DISABLE_PACKET_SPLIT */
        if (ring->rx_buf_len <= IXGBE_RXBUFFER_4K)
                rscctrl |= IXGBE_RSCCTL_MAXDESC_16;
        else if (ring->rx_buf_len <= IXGBE_RXBUFFER_8K)
                rscctrl |= IXGBE_RSCCTL_MAXDESC_8;
        else
                rscctrl |= IXGBE_RSCCTL_MAXDESC_4;
#endif
        IXGBE_WRITE_REG(hw, IXGBE_RSCCTL(reg_idx), rscctrl);
}

static void ixgbe_rx_desc_queue_enable(struct ixgbe_adapter *adapter,
                                       struct ixgbe_ring *ring)
{
        struct ixgbe_hw *hw = &adapter->hw;
        int wait_loop = IXGBE_MAX_RX_DESC_POLL;
        u32 rxdctl;
        u8 reg_idx = ring->reg_idx;

        if (IXGBE_REMOVED(hw->hw_addr))
                return;
        /* RXDCTL.EN will return 0 on 82598 if link is down, so skip it */
        if (hw->mac.type == ixgbe_mac_82598EB &&
            !(IXGBE_READ_REG(hw, IXGBE_LINKS) & IXGBE_LINKS_UP))
                return;

        do {
                msleep(1);
                rxdctl = IXGBE_READ_REG(hw, IXGBE_RXDCTL(reg_idx));
        } while (--wait_loop && !(rxdctl & IXGBE_RXDCTL_ENABLE));

        if (!wait_loop) {
                e_err(drv, "RXDCTL.ENABLE on Rx queue %d "
                      "not set within the polling period\n", reg_idx);
        }
}
