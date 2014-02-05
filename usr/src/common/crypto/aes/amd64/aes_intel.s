/*
 * ====================================================================
 * Written by Intel Corporation for the OpenSSL project to add support
 * for Intel AES-NI instructions. Rights for redistribution and usage
 * in source and binary forms are granted according to the OpenSSL
 * license.
 *
 *   Author: Huang Ying <ying.huang at intel dot com>
 *           Vinodh Gopal <vinodh.gopal at intel dot com>
 *           Kahraman Akdemir
 *
 * Intel AES-NI is a new set of Single Instruction Multiple Data (SIMD)
 * instructions that are going to be introduced in the next generation
 * of Intel processor, as of 2009. These instructions enable fast and
 * secure data encryption and decryption, using the Advanced Encryption
 * Standard (AES), defined by FIPS Publication number 197. The
 * architecture introduces six instructions that offer full hardware
 * support for AES. Four of them support high performance data
 * encryption and decryption, and the other two instructions support
 * the AES key expansion procedure.
 * ====================================================================
 */

/*
 * ====================================================================
 * Copyright (c) 1998-2008 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 */

/*
 * ====================================================================
 * OpenSolaris OS modifications
 *
 * This source originates as files aes-intel.S and eng_aesni_asm.pl, in
 * patches sent sent Dec. 9, 2008 and Dec. 24, 2008, respectively, by
 * Huang Ying of Intel to the openssl-dev mailing list under the subject
 * of "Add support to Intel AES-NI instruction set for x86_64 platform".
 *
 * This OpenSolaris version has these major changes from the original source:
 *
 * 1. Added OpenSolaris ENTRY_NP/SET_SIZE macros from
 * /usr/include/sys/asm_linkage.h, lint(1B) guards, and dummy C function
 * definitions for lint.
 *
 * 2. Formatted code, added comments, and added #includes and #defines.
 *
 * 3. If bit CR0.TS is set, clear and set the TS bit, after and before
 * calling kpreempt_disable() and kpreempt_enable().
 * If the TS bit is not set, Save and restore %xmm registers at the beginning
 * and end of function calls (%xmm* registers are not saved and restored by
 * during kernel thread preemption).
 *
 * 4. Renamed functions, reordered parameters, and changed return value
 * to match OpenSolaris:
 *
 * OpenSSL interface:
 *	int intel_AES_set_encrypt_key(const unsigned char *userKey,
 *		const int bits, AES_KEY *key);
 *	int intel_AES_set_decrypt_key(const unsigned char *userKey,
 *		const int bits, AES_KEY *key);
 *	Return values for above are non-zero on error, 0 on success.
 *
 *	void intel_AES_encrypt(const unsigned char *in, unsigned char *out,
 *		const AES_KEY *key);
 *	void intel_AES_decrypt(const unsigned char *in, unsigned char *out,
 *		const AES_KEY *key);
 *	typedef struct aes_key_st {
 *		unsigned int	rd_key[4 *(AES_MAXNR + 1)];
 *		int		rounds;
 *		unsigned int	pad[3];
 *	} AES_KEY;
 * Note: AES_LONG is undefined (that is, Intel uses 32-bit key schedules
 * (ks32) instead of 64-bit (ks64).
 * Number of rounds (aka round count) is at offset 240 of AES_KEY.
 *
 * OpenSolaris OS interface (#ifdefs removed for readability):
 *	int rijndael_key_setup_dec_intel(uint32_t rk[],
 *		const uint32_t cipherKey[], uint64_t keyBits);
 *	int rijndael_key_setup_enc_intel(uint32_t rk[],
 *		const uint32_t cipherKey[], uint64_t keyBits);
 *	Return values for above are 0 on error, number of rounds on success.
 *
 *	void aes_encrypt_intel(const aes_ks_t *ks, int Nr,
 *		const uint32_t pt[4], uint32_t ct[4]);
 *	void aes_decrypt_intel(const aes_ks_t *ks, int Nr,
 *		const uint32_t pt[4], uint32_t ct[4]);
 *	typedef union {uint64_t ks64[(MAX_AES_NR + 1) * 4];
 *		 uint32_t ks32[(MAX_AES_NR + 1) * 4]; } aes_ks_t;
 *
 *	typedef union {
 *		uint32_t	ks32[((MAX_AES_NR) + 1) * (MAX_AES_NB)];
 *	} aes_ks_t;
 *	typedef struct aes_key {
 *		aes_ks_t	encr_ks, decr_ks;
 *		long double	align128;
 *		int		flags, nr, type;
 *	} aes_key_t;
 *
 * Note: ks is the AES key schedule, Nr is number of rounds, pt is plain text,
 * ct is crypto text, and MAX_AES_NR is 14.
 * For the x86 64-bit architecture, OpenSolaris OS uses ks32 instead of ks64.
 *
 * Note2: aes_ks_t must be aligned on a 0 mod 128 byte boundary.
 *
 * ====================================================================
 */
/*
 * Copyright 2014 by Saso Kiselkov. All rights reserved.
 */

#if defined(lint) || defined(__lint)

#include <sys/types.h>

/* ARGSUSED */
void
aes_encrypt_intel(const uint32_t rk[], int Nr, const uint32_t pt[4],
    uint32_t ct[4]) {
}
/* ARGSUSED */
void
aes_decrypt_intel(const uint32_t rk[], int Nr, const uint32_t ct[4],
    uint32_t pt[4]) {
}
/* ARGSUSED */
int
rijndael_key_setup_enc_intel(uint32_t rk[], const uint32_t cipherKey[],
    uint64_t keyBits) {
	return (0);
}
/* ARGSUSED */
int
rijndael_key_setup_dec_intel(uint32_t rk[], const uint32_t cipherKey[],
   uint64_t keyBits) {
	return (0);
}


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

#define	CLEAR_TS_OR_PUSH_XMM0_XMM1(tmpreg) \
	push	%rbp; \
	mov	%rsp, %rbp; \
	movq	%cr0, tmpreg; \
	testq	$CR0_TS, tmpreg; \
	jnz	1f; \
	and	$-XMM_ALIGN, %rsp; \
	sub	$[XMM_SIZE * 2], %rsp; \
	movaps	%xmm0, 16(%rsp); \
	movaps	%xmm1, (%rsp); \
	jmp	2f; \
1: \
	PROTECTED_CLTS; \
2:

	/*
	 * If CR0_TS was not set above, pop %xmm0 and %xmm1 off stack,
	 * otherwise set CR0_TS.
	 */
#define	SET_TS_OR_POP_XMM0_XMM1(tmpreg) \
	testq	$CR0_TS, tmpreg; \
	jnz	1f; \
	movaps	(%rsp), %xmm1; \
	movaps	16(%rsp), %xmm0; \
	jmp	2f; \
1: \
	STTS(tmpreg); \
2: \
	mov	%rbp, %rsp; \
	pop	%rbp

	/*
	 * If CR0_TS is not set, align stack (with push %rbp) and push
	 * %xmm0 - %xmm6 on stack, otherwise clear CR0_TS
	 */
