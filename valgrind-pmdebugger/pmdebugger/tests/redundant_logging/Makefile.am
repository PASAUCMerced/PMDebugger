include $(top_srcdir)/Makefile.tool-tests.am

SUBDIRS = .

dist_noinst_SCRIPTS = \
	filter_stderr

floats_CFLAGS = -mno-sse2 -g -O3

EXTRA_DIST = \
	redundant_logging1.stderr.exp redundant_logging1.vgtest \
	redundant_logging2.stderr.exp redundant_logging2.vgtest	\
	redundant_logging3.stderr.exp redundant_logging3.vgtest	\
 	redundant_logging4.stderr.exp redundant_logging4.vgtest	\
	redundant_logging5.stderr.exp  redundant_logging5.vgtest

check_PROGRAMS = \
	redundant_logging1 \
	redundant_logging2	\
	redundant_logging3	\
	redundant_logging4	\
	redundant_logging5

