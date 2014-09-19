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

#ifndef	_SYS_SFT9001_H
#define	_SYS_SFT9001_H

#include "efx.h"

#ifdef	__cplusplus
extern "C" {
#endif

#if EFSYS_OPT_PHY_SFT9001

#define	SFT9001_10G_LOOPBACK_MASK					\
	    ((1 << EFX_LOOPBACK_PHY_XS) |				\
	    (1 << EFX_LOOPBACK_PCS) |					\
	    (1 << EFX_LOOPBACK_PMA_PMD) |				\
	    FALCON_XMAC_LOOPBACK_MASK)

#define	SFT9001_1G_LOOPBACK_MASK					\
	    ((1 << EFX_LOOPBACK_GPHY) |					\
	    FALCON_GMAC_LOOPBACK_MASK)

#define	SFT9001_LED_MASK						\
	    ((1 << EFX_PHY_LED_OFF) |					\
	    (1 << EFX_PHY_LED_ON) |					\
	    (1 << EFX_PHY_LED_FLASH))

/* START MKCONFIG GENERATED Sft9001PhyHeaderPropsBlock 9b100228a7cfe533 */
typedef enum sft9001_prop_e {
	SFT9001_SHORT_REACH,
	SFT9001_ROBUST,
	SFT9001_NPROPS
} sft9001_prop_t;

/* END MKCONFIG GENERATED Sft9001PhyHeaderPropsBlock */

#define	SFT9001_ADV_CAP_MASK						\
	    ((1 << EFX_PHY_CAP_AN) |					\
	    (1 << EFX_PHY_CAP_10000FDX) |				\
	    (1 << EFX_PHY_CAP_1000FDX) |				\
	    (1 << EFX_PHY_CAP_100FDX) |					\
	    (1 << EFX_PHY_CAP_PAUSE))

#define	SFT9001_ADV_CAP_PERM						\
	    ((1 << EFX_PHY_CAP_10000FDX) |				\
	    (1 << EFX_PHY_CAP_1000FDX) |				\
	    (1 << EFX_PHY_CAP_100FDX) |					\
	    (1 << EFX_PHY_CAP_100HDX) |					\
	    (1 << EFX_PHY_CAP_PAUSE) |					\
	    (1 << EFX_PHY_CAP_ASYM))

#define	SFT9001_BIST_MASK						\
	    ((1 << EFX_PHY_BIST_TYPE_CABLE_SHORT) |			\
	    (1 << EFX_PHY_BIST_TYPE_CABLE_LONG))

extern	__checkReturn	int
sft9001_reset(
	__in		efx_nic_t *enp);

extern	__checkReturn	int
sft9001_reconfigure(
	__in		efx_nic_t *enp);

extern	__checkReturn	int
sft9001_verify(
	__in		efx_nic_t *enp);

extern	__checkReturn	int
sft9001_uplink_check(
	__in		efx_nic_t *enp,
	__out		boolean_t *upp);

extern	__checkReturn	int
sft9001_downlink_check(
	__in		efx_nic_t *enp,
	__out		efx_link_mode_t *modep,
	__out		unsigned int *fcntlp,
	__out		uint32_t *lp_cap_maskp);

extern __checkReturn	int
sft9001_oui_get(
	__in		efx_nic_t *enp,
	__out		uint32_t *ouip);

#if EFSYS_OPT_PHY_STATS

/* START MKCONFIG GENERATED Sft9001PhyHeaderStatsMask 06818b95754126e3 */
#define	SFT9001_STAT_MASK \
	(1ULL << EFX_PHY_STAT_OUI) | \
	(1ULL << EFX_PHY_STAT_PMA_PMD_LINK_UP) | \
	(1ULL << EFX_PHY_STAT_PMA_PMD_RX_FAULT) | \
	(1ULL << EFX_PHY_STAT_PMA_PMD_TX_FAULT) | \
	(1ULL << EFX_PHY_STAT_PMA_PMD_REV_A) | \
	(1ULL << EFX_PHY_STAT_PMA_PMD_REV_B) | \
	(1ULL << EFX_PHY_STAT_PMA_PMD_REV_C) | \
	(1ULL << EFX_PHY_STAT_PMA_PMD_REV_D) | \
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
	(1ULL << EFX_PHY_STAT_CL22EXT_LINK_UP) | \
	(1ULL << EFX_PHY_STAT_SNR_A) | \
	(1ULL << EFX_PHY_STAT_SNR_B) | \
	(1ULL << EFX_PHY_STAT_SNR_C) | \
	(1ULL << EFX_PHY_STAT_SNR_D)

/* END MKCONFIG GENERATED Sft9001PhyHeaderStatsMask */

extern	__checkReturn			int
sft9001_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__out_ecount(EFX_PHY_NSTATS)	uint32_t *stat);