#define	CLEAR_TS_OR_PUSH_XMM0_TO_XMM6(tmpreg) \
	push	%rbp; \
	mov	%rsp, %rbp; \
	movq	%cr0, tmpreg; \
	testq	$CR0_TS, tmpreg; \
	jnz	1f; \
	and	$-XMM_ALIGN, %rsp; \
	sub	$[XMM_SIZE * 7], %rsp; \
	movaps	%xmm0, 96(%rsp); \
	movaps	%xmm1, 80(%rsp); \
	movaps	%xmm2, 64(%rsp); \
	movaps	%xmm3, 48(%rsp); \
	movaps	%xmm4, 32(%rsp); \
	movaps	%xmm5, 16(%rsp); \
	movaps	%xmm6, (%rsp); \
	jmp	2f; \
1: \
	PROTECTED_CLTS; \
2:


	/*
	 * If CR0_TS was not set above, pop %xmm0 - %xmm6 off stack,
	 * otherwise set CR0_TS.
	 */
#define	SET_TS_OR_POP_XMM0_TO_XMM6(tmpreg) \
	testq	$CR0_TS, tmpreg; \
	jnz	1f; \
	movaps	(%rsp), %xmm6; \
	movaps	16(%rsp), %xmm5; \
	movaps	32(%rsp), %xmm4; \
	movaps	48(%rsp), %xmm3; \
	movaps	64(%rsp), %xmm2; \
	movaps	80(%rsp), %xmm1; \
	movaps	96(%rsp), %xmm0; \
	jmp	2f; \
1: \
	STTS(tmpreg); \
2: \
	mov	%rbp, %rsp; \
	pop	%rbp

/*
 * void aes_accel_save(void *savestate);
 *
 * Saves all 16 XMM registers and CR0 to a temporary location pointed to
 * in the first argument and clears TS in CR0. This must be invoked before
 * executing any floating point operations inside the kernel (and kernel
 * thread preemption must be disabled as well). The memory region to which
 * all state is saved must be at least 16x 128-bit + 64-bit long and must
 * be 128-bit aligned.
 */
ENTRY_NP(aes_accel_save)
	movq	%cr0, %rax
	movq	%rax, 0x100(%rdi)
	testq	$CR0_TS, %rax
	jnz	1f
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
	SET_SIZE(aes_accel_save)

/*
 * void aes_accel_restore(void *savestate);
 *
 * Restores the saved XMM and CR0.TS state from aes_accel_save.
 */
ENTRY_NP(aes_accel_restore)
	mov	0x100(%rdi), %rax
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
	SET_SIZE(aes_accel_restore)

#else
#define	PROTECTED_CLTS
#define	CLEAR_TS_OR_PUSH_XMM0_XMM1(tmpreg)
#define	SET_TS_OR_POP_XMM0_XMM1(tmpreg)
#define	CLEAR_TS_OR_PUSH_XMM0_TO_XMM6(tmpreg)
#define	SET_TS_OR_POP_XMM0_TO_XMM6(tmpreg)
#endif	/* _KERNEL */


/*
 * _key_expansion_128(), * _key_expansion_192a(), _key_expansion_192b(),
 * _key_expansion_256a(), _key_expansion_256b()
 *
 * Helper functions called by rijndael_key_setup_inc_intel().
 * Also used indirectly by rijndael_key_setup_dec_intel().
 *
 * Input:
 * %xmm0	User-provided cipher key
 * %xmm1	Round constant
 * Output:
 * (%rcx)	AES key
 */

.align	16
_key_expansion_128:
_key_expansion_256a:
	pshufd	$0b11111111, %xmm1, %xmm1
	shufps	$0b00010000, %xmm0, %xmm4
	pxor	%xmm4, %xmm0
	shufps	$0b10001100, %xmm0, %xmm4
	pxor	%xmm4, %xmm0
	pxor	%xmm1, %xmm0
	movaps	%xmm0, (%rcx)
	add	$0x10, %rcx
	ret
	SET_SIZE(_key_expansion_128)
	SET_SIZE(_key_expansion_256a)

.align 16
_key_expansion_192a:
	pshufd	$0b01010101, %xmm1, %xmm1
	shufps	$0b00010000, %xmm0, %xmm4
	pxor	%xmm4, %xmm0
	shufps	$0b10001100, %xmm0, %xmm4
	pxor	%xmm4, %xmm0
	pxor	%xmm1, %xmm0

	movaps	%xmm2, %xmm5
	movaps	%xmm2, %xmm6
	pslldq	$4, %xmm5
	pshufd	$0b11111111, %xmm0, %xmm3
	pxor	%xmm3, %xmm2
	pxor	%xmm5, %xmm2

	movaps	%xmm0, %xmm1
	shufps	$0b01000100, %xmm0, %xmm6
	movaps	%xmm6, (%rcx)
	shufps	$0b01001110, %xmm2, %xmm1
	movaps	%xmm1, 0x10(%rcx)
	add	$0x20, %rcx
	ret
	SET_SIZE(_key_expansion_192a)

.align 16
_key_expansion_192b:
	pshufd	$0b01010101, %xmm1, %xmm1
	shufps	$0b00010000, %xmm0, %xmm4
	pxor	%xmm4, %xmm0
	shufps	$0b10001100, %xmm0, %xmm4
	pxor	%xmm4, %xmm0
	pxor	%xmm1, %xmm0

	movaps	%xmm2, %xmm5
	pslldq	$4, %xmm5
	pshufd	$0b11111111, %xmm0, %xmm3
	pxor	%xmm3, %xmm2
	pxor	%xmm5, %xmm2

	movaps	%xmm0, (%rcx)
	add	$0x10, %rcx
	ret
	SET_SIZE(_key_expansion_192b)

.align 16
_key_expansion_256b:
	pshufd	$0b10101010, %xmm1, %xmm1
	shufps	$0b00010000, %xmm2, %xmm4
	pxor	%xmm4, %xmm2
	shufps	$0b10001100, %xmm2, %xmm4
	pxor	%xmm4, %xmm2
	pxor	%xmm1, %xmm2
	movaps	%xmm2, (%rcx)
	add	$0x10, %rcx
	ret
	SET_SIZE(_key_expansion_256b)

/*
 * void aes_copy_intel(const uint8_t *src, uint8_t *dst);
 *
 * Copies one unaligned 128-bit block from `src' to `dst'. The copy is
 * performed using FPU registers, so make sure FPU state is saved when
 * running this in the kernel.
 */
ENTRY_NP(aes_copy_intel)
	movdqu	(%rdi), %xmm0
	movdqu	%xmm0, (%rsi)
	ret
	SET_SIZE(aes_copy_intel)

/*
 * void aes_xor_intel(const uint8_t *src, uint8_t *dst);
 *
 * XORs one pair of unaligned 128-bit blocks from `src' and `dst' and
 * stores the result at `dst'. The XOR is performed using FPU registers,
 * so make sure FPU state is saved when running this in the kernel.
 */
ENTRY_NP(aes_xor_intel)
	movdqu	(%rdi), %xmm0
	movdqu	(%rsi), %xmm1
	pxor	%xmm1, %xmm0
	movdqu	%xmm0, (%rsi)
	ret
	SET_SIZE(aes_xor_intel)

/*
 * void aes_xor_intel8(const uint8_t *src, uint8_t *dst);
 *
 * XORs eight pairs of consecutive unaligned 128-bit blocks from `src' and
 * 'dst' and stores the results at `dst'. The XOR is performed using FPU
 * registers, so make sure FPU state is saved when running this in the kernel.
 */
