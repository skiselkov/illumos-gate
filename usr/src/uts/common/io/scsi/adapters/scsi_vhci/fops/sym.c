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
 * Copyright (c) 2001, 2010, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2013 by Saso Kiselkov. All rights reserved.
 */

/*
 * Implementation of "scsi_vhci_f_sym" symmetric failover_ops.
 *
 * This file was historically meant for only symmetric implementation.  It has
 * been extended to manage SUN "supported" symmetric controllers. The supported
 * VID/PID shall be listed in the symmetric_dev_table.
 */

#include <sys/conf.h>
#include <sys/file.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/scsi/scsi.h>
#include <sys/scsi/adapters/scsi_vhci.h>

/* Supported device table entries.  */
char *symmetric_dev_table[] = {
/*	"                  111111" */
/*	"012345670123456789012345" */
/*	"|-VID--||-----PID------|" */
/*	see prefix_pattern_match for format description */
				/* disks */
	"HITACHI HUC######@@S###",	/* Ultrastar (SAS) C7K1000, C10K1200 */
					/*   C10K900, C10K600, C15K147 */
	"HITACHI HUS######@@F###",	/* Ultrastar (FC) 15K600 */
	"HITACHI HUS######@@S###",	/* Ultrastar (SAS) He6, 7K4000, */
					/*   7K3000, 15K600 */
	"HITACHI HUSM@####ASS###",	/* Ultrastar SSD800MH, SSD800MM */
					/*   SSD1000MR, SSD400M */
	"HITACHI HUSS@####BSS###",	/* Ultrastar SSD400S.B */
	"HP      DF",			/* HP Enterprise HDDs */
	"HP      DG",			/* HP Enterprise HDDs */
	"HP      DH",			/* HP Enterprise HDDs */
	"HP      EF",			/* HP Enterprise HDDs */
	"HP      EG",			/* HP Enterprise HDDs */
	"HP      EH",			/* HP Enterprise HDDs */
	"HP      EO",			/* HP Enterprise HDDs */
	"HP      MB",			/* HP Enterprise HDDs */
	"HP      MM",			/* HP Enterprise HDDs */
	"HP      ST",			/* HP Enterprise HDDs */
	"IBM     DDYFT",		/* IBM FC HDDs */
	"IBM     IC",
	"OCZ     TALOS",		/* Talos SSD */
	"Pliant  LS",			/* Pliant SAS SSDs */
	"Pliant  LB###",		/* Pliant SAS SSDs */
	"SEAGATE ST",			/* Seagate Enterprise HDDs/SSDs */
	"STEC    Z16",			/* ZeusIOPS */
	"STEC    Z4",			/* ZeusIOPS */
	"STEC    Zeus",			/* ZeusRAM SAS SSD */
	"STEC    S###",			/* s800 Series SAS SSD */
	"TOSHIBA AL13SE@###",		/* AL13SEBxxx, AL13SELxxx */
	"TOSHIBA MB@###?@@",		/* MBA3xxxFD, MBA3xxxNC, MBA3xxxNP, */
					/*   MBA3xxxRC, MBB2xxxRC, MBD2xxxRC, */
					/*   MBE2xxxRC, MBF2xxLRC, MBF2xxxRC */
	"TOSHIBA MG03@CA###",		/* MG03ACAxxx, MG03SCAxxx */
	"TOSHIBA MK##0#GRR@",		/* MKxx01GRRB, MKxx01GRRR */
	"TOSHIBA MK##0#T@KB",		/* MKxx01TRKB, MKxx02TSKB */
	"TOSHIBA MK##0#GRZB",		/* MKxx01GRZB */
	"TOSHIBA PX0#S@@###",		/* PX02SMxxxx, PX02SSFxxx, PX03SNxxxx */
	"WD      WD####@KHG",		/* WD Xe */
	"WD      WD####FYYG",		/* WD Re */
				/* enclosures */
	"SUN     SENA",			/* SES device */
	"SUN     SESS01",		/* VICOM SVE box */
	"SUNW    SUNWGS",		/* Daktari enclosure */
				/* arrays */
	"3PARdataVV",			/* 3PAR storage */
	"COMPELNTCompellent Vol",	/* Compellent storage */
	"HITACHI OPEN",			/* Hitachi storage */
	"NETAPP  LUN",			/* NetApp storage */
	"HP      OPEN",			/* HP storage */
	"HP      HSV2",			/* HP StorageWorks */
	"SUN     PSX1000",		/* Pirus Matterhorn */
	"SUN     SE6920",		/* Pirus */
	"SUN     SE6940",		/* DSP - Nauset */
	"SUN     StorEdge 3510",	/* Minnow FC */
	"SUN     StorEdge 3511",	/* Minnow SATA RAID */
	"SUN     StorageTek 6920",	/* DSP */
	"SUN     StorageTek 6940",	/* DSP - Nauset */
	"SUN     StorageTek NAS",	/* StorageTek NAS */
	"SUN     MRA300_R",		/* Shamrock - Controller */
	"SUN     MRA300_E",		/* Shamrock - Expansion */
			/* special */
	"?????????????????SUN??G",	/* Generic Sun supported disks */
	"?????????????????SUN??T",	/* Generic Sun supported disks */
	"?????????????????SUN???G",	/* Generic Sun supported disks */
	"?????????????????SUN???T",	/* Generic Sun supported disks */
	"ATA     ",			/* Generic ATA device */
	NULL
};

