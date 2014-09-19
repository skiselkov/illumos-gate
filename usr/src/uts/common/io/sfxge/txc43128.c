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
#include "txc43128.h"
#include "txc43128_impl.h"
#include "xphy.h"

#if EFSYS_OPT_PHY_TXC43128

static	__checkReturn	int
txc43128_bist_run(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	unsigned int count;
	int rc;

	/* Set the BIST type */
	EFX_POPULATE_WORD_1(word, BTYPE, TSDET_DECODE);
	if ((rc = falcon_mdio_write(enp, epp->ep_port, PCS_MMD, BCTL,
	    &word)) != 0)
		goto fail1;

	/* Enable BIST */
	EFX_SET_WORD_FIELD(word, BSTEN, 1);
	if ((rc = falcon_mdio_write(enp, epp->ep_port, PCS_MMD, BCTL,
	    &word)) != 0)
		goto fail2;

	/* Start BIST */
	EFX_SET_WORD_FIELD(word, BSTRT, 1);
	if ((rc = falcon_mdio_write(enp, epp->ep_port, PCS_MMD, BCTL,
	    &word)) != 0)
		goto fail3;

	EFX_SET_WORD_FIELD(word, BSTRT, 0);

	/* Spin for 100 us */
	EFSYS_SPIN(100);

	/* Stop BIST */
	EFX_SET_WORD_FIELD(word, BSTOP, 1);
	if ((rc = falcon_mdio_write(enp, epp->ep_port, PCS_MMD, BCTL,
	    &word)) != 0)
		goto fail4;

	/* Wait until BIST has stopped */
	count = 0;
	do {
		EFSYS_PROBE1(wait, unsigned int, count);

		/* Spin for 10 us */
		EFSYS_SPIN(10);

		/* Check whether the reset bit has cleared */
		if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
		    BCTL, &word)) != 0)
			goto fail5;

		if (EFX_WORD_FIELD(word, BSTOP) == 0)
			goto done;

	} while (++count < 20);

	rc = ETIMEDOUT;
	goto fail6;

done:
	/* Disable BIST */
	EFX_ZERO_WORD(word);
	if ((rc = falcon_mdio_write(enp, epp->ep_port, PCS_MMD, BCTL,
	    &word)) != 0)
		goto fail7;

	return (0);

fail7:
	EFSYS_PROBE(fail7);
fail6:
	EFSYS_PROBE(fail6);
fail5:
	EFSYS_PROBE(fail5);
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

static	__checkReturn	int
txc43128_bist_check(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	/* Check lane 0 frame count */
	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD, BRX0FRMCNT,
	    &word)) != 0)
		goto fail1;

	if (EFX_WORD_FIELD(word, EFX_WORD_0) == 0)
		goto fail2;

	/* Check lane 0 error count */
	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD, BRX0ERRCNT,
	    &word)) != 0)
		goto fail3;

	if (EFX_WORD_FIELD(word, EFX_WORD_0) != 0)
		goto fail4;

	/* Check lane 1 frame count */
	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD, BRX1FRMCNT,
	    &word)) != 0)
		goto fail5;

	if (EFX_WORD_FIELD(word, EFX_WORD_0) == 0)
		goto fail6;

	/* Check lane 1 error count */
	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD, BRX1ERRCNT,
	    &word)) != 0)
		goto fail7;

	if (EFX_WORD_FIELD(word, EFX_WORD_0) != 0)
		goto fail8;

	/* Check lane 2 frame count */
	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD, BRX2FRMCNT,
	    &word)) != 0)
		goto fail9;

	if (EFX_WORD_FIELD(word, EFX_WORD_0) == 0)
		goto fail10;

	/* Check lane 2 error count */
	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD, BRX2ERRCNT,
	    &word)) != 0)
		goto fail11;

	if (EFX_WORD_FIELD(word, EFX_WORD_0) != 0)
		goto fail12;

	/* Check lane 3 frame count */
	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD, BRX3FRMCNT,
	    &word)) != 0)
		goto fail13;

	if (EFX_WORD_FIELD(word, EFX_WORD_0) == 0)
		goto fail14;

	/* Check lane 3 error count */
	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD, BRX3ERRCNT,
	    &word)) != 0)
		goto fail15;

	if (EFX_WORD_FIELD(word, EFX_WORD_0) != 0)
		goto fail16;

	return (0);

