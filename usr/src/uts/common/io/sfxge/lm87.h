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

#ifndef	_SYS_LM87_H
#define	_SYS_LM87_H

#include "efx.h"

#ifdef	__cplusplus
extern "C" {
#endif

#if EFSYS_OPT_MON_LM87

#define	LM87_DEVID 0x2e

extern	__checkReturn	int
lm87_reset(
	__in		efx_nic_t *enp);

extern	__checkReturn	int
lm87_reconfigure(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_MON_STATS

/* START MKCONFIG GENERATED Lm87MonitorHeaderStatsMask ebc33ca917ba24d7 */
#define	LM87_STAT_MASK \
	(1ULL << EFX_MON_STAT_2_5V) | \
	(1ULL << EFX_MON_STAT_VCCP1) | \
	(1ULL << EFX_MON_STAT_VCC) | \
	(1ULL << EFX_MON_STAT_5V) | \
	(1ULL << EFX_MON_STAT_12V) | \
	(1ULL << EFX_MON_STAT_VCCP2) | \
	(1ULL << EFX_MON_STAT_EXT_TEMP) | \
	(1ULL << EFX_MON_STAT_INT_TEMP) | \
	(1ULL << EFX_MON_STAT_AIN1) | \
	(1ULL << EFX_MON_STAT_AIN2)

/* END MKCONFIG GENERATED Lm87MonitorHeaderStatsMask */

extern	__checkReturn			int
lm87_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__out_ecount(EFX_MON_NSTATS)	efx_mon_stat_value_t *values);

#endif	/* EFSYS_OPT_MON_STATS */

#endif	/* EFSYS_OPT_MON_LM87 */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_LM87_H */
