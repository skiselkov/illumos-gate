/*-
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

#ifndef _SYS_FALCON_IMPL_H
#define	_SYS_FALCON_IMPL_H

#include "efx.h"
#include "efx_regs.h"

#ifdef	__cplusplus
extern "C" {
#endif

typedef struct falcon_i2c_s {
	boolean_t	fi_sda;
	boolean_t	fi_scl;
} falcon_i2c_t;

typedef struct falcon_spi_dev_s {
	uint32_t	fsd_sf_sel;
	size_t		fsd_size;
	uint32_t	fsd_adbcnt;
	boolean_t	fsd_munge;
	uint32_t	fsd_erase_cmd;
	size_t 		fsd_erase_size;
	size_t		fsd_write_size;
} falcon_spi_dev_t;

extern	__checkReturn	int
falcon_nic_probe(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_PCIE_TUNE

extern 			int
falcon_nic_pcie_tune(
	__in		efx_nic_t *enp,
	__in		unsigned int nlanes);

#endif

#define	FALCON_NIC_CFG_RAW_SZ 0x400

extern	__checkReturn	int
falcon_nic_cfg_raw_read_verify(
	__in		efx_nic_t *enp,
	__in		uint32_t offset,
	__in		uint32_t size,
	__out		uint8_t *cfg);

extern	__checkReturn	int
falcon_nic_cfg_build(
	__in		efx_nic_t *enp,
	__out		efx_nic_cfg_t *encp);

extern	__checkReturn	int
falcon_nic_reset(
	__in		efx_nic_t *enp);

extern	__checkReturn	int
falcon_nic_init(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_DIAG

extern	__checkReturn	int
falcon_nic_register_test(
	__in		efx_nic_t *enp);

#endif	/* EFSYS_OPT_DIAG */

extern			void
falcon_nic_fini(
	__in		efx_nic_t *enp);

extern			void
falcon_nic_unprobe(
	__in		efx_nic_t *enp);

extern	__checkReturn	int
falcon_nic_mac_reset(
	__in		efx_nic_t *enp);

extern			void
falcon_mac_wrapper_enable(
	__in		efx_nic_t *enp);

