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
	uint32_t rxctrl, rfctl;
	int i;

	ixgbe_set_rx_mode(ih);

        ixgbe_disable_rx(ih);
        ixgbe_setup_psrtype(ih);
        ixgbe_setup_rdrxctl(ih);

        /* We don't support RSC */
        rfctl = IXGBE_READ_REG(hw, IXGBE_RFCTL);
        rfctl &= ~IXGBE_RFCTL_RSC_DIS;
        IXGBE_WRITE_REG(hw, IXGBE_RFCTL, rfctl);

        /* Program registers for the distribution of queues */
        ixgbe_setup_mrqc(ih);

        /* set_rx_buffer_len must be called before ring initialization */
        ixgbe_set_rx_buffer_len(ih);

        /*
         * Setup the HW Rx Head and Tail Descriptor Pointers and
         * the Base and Length of the Rx Descriptor Ring
         */
        for (i = 0; i < ih->num_queues; i++)
		ixgbe_configure_rx_ring(ih, i, &ih->rx_ring[i]);

        rxctrl = IXGBE_READ_REG(hw, IXGBE_RXCTRL);

        /* enable all receives */
	/* XXX: Do we need disable rx-sec-path before ixgbe_enable_rx ? */
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

void ixgbe_disable_rx(struct ixgbe_handle *ih)
{
        uint32_t pfdtxgswc;
        uint32_t rxctrl;

        rxctrl = IXGBE_READ_REG(ih, IXGBE_RXCTRL);
        if (rxctrl & IXGBE_RXCTRL_RXEN) {
		pfdtxgswc = IXGBE_READ_REG(hw, IXGBE_PFDTXGSWC);
		if (pfdtxgswc & IXGBE_PFDTXGSWC_VT_LBEN) {
			/* TODO: Confirm Local L2 switch is disabled, and delete this process */
		}

                rxctrl &= ~IXGBE_RXCTRL_RXEN;
                IXGBE_WRITE_REG(ih, IXGBE_RXCTRL, rxctrl);
        }

	return;
}

void ixgbe_enable_rx_generic(struct ixgbe_handle *ih)
{
        uint32_t pfdtxgswc;
        uint32_t rxctrl;

        rxctrl = IXGBE_READ_REG(ih, IXGBE_RXCTRL);
        IXGBE_WRITE_REG(ih, IXGBE_RXCTRL, (rxctrl | IXGBE_RXCTRL_RXEN));

	return;
}

static void ixgbe_setup_psrtype(struct ixgbe_handle *ih)
{
        /* PSRTYPE must be initialized in non 82598 adapters */
        uint32_t psrtype = IXGBE_PSRTYPE_TCPHDR |
				IXGBE_PSRTYPE_UDPHDR |
				IXGBE_PSRTYPE_IPV4HDR |
				IXGBE_PSRTYPE_L2HDR |
				IXGBE_PSRTYPE_IPV6HDR;

        if (ih->num_queues > 3)
                psrtype |= 2 << 29;
        else if (ih->num_queues > 1)
                psrtype |= 1 << 29;

	IXGBE_WRITE_REG(ih, IXGBE_PSRTYPE(0), psrtype);
	return;
}

static void ixgbe_setup_rdrxctl(struct ixgbe_handle *ih)
{
        uint32_t rdrxctl = IXGBE_READ_REG(ih, IXGBE_RDRXCTL);

        /* Disable RSC for ACK packets */
        IXGBE_WRITE_REG(ih, IXGBE_RSCDBU,
		(IXGBE_RSCDBU_RSCACKDIS | IXGBE_READ_REG(hw, IXGBE_RSCDBU)));
	rdrxctl &= ~IXGBE_RDRXCTL_RSCFRSTSIZE;
	/* hardware requires some bits to be set by default */
	rdrxctl |= (IXGBE_RDRXCTL_RSCACKC | IXGBE_RDRXCTL_FCOE_WRFIX);
	rdrxctl |= IXGBE_RDRXCTL_CRCSTRIP;

	IXGBE_WRITE_REG(ih, IXGBE_RDRXCTL, rdrxctl);
	return;
}

