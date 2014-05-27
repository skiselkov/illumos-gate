/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright (c) 2009 Intel Corporation
 * All Rights Reserved.
 */
/*
 * Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
/*
 * Copyright 2014 by Saso Kiselkov. All rights reserved.
 */

/*
 * Accelerated GHASH implementation with Intel PCLMULQDQ-NI
 * instructions.  This file contains an accelerated
 * Galois Field Multiplication implementation.
 *
 * PCLMULQDQ is used to accelerate the most time-consuming part of GHASH,
 * carry-less multiplication. More information about PCLMULQDQ can be
 * found at:
 * http://software.intel.com/en-us/articles/
 * carry-less-multiplication-and-its-usage-for-computing-the-gcm-mode/
 *
 */

/*
 * ====================================================================
 * OpenSolaris OS modifications
 *
 * This source originates as file galois_hash_asm.c from
 * Intel Corporation dated September 21, 2009.
 *
 * This OpenSolaris version has these major changes from the original source:
 *
 * 1. Added OpenSolaris ENTRY_NP/SET_SIZE macros from
 * /usr/include/sys/asm_linkage.h, lint(1B) guards, and a dummy C function
 * definition for lint.
 *
 * 2. Formatted code, added comments, and added #includes and #defines.
 *
 * 3. If bit CR0.TS is set, clear and set the TS bit, after and before
 * calling kpreempt_disable() and kpreempt_enable().
 * If the TS bit is not set, Save and restore %xmm registers at the beginning
 * and end of function calls (%xmm* registers are not saved and restored by
 * during kernel thread preemption).
 *
 * 4. Removed code to perform hashing.  This is already done with C macro
 * GHASH in gcm.c.  For better performance, this removed code should be
 * reintegrated in the future to replace the C GHASH macro.
 *
 * 5. Added code to byte swap 16-byte input and output.
 *
 * 6. Folded in comments from the original C source with embedded assembly
 * (SB_w_shift_xor.c)
 *
 * 7. Renamed function and reordered parameters to match OpenSolaris:
 * Intel interface:
 *	void galois_hash_asm(unsigned char *hk, unsigned char *s,
 *		unsigned char *d, int length)
 * OpenSolaris OS interface:
 *	void gcm_mul_pclmulqdq(uint64_t *x_in, uint64_t *y, uint64_t *res);
 * ====================================================================
 */


#if defined(lint) || defined(__lint)

#include <sys/types.h>

/* ARGSUSED */
void
gcm_mul_pclmulqdq(uint64_t *x_in, uint64_t *y, uint64_t *res) {
}

#ifdef	_KERNEL
/*ARGSUSED*/
void
gcm_intel_save(void *savestate)
{
}

/*ARGSUSED*/
void
gcm_accel_restore(void *savestate)
{
}
#endif	/* _KERNEL */

#else	/* lint */

#include <sys/asm_linkage.h>
#include <sys/controlregs.h>
#ifdef _KERNEL
#include <sys/machprivregs.h>
#endif

#ifdef _KERNEL
	/*
	 * Note: the CLTS macro clobbers P2 (%rsi) under i86xpv.  That is,
	 * it calls HYPERVISOR_fpu_taskswitch() which modifies %rsi when it
	 * uses it to pass P2 to syscall.
	 * This also occurs with the STTS macro, but we don't care if
	 * P2 (%rsi) is modified just before function exit.
	 * The CLTS and STTS macros push and pop P1 (%rdi) already.
	 */
#ifdef __xpv
#define	PROTECTED_CLTS \
	push	%rsi; \
	CLTS; \
	pop	%rsi
#else
#define	PROTECTED_CLTS \
	CLTS
#endif	/* __xpv */
#endif	/* _KERNEL */

.text
.align XMM_ALIGN
/*
 * Use this mask to byte-swap a 16-byte integer with the pshufb instruction:
 * static uint8_t byte_swap16_mask[] = {
 *	15, 14, 13, 12, 11, 10, 9, 8, 7, 6 ,5, 4, 3, 2, 1, 0 };
 */
.Lbyte_swap16_mask:
	.byte	15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0

