static void ixgbe_configure_tx(struct ixgbe_adapter *adapter)
{
        struct ixgbe_hw *hw = &adapter->hw;
        u32 dmatxctl;
        u32 i;

        ixgbe_setup_mtqc(adapter);

        if (hw->mac.type != ixgbe_mac_82598EB) {
                /* DMATXCTL.EN must be before Tx queues are enabled */
                dmatxctl = IXGBE_READ_REG(hw, IXGBE_DMATXCTL);
                dmatxctl |= IXGBE_DMATXCTL_TE;
                IXGBE_WRITE_REG(hw, IXGBE_DMATXCTL, dmatxctl);
        }

        /* Setup the HW Tx Head and Tail descriptor pointers */
        for (i = 0; i < adapter->num_tx_queues; i++)
                ixgbe_configure_tx_ring(adapter, adapter->tx_ring[i]);
}

static void ixgbe_setup_mtqc(struct ixgbe_adapter *adapter)
{
        struct ixgbe_hw *hw = &adapter->hw;
        u32 rttdcs, mtqc;
        u8 tcs = netdev_get_num_tc(adapter->netdev);

        if (hw->mac.type == ixgbe_mac_82598EB)
                return;

        /* disable the arbiter while setting MTQC */
        rttdcs = IXGBE_READ_REG(hw, IXGBE_RTTDCS);
        rttdcs |= IXGBE_RTTDCS_ARBDIS;
        IXGBE_WRITE_REG(hw, IXGBE_RTTDCS, rttdcs);

        /* set transmit pool layout */
        if (adapter->flags & IXGBE_FLAG_VMDQ_ENABLED) {
                mtqc = IXGBE_MTQC_VT_ENA;
                if (tcs > 4)
                        mtqc |= IXGBE_MTQC_RT_ENA | IXGBE_MTQC_8TC_8TQ;
                else if (tcs > 1)
                        mtqc |= IXGBE_MTQC_RT_ENA | IXGBE_MTQC_4TC_4TQ;
                else if (adapter->ring_feature[RING_F_RSS].indices == 4)
                        mtqc |= IXGBE_MTQC_32VF;
                else
                        mtqc |= IXGBE_MTQC_64VF;
        } else {
                if (tcs > 4)
                        mtqc = IXGBE_MTQC_RT_ENA | IXGBE_MTQC_8TC_8TQ;
                else if (tcs > 1)
                        mtqc = IXGBE_MTQC_RT_ENA | IXGBE_MTQC_4TC_4TQ;
                else
                        mtqc = IXGBE_MTQC_64Q_1PB;
        }

        IXGBE_WRITE_REG(hw, IXGBE_MTQC, mtqc);

        /* Enable Security TX Buffer IFG for multiple pb */
        if (tcs) {
                u32 sectx = IXGBE_READ_REG(hw, IXGBE_SECTXMINIFG);
                sectx |= IXGBE_SECTX_DCB;
                IXGBE_WRITE_REG(hw, IXGBE_SECTXMINIFG, sectx);
        }

        /* re-enable the arbiter */
        rttdcs &= ~IXGBE_RTTDCS_ARBDIS;
        IXGBE_WRITE_REG(hw, IXGBE_RTTDCS, rttdcs);
}

void ixgbe_configure_tx_ring(struct ixgbe_adapter *adapter,
                             struct ixgbe_ring *ring)
{
        struct ixgbe_hw *hw = &adapter->hw;
        u64 tdba = ring->dma;
        int wait_loop = 10;
        u32 txdctl = IXGBE_TXDCTL_ENABLE;
        u8 reg_idx = ring->reg_idx;

        /* disable queue to avoid issues while updating state */
        IXGBE_WRITE_REG(hw, IXGBE_TXDCTL(reg_idx), IXGBE_TXDCTL_SWFLSH);
        IXGBE_WRITE_FLUSH(hw);