static void ixgbe_setup_mrqc(struct ixgbe_handle *ih)
{
        static const uint32_t seed[10] = { 0xE291D73D, 0x1805EC6C, 0x2A94B30D,
					0xA54F2BEC, 0xEA49AF7C, 0xE214AD3D, 0xB855AABE,
					0x6A3E67EA, 0x14364D17, 0x3BED200D};
        uint32_t mrqc = 0, reta = 0;
        uint32_t rxcsum;
        int i, j, reta_entries = 128;
        int indices_multi;

        /* Fill out hash function seeds */
        for (i = 0; i < 10; i++)
                IXGBE_WRITE_REG(ih, IXGBE_RSSRK(i), seed[i]);

        /* Fill out the redirection table as follows:
         * 82598: 128 (8 bit wide) entries containing pair of 4 bit RSS indices
         * 82599/X540: 128 (8 bit wide) entries containing 4 bit RSS index
         */
	/* XXX: Do we need filling out redirection table ? */
	indices_multi = 0x1;
        for (i = 0, j = 0; i < reta_entries; i++, j++) {
                if (j == ih->num_queues)
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
        IXGBE_WRITE_REG(ih, IXGBE_RXCSUM, rxcsum);

	mrqc = IXGBE_MRQC_RSSEN;

        /* Perform hash on these packet types */
        mrqc |= IXGBE_MRQC_RSS_FIELD_IPV4 |
                IXGBE_MRQC_RSS_FIELD_IPV4_TCP |
		IXGBE_MRQC_RSS_FIELD_IPV4_UDP |
                IXGBE_MRQC_RSS_FIELD_IPV6 |
                IXGBE_MRQC_RSS_FIELD_IPV6_TCP | 
                IXGBE_MRQC_RSS_FIELD_IPV6_UDP;

        IXGBE_WRITE_REG(ih, IXGBE_MRQC, mrqc);
	return;
}

static void ixgbe_set_rx_buffer_len(struct ixgbe_handle *ih)
{
        struct ixgbe_ring *rx_ring;
        int i;
        uint32_t mhadd, hlreg0;

        /* adjust max frame to be at least the size of a standard frame */
        if (ih->mtu_frame < (ETH_FRAME_LEN + ETH_FCS_LEN))
                ih->mtu_frame = (ETH_FRAME_LEN + ETH_FCS_LEN);

        mhadd = IXGBE_READ_REG(ih, IXGBE_MHADD);
        if (ih->mtu_frame != (mhadd >> IXGBE_MHADD_MFS_SHIFT)) {
                mhadd &= ~IXGBE_MHADD_MFS_MASK;
                mhadd |= ih->mtu_frame << IXGBE_MHADD_MFS_SHIFT;

                IXGBE_WRITE_REG(ih, IXGBE_MHADD, mhadd);
        }

	/* MHADD will allow an extra 4 bytes past for vlan tagged frames */
	ih->mtu_frame += VLAN_HLEN;

        if(ih->mtu_frame <= MAXIMUM_ETHERNET_VLAN_SIZE)) {
                ih->buf_size = MAXIMUM_ETHERNET_VLAN_SIZE;
        /*
         * Make best use of allocation by using all but 1K of a
         * power of 2 allocation that will be used for skb->head.
         */
        } else if (max_frame <= IXGBE_RXBUFFER_3K) {
                ih->buf_size = IXGBE_RXBUFFER_3K;
        } else if (max_frame <= IXGBE_RXBUFFER_7K) {
                ih->buf_size = IXGBE_RXBUFFER_7K;
        } else if (max_frame <= IXGBE_RXBUFFER_15K) {
                ih->buf_size = IXGBE_RXBUFFER_15K;
        } else {
                ih->buf_size = IXGBE_MAX_RXBUFFER;
        }

        hlreg0 = IXGBE_READ_REG(ih, IXGBE_HLREG0);
        /* set jumbo enable since MHADD.MFS is keeping size locked at
         * max_frame
         */
        hlreg0 |= IXGBE_HLREG0_JUMBOEN;
        IXGBE_WRITE_REG(ih, IXGBE_HLREG0, hlreg0);

	return;
}

