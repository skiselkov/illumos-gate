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

#if EFSYS_OPT_FALCON

#define	I2C_READ_CMD(_devid)	(((_devid) << 1) | 1)
#define	I2C_WRITE_CMD(_devid)	(((_devid) << 1) | 0)

#define	I2C_PERIOD	100

#define	I2C_RETRY_LIMIT	10

static			void
falcon_i2c_set_sda_scl(
	__in		efx_nic_t *enp,
	__in		falcon_i2c_t *fip,
	__inout_opt	efsys_timestamp_t *timep)
{
	efx_oword_t oword;
	efsys_timestamp_t delta = 0;

	EFSYS_SPIN(I2C_PERIOD);
	delta += I2C_PERIOD;

	/* Set the pin state */
	EFX_BAR_READO(enp, FR_AB_GPIO_CTL_REG, &oword);

	EFX_SET_OWORD_FIELD(oword, FRF_AB_GPIO3_OUT, 0);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_GPIO3_OEN,
			    fip->fi_sda ? 0 : 1);

	EFX_SET_OWORD_FIELD(oword, FRF_AB_GPIO0_OEN, 1);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_GPIO0_OUT,
			    fip->fi_scl ? 1 : 0);

	EFX_BAR_WRITEO(enp, FR_AB_GPIO_CTL_REG, &oword);

	EFSYS_SPIN(I2C_PERIOD);
	delta += I2C_PERIOD;

	if (timep != NULL)
		*timep += delta;
}

static	__checkReturn	boolean_t
falcon_i2c_get_sda(
	__in		efx_nic_t *enp,
	__inout_opt	efsys_timestamp_t *timep)
{
	efx_oword_t oword;
	efsys_timestamp_t delta = 0;
	boolean_t state;

	EFSYS_SPIN(I2C_PERIOD);
	delta += I2C_PERIOD;

	/* Get the pin state */
	EFX_BAR_READO(enp, FR_AB_GPIO_CTL_REG, &oword);
	state = (EFX_OWORD_FIELD(oword, FRF_AB_GPIO3_IN) != 0);

	EFSYS_SPIN(I2C_PERIOD);
	delta += I2C_PERIOD;

	if (timep != NULL)
		*timep += delta;

	return (state);
}

static	__checkReturn	boolean_t
falcon_i2c_get_scl(
	__in		efx_nic_t *enp,
	__inout_opt	efsys_timestamp_t *timep)
{
	efx_oword_t oword;
	efsys_timestamp_t delta = 0;
	boolean_t state;

	EFSYS_SPIN(I2C_PERIOD);
	delta += I2C_PERIOD;

	/* Get the pin state */
	EFX_BAR_READO(enp, FR_AB_GPIO_CTL_REG, &oword);
	state = (EFX_OWORD_FIELD(oword, FRF_AB_GPIO0_IN) != 0);

	EFSYS_SPIN(I2C_PERIOD);
	delta += I2C_PERIOD;

	if (timep != NULL)
		*timep += delta;

	return (state);
}

static			void
falcon_i2c_sda_write(
	__in		efx_nic_t *enp,
	__in		boolean_t on,
	__inout_opt	efsys_timestamp_t *timep)
{
	falcon_i2c_t *fip = &(enp->en_u.falcon.enu_fip);

	fip->fi_sda = on;
	falcon_i2c_set_sda_scl(enp, fip, timep);
}

static			void
falcon_i2c_scl_write(
	__in		efx_nic_t *enp,
	__in   		boolean_t on,
	__inout_opt	efsys_timestamp_t *timep)
{
	falcon_i2c_t *fip = &(enp->en_u.falcon.enu_fip);

	fip->fi_scl = on;
	falcon_i2c_set_sda_scl(enp, fip, timep);
}

static			void
falcon_i2c_start(
	__in		efx_nic_t *enp,
	__inout_opt	efsys_timestamp_t *timep)
{
	falcon_i2c_t *fip = &(enp->en_u.falcon.enu_fip);

	EFSYS_ASSERT(timep != NULL);

	EFSYS_ASSERT(fip->fi_sda);

	falcon_i2c_scl_write(enp, B_TRUE, timep);
	falcon_i2c_sda_write(enp, B_FALSE, timep);
	falcon_i2c_scl_write(enp, B_FALSE, timep);
	falcon_i2c_sda_write(enp, B_TRUE, timep);
}

