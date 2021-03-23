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

SUBDIRS = . small_stacktrace
dist_noinst_SCRIPTS = \
	filter_stderr

# to trigger 64-bit float store
floats_CFLAGS = -mno-sse2 -g -O3

EXTRA_DIST = \
	cas.stderr.exp cas.vgtest \
	const_store.stderr.exp const_store.vgtest \
	floats.stderr.exp floats.vgtest \
	isa_check.stderr.exp isa_check.vgtest \
	multiple_stores.stderr.exp multiple_stores.vgtest \
	nt_stores.stderr.exp nt_stores.vgtest \
	set_clean.stderr.exp set_clean.vgtest \
	sse_stores.stderr.exp sse_stores.vgtest \
	state_machine2.stderr.exp state_machine2.vgtest \
	state_machine.stderr.exp state_machine.vgtest \
	state_no_flush_align.stderr.exp state_no_flush_align.vgtest \
	store_merge.stderr.exp store_merge.vgtest \
	store_split.stderr.exp store_split.vgtest \
	tmp_store.stderr.exp tmp_store.vgtest \
	trans_cache_overl.stderr.exp trans_cache_overl.vgtest \
	trans_excl.stderr.exp trans_excl.vgtest \
	trans_expl.stderr.exp trans_expl.vgtest \
	trans_impl.stderr.exp trans_impl.vgtest \
	trans_impl_nest.stderr.exp trans_impl_nest.vgtest \
	trans_nest.stderr.exp trans_nest.vgtest \
	trans_no_pmem.stderr.exp trans_no_pmem.vgtest \
	trans_only.stderr.exp trans_only.vgtest 



check_PROGRAMS = \
	cas \
	const_store \
	floats\
	isa_check\
	multiple_stores \
	nt_stores \
	set_clean \
	sse_stores \
	state_machine2 \
	state_machine \
	state_no_flush_align \
	store_merge \
	store_split \
	tmp_store \
	trans_cache_overl \
	trans_excl \
	trans_expl \
	trans_impl \
	trans_impl_nest \
	trans_nest \
	trans_no_pmem \
	trans_only 



	 
