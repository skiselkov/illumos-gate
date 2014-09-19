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
#include <sys/pci.h>
#include <sys/pcie.h>

/* PCIe 2.0 link speeds */
#ifndef PCIE_LINKCAP_MAX_SPEED_5_0
#define	PCIE_LINKCAP_MAX_SPEED_5_0	0x2
#endif
#ifndef PCIE_LINKSTS_SPEED_5_0
#define	PCIE_LINKSTS_SPEED_5_0		0x2
#endif

#include "sfxge.h"

int
sfxge_pci_cap_find(sfxge_t *sp, uint8_t cap_id, off_t *offp)
{
	off_t off;
	uint16_t stat;
	int rc;

	stat = pci_config_get16(sp->s_pci_handle, PCI_CONF_STAT);

	if (!(stat & PCI_STAT_CAP)) {
		rc = ENOTSUP;
		goto fail1;
	}

	for (off = pci_config_get8(sp->s_pci_handle, PCI_CONF_CAP_PTR);
	    off != PCI_CAP_NEXT_PTR_NULL;
	    off = pci_config_get8(sp->s_pci_handle, off + PCI_CAP_NEXT_PTR)) {
		if (cap_id == pci_config_get8(sp->s_pci_handle,
		    off + PCI_CAP_ID))
			goto done;
	}

	rc = ENOENT;
	goto fail2;

done:
	*offp = off;
	return (0);

fail2:
	DTRACE_PROBE(fail2);
fail1:
	DTRACE_PROBE1(fail1, int, rc);

	return (rc);
}

int
sfxge_pci_init(sfxge_t *sp)
{
	off_t off;
	uint16_t pciecap;
	uint16_t devctl;
	uint16_t linksts;
	uint16_t max_payload_size;
	uint16_t max_read_request;
	int rc;

	if (pci_config_setup(sp->s_dip, &(sp->s_pci_handle)) != DDI_SUCCESS) {
		rc = ENODEV;
		goto fail1;
	}

	sp->s_pci_venid = pci_config_get16(sp->s_pci_handle, PCI_CONF_VENID);
	sp->s_pci_devid = pci_config_get16(sp->s_pci_handle, PCI_CONF_DEVID);
	if ((rc = efx_family(sp->s_pci_venid, sp->s_pci_devid,
	    &sp->s_family)) != 0)
		goto fail2;

	if ((rc = sfxge_pci_cap_find(sp, PCI_CAP_ID_PCI_E, &off)) != 0)
		goto fail3;

	pciecap = pci_config_get16(sp->s_pci_handle, off + PCIE_PCIECAP);
	ASSERT3U((pciecap & PCIE_PCIECAP_VER_MASK), >=, PCIE_PCIECAP_VER_1_0);

	linksts = pci_config_get16(sp->s_pci_handle, off + PCIE_LINKSTS);
	switch (linksts & PCIE_LINKSTS_NEG_WIDTH_MASK) {
	case PCIE_LINKSTS_NEG_WIDTH_X1:
		sp->s_pcie_nlanes = 1;
		break;

	case PCIE_LINKSTS_NEG_WIDTH_X2:
		sp->s_pcie_nlanes = 2;
		break;

	case PCIE_LINKSTS_NEG_WIDTH_X4:
		sp->s_pcie_nlanes = 4;
		break;

	case PCIE_LINKSTS_NEG_WIDTH_X8:
		sp->s_pcie_nlanes = 8;
		break;

	default:
		ASSERT(B_FALSE);
		break;
	}

	switch (linksts & PCIE_LINKSTS_SPEED_MASK) {
	case PCIE_LINKSTS_SPEED_2_5:
		sp->s_pcie_linkspeed = 1;
		break;

	case PCIE_LINKSTS_SPEED_5_0:
		sp->s_pcie_linkspeed = 2;
		break;

	default:
		ASSERT(B_FALSE);
		break;
	}

	devctl = pci_config_get16(sp->s_pci_handle, off + PCIE_DEVCTL);

	max_payload_size = (devctl & PCIE_DEVCTL_MAX_PAYLOAD_MASK)
	    >> PCIE_DEVCTL_MAX_PAYLOAD_SHIFT;

	max_read_request = (devctl & PCIE_DEVCTL_MAX_READ_REQ_MASK)
	    >> PCIE_DEVCTL_MAX_READ_REQ_SHIFT;

	cmn_err(CE_NOTE,
	    SFXGE_CMN_ERR "PCIe MRR: %d TLP: %d Link: %s Lanes: x%d",
	    128 << max_read_request,
	    128 << max_payload_size,
	    (sp->s_pcie_linkspeed == 1) ? "2.5G" :
	    (sp->s_pcie_linkspeed == 2) ? "5.0G" :
	    "UNKNOWN",
	    sp->s_pcie_nlanes);

	return (0);

fail3:
	DTRACE_PROBE(fail3);
fail2:
	DTRACE_PROBE(fail2);

	pci_config_teardown(&(sp->s_pci_handle));
	sp->s_pci_handle = NULL;

fail1:
	DTRACE_PROBE1(fail1, int, rc);

	return (rc);
}

void
sfxge_pcie_check_link(sfxge_t *sp, unsigned int full_nlanes,
	unsigned int full_speed)
{
	if ((sp->s_pcie_linkspeed < full_speed) ||
	    (sp->s_pcie_nlanes    < full_nlanes))
		cmn_err(CE_NOTE,
		    SFXGE_CMN_ERR "The %s%d device requires %d PCIe lanes "
		    "at %s link speed to reach full bandwidth.",
		    ddi_driver_name(sp->s_dip),
		    ddi_get_instance(sp->s_dip),
		    full_nlanes,
		    (full_speed == 1) ? "2.5G" :
		    (full_speed == 2) ? "5.0G" :
		    "UNKNOWN");
}

int
sfxge_pci_ioctl(sfxge_t *sp, sfxge_pci_ioc_t *spip)
{
	int rc;

	switch (spip->spi_op) {
	case SFXGE_PCI_OP_READ:
		spip->spi_data = pci_config_get8(sp->s_pci_handle,
		    spip->spi_addr);
		break;

	case SFXGE_PCI_OP_WRITE:
		pci_config_put8(sp->s_pci_handle,
		    spip->spi_addr, spip->spi_data);
		break;

	default:
		rc = ENOTSUP;
		goto fail1;
	}

	return (0);

fail1:
	DTRACE_PROBE1(fail1, int, rc);

	return (0);
}

void
sfxge_pci_fini(sfxge_t *sp)
{
	sp->s_pcie_nlanes = 0;
	sp->s_pcie_linkspeed = 0;

	pci_config_teardown(&(sp->s_pci_handle));
	sp->s_pci_handle = NULL;
}
