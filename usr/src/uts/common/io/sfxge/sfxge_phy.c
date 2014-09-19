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

/*
 * All efx_phy_*() must be after efx_port_init()
 *
 * LOCKING STRATEGY: Aquire sm_lock and test sm_state==SFXGE_MAC_STARTED
 * to serialise against sfxge_restart()
 *
 * Note that there is no seperate PHY lock
 * Everything is driven from MAC code and the MAC lock is used
 */

/* PHY DMA attributes */
static ddi_device_acc_attr_t sfxge_phy_devacc = {

	DDI_DEVICE_ATTR_V0,	/* devacc_attr_version */
	DDI_NEVERSWAP_ACC,	/* devacc_attr_endian_flags */
	DDI_STRICTORDER_ACC	/* devacc_attr_dataorder */
};

static ddi_dma_attr_t sfxge_phy_dma_attr = {
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


/* The common code requires the loader *without* the REFLASH_HEADER */
#if EFSYS_OPT_FALCON && EFSYS_OPT_NVRAM_SFT9001
static const uint8_t SFT9001_FULL_LOADER[] = {
#include "firmware/image.h"
#include "firmware/SFT9001A_LOADER.c"
};
const uint8_t * const sft9001_loader = SFT9001_FULL_LOADER +
    sizeof (image_header_t);
const size_t sft9001_loader_size = sizeof (SFT9001_FULL_LOADER) -
    sizeof (image_header_t);
#endif

#if EFSYS_OPT_FALCON && EFSYS_OPT_NVRAM_SFX7101
static const uint8_t SFX7101_FULL_LOADER[] = {
#include "firmware/image.h"
#include "firmware/SFX7101B_LOADER.c"
};
const uint8_t * const sfx7101_loader = SFX7101_FULL_LOADER +
    sizeof (image_header_t);
const size_t sfx7101_loader_size = sizeof (SFX7101_FULL_LOADER) -
    sizeof (image_header_t);
#endif


static int
sfxge_phy_kstat_update(kstat_t *ksp, int rw)
{
	sfxge_t *sp = ksp->ks_private;
	sfxge_mac_t *smp = &(sp->s_mac);
	sfxge_phy_t *spp = &(smp->sm_phy);
	efx_nic_t *enp = sp->s_enp;
	kstat_named_t *knp;
	const efx_nic_cfg_t *encp;
	int rc, sn;

	if (rw != KSTAT_READ) {
		rc = EACCES;
		goto fail1;
	}

	ASSERT(mutex_owned(&(smp->sm_lock)));

	if (smp->sm_state != SFXGE_MAC_STARTED)
		goto done;

	/* Synchronize the DMA memory for reading */
	(void) ddi_dma_sync(spp->sp_mem.esm_dma_handle,
	    0,
	    EFX_PHY_STATS_SIZE,
	    DDI_DMA_SYNC_FORKERNEL);

	if ((rc = efx_phy_stats_update(enp, &spp->sp_mem, spp->sp_statbuf))
	    != 0)
		goto fail2;

	knp = spp->sp_stat;
	for (sn = 0; sn < EFX_PHY_NSTATS; sn++) {
		knp->value.ui64 = spp->sp_statbuf[sn];
		knp++;
	}

	encp = efx_nic_cfg_get(enp);
	knp->value.ui64 = encp->enc_port;

done:
	return (0);

fail2:
	DTRACE_PROBE(fail2);
fail1:
	DTRACE_PROBE1(fail1, int, rc);

	return (rc);
}

int
sfxge_phy_kstat_init(sfxge_t *sp)
{
	dev_info_t *dip = sp->s_dip;
	sfxge_phy_t *spp = &(sp->s_mac.sm_phy);
	efx_nic_t *enp = sp->s_enp;
	kstat_t *ksp;
	kstat_named_t *knp;
	const efx_nic_cfg_t *encp;
	unsigned int id;
	char name[MAXNAMELEN];
	int rc;

	if ((spp->sp_statbuf = kmem_zalloc(sizeof (uint32_t) * EFX_PHY_NSTATS,
	    KM_NOSLEEP)) == NULL) {
		rc = ENOMEM;
		goto fail1;
	}

	encp = efx_nic_cfg_get(enp);

	(void) snprintf(name, MAXNAMELEN - 1, "%s_%s", ddi_driver_name(dip),
	    encp->enc_phy_name);

	/* Create the set */
	if ((ksp = kstat_create((char *)ddi_driver_name(dip),
	    ddi_get_instance(dip), name, "phy", KSTAT_TYPE_NAMED,
	    EFX_PHY_NSTATS + 1, 0)) == NULL) {
		rc = ENOMEM;
		goto fail2;
	}

	spp->sp_ksp = ksp;

	ksp->ks_update = sfxge_phy_kstat_update;
	ksp->ks_private = sp;
	ksp->ks_lock = &(sp->s_mac.sm_lock);

	/* Initialise the named stats */
	spp->sp_stat = knp = ksp->ks_data;
	for (id = 0; id < EFX_PHY_NSTATS; id++) {
		kstat_named_init(knp, (char *)efx_phy_stat_name(enp, id),
		    KSTAT_DATA_UINT64);
		knp++;
	}

	kstat_named_init(knp, "port", KSTAT_DATA_UINT64);
	kstat_install(ksp);

	return (0);

fail2:
	DTRACE_PROBE(fail2)
	kmem_free(spp->sp_statbuf, sizeof (uint32_t) * EFX_PHY_NSTATS);

fail1:
	DTRACE_PROBE1(fail1, int, rc);

	return (rc);
}

void
sfxge_phy_kstat_fini(sfxge_t *sp)
{
	sfxge_phy_t *spp = &(sp->s_mac.sm_phy);

	/* Destroy the set */
	kstat_delete(spp->sp_ksp);
	spp->sp_ksp = NULL;
	spp->sp_stat = NULL;

	kmem_free(spp->sp_statbuf, sizeof (uint32_t) * EFX_PHY_NSTATS);
}


int
sfxge_phy_init(sfxge_t *sp)
{
	sfxge_phy_t *spp = &(sp->s_mac.sm_phy);
	efsys_mem_t *esmp = &(spp->sp_mem);
	sfxge_dma_buffer_attr_t dma_attr;
	int rc;

	dma_attr.sdba_dip	 = sp->s_dip;
	dma_attr.sdba_dattrp	 = &sfxge_phy_dma_attr;
	dma_attr.sdba_callback	 = DDI_DMA_SLEEP;
	dma_attr.sdba_length	 = EFX_PHY_STATS_SIZE;
	dma_attr.sdba_memflags	 = DDI_DMA_CONSISTENT;
	dma_attr.sdba_devaccp	 = &sfxge_phy_devacc;
	dma_attr.sdba_bindflags	 = DDI_DMA_READ | DDI_DMA_CONSISTENT;
	dma_attr.sdba_maxcookies = 1;
	dma_attr.sdba_zeroinit	 = B_TRUE;

	if ((rc = sfxge_dma_buffer_create(esmp, &dma_attr)) != 0)
		goto fail1;

	return (0);

fail1:
	DTRACE_PROBE1(fail1, int, rc);
	SFXGE_OBJ_CHECK(spp, sfxge_phy_t);

	return (rc);
}

static int
sfxge_phy_led_mode_set(sfxge_t *sp, efx_phy_led_mode_t mode)
{
	sfxge_mac_t *smp = &(sp->s_mac);
	int rc;

	ASSERT3U(smp->sm_state, ==, SFXGE_MAC_STARTED);

	if (mode >= EFX_PHY_LED_NMODES) {
		rc = EINVAL;
		goto fail1;
	}

	mutex_enter(&(smp->sm_lock));

	if ((rc = efx_phy_led_set(sp->s_enp, mode)) != 0)
		goto fail2;

	mutex_exit(&(smp->sm_lock));

	return (0);

fail2:
	DTRACE_PROBE(fail2);

	mutex_exit(&(smp->sm_lock));

fail1:
	DTRACE_PROBE1(fail1, int, rc);

	return (rc);
}


int
sfxge_phy_ioctl(sfxge_t *sp, sfxge_phy_ioc_t *spip)
{
	int rc;

	switch (spip->spi_op) {
	case SFXGE_PHY_OP_LINK: {
		break;
	}
	case SFXGE_PHY_OP_LED: {
		efx_phy_led_mode_t mode = spip->spi_data;

		if ((rc = sfxge_phy_led_mode_set(sp, mode)) != 0)
			goto fail1;

		break;
	}
	default:
		rc = ENOTSUP;
		goto fail1;
	}

	return (0);

fail1:
	DTRACE_PROBE1(fail1, int, rc);

	return (rc);
}


static uint8_t
bist_status(unsigned long value)
{
	uint8_t ret;

	efx_phy_cable_status_t status = (efx_phy_cable_status_t)value;
	switch (status) {
	case EFX_PHY_CABLE_STATUS_OK:
		ret = SFXGE_PHY_BIST_CABLE_OK;
		break;
	case EFX_PHY_CABLE_STATUS_INVALID:
		ret =  SFXGE_PHY_BIST_CABLE_INVALID;
		break;
	case EFX_PHY_CABLE_STATUS_OPEN:
		ret = SFXGE_PHY_BIST_CABLE_OPEN;
		break;
	case EFX_PHY_CABLE_STATUS_INTRAPAIRSHORT:
		ret = SFXGE_PHY_BIST_CABLE_INTRAPAIRSHORT;
		break;
	case EFX_PHY_CABLE_STATUS_INTERPAIRSHORT:
		ret = SFXGE_PHY_BIST_CABLE_INTERPAIRSHORT;
		break;
	case EFX_PHY_CABLE_STATUS_BUSY:
		ret = SFXGE_PHY_BIST_CABLE_BUSY;
		break;
	default:
		ret = SFXGE_PHY_BIST_CABLE_UNKNOWN;
	}

	return (ret);
}


int
sfxge_phy_bist_ioctl(sfxge_t *sp, sfxge_phy_bist_ioc_t *spbip)
{
	sfxge_mac_t *smp = &(sp->s_mac);
	efx_phy_bist_type_t bist_type;
	efx_nic_t *enp;
	const efx_nic_cfg_t *encp;
	efx_phy_bist_result_t result;
	unsigned long values[EFX_PHY_BIST_NVALUES];
	uint32_t mask;
	int rc;

	mutex_enter(&(smp->sm_lock));
	enp = sp->s_enp;
	encp = efx_nic_cfg_get(enp);

	bist_type = spbip->spbi_break_link ? EFX_PHY_BIST_TYPE_CABLE_LONG :
	    EFX_PHY_BIST_TYPE_CABLE_SHORT;

	if (~encp->enc_bist_mask & (1 << bist_type)) {
		rc = ENOTSUP;
		goto fail1;
	}

	if ((rc = efx_phy_bist_start(enp, bist_type)) != 0)
		goto fail2;

	do {
		/* Spin for 1 ms */
		drv_usecwait(1000);

		if ((rc = efx_phy_bist_poll(enp, bist_type, &result, &mask,
		    values, sizeof (values) / sizeof (values[0]))) != 0)
			goto fail3;

		ASSERT3U(result, !=, EFX_PHY_BIST_RESULT_UNKNOWN);

	} while (result == EFX_PHY_BIST_RESULT_RUNNING);

	ASSERT3U(mask, ==, ((1 << EFX_PHY_BIST_CABLE_LENGTH_A) |
			    (1 << EFX_PHY_BIST_CABLE_LENGTH_B) |
			    (1 << EFX_PHY_BIST_CABLE_LENGTH_C) |
			    (1 << EFX_PHY_BIST_CABLE_LENGTH_D) |
			    (1 << EFX_PHY_BIST_CABLE_STATUS_A) |
			    (1 << EFX_PHY_BIST_CABLE_STATUS_B) |
			    (1 << EFX_PHY_BIST_CABLE_STATUS_C) |
			    (1 << EFX_PHY_BIST_CABLE_STATUS_D)));

	spbip->spbi_status_a = bist_status(values[EFX_PHY_BIST_CABLE_STATUS_A]);
	spbip->spbi_status_b = bist_status(values[EFX_PHY_BIST_CABLE_STATUS_B]);
	spbip->spbi_status_c = bist_status(values[EFX_PHY_BIST_CABLE_STATUS_C]);
	spbip->spbi_status_d = bist_status(values[EFX_PHY_BIST_CABLE_STATUS_D]);

	spbip->spbi_length_ind_a =
	    (uint16_t)values[EFX_PHY_BIST_CABLE_LENGTH_A];
	spbip->spbi_length_ind_b =
	    (uint16_t)values[EFX_PHY_BIST_CABLE_LENGTH_B];
	spbip->spbi_length_ind_c =
	    (uint16_t)values[EFX_PHY_BIST_CABLE_LENGTH_C];
	spbip->spbi_length_ind_d =
	    (uint16_t)values[EFX_PHY_BIST_CABLE_LENGTH_D];

	/* Bring the PHY back to life */
	efx_phy_bist_stop(enp, bist_type);

	mutex_exit(&(smp->sm_lock));

	return (0);

fail3:
	DTRACE_PROBE(fail3);
	efx_phy_bist_stop(enp, bist_type);
fail2:
	DTRACE_PROBE(fail2);
fail1:
	mutex_exit(&(smp->sm_lock));
	DTRACE_PROBE1(fail1, int, rc);

	return (rc);
}


uint8_t
sfxge_phy_lp_cap_test(sfxge_t *sp, uint32_t field)
{
	sfxge_mac_t *smp = &(sp->s_mac);
	uint32_t cap = 0;

	mutex_enter(&(smp->sm_lock));

	if (smp->sm_state != SFXGE_MAC_STARTED)
		goto done;

	efx_phy_lp_cap_get(sp->s_enp, &cap);

done:
	mutex_exit(&(smp->sm_lock));

	return (cap & (1 << field));
}

/*
 * Set up the advertised capabilities that may have been asked for
 * when the mac was not in the state SFXGE_MAC_STARTED.
 * Must be called after efx_port_init().
 */
int
sfxge_phy_cap_apply(sfxge_t *sp, boolean_t use_default)
{
	sfxge_mac_t *smp = &(sp->s_mac);
	efx_nic_t *enp;
	uint32_t adv_cap;
	int rc;
	int err;

	ASSERT(mutex_owned(&(smp->sm_lock)));

	enp = sp->s_enp;

	if (use_default)
		efx_phy_adv_cap_get(enp, EFX_PHY_CAP_DEFAULT, &adv_cap);
	else
		efx_phy_adv_cap_get(enp, EFX_PHY_CAP_CURRENT, &adv_cap);

	adv_cap |= smp->sm_phy_cap_to_set;
	smp->sm_phy_cap_to_set = 0;
	adv_cap &= ~(smp->sm_phy_cap_to_unset);
	smp->sm_phy_cap_to_unset = 0;
	if ((err = efx_phy_adv_cap_set(enp, adv_cap)) != 0) {
		if (err == EINVAL) {
			/*
			 * The configuation wasn't accepted, so set to
			 * defaults.
			 */
			uint32_t requested = adv_cap;
			uint32_t supported;
			efx_phy_adv_cap_get(enp, EFX_PHY_CAP_PERM, &supported);
			efx_phy_adv_cap_get(enp, EFX_PHY_CAP_DEFAULT, &adv_cap);
			if ((rc = efx_phy_adv_cap_set(enp, adv_cap)) != 0)
				goto fail1;
			cmn_err(CE_WARN, SFXGE_CMN_ERR
			    "[%s%d] Setting of advertised link "
			    "capabilities failed. "
			    "Using default settings. "
			    "(Requested 0x%x Given 0x%x Supported 0x%x)",
			    ddi_driver_name(sp->s_dip),
			    ddi_get_instance(sp->s_dip),
			    requested,
			    adv_cap,
			    supported);
		} else {
			rc = err;
			goto fail2;
		}
	}

	return (0);

fail2:
	DTRACE_PROBE(fail2);

fail1:
	DTRACE_PROBE1(fail1, int, rc);

	return (rc);
}

uint8_t
sfxge_phy_cap_test(sfxge_t *sp, uint32_t flag, uint32_t field,
	boolean_t *mutablep)
{
	sfxge_mac_t *smp = &(sp->s_mac);
	efx_nic_t *enp;
	uint32_t cap = 0;
	uint32_t perm = 0;

	mutex_enter(&(smp->sm_lock));
	enp = sp->s_enp;

	if (smp->sm_state != SFXGE_MAC_STARTED)
		goto done;

	efx_phy_adv_cap_get(enp, flag, &cap);
	efx_phy_adv_cap_get(enp, EFX_PHY_CAP_PERM, &perm);

done:
	mutex_exit(&(smp->sm_lock));

	if (mutablep)
		*mutablep = (perm & (1 << field)) ? B_TRUE : B_FALSE;

	return ((cap & (1 << field)) ? 1 : 0);
}


int
sfxge_phy_cap_set(sfxge_t *sp, uint32_t field, int set)
{
	sfxge_mac_t *smp = &(sp->s_mac);
	efx_nic_t *enp = sp->s_enp;
	uint32_t cap;
	int rc = 0;

	mutex_enter(&(smp->sm_lock));

	if (smp->sm_state != SFXGE_MAC_STARTED) {
		/* Store the request for when the mac is started */
		if (set)
			smp->sm_phy_cap_to_set |= (1 << field);
		else
			smp->sm_phy_cap_to_unset |= (1 << field);
		goto done;
	}

	efx_phy_adv_cap_get(enp, EFX_PHY_CAP_CURRENT, &cap);

	if (set)
		cap |= (1 << field);
	else
		cap &= ~(1 << field);

	rc = efx_phy_adv_cap_set(enp, cap);
done:
	mutex_exit(&(smp->sm_lock));

	return (rc);
}


int
sfxge_phy_prop_get(sfxge_t *sp, unsigned int id, uint32_t flags, uint32_t *valp)
{
	sfxge_mac_t *smp = &(sp->s_mac);
	efx_nic_t *enp = sp->s_enp;
	int rc = 0;

	mutex_enter(&(smp->sm_lock));

	if (smp->sm_state != SFXGE_MAC_STARTED)
		goto done;

	rc = efx_phy_prop_get(enp, id, flags, valp);

done:
	mutex_exit(&(smp->sm_lock));

	return (rc);
}


int
sfxge_phy_prop_set(sfxge_t *sp, unsigned int id, uint32_t val)
{
	sfxge_mac_t *smp = &(sp->s_mac);
	efx_nic_t *enp = sp->s_enp;
	int rc = 0;

	mutex_enter(&(smp->sm_lock));

	if (smp->sm_state != SFXGE_MAC_STARTED)
		goto done;

	rc = efx_phy_prop_set(enp, id, val);

done:
	mutex_exit(&(smp->sm_lock));

	return (rc);
}


void
sfxge_phy_fini(sfxge_t *sp)
{
	sfxge_phy_t *spp = &(sp->s_mac.sm_phy);
	efsys_mem_t *esmp = &(spp->sp_mem);

	sfxge_dma_buffer_destroy(esmp);
}
