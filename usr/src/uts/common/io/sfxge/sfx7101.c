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
#include "falcon_nvram.h"
#include "sfx7101.h"
#include "sfx7101_impl.h"
#include "xphy.h"

#if EFSYS_OPT_PHY_SFX7101

static	__checkReturn	int
sfx7101_power_on(
	__in		efx_nic_t *enp,
	__in		boolean_t reflash)
{
	efx_byte_t byte;
	int rc;

	if ((rc = falcon_i2c_check(enp, PCA9539)) != 0)
		goto fail1;

	/* Enable all port 0 outputs on IO expander */
	EFX_ZERO_BYTE(byte);

	if ((rc = falcon_i2c_write(enp, PCA9539, P0_CONFIG, (caddr_t)&byte,
	    1)) != 0)
		goto fail2;

	/* Enable necessary port 1 outputs on IO expander */
	EFX_SET_BYTE(byte);
	EFX_SET_BYTE_FIELD(byte, P1_SPARE, 0);

	if ((rc = falcon_i2c_write(enp, PCA9539, P1_CONFIG, (caddr_t)&byte,
	    1)) != 0)
		goto fail3;

	/* Turn off all power rails */
	EFX_SET_BYTE(byte);
	if ((rc = falcon_i2c_write(enp, PCA9539, P0_OUT, (caddr_t)&byte,
	    1)) != 0)
		goto fail4;

	/* Sleep for 1 s */
	EFSYS_SLEEP(1000000);

	/* Turn on 1.2V, 2.5V, and 5V power rails */
	EFX_SET_BYTE(byte);
	EFX_SET_BYTE_FIELD(byte, P0_EN_1V2, 0);
	EFX_SET_BYTE_FIELD(byte, P0_EN_2V5, 0);
	EFX_SET_BYTE_FIELD(byte, P0_EN_5V, 0);
	EFX_SET_BYTE_FIELD(byte, P0_X_TRST, 0);

	/* Disable flash configuration */
	if (!reflash)
		EFX_SET_BYTE_FIELD(byte, P0_FLASH_CFG_EN, 0);

	/* Turn off JTAG */
	EFX_SET_BYTE_FIELD(byte, P0_SHORTEN_JTAG, 0);

	if ((rc = falcon_i2c_write(enp, PCA9539, P0_OUT, (caddr_t)&byte,
	    1)) != 0)
		goto fail5;

	/* Spin for 10 ms */
	EFSYS_SPIN(10000);

	/* Turn on 1V power rail */
	EFX_SET_BYTE_FIELD(byte, P0_EN_1V0X, 0);

	if ((rc = falcon_i2c_write(enp, PCA9539, P0_OUT, (caddr_t)&byte,
	    1)) != 0)
		goto fail6;

	/* Sleep for 1 s */
	EFSYS_SLEEP(1000000);

	enp->en_reset_flags |= EFX_RESET_PHY;

	return (0);

fail6:
	EFSYS_PROBE(fail6);
fail5:
	EFSYS_PROBE(fail5);
fail4:
	EFSYS_PROBE(fail4);

	/* Turn off all power rails */
	EFX_SET_BYTE(byte);

	(void) falcon_i2c_write(enp, PCA9539, P0_OUT, (caddr_t)&byte, 1);

fail3:
	EFSYS_PROBE(fail3);

	/* Disable port 1 outputs on IO expander */
	EFX_SET_BYTE(byte);
	(void) falcon_i2c_write(enp, PCA9539, P1_CONFIG, (caddr_t)&byte, 1);

fail2:
	EFSYS_PROBE(fail2);

	/* Disable port 0 outputs on IO expander */
	EFX_SET_BYTE(byte);
	(void) falcon_i2c_write(enp, PCA9539, P0_CONFIG, (caddr_t)&byte, 1);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static	__checkReturn	int
sfx7101_power_off(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	efx_byte_t byte;
	int rc;

	/* Power down the LNPGA */
	EFX_POPULATE_WORD_1(word, LNPGA_POWERDOWN, 1);
	if ((rc = falcon_mdio_write(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_XCONTROL_REG, &word)) != 0)
		goto fail1;

	/* Sleep for 200 ms */
	EFSYS_SLEEP(200000);

	/* Turn off all power rails */
	EFX_SET_BYTE(byte);
	if ((rc = falcon_i2c_write(enp, PCA9539, P0_OUT, (caddr_t)&byte,
	    1)) != 0)
		goto fail2;

	/* Disable port 1 outputs on IO expander */
	EFX_SET_BYTE(byte);
	if ((rc = falcon_i2c_write(enp, PCA9539, P1_CONFIG, (caddr_t)&byte,
	    1)) != 0)
		goto fail3;

	/* Disable port 0 outputs on IO expander */
	EFX_SET_BYTE(byte);
	if ((rc = falcon_i2c_write(enp, PCA9539, P0_CONFIG, (caddr_t)&byte,
	    1)) != 0)
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

	__checkReturn	int
sfx7101_power(
	__in		efx_nic_t *enp,
	__in		boolean_t on)
{
	int rc;

	if (on) {
		if ((rc = sfx7101_power_on(enp, B_FALSE)) != 0)
			goto fail1;

	} else {
		if ((rc = sfx7101_power_off(enp)) != 0)
			goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn	int
sfx7101_reset(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_XCONTROL_REG, &word)) != 0)
		goto fail1;

	EFX_SET_WORD_FIELD(word, SSR, 1);

	if ((rc = falcon_mdio_write(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_XCONTROL_REG, &word)) != 0)
		goto fail2;

	/* Sleep 500ms */
	EFSYS_SPIN(500000);

	enp->en_reset_flags |= EFX_RESET_PHY;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static	__checkReturn	int
sfx7101_an_set(
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
sfx7101_led_cfg(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_LED_CONTROL_REG, &word)) != 0)
		goto fail1;

	EFX_SET_WORD_FIELD(word, LED_ACTIVITY_EN, 1);

	if ((rc = falcon_mdio_write(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_LED_CONTROL_REG, &word)) != 0)
		goto fail2;

#if EFSYS_OPT_PHY_LED_CONTROL

	switch (epp->ep_phy_led_mode) {
	case EFX_PHY_LED_DEFAULT:
		EFX_POPULATE_WORD_3(word,
		    LED_LINK, LED_NORMAL_DECODE,
		    LED_TX, LED_NORMAL_DECODE,
		    LED_RX, LED_OFF_DECODE);
		break;

	case EFX_PHY_LED_OFF:
		EFX_POPULATE_WORD_3(word,
		    LED_LINK, LED_OFF_DECODE,
		    LED_TX, LED_OFF_DECODE,
		    LED_RX, LED_OFF_DECODE);
		break;

	case EFX_PHY_LED_ON:
		EFX_POPULATE_WORD_3(word,
		    LED_LINK, LED_ON_DECODE,
		    LED_TX, LED_ON_DECODE,
		    LED_RX, LED_ON_DECODE);
		break;

	case EFX_PHY_LED_FLASH:
		EFX_POPULATE_WORD_3(word,
		    LED_LINK, LED_FLASH_DECODE,
		    LED_TX, LED_FLASH_DECODE,
		    LED_RX, LED_FLASH_DECODE);
		break;

	default:
		EFSYS_ASSERT(B_FALSE);
		break;
	}

#else	/* EFSYS_OPT_PHY_LED_CONTROL */

	EFX_POPULATE_WORD_3(word,
	    LED_LINK, LED_NORMAL_DECODE,
	    LED_TX, LED_NORMAL_DECODE,
	    LED_RX, LED_OFF_DECODE);

#endif	/* EFSYS_OPT_PHY_LED_CONTROL */

	if ((rc = falcon_mdio_write(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_LED_OVERRIDE_REG, &word)) != 0)
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
sfx7101_clock_cfg(
	__in		efx_nic_t *enp,
	__in		boolean_t enable)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	EFX_POPULATE_WORD_1(word, CLK312_EN, (enable) ? 1 : 0);

	if ((rc = falcon_mdio_write(enp, epp->ep_port, PCS_MMD,
	    PCS_TEST_SELECT_REG, &word)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static	__checkReturn	int
sfx7101_adv_cap_cfg(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	/* Check base page */
	if ((rc = falcon_mdio_read(enp, epp->ep_port, AN_MMD,
	    AN_ADV_BP_CAP_REG, &word)) != 0)
		goto fail1;

	EFX_SET_WORD_FIELD(word, AN_ADV_TA_PAUSE,
	    ((epp->ep_adv_cap_mask >> EFX_PHY_CAP_PAUSE) & 0x1));
	EFX_SET_WORD_FIELD(word, AN_ADV_TA_ASM_DIR,
	    ((epp->ep_adv_cap_mask >> EFX_PHY_CAP_ASYM) & 0x1));

	if ((rc = falcon_mdio_write(enp, epp->ep_port, AN_MMD,
	    AN_ADV_BP_CAP_REG, &word)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#if EFSYS_OPT_LOOPBACK
static	__checkReturn	int
sfx7101_loopback_cfg(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	switch (epp->ep_loopback_type) {
	case EFX_LOOPBACK_PHY_XS:
		if ((rc = falcon_mdio_read(enp, epp->ep_port, PHY_XS_MMD,
		    PHY_XS_XGXS_TEST_REG, &word)) != 0)
			goto fail1;

		EFX_SET_WORD_FIELD(word, NE_LOOPBACK, 1);

		if ((rc = falcon_mdio_write(enp, epp->ep_port, PHY_XS_MMD,
		    PHY_XS_XGXS_TEST_REG, &word)) != 0)
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
#endif	/* EFSYS_OPT_LOOPBACK */

	__checkReturn	int
sfx7101_reconfigure(
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
	if ((rc = xphy_pkg_wait(enp, epp->ep_port, SFX7101_MMD_MASK)) != 0)
		goto fail3;

	/* Make sure auto-negotiation is off whilst we configure the PHY */
	if ((rc = sfx7101_an_set(enp, B_FALSE)) != 0)
		goto fail4;

	if ((rc = sfx7101_clock_cfg(enp, B_TRUE)) != 0)
		goto fail5;

#if EFSYS_OPT_LOOPBACK
	if ((rc = sfx7101_loopback_cfg(enp)) != 0)
		goto fail6;
#endif	/* EFSYS_OPT_LOOPBACK */

	if ((rc = sfx7101_led_cfg(enp)) != 0)
		goto fail7;

	if ((rc = sfx7101_adv_cap_cfg(enp)) != 0)
		goto fail8;

#if EFSYS_OPT_LOOPBACK
	if (epp->ep_loopback_type == EFX_LOOPBACK_OFF) {
		if ((rc = sfx7101_an_set(enp, B_TRUE)) != 0)
			goto fail9;
	}
#else	/* EFSYS_OPT_LOOPBACK */
	if ((rc = sfx7101_an_set(enp, B_TRUE)) != 0)
		goto fail9;
#endif	/* EFSYS_OPT_LOOPBACK */

	return (0);

fail9:
	EFSYS_PROBE(fail9);
fail8:
	EFSYS_PROBE(fail8);
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
sfx7101_verify(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	int rc;

	if ((rc = xphy_pkg_verify(enp, epp->ep_port, SFX7101_MMD_MASK)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn	int
sfx7101_uplink_check(
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

		void
sfx7101_uplink_reset(
	__in	efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	if (epp->ep_mac_type != EFX_MAC_FALCON_XMAC) {
		rc = ENOTSUP;
		goto fail1;
	}

	/* Disable the clock */
	if ((rc = sfx7101_clock_cfg(enp, B_FALSE)) != 0)
		goto fail2;

	/* Put the XGXS and SERDES into reset */
	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
	    PCS_SOFT_RST2_REG, &word)) != 0)
		goto fail3;

	EFX_SET_WORD_FIELD(word, XGXS_RST_N, 0);
	EFX_SET_WORD_FIELD(word, SERDES_RST_N, 0);

	if ((rc = falcon_mdio_write(enp, epp->ep_port, PCS_MMD,
	    PCS_SOFT_RST2_REG, &word)) != 0)
		goto fail4;

	/* Put the PLL into reset */
	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
	    PCS_CLOCK_CTRL_REG, &word)) != 0)
		goto fail5;

	EFX_SET_WORD_FIELD(word, PLL312_RST_N, 0);

	if ((rc = falcon_mdio_write(enp, epp->ep_port, PCS_MMD,
	    PCS_CLOCK_CTRL_REG, &word)) != 0)
		goto fail6;

	EFSYS_SPIN(10);

	/* Take the PLL out of reset */
	EFX_SET_WORD_FIELD(word, PLL312_RST_N, 1);

	if ((rc = falcon_mdio_write(enp, epp->ep_port, PCS_MMD,
	    PCS_CLOCK_CTRL_REG, &word)) != 0)
		goto fail7;

	EFSYS_SPIN(10);

	/* Take the XGXS and SERDES out of reset */
	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
	    PCS_SOFT_RST2_REG, &word)) != 0)
		goto fail8;

	EFX_SET_WORD_FIELD(word, XGXS_RST_N, 1);
	EFX_SET_WORD_FIELD(word, SERDES_RST_N, 1);

	if ((rc = falcon_mdio_write(enp, epp->ep_port, PCS_MMD,
	    PCS_SOFT_RST2_REG, &word)) != 0)
		goto fail9;

	/* Enable the clock */
	if ((rc = sfx7101_clock_cfg(enp, B_TRUE)) != 0)
		goto fail10;

	/* Sleep 200 ms */
	EFSYS_SLEEP(200000);

	return;

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
}

static	__checkReturn	int
sfx7101_lp_cap_get(
	__in		efx_nic_t *enp,
	__out		unsigned int *maskp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

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

	if ((rc = falcon_mdio_read(enp, epp->ep_port, AN_MMD,
	    AN_10G_BASE_T_STATUS_REG, &word)) != 0)
		goto fail2;

	if (EFX_WORD_FIELD(word, AN_10G_BASE_T_LP) != 0)
		*maskp |= (1 << EFX_PHY_CAP_10000FDX);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn	int
sfx7101_downlink_check(
	__in		efx_nic_t *enp,
	__out		efx_link_mode_t *modep,
	__out		unsigned int *fcntlp,
	__out		uint32_t *lp_cap_maskp)
{
	efx_port_t *epp = &(enp->en_port);
	unsigned int lp_cap_mask = epp->ep_lp_cap_mask;
	unsigned int fcntl = epp->ep_fcntl;
	uint32_t common;
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
	case EFX_LOOPBACK_PMA_PMD:
		rc = xphy_mmd_check(enp, epp->ep_port, PHY_XS_MMD, &up);
		if (rc != 0)
			goto fail1;

		*modep = (up) ? EFX_LINK_10000FDX : EFX_LINK_DOWN;
		goto done;

	default:
		break;
	}

#endif	/* EFSYS_OPT_LOOPBACK */
	rc = xphy_mmd_check(enp, epp->ep_port, AN_MMD, &up);
	if (rc != 0)
		goto fail1;

	/* Check the link partner capabilities */
	if ((rc = sfx7101_lp_cap_get(enp, &lp_cap_mask)) != 0)
		goto fail2;

	/* Resolve the common capabilities */
	common = epp->ep_adv_cap_mask & lp_cap_mask;

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

	*fcntlp = fcntl;
	*lp_cap_maskp = lp_cap_mask;
	*modep = (up) ? EFX_LINK_10000FDX : EFX_LINK_DOWN;

#if EFSYS_OPT_LOOPBACK
done:
#endif
	*fcntlp = epp->ep_fcntl;
	*lp_cap_maskp = epp->ep_lp_cap_mask;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn	int
sfx7101_oui_get(
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

#define	SFX7101_STAT_SET(_stat, _mask, _id, _val)			\
	do {								\
		(_mask) |= (1ULL << (_id));				\
		(_stat)[_id] = (uint32_t)(_val);			\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

static	__checkReturn	int
sfx7101_rev_get(
	__in		efx_nic_t *enp,
	__out		uint16_t *majorp,
	__out		uint16_t *minorp,
	__out		uint16_t *microp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word[2];
	int rc;

	/*
	 * There appear to be two version string formats in use:
	 *
	 * - ('0','major') ('.','minor') with micro == 0
	 * - ('major','.') ('minor','micro')
	 *
	 */

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_FW_REV0_REG, &(word[0]))) != 0)
		goto fail1;

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_FW_REV1_REG, &(word[1]))) != 0)
		goto fail2;

	if (EFX_WORD_FIELD(word[1], EFX_BYTE_0) == '0') {
		*majorp = EFX_WORD_FIELD(word[1], EFX_BYTE_1) - '0';

		if (EFX_WORD_FIELD(word[0], EFX_BYTE_0) != '.')
			goto fail3;

		*minorp = EFX_WORD_FIELD(word[0], EFX_BYTE_1) - '0';
		*microp = 0;

	} else {
		*majorp = EFX_WORD_FIELD(word[1], EFX_BYTE_0) - '0';

		if (EFX_WORD_FIELD(word[1], EFX_BYTE_1) != '.')
			goto fail3;

		*minorp = EFX_WORD_FIELD(word[0], EFX_BYTE_0) - '0';
		*microp = EFX_WORD_FIELD(word[0], EFX_BYTE_1) - '0';
	}

	return (0);

fail3:
	rc = EIO;
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static	__checkReturn			int
sfx7101_pma_pmd_stats_update(
	__in				efx_nic_t *enp,
	__inout				uint64_t *maskp,
	__inout_ecount(EFX_PHY_NSTATS)	uint32_t *stat)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	uint16_t major;
	uint16_t minor;
	uint16_t micro;
	int rc;

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_STATUS1_REG, &word)) != 0)
		goto fail1;

	SFX7101_STAT_SET(stat, *maskp, EFX_PHY_STAT_PMA_PMD_LINK_UP,
	    (EFX_WORD_FIELD(word, PMA_PMD_LINK_UP) != 0) ? 1 : 0);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_STATUS2_REG, &word)) != 0)
		goto fail2;

	SFX7101_STAT_SET(stat, *maskp, EFX_PHY_STAT_PMA_PMD_RX_FAULT,
	    (EFX_WORD_FIELD(word, PMA_PMD_RX_FAULT) != 0) ? 1 : 0);
	SFX7101_STAT_SET(stat, *maskp, EFX_PHY_STAT_PMA_PMD_TX_FAULT,
	    (EFX_WORD_FIELD(word, PMA_PMD_TX_FAULT) != 0) ? 1 : 0);

	if ((rc = sfx7101_rev_get(enp, &major, &minor, &micro)) != 0)
		goto fail3;

	SFX7101_STAT_SET(stat, *maskp, EFX_PHY_STAT_PMA_PMD_REV_MAJOR, major);
	SFX7101_STAT_SET(stat, *maskp, EFX_PHY_STAT_PMA_PMD_REV_MINOR, minor);
	SFX7101_STAT_SET(stat, *maskp, EFX_PHY_STAT_PMA_PMD_REV_MICRO, micro);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_CHANNELA_SNR_REG, &word)) != 0)
		goto fail4;

	SFX7101_STAT_SET(stat, *maskp, EFX_PHY_STAT_SNR_A,
	    EFX_WORD_FIELD(word, PMA_PMD_SNR));

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_CHANNELB_SNR_REG, &word)) != 0)
		goto fail5;

	SFX7101_STAT_SET(stat, *maskp, EFX_PHY_STAT_SNR_B,
	    EFX_WORD_FIELD(word, PMA_PMD_SNR));

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_CHANNELC_SNR_REG, &word)) != 0)
		goto fail6;

	SFX7101_STAT_SET(stat, *maskp, EFX_PHY_STAT_SNR_C,
	    EFX_WORD_FIELD(word, PMA_PMD_SNR));

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_CHANNELD_SNR_REG, &word)) != 0)
		goto fail7;

	SFX7101_STAT_SET(stat, *maskp, EFX_PHY_STAT_SNR_D,
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
sfx7101_pcs_stats_update(
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

	SFX7101_STAT_SET(stat, *maskp, EFX_PHY_STAT_PCS_LINK_UP,
	    (EFX_WORD_FIELD(word, PCS_LINK_UP) != 0) ? 1 : 0);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
	    PCS_STATUS2_REG, &word)) != 0)
		goto fail2;

	SFX7101_STAT_SET(stat, *maskp, EFX_PHY_STAT_PCS_RX_FAULT,
	    (EFX_WORD_FIELD(word, PCS_RX_FAULT) != 0) ? 1 : 0);
	SFX7101_STAT_SET(stat, *maskp, EFX_PHY_STAT_PCS_TX_FAULT,
	    (EFX_WORD_FIELD(word, PCS_TX_FAULT) != 0) ? 1 : 0);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
	    PCS_10GBASE_R_STATUS2_REG, &word)) != 0)
		goto fail3;

	SFX7101_STAT_SET(stat, *maskp, EFX_PHY_STAT_PCS_BER,
	    EFX_WORD_FIELD(word, PCS_BER));
	SFX7101_STAT_SET(stat, *maskp, EFX_PHY_STAT_PCS_BLOCK_ERRORS,
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
sfx7101_phy_xs_stats_update(
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

	SFX7101_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_LINK_UP,
	    (EFX_WORD_FIELD(word, PHY_XS_LINK_UP) != 0) ? 1 : 0);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PHY_XS_MMD,
	    PHY_XS_STATUS2_REG, &word)) != 0)
		goto fail2;

	SFX7101_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_RX_FAULT,
	    (EFX_WORD_FIELD(word, PHY_XS_RX_FAULT) != 0) ? 1 : 0);
	SFX7101_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_TX_FAULT,
	    (EFX_WORD_FIELD(word, PHY_XS_TX_FAULT) != 0) ? 1 : 0);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PHY_XS_MMD,
	    PHY_XS_LANE_STATUS_REG, &word)) != 0)
		goto fail3;

	SFX7101_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_ALIGN,
	    (EFX_WORD_FIELD(word, PHY_XS_ALIGNED) != 0) ? 1 : 0);
	SFX7101_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_SYNC_A,
	    (EFX_WORD_FIELD(word, PHY_XS_LANE0_SYNC) != 0) ? 1 : 0);
	SFX7101_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_SYNC_B,
	    (EFX_WORD_FIELD(word, PHY_XS_LANE1_SYNC) != 0) ? 1 : 0);
	SFX7101_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_SYNC_C,
	    (EFX_WORD_FIELD(word, PHY_XS_LANE2_SYNC) != 0) ? 1 : 0);
	SFX7101_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_SYNC_D,
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
sfx7101_an_stats_update(
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

	SFX7101_STAT_SET(stat, *maskp, EFX_PHY_STAT_AN_LINK_UP,
	    (EFX_WORD_FIELD(word, AN_LINK_UP) != 0) ? 1 : 0);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, AN_MMD,
	    AN_10G_BASE_T_STATUS_REG, &word)) != 0)
		goto fail2;

	SFX7101_STAT_SET(stat, *maskp, EFX_PHY_STAT_AN_MASTER,
	    (EFX_WORD_FIELD(word, AN_MASTER) != 0) ? 1 : 0);
	SFX7101_STAT_SET(stat, *maskp, EFX_PHY_STAT_AN_LOCAL_RX_OK,
	    (EFX_WORD_FIELD(word, AN_LOCAL_RX_OK) != 0) ? 1 : 0);
	SFX7101_STAT_SET(stat, *maskp, EFX_PHY_STAT_AN_REMOTE_RX_OK,
	    (EFX_WORD_FIELD(word, AN_REMOTE_RX_OK) != 0) ? 1 : 0);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn			int
