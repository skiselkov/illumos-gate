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
 * Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.
 */


#ifndef _KERNEL
#include <strings.h>
#include <limits.h>
#include <assert.h>
#include <security/cryptoki.h>
#endif	/* _KERNEL */

#include <sys/cmn_err.h>
#include <sys/types.h>
#include <sys/kmem.h>
#define	INLINE_CRYPTO_GET_PTRS
#include <modes/modes.h>
#include <sys/crypto/common.h>
#include <sys/crypto/impl.h>
#include <sys/byteorder.h>

#define	COUNTER_MASK	0x00000000ffffffffULL

#ifdef	_KERNEL
#include <sys/sdt.h>		/* SET_ERROR */
#endif	/* _KERNEL */

#define	PRINT_BLOCK(name, x)	cmn_err(CE_NOTE, name ": %016llx%016llx", \
	(unsigned long long) ntohll(x[0]), (unsigned long long) ntohll(x[1]))

#ifdef __amd64

#ifdef _KERNEL
#include <sys/cpuvar.h>		/* cpu_t, CPU */
#include <sys/x86_archext.h>	/* x86_featureset, X86FSET_*, CPUID_* */
#include <sys/disp.h>		/* kpreempt_disable(), kpreempt_enable */

/* Workaround for no XMM kernel thread save/restore */
extern void gcm_accel_save(void *savestate);
extern void gcm_accel_restore(void *savestate);

#if	defined(lint) || defined(__lint)
#define	GCM_ACCEL_SAVESTATE(name)	uint8_t name[16 * 16 + 8]
#else
#define	GCM_ACCEL_SAVESTATE(name) \
	/* stack space for xmm0--xmm15 and cr0 (16 x 128 bits + 64 bits) */ \
	uint8_t name[16 * 16 + 8] __attribute__((aligned(16)))
#endif

/*
 * Disables kernel thread preemption and conditionally gcm_accel_save() iff
 * Intel PCLMULQDQ support is present. Must be balanced by GCM_ACCEL_EXIT.
 * This must be present in all externally callable GCM functions which
 * invoke GHASH operations using FPU-accelerated implementations, or call
 * static functions which do (such as gcm_encrypt_fastpath128()).
 */
#define	GCM_ACCEL_ENTER \
	GCM_ACCEL_SAVESTATE(savestate); \
	do { \
		if (intel_pclmulqdq_instruction_present()) { \
			kpreempt_disable(); \
			gcm_accel_save(savestate); \
		} \
		_NOTE(CONSTCOND) \
	} while (0)
#define	GCM_ACCEL_EXIT \
	do { \
		if (intel_pclmulqdq_instruction_present()) { \
			gcm_accel_restore(savestate); \
			kpreempt_enable(); \
		} \
		_NOTE(CONSTCOND) \
	} while (0)

#else	/* _KERNEL */
#include <sys/auxv.h>		/* getisax() */
#include <sys/auxv_386.h>	/* AV_386_PCLMULQDQ bit */
#define	SET_ERROR(x)	(x)
#endif	/* _KERNEL */

extern void gcm_mul_pclmulqdq(uint64_t *x_in, uint64_t *y, uint64_t *res);
extern void gcm_init_clmul(const uint64_t hash_init[2], uint8_t Htable[256]);
extern void gcm_ghash_clmul(uint64_t ghash[2], const uint8_t Htable[256],
    const uint8_t *inp, size_t length);
static inline int intel_pclmulqdq_instruction_present(void);
#else	/* !__amd64 */
#define	GCM_ACCEL_ENTER
#define	GCM_ACCEL_EXIT
#endif	/* !__amd64 */

struct aes_block {
	uint64_t a;
	uint64_t b;
};


/*
 * gcm_mul()
 * Perform a carry-less multiplication (that is, use XOR instead of the
 * multiply operator) on *x_in and *y and place the result in *res.
 *
 * Byte swap the input (*x_in and *y) and the output (*res).
 *
 * Note: x_in, y, and res all point to 16-byte numbers (an array of two
 * 64-bit integers).
 */
