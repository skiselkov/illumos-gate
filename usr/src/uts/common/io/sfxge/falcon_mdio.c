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

static	__checkReturn	int
falcon_mdio_wait(
	__in		efx_nic_t *enp)
{
	efx_oword_t oword;
	unsigned int count;
	int rc;

	count = 0;
	do {
		EFSYS_PROBE1(wait, unsigned int, count);

		EFX_BAR_READO(enp, FR_AB_MD_STAT_REG, &oword);

		if (EFX_OWORD_FIELD(oword, FRF_AB_MD_BSY) == 0)
			goto done;

		/* Spin for 100 us */
		EFSYS_SPIN(100);

	} while (++count < 100);

	rc = ETIMEDOUT;
	goto fail1;

done:
	if (EFX_OWORD_FIELD(oword, FRF_AB_MD_LNFL) != 0) {
		rc = EIO;
		goto fail2;
	}

	if (EFX_OWORD_FIELD(oword, FRF_AB_MD_BSERR) != 0) {
		rc = EIO;
		goto fail3;
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

	__checkReturn	int
falcon_mdio_write(
	__in		efx_nic_t *enp,
	__in		uint8_t port,
	__in		uint8_t mmd,
	__in		uint16_t reg,
	__in		efx_word_t *ewp)
{
	int state;
	efx_oword_t oword;
	uint32_t prtad;
	uint32_t devad;
	uint32_t addr;
	uint16_t val;
	int rc;

	EFSYS_LOCK(enp->en_eslp, state);

	/* Check MDIO is not currently being accessed */
	if ((rc = falcon_mdio_wait(enp)) != 0)
		goto fail1;

	prtad = port;
	devad = mmd;

	/* Write address */
	EFX_POPULATE_OWORD_2(oword, FRF_AB_MD_PRT_ADR, prtad,
	    FRF_AB_MD_DEV_ADR, devad);
	EFX_BAR_WRITEO(enp, FR_AB_MD_ID_REG, &oword);

	addr = reg;

	EFX_POPULATE_OWORD_1(oword, FRF_AB_MD_PHY_ADR, addr);
	EFX_BAR_WRITEO(enp, FR_AB_MD_PHY_ADR_REG, &oword);

	/* Write data */
	val = EFX_WORD_FIELD(*ewp, EFX_WORD_0);

	EFSYS_PROBE4(mdio_write, uint8_t, port, uint8_t, mmd,
	    uint16_t, reg, uint16_t, val);

	EFX_POPULATE_OWORD_1(oword, FRF_AB_MD_TXD, (uint32_t)val);
	EFX_BAR_WRITEO(enp, FR_AB_MD_TXD_REG, &oword);

	/* Select clause 45 write cycle */
	EFX_POPULATE_OWORD_2(oword, FRF_AB_MD_WRC, 1, FRF_AB_MD_GC, 0);
	EFX_BAR_WRITEO(enp, FR_AB_MD_CS_REG, &oword);

	/* Wait for the cycle to complete */
	if ((rc = falcon_mdio_wait(enp)) != 0)
		goto fail2;

	EFSYS_UNLOCK(enp->en_eslp, state);
	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	EFSYS_UNLOCK(enp->en_eslp, state);
	return (rc);
}

	__checkReturn	int
falcon_mdio_read(
	__in		efx_nic_t *enp,
	__in		uint8_t port,
	__in		uint8_t mmd,
	__in		uint16_t reg,
	__out		efx_word_t *ewp)
{
	int state;
	efx_oword_t oword;
	uint32_t prtad;
	uint32_t devad;
	uint32_t addr;
	uint16_t val;
	int rc;

	EFSYS_LOCK(enp->en_eslp, state);

	/* Check MDIO is not currently being accessed */
	if ((rc = falcon_mdio_wait(enp)) != 0)
		goto fail1;

	prtad = port;
	devad = mmd;

	/* Write address */
	EFX_POPULATE_OWORD_2(oword, FRF_AB_MD_PRT_ADR, prtad,
	    FRF_AB_MD_DEV_ADR, devad);
	EFX_BAR_WRITEO(enp, FR_AB_MD_ID_REG, &oword);

	addr = reg;

	EFX_POPULATE_OWORD_1(oword, FRF_AB_MD_PHY_ADR, addr);
	EFX_BAR_WRITEO(enp, FR_AB_MD_PHY_ADR_REG, &oword);

	/* Clear the data register */
	EFX_SET_OWORD(oword);
	EFX_BAR_WRITEO(enp, FR_AB_MD_RXD_REG, &oword);

	/* Select clause 45 read cycle */
	EFX_POPULATE_OWORD_2(oword, FRF_AB_MD_RDC, 1, FRF_AB_MD_GC, 0);
	EFX_BAR_WRITEO(enp, FR_AB_MD_CS_REG, &oword);

	/* Wait for the cycle to complete */
	if ((rc = falcon_mdio_wait(enp)) != 0)
		goto fail2;

	/* Read data */
	EFX_BAR_READO(enp, FR_AB_MD_RXD_REG, &oword);
	val = (uint16_t)EFX_OWORD_FIELD(oword, FRF_AB_MD_RXD);

	EFSYS_PROBE4(mdio_read, uint8_t, port, uint8_t, mmd,
	    uint16_t, reg, uint16_t, val);

	EFX_POPULATE_WORD_1(*ewp, EFX_WORD_0, val);

	EFSYS_UNLOCK(enp->en_eslp, state);
	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	EFSYS_UNLOCK(enp->en_eslp, state);
	return (rc);
}

#endif	/* EFSYS_OPT_FALCON */
