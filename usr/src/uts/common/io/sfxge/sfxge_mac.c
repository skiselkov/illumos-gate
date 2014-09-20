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

/*
 * All efx_mac_*() must be after efx_port_init()
 * LOCKING STRATEGY: Aquire sm_lock and test sm_state==SFXGE_MAC_STARTED
 * to serialise against sfxge_restart()
 */

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

#include "sfxge.h"
#include "efx.h"

#define	SFXGE_MAC_POLL_PERIOD_MS 1000

static void sfxge_mac_link_update_locked(sfxge_t *sp, efx_link_mode_t mode);


/* MAC DMA attributes */
static ddi_device_acc_attr_t sfxge_mac_devacc = {

	DDI_DEVICE_ATTR_V0,	/* devacc_attr_version */
	DDI_NEVERSWAP_ACC,	/* devacc_attr_endian_flags */
	DDI_STRICTORDER_ACC	/* devacc_attr_dataorder */
};

static ddi_dma_attr_t sfxge_mac_dma_attr = {
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


static void
_sfxge_mac_stat_update(sfxge_mac_t *smp, int tries, int delay_usec)
{
	sfxge_t *sp = smp->sm_sp;
	efsys_mem_t *esmp = &(smp->sm_mem);
	/* LINTED(E_FUNC_SET_NOT_USED) */
	int rc, i;

	ASSERT(mutex_owned(&(smp->sm_lock)));
	ASSERT3U(smp->sm_state, !=, SFXGE_MAC_UNINITIALIZED);

	/* if no stats pending then they are already freshly updated */
	if (smp->sm_mac_stats_timer_reqd && !smp->sm_mac_stats_pend)
		return;

	for (i = 0; i < tries; i++) {
		/* Synchronize the DMA memory for reading */
		(void) ddi_dma_sync(smp->sm_mem.esm_dma_handle,
		    0,
		    EFX_MAC_STATS_SIZE,
		    DDI_DMA_SYNC_FORKERNEL);

		/* Try to update the cached counters */
		if ((rc = efx_mac_stats_update(sp->s_enp, esmp, smp->sm_stat,
		    NULL)) != EAGAIN)
			goto done;

		drv_usecwait(delay_usec);
	}

	DTRACE_PROBE(mac_stat_timeout);
	cmn_err(CE_NOTE, SFXGE_CMN_ERR "[%s%d] MAC stats timeout",
	    ddi_driver_name(sp->s_dip), ddi_get_instance(sp->s_dip));
	return;

done:
	smp->sm_mac_stats_pend = B_FALSE;
	smp->sm_lbolt = ddi_get_lbolt();
}

static void
sfxge_mac_stat_update_quick(sfxge_mac_t *smp)
{
	/*
	 * Update the statistics from the most recent DMA. This might race
	 * with an inflight dma, so retry once. Otherwise get mac stat
	 * values from the last mac_poll() or MC periodic stats.
	 */
	_sfxge_mac_stat_update(smp, 2, 50);
}

static void
sfxge_mac_stat_update_wait(sfxge_mac_t *smp)
{
	/* Wait a max of 20 * 500us = 10ms */
	_sfxge_mac_stat_update(smp, 20, 500);
}

static int
sfxge_mac_kstat_update(kstat_t *ksp, int rw)
{
	sfxge_mac_t *smp = ksp->ks_private;
	kstat_named_t *knp;
	int rc;

	if (rw != KSTAT_READ) {
		rc = EACCES;
		goto fail1;
	}

	ASSERT(mutex_owned(&(smp->sm_lock)));

	if (smp->sm_state != SFXGE_MAC_STARTED)
		goto done;

	sfxge_mac_stat_update_quick(smp);

	knp = smp->sm_stat;
	knp += EFX_MAC_NSTATS;

	knp->value.ui64 = (smp->sm_link_up) ? 1 : 0;
	knp++;

	knp->value.ui64 = smp->sm_link_speed;
	knp++;

	knp->value.ui64 = smp->sm_link_duplex;
	knp++;

done:
	return (0);

fail1:
	DTRACE_PROBE1(fail1, int, rc);

	return (rc);
}

static int
sfxge_mac_kstat_init(sfxge_t *sp)
{
	sfxge_mac_t *smp = &(sp->s_mac);
	dev_info_t *dip = sp->s_dip;
	char name[MAXNAMELEN];
	kstat_t *ksp;
	kstat_named_t *knp;
	unsigned int id;
	int rc;

	/* Create the set */
	(void) snprintf(name, MAXNAMELEN - 1, "%s_mac", ddi_driver_name(dip));

	if ((ksp = kstat_create((char *)ddi_driver_name(dip),
	    ddi_get_instance(dip), name, "mac", KSTAT_TYPE_NAMED,
	    EFX_MAC_NSTATS + 4, 0)) == NULL) {
		rc = ENOMEM;
		goto fail1;
	}

	smp->sm_ksp = ksp;

	ksp->ks_update = sfxge_mac_kstat_update;
	ksp->ks_private = smp;
	ksp->ks_lock = &(smp->sm_lock);

	/* Initialise the named stats */
	smp->sm_stat = knp = ksp->ks_data;
	for (id = 0; id < EFX_MAC_NSTATS; id++) {
		kstat_named_init(knp, (char *)efx_mac_stat_name(sp->s_enp, id),
		    KSTAT_DATA_UINT64);
		knp++;
	}

	kstat_named_init(knp++, "link_up", KSTAT_DATA_UINT64);
	kstat_named_init(knp++, "link_speed", KSTAT_DATA_UINT64);
	kstat_named_init(knp++, "link_duplex", KSTAT_DATA_UINT64);

	kstat_install(ksp);

	return (0);

fail1:
	DTRACE_PROBE1(fail1, int, rc);

	return (rc);
}

static void
sfxge_mac_kstat_fini(sfxge_t *sp)
{
	sfxge_mac_t *smp = &(sp->s_mac);

	/* Destroy the set */
	kstat_delete(smp->sm_ksp);
	smp->sm_ksp = NULL;
	smp->sm_stat = NULL;
}

void
sfxge_mac_stat_get(sfxge_t *sp, unsigned int id, uint64_t *valp)
{
	sfxge_mac_t *smp = &(sp->s_mac);

	/* Make sure the cached counter values are recent */
	mutex_enter(&(smp->sm_lock));

	if (smp->sm_state != SFXGE_MAC_STARTED)
		goto done;

	sfxge_mac_stat_update_quick(smp);

	*valp = smp->sm_stat[id].value.ui64;

done:
	mutex_exit(&(smp->sm_lock));
}

static void
sfxge_mac_poll(void *arg)
{
	sfxge_t *sp = arg;
	efx_nic_t *enp = sp->s_enp;
	sfxge_mac_t *smp = &(sp->s_mac);
	efsys_mem_t *esmp = &(smp->sm_mem);
	efx_link_mode_t mode;
	clock_t timeout;

	mutex_enter(&(smp->sm_lock));
	while (smp->sm_state == SFXGE_MAC_STARTED) {

		/* clears smp->sm_mac_stats_pend if appropriate */
		if (smp->sm_mac_stats_pend)
			sfxge_mac_stat_update_wait(smp);

		/* This may sleep waiting for MCDI completion */
		mode = EFX_LINK_UNKNOWN;
		if (efx_port_poll(enp, &mode) == 0)
			sfxge_mac_link_update_locked(sp, mode);

		if ((smp->sm_link_poll_reqd == B_FALSE) &&
		    (smp->sm_mac_stats_timer_reqd == B_FALSE))
			goto done;

		/* Zero the memory */
		(void) memset(esmp->esm_base, 0, EFX_MAC_STATS_SIZE);

		/* Trigger upload the MAC statistics counters */
		if (smp->sm_link_up &&
		    efx_mac_stats_upload(sp->s_enp, esmp) == 0)
			smp->sm_mac_stats_pend = B_TRUE;

		/* Wait for timeout or end of polling */
		timeout = ddi_get_lbolt() + drv_usectohz(1000 *
		    SFXGE_MAC_POLL_PERIOD_MS);
		while (smp->sm_state == SFXGE_MAC_STARTED) {
			if (cv_timedwait(&(smp->sm_link_poll_kv),
				&(smp->sm_lock), timeout) < 0) {
				/* Timeout - poll if polling still enabled */
				break;
			}
		}
	}
done:
	mutex_exit(&(smp->sm_lock));

}

static void
sfxge_mac_poll_start(sfxge_t *sp)
{
	sfxge_mac_t *smp = &(sp->s_mac);

	ASSERT(mutex_owned(&(smp->sm_lock)));
	ASSERT3U(smp->sm_state, ==, SFXGE_MAC_STARTED);

	/* Schedule a poll */
	(void) ddi_taskq_dispatch(smp->sm_tqp, sfxge_mac_poll, sp, DDI_SLEEP);
}

static void
sfxge_mac_poll_stop(sfxge_t *sp)
{
	sfxge_mac_t *smp = &(sp->s_mac);

	ASSERT(mutex_owned(&(smp->sm_lock)));
	ASSERT3U(smp->sm_state, ==, SFXGE_MAC_INITIALIZED);

	cv_broadcast(&(smp->sm_link_poll_kv));

	/* Wait for link polling to cease */
	mutex_exit(&(smp->sm_lock));
	ddi_taskq_wait(smp->sm_tqp);
	mutex_enter(&(smp->sm_lock));

	/* Wait for any pending DMAed stats to complete */
	sfxge_mac_stat_update_wait(smp);
}

int
sfxge_mac_init(sfxge_t *sp)
{
	sfxge_mac_t *smp = &(sp->s_mac);
	efsys_mem_t *esmp = &(smp->sm_mem);
	dev_info_t *dip = sp->s_dip;
	sfxge_dma_buffer_attr_t dma_attr;
	const efx_nic_cfg_t *encp;
	unsigned char *bytes;
	char buf[8];	/* sufficient for "true" or "false" plus NULL */
	char name[MAXNAMELEN];
	int *ints;
	unsigned int n;
	int err, rc;

	SFXGE_OBJ_CHECK(smp, sfxge_mac_t);

	ASSERT3U(smp->sm_state, ==, SFXGE_MAC_UNINITIALIZED);

	smp->sm_sp = sp;
	encp = efx_nic_cfg_get(sp->s_enp);
	smp->sm_link_poll_reqd = (~encp->enc_features &
				    EFX_FEATURE_LINK_EVENTS);
	smp->sm_mac_stats_timer_reqd = (~encp->enc_features &
				    EFX_FEATURE_PERIODIC_MAC_STATS);

	mutex_init(&(smp->sm_lock), NULL, MUTEX_DRIVER,
	    DDI_INTR_PRI(sp->s_intr.si_intr_pri));
	cv_init(&(smp->sm_link_poll_kv), NULL, CV_DRIVER, NULL);

	/* Create link poll taskq */
	(void) snprintf(name, MAXNAMELEN - 1, "%s_mac_tq",
	    ddi_driver_name(dip));
	smp->sm_tqp = ddi_taskq_create(dip, name, 1, TASKQ_DEFAULTPRI,
	    DDI_SLEEP);
	if (smp->sm_tqp == NULL) {
		rc = ENOMEM;
		goto fail1;
	}

	if ((rc = sfxge_phy_init(sp)) != 0)
		goto fail2;

	dma_attr.sdba_dip	 = dip;
	dma_attr.sdba_dattrp	 = &sfxge_mac_dma_attr;
	dma_attr.sdba_callback	 = DDI_DMA_SLEEP;
	dma_attr.sdba_length	 = EFX_MAC_STATS_SIZE;
	dma_attr.sdba_memflags	 = DDI_DMA_CONSISTENT;
	dma_attr.sdba_devaccp	 = &sfxge_mac_devacc;
	dma_attr.sdba_bindflags	 = DDI_DMA_READ | DDI_DMA_CONSISTENT;
	dma_attr.sdba_maxcookies = 1;
	dma_attr.sdba_zeroinit	 = B_TRUE;

	if ((rc = sfxge_dma_buffer_create(esmp, &dma_attr)) != 0)
		goto fail3;

	/*
	 * Set the initial group hash to allow reception of only broadcast
	 * packets.
	 */
	smp->sm_bucket[0xff] = 1;

	/* Set the initial flow control values */
	smp->sm_fcntl = EFX_FCNTL_RESPOND | EFX_FCNTL_GENERATE;

	/*
	 * Determine the 'burnt-in' MAC address:
	 *
	 * A: if the "mac-address" property is set on our device node use that.
	 * B: otherwise, if the system property "local-mac-address?" is set to
	 *    "false" then we use the system MAC address.
	 * C: otherwise, if the "local-mac-address" property is set on our
	 *    device node use that.
	 * D: otherwise, use the value from NVRAM.
	 */

	/* A */
	err = ddi_prop_lookup_byte_array(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    "mac-address", &bytes, &n);
	switch (err) {
	case DDI_PROP_SUCCESS:
		if (n == ETHERADDRL) {
			bcopy(bytes, smp->sm_bia, ETHERADDRL);
			goto done;
		}

		ddi_prop_free(bytes);
		break;

	default:
		break;
	}

	/* B */
	n = sizeof (buf);
	bzero(buf, n--);
	(void) ddi_getlongprop_buf(DDI_DEV_T_ANY, dip, DDI_PROP_CANSLEEP,
	    "local-mac-address?", buf, (int *)&n);

	if (strcmp(buf, "false") == 0) {
		struct ether_addr addr;

		if (localetheraddr(NULL, &addr) != 0) {
			bcopy((uint8_t *)&addr, smp->sm_bia, ETHERADDRL);
			goto done;
		}
	}

	/*
	 * C
	 *
	 * NOTE: "local-mac_address" maybe coded as an integer or byte array.
	 */
	err = ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    "local-mac-address", &ints, &n);
	switch (err) {
	case DDI_PROP_SUCCESS:
		if (n == ETHERADDRL) {
			while (n-- != 0)
				smp->sm_bia[n] = ints[n] & 0xff;

			goto done;
		}

		ddi_prop_free(ints);
		break;

	default:
		break;
	}

	err = ddi_prop_lookup_byte_array(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    "local-mac-address", &bytes, &n);
	switch (err) {
	case DDI_PROP_SUCCESS:
		if (n == ETHERADDRL) {
			bcopy(bytes, smp->sm_bia, ETHERADDRL);
			goto done;
		}

		ddi_prop_free(bytes);
		break;

	default:
		break;
	}

	/* D */
	bcopy(encp->enc_mac_addr, smp->sm_bia, ETHERADDRL);

done:
	/* Initialize the statistics */
	if ((rc = sfxge_mac_kstat_init(sp)) != 0)
		goto fail4;

	if ((rc = sfxge_phy_kstat_init(sp)) != 0)
		goto fail5;

	smp->sm_state = SFXGE_MAC_INITIALIZED;

	return (0);

fail5:
	DTRACE_PROBE(fail5);

	sfxge_mac_kstat_fini(sp);
fail4:
	DTRACE_PROBE(fail4);

	/* Tear down DMA setup */
	sfxge_dma_buffer_destroy(esmp);
fail3:
	DTRACE_PROBE(fail3);

	sfxge_phy_fini(sp);
fail2:
	DTRACE_PROBE(fail2);

	/* Destroy the link poll taskq */
	ddi_taskq_destroy(smp->sm_tqp);
	smp->sm_tqp = NULL;

fail1:
	DTRACE_PROBE1(fail1, int, rc);

	cv_destroy(&(smp->sm_link_poll_kv));

	mutex_destroy(&(smp->sm_lock));

	smp->sm_sp = NULL;

	SFXGE_OBJ_CHECK(smp, sfxge_mac_t);

	return (rc);
}

