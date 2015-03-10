/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

/*
 * Copyright 2015 Nexenta Inc.  All rights reserved.
 */

#ifndef _SYS_DKIOC_FREE_UTIL_H
#define	_SYS_DKIOC_FREE_UTIL_H

#include <sys/sunddi.h>
#include <sys/dkio.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	_KERNEL

static inline dkioc_free_list_t *
dfl_copyin(void *arg, int ddi_flags, int kmflags)
{
	dkioc_free_list_t *dfl;
	dkioc_free_list_ext_t *dfl_in_exts;

	dfl = kmem_alloc(sizeof (*dfl), kmflags);
	if (dfl == NULL || ddi_copyin((void *)arg, dfl, sizeof (*dfl),
	    ddi_flags)) {
		kmem_free(dfl, sizeof (*dfl));
		return (NULL);
	}

	dfl_in_exts = dfl->dfl_exts;
	dfl->dfl_exts = kmem_alloc(dfl->dfl_num_exts * sizeof (*dfl->dfl_exts),
	    KM_SLEEP);
	if (dfl->dfl_exts == NULL || ddi_copyin(dfl_in_exts, dfl->dfl_exts,
	    dfl->dfl_num_exts * sizeof (*dfl->dfl_exts), ddi_flags)) {
		kmem_free(dfl->dfl_exts, dfl->dfl_num_exts *
		    sizeof (*dfl->dfl_exts));
		kmem_free(dfl, sizeof (*dfl));
		return (NULL);
	}

	return (dfl);
}

#else	/* !_KERNEL */

/*ARGSUSED*/
static inline dkioc_free_list_t *
dfl_copyin(void *arg, int ddi_flags, int kmflags)
{
	return (arg);
}

#endif	/* !_KERNEL */

static inline void
dfl_copyin_destroy(dkioc_free_list_t *dfl)
{
	kmem_free(dfl->dfl_exts, sizeof (*dfl->dfl_exts) * dfl->dfl_num_exts);
	kmem_free(dfl, sizeof (*dfl));
}

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_DKIOC_FREE_UTIL_H */