extern	__checkReturn	int
falcon_mac_wrapper_disable(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_LOOPBACK

extern	__checkReturn	int
falcon_mac_loopback_set(
	__in		efx_nic_t *enp,
	__in		efx_link_mode_t link_mode,
	__in		efx_loopback_type_t loopback_type);

#endif	/* EFSYS_OPT_LOOPBACK */

extern			void
falcon_nic_phy_reset(
	__in		efx_nic_t *enp);

extern	__checkReturn		int
falcon_nvram_init(
	__in			efx_nic_t *enp);

#if EFSYS_OPT_NVRAM

#if EFSYS_OPT_DIAG

extern	__checkReturn		int
falcon_nvram_test(
	__in			efx_nic_t *enp);

#endif	/* EFSYS_OPT_DIAG */

extern	__checkReturn		int
falcon_nvram_size(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out			size_t *sizep);

extern	__checkReturn		int
falcon_nvram_get_version(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out			uint32_t *subtypep,
	__out_ecount(4)		uint16_t version[4]);

extern	__checkReturn		int
falcon_nvram_rw_start(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out			size_t *pref_chunkp);

extern	__checkReturn		int
falcon_nvram_read_chunk(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__in			unsigned int offset,
	__out_bcount(size)	caddr_t data,
	__in			size_t size);

extern	 __checkReturn		int
falcon_nvram_erase(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type);

extern	__checkReturn		int
falcon_nvram_write_chunk(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__in			unsigned int offset,
	__in_bcount(size)	caddr_t data,
	__in			size_t size);

extern				void
falcon_nvram_rw_finish(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type);

extern	__checkReturn		int
falcon_nvram_set_version(
	__in			efx_nic_t *enp,
	__in			efx_nvram_type_t type,
	__out			uint16_t version[4]);

#endif	/* EFSYS_OPT_NVRAM */

extern				void
falcon_nvram_fini(
	__in			efx_nic_t *enp);

#if EFSYS_OPT_VPD

extern	__checkReturn		int
falcon_vpd_size(
	__in			efx_nic_t *enp,
	__out			size_t *sizep);

extern	__checkReturn		int
falcon_vpd_read(
	__in			efx_nic_t *enp,
	__out_bcount(size)	caddr_t data,
	__in			size_t size);

extern	__checkReturn		int
falcon_vpd_verify(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size);

extern	__checkReturn		int
falcon_vpd_get(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size,
	__inout			efx_vpd_value_t *evvp);

extern	__checkReturn		int
falcon_vpd_set(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size,
	__in			efx_vpd_value_t *evvp);

extern	__checkReturn		int
falcon_vpd_next(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size,
	__out			efx_vpd_value_t *evvp,
	__inout			unsigned int *contp);

extern __checkReturn		int
falcon_vpd_write(
	__in			efx_nic_t *enp,
	__in_bcount(size)	caddr_t data,
	__in			size_t size);

#endif	/* EFSYS_OPT_VPD */

extern	__checkReturn	int
falcon_sram_init(
	__in		efx_nic_t *enp);

#if EFSYS_OPT_DIAG

extern	__checkReturn	int
falcon_sram_test(
	__in		efx_nic_t *enp,
	__in		efx_sram_pattern_fn_t func);

#endif	/* EFSYS_OPT_DIAG */

extern		void
falcon_sram_fini(
	__in	efx_nic_t *enp);

extern	__checkReturn	int
falcon_i2c_check(
	__in		efx_nic_t *enp,
	__in		uint8_t devid);

extern	__checkReturn		int
falcon_i2c_read(
	__in			efx_nic_t *enp,
	__in			uint8_t devid,
	__in			uint8_t addr,
	__out_bcount(size)	caddr_t base,
	__in			size_t size);

extern	__checkReturn		int
falcon_i2c_write(
	__in			efx_nic_t *enp,
	__in			uint8_t devid,
	__in			uint8_t addr,
	__in_bcount(size)	caddr_t base,
	__in			size_t size);

#if EFSYS_OPT_PHY_NULL

extern	__checkReturn		int
falcon_i2c_recv(
	__in			efx_nic_t *enp,
	__in			uint8_t devid,
	__out_bcount(size)	caddr_t base,
	__in			size_t size);

extern	__checkReturn		int
falcon_i2c_send(
	__in			efx_nic_t *enp,
	__in			uint8_t devid,
	__in_bcount(size)	caddr_t base,
	__in			size_t size);

#endif	/* EFSYS_OPT_PHY_NULL */

extern	__checkReturn	int
falcon_mdio_write(
	__in		efx_nic_t *enp,
	__in		uint8_t port,
	__in		uint8_t mmd,
	__in		uint16_t reg,
	__in		efx_word_t *ewp);

extern	__checkReturn	int
falcon_mdio_read(
	__in		efx_nic_t *enp,
	__in		uint8_t port,
	__in		uint8_t mmd,
	__in		uint16_t reg,
	__out		efx_word_t *ewp);

#if EFSYS_OPT_MAC_STATS

extern	__checkReturn			int
falcon_mac_stats_upload(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp);

#endif	/* EFSYS_OPT_MAC_STATS */

extern	__checkReturn	int
falcon_mac_poll(
       __in		efx_nic_t *enp,
       __out		efx_link_mode_t	*link_modep);

extern	__checkReturn	int
falcon_mac_up(
	__in		efx_nic_t *enp,
	__out		boolean_t *mac_upp);

#if EFSYS_OPT_LOOPBACK

extern	__checkReturn	int
falcon_mac_loopback_set(
	__in		efx_nic_t *enp,
	__in		efx_link_mode_t link_mode,
	__in		efx_loopback_type_t loopback_type);

#endif	/* EFSYS_OPT_LOOPBACK */

typedef enum falcon_spi_type_e {
	FALCON_SPI_FLASH = 0,
	FALCON_SPI_EEPROM,
	FALCON_SPI_NTYPES
} falcon_spi_type_t;

extern	__checkReturn		int
falcon_spi_dev_read(
	__in			efx_nic_t *enp,
	__in			falcon_spi_type_t type,
	__in			uint32_t addr,
	__out_bcount(size)	caddr_t base,
	__in			size_t size);

extern	__checkReturn		int
falcon_spi_dev_write(
	__in			efx_nic_t *enp,
	__in			falcon_spi_type_t type,
	__in			uint32_t addr,
	__in_bcount(size)	caddr_t base,
	__in			size_t size);

extern	__checkReturn		int
falcon_spi_dev_erase(
	__in			efx_nic_t *enp,
	__in			falcon_spi_type_t type,
	__in			uint32_t addr,
	__in			size_t size);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FALCON_IMPL_H */
