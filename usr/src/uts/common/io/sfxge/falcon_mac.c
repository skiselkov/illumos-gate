/*
 * Copyright 2007-2013 Solarflare Communications Inc.  All rights reserved.
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

#if EFSYS_OPT_FALCON

#if EFSYS_OPT_MAC_FALCON_GMAC
#include "falcon_gmac.h"
#endif

#if EFSYS_OPT_MAC_FALCON_XMAC
#include "falcon_xmac.h"
#endif

	__checkReturn	int
falcon_mac_poll(
	__in		efx_nic_t *enp,
	__out		efx_link_mode_t	*link_modep)
{
	efx_port_t *epp = &(enp->en_port);
	efx_phy_ops_t *epop = epp->ep_epop;
	efx_link_mode_t	link_mode;
	unsigned int fcntl;
	uint32_t lp_cap_mask;
	boolean_t mac_up = B_TRUE;
	boolean_t reconfigure_mac = B_FALSE;
	int rc;

	/* Poll PHY for link state changes */
	rc = epop->epo_downlink_check(enp, &link_mode, &fcntl, &lp_cap_mask);
	if (rc != 0) {
		fcntl = epp->ep_fcntl;
		lp_cap_mask = epp->ep_lp_cap_mask;
		link_mode = EFX_LINK_UNKNOWN;
	}

#if EFSYS_OPT_LOOPBACK
	/* Ignore the phy link state in MAC level loopback */
	switch (epp->ep_loopback_type) {
	case EFX_LOOPBACK_GMAC:
		link_mode = EFX_LINK_1000FDX;
		break;

	case EFX_LOOPBACK_XGMII:
	case EFX_LOOPBACK_XGXS:
	case EFX_LOOPBACK_XAUI:
		link_mode = EFX_LINK_10000FDX;
		break;

	default:
		break;
	}

	if (epp->ep_loopback_type != EFX_LOOPBACK_OFF) {
		/*
		 * We've already configured the correct MAC for the correct
		 * speed, so all we need to do is wait for the phy loopback
		 * to come up. Keep ep_link_mode et al. at the values forced
		 * by falcon_mac_loopback_set(), but return the current value
		 * of link up vs link down.
		 */
		goto poll_mac;
	}
#endif

	/* Hook in the correct MAC and reconfigure for the new link speed. */
	epp->ep_lp_cap_mask = lp_cap_mask;

	if (link_mode != epp->ep_link_mode) {
		epp->ep_link_mode = link_mode;

		if ((rc = efx_mac_select(enp)) != 0)
			goto fail1;

		reconfigure_mac = B_TRUE;
	}

	if (fcntl != epp->ep_fcntl) {
		epp->ep_fcntl = fcntl;
		reconfigure_mac = B_TRUE;
	}

	if (reconfigure_mac)
		epp->ep_emop->emo_reconfigure(enp);

#if EFSYS_OPT_LOOPBACK
poll_mac:
#endif
	/* The XMAC requires additional polling */
	if (epp->ep_mac_type == EFX_MAC_FALCON_XMAC)
		falcon_xmac_poll(enp, &mac_up);
	else
		mac_up = B_TRUE;

	epp->ep_mac_up = mac_up;
	*link_modep = link_mode;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn	int
falcon_mac_up(
	__in		efx_nic_t *enp,
	__out		boolean_t *mac_upp)
{
	efx_port_t *epp = &(enp->en_port);

	/* falcon_mac_poll() *must* be run on Falcon */
	*mac_upp = epp->ep_mac_up;

	return (0);
}

			void
