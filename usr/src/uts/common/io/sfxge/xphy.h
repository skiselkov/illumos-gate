/*-
 * Copyright 2006-2013 Solarflare Communications Inc.  All rights reserved.
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

#ifndef _SYS_XPHY_H
#define	_SYS_XPHY_H

#include "efx.h"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Common
 */

/* control register 1 */
#define	MMD_CONTROL1_REG 0x00
#define	MMD_PMA_LOOPBACK_LBN 0
#define	MMD_PMA_LOOPBACK_WIDTH 1
#define	MMD_LOOPBACK_LBN 14
#define	MMD_LOOPBACK_WIDTH 1
#define	MMD_RESET_LBN 15
#define	MMD_RESET_WIDTH 1

/* status register 1 */
#define	MMD_STATUS1_REG 0x01
#define	MMD_LINK_UP_LBN 2
#define	MMD_LINK_UP_WIDTH 1
#define	MMD_FAULT_LBN 7
#define	MMD_FAULT_WIDTH 1

/* identifier registers */
#define	MMD_IDH_REG 0x02
#define	MMD_IDH_LBN 0
#define	MMD_IDH_WIDTH 16

#define	MMD_IDL_REG 0x03
#define	MMD_IDL_LBN 0
#define	MMD_IDL_WIDTH 16

/* devices in package registers */
#define	MMD_DEVICES1_REG 0x05
#define	PMA_PMD_PRESENT_LBN 1
#define	PMA_PMD_PRESENT_WIDTH 1
#define	WIS_PRESENT_LBN 2
#define	WIS_PRESENT_WIDTH 1
#define	PCS_PRESENT_LBN 3
#define	PCS_PRESENT_WIDTH 1
#define	PHY_XS_PRESENT_LBN 4
#define	PHY_XS_PRESENT_WIDTH 1
#define	DTE_XS_PRESENT_LBN 5
#define	DTE_XS_PRESENT_WIDTH 1
#define	TC_PRESENT_LBN 6
#define	TC_PRESENT_WIDTH 1
#define	AN_PRESENT_LBN 7
#define	AN_PRESENT_WIDTH 1

#define	MMD_DEVICES2_REG 0x06
#define	CL22EXT_PRESENT_LBN 13
#define	CL22EXT_PRESENT_WIDTH 1
#define	VENDOR_DEVICE1_PRESENT_LBN 14
#define	VENDOR_DEVICE1_PRESENT_WIDTH 1
#define	VENDOR_DEVICE2_PRESENT_LBN 15
#define	VENDOR_DEVICE2_PRESENT_WIDTH 1

#define	MMD_STATUS2_REG 0x08
#define	MMD_RX_FAULT_LBN 10
#define	MMD_RX_FAULT_WIDTH 1
#define	MMD_TX_FAULT_LBN 11
#define	MMD_TX_FAULT_WIDTH 1
#define	MMD_RESPONDING_LBN 14
#define	MMD_RESPONDING_WIDTH 2
#define	MMD_RESPONDING_DECODE 0x2	/* 10 */

/*
 * PMA/PMD
 */

/* control register 1 */
#define	PMA_PMD_CONTROL1_REG 0x00
#define	PMA_PMD_LOOPBACK_EN_LBN 0
#define	PMA_PMD_LOOPBACK_EN_WIDTH 1
#define	PMA_PMD_LOW_POWER_EN_LBN 11
#define	PMA_PMD_LOW_POWER_EN_WIDTH 1
#define	PMA_PMD_RESET_LBN 15
#define	PMA_PMD_RESET_WIDTH 1

/* status register 1 */
#define	PMA_PMD_STATUS1_REG 0x01
#define	PMA_PMD_POWER_CAP_LBN 1
#define	PMA_PMD_POWER_CAP_WIDTH 1
#define	PMA_PMD_LINK_UP_LBN 2
#define	PMA_PMD_LINK_UP_WIDTH 1
#define	PMA_PMD_FAULT_LBN 7
#define	PMA_PMD_FAULT_WIDTH 1

