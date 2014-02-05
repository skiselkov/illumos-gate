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

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <modes/modes.h>
#include "aes_impl.h"
#ifndef	_KERNEL
#include <stdlib.h>
#endif	/* !_KERNEL */

#ifdef	__amd64

/*
 * XORs a range of contiguous AES blocks in `data' with blocks in 'dst'
 * and places the result in `dst'. On x86-64 this exploits the 128-bit
 * floating point registers (xmm) to maximize performance.
 */
static void
aes_xor_range(const uint8_t *data, uint8_t *dst, uint64_t length)
{
	uint64_t i = 0;

	/* First use the unrolled version. */
	for (; i + 8 * AES_BLOCK_LEN <= length; i += 8 * AES_BLOCK_LEN)
		aes_xor_intel8(&data[i], &dst[i]);
	/* Finish the rest in single blocks. */
	for (; i < length; i += AES_BLOCK_LEN)
		aes_xor_intel(&data[i], &dst[i]);
}

#else	/* !__amd64 */

/* Copy a 16-byte AES block from "in" to "out" */
void
aes_copy_block(const uint8_t *in, uint8_t *out)
{
	if (IS_P2ALIGNED2(in, out, sizeof (uint32_t))) {
		AES_COPY_BLOCK_ALIGNED(in, out);
	} else {
		AES_COPY_BLOCK_UNALIGNED(in, out);
	}
}

/* XOR a 16-byte AES block of data into dst */
inline void
aes_xor_block(const uint8_t *data, uint8_t *dst)
{
	if (IS_P2ALIGNED2(dst, data, sizeof (uint32_t))) {
		AES_XOR_BLOCK_ALIGNED(data, dst);
	} else {
		AES_XOR_BLOCK_UNALIGNED(data, dst);
	}
}

/*
 * XORs a range of contiguous AES blocks in `data' with blocks in 'dst'
 * and places the result in `dst'.
 */
static void
aes_xor_range(const uint8_t *data, uint8_t *dst, uint64_t length)
{
	uint64_t i = 0;

	if (IS_P2ALIGNED2(dst, data, sizeof (uint64_t))) {
		/* Unroll the loop to enable efficiency. */
		for (; i + 8 * AES_BLOCK_LEN < length; i += 8 * AES_BLOCK_LEN) {
			AES_XOR_BLOCK_ALIGNED(&data[i + 0x00], &dst[i + 0x00]);
			AES_XOR_BLOCK_ALIGNED(&data[i + 0x10], &dst[i + 0x10]);
			AES_XOR_BLOCK_ALIGNED(&data[i + 0x20], &dst[i + 0x20]);
			AES_XOR_BLOCK_ALIGNED(&data[i + 0x30], &dst[i + 0x30]);
			AES_XOR_BLOCK_ALIGNED(&data[i + 0x40], &dst[i + 0x40]);
			AES_XOR_BLOCK_ALIGNED(&data[i + 0x50], &dst[i + 0x50]);
			AES_XOR_BLOCK_ALIGNED(&data[i + 0x60], &dst[i + 0x60]);
			AES_XOR_BLOCK_ALIGNED(&data[i + 0x70], &dst[i + 0x70]);
		}
	}
	/* Finish the rest in single blocks. */
	for (; i < length; i += AES_BLOCK_LEN)
		AES_XOR_BLOCK(&data[i], &dst[i]);
}

#endif	/* !__amd64 */

/*
 * Encrypt multiple blocks of data according to mode.
 */
