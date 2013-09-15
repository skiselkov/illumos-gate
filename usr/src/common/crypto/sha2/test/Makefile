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
#	This makefile drives the production of the sha2 test suite.
#	To run the tests, simply type "make" or "make check".

include ../../../../Makefile.master

CFLAGS = -I${SRC}/uts/common -D_KERNEL -fno-builtin -O2 -W -Wall \
    -Wno-unknown-pragmas -Wno-unused -std=c99
CPUMHZ:sh = psrinfo -v 0 | awk '/processor operates at/{print $6}'
GCC = $(GCC_ROOT)/bin/gcc

PERL = perl
AS = /opt/onbld/bin/i386/aw
ASSFLAGS = -P -xarch=amd64 -P -Ui386 -U__i386 -D_KERNEL -D_SYSCALL32 \
    -D_SYSCALL32_IMPL -D_ELF64  -D_DDI_STRICT -Dsun -D__sun -D__SVR4 \
    -DDEBUG   -D_ASM -D__STDC__=0  -I../../../../uts/intel \
    -I../../../../uts/common
COMPILE.s = $(AS) $(ASSFLAGS)

check : sha2_test32_check sha2_test64_check
all : sha2_test32 sha2_test64

sha2_test32 : sha2_test.c
	@ echo Building 32-bit test...
	@ $(GCC) $(CFLAGS) -o sha2_test32 sha2_test.c ../sha2.c

sha2_test64 : sha2_test.c sha256-x86_64.o sha512-x86_64.o ../sha2.c
	@ echo Building 64-bit test...
	@ $(GCC) $(CFLAGS) -m64 -o $@ sha2_test.c sha256-x86_64.o \
	    sha512-x86_64.o ../sha2.c

sha2_test32_check : sha2_test32
	@ echo Running 32-bit test...
	@ ./sha2_test32 $(CPUMHZ)

sha2_test64_check : sha2_test64
	@ echo Running 64-bit test...
	@ ./sha2_test64 $(CPUMHZ)

%.o: %.s
	@ $(COMPILE.s) -o $@ ${@F:.o=.s}

sha512-x86_64.s: ../amd64/sha512-x86_64.pl
	@ $(PERL) $? $@

sha256-x86_64.s: ../amd64/sha512-x86_64.pl
	@ $(PERL) $? $@

clean :
	rm -f sha2_test32 sha2_test64 *.o *.s
