include $(top_srcdir)/Makefile.tool-tests.am

SUBDIRS = .

dist_noinst_SCRIPTS = \
	filter_stderr

floats_CFLAGS = -mno-sse2 -g -O3

EXTRA_DIST = \
	order_guarantee1.stderr.exp order_guarantee1.vgtest \
	order_guarantee2.stderr.exp order_guarantee2.vgtest	\
	order_guarantee3.stderr.exp order_guarantee3.vgtest	\
 	order_guarantee4.stderr.exp order_guarantee4.vgtest	

check_PROGRAMS = \
	order_guarantee1 \
	order_guarantee2	\
	order_guarantee3	\
	order_guarantee4	
