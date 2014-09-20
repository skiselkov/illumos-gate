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
# Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# uts/intel/sfxge/Makefile
#
#	This makefile drives the production of the sfxge
#	network driver kernel module.
#
#	intel architecture dependent
#

#
#	Paths to the base of the uts directory trees
#
UTSBASE = ../..

#
#	Define the module and object file sets.
#
MODULE		= sfxge
OBJECTS		= $(SFXGE_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(SFXGE_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/common/io/sfxge

#
#	Include common rules.
#
include $(UTSBASE)/intel/Makefile.intel

CERRWARN	+= -_gcc=-Wno-parentheses
CERRWARN	+= -_gcc=-Wno-switch
CERRWARN	+= -_gcc=-Wno-uninitialized
CERRWARN	+= -_gcc=-Wno-type-limits
CERRWARN	+= -_gcc=-Wno-pointer-sign
CERRWARN	+= -_gcc=-Wno-unused-variable
CERRWARN	+= -_gcc=-Wno-unused-function

#
#	Define targets
#
ALL_TARGET	= $(BINARY) $(CONFMOD)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOT_CONFFILE)

#
# Driver depends on MAC and IP
#
LDFLAGS		+= -dy -N misc/mac -N drv/ip

#
# CFLAGS for Sol11-based target
#
CFLAGS	  += -D_USE_DESBALLOC
CFLAGS    += -U_USE_XESBALLOC
CFLAGS    += -U_USE_CPU_PHYSID
CFLAGS	  += -U_USE_GLD_V2
CFLAGS    += -D_USE_GLD_V3
CFLAGS    += -D_USE_GLD_V3_SOL10
CFLAGS    += -D_USE_GLD_V3_SOL11
CFLAGS	  += -D_USE_MAC_PRIV_PROP
CFLAGS    += -D_USE_GLD_V3_PROPS
CFLAGS    += -U_USE_MTU_UPDATE
CFLAGS    += -D_USE_NDD_PROPS

LINTFLAGS += -D_USE_DESBALLOC
LINTFLAGS += -U_USE_XESBALLOC
LINTFLAGS += -U_USE_CPU_PHYSID
LINTFLAGS += -U_USE_GLD_V2
LINTFLAGS += -D_USE_GLD_V3
LINTFLAGS += -D_USE_GLD_V3_SOL10
LINTFLAGS += -D_USE_GLD_V3_SOL11
LINTFLAGS += -D_USE_MAC_PRIV_PROP
LINTFLAGS += -D_USE_GLD_V3_PROPS
LINTFLAGS += -U_USE_MTU_UPDATE
LINTFLAGS += -D_USE_NDD_PROPS

#
#	Default build targets.
#
.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

lint:		$(LINT_DEPS)

modlintlib:	$(MODLINTLIB_DEPS)

clean.lint:	$(CLEAN_LINT_DEPS)

install:	$(INSTALL_DEPS)

#
#	Include common targets.
#
include $(UTSBASE)/intel/Makefile.targ