/* control register 2 */
#define	PMA_PMD_CONTROL2_REG 0x07
#define	PMA_PMD_TYPE_LBN 0
#define	PMA_PMD_TYPE_WIDTH 4
#define	PMA_PMD_10BASE_T_DECODE 0xf	/* 1111 */
#define	PMA_PMD_100BASE_TX_DECODE 0xe	/* 1110 */
#define	PMA_PMD_1000BASE_KX_DECODE 0xd	/* 1101 */
#define	PMA_PMD_1000BASE_T_DECODE 0xc	/* 1100 */
#define	PMA_PMD_10GBASE_KR_DECODE 0xb	/* 1011 */
#define	PMA_PMD_10GBASE_KX4_DECODE 0xa	/* 1010 */
#define	PMA_PMD_10GBASE_T_DECODE 0x9	/* 1001 */
#define	PMA_PMD_10GBASE_LRM_DECODE 0x8	/* 1001 */
#define	PMA_PMD_10GBASE_SR_DECODE 0x7	/* 0111 */
#define	PMA_PMD_10GBASE_LR_DECODE 0x6	/* 0110 */
#define	PMA_PMD_10GBASE_ER_DECODE 0x5	/* 0101 */
#define	PMA_PMD_10GBASE_SW_DECODE 0x3	/* 0011 */
#define	PMA_PMD_10GBASE_LW_DECODE 0x2	/* 0010 */
#define	PMA_PMD_10GBASE_EW_DECODE 0x1	/* 0001 */

/* status register 2 */
#define	PMA_PMD_STATUS2_REG 0x08
#define	PMA_PMD_LOOPBACK_CAP_LBN 0
#define	PMA_PMD_LOOPBACK_CAP_WIDTH 1
#define	PMA_PMD_10GBASE_EW_CAP_LBN 1
#define	PMA_PMD_10GBASE_EW_CAP_WIDTH 1
#define	PMA_PMD_10GBASE_LW_CAP_LBN 2
#define	PMA_PMD_10GBASE_LW_CAP_WIDTH 1
#define	PMA_PMD_10GBASE_SW_CAP_LBN 3
#define	PMA_PMD_10GBASE_SW_CAP_WIDTH 1
#define	PMA_PMD_10GBASE_ER_CAP_LBN 5
#define	PMA_PMD_10GBASE_ER_CAP_WIDTH 1
#define	PMA_PMD_10GBASE_LR_CAP_LBN 6
#define	PMA_PMD_10GBASE_LR_CAP_WIDTH 1
#define	PMA_PMD_10GBASE_SR_CAP_LBN 7
#define	PMA_PMD_10GBASE_SR_CAP_WIDTH 1
#define	PMA_PMD_TX_DISABLE_CAP_LBN 8
#define	PMA_PMD_TX_DISABLE_CAP_WIDTH 1
#define	PMA_PMD_EXT_CAP_LBN 9
#define	PMA_PMD_EXT_CAP_WIDTH 1
#define	PMA_PMD_RX_FAULT_LBN 10
#define	PMA_PMD_RX_FAULT_WIDTH 1
#define	PMA_PMD_TX_FAULT_LBN 11
#define	PMA_PMD_TX_FAULT_WIDTH 1
#define	PMA_PMD_RX_FAULT_CAP_LBN 12
#define	PMA_PMD_RX_FAULT_CAP_WIDTH 1
#define	PMA_PMD_TX_FAULT_CAP_LBN 13
#define	PMA_PMD_TX_FAULT_CAP_WIDTH 1
#define	PMA_PMD_RESPONDING_LBN 14
#define	PMA_PMD_RESPONDING_WIDTH 2
#define	PMA_PMD_RESPONDING_DECODE 0x2	/* 10 */

