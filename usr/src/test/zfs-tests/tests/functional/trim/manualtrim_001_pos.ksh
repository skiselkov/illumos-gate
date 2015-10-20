#!/bin/ksh -p
#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#

#
# Copyright (c) 2017 by Tim Chase. All rights reserved.
# Copyright (c) 2017 by Nexenta Systems, Inc. All rights reserved.
#

. $STF_SUITE/include/libtest.shlib
. $STF_SUITE/tests/functional/trim/trim.cfg
. $STF_SUITE/tests/functional/trim/trim.kshlib

set_tunable64 zfs_trim_min_ext_sz 0

function dotrim
{
	typeset interrupt="$1"
	typeset scrub="$2"
	typeset poolvdevdir="$3"
	typeset done_status

	# Run manual trim for at most 30 seconds
	typeset stop_time=$(( $(date +%s) + 30 ))

	log_must rm "/$TRIMPOOL/$TESTFILE"
	log_must $ZPOOL export $TRIMPOOL
	if [ -n "$poolvdevdir" ]; then
		log_must $ZPOOL import -d $poolvdevdir $TRIMPOOL
	else
		log_must $ZPOOL import $TRIMPOOL
	fi
	if [ -z "$interrupt" ]; then
		log_must $ZPOOL trim $TRIMPOOL
		sleep 1
		done_status="completed"
	else
		# Run trim at the minimal rate to guarantee that it has
		# to be interrupted.
		log_must $ZPOOL trim -r 1 $TRIMPOOL
		sleep 1
		log_must $ZPOOL trim -s $TRIMPOOL
		done_status="interrupted"
	fi
	while true; do
		typeset st=$($ZPOOL status $TRIMPOOL | awk '/trim:/{print $2}')
		if [ -z "$st" ]; then
			log_fail "Pool reported '' trim status. Is TRIM" \
			    "supported on this box??"
		elif [[ "$st" = "completed" ]]; then
			[ -n "$interrupt" ] && log_fail "TRIM completed," \
			    "but we wanted it to be interrupted."
			break
		elif [[ "$st" = "interrupted" ]]; then
			[ -z "$interrupt" ] && log_fail "TRIM interrupted," \
			    "but we wanted it to complete."
			break
		elif [ "$(date +%s)" -ge $stop_time ]; then
			# Constrain the run time of trim so as not to overstep
			# the test time limit
			log_note "Exceeded trim time limit of 30s, stopping..."
			log_must $ZPOOL trim -s $TRIMPOOL
			break
		fi
		log_note "Waiting for TRIM status to change from \"$st\" to" \
		    "\"$done_status\""
		sleep 1
	done
	[ -n "$scrub" ] && checkpool $TRIMPOOL
	log_must $ZPOOL destroy $TRIMPOOL
	for i in {1..3}; do
		log_must sync
		log_must sleep 1
	done
}

#
# Let's try this on real disks first and do some scrubbing to check for
# data integrity.
#
for z in 1 2 3; do
	if [ $(echo "$TRIM_POOL_DISKS" | tr ' ' '\n' | grep -v '^$' | wc -l) \
	    -le $z ]; then
		continue
	fi
	log_must $ZPOOL create -o cachefile=none -f $TRIMPOOL raidz$z \
	    $TRIM_POOL_DISKS
	log_must $FILE_WRITE -o create -f "/$TRIMPOOL/$TESTFILE" \
	    -b $BLOCKSIZE -c $NUM_WRITES -d R -w
	for x in 1 2; do
		log_must $FILE_WRITE -o create \
		    -f "/$TRIMPOOL/${TESTFILE}-keep$x" \
		    -b $BLOCKSIZE -c $NUM_WRITES -d R -w
	done
	dotrim '' '1'
done

#
# Check various pool geometries: Create the pool, fill it, remove the test file,
# perform a manual trim, export the pool and verify that the vdevs shrunk.
#

#
# raidz
#
for z in 1 2 3; do
	setupvdevs
	log_must $ZPOOL create -o cachefile=none -f $TRIMPOOL raidz$z $VDEVS
	log_must $FILE_WRITE -o create -f "/$TRIMPOOL/$TESTFILE" \
	    -b $BLOCKSIZE -c $NUM_WRITES -d R -w
	dotrim '' '' $VDEVDIR
	checkvdevs
done

#
# mirror
#
setupvdevs
log_must $ZPOOL create -o cachefile=none -f $TRIMPOOL mirror $MIRROR_VDEVS_1 \
    mirror $MIRROR_VDEVS_2
log_must $FILE_WRITE -o create -f "/$TRIMPOOL/$TESTFILE" \
    -b $BLOCKSIZE -c $NUM_WRITES -d R -w
dotrim '' '' $VDEVDIR
checkvdevs

#
# stripe
#
setupvdevs
log_must $ZPOOL create -o cachefile=none -f $TRIMPOOL $STRIPE_VDEVS
log_must $FILE_WRITE -o create -f "/$TRIMPOOL/$TESTFILE" \
    -b $BLOCKSIZE -c $NUM_WRITES -d R -w
dotrim '' '' $VDEVDIR
checkvdevs

#
# trimus interruptus
#
setupvdevs
log_must $ZPOOL create -o cachefile=none -f $TRIMPOOL $STRIPE_VDEVS
log_must $FILE_WRITE -o create -f "/$TRIMPOOL/$TESTFILE" \
    -b $BLOCKSIZE -c $NUM_WRITES -d R -w
dotrim "1" '' $VDEVDIR
# Don't check vdev size, since we interrupted trim
log_must rm $VDEVS

log_pass Manual TRIM successfully shrunk vdevs
