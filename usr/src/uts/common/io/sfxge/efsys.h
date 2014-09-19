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

#ifndef	_SYS_EFSYS_H
#define	_SYS_EFSYS_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/cpuvar.h>
#include <sys/disp.h>
#include <sys/sdt.h>
#include <sys/kstat.h>
#include <sys/crc32.h>
#include <sys/note.h>
#include <sys/byteorder.h>

#define	EFSYS_HAS_UINT64 1
#define	EFSYS_USE_UINT64 0
#ifdef	_BIG_ENDIAN
#define	EFSYS_IS_BIG_ENDIAN 1
#endif
#ifdef	_LITTLE_ENDIAN
#define	EFSYS_IS_LITTLE_ENDIAN 1
#endif
#include "efx_types.h"

#ifdef _USE_GLD_V3_SOL10
#include "compat.h"
#endif

/* Modifiers used for DOS builds */
#define	__cs
#define	__far

/* Modifiers used for Windows builds */
#define	__in
#define	__in_opt
#define	__in_ecount(_n)
#define	__in_ecount_opt(_n)
#define	__in_bcount(_n)
#define	__in_bcount_opt(_n)

#define	__out
#define	__out_opt
#define	__out_ecount(_n)
#define	__out_ecount_opt(_n)
#define	__out_bcount(_n)
#define	__out_bcount_opt(_n)

#define	__deref_out

#define	__inout
#define	__inout_opt
#define	__inout_ecount(_n)
#define	__inout_ecount_opt(_n)
#define	__inout_bcount(_n)
#define	__inout_bcount_opt(_n)
#define	__inout_bcount_full_opt(_n)

#define	__deref_out_bcount_opt(n)

#define	__checkReturn

#define	__drv_when(_p, _c)

/* Code inclusion options */


#define	EFSYS_OPT_NAMES 1

#define	EFSYS_OPT_FALCON 1
#define	EFSYS_OPT_SIENA 1
#if DEBUG
#define	EFSYS_OPT_CHECK_REG 1
#else
#define	EFSYS_OPT_CHECK_REG 0
#endif

#define	EFSYS_OPT_MCDI 1

#define	EFSYS_OPT_MAC_FALCON_GMAC 1
#define	EFSYS_OPT_MAC_FALCON_XMAC 1
#define	EFSYS_OPT_MAC_STATS 1

#define	EFSYS_OPT_LOOPBACK 1

#define	EFSYS_OPT_MON_NULL 1
#define	EFSYS_OPT_MON_LM87 1
#define	EFSYS_OPT_MON_MAX6647 1
#define	EFSYS_OPT_MON_SIENA 1
#define	EFSYS_OPT_MON_STATS 1

#define	EFSYS_OPT_PHY_NULL 1
#define	EFSYS_OPT_PHY_QT2022C2 1
#define	EFSYS_OPT_PHY_SFX7101 1
#define	EFSYS_OPT_PHY_TXC43128 1
#define	EFSYS_OPT_PHY_PM8358 1
#define	EFSYS_OPT_PHY_SFT9001 1
#define	EFSYS_OPT_PHY_QT2025C 1
#define	EFSYS_OPT_PHY_STATS 1
#define	EFSYS_OPT_PHY_PROPS 1
#define	EFSYS_OPT_PHY_BIST 1
#define	EFSYS_OPT_PHY_LED_CONTROL 1

#define	EFSYS_OPT_VPD 1
#define	EFSYS_OPT_NVRAM 1
#define	EFSYS_OPT_NVRAM_FALCON_BOOTROM 1
#define	EFSYS_OPT_NVRAM_SFT9001	0
#define	EFSYS_OPT_NVRAM_SFX7101	0
#define	EFSYS_OPT_BOOTCFG 1

#define	EFSYS_OPT_PCIE_TUNE 1
#define	EFSYS_OPT_DIAG 1
#define	EFSYS_OPT_WOL 1
#define	EFSYS_OPT_RX_SCALE 1
#define	EFSYS_OPT_QSTATS 1

#define	EFSYS_OPT_EV_PREFETCH 0

#define	EFSYS_OPT_DECODE_INTR_FATAL 1

