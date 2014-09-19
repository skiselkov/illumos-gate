/*
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

#include "efsys.h"
#include "efx.h"
#include "falcon_nvram.h"
#include "efx_types.h"
#include "efx_regs.h"
#include "efx_regs_pci.h"
#include "efx_impl.h"

#if EFSYS_OPT_FALCON

#if EFSYS_OPT_MAC_FALCON_XMAC
#include "falcon_xmac.h"
#endif

#if EFSYS_OPT_MAC_FALCON_GMAC
#include "falcon_gmac.h"
#endif

#if EFSYS_OPT_MON_LM87
#include "lm87.h"
#endif

#if EFSYS_OPT_MON_MAX6647
#include "max6647.h"
#endif

#if EFSYS_OPT_PHY_NULL
#include "nullphy.h"
#endif

#if EFSYS_OPT_PHY_QT2022C2
#include "qt2022c2.h"
#endif

#if EFSYS_OPT_PHY_SFX7101
#include "sfx7101.h"
#endif

#if EFSYS_OPT_PHY_TXC43128
#include "txc43128.h"
#endif

#if EFSYS_OPT_PHY_SFT9001
#include "sft9001.h"
#endif

#if EFSYS_OPT_PHY_QT2025C
#include "qt2025c.h"
#endif

static	__checkReturn		int
falcon_nic_cfg_raw_read(
	__in			efx_nic_t *enp,
	__in			uint32_t offset,
	__in			uint32_t size,
	__out			void *cfg)
{
	EFSYS_ASSERT3U(offset + size, <=, FALCON_NIC_CFG_RAW_SZ);

#if EFSYS_OPT_FALCON_NIC_CFG_OVERRIDE
	if (enp->en_u.falcon.enu_forced_cfg != NULL) {
		memcpy(cfg, enp->en_u.falcon.enu_forced_cfg + offset, size);
		return (0);
	}
#endif	/* EFSYS_OPT_FALCON_NIC_CFG_OVERRIDE */

	return falcon_spi_dev_read(enp, FALCON_SPI_FLASH, offset,
		    (caddr_t)cfg, size);
}

	__checkReturn		int
