#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE Versions 3.13.7
# and higher are distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************
#
# Top Level EPICS Makefile
#        by Matthew Needes and Mike Bordua
#
#  Notes:
#    The build, clean, install, and depends "commands" do not have
#         their own dependency lists; they are instead handled by
#         the build.%, clean.%, etc. dependencies.
#
#    However, the release dependencies DOES require a complete
#         install because the release.% syntax is illegal.
#
# $Id$
#

TOP=.
include $(TOP)/config/CONFIG_BASE

DIRS = src config

INSTALL_BIN = $(INSTALL_LOCATION)/bin/$(HOST_ARCH)

#
# this bootstraps in makeMakefile.pl (and others) so that it can
# be used to create the first O.xxxx/Makefile
#
PERL_BOOTSTRAP_SCRIPTS = $(notdir $(wildcard $(TOP)/src/tools/*.pl))
PERL_BOOTSTRAP_SCRIPTS_INSTALL = $(PERL_BOOTSTRAP_SCRIPTS:%=$(INSTALL_BIN)/%)
all host cross inc rebuild clean depends buildInstall :: $(PERL_BOOTSTRAP_SCRIPTS_INSTALL)

RMDIR=$(PERL) $(TOP)/src/tools/rm.pl -rf

include $(TOP)/config/RULES_TOP

release: 
	@echo TOP: Creating Release...
	@./MakeRelease

built_release:
	@echo TOP: Creating Fully Built Release...
	@./MakeRelease -b $(INSTALL_LOCATION)

$(INSTALL_BIN)/%.pl: $(TOP)/src/tools/%.pl
	$(PERL) $(TOP)/src/tools/installEpics.pl -d -m 555 $< $(INSTALL_BIN)