static inline void
gcm_mul(uint64_t *x_in, uint64_t *y, uint64_t *res)
{
#ifdef __amd64
	if (intel_pclmulqdq_instruction_present()) {
		/*
		 * FPU context will have been saved and kernel thread
		 * preemption disabled already.
		 */
		gcm_mul_pclmulqdq(x_in, y, res);
	} else
#endif	/* __amd64 */
	{
		static const uint64_t R = 0xe100000000000000ULL;
		struct aes_block z = {0, 0};
		struct aes_block v;
		uint64_t x;
		int i, j;

		v.a = ntohll(y[0]);
		v.b = ntohll(y[1]);

		for (j = 0; j < 2; j++) {
			x = ntohll(x_in[j]);
			for (i = 0; i < 64; i++, x <<= 1) {
				if (x & 0x8000000000000000ULL) {
					z.a ^= v.a;
					z.b ^= v.b;
				}
				if (v.b & 1ULL) {
					v.b = (v.a << 63)|(v.b >> 1);
					v.a = (v.a >> 1) ^ R;
				} else {
					v.b = (v.a << 63)|(v.b >> 1);
					v.a = v.a >> 1;
				}
			}
		}
		res[0] = htonll(z.a);
		res[1] = htonll(z.b);
	}
}

#define	GHASH(c, d, t) \
	do { \
		xor_block((uint8_t *)(d), (uint8_t *)(c)->gcm_ghash); \
		gcm_mul((uint64_t *)(void *)(c)->gcm_ghash, (c)->gcm_H, \
		    (uint64_t *)(void *)(t)); \
		_NOTE(CONSTCOND) \
	} while (0)

boolean_t gcm_fastpath_enabled = B_TRUE;
boolean_t gcm_fast_enabled = B_TRUE;

static void
gcm_fastpath128(gcm_ctx_t *ctx, const uint8_t *data, size_t length,
    uint8_t *out, boolean_t encrypt,
    int (*encrypt_block)(const void *, const uint8_t *, uint8_t *),
    void (*xor_block)(const uint8_t *, uint8_t *),
    int (*cipher_ctr)(const void *, const uint8_t *, uint8_t *, uint64_t,
    uint64_t *))
{
	if (!encrypt) {
#ifdef	__amd64
		if (intel_pclmulqdq_instruction_present())
			gcm_ghash_clmul(ctx->gcm_ghash, ctx->gcm_H_table,
			    data, length);
		else
#endif	/* __amd64 */
			for (size_t i = 0; i < length; i += 16) {
				GHASH(ctx, &data[i], ctx->gcm_ghash);
		}
	}

	if (cipher_ctr != NULL) {
		/*
		 * GCM is almost but not quite like CTR. GCM increments the
		 * counter value *before* processing the first input block,
		 * whereas CTR does so afterwards. So we need to increment
		 * the counter before calling CTR and decrement it afterwards.
		 */
		uint64_t counter = ntohll(ctx->gcm_cb[1]);

		ctx->gcm_cb[1] = htonll((counter & ~COUNTER_MASK) |
		    ((counter & COUNTER_MASK) + 1));
		cipher_ctr(ctx->gcm_keysched, data, out, length, ctx->gcm_cb);
		counter = ntohll(ctx->gcm_cb[1]);
		ctx->gcm_cb[1] = htonll((counter & ~COUNTER_MASK) |
		    ((counter & COUNTER_MASK) - 1));
	} else {
		uint64_t counter = ntohll(ctx->gcm_cb[1]);

		for (size_t i = 0; i < length; i += 16) {
			/*LINTED(E_BAD_PTR_CAST_ALIGN)*/
			*(uint64_t *)&out[i] = ctx->gcm_cb[0];
			/*LINTED(E_BAD_PTR_CAST_ALIGN)*/
			*(uint64_t *)&out[i + 8] = htonll(counter++);
			encrypt_block(ctx->gcm_keysched, &out[i], &out[i]);
			xor_block(&data[i], &out[i]);
		}

		ctx->gcm_cb[1] = htonll(counter);
	}

	if (encrypt) {
#ifdef	__amd64
		if (intel_pclmulqdq_instruction_present())
			gcm_ghash_clmul(ctx->gcm_ghash, ctx->gcm_H_table,
			    out, length);
		else
#endif	/* __amd64 */
			for (size_t i = 0; i < length; i += 16)
				GHASH(ctx, &out[i], ctx->gcm_ghash);

		/* Copy a potential auth tag into the temp buffer */
		bcopy(&out[length - 16], ctx->gcm_tmp, 16);
	}

	ctx->gcm_processed_data_len += length;
}

