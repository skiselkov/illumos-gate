/*
 * Copyright 2007-2013 Solarflare Communications Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "efsys.h"
#include "efx.h"
#include "efx_types.h"
#include "efx_regs.h"
#include "efx_impl.h"
#include "nullphy.h"
#include "nullphy_impl.h"

#if EFSYS_OPT_PHY_NULL

	__checkReturn	int
nullphy_reset(
	__in		efx_nic_t *enp)
{
	_NOTE(ARGUNUSED(enp))

	enp->en_reset_flags |= EFX_RESET_PHY;

	return (0);
}

	__checkReturn	int
nullphy_reconfigure(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_word_t word;
	efx_word_t check;
	int rc;

	_NOTE(ARGUNUSED(enp))

	EFX_POPULATE_WORD_3(word, HOSTPORT_EQ, 1, PORTSEL, 0, CX4uC_RESET, 1);

	if ((rc = falcon_i2c_send(enp, PCF8575, (caddr_t)&word.ew_byte[0],
	    sizeof (word.ew_byte) / sizeof (efx_byte_t))) != 0)
		goto fail1;

	if ((rc = falcon_i2c_recv(enp, PCF8575, (caddr_t)&check.ew_byte[0],
	    sizeof (check.ew_byte) / sizeof (efx_byte_t))) != 0)
		goto fail2;

	if (EFX_WORD_FIELD(check, EFX_WORD_0) !=
	    EFX_WORD_FIELD(word, EFX_WORD_0)) {
		rc = EFAULT;
		goto fail3;
	}

	EFSYS_ASSERT3U(epp->ep_adv_cap_mask, ==, NULLPHY_ADV_CAP_MASK);

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn	int
nullphy_verify(
	__in		efx_nic_t *enp)
{
	_NOTE(ARGUNUSED(enp))

	return (ENOTSUP);
}

	__checkReturn	int
nullphy_downlink_check(
	__in		efx_nic_t *enp,
	__out		efx_link_mode_t *modep,
	__out		unsigned int *fcntlp,
	__out		uint32_t *lp_cap_maskp)
{
	efx_port_t *epp = &(enp->en_port);

	*modep = EFX_LINK_10000FDX;
	*fcntlp = epp->ep_fcntl;
	*lp_cap_maskp = NULLPHY_ADV_CAP_MASK;

	return (0);
}

	__checkReturn	int
nullphy_lp_cap_get(
	__in		efx_nic_t *enp,
	__out		uint32_t *maskp)
{
	_NOTE(ARGUNUSED(enp, maskp))

	return (ENOTSUP);
}

	__checkReturn	int
nullphy_oui_get(
	__in		efx_nic_t *enp,
	__out		uint32_t *ouip)
{
	_NOTE(ARGUNUSED(enp, ouip))

	return (ENOTSUP);
}

#if EFSYS_OPT_PHY_STATS

	__checkReturn			int
nullphy_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__out_ecount(EFX_PHY_NSTATS)	uint32_t *stat)
{
	_NOTE(ARGUNUSED(enp, esmp, stat))

	return (ENOTSUP);
}
#endif	/* EFSYS_OPT_PHY_STATS */

#if EFSYS_OPT_PHY_PROPS

#if EFSYS_OPT_NAMES
		const char __cs *
nullphy_prop_name(
	__in	efx_nic_t *enp,
	__in	unsigned int id)
{
	_NOTE(ARGUNUSED(enp, id))

	EFSYS_ASSERT(B_FALSE);

	return (NULL);
}
#endif	/* EFSYS_OPT_NAMES */

	__checkReturn	int
nullphy_prop_get(
	__in		efx_nic_t *enp,
	__in		unsigned int id,
	__in		uint32_t flags,
	__out		uint32_t *valp)
{
	_NOTE(ARGUNUSED(enp, id, flags, valp))

	EFSYS_ASSERT(B_FALSE);

	return (ENOTSUP);
}

	__checkReturn	int
nullphy_prop_set(
	__in		efx_nic_t *enp,
	__in		unsigned int id,
	__in		uint32_t val)
{
	_NOTE(ARGUNUSED(enp, id, val))

	EFSYS_ASSERT(B_FALSE);

	return (ENOTSUP);
}
#endif	/* EFSYS_OPT_PHY_PROPS */

#endif	/* EFSYS_OPT_PHY_NULL */
