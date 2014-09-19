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
#include "falcon_nvram.h"
#include "sft9001.h"
#include "sft9001_impl.h"
#include "xphy.h"
#include "falcon_impl.h"

#if EFSYS_OPT_PHY_SFT9001

static	__checkReturn	int
sft9001_short_reach_set(
	__in		efx_nic_t *enp,
	__in		boolean_t on)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	if ((rc = falcon_mdio_read(enp, epp->ep_port,
	    PMA_PMD_MMD, PMA_PMD_PWR_BACKOFF_REG, &word)) != 0)
		goto fail1;

	EFX_SET_WORD_FIELD(word, SHORT_REACH, (on) ? 1 : 0);

	if ((rc = falcon_mdio_write(enp, epp->ep_port,
	    PMA_PMD_MMD, PMA_PMD_PWR_BACKOFF_REG, &word)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static	__checkReturn	int
sft9001_short_reach_get(
	__in		efx_nic_t *enp,
	__out		boolean_t *onp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	if ((rc = falcon_mdio_read(enp, epp->ep_port,
	    PMA_PMD_MMD, PMA_PMD_PWR_BACKOFF_REG, &word)) != 0)
		goto fail1;

	*onp = (EFX_WORD_FIELD(word, SHORT_REACH) != 0);

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static	__checkReturn	int
sft9001_robust_set(
	__in		efx_nic_t *enp,
	__in		boolean_t on)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_XCONTROL_REG, &word)) != 0)
		goto fail1;

	EFX_SET_WORD_FIELD(word, ROBUST, (on) ? 1 : 0);

	if ((rc = falcon_mdio_write(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_XCONTROL_REG, &word)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static	__checkReturn	int
sft9001_robust_get(
	__in		efx_nic_t *enp,
	__out		boolean_t *onp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_XCONTROL_REG, &word)) != 0)
		goto fail1;

	*onp = (EFX_WORD_FIELD(word, ROBUST) != 0);

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static	__checkReturn	int
sft9001_an_set(
	__in		efx_nic_t *enp,
	__in		boolean_t on)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	if ((rc = falcon_mdio_read(enp, epp->ep_port, AN_MMD,
	    AN_CONTROL1_REG, &word)) != 0)
		goto fail1;

	if (on) {
		EFX_SET_WORD_FIELD(word, AN_ENABLE, 1);
		EFX_SET_WORD_FIELD(word, AN_RESTART, 1);
	} else {
		EFX_SET_WORD_FIELD(word, AN_ENABLE, 0);
	}

	if ((rc = falcon_mdio_write(enp, epp->ep_port, AN_MMD,
	    AN_CONTROL1_REG, &word)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static	__checkReturn	int
sft9001_gmii_cfg(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_XCONTROL_REG, &word)) != 0)
		goto fail1;

	EFX_SET_WORD_FIELD(word, GMII_EN, 1);

	if ((rc = falcon_mdio_write(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_XCONTROL_REG, &word)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static	__checkReturn	int
sft9001_clock_cfg(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_XCONTROL_REG, &word)) != 0)
		goto fail1;

	/* Select 312MHz clock on CLK312_OUT_{P,N} */
	EFX_SET_WORD_FIELD(word, CLK312_OUT_SEL, SEL_312MHZ_DECODE);
	EFX_SET_WORD_FIELD(word, CLK312_OUT_EN, 1);

	/* Select 125MHz clock on TEST_CLKOUT_{P,N} */
	EFX_SET_WORD_FIELD(word, TEST_CLKOUT_SEL, SEL_125MHZ_DECODE);
	EFX_SET_WORD_FIELD(word, TEST_CLKOUT_EN, 1);

	if ((rc = falcon_mdio_write(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_XCONTROL_REG, &word)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static	__checkReturn	int
sft9001_adv_cap_cfg(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	/* Check base page */
	if ((rc = falcon_mdio_read(enp, epp->ep_port, AN_MMD,
	    AN_ADV_BP_CAP_REG, &word)) != 0)
		goto fail1;

	EFSYS_ASSERT(!(epp->ep_adv_cap_mask & (1 << EFX_PHY_CAP_10HDX)));
	if (EFX_WORD_FIELD(word, AN_ADV_TA_10BASE_T) != 0)
		EFX_SET_WORD_FIELD(word, AN_ADV_TA_10BASE_T, 0);

	EFSYS_ASSERT(!(epp->ep_adv_cap_mask & (1 << EFX_PHY_CAP_10FDX)));
	if (EFX_WORD_FIELD(word, AN_ADV_TA_10BASE_T_FDX) != 0)
		EFX_SET_WORD_FIELD(word, AN_ADV_TA_10BASE_T_FDX, 0);

	if (EFX_WORD_FIELD(word, AN_ADV_TA_100BASE_TX) == 0 &&
	    epp->ep_adv_cap_mask & (1 << EFX_PHY_CAP_100HDX))
		EFX_SET_WORD_FIELD(word, AN_ADV_TA_100BASE_TX, 1);
	else if (EFX_WORD_FIELD(word, AN_ADV_TA_100BASE_TX) != 0 &&
	    !(epp->ep_adv_cap_mask & (1 << EFX_PHY_CAP_100HDX)))
		EFX_SET_WORD_FIELD(word, AN_ADV_TA_100BASE_TX, 0);

	if (EFX_WORD_FIELD(word, AN_ADV_TA_100BASE_TX_FDX) == 0 &&
	    epp->ep_adv_cap_mask & (1 << EFX_PHY_CAP_100FDX))
		EFX_SET_WORD_FIELD(word, AN_ADV_TA_100BASE_TX_FDX, 1);
	else if (EFX_WORD_FIELD(word, AN_ADV_TA_100BASE_TX_FDX) != 0 &&
	    !(epp->ep_adv_cap_mask & (1 << EFX_PHY_CAP_100FDX)))
		EFX_SET_WORD_FIELD(word, AN_ADV_TA_100BASE_TX_FDX, 0);

	if (EFX_WORD_FIELD(word, AN_ADV_TA_PAUSE) == 0 &&
	    epp->ep_adv_cap_mask & (1 << EFX_PHY_CAP_PAUSE))
		EFX_SET_WORD_FIELD(word, AN_ADV_TA_PAUSE, 1);
	else if (EFX_WORD_FIELD(word, AN_ADV_TA_PAUSE) != 0 &&
	    !(epp->ep_adv_cap_mask & (1 << EFX_PHY_CAP_PAUSE)))
		EFX_SET_WORD_FIELD(word, AN_ADV_TA_PAUSE, 0);

	if (EFX_WORD_FIELD(word, AN_ADV_TA_ASM_DIR) == 0 &&
	    epp->ep_adv_cap_mask & (1 << EFX_PHY_CAP_ASYM))
		EFX_SET_WORD_FIELD(word, AN_ADV_TA_ASM_DIR, 1);
	else if (EFX_WORD_FIELD(word, AN_ADV_TA_ASM_DIR) != 0 &&
	    !(epp->ep_adv_cap_mask & (1 << EFX_PHY_CAP_ASYM)))
		EFX_SET_WORD_FIELD(word, AN_ADV_TA_ASM_DIR, 0);

	if ((rc = falcon_mdio_write(enp, epp->ep_port, AN_MMD,
	    AN_ADV_BP_CAP_REG, &word)) != 0)
		goto fail2;

	/* Check 1G operation */
	if ((rc = falcon_mdio_read(enp, epp->ep_port, CL22EXT_MMD,
	    CL22EXT_MS_CONTROL_REG, &word)) != 0)
		goto fail3;

	EFSYS_ASSERT(!(epp->ep_adv_cap_mask & (1 << EFX_PHY_CAP_1000HDX)));
	if (EFX_WORD_FIELD(word, CL22EXT_1000BASE_T_ADV) != 0)
		EFX_SET_WORD_FIELD(word, CL22EXT_1000BASE_T_ADV, 0);

	if (EFX_WORD_FIELD(word, CL22EXT_1000BASE_T_FDX_ADV) == 0 &&
	    epp->ep_adv_cap_mask & (1 << EFX_PHY_CAP_1000FDX))
		EFX_SET_WORD_FIELD(word, CL22EXT_1000BASE_T_FDX_ADV, 1);
	else if (EFX_WORD_FIELD(word, CL22EXT_1000BASE_T_FDX_ADV) != 0 &&
	    !(epp->ep_adv_cap_mask & (1 << EFX_PHY_CAP_1000FDX)))
		EFX_SET_WORD_FIELD(word, CL22EXT_1000BASE_T_FDX_ADV, 0);

	if ((rc = falcon_mdio_write(enp, epp->ep_port, CL22EXT_MMD,
	    CL22EXT_MS_CONTROL_REG, &word)) != 0)
		goto fail4;

	/* Check 10G operation */
	if ((rc = falcon_mdio_read(enp, epp->ep_port, AN_MMD,
	    AN_10G_BASE_T_CONTROL_REG, &word)) != 0)
		goto fail5;

	if (EFX_WORD_FIELD(word, AN_10G_BASE_T_ADV) == 0 &&
	    epp->ep_adv_cap_mask & (1 << EFX_PHY_CAP_10000FDX))
		EFX_SET_WORD_FIELD(word, AN_10G_BASE_T_ADV, 1);
	else if (EFX_WORD_FIELD(word, AN_10G_BASE_T_ADV) != 0 &&
	    !(epp->ep_adv_cap_mask & (1 << EFX_PHY_CAP_10000FDX)))
		EFX_SET_WORD_FIELD(word, AN_10G_BASE_T_ADV, 0);

	if ((rc = falcon_mdio_write(enp, epp->ep_port, AN_MMD,
	    AN_10G_BASE_T_CONTROL_REG, &word)) != 0)
		goto fail6;

	return (0);

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

#if EFSYS_OPT_LOOPBACK
static	__checkReturn	int
sft9001_loopback_cfg(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	switch (epp->ep_loopback_type) {
	case EFX_LOOPBACK_PHY_XS:
		if ((rc = falcon_mdio_read(enp, epp->ep_port,
		    PHY_XS_MMD, PHY_XS_TEST1_REG, &word)) != 0)
			goto fail1;

		EFX_SET_WORD_FIELD(word, PHY_XS_NE_LOOPBACK, 1);

		if ((rc = falcon_mdio_write(enp, epp->ep_port,
		    PHY_XS_MMD, PHY_XS_TEST1_REG, &word)) != 0)
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

	case EFX_LOOPBACK_GPHY:
		if ((rc = falcon_mdio_read(enp, epp->ep_port,
		    CL22EXT_MMD, CL22EXT_CONTROL_REG, &word)) != 0)
			goto fail1;

		EFX_SET_WORD_FIELD(word, CL22EXT_NE_LOOPBACK, 1);

		if ((rc = falcon_mdio_write(enp, epp->ep_port,
		    CL22EXT_MMD, CL22EXT_CONTROL_REG, &word)) != 0)
			goto fail2;

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

static	__checkReturn	int
sft9001_led_cfg(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

#if EFSYS_OPT_PHY_LED_CONTROL

	switch (epp->ep_phy_led_mode) {
	case EFX_PHY_LED_DEFAULT:
		EFX_POPULATE_WORD_8(word,
		    LED_TMODE, LED_NORMAL_DECODE,
		    LED_SPARE, LED_NORMAL_DECODE,
		    LED_MS, LED_NORMAL_DECODE,
		    LED_RX, LED_NORMAL_DECODE,
		    LED_TX, LED_NORMAL_DECODE,
		    LED_SPEED0, LED_NORMAL_DECODE,
		    LED_SPEED1, LED_NORMAL_DECODE,
		    LED_LINK, LED_NORMAL_DECODE);
		break;

	case EFX_PHY_LED_OFF:
		EFX_POPULATE_WORD_8(word,
		    LED_TMODE, LED_OFF_DECODE,
		    LED_SPARE, LED_OFF_DECODE,
		    LED_MS, LED_OFF_DECODE,
		    LED_RX, LED_OFF_DECODE,
		    LED_TX, LED_OFF_DECODE,
		    LED_SPEED0, LED_OFF_DECODE,
		    LED_SPEED1, LED_OFF_DECODE,
		    LED_LINK, LED_OFF_DECODE);
		break;

	case EFX_PHY_LED_ON:
		EFX_POPULATE_WORD_8(word,
		    LED_TMODE, LED_ON_DECODE,
		    LED_SPARE, LED_ON_DECODE,
		    LED_MS, LED_ON_DECODE,
		    LED_RX, LED_ON_DECODE,
		    LED_TX, LED_ON_DECODE,
		    LED_SPEED0, LED_ON_DECODE,
		    LED_SPEED1, LED_ON_DECODE,
		    LED_LINK, LED_ON_DECODE);
		break;

	case EFX_PHY_LED_FLASH:
		EFX_POPULATE_WORD_8(word,
		    LED_TMODE, LED_FLASH_DECODE,
		    LED_SPARE, LED_FLASH_DECODE,
		    LED_MS, LED_FLASH_DECODE,
		    LED_RX, LED_FLASH_DECODE,
		    LED_TX, LED_FLASH_DECODE,
		    LED_SPEED0, LED_FLASH_DECODE,
		    LED_SPEED1, LED_FLASH_DECODE,
		    LED_LINK, LED_FLASH_DECODE);
		break;

	default:
		EFSYS_ASSERT(B_FALSE);
		break;
	}

#else	/* EFSYS_OPT_PHY_LED_CONTROL */

	EFX_POPULATE_WORD_8(word,
	    LED_TMODE, LED_NORMAL_DECODE,
	    LED_SPARE, LED_NORMAL_DECODE,
	    LED_MS, LED_NORMAL_DECODE,
	    LED_RX, LED_NORMAL_DECODE,
	    LED_TX, LED_NORMAL_DECODE,
	    LED_SPEED0, LED_NORMAL_DECODE,
	    LED_SPEED1, LED_NORMAL_DECODE,
	    LED_LINK, LED_NORMAL_DECODE);

#endif	/* EFSYS_OPT_PHY_LED_CONTROL */

	if ((rc = falcon_mdio_write(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_LED_OVERRIDE_REG, &word)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn	int
sft9001_reset(
	__in		efx_nic_t *enp)
{
	int state;

	/* Lock I2C bus because sft9001 is sensitive to GPIO3 */
	EFSYS_LOCK(enp->en_eslp, state);
	EFSYS_ASSERT(!enp->en_u.falcon.enu_i2c_locked);
	enp->en_u.falcon.enu_i2c_locked = B_TRUE;
	EFSYS_UNLOCK(enp->en_eslp, state);

	/* Pull the external reset line */
	falcon_nic_phy_reset(enp);

	/* Unlock I2C */
	EFSYS_LOCK(enp->en_eslp, state);
	enp->en_u.falcon.enu_i2c_locked = B_FALSE;
	EFSYS_UNLOCK(enp->en_eslp, state);

	return (0);
}

	__checkReturn	int
sft9001_reconfigure(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	unsigned int count;
	int rc;

	/* Wait for the firmware boot to complete */
	count = 0;
	do {
		efx_word_t word;

		EFSYS_PROBE1(wait, unsigned int, count);

		/* Spin for 1 ms */
		EFSYS_SPIN(1000);

		if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
		    PCS_BOOT_STATUS_REG, &word)) != 0)
			goto fail1;

		if (EFX_WORD_FIELD(word, FATAL_ERR) != 0)
			break;	/* no point in continuing */

		if (EFX_WORD_FIELD(word, BOOT_STATUS) != 0 &&
		    EFX_WORD_FIELD(word, CODE_DOWNLOAD) != 0 &&
		    EFX_WORD_FIELD(word, CKSUM_OK) != 0 &&
		    EFX_WORD_FIELD(word, CODE_STARTED) != 0 &&
		    EFX_WORD_FIELD(word, BOOT_PROGRESS) == APP_JMP_DECODE)
			goto configure;

	} while (++count < 1000);

	rc = ENOTACTIVE;
	goto fail2;

configure:
	if ((rc = xphy_pkg_wait(enp, epp->ep_port, SFT9001_MMD_MASK)) != 0)
		goto fail3;

	/* Make sure auto-negotiation is off whilst we configure the PHY */
	if ((rc = sft9001_an_set(enp, B_FALSE)) != 0)
		goto fail4;

	if ((rc = sft9001_gmii_cfg(enp)) != 0)
		goto fail5;

	if ((rc = sft9001_clock_cfg(enp)) != 0)
		goto fail6;

	if ((rc = sft9001_adv_cap_cfg(enp)) != 0)
		goto fail7;

#if EFSYS_OPT_LOOPBACK
	if ((rc = sft9001_loopback_cfg(enp)) != 0)
		goto fail8;
#endif	/* EFSYS_OPT_LOOPBACK */

	if ((rc = sft9001_led_cfg(enp)) != 0)
		goto fail9;

	if ((rc = sft9001_robust_set(enp, B_TRUE)) != 0)
		goto fail10;

#if EFSYS_OPT_LOOPBACK
	if (epp->ep_loopback_type == EFX_LOOPBACK_OFF) {
		if ((rc = sft9001_an_set(enp, B_TRUE)) != 0)
			goto fail11;
	}
#else	/* EFSYS_OPT_LOOPBACK */
	if ((rc = sft9001_an_set(enp, B_TRUE)) != 0)
		goto fail11;
#endif	/* EFSYS_OPT_LOOPBACK */

	return (0);

fail11:
	EFSYS_PROBE(fail11);
fail10:
	EFSYS_PROBE(fail10);
fail9:
	EFSYS_PROBE(fail9);

#if EFSYS_OPT_LOOPBACK
fail8:
	EFSYS_PROBE(fail8);
#endif	/* EFSYS_OPT_LOOPBACK */

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

	__checkReturn	int
sft9001_verify(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	int rc;

	if ((rc = xphy_pkg_verify(enp, epp->ep_port, SFT9001_MMD_MASK)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn	int
sft9001_uplink_check(
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

static	__checkReturn	int
sft9001_lp_cap_get(
	__in		efx_nic_t *enp,
	__out		unsigned int *maskp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	*maskp = 0;

	if ((rc = falcon_mdio_read(enp, epp->ep_port, AN_MMD,
	    AN_LP_BP_CAP_REG, &word)) != 0)
		goto fail1;

	if (EFX_WORD_FIELD(word, AN_LP_TA_10BASE_T) != 0)
		*maskp |= (1 << EFX_PHY_CAP_10HDX);

	if (EFX_WORD_FIELD(word, AN_LP_TA_10BASE_T_FDX) != 0)
		*maskp |= (1 << EFX_PHY_CAP_10FDX);

	if (EFX_WORD_FIELD(word, AN_LP_TA_100BASE_TX) != 0)
		*maskp |= (1 << EFX_PHY_CAP_100HDX);

	if (EFX_WORD_FIELD(word, AN_LP_TA_100BASE_TX_FDX) != 0)
		*maskp |= (1 << EFX_PHY_CAP_100FDX);

	if (EFX_WORD_FIELD(word, AN_LP_TA_PAUSE) != 0)
		*maskp |= (1 << EFX_PHY_CAP_PAUSE);

	if (EFX_WORD_FIELD(word, AN_LP_TA_ASM_DIR) != 0)
		*maskp |= (1 << EFX_PHY_CAP_ASYM);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, CL22EXT_MMD,
	    CL22EXT_MS_STATUS_REG, &word)) != 0)
		goto fail2;

	if (EFX_WORD_FIELD(word, CL22EXT_1000BASE_T_LP) != 0)
		*maskp |= (1 << EFX_PHY_CAP_1000HDX);

	if (EFX_WORD_FIELD(word, CL22EXT_1000BASE_T_FDX_LP) != 0)
		*maskp |= (1 << EFX_PHY_CAP_1000FDX);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, AN_MMD,
	    AN_10G_BASE_T_STATUS_REG, &word)) != 0)
		goto fail3;

	if (EFX_WORD_FIELD(word, AN_10G_BASE_T_LP) != 0)
		*maskp |= (1 << EFX_PHY_CAP_10000FDX);

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn	int
sft9001_downlink_check(
	__in		efx_nic_t *enp,
	__out		efx_link_mode_t *modep,
	__out		unsigned int *fcntlp,
	__out		uint32_t *lp_cap_maskp)
{
	efx_port_t *epp = &(enp->en_port);
	unsigned int fcntl = epp->ep_fcntl;
	unsigned int lp_cap_mask = epp->ep_lp_cap_mask;
	boolean_t up;
	uint32_t common;
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
	case EFX_LOOPBACK_PMA_PMD:
		rc = xphy_mmd_check(enp, epp->ep_port, PHY_XS_MMD, &up);
		if (rc != 0)
			goto fail1;

		*modep = (up) ? EFX_LINK_10000FDX : EFX_LINK_DOWN;
		goto done;

	case EFX_LOOPBACK_GPHY:
		*modep = EFX_LINK_1000FDX;
		goto done;

	default:
		break;
	}
#endif	/* EFSYS_OPT_LOOPBACK */

	if ((rc = xphy_mmd_check(enp, epp->ep_port, AN_MMD, &up)) != 0)
		goto fail1;

	if (!up) {
		*modep = EFX_LINK_DOWN;
		goto done;
	}

	/* Check the link partner capabilities */
	if ((rc = sft9001_lp_cap_get(enp, &lp_cap_mask)) != 0)
		goto fail2;

	/* Resolve the common capabilities */
	common = epp->ep_adv_cap_mask & lp_cap_mask;

	/* The 'best' common link mode should be the one in operation */
	if (common & (1 << EFX_PHY_CAP_10000FDX)) {
		*modep = EFX_LINK_10000FDX;
	} else if (common & (1 << EFX_PHY_CAP_1000FDX)) {
		*modep = EFX_LINK_1000FDX;
	} else if (common & (1 << EFX_PHY_CAP_100FDX)) {
		*modep = EFX_LINK_100FDX;
	} else if (common & (1 << EFX_PHY_CAP_100HDX)) {
		*modep = EFX_LINK_100HDX;
	} else {
		*modep = EFX_LINK_UNKNOWN;
	}

	/* Determine negotiated or forced flow control mode */
	fcntl = 0;
	if (epp->ep_fcntl_autoneg) {
		if (common & (1 << EFX_PHY_CAP_PAUSE))
			fcntl = EFX_FCNTL_GENERATE | EFX_FCNTL_RESPOND;
		else if (common & (1 << EFX_PHY_CAP_ASYM)) {
			if (epp->ep_adv_cap_mask & (1 << EFX_PHY_CAP_PAUSE))
				fcntl = EFX_FCNTL_RESPOND;
			else if (lp_cap_mask & (1 << EFX_PHY_CAP_PAUSE))
				fcntl = EFX_FCNTL_GENERATE;
		}
	} else {
		if (epp->ep_adv_cap_mask & (1 << EFX_PHY_CAP_PAUSE))
			fcntl = EFX_FCNTL_GENERATE | EFX_FCNTL_RESPOND;
		if (epp->ep_adv_cap_mask & (1 << EFX_PHY_CAP_ASYM))
			fcntl ^= EFX_FCNTL_GENERATE;
	}

done:
	*fcntlp = fcntl;
	*lp_cap_maskp = lp_cap_mask;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn	int
sft9001_oui_get(
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

#define	SFT9001_STAT_SET(_stat, _mode, _id, _val)			\
	do {								\
		(_mode) |= (1 << (_id));				\
		(_stat)[_id] = (uint32_t)(_val);			\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

static	__checkReturn	int
sft9001_rev_get(
	__in		efx_nic_t *enp,
	__out		uint8_t *ap,
	__out		uint8_t *bp,
	__out		uint8_t *cp,
	__out		uint8_t *dp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_FW_REV0_REG, &word)) != 0)
		goto fail1;

	*ap = (uint8_t)EFX_WORD_FIELD(word, EFX_BYTE_1);
	*bp = (uint8_t)EFX_WORD_FIELD(word, EFX_BYTE_0);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_FW_REV1_REG, &word)) != 0)
		goto fail2;

	*cp = (uint8_t)EFX_WORD_FIELD(word, EFX_BYTE_1);
	*dp = (uint8_t)EFX_WORD_FIELD(word, EFX_BYTE_0);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#if EFSYS_OPT_PHY_STATS

static	__checkReturn			int
sft9001_pma_pmd_stats_update(
	__in				efx_nic_t *enp,
	__inout				uint64_t *maskp,
	__inout_ecount(EFX_PHY_NSTATS)	uint32_t *stat)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	uint8_t a;
	uint8_t b;
	uint8_t c;
	uint8_t d;
	int rc;

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_STATUS1_REG, &word)) != 0)
		goto fail1;

	SFT9001_STAT_SET(stat, *maskp, EFX_PHY_STAT_PMA_PMD_LINK_UP,
	    (EFX_WORD_FIELD(word, PMA_PMD_LINK_UP) != 0) ? 1 : 0);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_STATUS2_REG, &word)) != 0)
		goto fail2;

	SFT9001_STAT_SET(stat, *maskp, EFX_PHY_STAT_PMA_PMD_RX_FAULT,
	    (EFX_WORD_FIELD(word, PMA_PMD_RX_FAULT) != 0) ? 1 : 0);
	SFT9001_STAT_SET(stat, *maskp, EFX_PHY_STAT_PMA_PMD_TX_FAULT,
	    (EFX_WORD_FIELD(word, PMA_PMD_TX_FAULT) != 0) ? 1 : 0);

	if ((rc = sft9001_rev_get(enp, &a, &b, &c, &d)) != 0)
		goto fail3;

	SFT9001_STAT_SET(stat, *maskp, EFX_PHY_STAT_PMA_PMD_REV_A, a);
	SFT9001_STAT_SET(stat, *maskp, EFX_PHY_STAT_PMA_PMD_REV_B, b);
	SFT9001_STAT_SET(stat, *maskp, EFX_PHY_STAT_PMA_PMD_REV_C, c);
	SFT9001_STAT_SET(stat, *maskp, EFX_PHY_STAT_PMA_PMD_REV_D, d);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_CHANNELA_SNR_REG, &word)) != 0)
		goto fail4;

	SFT9001_STAT_SET(stat, *maskp, EFX_PHY_STAT_SNR_A,
	    EFX_WORD_FIELD(word, PMA_PMD_SNR));

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_CHANNELB_SNR_REG, &word)) != 0)
		goto fail5;

	SFT9001_STAT_SET(stat, *maskp, EFX_PHY_STAT_SNR_B,
	    EFX_WORD_FIELD(word, PMA_PMD_SNR));

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_CHANNELC_SNR_REG, &word)) != 0)
		goto fail6;

	SFT9001_STAT_SET(stat, *maskp, EFX_PHY_STAT_SNR_C,
	    EFX_WORD_FIELD(word, PMA_PMD_SNR));

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_CHANNELD_SNR_REG, &word)) != 0)
		goto fail7;

	SFT9001_STAT_SET(stat, *maskp, EFX_PHY_STAT_SNR_D,
	    EFX_WORD_FIELD(word, PMA_PMD_SNR));

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