int
sfxge_mac_start(sfxge_t *sp, boolean_t restart)
{
	sfxge_mac_t *smp = &(sp->s_mac);
	efsys_mem_t *esmp = &(smp->sm_mem);
	efx_nic_t *enp = sp->s_enp;
	size_t pdu;
	int rc;

	mutex_enter(&(smp->sm_lock));

	ASSERT3U(smp->sm_state, ==, SFXGE_MAC_INITIALIZED);

	if ((rc = efx_port_init(enp)) != 0)
		goto fail1;

	/*
	 * Set up the advertised capabilities that may have been asked for
	 * before the call to efx_port_init().
	 */
	if ((rc = sfxge_phy_cap_apply(sp, !restart)) != 0)
		goto fail2;

	/* Set the SDU */
	pdu = EFX_MAC_PDU(sp->s_mtu);
	if ((rc = efx_mac_pdu_set(enp, pdu)) != 0)
		goto fail3;

	if ((rc = efx_mac_fcntl_set(enp, smp->sm_fcntl, B_TRUE)) != 0)
		goto fail4;

	/* Set the unicast address */
	if ((rc = efx_mac_addr_set(enp, (smp->sm_laa_valid) ?
	    smp->sm_laa : smp->sm_bia)) != 0)
		goto fail5;

	/* Set the unicast filter */
	if ((rc = efx_mac_filter_set(enp,
	    (smp->sm_promisc == SFXGE_PROMISC_ALL_PHYS), B_TRUE)) != 0) {
		goto fail6;
	};

	/* Set the group hash */
	if (smp->sm_promisc >= SFXGE_PROMISC_ALL_MULTI) {
		unsigned int bucket[EFX_MAC_HASH_BITS];
		unsigned int index;

		for (index = 0; index < EFX_MAC_HASH_BITS; index++)
			bucket[index] = 1;

		if ((rc = efx_mac_hash_set(enp, bucket)) != 0)
			goto fail7;
	} else {
		if ((rc = efx_mac_hash_set(enp, smp->sm_bucket)) != 0)
			goto fail8;
	}

	if (!smp->sm_mac_stats_timer_reqd) {
		if ((rc = efx_mac_stats_periodic(enp, esmp,
		    SFXGE_MAC_POLL_PERIOD_MS, B_FALSE)) != 0)
			goto fail9;
	}

	if ((rc = efx_mac_drain(enp, B_FALSE)) != 0)
		goto fail10;

	smp->sm_state = SFXGE_MAC_STARTED;

#ifdef _USE_MAC_PRIV_PROP
	sfxge_gld_priv_prop_rename(sp);
#endif

	/*
	 * Start link state polling. For hardware that reports link change
	 * events we still poll once to update the initial link state.
	 */
	sfxge_mac_poll_start(sp);

	mutex_exit(&(smp->sm_lock));
	return (0);

fail10:
	DTRACE_PROBE(fail10);
	(void) efx_mac_stats_periodic(enp, esmp, 0, B_FALSE);
fail9:
	DTRACE_PROBE(fail9);
fail8:
	DTRACE_PROBE(fail8);
fail7:
	DTRACE_PROBE(fail7);
fail6:
	DTRACE_PROBE(fail6);
fail5:
	DTRACE_PROBE(fail5);
fail4:
	DTRACE_PROBE(fail4);
fail3:
	DTRACE_PROBE(fail3);
fail2:
	DTRACE_PROBE(fail2);
	efx_port_fini(enp);
fail1:
	DTRACE_PROBE1(fail1, int, rc);

	mutex_exit(&(smp->sm_lock));

	return (rc);
}


