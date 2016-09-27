/*
 * This file and its contents are supplied under the terms of the
 * Common Development and Distribution License ("CDDL"), version 1.0.
 * You may only use this file in accordance with the terms of version
 * 1.0 of the CDDL.
 *
 * A full copy of the text of the CDDL should have accompanied this
 * source.  A copy of the CDDL is also available via the Internet at
 * http://www.illumos.org/license/CDDL.
 */

/*
 * Copyright 2016 Nexenta Systems, Inc. All rights reserved.
 */

/*
 * This is the ZFS interface for the Zstandard compression algorithm
 * contained in the "zstd" directory.
 */

#include <sys/zfs_context.h>

#include "zstd/zstd.h"

/*ARGSUSED*/
size_t
zstd_compress(void *s_start, void *d_start, size_t s_len, size_t d_len, int lvl)
{
	uint32_t bufsiz;
	char *dest = d_start;

	ASSERT(d_len >= sizeof (bufsiz));

	bufsiz = ZSTD_compress(&dest[sizeof (bufsiz)], d_len - sizeof (bufsiz),
	    s_start, s_len, lvl);

	/* Signal an error if the compression routine returned zero. */
	if (ZSTD_isError(bufsiz))
		return (s_len);
	/*
	 * Encode the compresed buffer size at the start. We'll need this in
	 * decompression to counter the effects of padding which might be
	 * added to the compressed buffer and which, if unhandled, would
	 * confuse the hell out of our decompression function.
	 */
	*(uint32_t *)dest = BE_32(bufsiz);

	return (bufsiz + sizeof (bufsiz));
}

/*ARGSUSED*/
int
zstd_decompress(void *s_start, void *d_start, size_t s_len, size_t d_len, int n)
{
	const char *src = s_start;
	uint32_t bufsiz = BE_IN32(src);

	/* invalid compressed buffer size encoded at start */
	if (bufsiz + sizeof (bufsiz) > s_len)
		return (1);

	/*
	 * Returns 0 on success (decompression function returned non-negative)
	 * and non-zero on failure (decompression function returned negative.
	 */
	return (ZSTD_isError(ZSTD_decompress(d_start, d_len,
	    &src[sizeof (bufsiz)], bufsiz)));
}
