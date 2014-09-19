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
falcon_sram_reset(
	__in		efx_nic_t *enp)
{
	efx_oword_t oword;
	unsigned int count;
	int rc;

	/* Initiate SRAM reset */
	EFX_POPULATE_OWORD_3(oword,
			    FRF_AZ_SRM_NUM_BANK,
			    enp->en_u.falcon.enu_sram_num_bank,
			    FRF_AZ_SRM_BANK_SIZE,
			    enp->en_u.falcon.enu_sram_bank_size,
			    FRF_AZ_SRM_INIT_EN, 1);
	EFX_BAR_WRITEO(enp, FR_AZ_SRM_CFG_REG, &oword);

	/* Wait for SRAM reset to complete */
	count = 0;
	do {
		EFSYS_PROBE1(wait, unsigned int, count);

		/* Spin for 1 ms */
		EFSYS_SPIN(1000);

		/* Check for reset complete */
		EFX_BAR_READO(enp, FR_AZ_SRM_CFG_REG, &oword);
		if (EFX_OWORD_FIELD(oword, FRF_AZ_SRM_INIT_EN) == 0)
			goto done;
	} while (++count < 100);

	rc = ETIMEDOUT;
	goto fail1;

done:
	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn	int
falcon_sram_init(
	__in		efx_nic_t *enp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_oword_t oword;
	uint32_t tx_base, rx_base;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	/* Position the descriptor caches */
	if (enp->en_u.falcon.enu_internal_sram) {
		tx_base = 0x130000;
		rx_base = 0x100000;
	} else {
		rx_base = encp->enc_buftbl_limit * 8;
		tx_base = rx_base + (encp->enc_rxq_limit * 64 * 8);
	}

	/* Select internal SRAM */
	EFX_BAR_READO(enp, FR_AB_NIC_STAT_REG, &oword);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_ONCHIP_SRAM,
	    enp->en_u.falcon.enu_internal_sram ? 1 : 0);
	EFX_BAR_WRITEO(enp, FR_AB_NIC_STAT_REG, &oword);

	/* Sleep/Wake any external SRAM */
	EFX_BAR_READO(enp, FR_AB_GPIO_CTL_REG, &oword);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_GPIO1_OEN, 1);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_GPIO1_OUT,
	    enp->en_u.falcon.enu_internal_sram ? 1 : 0);
	EFX_BAR_WRITEO(enp, FR_AB_GPIO_CTL_REG, &oword);

	/* Clear the SRAM contents */
	if ((rc = falcon_sram_reset(enp)) != 0)
		goto fail1;

	/* Initialize the buffer table */
	EFX_POPULATE_OWORD_1(oword, FRF_AZ_BUF_TBL_MODE, 1);
	EFX_BAR_WRITEO(enp, FR_AZ_BUF_TBL_CFG_REG, &oword);

	/* Initialize the transmit descriptor cache */
	EFX_POPULATE_OWORD_1(oword, FRF_AZ_SRM_TX_DC_BASE_ADR, tx_base);
	EFX_BAR_WRITEO(enp, FR_AZ_SRM_TX_DC_CFG_REG, &oword);

	EFX_POPULATE_OWORD_1(oword, FRF_AZ_TX_DC_SIZE, 1); /* 16 descriptors */
	EFX_BAR_WRITEO(enp, FR_AZ_TX_DC_CFG_REG, &oword);

	/* Initialize the receive descriptor cache */
	EFX_POPULATE_OWORD_1(oword, FRF_AZ_SRM_RX_DC_BASE_ADR, rx_base);
	EFX_BAR_WRITEO(enp, FR_AZ_SRM_RX_DC_CFG_REG, &oword);

	EFX_POPULATE_OWORD_1(oword, FRF_AZ_RX_DC_SIZE, 3); /* 64 descriptors */
	EFX_BAR_WRITEO(enp, FR_AZ_RX_DC_CFG_REG, &oword);

	/* Set receive descriptor pre-fetch low water mark */
	EFX_POPULATE_OWORD_1(oword, FRF_AZ_RX_DC_PF_LWM, 56);
	EFX_BAR_WRITEO(enp, FR_AZ_RX_DC_PF_WM_REG, &oword);

	/* Set the event queue to use for SRAM updates */
	EFX_POPULATE_OWORD_1(oword, FRF_AZ_SRM_UPD_EVQ_ID, 0);
	EFX_BAR_WRITEO(enp, FR_AZ_SRM_UPD_EVQ_REG, &oword);

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#if EFSYS_OPT_DIAG

	__checkReturn	int
falcon_sram_test(
	__in		efx_nic_t *enp,
	__in		efx_sram_pattern_fn_t func)
{
	efx_qword_t qword;
	efx_qword_t verify;
	size_t rows = 0x1000;
	unsigned int rptr;
	unsigned int wptr;
	int rc;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	/*
	 * Write the pattern through the SRAM debug aperture. Write
	 * in 64 entry batches, waiting 1us in between each batch
	 * to guarantee not to overflow the SRAM fifo
	 */
	for (wptr = 0, rptr = 0; wptr < rows; ++wptr) {
		func(wptr, B_FALSE, &qword);
		EFX_BAR_TBL_WRITEQ(enp, FR_AZ_SRM_DBG_REG, wptr, &qword);

		if ((wptr - rptr) < 64 && wptr < rows - 1)
			continue;

		EFSYS_SPIN(1);

		for (; rptr <= wptr; ++rptr) {
			func(rptr, B_FALSE, &qword);
			EFX_BAR_TBL_READQ(enp, FR_AZ_SRM_DBG_REG, rptr,
			    &verify);

			if (!EFX_QWORD_IS_EQUAL(verify, qword)) {
				rc = EFAULT;
				goto fail1;
			}
		}
	}

	/* And do the same negated */
	for (wptr = 0, rptr = 0; wptr < rows; ++wptr) {
		func(wptr, B_TRUE, &qword);
		EFX_BAR_TBL_WRITEQ(enp, FR_AZ_SRM_DBG_REG, wptr, &qword);

		if ((wptr - rptr) < 64 && wptr < rows - 1)
			continue;

		EFSYS_SPIN(1);

		for (; rptr <= wptr; ++rptr) {
			func(rptr, B_TRUE, &qword);
			EFX_BAR_TBL_READQ(enp, FR_AZ_SRM_DBG_REG, rptr,
			    &verify);

			if (!EFX_QWORD_IS_EQUAL(verify, qword)) {
				rc = EFAULT;
				goto fail2;
			}
		}
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#endif	/* EFSYS_OPT_DIAG */

		void
falcon_sram_fini(
	__in	efx_nic_t *enp)
{
	efx_oword_t oword;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_EV));
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_RX));
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_TX));

	/* Allow the GPIO1 pin to float */
	EFX_BAR_READO(enp, FR_AB_GPIO_CTL_REG, &oword);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_GPIO1_OEN, 0);
	EFX_BAR_WRITEO(enp, FR_AB_GPIO_CTL_REG, &oword);
}

#endif	/* EFS_OPT_FALCON */