/* ID */

typedef struct __efsys_identifier_s	efsys_identifier_t;

/* DMA */

typedef uint64_t		efsys_dma_addr_t;

typedef struct efsys_mem_s {
	ddi_dma_handle_t	esm_dma_handle; /* DMA memory allocate/bind */
	ddi_acc_handle_t	esm_acc_handle;	/* DMA memory read/write */
	caddr_t			esm_base;
	efsys_dma_addr_t	esm_addr;
	size_t			esm_size;
} efsys_mem_t;


#define	EFSYS_MEM_ZERO(_esmp, _size)					\
	do {								\
		(void) memset((_esmp)->esm_base, 0, (_size));		\
									\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFSYS_MEM_READD(_esmp, _offset, _edp)				\
	do {								\
		uint32_t *addr;						\
									\
		_NOTE(CONSTANTCONDITION)				\
		ASSERT(IS_P2ALIGNED(_offset, sizeof (efx_dword_t)));	\
									\
		addr = (void *)((_esmp)->esm_base + (_offset));		\
									\
		(_edp)->ed_u32[0] = ddi_get32((_esmp)->esm_acc_handle,	\
		    addr);						\
									\
		DTRACE_PROBE2(mem_readd, unsigned int, (_offset),	\
		    uint32_t, (_edp)->ed_u32[0]);			\
									\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFSYS_MEM_READQ(_esmp, _offset, _eqp)				\
	do {								\
		uint32_t *addr;						\
									\
		_NOTE(CONSTANTCONDITION)				\
		ASSERT(IS_P2ALIGNED(_offset, sizeof (efx_qword_t)));	\
									\
		addr = (void *)((_esmp)->esm_base + (_offset));		\
									\
		(_eqp)->eq_u32[0] = ddi_get32((_esmp)->esm_acc_handle,	\
		    addr++);						\
		(_eqp)->eq_u32[1] = ddi_get32((_esmp)->esm_acc_handle,	\
		    addr);						\
									\
		DTRACE_PROBE3(mem_readq, unsigned int, (_offset),	\
		    uint32_t, (_eqp)->eq_u32[1],			\
		    uint32_t, (_eqp)->eq_u32[0]);			\
									\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFSYS_MEM_READO(_esmp, _offset, _eop)				\
	do {								\
		uint32_t *addr;						\
									\
		_NOTE(CONSTANTCONDITION)				\
		ASSERT(IS_P2ALIGNED(_offset, sizeof (efx_oword_t)));	\
									\
		addr = (void *)((_esmp)->esm_base + (_offset));		\
									\
		(_eop)->eo_u32[0] = ddi_get32((_esmp)->esm_acc_handle,	\
		    addr++);						\
		(_eop)->eo_u32[1] = ddi_get32((_esmp)->esm_acc_handle,	\
		    addr++);						\
		(_eop)->eo_u32[2] = ddi_get32((_esmp)->esm_acc_handle,	\
		    addr++);						\
		(_eop)->eo_u32[3] = ddi_get32((_esmp)->esm_acc_handle,	\
		    addr);						\
									\
		DTRACE_PROBE5(mem_reado, unsigned int, (_offset),	\
		    uint32_t, (_eop)->eo_u32[3],			\
		    uint32_t, (_eop)->eo_u32[2],			\
		    uint32_t, (_eop)->eo_u32[1],			\
		    uint32_t, (_eop)->eo_u32[0]);			\
									\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFSYS_MEM_WRITED(_esmp, _offset, _edp)				\
	do {								\
		uint32_t *addr;						\
									\
		_NOTE(CONSTANTCONDITION)				\
		ASSERT(IS_P2ALIGNED(_offset, sizeof (efx_dword_t)));	\
									\
		DTRACE_PROBE2(mem_writed, unsigned int, (_offset),	\
		    uint32_t, (_edp)->ed_u32[0]);			\
									\
		addr = (void *)((_esmp)->esm_base + (_offset));		\
									\
		ddi_put32((_esmp)->esm_acc_handle, addr,		\
		    (_edp)->ed_u32[0]);					\
									\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFSYS_MEM_WRITEQ(_esmp, _offset, _eqp)				\
	do {								\
		uint32_t *addr;						\
									\
		_NOTE(CONSTANTCONDITION)				\
		ASSERT(IS_P2ALIGNED(_offset, sizeof (efx_qword_t)));	\
									\
		DTRACE_PROBE3(mem_writeq, unsigned int, (_offset),	\
		    uint32_t, (_eqp)->eq_u32[1],			\
		    uint32_t, (_eqp)->eq_u32[0]);			\
									\
		addr = (void *)((_esmp)->esm_base + (_offset));		\
									\
		ddi_put32((_esmp)->esm_acc_handle, addr++,		\
		    (_eqp)->eq_u32[0]);					\
		ddi_put32((_esmp)->esm_acc_handle, addr,		\
		    (_eqp)->eq_u32[1]);					\
									\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFSYS_MEM_WRITEO(_esmp, _offset, _eop)				\
	do {								\
		uint32_t *addr;						\
									\
		_NOTE(CONSTANTCONDITION)				\
		ASSERT(IS_P2ALIGNED(_offset, sizeof (efx_oword_t)));	\
									\
		DTRACE_PROBE5(mem_writeo, unsigned int, (_offset),	\
		    uint32_t, (_eop)->eo_u32[3],			\
		    uint32_t, (_eop)->eo_u32[2],			\
		    uint32_t, (_eop)->eo_u32[1],			\
		    uint32_t, (_eop)->eo_u32[0]);			\
									\
		addr = (void *)((_esmp)->esm_base + (_offset));		\
									\
		ddi_put32((_esmp)->esm_acc_handle, addr++,		\
		    (_eop)->eo_u32[0]);					\
		ddi_put32((_esmp)->esm_acc_handle, addr++,		\
		    (_eop)->eo_u32[1]);					\
		ddi_put32((_esmp)->esm_acc_handle, addr++,		\
		    (_eop)->eo_u32[2]);					\
		ddi_put32((_esmp)->esm_acc_handle, addr,		\
		    (_eop)->eo_u32[3]);					\
									\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFSYS_MEM_ADDR(_esmp)						\
	((_esmp)->esm_addr)

/* BAR */

typedef struct efsys_bar_s {
	kmutex_t		esb_lock;
	ddi_acc_handle_t	esb_handle;
	caddr_t			esb_base;
} efsys_bar_t;

#define	EFSYS_BAR_READD(_esbp, _offset, _edp, _lock)			\
	do {								\
		uint32_t *addr;						\
									\
		_NOTE(CONSTANTCONDITION)				\
		ASSERT(IS_P2ALIGNED(_offset, sizeof (efx_dword_t)));	\
									\
		_NOTE(CONSTANTCONDITION)				\
		if (_lock)						\
			mutex_enter(&((_esbp)->esb_lock));		\
									\
		addr = (void *)((_esbp)->esb_base + (_offset));		\
									\
		(_edp)->ed_u32[0] = ddi_get32((_esbp)->esb_handle,	\
		    addr);						\
									\
		DTRACE_PROBE2(bar_readd, unsigned int, (_offset),	\
		    uint32_t, (_edp)->ed_u32[0]);			\
									\
		_NOTE(CONSTANTCONDITION)				\
		if (_lock)						\
			mutex_exit(&((_esbp)->esb_lock));		\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFSYS_BAR_READQ(_esbp, _offset, _eqp)				\
	do {								\
		uint32_t *addr;						\
									\
		_NOTE(CONSTANTCONDITION)				\
		ASSERT(IS_P2ALIGNED(_offset, sizeof (efx_qword_t)));	\
									\
		mutex_enter(&((_esbp)->esb_lock));			\
									\
		addr = (void *)((_esbp)->esb_base + (_offset));		\
									\
		(_eqp)->eq_u32[0] = ddi_get32((_esbp)->esb_handle,	\
		    addr++);						\
		(_eqp)->eq_u32[1] = ddi_get32((_esbp)->esb_handle,	\
		    addr);						\
									\
		DTRACE_PROBE3(bar_readq, unsigned int, (_offset),	\
		    uint32_t, (_eqp)->eq_u32[1],			\
		    uint32_t, (_eqp)->eq_u32[0]);			\
									\
		mutex_exit(&((_esbp)->esb_lock));			\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFSYS_BAR_READO(_esbp, _offset, _eop, _lock)			\
	do {								\
		uint32_t *addr;						\
									\
		_NOTE(CONSTANTCONDITION)				\
		ASSERT(IS_P2ALIGNED(_offset, sizeof (efx_oword_t)));	\
									\
		_NOTE(CONSTANTCONDITION)				\
		if (_lock)						\
			mutex_enter(&((_esbp)->esb_lock));		\
									\
		addr = (void *)((_esbp)->esb_base + (_offset));		\
									\
		(_eop)->eo_u32[0] = ddi_get32((_esbp)->esb_handle,	\
		    addr++);						\
		(_eop)->eo_u32[1] = ddi_get32((_esbp)->esb_handle,	\
		    addr++);						\
		(_eop)->eo_u32[2] = ddi_get32((_esbp)->esb_handle,	\
		    addr++);						\
		(_eop)->eo_u32[3] = ddi_get32((_esbp)->esb_handle,	\
		    addr);						\
									\
		DTRACE_PROBE5(bar_reado, unsigned int, (_offset),	\
		    uint32_t, (_eop)->eo_u32[3],			\
		    uint32_t, (_eop)->eo_u32[2],			\
		    uint32_t, (_eop)->eo_u32[1],			\
		    uint32_t, (_eop)->eo_u32[0]);			\
									\
		_NOTE(CONSTANTCONDITION)				\
		if (_lock)						\
			mutex_exit(&((_esbp)->esb_lock));		\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFSYS_BAR_WRITED(_esbp, _offset, _edp, _lock)			\
	do {								\
		uint32_t *addr;						\
									\
		_NOTE(CONSTANTCONDITION)				\
		ASSERT(IS_P2ALIGNED(_offset, sizeof (efx_dword_t)));	\
									\
		_NOTE(CONSTANTCONDITION)				\
		if (_lock)						\
			mutex_enter(&((_esbp)->esb_lock));		\
									\
		DTRACE_PROBE2(bar_writed, unsigned int, (_offset),	\
		    uint32_t, (_edp)->ed_u32[0]);			\
									\
		addr = (void *)((_esbp)->esb_base + (_offset));		\
									\
		ddi_put32((_esbp)->esb_handle, addr,			\
		    (_edp)->ed_u32[0]);					\
									\
		_NOTE(CONSTANTCONDITION)				\
		if (_lock)						\
			mutex_exit(&((_esbp)->esb_lock));		\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFSYS_BAR_WRITEQ(_esbp, _offset, _eqp)				\
	do {								\
		uint32_t *addr;						\
									\
		_NOTE(CONSTANTCONDITION)				\
		ASSERT(IS_P2ALIGNED(_offset, sizeof (efx_qword_t)));	\
									\
		mutex_enter(&((_esbp)->esb_lock));			\
									\
		DTRACE_PROBE3(bar_writeq, unsigned int, (_offset),	\
		    uint32_t, (_eqp)->eq_u32[1],			\
		    uint32_t, (_eqp)->eq_u32[0]);			\
									\
		addr = (void *)((_esbp)->esb_base + (_offset));		\
									\
		ddi_put32((_esbp)->esb_handle, addr++,			\
		    (_eqp)->eq_u32[0]);					\
		ddi_put32((_esbp)->esb_handle, addr,			\
		    (_eqp)->eq_u32[1]);					\
									\
		mutex_exit(&((_esbp)->esb_lock));			\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFSYS_BAR_WRITEO(_esbp, _offset, _eop, _lock)			\
	do {								\
		uint32_t *addr;						\
									\
		_NOTE(CONSTANTCONDITION)				\
		ASSERT(IS_P2ALIGNED(_offset, sizeof (efx_oword_t)));	\
									\
		_NOTE(CONSTANTCONDITION)				\
		if (_lock)						\
			mutex_enter(&((_esbp)->esb_lock));		\
									\
		DTRACE_PROBE5(bar_writeo, unsigned int, (_offset),	\
		    uint32_t, (_eop)->eo_u32[3],			\
		    uint32_t, (_eop)->eo_u32[2],			\
		    uint32_t, (_eop)->eo_u32[1],			\
		    uint32_t, (_eop)->eo_u32[0]);			\
									\
		addr = (void *)((_esbp)->esb_base + (_offset));		\
									\
		ddi_put32((_esbp)->esb_handle, addr++,			\
		    (_eop)->eo_u32[0]);					\
		ddi_put32((_esbp)->esb_handle, addr++,			\
		    (_eop)->eo_u32[1]);					\
		ddi_put32((_esbp)->esb_handle, addr++,			\
		    (_eop)->eo_u32[2]);					\
		ddi_put32((_esbp)->esb_handle, addr,			\
		    (_eop)->eo_u32[3]);					\
									\
		_NOTE(CONSTANTCONDITION)				\
		if (_lock)						\
			mutex_exit(&((_esbp)->esb_lock));		\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

/* SPIN */

#define	EFSYS_SPIN(_us)							\
	do {								\
		drv_usecwait(_us);					\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFSYS_SLEEP	EFSYS_SPIN

/* BARRIERS */

/* Strict ordering guaranteed by devacc.devacc_attr_dataorder */
#define	EFSYS_MEM_READ_BARRIER()	membar_consumer()
/* TODO: Is ddi_put32() properly barriered? */
#define	EFSYS_PIO_WRITE_BARRIER()

/* TIMESTAMP */

typedef	clock_t	efsys_timestamp_t;

#define	EFSYS_TIMESTAMP(_usp)						\
	do {								\
		clock_t now;						\
									\
		now = ddi_get_lbolt();					\
		*(_usp) = drv_hztousec(now);				\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

/* KMEM */

#define	EFSYS_KMEM_ALLOC(_esip, _size, _p)				\
	do {								\
		(_esip) = (_esip);					\
		(_p) = kmem_zalloc((_size), KM_NOSLEEP);		\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFSYS_KMEM_FREE(_esip, _size, _p)				\
	do {								\
		(_esip) = (_esip);					\
		kmem_free((_p), (_size));				\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

/* LOCK */

typedef kmutex_t	efsys_lock_t;

#define	EFSYS_LOCK_MAGIC	0x000010c4

#define	EFSYS_LOCK(_lockp, _state)					\
	do {								\
		mutex_enter(_lockp);					\
		(_state) = EFSYS_LOCK_MAGIC;				\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFSYS_UNLOCK(_lockp, _state)					\
	do {								\
		if ((_state) != EFSYS_LOCK_MAGIC)			\
			ASSERT(B_FALSE);				\
		mutex_exit(_lockp);					\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

/* PREEMPT */

#define	EFSYS_PREEMPT_DISABLE(_state)					\
	do {								\
		(_state) = ddi_enter_critical();			\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFSYS_PREEMPT_ENABLE(_state)					\
	do {								\
		ddi_exit_critical(_state);				\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

/* STAT */

typedef kstat_named_t		efsys_stat_t;

#define	EFSYS_STAT_INCR(_knp, _delta) 					\
	do {								\
		((_knp)->value.ui64) += (_delta);			\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFSYS_STAT_DECR(_knp, _delta) 					\
	do {								\
		((_knp)->value.ui64) -= (_delta);			\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFSYS_STAT_SET(_knp, _val)					\
	do {								\
		((_knp)->value.ui64) = (_val);				\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFSYS_STAT_SET_QWORD(_knp, _valp)				\
	do {								\
		((_knp)->value.ui64) = LE_64((_valp)->eq_u64[0]);	\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFSYS_STAT_SET_DWORD(_knp, _valp)				\
	do {								\
		((_knp)->value.ui64) = LE_32((_valp)->ed_u32[0]);	\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFSYS_STAT_INCR_QWORD(_knp, _valp)				\
	do {								\
		((_knp)->value.ui64) += LE_64((_valp)->eq_u64[0]);	\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

#define	EFSYS_STAT_SUBR_QWORD(_knp, _valp)				\
	do {								\
		((_knp)->value.ui64) -= LE_64((_valp)->eq_u64[0]);	\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

/* ERR */

extern void	sfxge_err(efsys_identifier_t *, unsigned int,
		    uint32_t, uint32_t);

#if EFSYS_OPT_DECODE_INTR_FATAL
#define	EFSYS_ERR(_esip, _code, _dword0, _dword1)			\
	do {								\
		sfxge_err((_esip), (_code), (_dword0), (_dword1));	\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)
#endif

/* PROBE */

#define	EFSYS_PROBE(_name)						\
	DTRACE_PROBE(_name)

#define	EFSYS_PROBE1(_name, _type1, _arg1)				\
	DTRACE_PROBE1(_name, _type1, _arg1)

#define	EFSYS_PROBE2(_name, _type1, _arg1, _type2, _arg2)		\
	DTRACE_PROBE2(_name, _type1, _arg1, _type2, _arg2)

#define	EFSYS_PROBE3(_name, _type1, _arg1, _type2, _arg2,		\
	    _type3, _arg3)						\
	DTRACE_PROBE3(_name, _type1, _arg1, _type2, _arg2,		\
	    _type3, _arg3)

#define	EFSYS_PROBE4(_name, _type1, _arg1, _type2, _arg2,		\
	    _type3, _arg3, _type4, _arg4)				\
	DTRACE_PROBE4(_name, _type1, _arg1, _type2, _arg2,		\
	    _type3, _arg3, _type4, _arg4)

#define	EFSYS_PROBE5(_name, _type1, _arg1, _type2, _arg2,		\
	    _type3, _arg3, _type4, _arg4, _type5, _arg5)		\
	DTRACE_PROBE5(_name, _type1, _arg1, _type2, _arg2,		\
	    _type3, _arg3, _type4, _arg4, _type5, _arg5)

#ifdef DTRACE_PROBE6
#define	EFSYS_PROBE6(_name, _type1, _arg1, _type2, _arg2,		\
	    _type3, _arg3, _type4, _arg4, _type5, _arg5,		\
	    _type6, _arg6)						\
	DTRACE_PROBE6(_name, _type1, _arg1, _type2, _arg2,		\
	    _type3, _arg3, _type4, _arg4, _type5, _arg5,		\
	    _type6, _arg6)
#else
#define	EFSYS_PROBE6(_name, _type1, _arg1, _type2, _arg2,		\
	    _type3, _arg3, _type4, _arg4, _type5, _arg5,		\
	    _type6, _arg6)						\
	DTRACE_PROBE5(_name, _type1, _arg1, _type2, _arg2,		\
	    _type3, _arg3, _type4, _arg4, _type5, _arg5)
#endif

#ifdef DTRACE_PROBE7
#define	EFSYS_PROBE7(_name, _type1, _arg1, _type2, _arg2,		\
	    _type3, _arg3, _type4, _arg4, _type5, _arg5,		\
	    _type6, _arg6, _type7, _arg7)				\
	DTRACE_PROBE7(_name, _type1, _arg1, _type2, _arg2,		\
	    _type3, _arg3, _type4, _arg4, _type5, _arg5,		\
	    _type6, _arg6, _type7, _arg7)
#else
#define	EFSYS_PROBE7(_name, _type1, _arg1, _type2, _arg2,		\
	    _type3, _arg3, _type4, _arg4, _type5, _arg5,		\
	    _type6, _arg6, _type7, _arg7)				\
	DTRACE_PROBE5(_name, _type1, _arg1, _type2, _arg2,		\
	    _type3, _arg3, _type4, _arg4, _type5, _arg5)
#endif

/* ASSERT */

#define	EFSYS_ASSERT(_exp)		ASSERT(_exp)
#define	EFSYS_ASSERT3U(_x, _op, _y)	ASSERT3U(_x, _op, _y)
#define	EFSYS_ASSERT3S(_x, _op, _y)	ASSERT3S(_x, _op, _y)
#define	EFSYS_ASSERT3P(_x, _op, _y)	ASSERT3P(_x, _op, _y)

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_EFSYS_H */