void ixgbe_configure_rx_ring(struct ixgbe_handle *ih,
	uint8_t reg_idx, struct ixgbe_ring *ring)
{
        uint64_t rdba = ring->paddr;
        uint32_t rxdctl;

	/* disable queue to avoid issues while updating state */
	rxdctl = IXGBE_READ_REG(ih, IXGBE_RXDCTL(reg_idx));
	ixgbe_disable_rx_queue(ih, reg_idx, ring);

        IXGBE_WRITE_REG(ih, IXGBE_RDBAL(reg_idx), rdba & DMA_BIT_MASK(32));
        IXGBE_WRITE_REG(ih, IXGBE_RDBAH(reg_idx), rdba >> 32);
        IXGBE_WRITE_REG(ih, IXGBE_RDLEN(reg_idx),
		ring->count * sizeof(union ixgbe_adv_rx_desc));

        /* reset head and tail pointers */
	IXGBE_WRITE_REG(ih, IXGBE_RDH(reg_idx), 0);
	IXGBE_WRITE_REG(ih, IXGBE_RDT(reg_idx), 0);

	ring->tail = ih->bar + IXGBE_RDT(reg_idx);

        /* reset ntu and ntc to place SW in sync with hardwdare */
	ring->next_to_clean = 0;
	ring->next_to_use = 0;

        ixgbe_configure_srrctl(ih, reg_idx, ring);

	/* enable receive descriptor ring */
	rxdctl |= IXGBE_RXDCTL_ENABLE;
	IXGBE_WRITE_REG(ih, IXGBE_RXDCTL(reg_idx), rxdctl);

	ixgbe_rx_desc_queue_enable(ih, reg_idx, ring);
}

void ixgbe_disable_rx_queue(struct ixgbe_handle *ih,
	uint8_t reg_idx, struct ixgbe_ring *ring)
{
	int wait_loop = IXGBE_MAX_RX_DESC_POLL;
	uint32_t rxdctl;

        rxdctl = IXGBE_READ_REG(ih, IXGBE_RXDCTL(reg_idx));
        rxdctl &= ~IXGBE_RXDCTL_ENABLE;

        /* write value back with RXDCTL.ENABLE bit cleared */
        IXGBE_WRITE_REG(ih, IXGBE_RXDCTL(reg_idx), rxdctl);

	/* the hardware may take up to 100us to really disable the rx queue */
	do {
		udelay(10);
		rxdctl = IXGBE_READ_REG(ih, IXGBE_RXDCTL(reg_idx));
	} while (--wait_loop && (rxdctl & IXGBE_RXDCTL_ENABLE));

	if (!wait_loop) {
		printf("RXDCTL.ENABLE on Rx queue %d not cleared within the polling period\n",
			reg_idx);
	}
}

static void ixgbe_configure_srrctl(struct ixgbe_handle *ih,
	uint8_t reg_idx, struct ixgbe_ring *rx_ring)
{
	uint32_t srrctl;

        /* configure header buffer length, needed for RSC */
        srrctl = IXGBE_RX_HDR_SIZE << IXGBE_SRRCTL_BSIZEHDRSIZE_SHIFT;

        /* configure the packet buffer length */
        srrctl |= ALIGN(ih->buf_size, 1024) >>
                  IXGBE_SRRCTL_BSIZEPKT_SHIFT;

        /* configure descriptor type */
        srrctl |= IXGBE_SRRCTL_DESCTYPE_ADV_ONEBUF;

	/*
	 * Enable the hardware to drop packets when the buffer is
	 * full. This is useful when multiqueue,so that no single
	 * queue being full stalls the entire RX engine. We only
	 * enable this when Multiqueue AND when Flow Control is 
	 * disabled.
	 */
	if(ih->num_queues > 1){
		/* TODO: Confirm kernel side module disables flow control(FC) features */
		srrctl |= IXGBE_SRRCTL_DROP_EN;
	}

        IXGBE_WRITE_REG(hw, IXGBE_SRRCTL(reg_idx), srrctl);
	return;
}

static void ixgbe_rx_desc_queue_enable(struct ixgbe_handle *ih,
	uint8_t reg_idx, struct ixgbe_ring *ring)
{
        int wait_loop = IXGBE_MAX_RX_DESC_POLL;
        uint32_t rxdctl;

        do {
                msleep(1);
                rxdctl = IXGBE_READ_REG(ih, IXGBE_RXDCTL(reg_idx));
        } while (--wait_loop && !(rxdctl & IXGBE_RXDCTL_ENABLE));

        if (!wait_loop) {
		printf("RXDCTL.ENABLE on Rx queue %d not cleared within the polling period\n",
			reg_idx);
        }
}