static void
sfxge_mac_link_update_locked(sfxge_t *sp, efx_link_mode_t mode)
{
	sfxge_mac_t *smp = &(sp->s_mac);
	const char *change, *duplex;
	char info[sizeof (": now 10000Mbps FULL duplex")];

	ASSERT(mutex_owned(&(smp->sm_lock)));
	if (smp->sm_state != SFXGE_MAC_STARTED)
		return;

	if (smp->sm_link_mode == mode)
		return;

	smp->sm_link_mode = mode;
	smp->sm_link_up = B_TRUE;

	switch (smp->sm_link_mode) {
	case EFX_LINK_UNKNOWN:
	case EFX_LINK_DOWN:
		smp->sm_link_speed = 0;
		smp->sm_link_duplex = SFXGE_LINK_DUPLEX_UNKNOWN;
		smp->sm_link_up = B_FALSE;
		break;

	case EFX_LINK_10HDX:
	case EFX_LINK_10FDX:
		smp->sm_link_speed = 10;
		smp->sm_link_duplex = (smp->sm_link_mode == EFX_LINK_10HDX) ?
		    SFXGE_LINK_DUPLEX_HALF : SFXGE_LINK_DUPLEX_FULL;
		break;

	case EFX_LINK_100HDX:
	case EFX_LINK_100FDX:
		smp->sm_link_speed = 100;
		smp->sm_link_duplex = (smp->sm_link_mode == EFX_LINK_100HDX) ?
		    SFXGE_LINK_DUPLEX_HALF : SFXGE_LINK_DUPLEX_FULL;
		break;

	case EFX_LINK_1000HDX:
	case EFX_LINK_1000FDX:
		smp->sm_link_speed = 1000;
		smp->sm_link_duplex = (smp->sm_link_mode == EFX_LINK_1000HDX) ?
		    SFXGE_LINK_DUPLEX_HALF : SFXGE_LINK_DUPLEX_FULL;
		break;

	case EFX_LINK_10000FDX:
		smp->sm_link_speed = 10000;
		smp->sm_link_duplex = SFXGE_LINK_DUPLEX_FULL;
		break;

	default:
		ASSERT(B_FALSE);
		break;
	}

	duplex = (smp->sm_link_duplex == SFXGE_LINK_DUPLEX_FULL) ?
	    "full" : "half";
	change = (smp->sm_link_up) ? "UP" : "DOWN";
	snprintf(info, sizeof (info), ": now %dMbps %s duplex",
	    smp->sm_link_speed, duplex);

	cmn_err(CE_NOTE, SFXGE_CMN_ERR "[%s%d] Link %s%s",
	    ddi_driver_name(sp->s_dip), ddi_get_instance(sp->s_dip),
	    change, smp->sm_link_up ? info : "");

	/* Push link state update to the OS */
	sfxge_gld_link_update(sp);
}