falcon_mac_wrapper_enable(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_oword_t oword;
	uint32_t speed;
	boolean_t drain = epp->ep_mac_drain;

	switch (epp->ep_link_mode) {
	case EFX_LINK_100FDX:
	case EFX_LINK_100HDX:
		speed = FRF_AB_MAC_SPEED_100M;
		break;

	case EFX_LINK_1000FDX:
	case EFX_LINK_1000HDX:
		speed = FRF_AB_MAC_SPEED_1G;
		break;

	case EFX_LINK_10000FDX:
		speed = FRF_AB_MAC_SPEED_10G;
		break;

	default:
#if EFSYS_OPT_LOOPBACK
		EFSYS_ASSERT3U(epp->ep_loopback_type, ==, EFX_LOOPBACK_OFF);
#endif	/* EFSYS_OPT_LOOPBACK */
		drain = B_TRUE;
		speed = FRF_AB_MAC_SPEED_10M;
	};

	/* Bring the mac wrapper out of reset and configure it */
	switch (epp->ep_mac_type) {
	case EFX_MAC_FALCON_GMAC:
		EFX_POPULATE_OWORD_6(oword,
		    FRF_AB_MAC_XOFF_VAL, 0xffff,
		    FRF_AB_MAC_XG_DISTXCRC, 0,
		    FRF_AB_MAC_BCAD_ACPT, (epp->ep_brdcst) ? 1 : 0,
		    FRF_AB_MAC_UC_PROM, (epp->ep_unicst) ? 1 : 0,
		    FRF_AB_MAC_LINK_STATUS, 1,
		    FRF_AB_MAC_SPEED, speed);
		break;

	case EFX_MAC_FALCON_XMAC:
		EFX_POPULATE_OWORD_6(oword,
		    FRF_AB_MAC_XOFF_VAL, 0xffff,
		    FRF_AB_MAC_XG_DISTXCRC, 0,
		    FRF_AB_MAC_BCAD_ACPT, 1,
		    FRF_AB_MAC_UC_PROM, 0,
		    FRF_AB_MAC_LINK_STATUS, 1,
		    FRF_AB_MAC_SPEED, speed);
		break;

	default:
		EFSYS_ASSERT(B_FALSE);
		break;
	}

	/* Open TX_DRAIN if the link is up */
	if (enp->en_family == EFX_FAMILY_FALCON)
		EFX_SET_OWORD_FIELD(oword, FRF_BB_TXFIFO_DRAIN_EN,
				    drain ? 1 : 0);
	EFX_BAR_WRITEO(enp, FR_AB_MAC_CTRL_REG, &oword);

	/* Push multicast hash. Set the broadcast bit (0xff) appropriately */
	EFX_BAR_WRITEO(enp, FR_AB_MAC_MC_HASH0_REG,
	    &(epp->ep_multicst_hash[0]));
	memcpy(&oword, &(epp->ep_multicst_hash[1]), sizeof (oword));
	if (epp->ep_brdcst)
		EFX_SET_OWORD_BIT(oword, 0x7f);
	EFX_BAR_WRITEO(enp, FR_AB_MAC_MC_HASH1_REG, &oword);

	/* Configure RX <-> mac_wrapper link */
	EFX_BAR_READO(enp, FR_AZ_RX_CFG_REG, &oword);
	EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_XON_TX_TH, 25);
	EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_XOFF_TX_TH, 20);
	/* Send XON and XOFF at ~3 * max MTU away from empty/full */
	EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_XON_MAC_TH, 27648 >> 8);
	EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_XOFF_MAC_TH, 54272 >> 8);
	EFX_SET_OWORD_FIELD(oword, FRF_AZ_RX_XOFF_MAC_EN,
	    (epp->ep_fcntl & EFX_FCNTL_GENERATE) ? 1 : 0);

	if (enp->en_family == EFX_FAMILY_FALCON)
		EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_INGR_EN, drain ? 0 : 1);

	EFX_BAR_WRITEO(enp, FR_AZ_RX_CFG_REG, &oword);
}

	__checkReturn	int