#ifdef	_KERNEL
/*
 * void gcm_intel_save(void *savestate)
 *
 * Saves the XMM0--XMM14 registers and CR0 to a temporary location pointed
 * to in the first argument and clears TS in CR0. This must be invoked before
 * executing accelerated GCM computations inside the kernel (and kernel
 * thread preemption must be disabled as well). The memory region to which
 * all state is saved must be at least 16x 128-bit + 64-bit long and must
 * be 128-bit aligned.
 */
ENTRY_NP(gcm_accel_save)
	movq	%cr0, %rax
	movq	%rax, 0x100(%rdi)
	testq	$CR0_TS, %rax
	jnz	1f
	/* FPU is in use, save registers */
	movaps	%xmm0, 0x00(%rdi)
	movaps	%xmm1, 0x10(%rdi)
	movaps	%xmm2, 0x20(%rdi)
	movaps	%xmm3, 0x30(%rdi)
	movaps	%xmm4, 0x40(%rdi)
	movaps	%xmm5, 0x50(%rdi)
	movaps	%xmm6, 0x60(%rdi)
	movaps	%xmm7, 0x70(%rdi)
	movaps	%xmm8, 0x80(%rdi)
	movaps	%xmm9, 0x90(%rdi)
	movaps	%xmm10, 0xa0(%rdi)
	movaps	%xmm11, 0xb0(%rdi)
	movaps	%xmm12, 0xc0(%rdi)
	movaps	%xmm13, 0xd0(%rdi)
	movaps	%xmm14, 0xe0(%rdi)
	movaps	%xmm15, 0xf0(%rdi)
	ret
1:
	PROTECTED_CLTS
	ret
	SET_SIZE(gcm_accel_save)

/*
 * void gcm_accel_restore(void *savestate)
 *
 * Restores the saved XMM and CR0.TS state from aes_accel_save.
 */
ENTRY_NP(gcm_accel_restore)
	movq	0x100(%rdi), %rax
	testq	$CR0_TS, %rax
	jnz	1f
	movaps	0x00(%rdi), %xmm0
	movaps	0x10(%rdi), %xmm1
	movaps	0x20(%rdi), %xmm2
	movaps	0x30(%rdi), %xmm3
	movaps	0x40(%rdi), %xmm4
	movaps	0x50(%rdi), %xmm5
	movaps	0x60(%rdi), %xmm6
	movaps	0x70(%rdi), %xmm7
	movaps	0x80(%rdi), %xmm8
	movaps	0x90(%rdi), %xmm9
	movaps	0xa0(%rdi), %xmm10
	movaps	0xb0(%rdi), %xmm11
	movaps	0xc0(%rdi), %xmm12
	movaps	0xd0(%rdi), %xmm13
	movaps	0xe0(%rdi), %xmm14
	movaps	0xf0(%rdi), %xmm15
	ret
1:
	STTS(%rax)
	ret
	SET_SIZE(gcm_accel_restore)

#endif	/* _KERNEL */

/*
 * void gcm_mul_pclmulqdq(uint64_t *x_in, uint64_t *y, uint64_t *res);
 *
 * Perform a carry-less multiplication (that is, use XOR instead of the
 * multiply operator) on P1 and P2 and place the result in P3.
 *
 * Byte swap the input and the output.
 *
 * Note: x_in, y, and res all point to a block of 16-byte numbers
 * (an array of two 64-bit integers).
 *
 * Note2: For kernel code, caller is responsible for bracketing this call with
 * disabling kernel thread preemption and calling gcm_accel_save/restore().
 *
 * Note3: Original Intel definition:
 * void galois_hash_asm(unsigned char *hk, unsigned char *s,
 *	unsigned char *d, int length)
 *
 * Note4: Register/parameter mapping:
 * Intel:
 *	Parameter 1: %rcx (copied to %xmm0)	hk or x_in
 *	Parameter 2: %rdx (copied to %xmm1)	s or y
 *	Parameter 3: %rdi (result)		d or res
 * OpenSolaris:
 *	Parameter 1: %rdi (copied to %xmm0)	x_in
 *	Parameter 2: %rsi (copied to %xmm1)	y
 *	Parameter 3: %rdx (result)		res
 */