falcon_nic_cfg_raw_read_verify(
	__in			efx_nic_t *enp,
	__in			uint32_t offset,
	__in			uint32_t size,
	__out			uint8_t *cfg)
{
	uint32_t csum_offset;
	uint16_t magic, version, cksum;
	int rc;
	efx_word_t word;

	if ((rc = falcon_nic_cfg_raw_read(enp, CFG_MAGIC_REG_SF_OFST,
		    sizeof (word), &word)) != 0)
		goto fail1;

	magic = EFX_WORD_FIELD(word, MAGIC);

	if ((rc = falcon_nic_cfg_raw_read(enp, CFG_VERSION_REG_SF_OFST,
		    sizeof (word), &word)) != 0)
		goto fail2;

	version = EFX_WORD_FIELD(word, VERSION);

	cksum = 0;
	for (csum_offset = (version < 4) ? CFG_MAGIC_REG_SF_OFST : 0;
	    csum_offset < FALCON_NIC_CFG_RAW_SZ;
	    csum_offset += sizeof (efx_word_t)) {

		if ((rc = falcon_nic_cfg_raw_read(enp, csum_offset,
			    sizeof (word), &word)) != 0)
			goto fail3;

		cksum += EFX_WORD_FIELD(word, EFX_WORD_0);
	}
	if (magic != MAGIC_DECODE || version < 2 || cksum != 0xffff) {
		rc = EINVAL;
		goto fail4;
	}

	if ((rc = falcon_nic_cfg_raw_read(enp, offset, size, cfg)) != 0)
		goto fail5;

	return (0);

fail5:
	EFSYS_PROBE(fail5);
fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn	int
falcon_nic_probe(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	int rc;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	/* Initialise the nvram */
	if ((rc = falcon_nvram_init(enp)) != 0)
		goto fail1;

	/* Probe the board configuration */
	if ((rc = falcon_nic_cfg_build(enp, encp)) != 0)
		goto fail2;
	epp->ep_adv_cap_mask = epp->ep_default_adv_cap_mask;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#define	FALCON_NIC_CFG_BUILD_LOWEST_REG				\
	MIN(CFG_BOARD_REV_REG_SF_OFST,				\
	    MIN(CFG_BOARD_TYPE_REG_SF_OFST,			\
		MIN(CFG_PHY_PORT_REG_SF_OFST,			\
		    MIN(CFG_PHY_TYPE_REG_SF_OFST,		\
			MIN(MAC_ADDRESS_SF_OFST,		\
			    MIN(NIC_STAT_SF_OFST,		\
				SRAM_CFG_SF_OFST))))))


#define	FALCON_NIC_CFG_BUILD_HIGHEST_REG			\
	MAX(CFG_BOARD_REV_REG_SF_OFST + 1,			\
	    MAX(CFG_BOARD_TYPE_REG_SF_OFST + 1,			\
		MAX(CFG_PHY_PORT_REG_SF_OFST + 1,		\
		    MAX(CFG_PHY_TYPE_REG_SF_OFST + 1,		\
			MAX(MAC_ADDRESS_SF_OFST + 6,		\
			    MAX(NIC_STAT_SF_OFST + 16,		\
				SRAM_CFG_SF_OFST + 16))))))

#define	FALCON_NIC_CFG_BUILD_NEEDED_CFG_SIZE		\
	(FALCON_NIC_CFG_BUILD_HIGHEST_REG -		\
	    FALCON_NIC_CFG_BUILD_LOWEST_REG)

	__checkReturn	int
falcon_nic_cfg_build(
	__in		efx_nic_t *enp,
	__out		efx_nic_cfg_t *encp)
{
	efx_port_t *epp = &(enp->en_port);
	uint8_t cfg[FALCON_NIC_CFG_BUILD_NEEDED_CFG_SIZE];
	uint8_t *origin = cfg - FALCON_NIC_CFG_BUILD_LOWEST_REG;
	efx_oword_t *owordp;
	uint8_t major;
	uint8_t minor;
	int rc;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	(void) memset(encp, 0, sizeof (efx_nic_cfg_t));

	if ((rc = falcon_nic_cfg_raw_read_verify(enp,
	    FALCON_NIC_CFG_BUILD_LOWEST_REG,
	    FALCON_NIC_CFG_BUILD_NEEDED_CFG_SIZE, cfg)) != 0)
		goto fail1;

	encp->enc_board_type = EFX_BYTE_FIELD(
		((efx_byte_t *)origin)[CFG_BOARD_TYPE_REG_SF_OFST],
		EFX_BYTE_0);

	/* Read board revision */
	major = EFX_BYTE_FIELD(
	    ((efx_byte_t *)origin)[CFG_BOARD_REV_REG_SF_OFST],
	    BOARD_REV_MAJOR);
	minor = EFX_BYTE_FIELD(
	    ((efx_byte_t *)origin)[CFG_BOARD_REV_REG_SF_OFST],
	    BOARD_REV_MINOR);
	enp->en_u.falcon.enu_board_rev = (major << 4) | minor;

	/* Sram mode */
	owordp = (efx_oword_t *)(origin + NIC_STAT_SF_OFST);
	enp->en_u.falcon.enu_internal_sram =
	    EFX_OWORD_FIELD(*owordp, FRF_AB_ONCHIP_SRAM) != 0;
	if (enp->en_u.falcon.enu_internal_sram) {
		enp->en_u.falcon.enu_sram_num_bank = 0;
		enp->en_u.falcon.enu_sram_bank_size = 0;

		/* Resource limits */
		encp->enc_evq_limit = 64;	/* Interrupt-capable */
		encp->enc_txq_limit = 512;
		encp->enc_rxq_limit = 384;
		encp->enc_buftbl_limit = 4096;
	} else {
		uint32_t sram_rows;

		owordp = (efx_oword_t *)(origin + SRAM_CFG_SF_OFST);
		enp->en_u.falcon.enu_sram_num_bank =
		    (uint8_t)EFX_OWORD_FIELD(*owordp, FRF_AZ_SRM_NUM_BANK);
		enp->en_u.falcon.enu_sram_bank_size =
		    (uint8_t)EFX_OWORD_FIELD(*owordp, FRF_AZ_SRM_BANK_SIZE);
		sram_rows = (enp->en_u.falcon.enu_sram_num_bank + 1)
		    << (18 + enp->en_u.falcon.enu_sram_bank_size);

		/* Resource limits */
		encp->enc_evq_limit = 64;	/* Interrupt-capable */
		encp->enc_txq_limit = EFX_TXQ_LIMIT_TARGET;
		encp->enc_rxq_limit = EFX_RXQ_LIMIT_TARGET;
		encp->enc_buftbl_limit = sram_rows -
		    (encp->enc_txq_limit * EFX_TXQ_DC_NDESCS(EFX_TXQ_DC_SIZE) +
		    encp->enc_rxq_limit * EFX_RXQ_DC_NDESCS(EFX_RXQ_DC_SIZE));
	}

	encp->enc_clk_mult = 1;
	encp->enc_evq_timer_quantum_ns =
		EFX_EVQ_FALCON_TIMER_QUANTUM_NS / encp->enc_clk_mult;
	encp->enc_evq_timer_max_us = (encp->enc_evq_timer_quantum_ns <<
		FRF_AB_TIMER_VAL_WIDTH) / 1000;

	/* Determine system monitor configuration */
	switch (encp->enc_board_type) {
#if EFSYS_OPT_MON_LM87
	case BOARD_TYPE_SFE4002_DECODE:
	case BOARD_TYPE_SFE4003_DECODE:
	case BOARD_TYPE_SFE4005_DECODE:
	case BOARD_TYPE_SFN4112F_DECODE:
		encp->enc_mon_type = EFX_MON_LM87;
		enp->en_u.falcon.enu_mon_devid = LM87_DEVID;
#if EFSYS_OPT_MON_STATS
		encp->enc_mon_stat_mask = LM87_STAT_MASK;
#endif
		break;
#endif	/* EFSYS_OPT_MON_LM87 */

#if EFSYS_OPT_MON_MAX6647
	case BOARD_TYPE_SFE4001_DECODE:
		encp->enc_mon_type = EFX_MON_MAX6647;
		enp->en_u.falcon.enu_mon_devid = MAX6647_DEVID;
#if EFSYS_OPT_MON_STATS
		encp->enc_mon_stat_mask = MAX6647_STAT_MASK;
#endif
		break;
#endif	/* EFSYS_OPT_MON_MAX6647 */

#if EFSYS_OPT_MON_MAX6647
	case BOARD_TYPE_SFN4111T_DECODE:
		encp->enc_mon_type = EFX_MON_MAX6647;
#if EFSYS_OPT_MON_STATS
		encp->enc_mon_stat_mask = MAX6647_STAT_MASK;
#endif
		/*
		 * MAX6646 chips are identical to MAX6647 chips in every way
		 * that matters to the driver so we pretend that the chip
		 * is always a MAX6647, but adjust the identifier accordingly
		 */
		enp->en_u.falcon.enu_mon_devid = (major == 0 && minor < 5) ?
		    MAX6647_DEVID : MAX6646_DEVID;
		break;
#endif	/* EFSYS_OPT_MON_MAX6647 */

	default:
		encp->enc_mon_type = EFX_MON_NULL;
		enp->en_u.falcon.enu_mon_devid = 0;
#if EFSYS_OPT_MON_STATS
		encp->enc_mon_stat_mask = 0;
#endif
		break;
	}

	/* Copy the feature flags */
	encp->enc_features = enp->en_features;

	/* Read PHY MII prt and type, and mac address */
	encp->enc_port = EFX_BYTE_FIELD(
		((efx_byte_t *)origin)[CFG_PHY_PORT_REG_SF_OFST], EFX_BYTE_0);
	encp->enc_phy_type = EFX_BYTE_FIELD(
		((efx_byte_t *)origin)[CFG_PHY_TYPE_REG_SF_OFST], EFX_BYTE_0);

	EFX_MAC_ADDR_COPY(encp->enc_mac_addr, origin + MAC_ADDRESS_SF_OFST);

	/* Populate phy capabalities */
	EFX_STATIC_ASSERT(EFX_PHY_NULL == PHY_TYPE_NONE_DECODE);
	EFX_STATIC_ASSERT(EFX_PHY_TXC43128 == PHY_TYPE_TXC43128_DECODE);
	EFX_STATIC_ASSERT(EFX_PHY_SFX7101 == PHY_TYPE_SFX7101_DECODE);
	EFX_STATIC_ASSERT(EFX_PHY_QT2022C2 == PHY_TYPE_QT2022C2_DECODE);
	EFX_STATIC_ASSERT(EFX_PHY_SFT9001A == PHY_TYPE_SFT9001A_DECODE);
	EFX_STATIC_ASSERT(EFX_PHY_QT2025C == PHY_TYPE_QT2025C_DECODE);
	EFX_STATIC_ASSERT(EFX_PHY_SFT9001B == PHY_TYPE_SFT9001B_DECODE);

	switch (encp->enc_phy_type) {
#if EFSYS_OPT_PHY_NULL
	case PHY_TYPE_NONE_DECODE:
		epp->ep_fixed_port_type = EFX_PHY_MEDIA_XAUI;
		epp->ep_default_adv_cap_mask = NULLPHY_ADV_CAP_MASK;
		epp->ep_phy_cap_mask =
			NULLPHY_ADV_CAP_MASK | NULLPHY_ADV_CAP_PERM;
#if EFSYS_OPT_NAMES
		(void) strncpy(encp->enc_phy_name, "nullphy",
		    sizeof (encp->enc_phy_name));
#endif	/* EFSYS_OPT_NAMES */
#if EFSYS_OPT_PHY_LED_CONTROL
		encp->enc_led_mask = NULLPHY_LED_MASK;
#endif	/* EFSYS_OPT_PHY_LED_CONTROL */
#if EFSYS_OPT_LOOPBACK
		encp->enc_loopback_types[EFX_LINK_10000FDX] =
		    NULLPHY_LOOPBACK_MASK;
#endif	/* EFSYS_OPT_LOOPBACK */
#if EFSYS_OPT_PHY_STATS
		encp->enc_phy_stat_mask = NULLPHY_STAT_MASK;
#endif	/* EFSYS_OPT_PHY_STATS */
#if EFSYS_OPT_PHY_PROPS
		encp->enc_phy_nprops = NULLPHY_NPROPS;
#endif	/* EFSYS_OPT_PHY_PROPS */
#if EFSYS_OPT_PHY_BIST
		encp->enc_bist_mask = NULLPHY_BIST_MASK;
#endif	/* EFSYS_OPT_PHY_BIST */
		break;
#endif	/* EFSYS_OPT_PHY_NULL */

#if EFSYS_OPT_PHY_QT2022C2
	case PHY_TYPE_QT2022C2_DECODE:
		epp->ep_fixed_port_type = EFX_PHY_MEDIA_XFP;
		epp->ep_default_adv_cap_mask = QT2022C2_ADV_CAP_MASK;
		epp->ep_phy_cap_mask =
			QT2022C2_ADV_CAP_MASK | QT2022C2_ADV_CAP_PERM;
#if EFSYS_OPT_NAMES
		(void) strncpy(encp->enc_phy_name, "qt2022c2",
		    sizeof (encp->enc_phy_name));
#endif	/* EFSYS_OPT_NAMES */
#if EFSYS_OPT_PHY_LED_CONTROL
		encp->enc_led_mask = QT2022C2_LED_MASK;
#endif	/* EFSYS_OPT_PHY_LED_CONTROL */
#if EFSYS_OPT_LOOPBACK
		encp->enc_loopback_types[EFX_LINK_10000FDX] =
		    QT2022C2_LOOPBACK_MASK;
#endif	/* EFSYS_OPT_LOOPBACK */
#if EFSYS_OPT_PHY_STATS
		encp->enc_phy_stat_mask = QT2022C2_STAT_MASK;
#endif	/* EFSYS_OPT_PHY_STATS */
#if EFSYS_OPT_PHY_PROPS
		encp->enc_phy_nprops = QT2022C2_NPROPS;
#endif	/* EFSYS_OPT_PHY_PROPS */
#if EFSYS_OPT_PHY_BIST
		encp->enc_bist_mask = QT2022C2_BIST_MASK;
#endif	/* EFSYS_OPT_PHY_BIST */
		break;
#endif	/* EFSYS_OPT_PHY_QT2022C2 */

#if EFSYS_OPT_PHY_SFX7101
	case PHY_TYPE_SFX7101_DECODE:
		epp->ep_fixed_port_type = EFX_PHY_MEDIA_BASE_T;
		epp->ep_default_adv_cap_mask = SFX7101_ADV_CAP_MASK;
		epp->ep_phy_cap_mask =
			SFX7101_ADV_CAP_MASK | SFX7101_ADV_CAP_PERM;
#if EFSYS_OPT_NAMES
		(void) strncpy(encp->enc_phy_name, "sfx7101",
		    sizeof (encp->enc_phy_name));
#endif	/* EFSYS_OPT_NAMES */
#if EFSYS_OPT_PHY_LED_CONTROL
		encp->enc_led_mask = SFX7101_LED_MASK;
#endif	/* EFSYS_OPT_PHY_LED_CONTROL */
#if EFSYS_OPT_LOOPBACK
		encp->enc_loopback_types[EFX_LINK_10000FDX] =
		    SFX7101_LOOPBACK_MASK;
#endif	/* EFSYS_OPT_LOOPBACK */
#if EFSYS_OPT_PHY_STATS
		encp->enc_phy_stat_mask = SFX7101_STAT_MASK;
#endif	/* EFSYS_OPT_PHY_STATS */
#if EFSYS_OPT_PHY_PROPS
		encp->enc_phy_nprops = SFX7101_NPROPS;
#endif	/* EFSYS_OPT_PHY_PROPS */
#if EFSYS_OPT_PHY_BIST
		encp->enc_bist_mask = SFX7101_BIST_MASK;
#endif	/* EFSYS_OPT_PHY_BIST */
		break;
#endif	/* EFSYS_OPT_PHY_SFX7101 */

#if EFSYS_OPT_PHY_TXC43128
	case PHY_TYPE_TXC43128_DECODE:
		epp->ep_fixed_port_type = EFX_PHY_MEDIA_CX4;
		epp->ep_default_adv_cap_mask = TXC43128_ADV_CAP_MASK;
		epp->ep_phy_cap_mask =
			TXC43128_ADV_CAP_MASK | TXC43128_ADV_CAP_PERM;
#if EFSYS_OPT_NAMES
		(void) strncpy(encp->enc_phy_name, "txc43128",
		    sizeof (encp->enc_phy_name));
#endif	/* EFSYS_OPT_NAMES */
#if EFSYS_OPT_PHY_LED_CONTROL
		encp->enc_led_mask = TXC43128_LED_MASK;
#endif	/* EFSYS_OPT_PHY_LED_CONTROL */
#if EFSYS_OPT_LOOPBACK
		encp->enc_loopback_types[EFX_LINK_10000FDX] =
		    TXC43128_LOOPBACK_MASK;
#endif	/* EFSYS_OPT_LOOPBACK */
#if EFSYS_OPT_PHY_STATS
		encp->enc_phy_stat_mask = TXC43128_STAT_MASK;
#endif	/* EFSYS_OPT_PHY_STATS */
#if EFSYS_OPT_PHY_PROPS
		encp->enc_phy_nprops = TXC43128_NPROPS;
#endif	/* EFSYS_OPT_PHY_PROPS */
#if EFSYS_OPT_PHY_BIST
		encp->enc_bist_mask = TXC43128_BIST_MASK;
#endif	/* EFSYS_OPT_PHY_BIST */
		break;
#endif	/* EFSYS_OPT_PHY_TXC43128 */

#if EFSYS_OPT_PHY_SFT9001
	case PHY_TYPE_SFT9001A_DECODE:
	case PHY_TYPE_SFT9001B_DECODE:
		epp->ep_fixed_port_type = EFX_PHY_MEDIA_BASE_T;
		epp->ep_default_adv_cap_mask = SFT9001_ADV_CAP_MASK;
		epp->ep_phy_cap_mask =
			SFT9001_ADV_CAP_MASK | SFT9001_ADV_CAP_PERM;
#if EFSYS_OPT_NAMES
		(void) strncpy(encp->enc_phy_name, "sft9001",
		    sizeof (encp->enc_phy_name));
#endif	/* EFSYS_OPT_NAMES */
#if EFSYS_OPT_PHY_LED_CONTROL
		encp->enc_led_mask = SFT9001_LED_MASK;
#endif	/* EFSYS_OPT_PHY_LED_CONTROL */
#if EFSYS_OPT_LOOPBACK
		encp->enc_loopback_types[EFX_LINK_10000FDX] =
		    SFT9001_10G_LOOPBACK_MASK;
		encp->enc_loopback_types[EFX_LINK_1000FDX] =
		    SFT9001_1G_LOOPBACK_MASK;
#endif	/* EFSYS_OPT_LOOPBACK */
#if EFSYS_OPT_PHY_STATS
		encp->enc_phy_stat_mask = SFT9001_STAT_MASK;
#endif	/* EFSYS_OPT_PHY_STATS */
#if EFSYS_OPT_PHY_PROPS
		encp->enc_phy_nprops = SFT9001_NPROPS;
#endif	/* EFSYS_OPT_PHY_PROPS */
#if EFSYS_OPT_PHY_BIST
		encp->enc_bist_mask = SFT9001_BIST_MASK;
#endif	/* EFSYS_OPT_PHY_BIST */
		break;
#endif	/* EFSYS_OPT_PHY_SFT9001 */

#if EFSYS_OPT_PHY_QT2025C
	case EFX_PHY_QT2025C:
		epp->ep_fixed_port_type = EFX_PHY_MEDIA_SFP_PLUS;
		epp->ep_default_adv_cap_mask = QT2025C_ADV_CAP_MASK;
		epp->ep_phy_cap_mask =
			QT2025C_ADV_CAP_MASK | QT2025C_ADV_CAP_PERM;
#if EFSYS_OPT_NAMES
		(void) strncpy(encp->enc_phy_name, "qt2025c",
		    sizeof (encp->enc_phy_name));
#endif	/* EFSYS_OPT_NAMES */
#if EFSYS_OPT_PHY_LED_CONTROL
		encp->enc_led_mask = QT2025C_LED_MASK;
#endif	/* EFSYS_OPT_PHY_LED_CONTROL */
#if EFSYS_OPT_LOOPBACK
		encp->enc_loopback_types[EFX_LINK_10000FDX] =
		    QT2025C_LOOPBACK_MASK;
#endif	/* EFSYS_OPT_LOOPBACK */
#if EFSYS_OPT_PHY_STATS
		encp->enc_phy_stat_mask = QT2025C_STAT_MASK;
#endif	/* EFSYS_OPT_PHY_STATS */
#if EFSYS_OPT_PHY_PROPS
		encp->enc_phy_nprops = QT2025C_NPROPS;
#endif	/* EFSYS_OPT_PHY_PROPS */
#if EFSYS_OPT_PHY_BIST
		encp->enc_bist_mask = QT2025C_BIST_MASK;
#endif	/* EFSYS_OPT_PHY_BIST */
		break;
#endif	/* EFSYS_OPT_PHY_QT2025C */

	default:
		rc = ENOTSUP;
		goto fail2;
	}

#if EFSYS_OPT_LOOPBACK
	encp->enc_loopback_types[EFX_LINK_UNKNOWN] =
	    (1 << EFX_LOOPBACK_OFF) |
	    encp->enc_loopback_types[EFX_LINK_1000FDX] |
	    encp->enc_loopback_types[EFX_LINK_10000FDX];
#endif	/* EFSYS_OPT_LOOPBACK */

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#if EFSYS_OPT_PCIE_TUNE

static			void
falcon_nic_pcie_core_read(
	__in		efx_nic_t *enp,
	__in		uint32_t addr,
	__out		efx_dword_t *edp)
{
	int lstate;
	efx_oword_t oword;
	uint32_t val;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	EFSYS_LOCK(enp->en_eslp, lstate);

	EFX_POPULATE_OWORD_2(oword, FRF_BB_PCIE_CORE_TARGET_REG_ADRS, addr,
	    FRF_BB_PCIE_CORE_INDIRECT_ACCESS_DIR, 0);
	EFX_BAR_WRITEO(enp, FR_BB_PCIE_CORE_INDIRECT_REG, &oword);

	EFSYS_SPIN(10);

	EFX_BAR_READO(enp, FR_BB_PCIE_CORE_INDIRECT_REG, &oword);
	val = EFX_OWORD_FIELD(oword, FRF_BB_PCIE_CORE_TARGET_DATA);

	EFX_POPULATE_DWORD_1(*edp, EFX_DWORD_0, val);

	EFSYS_UNLOCK(enp->en_eslp, lstate);
}

static 			void
falcon_nic_pcie_core_write(
	__in		efx_nic_t *enp,
	__in		uint32_t addr,
	__out		efx_dword_t *edp)
{
	int lstate;
	efx_oword_t oword;
	uint32_t val;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	EFSYS_LOCK(enp->en_eslp, lstate);

	val = EFX_DWORD_FIELD(*edp, EFX_DWORD_0);

	EFX_POPULATE_OWORD_3(oword, FRF_BB_PCIE_CORE_TARGET_REG_ADRS, addr,
	    FRF_BB_PCIE_CORE_TARGET_DATA, val,
	    FRF_BB_PCIE_CORE_INDIRECT_ACCESS_DIR, 0);
	EFX_BAR_WRITEO(enp, FR_BB_PCIE_CORE_INDIRECT_REG, &oword);

	EFSYS_UNLOCK(enp->en_eslp, lstate);
}
#endif	/* EFSYS_OPT_PCIE_TUNE || EFSYS_OPT_DIAG */

#if EFSYS_OPT_PCIE_TUNE

typedef struct falcon_pcie_rpl_s {
	size_t		fpr_tlp_size;
	uint32_t	fpr_value[4];
} falcon_pcie_rpl_t;

/*  TLP		1x	2x	4x	8x */
static falcon_pcie_rpl_t	__cs falcon_nic_pcie_rpl[] = {
	{ 128,	{ 421,	257,	174,	166 } },
	{ 256,	{ 698,	391,	241,	225 } },
	{ 512,	{ 903,	498,	295,	193 } },
	{ 1024,	{ 1670,	881,	487,	290 } }
};

static			void
falcon_nic_pcie_rpl_tl_set(
	__in		efx_nic_t *enp,
	__in		unsigned int nlanes)
{
	uint32_t index;
	uint32_t current;
	falcon_pcie_rpl_t *fprp;
	uint32_t expected;
	efx_dword_t dword;

	EFSYS_ASSERT3U(nlanes, >, 0);
	EFSYS_ASSERT3U(nlanes, <=, 8);
	EFSYS_ASSERT(ISP2(nlanes));

	/* Get the appropriate set of replay timer values */
	falcon_nic_pcie_core_read(enp, PCR_AB_DEV_CTL_REG, &dword);

	index = EFX_DWORD_FIELD(dword, PCRF_AZ_MAX_PAYL_SIZE);
	if (index >= 4) {
		EFSYS_PROBE1(fail1, int, EIO);
		return;
	}

	fprp = (falcon_pcie_rpl_t *)&(falcon_nic_pcie_rpl[index]);

	EFSYS_PROBE1(pcie_tlp_size, size_t, fprp->fpr_tlp_size);

	for (index = 0; index < 4; index++)
		if ((1 << index) == nlanes)
			break;

	/* Get the current replay timer value */
	falcon_nic_pcie_core_read(enp, PCR_AC_ACK_LAT_TMR_REG, &dword);
	current = EFX_DWORD_FIELD(dword, PCRF_AC_RT);

	/* Get the appropriate replay timer value from the set */
	expected = fprp->fpr_value[index];

	EFSYS_PROBE2(pcie_rpl_tl, uint32_t, current, uint32_t, expected);

	EFSYS_PROBE1(pcie_ack_tl,
	    uint32_t, EFX_DWORD_FIELD(dword, PCRF_AC_ALT));

	if (expected > current) {
		EFX_SET_DWORD_FIELD(dword, PCRF_AC_RT, expected);
		falcon_nic_pcie_core_write(enp, PCR_AC_ACK_LAT_TMR_REG,
		    &dword);
	}
}

static			void
falcon_nic_pcie_ack_freq_set(
	__in		efx_nic_t *enp,
	__in		uint32_t freq)
{
	efx_dword_t dword;

	falcon_nic_pcie_core_read(enp, PCR_AC_ACK_FREQ_REG, &dword);

	EFSYS_PROBE2(pcie_ack_freq,
	    uint32_t, EFX_DWORD_FIELD(dword, PCRF_AC_ACK_FREQ),
	    uint32_t, freq);

	/* Set to zero to ensure that we always ACK after timeout */
	EFX_SET_DWORD_FIELD(dword, PCRF_AC_ACK_FREQ, freq);
	falcon_nic_pcie_core_write(enp, PCR_AC_ACK_FREQ_REG, &dword);
}

			int
falcon_nic_pcie_tune(
	__in		efx_nic_t *enp,
	__in		unsigned int nlanes)
{
	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	enp->en_u.falcon.enu_nlanes = nlanes;
	return (0);
}

#endif	/* EFSYS_OPT_PCIE_TUNE */

	__checkReturn	int
falcon_nic_reset(
	__in		efx_nic_t *enp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	falcon_i2c_t *fip = &(enp->en_u.falcon.enu_fip);
	efx_oword_t oword;
	unsigned int count;
	int rc;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	/* Check the reset register */
	EFX_BAR_READO(enp, FR_AB_GLB_CTL_REG, &oword);

	/* Select units for reset */
	EFX_SET_OWORD_FIELD(oword, FRF_AB_RST_CS, 1);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_RST_TX, 1);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_RST_XGTX, 1);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_RST_RX, 1);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_RST_XGRX, 1);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_RST_SR, 1);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_RST_EV, 1);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_RST_EM, 1);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_RST_PCIE_STKY, 1);
	EFX_SET_OWORD_FIELD(oword, FRF_BB_RST_BIU, 1);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_RST_XAUI_SD, 1);

	/* Initiate reset */
	EFX_BAR_WRITEO(enp, FR_AB_GLB_CTL_REG, &oword);

	/* Wait for the reset to complete */
	count = 0;
	do {
		EFSYS_PROBE1(wait, unsigned int, count);

		/* Spin for 10 us */
		EFSYS_SPIN(10);

		/* Test for reset complete */
		EFX_BAR_READO(enp, FR_AB_GLB_CTL_REG, &oword);
		if (EFX_OWORD_FIELD(oword, FRF_AB_RST_CS) == 0 &&
		    EFX_OWORD_FIELD(oword, FRF_AB_RST_TX) == 0 &&
		    EFX_OWORD_FIELD(oword, FRF_AB_RST_XGTX) == 0 &&
		    EFX_OWORD_FIELD(oword, FRF_AB_RST_RX) == 0 &&
		    EFX_OWORD_FIELD(oword, FRF_AB_RST_XGRX) == 0 &&
		    EFX_OWORD_FIELD(oword, FRF_AB_RST_SR) == 0 &&
		    EFX_OWORD_FIELD(oword, FRF_AB_RST_EV) == 0 &&
		    EFX_OWORD_FIELD(oword, FRF_AB_RST_EM) == 0 &&
		    EFX_OWORD_FIELD(oword, FRF_AB_RST_PCIE_STKY) == 0 &&
		    EFX_OWORD_FIELD(oword, FRF_BB_RST_BIU) == 0 &&
		    EFX_OWORD_FIELD(oword, FRF_AB_RST_XAUI_SD) == 0)
			goto done;
	} while (++count < 1000);

	rc = ETIMEDOUT;
	goto fail1;

