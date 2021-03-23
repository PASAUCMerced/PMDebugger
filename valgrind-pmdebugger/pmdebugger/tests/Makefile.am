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

SUBDIRS = . address_specific epoch_durability epoch_redundant_fence flush_nothing lack_ordering_in_strand logging_related multiple_overwrite no_durability_guarantee order_guarantee redundant_flush redundant_logging

dist_noinst_SCRIPTS = \
	filter_stderr


noinst_HEADERS = common.h

# to trigger 64-bit float store
floats_CFLAGS = -mno-sse2 -g -O3

EXTRA_DIST = 

check_PROGRAMS = 