void
sfxge_mac_link_update(sfxge_t *sp, efx_link_mode_t mode)
{
	sfxge_mac_t *smp = &(sp->s_mac);

	mutex_enter(&(smp->sm_lock));
	sfxge_mac_link_update_locked(sp, mode);
	mutex_exit(&(smp->sm_lock));
}

void
sfxge_mac_link_check(sfxge_t *sp, boolean_t *upp)
{
	sfxge_mac_t *smp = &(sp->s_mac);

	mutex_enter(&(smp->sm_lock));
	*upp = smp->sm_link_up;
	mutex_exit(&(smp->sm_lock));
}

void
sfxge_mac_link_speed_get(sfxge_t *sp, unsigned int *speedp)
{
	sfxge_mac_t *smp = &(sp->s_mac);

	mutex_enter(&(smp->sm_lock));
	*speedp = smp->sm_link_speed;
	mutex_exit(&(smp->sm_lock));
}

void
sfxge_mac_link_duplex_get(sfxge_t *sp, sfxge_link_duplex_t *duplexp)
{
	sfxge_mac_t *smp = &(sp->s_mac);

	mutex_enter(&(smp->sm_lock));
	*duplexp = smp->sm_link_duplex;
	mutex_exit(&(smp->sm_lock));
}

void
sfxge_mac_fcntl_get(sfxge_t *sp, unsigned int *fcntlp)
{
	sfxge_mac_t *smp = &(sp->s_mac);

	mutex_enter(&(smp->sm_lock));
	*fcntlp = smp->sm_fcntl;
	mutex_exit(&(smp->sm_lock));
}