static	__checkReturn			int
sft9001_pcs_stats_update(
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

	SFT9001_STAT_SET(stat, *maskp, EFX_PHY_STAT_PCS_LINK_UP,
	    (EFX_WORD_FIELD(word, PCS_LINK_UP) != 0) ? 1 : 0);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
	    PCS_STATUS2_REG, &word)) != 0)
		goto fail2;

	SFT9001_STAT_SET(stat, *maskp, EFX_PHY_STAT_PCS_RX_FAULT,
	    (EFX_WORD_FIELD(word, PCS_RX_FAULT) != 0) ? 1 : 0);
	SFT9001_STAT_SET(stat, *maskp, EFX_PHY_STAT_PCS_TX_FAULT,
	    (EFX_WORD_FIELD(word, PCS_TX_FAULT) != 0) ? 1 : 0);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
	    PCS_10GBASE_T_STATUS2_REG, &word)) != 0)
		goto fail3;

	SFT9001_STAT_SET(stat, *maskp, EFX_PHY_STAT_PCS_BER,
	    EFX_WORD_FIELD(word, PCS_BER));
	SFT9001_STAT_SET(stat, *maskp, EFX_PHY_STAT_PCS_BLOCK_ERRORS,
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
sft9001_phy_xs_stats_update(
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

	SFT9001_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_LINK_UP,
	    (EFX_WORD_FIELD(word, PHY_XS_LINK_UP) != 0) ? 1 : 0);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PHY_XS_MMD,
	    PHY_XS_STATUS2_REG, &word)) != 0)
		goto fail2;

	SFT9001_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_RX_FAULT,
	    (EFX_WORD_FIELD(word, PHY_XS_RX_FAULT) != 0) ? 1 : 0);
	SFT9001_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_TX_FAULT,
	    (EFX_WORD_FIELD(word, PHY_XS_TX_FAULT) != 0) ? 1 : 0);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PHY_XS_MMD,
	    PHY_XS_LANE_STATUS_REG, &word)) != 0)
		goto fail3;

	SFT9001_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_ALIGN,
	    (EFX_WORD_FIELD(word, PHY_XS_ALIGNED) != 0) ? 1 : 0);
	SFT9001_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_SYNC_A,
	    (EFX_WORD_FIELD(word, PHY_XS_LANE0_SYNC) != 0) ? 1 : 0);
	SFT9001_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_SYNC_B,
	    (EFX_WORD_FIELD(word, PHY_XS_LANE1_SYNC) != 0) ? 1 : 0);
	SFT9001_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_SYNC_C,
	    (EFX_WORD_FIELD(word, PHY_XS_LANE2_SYNC) != 0) ? 1 : 0);
	SFT9001_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_SYNC_D,
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