ENTRY_NP(aes_xor_intel8)
	movdqu	0x00(%rdi), %xmm0
	movdqu	0x00(%rsi), %xmm1
	movdqu	0x10(%rdi), %xmm2
	movdqu	0x10(%rsi), %xmm3
	movdqu	0x20(%rdi), %xmm4
	movdqu	0x20(%rsi), %xmm5
	movdqu	0x30(%rdi), %xmm6
	movdqu	0x30(%rsi), %xmm7
	movdqu	0x40(%rdi), %xmm8
	movdqu	0x40(%rsi), %xmm9
	movdqu	0x50(%rdi), %xmm10
	movdqu	0x50(%rsi), %xmm11
	movdqu	0x60(%rdi), %xmm12
	movdqu	0x60(%rsi), %xmm13
	movdqu	0x70(%rdi), %xmm14
	movdqu	0x70(%rsi), %xmm15
	pxor	%xmm1, %xmm0
	pxor	%xmm3, %xmm2
	pxor	%xmm5, %xmm4
	pxor	%xmm7, %xmm6
	pxor	%xmm9, %xmm8
	pxor	%xmm11, %xmm10
	pxor	%xmm13, %xmm12
	pxor	%xmm15, %xmm14
	movdqu	%xmm0, 0x00(%rsi)
	movdqu	%xmm2, 0x10(%rsi)
	movdqu	%xmm4, 0x20(%rsi)
	movdqu	%xmm6, 0x30(%rsi)
	movdqu	%xmm8, 0x40(%rsi)
	movdqu	%xmm10, 0x50(%rsi)
	movdqu	%xmm12, 0x60(%rsi)
	movdqu	%xmm14, 0x70(%rsi)
	ret
	SET_SIZE(aes_xor_intel8)

/*
 * rijndael_key_setup_enc_intel()
 * Expand the cipher key into the encryption key schedule.
 *
 * For kernel code, caller is responsible for ensuring kpreempt_disable()
 * has been called.  This is because %xmm registers are not saved/restored.
 * Clear and set the CR0.TS bit on entry and exit, respectively,  if TS is set
 * on entry.  Otherwise, if TS is not set, save and restore %xmm registers
 * on the stack.
 *
 * OpenSolaris interface:
 * int rijndael_key_setup_enc_intel(uint32_t rk[], const uint32_t cipherKey[],
 *	uint64_t keyBits);
 * Return value is 0 on error, number of rounds on success.
 *
 * Original Intel OpenSSL interface:
 * int intel_AES_set_encrypt_key(const unsigned char *userKey,
 *	const int bits, AES_KEY *key);
 * Return value is non-zero on error, 0 on success.
 */

#ifdef	OPENSSL_INTERFACE
#define	rijndael_key_setup_enc_intel	intel_AES_set_encrypt_key
#define	rijndael_key_setup_dec_intel	intel_AES_set_decrypt_key

#define	USERCIPHERKEY		rdi	/* P1, 64 bits */
#define	KEYSIZE32		esi	/* P2, 32 bits */
#define	KEYSIZE64		rsi	/* P2, 64 bits */
#define	AESKEY			rdx	/* P3, 64 bits */

#else	/* OpenSolaris Interface */
#define	AESKEY			rdi	/* P1, 64 bits */
#define	USERCIPHERKEY		rsi	/* P2, 64 bits */
#define	KEYSIZE32		edx	/* P3, 32 bits */
#define	KEYSIZE64		rdx	/* P3, 64 bits */
#endif	/* OPENSSL_INTERFACE */

#define	ROUNDS32		KEYSIZE32	/* temp */
#define	ROUNDS64		KEYSIZE64	/* temp */
#define	ENDAESKEY		USERCIPHERKEY	/* temp */


ENTRY_NP(rijndael_key_setup_enc_intel)
	CLEAR_TS_OR_PUSH_XMM0_TO_XMM6(%r10)

	/ NULL pointer sanity check
	test	%USERCIPHERKEY, %USERCIPHERKEY
	jz	.Lenc_key_invalid_param
	test	%AESKEY, %AESKEY
	jz	.Lenc_key_invalid_param

	movups	(%USERCIPHERKEY), %xmm0	/ user key (first 16 bytes)
	movaps	%xmm0, (%AESKEY)
	lea	0x10(%AESKEY), %rcx	/ key addr
	pxor	%xmm4, %xmm4		/ xmm4 is assumed 0 in _key_expansion_x

	cmp	$256, %KEYSIZE32
	jnz	.Lenc_key192

	/ AES 256: 14 rounds in encryption key schedule
#ifdef OPENSSL_INTERFACE
	mov	$14, %ROUNDS32
	movl	%ROUNDS32, 240(%AESKEY)		/ key.rounds = 14
#endif	/* OPENSSL_INTERFACE */

	movups	0x10(%USERCIPHERKEY), %xmm2	/ other user key (2nd 16 bytes)
	movaps	%xmm2, (%rcx)
	add	$0x10, %rcx

	aeskeygenassist $0x1, %xmm2, %xmm1	/ expand the key
	call	_key_expansion_256a
	aeskeygenassist $0x1, %xmm0, %xmm1
	call	_key_expansion_256b
	aeskeygenassist $0x2, %xmm2, %xmm1	/ expand the key
	call	_key_expansion_256a
	aeskeygenassist $0x2, %xmm0, %xmm1
	call	_key_expansion_256b
	aeskeygenassist $0x4, %xmm2, %xmm1	/ expand the key
	call	_key_expansion_256a
	aeskeygenassist $0x4, %xmm0, %xmm1
	call	_key_expansion_256b
	aeskeygenassist $0x8, %xmm2, %xmm1	/ expand the key
	call	_key_expansion_256a
	aeskeygenassist $0x8, %xmm0, %xmm1
	call	_key_expansion_256b
	aeskeygenassist $0x10, %xmm2, %xmm1	/ expand the key
	call	_key_expansion_256a
	aeskeygenassist $0x10, %xmm0, %xmm1
	call	_key_expansion_256b
	aeskeygenassist $0x20, %xmm2, %xmm1	/ expand the key
	call	_key_expansion_256a
	aeskeygenassist $0x20, %xmm0, %xmm1
	call	_key_expansion_256b
	aeskeygenassist $0x40, %xmm2, %xmm1	/ expand the key
	call	_key_expansion_256a

	SET_TS_OR_POP_XMM0_TO_XMM6(%r10)
#ifdef	OPENSSL_INTERFACE
	xor	%rax, %rax			/ return 0 (OK)
#else	/* Open Solaris Interface */
	mov	$14, %rax			/ return # rounds = 14
#endif
	ret

.align 4
.Lenc_key192:
	cmp	$192, %KEYSIZE32
	jnz	.Lenc_key128

	/ AES 192: 12 rounds in encryption key schedule
#ifdef OPENSSL_INTERFACE
	mov	$12, %ROUNDS32
	movl	%ROUNDS32, 240(%AESKEY)	/ key.rounds = 12
#endif	/* OPENSSL_INTERFACE */

	movq	0x10(%USERCIPHERKEY), %xmm2	/ other user key
	aeskeygenassist $0x1, %xmm2, %xmm1	/ expand the key
	call	_key_expansion_192a
	aeskeygenassist $0x2, %xmm2, %xmm1	/ expand the key
	call	_key_expansion_192b
	aeskeygenassist $0x4, %xmm2, %xmm1	/ expand the key
	call	_key_expansion_192a
	aeskeygenassist $0x8, %xmm2, %xmm1	/ expand the key
	call	_key_expansion_192b
	aeskeygenassist $0x10, %xmm2, %xmm1	/ expand the key
	call	_key_expansion_192a
	aeskeygenassist $0x20, %xmm2, %xmm1	/ expand the key
	call	_key_expansion_192b
	aeskeygenassist $0x40, %xmm2, %xmm1	/ expand the key
	call	_key_expansion_192a
	aeskeygenassist $0x80, %xmm2, %xmm1	/ expand the key
	call	_key_expansion_192b

	SET_TS_OR_POP_XMM0_TO_XMM6(%r10)
