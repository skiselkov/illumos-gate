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
 * Copyright 2009 Solarflare Communications Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#include <sys/types.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/stream.h>
#include <sys/dlpi.h>

#include "sfxge.h"
#include "efsys.h"
#include "efx.h"
#include "efx_mcdi.h"
#include "efx_regs_mcdi.h"


/*
 * Notes on MCDI operation:
 * ------------------------
 * MCDI requests can be made in arbitrary thread context, and as a synchronous
 * API must therefore block until the response is available from the MC, or
 * a watchdog timeout occurs.
 *
 * This interacts badly with the limited number of worker threads (2 per CPU)
 * used by the Solaris callout subsystem to invoke timeout handlers. If both
 * worker threads are blocked (e.g. waiting for a condvar or mutex) then timeout
 * processing is deadlocked on that CPU, causing system failure.
 *
 * For this reason the driver does not use event based MCDI completion, as this
 * leads to numerous paths involving timeouts and reentrant GLDv3 entrypoints
 * that result in a deadlocked system.
 */
#define	SFXGE_MCDI_POLL_INTERVAL	10		/* 10us in 1us units */
#define	SFXGE_MCDI_WATCHDOG_INTERVAL	10000000	/* 10s in 1us units */


/* Acquire exclusive access to MCDI for the duration of a request */
static void
sfxge_mcdi_acquire(sfxge_mcdi_t *smp)
{
	mutex_enter(&(smp->sm_lock));
	ASSERT3U(smp->sm_state, !=, SFXGE_MCDI_UNINITIALIZED);

	while (smp->sm_state != SFXGE_MCDI_INITIALIZED) {
		(void) cv_wait_sig(&(smp->sm_kv), &(smp->sm_lock));
	}
	smp->sm_state = SFXGE_MCDI_BUSY;

	mutex_exit(&(smp->sm_lock));
}


/* Release ownership of MCDI on request completion */
static void
sfxge_mcdi_release(sfxge_mcdi_t *smp)
{
	mutex_enter(&(smp->sm_lock));
	ASSERT((smp->sm_state == SFXGE_MCDI_BUSY) ||
	    (smp->sm_state == SFXGE_MCDI_COMPLETED));

	smp->sm_state = SFXGE_MCDI_INITIALIZED;
	cv_broadcast(&(smp->sm_kv));

	mutex_exit(&(smp->sm_lock));
}


static void
sfxge_mcdi_timeout(sfxge_t *sp)
{
	dev_info_t *dip = sp->s_dip;

	cmn_err(CE_WARN, SFXGE_CMN_ERR "[%s%d] MC_TIMEOUT",
	    ddi_driver_name(dip), ddi_get_instance(dip));

	DTRACE_PROBE(mcdi_timeout);
	(void) sfxge_restart_dispatch(sp, DDI_SLEEP, SFXGE_HW_ERR,
	    "MCDI timeout", 0);
}


static void
sfxge_mcdi_poll(sfxge_t *sp)
{
	efx_nic_t *enp = sp->s_enp;
	clock_t timeout;
	boolean_t aborted;

	/* Poll until request completes or timeout */
	timeout = ddi_get_lbolt() + drv_usectohz(SFXGE_MCDI_WATCHDOG_INTERVAL);
	while (efx_mcdi_request_poll(enp) == B_FALSE) {

		/* No response received yet */
		if (ddi_get_lbolt() > timeout) {
			/* Timeout expired */
			goto fail;
		}

		/* Short delay to avoid excessive PCIe traffic */
		drv_usecwait(SFXGE_MCDI_POLL_INTERVAL);
	}

	/* Request completed (or polling failed) */
	return;

fail:
	/* Timeout before request completion */
	DTRACE_PROBE(fail);
	aborted = efx_mcdi_request_abort(enp);
	ASSERT(aborted);
	sfxge_mcdi_timeout(sp);
}


static void
sfxge_mcdi_execute(void *arg, efx_mcdi_req_t *emrp)
{
	sfxge_t *sp = (sfxge_t *)arg;
	sfxge_mcdi_t *smp = &(sp->s_mcdi);

	sfxge_mcdi_acquire(smp);

	/* Issue request and poll for completion */
	efx_mcdi_request_start(sp->s_enp, emrp, B_FALSE);
	sfxge_mcdi_poll(sp);

	sfxge_mcdi_release(smp);
}


static void
sfxge_mcdi_ev_cpl(void *arg)
{
	sfxge_t *sp = (sfxge_t *)arg;
	sfxge_mcdi_t *smp = &(sp->s_mcdi);

	mutex_enter(&(smp->sm_lock));
	ASSERT(smp->sm_state == SFXGE_MCDI_BUSY);
	smp->sm_state = SFXGE_MCDI_COMPLETED;
	cv_broadcast(&(smp->sm_kv));
	mutex_exit(&(smp->sm_lock));
}