static			void
falcon_i2c_bit_out(
	__in		efx_nic_t *enp,
	__in		boolean_t bit,
	__inout_opt	efsys_timestamp_t *timep)
{
	falcon_i2c_t *fip = &(enp->en_u.falcon.enu_fip);

	EFSYS_ASSERT(timep != NULL);

	EFSYS_ASSERT(!(fip->fi_scl));

	falcon_i2c_sda_write(enp, bit, timep);
	falcon_i2c_scl_write(enp, B_TRUE, timep);
	falcon_i2c_scl_write(enp, B_FALSE, timep);
	falcon_i2c_sda_write(enp, B_TRUE, timep);
}

static			boolean_t
falcon_i2c_bit_in(
	__in		efx_nic_t *enp,
	__inout_opt	efsys_timestamp_t *timep)
{
	falcon_i2c_t *fip = &(enp->en_u.falcon.enu_fip);
	boolean_t bit;

	EFSYS_ASSERT(timep != NULL);

	EFSYS_ASSERT(!(fip->fi_scl));
	EFSYS_ASSERT(fip->fi_sda);

	falcon_i2c_scl_write(enp, B_TRUE, timep);
	bit = falcon_i2c_get_sda(enp, timep);
	falcon_i2c_scl_write(enp, B_FALSE, timep);

	return (bit);
}

static			void
falcon_i2c_stop(
	__in		efx_nic_t *enp,
	__inout_opt	efsys_timestamp_t *timep)
{
	falcon_i2c_t *fip = &(enp->en_u.falcon.enu_fip);

	EFSYS_ASSERT(timep != NULL);

	EFSYS_ASSERT(!(fip->fi_scl));

	falcon_i2c_sda_write(enp, B_FALSE, timep);
	falcon_i2c_scl_write(enp, B_TRUE, timep);
	falcon_i2c_sda_write(enp, B_TRUE, timep);
}

static			void
falcon_i2c_release(
	__in		efx_nic_t *enp,
	__inout_opt	efsys_timestamp_t *timep)
{
	falcon_i2c_t *fip = &(enp->en_u.falcon.enu_fip);

	EFSYS_ASSERT(timep != NULL);

	falcon_i2c_scl_write(enp, B_TRUE, timep);
	falcon_i2c_sda_write(enp, B_TRUE, timep);

	EFSYS_ASSERT(falcon_i2c_get_sda(enp, timep));
	EFSYS_ASSERT(falcon_i2c_get_scl(enp, timep));

	EFSYS_ASSERT(fip->fi_scl);
	EFSYS_ASSERT(fip->fi_sda);

	EFSYS_SPIN(10000);
	*timep += 10000;
}