#ifdef	OPENSSL_INTERFACE
	xor	%rax, %rax			/ return 0 (OK)
#else	/* OpenSolaris Interface */
	mov	$12, %rax			/ return # rounds = 12
#endif
	ret

.align 4
.Lenc_key128:
	cmp $128, %KEYSIZE32
	jnz .Lenc_key_invalid_key_bits

	/ AES 128: 10 rounds in encryption key schedule
#ifdef OPENSSL_INTERFACE
	mov	$10, %ROUNDS32
	movl	%ROUNDS32, 240(%AESKEY)		/ key.rounds = 10
#endif	/* OPENSSL_INTERFACE */

	aeskeygenassist $0x1, %xmm0, %xmm1	/ expand the key
	call	_key_expansion_128
	aeskeygenassist $0x2, %xmm0, %xmm1	/ expand the key
	call	_key_expansion_128
	aeskeygenassist $0x4, %xmm0, %xmm1	/ expand the key
	call	_key_expansion_128
	aeskeygenassist $0x8, %xmm0, %xmm1	/ expand the key
	call	_key_expansion_128
	aeskeygenassist $0x10, %xmm0, %xmm1	/ expand the key
	call	_key_expansion_128
	aeskeygenassist $0x20, %xmm0, %xmm1	/ expand the key
	call	_key_expansion_128
	aeskeygenassist $0x40, %xmm0, %xmm1	/ expand the key
	call	_key_expansion_128
	aeskeygenassist $0x80, %xmm0, %xmm1	/ expand the key
	call	_key_expansion_128
	aeskeygenassist $0x1b, %xmm0, %xmm1	/ expand the key
	call	_key_expansion_128
	aeskeygenassist $0x36, %xmm0, %xmm1	/ expand the key
	call	_key_expansion_128

	SET_TS_OR_POP_XMM0_TO_XMM6(%r10)
#ifdef	OPENSSL_INTERFACE
	xor	%rax, %rax			/ return 0 (OK)
#else	/* OpenSolaris Interface */
	mov	$10, %rax			/ return # rounds = 10
#endif
	ret

.Lenc_key_invalid_param:
#ifdef	OPENSSL_INTERFACE
	SET_TS_OR_POP_XMM0_TO_XMM6(%r10)
	mov	$-1, %rax	/ user key or AES key pointer is NULL
	ret
#else
	/* FALLTHROUGH */
#endif	/* OPENSSL_INTERFACE */

.Lenc_key_invalid_key_bits:
	SET_TS_OR_POP_XMM0_TO_XMM6(%r10)
#ifdef	OPENSSL_INTERFACE
	mov	$-2, %rax	/ keysize is invalid
#else	/* Open Solaris Interface */
	xor	%rax, %rax	/ a key pointer is NULL or invalid keysize
#endif	/* OPENSSL_INTERFACE */

	ret
	SET_SIZE(rijndael_key_setup_enc_intel)


/*
 * rijndael_key_setup_dec_intel()
 * Expand the cipher key into the decryption key schedule.
 *
 * For kernel code, caller is responsible for ensuring kpreempt_disable()
 * has been called.  This is because %xmm registers are not saved/restored.
 * Clear and set the CR0.TS bit on entry and exit, respectively,  if TS is set
 * on entry.  Otherwise, if TS is not set, save and restore %xmm registers
 * on the stack.
 *
 * OpenSolaris interface:
 * int rijndael_key_setup_dec_intel(uint32_t rk[], const uint32_t cipherKey[],
 *	uint64_t keyBits);
 * Return value is 0 on error, number of rounds on success.
 * P1->P2, P2->P3, P3->P1
 *
 * Original Intel OpenSSL interface:
 * int intel_AES_set_decrypt_key(const unsigned char *userKey,
 *	const int bits, AES_KEY *key);
 * Return value is non-zero on error, 0 on success.
 */
ENTRY_NP(rijndael_key_setup_dec_intel)
	/ Generate round keys used for encryption
	call	rijndael_key_setup_enc_intel
	test	%rax, %rax
#ifdef	OPENSSL_INTERFACE
	jnz	.Ldec_key_exit	/ Failed if returned non-0
#else	/* OpenSolaris Interface */
	jz	.Ldec_key_exit	/ Failed if returned 0
#endif	/* OPENSSL_INTERFACE */

	CLEAR_TS_OR_PUSH_XMM0_XMM1(%r10)

	/*
	 * Convert round keys used for encryption
	 * to a form usable for decryption
	 */
#ifndef	OPENSSL_INTERFACE		/* OpenSolaris Interface */
	mov	%rax, %ROUNDS64		/ set # rounds (10, 12, or 14)
					/ (already set for OpenSSL)
#endif

	lea	0x10(%AESKEY), %rcx	/ key addr
	shl	$4, %ROUNDS32
	add	%AESKEY, %ROUNDS64
	mov	%ROUNDS64, %ENDAESKEY

.align 4
.Ldec_key_reorder_loop:
	movaps	(%AESKEY), %xmm0
	movaps	(%ROUNDS64), %xmm1
	movaps	%xmm0, (%ROUNDS64)
	movaps	%xmm1, (%AESKEY)
	lea	0x10(%AESKEY), %AESKEY
	lea	-0x10(%ROUNDS64), %ROUNDS64
	cmp	%AESKEY, %ROUNDS64
	ja	.Ldec_key_reorder_loop

.align 4
.Ldec_key_inv_loop:
	movaps	(%rcx), %xmm0
	/ Convert an encryption round key to a form usable for decryption
	/ with the "AES Inverse Mix Columns" instruction
	aesimc	%xmm0, %xmm1
	movaps	%xmm1, (%rcx)
	lea	0x10(%rcx), %rcx
	cmp	%ENDAESKEY, %rcx
	jnz	.Ldec_key_inv_loop

	SET_TS_OR_POP_XMM0_XMM1(%r10)

.Ldec_key_exit:
	/ OpenSolaris: rax = # rounds (10, 12, or 14) or 0 for error
	/ OpenSSL: rax = 0 for OK, or non-zero for error
	ret
	SET_SIZE(rijndael_key_setup_dec_intel)


#ifdef	OPENSSL_INTERFACE
#define	aes_encrypt_intel	intel_AES_encrypt
#define	aes_decrypt_intel	intel_AES_decrypt

#define	INP		rdi	/* P1, 64 bits */
#define	OUTP		rsi	/* P2, 64 bits */
#define	KEYP		rdx	/* P3, 64 bits */

/* No NROUNDS parameter--offset 240 from KEYP saved in %ecx:  */
#define	NROUNDS32	ecx	/* temporary, 32 bits */
#define	NROUNDS		cl	/* temporary,  8 bits */

#else	/* OpenSolaris Interface */
#define	KEYP		rdi	/* P1, 64 bits */
#define	NROUNDS		esi	/* P2, 32 bits */
#define	INP		rdx	/* P3, 64 bits */
#define	OUTP		rcx	/* P4, 64 bits */
#define	LENGTH		r8	/* P5, 64 bits */
#endif	/* OPENSSL_INTERFACE */