done:
	/* GPIO initialization */
	EFX_BAR_READO(enp, FR_AB_GPIO_CTL_REG, &oword);

	/* Set I2C SCL to 1 */
	EFX_SET_OWORD_FIELD(oword, FRF_AB_GPIO0_OUT, 1);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_GPIO0_OEN, 1);

	/* Select external 1G MAC clock if it is available */
	EFX_SET_OWORD_FIELD(oword, FRF_BB_USE_NIC_CLK,
	    (encp->enc_board_type == BOARD_TYPE_SFN4111T_DECODE));

	EFX_BAR_WRITEO(enp, FR_AB_GPIO_CTL_REG, &oword);

	fip->fi_sda = B_TRUE;
	fip->fi_scl = B_TRUE;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static			void
falcon_nic_timer_tbl_watchdog(
	__in		efx_nic_t *enp)
{
	efx_oword_t oword;

	if (enp->en_family != EFX_FAMILY_FALCON)
		return;

	/*
	 * Ensure that PCI writes to the event queue read pointers are spaced
	 * out far enough to avoid write loss.
	 */
	EFX_BAR_READO(enp, FR_AZ_HW_INIT_REG, &oword);
	EFX_SET_OWORD_FIELD(oword, FRF_AZ_POST_WR_MASK, 0xf);
	EFX_SET_OWORD_FIELD(oword, FRF_AZ_WD_TIMER, 0x10);
	EFX_BAR_WRITEO(enp, FR_AZ_HW_INIT_REG, &oword);
}

