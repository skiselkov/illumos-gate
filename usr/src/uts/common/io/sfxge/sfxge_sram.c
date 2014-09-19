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

void
sfxge_sram_init(sfxge_t *sp)
{
	sfxge_sram_t *ssp = &(sp->s_sram);
	dev_info_t *dip = sp->s_dip;
	char name[MAXNAMELEN];

	ASSERT3U(ssp->ss_state, ==, SFXGE_SRAM_UNINITIALIZED);

	mutex_init(&(ssp->ss_lock), NULL, MUTEX_DRIVER, NULL);

	/*
	 * Create a VMEM arena for the buffer table
	 * Note that these are not in the DDI/DKI.
	 * See "man rmalloc" for a possible alternative
	 */
	(void) snprintf(name, MAXNAMELEN - 1, "%s%d_sram", ddi_driver_name(dip),
	    ddi_get_instance(dip));
	ssp->ss_buf_tbl = vmem_create(name, (void *)1, EFX_BUF_TBL_SIZE, 1,
	    NULL, NULL, NULL, 1, VM_SLEEP | VMC_IDENTIFIER);

	ssp->ss_state = SFXGE_SRAM_INITIALIZED;
}

int
sfxge_sram_buf_tbl_alloc(sfxge_t *sp, size_t n, uint32_t *idp)
{
	sfxge_sram_t *ssp = &(sp->s_sram);
	void *base;
	int rc;

	mutex_enter(&(ssp->ss_lock));

	ASSERT(ssp->ss_state != SFXGE_SRAM_UNINITIALIZED);

	if ((base = vmem_alloc(ssp->ss_buf_tbl, n, VM_NOSLEEP)) == NULL) {
		rc = ENOSPC;
		goto fail1;
	}

	*idp = (uint32_t)((uintptr_t)base - 1);

	mutex_exit(&(ssp->ss_lock));

	return (0);

fail1:
	DTRACE_PROBE1(fail1, int, rc);

	mutex_exit(&(ssp->ss_lock));

	return (rc);
}

int
sfxge_sram_start(sfxge_t *sp)
{
	sfxge_sram_t *ssp = &(sp->s_sram);

	mutex_enter(&(ssp->ss_lock));

	ASSERT3U(ssp->ss_state, ==, SFXGE_SRAM_INITIALIZED);
	ASSERT3U(ssp->ss_count, ==, 0);

	ssp->ss_state = SFXGE_SRAM_STARTED;

	mutex_exit(&(ssp->ss_lock));

	return (0);
}

int
sfxge_sram_buf_tbl_set(sfxge_t *sp, uint32_t id, efsys_mem_t *esmp,
    size_t n)
{
	sfxge_sram_t *ssp = &(sp->s_sram);
	int rc;

	mutex_enter(&(ssp->ss_lock));

	ASSERT3U(ssp->ss_state, ==, SFXGE_SRAM_STARTED);

	if ((rc = efx_sram_buf_tbl_set(sp->s_enp, id, esmp, n)) != 0)
		goto fail1;

	ssp->ss_count += n;

	mutex_exit(&(ssp->ss_lock));

	return (0);

fail1:
	DTRACE_PROBE1(fail1, int, rc);

	mutex_exit(&(ssp->ss_lock));

	return (rc);
}

void
sfxge_sram_buf_tbl_clear(sfxge_t *sp, uint32_t id, size_t n)
{
	sfxge_sram_t *ssp = &(sp->s_sram);

	mutex_enter(&(ssp->ss_lock));

	ASSERT3U(ssp->ss_state, ==, SFXGE_SRAM_STARTED);

	ASSERT3U(ssp->ss_count, >=, n);
	ssp->ss_count -= n;

	efx_sram_buf_tbl_clear(sp->s_enp, id, n);

	mutex_exit(&(ssp->ss_lock));
}

void
sfxge_sram_stop(sfxge_t *sp)
{
	sfxge_sram_t *ssp = &(sp->s_sram);

	mutex_enter(&(ssp->ss_lock));

	ASSERT3U(ssp->ss_state, ==, SFXGE_SRAM_STARTED);
	ASSERT3U(ssp->ss_count, ==, 0);

	ssp->ss_state = SFXGE_SRAM_INITIALIZED;

	mutex_exit(&(ssp->ss_lock));
}

void
sfxge_sram_buf_tbl_free(sfxge_t *sp, uint32_t id, size_t n)
{
	sfxge_sram_t *ssp = &(sp->s_sram);
	void *base;

	mutex_enter(&(ssp->ss_lock));

	ASSERT(ssp->ss_state != SFXGE_SRAM_UNINITIALIZED);

	base = (void *)((uintptr_t)id + 1);
	vmem_free(ssp->ss_buf_tbl, base, n);

	mutex_exit(&(ssp->ss_lock));
}

static int
sfxge_sram_test(sfxge_t *sp, efx_pattern_type_t type)
{
	sfxge_sram_t *ssp = &(sp->s_sram);
	int rc;

	if (ssp->ss_state != SFXGE_SRAM_INITIALIZED) {
		rc = EFAULT;
		goto fail1;
	}

	if (type >= EFX_PATTERN_NTYPES) {
		rc = EINVAL;
		goto fail2;
	}

	mutex_enter(&(ssp->ss_lock));

	if ((rc = efx_sram_test(sp->s_enp, type)) != 0)
		goto fail3;

	mutex_exit(&(ssp->ss_lock));

	return (0);

fail3:
	DTRACE_PROBE(fail3);
	mutex_exit(&(ssp->ss_lock));
fail2:
	DTRACE_PROBE(fail2);
fail1:
	DTRACE_PROBE1(fail1, int, rc);

	return (rc);
}

int
sfxge_sram_ioctl(sfxge_t *sp, sfxge_sram_ioc_t *ssip)
{
	int rc;

	switch (ssip->ssi_op) {
	case SFXGE_SRAM_OP_TEST: {
		efx_pattern_type_t type = ssip->ssi_data;

		if ((rc = sfxge_sram_test(sp, type)) != 0)
			goto fail1;

		break;
	}
	default:
		rc = ENOTSUP;
		goto fail1;
	}

	return (0);

fail1:
	DTRACE_PROBE1(fail1, int, rc);

	return (rc);
}

void
sfxge_sram_fini(sfxge_t *sp)
{
	sfxge_sram_t *ssp = &(sp->s_sram);

	ASSERT3U(ssp->ss_state, ==, SFXGE_SRAM_INITIALIZED);

	/* Destroy the VMEM arena */
	vmem_destroy(ssp->ss_buf_tbl);
	ssp->ss_buf_tbl = NULL;

	mutex_destroy(&(ssp->ss_lock));

	ssp->ss_state = SFXGE_SRAM_UNINITIALIZED;
}