static int
gcm_process_contiguous_blocks(gcm_ctx_t *ctx, char *data, size_t length,
    crypto_data_t *out, size_t block_size, boolean_t encrypt,
    int (*encrypt_block)(const void *, const uint8_t *, uint8_t *),
    void (*copy_block)(const uint8_t *, uint8_t *),
    void (*xor_block)(const uint8_t *, uint8_t *),
    int (*cipher_ctr)(const void *, const uint8_t *, uint8_t *, uint64_t,
    uint64_t *))
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
	uint64_t counter;
	uint64_t counter_mask = ntohll(0x00000000ffffffffULL);
	int rv = CRYPTO_SUCCESS;

	GCM_ACCEL_ENTER;

	/*
	 * GCM mode fastpath requirements:
	 * - fastpath is enabled
	 * - block size is 128 bits
	 * - input is block-aligned
	 * - the counter value won't overflow
	 * - output is a single contiguous region and doesn't alias input
	 */
	if (gcm_fastpath_enabled && block_size == 16 &&
	    ctx->gcm_remainder_len == 0 && length % block_size == 0 &&
	    ntohll(ctx->gcm_cb[1] & counter_mask) <= ntohll(counter_mask) -
	    length / block_size && CRYPTO_DATA_IS_SINGLE_BLOCK(out)) {
		gcm_fastpath128(ctx, (uint8_t *)data, length,
		    CRYPTO_DATA_FIRST_BLOCK(out), encrypt, encrypt_block,
		    xor_block, cipher_ctr);
		out->cd_offset += length;
		goto out;
	}

	if (length + ctx->gcm_remainder_len < block_size) {
		/* accumulate bytes here and return */
		bcopy(datap,
		    (uint8_t *)ctx->gcm_remainder + ctx->gcm_remainder_len,
		    length);
		ctx->gcm_remainder_len += length;
		ctx->gcm_copy_to = datap;
		goto out;
	}

	lastp = (uint8_t *)ctx->gcm_cb;
	if (out != NULL)
		crypto_init_ptrs(out, &iov_or_mp, &offset);

	do {
		/* Unprocessed data from last call. */
		if (ctx->gcm_remainder_len > 0) {
			need = block_size - ctx->gcm_remainder_len;

			if (need > remainder) {
				rv = SET_ERROR(CRYPTO_DATA_LEN_RANGE);
				goto out;
			}

			bcopy(datap, &((uint8_t *)ctx->gcm_remainder)
			    [ctx->gcm_remainder_len], need);

			blockp = (uint8_t *)ctx->gcm_remainder;
		} else {
			blockp = datap;
		}

		if (!encrypt)
			GHASH(ctx, blockp, ctx->gcm_ghash);

		/*
		 * Increment counter. Counter bits are confined
		 * to the bottom 32 bits of the counter block.
		 */
		counter = ntohll(ctx->gcm_cb[1] & counter_mask);
		counter = htonll(counter + 1);
		counter &= counter_mask;
		ctx->gcm_cb[1] = (ctx->gcm_cb[1] & ~counter_mask) | counter;

		encrypt_block(ctx->gcm_keysched, (uint8_t *)ctx->gcm_cb,
		    (uint8_t *)ctx->gcm_tmp);
		xor_block(blockp, (uint8_t *)ctx->gcm_tmp);

		lastp = (uint8_t *)ctx->gcm_tmp;

		ctx->gcm_processed_data_len += block_size;

		if (out == NULL) {
			if (ctx->gcm_remainder_len > 0) {
				bcopy(blockp, ctx->gcm_copy_to,
				    ctx->gcm_remainder_len);
				bcopy(blockp + ctx->gcm_remainder_len, datap,
				    need);
			}
		} else {
			crypto_get_ptrs(out, &iov_or_mp, &offset, &out_data_1,
			    &out_data_1_len, &out_data_2, block_size);

			/* copy block to where it belongs */
			if (out_data_1_len == block_size) {
				copy_block(lastp, out_data_1);
			} else {
				bcopy(lastp, out_data_1, out_data_1_len);
				if (out_data_2 != NULL) {
					bcopy(lastp + out_data_1_len,
					    out_data_2,
					    block_size - out_data_1_len);
				}
			}
			/* update offset */
			out->cd_offset += block_size;
		}

		/* add ciphertext to the hash */
		if (encrypt)
			GHASH(ctx, ctx->gcm_tmp, ctx->gcm_ghash);

		/* Update pointer to next block of data to be processed. */
		if (ctx->gcm_remainder_len != 0) {
			datap += need;
			ctx->gcm_remainder_len = 0;
		} else {
			datap += block_size;
		}

		remainder = (size_t)&data[length] - (size_t)datap;

		/* Incomplete last block. */
		if (remainder > 0 && remainder < block_size) {
			bcopy(datap, ctx->gcm_remainder, remainder);
			ctx->gcm_remainder_len = remainder;
			ctx->gcm_copy_to = datap;
			goto out;
		}
		ctx->gcm_copy_to = NULL;

	} while (remainder > 0);
