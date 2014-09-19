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

#ifndef	_SYS_TXC43128_IMPL_H
#define	_SYS_TXC43128_IMPL_H

#ifdef	__cplusplus
extern "C" {
#endif

#if EFSYS_OPT_PHY_TXC43128

#define	TXC43128_MMD_MASK						\
	((1 << PMA_PMD_MMD) |						\
	(1 << PCS_MMD) |						\
	(1 << PHY_XS_MMD))

#define	SIGDET	0x000a
#define	RX0SIGDET_LBN 1
#define	RX0SIGDET_WIDTH 1
#define	RX1SIGDET_LBN 2
#define	RX1SIGDET_WIDTH 1
#define	RX2SIGDET_LBN 3
#define	RX2SIGDET_WIDTH 1
#define	RX3SIGDET_LBN 4
#define	RX3SIGDET_WIDTH 1

#define	GLCMD 0xc004
#define	LMTSWRST_LBN 14
#define	LMTSWRST_WIDTH 1

#define	ATXPRE0 0xc043
#define	ATXPRE1 0xc044
#define	TXPRE02_LBN 3
#define	TXPRE02_WIDTH 5
#define	TXPRE13_LBN 11
#define	TXPRE13_WIDTH 5

#define	ATXAMP0 0xc041
#define	ATXAMP1 0xc042
#define	TXAMP02_LBN 3
#define	TXAMP02_WIDTH 5
#define	TXAMP13_LBN 11
#define	TXAMP13_WIDTH 5

#define	BCTL 0xc280
#define	BSTRT_LBN 15
#define	BSTRT_WIDTH 1
#define	BSTOP_LBN 14
#define	BSTOP_WIDTH 1
#define	BSTEN_LBN 13
#define	BSTEN_WIDTH 1
#define	B10EN_LBN 12
#define	B10EN_WIDTH 1
#define	BTYPE_LBN 10
#define	BTYPE_WIDTH 2
#define	TSDET_DECODE 0
#define	CRPAT_DECODE 1
#define	CJPAT_DECODE 2
#define	TSRND_DECODE 3

#define	BTXFRMCNT 0xc281
#define	BRX0FRMCNT 0xc282
#define	BRX1FRMCNT 0xc283
#define	BRX2FRMCNT 0xc284
#define	BRX3FRMCNT 0xc285
#define	BRX0ERRCNT 0xc286
#define	BRX1ERRCNT 0xc287
#define	BRX2ERRCNT 0xc288
#define	BRX3ERRCNT 0xc289

#define	MNCTL 0xc340
#define	MRST_LBN 15
#define	MRST_WIDTH 1
#define	ALG2TXALED_LBN 14
#define	ALG2TXALED_WIDTH 1
#define	ALG2RXALED_LBN 13
#define	ALG2RXALED_WIDTH 1

#define	PIOCFG 0xc345
#define	PIO15FNC_LBN 15
#define	PIO15FNC_WIDTH 1
#define	PIO14FNC_LBN 14
#define	PIO14FNC_WIDTH 1
#define	PIO13FNC_LBN 13
#define	PIO13FNC_WIDTH 1
#define	PIO12FNC_LBN 12
#define	PIO12FNC_WIDTH 1
#define	PIO11FNC_LBN 11
#define	PIO11FNC_WIDTH 1
#define	PIO10FNC_LBN 10
#define	PIO10FNC_WIDTH 1
#define	PIO9FNC_LBN 9
#define	PIO9FNC_WIDTH 1
#define	PIO8FNC_LBN 8
#define	PIO8FNC_WIDTH 1
#define	PIOFNC_LED_DECODE 0
#define	PIOFNC_GPIO_DECODE 1

#define	PIODO 0xc346
#define	PIO15OUT_LBN 15
#define	PIO15OUT_WIDTH 1
#define	PIO14OUT_LBN 14
#define	PIO14OUT_WIDTH 1
#define	PIO13OUT_LBN 13
#define	PIO13OUT_WIDTH 1
#define	PIO12OUT_LBN 12
#define	PIO12OUT_WIDTH 1
#define	PIO11OUT_LBN 11
#define	PIO11OUT_WIDTH 1
#define	PIO10OUT_LBN 10
#define	PIO10OUT_WIDTH 1
#define	PIO9OUT_LBN 9
#define	PIO9OUT_WIDTH 1
#define	PIO8OUT_LBN 8
#define	PIO8OUT_WIDTH 1

#define	PIODIR 0xc348
#define	PIO15DIR_LBN 15
#define	PIO15DIR_WIDTH 1
#define	PIO14DIR_LBN 14
#define	PIO14DIR_WIDTH 1
#define	PIO13DIR_LBN 13
#define	PIO13DIR_WIDTH 1
#define	PIO12DIR_LBN 12
#define	PIO12DIR_WIDTH 1
#define	PIO11DIR_LBN 11
#define	PIO11DIR_WIDTH 1
#define	PIO10DIR_LBN 10
#define	PIO10DIR_WIDTH 1
#define	PIO9DIR_LBN 9
#define	PIO9DIR_WIDTH 1
#define	PIO8DIR_LBN 8
#define	PIO8DIR_WIDTH 1
#define	PIODIR_IN_DECODE 0
#define	PIODIR_OUT_DECODE 1

#define	MNDBLCFG 0xc34f
#define	PXS8BLPBK_LBN 11
#define	PXS8BLPBK_WIDTH 1
#define	LNALPBK_LBN 10
#define	LNALPBK_WIDTH 1

#endif	/* EFSYS_OPT_PHY_TXC43128 */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_TXC43128_IMPL_H */