sfx7101_stats_update(
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

	SFX7101_STAT_SET(stat, mask, EFX_PHY_STAT_OUI, oui);

	if ((rc = sfx7101_pma_pmd_stats_update(enp, &mask, stat)) != 0)
		goto fail2;

	if ((rc = sfx7101_pcs_stats_update(enp, &mask, stat)) != 0)
		goto fail3;

	if ((rc = sfx7101_phy_xs_stats_update(enp, &mask, stat)) != 0)
		goto fail4;

	if ((rc = sfx7101_an_stats_update(enp, &mask, stat)) != 0)
		goto fail5;

	/* Ensure all the supported statistics are up to date */
	EFSYS_ASSERT(mask == encp->enc_phy_stat_mask);

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
#endif	/* EFSYS_OPT_PHY_STATS */

#if EFSYS_OPT_PHY_PROPS

#if EFSYS_OPT_NAMES
		const char __cs *
sfx7101_prop_name(
	__in	efx_nic_t *enp,
	__in	unsigned int id)
{
	_NOTE(ARGUNUSED(enp, id))

	EFSYS_ASSERT(B_FALSE);

	return (NULL);
}
#endif	/* EFSYS_OPT_NAMES */

	__checkReturn	int
sfx7101_prop_get(
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
sfx7101_prop_set(
	__in		efx_nic_t *enp,
	__in		unsigned int id,
	__in		uint32_t val)
{
	_NOTE(ARGUNUSED(enp, id, val))

	EFSYS_ASSERT(B_FALSE);

	return (ENOTSUP);
}

#endif	/* EFSYS_OPT_PHY_PROPS */

#if EFSYS_OPT_NVRAM_SFX7101

static	__checkReturn		int
sfx7101_loader_wait(
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
sfx7101_program_loader(
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
sfx7101_nvram_size(
	__in			efx_nic_t *enp,
	__out			size_t *sizep)
{
	_NOTE(ARGUNUSED(enp))
	EFSYS_ASSERT(sizep);

	*sizep = FIRMWARE_MAX_SIZE;

	return (0);
}

	__checkReturn		int
sfx7101_nvram_get_version(
	__in			efx_nic_t *enp,
	__out			uint32_t *subtypep,
	__out_ecount(4)		uint16_t version[4])
{
	uint16_t major, minor, micro;
	int rc;

	if ((rc = sfx7101_rev_get(enp, &major, &minor, &micro)) != 0)
		goto fail1;

	version[0] = major;
	version[1] = minor;
	version[2] = 0;
	version[3] = 0;

	*subtypep = PHY_TYPE_SFX7101_DECODE;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (0);
}

	__checkReturn		int
sfx7101_nvram_rw_start(
	__in			efx_nic_t *enp,
	__out			size_t *block_sizep)
{
	efx_port_t *epp = &(enp->en_port);
	sfx7101_firmware_header_t header;
	efx_word_t word;
	unsigned int pos;
	int rc;

	/* Ensure the PHY is on */
	if ((rc = sfx7101_power_on(enp, B_TRUE)) != 0)
		goto fail1;

	/* Special software reset */
	if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
		    PMA_PMD_XCONTROL_REG, &word)) != 0)
		goto fail2;
	EFX_SET_WORD_FIELD(word, SSR, 1);
	if ((rc = falcon_mdio_write(enp, epp->ep_port, PMA_PMD_MMD,
		    PMA_PMD_XCONTROL_REG, &word)) != 0)
		goto fail3;

	/* Sleep for 500ms */
	EFSYS_SLEEP(500000);

	/* Check that the C166 is idle, and in download mode */
	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
		    PCS_BOOT_STATUS_REG, &word)) != 0)
		goto fail4;
	if (EFX_WORD_FIELD(word, BOOT_STATUS) == 0 ||
	    EFX_WORD_FIELD(word, BOOT_PROGRESS) != MDIO_WAIT_DECODE) {
		rc = ETIMEDOUT;
		goto fail5;
	}

	/* Download loader code into PHY RAM */
	EFX_ZERO_WORD(word);
	if ((rc = falcon_mdio_write(enp, epp->ep_port, PCS_MMD,
		    PCS_LM_RAM_LS_ADDR_REG, &word)) != 0)
		goto fail6;
	if ((rc = falcon_mdio_write(enp, epp->ep_port, PCS_MMD,
		    PCS_LM_RAM_MS_ADDR_REG, &word)) != 0)
		goto fail7;

	for (pos = 0; pos < sfx7101_loader_size / sizeof (uint16_t); pos++) {
		/* Firmware is little endian */
		word.ew_u8[0] = sfx7101_loader[pos];
		word.ew_u8[1] = sfx7101_loader[pos+1];
		if ((rc = falcon_mdio_write(enp, epp->ep_port, PCS_MMD,
			    PCS_LM_RAM_DATA_REG, &word)) != 0)
			goto fail8;
	}

	/* Sleep for 500ms */
	EFSYS_SLEEP(500000);

	/* Start downloaded code */
	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
		    PCS_BOOT_STATUS_REG, &word)) != 0)
		goto fail9;
	EFX_SET_WORD_FIELD(word, CODE_DOWNLOAD, 1);
	if ((rc = falcon_mdio_write(enp, epp->ep_port, PCS_MMD,
		    PCS_BOOT_STATUS_REG, &word)) != 0)
		goto fail10;

	/* Sleep 1s */
	EFSYS_SLEEP(1000000);

	/* And check it started */
	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
		    PCS_BOOT_STATUS_REG, &word)) != 0)
		goto fail11;

	if (!EFX_WORD_FIELD(word, CODE_STARTED)) {
		rc = ETIMEDOUT;
		goto fail12;
	}

	/* Verify program block size is appropriate */
	if ((rc = falcon_mdio_read(enp, epp->ep_port, LOADER_MMD,
		    LOADER_MAX_BUFF_SZ_REG, &word)) != 0)
		goto fail13;
	if (EFX_WORD_FIELD(word, EFX_WORD_0) < FIRMWARE_BLOCK_SIZE) {
		rc = EIO;
		goto fail14;
	}
	if (block_sizep != NULL)
		*block_sizep = FIRMWARE_BLOCK_SIZE;

	/* Read firmware header */
	if ((rc = sfx7101_nvram_read_chunk(enp, 0, (void *)&header,
	    sizeof (header))) != 0)
		goto fail15;

	/* Verify firmware isn't too large */
	if (EFX_DWORD_FIELD(header.code_length, EFX_DWORD_0) +
	    sizeof (sfx7101_firmware_header_t) > FIRMWARE_MAX_SIZE) {
		rc = EIO;
		goto fail16;
	}

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

	/* Restore the original image */
	sfx7101_nvram_rw_finish(enp);

	return (rc);
}

	__checkReturn		int