#endif	/* EFSYS_OPT_PHY_STATS */

#if EFSYS_OPT_PHY_PROPS

#if EFSYS_OPT_NAMES

extern		const char __cs *
sft9001_prop_name(
	__in	efx_nic_t *enp,
	__in	unsigned int id);

#endif	/* EFSYS_OPT_NAMES */

extern	__checkReturn	int
sft9001_prop_get(
	__in		efx_nic_t *enp,
	__in		unsigned int id,
	__in		uint32_t flags,
	__out		uint32_t *valp);

extern	__checkReturn	int
sft9001_prop_set(
	__in		efx_nic_t *enp,
	__in		unsigned int id,
	__in		uint32_t val);

#endif	/* EFSYS_OPT_PHY_PROPS */

#if EFSYS_OPT_NVRAM_SFT9001

extern	__checkReturn		int
sft9001_nvram_size(
	__in			efx_nic_t *enp,
	__out			size_t *sizep);

extern	__checkReturn		int
sft9001_nvram_get_version(
	__in			efx_nic_t *enp,
	__out			uint32_t *subtypep,
	__out_ecount(4)		uint16_t version[4]);

extern	__checkReturn		int
sft9001_nvram_rw_start(
	__in			efx_nic_t *enp,
	__out			size_t *block_sizep);

extern	__checkReturn		int
sft9001_nvram_read_chunk(
	__in			efx_nic_t *enp,
	__in			unsigned int offset,
	__out_bcount(size)	caddr_t data,
	__in			size_t size);

extern	__checkReturn		int
sft9001_nvram_erase(
	__in			efx_nic_t *enp);

extern	__checkReturn		int
sft9001_nvram_write_chunk(
	__in			efx_nic_t *enp,
	__in			unsigned int offset,
	__in_bcount(size)	caddr_t data,
	__in			size_t size);

extern				void
sft9001_nvram_rw_finish(
	__in			efx_nic_t *enp);

extern const uint8_t * const sft9001_loader;
extern const size_t sft9001_loader_size;

#pragma pack(1)

typedef struct sft9001_firmware_header_s {
	efx_dword_t code_length;
	efx_dword_t destination_address;
	efx_word_t code_checksum;
	efx_word_t boot_config;
	efx_word_t header_csum;
	efx_byte_t reserved[18];
} sft9001_firmware_header_t;

#pragma pack()

#endif	/* EFSYS_OPT_NVRAM_SFT9001 */

#if EFSYS_OPT_PHY_BIST

extern	__checkReturn		int
sft9001_bist_start(
	__in			efx_nic_t *enp,
	__in			efx_phy_bist_type_t type);

extern	__checkReturn		int
sft9001_bist_poll(
	__in			efx_nic_t *enp,
	__in			efx_phy_bist_type_t type,
	__out			efx_phy_bist_result_t *resultp,
	__out_opt		uint32_t *value_maskp,
	__out_ecount_opt(count)	unsigned long *valuesp,
	__in			size_t count);

extern				void
sft9001_bist_stop(
	__in			efx_nic_t *enp,
	__in			efx_phy_bist_type_t type);

#endif	/* EFSYS_OPT_PHY_BIST */

#endif	/* EFSYS_OPT_SFT9001 */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SFT9001_H */
