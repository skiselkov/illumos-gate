/*
 * Copyright (c) 2013, CRYPTOGAMS by <appro@openssl.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *	Redistributions of source code must retain copyright notices,
 *	this list of conditions and the following disclaimer.
 *
 * *	Redistributions in binary form must reproduce the above
 *	copyright notice, this list of conditions and the following
 *	disclaimer in the documentation and/or other materials
 *	provided with the distribution.
 *
 * *	Neither the name of the CRYPTOGAMS nor the names of its
 *	copyright holder and contributors may be used to endorse or
 *	promote products derived from this software without specific
 *	prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL), in which case the provisions of the GPL apply INSTEAD OF
 * those given above.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Copyright 2014 by Saso Kiselkov on Illumos port sections.
 */

#if defined(lint) || defined(__lint)

#include <sys/types.h>

/*ARGSUSED*/
void
gcm_ghash_clmul(uint64_t ghash[2], const uint8_t Htable[256],
    const uint8_t *inp, size_t len)
{
}

/*ARGSUSED*/
void
gcm_init_clmul(const uint64_t hash_init[2], uint8_t Htable[256])
{
}

#else	/* lint */

#include <sys/asm_linkage.h>
#include <sys/controlregs.h>
#ifdef _KERNEL
#include <sys/machprivregs.h>
#endif

.text
.align XMM_ALIGN
.Lbyte_swap16_mask:
	.byte	15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0
.L0x1c2_polynomial:
	.byte	1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xc2
.L7_mask:
	.long	7, 0, 7, 0

#define	Xi	xmm0	/* hash value */
#define	Xhi	xmm1	/* hash value high order 64 bits */
#define	Hkey	xmm2	/* hash key */
#define	T1	xmm3	/* temp1 */
#define	T2	xmm4	/* temp2 */
#define	T3	xmm5	/* temp3 */
#define	Xb0	xmm6	/* cipher block #0 */
#define	Xb1	xmm7	/* cipher block #1 */
#define	Xb2	xmm8	/* cipher block #2 */
#define	Xb3	xmm9	/* cipher block #3 */
#define	Xb4	xmm10	/* cipher block #4 */
#define	Xb5	xmm11	/* cipher block #5 */
#define	Xb6	xmm12	/* cipher block #6 */
#define	Xb7	xmm13	/* cipher block #7 */

#define	clmul64x64_T2(tmpreg)				\
	movdqa		%Xi, %Xhi;			\
	pshufd		$0b01001110, %Xi, %T1;		\
	pxor		%Xi, %T1;			\
							\
	pclmulqdq	$0x00, %Hkey, %Xi;		\
	pclmulqdq	$0x11, %Hkey, %Xhi;		\
	pclmulqdq	$0x00, %tmpreg, %T1;		\
	pxor		%Xi, %T1;			\
	pxor		%Xhi, %T1;			\
							\
	movdqa		%T1, %T2;			\
	psrldq		$8, %T1;			\
	pslldq		$8, %T2;			\
	pxor		%T1, %Xhi;			\
	pxor		%T2, %Xi

#define	reduction_alg9					\
	/* 1st phase */					\
	movdqa		%Xi, %T2;			\
	movdqa		%Xi, %T1;			\
	psllq		$5, %Xi;			\
	pxor		%Xi, %T1;			\
	psllq		$1, %Xi;			\
	pxor		%T1, %Xi;			\
	psllq		$57, %Xi;			\
	movdqa		%Xi, %T1;			\
	pslldq		$8, %Xi;			\
	psrldq		$8, %T1;			\
	pxor		%T2, %Xi;			\
	pxor		%T1, %Xhi;			\
	/* 2nd phase */					\
	movdqa		%Xi, %T2;			\
	psrlq		$1, %Xi;			\
	pxor		%T2, %Xhi;			\
	pxor		%Xi, %T2;			\
	psrlq		$5, %Xi;			\
	pxor		%T2, %Xi;			\
	psrlq		$1, %Xi;			\
	pxor		%Xhi, %Xi

#define	Xip	rdi
#define	Htbl	rsi
#define	inp	rdx
#define	len	rcx

#define	Xln	xmm6
#define	Xmn	xmm7
#define	Xhn	xmm8
#define	Hkey2	xmm9
#define	HK	xmm10
#define	Xl	xmm11
#define	Xm	xmm12
#define	Xh	xmm13
#define	Hkey3	xmm14
#define	Hkey4	xmm15

/*
 * void gcm_ghash_clmul(uint64_t Xi[2], const uint64_t Htable[32],
 *	const uint8_t *inp, size_t len)
 */