out:
	GCM_ACCEL_EXIT;

	return (rv);
}


/*
 * Encrypt multiple blocks of data in GCM mode.  Decrypt for GCM mode
 * is done in another function.
 */
/*ARGSUSED*/
int
gcm_mode_encrypt_contiguous_blocks(gcm_ctx_t *ctx, char *data, size_t length,
    crypto_data_t *out, size_t block_size,
    int (*encrypt_block)(const void *, const uint8_t *, uint8_t *),
    void (*copy_block)(const uint8_t *, uint8_t *),
    void (*xor_block)(const uint8_t *, uint8_t *),
    int (*cipher_ctr)(const void *, const uint8_t *, uint8_t *, uint64_t,
    uint64_t *))
{
	return (gcm_process_contiguous_blocks(ctx, data, length, out,
	    block_size, B_TRUE, encrypt_block, copy_block, xor_block,
	    cipher_ctr));
}

/* ARGSUSED */
int
gcm_encrypt_final(gcm_ctx_t *ctx, crypto_data_t *out, size_t block_size,
    int (*encrypt_block)(const void *, const uint8_t *, uint8_t *),
    void (*copy_block)(const uint8_t *, uint8_t *),
    void (*xor_block)(const uint8_t *, uint8_t *))
{
	uint64_t counter_mask = ntohll(0x00000000ffffffffULL);
	uint8_t *ghash, *macp;
	int i, rv;

	GCM_ACCEL_ENTER;

	if (out->cd_length < (ctx->gcm_remainder_len + ctx->gcm_tag_len)) {
		rv = CRYPTO_DATA_LEN_RANGE;
		goto out;
	}

	ghash = (uint8_t *)ctx->gcm_ghash;

	if (ctx->gcm_remainder_len > 0) {
		uint64_t counter;
		uint8_t *tmpp = (uint8_t *)ctx->gcm_tmp;

		/*
		 * Here is where we deal with data that is not a
		 * multiple of the block size.
		 */

		/*
		 * Increment counter.
		 */
		counter = ntohll(ctx->gcm_cb[1] & counter_mask);
		counter = htonll(counter + 1);
		counter &= counter_mask;
		ctx->gcm_cb[1] = (ctx->gcm_cb[1] & ~counter_mask) | counter;

		encrypt_block(ctx->gcm_keysched, (uint8_t *)ctx->gcm_cb,
		    (uint8_t *)ctx->gcm_tmp);

		macp = (uint8_t *)ctx->gcm_remainder;
		bzero(macp + ctx->gcm_remainder_len,
		    block_size - ctx->gcm_remainder_len);

		/* XOR with counter block */
		for (i = 0; i < ctx->gcm_remainder_len; i++) {
			macp[i] ^= tmpp[i];
		}

		/* add ciphertext to the hash */
		GHASH(ctx, macp, ghash);

		ctx->gcm_processed_data_len += ctx->gcm_remainder_len;
	}

	ctx->gcm_len_a_len_c[1] =
	    htonll(CRYPTO_BYTES2BITS(ctx->gcm_processed_data_len));
	GHASH(ctx, ctx->gcm_len_a_len_c, ghash);
	encrypt_block(ctx->gcm_keysched, (uint8_t *)ctx->gcm_J0,
	    (uint8_t *)ctx->gcm_J0);
	xor_block((uint8_t *)ctx->gcm_J0, ghash);

	if (ctx->gcm_remainder_len > 0) {
		rv = crypto_put_output_data(macp, out, ctx->gcm_remainder_len);
		if (rv != CRYPTO_SUCCESS)
			goto out;
	}
	out->cd_offset += ctx->gcm_remainder_len;
	ctx->gcm_remainder_len = 0;
	rv = crypto_put_output_data(ghash, out, ctx->gcm_tag_len);
	if (rv != CRYPTO_SUCCESS)
		goto out;
	out->cd_offset += ctx->gcm_tag_len;
out:
	GCM_ACCEL_EXIT;
	return (rv);
}

/*
 * This will only deal with decrypting the last block of the input that
 * might not be a multiple of block length.
 */
