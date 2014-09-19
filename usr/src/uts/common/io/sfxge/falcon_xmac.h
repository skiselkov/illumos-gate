/*-
 * Copyright 2008-2013 Solarflare Communications Inc.  All rights reserved.
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

#ifndef	_SYS_FALCON_XMAC_H
#define	_SYS_FALCON_XMAC_H

#include "efx.h"

#ifdef	__cplusplus
extern "C" {
#endif

#if EFSYS_OPT_MAC_FALCON_XMAC

#if EFSYS_OPT_LOOPBACK

#define	FALCON_XMAC_LOOPBACK_MASK					\
	((1 << EFX_LOOPBACK_XGMII) |					\
	(1 << EFX_LOOPBACK_XGXS) |					\
	(1 << EFX_LOOPBACK_XAUI))

#endif	/* EFSYS_OPT_LOOPBACK */

#define	XMAC_INTR_SUPPORTED	B_TRUE

extern	__checkReturn	int
falcon_xmac_reset(
	__in		efx_nic_t *enp);

extern	__checkReturn	int
falcon_xmac_reconfigure(
	__in	efx_nic_t *enp);

#if EFSYS_OPT_MAC_STATS

extern	__checkReturn			int
falcon_xmac_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__in_ecount(EFX_MAC_NSTATS)	efsys_stat_t *essp,
	__out_opt			uint32_t *generationp);

#endif

#endif	/* EFSYS_OPT_MAC_XMAC */

extern			void
falcon_xmac_poll(
	__in		efx_nic_t *enp,
	__out		boolean_t *mac_upp);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FALCON_XMAC_H */
