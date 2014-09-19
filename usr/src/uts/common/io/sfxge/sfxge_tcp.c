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
#include <sys/stream.h>
#include <sys/strsun.h>
#include <sys/strsubr.h>
#include <sys/pattr.h>

#include <sys/ethernet.h>
#include <inet/ip.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#include "sfxge.h"

#include "efx.h"

void
sfxge_tcp_parse(mblk_t *mp, struct ether_header **etherhpp, struct ip **iphpp,
    struct tcphdr **thpp, size_t *offp, size_t *sizep)
{
	struct ether_header *etherhp;
	uint16_t ether_type;
	size_t etherhs;
	struct ip *iphp;
	size_t iphs;
	struct tcphdr *thp;
	size_t ths;
	size_t off;
	size_t size;

	etherhp = NULL;
	iphp = NULL;
	thp = NULL;
	off = 0;
	size = 0;

	/* Grab the MAC header */
	if (MBLKL(mp) < sizeof (struct ether_header))
		if (!pullupmsg(mp, sizeof (struct ether_header)))
			goto done;

	/*LINTED*/
	etherhp = (struct ether_header *)(mp->b_rptr);
	ether_type = etherhp->ether_type;
	etherhs = sizeof (struct ether_header);

	if (ether_type == htons(ETHERTYPE_VLAN)) {
		struct ether_vlan_header *ethervhp;

		if (MBLKL(mp) < sizeof (struct ether_vlan_header))
			if (!pullupmsg(mp, sizeof (struct ether_vlan_header)))
				goto done;

		/*LINTED*/
		ethervhp = (struct ether_vlan_header *)(mp->b_rptr);
		ether_type = ethervhp->ether_type;
		etherhs = sizeof (struct ether_vlan_header);
	}

	if (ether_type != htons(ETHERTYPE_IP))
		goto done;

	/* Skip over the MAC header */
	off += etherhs;

	/* Grab the IP header */
	if (MBLKL(mp) < off + sizeof (struct ip))
		if (!pullupmsg(mp, off + sizeof (struct ip)))
			goto done;

	/*LINTED*/
	iphp = (struct ip *)(mp->b_rptr + off);
	iphs = iphp->ip_hl * 4;

	if (iphp->ip_v != IPV4_VERSION)
		goto done;

	/* Get the size of the packet */
	size = ntohs(iphp->ip_len);

	ASSERT3U(etherhs + size, <=, msgdsize(mp));

	if (iphp->ip_p != IPPROTO_TCP)
		goto done;

	/* Skip over the IP header */
	off += iphs;
	size -= iphs;

	/* Grab the TCP header */
	if (MBLKL(mp) < off + sizeof (struct tcphdr))
		if (!pullupmsg(mp, off + sizeof (struct tcphdr)))
			goto done;

	/*LINTED*/
	thp = (struct tcphdr *)(mp->b_rptr + off);
	ths = thp->th_off * 4;

	/* Skip over the TCP header */
	off += ths;
	size -= ths;

	if (MBLKL(mp) < off)
		(void) pullupmsg(mp, off);

done:
	*etherhpp = etherhp;
	*iphpp = iphp;
	*thpp = thp;
	*offp = off;
	*sizep = size;
}
