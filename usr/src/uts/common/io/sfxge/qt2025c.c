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
#include "xphy.h"
#include "qt2025c.h"
#include "qt2025c_impl.h"
#include "falcon_impl.h"

#if EFSYS_OPT_PHY_QT2025C

static	__checkReturn	int
qt2025c_led_cfg(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t led1;
	efx_word_t led2;
	int rc;

#if EFSYS_OPT_PHY_LED_CONTROL

	switch (epp->ep_phy_led_mode) {
	case EFX_PHY_LED_DEFAULT:
		EFX_POPULATE_WORD_2(led1, PMA_PMD_LED_CFG, LED_CFG_LA_DECODE,
		    PMA_PMD_LED_PATH, LED_PATH_RX_DECODE);
		EFX_POPULATE_WORD_2(led2, PMA_PMD_LED_CFG, LED_CFG_LS_DECODE,
		    PMA_PMD_LED_PATH, LED_PATH_RX_DECODE);
		break;

	case EFX_PHY_LED_OFF:
		EFX_POPULATE_WORD_1(led1, PMA_PMD_LED_CFG, LED_CFG_OFF_DECODE);
		EFX_POPULATE_WORD_1(led2, PMA_PMD_LED_CFG, LED_CFG_OFF_DECODE);
		break;

	case EFX_PHY_LED_ON:
		EFX_POPULATE_WORD_1(led1, PMA_PMD_LED_CFG, LED_CFG_ON_DECODE);
		EFX_POPULATE_WORD_1(led2, PMA_PMD_LED_CFG, LED_CFG_ON_DECODE);
		break;

	default:
		EFSYS_ASSERT(B_FALSE);
		break;
	}

#else	/* EFSYS_OPT_PHY_LED_CONTROL */

	EFX_POPULATE_WORD_2(led1, PMA_PMD_LED_CFG, LED_CFG_LA_DECODE,
	    PMA_PMD_LED_PATH, LED_PATH_RX_DECODE);
	EFX_POPULATE_WORD_2(led2, PMA_PMD_LED_CFG, LED_CFG_LS_DECODE,
	    PMA_PMD_LED_PATH, LED_PATH_RX_DECODE);

#endif	/* EFSYS_OPT_PHY_LED_CONTROL */

	if ((rc = falcon_mdio_write(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_LED1_REG, &led1)) != 0)
		goto fail1;

	if ((rc = falcon_mdio_write(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_LED2_REG, &led2)) != 0)
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
qt2025c_loopback_cfg(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	/* Set static mode appropriately */
	if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_FTX_CTRL2_REG, &word)) != 0)
		goto fail1;

	EFX_SET_WORD_FIELD(word, FTX_STATIC,
	    (epp->ep_loopback_type == EFX_LOOPBACK_PCS ||
	    epp->ep_loopback_type == EFX_LOOPBACK_PMA_PMD) ? 1 : 0);

	/* Set static mode appropriately */
	if ((rc = falcon_mdio_write(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_FTX_CTRL2_REG, &word)) != 0)
		goto fail2;

	switch (epp->ep_loopback_type) {
	case EFX_LOOPBACK_PHY_XS: {
		efx_word_t xsword;

		if ((rc = falcon_mdio_read(enp, epp->ep_port, PHY_XS_MMD,
		    XGXS_VENDOR_SPECIFIC_1_REG, &xsword)) != 0)
			goto fail3;

		EFX_SET_WORD_FIELD(xsword, XGXS_SYSLPBK, 1);

		if ((rc = falcon_mdio_write(enp, epp->ep_port, PHY_XS_MMD,
		    XGXS_VENDOR_SPECIFIC_1_REG, &xsword)) != 0)
			goto fail4;

		break;
	}
	case EFX_LOOPBACK_PCS:
		if ((rc = xphy_mmd_loopback_set(enp, epp->ep_port, PCS_MMD,
		    B_TRUE)) != 0)
			goto fail3;

		break;

	case EFX_LOOPBACK_PMA_PMD:
		if ((rc = xphy_mmd_loopback_set(enp, epp->ep_port, PMA_PMD_MMD,
		    B_TRUE)) != 0)
			goto fail3;

		break;

	default:
		break;
	}

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
#endif	/* EFSYS_OPT_LOOPBACK */