static	__checkReturn			int
sft9001_an_stats_update(
	__in				efx_nic_t *enp,
	__inout				uint64_t *maskp,
	__inout_ecount(EFX_PHY_NSTATS)	uint32_t *stat)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	if ((rc = falcon_mdio_read(enp, epp->ep_port, AN_MMD,
	    AN_STATUS1_REG, &word)) != 0)
		goto fail1;

	SFT9001_STAT_SET(stat, *maskp, EFX_PHY_STAT_AN_LINK_UP,
	    (EFX_WORD_FIELD(word, AN_LINK_UP) != 0) ? 1 : 0);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, AN_MMD,
	    AN_10G_BASE_T_STATUS_REG, &word)) != 0)
		goto fail2;

	SFT9001_STAT_SET(stat, *maskp, EFX_PHY_STAT_AN_MASTER,
	    (EFX_WORD_FIELD(word, AN_MASTER) != 0) ? 1 : 0);
	SFT9001_STAT_SET(stat, *maskp, EFX_PHY_STAT_AN_LOCAL_RX_OK,
	    (EFX_WORD_FIELD(word, AN_LOCAL_RX_OK) != 0) ? 1 : 0);
	SFT9001_STAT_SET(stat, *maskp, EFX_PHY_STAT_AN_REMOTE_RX_OK,
	    (EFX_WORD_FIELD(word, AN_REMOTE_RX_OK) != 0) ? 1 : 0);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static	__checkReturn			int
