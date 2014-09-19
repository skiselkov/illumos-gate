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
#include "falcon_gmac.h"
#include "falcon_stats.h"

#if EFSYS_OPT_MAC_FALCON_GMAC

	__checkReturn	int
falcon_gmac_reset(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_oword_t oword;
	int rc;

	EFSYS_ASSERT3U(epp->ep_mac_type, ==, EFX_MAC_FALCON_GMAC);

	EFSYS_PROBE(reset);

	/* Ensure that the GMAC registers are accessible */
	EFX_BAR_READO(enp, FR_AB_NIC_STAT_REG, &oword);
	EFX_SET_OWORD_FIELD(oword, FRF_BB_EE_STRAP_EN, 1);
	EFX_SET_OWORD_FIELD(oword, FRF_BB_EE_STRAP, 0x3);
	EFX_BAR_WRITEO(enp, FR_AB_NIC_STAT_REG, &oword);

	if ((rc = falcon_mac_wrapper_disable(enp)) != 0)
	    goto fail1;

	enp->en_reset_flags |= EFX_RESET_MAC;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static			void
falcon_gmac_core_reconfigure(
	__in	efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_oword_t oword;
	boolean_t full_duplex;
	boolean_t byte_mode;

	full_duplex = (epp->ep_link_mode == EFX_LINK_100FDX ||
	    epp->ep_link_mode == EFX_LINK_1000FDX);
	byte_mode = (epp->ep_link_mode == EFX_LINK_1000HDX ||
	    epp->ep_link_mode == EFX_LINK_1000FDX);
#if EFSYS_OPT_LOOPBACK
	byte_mode |= (epp->ep_loopback_type == EFX_LOOPBACK_GMAC);

	EFX_POPULATE_OWORD_5(oword,
	    FRF_AB_GM_LOOP,
	    (epp->ep_loopback_type == EFX_LOOPBACK_GMAC) ? 1 : 0,
	    FRF_AB_GM_TX_EN, 1,
	    FRF_AB_GM_TX_FC_EN, (epp->ep_fcntl & EFX_FCNTL_GENERATE) ? 1 : 0,
	    FRF_AB_GM_RX_EN, 1,
	    FRF_AB_GM_RX_FC_EN, (epp->ep_fcntl & EFX_FCNTL_RESPOND) ? 1 : 0);

#else	/* EFSYS_OPT_LOOPBACK */

	EFX_POPULATE_OWORD_4(oword,
	    FRF_AB_GM_TX_EN, 1,
	    FRF_AB_GM_TX_FC_EN, (epp->ep_fcntl & EFX_FCNTL_GENERATE) ? 1 : 0,
	    FRF_AB_GM_RX_EN, 1,
	    FRF_AB_GM_RX_FC_EN, (epp->ep_fcntl & EFX_FCNTL_RESPOND) ? 1 : 0);

#endif	/* EFSYS_OPT_LOOPBACK */

	EFX_BAR_WRITEO(enp, FR_AB_GM_CFG1_REG, &oword);
	EFSYS_SPIN(10);

	EFX_POPULATE_OWORD_4(oword,
	    FRF_AB_GM_IF_MODE, (byte_mode) ?
	    FRF_AB_GM_IF_MODE_BYTE_MODE : FRF_AB_GM_IF_MODE_NIBBLE_MODE,
	    FRF_AB_GM_PAD_CRC_EN, 1,
	    FRF_AB_GM_FD, full_duplex ? 1 : 0,
	    FRF_AB_GM_PAMBL_LEN, 7);

	EFX_BAR_WRITEO(enp, FR_AB_GM_CFG2_REG, &oword);
	EFSYS_SPIN(10);

	EFX_POPULATE_OWORD_1(oword, FRF_AB_GM_MAX_FLEN, epp->ep_mac_pdu);

	EFX_BAR_WRITEO(enp, FR_AB_GM_MAX_FLEN_REG, &oword);
	EFSYS_SPIN(10);

	EFX_POPULATE_OWORD_5(oword,
	    FRF_AB_GMF_FTFENREQ, 1,
	    FRF_AB_GMF_STFENREQ, 1,
	    FRF_AB_GMF_FRFENREQ, 1,
	    FRF_AB_GMF_SRFENREQ, 1,
	    FRF_AB_GMF_WTMENREQ, 1);

	EFX_BAR_WRITEO(enp, FR_AB_GMF_CFG0_REG, &oword);
	EFSYS_SPIN(10);

	EFX_POPULATE_OWORD_2(oword,
	    FRF_AB_GMF_CFGFRTH, 0x12,
	    FRF_AB_GMF_CFGXOFFRTX, 0xffff);

	EFX_BAR_WRITEO(enp, FR_AB_GMF_CFG1_REG, &oword);
	EFSYS_SPIN(10);

	EFX_POPULATE_OWORD_2(oword,
	    FRF_AB_GMF_CFGHWM, 0x3f,
	    FRF_AB_GMF_CFGLWM, 0x0a);

	EFX_BAR_WRITEO(enp, FR_AB_GMF_CFG2_REG, &oword);
	EFSYS_SPIN(10);

	EFX_POPULATE_OWORD_2(oword,
	    FRF_AB_GMF_CFGHWMFT, 0x1c,
	    FRF_AB_GMF_CFGFTTH, 0x08);

	EFX_BAR_WRITEO(enp, FR_AB_GMF_CFG3_REG, &oword);
	EFSYS_SPIN(10);

	EFX_POPULATE_OWORD_1(oword, FRF_AB_GMF_HSTFLTRFRM, 0x1000);

	EFX_BAR_WRITEO(enp, FR_AB_GMF_CFG4_REG, &oword);
	EFSYS_SPIN(10);

	EFX_BAR_READO(enp, FR_AB_GMF_CFG5_REG, &oword);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_GMF_CFGBYTMODE, byte_mode ? 1 : 0);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_GMF_CFGHDPLX, full_duplex ? 0 : 1);
	EFX_SET_OWORD_FIELD(oword,
	    FRF_AB_GMF_HSTDRPLT64, full_duplex ? 0 : 1);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_GMF_HSTFLTRFRMDC, 0x3efff);
	EFX_BAR_WRITEO(enp, FR_AB_GMF_CFG5_REG, &oword);
	EFSYS_SPIN(10);

	EFX_POPULATE_OWORD_4(oword,
	    FRF_AB_GM_ADR_B0, epp->ep_mac_addr[5],
	    FRF_AB_GM_ADR_B1, epp->ep_mac_addr[4],
	    FRF_AB_GM_ADR_B2, epp->ep_mac_addr[3],
	    FRF_AB_GM_ADR_B3, epp->ep_mac_addr[2]);

	EFX_BAR_WRITEO(enp, FR_AB_GM_ADR1_REG, &oword);
	EFSYS_SPIN(10);

	EFX_POPULATE_OWORD_2(oword,
	    FRF_AB_GM_ADR_B4, epp->ep_mac_addr[1],
	    FRF_AB_GM_ADR_B5, epp->ep_mac_addr[0]);

	EFX_BAR_WRITEO(enp, FR_AB_GM_ADR2_REG, &oword);
	EFSYS_SPIN(10);
}

	__checkReturn	int
