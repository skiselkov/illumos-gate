/*
 * Copyright 2008-2013 Solarflare Communications Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "efsys.h"
#include "efx.h"
#include "efx_types.h"
#include "efx_regs.h"
#include "efx_impl.h"
#include "falcon_stats.h"
#include "falcon_xmac.h"

#if EFSYS_OPT_MAC_FALCON_XMAC

static	__checkReturn	int
falcon_xmac_xgxs_reset(
	__in		efx_nic_t *enp)
{
	efx_oword_t oword;
	unsigned int count;
	int rc;

	/* Reset the XGMAC via the Vendor Register */
	EFX_POPULATE_OWORD_1(oword, FRF_AB_XX_RST_XX_EN, 1);
	EFX_BAR_WRITEO(enp, FR_AB_XX_PWR_RST_REG, &oword);

	count = 0;
	do {
		EFSYS_PROBE1(wait, unsigned int, count);

		/* Spin for 10us */
		EFSYS_SPIN(10);

		/* Test for reset complete */
		EFX_BAR_READO(enp, FR_AB_XX_PWR_RST_REG, &oword);
		if (EFX_OWORD_FIELD(oword, FRF_AB_XX_RST_XX_EN) == 0 &&
		    EFX_OWORD_FIELD(oword, FRF_AB_XX_SD_RST_ACT) == 0)
			goto done;
	} while (++count < 1000);

	rc = ETIMEDOUT;
	goto fail1;

done:
	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static			void
falcon_xmac_xgxs_reconfigure(
	__in		efx_nic_t *enp)
{
#if EFSYS_OPT_LOOPBACK
	efx_port_t *epp = &(enp->en_port);
#endif	/* EFSYS_OPT_LOOPBACK */
	efx_oword_t oword;
	uint32_t force_sig;
	uint32_t force_sig_val;
	uint32_t xgxs_lb_en;
	uint32_t xgmii_lb_en;
	uint32_t lpbk;
	boolean_t need_reset;

#if EFSYS_OPT_LOOPBACK
	force_sig =
	    (epp->ep_loopback_type == EFX_LOOPBACK_XGXS ||
	    epp->ep_loopback_type == EFX_LOOPBACK_XAUI) ?
	    1 : 0;
	force_sig_val =
	    (epp->ep_loopback_type == EFX_LOOPBACK_XGXS ||
	    epp->ep_loopback_type == EFX_LOOPBACK_XAUI) ?
	    1 : 0;
	xgxs_lb_en = (epp->ep_loopback_type == EFX_LOOPBACK_XGXS) ?
	    1 : 0;
	xgmii_lb_en = (epp->ep_loopback_type == EFX_LOOPBACK_XGMII) ?
	    1 : 0;
	lpbk = (epp->ep_loopback_type == EFX_LOOPBACK_XAUI) ?
	    1 : 0;
#else	/* EFSYS_OPT_LOOPBACK */
	force_sig = 0;
	force_sig_val = 0;
	xgxs_lb_en = 0;
	xgmii_lb_en = 0;
	lpbk = 0;
#endif	/* EFSYS_OPT_LOOPBACK */

	EFX_BAR_READO(enp, FR_AB_XX_CORE_STAT_REG, &oword);
	need_reset =
	    (EFX_OWORD_FIELD(oword, FRF_AB_XX_FORCE_SIG0) != force_sig ||
	    EFX_OWORD_FIELD(oword, FRF_AB_XX_FORCE_SIG0_VAL) != force_sig_val ||
	    EFX_OWORD_FIELD(oword, FRF_AB_XX_FORCE_SIG1) != force_sig ||
	    EFX_OWORD_FIELD(oword, FRF_AB_XX_FORCE_SIG1_VAL) != force_sig_val ||
	    EFX_OWORD_FIELD(oword, FRF_AB_XX_FORCE_SIG2) != force_sig ||
	    EFX_OWORD_FIELD(oword, FRF_AB_XX_FORCE_SIG2_VAL) != force_sig_val ||
	    EFX_OWORD_FIELD(oword, FRF_AB_XX_FORCE_SIG3) != force_sig ||
	    EFX_OWORD_FIELD(oword, FRF_AB_XX_FORCE_SIG3_VAL) != force_sig_val ||
	    EFX_OWORD_FIELD(oword, FRF_AB_XX_XGXS_LB_EN) != xgxs_lb_en ||
	    EFX_OWORD_FIELD(oword, FRF_AB_XX_XGMII_LB_EN) != xgmii_lb_en);

	EFX_BAR_READO(enp, FR_AB_XX_SD_CTL_REG, &oword);
	need_reset |= (EFX_OWORD_FIELD(oword, FRF_AB_XX_LPBKA) != lpbk ||
	    EFX_OWORD_FIELD(oword, FRF_AB_XX_LPBKB) != lpbk ||
	    EFX_OWORD_FIELD(oword, FRF_AB_XX_LPBKC) != lpbk ||
	    EFX_OWORD_FIELD(oword, FRF_AB_XX_LPBKD) != lpbk);

	if (need_reset)
		(void) falcon_xmac_xgxs_reset(enp);

	EFX_BAR_READO(enp, FR_AB_XX_CORE_STAT_REG, &oword);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_XX_FORCE_SIG0, force_sig);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_XX_FORCE_SIG0_VAL, force_sig_val);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_XX_FORCE_SIG1, force_sig);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_XX_FORCE_SIG1_VAL, force_sig_val);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_XX_FORCE_SIG2, force_sig);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_XX_FORCE_SIG2_VAL, force_sig_val);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_XX_FORCE_SIG3, force_sig);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_XX_FORCE_SIG3_VAL, force_sig_val);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_XX_XGXS_LB_EN, xgxs_lb_en);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_XX_XGMII_LB_EN, xgmii_lb_en);
	EFX_BAR_WRITEO(enp, FR_AB_XX_CORE_STAT_REG, &oword);

	EFX_BAR_READO(enp, FR_AB_XX_SD_CTL_REG, &oword);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_XX_LPBKD, lpbk);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_XX_LPBKB, lpbk);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_XX_LPBKC, lpbk);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_XX_LPBKA, lpbk);
	EFX_BAR_WRITEO(enp, FR_AB_XX_SD_CTL_REG, &oword);
}