sft9001_cl22ext_stats_update(
	__in				efx_nic_t *enp,
	__inout				uint64_t *maskp,
	__inout_ecount(EFX_PHY_NSTATS)	uint32_t *stat)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	if ((rc = falcon_mdio_read(enp, epp->ep_port, CL22EXT_MMD,
	    CL22EXT_STATUS_REG, &word)) != 0)
		goto fail1;

	SFT9001_STAT_SET(stat, *maskp, EFX_PHY_STAT_CL22EXT_LINK_UP,
	    (EFX_WORD_FIELD(word, CL22EXT_LINK_UP) != 0) ? 1 : 0);

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}


	__checkReturn			int
sft9001_stats_update(
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

	SFT9001_STAT_SET(stat, mask, EFX_PHY_STAT_OUI, oui);

	if ((rc = sft9001_pma_pmd_stats_update(enp, &mask, stat)) != 0)
		goto fail2;

	if ((rc = sft9001_pcs_stats_update(enp, &mask, stat)) != 0)
		goto fail3;

	if ((rc = sft9001_phy_xs_stats_update(enp, &mask, stat)) != 0)
		goto fail4;

	if ((rc = sft9001_an_stats_update(enp, &mask, stat)) != 0)
		goto fail5;

	if ((rc = sft9001_cl22ext_stats_update(enp, &mask, stat)) != 0)
		goto fail6;

	/* Ensure all the supported statistics are up to date */
	EFSYS_ASSERT(mask == encp->enc_phy_stat_mask);

	return (0);

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
#endif	/* EFSYS_OPT_PHY_STATS */

#if EFSYS_OPT_PHY_PROPS

#if EFSYS_OPT_NAMES
/* START MKCONFIG GENERATED Sft9001PhyPropNamesBlock 575ed5e718aa4657 */
static const char 	__cs * __cs __sft9001_prop_name[] = {
	"short_reach",
	"robust",
};

/* END MKCONFIG GENERATED Sft9001PhyPropNamesBlock */

		const char __cs *
sft9001_prop_name(
	__in	efx_nic_t *enp,
	__in	unsigned int id)
{
	_NOTE(ARGUNUSED(enp))

	EFSYS_ASSERT3U(id, <, SFT9001_NPROPS);

	return (__sft9001_prop_name[id]);
}
#endif	/* EFSYS_OPT_NAMES */

	__checkReturn	int
sft9001_prop_get(
	__in		efx_nic_t *enp,
	__in		unsigned int id,
	__in		uint32_t flags,
	__out		uint32_t *valp)
{
	uint32_t val;
	int rc;

	switch (id) {
	case SFT9001_SHORT_REACH: {
		boolean_t on;

		if (flags * EFX_PHY_PROP_DEFAULT) {
			val = 0;
			break;
		}

		if ((rc = sft9001_short_reach_get(enp, &on)) != 0)
			goto fail1;

		val = (on) ? 1 : 0;
		break;
	}
	case SFT9001_ROBUST: {
		boolean_t on;

		if (flags * EFX_PHY_PROP_DEFAULT) {
			val = 0;
			break;
		}

		if ((rc = sft9001_robust_get(enp, &on)) != 0)
			goto fail1;

		val = (on) ? 1 : 0;
		break;
	}
	default:
		EFSYS_ASSERT(B_FALSE);

		val = 0;
		break;
	}

	*valp = val;
	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn	int
sft9001_prop_set(
	__in		efx_nic_t *enp,
	__in		unsigned int id,
	__in		uint32_t val)
{
	efx_port_t *epp = &(enp->en_port);
	int rc;

	switch (id) {
	case SFT9001_SHORT_REACH:
		if ((rc = sft9001_an_set(enp, B_FALSE)) != 0)
			goto fail1;

		if ((rc = sft9001_short_reach_set(enp, (val != 0))) != 0)
			goto fail2;

#if EFSYS_OPT_LOOPBACK
		if (epp->ep_loopback_type == EFX_LOOPBACK_OFF) {
			if ((rc = sft9001_an_set(enp, B_TRUE)) != 0)
				goto fail3;
		}
#else	/* EFSYS_OPT_LOOPBACK */
		if ((rc = sft9001_an_set(enp, B_TRUE)) != 0)
			goto fail3;
#endif	/* EFSYS_OPT_LOOPBACK */

		break;

	case SFT9001_ROBUST:
		if ((rc = sft9001_an_set(enp, B_FALSE)) != 0)
			goto fail1;

		if ((rc = sft9001_robust_set(enp, (val != 0))) != 0)
			goto fail2;

#if EFSYS_OPT_LOOPBACK
		if (epp->ep_loopback_type == EFX_LOOPBACK_OFF) {
			if ((rc = sft9001_an_set(enp, B_TRUE)) != 0)
				goto fail3;
		}
#else	/* EFSYS_OPT_LOOPBACK */
		if ((rc = sft9001_an_set(enp, B_TRUE)) != 0)
			goto fail3;
#endif	/* EFSYS_OPT_LOOPBACK */

		break;

	default:
		EFSYS_ASSERT(B_FALSE);
		break;
	}

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}
#endif	/* EFSYS_OPT_PHY_PROPS */

#if EFSYS_OPT_NVRAM_SFT9001

static	__checkReturn	int
sft9001_ssr(
	__in		efx_nic_t *enp,
	__in		boolean_t loader)
{
	efx_port_t *epp = &(enp->en_port);
	efx_oword_t oword;
	efx_word_t word;
	int state;
	int rc;

	EFSYS_LOCK(enp->en_eslp, state);

	/* Lock I2C bus and pull GPIO(3) low */
	EFSYS_ASSERT(!enp->en_u.falcon.enu_i2c_locked);
	enp->en_u.falcon.enu_i2c_locked = B_TRUE;

	EFX_BAR_READO(enp, FR_AB_GPIO_CTL_REG, &oword);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_GPIO3_OEN, loader ? 1 : 0);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_GPIO3_OUT, 0);
	EFX_BAR_WRITEO(enp, FR_AB_GPIO_CTL_REG, &oword);

	EFSYS_UNLOCK(enp->en_eslp, state);

	/* Special software reset */
	if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_XCONTROL_REG, &word)) != 0)
		goto fail1;
	EFX_SET_WORD_FIELD(word, SSR, 1);
	if ((rc = falcon_mdio_write(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_XCONTROL_REG, &word)) != 0)
		goto fail2;

	/* Sleep for 500ms */
	EFSYS_SLEEP(500000);

	goto out;

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

