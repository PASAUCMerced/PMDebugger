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

include $(top_srcdir)/Makefile.tool-tests.am

SUBDIRS = .

dist_noinst_SCRIPTS = \
	filter_stderr

floats_CFLAGS = -mno-sse2 -g -O3

EXTRA_DIST = \
	redundant_flush1.stderr.exp redundant_flush1.vgtest \
	redundant_flush2.stderr.exp redundant_flush2.vgtest	

check_PROGRAMS = \
	redundant_flush1 \
	redundant_flush2	

