#ifndef _SFXGE_COMPAT_H
#define _SFXGE_COMPAT_H

/*
 * Macro available in newer versions of /usr/include/sdt.h
 */
#include <sys/sdt.h>
#include <sys/inline.h>

#ifndef	DTRACE_PROBE5
#define	DTRACE_PROBE5(name, type1, arg1, type2, arg2, type3, arg3,	\
	type4, arg4, type5, arg5)					\
	{								\
		extern void __dtrace_probe_##name(uintptr_t, uintptr_t,	\
		    uintptr_t, uintptr_t, uintptr_t);			\
		__dtrace_probe_##name((uintptr_t)(arg1),		\
		    (uintptr_t)(arg2), (uintptr_t)(arg3),		\
		    (uintptr_t)(arg4), (uintptr_t)(arg5));		\
	}
#endif	/* DTRACE_PROBE5 */

#endif