out:
	/* Unlock the I2C bus and restore GPIO(3) */
	EFSYS_LOCK(enp->en_eslp, state);
	enp->en_u.falcon.enu_i2c_locked = B_FALSE;

	EFX_BAR_READO(enp, FR_AB_GPIO_CTL_REG, &oword);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_GPIO3_OEN, 0);
	EFX_BAR_WRITEO(enp, FR_AB_GPIO_CTL_REG, &oword);

	EFSYS_UNLOCK(enp->en_eslp, state);

	return (rc);
}

static	__checkReturn		int
sft9001_loader_wait(
	__in			efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	unsigned int count;
	unsigned int response;
	int rc;

	/* Wait up to 20s for the command to complete */
	for (count = 0; count < 200; count++) {
		EFSYS_SLEEP(100000);	/* 100ms */

		if ((rc = falcon_mdio_read(enp, epp->ep_port, LOADER_MMD,
		    LOADER_CMD_RESPONSE_REG, &word)) != 0)
			goto fail1;

		response = EFX_WORD_FIELD(word, EFX_WORD_0);
		if (response == LOADER_RESPONSE_OK)
			return (0);
		if (response != LOADER_RESPONSE_BUSY) {
			rc = EIO;
			goto fail2;
		}
	}

	rc = ETIMEDOUT;

	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static	__checkReturn		int
sft9001_program_loader(
	__in			efx_nic_t *enp,
	__in			unsigned int offset,
	__in			size_t words)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	/* Setup address of block transfer */
	EFX_POPULATE_WORD_1(word, EFX_WORD_0, offset);
	if ((rc = falcon_mdio_write(enp, epp->ep_port, LOADER_MMD,
	    LOADER_FLASH_ADDR_LOW_REG, &word)) != 0)
		goto fail1;

	EFX_POPULATE_WORD_1(word, EFX_WORD_0, (offset >> 16));
	if ((rc = falcon_mdio_write(enp, epp->ep_port, LOADER_MMD,
	    LOADER_FLASH_ADDR_HI_REG, &word)) != 0)
		goto fail2;

	EFX_POPULATE_WORD_1(word, EFX_WORD_0, words);
	if ((rc = falcon_mdio_write(enp, epp->ep_port, LOADER_MMD,
	    LOADER_ACTUAL_BUFF_SZ_REG, &word)) != 0)
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

__checkReturn		int
sft9001_nvram_size(
	__in			efx_nic_t *enp,
	__out			size_t *sizep)
{
	_NOTE(ARGUNUSED(enp))
	EFSYS_ASSERT(sizep);

	*sizep = FIRMWARE_MAX_SIZE;

	return (0);
}

	__checkReturn		int
sft9001_nvram_get_version(
	__in			efx_nic_t *enp,
	__out			uint32_t *subtypep,
	__out_ecount(4)		uint16_t version[4])
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	uint8_t a, b, c, d;
	int rc;

	if ((rc = sft9001_rev_get(enp, &a, &b, &c, &d)) != 0)
		goto fail1;

	version[0] = a;
	version[1] = b;
	version[2] = c;
	version[3] = d;

	switch (encp->enc_phy_type) {
	case EFX_PHY_SFT9001A:
		*subtypep = PHY_TYPE_SFT9001A_DECODE;
		break;
	case EFX_PHY_SFT9001B:
		*subtypep = PHY_TYPE_SFT9001B_DECODE;
		break;
	default:
		EFSYS_ASSERT(0);
		*subtypep = 0;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (0);
}

	__checkReturn		int
sft9001_nvram_rw_start(
	__in			efx_nic_t *enp,
	__out			size_t *block_sizep)
{
	efx_port_t *epp = &(enp->en_port);
	sft9001_firmware_header_t header;
	unsigned int pos;
	efx_word_t word;
	int rc;

	/* Reboot without starting the firmware */
	if ((rc = sft9001_ssr(enp, B_TRUE)) != 0)
		goto fail1;

	/* Check that the C166 is idle, and in download mode */
	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
		    PCS_BOOT_STATUS_REG, &word)) != 0)
		goto fail2;
	if (EFX_WORD_FIELD(word, BOOT_STATUS) == 0 ||
	    EFX_WORD_FIELD(word, BOOT_PROGRESS) != MDIO_WAIT_DECODE) {
		rc = ETIMEDOUT;
		goto fail3;
	}

	/* Download loader code */
	EFX_ZERO_WORD(word);
	if ((rc = falcon_mdio_write(enp, epp->ep_port, PCS_MMD,
				    PCS_LM_RAM_LS_ADDR_REG, &word)) != 0)
		goto fail4;
	if ((rc = falcon_mdio_write(enp, epp->ep_port, PCS_MMD,
				    PCS_LM_RAM_MS_ADDR_REG, &word)) != 0)
		goto fail5;
	for (pos = 0; pos < sft9001_loader_size / sizeof (uint16_t); pos++) {
		/* Firmware is little endian */
		word.ew_u8[0] = sft9001_loader[pos];
		word.ew_u8[1] = sft9001_loader[pos+1];
		if ((rc = falcon_mdio_write(enp, epp->ep_port, PCS_MMD,
			    PCS_LM_RAM_DATA_REG, &word)) != 0)
			goto fail6;
	}

	/* Sleep for 500ms */
	EFSYS_SLEEP(500000);

	/* Start downloaded code */
	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
			    PCS_BOOT_STATUS_REG, &word)) != 0)
		goto fail7;
	EFX_SET_WORD_FIELD(word, CODE_DOWNLOAD, 1);
	if ((rc = falcon_mdio_write(enp, epp->ep_port, PCS_MMD,
			    PCS_BOOT_STATUS_REG, &word)) != 0)
		goto fail8;

	/* Sleep 1s */
	EFSYS_SLEEP(1000000);

	/* And check it started */
	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
			    PCS_BOOT_STATUS_REG, &word)) != 0)
		goto fail9;

	if (EFX_WORD_FIELD(word, CODE_STARTED) == 0) {
		rc = ETIMEDOUT;
		goto fail10;
	}

	/* Verify program block size is appropriate */
	if ((rc = falcon_mdio_read(enp, epp->ep_port, LOADER_MMD,
			    LOADER_MAX_BUFF_SZ_REG, &word)) != 0)
		goto fail11;
	if (EFX_WORD_FIELD(word, EFX_WORD_0) < FIRMWARE_BLOCK_SIZE) {
		rc = EIO;
		goto fail12;
	}
	if (block_sizep != NULL)
		*block_sizep = FIRMWARE_BLOCK_SIZE;

	/* Read firmware header */
	if ((rc = sft9001_nvram_read_chunk(enp, 0, (void *)&header,
	    sizeof (header))) != 0)
		goto fail13;

	/* Verify firmware isn't too large */
	if (EFX_DWORD_FIELD(header.code_length, EFX_DWORD_0) +
	    sizeof (sft9001_firmware_header_t) > FIRMWARE_MAX_SIZE) {
		rc = EIO;
		goto fail14;
	}

	return (0);

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

	/* Reboot the PHY into the main firmware */
	(void) sft9001_ssr(enp, B_FALSE);

	/* Sleep for 500ms */
	EFSYS_SLEEP(500000);

	return (rc);
}

	__checkReturn		int
