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
#include <sys/gld.h>
#include <sys/stream.h>
#include <sys/strsun.h>
#include <sys/strsubr.h>
#include <sys/dlpi.h>
#include <sys/pattr.h>
#include <sys/ksynch.h>
#include <sys/spl.h>

#include <inet/nd.h>
#include <inet/mi.h>

#include "sfxge.h"

#ifdef _USE_GLD_V2

void
sfxge_gld_link_update(sfxge_t *sp)
{
	sfxge_mac_t *smp = &(sp->s_mac);
	int32_t link;

	switch (smp->sm_link_mode) {
	case EFX_LINK_UNKNOWN:
		link = GLD_LINKSTATE_UNKNOWN;
		break;
	case EFX_LINK_DOWN:
		link = GLD_LINKSTATE_DOWN;
		break;
	default:
		link = GLD_LINKSTATE_UP;
	}

	gld_linkstate(sp->s_gmip, link);
}

void
sfxge_gld_mtu_update(sfxge_t *sp)
{
}

void
sfxge_gld_rx_post(sfxge_t *sp, unsigned int index, mblk_t *mp)
{
	mblk_t **mpp;

	_NOTE(ARGUNUSED(index))

	mutex_enter(&(sp->s_rx_lock));
	*(sp->s_mpp) = mp;

	mpp = &mp;
	while ((mp = *mpp) != NULL)
		mpp = &(mp->b_next);

	sp->s_mpp = mpp;
	mutex_exit(&(sp->s_rx_lock));
}

void
sfxge_gld_rx_push(sfxge_t *sp)
{
	mblk_t *mp;

	mutex_enter(&(sp->s_rx_lock));
	mp = sp->s_mp;
	sp->s_mp = NULL;
	sp->s_mpp = &(sp->s_mp);
	mutex_exit(&(sp->s_rx_lock));

	while (mp != NULL) {
		mblk_t *next;

		next = mp->b_next;
		mp->b_next = NULL;

		/* The stack behaves badly with multi-fragment messages */
		if (mp->b_cont != NULL) {
			uint16_t flags = DB_CKSUMFLAGS(mp);

			(void) pullupmsg(mp, -1);

			DB_CKSUMFLAGS(mp) = flags;
		}

		gld_recv(sp->s_gmip, mp);

		mp = next;
	}
}

static int
sfxge_gld_get_stats(gld_mac_info_t *gmip, struct gld_stats *gsp)
{
	sfxge_t *sp = (sfxge_t *)(gmip->gldm_private);
	unsigned int speed;
	sfxge_link_duplex_t duplex;
	uint64_t val;

	sfxge_mac_stat_get(sp, EFX_MAC_TX_ERRORS, &val);
	gsp->glds_errxmt = (uint32_t)val;

	sfxge_mac_stat_get(sp, EFX_MAC_RX_ERRORS, &val);
	gsp->glds_errrcv = (uint32_t)val;

	sfxge_mac_stat_get(sp, EFX_MAC_RX_FCS_ERRORS, &val);
	gsp->glds_crc = (uint32_t)val;

	sfxge_mac_stat_get(sp, EFX_MAC_RX_DROP_EVENTS, &val);
	gsp->glds_norcvbuf = (uint32_t)val;

	sfxge_mac_link_speed_get(sp, &speed);
	gsp->glds_speed = (uint64_t)speed * 1000000ull;

	sfxge_mac_link_duplex_get(sp, &duplex);

	switch (duplex) {
	case SFXGE_LINK_DUPLEX_UNKNOWN:
		gsp->glds_duplex = GLD_DUPLEX_UNKNOWN;
		break;

	case SFXGE_LINK_DUPLEX_HALF:
		gsp->glds_duplex = GLD_DUPLEX_HALF;
		break;

	case SFXGE_LINK_DUPLEX_FULL:
		gsp->glds_duplex = GLD_DUPLEX_FULL;
		break;

	default:
		ASSERT(B_FALSE);
		break;
	}

	return (GLD_SUCCESS);
}

static int
sfxge_gld_reset(gld_mac_info_t *gmip)
{
	/*
	 * The driver already has hardware resets at appropriate times
	 * This is only ever called before gld_start()
	 */
	return (GLD_SUCCESS);
}

