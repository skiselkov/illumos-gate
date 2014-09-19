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

#ifndef	_SYS_NULLPHY_IMPL_H
#define	_SYS_NULLPHY_IMPL_H

#ifdef	__cplusplus
extern "C" {
#endif

#if EFSYS_OPT_PHY_NULL

/* IO expender */
#define	PCF8575	0x20

#define	PORT0_EXTLOOP_LBN 0
#define	PORT0_EXTLOOP_WIDTH 1
#define	PORT1_EXTLOOP_LBN 1
#define	PORT1_EXTLOOP_WIDTH 1
#define	HOSTPORT_LOOP_LBN 2
#define	HOSTPORT_LOOP_WIDTH 1
#define	BCAST_LBN 3
#define	BCAST_WIDTH 1
#define	PORT0_EQ_LBN 4
#define	PORT0_EQ_WIDTH 1
#define	PORT1_EQ_LBN 5
#define	PORT1_EQ_WIDTH 1
#define	HOSTPORT_EQ_LBN 6
#define	HOSTPORT_EQ_WIDTH 1
#define	PORTSEL_LBN 7
#define	PORTSEL_WIDTH 1
#define	PORT0_PRE_LBN 8
#define	PORT0_PRE_WIDTH 2
#define	PORT1_PRE_LBN 10
#define	PORT1_PRE_WIDTH 2
#define	HOSTPORT_PRE_LBN 12
#define	HOSTPORT_PRE_WIDTH 2
#define	CX4uC_RESET_LBN 15
#define	CX4uC_RESET_WIDTH 1

#endif	/* EFSYS_OPT_PHY_NULL */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_NULLPHY_IMPL_H */