/* extended capabilities register */
#define	PMA_PMD_EXT_CAP_REG 0x0b
#define	PMA_PMD_10GBASE_CX4_CAP_LBN 0
#define	PMA_PMD_10GBASE_CX4_CAP_WIDTH 1
#define	PMA_PMD_10GBASE_LRM_CAP_LBN 1
#define	PMA_PMD_10GBASE_LRM_CAP_WIDTH 1
#define	PMA_PMD_10GBASE_T_CAP_LBN 2
#define	PMA_PMD_10GBASE_T_CAP_WIDTH 1
#define	PMA_PMD_10GBASE_KX4_CAP_LBN 3
#define	PMA_PMD_10GBASE_KX4_CAP_WIDTH 1
#define	PMA_PMD_10GBASE_KR_CAP_LBN 4
#define	PMA_PMD_10GBASE_KR_CAP_WIDTH 1
#define	PMA_PMD_1000BASE_T_CAP_LBN 5
#define	PMA_PMD_1000BASE_T_CAP_WIDTH 1
#define	PMA_PMD_1000BASE_KX_CAP_LBN 6
#define	PMA_PMD_1000BASE_KX_CAP_WIDTH 1
#define	PMA_PMD_100BASE_TX_CAP_LBN 7
#define	PMA_PMD_100BASE_TX_CAP_WIDTH 1
#define	PMA_PMD_10BASE_T_CAP_LBN 8
#define	PMA_PMD_10BASE_T_CAP_WIDTH 1

/* channel A-D signal/noise ratio registers */
#define	PMA_PMD_CHANNELA_SNR_REG 0x85
#define	PMA_PMD_CHANNELB_SNR_REG 0x86
#define	PMA_PMD_CHANNELC_SNR_REG 0x87
#define	PMA_PMD_CHANNELD_SNR_REG 0x88
#define	PMA_PMD_CHANNELA_MIN_SNR_REG 0x89
#define	PMA_PMD_CHANNELB_MIN_SNR_REG 0x8a
#define	PMA_PMD_CHANNELC_MIN_SNR_REG 0x8b
#define	PMA_PMD_CHANNELD_MIN_SNR_REG 0x8c
#define	PMA_PMD_SNR_LBN 0
#define	PMA_PMD_SNR_WIDTH 16

/* LASI control register */
#define	PMA_PMD_LASI_CONTROL_REG 0x9002
#define	PMA_PMD_LS_ALARM_EN_LBN 0
#define	PMA_PMD_LS_ALARM_EN_WIDTH 1
#define	PMA_PMD_TX_ALARM_EN_LBN 1
#define	PMA_PMD_TX_ALARM_EN_WIDTH 1
#define	PMA_PMD_RX_ALARM_EN_LBN 2
#define	PMA_PMD_RX_ALARM_EN_WIDTH 1

/* LASI status register */
#define	PMA_PMD_LASI_STATUS_REG 0x9005
#define	PMA_PMD_LS_ALARM_LBN 0
#define	PMA_PMD_LS_ALARM_WIDTH 1
#define	PMA_PMD_TX_ALARM_LBN 1
#define	PMA_PMD_TX_ALARM_WIDTH 1
#define	PMA_PMD_RX_ALARM_LBN 2
#define	PMA_PMD_RX_ALARM_WIDTH 1

/*
 * PCS
 */

/* control register 1 */
#define	PCS_CONTROL1_REG 0x00
#define	PCS_LOW_POWER_EN_LBN 11
#define	PCS_LOW_POWER_EN_WIDTH 1
#define	PCS_LOOPBACK_EN_LBN 14
#define	PCS_LOOPBACK_EN_WIDTH 1
#define	PCS_RESET_LBN 15
#define	PCS_RESET_WIDTH 1

/* status register 1 */
#define	PCS_STATUS1_REG 0x01
#define	PCS_POWER_CAP_LBN 1
#define	PCS_POWER_CAP_WIDTH 1
#define	PCS_LINK_UP_LBN 2
#define	PCS_LINK_UP_WIDTH 1
#define	PCS_FAULT_LBN 7
#define	PCS_FAULT_WIDTH 1