static int
sfxge_gld_start(gld_mac_info_t *gmip)
{
	sfxge_t *sp = (sfxge_t *)(gmip->gldm_private);
	int rc;

	mutex_enter(&(sp->s_rx_lock));
	sp->s_mpp = &(sp->s_mp);
	mutex_exit(&(sp->s_rx_lock));

	if ((rc = sfxge_start(sp, B_FALSE)) != 0)
		goto fail1;

	return (GLD_SUCCESS);

fail1:
	DTRACE_PROBE1(fail1, int, rc);

	mutex_enter(&(sp->s_rx_lock));
	ASSERT3P(sp->s_mp, ==, NULL);
	sp->s_mpp = NULL;
	mutex_exit(&(sp->s_rx_lock));

	return (GLD_FAILURE);
}

static int
sfxge_gld_stop(gld_mac_info_t *gmip)
{
	sfxge_t *sp = (sfxge_t *)(gmip->gldm_private);

	sfxge_stop(sp);

	mutex_enter(&(sp->s_rx_lock));
	ASSERT3P(sp->s_mp, ==, NULL);
	sp->s_mpp = NULL;
	mutex_exit(&(sp->s_rx_lock));

	return (GLD_SUCCESS);
}

static int
sfxge_gld_set_promiscuous(gld_mac_info_t *gmip, int flags)
{
	sfxge_t *sp = (sfxge_t *)(gmip->gldm_private);
	int rc;

	switch (flags) {
	case GLD_MAC_PROMISC_NONE:
		if ((rc = sfxge_mac_promisc_set(sp, SFXGE_PROMISC_OFF)) != 0)
			goto fail1;
		break;
	case GLD_MAC_PROMISC_MULTI:
		if ((rc = sfxge_mac_promisc_set(sp, SFXGE_PROMISC_ALL_MULTI))
		    != 0)
			goto fail2;
		break;
	default:
		if ((rc = sfxge_mac_promisc_set(sp, SFXGE_PROMISC_ALL_PHYS))
		    != 0)
			goto fail3;
		break;
	}

	return (GLD_SUCCESS);

fail3:
	DTRACE_PROBE(fail3);
fail2:
	DTRACE_PROBE(fail2);
fail1:
	DTRACE_PROBE1(fail1, int, rc);
	return (GLD_FAILURE);
}

static int
sfxge_gld_set_multicast(gld_mac_info_t *gmip, unsigned char *addr, int flag)
{
	sfxge_t *sp = (sfxge_t *)(gmip->gldm_private);
	int rc;

	switch (flag) {
	case GLD_MULTI_ENABLE:
		if ((rc = sfxge_mac_multicst_add(sp, addr)) != 0)
			goto fail1;
		break;
	case GLD_MULTI_DISABLE:
		if ((rc = sfxge_mac_multicst_remove(sp, addr)) != 0)
			goto fail2;
		break;
	default:
		ASSERT(B_FALSE);
		break;
	}

	return (GLD_SUCCESS);

fail2:
	DTRACE_PROBE(fail2);
fail1:
	DTRACE_PROBE1(fail1, int, rc);
	return (GLD_FAILURE);
}

static int
sfxge_gld_set_mac_addr(gld_mac_info_t *gmip, unsigned char *addr)
{
	sfxge_t *sp = (sfxge_t *)(gmip->gldm_private);
	int rc;

	if ((rc = sfxge_mac_unicst_set(sp, addr)) != 0)
		goto fail1;

	return (GLD_SUCCESS);

fail1:
	DTRACE_PROBE1(fail1, int, rc);

	return (GLD_BADARG);
}

int
sfxge_gld_send(gld_mac_info_t *gmip, mblk_t *mp)
{
	sfxge_t *sp = (sfxge_t *)(gmip->gldm_private);

	(void) sfxge_tx_packet_add(sp, mp);

	/*
	 * This gives no TX backpressure, which can cause unbounded TX DPL
	 * size. See bug 18984. This feature is implemented for GLDv3
	 */
	return (GLD_SUCCESS);
}

