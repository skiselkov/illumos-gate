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

/* Commands common to all known devices */

#define	SPI_CMD_WRSR 0x01	/* write status register */
#define	SPI_CMD_WRITE 0x02	/* Write data to memory array */
#define	SPI_CMD_READ 0x03	/* Read data from memory array */
#define	SPI_CMD_WRDI 0x04	/* reset write enable latch */
#define	SPI_CMD_RDSR 0x05	/* read status register */
#define	SPI_CMD_WREN 0x06	/* set write enable latch */

#define	SPI_CMD_SECE 0x52	/* erase one sector */

#define	SPI_STATUS_WPEN_LBN 7
#define	SPI_STATUS_WPEN_WIDTH 1
#define	SPI_STATUS_BP2_LBN 4
#define	SPI_STATUS_BP2_WIDTH 1
#define	SPI_STATUS_BP1_LBN 3
#define	SPI_STATUS_BP1_WIDTH 1
#define	SPI_STATUS_BP0_LBN 2
#define	SPI_STATUS_BP0_WIDTH 1
#define	SPI_STATUS_WEN_LBN 1
#define	SPI_STATUS_WEN_WIDTH 1
#define	SPI_STATUS_NRDY_LBN 0
#define	SPI_STATUS_NRDY_WIDTH 1

