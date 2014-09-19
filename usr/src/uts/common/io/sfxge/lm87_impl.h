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

#ifndef	_SYS_LM87_IMPL_H
#define	_SYS_LM87_IMPL_H

#ifdef	__cplusplus
extern "C" {
#endif

#if EFSYS_OPT_MON_LM87

#define	TEST_REG 0x15
#define	SHUTDOWN_LBN 0
#define	SHUTDOWN_WIDTH 1

#define	CHANNEL_MODE_REG 0x16
#define	FAN1_AIN1_LBN 0
#define	FAN1_AIN1_WIDTH 1
#define	FAN2_AIN2_LBN 1
#define	FAN2_AIN2_WIDTH 1

#define	CONFIG1_REG 0x40
#define	START_LBN 0
#define	START_WIDTH 1
#define	INT_EN_LBN 1
#define	INT_EN_WIDTH 1
#define	INIT_LBN 7
#define	INIT_WIDTH 1

#define	INTERRUPT_MASK1_REG 0x43
#define	INTERRUPT_MASK2_REG 0x44

#define	VALUE_2_5V_REG 0x20
#define	VALUE_VCCP1_REG 0x21
#define	VALUE_VCC_REG 0x22
#define	VALUE_5V_REG 0x23
#define	VALUE_12V_REG 0x24
#define	VALUE_VCCP2_REG 0x25
#define	VALUE_EXT_TEMP_REG 0x26
#define	VALUE_INT_TEMP_REG 0x27
#define	VALUE_AIN1_REG 0x28
#define	VALUE_AIN2_REG 0x29

#define	ID_REG 0x3e
#define	ID_DECODE 0x02

#define	REV_REG 0x3f
#define	REV_DECODE 0x06

#endif	/* EFSYS_OPT_MON_LM87 */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_LM87_IMPL_H */