int
sfxge_gld_ioctl(gld_mac_info_t *gmip, queue_t *wq, mblk_t *mp)
{
	sfxge_t *sp = (sfxge_t *)(gmip->gldm_private);
	struct iocblk *iocp;

	iocp = (struct iocblk *)mp->b_rptr;

	switch (iocp->ioc_cmd) {
	case ND_GET:
	case ND_SET:
		if (!nd_getset(wq, sp->s_ndh, mp))
			miocnak(wq, mp, 0, EINVAL);
		else
			qreply(wq, mp);
		break;

	default:
		sfxge_ioctl(sp, wq, mp);
		break;
	}

	return (GLD_SUCCESS);
}


int
sfxge_gld_register(sfxge_t *sp)
{
	gld_mac_info_t *gmip;
	unsigned int pri;
	int rc;

	mutex_init(&(sp->s_rx_lock), NULL, MUTEX_DRIVER,
	    DDI_INTR_PRI(sp->s_intr.si_intr_pri));

	if ((rc = sfxge_gld_nd_register(sp)) != 0)
		goto fail1;

	gmip = gld_mac_alloc(sp->s_dip);

	gmip->gldm_private = (caddr_t)sp;

	gmip->gldm_reset = sfxge_gld_reset;
	gmip->gldm_start = sfxge_gld_start;
	gmip->gldm_stop = sfxge_gld_stop;
	gmip->gldm_set_mac_addr = sfxge_gld_set_mac_addr;
	gmip->gldm_set_multicast = sfxge_gld_set_multicast;
	gmip->gldm_set_promiscuous = sfxge_gld_set_promiscuous;
	gmip->gldm_send = sfxge_gld_send;
	gmip->gldm_get_stats = sfxge_gld_get_stats;
	gmip->gldm_ioctl = sfxge_gld_ioctl;

	gmip->gldm_ident = (char *)sfxge_ident;
	gmip->gldm_type = DL_ETHER;
	gmip->gldm_minpkt = 0;
	gmip->gldm_maxpkt = sp->s_mtu;
	gmip->gldm_addrlen = ETHERADDRL;
	gmip->gldm_saplen = -2;
	gmip->gldm_broadcast_addr = sfxge_brdcst;

	gmip->gldm_vendor_addr = kmem_alloc(ETHERADDRL, KM_SLEEP);

	if ((rc = sfxge_mac_unicst_get(sp, SFXGE_UNICST_BIA,
	    gmip->gldm_vendor_addr)) != 0)
		goto fail2;

	gmip->gldm_devinfo = sp->s_dip;
	gmip->gldm_ppa = ddi_get_instance(sp->s_dip);

	gmip->gldm_cookie = spltoipl(0);

	gmip->gldm_capabilities =
	    GLD_CAP_LINKSTATE |
	    GLD_CAP_CKSUM_IPHDR |
	    GLD_CAP_CKSUM_FULL_V4 |
	    GLD_CAP_ZEROCOPY;

	if ((rc = gld_register(sp->s_dip, (char *)ddi_driver_name(sp->s_dip),
	    gmip)) != 0)
		goto fail3;

	sp->s_gmip = gmip;
	return (0);

fail3:
	DTRACE_PROBE(fail3);
fail2:
	DTRACE_PROBE(fail2);

	kmem_free(gmip->gldm_vendor_addr, ETHERADDRL);
	gld_mac_free(gmip);

	sfxge_gld_nd_unregister(sp);

fail1:
	DTRACE_PROBE1(fail1, int, rc);
	mutex_destroy(&(sp->s_rx_lock));

	return (rc);
}

int
sfxge_gld_unregister(sfxge_t *sp)
{
	gld_mac_info_t *gmip = sp->s_gmip;
	int rc;

	if ((rc = gld_unregister(gmip)) != 0)
		goto fail1;

	sp->s_gmip = NULL;

	kmem_free(gmip->gldm_vendor_addr, ETHERADDRL);
	gld_mac_free(gmip);

	sfxge_gld_nd_unregister(sp);

	mutex_destroy(&(sp->s_rx_lock));

	return (0);

fail1:
	DTRACE_PROBE1(fail1, int, rc);

	return (rc);
}
#endif	/* _USE_GLD_V2 */