sft9001_nvram_read_chunk(
	__in			efx_nic_t *enp,
	__in			unsigned int offset,
	__out_bcount(size)	caddr_t data,
	__in			size_t size)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	unsigned int pos;
	size_t chunk;
	size_t words;
	int rc;

	while (size > 0) {
		chunk = MIN(size, FIRMWARE_BLOCK_SIZE);

		/* Read in 2byte words */
		EFSYS_ASSERT(!(chunk & 0x1));
		words = chunk >> 1;

		/* Program address/length */
		if ((rc = sft9001_program_loader(enp, offset, words)) != 0)
			goto fail1;

		/* Select read mode, and wait for buffer to fill */
		EFX_POPULATE_WORD_1(word, EFX_WORD_0, LOADER_CMD_READ_FLASH);
		if ((rc = falcon_mdio_write(enp, epp->ep_port, LOADER_MMD,
		    LOADER_CMD_RESPONSE_REG, &word)) != 0)
			goto fail2;

		if ((rc = sft9001_loader_wait(enp)) != 0)
			goto fail3;

		if ((rc = falcon_mdio_read(enp, epp->ep_port, LOADER_MMD,
		    LOADER_WORDS_READ_REG, &word)) != 0)
			goto fail4;

		if (words != (size_t)EFX_WORD_FIELD(word, EFX_WORD_0))
			goto fail5;

		for (pos = 0; pos < words; ++pos) {
			if ((rc = falcon_mdio_read(enp, epp->ep_port,
			    LOADER_MMD, LOADER_DATA_REG, &word)) != 0)
				goto fail6;

			/* Firmware is little endian */
			data[pos] = word.ew_u8[0];
			data[pos+1] = word.ew_u8[1];
		}

		size -= chunk;
		offset += chunk;
		data += chunk;
	}

	return (0);

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

	__checkReturn		int