static			void
falcon_rx_reset_recovery_enable(
	__in		efx_nic_t *enp)
{
	efx_oword_t oword;

	/*
	 * Set number of channels for receive path and also set filter table
	 * search limits to 8, to reduce the frequency of RX_RECOVERY events
	 */
	EFX_POPULATE_OWORD_4(oword,
				FRF_AZ_UDP_FULL_SRCH_LIMIT, 8,
				FRF_AZ_UDP_WILD_SRCH_LIMIT, 8,
				FRF_AZ_TCP_FULL_SRCH_LIMIT, 8,
				FRF_AZ_TCP_WILD_SRCH_LIMIT, 8);
	EFX_BAR_WRITEO(enp, FR_AZ_RX_FILTER_CTL_REG, &oword);

	/*
	 * Enable RX Self-Reset functionality.
	 * Disable ISCSI digest to reduce RX_RECOVERY frequency
	 */
	EFX_BAR_READO(enp, FR_AZ_RX_SELF_RST_REG, &oword);
	EFX_SET_OWORD_FIELD(oword, FRF_AZ_RX_ISCSI_DIS, 1);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_RX_SW_RST_REG, 1);
	EFX_BAR_WRITEO(enp, FR_AZ_RX_SELF_RST_REG, &oword);
}

	__checkReturn	int