int
aes_encrypt_contiguous_blocks(void *ctx, char *data, size_t length,
    crypto_data_t *out)
{
	aes_ctx_t *aes_ctx = ctx;
	int rv;

	for (size_t i = 0; i < length; i += AES_OPSZ) {
		size_t opsz = MIN(length - i, AES_OPSZ);
		AES_ACCEL_SAVESTATE(savestate);
		aes_accel_enter(savestate);

		if (aes_ctx->ac_flags & CTR_MODE) {
			rv = ctr_mode_contiguous_blocks(ctx, &data[i], opsz,
			    out, AES_BLOCK_LEN, aes_encrypt_block,
			    AES_XOR_BLOCK, aes_ctr_mode);
#ifdef _KERNEL
		} else if (aes_ctx->ac_flags & CCM_MODE) {
			rv = ccm_mode_encrypt_contiguous_blocks(ctx, &data[i],
			    opsz, out, AES_BLOCK_LEN, aes_encrypt_block,
			    AES_COPY_BLOCK, AES_XOR_BLOCK);
		} else if (aes_ctx->ac_flags & (GCM_MODE|GMAC_MODE)) {
			rv = gcm_mode_encrypt_contiguous_blocks(ctx, &data[i],
			    opsz, out, AES_BLOCK_LEN, aes_encrypt_block,
			    AES_COPY_BLOCK, AES_XOR_BLOCK, aes_ctr_mode);
#endif
		} else if (aes_ctx->ac_flags & CBC_MODE) {
			rv = cbc_encrypt_contiguous_blocks(ctx, &data[i], opsz,
			    out, AES_BLOCK_LEN, aes_encrypt_block,
			    AES_COPY_BLOCK, AES_XOR_BLOCK, aes_encrypt_cbc);
		} else {
			rv = ecb_cipher_contiguous_blocks(ctx, &data[i], opsz,
			    out, AES_BLOCK_LEN, aes_encrypt_block,
			    aes_encrypt_ecb);
		}

		aes_accel_exit(savestate);

		if (rv != CRYPTO_SUCCESS)
				break;
	}

	return (rv);
}

/*
 * Decrypt multiple blocks of data according to mode.
 */
int
aes_decrypt_contiguous_blocks(void *ctx, char *data, size_t length,
    crypto_data_t *out)
{
	aes_ctx_t *aes_ctx = ctx;
	int rv;


	for (size_t i = 0; i < length; i += AES_OPSZ) {
		size_t opsz = MIN(length - i, AES_OPSZ);
		AES_ACCEL_SAVESTATE(savestate);
		aes_accel_enter(savestate);

		if (aes_ctx->ac_flags & CTR_MODE) {
			rv = ctr_mode_contiguous_blocks(ctx, &data[i], opsz,
			    out, AES_BLOCK_LEN, aes_encrypt_block,
			    AES_XOR_BLOCK, aes_ctr_mode);
			if (rv == CRYPTO_DATA_LEN_RANGE)
				rv = CRYPTO_ENCRYPTED_DATA_LEN_RANGE;
#ifdef _KERNEL
		} else if (aes_ctx->ac_flags & CCM_MODE) {
			rv = ccm_mode_decrypt_contiguous_blocks(ctx, &data[i],
			    opsz, out, AES_BLOCK_LEN, aes_encrypt_block,
			    AES_COPY_BLOCK, AES_XOR_BLOCK);
		} else if (aes_ctx->ac_flags & (GCM_MODE|GMAC_MODE)) {
			rv = gcm_mode_decrypt_contiguous_blocks(ctx, &data[i],
			    opsz, out, AES_BLOCK_LEN, aes_encrypt_block,
			    AES_COPY_BLOCK, AES_XOR_BLOCK);
#endif
		} else if (aes_ctx->ac_flags & CBC_MODE) {
			rv = cbc_decrypt_contiguous_blocks(ctx, &data[i],
			    opsz, out, AES_BLOCK_LEN, aes_decrypt_block,
			    AES_COPY_BLOCK, AES_XOR_BLOCK, aes_decrypt_ecb,
			    aes_xor_range);
		} else {
			rv = ecb_cipher_contiguous_blocks(ctx, &data[i],
			    opsz, out, AES_BLOCK_LEN, aes_decrypt_block,
			    aes_decrypt_ecb);
			if (rv == CRYPTO_DATA_LEN_RANGE)
				rv = CRYPTO_ENCRYPTED_DATA_LEN_RANGE;
		}

		aes_accel_exit(savestate);

		if (rv != CRYPTO_SUCCESS)
				break;
	}

	return (rv);
}
