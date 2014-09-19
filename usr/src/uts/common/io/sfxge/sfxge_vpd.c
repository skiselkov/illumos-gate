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
#include <sys/kmem.h>
#include "sfxge.h"


int
sfxge_vpd_get_keyword(sfxge_t *sp, sfxge_vpd_ioc_t *svip)
{
	efx_nic_t *enp = sp->s_enp;
	efx_vpd_value_t vpd;
	size_t size;
	void *buf;
	int rc;

	if ((rc = efx_vpd_size(enp, &size)) != 0)
		goto fail1;

	buf = kmem_zalloc(size, KM_SLEEP);
	ASSERT(buf);

	if ((rc = efx_vpd_read(enp, buf, size)) != 0)
		goto fail2;

	if ((rc = efx_vpd_verify(enp, buf, size)) != 0)
		goto fail3;

	vpd.evv_tag = svip->svi_tag;
	vpd.evv_keyword = svip->svi_keyword;

	if ((rc = efx_vpd_get(enp, buf, size, &vpd)) != 0)
		goto fail4;

	svip->svi_len = vpd.evv_length;
	EFX_STATIC_ASSERT(sizeof (svip->svi_payload) == sizeof (vpd.evv_value));
	memcpy(svip->svi_payload, &vpd.evv_value[0],
	    sizeof (svip->svi_payload));

	kmem_free(buf, size);

	return (0);

fail4:
	DTRACE_PROBE(fail4);
fail3:
	DTRACE_PROBE(fail3);
fail2:
	DTRACE_PROBE(fail2);
	kmem_free(buf, size);
fail1:
	DTRACE_PROBE1(fail1, int, rc);

	return (rc);
}


int
sfxge_vpd_set_keyword(sfxge_t *sp, sfxge_vpd_ioc_t *svip)
{
	efx_nic_t *enp = sp->s_enp;
	efx_vpd_value_t vpd;
	size_t size;
	void *buf;
	int rc;

	/* restriction on writable tags is in efx_vpd_hunk_set() */

	if ((rc = efx_vpd_size(enp, &size)) != 0)
		goto fail1;

	buf = kmem_zalloc(size, KM_SLEEP);
	ASSERT(buf);

	if ((rc = efx_vpd_read(enp, buf, size)) != 0)
		goto fail2;

	if ((rc = efx_vpd_verify(enp, buf, size)) != 0) {
		if ((rc = efx_vpd_reinit(enp, buf, size)) != 0)
			goto fail3;
		if ((rc = efx_vpd_verify(enp, buf, size)) != 0)
			goto fail4;
	}

	vpd.evv_tag = svip->svi_tag;
	vpd.evv_keyword = svip->svi_keyword;
	vpd.evv_length = svip->svi_len;

	EFX_STATIC_ASSERT(sizeof (svip->svi_payload) == sizeof (vpd.evv_value));
	memcpy(&vpd.evv_value[0], svip->svi_payload,
	    sizeof (svip->svi_payload));

	if ((rc = efx_vpd_set(enp, buf, size, &vpd)) != 0)
		goto fail5;

	if ((rc = efx_vpd_verify(enp, buf, size)) != 0)
			goto fail6;

	/* And write the VPD back to the hardware */
	if ((rc = efx_vpd_write(enp, buf, size)) != 0)
		goto fail7;

	kmem_free(buf, size);

	return (0);

fail7:
	DTRACE_PROBE(fail7);
fail6:
	DTRACE_PROBE(fail6);
fail5:
	DTRACE_PROBE(fail5);
fail4:
	DTRACE_PROBE(fail4);
fail3:
	DTRACE_PROBE(fail3);
fail2:
	DTRACE_PROBE(fail2);
	kmem_free(buf, size);
fail1:
	DTRACE_PROBE1(fail1, int, rc);

	return (rc);
}


int
sfxge_vpd_ioctl(sfxge_t *sp, sfxge_vpd_ioc_t *svip)
{
	int rc;

	switch (svip->svi_op) {
	case SFXGE_VPD_OP_GET_KEYWORD:
		if ((rc = sfxge_vpd_get_keyword(sp, svip)) != 0)
			goto fail1;
		break;
	case SFXGE_VPD_OP_SET_KEYWORD:
		if ((rc = sfxge_vpd_set_keyword(sp, svip)) != 0)
			goto fail1;
		break;
	default:
		rc = EINVAL;
		goto fail2;
	}

	return (0);

fail2:
	DTRACE_PROBE(fail2);
fail1:
	DTRACE_PROBE1(fail1, int, rc);

	return (rc);
}
