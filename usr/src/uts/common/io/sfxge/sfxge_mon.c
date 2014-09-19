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
#include <sys/sysmacros.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/cyclic.h>

#include "sfxge.h"

#include "efx.h"

/* Monitor DMA attributes */
static ddi_device_acc_attr_t sfxge_mon_devacc = {

	DDI_DEVICE_ATTR_V0,	/* devacc_attr_version */
	DDI_NEVERSWAP_ACC,	/* devacc_attr_endian_flags */
	DDI_STRICTORDER_ACC	/* devacc_attr_dataorder */
};

static ddi_dma_attr_t sfxge_mon_dma_attr = {
	DMA_ATTR_V0,		/* dma_attr_version	*/
	0,			/* dma_attr_addr_lo	*/
	0xffffffffffffffffull,	/* dma_attr_addr_hi	*/
	0xffffffffffffffffull,	/* dma_attr_count_max	*/
	0x1000,			/* dma_attr_align	*/
	0xffffffff,		/* dma_attr_burstsizes	*/
	1,			/* dma_attr_minxfer	*/
	0xffffffffffffffffull,	/* dma_attr_maxxfer	*/
	0xffffffffffffffffull,	/* dma_attr_seg		*/
	1,			/* dma_attr_sgllen	*/
	1,			/* dma_attr_granular	*/
	0			/* dma_attr_flags	*/
};


static int
sfxge_mon_kstat_update(kstat_t *ksp, int rw)
{
	sfxge_t *sp = ksp->ks_private;
	sfxge_mon_t *smp = &(sp->s_mon);
	efsys_mem_t *esmp = &(smp->sm_mem);
	efx_nic_t *enp = sp->s_enp;
	kstat_named_t *knp;
	int rc, sn;

	if (rw != KSTAT_READ) {
		rc = EACCES;
		goto fail1;
	}

	ASSERT(mutex_owned(&(smp->sm_lock)));

	if (smp->sm_state != SFXGE_MON_STARTED)
		goto done;

	/* Synchronize the DMA memory for reading */
	(void) ddi_dma_sync(smp->sm_mem.esm_dma_handle,
	    0,
	    EFX_MON_STATS_SIZE,
	    DDI_DMA_SYNC_FORKERNEL);

	if ((rc = efx_mon_stats_update(enp, esmp, smp->sm_statbuf)) != 0)
		goto fail2;

	knp = smp->sm_stat;
	for (sn = 0; sn < EFX_MON_NSTATS; sn++) {
		knp->value.ui64 = smp->sm_statbuf[sn].emsv_value;
		knp++;
	}

	knp->value.ui32 = sp->s_num_restarts;
	knp++;
	knp->value.ui32 = sp->s_num_restarts_hw_err;
	knp++;

done:
	return (0);

fail2:
	DTRACE_PROBE(fail2);
fail1:
	DTRACE_PROBE1(fail1, int, rc);

	return (rc);
}

static int
sfxge_mon_kstat_init(sfxge_t *sp)
{
	sfxge_mon_t *smp = &(sp->s_mon);
	dev_info_t *dip = sp->s_dip;
	efx_nic_t *enp = sp->s_enp;
	kstat_t *ksp;
	kstat_named_t *knp;
	char name[MAXNAMELEN];
	unsigned int id;
	int rc;

	if ((smp->sm_statbuf = kmem_zalloc(sizeof (uint32_t) * EFX_MON_NSTATS,
	    KM_NOSLEEP)) == NULL) {
		rc = ENOMEM;
		goto fail1;
	}

	(void) snprintf(name, MAXNAMELEN - 1, "%s_%s", ddi_driver_name(dip),
	    efx_mon_name(enp));

	/* Create the set */
	if ((ksp = kstat_create((char *)ddi_driver_name(dip),
	    ddi_get_instance(dip), name, "mon", KSTAT_TYPE_NAMED,
	    EFX_MON_NSTATS+2, 0)) == NULL) {
		rc = ENOMEM;
		goto fail2;
	}

	smp->sm_ksp = ksp;

	ksp->ks_update = sfxge_mon_kstat_update;
	ksp->ks_private = sp;
	ksp->ks_lock = &(smp->sm_lock);

	/* Initialise the named stats */
	smp->sm_stat = knp = ksp->ks_data;
	for (id = 0; id < EFX_MON_NSTATS; id++) {
		kstat_named_init(knp, (char *)efx_mon_stat_name(enp, id),
		    KSTAT_DATA_UINT64);
		knp++;
	}
	kstat_named_init(knp, "num_restarts", KSTAT_DATA_UINT32);
	knp++;
	kstat_named_init(knp, "num_restarts_hw_err", KSTAT_DATA_UINT32);
	knp++;

	kstat_install(ksp);

	return (0);

fail2:
	DTRACE_PROBE(fail2);
	kmem_free(smp->sm_statbuf, sizeof (uint32_t) * EFX_MON_NSTATS);
fail1:
	DTRACE_PROBE1(fail1, int, rc);

	return (rc);
}

