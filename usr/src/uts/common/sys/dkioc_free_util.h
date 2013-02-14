/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensource.org/licenses/CDDL-1.0.
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
 * Copyright 2014 Saso Kiselkov.  All rights reserved.
 */

#ifndef _SYS_DKIOC_FREE_UTIL_H
#define	_SYS_DKIOC_FREE_UTIL_H

#include <sys/dkio.h>

#ifdef	__cplusplus
extern "C" {
#endif

static dkioc_free_list_t *
dfl_new(int kmflags, int dkioc_free_flags)
{
	dkioc_free_list_t *dfl = kmem_zalloc(sizeof (*dfl), kmflags);
	dfl->dfl_flags = dkioc_free_flags;
	return (dfl);
}

static void
dfl_destroy(dkioc_free_list_t *dfl)
{
	kmem_free(dfl, sizeof (*dfl));
}

static void
dfl_append(dkioc_free_list_t *dfl, diskaddr_t start, diskaddr_t length)
{
	VERIFY3U(dfl->dfl_num_exts, <, DFL_MAX_EXTENTS);
	ASSERT3U(length, <=, UINT32_MAX * DEV_BSIZE);

	dfl->dfl_exts[dfl->dfl_num_exts].ext_start = start;
	dfl->dfl_exts[dfl->dfl_num_exts].ext_length = length;
	dfl->dfl_num_exts++;
}

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_DKIOC_FREE_UTIL_H */
