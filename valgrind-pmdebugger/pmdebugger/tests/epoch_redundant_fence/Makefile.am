include $(top_srcdir)/Makefile.tool-tests.am

SUBDIRS = .

dist_noinst_SCRIPTS = \
	filter_stderr

floats_CFLAGS = -mno-sse2 -g -O3

EXTRA_DIST = \
	epoch_redundant_fence1.stderr.exp epoch_redundant_fence1.vgtest \
	epoch_redundant_fence2.stderr.exp epoch_redundant_fence2.vgtest	\
	epoch_redundant_fence3.stderr.exp epoch_redundant_fence3.vgtest	\
 	epoch_redundant_fence4.stderr.exp epoch_redundant_fence4.vgtest	

check_PROGRAMS = \
	epoch_redundant_fence1 \
	epoch_redundant_fence2	\
	epoch_redundant_fence3	\
	epoch_redundant_fence4	
