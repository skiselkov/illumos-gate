/*
 * Copyright 2006-2013 Solarflare Communications Inc.  All rights reserved.
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

#if EFSYS_OPT_FALCON

static	__checkReturn	int
xphy_mmd_reset_wait(
	__in		efx_nic_t *enp,
	__in		uint8_t port,
	__in		uint8_t mmd)
{
	unsigned int count;
	int rc;

	/* The Clause 22 extenion MMD does not implement IEEE registers */
	if (mmd == CL22EXT_MMD) {
		rc = ENOTSUP;
		goto fail1;
	}

	count = 0;
	do {
		efx_word_t word;

		EFSYS_PROBE1(wait, unsigned int, count);

		/* Sleep for 100 ms */
		EFSYS_SLEEP(100000);

		/* Read control 1 register and check reset state */
		if ((rc = falcon_mdio_read(enp, port, mmd,
		    MMD_CONTROL1_REG, &word)) != 0)
			goto fail2;

		if (EFX_WORD_FIELD(word, MMD_RESET) == 0)
			goto done;

	} while (++count < 100);

	rc = ETIMEDOUT;
	goto fail3;

done:
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
xphy_pkg_wait(
	__in		efx_nic_t *enp,
	__in		uint8_t port,
	__in		uint32_t mask)
{
	uint8_t mmd;
	int rc;

	for (mmd = 0; mmd <= MAXMMD; mmd++) {
		/* Only check MMDs in the mask */
		if (((1 << mmd) & mask) == 0)
			continue;

		/*
		 * The Clause 22 extenion MMD does not implement IEEE
		 * registers.
		 */
		if (mmd == CL22EXT_MMD)
			continue;

		/* Wait for the MMD to come out of reset */
		if ((rc = xphy_mmd_reset_wait(enp, port, mmd)) != 0)
			goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn	int
xphy_pkg_verify(
	__in		efx_nic_t *enp,
	__in		uint8_t port,
	__in		uint32_t mask)
{
	uint8_t mmd;
	efx_word_t word;
	int rc;

	/* Find the first MMD */
	for (mmd = 0; mmd <= MAXMMD; mmd++) {
		uint32_t devices;

		if (((1 << mmd) & mask) == 0)
			continue;

		/*
		 * The Clause 22 extenion MMD does not implement IEEE
		 * registers.
		 */
		if (mmd == CL22EXT_MMD)
			continue;

		if ((rc = falcon_mdio_read(enp, port, mmd,
		    MMD_DEVICES2_REG, &word)) != 0)
			goto fail1;

		devices = (uint32_t)EFX_WORD_FIELD(word, EFX_WORD_0) << 16;

		if ((rc = falcon_mdio_read(enp, port, mmd,
		    MMD_DEVICES1_REG, &word)) != 0)
			goto fail2;

		devices |= (uint32_t)EFX_WORD_FIELD(word, EFX_WORD_0);

		EFSYS_PROBE1(devices, uint32_t, devices);

		/* Check that all the specified MMDs are present */
		if ((devices & mask) != mask) {
			rc = EFAULT;
			goto fail3;
		}

		goto done;
	}

	rc = ENODEV;
	goto fail4;

done:
	for (mmd = 0; mmd <= MAXMMD; mmd++) {
		/* Only check MMDs in the mask */
		if (((1 << mmd) & mask) == 0)
			continue;

		/*
		 * The Clause 22 extenion MMD does not implement IEEE
		 * registers.
		 */
		if (mmd == CL22EXT_MMD)
			continue;

		/* Check the MMD is responding */
		if ((rc = falcon_mdio_read(enp, port, mmd,
		    MMD_STATUS2_REG, &word)) != 0)
			goto fail5;

		if (EFX_WORD_FIELD(word, MMD_RESPONDING) !=
		    MMD_RESPONDING_DECODE)
			goto fail6;

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

	__checkReturn	int
xphy_mmd_oui_get(
	__in		efx_nic_t *enp,
	__in		uint8_t port,
	__in		uint8_t mmd,
	__out		uint32_t *ouip)
{
	efx_word_t word;
	int rc;

	/* The Clause 22 extenion MMD does not implement IEEE registers */
	if (mmd == CL22EXT_MMD) {
		rc = ENOTSUP;
		goto fail1;
	}

	if ((rc = falcon_mdio_read(enp, port, mmd, MMD_IDH_REG,
	    &word)) != 0)
		goto fail2;

	*ouip = (uint32_t)EFX_WORD_FIELD(word, MMD_IDH) << 16;

	if ((rc = falcon_mdio_read(enp, port, mmd, MMD_IDL_REG,
	    &word)) != 0)
		goto fail3;

	*ouip |= (uint32_t)EFX_WORD_FIELD(word, MMD_IDL) & 0x0000ffff;

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
xphy_mmd_check(
	__in		efx_nic_t *enp,
	__in		uint8_t port,
	__in		uint8_t mmd,
	__out		boolean_t *upp)
{
	efx_word_t word;
	int rc;

	/* Reset the latched status and fault flags */
	if ((rc = falcon_mdio_read(enp, port, mmd, MMD_STATUS1_REG,
	    &word)) != 0)
		goto fail1;

	if (mmd == PMA_PMD_MMD || mmd == PCS_MMD || mmd == PHY_XS_MMD) {
		if ((rc = falcon_mdio_read(enp, port, mmd,
		    MMD_STATUS2_REG, &word)) != 0)
			goto fail2;
	}

	/* Check the current status and fault flags */
	if ((rc = falcon_mdio_read(enp, port, mmd, MMD_STATUS1_REG,
	    &word)) != 0)
		goto fail3;

	*upp = (EFX_WORD_FIELD(word, MMD_LINK_UP) != 0 &&
		EFX_WORD_FIELD(word, MMD_FAULT) == 0);

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
xphy_mmd_fault(
	__in		efx_nic_t *enp,
	__in		uint8_t port,
	__out		boolean_t *upp)
{
	efx_word_t word;
	int rc;

	if ((rc = falcon_mdio_read(enp, port, PHY_XS_MMD,
	    MMD_STATUS2_REG, &word)) != 0)
		goto fail1;

	*upp = (EFX_WORD_FIELD(word, MMD_RX_FAULT) == 0);

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn	int
xphy_mmd_loopback_set(
	__in		efx_nic_t *enp,
	__in		uint8_t port,
	__in		uint8_t mmd,
	__in		boolean_t on)
{
	efx_word_t word;
	int rc;

	/* The Clause 22 extenion MMD does not implement IEEE registers */
	if (mmd == CL22EXT_MMD) {
		rc = ENOTSUP;
		goto fail1;
	}

	if ((rc = falcon_mdio_read(enp, port, mmd, MMD_CONTROL1_REG,
	    &word)) != 0)
		goto fail2;

	if (mmd == PMA_PMD_MMD)
		EFX_SET_WORD_FIELD(word, MMD_PMA_LOOPBACK, (on) ? 1 : 0);
	else
		EFX_SET_WORD_FIELD(word, MMD_LOOPBACK, (on) ? 1 : 0);

	if ((rc = falcon_mdio_write(enp, port, mmd, MMD_CONTROL1_REG,
	    &word)) != 0)
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

#endif	/* EFSYS_OPT_FALCON */
