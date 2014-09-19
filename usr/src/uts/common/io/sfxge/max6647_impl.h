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

#ifndef	_SYS_MAX6647_IMPL_H
#define	_SYS_MAX6647_IMPL_H

#ifdef	__cplusplus
extern "C" {
#endif

#if EFSYS_OPT_MON_MAX6647 || EFSYS_OPT_PHY_SFX7101

#define	RLTS_REG 0x00

#define	RRTE_REG 0x01

#define	RSL_REG 0x02
#define	BUSY_LBN 7
#define	BUSY_WIDTH 1
#define	LHIGH_LBN 6
#define	LHIGH_WIDTH 1
#define	LLOW_LBN 5
#define	LLOW_WIDTH 1
#define	RHIGH_LBN 4
#define	RHIGH_WIDTH 1
#define	RLOW_LBN 3
#define	RLOW_WIDTH 1
#define	FAULT_LBN 2
#define	FAULT_WIDTH 1
#define	EOT_LBN 1
#define	EOT_WIDTH 1
#define	IOT_LBN 0
#define	IOT_WIDTH 1

#define	RCL_REG 0x03
#define	MASK_LBN 7
#define	MASK_WIDTH 1
#define	NRUN_LBN 6
#define	NRUN_WIDTH 1

#define	RCRA_REG 0x04

#define	RLHN_REG 0x05

#define	RLLI_REG 0x06

#define	RRHI_REG 0x07

#define	RRLS_REG 0x08

#define	WCA_REG 0x09

#define	WCRW_REG 0x0a

#define	WLHO_REG 0x0b

#define	WRHA_REG 0x0c

#define	WRLN_REG 0x0e

#define	OSHT_REG 0x0f

#define	REET_REG 0x10

#define	RIET_REG 0x11

#define	RWOE_REG 0x19

#define	RWOI_REG 0x20

#define	HYS_REG 0x21

#define	QUEUE_REG 0x22

#define	MFID_REG 0xfe
#define	MFID_DECODE 0x4d

#define	REVID_REG 0xff
#define	REVID_DECODE 0x59

#endif	/* EFSYS_OPT_MON_MAX6647 || EFSYS_OPT_PHY_SFX7101 */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MAX6647_IMPL_H */
