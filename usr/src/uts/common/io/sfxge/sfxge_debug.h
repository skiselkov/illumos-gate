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
 * Copyright 2008-2013 Solarflare Communications Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_SYS_SFXGE_DEBUG_H
#define	_SYS_SFXGE_DEBUG_H

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	DEBUG

extern boolean_t sfxge_aask;

#define	SFXGE_OBJ_CHECK(_objp, _type)					\
	do {								\
		uint8_t *p = (uint8_t *)(_objp);			\
		size_t off;						\
									\
		for (off = 0; off < sizeof (_type); off++) {		\
			char buf[MAXNAMELEN];				\
									\
			if (*p++ == 0)					\
				continue;				\
									\
			(void) snprintf(buf, MAXNAMELEN - 1,		\
			    "%s[%d]: non-zero byte found in %s "	\
			    "at 0x%p+%lx", __FILE__, __LINE__, #_type,	\
			    (void *)(_objp), off);			\
									\
			if (sfxge_aask)					\
				debug_enter(buf);			\
			break;						\
		}							\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

/* Log cmn_err(9F) messages to console and system log */
#define	SFXGE_CMN_ERR	""

#else	/* DEBUG */

#define	SFXGE_OBJ_CHECK(_objp, _type)

/* Log cmn_err(9F) messages to system log only */
#define	SFXGE_CMN_ERR	"!"

#endif	/* DEBUG */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SFXGE_DEBUG_H */