#define	KEY		xmm0	/* temporary, 128 bits */
#define	STATE0		xmm8	/* temporary, 128 bits */
#define	STATE1		xmm9	/* temporary, 128 bits */
#define	STATE2		xmm10	/* temporary, 128 bits */
#define	STATE3		xmm11	/* temporary, 128 bits */
#define	STATE4		xmm12	/* temporary, 128 bits */
#define	STATE5		xmm13	/* temporary, 128 bits */
#define	STATE6		xmm14	/* temporary, 128 bits */
#define	STATE7		xmm15	/* temporary, 128 bits */

/*
 * Runs the first two rounds of AES256 on a state register. `op' should be
 * aesenc or aesdec.
 */
#define	AES256_ROUNDS(op, statereg)	\
	movaps	-0x60(%KEYP), %KEY;	\
	op	%KEY, %statereg;	\
	movaps	-0x50(%KEYP), %KEY;	\
	op	%KEY, %statereg

/*
 * Runs the first two rounds of AES192, or the 3rd & 4th round of AES256 on
 * a state register. `op' should be aesenc or aesdec.
 */
#define	AES192_ROUNDS(op, statereg)	\
	movaps	-0x40(%KEYP), %KEY;	\
	op	%KEY, %statereg;	\
	movaps	-0x30(%KEYP), %KEY;	\
	op	%KEY, %statereg

/*
 * Runs the full 10 rounds of AES128, or the last 10 rounds of AES192/AES256
 * on a state register. `op' should be aesenc or aesdec and `lastop' should
 * be aesenclast or aesdeclast.
 */
#define	AES128_ROUNDS(op, lastop, statereg) \
	movaps	-0x20(%KEYP), %KEY;	\
	op	%KEY, %statereg;	\
	movaps	-0x10(%KEYP), %KEY;	\
	op	%KEY, %statereg;	\
	movaps	(%KEYP), %KEY;		\
	op	%KEY, %statereg;	\
	movaps	0x10(%KEYP), %KEY;	\
	op	%KEY, %statereg;	\
	movaps	0x20(%KEYP), %KEY;	\
	op	%KEY, %statereg;	\
	movaps	0x30(%KEYP), %KEY;	\
	op	%KEY, %statereg;	\
	movaps	0x40(%KEYP), %KEY;	\
	op	%KEY, %statereg;	\
	movaps	0x50(%KEYP), %KEY;	\
	op	%KEY, %statereg;	\
	movaps	0x60(%KEYP), %KEY;	\
	op	%KEY, %statereg;	\
	movaps	0x70(%KEYP), %KEY;	\
	lastop	%KEY, %statereg

/*
 * Macros to run AES encryption rounds. Input must be prefilled in state
 * register - output will be left there as well.
 * To run AES256, invoke all of these macros in sequence. To run AES192,
 * invoke only the -192 and -128 variants. To run AES128, invoke only the
 * -128 variant.
 */
#define	AES256_ENC_ROUNDS(statereg) \
	AES256_ROUNDS(aesenc, statereg)
#define	AES192_ENC_ROUNDS(statereg) \
	AES192_ROUNDS(aesenc, statereg)
#define	AES128_ENC_ROUNDS(statereg) \
	AES128_ROUNDS(aesenc, aesenclast, statereg)

/* Same as the AES*_ENC_ROUNDS macros, but for decryption. */
#define	AES256_DEC_ROUNDS(statereg) \
	AES256_ROUNDS(aesdec, statereg)
#define	AES192_DEC_ROUNDS(statereg) \
	AES192_ROUNDS(aesdec, statereg)
#define	AES128_DEC_ROUNDS(statereg) \
	AES128_ROUNDS(aesdec, aesdeclast, statereg)


/*
 * aes_encrypt_intel()
 * Encrypt a single block (in and out can overlap).
 *
 * For kernel code, caller is responsible for bracketing this call with
 * disabling kernel thread preemption and calling aes_accel_save/restore().
 *
 * Temporary register usage:
 * %xmm0	Key
 * %xmm8	State
 *
 * Original OpenSolaris Interface:
 * void aes_encrypt_intel(const aes_ks_t *ks, int Nr,
 *	const uint32_t pt[4], uint32_t ct[4])
 *
 * Original Intel OpenSSL Interface:
 * void intel_AES_encrypt(const unsigned char *in, unsigned char *out,
 *	const AES_KEY *key)
 */
ENTRY_NP(aes_encrypt_intel)
	movups	(%INP), %STATE0			/ input
	movaps	(%KEYP), %KEY			/ key

#ifdef	OPENSSL_INTERFACE
	mov	240(%KEYP), %NROUNDS32		/ round count
#else	/* OpenSolaris Interface */
	/* Round count is already present as P2 in %rsi/%esi */
#endif	/* OPENSSL_INTERFACE */

	pxor	%KEY, %STATE0			/ round 0
	lea	0x30(%KEYP), %KEYP
	cmp	$12, %NROUNDS
	jb	.Lenc128
	lea	0x20(%KEYP), %KEYP
	je	.Lenc192

	/ AES 256
	lea	0x20(%KEYP), %KEYP
	AES256_ENC_ROUNDS(STATE0)

.align 4
.Lenc192:
	/ AES 192 and 256
	AES192_ENC_ROUNDS(STATE0)

.align 4
.Lenc128:
	/ AES 128, 192, and 256
	AES128_ENC_ROUNDS(STATE0)
	movups	%STATE0, (%OUTP)		/ output

	ret
	SET_SIZE(aes_encrypt_intel)

/*
 * aes_decrypt_intel()
 * Decrypt a single block (in and out can overlap).
 *
 * For kernel code, caller is responsible for bracketing this call with
 * disabling kernel thread preemption and calling aes_accel_save/restore().
 *
 * Temporary register usage:
 * %xmm0	State
 * %xmm1	Key
 *
 * Original OpenSolaris Interface:
 * void aes_decrypt_intel(const aes_ks_t *ks, int Nr,
 *	const uint32_t pt[4], uint32_t ct[4])
 *
 * Original Intel OpenSSL Interface:
 * void intel_AES_decrypt(const unsigned char *in, unsigned char *out,
 *	const AES_KEY *key);
 */
ENTRY_NP(aes_decrypt_intel)
	movups	(%INP), %STATE0			/ input
	movaps	(%KEYP), %KEY			/ key

#ifdef	OPENSSL_INTERFACE
	mov	240(%KEYP), %NROUNDS32		/ round count
#else	/* OpenSolaris Interface */
	/* Round count is already present as P2 in %rsi/%esi */
#endif	/* OPENSSL_INTERFACE */

	pxor	%KEY, %STATE0			/ round 0
	lea	0x30(%KEYP), %KEYP
	cmp	$12, %NROUNDS
	jb	.Ldec128
	lea	0x20(%KEYP), %KEYP
	je	.Ldec192

	/ AES 256
	lea	0x20(%KEYP), %KEYP
	AES256_DEC_ROUNDS(STATE0)

.align 4
.Ldec192:
	/ AES 192 and 256
	AES192_DEC_ROUNDS(STATE0)

.align 4
.Ldec128:
	/ AES 128, 192, and 256
	AES128_DEC_ROUNDS(STATE0)
	movups	%STATE0, (%OUTP)		/ output

	ret
	SET_SIZE(aes_decrypt_intel)