fail16:
	EFSYS_PROBE(fail16);
fail15:
	EFSYS_PROBE(fail15);
fail14:
	EFSYS_PROBE(fail14);
fail13:
	EFSYS_PROBE(fail13);
fail12:
	EFSYS_PROBE(fail12);
fail11:
	EFSYS_PROBE(fail11);
fail10:
	EFSYS_PROBE(fail10);
fail9:
	EFSYS_PROBE(fail9);
fail8:
	EFSYS_PROBE(fail8);
fail7:
	EFSYS_PROBE(fail7);
fail6:
	EFSYS_PROBE(fail6);
fail5:
	EFSYS_PROBE(fail5);
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

static	__checkReturn	int
txc43128_bist(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	/* Set PMA into loopback */
	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD, MNDBLCFG,
	    &word)) != 0)
		goto fail1;

	EFX_SET_WORD_FIELD(word, LNALPBK, 1);

	if ((rc = falcon_mdio_write(enp, epp->ep_port, PCS_MMD, MNDBLCFG,
	    &word)) != 0)
		goto fail2;

	/* Run BIST */
	if ((rc = txc43128_bist_run(enp)) != 0)
		goto fail3;

	/* Check BIST */
	if ((rc = txc43128_bist_check(enp)) != 0)
		goto fail4;

	/* Turn off loopback */
	EFX_SET_WORD_FIELD(word, LNALPBK, 0);

	if ((rc = falcon_mdio_write(enp, epp->ep_port, PCS_MMD, MNDBLCFG,
	    &word)) != 0)
		goto fail5;

	return (0);

fail5:
	EFSYS_PROBE(fail5);
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