falcon_nic_init(
	__in		efx_nic_t *enp)
{
	int rc;

	if ((rc = falcon_sram_init(enp)) != 0)
		goto fail1;

#if EFSYS_OPT_PCIE_TUNE
	/* Tune up the PCIe core */
	if (enp->en_u.falcon.enu_nlanes > 0) {
		falcon_nic_pcie_rpl_tl_set(enp, enp->en_u.falcon.enu_nlanes);
		falcon_nic_pcie_ack_freq_set(enp, 0);
	}
#endif
	/* Fix NMI's due to premature timer_tbl biu watchdog */
	falcon_nic_timer_tbl_watchdog(enp);

	/* Enable RX_RECOVERY feature */
	falcon_rx_reset_recovery_enable(enp);

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn	int
falcon_nic_mac_reset(
	__in		efx_nic_t *enp)
{
	efx_oword_t oword;
	unsigned int count;
	int rc;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	EFX_BAR_READO(enp, FR_AB_MAC_CTRL_REG, &oword);
	EFX_SET_OWORD_FIELD(oword, FRF_BB_TXFIFO_DRAIN_EN, 1);
	EFX_BAR_WRITEO(enp, FR_AB_MAC_CTRL_REG, &oword);

	/* Reset the MAC and EM units */
	EFX_BAR_READO(enp, FR_AB_GLB_CTL_REG, &oword);

	EFX_SET_OWORD_FIELD(oword, FRF_AB_RST_EM, 1);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_RST_XGRX, 1);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_RST_XGTX, 1);

	/* Initiate reset */
	EFX_BAR_WRITEO(enp, FR_AB_GLB_CTL_REG, &oword);

	/* Wait for the reset to complete */
	count = 0;
	do {
		EFSYS_PROBE1(wait, unsigned int, count);

		/* Spin for 10 us */
		EFSYS_SPIN(10);

		/* Test for reset complete */
		EFX_BAR_READO(enp, FR_AB_GLB_CTL_REG, &oword);
		if (EFX_OWORD_FIELD(oword, FRF_AB_RST_EM) == 0 &&
		    EFX_OWORD_FIELD(oword, FRF_AB_RST_XGRX) == 0 &&
		    EFX_OWORD_FIELD(oword, FRF_AB_RST_XGTX) == 0)
			goto done;
	} while (++count < 1000);

	rc = ETIMEDOUT;
	goto fail1;