static void
sfxge_mcdi_exception(void *arg, efx_mcdi_exception_t eme)
{
	sfxge_t *sp = (sfxge_t *)arg;
	const char *reason;

	if (eme == EFX_MCDI_EXCEPTION_MC_REBOOT)
		reason = "MC_REBOOT";
	else if (eme == EFX_MCDI_EXCEPTION_MC_BADASSERT)
		reason = "MC_BADASSERT";
	else
		reason = "MC_UNKNOWN";

	DTRACE_PROBE(mcdi_exception);
	/* sfxge_evq_t->se_lock held */
	(void) sfxge_restart_dispatch(sp, DDI_SLEEP, SFXGE_HW_ERR, reason, 0);
}


int
sfxge_mcdi_init(sfxge_t *sp)
{
	efx_nic_t *enp = sp->s_enp;
	sfxge_mcdi_t *smp = &(sp->s_mcdi);
	efx_mcdi_transport_t *emtp = &(smp->sm_emt);
	int rc;

	ASSERT3U(smp->sm_state, ==, SFXGE_MCDI_UNINITIALIZED);

	mutex_init(&(smp->sm_lock), NULL, MUTEX_DRIVER, NULL);

	smp->sm_state = SFXGE_MCDI_INITIALIZED;

	emtp->emt_context = sp;
	emtp->emt_execute = sfxge_mcdi_execute;
	emtp->emt_ev_cpl = sfxge_mcdi_ev_cpl;
	emtp->emt_exception = sfxge_mcdi_exception;

	cv_init(&(smp->sm_kv), NULL, CV_DRIVER, NULL);

	if ((rc = efx_mcdi_init(enp, emtp)) != 0)
		goto fail1;

	return (0);

fail1:
	DTRACE_PROBE1(fail1, int, rc);

	cv_destroy(&(smp->sm_kv));
	mutex_destroy(&(smp->sm_lock));

	smp->sm_state = SFXGE_MCDI_UNINITIALIZED;
	smp->sm_sp = NULL;
	SFXGE_OBJ_CHECK(smp, sfxge_mcdi_t);

	return (rc);
}


void
sfxge_mcdi_fini(sfxge_t *sp)
{
	efx_nic_t *enp = sp->s_enp;
	sfxge_mcdi_t *smp = &(sp->s_mcdi);
	efx_mcdi_transport_t *emtp;

	mutex_enter(&(smp->sm_lock));
	ASSERT3U(smp->sm_state, ==, SFXGE_MCDI_INITIALIZED);

	efx_mcdi_fini(enp);
	emtp = &(smp->sm_emt);
	bzero(emtp, sizeof (*emtp));

	smp->sm_sp = NULL;

	cv_destroy(&(smp->sm_kv));
	mutex_exit(&(smp->sm_lock));

	mutex_destroy(&(smp->sm_lock));

	smp->sm_state = SFXGE_MCDI_UNINITIALIZED;
	SFXGE_OBJ_CHECK(smp, sfxge_mcdi_t);
}


int
sfxge_mcdi_ioctl(sfxge_t *sp, sfxge_mcdi_ioc_t *smip)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(sp->s_enp);
	sfxge_mcdi_t *smp = &(sp->s_mcdi);
	efx_mcdi_req_t emr;
	uint8_t *out;
	int rc;

	if (smp->sm_state == SFXGE_MCDI_UNINITIALIZED) {
		rc = ENODEV;
		goto fail1;
	}

	if (!(encp->enc_features & EFX_FEATURE_MCDI)) {
		rc = ENOTSUP;
		goto fail2;
	}

	if ((out = kmem_zalloc(sizeof (smip->smi_payload), KM_SLEEP)) == NULL) {
		rc = ENOMEM;
		goto fail3;
	}

	emr.emr_cmd = smip->smi_cmd;
	emr.emr_in_buf = smip->smi_payload;
	emr.emr_in_length = smip->smi_len;

	emr.emr_out_buf = out;
	emr.emr_out_length = sizeof (smip->smi_payload);

	sfxge_mcdi_execute(sp, &emr);

	smip->smi_rc = (uint8_t)emr.emr_rc;
	smip->smi_cmd = (uint8_t)emr.emr_cmd;
	smip->smi_len = (uint8_t)emr.emr_out_length_used;
	memcpy(smip->smi_payload, out, smip->smi_len);

	/*
	 * Helpfully trigger a device reset in response to an MCDI_CMD_REBOOT
	 * Both ports will see ->emt_exception callbacks on the next MCDI poll
	 */
	if (smip->smi_cmd == MC_CMD_REBOOT) {

		DTRACE_PROBE(mcdi_ioctl_mc_reboot);
		/* sfxge_t->s_state_lock held */
		(void) sfxge_restart_dispatch(sp, DDI_SLEEP, SFXGE_HW_OK,
		    "MC_REBOOT triggering restart", 0);
	}

	kmem_free(out, sizeof (smip->smi_payload));

	return (0);

fail3:
	DTRACE_PROBE(fail3);
fail2:
	DTRACE_PROBE(fail2);
fail1:
	DTRACE_PROBE1(fail1, int, rc);
	return (rc);
}