/*ARGSUSED*/
static void
gcm_decrypt_incomplete_block(gcm_ctx_t *ctx, uint8_t *data, size_t length,
    size_t block_size, crypto_data_t *out,
    int (*encrypt_block)(const void *, const uint8_t *, uint8_t *),
    void (*xor_block)(const uint8_t *, uint8_t *))
{
	uint64_t counter;
	uint64_t counter_mask = ntohll(0x00000000ffffffffULL);

	/* padd last block and add to GHASH */
	bcopy(data, ctx->gcm_tmp, length);
	bzero(((uint8_t*)ctx->gcm_tmp) + length,
	    sizeof (ctx->gcm_tmp) - length);
	GHASH(ctx, ctx->gcm_tmp, ctx->gcm_ghash);

	/*
	 * Increment counter.
	 * Counter bits are confined to the bottom 32 bits.
	 */
	counter = ntohll(ctx->gcm_cb[1] & counter_mask);
	counter = htonll(counter + 1);
	counter &= counter_mask;
	ctx->gcm_cb[1] = (ctx->gcm_cb[1] & ~counter_mask) | counter;

	encrypt_block(ctx->gcm_keysched, (uint8_t *)ctx->gcm_cb,
	    (uint8_t *)ctx->gcm_tmp);

	/* XOR with counter block */
	for (size_t i = 0; i < length; i++)
		((uint8_t *)ctx->gcm_tmp)[i] ^= data[i];

	if (out != NULL) {
		(void) crypto_put_output_data((uchar_t *)ctx->gcm_tmp, out,
		    length);
		out->cd_offset += length;
	} else {
		bcopy(ctx->gcm_tmp, data, length);
	}
}

/* ARGSUSED */
int
gcm_mode_decrypt_contiguous_blocks(gcm_ctx_t *ctx, char *data, size_t length,
    crypto_data_t *out, size_t block_size,
    int (*encrypt_block)(const void *, const uint8_t *, uint8_t *),
    void (*copy_block)(const uint8_t *, uint8_t *),
    void (*xor_block)(const uint8_t *, uint8_t *),
    int (*cipher_ctr)(const void *, const uint8_t *, uint8_t *, uint64_t,
    uint64_t *))
{
	size_t to_copy;
	int rv = CRYPTO_SUCCESS;

	GCM_ACCEL_ENTER;

	/*
	 * Previous calls accumulate data in the input buffer to make sure
	 * we have the auth tag (the last part of the ciphertext) when we
	 * receive a final() call.
	 */
	if (ctx->gcm_last_input_fill > 0) {
		/* Try to complete the input buffer */
		to_copy = MIN(sizeof (ctx->gcm_last_input), length);
		bcopy(data, ctx->gcm_last_input + ctx->gcm_last_input_fill,
		    to_copy);
		data += to_copy;
		ctx->gcm_last_input_fill += to_copy;
		length -= to_copy;

		if (ctx->gcm_last_input_fill < block_size + ctx->gcm_tag_len)
			/* Not enough input data to continue */
			goto out;

		if (length < ctx->gcm_tag_len) {
			/*
			 * There isn't enough data ahead to constitute a full
			 * auth tag, so only crunch one input block and copy
			 * the remainder of the input into our buffer.
			 */
			rv = gcm_process_contiguous_blocks(ctx,
			    (char *)ctx->gcm_last_input, block_size, out,
			    block_size, B_FALSE, encrypt_block, copy_block,
			    xor_block, cipher_ctr);
			if (rv != CRYPTO_SUCCESS)
				goto out;
			ctx->gcm_last_input_fill -= block_size;
			bcopy(ctx->gcm_last_input + block_size,
			    ctx->gcm_last_input, ctx->gcm_last_input_fill);
			bcopy(ctx->gcm_last_input + block_size, data, length);
			goto out;
		}
		/*
		 * There is enough data ahead to be the auth tag, so crunch
		 * everything in our buffer now.
		 */
		rv = gcm_process_contiguous_blocks(ctx,
		    (char *)ctx->gcm_last_input, ctx->gcm_last_input_fill,
		    out, block_size, B_FALSE, encrypt_block, copy_block,
		    xor_block, cipher_ctr);
		if (rv != CRYPTO_SUCCESS)
			goto out;
		ctx->gcm_last_input_fill = 0;
	}
	/*
	 * Last input buffer is empty now and there is enough input data
	 * ahead to constitute an auth tag, so crunch all the buffers in
	 * between.
	 */
	ASSERT(ctx->gcm_last_input_fill == 0 && length >= ctx->gcm_tag_len);
	if (length > ctx->gcm_tag_len) {
		to_copy = ((length - ctx->gcm_tag_len) / block_size) *
		    block_size;
		rv = gcm_process_contiguous_blocks(ctx, data, to_copy, out,
		    block_size, B_FALSE, encrypt_block, copy_block, xor_block,
		    cipher_ctr);
		if (rv != CRYPTO_SUCCESS)
			goto out;
		data += to_copy;
		length -= to_copy;
	}
	/*
	 * Copy the remainder into our input buffer, it's potentially
	 * the auth tag.
	 */
	ASSERT(length >= ctx->gcm_tag_len &&
	    length <= sizeof (ctx->gcm_last_input));
	bcopy(data, ctx->gcm_last_input, length);
	ctx->gcm_last_input_fill += length;
out:
	GCM_ACCEL_EXIT;

	return (rv);
}

