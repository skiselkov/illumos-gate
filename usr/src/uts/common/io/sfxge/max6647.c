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
#include "max6647.h"
#include "max6647_impl.h"

#if EFSYS_OPT_MON_MAX6647

	__checkReturn	int
max6647_reset(
	__in		efx_nic_t *enp)
{
	uint8_t devid = enp->en_u.falcon.enu_mon_devid;
	efx_byte_t byte;
	int rc;

	if ((rc = falcon_i2c_check(enp, devid)) != 0)
		goto fail1;

	/* Reset the chip */
	EFX_POPULATE_BYTE_2(byte, MASK, 1, NRUN, 1);
	if ((rc = falcon_i2c_write(enp, devid, WCA_REG, (caddr_t)&byte,
	    1)) != 0)
		goto fail2;

	EFSYS_SPIN(10);

	EFX_POPULATE_BYTE_2(byte, MASK, 1, NRUN, 0);
	if ((rc = falcon_i2c_write(enp, devid, WCA_REG, (caddr_t)&byte,
	    1)) != 0)
		goto fail3;

	/* Check the company identifier and revision */
	if ((rc = falcon_i2c_read(enp, devid, MFID_REG, (caddr_t)&byte,
	    1)) != 0)
		goto fail2;

	if (EFX_BYTE_FIELD(byte, EFX_BYTE_0) != MFID_DECODE) {
		rc = ENODEV;
		goto fail3;
	}

	if ((rc = falcon_i2c_read(enp, devid, REVID_REG, (caddr_t)&byte,
	    1)) != 0)
		goto fail4;

	if (EFX_BYTE_FIELD(byte, EFX_BYTE_0) != REVID_DECODE) {
		rc = ENODEV;
		goto fail5;
	}

	return (0);

fail5:
	EFSYS_PROBE(fail5);
fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn	int
max6647_reconfigure(
	__in		efx_nic_t *enp)
{
	uint8_t devid = enp->en_u.falcon.enu_mon_devid;
	efx_byte_t byte;
	int rc;

	/* Clear any latched status */
	if ((rc = falcon_i2c_read(enp, devid, RSL_REG, (caddr_t)&byte,
	    1)) != 0)
		goto fail1;

	/* Override the default alert limit */
	EFX_POPULATE_BYTE_1(byte, EFX_BYTE_0, 90);
	if ((rc = falcon_i2c_write(enp, devid, WLHO_REG, (caddr_t)&byte,
	    1)) != 0)
		goto fail2;

	/* Read it back and verify */
	if ((rc = falcon_i2c_read(enp, devid, RLHN_REG, (caddr_t)&byte,
	    1)) != 0)
		goto fail3;

	if (EFX_BYTE_FIELD(byte, EFX_BYTE_0) != 90) {
		rc = EFAULT;
		goto fail4;
	}

	/* Enable the alert signal */
	EFX_POPULATE_BYTE_2(byte, MASK, 0, NRUN, 0);
	if ((rc = falcon_i2c_write(enp, devid, WCA_REG, (caddr_t)&byte,
	    1)) != 0)
		goto fail5;

	return (0);

fail5:
	EFSYS_PROBE(fail5);
fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#if EFSYS_OPT_MON_STATS

#define	MAX6647_STAT_SET(_enp, _value, _id, _rc)			\
	do {								\
		uint8_t devid = enp->en_u.falcon.enu_mon_devid;		\
		efx_byte_t byte;					\
									\
		if (((_rc) = falcon_i2c_read((_enp), devid,		\
		    _id ## _REG, (caddr_t)&byte, 1)) == 0) {		\
			(_value)->emsv_value =				\
				(uint16_t)EFX_BYTE_FIELD(byte, 		\
							EFX_BYTE_0);	\
			(_value)->emsv_state = 0;			\
		}							\
		_NOTE(CONSTANTCONDITION)				\
	} while (B_FALSE)

	__checkReturn			int
max6647_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__out_ecount(EFX_MON_NSTATS)	efx_mon_stat_value_t *values)
{
	int rc;

	_NOTE(ARGUNUSED(esmp))

	MAX6647_STAT_SET(enp, values + EFX_MON_STAT_INT_TEMP, RLTS, rc);
	if (rc != 0)
		goto fail1;

	MAX6647_STAT_SET(enp, values + EFX_MON_STAT_EXT_TEMP, RRTE, rc);
	if (rc != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}
#endif	/* EFSYS_OPT_MON_STATS */

#endif	/* EFSYS_OPT_MON_MAX6647 */