static	__checkReturn	int
qt2025c_version_get(
	__in		efx_nic_t *enp,
	__out_ecount(4)	uint8_t version[])
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
	    FW_VERSION1_REG, &word)) != 0)
		goto fail1;

	version[0] = (uint8_t)EFX_WORD_FIELD(word, EFX_BYTE_0) >> 4;
	version[1] = (uint8_t)EFX_WORD_FIELD(word, EFX_BYTE_0) & 0x0f;

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
	    FW_VERSION2_REG, &word)) != 0)
		goto fail2;

	version[2] = (uint8_t)EFX_WORD_FIELD(word, EFX_BYTE_0);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
	    FW_VERSION3_REG, &word)) != 0)
		goto fail3;

	version[3] = (uint8_t)EFX_WORD_FIELD(word, EFX_BYTE_0);

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

/*
 * Convinience function so that the large number of MDIO writes
 * in qt2025c_select_phy_mode are condensed, and easier to compare
 * directly with the data sheet.
 */
static	__checkReturn	int
qt2025c_mdio_write(
	__in		efx_nic_t *enp,
	__in		uint8_t mmd,
	__in		uint16_t addr,
	__in		uint16_t value)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	EFX_POPULATE_WORD_1(word, EFX_WORD_0, value);
	rc = falcon_mdio_write(enp, epp->ep_port, mmd, addr, &word);

	return (rc);
}