ENTRY_NP(gcm_ghash_clmul)
	movdqa		.Lbyte_swap16_mask(%rip), %T3
	mov		$0xA040608020C0E000, %rax	/ ((7..0) ? 0xE0) & 0xff

	movdqu		(%Xip), %Xi
	movdqu		(%Htbl), %Hkey
	movdqu		0x20(%Htbl), %HK
	pshufb		%T3, %Xi

	sub		$0x10, %len
	jz		.Lodd_tail

	movdqu		0x10(%Htbl), %Hkey2

	cmp		$0x30, %len
	jb		.Lskip4x

	sub		$0x30, %len
	movdqu		0x30(%Htbl),%Hkey3
	movdqu		0x40(%Htbl),%Hkey4

	/* Xi+4 =[(H*Ii+3) + (H^2*Ii+2) + (H^3*Ii+1) + H^4*(Ii+Xi)] mod P */
	movdqu		0x30(%inp), %Xln
	movdqu		0x20(%inp), %Xl
	pshufb		%T3, %Xln
	pshufb		%T3, %Xl
	movdqa		%Xln, %Xhn
	pshufd		$0b01001110, %Xln, %Xmn
	pxor		%Xln, %Xmn
	pclmulqdq	$0x00, %Hkey, %Xln
	pclmulqdq	$0x11, %Hkey, %Xhn
	pclmulqdq	$0x00, %HK, %Xmn

	movdqa		%Xl, %Xh
	pshufd		$0b01001110, %Xl, %Xm
	pxor		%Xl, %Xm
	pclmulqdq	$0x00, %Hkey2, %Xl
	pclmulqdq	$0x11, %Hkey2, %Xh
	xorps		%Xl, %Xln
	pclmulqdq	$0x10, %HK, %Xm
	xorps		%Xh, %Xhn
	movups		0x50(%Htbl), %HK
	xorps		%Xm, %Xmn

	movdqu		0x10(%inp), %Xl
	movdqu		0x00(%inp), %T1
	pshufb		%T3, %Xl
	pshufb		%T3, %T1
	movdqa		%Xl, %Xh
	pshufd		$0b01001110, %Xl, %Xm
	pxor		%T1, %Xi
	pxor		%Xl, %Xm
	pclmulqdq	$0x00, %Hkey3, %Xl
	movdqa		%Xi, %Xhi
	pshufd		$0b01001110, %Xi, %T1
	pxor		%Xi, %T1
	pclmulqdq	$0x11, %Hkey3, %Xh
	xorps		%Xl, %Xln
	pclmulqdq	$0x00, %HK, %Xm
	xorps		%Xh, %Xhn

	lea		0x40(%inp), %inp
	sub		$0x40, %len
	jc		.Ltail4x

	jmp		.Lmod4_loop

.align	32
.Lmod4_loop:
	pclmulqdq	$0x00, %Hkey4, %Xi
	xorps		%Xm, %Xmn
	movdqu		0x30(%inp), %Xl
	pshufb		%T3, %Xl
	pclmulqdq	$0x11, %Hkey4, %Xhi
	xorps		%Xln, %Xi
	movdqu		0x20(%inp), %Xln
	movdqa		%Xl, %Xh
	pshufd		$0b01001110, %Xl, %Xm
	pclmulqdq	$0x10, %HK, %T1
	xorps		%Xhn, %Xhi
	pxor		%Xl, %Xm
	pshufb		%T3, %Xln
	movups		0x20(%Htbl), %HK
	pclmulqdq	$0x00, %Hkey, %Xl
	xorps		%Xmn, %T1
	movdqa		%Xln, %Xhn
	pshufd		$0b01001110, %Xln, %Xmn

	pxor		%Xi, %T1	/ aggregated Karatsuba post-processing
	pxor		%Xln, %Xmn
	pxor		%Xhi, %T1
	movdqa		%T1, %T2
	pslldq		$8, %T1
	pclmulqdq	$0x11, %Hkey, %Xh
	psrldq		$8, %T2
	pxor		%T1, %Xi
	movdqa		.L7_mask(%rip), %T1
	pxor		%T2, %Xhi
	movq		%rax, %T2

	pand		%Xi, %T1	/ 1st phase
	pshufb		%T1, %T2
	pclmulqdq	$0x00, %HK, %Xm
	pxor		%Xi, %T2
	psllq		$57, %T2
	movdqa		%T2, %T1
	pslldq		$8, %T2
	pclmulqdq	$0x00, %Hkey2, %Xln
	psrldq		$8, %T1
	pxor		%T2, %Xi
	pxor		%T1, %Xhi
	movdqu		0(%inp), %T1

	movdqa		%Xi, %T2	/ 2nd phase
	psrlq		$1, %Xi
	pclmulqdq	$0x11, %Hkey2, %Xhn
	xorps		%Xl, %Xln
	movdqu		0x10(%inp), %Xl
	pshufb		%T3, %Xl
	pclmulqdq	$0x10, %HK, %Xmn
	xorps		%Xh, %Xhn
	movups		0x50(%Htbl), %HK
	pshufb		%T3, %T1
	pxor		%T2, %Xhi
	pxor		%Xi, %T2
	psrlq		$5, %Xi

	movdqa		%Xl, %Xh
	pxor		%Xm, %Xmn
	pshufd		$0b01001110, %Xl, %Xm
	pxor		%Xl, %Xm
	pclmulqdq	$0x00, %Hkey3, %Xl
	pxor		%T2, %Xi
	pxor		%T1, %Xhi
	psrlq		$1, %Xi
	pclmulqdq	$0x11, %Hkey3, %Xh
	xorps		%Xl, %Xln
	pxor		%Xhi, %Xi

	pclmulqdq	$0x00, %HK, %Xm
	xorps		%Xh, %Xhn

	movdqa		%Xi, %Xhi
	pshufd		$0b01001110, %Xi, %T1
	pxor		%Xi, %T1

	lea		0x40(%inp), %inp
	sub		$0x40, %len
	jnc		.Lmod4_loop

