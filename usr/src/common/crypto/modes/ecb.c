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
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
/*
 * Copyright 2014 by Saso Kiselkov. All rights reserved.
 */

#ifndef _KERNEL
#include <strings.h>
#include <limits.h>
#include <assert.h>
#include <security/cryptoki.h>
#endif

#include <sys/types.h>
#define	INLINE_CRYPTO_GET_PTRS
#include <modes/modes.h>
#include <sys/crypto/common.h>
#include <sys/crypto/impl.h>

boolean_t ecb_fastpath_enabled = B_TRUE;

/*
 * Algorithm independent ECB functions.
 * `cipher' is a single-block version of the cipher function to be performed
 * on each input block. `cipher_ecb' is an optional parameter, which if
 * passed and the input/output conditions allow it, will be invoked for the
 * entire input buffer once to accelerate the operation.
 */
int
ecb_cipher_contiguous_blocks(ecb_ctx_t *ctx, char *data, size_t length,
    crypto_data_t *out, size_t block_size,
    int (*cipher)(const void *ks, const uint8_t *pt, uint8_t *ct),
    int (*cipher_ecb)(const void *ks, const uint8_t *pt, uint8_t *ct,
    uint64_t len))
{
	size_t remainder = length;
	size_t need;
	uint8_t *datap = (uint8_t *)data;
	uint8_t *blockp;
	uint8_t *lastp;
	void *iov_or_mp;
	offset_t offset;
	uint8_t *out_data_1;
	uint8_t *out_data_2;
	size_t out_data_1_len;

	/*
	 * ECB encryption/decryption fastpath requirements:
	 * - fastpath is enabled
	 * - caller passed an accelerated ECB version of the cipher function
	 * - input is block-aligned
	 * - output is a single contiguous region or the user requested that
	 *   we overwrite their input buffer (input/output aliasing allowed)
	 */
	if (ecb_fastpath_enabled && cipher_ecb != NULL &&
	    ctx->ecb_remainder_len == 0 && length % block_size == 0 &&
	    (out == NULL || CRYPTO_DATA_IS_SINGLE_BLOCK(out))) {
		if (out == NULL) {
			cipher_ecb(ctx->ecb_keysched, (uint8_t *)data,
			    (uint8_t *)data, length);
		} else {
			cipher_ecb(ctx->ecb_keysched, (uint8_t *)data,
			    CRYPTO_DATA_FIRST_BLOCK(out), length);
			out->cd_offset += length;
		}
		return (CRYPTO_SUCCESS);
	}

	if (length + ctx->ecb_remainder_len < block_size) {
		/* accumulate bytes here and return */
		bcopy(datap,
		    (uint8_t *)ctx->ecb_remainder + ctx->ecb_remainder_len,
		    length);
		ctx->ecb_remainder_len += length;
		ctx->ecb_copy_to = datap;
		return (CRYPTO_SUCCESS);
	}

	lastp = (uint8_t *)ctx->ecb_iv;
	if (out != NULL)
		crypto_init_ptrs(out, &iov_or_mp, &offset);

	do {
		/* Unprocessed data from last call. */
		if (ctx->ecb_remainder_len > 0) {
			need = block_size - ctx->ecb_remainder_len;

			if (need > remainder)
				return (CRYPTO_DATA_LEN_RANGE);

			bcopy(datap, &((uint8_t *)ctx->ecb_remainder)
			    [ctx->ecb_remainder_len], need);

			blockp = (uint8_t *)ctx->ecb_remainder;
		} else {
			blockp = datap;
		}

		if (out == NULL) {
			cipher(ctx->ecb_keysched, blockp, blockp);

			ctx->ecb_lastp = blockp;
			lastp = blockp;

			if (ctx->ecb_remainder_len > 0) {
				bcopy(blockp, ctx->ecb_copy_to,
				    ctx->ecb_remainder_len);
				bcopy(blockp + ctx->ecb_remainder_len, datap,
				    need);
			}
		} else {
			cipher(ctx->ecb_keysched, blockp, lastp);
			crypto_get_ptrs(out, &iov_or_mp, &offset, &out_data_1,
			    &out_data_1_len, &out_data_2, block_size);

			/* copy block to where it belongs */
			bcopy(lastp, out_data_1, out_data_1_len);
			if (out_data_2 != NULL) {
				bcopy(lastp + out_data_1_len, out_data_2,
				    block_size - out_data_1_len);
			}
			/* update offset */
			out->cd_offset += block_size;
		}

		/* Update pointer to next block of data to be processed. */
		if (ctx->ecb_remainder_len != 0) {
			datap += need;
			ctx->ecb_remainder_len = 0;
		} else {
			datap += block_size;
		}

		remainder = (size_t)&data[length] - (size_t)datap;

		/* Incomplete last block. */
		if (remainder > 0 && remainder < block_size) {
			bcopy(datap, ctx->ecb_remainder, remainder);
			ctx->ecb_remainder_len = remainder;
			ctx->ecb_copy_to = datap;
			goto out;
		}
		ctx->ecb_copy_to = NULL;

	} while (remainder > 0);

out:
	return (CRYPTO_SUCCESS);
}

/* ARGSUSED */
void *
ecb_alloc_ctx(int kmflag)
{
	ecb_ctx_t *ecb_ctx;

#ifdef _KERNEL
	if ((ecb_ctx = kmem_zalloc(sizeof (ecb_ctx_t), kmflag)) == NULL)
#else
	if ((ecb_ctx = calloc(1, sizeof (ecb_ctx_t))) == NULL)
#endif
		return (NULL);

	ecb_ctx->ecb_flags = ECB_MODE;
	return (ecb_ctx);
}
