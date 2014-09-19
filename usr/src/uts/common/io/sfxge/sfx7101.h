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

#ifndef	_SYS_SFX7101_H
#define	_SYS_SFX7101_H

#include "efx.h"

#ifdef	__cplusplus
extern "C" {
#endif

#if EFSYS_OPT_PHY_SFX7101

#define	SFX7101_LOOPBACK_MASK						\
	    ((1 << EFX_LOOPBACK_PHY_XS) |				\
	    (1 << EFX_LOOPBACK_PCS) |					\
	    (1 << EFX_LOOPBACK_PMA_PMD) |				\
	    FALCON_XMAC_LOOPBACK_MASK)

#define	SFX7101_LED_MASK						\
	    ((1 << EFX_PHY_LED_OFF) |					\
	    (1 << EFX_PHY_LED_ON) |					\
	    (1 << EFX_PHY_LED_FLASH))

#define	SFX7101_NPROPS		0

#define	SFX7101_ADV_CAP_MASK						\
	    ((1 << EFX_PHY_CAP_AN) |					\
	    (1 << EFX_PHY_CAP_10000FDX) |				\
	    (1 << EFX_PHY_CAP_ASYM) |					\
	    (1 << EFX_PHY_CAP_PAUSE))

#define	SFX7101_ADV_CAP_PERM	0

#define	SFX7101_BIST_MASK	0

extern	__checkReturn	int
sfx7101_power(
	__in		efx_nic_t *enp,
	__in		boolean_t on);

extern	__checkReturn	int
sfx7101_reset(
	__in		efx_nic_t *enp);

extern	__checkReturn	int
sfx7101_reconfigure(
	__in		efx_nic_t *enp);

extern	__checkReturn	int
sfx7101_verify(
	__in		efx_nic_t *enp);

extern	__checkReturn	int
sfx7101_uplink_check(
	__in		efx_nic_t *enp,
	__out		boolean_t *upp);

extern		void
sfx7101_uplink_reset(
	__in	efx_nic_t *enp);

extern	__checkReturn	int
sfx7101_downlink_check(
	__in		efx_nic_t *enp,
	__out		efx_link_mode_t *modep,
	__out		unsigned int *fcntlp,
	__out		uint32_t *lp_cap_maskp);

extern	__checkReturn	int
sfx7101_oui_get(
	__in		efx_nic_t *enp,
	__out		uint32_t *ouip);

#if EFSYS_OPT_PHY_STATS

/* START MKCONFIG GENERATED Sfx7101PhyHeaderStatsMask edaf3cd6dfd8b815 */
#define	SFX7101_STAT_MASK \
	(1ULL << EFX_PHY_STAT_OUI) | \
	(1ULL << EFX_PHY_STAT_PMA_PMD_LINK_UP) | \
	(1ULL << EFX_PHY_STAT_PMA_PMD_RX_FAULT) | \
	(1ULL << EFX_PHY_STAT_PMA_PMD_TX_FAULT) | \
	(1ULL << EFX_PHY_STAT_PMA_PMD_REV_MAJOR) | \
	(1ULL << EFX_PHY_STAT_PMA_PMD_REV_MINOR) | \
	(1ULL << EFX_PHY_STAT_PMA_PMD_REV_MICRO) | \
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
	(1ULL << EFX_PHY_STAT_PHY_XS_SYNC_D) | \
	(1ULL << EFX_PHY_STAT_AN_LINK_UP) | \
	(1ULL << EFX_PHY_STAT_AN_MASTER) | \
	(1ULL << EFX_PHY_STAT_AN_LOCAL_RX_OK) | \
	(1ULL << EFX_PHY_STAT_AN_REMOTE_RX_OK) | \
	(1ULL << EFX_PHY_STAT_SNR_A) | \
	(1ULL << EFX_PHY_STAT_SNR_B) | \
	(1ULL << EFX_PHY_STAT_SNR_C) | \
	(1ULL << EFX_PHY_STAT_SNR_D)

/* END MKCONFIG GENERATED Sfx7101PhyHeaderStatsMask */

extern	__checkReturn			int
sfx7101_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__out_ecount(EFX_PHY_NSTATS)	uint32_t *stat);

#endif	/* EFSYS_OPT_PHY_STATS */

#if EFSYS_OPT_PHY_PROPS

#if EFSYS_OPT_NAMES

extern		const char __cs *
sfx7101_prop_name(
	__in	efx_nic_t *enp,
	__in	unsigned int id);

#endif

extern	__checkReturn	int
sfx7101_prop_get(
	__in		efx_nic_t *enp,
	__in		unsigned int id,
	__in		uint32_t flags,
	__out		uint32_t *valp);

extern	__checkReturn	int
sfx7101_prop_set(
	__in		efx_nic_t *enp,
	__in		unsigned int id,
	__in		uint32_t val);

#endif	/* EFSYS_OPT_PHY_PROPS */

#if EFSYS_OPT_NVRAM_SFX7101

extern	__checkReturn		int
sfx7101_nvram_size(
	__in			efx_nic_t *enp,
	__out			size_t *sizep);

extern	__checkReturn		int
sfx7101_nvram_get_version(
	__in			efx_nic_t *enp,
	__out			uint32_t *subtypep,
	__out_ecount(4)		uint16_t version[4]);

extern	__checkReturn		int
sfx7101_nvram_rw_start(
	__in			efx_nic_t *enp,
	__out			size_t *block_sizep);

extern	__checkReturn		int
sfx7101_nvram_read_chunk(
	__in			efx_nic_t *enp,
	__in			unsigned int offset,
	__out_bcount(size)	caddr_t data,
	__in			size_t size);

extern	__checkReturn		int
sfx7101_nvram_erase(
	__in			efx_nic_t *enp);

extern	__checkReturn		int
sfx7101_nvram_write_chunk(
	__in			efx_nic_t *enp,
	__in			unsigned int offset,
	__in_bcount(size)	caddr_t data,
	__in			size_t size);

extern				void
sfx7101_nvram_rw_finish(
	__in			efx_nic_t *enp);

extern const uint8_t * const sfx7101_loader;
extern const size_t sfx7101_loader_size;

#pragma pack(1)

typedef struct sfx7001_firmware_header_s {
	efx_dword_t code_length;
	efx_dword_t destination_address;
	efx_word_t code_checksum;
	efx_word_t boot_config;
	efx_word_t header_csum;
	efx_byte_t reserved[18];
} sfx7101_firmware_header_t;

#pragma pack()

#endif	/* EFSYS_OPT_NVRAM_SFX7101 */

#endif	/* EFSYS_OPT_PHY_SFX7101 */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SFX7101_H */