/* Does a pipelined load of eight input blocks into our AES state registers. */
#define	AES_LOAD_INPUT_8BLOCKS		\
	movups	0x00(%INP), %STATE0;	\
	movups	0x10(%INP), %STATE1;	\
	movups	0x20(%INP), %STATE2;	\
	movups	0x30(%INP), %STATE3;	\
	movups	0x40(%INP), %STATE4;	\
	movups	0x50(%INP), %STATE5;	\
	movups	0x60(%INP), %STATE6;	\
	movups	0x70(%INP), %STATE7;

/* Does a pipelined store of eight AES state registers to the output. */
#define	AES_STORE_OUTPUT_8BLOCKS	\
	movups	%STATE0, 0x00(%OUTP);	\
	movups	%STATE1, 0x10(%OUTP);	\
	movups	%STATE2, 0x20(%OUTP);	\
	movups	%STATE3, 0x30(%OUTP);	\
	movups	%STATE4, 0x40(%OUTP);	\
	movups	%STATE5, 0x50(%OUTP);	\
	movups	%STATE6, 0x60(%OUTP);	\
	movups	%STATE7, 0x70(%OUTP);

/* Performs a pipelined AES instruction with the key on all state registers. */
#define	AES_KEY_STATE_OP_8BLOCKS(op)	\
	op	%KEY, %STATE0;		\
	op	%KEY, %STATE1;		\
	op	%KEY, %STATE2;		\
	op	%KEY, %STATE3;		\
	op	%KEY, %STATE4;		\
	op	%KEY, %STATE5;		\
	op	%KEY, %STATE6;		\
	op	%KEY, %STATE7

/* XOR all AES state regs with key to initiate encryption/decryption. */
#define	AES_XOR_STATE_8BLOCKS		\
	AES_KEY_STATE_OP_8BLOCKS(pxor)

/*
 * Loads a round key from the key schedule offset `off' into the KEY
 * register and performs `op' using the KEY on all 8 STATE registers.
 */
#define	AES_RND_8BLOCKS(op, off)	\
	movaps	off(%KEYP), %KEY;	\
	AES_KEY_STATE_OP_8BLOCKS(op)

/*
 * void aes_encrypt_intel8(const uint32_t roundkeys[], int numrounds,
 *	const void *plaintext, void *ciphertext)
 *
 * Same as aes_encrypt_intel, but performs the encryption operation on
 * 8 independent blocks in sequence, exploiting instruction pipelining.
 * This function doesn't support the OpenSSL interface, it's only meant
 * for kernel use.
 */
ENTRY_NP(aes_encrypt_intel8)
	AES_LOAD_INPUT_8BLOCKS		/ load input
	movaps	(%KEYP), %KEY		/ key
	AES_XOR_STATE_8BLOCKS		/ round 0

	lea	0x30(%KEYP), %KEYP	/ point to key schedule
	cmp	$12, %NROUNDS		/ determine AES variant
	jb	.Lenc8_128
	lea	0x20(%KEYP), %KEYP	/ AES192 has larger key schedule
	je	.Lenc8_192

	lea	0x20(%KEYP), %KEYP	/ AES256 has even larger key schedule
	AES_RND_8BLOCKS(aesenc, -0x60)	/ AES256 R.1
	AES_RND_8BLOCKS(aesenc, -0x50)	/ AES256 R.2

.align 4
.Lenc8_192:
	AES_RND_8BLOCKS(aesenc, -0x40)	/ AES192 R.1; AES256 R.3
	AES_RND_8BLOCKS(aesenc, -0x30)	/ AES192 R.2; AES256 R.4

.align 4
.Lenc8_128:
	AES_RND_8BLOCKS(aesenc, -0x20)	/ AES128 R.1; AES192 R.3; AES256 R.5
	AES_RND_8BLOCKS(aesenc, -0x10)	/ AES128 R.2; AES192 R.4; AES256 R.6
	AES_RND_8BLOCKS(aesenc, 0x00)	/ AES128 R.3; AES192 R.5; AES256 R.7
	AES_RND_8BLOCKS(aesenc, 0x10)	/ AES128 R.4; AES192 R.6; AES256 R.8
	AES_RND_8BLOCKS(aesenc, 0x20)	/ AES128 R.5; AES192 R.7; AES256 R.9
	AES_RND_8BLOCKS(aesenc, 0x30)	/ AES128 R.6; AES192 R.8; AES256 R.10
	AES_RND_8BLOCKS(aesenc, 0x40)	/ AES128 R.7; AES192 R.9; AES256 R.11
	AES_RND_8BLOCKS(aesenc, 0x50)	/ AES128 R.8; AES192 R.10; AES256 R.12
	AES_RND_8BLOCKS(aesenc, 0x60)	/ AES128 R.9; AES192 R.11; AES256 R.13
	AES_RND_8BLOCKS(aesenclast, 0x70)/ AES128 R.10; AES192 R.12; AES256 R.14

	AES_STORE_OUTPUT_8BLOCKS	/ store output
	ret
	SET_SIZE(aes_encrypt_intel8)


/*
 * void aes_decrypt_intel8(const uint32_t roundkeys[], int numrounds,
 *	const void *ciphertext, void *plaintext)
 *
 * Same as aes_decrypt_intel, but performs the decryption operation on
 * 8 independent blocks in sequence, exploiting instruction pipelining.
 * This function doesn't support the OpenSSL interface, it's only meant
 * for kernel use.
 */
ENTRY_NP(aes_decrypt_intel8)
	AES_LOAD_INPUT_8BLOCKS		/ load input
	movaps	(%KEYP), %KEY		/ key
	AES_XOR_STATE_8BLOCKS		/ round 0

	lea	0x30(%KEYP), %KEYP	/ point to key schedule
	cmp	$12, %NROUNDS		/ determine AES variant
	jb	.Ldec8_128
	lea	0x20(%KEYP), %KEYP	/ AES192 has larger key schedule
	je	.Ldec8_192

	lea	0x20(%KEYP), %KEYP	/ AES256 has even larger key schedule
	AES_RND_8BLOCKS(aesdec, -0x60)	/ AES256 R.1
	AES_RND_8BLOCKS(aesdec, -0x50)	/ AES256 R.2

.align 4
.Ldec8_192:
	AES_RND_8BLOCKS(aesdec, -0x40)	/ AES192 R.1; AES256 R.3
	AES_RND_8BLOCKS(aesdec, -0x30)	/ AES192 R.2; AES256 R.4

.align 4
.Ldec8_128:
	AES_RND_8BLOCKS(aesdec, -0x20)	/ AES128 R.1; AES192 R.3; AES256 R.5
	AES_RND_8BLOCKS(aesdec, -0x10)	/ AES128 R.2; AES192 R.4; AES256 R.6
	AES_RND_8BLOCKS(aesdec, 0x00)	/ AES128 R.3; AES192 R.5; AES256 R.7
	AES_RND_8BLOCKS(aesdec, 0x10)	/ AES128 R.4; AES192 R.6; AES256 R.8
	AES_RND_8BLOCKS(aesdec, 0x20)	/ AES128 R.5; AES192 R.7; AES256 R.9
	AES_RND_8BLOCKS(aesdec, 0x30)	/ AES128 R.6; AES192 R.8; AES256 R.10
	AES_RND_8BLOCKS(aesdec, 0x40)	/ AES128 R.7; AES192 R.9; AES256 R.11
	AES_RND_8BLOCKS(aesdec, 0x50)	/ AES128 R.8; AES192 R.10; AES256 R.12
	AES_RND_8BLOCKS(aesdec, 0x60)	/ AES128 R.9; AES192 R.11; AES256 R.13
	AES_RND_8BLOCKS(aesdeclast, 0x70)/ AES128 R.10; AES192 R.12; AES256 R.14

	AES_STORE_OUTPUT_8BLOCKS	/ store output
	ret
	SET_SIZE(aes_decrypt_intel8)