static			void
falcon_xmac_core_reconfigure(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_oword_t oword;

	EFX_POPULATE_OWORD_3(oword,
	    FRF_AB_XM_RX_JUMBO_MODE, 1,
	    FRF_AB_XM_TX_STAT_EN, 1,
	    FRF_AB_XM_RX_STAT_EN, 1);

	EFX_BAR_WRITEO(enp, FR_AB_XM_GLB_CFG_REG, &oword);

	EFX_POPULATE_OWORD_6(oword,
	    FRF_AB_XM_TXEN, 1,
	    FRF_AB_XM_TX_PRMBL, 1,
	    FRF_AB_XM_AUTO_PAD, 1,
	    FRF_AB_XM_TXCRC, 1,
	    FRF_AB_XM_FCNTL, 1,
	    FRF_AB_XM_IPG, 3);

	EFX_BAR_WRITEO(enp, FR_AB_XM_TX_CFG_REG, &oword);

	EFX_POPULATE_OWORD_6(oword,
	    FRF_AB_XM_RXEN, 1,
	    FRF_AB_XM_AUTO_DEPAD, 0,
	    FRF_AB_XM_REJ_BCAST, (epp->ep_brdcst) ? 0 : 1,
	    FRF_AB_XM_ACPT_ALL_MCAST, 1,
	    FRF_AB_XM_ACPT_ALL_UCAST, (epp->ep_unicst) ? 1 : 0,
	    FRF_AB_XM_PASS_CRC_ERR, 1);

	EFX_BAR_WRITEO(enp, FR_AB_XM_RX_CFG_REG, &oword);

	EFX_POPULATE_OWORD_2(oword,
	    FRF_AB_XM_PAUSE_TIME, 0xfffe,
	    FRF_AB_XM_DIS_FCNTL, (epp->ep_fcntl != 0) ? 0 : 1);

	EFX_BAR_WRITEO(enp, FR_AB_XM_FC_REG, &oword);

	EFX_POPULATE_OWORD_1(oword, FRF_AB_XM_ADR_LO,
	    ((uint32_t)epp->ep_mac_addr[0] << 0) |
	    ((uint32_t)epp->ep_mac_addr[1] << 8) |
	    ((uint32_t)epp->ep_mac_addr[2] << 16) |
	    ((uint32_t)epp->ep_mac_addr[3] << 24));

	EFX_BAR_WRITEO(enp, FR_AB_XM_ADR_LO_REG, &oword);

	EFX_POPULATE_OWORD_1(oword, FRF_AB_XM_ADR_HI,
	    ((uint32_t)epp->ep_mac_addr[4] << 0) |
	    ((uint32_t)epp->ep_mac_addr[5] << 8));

	EFX_BAR_WRITEO(enp, FR_AB_XM_ADR_HI_REG, &oword);

	EFX_POPULATE_OWORD_1(oword,
	    FRF_AB_XM_MAX_RX_FRM_SIZE_HI, epp->ep_mac_pdu >> 3);

	EFX_BAR_WRITEO(enp, FR_AB_XM_RX_PARAM_REG, &oword);

	EFX_POPULATE_OWORD_2(oword,
	    FRF_AB_XM_MAX_TX_FRM_SIZE_HI, epp->ep_mac_pdu >> 3,
	    FRF_AB_XM_TX_JUMBO_MODE, 1);

	EFX_BAR_WRITEO(enp, FR_AB_XM_TX_PARAM_REG, &oword);
}

	__checkReturn	int