.Ltail4x:
	pclmulqdq	$0x00, %Hkey4, %Xi
	xorps		%Xm, %Xmn
	pclmulqdq	$0x11, %Hkey4, %Xhi
	xorps		%Xln, %Xi
	pclmulqdq	$0x10, %HK, %T1
	xorps		%Xhn, %Xhi
	pxor		%Xi, %Xhi	/ aggregated Karatsuba post-processing
	pxor		%Xmn, %T1

	pxor		%Xhi, %T1
	pxor		%Xi, %Xhi

	movdqa		%T1, %T2
	psrldq		$8, %T1
	pslldq		$8, %T2
	pxor		%T1, %Xhi
	pxor		%T2, %Xi

	reduction_alg9

	add	$0x40, %len
	jz	.Ldone
	movdqu	0x20(%Htbl), %HK
	sub	$0x10, %len
	jz	.Lodd_tail
.Lskip4x:

	/*
	 * Xi+2 =[H*(Ii+1 + Xi+1)] mod P =
	 *	[(H*Ii+1) + (H*Xi+1)] mod P =
	 *	[(H*Ii+1) + H^2*(Ii+Xi)] mod P
	 */
	movdqu		(%inp), %T1		/ Ii
	movdqu		16(%inp), %Xln		/ Ii+1
	pshufb		%T3, %T1
	pshufb		%T3, %Xln
	pxor		%T1, %Xi		/ Ii+Xi

	movdqa		%Xln, %Xhn
	pshufd		$0b01001110, %Xln, %T1
	pxor		%Xln, %T1
	pclmulqdq	$0x00, %Hkey, %Xln
	pclmulqdq	$0x11, %Hkey, %Xhn
	pclmulqdq	$0x00, %HK, %T1

	lea		32(%inp), %inp		/ i+=2
	sub		$0x20, %len
	jbe		.Leven_tail
	jmp		.Lmod_loop

.align	32
.Lmod_loop:
	movdqa		%Xi, %Xhi
	pshufd		$0b01001110, %Xi, %T2
	pxor		%Xi, %T2

	pclmulqdq	$0x00, %Hkey2, %Xi
	pclmulqdq	$0x11, %Hkey2, %Xhi
	pclmulqdq	$0x10, %HK, %T2

	pxor		%Xln, %Xi		/ (H*Ii+1) + H^2*(Ii+Xi)
	pxor		%Xhn, %Xhi
	movdqu		(%inp), %Xhn		/ Ii
	pshufb		%T3, %Xhn
	movdqu		16(%inp), %Xln		/ Ii+1

	pxor		%Xi, %T1		/ aggregated Karatsuba post-proc
	pxor		%Xhi, %T1
	pxor		%Xhn, %Xhi		/ "Ii+Xi",  consume early
	pxor		%T1, %T2
	pshufb		%T3, %Xln
	movdqa		%T2, %T1
	psrldq		$8, %T1
	pslldq		$8, %T2
	pxor		%T1, %Xhi
	pxor		%T2, %Xi

	movdqa		%Xln, %Xhn

	movdqa		%Xi, %T2		/ 1st phase
	movdqa		%Xi, %T1
	psllq		$5, %Xi
	pclmulqdq	$0x00, %Hkey, %Xln
	pxor		%Xi, %T1
	psllq		$1, %Xi
	pxor		%T1, %Xi
	psllq		$57, %Xi
	movdqa		%Xi, %T1
	pslldq		$8, %Xi
	psrldq		$8, %T1
	pxor		%T2, %Xi
	pxor		%T1, %Xhi
	pshufd		$0b01001110, %Xhn, %T1
	pxor		%Xhn, %T1

	pclmulqdq	$0x11, %Hkey, %Xhn
	movdqa		%Xi, %T2		/ 2nd phase
	psrlq		$1, %Xi
	pxor		%T2, %Xhi
	pxor		%Xi, %T2
	psrlq		$5, %Xi
	pxor		%T2, %Xi
	psrlq		$1, %Xi
	pclmulqdq	$0x00, %HK, %T1
	pxor		%Xhi, %Xi

	lea		32(%inp), %inp
	sub		$0x20, %len
	ja		.Lmod_loop