int
sfxge_mac_fcntl_set(sfxge_t *sp, unsigned int fcntl)
{
	sfxge_mac_t *smp = &(sp->s_mac);
	int rc;

	mutex_enter(&(smp->sm_lock));

	if (smp->sm_fcntl == fcntl)
		goto done;

	smp->sm_fcntl = fcntl;

	if (smp->sm_state != SFXGE_MAC_STARTED)
		goto done;

	if ((rc = efx_mac_fcntl_set(sp->s_enp, smp->sm_fcntl, B_TRUE)) != 0)
		goto fail1;

done:
	mutex_exit(&(smp->sm_lock));

	return (0);

fail1:
	DTRACE_PROBE1(fail1, int, rc);

	mutex_exit(&(smp->sm_lock));

	return (rc);
}

int
sfxge_mac_unicst_get(sfxge_t *sp, sfxge_unicst_type_t type, uint8_t *addr)
{
	sfxge_mac_t *smp = &(sp->s_mac);
	int rc;

	if (type >= SFXGE_UNICST_NTYPES) {
		rc = EINVAL;
		goto fail1;
	}

	mutex_enter(&(smp->sm_lock));

	if (smp->sm_state != SFXGE_MAC_INITIALIZED &&
	    smp->sm_state != SFXGE_MAC_STARTED) {
		rc = EFAULT;
		goto fail2;
	}

	switch (type) {
	case SFXGE_UNICST_BIA:
		bcopy(smp->sm_bia, addr, ETHERADDRL);
		break;

	case SFXGE_UNICST_LAA:
		if (!(smp->sm_laa_valid)) {
			rc = ENOENT;
			goto fail3;
		}

		bcopy(smp->sm_laa, addr, ETHERADDRL);
		break;

	default:
		ASSERT(B_FALSE);
		break;
	}

	mutex_exit(&(smp->sm_lock));

	return (0);


fail3:
	DTRACE_PROBE(fail3);
fail2:
	DTRACE_PROBE(fail2);

	mutex_exit(&(smp->sm_lock));

fail1:
	DTRACE_PROBE1(fail1, int, rc);

	return (rc);
}

