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
#include "xphy.h"
#include "qt2022c2.h"
#include "qt2022c2_impl.h"

#if EFSYS_OPT_PHY_QT2022C2

static	__checkReturn	int
qt2022c2_led_cfg(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t led1;
	efx_word_t led2;
	efx_word_t led3;
	int rc;

#if EFSYS_OPT_PHY_LED_CONTROL

	switch (epp->ep_phy_led_mode) {
	case EFX_PHY_LED_DEFAULT:
		EFX_POPULATE_WORD_2(led1, PMA_PMD_LED_CFG, LED_CFG_LSA_DECODE,
		    PMA_PMD_LED_PATH, LED_PATH_RX_DECODE);
		EFX_POPULATE_WORD_2(led2, PMA_PMD_LED_CFG, LED_CFG_LSA_DECODE,
		    PMA_PMD_LED_PATH, LED_PATH_TX_DECODE);
		EFX_POPULATE_WORD_1(led3, PMA_PMD_LED_CFG, LED_CFG_OFF_DECODE);
		break;

	case EFX_PHY_LED_OFF:
		EFX_POPULATE_WORD_1(led1, PMA_PMD_LED_CFG, LED_CFG_OFF_DECODE);
		EFX_POPULATE_WORD_1(led2, PMA_PMD_LED_CFG, LED_CFG_OFF_DECODE);
		EFX_POPULATE_WORD_1(led3, PMA_PMD_LED_CFG, LED_CFG_OFF_DECODE);
		break;

	case EFX_PHY_LED_ON:
		EFX_POPULATE_WORD_1(led1, PMA_PMD_LED_CFG, LED_CFG_ON_DECODE);
		EFX_POPULATE_WORD_1(led2, PMA_PMD_LED_CFG, LED_CFG_ON_DECODE);
		EFX_POPULATE_WORD_1(led3, PMA_PMD_LED_CFG, LED_CFG_ON_DECODE);
		break;

	default:
		EFSYS_ASSERT(B_FALSE);
		break;
	}

#else	/* EFSYS_OPT_PHY_LED_CONTROL */

	EFX_POPULATE_WORD_2(led1, PMA_PMD_LED_CFG, LED_CFG_LSA_DECODE,
	    PMA_PMD_LED_PATH, LED_PATH_RX_DECODE);
	EFX_POPULATE_WORD_2(led2, PMA_PMD_LED_CFG, LED_CFG_LSA_DECODE,
	    PMA_PMD_LED_PATH, LED_PATH_TX_DECODE);
	EFX_POPULATE_WORD_1(led3, PMA_PMD_LED_CFG, LED_CFG_OFF_DECODE);

#endif	/* EFSYS_OPT_PHY_LED_CONTROL */

	if ((rc = falcon_mdio_write(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_LED1_REG, &led1)) != 0)
		goto fail1;

	if ((rc = falcon_mdio_write(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_LED2_REG, &led2)) != 0)
		goto fail2;

	if ((rc = falcon_mdio_write(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_LED3_REG, &led3)) != 0)
		goto fail3;

	return (0);

fail3:
	EFSYS_PROBE(fail2);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#if EFSYS_OPT_LOOPBACK
static	__checkReturn	int
qt2022c2_loopback_cfg(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	int rc;

	switch (epp->ep_loopback_type) {
	case EFX_LOOPBACK_PHY_XS: {
		efx_word_t word;

		if ((rc = falcon_mdio_read(enp, epp->ep_port, PHY_XS_MMD,
		    PHY_XS_VENDOR0_REG, &word)) != 0)
			goto fail1;

		EFX_SET_WORD_FIELD(word, XAUI_SYSTEM_LOOPBACK, 1);

		if ((rc = falcon_mdio_write(enp, epp->ep_port, PHY_XS_MMD,
		    PHY_XS_VENDOR0_REG, &word)) != 0)
			goto fail2;

		break;
	}
	case EFX_LOOPBACK_PCS:
		if ((rc = xphy_mmd_loopback_set(enp, epp->ep_port, PCS_MMD,
		    B_TRUE)) != 0)
			goto fail1;

		break;

	case EFX_LOOPBACK_PMA_PMD:
		if ((rc = xphy_mmd_loopback_set(enp, epp->ep_port, PMA_PMD_MMD,
		    B_TRUE)) != 0)
			goto fail1;

		break;

	default:
		break;
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}
#endif	/* EFSYS_OPT_LOOPBACK */

	__checkReturn	int
qt2022c2_reset(
	__in		efx_nic_t *enp)
{
	/* Pull the external reset line */
	falcon_nic_phy_reset(enp);

	return (0);
}

	__checkReturn	int
qt2022c2_reconfigure(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	int rc;

	if ((rc = xphy_pkg_wait(enp, epp->ep_port, QT2022C2_MMD_MASK)) != 0)
		goto fail1;

	if ((rc = qt2022c2_led_cfg(enp)) != 0)
		goto fail2;

	EFSYS_ASSERT3U(epp->ep_adv_cap_mask, ==, QT2022C2_ADV_CAP_MASK);

#if EFSYS_OPT_LOOPBACK
	if ((rc = qt2022c2_loopback_cfg(enp)) != 0)
		goto fail3;
#endif	/* EFSYS_OPT_LOOPBACK */

	return (0);

#if EFSYS_OPT_LOOPBACK
fail3:
	EFSYS_PROBE(fail3);
#endif	/* EFSYS_OPT_LOOPBACK */

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn	int
qt2022c2_verify(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	int rc;

	if ((rc = xphy_pkg_verify(enp, epp->ep_port, QT2022C2_MMD_MASK)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn	int
qt2022c2_uplink_check(
	__in		efx_nic_t *enp,
	__out		boolean_t *upp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	if (epp->ep_mac_type != EFX_MAC_FALCON_XMAC) {
		rc = ENOTSUP;
		goto fail1;
	}

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PHY_XS_MMD,
	    PHY_XS_LANE_STATUS_REG, &word)) != 0)
		goto fail2;

	*upp = ((EFX_WORD_FIELD(word, PHY_XS_ALIGNED) != 0) &&
	    (EFX_WORD_FIELD(word, PHY_XS_LANE0_SYNC) != 0) &&
	    (EFX_WORD_FIELD(word, PHY_XS_LANE1_SYNC) != 0) &&
	    (EFX_WORD_FIELD(word, PHY_XS_LANE2_SYNC) != 0) &&
	    (EFX_WORD_FIELD(word, PHY_XS_LANE3_SYNC) != 0));

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn	int
qt2022c2_downlink_check(
	__in		efx_nic_t *enp,
	__out		efx_link_mode_t *modep,
	__out		unsigned int *fcntlp,
	__out		uint32_t *lp_cap_maskp)
{
	efx_port_t *epp = &(enp->en_port);
	boolean_t up;
	int rc;

#if EFSYS_OPT_LOOPBACK
	switch (epp->ep_loopback_type) {
	case EFX_LOOPBACK_PHY_XS:
		rc = xphy_mmd_fault(enp, epp->ep_port, &up);
		if (rc != 0)
			goto fail1;

		*modep = (up) ? EFX_LINK_10000FDX : EFX_LINK_DOWN;
		goto done;

	case EFX_LOOPBACK_PCS:
		rc = xphy_mmd_check(enp, epp->ep_port, PHY_XS_MMD, &up);
		if (rc != 0)
			goto fail1;

		*modep = (up) ? EFX_LINK_10000FDX : EFX_LINK_DOWN;
		goto done;

	default:
		break;
	}
#endif /* EFSYS_OPT_LOOPBACK */

	if ((rc = xphy_mmd_check(enp, epp->ep_port, PCS_MMD, &up)) != 0)
		goto fail1;

	*modep = (up) ? EFX_LINK_10000FDX : EFX_LINK_DOWN;

#if EFSYS_OPT_LOOPBACK
done:
#endif
	*fcntlp = epp->ep_fcntl;
	*lp_cap_maskp = epp->ep_lp_cap_mask;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn	int
qt2022c2_oui_get(
	__in		efx_nic_t *enp,
	__out		uint32_t *ouip)
{
	efx_port_t *epp = &(enp->en_port);
	int rc;

	if ((rc = xphy_mmd_oui_get(enp, epp->ep_port, PMA_PMD_MMD, ouip)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#if EFSYS_OPT_PHY_STATS

#define	QT2022C2_STAT_SET(_stat, _mask, _id, _val)			\
	do {								\
		(_mask) |= (1 << (_id));				\
		(_stat)[_id] = (uint32_t)(_val);			\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

static	__checkReturn			int
qt2022c2_pma_pmd_stats_update(
	__in				efx_nic_t *enp,
	__inout				uint64_t *maskp,
	__inout_ecount(EFX_PHY_NSTATS)	uint32_t *stat)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_STATUS1_REG, &word)) != 0)
		goto fail1;

	QT2022C2_STAT_SET(stat, *maskp, EFX_PHY_STAT_PMA_PMD_LINK_UP,
	    (EFX_WORD_FIELD(word, PMA_PMD_LINK_UP) != 0) ? 1 : 0);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_STATUS2_REG, &word)) != 0)
		goto fail2;

	QT2022C2_STAT_SET(stat, *maskp, EFX_PHY_STAT_PMA_PMD_RX_FAULT,
	    (EFX_WORD_FIELD(word, PMA_PMD_RX_FAULT) != 0) ? 1 : 0);
	QT2022C2_STAT_SET(stat, *maskp, EFX_PHY_STAT_PMA_PMD_TX_FAULT,
	    (EFX_WORD_FIELD(word, PMA_PMD_TX_FAULT) != 0) ? 1 : 0);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static	__checkReturn			int
qt2022c2_pcs_stats_update(
	__in				efx_nic_t *enp,
	__inout				uint64_t *maskp,
	__inout_ecount(EFX_PHY_NSTATS)	uint32_t *stat)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
	    PCS_STATUS1_REG, &word)) != 0)
		goto fail1;

	QT2022C2_STAT_SET(stat, *maskp, EFX_PHY_STAT_PCS_LINK_UP,
	    (EFX_WORD_FIELD(word, PCS_LINK_UP) != 0) ? 1 : 0);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
	    PCS_STATUS2_REG, &word)) != 0)
		goto fail2;

	QT2022C2_STAT_SET(stat, *maskp, EFX_PHY_STAT_PCS_RX_FAULT,
	    (EFX_WORD_FIELD(word, PCS_RX_FAULT) != 0) ? 1 : 0);
	QT2022C2_STAT_SET(stat, *maskp, EFX_PHY_STAT_PCS_TX_FAULT,
	    (EFX_WORD_FIELD(word, PCS_TX_FAULT) != 0) ? 1 : 0);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
	    PCS_10GBASE_R_STATUS2_REG, &word)) != 0)
		goto fail3;

	QT2022C2_STAT_SET(stat, *maskp, EFX_PHY_STAT_PCS_BER,
	    EFX_WORD_FIELD(word, PCS_BER));
	QT2022C2_STAT_SET(stat, *maskp, EFX_PHY_STAT_PCS_BLOCK_ERRORS,
	    EFX_WORD_FIELD(word, PCS_ERR));

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static	__checkReturn			int
qt2022c2_phy_xs_stats_update(
	__in				efx_nic_t *enp,
	__inout				uint64_t *maskp,
	__inout_ecount(EFX_PHY_NSTATS)	uint32_t *stat)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PHY_XS_MMD,
	    PHY_XS_STATUS1_REG, &word)) != 0)
		goto fail1;

	QT2022C2_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_LINK_UP,
	    (EFX_WORD_FIELD(word, PHY_XS_LINK_UP) != 0) ? 1 : 0);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PHY_XS_MMD,
	    PHY_XS_STATUS2_REG, &word)) != 0)
		goto fail2;

	QT2022C2_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_RX_FAULT,
	    (EFX_WORD_FIELD(word, PHY_XS_RX_FAULT) != 0) ? 1 : 0);
	QT2022C2_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_TX_FAULT,
	    (EFX_WORD_FIELD(word, PHY_XS_TX_FAULT) != 0) ? 1 : 0);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PHY_XS_MMD,
	    PHY_XS_LANE_STATUS_REG, &word)) != 0)
		goto fail3;

	QT2022C2_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_ALIGN,
	    (EFX_WORD_FIELD(word, PHY_XS_ALIGNED) != 0) ? 1 : 0);
	QT2022C2_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_SYNC_A,
	    (EFX_WORD_FIELD(word, PHY_XS_LANE0_SYNC) != 0) ? 1 : 0);
	QT2022C2_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_SYNC_B,
	    (EFX_WORD_FIELD(word, PHY_XS_LANE1_SYNC) != 0) ? 1 : 0);
	QT2022C2_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_SYNC_C,
	    (EFX_WORD_FIELD(word, PHY_XS_LANE2_SYNC) != 0) ? 1 : 0);
	QT2022C2_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_SYNC_D,
	    (EFX_WORD_FIELD(word, PHY_XS_LANE3_SYNC) != 0) ? 1 : 0);

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn			int
qt2022c2_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__out_ecount(EFX_PHY_NSTATS)	uint32_t *stat)
{
	efx_port_t *epp = &(enp->en_port);
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	uint32_t oui;
	uint64_t mask = 0;
	int rc;

	_NOTE(ARGUNUSED(esmp))

	if ((rc = xphy_mmd_oui_get(enp, epp->ep_port, PMA_PMD_MMD, &oui)) != 0)
		goto fail1;

	QT2022C2_STAT_SET(stat, mask, EFX_PHY_STAT_OUI, oui);

	if ((rc = qt2022c2_pma_pmd_stats_update(enp, &mask, stat)) != 0)
		goto fail2;

	if ((rc = qt2022c2_pcs_stats_update(enp, &mask, stat)) != 0)
		goto fail3;

	if ((rc = qt2022c2_phy_xs_stats_update(enp, &mask, stat)) != 0)
		goto fail4;

	/* Ensure all the supported statistics are up to date */
	EFSYS_ASSERT(mask == encp->enc_phy_stat_mask);

	return (0);

fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}
#endif	/* EFSYS_OPT_PHY_STATS */

#if EFSYS_OPT_PHY_PROPS

#if EFSYS_OPT_NAMES
		const char __cs *
qt2022c2_prop_name(
	__in	efx_nic_t *enp,
	__in	unsigned int id)
{
	_NOTE(ARGUNUSED(enp, id))

	EFSYS_ASSERT(B_FALSE);

	return (NULL);
}
#endif	/* EFSYS_OPT_NAMES */

	__checkReturn	int
qt2022c2_prop_get(
	__in		efx_nic_t *enp,
	__in		unsigned int id,
	__in		uint32_t flags,
	__out		uint32_t *valp)
{
	_NOTE(ARGUNUSED(enp, id, flags, valp))

	EFSYS_ASSERT(B_FALSE);

	return (ENOTSUP);
}

	__checkReturn	int
qt2022c2_prop_set(
	__in		efx_nic_t *enp,
	__in		unsigned int id,
	__in		uint32_t val)
{
	_NOTE(ARGUNUSED(enp, id, val))

	EFSYS_ASSERT(B_FALSE);

	return (ENOTSUP);
}
#endif	/* EFSYS_OPT_PHY_PROPS */

#endif	/* EFSYS_OPT_PHY_QT2022C2 */