falcon_mac_wrapper_disable(
	__in		efx_nic_t *enp)
{
	efx_oword_t oword;
	int rc;

	EFX_BAR_READO(enp, FR_AZ_RX_CFG_REG, &oword);
	EFX_SET_OWORD_FIELD(oword, FRF_BZ_RX_INGR_EN, 0);
	EFX_BAR_WRITEO(enp, FR_AZ_RX_CFG_REG, &oword);

	/* There is no point in draining more than once */
	EFX_BAR_READO(enp, FR_AB_MAC_CTRL_REG, &oword);
	if (EFX_OWORD_FIELD(oword, FRF_BB_TXFIFO_DRAIN_EN))
		return (0);

	if ((rc = falcon_nic_mac_reset(enp)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#if EFSYS_OPT_LOOPBACK

	__checkReturn	int
falcon_mac_loopback_set(
	__in		efx_nic_t *enp,
	__in		efx_link_mode_t link_mode,
	__in		efx_loopback_type_t loopback_type)
{
	efx_port_t *epp = &(enp->en_port);
	efx_phy_ops_t *epop = epp->ep_epop;
	const efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_loopback_type_t old_loopback_type;
	efx_link_mode_t old_loopback_link_mode;
	uint32_t phy_loopback_mask;
	boolean_t phy_loopback_changed;
	int rc;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	old_loopback_type = epp->ep_loopback_type;
	old_loopback_link_mode = epp->ep_loopback_link_mode;

	phy_loopback_mask = EFX_LOOPBACK_MASK &
		~(EFX_LOOPBACK_MAC_MASK | (1 << EFX_LOOPBACK_OFF));
	phy_loopback_changed = (phy_loopback_mask &
		((1 << loopback_type) ^ (1 << epp->ep_loopback_type))) != 0;
	epp->ep_loopback_type = loopback_type;
	epp->ep_loopback_link_mode = link_mode;

	if (loopback_type == EFX_LOOPBACK_OFF)
		link_mode = EFX_LINK_DOWN;
	else if (link_mode == EFX_LINK_UNKNOWN) {
		for (link_mode = EFX_LINK_NMODES - 1;
		    link_mode > EFX_LINK_UNKNOWN; --link_mode) {
		if ((1 << loopback_type) &
		    encp->enc_loopback_types[link_mode])
			break;
		}
	}

	EFSYS_ASSERT(((1 << loopback_type) & ~FALCON_GMAC_LOOPBACK_MASK) ||
			link_mode == EFX_LINK_1000FDX);
	EFSYS_ASSERT(((1 << loopback_type) & ~FALCON_XMAC_LOOPBACK_MASK) ||
			link_mode == EFX_LINK_10000FDX);

	/*
	 * In loopback, a PHY typically requires the correct MAC and link
	 * to be initialized before they will report link up. Determine
	 * the expected link speed and select the correct MAC. Ensure that
	 * ep_link_mode != LINK_DOWN in loopback so that TXDRAIN isn't enabled,
	 * because we'll never again run efx_mac_select() to subsequently
	 * reset the EM block.
	 */
	epp->ep_link_mode = link_mode;
	if ((rc = efx_mac_select(enp)) != 0)
		goto fail1;

	/*
	 * Don't reset and reconfigure the PHY unless it's
	 * configuration has actually changed
	 */
	if (phy_loopback_changed) {
		if ((rc = epop->epo_reset(enp)) != 0)
			goto fail2;

		EFSYS_ASSERT(enp->en_reset_flags & EFX_RESET_PHY);
		enp->en_reset_flags &= ~EFX_RESET_PHY;

		if ((rc = epop->epo_reconfigure(enp)) != 0)
			goto fail3;
	}

	epp->ep_emop->emo_reconfigure(enp);

	/* Ensure that the MAC is subsequently polled */
	epp->ep_mac_poll_needed = B_TRUE;
	epp->ep_mac_up = B_FALSE;

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	epp->ep_loopback_type = old_loopback_type;
	epp->ep_loopback_link_mode = old_loopback_link_mode;
	epp->ep_link_mode = EFX_LINK_DOWN;

	return (rc);
}

#endif	/* EFSYS_OPT_LOOPBACK */

#if EFSYS_OPT_MAC_STATS

	__checkReturn			int
falcon_mac_stats_upload(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp)
{
	efx_oword_t oword;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	EFX_POPULATE_OWORD_3(oword,
	    FRF_AB_MAC_STAT_DMA_ADR_DW0, EFSYS_MEM_ADDR(esmp) & 0xffffffff,
	    FRF_AB_MAC_STAT_DMA_ADR_DW1, EFSYS_MEM_ADDR(esmp) >> 32,
	    FRF_AB_MAC_STAT_DMA_CMD, 1);

	EFX_BAR_WRITEO(enp, FR_AB_MAC_STAT_DMA_REG, &oword);

	return (0);
}

#endif	/* EFSYS_OPT_MAC_STATS */

#endif	/* EFSYS_OPT_FALCON */