int
gcm_decrypt_final(gcm_ctx_t *ctx, crypto_data_t *out, size_t block_size,
    int (*encrypt_block)(const void *, const uint8_t *, uint8_t *),
    void (*copy_block)(const uint8_t *, uint8_t *),
    void (*xor_block)(const uint8_t *, uint8_t *),
    int (*cipher_ctr)(const void *, const uint8_t *, uint8_t *, uint64_t,
    uint64_t *))
{
	int rv = CRYPTO_SUCCESS;

	/* Check there's enough data to at least compute a tag */
	if (ctx->gcm_last_input_fill < ctx->gcm_tag_len)
		return (SET_ERROR(CRYPTO_DATA_LEN_RANGE));

	GCM_ACCEL_ENTER;

	/* Finish any unprocessed input */
	if (ctx->gcm_last_input_fill > ctx->gcm_tag_len) {
		size_t last_blk_len = MIN(block_size,
		    ctx->gcm_last_input_fill - ctx->gcm_tag_len);

		/* Finish last full block */
		if (last_blk_len >= block_size) {
			rv = gcm_process_contiguous_blocks(ctx,
			    (char *)ctx->gcm_last_input, block_size, out,
			    block_size, B_FALSE, encrypt_block, copy_block,
			    xor_block, cipher_ctr);
			if (rv != CRYPTO_SUCCESS)
				goto errout;

			last_blk_len -= block_size;
			ctx->gcm_processed_data_len += block_size;
			ctx->gcm_last_input_fill -= block_size;

			/* Shift what remains in the input buffer forward */
			bcopy(ctx->gcm_last_input + block_size,
			    ctx->gcm_last_input, ctx->gcm_last_input_fill);
		}
		/* Finish last incomplete block before auth tag */
		if (last_blk_len > 0) {
			gcm_decrypt_incomplete_block(ctx, ctx->gcm_last_input,
			    last_blk_len, block_size, out, encrypt_block,
			    xor_block);

			ctx->gcm_processed_data_len += last_blk_len;
			ctx->gcm_last_input_fill -= last_blk_len;

			/* Shift what remains in the input buffer forward */
			bcopy(ctx->gcm_last_input + last_blk_len,
			    ctx->gcm_last_input, ctx->gcm_last_input_fill);
		}
		/* Now the last_input buffer holds just the auth tag */
	}

	ASSERT(ctx->gcm_last_input_fill == ctx->gcm_tag_len);

	ctx->gcm_len_a_len_c[1] =
	    htonll(CRYPTO_BYTES2BITS(ctx->gcm_processed_data_len));
	GHASH(ctx, ctx->gcm_len_a_len_c, ctx->gcm_ghash);
	encrypt_block(ctx->gcm_keysched, (uint8_t *)ctx->gcm_J0,
	    (uint8_t *)ctx->gcm_J0);
	xor_block((uint8_t *)ctx->gcm_J0, (uint8_t *)ctx->gcm_ghash);

	GCM_ACCEL_EXIT;

	/* compare the input authentication tag with what we calculated */
	if (bcmp(&ctx->gcm_last_input, ctx->gcm_ghash, ctx->gcm_tag_len) != 0)
		return (SET_ERROR(CRYPTO_INVALID_MAC));

	return (CRYPTO_SUCCESS);

errout:
	GCM_ACCEL_EXIT;
	return (rv);
}

static int
gcm_validate_args(CK_AES_GCM_PARAMS *gcm_param)
{
	size_t tag_len;

	/*
	 * Check the length of the authentication tag (in bits).
	 */
	tag_len = gcm_param->ulTagBits;
	switch (tag_len) {
	case 32:
	case 64:
	case 96:
	case 104:
	case 112:
	case 120:
	case 128:
		break;
	default:
		return (SET_ERROR(CRYPTO_MECHANISM_PARAM_INVALID));
	}

	if (gcm_param->ulIvLen == 0)
		return (SET_ERROR(CRYPTO_MECHANISM_PARAM_INVALID));

	return (CRYPTO_SUCCESS);
}