static	__checkReturn	int
falcon_spi_wait(
	__in		efx_nic_t *enp)
{
	unsigned int count;
	int rc;

	count = 0;
	do {
		efx_oword_t oword;

		EFSYS_PROBE1(wait, unsigned int, count);

		EFX_BAR_READO(enp, FR_AB_EE_SPI_HCMD_REG, &oword);

		if (EFX_OWORD_FIELD(oword, FRF_AB_EE_SPI_HCMD_CMD_EN) == 0)
			goto done;

		/* Spin for 10 us */
		EFSYS_SPIN(10);
	} while (++count < 10000);

	rc = ETIMEDOUT;
	goto fail1;

done:
	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#define	FALCON_SPI_COPY(_dst, _src, _size)				\
	do {								\
		unsigned int index;					\
									\
		for (index = 0; index < (unsigned int)(_size); index++)	\
			*((uint8_t *)(_dst) + index) =			\
			    *((uint8_t *)(_src) + index);		\
									\
	_NOTE(CONSTANTCONDITION)					\
	} while (B_FALSE)

static	__checkReturn			int
falcon_spi_cmd(
	__in				efx_nic_t *enp,
	__in				uint32_t sf_sel,
	__in				uint32_t adbcnt,
	__in				uint32_t cmd,
	__in				uint32_t addr,
	__in				boolean_t munge,
	__drv_when(cmd == SPI_CMD_WRSR || cmd == SPI_CMD_WRITE,
	    __drv_in(__byte_readableTo(dabcnt)))
	__drv_when(cmd == SPI_CMD_RDSR || cmd == SPI_CMD_READ,
	    __drv_out(__byte_readableTo(dabcnt)))
					caddr_t base,
	__in				uint32_t dabcnt)
{
	uint32_t enc;
	efx_oword_t oword;
	int rc;

	EFSYS_ASSERT3U(dabcnt, <=,  sizeof (efx_oword_t));

	/* Wait for the SPI to become available */
	if ((rc = falcon_spi_wait(enp)) != 0)
		goto fail1;

	/* Program the data register */
	if (cmd == SPI_CMD_WRSR || cmd == SPI_CMD_WRITE) {
		EFSYS_ASSERT(base != NULL);
		EFSYS_ASSERT(dabcnt != 0);
		EFX_ZERO_OWORD(oword);
		FALCON_SPI_COPY(&oword, base, dabcnt);
		EFX_BAR_WRITEO(enp, FR_AB_EE_SPI_HDATA_REG, &oword);
	}

	/* Program the address register */
	if (cmd == SPI_CMD_READ || cmd == SPI_CMD_WRITE ||
	    cmd == SPI_CMD_SECE) {
		EFX_POPULATE_OWORD_1(oword, FRF_AB_EE_SPI_HADR_ADR, addr);
		EFX_BAR_WRITEO(enp, FR_AB_EE_SPI_HADR_REG, &oword);

		enc = (munge) ? (cmd | ((addr >> 8) << 3)) : cmd;
	} else {
		enc = cmd;
	}

	/* Issue command */
	EFX_POPULATE_OWORD_6(oword, FRF_AB_EE_SPI_HCMD_CMD_EN, 1,
	    FRF_AB_EE_SPI_HCMD_SF_SEL, sf_sel,
	    FRF_AB_EE_SPI_HCMD_DABCNT, dabcnt,
	    FRF_AB_EE_SPI_HCMD_DUBCNT, 0,
	    FRF_AB_EE_SPI_HCMD_ADBCNT, adbcnt,
	    FRF_AB_EE_SPI_HCMD_ENC, enc);

	EFX_SET_OWORD_FIELD(oword,
	    FRF_AB_EE_SPI_HCMD_READ,
	    (cmd == SPI_CMD_RDSR || cmd == SPI_CMD_READ) ? 1 : 0);

	EFX_BAR_WRITEO(enp, FR_AB_EE_SPI_HCMD_REG, &oword);

	/* Wait for read to complete */
	if ((rc = falcon_spi_wait(enp)) != 0)
		goto fail2;

	/* Read the data register */
	if (cmd == SPI_CMD_RDSR || cmd == SPI_CMD_READ) {
		EFX_BAR_READO(enp, FR_AB_EE_SPI_HDATA_REG, &oword);
		EFSYS_ASSERT(base != NULL);
		FALCON_SPI_COPY(base, &oword, dabcnt);
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static	__checkReturn	int
falcon_spi_dev_wait(
	__in		efx_nic_t *enp,
	__in		falcon_spi_dev_t *fsdp,
	__in		unsigned int us,
	__in		unsigned int n)
{
	unsigned int count;
	int rc;

	count = 0;
	do {
		efx_byte_t byte;

		EFSYS_PROBE1(wait, unsigned int, count);

		if ((rc = falcon_spi_cmd(enp, fsdp->fsd_sf_sel,
		    fsdp->fsd_adbcnt, SPI_CMD_RDSR, 0, fsdp->fsd_munge,
		    (caddr_t)&byte, sizeof (efx_byte_t))) != 0)
			goto fail1;

		if (EFX_BYTE_FIELD(byte, SPI_STATUS_NRDY) == 0)
			goto done;

		EFSYS_SPIN(us);
	} while (++count < n);

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

	__checkReturn		int
falcon_spi_dev_read(
	__in			efx_nic_t *enp,
	__in			falcon_spi_type_t type,
	__in			uint32_t addr,
	__out_bcount(size)	caddr_t base,
	__in			size_t size)
{
	falcon_spi_dev_t *fsdp = &(enp->en_u.falcon.enu_fsd[type]);
	uint32_t end = addr + size;
	int state;
	int rc;

	EFSYS_ASSERT3U(type, <, FALCON_SPI_NTYPES);

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	if (fsdp == NULL) {
		rc = ENODEV;
		goto fail1;
	}

	EFSYS_LOCK(enp->en_eslp, state);

	while (addr != end) {
		uint32_t dabcnt = MIN(end - addr, sizeof (efx_oword_t));

		if ((rc = falcon_spi_cmd(enp, fsdp->fsd_sf_sel,
		    fsdp->fsd_adbcnt, SPI_CMD_READ, addr, fsdp->fsd_munge,
		    base, dabcnt)) != 0)
			goto fail2;

		EFSYS_ASSERT3U(addr, <, end);
		addr += dabcnt;
		base += dabcnt;
	}

	EFSYS_UNLOCK(enp->en_eslp, state);
	return (0);

fail2:
	EFSYS_PROBE(fail2);

	EFSYS_UNLOCK(enp->en_eslp, state);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn		int
falcon_spi_dev_write(
	__in			efx_nic_t *enp,
	__in			falcon_spi_type_t type,
	__in			uint32_t addr,
	__in_bcount(size)	caddr_t base,
	__in			size_t size)
{
	falcon_spi_dev_t *fsdp = &(enp->en_u.falcon.enu_fsd[type]);
	uint32_t end = addr + size;
	int state;
	int rc;

	EFSYS_ASSERT3U(type, <, FALCON_SPI_NTYPES);

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	if (fsdp == NULL) {
		rc = ENODEV;
		goto fail1;
	}

	EFSYS_LOCK(enp->en_eslp, state);

	while (addr != end) {
		efx_oword_t oword;
		uint32_t dabcnt;
		unsigned int index;

		if ((rc = falcon_spi_cmd(enp, fsdp->fsd_sf_sel,
			    fsdp->fsd_adbcnt, SPI_CMD_WREN, 0, fsdp->fsd_munge,
			    NULL, 0)) != 0)
			goto fail2;

		dabcnt = MIN(end - addr, fsdp->fsd_write_size -
		    (addr & (fsdp->fsd_write_size - 1)));
		dabcnt = MIN(dabcnt, sizeof (efx_oword_t));

		if ((rc = falcon_spi_cmd(enp, fsdp->fsd_sf_sel,
			    fsdp->fsd_adbcnt, SPI_CMD_WRITE, addr,
			    fsdp->fsd_munge, base, dabcnt)) != 0)
			goto fail3;

		if ((rc = falcon_spi_dev_wait(enp, fsdp, 1000, 20)) != 0)
			goto fail4;

		if ((rc = falcon_spi_cmd(enp, fsdp->fsd_sf_sel,
			    fsdp->fsd_adbcnt, SPI_CMD_READ, addr,
			    fsdp->fsd_munge, (caddr_t)&oword, dabcnt)) != 0)
			goto fail5;

		for (index = 0; index < dabcnt; index++) {
			if (oword.eo_u8[index] != *(uint8_t *)(base + index)) {
				rc = EIO;
				goto fail6;
			}
		}

		EFSYS_ASSERT3U(addr, <, end);
		addr += dabcnt;
		base += dabcnt;
	}

	EFSYS_UNLOCK(enp->en_eslp, state);
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

	EFSYS_UNLOCK(enp->en_eslp, state);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn	int
falcon_spi_dev_erase(
	__in		efx_nic_t *enp,
	__in		falcon_spi_type_t type,
	__in		uint32_t addr,
	__in		size_t size)
{
	falcon_spi_dev_t *fsdp = &(enp->en_u.falcon.enu_fsd[type]);
	uint32_t end = addr + size;
	int state;
	int rc;

	EFSYS_ASSERT3U(type, <, FALCON_SPI_NTYPES);

	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	if (fsdp == NULL) {
		rc = ENODEV;
		goto fail1;
	}

	if (fsdp->fsd_erase_cmd == 0) {
		rc = ENOTSUP;
		goto fail2;
	}

	if (!IS_P2ALIGNED(addr, fsdp->fsd_erase_size) ||
	    !IS_P2ALIGNED(size, fsdp->fsd_erase_size)) {
		rc = EINVAL;
		goto fail3;
	}

	EFSYS_LOCK(enp->en_eslp, state);

	while (addr != end) {
		if ((rc = falcon_spi_cmd(enp, fsdp->fsd_sf_sel,
			    fsdp->fsd_adbcnt, SPI_CMD_WREN, 0, fsdp->fsd_munge,
			    NULL, 0)) != 0)
			goto fail4;

		if ((rc = falcon_spi_cmd(enp, fsdp->fsd_sf_sel,
			    fsdp->fsd_adbcnt, fsdp->fsd_erase_cmd, addr,
			    fsdp->fsd_munge, NULL, 0)) != 0)
			goto fail5;

		if ((rc = falcon_spi_dev_wait(enp, fsdp, 40000, 100)) != 0)
			goto fail6;

		EFSYS_ASSERT3U(addr, <, end);
		addr += fsdp->fsd_erase_size;
	}

	EFSYS_UNLOCK(enp->en_eslp, state);
	return (0);

fail6:
	EFSYS_PROBE(fail6);
fail5:
	EFSYS_PROBE(fail5);
fail4:
	EFSYS_PROBE(fail4);

	EFSYS_UNLOCK(enp->en_eslp, state);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#endif	/* EFSYS_OPT_FALCON */
