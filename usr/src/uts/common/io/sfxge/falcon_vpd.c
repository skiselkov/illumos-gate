/*
 * Copyright 2009 Solarflare Communications Inc.  All rights reserved.
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

/*
 * Falcon conviniently uses an EEPROM to store it's VPD configuration,
 * and it stores the VPD contents in native VPD format. This code does
 * not cope with the presence of an RW block in VPD at all.
 */

#include "efsys.h"
#include "efx.h"
#include "falcon_nvram.h"
#include "efx_types.h"
#include "efx_impl.h"

#if EFSYS_OPT_FALCON

#if EFSYS_OPT_VPD

typedef struct {
	size_t		fvpdd_base;
	size_t		fvpdd_size;
	boolean_t	fvpdd_writable;
} falcon_vpd_dimension_t;

static				void
falcon_vpd_dimension(
	__in			efx_nic_t *enp,
	__out			falcon_vpd_dimension_t *dimp)
{
	efx_oword_t oword;

#if EFSYS_OPT_FALCON_NIC_CFG_OVERRIDE
	if (enp->en_u.falcon.enu_forced_cfg != NULL) {
		memcpy(&oword, (enp->en_u.falcon.enu_forced_cfg
				+ EE_VPD_CFG0_REG_SF_OFST), sizeof (oword));
	}
	else
#endif	/* EFSYS_OPT_FALCON_NIC_CFG_OVERRIDE */
	{
		EFX_BAR_READO(enp, FR_AB_EE_VPD_CFG0_REG, &oword);
	}

	dimp->fvpdd_base = EFX_OWORD_FIELD(oword, FRF_AB_EE_VPD_BASE);
	dimp->fvpdd_size = EFX_OWORD_FIELD(oword, FRF_AB_EE_VPD_LENGTH);
	if (dimp->fvpdd_size != 0)
		/* Non-zero "lengths" are actually maximum dword offsets */
		dimp->fvpdd_size += 4;
	dimp->fvpdd_writable =
		EFX_OWORD_FIELD(oword, FRF_AB_EE_VPDW_LENGTH) != 0;
}

	__checkReturn		int
falcon_vpd_size(
	__in			efx_nic_t *enp,
	__out			size_t *sizep)
{
	falcon_vpd_dimension_t dim;
	int rc;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	falcon_vpd_dimension(enp, &dim);
	if (dim.fvpdd_size == 0 || dim.fvpdd_writable) {
		rc = ENOTSUP;
		goto fail1;
	}

	*sizep = dim.fvpdd_size;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	*sizep = 0;

	return (rc);
}

	__checkReturn		int
falcon_vpd_read(
	__in			efx_nic_t *enp,
	__out_bcount(size)	caddr_t data,
	__in			size_t size)
{
	falcon_vpd_dimension_t dim;
	int rc;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	falcon_vpd_dimension(enp, &dim);
	if (size < dim.fvpdd_size || size == 0) {
		rc = ENOTSUP;
		goto fail1;
	}

	if ((rc = falcon_spi_dev_read(enp, FALCON_SPI_EEPROM,
		    dim.fvpdd_base, data, size)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn		int
falcon_vpd_verify(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size)
{
	falcon_vpd_dimension_t dim;
	boolean_t cksummed;
	int rc;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	falcon_vpd_dimension(enp, &dim);
	EFSYS_ASSERT3U(dim.fvpdd_size, <=, size);
	EFSYS_ASSERT(!dim.fvpdd_writable);

	if ((rc = efx_vpd_hunk_verify(data, size, &cksummed)) != 0)
		goto fail1;

	if (!cksummed) {
		rc = EFAULT;
		goto fail2;
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn		int
falcon_vpd_get(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size,
	__inout			efx_vpd_value_t *evvp)
{
	falcon_vpd_dimension_t dim;
	unsigned int offset;
	uint8_t length;
	int rc;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	if (evvp->evv_tag != EFX_VPD_ID && evvp->evv_tag != EFX_VPD_RO) {
		rc = EINVAL;
		goto fail1;
	}

	falcon_vpd_dimension(enp, &dim);
	EFSYS_ASSERT3U(dim.fvpdd_size, <=, size);
	EFSYS_ASSERT(!dim.fvpdd_writable);

	if ((rc = efx_vpd_hunk_get(data, size, evvp->evv_tag,
	    evvp->evv_keyword, &offset, &length)) != 0)
		goto fail2;

	/* Copy out */
	evvp->evv_length = length;
	memcpy(evvp->evv_value, data + offset, length);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn		int
falcon_vpd_set(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size,
	__in			efx_vpd_value_t *evvp)
{
	falcon_vpd_dimension_t dim;
	int rc;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	falcon_vpd_dimension(enp, &dim);
	EFSYS_ASSERT3U(dim.fvpdd_size, <=, size);

	if ((rc = efx_vpd_hunk_set(data, size, evvp)) != 0)
		goto fail1;

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn		int
falcon_vpd_next(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size,
	__out			efx_vpd_value_t *evvp,
	__inout			unsigned int *contp)
{
	falcon_vpd_dimension_t dim;
	unsigned int offset;
	int rc;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	falcon_vpd_dimension(enp, &dim);
	EFSYS_ASSERT3U(dim.fvpdd_size, <=, size);
	EFSYS_ASSERT(!dim.fvpdd_writable);

	/* Find the (tag, keyword) */
	if ((rc = efx_vpd_hunk_next(data, size, &evvp->evv_tag,
	    &evvp->evv_keyword, &offset, &evvp->evv_length, contp)) != 0)
		goto fail1;

	/* Copyout */
	memcpy(evvp->evv_value, data + offset, evvp->evv_length);

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn		int
falcon_vpd_write(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size)
{
	falcon_vpd_dimension_t dim;
	int rc;

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	falcon_vpd_dimension(enp, &dim);
	if (dim.fvpdd_size != size) {
		/* User hasn't provided sufficient data */
		rc = EINVAL;
		goto fail1;
	}

	if ((rc = falcon_spi_dev_write(enp, FALCON_SPI_EEPROM,
		    dim.fvpdd_base, data, dim.fvpdd_size)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#endif	/* EFSYS_OPT_FALCON */

#endif	/* EFSYS_OPT_VPD */