int
sfxge_mac_unicst_set(sfxge_t *sp, uint8_t *addr)
{
	sfxge_mac_t *smp = &(sp->s_mac);
	efx_nic_t *enp = sp->s_enp;
	int rc;

	mutex_enter(&(smp->sm_lock));

	bcopy(addr, smp->sm_laa, ETHERADDRL);
	smp->sm_laa_valid = B_TRUE;

	if (smp->sm_state != SFXGE_MAC_STARTED)
		goto done;

	if ((rc = efx_mac_addr_set(enp, smp->sm_laa)) != 0)
		goto fail1;

done:
	mutex_exit(&(smp->sm_lock));

	return (0);

fail1:
	DTRACE_PROBE1(fail1, int, rc);

	mutex_exit(&(smp->sm_lock));

	return (rc);
}

int
sfxge_mac_promisc_set(sfxge_t *sp, sfxge_promisc_type_t promisc)
{
	sfxge_mac_t *smp = &(sp->s_mac);
	efx_nic_t *enp = sp->s_enp;
	int rc;

	mutex_enter(&(smp->sm_lock));

	if (smp->sm_promisc == promisc)
		goto done;

	smp->sm_promisc = promisc;

	if (smp->sm_state != SFXGE_MAC_STARTED)
		goto done;

	if ((rc = efx_mac_filter_set(enp, (promisc == SFXGE_PROMISC_ALL_PHYS),
	    B_TRUE)) != 0)
		goto fail1;

	if (promisc >= SFXGE_PROMISC_ALL_MULTI) {
		unsigned int bucket[EFX_MAC_HASH_BITS];
		unsigned int index;

		for (index = 0; index < EFX_MAC_HASH_BITS; index++)
			bucket[index] = 1;

		if ((rc = efx_mac_hash_set(enp, bucket)) != 0)
			goto fail2;
	} else {
		if ((rc = efx_mac_hash_set(enp, smp->sm_bucket)) != 0)
			goto fail3;
	}

done:
	mutex_exit(&(smp->sm_lock));
	return (0);

fail3:
	DTRACE_PROBE(fail3);
fail2:
	DTRACE_PROBE(fail2);
fail1:
	DTRACE_PROBE1(fail1, int, rc);
	mutex_exit(&(smp->sm_lock));

	return (rc);
}