/*
 * This macro encapsulates the entire AES encryption algo for a single
 * block, which is prefilled in statereg and which will be replaced by
 * the encrypted output. The KEYP register must already point to the
 * AES128 key schedule ("lea 0x30(%KEYP), %KEYP" from encryption
 * function call) so that consecutive invocations of this macro are
 * supported (KEYP is restored after each invocation).
 */
#define	AES_ENC(statereg, label_128, label_192, label_out)	\
	cmp	$12, %NROUNDS;					\
	jb	label_128;					\
	je	label_192;					\
	/* AES 256 only */					\
	lea	0x40(%KEYP), %KEYP;				\
	AES256_ENC_ROUNDS(statereg);				\
	AES192_ENC_ROUNDS(statereg);				\
	AES128_ENC_ROUNDS(statereg);				\
	lea	-0x40(%KEYP), %KEYP;				\
	jmp	label_out;					\
.align 4;							\
label_192:							\
	lea	0x20(%KEYP), %KEYP;				\
	/* AES 192 only */					\
	AES192_ENC_ROUNDS(statereg);				\
	AES128_ENC_ROUNDS(statereg);				\
	lea	-0x20(%KEYP), %KEYP;				\
	jmp	label_out;					\
.align 4;							\
label_128:							\
	/* AES 128 only */					\
	AES128_ENC_ROUNDS(statereg);				\
.align 4;							\
label_out:


/*
 * void aes_encrypt_cbc_intel8(const uint32_t roundkeys[], int numrounds,
 *	const void *plaintext, void *ciphertext, const void *IV)
 *
 * Encrypts 8 consecutive AES blocks in the CBC mode. Input and output
 * may overlap. This provides a modest performance boost over invoking
 * the encryption and XOR in separate functions because we can avoid
 * copying the ciphertext block to and from memory between encryption
 * and XOR calls.
 */
#define	CBC_IV			r8	/* input - IV blk pointer */
#define	CBC_IV_XMM		xmm1	/* tmp IV location for alignment */

ENTRY_NP(aes_encrypt_cbc_intel8)
	AES_LOAD_INPUT_8BLOCKS		/ load input
	movaps	(%KEYP), %KEY		/ key
	AES_XOR_STATE_8BLOCKS		/ round 0

	lea	0x30(%KEYP), %KEYP	/ point to key schedule
	movdqu	(%CBC_IV), %CBC_IV_XMM	/ load IV from unaligned memory
	pxor	%CBC_IV_XMM, %STATE0	/ XOR IV with input block and encrypt
	AES_ENC(STATE0, .Lenc_cbc_0_128, .Lenc_cbc_0_192, .Lenc_cbc_0_out)
	pxor	%STATE0, %STATE1
	AES_ENC(STATE1, .Lenc_cbc_1_128, .Lenc_cbc_1_192, .Lenc_cbc_1_out)
	pxor	%STATE1, %STATE2
	AES_ENC(STATE2, .Lenc_cbc_2_128, .Lenc_cbc_2_192, .Lenc_cbc_2_out)
	pxor	%STATE2, %STATE3
	AES_ENC(STATE3, .Lenc_cbc_3_128, .Lenc_cbc_3_192, .Lenc_cbc_3_out)
	pxor	%STATE3, %STATE4
	AES_ENC(STATE4, .Lenc_cbc_4_128, .Lenc_cbc_4_192, .Lenc_cbc_4_out)
	pxor	%STATE4, %STATE5
	AES_ENC(STATE5, .Lenc_cbc_5_128, .Lenc_cbc_5_192, .Lenc_cbc_5_out)
	pxor	%STATE5, %STATE6
	AES_ENC(STATE6, .Lenc_cbc_6_128, .Lenc_cbc_6_192, .Lenc_cbc_6_out)
	pxor	%STATE6, %STATE7
	AES_ENC(STATE7, .Lenc_cbc_7_128, .Lenc_cbc_7_192, .Lenc_cbc_7_out)

	AES_STORE_OUTPUT_8BLOCKS	/ store output
	ret
	SET_SIZE(aes_encrypt_cbc_intel8)

/*
 * Prefills register state with counters suitable for the CTR encryption
 * mode. The counter is assumed to consist of two portions:
 * - A lower monotonically increasing 64-bit counter. If the caller wants
 *   a smaller counter, they are responsible for checking that it doesn't
 *   overflow between encryption calls.
 * - An upper static "nonce" portion, in big endian, preloaded into the
 *   lower portion of an XMM register.
 * This macro adds `ctridx' to the lower_LE counter, swaps it to big
 * endian and by way of a temporary general-purpose register loads the
 * lower and upper counter portions into a target XMM result register,
 * which can then be handed off to the encryption process.
 */
#define	PREP_CTR_BLOCKS(lower_LE, upper_BE_xmm, ctridx, tmpreg, resreg)	\
	lea	ctridx(%lower_LE), %tmpreg;				\
	bswap	%tmpreg;						\
	movq	%tmpreg, %resreg;					\
	movlhps	%upper_BE_xmm, %resreg;					\
	pshufd	$0b01001110, %resreg, %resreg

#define	CTR_UPPER_BE		r8	/* input - counter upper 64 bits (BE) */
#define	CTR_UPPER_BE_XMM	xmm1	/* tmp for upper counter bits */
#define	CTR_LOWER_LE		r9	/* input - counter lower 64 bits (LE) */
#define	CTR_TMP0		rax	/* tmp for lower 64 bit add & bswap */
#define	CTR_TMP1		rbx	/* tmp for lower 64 bit add & bswap */
#define	CTR_TMP2		r10	/* tmp for lower 64 bit add & bswap */
#define	CTR_TMP3		r11	/* tmp for lower 64 bit add & bswap */
#define	CTR_TMP4		r12	/* tmp for lower 64 bit add & bswap */
#define	CTR_TMP5		r13	/* tmp for lower 64 bit add & bswap */
#define	CTR_TMP6		r14	/* tmp for lower 64 bit add & bswap */
#define	CTR_TMP7		r15	/* tmp for lower 64 bit add & bswap */

/*
 * These are used in case CTR encryption input is unaligned before XORing.
 * Must not overlap with any STATE[0-7] register.
 */
#define	TMP_INPUT0	xmm0
#define	TMP_INPUT1	xmm1
#define	TMP_INPUT2	xmm2
#define	TMP_INPUT3	xmm3
#define	TMP_INPUT4	xmm4
#define	TMP_INPUT5	xmm5
#define	TMP_INPUT6	xmm6
#define	TMP_INPUT7	xmm7

/*
 * void aes_ctr_intel8(const uint32_t roundkeys[], int numrounds,
 *	const void *input, void *output, uint64_t counter_upper_BE,
 *	uint64_t counter_lower_LE)
 *
 * Runs AES on 8 consecutive blocks in counter mode (encryption and
 * decryption in counter mode are the same).
 */