done:
	enp->en_reset_flags |= EFX_RESET_MAC;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);
	return (rc);
}

			void
falcon_nic_phy_reset(
	__in		efx_nic_t *enp)
{
	efx_oword_t oword;
	int state;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	/* Set PHY_RSTn to 0 */
	EFSYS_LOCK(enp->en_eslp, state);
	EFX_BAR_READO(enp, FR_AB_GPIO_CTL_REG, &oword);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_GPIO2_OUT, 0);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_GPIO2_OEN, 1);
	EFX_BAR_WRITEO(enp, FR_AB_GPIO_CTL_REG, &oword);
	EFSYS_UNLOCK(enp->en_eslp, state);

	EFSYS_SLEEP(500000);

	EFSYS_LOCK(enp->en_eslp, state);
	EFX_BAR_READO(enp, FR_AB_GPIO_CTL_REG, &oword);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_GPIO2_OEN, 0);
	EFX_BAR_WRITEO(enp, FR_AB_GPIO_CTL_REG, &oword);
	EFSYS_UNLOCK(enp->en_eslp, state);

	EFSYS_SLEEP(100000);

	enp->en_reset_flags |= EFX_RESET_PHY;
}

			void
falcon_nic_fini(
	__in		efx_nic_t *enp)
{
	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

#if EFSYS_OPT_PCIE_TUNE
	falcon_nic_pcie_ack_freq_set(enp, 1);
#endif	/* EFSYS_OPT_PCIE_TUNE */

	falcon_sram_fini(enp);
}

			void