.Leven_tail:
	movdqa		%Xi, %Xhi
	pshufd		$0b01001110, %Xi, %T2
	pxor		%Xi, %T2

	pclmulqdq	$0x00, %Hkey2, %Xi
	pclmulqdq	$0x11, %Hkey2, %Xhi
	pclmulqdq	$0x10, %HK, %T2

	pxor		%Xln, %Xi		/* (H*Ii+1) + H^2*(Ii+Xi) */
	pxor		%Xhn, %Xhi
	pxor		%Xi, %T1
	pxor		%Xhi, %T1
	pxor		%T1, %T2
	movdqa		%T2, %T1
	psrldq		$8, %T1
	pslldq		$8, %T2
	pxor		%T1, %Xhi
	pxor		%T2, %Xi

	reduction_alg9

	test		%len, %len
	jnz		.Ldone

.Lodd_tail:
	movdqu		(%inp), %T1			/ Ii
	pshufb		%T3, %T1
	pxor		%T1, %Xi			/ Ii+Xi

	clmul64x64_T2(HK)				/ H*(Ii+Xi)
	reduction_alg9

.Ldone:
	pshufb		%T3, %Xi
	movdqu		%Xi, (%Xip)

	ret
	SET_SIZE(gcm_ghash_clmul)

/*
 * void gcm_init_clmul(const void *Xi, void *Htable)
 */
ENTRY_NP(gcm_init_clmul)
	movdqu		(%Xip), %Hkey
	pshufd		$0b01001110, %Hkey, %Hkey	/ dword swap

	/ <<1 twist
	pshufd		$0b11111111, %Hkey, %T2	/ broadcast uppermost dword
	movdqa		%Hkey, %T1
	psllq		$1, %Hkey
	pxor		%T3, %T3
	psrlq		$63, %T1
	pcmpgtd		%T2, %T3		/ broadcast carry bit
	pslldq		$8, %T1
	por		%T1, %Hkey		/ H<<=1

	/ magic reduction
	pand		.L0x1c2_polynomial(%rip), %T3
	pxor		%T3, %Hkey		/ if(carry) H^=0x1c2_polynomial

	/ calculate H^2
	pshufd		$0b01001110, %Hkey, %HK
	movdqa		%Hkey, %Xi
	pxor		%Hkey, %HK

	clmul64x64_T2(HK)
	reduction_alg9

	pshufd		$0b01001110, %Hkey, %T1
	pshufd		$0b01001110, %Xi, %T2
	pxor		%Hkey, %T1		/ Karatsuba pre-processing
	movdqu		%Hkey, 0x00(%Htbl)	/ save H
	pxor		%Xi, %T2		/ Karatsuba pre-processing
	movdqu		%Xi, 0x10(%Htbl)	/ save H^2
	palignr		$8, %T1, %T2		/ low part is H.lo^H.hi...
	movdqu		%T2, 0x20(%Htbl)	/ save Karatsuba "salt"

	clmul64x64_T2(HK)			/ H^3
	reduction_alg9

	movdqa		%Xi, %T3

	clmul64x64_T2(HK)			/ H^4
	reduction_alg9

	pshufd		$0b01001110, %T3, %T1
	pshufd		$0b01001110, %Xi, %T2
	pxor		%T3, %T1		/ Karatsuba pre-processing
	movdqu		%T3, 0x30(%Htbl)	/ save H^3
	pxor		%Xi, %T2		/ Karatsuba pre-processing
	movdqu		%Xi, 0x40(%Htbl)	/ save H^4
	palignr		$8, %T1, %T2		/ low part is H^3.lo^H^3.hi...
	movdqu		%T2, 0x50(%Htbl)	/ save Karatsuba "salt"

	ret
	SET_SIZE(gcm_init_clmul)

#endif	/* lint || __lint */
