/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright 2008-2013 Solarflare Communications Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <sys/types.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/stream.h>
#include <sys/dlpi.h>

#include "sfxge.h"

int
sfxge_bar_init(sfxge_t *sp)
{
	efsys_bar_t *esbp = &(sp->s_bar);
	ddi_device_acc_attr_t devacc;
	int rc;

	devacc.devacc_attr_version = DDI_DEVICE_ATTR_V0;
	devacc.devacc_attr_endian_flags = DDI_NEVERSWAP_ACC;
	devacc.devacc_attr_dataorder = DDI_STRICTORDER_ACC;

	if (ddi_regs_map_setup(sp->s_dip, EFX_MEM_BAR, &(esbp->esb_base), 0, 0,
	    &devacc, &(esbp->esb_handle)) != DDI_SUCCESS) {
		rc = ENODEV;
		goto fail1;
	}

	mutex_init(&(esbp->esb_lock), NULL, MUTEX_DRIVER, NULL);

	return (0);

fail1:
	DTRACE_PROBE1(fail1, int, rc);

	return (rc);
}

int
sfxge_bar_ioctl(sfxge_t *sp, sfxge_bar_ioc_t *sbip)
{
	efsys_bar_t *esbp = &(sp->s_bar);
	efx_oword_t oword;
	int rc;

	if (!IS_P2ALIGNED(sbip->sbi_addr, sizeof (efx_oword_t))) {
		rc = EINVAL;
		goto fail1;
	}

	switch (sbip->sbi_op) {
	case SFXGE_BAR_OP_READ:
		EFSYS_BAR_READO(esbp, sbip->sbi_addr, &oword, B_TRUE);

		sbip->sbi_data[0] = EFX_OWORD_FIELD(oword, EFX_DWORD_0);
		sbip->sbi_data[1] = EFX_OWORD_FIELD(oword, EFX_DWORD_1);
		sbip->sbi_data[2] = EFX_OWORD_FIELD(oword, EFX_DWORD_2);
		sbip->sbi_data[3] = EFX_OWORD_FIELD(oword, EFX_DWORD_3);

		break;

	case SFXGE_BAR_OP_WRITE:
		EFX_POPULATE_OWORD_4(oword,
		    EFX_DWORD_0, sbip->sbi_data[0],
		    EFX_DWORD_1, sbip->sbi_data[1],
		    EFX_DWORD_2, sbip->sbi_data[2],
		    EFX_DWORD_3, sbip->sbi_data[3]);

		EFSYS_BAR_WRITEO(esbp, sbip->sbi_addr, &oword, B_TRUE);
		break;

	default:
		rc = ENOTSUP;
		goto fail2;
	}

	return (0);

fail2:
	DTRACE_PROBE(fail2);
fail1:
	DTRACE_PROBE1(fail1, int, rc);

	return (rc);
}

void
sfxge_bar_fini(sfxge_t *sp)
{
	efsys_bar_t *esbp = &(sp->s_bar);

	ddi_regs_map_free(&(esbp->esb_handle));

	mutex_destroy(&(esbp->esb_lock));

	esbp->esb_base = NULL;
	esbp->esb_handle = NULL;
}