sfx7101_nvram_read_chunk(
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
		if ((rc = sfx7101_program_loader(enp, offset, words)) != 0)
			goto fail1;

		/* Select read mode, and wait for buffer to fill */
		EFX_POPULATE_WORD_1(word, EFX_WORD_0, LOADER_CMD_READ_FLASH);
		if ((rc = falcon_mdio_write(enp, epp->ep_port, LOADER_MMD,
		    LOADER_CMD_RESPONSE_REG, &word)) != 0)
			goto fail2;

		if ((rc = sfx7101_loader_wait(enp)) != 0)
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

		offset += chunk;
		data += chunk;
		size -= chunk;
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

	__checkReturn				int
sfx7101_nvram_erase(
	__in					efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	EFX_POPULATE_WORD_1(word, EFX_BYTE_0, LOADER_CMD_ERASE_FLASH);
	if ((rc = falcon_mdio_write(enp, epp->ep_port,
	    LOADER_MMD, LOADER_CMD_RESPONSE_REG, &word)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn				int
sfx7101_nvram_write_chunk(
	__in					efx_nic_t *enp,
	__in					unsigned int offset,
	__in_bcount(size)			caddr_t data,
	__in					size_t size)
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
		if ((rc = sfx7101_program_loader(enp, offset, words)) != 0)
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

		if ((rc = sfx7101_loader_wait(enp)) != 0)
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
sfx7101_nvram_rw_finish(
	__in			efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	efx_byte_t byte;
	int rc;

	EFX_SET_BYTE(byte);
	EFX_SET_BYTE_FIELD(byte, P0_EN_1V2, 0);
	EFX_SET_BYTE_FIELD(byte, P0_EN_2V5, 0);
	EFX_SET_BYTE_FIELD(byte, P0_EN_5V, 0);
	EFX_SET_BYTE_FIELD(byte, P0_X_TRST, 0);
	EFX_SET_BYTE_FIELD(byte, P0_FLASH_CFG_EN, 0);

	if ((rc = falcon_i2c_write(enp, PCA9539, P0_OUT,
		    (caddr_t)&byte, 1)) != 0)
		goto fail1;

	/* Special software reset */
	if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
			    PMA_PMD_XCONTROL_REG, &word)) != 0)
		goto fail2;
	EFX_SET_WORD_FIELD(word, SSR, 1);
	if ((rc = falcon_mdio_write(enp, epp->ep_port, PMA_PMD_MMD,
			    PMA_PMD_XCONTROL_REG, &word)) != 0)
		goto fail3;

	/* Sleep 1/2 second */
	EFSYS_SLEEP(500000);

	/* Verify that PHY rebooted */
	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
			    PCS_BOOT_STATUS_REG, &word)) != 0)
		goto fail4;
	if (EFX_WORD_FIELD(word, EFX_WORD_0) != 0x7E)
		goto fail5;

	return;

fail5:
	EFSYS_PROBE(fail4);
fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);
}

#endif	/* EFSYS_OPT_NVRAM_SFX7101 */

#endif	/* EFSYS_OPT_PHY_SFX7101 */
