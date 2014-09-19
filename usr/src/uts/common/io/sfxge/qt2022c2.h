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

#ifndef	_SYS_QT2022C2_H
#define	_SYS_QT2022C2_H

#include "efx.h"

#ifdef	__cplusplus
extern "C" {
#endif

#if EFSYS_OPT_PHY_QT2022C2

#define	QT2022C2_LOOPBACK_MASK						\
	    ((1 << EFX_LOOPBACK_PHY_XS) |				\
	    (1 << EFX_LOOPBACK_PCS) |					\
	    (1 << EFX_LOOPBACK_PMA_PMD) |				\
	    FALCON_XMAC_LOOPBACK_MASK)

#define	QT2022C2_LED_MASK						\
	    ((1 << EFX_PHY_LED_OFF) |					\
	    (1 << EFX_PHY_LED_ON))

#define	QT2022C2_NPROPS		0

#define	QT2022C2_ADV_CAP_MASK						\
	    ((1 << EFX_PHY_CAP_10000FDX) |				\
	    (1 << EFX_PHY_CAP_PAUSE))

#define	QT2022C2_ADV_CAP_PERM	0

#define	QT2022C2_BIST_MASK	0

extern	__checkReturn	int
qt2022c2_reset(
	__in		efx_nic_t *enp);

extern	__checkReturn	int
qt2022c2_reconfigure(
	__in		efx_nic_t *enp);

extern	__checkReturn	int
qt2022c2_verify(
	__in		efx_nic_t *enp);

extern	__checkReturn	int
qt2022c2_uplink_check(
	__in		efx_nic_t *enp,
	__out		boolean_t *upp);

extern	__checkReturn	int
qt2022c2_downlink_check(
	__in		efx_nic_t *enp,
	__out		efx_link_mode_t *modep,
	__out		unsigned int *fcntlp,
	__out		uint32_t *lp_cap_maskp);

extern	__checkReturn	int
qt2022c2_oui_get(
	__in		efx_nic_t *enp,
	__out		uint32_t *ouip);

#if EFSYS_OPT_PHY_STATS

/* START MKCONFIG GENERATED Qt2022c2PhyHeaderStatsMask 5655dc14f9b46071 */
#define	QT2022C2_STAT_MASK \
	(1ULL << EFX_PHY_STAT_OUI) | \
	(1ULL << EFX_PHY_STAT_PMA_PMD_LINK_UP) | \
	(1ULL << EFX_PHY_STAT_PMA_PMD_RX_FAULT) | \
	(1ULL << EFX_PHY_STAT_PMA_PMD_TX_FAULT) | \
	(1ULL << EFX_PHY_STAT_PCS_LINK_UP) | \
	(1ULL << EFX_PHY_STAT_PCS_RX_FAULT) | \
	(1ULL << EFX_PHY_STAT_PCS_TX_FAULT) | \
	(1ULL << EFX_PHY_STAT_PCS_BER) | \
	(1ULL << EFX_PHY_STAT_PCS_BLOCK_ERRORS) | \
	(1ULL << EFX_PHY_STAT_PHY_XS_LINK_UP) | \
	(1ULL << EFX_PHY_STAT_PHY_XS_RX_FAULT) | \
	(1ULL << EFX_PHY_STAT_PHY_XS_TX_FAULT) | \
	(1ULL << EFX_PHY_STAT_PHY_XS_ALIGN) | \
	(1ULL << EFX_PHY_STAT_PHY_XS_SYNC_A) | \
	(1ULL << EFX_PHY_STAT_PHY_XS_SYNC_B) | \
	(1ULL << EFX_PHY_STAT_PHY_XS_SYNC_C) | \
	(1ULL << EFX_PHY_STAT_PHY_XS_SYNC_D)

/* END MKCONFIG GENERATED Qt2022c2PhyHeaderStatsMask */

extern	__checkReturn			int
qt2022c2_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__out_ecount(EFX_PHY_NSTATS)	uint32_t *stat);

#endif	/* EFSYS_OPT_PHY_STATS */

#if EFSYS_OPT_PHY_PROPS

#if EFSYS_OPT_NAMES

extern		const char __cs *
qt2022c2_prop_name(
	__in	efx_nic_t *enp,
	__in	unsigned int id);

#endif

extern	__checkReturn	int
qt2022c2_prop_get(
	__in		efx_nic_t *enp,
	__in		unsigned int id,
	__in		uint32_t flags,
	__out		uint32_t *valp);

extern	__checkReturn	int
qt2022c2_prop_set(
	__in		efx_nic_t *enp,
	__in		unsigned int id,
	__in		uint32_t val);

#endif	/* EFSYS_OPT_PHY_PROPS */

#endif	/* EFSYS_OPT_PHY_QT2022C2 */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_QT2022C2_H */
