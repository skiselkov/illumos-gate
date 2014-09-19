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
#include "falcon_nvram.h"
#include "efx_types.h"
#include "efx_impl.h"

#if EFSYS_OPT_FALCON

#if EFSYS_OPT_MON_LM87
#include "lm87.h"
#endif

#if EFSYS_OPT_MON_MAX6647
#include "max6647.h"
#endif

#if EFSYS_OPT_NVRAM_SFX7101
#include "sfx7101.h"
#endif

#if EFSYS_OPT_NVRAM_SFT9001
#include "sft9001.h"
#endif

#define	FALCON_NVRAM_INIT_LOWEST_REG				\
	MIN(CFG_VERSION_REG_SF_OFST,				\
	    MIN(CFG_FLASH_DEV_REG_SF_OFST,			\
		CFG_EEPROM_DEV_REG_SF_OFST))

#define	FALCON_NVRAM_INIT_HIGHEST_REG    			\
	MAX(CFG_VERSION_REG_SF_OFST,				\
	    MAX(CFG_FLASH_DEV_REG_SF_OFST,			\
		CFG_EEPROM_DEV_REG_SF_OFST))

#define	FALCON_NVRAM_INIT_NEEDED_CFG_SIZE 			\
	(FALCON_NVRAM_INIT_HIGHEST_REG + sizeof (efx_oword_t) 	\
	    - FALCON_NVRAM_INIT_LOWEST_REG)

	__checkReturn	int
falcon_nvram_init(
	__in		efx_nic_t *enp)
{
	falcon_spi_dev_t *fsdp;
	efx_oword_t oword;
	uint16_t version;
	int rc;
	uint8_t cfg[FALCON_NVRAM_INIT_NEEDED_CFG_SIZE];
	uint8_t *origin = cfg - FALCON_NVRAM_INIT_LOWEST_REG;

	/* All boards have flash and EEPROM */
	EFX_BAR_READO(enp, FR_AB_NIC_STAT_REG, &oword);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_SF_PRST, 1);
	EFX_SET_OWORD_FIELD(oword, FRF_AB_EE_PRST, 1);
	EFX_BAR_WRITEO(enp, FR_AB_NIC_STAT_REG, &oword);

	/* Set up partial flash parameters */
	fsdp = &(enp->en_u.falcon.enu_fsd[FALCON_SPI_FLASH]);
	fsdp->fsd_sf_sel = 1;
	fsdp->fsd_adbcnt = 3;
	fsdp->fsd_munge = B_FALSE;

	if ((rc = falcon_nic_cfg_raw_read_verify(enp,
	    FALCON_NVRAM_INIT_LOWEST_REG,
	    FALCON_NVRAM_INIT_NEEDED_CFG_SIZE, cfg)) != 0)
		goto fail1;

	version = EFX_WORD_FIELD(
		*(efx_word_t *)(origin + CFG_VERSION_REG_SF_OFST), VERSION);

	if (version < 3) {
		fsdp->fsd_size = (size_t)1 << 17;
		fsdp->fsd_erase_cmd = 0x52;
		fsdp->fsd_erase_size = (size_t)1 << 15;
		fsdp->fsd_write_size = (size_t)1 << 8;
	} else {
		efx_dword_t *dwordp =
			(efx_dword_t *)(origin + CFG_FLASH_DEV_REG_SF_OFST);

		if (EFX_DWORD_FIELD(*dwordp, SPI_DEV_ADBCNT) != 3 ||
		    EFX_DWORD_FIELD(*dwordp, SPI_DEV_SIZE) >= 24) {
			rc = EINVAL;
			goto fail2;
		}

		fsdp->fsd_size = (size_t)1 << EFX_DWORD_FIELD(*dwordp,
		    SPI_DEV_SIZE);
		fsdp->fsd_erase_cmd = EFX_DWORD_FIELD(*dwordp,
					    SPI_DEV_ERASE_CMD);
		fsdp->fsd_erase_size = (size_t)1 << EFX_DWORD_FIELD(*dwordp,
		    SPI_DEV_ERASE_SIZE);
		fsdp->fsd_write_size = (size_t)1 << EFX_DWORD_FIELD(*dwordp,
		    SPI_DEV_WRITE_SIZE);
	}

	/* Configure the EEPROM */
	fsdp = &(enp->en_u.falcon.enu_fsd[FALCON_SPI_EEPROM]);

	fsdp->fsd_sf_sel = 0;
	if (version < 3) {
		fsdp->fsd_adbcnt = 1;
		fsdp->fsd_size = (size_t)1 << 9;
		fsdp->fsd_munge = B_TRUE;
		fsdp->fsd_erase_cmd = 0;
		fsdp->fsd_erase_size = 1;
		fsdp->fsd_write_size = (size_t)1 << 3;
	} else {
		efx_dword_t *dwordp =
			(efx_dword_t *)(origin + CFG_EEPROM_DEV_REG_SF_OFST);

		fsdp->fsd_adbcnt = EFX_DWORD_FIELD(*dwordp, SPI_DEV_ADBCNT);
		fsdp->fsd_size = (size_t)1 << EFX_DWORD_FIELD(*dwordp,
		    SPI_DEV_SIZE);
		fsdp->fsd_munge = (EFX_DWORD_FIELD(*dwordp, SPI_DEV_SIZE) >
		    fsdp->fsd_adbcnt * 8);
		fsdp->fsd_erase_cmd = EFX_DWORD_FIELD(*dwordp,
		    SPI_DEV_ERASE_CMD);
		fsdp->fsd_erase_size = (size_t)1 << EFX_DWORD_FIELD(*dwordp,
		    SPI_DEV_ERASE_SIZE);
		fsdp->fsd_write_size = (size_t)1 << EFX_DWORD_FIELD(*dwordp,
		    SPI_DEV_WRITE_SIZE);
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	(void) memset(enp->en_u.falcon.enu_fsd, 0,
	    sizeof (enp->en_u.falcon.enu_fsd));

	return (rc);
}