static	__checkReturn	int
txc43128_led_cfg(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	/* PIO11 allows us to control the red LED */

	EFX_POPULATE_WORD_1(word, PIO11FNC, PIOFNC_GPIO_DECODE);

	if ((rc = falcon_mdio_write(enp, epp->ep_port, PHY_XS_MMD,
	    PIOCFG, &word)) != 0)
		goto fail1;

	EFX_POPULATE_WORD_1(word, PIO11DIR, PIODIR_OUT_DECODE);

	if ((rc = falcon_mdio_write(enp, epp->ep_port, PHY_XS_MMD,
	    PIODIR, &word)) != 0)
		goto fail2;


#if EFSYS_OPT_PHY_LED_CONTROL

	switch (epp->ep_phy_led_mode) {
	case EFX_PHY_LED_DEFAULT:
	case EFX_PHY_LED_OFF:
		EFX_POPULATE_WORD_1(word, PIO11OUT, 0);
		break;

	case EFX_PHY_LED_ON:
		EFX_POPULATE_WORD_1(word, PIO11OUT, 1);
		break;

	default:
		EFSYS_ASSERT(B_FALSE);
		break;
	}

#else	/* EFSYS_OPT_PHY_LED_CONTROL */

	EFX_POPULATE_WORD_1(word, PIO11OUT, 0);

#endif	/* EFSYS_OPT_PHY_LED_CONTROL */

	if ((rc = falcon_mdio_write(enp, epp->ep_port, PHY_XS_MMD,
	    PIODO, &word)) != 0)
		goto fail3;

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static	__checkReturn	int
txc43128_preemphasis_cfg(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	/* XAUI settings */
	EFX_POPULATE_WORD_2(word, TXPRE02, 0, TXPRE13, 0);

	if ((rc = falcon_mdio_write(enp, epp->ep_port, PHY_XS_MMD,
	    ATXPRE0, &word)) != 0)
		goto fail1;

	if ((rc = falcon_mdio_write(enp, epp->ep_port, PHY_XS_MMD,
	    ATXPRE1, &word)) != 0)
		goto fail2;

	/* Line settings */
	EFX_POPULATE_WORD_2(word, TXPRE02, 2, TXPRE13, 2);

	if ((rc = falcon_mdio_write(enp, epp->ep_port, PMA_PMD_MMD,
	    ATXPRE0, &word)) != 0)
		goto fail3;

	if ((rc = falcon_mdio_write(enp, epp->ep_port, PMA_PMD_MMD,
	    ATXPRE1, &word)) != 0)
		goto fail4;

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

static	__checkReturn	int
txc43128_amplitude_cfg(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	/* XAUI settings */
	EFX_POPULATE_WORD_2(word, TXAMP02, 25, TXAMP13, 25);

	if ((rc = falcon_mdio_write(enp, epp->ep_port, PHY_XS_MMD,
	    ATXAMP0, &word)) != 0)
		goto fail1;

	if ((rc = falcon_mdio_write(enp, epp->ep_port, PHY_XS_MMD,
	    ATXAMP1, &word)) != 0)
		goto fail2;

	/* Line settings */
	EFX_POPULATE_WORD_2(word, TXAMP02, 12, TXAMP13, 12);

	if ((rc = falcon_mdio_write(enp, epp->ep_port, PMA_PMD_MMD,
	    ATXAMP0, &word)) != 0)
		goto fail3;

	if ((rc = falcon_mdio_write(enp, epp->ep_port, PMA_PMD_MMD,
	    ATXAMP1, &word)) != 0)
		goto fail4;

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

#if EFSYS_OPT_LOOPBACK
static	__checkReturn	int
txc43128_loopback_cfg(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	switch (epp->ep_loopback_type) {
	case EFX_LOOPBACK_PHY_XS:
		if ((rc = falcon_mdio_read(enp, epp->ep_port, PHY_XS_MMD,
		    MNDBLCFG, &word)) != 0)
			goto fail1;

		EFX_SET_WORD_FIELD(word, PXS8BLPBK, 1);

		if ((rc = falcon_mdio_write(enp, epp->ep_port, PHY_XS_MMD,
		    MNDBLCFG, &word)) != 0)
			goto fail2;

		break;

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
#endif	/* EFSYS_PHY_LOOPBACK */

static	__checkReturn	int
txc43128_logic_reset(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	unsigned int count;
	int rc;

	EFSYS_PROBE(logic_reset);

	/* Set the reset bit in the global command register */
	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD, GLCMD,
	    &word)) != 0)
		goto fail1;

	EFX_SET_WORD_FIELD(word, LMTSWRST, 1);

	if ((rc = falcon_mdio_write(enp, epp->ep_port, PCS_MMD, GLCMD,
	    &word)) != 0)
		goto fail2;

	count = 0;
	do {
		EFSYS_PROBE1(wait, unsigned int, count);

		/* Spin for 10 us */
		EFSYS_SPIN(10);

		/* Check whether the reset bit has cleared */
		if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
		    GLCMD, &word)) != 0)
			goto fail3;

		if (EFX_WORD_FIELD(word, LMTSWRST) == 0)
			goto done;

	} while (++count < 1000);

	rc = ETIMEDOUT;
	goto fail4;

done:
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

	__checkReturn	int
