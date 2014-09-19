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

#include <sys/types.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

#include "sfxge.h"

#include "efx.h"

static const char *__sfxge_err[] = {
	"",
	"SRAM out-of-bounds",
	"Buffer ID out-of-bounds",
	"Internal memory parity",
	"Receive buffer ownership",
	"Transmit buffer ownership",
	"Receive descriptor ownership",
	"Transmit descriptor ownership",
	"Event queue ownership",
	"Event queue FIFO overflow",
	"Illegal address",
	"SRAM parity"
};

void
sfxge_err(efsys_identifier_t *arg, unsigned int code, uint32_t dword0,
    uint32_t dword1)
{
	sfxge_t *sp = (sfxge_t *)arg;
	dev_info_t *dip = sp->s_dip;

	ASSERT3U(code, <, EFX_ERR_NCODES);

	cmn_err(CE_WARN, SFXGE_CMN_ERR "[%s%d] FATAL ERROR: %s (0x%08x%08x)",
	    ddi_driver_name(dip), ddi_get_instance(dip), __sfxge_err[code],
	    dword1, dword0);
}

void
sfxge_intr_fatal(sfxge_t *sp)
{
	efx_nic_t *enp = sp->s_enp;
	int err;

	efx_intr_disable(enp);
	efx_intr_fatal(enp);

	err = sfxge_restart_dispatch(sp, DDI_NOSLEEP, SFXGE_HW_ERR,
	    "Fatal Interrupt", 0);
	if (err != 0) {
		cmn_err(CE_WARN, SFXGE_CMN_ERR
			    "[%s%d] UNRECOVERABLE ERROR:"
			    " Could not schedule driver restart."
			    " err=%d\n",
			    ddi_driver_name(sp->s_dip),
			    ddi_get_instance(sp->s_dip),
			    err);
		ASSERT(B_FALSE);
	}
}