falcon_nic_unprobe(
	__in		efx_nic_t *enp)
{
	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	falcon_nvram_fini(enp);
}

#if EFSYS_OPT_DIAG

static efx_register_set_t __cs	__falcon_b0_registers[] = {
	{ FR_AZ_ADR_REGION_REG_OFST, 0, 1 },
	{ FR_AZ_RX_CFG_REG_OFST, 0, 1 },
	{ FR_AZ_TX_CFG_REG_OFST, 0, 1 },
	{ FR_AZ_TX_RESERVED_REG_OFST, 0, 1 },
	{ FR_AB_MAC_CTRL_REG_OFST, 0, 1 },
	{ FR_AZ_SRM_TX_DC_CFG_REG_OFST, 0, 1 },
	{ FR_AZ_RX_DC_CFG_REG_OFST, 0, 1 },
	{ FR_AZ_RX_DC_PF_WM_REG_OFST, 0, 1 },
	{ FR_AZ_DP_CTRL_REG_OFST, 0, 1 },
	{ FR_AB_GM_CFG2_REG_OFST, 0, 1 },
	{ FR_AB_GMF_CFG0_REG_OFST, 0, 1 },
	{ FR_AB_XM_GLB_CFG_REG_OFST, 0, 1 },
	{ FR_AB_XM_TX_CFG_REG_OFST, 0, 1 },
	{ FR_AB_XM_RX_CFG_REG_OFST, 0, 1 },
	{ FR_AB_XM_RX_PARAM_REG_OFST, 0, 1 },
	{ FR_AB_XM_FC_REG_OFST, 0, 1 },
	{ FR_AB_XM_ADR_LO_REG_OFST, 0, 1 },
	{ FR_AB_XX_SD_CTL_REG_OFST, 0, 1 },
};