txc43128_reset(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	unsigned int count;
	int rc;

	/* Set the reset bit in the main control register */
	if ((rc = falcon_mdio_read(enp, epp->ep_port, PHY_XS_MMD, MNCTL,
	    &word)) != 0)
		goto fail1;

	EFX_SET_WORD_FIELD(word, MRST, 1);

	if ((rc = falcon_mdio_write(enp, epp->ep_port, PHY_XS_MMD, MNCTL,
	    &word)) != 0)
		goto fail2;

	count = 0;
	do {
		EFSYS_PROBE1(wait, unsigned int, count);

		/* Spin for 10 us */
		EFSYS_SPIN(10);

		/* Check whether the reset bit has cleared */
		if ((rc = falcon_mdio_read(enp, epp->ep_port, PHY_XS_MMD,
		    MNCTL, &word)) != 0)
			goto fail3;

		if (EFX_WORD_FIELD(word, MRST) == 0)
			goto done;

	} while (++count < 1000);

	rc = ETIMEDOUT;
	goto fail4;

done:
	/*
	 * Despite the global reset having completed, we will still be unable
	 * to get sensible values out of the 802.3ae registers for some
	 * unknown time. 250 ms seems to cover it but this is empirically
	 * determined.
	 */
	EFSYS_SLEEP(250000);

	enp->en_reset_flags |= EFX_RESET_PHY;

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

	__checkReturn	int
txc43128_reconfigure(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	int rc;

	if ((rc = xphy_pkg_wait(enp, epp->ep_port, TXC43128_MMD_MASK)) != 0)
		goto fail1;

	if ((rc = txc43128_bist(enp)) != 0)
		goto fail2;

	if ((rc = txc43128_led_cfg(enp)) != 0)
		goto fail3;

	if ((rc = txc43128_preemphasis_cfg(enp)) != 0)
		goto fail4;

	if ((rc = txc43128_amplitude_cfg(enp)) != 0)
		goto fail5;

	EFSYS_ASSERT3U(epp->ep_adv_cap_mask, ==, TXC43128_ADV_CAP_MASK);

#if EFSYS_OPT_LOOPBACK
	if ((rc = txc43128_loopback_cfg(enp)) != 0)
		goto fail6;
#endif	/* EFSYS_OPT_LOOPBACK */

	if ((rc = txc43128_logic_reset(enp)) != 0)
		goto fail7;

	return (0);

fail7:
	EFSYS_PROBE(fail7);

#if EFSYS_OPT_LOOPBACK
fail6:
	EFSYS_PROBE(fail6);
#endif	/* EFSYS_OPT_LOOPBACK */

fail5:
	EFSYS_PROBE(fail5);
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

	__checkReturn	int
txc43128_verify(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	int rc;

	if ((rc = xphy_pkg_verify(enp, epp->ep_port, TXC43128_MMD_MASK)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn	int
txc43128_uplink_check(
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
	    ((EFX_WORD_FIELD(word, PHY_XS_LANE0_SYNC) != 0) ||
	    (EFX_WORD_FIELD(word, PHY_XS_LANE1_SYNC) != 0) ||
	    (EFX_WORD_FIELD(word, PHY_XS_LANE2_SYNC) != 0) ||
	    (EFX_WORD_FIELD(word, PHY_XS_LANE3_SYNC) != 0)));

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn	int
txc43128_downlink_check(
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

	/* bug10934: Reset the controller if the PCS block is stuck down */
	if (up)
		epp->ep_txc43128.bug10934_count = 0;
	else if (++(epp->ep_txc43128.bug10934_count) > 10) {
		(void) txc43128_logic_reset(enp);
		epp->ep_txc43128.bug10934_count = 0;
	}

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
txc43128_oui_get(
	__in		efx_nic_t *enp,
	__out		uint32_t *ouip)
{
	efx_port_t *epp = &(enp->en_port);
	int rc;

	if ((rc = xphy_mmd_oui_get(enp, epp->ep_port, PMA_PMD_MMD,
	    ouip)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#if EFSYS_OPT_PHY_STATS

#define	TXC43128_STAT_SET(_stat, _mask, _id, _val)			\
	do {								\
		(_mask) |= (1ULL << (_id));				\
		(_stat)[_id] = (uint32_t)(_val);			\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

static	__checkReturn			int
txc43128_pma_pmd_stats_update(
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

	TXC43128_STAT_SET(stat, *maskp, EFX_PHY_STAT_PMA_PMD_LINK_UP,
	    (EFX_WORD_FIELD(word, PMA_PMD_LINK_UP) != 0) ? 1 : 0);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_STATUS2_REG, &word)) != 0)
		goto fail2;

	TXC43128_STAT_SET(stat, *maskp, EFX_PHY_STAT_PMA_PMD_RX_FAULT,
	    (EFX_WORD_FIELD(word, PMA_PMD_RX_FAULT) != 0) ? 1 : 0);
	TXC43128_STAT_SET(stat, *maskp, EFX_PHY_STAT_PMA_PMD_TX_FAULT,
	    (EFX_WORD_FIELD(word, PMA_PMD_TX_FAULT) != 0) ? 1 : 0);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD, SIGDET,
	    &word)) != 0)
		goto fail3;

	TXC43128_STAT_SET(stat, *maskp, EFX_PHY_STAT_PMA_PMD_SIGNAL_A,
	    (EFX_WORD_FIELD(word, RX0SIGDET) != 0));
	TXC43128_STAT_SET(stat, *maskp, EFX_PHY_STAT_PMA_PMD_SIGNAL_B,
	    (EFX_WORD_FIELD(word, RX1SIGDET) != 0));
	TXC43128_STAT_SET(stat, *maskp, EFX_PHY_STAT_PMA_PMD_SIGNAL_C,
	    (EFX_WORD_FIELD(word, RX2SIGDET) != 0));
	TXC43128_STAT_SET(stat, *maskp, EFX_PHY_STAT_PMA_PMD_SIGNAL_D,
	    (EFX_WORD_FIELD(word, RX3SIGDET) != 0));

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
txc43128_pcs_stats_update(
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

	TXC43128_STAT_SET(stat, *maskp, EFX_PHY_STAT_PCS_LINK_UP,
	    (EFX_WORD_FIELD(word, PCS_LINK_UP) != 0) ? 1 : 0);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
	    PCS_STATUS2_REG, &word)) != 0)
		goto fail2;

	TXC43128_STAT_SET(stat, *maskp, EFX_PHY_STAT_PCS_RX_FAULT,
	    (EFX_WORD_FIELD(word, PCS_RX_FAULT) != 0) ? 1 : 0);
	TXC43128_STAT_SET(stat, *maskp, EFX_PHY_STAT_PCS_TX_FAULT,
	    (EFX_WORD_FIELD(word, PCS_TX_FAULT) != 0) ? 1 : 0);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static	__checkReturn			int
txc43128_phy_xs_stats_update(
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

	TXC43128_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_LINK_UP,
	    (EFX_WORD_FIELD(word, PHY_XS_LINK_UP) != 0) ? 1 : 0);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PHY_XS_MMD,
	    PHY_XS_STATUS2_REG, &word)) != 0)
		goto fail2;

	TXC43128_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_RX_FAULT,
	    (EFX_WORD_FIELD(word, PHY_XS_RX_FAULT) != 0) ? 1 : 0);
	TXC43128_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_TX_FAULT,
	    (EFX_WORD_FIELD(word, PHY_XS_TX_FAULT) != 0) ? 1 : 0);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PHY_XS_MMD,
	    PHY_XS_LANE_STATUS_REG, &word)) != 0)
		goto fail3;

	TXC43128_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_ALIGN,
	    (EFX_WORD_FIELD(word, PHY_XS_ALIGNED) != 0) ? 1 : 0);
	TXC43128_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_SYNC_A,
	    (EFX_WORD_FIELD(word, PHY_XS_LANE0_SYNC) != 0) ? 1 : 0);
	TXC43128_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_SYNC_B,
	    (EFX_WORD_FIELD(word, PHY_XS_LANE1_SYNC) != 0) ? 1 : 0);
	TXC43128_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_SYNC_C,
	    (EFX_WORD_FIELD(word, PHY_XS_LANE2_SYNC) != 0) ? 1 : 0);
	TXC43128_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_SYNC_D,
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
txc43128_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__out_ecount(EFX_PHY_NSTATS)	uint32_t *stat)
{
	efx_port_t *epp = &(enp->en_port);
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	uint64_t mask = 0;
	uint32_t oui;
	int rc;

	_NOTE(ARGUNUSED(esmp))

	if ((rc = xphy_mmd_oui_get(enp, epp->ep_port, PMA_PMD_MMD, &oui)) != 0)
		goto fail1;

	TXC43128_STAT_SET(stat, mask, EFX_PHY_STAT_OUI, oui);

	if ((rc = txc43128_pma_pmd_stats_update(enp, &mask, stat)) != 0)
		goto fail2;

	if ((rc = txc43128_pcs_stats_update(enp, &mask, stat)) != 0)
		goto fail3;

	if ((rc = txc43128_phy_xs_stats_update(enp, &mask, stat)) != 0)
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
txc43128_prop_name(
	__in	efx_nic_t *enp,
	__in	unsigned int id)
{
	_NOTE(ARGUNUSED(enp, id))

	EFSYS_ASSERT(B_FALSE);

	return (NULL);
}
#endif	/* EFSYS_OPT_NAMES */

	__checkReturn	int
txc43128_prop_get(
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
txc43128_prop_set(
	__in		efx_nic_t *enp,
	__in		unsigned int id,
	__in		uint32_t val)
{
	_NOTE(ARGUNUSED(enp, id, val))

	EFSYS_ASSERT(B_FALSE);

	return (ENOTSUP);
}
#endif	/* EFSYS_OPT_PHY_PROPS */

#endif	/* EFSYS_OPT_PHY_TXC43128 */