falcon_xmac_reset(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_oword_t oword;
	int rc;

	EFSYS_ASSERT3U(epp->ep_mac_type, ==, EFX_MAC_FALCON_XMAC);

	EFSYS_PROBE(reset);

	/* Ensure that the XMAC registers are accessible */
	EFX_BAR_READO(enp, FR_AB_NIC_STAT_REG, &oword);
	EFX_SET_OWORD_FIELD(oword, FRF_BB_EE_STRAP_EN, 1);
	EFX_SET_OWORD_FIELD(oword, FRF_BB_EE_STRAP, 0x5);
	EFX_BAR_WRITEO(enp, FR_AB_NIC_STAT_REG, &oword);

	if ((rc = falcon_mac_wrapper_disable(enp)) != 0)
	    goto fail1;

	/*
	 * Force the PHY XAUI state machine to restart after the EM block
	 * reset. Don't do this if we're now in a MAC level loopback.
	 */
#if EFSYS_OPT_LOOPBACK
	if ((1 << epp->ep_loopback_type) & ~EFX_LOOPBACK_MAC_MASK)
#endif
		if ((rc = falcon_xmac_xgxs_reset(enp)) != 0)
			goto fail2;

	epp->ep_mac_poll_needed = B_TRUE;

	enp->en_reset_flags |= EFX_RESET_MAC;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn	int
falcon_xmac_reconfigure(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);

	EFSYS_PROBE(reconfigure);

	EFSYS_ASSERT3U(epp->ep_mac_type, ==, EFX_MAC_FALCON_XMAC);
	EFSYS_ASSERT(epp->ep_link_mode == EFX_LINK_UNKNOWN ||
		    epp->ep_link_mode == EFX_LINK_DOWN ||
		    epp->ep_link_mode == EFX_LINK_10000FDX);

	falcon_xmac_xgxs_reconfigure(enp);
	falcon_xmac_core_reconfigure(enp);

	falcon_mac_wrapper_enable(enp);

	return (0);
}

#if EFSYS_OPT_MAC_STATS
static		uint32_t
falcon_xmac_stat_read(
	__in	efsys_mem_t *esmp,
	__in	unsigned int offset,
	__in	unsigned int width)
{
	uint32_t val;

	switch (width) {
	case 2: {
		efx_dword_t dword;

		EFSYS_MEM_READD(esmp, offset, &dword);

		val = (uint16_t)EFX_DWORD_FIELD(dword, EFX_WORD_0);
		break;
	}
	case 4: {
		efx_dword_t dword;

		EFSYS_MEM_READD(esmp, offset, &dword);

		val = EFX_DWORD_FIELD(dword, EFX_DWORD_0);
		break;
	}
	case 6: {
		efx_qword_t qword;

		EFSYS_MEM_READQ(esmp, offset, &qword);

		val = EFX_QWORD_FIELD(qword, EFX_DWORD_0);
		break;
	}
	default:
		EFSYS_ASSERT(B_FALSE);

		val = 0;
		break;
	}

	return (val);
}

#define	XMAC_STAT_READ(_esmp, _id)					\
	falcon_xmac_stat_read((_esmp), XG_STAT_OFFSET(_id),		\
	    XG_STAT_WIDTH(_id))

#define	XMAC_STAT_INCR(_esmp, _stat, _id)				\
	do {								\
		delta = XMAC_STAT_READ(_esmp, _id);			\
		EFSYS_STAT_INCR(&(_stat), delta);			\
	_NOTE(CONSTANTCONDITION)					\
	} while (0)

