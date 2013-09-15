#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://opensource.org/licenses/CDDL-1.0.
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
# Copyright 2013 Saso Kiselkov.  All rights reserved.
#
#	This makefile drives the production of the skein test suite.
#	To run the tests, simply type "make" or "make check".

include ../../../../Makefile.master

CFLAGS = -I${SRC}/uts/common -D_KERNEL -fno-builtin -O2 -W -Wall \
  -Wno-unknown-pragmas -std=c99
CPUMHZ:sh = psrinfo -v 0 | awk '/processor operates at/{print $6}'
GCC = $(GCC_ROOT)/bin/gcc

check : skein_test32_check skein_test64_check

all : skein_test32 skein_test64

skein_test32 : skein_test.c
	@ echo Building 32-bit test...
	@ $(GCC) $(CFLAGS) -o skein_test32 skein_test.c ../skein.c \
	    ../skein_block.c

skein_test64 : skein_test.c
	@ echo Building 64-bit test...
	@ $(GCC) $(CFLAGS) -m64 -o skein_test64 skein_test.c ../skein.c \
	    ../skein_block.c

skein_test32_check : skein_test32
	@ echo Running 32-bit test...
	@ ./skein_test32 $(CPUMHZ)

skein_test64_check : skein_test64
	@ echo Running 64-bit test...
	@ ./skein_test64 $(CPUMHZ)

clean :
	rm -f skein_test32 skein_test64