ENTRY_NP(gcm_mul_pclmulqdq)
	//
	// Copy Parameters
	//
	movdqu	(%rdi), %xmm0	// P1
	movdqu	(%rsi), %xmm1	// P2

	//
	// Byte swap 16-byte input
	//
	lea	.Lbyte_swap16_mask(%rip), %rax
	movaps	(%rax), %xmm10
	pshufb	%xmm10, %xmm0
	pshufb	%xmm10, %xmm1


	//
	// Multiply with the hash key
	//
	movdqu	%xmm0, %xmm3
	pclmulqdq $0, %xmm1, %xmm3	// xmm3 holds a0*b0

	movdqu	%xmm0, %xmm4
	pclmulqdq $16, %xmm1, %xmm4	// xmm4 holds a0*b1

	movdqu	%xmm0, %xmm5
	pclmulqdq $1, %xmm1, %xmm5	// xmm5 holds a1*b0
	movdqu	%xmm0, %xmm6
	pclmulqdq $17, %xmm1, %xmm6	// xmm6 holds a1*b1

	pxor	%xmm5, %xmm4	// xmm4 holds a0*b1 + a1*b0

	movdqu	%xmm4, %xmm5	// move the contents of xmm4 to xmm5
	psrldq	$8, %xmm4	// shift by xmm4 64 bits to the right
	pslldq	$8, %xmm5	// shift by xmm5 64 bits to the left
	pxor	%xmm5, %xmm3
	pxor	%xmm4, %xmm6	// Register pair <xmm6:xmm3> holds the result
				// of the carry-less multiplication of
				// xmm0 by xmm1.

	// We shift the result of the multiplication by one bit position
	// to the left to cope for the fact that the bits are reversed.
	movdqu	%xmm3, %xmm7
	movdqu	%xmm6, %xmm8
	pslld	$1, %xmm3
	pslld	$1, %xmm6
	psrld	$31, %xmm7
	psrld	$31, %xmm8
	movdqu	%xmm7, %xmm9
	pslldq	$4, %xmm8
	pslldq	$4, %xmm7
	psrldq	$12, %xmm9
	por	%xmm7, %xmm3
	por	%xmm8, %xmm6
	por	%xmm9, %xmm6

	//
	// First phase of the reduction
	//
	// Move xmm3 into xmm7, xmm8, xmm9 in order to perform the shifts
	// independently.
	movdqu	%xmm3, %xmm7
	movdqu	%xmm3, %xmm8
	movdqu	%xmm3, %xmm9
	pslld	$31, %xmm7	// packed right shift shifting << 31
	pslld	$30, %xmm8	// packed right shift shifting << 30
	pslld	$25, %xmm9	// packed right shift shifting << 25
	pxor	%xmm8, %xmm7	// xor the shifted versions
	pxor	%xmm9, %xmm7
	movdqu	%xmm7, %xmm8
	pslldq	$12, %xmm7
	psrldq	$4, %xmm8
	pxor	%xmm7, %xmm3	// first phase of the reduction complete

	//
	// Second phase of the reduction
	//
	// Make 3 copies of xmm3 in xmm2, xmm4, xmm5 for doing these
	// shift operations.
	movdqu	%xmm3, %xmm2
	movdqu	%xmm3, %xmm4	// packed left shifting >> 1
	movdqu	%xmm3, %xmm5
	psrld	$1, %xmm2
	psrld	$2, %xmm4	// packed left shifting >> 2
	psrld	$7, %xmm5	// packed left shifting >> 7
	pxor	%xmm4, %xmm2	// xor the shifted versions
	pxor	%xmm5, %xmm2
	pxor	%xmm8, %xmm2
	pxor	%xmm2, %xmm3
	pxor	%xmm3, %xmm6	// the result is in xmm6

	//
	// Byte swap 16-byte result
	//
	pshufb	%xmm10, %xmm6	// %xmm10 has the swap mask

	//
	// Store the result
	//
	movdqu	%xmm6, (%rdx)	// P3


	//
	// Cleanup and Return
	//
	ret
	SET_SIZE(gcm_mul_pclmulqdq)

#endif	/* lint || __lint */