static	__checkReturn	int
falcon_i2c_wait(
	__in		efx_nic_t *enp)
{
	falcon_i2c_t *fip = &(enp->en_u.falcon.enu_fip);
	int count;
	int rc;

	if (enp->en_u.falcon.enu_i2c_locked) {
		rc = EBUSY;
		goto fail1;
	}

	EFSYS_ASSERT(fip->fi_scl);
	EFSYS_ASSERT(fip->fi_sda);

	count = 0;
	do {
		EFSYS_PROBE1(wait, unsigned int, count);

		if (falcon_i2c_get_scl(enp, NULL) &&
		    falcon_i2c_get_sda(enp, NULL))
			goto done;

		/* Spin for 1 ms */
		EFSYS_SPIN(1000);

	} while (++count < 20);

	rc = ETIMEDOUT;
	goto fail2;

done:
	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static	__checkReturn	int
falcon_i2c_byte_out(
	__in		efx_nic_t *enp,
	__in		uint8_t byte,
	__inout_opt	efsys_timestamp_t *timep)
{
	unsigned int bit;

	for (bit = 0; bit < 8; bit++) {
		falcon_i2c_bit_out(enp, ((byte & 0x80) != 0), timep);
		byte <<= 1;
	}

	/* Check for acknowledgement */
	return ((falcon_i2c_bit_in(enp, timep)) ? EIO : 0);
}

static			uint8_t
falcon_i2c_byte_in(
	__in		efx_nic_t *enp,
	__in		boolean_t ack,
	__inout_opt	efsys_timestamp_t *timep)
{
	uint8_t byte;
	unsigned int bit;

	byte = 0;
	for (bit = 0; bit < 8; bit++) {
		byte <<= 1;
		byte |= falcon_i2c_bit_in(enp, timep);
	}

	/* Send acknowledgement */
	falcon_i2c_bit_out(enp, !ack, timep);

	return (byte);
}

	__checkReturn	int
falcon_i2c_check(
	__in		efx_nic_t *enp,
	__in		uint8_t devid)
{
	int pstate;
	int lstate;
	efsys_timestamp_t start;
	efsys_timestamp_t end;
	efsys_timestamp_t expected;
	unsigned int attempt;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	EFSYS_LOCK(enp->en_eslp, lstate);
	EFSYS_PREEMPT_DISABLE(pstate);

	/* Check the state of the I2C bus */
	if ((rc = falcon_i2c_wait(enp)) != 0)
		goto fail1;

	attempt = 0;

again:
	EFSYS_TIMESTAMP(&start);
	expected = 0;

	/* Select device and write cycle */
	falcon_i2c_start(enp, &expected);

	if ((rc = falcon_i2c_byte_out(enp, I2C_WRITE_CMD(devid),
	    &expected)) != 0)
		goto fail2;

	/* Abort the cycle since we're only testing the target is there */
	falcon_i2c_stop(enp, &expected);
	falcon_i2c_release(enp, &expected);

	EFSYS_TIMESTAMP(&end);

	/* The transaction may have timed out */
	if (end - start > expected * 2) {
		if (++attempt < I2C_RETRY_LIMIT) {
			EFSYS_PROBE1(retry, unsigned int, attempt);

			goto again;
		}

		rc = ETIMEDOUT;
		goto fail3;
	}

	EFSYS_PREEMPT_ENABLE(pstate);
	EFSYS_UNLOCK(enp->en_eslp, lstate);

	EFSYS_PROBE(success);
	return (0);

fail3:
	EFSYS_PROBE(fail3);

	goto fail1;

fail2:
	falcon_i2c_stop(enp, &expected);
	falcon_i2c_release(enp, &expected);

	EFSYS_TIMESTAMP(&end);

	/* The transaction may have timed out */
	if (end - start > expected * 2) {
		if (++attempt < I2C_RETRY_LIMIT) {
			EFSYS_PROBE1(retry, unsigned int, attempt);

			goto again;
		}

		rc = ETIMEDOUT;
	}

	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	EFSYS_PREEMPT_ENABLE(pstate);
	EFSYS_UNLOCK(enp->en_eslp, lstate);
	return (rc);
}

	__checkReturn		int
falcon_i2c_read(
	__in			efx_nic_t *enp,
	__in			uint8_t devid,
	__in			uint8_t addr,
	__out_bcount(size)	caddr_t base,
	__in			size_t size)
{
	int pstate;
	int lstate;
	efsys_timestamp_t start;
	efsys_timestamp_t end;
	efsys_timestamp_t expected;
	unsigned int attempt;
	unsigned int i;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	EFSYS_ASSERT(size != 0);

	EFSYS_LOCK(enp->en_eslp, lstate);
	EFSYS_PREEMPT_DISABLE(pstate);

	/* Check the state of the I2C bus */
	if ((rc = falcon_i2c_wait(enp)) != 0)
		goto fail1;

	attempt = 0;

again:
	EFSYS_TIMESTAMP(&start);
	expected = 0;

	/* Select device and write cycle */
	falcon_i2c_start(enp, &expected);

	if ((rc = falcon_i2c_byte_out(enp, I2C_WRITE_CMD(devid),
	    &expected)) != 0)
		goto fail2;

	/* Write address */
	if ((rc = falcon_i2c_byte_out(enp, addr, &expected)) != 0)
		goto fail3;

	/* Select device and read cycle */
	falcon_i2c_start(enp, &expected);

	if ((rc = falcon_i2c_byte_out(enp, I2C_READ_CMD(devid),
	    &expected)) != 0)
		goto fail4;

	/* Read bytes */
	for (i = 0; i < size; i++)
		*(uint8_t *)(base + i) =
			falcon_i2c_byte_in(enp, (i < size - 1), &expected);

	falcon_i2c_stop(enp, &expected);
	falcon_i2c_release(enp, &expected);

	EFSYS_TIMESTAMP(&end);

	/* The transaction may have timed out */
	if (end - start > expected * 2) {
		if (++attempt < I2C_RETRY_LIMIT) {
			EFSYS_PROBE1(retry, unsigned int, attempt);

			goto again;
		}

		rc = ETIMEDOUT;
		goto fail5;
	}

	EFSYS_PREEMPT_ENABLE(pstate);
	EFSYS_UNLOCK(enp->en_eslp, lstate);
	return (0);

fail5:
	EFSYS_PROBE(fail5);

	goto fail1;

fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	falcon_i2c_stop(enp, &expected);
	falcon_i2c_release(enp, &expected);

	EFSYS_TIMESTAMP(&end);

	/* The transaction may have timed out */
	if (end - start > expected * 2) {
		if (++attempt < I2C_RETRY_LIMIT) {
			EFSYS_PROBE1(retry, unsigned int, attempt);

			goto again;
		}

		rc = ETIMEDOUT;
	}

	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	EFSYS_PREEMPT_ENABLE(pstate);
	EFSYS_UNLOCK(enp->en_eslp, lstate);
	return (rc);
}

	__checkReturn		int
falcon_i2c_write(
	__in			efx_nic_t *enp,
	__in			uint8_t devid,
	__in			uint8_t addr,
	__in_bcount(size)	caddr_t base,
	__in			size_t size)
{
	int pstate;
	int lstate;
	efsys_timestamp_t start;
	efsys_timestamp_t end;
	efsys_timestamp_t expected;
	unsigned int attempt;
	unsigned int i;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	EFSYS_ASSERT(size != 0);

	EFSYS_LOCK(enp->en_eslp, lstate);
	EFSYS_PREEMPT_DISABLE(pstate);

	/* Check the state of the I2C bus */
	if ((rc = falcon_i2c_wait(enp)) != 0)
		goto fail1;

	attempt = 0;

again:
	EFSYS_TIMESTAMP(&start);
	expected = 0;

	/* Select device and write cycle */
	falcon_i2c_start(enp, &expected);

	if ((rc = falcon_i2c_byte_out(enp, I2C_WRITE_CMD(devid),
	    &expected)) != 0)
		goto fail2;

	/* Write address */
	if ((rc = falcon_i2c_byte_out(enp, addr, &expected)) != 0)
		goto fail3;

	/* Write bytes */
	for (i = 0; i < size; i++) {
		if ((rc = falcon_i2c_byte_out(enp, *(uint8_t *)(base + i),
		    &expected)) != 0)
			goto fail4;
	}

	falcon_i2c_stop(enp, &expected);
	falcon_i2c_release(enp, &expected);

	EFSYS_TIMESTAMP(&end);

	/* The transaction may have timed out */
	if (end - start > expected * 2) {
		if (++attempt < I2C_RETRY_LIMIT) {
			EFSYS_PROBE1(retry, unsigned int, attempt);

			goto again;
		}

		rc = ETIMEDOUT;
		goto fail5;
	}

	EFSYS_PREEMPT_ENABLE(pstate);
	EFSYS_UNLOCK(enp->en_eslp, lstate);
	return (0);

fail5:
	EFSYS_PROBE(fail5);

	goto fail1;

fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	falcon_i2c_stop(enp, &expected);
	falcon_i2c_release(enp, &expected);

	EFSYS_TIMESTAMP(&end);

	/* The transaction may have timed out */
	if (end - start > expected * 2) {
		if (++attempt < I2C_RETRY_LIMIT) {
			EFSYS_PROBE1(retry, unsigned int, attempt);

			goto again;
		}

		rc = ETIMEDOUT;
	}

	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	EFSYS_PREEMPT_ENABLE(pstate);
	EFSYS_UNLOCK(enp->en_eslp, lstate);
	return (rc);
}

#if EFSYS_OPT_PHY_NULL

	__checkReturn		int
falcon_i2c_recv(
	__in			efx_nic_t *enp,
	__in			uint8_t devid,
	__out_bcount(size)	caddr_t base,
	__in			size_t size)
{
	int pstate;
	int lstate;
	efsys_timestamp_t start;
	efsys_timestamp_t end;
	efsys_timestamp_t expected;
	unsigned int attempt;
	unsigned int i;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	EFSYS_ASSERT(size != 0);

	EFSYS_LOCK(enp->en_eslp, lstate);
	EFSYS_PREEMPT_DISABLE(pstate);

	/* Check the state of the I2C bus */
	if ((rc = falcon_i2c_wait(enp)) != 0)
		goto fail1;

	attempt = 0;

again:
	EFSYS_TIMESTAMP(&start);
	expected = 0;

	/* Select device and read cycle */
	falcon_i2c_start(enp, &expected);

	if ((rc = falcon_i2c_byte_out(enp, I2C_READ_CMD(devid), &expected))
	    != 0)
		goto fail2;

	/* Read bytes */
	for (i = 0; i < size; i++)
		*(uint8_t *)(base + i) = falcon_i2c_byte_in(enp, (i < size - 1),
		    &expected);

	falcon_i2c_stop(enp, &expected);
	falcon_i2c_release(enp, &expected);

	EFSYS_TIMESTAMP(&end);

	/* The transaction may have timed out */
	if (end - start > expected * 2) {
		if (++attempt < I2C_RETRY_LIMIT) {
			EFSYS_PROBE1(retry, unsigned int, attempt);

			goto again;
		}

		rc = ETIMEDOUT;
		goto fail3;
	}

	EFSYS_PREEMPT_ENABLE(pstate);
	EFSYS_UNLOCK(enp->en_eslp, lstate);
	return (0);

fail3:
	EFSYS_PROBE(fail5);

	goto fail1;

fail2:
	falcon_i2c_stop(enp, &expected);
	falcon_i2c_release(enp, &expected);

	EFSYS_TIMESTAMP(&end);

	/* The transaction may have timed out */
	if (end - start > expected * 2) {
		if (++attempt < I2C_RETRY_LIMIT) {
			EFSYS_PROBE1(retry, unsigned int, attempt);

			goto again;
		}

		rc = ETIMEDOUT;
	}

	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	EFSYS_PREEMPT_ENABLE(pstate);
	EFSYS_UNLOCK(enp->en_eslp, lstate);
	return (rc);
}

	__checkReturn		int
falcon_i2c_send(
	__in			efx_nic_t *enp,
	__in			uint8_t devid,
	__in_bcount(size)	caddr_t base,
	__in			size_t size)
{
	int pstate;
	int lstate;
	efsys_timestamp_t start;
	efsys_timestamp_t end;
	efsys_timestamp_t expected;
	unsigned int attempt;
	unsigned int i;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	EFSYS_ASSERT(size != 0);

	EFSYS_LOCK(enp->en_eslp, lstate);
	EFSYS_PREEMPT_DISABLE(pstate);

	/* Check the state of the I2C bus */
	if ((rc = falcon_i2c_wait(enp)) != 0)
		goto fail1;

	attempt = 0;

again:
	EFSYS_TIMESTAMP(&start);
	expected = 0;

	/* Select device and write cycle */
	falcon_i2c_start(enp, &expected);

	if ((rc = falcon_i2c_byte_out(enp, I2C_WRITE_CMD(devid), &expected))
	    != 0)
		goto fail2;

	/* Write bytes */
	for (i = 0; i < size; i++)
		if ((rc = falcon_i2c_byte_out(enp,	*(uint8_t *)(base + i),
		    &expected)) != 0)
			goto fail3;

	falcon_i2c_stop(enp, &expected);
	falcon_i2c_release(enp, &expected);

	EFSYS_TIMESTAMP(&end);

	/* The transaction may have timed out */
	if (end - start > expected * 2) {
		if (++attempt < I2C_RETRY_LIMIT) {
			EFSYS_PROBE1(retry, unsigned int, attempt);

			goto again;
		}

		rc = ETIMEDOUT;
		goto fail4;
	}

	EFSYS_PREEMPT_ENABLE(pstate);
	EFSYS_UNLOCK(enp->en_eslp, lstate);
	return (0);

fail4:
	EFSYS_PROBE(fail5);

	goto fail1;

fail3:
	EFSYS_PROBE(fail3);
fail2:
	falcon_i2c_stop(enp, &expected);
	falcon_i2c_release(enp, &expected);

	EFSYS_TIMESTAMP(&end);

	/* The transaction may have timed out */
	if (end - start > expected * 2) {
		if (++attempt < I2C_RETRY_LIMIT) {
			EFSYS_PROBE1(retry, unsigned int, attempt);

			goto again;
		}

		rc = ETIMEDOUT;
	}

	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	EFSYS_PREEMPT_ENABLE(pstate);
	EFSYS_UNLOCK(enp->en_eslp, lstate);
	return (rc);
}
#endif	/* EFSYS_OPT_PHY_NULL */

#endif	/* EFSYS_OPT_FALCON */