        IXGBE_WRITE_REG(hw, IXGBE_TDBAL(reg_idx), tdba & DMA_BIT_MASK(32));
        IXGBE_WRITE_REG(hw, IXGBE_TDBAH(reg_idx), tdba >> 32);
        IXGBE_WRITE_REG(hw, IXGBE_TDLEN(reg_idx),
                        ring->count * sizeof(union ixgbe_adv_tx_desc));

        /* disable head writeback */
        IXGBE_WRITE_REG(hw, IXGBE_TDWBAH(reg_idx), 0);
        IXGBE_WRITE_REG(hw, IXGBE_TDWBAL(reg_idx), 0);

        /* reset head and tail pointers */
        IXGBE_WRITE_REG(hw, IXGBE_TDH(reg_idx), 0);
        IXGBE_WRITE_REG(hw, IXGBE_TDT(reg_idx), 0);
#ifndef NO_LER_WRITE_CHECKS
        ring->adapter_present = &hw->hw_addr;
#endif /* NO_LER_WRITE_CHECKS */
        ring->tail = adapter->io_addr + IXGBE_TDT(reg_idx);

        /* reset ntu and ntc to place SW in sync with hardwdare */
        ring->next_to_clean = 0;
        ring->next_to_use = 0;

        /*
         * set WTHRESH to encourage burst writeback, it should not be set
         * higher than 1 when:
         * - ITR is 0 as it could cause false TX hangs
         * - ITR is set to > 100k int/sec and BQL is enabled
         *
         * In order to avoid issues WTHRESH + PTHRESH should always be equal
         * to or less than the number of on chip descriptors, which is
         * currently 40.
         */
#if IS_ENABLED(CONFIG_BQL)
        if (!ring->q_vector || (ring->q_vector->itr < IXGBE_100K_ITR))
#else
        if (!ring->q_vector || (ring->q_vector->itr < 8))
#endif
                txdctl |= (1 << 16);    /* WTHRESH = 1 */
        else
                txdctl |= (8 << 16);    /* WTHRESH = 8 */

        /*
         * Setting PTHRESH to 32 both improves performance
         * and avoids a TX hang with DFP enabled
         */
        txdctl |= (1 << 8) |    /* HTHRESH = 1 */
                   32;          /* PTHRESH = 32 */

        /* reinitialize flowdirector state */
        if (adapter->flags & IXGBE_FLAG_FDIR_HASH_CAPABLE) {
                ring->atr_sample_rate = adapter->atr_sample_rate;
                ring->atr_count = 0;
                set_bit(__IXGBE_TX_FDIR_INIT_DONE, &ring->state);
        } else {
                ring->atr_sample_rate = 0;
        }

        /* initialize XPS */
        if (!test_and_set_bit(__IXGBE_TX_XPS_INIT_DONE, &ring->state)) {
                struct ixgbe_q_vector *q_vector = ring->q_vector;

                if (q_vector)
                        netif_set_xps_queue(adapter->netdev,
                                            &q_vector->affinity_mask,
                                            ring->queue_index);
        }

        clear_bit(__IXGBE_HANG_CHECK_ARMED, &ring->state);

        /* enable queue */
        IXGBE_WRITE_REG(hw, IXGBE_TXDCTL(reg_idx), txdctl);

        /* TXDCTL.EN will return 0 on 82598 if link is down, so skip it */
        if (hw->mac.type == ixgbe_mac_82598EB &&
            !(IXGBE_READ_REG(hw, IXGBE_LINKS) & IXGBE_LINKS_UP))
                return;

        /* poll to verify queue is enabled */
        do {
                msleep(1);
                txdctl = IXGBE_READ_REG(hw, IXGBE_TXDCTL(reg_idx));
        } while (--wait_loop && !(txdctl & IXGBE_TXDCTL_ENABLE));
        if (!wait_loop)
                e_err(drv, "Could not enable Tx Queue %d\n", reg_idx);
}