static void
sfxge_mon_kstat_fini(sfxge_t *sp)
{
	sfxge_mon_t *smp = &(sp->s_mon);

	/* Destroy the set */
	kstat_delete(smp->sm_ksp);
	smp->sm_ksp = NULL;
	smp->sm_stat = NULL;

	kmem_free(smp->sm_statbuf, sizeof (uint32_t) * EFX_MON_NSTATS);
}

int
sfxge_mon_init(sfxge_t *sp)
{
	sfxge_mon_t *smp = &(sp->s_mon);
	efx_nic_t *enp = sp->s_enp;
	efsys_mem_t *esmp = &(smp->sm_mem);
	sfxge_dma_buffer_attr_t dma_attr;
	const efx_nic_cfg_t *encp;
	int rc;

	SFXGE_OBJ_CHECK(smp, sfxge_mon_t);

	ASSERT3U(smp->sm_state, ==, SFXGE_MON_UNINITIALIZED);

	smp->sm_sp = sp;

	mutex_init(&(smp->sm_lock), NULL, MUTEX_DRIVER, NULL);

	dma_attr.sdba_dip	 = sp->s_dip;
	dma_attr.sdba_dattrp	 = &sfxge_mon_dma_attr;
	dma_attr.sdba_callback	 = DDI_DMA_SLEEP;
	dma_attr.sdba_length	 = EFX_MON_STATS_SIZE;
	dma_attr.sdba_memflags	 = DDI_DMA_CONSISTENT;
	dma_attr.sdba_devaccp	 = &sfxge_mon_devacc;
	dma_attr.sdba_bindflags	 = DDI_DMA_READ | DDI_DMA_CONSISTENT;
	dma_attr.sdba_maxcookies = 1;
	dma_attr.sdba_zeroinit	 = B_TRUE;

	if ((rc = sfxge_dma_buffer_create(esmp, &dma_attr)) != 0)
		goto fail1;

	encp = efx_nic_cfg_get(enp);
	smp->sm_type = encp->enc_mon_type;

	DTRACE_PROBE1(mon, efx_mon_type_t, smp->sm_type);

	smp->sm_state = SFXGE_MON_INITIALIZED;

	/* Initialize the statistics */
	if ((rc = sfxge_mon_kstat_init(sp)) != 0)
		goto fail2;

	return (0);

fail2:
	DTRACE_PROBE(fail2);

	/* Tear down DMA setup */
	sfxge_dma_buffer_destroy(esmp);

fail1:
	DTRACE_PROBE1(fail1, int, rc);
	mutex_destroy(&(smp->sm_lock));

	smp->sm_sp = NULL;

	SFXGE_OBJ_CHECK(smp, sfxge_mac_t);

	return (rc);
}

int
sfxge_mon_start(sfxge_t *sp)
{
	sfxge_mon_t *smp = &(sp->s_mon);
	int rc;

	mutex_enter(&(smp->sm_lock));
	ASSERT3U(smp->sm_state, ==, SFXGE_MON_INITIALIZED);

	/* Initialize the MON module */
	if ((rc = efx_mon_init(sp->s_enp)) != 0)
		goto fail1;

	smp->sm_state = SFXGE_MON_STARTED;

	mutex_exit(&(smp->sm_lock));

	return (0);

fail1:
	DTRACE_PROBE1(fail1, int, rc);

	mutex_exit(&(smp->sm_lock));

	return (rc);
}

void
sfxge_mon_stop(sfxge_t *sp)
{
	sfxge_mon_t *smp = &(sp->s_mon);

	mutex_enter(&(smp->sm_lock));

	ASSERT3U(smp->sm_state, ==, SFXGE_MON_STARTED);
	smp->sm_state = SFXGE_MON_INITIALIZED;

	/* Tear down the MON module */
	efx_mon_fini(sp->s_enp);

	mutex_exit(&(smp->sm_lock));
}

void
sfxge_mon_fini(sfxge_t *sp)
{
	sfxge_mon_t *smp = &(sp->s_mon);
	efsys_mem_t *esmp = &(smp->sm_mem);

	ASSERT3U(smp->sm_state, ==, SFXGE_MON_INITIALIZED);

	/* Tear down the statistics */
	sfxge_mon_kstat_fini(sp);

	smp->sm_state = SFXGE_MON_UNINITIALIZED;
	mutex_destroy(&(smp->sm_lock));

	smp->sm_sp = NULL;
	smp->sm_type = EFX_MON_INVALID;

	/* Tear down DMA setup */
	sfxge_dma_buffer_destroy(esmp);

	SFXGE_OBJ_CHECK(smp, sfxge_mon_t);
}