#define	XMAC_STAT_DECR(_esmp, _stat, _id)				\
	do {								\
		delta = XMAC_STAT_READ(_esmp, _id);			\
		EFSYS_STAT_DECR(&(_stat), delta);			\
	_NOTE(CONSTANTCONDITION)					\
	} while (0)

	__checkReturn			int
falcon_xmac_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__in_ecount(EFX_MAC_NSTATS)	efsys_stat_t *stat,
	__out_opt			uint32_t *generationp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_oword_t oword;
	uint32_t delta;

	_NOTE(ARGUNUSED(generationp));
	EFSYS_ASSERT3U(epp->ep_mac_type, ==, EFX_MAC_FALCON_XMAC);

	/* Check the DMA flag */
	if (XMAC_STAT_READ(esmp, DMA_DONE) != DMA_IS_DONE)
		return (EAGAIN);
	EFSYS_MEM_READ_BARRIER();

	/* RX */
	XMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_OCTETS], RX_OCTETS);
	XMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_PKTS], RX_PKTS);
	XMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_UNICST_PKTS], RX_UNICAST_PKTS);
	XMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_MULTICST_PKTS],
	    RX_MULTICAST_PKTS);
	XMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_BRDCST_PKTS], RX_BROADCAST_PKTS);
	XMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_PAUSE_PKTS], RX_PAUSE_PKTS);
	XMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_LE_64_PKTS], RX_UNDERSIZE_PKTS);

	XMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_LE_64_PKTS], RX_PKTS_64_OCTETS);
	XMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_65_TO_127_PKTS],
	    RX_PKTS_65_TO_127_OCTETS);
	XMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_128_TO_255_PKTS],
	    RX_PKTS_128_TO_255_OCTETS);
	XMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_256_TO_511_PKTS],
	    RX_PKTS_256_TO_511_OCTETS);
	XMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_512_TO_1023_PKTS],
	    RX_PKTS_512_TO_1023_OCTETS);
	XMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_1024_TO_15XX_PKTS],
	    RX_PKTS_1024_TO_15XX_OCTETS);

	XMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_GE_15XX_PKTS],
	    RX_PKTS_15XX_TO_MAX_OCTETS);
	XMAC_STAT_DECR(esmp, stat[EFX_MAC_RX_GE_15XX_PKTS],
	    RX_OVERSIZE_PKTS);

	XMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_ERRORS], RX_PKTS);
	XMAC_STAT_DECR(esmp, stat[EFX_MAC_RX_ERRORS], RX_PKTS_OK);

	XMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_FCS_ERRORS],
	    RX_UNDERSIZE_FCS_ERROR_PKTS);
	XMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_FCS_ERRORS], RX_FCS_ERROR_PKTS);
	XMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_DROP_EVENTS], RX_DROP_EVENTS);
	XMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_SYMBOL_ERRORS], RX_SYMBOL_ERROR);
	XMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_ALIGN_ERRORS], RX_ALIGN_ERROR);
	XMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_INTERNAL_ERRORS],
	    RX_INTERNAL_MAC_ERROR);

	EFX_BAR_READO(enp, FR_AZ_RX_NODESC_DROP_REG, &oword);
	delta = (uint32_t)EFX_OWORD_FIELD(oword, FRF_AZ_RX_NODESC_DROP_CNT);
	EFSYS_STAT_INCR(&(stat[EFX_MAC_RX_NODESC_DROP_CNT]), delta);

	/* TX */
	XMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_OCTETS], TX_OCTETS);
	XMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_PKTS], TX_PKTS);
	XMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_UNICST_PKTS], TX_UNICAST_PKTS);
	XMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_MULTICST_PKTS],
	    TX_MULTICAST_PKTS);
	XMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_BRDCST_PKTS], TX_BROADCAST_PKTS);
	XMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_PAUSE_PKTS], TX_PAUSE_PKTS);

	XMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_LE_64_PKTS], TX_UNDERSIZE_PKTS);
	XMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_LE_64_PKTS], TX_PKTS_64_OCTETS);
	XMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_65_TO_127_PKTS],
	    TX_PKTS_65_TO_127_OCTETS);
	XMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_128_TO_255_PKTS],
	    TX_PKTS_128_TO_255_OCTETS);
	XMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_256_TO_511_PKTS],
	    TX_PKTS_256_TO_511_OCTETS);
	XMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_512_TO_1023_PKTS],
	    TX_PKTS_512_TO_1023_OCTETS);
	XMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_1024_TO_15XX_PKTS],
	    TX_PKTS_1024_TO_15XX_OCTETS);
	XMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_GE_15XX_PKTS],
	    TX_PKTS_15XX_TO_MAX_OCTETS);
	XMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_GE_15XX_PKTS],
	    TX_OVERSIZE_PKTS);

	XMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_ERRORS], TX_MAC_SRC_ERR_PKTS);
	XMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_ERRORS], TX_IP_SRC_ERR_PKTS);

	return (0);
}
#endif	/* EFSYS_OPT_MAC_STATS */

