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
set_tunable32 zfs_txgs_per_trim 2

function txgs
{
	typeset x

	# Run some txgs in order to let autotrim do its work.
	#
	for x in 1 2 3; do
		log_must $ZFS snapshot $TRIMPOOL@snap
		log_must $ZFS destroy  $TRIMPOOL@snap
		log_must $ZFS snapshot $TRIMPOOL@snap
		log_must $ZFS destroy  $TRIMPOOL@snap
		# We need to introduce some delay to get the queued trim zios
		# to run through and actaully affect the host FS
		sync
		sleep 1
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
	log_must $ZPOOL set autotrim=on $TRIMPOOL
	log_must $FILE_WRITE -o create -f "/$TRIMPOOL/$TESTFILE" \
	    -b $BLOCKSIZE -c $NUM_WRITES -d R -w
	for x in 1 2 3; do
		log_must $FILE_WRITE -o create \
		    -f "/$TRIMPOOL/${TESTFILE}-keep$x" \
		    -b $BLOCKSIZE -c $NUM_WRITES -d R -w
	done
	log_must rm "/$TRIMPOOL/$TESTFILE"
	txgs
	checkpool $TRIMPOOL
	log_must $ZPOOL destroy $TRIMPOOL
done

#
# Check various pool geometries: Create the pool, fill it, remove the test file,
# run some txgs, export the pool and verify that the vdevs shrunk.
#

#
# raidz
#
for z in 1 2 3; do
	setupvdevs
	log_must $ZPOOL create -o cachefile=none -f $TRIMPOOL raidz$z $VDEVS
	log_must $ZPOOL set autotrim=on $TRIMPOOL
	log_must $FILE_WRITE -o create -f "/$TRIMPOOL/$TESTFILE" \
	    -b $BLOCKSIZE -c $NUM_WRITES -d R -w
	log_must rm "/$TRIMPOOL/$TESTFILE"
	txgs
	log_must $ZPOOL export $TRIMPOOL
	checkvdevs
done

#
# mirror
#
setupvdevs
log_must $ZPOOL create -o cachefile=none -f $TRIMPOOL mirror $MIRROR_VDEVS_1 \
    mirror $MIRROR_VDEVS_2
log_must $ZPOOL set autotrim=on $TRIMPOOL
log_must $FILE_WRITE -o create -f "/$TRIMPOOL/$TESTFILE" \
    -b $BLOCKSIZE -c $NUM_WRITES -d R -w
log_must rm "/$TRIMPOOL/$TESTFILE"
txgs
log_must $ZPOOL export $TRIMPOOL
checkvdevs

#
# stripe
#
setupvdevs
log_must $ZPOOL create -o cachefile=none -f $TRIMPOOL $STRIPE_VDEVS
log_must $ZPOOL set autotrim=on $TRIMPOOL
log_must $FILE_WRITE -o create -f "/$TRIMPOOL/$TESTFILE" \
    -b $BLOCKSIZE -c $NUM_WRITES -d R -w
log_must rm "/$TRIMPOOL/$TESTFILE"
txgs
log_must $ZPOOL export $TRIMPOOL
checkvdevs

log_pass TRIM successfully shrunk vdevs