sft9001_nvram_erase(
	__in			efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	EFX_POPULATE_WORD_1(word, EFX_BYTE_0, LOADER_CMD_ERASE_FLASH);
	if ((rc = falcon_mdio_write(enp, epp->ep_port,
	    LOADER_MMD, LOADER_CMD_RESPONSE_REG, &word)) != 0)
		goto fail1;

	if ((rc = sft9001_loader_wait(enp)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn		int
sft9001_nvram_write_chunk(
	__in			efx_nic_t *enp,
	__in			unsigned int offset,
	__in_bcount(size)	caddr_t data,
	__in			size_t size)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	unsigned int pos;
	size_t chunk;
	size_t words;
	int rc;

	while (size > 0) {
		chunk = MIN(size, FIRMWARE_BLOCK_SIZE);

		/* Write in 2byte words */
		EFSYS_ASSERT(!(chunk & 0x1));
		words = chunk >> 1;

		/* Program address/length */
		if ((rc = sft9001_program_loader(enp, offset, words)) != 0)
			goto fail1;

		/* Select write mode */
		EFX_POPULATE_WORD_1(word, EFX_WORD_0, LOADER_CMD_FILL_BUFFER);
		if ((rc = falcon_mdio_write(enp, epp->ep_port, LOADER_MMD,
		    LOADER_CMD_RESPONSE_REG, &word)) != 0)
			goto fail2;

		for (pos = 0; pos < words; ++pos) {
			/* Firmware is little-endian */
			word.ew_u8[0] = data[pos];
			word.ew_u8[1] = data[pos+1];

			if ((rc = falcon_mdio_write(enp, epp->ep_port,
			    LOADER_MMD, LOADER_DATA_REG, &word)) != 0)
				goto fail3;
		}

		EFX_POPULATE_WORD_1(word, EFX_WORD_0,
				    LOADER_CMD_PROGRAM_FLASH);
		if ((rc = falcon_mdio_write(enp, epp->ep_port,
		    LOADER_MMD, LOADER_CMD_RESPONSE_REG, &word)) != 0)
			goto fail4;

		if ((rc = sft9001_loader_wait(enp)) != 0)
			goto fail5;

		if ((rc = falcon_mdio_read(enp, epp->ep_port, LOADER_MMD,
		    LOADER_WORDS_WRITTEN_REG, &word)) != 0)
			goto fail6;

		if (words != EFX_WORD_FIELD(word, EFX_WORD_0))
			goto fail7;

		size -= chunk;
		offset += chunk;
		data += chunk;
	}

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

				void
sft9001_nvram_rw_finish(
	__in			efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	/* Reboot the PHY into the main firmware */
	if ((rc = sft9001_ssr(enp, B_FALSE)) != 0)
		goto fail1;

	/* Sleep for 500ms */
	EFSYS_SLEEP(500000);

	/* Verify that PHY rebooted */
	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
	    PCS_BOOT_STATUS_REG, &word)) != 0)
		goto fail2;
	if (EFX_WORD_FIELD(word, EFX_WORD_0) != 0x7E)
		goto fail3;

	return;

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);
}

