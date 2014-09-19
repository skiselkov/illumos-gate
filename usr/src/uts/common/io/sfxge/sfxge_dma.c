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

#include <sys/ddi.h>

#include "sfxge.h"
#include "efx.h"

static int
sfxge_dma_buffer_unbind_handle(efsys_mem_t *esmp)
{
	int rc;

	esmp->esm_addr = 0;
	rc = ddi_dma_unbind_handle(esmp->esm_dma_handle);
	if (rc != DDI_SUCCESS)
		goto fail1;

	return (0);

fail1:
	DTRACE_PROBE1(fail1, int, rc);

	return (rc);
}

static void
sfxge_dma_buffer_mem_free(efsys_mem_t *esmp)
{
	esmp->esm_base = NULL;
	ddi_dma_mem_free(&(esmp->esm_acc_handle));
	esmp->esm_acc_handle = NULL;
}

static void
sfxge_dma_buffer_handle_free(ddi_dma_handle_t *dhandlep)
{
	ddi_dma_free_handle(dhandlep);
	*dhandlep = NULL;
}

int
sfxge_dma_buffer_create(efsys_mem_t *esmp, const sfxge_dma_buffer_attr_t *sdbap)
{
	int err;
	int rc;
	size_t unit;
	ddi_dma_cookie_t dmac;
	unsigned int ncookies;

	/* Allocate a DMA handle */
	err = ddi_dma_alloc_handle(sdbap->sdba_dip, sdbap->sdba_dattrp,
	    sdbap->sdba_callback, NULL, &(esmp->esm_dma_handle));
	switch (err) {
	case DDI_SUCCESS:
		break;

	case DDI_DMA_BADATTR:
		rc = EINVAL;
		goto fail1;

	case DDI_DMA_NORESOURCES:
		rc = ENOMEM;
		goto fail1;

	default:
		rc = EFAULT;
		goto fail1;
	}

	/* Allocate some DMA memory */
	err = ddi_dma_mem_alloc(esmp->esm_dma_handle, sdbap->sdba_length,
	    sdbap->sdba_devaccp, sdbap->sdba_memflags,
	    sdbap->sdba_callback, NULL,
	    &(esmp->esm_base), &unit, &(esmp->esm_acc_handle));
	switch (err) {
	case DDI_SUCCESS:
		break;

	case DDI_FAILURE:
		/*FALLTHRU*/
	default:
		rc = EFAULT;
		goto fail2;
	}

	if (sdbap->sdba_zeroinit)
		bzero(esmp->esm_base, sdbap->sdba_length);

	/* Bind the DMA memory to the DMA handle */
	/* We aren't handling partial mappings */
	ASSERT3U(sdbap->sdba_bindflags & DDI_DMA_PARTIAL, !=, DDI_DMA_PARTIAL);
	err = ddi_dma_addr_bind_handle(esmp->esm_dma_handle, NULL,
	    esmp->esm_base, sdbap->sdba_length, sdbap->sdba_bindflags,
	    sdbap->sdba_callback, NULL, &dmac, &ncookies);
	switch (err) {
	case DDI_DMA_MAPPED:
		break;

	case DDI_DMA_INUSE:
		rc = EEXIST;
		goto fail3;

	case DDI_DMA_NORESOURCES:
		rc = ENOMEM;
		goto fail3;

	case DDI_DMA_NOMAPPING:
		rc = ENOTSUP;
		goto fail3;

	case DDI_DMA_TOOBIG:
		rc = EFBIG;
		goto fail3;

	default:
		rc = EFAULT;
		goto fail3;
	}
	ASSERT3U(ncookies, >=, 1);
	ASSERT3U(ncookies, <=, sdbap->sdba_maxcookies);

	esmp->esm_addr = dmac.dmac_laddress;
	DTRACE_PROBE1(addr, efsys_dma_addr_t, esmp->esm_addr);

	return (0);

fail3:
	DTRACE_PROBE(fail3);

	sfxge_dma_buffer_mem_free(esmp);

fail2:
	DTRACE_PROBE(fail2);

	sfxge_dma_buffer_handle_free(&(esmp->esm_dma_handle));
	esmp->esm_dma_handle = NULL;

fail1:
	DTRACE_PROBE1(fail1, int, rc);

	return (-1);
}

void
sfxge_dma_buffer_destroy(efsys_mem_t *esmp)
{
	int rc;

	rc = sfxge_dma_buffer_unbind_handle(esmp);
	if (rc != 0) {
		cmn_err(CE_WARN, SFXGE_CMN_ERR "ERROR: DMA Unbind failed rc=%d",
		    rc);
	}
	sfxge_dma_buffer_mem_free(esmp);
	sfxge_dma_buffer_handle_free(&(esmp->esm_dma_handle));
}