#if EFSYS_OPT_NVRAM

typedef struct falcon_nvram_ops_s {
	int	(*fnvo_size)(efx_nic_t *, size_t *);
	int	(*fnvo_get_version)(efx_nic_t *, uint32_t *, uint16_t *);
	int	(*fnvo_rw_start)(efx_nic_t *, size_t *);
	int	(*fnvo_read_chunk)(efx_nic_t *, unsigned int,
		    caddr_t, size_t);
	int	(*fnvo_erase)(efx_nic_t *);
	int	(*fnvo_write_chunk)(efx_nic_t *, unsigned int,
		    caddr_t, size_t);
	void	(*fnvo_rw_finish)(efx_nic_t *);
} falcon_nvram_ops_t;

#if EFSYS_OPT_NVRAM_FALCON_BOOTROM

#define	FALCON_GPXE_IMAGE_OFFSET	0x8000
#define	FALCON_GPXE_IMAGE_SIZE		0x18000

static	__checkReturn		int
falcon_nvram_bootrom_size(
	__in			efx_nic_t *enp,
	__out			size_t *sizep)
{
	_NOTE(ARGUNUSED(enp))
	EFSYS_ASSERT(sizep != NULL);
	*sizep = FALCON_GPXE_IMAGE_SIZE;

	return (0);
}

static	__checkReturn		int
falcon_nvram_bootrom_get_version(
	__in			efx_nic_t *enp,
	__out			uint32_t *subtypep,
	__out_ecount(4)		uint16_t version[4])
{
	const char prefix[] = "Solarstorm Boot Manager (v";
	char buf[16], p;
	size_t current, needle;
	uint16_t *versionp;
	int rc;

	version[0] = version[1] = version[2] = version[3] = 0;
	versionp = NULL;
	needle = 0;

	/*
	 * Search from [current, end) for prefix, and return the
	 * trailing four decimal number.
	 */
	for (current = 0; current < 0x600; current++) {
		if (current % sizeof (buf) == 0) {
			if ((rc = falcon_spi_dev_read(enp, FALCON_SPI_FLASH,
			    FALCON_GPXE_IMAGE_OFFSET + current, buf,
			    sizeof (buf))) != 0)
				break;
		}

		p = buf[current % sizeof (buf)];
		if (versionp == NULL) {
			if (prefix[needle] == p) {
				++needle;
				if (needle == sizeof (prefix) - 1)
					versionp = version;
			} else
				needle = 0;
		} else {
			if (p == ')' && versionp == version + 3)
				goto done;
			else if (p >= '0' && p <= '9')
				*versionp = (*versionp * 10) + (p - '0');
			else if (p == '.' && versionp != version + 3)
				++versionp;
			else
				/* Invalid format */
				break;
		}
	}

	version[0] = version[1] = version[2] = version[3] = 0;

done:
	*subtypep = 0;	/* Falcon bootrom is type 0 */

	return (0);
}

static	__checkReturn		int
falcon_nvram_bootrom_rw_start(
	__in			efx_nic_t *enp,
	__out			size_t *chunk_sizep)
{
	_NOTE(ARGUNUSED(enp))
	if (chunk_sizep != NULL)
		*chunk_sizep = sizeof (efx_oword_t);

	return (0);
}