static	__checkReturn	int
qt2025c_restart_firmware(
	__in		efx_nic_t *enp)
{
	int rc;

	/* Restart microcontroller execution of firmware from RAM */
	if ((rc = qt2025c_mdio_write(enp, 3, 0xe854, 0x00c0)) != 0)
		goto fail1;

	if ((rc = qt2025c_mdio_write(enp, 3, 0xe854, 0x0040)) != 0)
		goto fail2;

	EFSYS_SLEEP(50000);	/* 50ms */

	return (0);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static	__checkReturn	int
qt2025c_wait_heartbeat(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	unsigned int count;
	uint8_t heartb;
	int rc;

	/* Read the initial heartbeat */
	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
	    FW_HEARTBEAT_REG, &word)) != 0)
		goto fail1;

	heartb = EFX_WORD_FIELD(word, FW_HEARTB);

	/* Wait for the heartbeat to start updating */
	count = 0;
	do {
		EFSYS_PROBE1(wait, unsigned int, count);

		EFSYS_SLEEP(100000);	/* 100ms */

		if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
		    FW_HEARTBEAT_REG, &word)) != 0)
			goto fail2;

		if (EFX_WORD_FIELD(word, FW_HEARTB) != heartb)
			return (0);

	} while (++count < 50);		/* For up to 5s */

	rc = ENOTACTIVE;
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static	__checkReturn	int
qt2025c_wait_firmware(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	unsigned int count;
	uint16_t status;
	int rc;

	count = 0;
	do {
		EFSYS_PROBE1(wait, unsigned int, count);

		EFSYS_SLEEP(100000);	/* 100ms */

		if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
		    UC8051_STATUS_REG, &word)) != 0)
			goto fail1;

		status = EFX_WORD_FIELD(word, UC_STATUS);
		if (status >= UC_STATUS_FW_SAVE_DECODE)
			return (0);

	} while (++count < 25);		/* For up to 2.5s */

	rc = ENOTACTIVE;
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static	__checkReturn	int
qt2025c_wait_reset(
	__in		efx_nic_t *enp)
{
	boolean_t retry = B_TRUE;
	int rc;

	/*
	 * Bug17689: occasionally heartbeat starts but firmware status
	 * code never progresses beyond 0x00.  Try again, once, after
	 * restarting execution of the firmware image.
	 */
again:
	if ((rc = qt2025c_wait_heartbeat(enp)) != 0)
		goto fail1;

	rc = qt2025c_wait_firmware(enp);
	if (rc == ENOTACTIVE && retry) {
		if ((rc = qt2025c_restart_firmware(enp)) != 0)
			goto fail2;

		retry = B_FALSE;
		goto again;
	}
	if (rc != 0)
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

	__checkReturn	int
qt2025c_reset(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	uint8_t version[4];
	int rc;

	/* Pull the external reset line */
	falcon_nic_phy_reset(enp);

	/* Wait for the reset to complete */
	if ((rc = qt2025c_wait_reset(enp)) != 0)
		goto fail1;

	/* Read the firmware version again post-reset */
	if ((rc = qt2025c_version_get(enp, version)) != 0)
		goto fail2;
	epp->ep_fwver = \
	    ((uint32_t)version[0] << 24 | (uint32_t)version[1] << 16 | \
	    (uint32_t)version[2] << 8 | (uint32_t)version[3]);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static	__checkReturn	int
qt2025c_select_phy_mode(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	uint16_t phy_op_mode;
	int i;
	int rc;

	/*
	 * Only 2.0.1.0+ PHY firmware supports the more optimal SFP+
	 * Self-Configure mode.  Don't attempt any switching if we encounter
	 * older firmware.
	 *
	 * NOTE: This code uses explicit integers for MMD's and register
	 * addresses so that it is trivially comparable to the firmware
	 * release notes.
	 */
	if (epp->ep_fwver < 0x02000100)
		return (0);

	/*
	 * In general we will get optimal behaviour in "SFP+ Self-Configure"
	 * mode; however, that powers down most of the PHY when no module is
	 * present, so we must use a different mode (any fixed mode will do)
	 * to be sure that loopbacks will work.
	 */
	phy_op_mode = 0x0038;
#if EFSYS_OPT_LOOPBACK
	if (epp->ep_loopback_type != EFX_LOOPBACK_OFF)
		phy_op_mode = 0x0020;
#endif	/* EFSYS_OPT_LOOPBACK */

	/* Only change mode if really necessary */
	if ((rc = falcon_mdio_read(enp, epp->ep_port, 1, 0xc319, &word)) != 0)
		goto fail1;
	if ((EFX_WORD_FIELD(word, EFX_WORD_0) & 0x0038) == phy_op_mode)
		return (0);

	/* Do a full reset and wait for it to complete */
	if ((rc = qt2025c_mdio_write(enp, 1, 0x0000, 0x8000)) != 0)
		goto fail2;
	EFSYS_SLEEP(50000);		/* 50ms */

	if ((rc = qt2025c_wait_reset(enp)) != 0)
		goto fail3;

	/*
	 * This sequence replicates the register writes configured in the boot
	 * EEPROM (including the differences between board revisions), except
	 * that the operating mode is changed, and the PHY is prevented from
	 * unnecessarily reloading the main firmware image again.
	 */
	if ((rc = qt2025c_mdio_write(enp, 1, 0xc300, 0)) != 0)
		goto fail4;

	/*
	 * (Note: this portion of the boot EEPROM sequence, which bit-bashes 9
	 * STOPs onto the firmware/module I2C bus to reset it, varies across
	 * board revisions, as the bus is connected to different GPIO/LED
	 * outputs on the PHY.)
	 */
	if (enp->en_u.falcon.enu_board_rev < 2) {
		if ((rc = qt2025c_mdio_write(enp, 1, 0xc303, 0x4498)) != 0)
			goto fail5;
		for (i = 0; i < 9; i++) {
			rc = qt2025c_mdio_write(enp, 1, 0xc303, 0x4488);
			if (rc != 0)
				goto fail5;
			rc = qt2025c_mdio_write(enp, 1, 0xc303, 0x4480);
			if (rc != 0)
				goto fail5;
			rc = qt2025c_mdio_write(enp, 1, 0xc303, 0x4490);
			if (rc != 0)
				goto fail5;
			rc = qt2025c_mdio_write(enp, 1, 0xc303, 0x4498);
			if (rc != 0)
				goto fail5;
		}
	} else {
		if ((rc = qt2025c_mdio_write(enp, 1, 0xc303, 0x0920)) != 0)
			goto fail5;
		if ((rc = qt2025c_mdio_write(enp, 1, 0xd008, 0x0004)) != 0)
			goto fail5;
		for (i = 0; i < 9; i++) {
			rc = qt2025c_mdio_write(enp, 1, 0xc303, 0x0900);
			if (rc != 0)
				goto fail5;
			rc = qt2025c_mdio_write(enp, 1, 0xd008, 0x0005);
			if (rc != 0)
				goto fail5;
			rc = qt2025c_mdio_write(enp, 1, 0xc303, 0x0920);
			if (rc != 0)
				goto fail5;
			rc = qt2025c_mdio_write(enp, 1, 0xd008, 0x0004);
			if (rc != 0)
				goto fail5;
		}
		if ((rc = qt2025c_mdio_write(enp, 1, 0xc303, 0x4900)) != 0)
			goto fail5;
	}

	if ((rc = qt2025c_mdio_write(enp, 1, 0xc303, 0x4900)) != 0)
		goto fail6;
	if ((rc = qt2025c_mdio_write(enp, 1, 0xc302, 0x0004)) != 0)
		goto fail6;
	if ((rc = qt2025c_mdio_write(enp, 1, 0xc316, 0x0013)) != 0)
		goto fail6;
	if ((rc = qt2025c_mdio_write(enp, 1, 0xc318, 0x0054)) != 0)
		goto fail6;
	if ((rc = qt2025c_mdio_write(enp, 1, 0xc319, phy_op_mode)) != 0)
		goto fail6;
	if ((rc = qt2025c_mdio_write(enp, 1, 0xc31a, 0x0098)) != 0)
		goto fail6;
	if ((rc = qt2025c_mdio_write(enp, 3, 0x0026, 0x0e00)) != 0)
		goto fail6;
	if ((rc = qt2025c_mdio_write(enp, 3, 0x0027, 0x0013)) != 0)
		goto fail6;
	if ((rc = qt2025c_mdio_write(enp, 3, 0x0028, 0xa528)) != 0)
		goto fail6;
	if ((rc = qt2025c_mdio_write(enp, 1, 0xd006, 0x000a)) != 0)
		goto fail6;
	if ((rc = qt2025c_mdio_write(enp, 1, 0xd007, 0x0009)) != 0)
		goto fail6;
	if ((rc = qt2025c_mdio_write(enp, 1, 0xd008, 0x0004)) != 0)
		goto fail6;
	/*
	 * This additional write prevents the PHY boot ROM doing another
	 * pointless reload of the firmware image (the microcontroller's
	 * code memory is not affected by the microcontroller reset).
	 */
	if ((rc = qt2025c_mdio_write(enp, 1, 0xc317, 0x00ff)) != 0)
		goto fail7;
	if ((rc = qt2025c_mdio_write(enp, 1, 0xc300, 0x0002)) != 0)
		goto fail7;
	EFSYS_SLEEP(20000);		/* 20ms */

	/* Restart microcontroller execution from RAM */
	if ((rc = qt2025c_restart_firmware(enp)) != 0)
		goto fail8;

	/* Wait for the microcontroller to be ready again */
	if ((rc = qt2025c_wait_reset(enp)) != 0)
		goto fail9;

	return (0);

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

	__checkReturn	int
qt2025c_reconfigure(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	int rc;

	if ((rc = qt2025c_select_phy_mode(enp)) != 0)
		goto fail1;

	if ((rc = xphy_pkg_wait(enp, epp->ep_port, QT2025C_MMD_MASK)) != 0)
		goto fail2;

	if ((rc = qt2025c_led_cfg(enp)) != 0)
		goto fail3;

	EFSYS_ASSERT3U(epp->ep_adv_cap_mask, ==, QT2025C_ADV_CAP_MASK);

#if EFSYS_OPT_LOOPBACK
	if ((rc = qt2025c_loopback_cfg(enp)) != 0)
		goto fail4;
#endif	/* EFSYS_OPT_LOOPBACK */

	return (0);

#if EFSYS_OPT_LOOPBACK
fail4:
	EFSYS_PROBE(fail4);
#endif	/* EFSYS_OPT_LOOPBACK */
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn	int
qt2025c_verify(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	int rc;

	if ((rc = xphy_pkg_verify(enp, epp->ep_port, QT2025C_MMD_MASK)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn	int
qt2025c_uplink_check(
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

/* Recover from bug17190 by moving into and out of PMA loopback */
static	__checkReturn	int
qt2025c_bug17190_workaround(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	if ((rc = falcon_mdio_read(enp, epp->ep_port,
	    PMA_PMD_MMD, MMD_CONTROL1_REG, &word)) != 0)
		goto fail1;
	EFX_SET_WORD_FIELD(word, MMD_PMA_LOOPBACK, 1);
	if ((rc = falcon_mdio_write(enp, epp->ep_port,
	    PMA_PMD_MMD, MMD_CONTROL1_REG, &word)) != 0)
		goto fail2;

	EFSYS_SLEEP(100000);	/* 100ms */

	EFX_SET_WORD_FIELD(word, MMD_PMA_LOOPBACK, 0);
	if ((rc = falcon_mdio_write(enp, epp->ep_port,
	    PMA_PMD_MMD, MMD_CONTROL1_REG, &word)) != 0)
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

	__checkReturn	int
qt2025c_downlink_check(
	__in		efx_nic_t *enp,
	__out		efx_link_mode_t *modep,
	__out		unsigned int *fcntlp,
	__out		uint32_t *lp_cap_maskp)
{
	efx_port_t *epp = &(enp->en_port);
	uint8_t mmd;
	uint8_t mmds;
	boolean_t broken;
	boolean_t ok;
	boolean_t up;
	int rc;

	mmds = (1 << PMA_PMD_MMD) | (1 << PCS_MMD) | (1 << PHY_XS_MMD);
#if EFSYS_OPT_LOOPBACK
	switch (epp->ep_loopback_type) {
	case EFX_LOOPBACK_PHY_XS:
		mmds = 0;
		break;
	case EFX_LOOPBACK_PCS:
		mmds = (1 << PHY_XS_MMD);
		break;
	case EFX_LOOPBACK_PMA_PMD:
		mmds = (1 << PHY_XS_MMD) | (1 << PCS_MMD);
		break;
	default:
		break;
	}
#endif /* EFSYS_OPT_LOOPBACK */

	/*
	 * bug17190 is a persistent failure of PCS block lock when PMA
	 * and PHYXS are up. Only check for this when not in phy loopback.
	 */
	broken = !!(mmds & (1 << PMA_PMD_MMD));

	if (!mmds) {
		/* Use the fault state instead */
		if ((rc = xphy_mmd_fault(enp, epp->ep_port, &up)) != 0)
			goto fail1;

	} else {
		up = B_TRUE;
		for (mmd = 0; mmd < MAXMMD; mmd++) {
			if (~mmds & (1 << mmd))
				continue;

			rc = xphy_mmd_check(enp, epp->ep_port, mmd, &ok);
			if (rc != 0)
				goto fail2;
			up &= ok;
			broken &= (ok == (mmd != PCS_MMD));
		}
	}

	/* Recover from bug17190 if we've been stuck there for 3 polls. */
	if (!broken)
		epp->ep_qt2025c.bug17190_count = 0;
	else if (++(epp->ep_qt2025c.bug17190_count) >= 3) {
		epp->ep_qt2025c.bug17190_count = 0;
		EFSYS_PROBE(bug17190_recovery);

		if ((rc = qt2025c_bug17190_workaround(enp)) != 0)
			goto fail3;
	}

	*modep = (up) ? EFX_LINK_10000FDX : EFX_LINK_DOWN;
	*fcntlp = epp->ep_fcntl;
	*lp_cap_maskp = epp->ep_lp_cap_mask;

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
qt2025c_oui_get(
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

#define	QT2025C_STAT_SET(_stat, _mask, _id, _val)			\
	do {								\
		(_mask) |= (1ULL << (_id));				\
		(_stat)[_id] = (uint32_t)(_val);			\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

static	__checkReturn	int
qt2025c_build_get(
	__in		efx_nic_t *enp,
	__out		uint8_t *yp,
	__out		uint8_t *mp,
	__out		uint8_t *dp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	int rc;

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
	    FW_BUILD1_REG, &word)) != 0)
		goto fail1;

	*yp = (uint8_t)EFX_WORD_FIELD(word, EFX_BYTE_0);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
	    FW_BUILD2_REG, &word)) != 0)
		goto fail2;

	*mp = (uint8_t)EFX_WORD_FIELD(word, EFX_BYTE_0);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
	    FW_BUILD3_REG, &word)) != 0)
		goto fail3;

	*dp = (uint8_t)EFX_WORD_FIELD(word, EFX_BYTE_0);

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
qt2025c_pma_pmd_stats_update(
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

	QT2025C_STAT_SET(stat, *maskp, EFX_PHY_STAT_PMA_PMD_LINK_UP,
	    (EFX_WORD_FIELD(word, PMA_PMD_LINK_UP) != 0) ? 1 : 0);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PMA_PMD_MMD,
	    PMA_PMD_STATUS2_REG, &word)) != 0)
		goto fail2;

	QT2025C_STAT_SET(stat, *maskp, EFX_PHY_STAT_PMA_PMD_RX_FAULT,
	    (EFX_WORD_FIELD(word, PMA_PMD_RX_FAULT) != 0) ? 1 : 0);
	QT2025C_STAT_SET(stat, *maskp, EFX_PHY_STAT_PMA_PMD_TX_FAULT,
	    (EFX_WORD_FIELD(word, PMA_PMD_TX_FAULT) != 0) ? 1 : 0);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static	__checkReturn			int
qt2025c_pcs_stats_update(
	__in				efx_nic_t *enp,
	__inout				uint64_t *maskp,
	__inout_ecount(EFX_PHY_NSTATS)	uint32_t *stat)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	uint8_t version[4];
	uint8_t yy;
	uint8_t mm;
	uint8_t dd;
	int rc;

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
	    PCS_STATUS1_REG, &word)) != 0)
		goto fail1;

	QT2025C_STAT_SET(stat, *maskp, EFX_PHY_STAT_PCS_LINK_UP,
	    (EFX_WORD_FIELD(word, PCS_LINK_UP) != 0) ? 1 : 0);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
	    PCS_STATUS2_REG, &word)) != 0)
		goto fail2;

	QT2025C_STAT_SET(stat, *maskp, EFX_PHY_STAT_PCS_RX_FAULT,
	    (EFX_WORD_FIELD(word, PCS_RX_FAULT) != 0) ? 1 : 0);
	QT2025C_STAT_SET(stat, *maskp, EFX_PHY_STAT_PCS_TX_FAULT,
	    (EFX_WORD_FIELD(word, PCS_TX_FAULT) != 0) ? 1 : 0);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
	    PCS_10GBASE_R_STATUS2_REG, &word)) != 0)
		goto fail3;

	QT2025C_STAT_SET(stat, *maskp, EFX_PHY_STAT_PCS_BER,
	    EFX_WORD_FIELD(word, PCS_BER));
	QT2025C_STAT_SET(stat, *maskp, EFX_PHY_STAT_PCS_BLOCK_ERRORS,
	    EFX_WORD_FIELD(word, PCS_ERR));

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PCS_MMD,
	    OP_MODE_REG, &word)) != 0)
		goto fail4;

	QT2025C_STAT_SET(stat, *maskp, EFX_PHY_STAT_PCS_OP_MODE,
	    EFX_WORD_FIELD(word, OP_MODE_CURRENT));

	if ((rc = qt2025c_version_get(enp, version)) != 0)
		goto fail5;

	QT2025C_STAT_SET(stat, *maskp, EFX_PHY_STAT_PCS_FW_VERSION_0,
	    version[0]);
	QT2025C_STAT_SET(stat, *maskp, EFX_PHY_STAT_PCS_FW_VERSION_1,
	    version[1]);
	QT2025C_STAT_SET(stat, *maskp, EFX_PHY_STAT_PCS_FW_VERSION_2,
	    version[2]);
	QT2025C_STAT_SET(stat, *maskp, EFX_PHY_STAT_PCS_FW_VERSION_3,
	    version[3]);

	if ((rc = qt2025c_build_get(enp, &yy, &mm, &dd)) != 0)
		goto fail6;

	QT2025C_STAT_SET(stat, *maskp, EFX_PHY_STAT_PCS_FW_BUILD_YY, yy);
	QT2025C_STAT_SET(stat, *maskp, EFX_PHY_STAT_PCS_FW_BUILD_MM, mm);
	QT2025C_STAT_SET(stat, *maskp, EFX_PHY_STAT_PCS_FW_BUILD_DD, dd);

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

