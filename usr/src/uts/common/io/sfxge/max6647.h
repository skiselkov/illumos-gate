/*-
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

#ifndef	_SYS_MAX6647_H
#define	_SYS_MAX6647_H

#include "efx.h"

#ifdef	__cplusplus
extern "C" {
#endif

#if EFSYS_OPT_MON_MAX6647

#define	MAX6646_DEVID 0x4d
#define	MAX6647_DEVID 0x4e

extern	__checkReturn	int
max6647_reset(
	__in		efx_nic_t *enp);

extern	__checkReturn	int
max6647_reconfigure(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_MON_STATS

/* START MKCONFIG GENERATED Max6647MonitorHeaderStatsMask b4d91f25c4d293e1 */
#define	MAX6647_STAT_MASK \
	(1ULL << EFX_MON_STAT_INT_TEMP) | \
	(1ULL << EFX_MON_STAT_EXT_TEMP)

/* END MKCONFIG GENERATED Max6647MonitorHeaderStatsMask */

extern	__checkReturn			int
max6647_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__out_ecount(EFX_MON_NSTATS)	efx_mon_stat_value_t *values);

#endif	/* EFSYS_OPT_MON_STATS */

#endif	/* EFSYS_OPT_MON_MAX6647 */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MAX6647_H */