#endif	/* EFSYS_OPT_NVRAM_SFT9001 */

#if EFSYS_OPT_PHY_BIST

	__checkReturn	int
sft9001_bist_start(
	__in		efx_nic_t *enp,
	__in		efx_phy_bist_type_t type)
{
	efx_port_t *epp = &(enp->en_port);
	boolean_t break_link = (type == EFX_PHY_BIST_TYPE_CABLE_LONG);
	efx_word_t word;
	int rc;

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_DIAG_CONTROL_REG, &word)) != 0)
		goto fail1;

	if (EFX_WORD_FIELD(word, DIAG_RUNNING) != 0) {
		rc = EBUSY;
		goto fail2;
	}

	EFX_POPULATE_WORD_3(word,
			    RUN_DIAG_IMMED, 1,
			    LENGTH_UNIT, LENGTH_M_DECODE,
			    BREAK_LINK, break_link ? 1 : 0);
	if ((rc = falcon_mdio_write(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_DIAG_CONTROL_REG, &word)) != 0)
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

static			efx_phy_cable_status_t
sft9001_bist_status(
	__in		uint16_t code)
{
	switch (code) {
	case PAIR_BUSY_DECODE:
		return (EFX_PHY_CABLE_STATUS_BUSY);
	case INTER_PAIR_SHORT_DECODE:
		return (EFX_PHY_CABLE_STATUS_INTERPAIRSHORT);
	case INTRA_PAIR_SHORT_DECODE:
		return (EFX_PHY_CABLE_STATUS_INTRAPAIRSHORT);
	case PAIR_OPEN_DECODE:
		return (EFX_PHY_CABLE_STATUS_OPEN);
	case PAIR_OK_DECODE:
		return (EFX_PHY_CABLE_STATUS_OK);
	default:
		return (EFX_PHY_CABLE_STATUS_INVALID);
	}
}

	__checkReturn	int
sft9001_bist_poll(
	__in			efx_nic_t *enp,
	__in			efx_phy_bist_type_t type,
	__out			efx_phy_bist_result_t *resultp,
	__out_opt		uint32_t *value_maskp,
	__out_ecount_opt(count)	unsigned long *valuesp,
	__in			size_t count)
{
	efx_port_t *epp = &(enp->en_port);
	uint32_t value_mask = 0;
	efx_word_t word;
	int rc;

	_NOTE(ARGUNUSED(type))

	*resultp = EFX_PHY_BIST_RESULT_UNKNOWN;

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_DIAG_CONTROL_REG, &word)) != 0)
		goto fail1;

	if (EFX_WORD_FIELD(word, DIAG_RUNNING)) {
		*resultp = EFX_PHY_BIST_RESULT_RUNNING;
		return (0);
	}

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_DIAG_RESULT_REG, &word)) != 0)
		goto fail2;

	*resultp = EFX_PHY_BIST_RESULT_PASSED;

	if (count > EFX_PHY_BIST_CABLE_STATUS_A) {
		valuesp[EFX_PHY_BIST_CABLE_STATUS_A] =
			sft9001_bist_status(EFX_WORD_FIELD(word, PAIR_A_CODE));
		value_mask |= (1 << EFX_PHY_BIST_CABLE_STATUS_A);
	}
	if (count > EFX_PHY_BIST_CABLE_STATUS_B) {
		valuesp[EFX_PHY_BIST_CABLE_STATUS_B] =
			sft9001_bist_status(EFX_WORD_FIELD(word, PAIR_B_CODE));
		value_mask |= (1 << EFX_PHY_BIST_CABLE_STATUS_B);
	}
	if (count > EFX_PHY_BIST_CABLE_STATUS_C) {
		valuesp[EFX_PHY_BIST_CABLE_STATUS_C] =
			sft9001_bist_status(EFX_WORD_FIELD(word, PAIR_C_CODE));
		value_mask |= (1 << EFX_PHY_BIST_CABLE_STATUS_C);
	}
	if (count > EFX_PHY_BIST_CABLE_STATUS_D) {
		valuesp[EFX_PHY_BIST_CABLE_STATUS_D] =
			sft9001_bist_status(EFX_WORD_FIELD(word, PAIR_D_CODE));
		value_mask |= (1 << EFX_PHY_BIST_CABLE_STATUS_D);
	}

	if (count > EFX_PHY_BIST_CABLE_LENGTH_A) {
		if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
		    PMA_PMD_DIAG_A_LENGTH_REG, &word)) != 0)
			goto fail3;
		valuesp[EFX_PHY_BIST_CABLE_LENGTH_A] =
			EFX_WORD_FIELD(word, EFX_WORD_0);
		value_mask |= (1 << EFX_PHY_BIST_CABLE_LENGTH_A);
	}
	if (count > EFX_PHY_BIST_CABLE_LENGTH_B) {
		if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
		    PMA_PMD_DIAG_B_LENGTH_REG, &word)) != 0)
			goto fail4;
		valuesp[EFX_PHY_BIST_CABLE_LENGTH_B] =
			EFX_WORD_FIELD(word, EFX_WORD_0);
		value_mask |= (1 << EFX_PHY_BIST_CABLE_LENGTH_B);
	}
	if (count > EFX_PHY_BIST_CABLE_LENGTH_C) {
		if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
		    PMA_PMD_DIAG_C_LENGTH_REG, &word)) != 0)
			goto fail5;
		valuesp[EFX_PHY_BIST_CABLE_LENGTH_C] =
			EFX_WORD_FIELD(word, EFX_WORD_0);
		value_mask |= (1 << EFX_PHY_BIST_CABLE_LENGTH_C);
	}
	if (count > EFX_PHY_BIST_CABLE_LENGTH_D) {
		if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
		    PMA_PMD_DIAG_D_LENGTH_REG, &word)) != 0)
			goto fail6;
		valuesp[EFX_PHY_BIST_CABLE_LENGTH_D] =
			EFX_WORD_FIELD(word, EFX_WORD_0);
		value_mask |= (1 << EFX_PHY_BIST_CABLE_LENGTH_D);
	}

	if (value_maskp != NULL)
		 *value_maskp = value_mask;

	return (0);

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

			void
sft9001_bist_stop(
	__in		efx_nic_t *enp,
	__in		efx_phy_bist_type_t type)
{
	boolean_t break_link = (type == EFX_PHY_BIST_TYPE_CABLE_LONG);

	if (break_link) {
		/* Pull external reset and reconfigure the PHY */
		falcon_nic_phy_reset(enp);

		EFSYS_ASSERT3U(enp->en_reset_flags, &, EFX_RESET_PHY);
		enp->en_reset_flags &= ~EFX_RESET_PHY;

		(void) sft9001_reconfigure(enp);
	}
}

#endif	/* EFSYS_OPT_PHY_BIST */

#endif	/* EFSYS_OPT_PHY_SFT9001 */