/* control register 2 */
#define	PCS_CONTROL2_REG 0x07
#define	PCS_TYPE_LBN 0
#define	PCS_TYPE_WIDTH 2
#define	PCS_10GBASE_T_DECODE 0x3	/* 11 */
#define	PCS_10GBASE_W_DECODE 0x2	/* 10 */
#define	PCS_10GBASE_X_DECODE 0x1	/* 01 */
#define	PCS_10GBASE_R_DECODE 0x0	/* 00 */

/* 10G status register 2 */
#define	PCS_STATUS2_REG 0x08
#define	PCS_10GBASE_R_CAP_LBN 0
#define	PCS_10GBASE_R_CAP_WIDTH 1
#define	PCS_10GBASE_X_CAP_LBN 1
#define	PCS_10GBASE_X_CAP_WIDTH 1
#define	PCS_10GBASE_W_CAP_LBN 2
#define	PCS_10GBASE_W_CAP_WIDTH 1
#define	PCS_10GBASE_T_CAP_LBN 3
#define	PCS_10GBASE_T_CAP_WIDTH 1
#define	PCS_RX_FAULT_LBN 10
#define	PCS_RX_FAULT_WIDTH 1
#define	PCS_TX_FAULT_LBN 11
#define	PCS_TX_FAULT_WIDTH 1
#define	PCS_RESPONDING_LBN 14
#define	PCS_RESPONDING_WIDTH 2
#define	PCS_RESPONDING_DECODE 0x2	/* 10 */

/* 10G-BASE-T/R status register 2 */
#define	PCS_10GBASE_T_STATUS2_REG 0x21
#define	PCS_10GBASE_R_STATUS2_REG 0x21
#define	PCS_ERR_LBN 0
#define	PCS_ERR_WIDTH 7
#define	PCS_BER_LBN 8
#define	PCS_BER_WIDTH 6
#define	PCS_HIGH_BER_LBN 14
#define	PCS_HIGH_BER_WIDTH 1
#define	PCS_BLOCK_LOCK_LBN 15
#define	PCS_BLOCK_LOCK_WIDTH 1

/*
 * PHY_XS
 */

/* control register 1 */
#define	PHY_XS_CONTROL1_REG 0x00
#define	PHY_XS_LOW_POWER_EN_LBN 11
#define	PHY_XS_LOW_POWER_EN_WIDTH 1
#define	PHY_XS_LOOPBACK_EN_LBN 14
#define	PHY_XS_LOOPBACK_EN_WIDTH 1
#define	PHY_XS_RESET_LBN 15
#define	PHY_XS_RESET_WIDTH 1

/* status register 1 */
#define	PHY_XS_STATUS1_REG 0x01
#define	PHY_XS_POWER_CAP_LBN 1
#define	PHY_XS_POWER_CAP_WIDTH 1
#define	PHY_XS_LINK_UP_LBN 2
#define	PHY_XS_LINK_UP_WIDTH 1
#define	PHY_XS_FAULT_LBN 7
#define	PHY_XS_FAULT_WIDTH 1

/* no control register 2 */

/* status register 2 */
#define	PHY_XS_STATUS2_REG 0x08
#define	PHY_XS_RX_FAULT_LBN 10
#define	PHY_XS_RX_FAULT_WIDTH 1
#define	PHY_XS_TX_FAULT_LBN 11
#define	PHY_XS_TX_FAULT_WIDTH 1
#define	PHY_XS_RESPONDING_LBN 14
#define	PHY_XS_RESPONDING_WIDTH 2
#define	PHY_XS_RESPONDING_DECODE 0x2	/* 10 */

