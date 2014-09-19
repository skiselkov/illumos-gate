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
 * Copyright 2009 Solarflare Communications Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <sys/types.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/stream.h>
#include <sys/dlpi.h>

#include "sfxge.h"

int
sfxge_nvram_rw(sfxge_t *sp, sfxge_nvram_ioc_t *snip, efx_nvram_type_t type,
    boolean_t write)
{
	int (*op)(efx_nic_t *, efx_nvram_type_t, unsigned int, caddr_t, size_t);
	efx_nic_t *enp = sp->s_enp;
	size_t chunk_size;
	off_t off;
	int rc;

	op = (write) ? efx_nvram_write_chunk : efx_nvram_read_chunk;

	if ((rc = efx_nvram_rw_start(enp, type, &chunk_size)) != 0)
		goto fail1;

	off = 0;
	while (snip->sni_size) {
		size_t len = MIN(chunk_size, snip->sni_size);
		caddr_t buf = (caddr_t)(&snip->sni_data[off]);

		if ((rc = op(enp, type, snip->sni_offset + off, buf, len)) != 0)
			goto fail2;

		snip->sni_size -= len;
		off += len;
	}

	efx_nvram_rw_finish(enp, type);
	return (0);

fail2:
	DTRACE_PROBE(fail2);
	efx_nvram_rw_finish(enp, type);
fail1:
	DTRACE_PROBE1(fail1, int, rc);
	return (rc);
}


int
sfxge_nvram_erase(sfxge_t *sp, sfxge_nvram_ioc_t *snip, efx_nvram_type_t type)
{
	efx_nic_t *enp = sp->s_enp;
	size_t chunk_size;
	int rc;

	if ((rc = efx_nvram_rw_start(enp, type, &chunk_size)) != 0)
		goto fail1;

	if ((rc = efx_nvram_erase(enp, type)) != 0)
		goto fail2;

	efx_nvram_rw_finish(enp, type);
	return (0);

fail2:
	DTRACE_PROBE(fail2);
	efx_nvram_rw_finish(enp, type);
fail1:
	DTRACE_PROBE1(fail1, int, rc);
	return (rc);
}

int
sfxge_nvram_ioctl(sfxge_t *sp, sfxge_nvram_ioc_t *snip)
{
	efx_nic_t *enp = sp->s_enp;
	efx_nvram_type_t type;
	int rc;

	if (snip->sni_type == SFXGE_NVRAM_TYPE_MC_GOLDEN &&
	    (snip->sni_op == SFXGE_NVRAM_OP_WRITE ||
	    snip->sni_op == SFXGE_NVRAM_OP_ERASE ||
	    snip->sni_op == SFXGE_NVRAM_OP_SET_VER)) {
		rc = ENOTSUP;
		goto fail4;
	}

	switch (snip->sni_type) {
	case SFXGE_NVRAM_TYPE_BOOTROM:
		type = EFX_NVRAM_BOOTROM;
		break;
	case SFXGE_NVRAM_TYPE_BOOTROM_CFG:
		type = EFX_NVRAM_BOOTROM_CFG;
		break;
	case SFXGE_NVRAM_TYPE_MC:
		type = EFX_NVRAM_MC_FIRMWARE;
		break;
	case SFXGE_NVRAM_TYPE_MC_GOLDEN:
		type = EFX_NVRAM_MC_GOLDEN;
		break;
	case SFXGE_NVRAM_TYPE_PHY:
		type = EFX_NVRAM_PHY;
		break;
	case SFXGE_NVRAM_TYPE_NULL_PHY:
		type = EFX_NVRAM_NULLPHY;
		break;
	case SFXGE_NVRAM_TYPE_FPGA: /* PTP timestamping FPGA */
		type = EFX_NVRAM_FPGA;
		break;
	default:
		rc = EINVAL;
		goto fail1;
	}

	if (snip->sni_size > sizeof (snip->sni_data)) {
		rc = ENOSPC;
		goto fail2;
	}

	switch (snip->sni_op) {
	case SFXGE_NVRAM_OP_SIZE:
	{
		size_t size;
		if ((rc = efx_nvram_size(enp, type, &size)) != 0)
			goto fail3;
		snip->sni_size = size;
		break;
	}
	case SFXGE_NVRAM_OP_READ:
		if ((rc = sfxge_nvram_rw(sp, snip, type, B_FALSE)) != 0)
			goto fail3;
		break;
	case SFXGE_NVRAM_OP_WRITE:
		if ((rc = sfxge_nvram_rw(sp, snip, type, B_TRUE)) != 0)
			goto fail3;
		break;
	case SFXGE_NVRAM_OP_ERASE:
		if ((rc = sfxge_nvram_erase(sp, snip, type)) != 0)
			goto fail3;
		break;
	case SFXGE_NVRAM_OP_GET_VER:
		if ((rc = efx_nvram_get_version(enp, type, &snip->sni_subtype,
		    &snip->sni_version[0])) != 0)
			goto fail3;
		break;
	case SFXGE_NVRAM_OP_SET_VER:
		if ((rc = efx_nvram_set_version(enp, type,
		    &snip->sni_version[0])) != 0)
			goto fail3;
		break;
	default:
		rc = ENOTSUP;
		goto fail4;
	}

	return (0);

fail4:
	DTRACE_PROBE(fail4);
fail3:
	DTRACE_PROBE(fail3);
fail2:
	DTRACE_PROBE(fail2);
fail1:
	DTRACE_PROBE1(fail1, int, rc);

	return (rc);
}