int
sfxge_mac_multicst_add(sfxge_t *sp, uint8_t *addr)
{
	sfxge_mac_t *smp = &(sp->s_mac);
	efx_nic_t *enp = sp->s_enp;
	uint32_t crc;
	int rc;

	mutex_enter(&(smp->sm_lock));

	CRC32(crc, addr, ETHERADDRL, 0xffffffff, crc32_table);
	smp->sm_bucket[crc % EFX_MAC_HASH_BITS]++;

	if (smp->sm_state != SFXGE_MAC_STARTED)
		goto done;

	if (smp->sm_promisc >= SFXGE_PROMISC_ALL_MULTI)
		goto done;

	if ((rc = efx_mac_hash_set(enp, smp->sm_bucket)) != 0)
		goto fail1;

done:
	mutex_exit(&(smp->sm_lock));
	return (0);

fail1:
	DTRACE_PROBE1(fail1, int, rc);
	mutex_exit(&(smp->sm_lock));

	return (rc);
}

int
sfxge_mac_multicst_remove(sfxge_t *sp, uint8_t *addr)
{
	sfxge_mac_t *smp = &(sp->s_mac);
	efx_nic_t *enp = sp->s_enp;
	uint32_t crc;
	int rc;

	mutex_enter(&(smp->sm_lock));

	CRC32(crc, addr, ETHERADDRL, 0xffffffff, crc32_table);
	ASSERT(smp->sm_bucket[crc % EFX_MAC_HASH_BITS] != 0);
	smp->sm_bucket[crc % EFX_MAC_HASH_BITS]--;

	if (smp->sm_state != SFXGE_MAC_STARTED)
		goto done;

	if (smp->sm_promisc >= SFXGE_PROMISC_ALL_MULTI)
		goto done;

	if ((rc = efx_mac_hash_set(enp, smp->sm_bucket)) != 0)
		goto fail1;

done:
	mutex_exit(&(smp->sm_lock));
	return (0);

fail1:
	DTRACE_PROBE1(fail1, int, rc);
	mutex_exit(&(smp->sm_lock));

	return (rc);
}