/* lane status register */
#define	PHY_XS_LANE_STATUS_REG 0x18
#define	PHY_XS_ALIGNED_LBN 12
#define	PHY_XS_ALIGNED_WIDTH 1
#define	PHY_XS_LANE0_SYNC_LBN 0
#define	PHY_XS_LANE0_SYNC_WIDTH 1
#define	PHY_XS_LANE1_SYNC_LBN 1
#define	PHY_XS_LANE1_SYNC_WIDTH 1
#define	PHY_XS_LANE2_SYNC_LBN 2
#define	PHY_XS_LANE2_SYNC_WIDTH 1
#define	PHY_XS_LANE3_SYNC_LBN 3
#define	PHY_XS_LANE3_SYNC_WIDTH 1

/*
 * DTE_XS
 */

/* control register 1 */
#define	DTE_XS_CONTROL1_REG 0x00
#define	DTE_XS_LOW_POWER_EN_LBN 11
#define	DTE_XS_LOW_POWER_EN_WIDTH 1
#define	DTE_XS_LOOPBACK_EN_LBN 14
#define	DTE_XS_LOOPBACK_EN_WIDTH 1
#define	DTE_XS_RESET_LBN 15
#define	DTE_XS_RESET_WIDTH 1

/* status register 1 */
#define	DTE_XS_STATUS1_REG 0x01
#define	DTE_XS_POWER_CAP_LBN 1
#define	DTE_XS_POWER_CAP_WIDTH 1
#define	DTE_XS_LINK_UP_LBN 2
#define	DTE_XS_LINK_UP_WIDTH 1
#define	DTE_XS_FAULT_LBN 7
#define	DTE_XS_FAULT_WIDTH 1

/* no control register 2 */

/* status register 2 */
#define	DTE_XS_STATUS2_REG 0x08
#define	DTE_XS_RX_FAULT_LBN 10
#define	DTE_XS_RX_FAULT_WIDTH 1
#define	DTE_XS_TX_FAULT_LBN 11
#define	DTE_XS_TX_FAULT_WIDTH 1
#define	DTE_XS_RESPONDING_LBN 14
#define	DTE_XS_RESPONDING_WIDTH 2
#define	DTE_XS_RESPONDING_DECODE 0x2	/* 10 */

/* lane status register */
#define	DTE_XS_LANE_STATUS_REG 0x18
#define	DTE_XS_ALIGNED_LBN 12
#define	DTE_XS_ALIGNED_WIDTH 1
#define	DTE_XS_LANE0_SYNC_LBN 0
#define	DTE_XS_LANE0_SYNC_WIDTH 1
#define	DTE_XS_LANE1_SYNC_LBN 1
#define	DTE_XS_LANE1_SYNC_WIDTH 1
#define	DTE_XS_LANE2_SYNC_LBN 2
#define	DTE_XS_LANE2_SYNC_WIDTH 1
#define	DTE_XS_LANE3_SYNC_LBN 3
#define	DTE_XS_LANE3_SYNC_WIDTH 1

/*
 * AN
 */

/* control register 1 */
#define	AN_CONTROL1_REG	0x00
#define	AN_RESTART_LBN 9
#define	AN_RESTART_WIDTH 1
#define	AN_ENABLE_LBN 12
#define	AN_ENABLE_WIDTH 1
#define	AN_RESET_LBN 15
#define	AN_RESET_WIDTH 1

/* status register 1 */
#define	AN_STATUS1_REG 0x01
#define	AN_LP_CAP_LBN 0
#define	AN_LP_CAP_WIDTH 1
#define	AN_LINK_UP_LBN 2
#define	AN_LINK_UP_WIDTH 1
#define	AN_FAULT_LBN 4
#define	AN_FAULT_WIDTH 1
#define	AN_COMPLETE_LBN 5
#define	AN_COMPLETE_WIDTH 1
#define	AN_PAGE_RCVD_LBN 6
#define	AN_PAGE_RCVD_WIDTH 1
#define	AN_XNP_CAP_LBN 7
#define	AN_XNP_CAP_WIDTH 1

