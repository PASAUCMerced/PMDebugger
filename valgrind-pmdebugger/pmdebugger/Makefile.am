# Persistent memory checker.
# Copyright (c) 2014-2015, Intel Corporation.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms and conditions of the GNU General Public License,
# version 2, or (at your option) any later version, as published
# by the Free Software Foundation.
#
# This program is distributed in the hope it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.

include $(top_srcdir)/Makefile.tool.am

EXTRA_DIST = docs/pmc-manual.xml

#----------------------------------------------------------------------------
# Headers
#----------------------------------------------------------------------------

pkginclude_HEADERS = pmdebugger.h

noinst_HEADERS = pmc_include.h

#----------------------------------------------------------------------------
# pmdebugger-<platform>
#----------------------------------------------------------------------------

noinst_PROGRAMS  = pmdebugger-@VGCONF_ARCH_PRI@-@VGCONF_OS@
if VGCONF_HAVE_PLATFORM_SEC
noinst_PROGRAMS += pmdebugger-@VGCONF_ARCH_SEC@-@VGCONF_OS@
endif

PMDEBUGGER_SOURCES_COMMON = \
	pmc_main.c \
	pmc_tx.c \
	pmc_common.c

pmdebugger_@VGCONF_ARCH_PRI@_@VGCONF_OS@_SOURCES      = \
	$(PMDEBUGGER_SOURCES_COMMON)
pmdebugger_@VGCONF_ARCH_PRI@_@VGCONF_OS@_CPPFLAGS     = \
	$(AM_CPPFLAGS_@VGCONF_PLATFORM_PRI_CAPS@)
pmdebugger_@VGCONF_ARCH_PRI@_@VGCONF_OS@_CFLAGS       = \
	$(AM_CFLAGS_@VGCONF_PLATFORM_PRI_CAPS@) -O2
pmdebugger_@VGCONF_ARCH_PRI@_@VGCONF_OS@_DEPENDENCIES = \
	$(TOOL_DEPENDENCIES_@VGCONF_PLATFORM_PRI_CAPS@)
pmdebugger_@VGCONF_ARCH_PRI@_@VGCONF_OS@_LDADD        = \
	$(TOOL_LDADD_@VGCONF_PLATFORM_PRI_CAPS@)
pmdebugger_@VGCONF_ARCH_PRI@_@VGCONF_OS@_LDFLAGS      = \
	$(TOOL_LDFLAGS_@VGCONF_PLATFORM_PRI_CAPS@)
pmdebugger_@VGCONF_ARCH_PRI@_@VGCONF_OS@_LINK = \
	$(top_builddir)/coregrind/link_tool_exe_@VGCONF_OS@ \
	@VALT_LOAD_ADDRESS_PRI@ \
	$(LINK) \
	$(pmdebugger_@VGCONF_ARCH_PRI@_@VGCONF_OS@_CFLAGS) \
	$(pmdebugger_@VGCONF_ARCH_PRI@_@VGCONF_OS@_LDFLAGS)

if VGCONF_HAVE_PLATFORM_SEC
pmdebugger_@VGCONF_ARCH_SEC@_@VGCONF_OS@_SOURCES      = \
	$(PMDEBUGGER_SOURCES_COMMON)
pmdebugger_@VGCONF_ARCH_SEC@_@VGCONF_OS@_CPPFLAGS     = \
	$(AM_CPPFLAGS_@VGCONF_PLATFORM_SEC_CAPS@)
pmdebugger_@VGCONF_ARCH_SEC@_@VGCONF_OS@_CFLAGS       = \
	$(AM_CFLAGS_@VGCONF_PLATFORM_SEC_CAPS@) -O2
pmdebugger_@VGCONF_ARCH_SEC@_@VGCONF_OS@_DEPENDENCIES = \
	$(TOOL_DEPENDENCIES_@VGCONF_PLATFORM_SEC_CAPS@)
pmdebugger_@VGCONF_ARCH_SEC@_@VGCONF_OS@_LDADD        = \
	$(TOOL_LDADD_@VGCONF_PLATFORM_SEC_CAPS@)
pmdebugger_@VGCONF_ARCH_SEC@_@VGCONF_OS@_LDFLAGS      = \
	$(TOOL_LDFLAGS_@VGCONF_PLATFORM_SEC_CAPS@)
pmdebugger_@VGCONF_ARCH_SEC@_@VGCONF_OS@_LINK = \
	$(top_builddir)/coregrind/link_tool_exe_@VGCONF_OS@ \
	@VALT_LOAD_ADDRESS_SEC@ \
	$(LINK) \
	$(pmdebugger_@VGCONF_ARCH_SEC@_@VGCONF_OS@_CFLAGS) \
	$(pmdebugger_@VGCONF_ARCH_SEC@_@VGCONF_OS@_LDFLAGS)
endif

# pmc_main.c contains the helper function for pmdebugger that get called
# all the time. To maximise performance compile with -fomit-frame-pointer
# Primary beneficiary is x86.
pmc_main.o: CFLAGS += -fomit-frame-pointer