/*ARGSUSED*/
static void
gcm_format_initial_blocks(uchar_t *iv, ulong_t iv_len,
    gcm_ctx_t *ctx, size_t block_size,
    void (*copy_block)(const uint8_t *, uint8_t *),
    void (*xor_block)(const uint8_t *, uint8_t *))
{
	uint8_t *cb;
	ulong_t remainder = iv_len;
	ulong_t processed = 0;
	uint8_t *datap, *ghash;
	uint64_t len_a_len_c[2];

	ghash = (uint8_t *)ctx->gcm_ghash;
	cb = (uint8_t *)ctx->gcm_cb;
	if (iv_len == 12) {
		bcopy(iv, cb, 12);
		cb[12] = 0;
		cb[13] = 0;
		cb[14] = 0;
		cb[15] = 1;
		/* J0 will be used again in the final */
		copy_block(cb, (uint8_t *)ctx->gcm_J0);
	} else {
		/* GHASH the IV */
		do {
			if (remainder < block_size) {
				bzero(cb, block_size);
				bcopy(&(iv[processed]), cb, remainder);
				datap = (uint8_t *)cb;
				remainder = 0;
			} else {
				datap = (uint8_t *)(&(iv[processed]));
				processed += block_size;
				remainder -= block_size;
			}
			GHASH(ctx, datap, ghash);
		} while (remainder > 0);

		len_a_len_c[0] = 0;
		len_a_len_c[1] = htonll(CRYPTO_BYTES2BITS(iv_len));
		GHASH(ctx, len_a_len_c, ctx->gcm_J0);

		/* J0 will be used again in the final */
		copy_block((uint8_t *)ctx->gcm_J0, (uint8_t *)cb);
	}
}

/*
 * The following function is called at encrypt or decrypt init time
 * for AES GCM mode.
 */
int
gcm_init(gcm_ctx_t *ctx, unsigned char *iv, size_t iv_len,
    unsigned char *auth_data, size_t auth_data_len, size_t block_size,
    int (*encrypt_block)(const void *, const uint8_t *, uint8_t *),
    void (*copy_block)(const uint8_t *, uint8_t *),
    void (*xor_block)(const uint8_t *, uint8_t *))
{
	uint8_t *ghash, *datap, *authp;
	size_t remainder, processed;

	GCM_ACCEL_ENTER;

	/* encrypt zero block to get subkey H */
	bzero(ctx->gcm_H, sizeof (ctx->gcm_H));
	encrypt_block(ctx->gcm_keysched, (uint8_t *)ctx->gcm_H,
	    (uint8_t *)ctx->gcm_H);

	gcm_format_initial_blocks(iv, iv_len, ctx, block_size,
	    copy_block, xor_block);

#ifdef	__amd64
	if (intel_pclmulqdq_instruction_present()) {
		uint64_t H_bswap64[2] = {
		    ntohll(ctx->gcm_H[0]), ntohll(ctx->gcm_H[1])
		};

		gcm_init_clmul(H_bswap64, ctx->gcm_H_table);
	}
#endif

	authp = (uint8_t *)ctx->gcm_tmp;
	ghash = (uint8_t *)ctx->gcm_ghash;
	bzero(authp, block_size);
	bzero(ghash, block_size);

	processed = 0;
	remainder = auth_data_len;
	do {
		if (remainder < block_size) {
			/*
			 * There's not a block full of data, pad rest of
			 * buffer with zero
			 */
			bzero(authp, block_size);
			bcopy(&(auth_data[processed]), authp, remainder);
			datap = (uint8_t *)authp;
			remainder = 0;
		} else {
			datap = (uint8_t *)(&(auth_data[processed]));
			processed += block_size;
			remainder -= block_size;
		}

		/* add auth data to the hash */
		GHASH(ctx, datap, ghash);

	} while (remainder > 0);

	GCM_ACCEL_EXIT;

	return (CRYPTO_SUCCESS);
}