/* advertised capabilities register */
#define	AN_ADV_BP_CAP_REG 0x10
#define	AN_ADV_TA_10BASE_T_LBN 5
#define	AN_ADV_TA_10BASE_T_WIDTH 1
#define	AN_ADV_TA_10BASE_T_FDX_LBN 6
#define	AN_ADV_TA_10BASE_T_FDX_WIDTH 1
#define	AN_ADV_TA_100BASE_TX_LBN 7
#define	AN_ADV_TA_100BASE_TX_WIDTH 1
#define	AN_ADV_TA_100BASE_TX_FDX_LBN 8
#define	AN_ADV_TA_100BASE_TX_FDX_WIDTH 1
#define	AN_ADV_TA_100BASE_T4_LBN 9
#define	AN_ADV_TA_100BASE_T4_WIDTH 1
#define	AN_ADV_TA_PAUSE_LBN 10
#define	AN_ADV_TA_PAUSE_WIDTH 1
#define	AN_ADV_TA_ASM_DIR_LBN 11
#define	AN_ADV_TA_ASM_DIR_WIDTH 1

/* link partner base page capabilities register */
#define	AN_LP_BP_CAP_REG 0x13
#define	AN_LP_TA_10BASE_T_LBN 5
#define	AN_LP_TA_10BASE_T_WIDTH 1
#define	AN_LP_TA_10BASE_T_FDX_LBN 6
#define	AN_LP_TA_10BASE_T_FDX_WIDTH 1
#define	AN_LP_TA_100BASE_TX_LBN 7
#define	AN_LP_TA_100BASE_TX_WIDTH 1
#define	AN_LP_TA_100BASE_TX_FDX_LBN 8
#define	AN_LP_TA_100BASE_TX_FDX_WIDTH 1
#define	AN_LP_TA_100BASE_T4_LBN 9
#define	AN_LP_TA_100BASE_T4_WIDTH 1
#define	AN_LP_TA_PAUSE_LBN 10
#define	AN_LP_TA_PAUSE_WIDTH 1
#define	AN_LP_TA_ASM_DIR_LBN 11
#define	AN_LP_TA_ASM_DIR_WIDTH 1

/* 10GBASE-T control register */
#define	AN_10G_BASE_T_CONTROL_REG 0x20
#define	AN_10G_BASE_T_ADV_LBN 12
#define	AN_10G_BASE_T_ADV_WIDTH 1

/* 10GBASE-T status register */
#define	AN_10G_BASE_T_STATUS_REG 0x21
#define	AN_10G_BASE_T_LP_LBN 11
#define	AN_10G_BASE_T_LP_WIDTH 1
#define	AN_CONFIG_FAULT_LBN 15
#define	AN_CONFIG_FAULT_WIDTH 1
#define	AN_MASTER_LBN 14
#define	AN_MASTER_WIDTH 1
#define	AN_LOCAL_RX_OK_LBN 13
#define	AN_LOCAL_RX_OK_WIDTH 1
#define	AN_REMOTE_RX_OK_LBN 12
#define	AN_REMOTE_RX_OK_WIDTH 1

extern	__checkReturn	int
xphy_pkg_verify(
	__in		efx_nic_t *enp,
	__in		uint8_t port,
	__in		uint32_t mask);

extern	__checkReturn	int
xphy_pkg_wait(
	__in		efx_nic_t *enp,
	__in		uint8_t port,
	__in		uint32_t mask);

extern	__checkReturn	int
xphy_mmd_oui_get(
	__in		efx_nic_t *enp,
	__in		uint8_t port,
	__in		uint8_t mmd,
	__out		uint32_t *ouip);

extern	__checkReturn	int
xphy_mmd_check(
	__in		efx_nic_t *enp,
	__in		uint8_t port,
	__in		uint8_t mmd,
	__out		boolean_t *upp);

extern	__checkReturn	int
xphy_mmd_fault(
	__in		efx_nic_t *enp,
	__in		uint8_t port,
	__out		boolean_t *upp);

extern	__checkReturn	int
xphy_mmd_loopback_set(
	__in		efx_nic_t *enp,
	__in		uint8_t port,
	__in		uint8_t mmd,
	__in		boolean_t on);

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_XPHY_H */
