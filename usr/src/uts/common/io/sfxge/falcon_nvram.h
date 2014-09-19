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

#ifndef	_SYS_FALCON_NVRAM_H
#define	_SYS_FALCON_NVRAM_H

#ifdef	__cplusplus
extern "C" {
#endif

/* PCI subsystem vendor ID */
#define	PC_SS_VEND_ID_REG_SF_OFST 0x12c

/* PCI subsystem device ID */
#define	PC_SS_ID_REG_SF_OFST 0x12e

/* PCIe serial number */
#define	PCI_SN_SF_OFST 0x1c4

/* NVRAM and VPD configuration */
#define	EE_VPD_CFG0_REG_SF_OFST 0x300

/* MAC address */
#define	MAC_ADDRESS_SF_OFST 0x310

/* NIC stat */
#define NIC_STAT_SF_OFST 0x360

/* Sram config */
#define SRAM_CFG_SF_OFST 0x380

/* Magic number */
#define	CFG_MAGIC_REG_SF_OFST 0x3a0

#define	MAGIC_LBN 0
#define	MAGIC_WIDTH 16
#define	MAGIC_DECODE 0xfa1c

/* Version */
#define	CFG_VERSION_REG_SF_OFST 0x3a2

#define	VERSION_LBN 0
#define	VERSION_WIDTH 16

/* Checksum */
#define	CFG_CKSUM_REG_SF_OFST 0x3a4

#define	CKSUM_LBN 0
#define	CKSUM_WIDTH 16

/* PHY address */
#define	CFG_PHY_PORT_REG_SF_OFST 0x3a8

#define	PHY_PORT_LBN 0
#define	PHY_PORT_WIDTH 8
#define	PHY_PORT_INVALID_DECODE 0xff

/* PHY type */
#define	CFG_PHY_TYPE_REG_SF_OFST 0x3a9

#define	PHY_TYPE_LBN 0
#define	PHY_TYPE_WIDTH 8
#define	PHY_TYPE_NONE_DECODE 0x00
#define	PHY_TYPE_TXC43128_DECODE 0x01
#define	PHY_TYPE_88E1111_DECODE 0x02
#define	PHY_TYPE_SFX7101_DECODE 0x03
#define	PHY_TYPE_QT2022C2_DECODE 0x04
#define	PHY_TYPE_SFT9001A_DECODE 0x08
#define	PHY_TYPE_QT2025C_DECODE 0x09
#define	PHY_TYPE_SFT9001B_DECODE 0x0a

/* ASIC revision */
#define	CFG_ASIC_REV_REG_SF_OFST 0x3ac

#define	ASIC_REV_MINOR_LBN 0
#define	ASIC_REV_MINOR_WIDTH 8
#define	ASIC_REV_MAJOR_LBN 8
#define	ASIC_REV_MAJOR_WIDTH 16

/* Board revision */
#define	CFG_BOARD_REV_REG_SF_OFST 0x3ae

#define	BOARD_REV_MINOR_LBN 0
#define	BOARD_REV_MINOR_WIDTH 4
#define	BOARD_REV_MAJOR_LBN 4
#define	BOARD_REV_MAJOR_WIDTH 4

/* Board type */
#define	CFG_BOARD_TYPE_REG_SF_OFST 0x3af

#define	BOARD_TYPE_LBN 0
#define	BOARD_TYPE_WIDTH 8
#define	BOARD_TYPE_SFE4001_DECODE 0x01
#define	BOARD_TYPE_SFE4002_DECODE 0x02
#define	BOARD_TYPE_SFE4003_DECODE 0x03
#define	BOARD_TYPE_SFE4005_DECODE 0x04
#define	BOARD_TYPE_SFN4111T_DECODE 0x51
#define	BOARD_TYPE_SFN4112F_DECODE 0x52

/* EEPROM information */
#define	CFG_EEPROM_DEV_REG_SF_OFST 0x3c0

/* FLASH information */
#define	CFG_FLASH_DEV_REG_SF_OFST 0x3c4

#define	SPI_DEV_SIZE_LBN 0
#define	SPI_DEV_SIZE_WIDTH 5
#define	SPI_DEV_ADBCNT_LBN 6
#define	SPI_DEV_ADBCNT_WIDTH 2
#define	SPI_DEV_ERASE_CMD_LBN 8
#define	SPI_DEV_ERASE_CMD_WIDTH 8
#define	SPI_DEV_ERASE_SIZE_LBN 16
#define	SPI_DEV_ERASE_SIZE_WIDTH 5
#define	SPI_DEV_WRITE_SIZE_LBN 24
#define	SPI_DEV_WRITE_SIZE_WIDTH 5

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_FALCON_NVRAM_H */