int
gcm_init_ctx(gcm_ctx_t *gcm_ctx, char *param, size_t block_size,
    int (*encrypt_block)(const void *, const uint8_t *, uint8_t *),
    void (*copy_block)(const uint8_t *, uint8_t *),
    void (*xor_block)(const uint8_t *, uint8_t *))
{
	/*
	 * No GHASH invocations in this function and gcm_init does its own
	 * FPU saving, so no need to GCM_ACCEL_ENTER/GCM_ACCEL_EXIT here.
	 */
	int rv;
	CK_AES_GCM_PARAMS *gcm_param;

	if (param != NULL) {
		gcm_param = (CK_AES_GCM_PARAMS *)(void *)param;

		if ((rv = gcm_validate_args(gcm_param)) != 0) {
			return (rv);
		}

		gcm_ctx->gcm_tag_len = gcm_param->ulTagBits;
		gcm_ctx->gcm_tag_len >>= 3;
		gcm_ctx->gcm_processed_data_len = 0;

		/* these values are in bits */
		gcm_ctx->gcm_len_a_len_c[0]
		    = htonll(CRYPTO_BYTES2BITS(gcm_param->ulAADLen));

		rv = CRYPTO_SUCCESS;
		gcm_ctx->gcm_flags |= GCM_MODE;
	} else {
		rv = CRYPTO_MECHANISM_PARAM_INVALID;
		goto out;
	}

	if (gcm_init(gcm_ctx, gcm_param->pIv, gcm_param->ulIvLen,
	    gcm_param->pAAD, gcm_param->ulAADLen, block_size,
	    encrypt_block, copy_block, xor_block) != 0) {
		rv = CRYPTO_MECHANISM_PARAM_INVALID;
	}
out:
	return (rv);
}

int
gmac_init_ctx(gcm_ctx_t *gcm_ctx, char *param, size_t block_size,
    int (*encrypt_block)(const void *, const uint8_t *, uint8_t *),
    void (*copy_block)(const uint8_t *, uint8_t *),
    void (*xor_block)(const uint8_t *, uint8_t *))
{
	/*
	 * No GHASH invocations in this function and gcm_init does its own
	 * FPU saving, so no need to GCM_ACCEL_ENTER/GCM_ACCEL_EXIT here.
	 */
	int rv;
	CK_AES_GMAC_PARAMS *gmac_param;

	if (param != NULL) {
		gmac_param = (CK_AES_GMAC_PARAMS *)(void *)param;

		gcm_ctx->gcm_tag_len = CRYPTO_BITS2BYTES(AES_GMAC_TAG_BITS);
		gcm_ctx->gcm_processed_data_len = 0;

		/* these values are in bits */
		gcm_ctx->gcm_len_a_len_c[0]
		    = htonll(CRYPTO_BYTES2BITS(gmac_param->ulAADLen));

		rv = CRYPTO_SUCCESS;
		gcm_ctx->gcm_flags |= GMAC_MODE;
	} else {
		rv = CRYPTO_MECHANISM_PARAM_INVALID;
		goto out;
	}

	if (gcm_init(gcm_ctx, gmac_param->pIv, AES_GMAC_IV_LEN,
	    gmac_param->pAAD, gmac_param->ulAADLen, block_size,
	    encrypt_block, copy_block, xor_block) != 0) {
		rv = CRYPTO_MECHANISM_PARAM_INVALID;
	}
out:
	return (rv);
}

void *
gcm_alloc_ctx(int kmflag)
{
	gcm_ctx_t *gcm_ctx;

#ifdef _KERNEL
	if ((gcm_ctx = kmem_zalloc(sizeof (gcm_ctx_t), kmflag)) == NULL)
#else
	if ((gcm_ctx = calloc(1, sizeof (gcm_ctx_t))) == NULL)
#endif
		return (NULL);

	gcm_ctx->gcm_flags = GCM_MODE;
	return (gcm_ctx);
}

void *
gmac_alloc_ctx(int kmflag)
{
	gcm_ctx_t *gcm_ctx;

#ifdef _KERNEL
	if ((gcm_ctx = kmem_zalloc(sizeof (gcm_ctx_t), kmflag)) == NULL)
#else
	if ((gcm_ctx = calloc(1, sizeof (gcm_ctx_t))) == NULL)
#endif
		return (NULL);

	gcm_ctx->gcm_flags = GMAC_MODE;
	return (gcm_ctx);
}

void
gcm_set_kmflag(gcm_ctx_t *ctx, int kmflag)
{
	ctx->gcm_kmflag = kmflag;
}


#ifdef __amd64
/*
 * Return 1 if executing on Intel with PCLMULQDQ instructions,
 * otherwise 0 (i.e., Intel without PCLMULQDQ or AMD64).
 * Cache the result, as the CPU can't change.
 *
 * Note: the userland version uses getisax().  The kernel version uses
 * is_x86_featureset().
 */
static inline int
intel_pclmulqdq_instruction_present(void)
{
	static int	cached_result = -1;

	if (cached_result == -1) { /* first time */
#ifdef _KERNEL
		cached_result =
		    is_x86_feature(x86_featureset, X86FSET_PCLMULQDQ);
#else
		uint_t		ui = 0;

		(void) getisax(&ui, 1);
		cached_result = (ui & AV_386_PCLMULQDQ) != 0;
#endif	/* _KERNEL */
	}

	return (cached_result);
}
#endif	/* __amd64 */