static			void
falcon_xmac_downlink_check(
	__in		efx_nic_t *enp,
	__out		boolean_t *upp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_oword_t oword;
	boolean_t align_done;
	boolean_t sync_stat;

	EFSYS_ASSERT3U(epp->ep_mac_type, ==, EFX_MAC_FALCON_XMAC);

	/* XGXS state irrelevent in XGMII loopback */
#if EFSYS_OPT_LOOPBACK
	if (epp->ep_loopback_type == EFX_LOOPBACK_XGMII) {
		*upp = B_TRUE;
		return;
	}
#endif	/* EFSYS_OPT_LOOPBACK */

	EFX_BAR_READO(enp, FR_AB_XX_CORE_STAT_REG, &oword);

	align_done = (EFX_OWORD_FIELD(oword, FRF_AB_XX_ALIGN_DONE) != 0);
	sync_stat = (EFX_OWORD_FIELD(oword, FRF_AB_XX_SYNC_STAT0) != 0 &&
	    EFX_OWORD_FIELD(oword, FRF_AB_XX_SYNC_STAT1) != 0 &&
	    EFX_OWORD_FIELD(oword, FRF_AB_XX_SYNC_STAT2) != 0 &&
	    EFX_OWORD_FIELD(oword, FRF_AB_XX_SYNC_STAT3) != 0);

	*upp = (align_done && sync_stat);

	EFX_SET_OWORD_FIELD(oword, FRF_AB_XX_COMMA_DET_CH0, 1);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_XX_COMMA_DET_CH1, 1);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_XX_COMMA_DET_CH2, 1);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_XX_COMMA_DET_CH3, 1);

	EFX_SET_OWORD_FIELD(oword, FRF_AB_XX_CHAR_ERR_CH0, 1);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_XX_CHAR_ERR_CH1, 1);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_XX_CHAR_ERR_CH2, 1);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_XX_CHAR_ERR_CH3, 1);

	EFX_SET_OWORD_FIELD(oword, FRF_AB_XX_DISPERR_CH0, 1);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_XX_DISPERR_CH1, 1);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_XX_DISPERR_CH2, 1);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_XX_DISPERR_CH3, 1);

	EFX_BAR_WRITEO(enp, FR_AB_XX_CORE_STAT_REG, &oword);
}

			void
falcon_xmac_poll(
	__in		efx_nic_t *enp,
	__out		boolean_t *mac_upp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_phy_ops_t *epop = epp->ep_epop;
	boolean_t mac_up = B_TRUE;
	boolean_t ok;

	/* Poll the mac link state if required */
	if (epp->ep_mac_poll_needed) {
		falcon_xmac_downlink_check(enp, &mac_up);
		if (!mac_up)
			goto done;
#if EFSYS_OPT_LOOPBACK
		/* PHYXS link state irrelevent in MAC loopback */
		if ((1 << epp->ep_loopback_type) & EFX_LOOPBACK_MAC_MASK)
			goto done;
#endif
		if (epop->epo_uplink_check != NULL) {
			if (epop->epo_uplink_check(enp, &ok) == 0)
				mac_up = ok;
		}
	}

done:
	*mac_upp = mac_up;

	/*
	 * If the XAUI link (and therefore wireside link) are UP, then we
	 * can use XGMT interrupts rather than polling the link state to spot
	 * downwards transitions. To spot upwards transitions, we must poll
	 */
	epp->ep_mac_poll_needed = !mac_up;
	if (mac_up) {
		efx_oword_t oword;

		/* ACK the interrupt */
		EFX_BAR_READO(enp, FR_AB_XM_MGT_INT_REG, &oword);
	} else {
		(void) falcon_xmac_xgxs_reset(enp);
	}
}

#endif	/* EFSYS_OPT_MAC_FALCON_XMAC */
