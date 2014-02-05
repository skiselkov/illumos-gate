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

#ifndef	_DES_IMPL_H
#define	_DES_IMPL_H

/*
 * Common definitions used by DES
 */

#ifdef	__cplusplus
extern "C" {
#endif

#define	DES_BLOCK_LEN	8

#define	DES_COPY_BLOCK(src, dst) \
	(dst)[0] = (src)[0]; \
	(dst)[1] = (src)[1]; \
	(dst)[2] = (src)[2]; \
	(dst)[3] = (src)[3]; \
	(dst)[4] = (src)[4]; \
	(dst)[5] = (src)[5]; \
	(dst)[6] = (src)[6]; \
	(dst)[7] = (src)[7];

#define	DES_XOR_BLOCK(src, dst) \
	(dst)[0] ^= (src)[0]; \
	(dst)[1] ^= (src)[1]; \
	(dst)[2] ^= (src)[2]; \
	(dst)[3] ^= (src)[3]; \
	(dst)[4] ^= (src)[4]; \
	(dst)[5] ^= (src)[5]; \
	(dst)[6] ^= (src)[6]; \
	(dst)[7] ^= (src)[7]

typedef enum des_strength {
	DES = 1,
	DES2,
	DES3
} des_strength_t;

#define	DES3_STRENGTH	0x08000000

#define	DES_KEYSIZE	8
#define	DES_MINBITS	64
#define	DES_MAXBITS	64
#define	DES_MINBYTES	(DES_MINBITS / 8)
#define	DES_MAXBYTES	(DES_MAXBITS / 8)
#define	DES_IV_LEN	8

#define	DES2_KEYSIZE	(2 * DES_KEYSIZE)
#define	DES2_MINBITS	(2 * DES_MINBITS)
#define	DES2_MAXBITS	(2 * DES_MAXBITS)
#define	DES2_MINBYTES	(DES2_MINBITS / 8)
#define	DES2_MAXBYTES	(DES2_MAXBITS / 8)

#define	DES3_KEYSIZE	(3 * DES_KEYSIZE)
#define	DES3_MINBITS	(2 * DES_MINBITS)	/* DES3 handles CKK_DES2 keys */
#define	DES3_MAXBITS	(3 * DES_MAXBITS)
#define	DES3_MINBYTES	(DES3_MINBITS / 8)
#define	DES3_MAXBYTES	(DES3_MAXBITS / 8)

extern int des_encrypt_contiguous_blocks(void *, char *, size_t,
    crypto_data_t *);
extern int des_decrypt_contiguous_blocks(void *, char *, size_t,
    crypto_data_t *);
extern uint64_t des_crypt_impl(uint64_t *, uint64_t, int);
extern void des_ks(uint64_t *, uint64_t);
extern int des_crunch_block(const void *, const uint8_t *, uint8_t *,
    boolean_t);
extern int des3_crunch_block(const void *, const uint8_t *, uint8_t *,
    boolean_t);
extern void des_init_keysched(uint8_t *, des_strength_t, void *);
extern void *des_alloc_keysched(size_t *, des_strength_t, int);
extern boolean_t des_keycheck(uint8_t *, des_strength_t, uint8_t *);
extern void des_parity_fix(uint8_t *, des_strength_t, uint8_t *);
extern void des_copy_block(const uint8_t *, uint8_t *);
extern void des_xor_block(const uint8_t *, uint8_t *);
extern int des_encrypt_block(const void *, const uint8_t *, uint8_t *);
extern int des3_encrypt_block(const void *, const uint8_t *, uint8_t *);
extern int des_decrypt_block(const void *, const uint8_t *, uint8_t *);
extern int des3_decrypt_block(const void *, const uint8_t *, uint8_t *);

#ifdef _DES_IMPL

#ifdef _KERNEL
typedef enum des_mech_type {
	DES_ECB_MECH_INFO_TYPE,		/* SUN_CKM_DES_ECB */
	DES_CBC_MECH_INFO_TYPE,		/* SUN_CKM_DES_CBC */
	DES_CFB_MECH_INFO_TYPE,		/* SUN_CKM_DES_CFB */
	DES3_ECB_MECH_INFO_TYPE,	/* SUN_CKM_DES3_ECB */
	DES3_CBC_MECH_INFO_TYPE,	/* SUN_CKM_DES3_CBC */
	DES3_CFB_MECH_INFO_TYPE		/* SUN_CKM_DES3_CFB */
} des_mech_type_t;

#endif	/* _KERNEL */
#endif	/* _DES_IMPL */

#ifdef	__cplusplus
}
#endif

#endif	/* _DES_IMPL_H */