static	__checkReturn		int
falcon_nvram_bootrom_read_chunk(
	__in			efx_nic_t *enp,
	__in			unsigned int offset,
	__out_bcount(size)	caddr_t data,
	__in			size_t size)
{
	int rc;

	EFSYS_ASSERT3U(size + offset, <=, FALCON_GPXE_IMAGE_SIZE);

	if ((rc = falcon_spi_dev_read(enp, FALCON_SPI_FLASH,
	    FALCON_GPXE_IMAGE_OFFSET + offset, data, size)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static	 __checkReturn		int
falcon_nvram_bootrom_erase(
	__in			efx_nic_t *enp)
{
	int rc;

	if ((rc = falcon_spi_dev_erase(enp, FALCON_SPI_FLASH,
	    FALCON_GPXE_IMAGE_OFFSET, FALCON_GPXE_IMAGE_SIZE)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static	__checkReturn		int
falcon_nvram_bootrom_write_chunk(
	__in			efx_nic_t *enp,
	__in			unsigned int offset,
	__out_bcount(size)	caddr_t base,
	__in			size_t size)
{
	int rc;

	if ((rc = falcon_spi_dev_write(enp, FALCON_SPI_FLASH,
	    FALCON_GPXE_IMAGE_OFFSET + offset, base, size)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static falcon_nvram_ops_t	__cs	__falcon_nvram_bootrom_ops = {
	falcon_nvram_bootrom_size,		/* fnvo_size */
	falcon_nvram_bootrom_get_version,	/* fnvo_get_version */
	falcon_nvram_bootrom_rw_start,		/* fnvo_rw_start */
	falcon_nvram_bootrom_read_chunk,	/* fnvo_read_chunk */
	falcon_nvram_bootrom_erase,		/* fnvo_erase */
	falcon_nvram_bootrom_write_chunk,	/* fnvo_write_chunk */
	NULL,					/* fnvo_rw_finish */
};

#define	FALCON_GPXE_CFG_OFFSET		0x800

static	__checkReturn		int
falcon_nvram_bootrom_cfg_size(
	__in			efx_nic_t *enp,
	__out			size_t *sizep)
{
	falcon_spi_dev_t *fsdp =
		&(enp->en_u.falcon.enu_fsd[FALCON_SPI_EEPROM]);
	int rc;

	EFSYS_ASSERT(sizep != NULL);
	EFSYS_ASSERT(fsdp != NULL);

	if (fsdp->fsd_size < FALCON_GPXE_CFG_OFFSET) {
		*sizep = 0;
		rc = ENOTSUP;
		goto fail1;
	}

	*sizep = fsdp->fsd_size - FALCON_GPXE_CFG_OFFSET;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static	__checkReturn		int
falcon_nvram_bootrom_cfg_get_version(
	__in			efx_nic_t *enp,
	__out			uint32_t *subtypep,
	__out_ecount(4)		uint16_t version[4])
{
	falcon_spi_dev_t *fsdp =
		&(enp->en_u.falcon.enu_fsd[FALCON_SPI_EEPROM]);
	int rc;

	EFSYS_ASSERT(fsdp != NULL);
	if (fsdp->fsd_size < FALCON_GPXE_CFG_OFFSET) {
		rc = ENOTSUP;
		goto fail1;
	}

	/* gpxecfg is not versioned */
	*subtypep = 0;
	version[0] = version[1] = version[2] = version[3];

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static	__checkReturn		int
falcon_nvram_bootrom_cfg_rw_start(
	__in			efx_nic_t *enp,
	__out			size_t *chunk_sizep)
{
	falcon_spi_dev_t *fsdp =
		&(enp->en_u.falcon.enu_fsd[FALCON_SPI_EEPROM]);
	int rc;

	EFSYS_ASSERT(fsdp != NULL);
	if (fsdp->fsd_size < FALCON_GPXE_CFG_OFFSET) {
		rc = ENOTSUP;
		goto fail1;
	}

	if (chunk_sizep != NULL)
		*chunk_sizep = sizeof (efx_oword_t);

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}


static	__checkReturn		int
falcon_nvram_bootrom_cfg_read_chunk(
	__in			efx_nic_t *enp,
	__in			unsigned int offset,
	__out_bcount(size)	caddr_t data,
	__in			size_t size)
{
	falcon_spi_dev_t *fsdp =
		&(enp->en_u.falcon.enu_fsd[FALCON_SPI_EEPROM]);
	int rc;

	EFSYS_ASSERT(fsdp != NULL);
	EFSYS_ASSERT3U(fsdp->fsd_size, >=, FALCON_GPXE_CFG_OFFSET);
	EFSYS_ASSERT3U(offset + size, <=,
	    fsdp->fsd_size - FALCON_GPXE_CFG_OFFSET);

	if ((rc = falcon_spi_dev_read(enp, FALCON_SPI_EEPROM,
	    FALCON_GPXE_CFG_OFFSET + offset, data, size)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static	__checkReturn		int
falcon_nvram_bootrom_cfg_write_chunk(
	__in			efx_nic_t *enp,
	__in			unsigned int offset,
	__in_bcount(size)	caddr_t base,
	__in			size_t size)
{
	falcon_spi_dev_t *fsdp =
		&(enp->en_u.falcon.enu_fsd[FALCON_SPI_EEPROM]);
	int rc;

	EFSYS_ASSERT(fsdp != NULL);
	EFSYS_ASSERT3U(fsdp->fsd_size, >=, FALCON_GPXE_CFG_OFFSET);
	EFSYS_ASSERT3U(offset + size, <=,
	    fsdp->fsd_size - FALCON_GPXE_CFG_OFFSET);

	if ((rc = falcon_spi_dev_write(enp, FALCON_SPI_EEPROM,
	    FALCON_GPXE_CFG_OFFSET + offset, base, size)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

static falcon_nvram_ops_t	__cs	__falcon_nvram_bootrom_cfg_ops = {
	falcon_nvram_bootrom_cfg_size,		/* fnvo_size */
	falcon_nvram_bootrom_cfg_get_version,	/* fnvo_get_version */
	falcon_nvram_bootrom_cfg_rw_start,	/* fnvo_rw_start */
	falcon_nvram_bootrom_cfg_read_chunk,	/* fnvo_read_chunk */
	NULL,					/* fnvo_erase */
	falcon_nvram_bootrom_cfg_write_chunk,	/* fnvo_write_chunk */
	NULL,					/* fnvo_rw_finish */
};

#endif	/* EFSYS_OPT_NVRAM_FALCON_BOOTROM */

#if EFSYS_OPT_NVRAM_SFX7101

static falcon_nvram_ops_t	__cs	__falcon_sfx7101_ops = {
	sfx7101_nvram_size,		/* fnvo_size */
	sfx7101_nvram_get_version,	/* fnvo_get_version */
	sfx7101_nvram_rw_start,		/* fnvo_rw_start */
	sfx7101_nvram_read_chunk,	/* fnvo_read_chunk */
	sfx7101_nvram_erase,		/* fnvo_erase */
	sfx7101_nvram_write_chunk,	/* fnvo_write_chunk */
	sfx7101_nvram_rw_finish,	/* fnvo_rw_finish */
};

#endif	/* EFSYS_OPT_NVRAM_SFX7101 */

#if EFSYS_OPT_NVRAM_SFT9001

static falcon_nvram_ops_t	__cs	__falcon_sft9001_ops = {
	sft9001_nvram_size,		/* fnvo_size */
	sft9001_nvram_get_version,	/* fnvo_get_version */
	sft9001_nvram_rw_start,		/* fnvo_rw_start */
	sft9001_nvram_read_chunk,	/* fnvo_read_chunk */
	sft9001_nvram_erase,		/* fnvo_erase */
	sft9001_nvram_write_chunk,	/* fnvo_write_chunk */
	sft9001_nvram_rw_finish,	/* fnvo_rw_finish */
};

#endif	/* EFSYS_OPT_NVRAM_SFT9001 */

static	__checkReturn		int
falcon_nvram_get_ops(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out			falcon_nvram_ops_t **fnvopp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	falcon_nvram_ops_t *fnvop;
	int rc;

	EFSYS_ASSERT3U(type, <, EFX_NVRAM_NTYPES);

	switch (type) {
#if EFSYS_OPT_NVRAM_FALCON_BOOTROM
	case EFX_NVRAM_BOOTROM_CFG:
		fnvop = (falcon_nvram_ops_t *)&__falcon_nvram_bootrom_cfg_ops;
		goto done;

	case EFX_NVRAM_BOOTROM:
		fnvop = (falcon_nvram_ops_t *)&__falcon_nvram_bootrom_ops;
		goto done;
#endif
	case EFX_NVRAM_PHY:
		switch (encp->enc_phy_type) {
#if EFSYS_OPT_NVRAM_SFX7101
		case EFX_PHY_SFX7101:
			fnvop = (falcon_nvram_ops_t *)&__falcon_sfx7101_ops;
			goto done;
#endif	/* EFSYS_OPT_NVRAM_SFX7101 */

#if EFSYS_OPT_NVRAM_SFT9001
		case EFX_PHY_SFT9001B:
			fnvop = (falcon_nvram_ops_t *)&__falcon_sft9001_ops;
			goto done;
#endif	/* EFSYS_OPT_NVRAM_SFT9001 */

		default:
			break;
		}

		break;

	default:
		break;
	}

	rc = ENOTSUP;
	goto fail1;

done:
	*fnvopp = fnvop;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#if EFSYS_OPT_DIAG

	__checkReturn		int
falcon_nvram_test(
	__in			efx_nic_t *enp)
{
	efx_nic_cfg_t enc;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	if ((rc = falcon_nic_cfg_build(enp, &enc)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#endif	/* EFSYS_OPT_DIAG */

	__checkReturn		int
falcon_nvram_size(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out			size_t *sizep)
{
	falcon_nvram_ops_t *fnvop;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	if ((rc = falcon_nvram_get_ops(enp, type, &fnvop)) != 0)
		goto fail1;

	if ((rc = fnvop->fnvo_size(enp, sizep)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn		int
falcon_nvram_get_version(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out			uint32_t *subtypep,
	__out_ecount(4)		uint16_t version[4])
{
	falcon_nvram_ops_t *fnvop;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	if ((rc = falcon_nvram_get_ops(enp, type, &fnvop)) != 0)
		goto fail1;

	if ((rc = fnvop->fnvo_get_version(enp, subtypep, version)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn		int
falcon_nvram_rw_start(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out			size_t *chunk_sizep)
{
	falcon_nvram_ops_t *fnvop;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	if ((rc = falcon_nvram_get_ops(enp, type, &fnvop)) != 0)
		goto fail1;

	if ((rc = fnvop->fnvo_rw_start(enp, chunk_sizep)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn		int
falcon_nvram_read_chunk(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__in			unsigned int offset,
	__out_bcount(size)	caddr_t data,
	__in			size_t size)
{
	falcon_nvram_ops_t *fnvop;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	if ((rc = falcon_nvram_get_ops(enp, type, &fnvop)) != 0)
		goto fail1;

	if ((rc = fnvop->fnvo_read_chunk(enp, offset, data, size)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

	__checkReturn		int
falcon_nvram_erase(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type)
{
	falcon_nvram_ops_t *fnvop;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	if ((rc = falcon_nvram_get_ops(enp, type, &fnvop)) != 0)
		goto fail1;

	if (fnvop->fnvo_erase != NULL) {
		if ((rc = fnvop->fnvo_erase(enp)) != 0)
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
falcon_nvram_write_chunk(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__in			unsigned int offset,
	__in_bcount(size)	caddr_t data,
	__in			size_t size)
{
	falcon_nvram_ops_t *fnvop;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	if ((rc = falcon_nvram_get_ops(enp, type, &fnvop)) != 0)
		goto fail1;

	if ((rc = fnvop->fnvo_write_chunk(enp, offset, data, size)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

				void
falcon_nvram_rw_finish(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type)
{
	falcon_nvram_ops_t *fnvop;
	int rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	rc = falcon_nvram_get_ops(enp, type, &fnvop);
	EFSYS_ASSERT(rc == 0);
	if (rc == 0) {
		if (fnvop->fnvo_rw_finish != NULL)
			fnvop->fnvo_rw_finish(enp);
	}
}

	__checkReturn		int
falcon_nvram_set_version(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out			uint16_t version[4])
{
	falcon_nvram_ops_t *fnvop;
	uint32_t subtype;
	uint16_t old_version[4];
	int rc;

	_NOTE(ARGUNUSED(enp))
	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_family, ==, EFX_FAMILY_FALCON);

	/*
	 * There is no room on Falcon to version anything, so it's
	 * inferred where possible from the underlying entity.
	 */
	if ((rc = falcon_nvram_get_ops(enp, type, &fnvop)) != 0)
		goto fail1;

	/*
	 * The user *really should be setting the version correctly
	 */
	if ((rc = fnvop->fnvo_get_version(enp, &subtype, old_version)) != 0)
		goto fail2;
	EFSYS_ASSERT(memcmp(old_version, version, sizeof (old_version)) == 0);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, int, rc);

	return (rc);
}

#endif	/* EFSYS_OPT_NVRAM */

		void
falcon_nvram_fini(
	__in	efx_nic_t *enp)
{
	(void) memset(&enp->en_u.falcon.enu_fsd, 0,
	    sizeof (enp->en_u.falcon.enu_fsd));
}

#endif	/* EFSYS_OPT_FALCON */
