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

#ifndef	_SYS_QT2022C2_IMPL_H
#define	_SYS_QT2022C2_IMPL_H

#ifdef	__cplusplus
extern "C" {
#endif

#if EFSYS_OPT_PHY_QT2022C2

#define	QT2022C2_MMD_MASK						\
	    ((1 << PMA_PMD_MMD) |					\
	    (1 << PCS_MMD) |						\
	    (1 << PHY_XS_MMD))

#define	PMA_PMD_LED1_REG 0xd006	/* Green */
#define	PMA_PMD_LED2_REG 0xd007	/* Amber */
#define	PMA_PMD_LED3_REG 0xd008 /* Red */

#define	PMA_PMD_LED_CFG_LBN 0
#define	PMA_PMD_LED_CFG_WIDTH 3
#define	LED_CFG_LS_DECODE 0x1
#define	LED_CFG_LA_DECODE 0x2
#define	LED_CFG_LSA_DECODE 0x3
#define	LED_CFG_OFF_DECODE 0x4
#define	LED_CFG_ON_DECODE 0x5
#define	PMA_PMD_LED_PATH_LBN 3
#define	PMA_PMD_LED_PATH_WIDTH 1
#define	LED_PATH_TX_DECODE 0x0
#define	LED_PATH_RX_DECODE 0x1

#define	PHY_XS_VENDOR0_REG 0xc000
#define	XAUI_SYSTEM_LOOPBACK_LBN 14
#define	XAUI_SYSTEM_LOOPBACK_WIDTH 1

#endif	/* EFSYS_OPT_PHY_QT2022C2 */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_QT2022C2_IMPL_H */
