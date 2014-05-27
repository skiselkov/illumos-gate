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
 * Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
/*
 * Copyright 2014 by Saso Kiselkov. All rights reserved.
 */

#ifndef	_AES_IMPL_H
#define	_AES_IMPL_H

/*
 * Common definitions used by AES.
 */

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/crypto/common.h>

/* Similar to sysmacros.h IS_P2ALIGNED, but checks two pointers: */
#define	IS_P2ALIGNED2(v, w, a) \
	((((uintptr_t)(v) | (uintptr_t)(w)) & ((uintptr_t)(a) - 1)) == 0)

#define	AES_BLOCK_LEN	16	/* bytes */
/* Round constant length, in number of 32-bit elements: */
#define	RC_LENGTH	(5 * ((AES_BLOCK_LEN) / 4 - 2))

#define	AES_COPY_BLOCK_UNALIGNED(src, dst) \
	(dst)[0] = (src)[0]; \
	(dst)[1] = (src)[1]; \
	(dst)[2] = (src)[2]; \
	(dst)[3] = (src)[3]; \
	(dst)[4] = (src)[4]; \
	(dst)[5] = (src)[5]; \
	(dst)[6] = (src)[6]; \
	(dst)[7] = (src)[7]; \
	(dst)[8] = (src)[8]; \
	(dst)[9] = (src)[9]; \
	(dst)[10] = (src)[10]; \
	(dst)[11] = (src)[11]; \
	(dst)[12] = (src)[12]; \
	(dst)[13] = (src)[13]; \
	(dst)[14] = (src)[14]; \
	(dst)[15] = (src)[15]

#define	AES_XOR_BLOCK_UNALIGNED(src, dst) \
	(dst)[0] ^= (src)[0]; \
	(dst)[1] ^= (src)[1]; \
	(dst)[2] ^= (src)[2]; \
	(dst)[3] ^= (src)[3]; \
	(dst)[4] ^= (src)[4]; \
	(dst)[5] ^= (src)[5]; \
	(dst)[6] ^= (src)[6]; \
	(dst)[7] ^= (src)[7]; \
	(dst)[8] ^= (src)[8]; \
	(dst)[9] ^= (src)[9]; \
	(dst)[10] ^= (src)[10]; \
	(dst)[11] ^= (src)[11]; \
	(dst)[12] ^= (src)[12]; \
	(dst)[13] ^= (src)[13]; \
	(dst)[14] ^= (src)[14]; \
	(dst)[15] ^= (src)[15]

#define	AES_COPY_BLOCK_ALIGNED(src, dst) \
	((uint64_t *)(void *)(dst))[0] = ((uint64_t *)(void *)(src))[0]; \
	((uint64_t *)(void *)(dst))[1] = ((uint64_t *)(void *)(src))[1]

#define	AES_XOR_BLOCK_ALIGNED(src, dst) \
	((uint64_t *)(void *)(dst))[0] ^= ((uint64_t *)(void *)(src))[0]; \
	((uint64_t *)(void *)(dst))[1] ^= ((uint64_t *)(void *)(src))[1]

/* AES key size definitions */
#define	AES_MINBITS		128
#define	AES_MINBYTES		((AES_MINBITS) >> 3)
#define	AES_MAXBITS		256
#define	AES_MAXBYTES		((AES_MAXBITS) >> 3)

#define	AES_MIN_KEY_BYTES	((AES_MINBITS) >> 3)
#define	AES_MAX_KEY_BYTES	((AES_MAXBITS) >> 3)
#define	AES_192_KEY_BYTES	24
#define	AES_IV_LEN		16

/* AES key schedule may be implemented with 32- or 64-bit elements: */
#define	AES_32BIT_KS		32
#define	AES_64BIT_KS		64

#define	MAX_AES_NR		14 /* Maximum number of rounds */
#define	MAX_AES_NB		4  /* Number of columns comprising a state */

/*
 * Architecture-specific acceleration support autodetection.
 * Some architectures provide hardware-assisted acceleration using floating
 * point registers, which need special handling inside of the kernel, so the
 * macros below define the auxiliary functions needed to utilize them.
 */
#if	defined(__amd64) && defined(_KERNEL)
/*
 * Using floating point registers requires temporarily disabling kernel
 * thread preemption, so we need to operate on small-enough chunks to
 * prevent scheduling latency bubbles.
 * A typical 64-bit CPU can sustain around 300-400MB/s/core even in the
 * slowest encryption modes (CBC), which with 32k per run works out to ~100us
 * per run. CPUs with AES-NI in fast modes (ECB, CTR, CBC decryption) can
 * easily sustain 3GB/s/core, so the latency potential essentially vanishes.
 */
#define	AES_OPSZ	32768

#if	defined(lint) || defined(__lint)
#define	AES_ACCEL_SAVESTATE(name)	uint8_t name[16 * 16 + 8]
#else	/* lint || __lint */
#define	AES_ACCEL_SAVESTATE(name) \
	/* stack space for xmm0--xmm15 and cr0 (16 x 128 bits + 64 bits) */ \
	uint8_t name[16 * 16 + 8] __attribute__((aligned(16)))
