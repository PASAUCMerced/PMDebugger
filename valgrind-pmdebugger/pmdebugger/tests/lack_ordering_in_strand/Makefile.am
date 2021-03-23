include $(top_srcdir)/Makefile.tool-tests.am

SUBDIRS = .

dist_noinst_SCRIPTS = \
	filter_stderr

floats_CFLAGS = -mno-sse2 -g -O3

EXTRA_DIST = \
	lack_ordering_in_strand1.stderr.exp lack_ordering_in_strand1.vgtest \
	lack_ordering_in_strand2.stderr.exp lack_ordering_in_strand2.vgtest	

check_PROGRAMS = \
	lack_ordering_in_strand1 \
	lack_ordering_in_strand2	