static const uint32_t __cs	__falcon_b0_register_masks[] = {
	0x0003FFFF, 0x0003FFFF, 0x0003FFFF, 0x0003FFFF,
	0xFFFFFFFE, 0x00017FFF, 0x00000000, 0x00000000,
	0x7FFF0037, 0x00000000, 0x00000000, 0x00000000,
	0xFFFEFE80, 0x1FFFFFFF, 0x020000FE, 0x007FFFFF,
	0xFFFF0000, 0x00000000, 0x00000000, 0x00000000,
	0x001FFFFF, 0x00000000, 0x00000000, 0x00000000,
	0x0000000F, 0x00000000, 0x00000000, 0x00000000,
	0x000003FF, 0x00000000, 0x00000000, 0x00000000,
	0x00000FFF, 0x00000000, 0x00000000, 0x00000000,
	0x00007337, 0x00000000, 0x00000000, 0x00000000,
	0x00001F1F, 0x00000000, 0x00000000, 0x00000000,
	0x00000C68, 0x00000000, 0x00000000, 0x00000000,
	0x00080164, 0x00000000, 0x00000000, 0x00000000,
	0x07100A0C, 0x00000000, 0x00000000, 0x00000000,
	0x00001FF8, 0x00000000, 0x00000000, 0x00000000,
	0xFFFF0001, 0x00000000, 0x00000000, 0x00000000,
	0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000,
	0x0003FF0F, 0x00000000, 0x00000000, 0x00000000,
};

static efx_register_set_t __cs	__falcon_b0_tables[] = {
	{ FR_AZ_RX_FILTER_TBL0_OFST, FR_AZ_RX_FILTER_TBL0_STEP,
	    FR_AZ_RX_FILTER_TBL0_ROWS },
	{ FR_AB_RX_FILTER_TBL1_OFST, FR_AB_RX_FILTER_TBL1_STEP,
	    FR_AB_RX_FILTER_TBL1_ROWS },
	{ FR_AZ_RX_DESC_PTR_TBL_OFST,
	    FR_AZ_RX_DESC_PTR_TBL_STEP, FR_AB_RX_DESC_PTR_TBL_ROWS },
	{ FR_AZ_TX_DESC_PTR_TBL_OFST,
	    FR_AZ_TX_DESC_PTR_TBL_STEP, FR_AB_TX_DESC_PTR_TBL_ROWS },
};

static const uint32_t __cs	__falcon_b0_table_masks[] = {
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x000003FF,
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x000003FF,
	0xFFFFFFFE, 0x0FFFFFFF, 0x01800000, 0x00000000,
	0x3FFFFFFE, 0x0FFFFFFF, 0x0C000000, 0x00000000,
};

	__checkReturn	int
falcon_nic_register_test(
	__in		efx_nic_t *enp)
{
	efx_register_set_t *rsp;
	const uint32_t *dwordp;
	unsigned int nitems;
	unsigned int count;
	int rc;

	/* Fill out the register mask entries */
	EFX_STATIC_ASSERT(EFX_ARRAY_SIZE(__falcon_b0_register_masks)
		    == EFX_ARRAY_SIZE(__falcon_b0_registers) * 4);

	nitems = EFX_ARRAY_SIZE(__falcon_b0_registers);
	dwordp = __falcon_b0_register_masks;
	for (count = 0; count < nitems; ++count) {
		rsp = __falcon_b0_registers + count;
		rsp->mask.eo_u32[0] = *dwordp++;
		rsp->mask.eo_u32[1] = *dwordp++;
		rsp->mask.eo_u32[2] = *dwordp++;
		rsp->mask.eo_u32[3] = *dwordp++;
	}

	/* Fill out the register table entries */
	EFX_STATIC_ASSERT(EFX_ARRAY_SIZE(__falcon_b0_table_masks)
		    == EFX_ARRAY_SIZE(__falcon_b0_tables) * 4);

	nitems = EFX_ARRAY_SIZE(__falcon_b0_tables);
	dwordp = __falcon_b0_table_masks;
	for (count = 0; count < nitems; ++count) {
		rsp = __falcon_b0_tables + count;
		rsp->mask.eo_u32[0] = *dwordp++;
		rsp->mask.eo_u32[1] = *dwordp++;
		rsp->mask.eo_u32[2] = *dwordp++;
		rsp->mask.eo_u32[3] = *dwordp++;
	}

	if ((rc = efx_nic_test_registers(enp, __falcon_b0_registers,
	    EFX_ARRAY_SIZE(__falcon_b0_registers))) != 0)
		goto fail1;

	if ((rc = efx_nic_test_tables(enp, __falcon_b0_tables,
	    EFX_PATTERN_BYTE_ALTERNATE,
	    EFX_ARRAY_SIZE(__falcon_b0_tables))) != 0)
		goto fail2;

	if ((rc = efx_nic_test_tables(enp, __falcon_b0_tables,
	    EFX_PATTERN_BYTE_CHANGING,
	    EFX_ARRAY_SIZE(__falcon_b0_tables))) != 0)
		goto fail3;

	if ((rc = efx_nic_test_tables(enp, __falcon_b0_tables,
	    EFX_PATTERN_BIT_SWEEP, EFX_ARRAY_SIZE(__falcon_b0_tables))) != 0)
		goto fail4;

	return (0);

fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#endif	/* EFSYS_OPT_DIAG */

#endif	/* EFSYS_OPT_FALCON */