#endif	/* lint || __lint */

#else	/* !defined(__amd64) || !defined(_KERNEL) */
/*
 * All other accel support
 */
#define	AES_OPSZ	((uint64_t)-1)
/* On other architectures or outside of the kernel these get stubbed out */
#define	AES_ACCEL_SAVESTATE(name)
#define	aes_accel_enter(savestate)
#define	aes_accel_exit(savestate)
#endif	/* !defined(__amd64) || !defined(_KERNEL) */

typedef union {
#ifdef	sun4u
	uint64_t	ks64[((MAX_AES_NR) + 1) * (MAX_AES_NB)];
#endif
	uint32_t	ks32[((MAX_AES_NR) + 1) * (MAX_AES_NB)];
} aes_ks_t;

typedef struct aes_key aes_key_t;
struct aes_key {
	aes_ks_t	encr_ks;  /* encryption key schedule */
	aes_ks_t	decr_ks;  /* decryption key schedule */
#ifdef __amd64
	long double	align128; /* Align fields above for Intel AES-NI */
#endif	/* __amd64 */
	int		nr;	  /* number of rounds (10, 12, or 14) */
	int		type;	  /* key schedule size (32 or 64 bits) */
};

/*
 * Core AES functions.
 * ks and keysched are pointers to aes_key_t.
 * They are declared void* as they are intended to be opaque types.
 * Use function aes_alloc_keysched() to allocate memory for ks and keysched.
 */
extern void *aes_alloc_keysched(size_t *size, int kmflag);
extern void aes_init_keysched(const uint8_t *cipherKey, uint_t keyBits,
	void *keysched);
extern int aes_encrypt_block(const void *ks, const uint8_t *pt, uint8_t *ct);
extern int aes_decrypt_block(const void *ks, const uint8_t *ct, uint8_t *pt);
extern int aes_encrypt_ecb(const void *ks, const uint8_t *pt, uint8_t *ct,
    uint64_t length);
extern int aes_decrypt_ecb(const void *ks, const uint8_t *pt, uint8_t *ct,
    uint64_t length);
extern int aes_encrypt_cbc(const void *ks, const uint8_t *pt, uint8_t *ct,
    const uint8_t *iv, uint64_t length);
extern int aes_ctr_mode(const void *ks, const uint8_t *pt, uint8_t *ct,
    uint64_t length, uint64_t counter[2]);

/*
 * AES mode functions.
 * The first 2 functions operate on 16-byte AES blocks.
 */
#ifdef	__amd64
#define	AES_COPY_BLOCK	aes_copy_intel
#define	AES_XOR_BLOCK	aes_xor_intel
extern void aes_copy_intel(const uint8_t *src, uint8_t *dst);
extern void aes_xor_intel(const uint8_t *src, uint8_t *dst);
extern void aes_xor_intel8(const uint8_t *src, uint8_t *dst);
#else	/* !__amd64 */
#define	AES_COPY_BLOCK	aes_copy_block
#define	AES_XOR_BLOCK	aes_xor_block
extern void aes_copy_block(const uint8_t *src, uint8_t *dst);
extern void aes_xor_block(const uint8_t *src, uint8_t *dst);
#endif	/* !__amd64 */

/* Note: ctx is a pointer to aes_ctx_t defined in modes.h */
extern int aes_encrypt_contiguous_blocks(void *ctx, char *data, size_t length,
    crypto_data_t *out);
extern int aes_decrypt_contiguous_blocks(void *ctx, char *data, size_t length,
    crypto_data_t *out);

#ifdef	__amd64
/*
 * When AES floating-point acceleration is available, these will be called
 * by the worker functions to clear and restore floating point state and
 * control kernel thread preemption.
 */
void aes_accel_enter(void *savestate);
void aes_accel_exit(void *savestate);
#endif	/* __amd64 */

/*
 * The following definitions and declarations are only used by AES FIPS POST
 */
#ifdef _AES_IMPL

#ifdef _KERNEL
typedef enum aes_mech_type {
	AES_ECB_MECH_INFO_TYPE,		/* SUN_CKM_AES_ECB */
	AES_CBC_MECH_INFO_TYPE,		/* SUN_CKM_AES_CBC */
	AES_CBC_PAD_MECH_INFO_TYPE,	/* SUN_CKM_AES_CBC_PAD */
	AES_CTR_MECH_INFO_TYPE,		/* SUN_CKM_AES_CTR */
	AES_CCM_MECH_INFO_TYPE,		/* SUN_CKM_AES_CCM */
	AES_GCM_MECH_INFO_TYPE,		/* SUN_CKM_AES_GCM */
	AES_GMAC_MECH_INFO_TYPE		/* SUN_CKM_AES_GMAC */
} aes_mech_type_t;

#endif	/* _KERNEL */
#endif /* _AES_IMPL */

#ifdef	__cplusplus
}
#endif

#endif	/* _AES_IMPL_H */
