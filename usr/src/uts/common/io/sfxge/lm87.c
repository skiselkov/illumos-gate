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
#include "lm87.h"
#include "lm87_impl.h"

#if EFSYS_OPT_MON_LM87

	__checkReturn	int
lm87_reset(
	__in		efx_nic_t *enp)
{
	uint8_t devid = enp->en_u.falcon.enu_mon_devid;
	efx_byte_t byte;
	int rc;

	/* Check we can communicate with the chip */
	if ((rc = falcon_i2c_check(enp, devid)) != 0)
		goto fail1;

	/* Reset the chip */
	EFX_POPULATE_BYTE_1(byte, INIT, 1);
	if ((rc = falcon_i2c_write(enp, devid, CONFIG1_REG, (caddr_t)&byte,
	    1)) != 0)
		goto fail2;

	/* Check the company identifier and revision */
	if ((rc = falcon_i2c_read(enp, devid, ID_REG, (caddr_t)&byte, 1)) != 0)
		goto fail3;

	if (EFX_BYTE_FIELD(byte, EFX_BYTE_0) != ID_DECODE) {
		rc = ENODEV;
		goto fail4;
	}

	if ((rc = falcon_i2c_read(enp, devid, REV_REG, (caddr_t)&byte, 1)) != 0)
		goto fail5;

	if (EFX_BYTE_FIELD(byte, EFX_BYTE_0) != REV_DECODE) {
		rc = ENODEV;
		goto fail6;
	}

	return (0);

fail6:
	EFSYS_PROBE(fail6);
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
lm87_reconfigure(
	__in		efx_nic_t *enp)
{
	uint8_t devid = enp->en_u.falcon.enu_mon_devid;
	efx_byte_t byte;
	int rc;

	/* Configure the channel mode to select AIN1/2 rather than FAN1/2 */
	EFX_POPULATE_BYTE_2(byte, FAN1_AIN1, 1, FAN2_AIN2, 1);
	if ((rc = falcon_i2c_write(enp, devid, CHANNEL_MODE_REG, (caddr_t)&byte,
	    1)) != 0)
		goto fail1;

	/* Mask out all interrupts */
	EFX_SET_BYTE(byte);
	if ((rc = falcon_i2c_write(enp, devid, INTERRUPT_MASK1_REG,
	    (caddr_t)&byte, 1)) != 0)
		goto fail2;
	if ((rc = falcon_i2c_write(enp, devid, INTERRUPT_MASK2_REG,
	    (caddr_t)&byte, 1)) != 0)
		goto fail3;

	/* Start monitoring */
	EFX_POPULATE_BYTE_1(byte, START, 1);
	if ((rc = falcon_i2c_write(enp, devid, CONFIG1_REG, (caddr_t)&byte,
	    1)) != 0)
		goto fail4;

	return (0);

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

#define	LM87_STAT_SET(_enp, _mask, _value, _id, _rc)			\
	do {								\
		uint8_t devid = enp->en_u.falcon.enu_mon_devid;		\
		efx_mon_stat_value_t *value =				\
			(_value) + EFX_MON_STAT_ ## _id;		\
		efx_byte_t byte;					\
									\
		if (((_rc) = falcon_i2c_read((_enp), devid, 		\
		    VALUE_ ## _id ## _REG, (caddr_t)&byte, 1)) == 0) {	\
			uint8_t val = EFX_BYTE_FIELD(byte, EFX_BYTE_0);	\
			value->emsv_value = (uint16_t)val;		\
			value->emsv_state = 0;				\
		}							\
									\
		(_mask) |= (1 << (EFX_MON_STAT_ ## _id));		\
		_NOTE(CONSTANTCONDITION)				\
	} while (B_FALSE)

	__checkReturn			int
lm87_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__out_ecount(EFX_MON_NSTATS)	efx_mon_stat_value_t *values)
{
	uint32_t mask = 0;
	int rc;

	_NOTE(ARGUNUSED(esmp))

	LM87_STAT_SET(enp, mask, values, 2_5V, rc);
	if (rc != 0)
		goto fail1;

	LM87_STAT_SET(enp, mask, values, VCCP1, rc);
	if (rc != 0)
		goto fail2;

	LM87_STAT_SET(enp, mask, values, VCC, rc);
	if (rc != 0)
		goto fail3;

	LM87_STAT_SET(enp, mask, values, 5V, rc);
	if (rc != 0)
		goto fail4;

	LM87_STAT_SET(enp, mask, values, 12V, rc);
	if (rc != 0)
		goto fail5;

	LM87_STAT_SET(enp, mask, values, VCCP2, rc);
	if (rc != 0)
		goto fail6;

	LM87_STAT_SET(enp, mask, values, EXT_TEMP, rc);
	if (rc != 0)
		goto fail7;

	LM87_STAT_SET(enp, mask, values, INT_TEMP, rc);
	if (rc != 0)
		goto fail8;

	LM87_STAT_SET(enp, mask, values, AIN1, rc);
	if (rc != 0)
		goto fail9;

	LM87_STAT_SET(enp, mask, values, AIN2, rc);
	if (rc != 0)
		goto fail10;

	EFSYS_ASSERT3U(mask, ==, LM87_STAT_MASK);

	return (0);

fail10:
	EFSYS_PROBE(fail10);
fail9:
	EFSYS_PROBE(fail9);
fail8:
	EFSYS_PROBE(fail8);
fail7:
	EFSYS_PROBE(fail7);
fail6:
	EFSYS_PROBE(fail6);
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
#endif	/* EFSYS_OPT_MON_STATS */

#endif	/* EFSYS_OPT_MON_LM87 */