ENTRY_NP(aes_ctr_intel8)
	/* save caller's regs */
	pushq	%rbp
	movq	%rsp, %rbp
	subq	$0x38, %rsp
	/ CTR_TMP0 is rax, no need to save
	movq	%CTR_TMP1, -0x38(%rbp)
	movq	%CTR_TMP2, -0x30(%rbp)
	movq	%CTR_TMP3, -0x28(%rbp)
	movq	%CTR_TMP4, -0x20(%rbp)
	movq	%CTR_TMP5, -0x18(%rbp)
	movq	%CTR_TMP6, -0x10(%rbp)
	movq	%CTR_TMP7, -0x08(%rbp)

	/*
	 * CTR step 1: prepare big-endian formatted 128-bit counter values,
	 * placing the result in the AES-NI input state registers.
	 */
	movq	%CTR_UPPER_BE, %CTR_UPPER_BE_XMM
	PREP_CTR_BLOCKS(CTR_LOWER_LE, CTR_UPPER_BE_XMM, 0, CTR_TMP0, STATE0)
	PREP_CTR_BLOCKS(CTR_LOWER_LE, CTR_UPPER_BE_XMM, 1, CTR_TMP1, STATE1)
	PREP_CTR_BLOCKS(CTR_LOWER_LE, CTR_UPPER_BE_XMM, 2, CTR_TMP2, STATE2)
	PREP_CTR_BLOCKS(CTR_LOWER_LE, CTR_UPPER_BE_XMM, 3, CTR_TMP3, STATE3)
	PREP_CTR_BLOCKS(CTR_LOWER_LE, CTR_UPPER_BE_XMM, 4, CTR_TMP4, STATE4)
	PREP_CTR_BLOCKS(CTR_LOWER_LE, CTR_UPPER_BE_XMM, 5, CTR_TMP5, STATE5)
	PREP_CTR_BLOCKS(CTR_LOWER_LE, CTR_UPPER_BE_XMM, 6, CTR_TMP6, STATE6)
	PREP_CTR_BLOCKS(CTR_LOWER_LE, CTR_UPPER_BE_XMM, 7, CTR_TMP7, STATE7)

	/*
	 * CTR step 2: Encrypt the counters.
	 */
	movaps	(%KEYP), %KEY		/ key
	AES_XOR_STATE_8BLOCKS		/ round 0

	/* Determine the AES variant we're going to compute */
	lea	0x30(%KEYP), %KEYP	/ point to key schedule
	cmp	$12, %NROUNDS		/ determine AES variant
	jb	.Lctr8_128
	lea	0x20(%KEYP), %KEYP	/ AES192 has larger key schedule
	je	.Lctr8_192

	/* AES 256 */
	lea	0x20(%KEYP), %KEYP	/ AES256 has even larger key schedule
	AES_RND_8BLOCKS(aesenc, -0x60)	/ AES256 R.1
	AES_RND_8BLOCKS(aesenc, -0x50)	/ AES256 R.2

.align 4
.Lctr8_192:
	/* AES 192 and 256 */
	AES_RND_8BLOCKS(aesenc, -0x40)	/ AES192 R.1; AES256 R.3
	AES_RND_8BLOCKS(aesenc, -0x30)	/ AES192 R.2; AES256 R.4

.align 4
.Lctr8_128:
	/* AES 128, 192, and 256 */
	AES_RND_8BLOCKS(aesenc, -0x20)	/ AES128 R.1; AES192 R.3; AES256 R.5
	AES_RND_8BLOCKS(aesenc, -0x10)	/ AES128 R.2; AES192 R.4; AES256 R.6
	AES_RND_8BLOCKS(aesenc, 0x00)	/ AES128 R.3; AES192 R.5; AES256 R.7
	AES_RND_8BLOCKS(aesenc, 0x10)	/ AES128 R.4; AES192 R.6; AES256 R.8
	AES_RND_8BLOCKS(aesenc, 0x20)	/ AES128 R.5; AES192 R.7; AES256 R.9
	AES_RND_8BLOCKS(aesenc, 0x30)	/ AES128 R.6; AES192 R.8; AES256 R.10
	AES_RND_8BLOCKS(aesenc, 0x40)	/ AES128 R.7; AES192 R.9; AES256 R.11
	AES_RND_8BLOCKS(aesenc, 0x50)	/ AES128 R.8; AES192 R.10; AES256 R.12
	AES_RND_8BLOCKS(aesenc, 0x60)	/ AES128 R.9; AES192 R.11; AES256 R.13
	AES_RND_8BLOCKS(aesenclast, 0x70)/ AES128 R.10; AES192 R.12; AES256 R.14

	/*
	 * CTR step 3: XOR input data blocks with encrypted counters to
	 * produce result.
	 */
	mov	%INP, %rax		/ pxor requires alignment, so check
	andq	$0xf, %rax
	jnz	.Lctr_input_unaligned
	pxor	0x00(%INP), %STATE0
	pxor	0x10(%INP), %STATE1
	pxor	0x20(%INP), %STATE2
	pxor	0x30(%INP), %STATE3
	pxor	0x40(%INP), %STATE4
	pxor	0x50(%INP), %STATE5
	pxor	0x60(%INP), %STATE6
	pxor	0x70(%INP), %STATE7
	jmp	.Lctr_out

.align 4
.Lctr_input_unaligned:
	movdqu	0x00(%INP), %TMP_INPUT0
	movdqu	0x10(%INP), %TMP_INPUT1
	movdqu	0x20(%INP), %TMP_INPUT2
	movdqu	0x30(%INP), %TMP_INPUT3
	movdqu	0x40(%INP), %TMP_INPUT4
	movdqu	0x50(%INP), %TMP_INPUT5
	movdqu	0x60(%INP), %TMP_INPUT6
	movdqu	0x70(%INP), %TMP_INPUT7
	pxor	%TMP_INPUT0, %STATE0
	pxor	%TMP_INPUT1, %STATE1
	pxor	%TMP_INPUT2, %STATE2
	pxor	%TMP_INPUT3, %STATE3
	pxor	%TMP_INPUT4, %STATE4
	pxor	%TMP_INPUT5, %STATE5
	pxor	%TMP_INPUT6, %STATE6
	pxor	%TMP_INPUT7, %STATE7

.align 4
.Lctr_out:
	/*
	 * Step 4: Write out processed blocks to memory.
	 */
	movdqu	%STATE0, 0x00(%OUTP)
	movdqu	%STATE1, 0x10(%OUTP)
	movdqu	%STATE2, 0x20(%OUTP)
	movdqu	%STATE3, 0x30(%OUTP)
	movdqu	%STATE4, 0x40(%OUTP)
	movdqu	%STATE5, 0x50(%OUTP)
	movdqu	%STATE6, 0x60(%OUTP)
	movdqu	%STATE7, 0x70(%OUTP)

	/* restore caller's regs */
	/ CTR_TMP0 is rax, no need to restore
	movq	-0x38(%rbp), %CTR_TMP1
	movq	-0x30(%rbp), %CTR_TMP2
	movq	-0x28(%rbp), %CTR_TMP3
	movq	-0x20(%rbp), %CTR_TMP4
	movq	-0x18(%rbp), %CTR_TMP5
	movq	-0x10(%rbp), %CTR_TMP6
	movq	-0x08(%rbp), %CTR_TMP7
	leave
	ret
	SET_SIZE(aes_ctr_intel8)

#endif	/* lint || __lint */