falcon_gmac_downlink_check(
	__in		efx_nic_t *enp,
	__out		boolean_t *upp)
{
	efx_port_t *epp = &(enp->en_port);

	_NOTE(ARGUNUSED(upp))

	EFSYS_ASSERT3U(epp->ep_mac_type, ==, EFX_MAC_FALCON_GMAC);

	return (ENOTSUP);
}

	__checkReturn	int
falcon_gmac_reconfigure(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);

	EFSYS_ASSERT3U(epp->ep_mac_type, ==, EFX_MAC_FALCON_GMAC);
	EFSYS_ASSERT3U(epp->ep_link_mode, !=, EFX_LINK_10000FDX);

	EFSYS_PROBE(reconfigure);

	falcon_gmac_core_reconfigure(enp);
	falcon_mac_wrapper_enable(enp);

	return (0);
}

		void
falcon_gmac_downlink_reset(
	__in	efx_nic_t *enp,
	__in	boolean_t hold)
{
	_NOTE(ARGUNUSED(enp, hold))
}

#if EFSYS_OPT_MAC_STATS
static		uint32_t
falcon_gmac_stat_read(
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

#define	GMAC_STAT_READ(_esmp, _id)					\
	falcon_gmac_stat_read((_esmp), G_STAT_OFFSET(_id),		\
	    G_STAT_WIDTH(_id))

#define	GMAC_STAT_INCR(_esmp, _stat, _id)				\
	do {								\
		delta = GMAC_STAT_READ(_esmp, _id);			\
		EFSYS_STAT_INCR(&(_stat), delta);			\
	_NOTE(CONSTANTCONDITION)					\
	} while (0)

#define	GMAC_STAT_DECR(_esmp, _stat, _id)				\
	do {								\
		delta = GMAC_STAT_READ(_esmp, _id);			\
		EFSYS_STAT_DECR(&(_stat), delta);			\
	_NOTE(CONSTANTCONDITION)					\
	} while (0)

	__checkReturn			int
falcon_gmac_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__inout_ecount(EFX_MAC_NSTATS)	efsys_stat_t *stat,
	__out_opt			uint32_t *generationp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_oword_t oword;
	uint32_t delta;

	_NOTE(ARGUNUSED(generationp));
	EFSYS_ASSERT3U(epp->ep_mac_type, ==, EFX_MAC_FALCON_GMAC);

	/* Check the DMA flag */
	if (GMAC_STAT_READ(esmp, DMA_DONE) != DMA_IS_DONE)
		return (EAGAIN);
	EFSYS_MEM_READ_BARRIER();

	/* RX */
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_OCTETS], RX_GOOD_OCT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_OCTETS], RX_BAD_OCT);

	GMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_PKTS], RX_GOOD_LT_64_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_PKTS], RX_64_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_PKTS], RX_65_TO_127_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_PKTS], RX_128_TO_255_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_PKTS], RX_256_TO_511_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_PKTS], RX_512_TO_1023_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_PKTS], RX_1024_TO_15XX_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_PKTS], RX_15XX_TO_JUMBO_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_PKTS], RX_GT_JUMBO_PKT);

	GMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_UNICST_PKTS], RX_UCAST_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_MULTICST_PKTS], RX_MCAST_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_BRDCST_PKTS], RX_BCAST_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_PAUSE_PKTS], RX_PAUSE_PKT);

	GMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_LE_64_PKTS], RX_GOOD_LT_64_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_LE_64_PKTS], RX_BAD_LT_64_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_LE_64_PKTS], RX_64_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_65_TO_127_PKTS],
	    RX_65_TO_127_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_128_TO_255_PKTS],
	    RX_128_TO_255_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_256_TO_511_PKTS],
	    RX_256_TO_511_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_512_TO_1023_PKTS],
	    RX_512_TO_1023_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_1024_TO_15XX_PKTS],
	    RX_1024_TO_15XX_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_GE_15XX_PKTS],
	    RX_15XX_TO_JUMBO_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_GE_15XX_PKTS], RX_GT_JUMBO_PKT);

	GMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_ERRORS], RX_BAD_PKT);
	GMAC_STAT_DECR(esmp, stat[EFX_MAC_RX_ERRORS], RX_PAUSE_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_FCS_ERRORS],
	    RX_FCS_ERR_64_TO_15XX_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_FCS_ERRORS],
	    RX_FCS_ERR_15XX_TO_JUMBO_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_FCS_ERRORS],
	    RX_FCS_ERR_GT_JUMBO_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_DROP_EVENTS], RX_MISS_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_RX_FALSE_CARRIER_ERRORS],
	    RX_FALSE_CRS);

	EFX_BAR_READO(enp, FR_AZ_RX_NODESC_DROP_REG, &oword);
	delta = (uint32_t)EFX_OWORD_FIELD(oword, FRF_AZ_RX_NODESC_DROP_CNT);
	EFSYS_STAT_INCR(&(stat[EFX_MAC_RX_NODESC_DROP_CNT]), delta);

	/* TX */
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_OCTETS], TX_GOOD_BAD_OCT);

	GMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_PKTS], TX_LT_64_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_PKTS], TX_64_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_PKTS], TX_65_TO_127_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_PKTS], TX_128_TO_255_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_PKTS], TX_256_TO_511_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_PKTS], TX_512_TO_1023_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_PKTS], TX_1024_TO_15XX_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_PKTS], TX_15XX_TO_JUMBO_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_PKTS], TX_GT_JUMBO_PKT);

	GMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_UNICST_PKTS], TX_UCAST_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_MULTICST_PKTS], TX_MCAST_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_BRDCST_PKTS], TX_BCAST_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_PAUSE_PKTS], TX_PAUSE_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_LE_64_PKTS], TX_LT_64_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_LE_64_PKTS], TX_64_PKT);

	GMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_65_TO_127_PKTS],
	    TX_65_TO_127_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_128_TO_255_PKTS],
	    TX_128_TO_255_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_256_TO_511_PKTS],
	    TX_256_TO_511_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_512_TO_1023_PKTS],
	    TX_512_TO_1023_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_1024_TO_15XX_PKTS],
	    TX_1024_TO_15XX_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_GE_15XX_PKTS],
	    TX_15XX_TO_JUMBO_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_GE_15XX_PKTS],
	    TX_GT_JUMBO_PKT);

	GMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_ERRORS], TX_BAD_PKT);
	GMAC_STAT_DECR(esmp, stat[EFX_MAC_TX_ERRORS], TX_PAUSE_PKT);

	GMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_SGL_COL_PKTS], TX_SGL_COL_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_MULT_COL_PKTS], TX_MULT_COL_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_EX_COL_PKTS], TX_EX_COL_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_LATE_COL_PKTS], TX_LATE_COL);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_DEF_PKTS], TX_DEF_PKT);
	GMAC_STAT_INCR(esmp, stat[EFX_MAC_TX_EX_DEF_PKTS], TX_EX_DEF_PKT);

	return (0);
}

#endif	/* EFSYS_OPT_MAC_STATS */

#endif	/* EFSYS_OPT_MAC_FALCON_GMAC */