static	__checkReturn			int
qt2025c_phy_xs_stats_update(
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

	QT2025C_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_LINK_UP,
	    (EFX_WORD_FIELD(word, PHY_XS_LINK_UP) != 0) ? 1 : 0);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PHY_XS_MMD,
	    PHY_XS_STATUS2_REG, &word)) != 0)
		goto fail2;

	QT2025C_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_RX_FAULT,
	    (EFX_WORD_FIELD(word, PHY_XS_RX_FAULT) != 0) ? 1 : 0);
	QT2025C_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_TX_FAULT,
	    (EFX_WORD_FIELD(word, PHY_XS_TX_FAULT) != 0) ? 1 : 0);

	if ((rc = falcon_mdio_read(enp, epp->ep_port, PHY_XS_MMD,
	    PHY_XS_LANE_STATUS_REG, &word)) != 0)
		goto fail3;

	QT2025C_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_ALIGN,
	    (EFX_WORD_FIELD(word, PHY_XS_ALIGNED) != 0) ? 1 : 0);
	QT2025C_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_SYNC_A,
	    (EFX_WORD_FIELD(word, PHY_XS_LANE0_SYNC) != 0) ? 1 : 0);
	QT2025C_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_SYNC_B,
	    (EFX_WORD_FIELD(word, PHY_XS_LANE1_SYNC) != 0) ? 1 : 0);
	QT2025C_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_SYNC_C,
	    (EFX_WORD_FIELD(word, PHY_XS_LANE2_SYNC) != 0) ? 1 : 0);
	QT2025C_STAT_SET(stat, *maskp, EFX_PHY_STAT_PHY_XS_SYNC_D,
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
qt2025c_stats_update(
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

	QT2025C_STAT_SET(stat, mask, EFX_PHY_STAT_OUI, oui);

	if ((rc = qt2025c_pma_pmd_stats_update(enp, &mask, stat)) != 0)
		goto fail2;

	if ((rc = qt2025c_pcs_stats_update(enp, &mask, stat)) != 0)
		goto fail3;

	if ((rc = qt2025c_phy_xs_stats_update(enp, &mask, stat)) != 0)
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
qt2025c_prop_name(
	__in	efx_nic_t *enp,
	__in	unsigned int id)
{
	_NOTE(ARGUNUSED(enp, id))

	EFSYS_ASSERT(B_FALSE);

	return (NULL);
}
#endif	/* EFSYS_OPT_NAMES */

	__checkReturn	int
qt2025c_prop_get(
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
qt2025c_prop_set(
	__in		efx_nic_t *enp,
	__in		unsigned int id,
	__in		uint32_t val)
{
	_NOTE(ARGUNUSED(enp, id, val))

	EFSYS_ASSERT(B_FALSE);

	return (ENOTSUP);
}
#endif	/* EFSYS_OPT_PHY_PROPS */

#endif	/* EFSYS_OPT_PHY_QT2025C */