/* Failover module plumbing. */
SCSI_FAILOVER_OP(SFO_NAME_SYM, symmetric);

/*
 * Perform a match of a pattern on a string. The match is performed from
 * the start of the string and needn't match its entire length (i.e. it's
 * a prefix-match), though any direct conflict with the pattern isn't
 * allowed. The pattern can contain the following special characters:
 *	? - any character
 *	@ - any ASCII letter (case insensitive)
 *	# - any ASCII digit
 */
static boolean_t
prefix_pattern_match(const char *pat, const char *str)
{
	for (; *pat && *str; pat++, str++) {
		switch (*pat) {
		case '?':	/* any character */
			break;
		case '@':	/* A-Za-z */
			if ((*str < 'a' || *str > 'z') &&
			    (*str < 'A' || *str > 'Z'))
				return (B_FALSE);
			break;
		case '#':	/* 0-9 */
			if (*str < '0' || *str > '9')
				return (B_FALSE);
			break;
		default:	/* exact match */
			if (*pat != *pat)
				return (B_FALSE);
			break;
		}
	}
	return (*pat == 0);
}

/* ARGSUSED */
static int
symmetric_device_probe(struct scsi_device *sd, struct scsi_inquiry *stdinq,
    void **ctpriv)
{
	char	**dt;
	VHCI_DEBUG(6, (CE_NOTE, NULL, "!inq str: %s\n", stdinq->inq_vid));
	for (dt = symmetric_dev_table; *dt; dt++)
		if (prefix_pattern_match (*dt, stdinq->inq_vid))
			return (SFO_DEVICE_PROBE_VHCI);
	return (SFO_DEVICE_PROBE_PHCI);
}

/* ARGSUSED */
static void
symmetric_device_unprobe(struct scsi_device *sd, void *ctpriv)
{
	/*
	 * NOP for symmetric
	 */
}

/* ARGSUSED */
static int
symmetric_path_activate(struct scsi_device *sd, char *pathclass, void *ctpriv)
{
	return (0);
}

/* ARGSUSED */
static int
symmetric_path_deactivate(struct scsi_device *sd, char *pathclass,
void *ctpriv)
{
	return (0);
}

/* ARGSUSED */
static int
symmetric_path_get_opinfo(struct scsi_device *sd,
struct scsi_path_opinfo *opinfo, void *ctpriv)
{
	opinfo->opinfo_rev = OPINFO_REV;
	(void) strcpy(opinfo->opinfo_path_attr, "primary");
	opinfo->opinfo_path_state  = SCSI_PATH_ACTIVE;
	opinfo->opinfo_pswtch_best = 0;		/* N/A */
	opinfo->opinfo_pswtch_worst = 0;	/* N/A */
	opinfo->opinfo_xlf_capable = 0;
	opinfo->opinfo_mode = SCSI_NO_FAILOVER;
	opinfo->opinfo_preferred = 1;

	return (0);
}

/* ARGSUSED */
static int
symmetric_path_ping(struct scsi_device *sd, void *ctpriv)
{
	return (1);
}

/* ARGSUSED */
static int
symmetric_analyze_sense(struct scsi_device *sd,
uint8_t *sense, void *ctpriv)
{
	return (SCSI_SENSE_NOFAILOVER);
}

/* ARGSUSED */
static int
symmetric_pathclass_next(char *cur, char **nxt, void *ctpriv)
{
	if (cur == NULL) {
		*nxt = PCLASS_PRIMARY;
		return (0);
	} else if (strcmp(cur, PCLASS_PRIMARY) == 0) {
		return (ENOENT);
	} else {
		return (EINVAL);
	}
}