static int
sfxge_mac_loopback_set(sfxge_t *sp, efx_loopback_type_t type)
{
	sfxge_mac_t *smp = &(sp->s_mac);
	efx_nic_t *enp = sp->s_enp;
	int rc;

	mutex_enter(&(smp->sm_lock));
	ASSERT3U(smp->sm_state, ==, SFXGE_MAC_STARTED);

	if ((rc = efx_port_loopback_set(enp, EFX_LINK_UNKNOWN, type)) != 0)
		goto fail1;

	mutex_exit(&(smp->sm_lock));
	return (0);

fail1:
	DTRACE_PROBE1(fail1, int, rc);
	mutex_exit(&(smp->sm_lock));

	return (rc);
}

int
sfxge_mac_ioctl(sfxge_t *sp, sfxge_mac_ioc_t *smip)
{
	int rc;

	switch (smip->smi_op) {
	case SFXGE_MAC_OP_LOOPBACK: {
		efx_loopback_type_t type = smip->smi_data;

		if ((rc = sfxge_mac_loopback_set(sp, type)) != 0)
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

void
sfxge_mac_stop(sfxge_t *sp)
{
	sfxge_mac_t *smp = &(sp->s_mac);
	efx_nic_t *enp = sp->s_enp;
	efsys_mem_t *esmp = &(smp->sm_mem);

	mutex_enter(&(smp->sm_lock));

	ASSERT3U(smp->sm_state, ==, SFXGE_MAC_STARTED);
	ASSERT3P(smp->sm_sp, ==, sp);
	smp->sm_state = SFXGE_MAC_INITIALIZED;

	/* If stopping in response to an MC reboot this may fail */
	if (!smp->sm_mac_stats_timer_reqd)
		(void) efx_mac_stats_periodic(enp, esmp, 0, B_FALSE);

	sfxge_mac_poll_stop(sp);

	smp->sm_lbolt = 0;

	smp->sm_link_up = B_FALSE;
	smp->sm_link_speed = 0;
	smp->sm_link_duplex = SFXGE_LINK_DUPLEX_UNKNOWN;

	/* This may call MCDI */
	(void) efx_mac_drain(enp, B_TRUE);

	smp->sm_link_mode = EFX_LINK_UNKNOWN;


	efx_port_fini(enp);

	mutex_exit(&(smp->sm_lock));
}

void
sfxge_mac_fini(sfxge_t *sp)
{
	sfxge_mac_t *smp = &(sp->s_mac);
	efsys_mem_t *esmp = &(smp->sm_mem);
	unsigned int index;

	ASSERT3U(smp->sm_state, ==, SFXGE_MAC_INITIALIZED);
	ASSERT3P(smp->sm_sp, ==, sp);

	/* Tear down the statistics */
	sfxge_phy_kstat_fini(sp);
	sfxge_mac_kstat_fini(sp);

	smp->sm_state = SFXGE_MAC_UNINITIALIZED;
	smp->sm_link_mode = EFX_LINK_UNKNOWN;
	smp->sm_promisc = SFXGE_PROMISC_OFF;

	/* Clear the group hash */
	for (index = 0; index < EFX_MAC_HASH_BITS; index++)
		smp->sm_bucket[index] = 0;

	bzero(smp->sm_laa, ETHERADDRL);
	smp->sm_laa_valid = B_FALSE;

	bzero(smp->sm_bia, ETHERADDRL);

	smp->sm_fcntl = 0;

	/* Finish with PHY DMA memory */
	sfxge_phy_fini(sp);

	/* Teardown the DMA */
	sfxge_dma_buffer_destroy(esmp);

	/* Destroy the link poll taskq */
	ddi_taskq_destroy(smp->sm_tqp);
	smp->sm_tqp = NULL;

	mutex_destroy(&(smp->sm_lock));

	smp->sm_sp = NULL;

	SFXGE_OBJ_CHECK(smp, sfxge_mac_t);
}
