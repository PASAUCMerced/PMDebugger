include $(top_srcdir)/Makefile.tool-tests.am

SUBDIRS = .

dist_noinst_SCRIPTS = \
	filter_stderr

floats_CFLAGS = -mno-sse2 -g -O3

EXTRA_DIST = \
	epoch_durability1.stderr.exp epoch_durability1.vgtest \
	epoch_durability2.stderr.exp epoch_durability2.vgtest	\
	epoch_durability3.stderr.exp epoch_durability3.vgtest	\
 	epoch_durability4.stderr.exp epoch_durability4.vgtest	

check_PROGRAMS = \
	epoch_durability1 \
	epoch_durability2	\
	epoch_durability3	\
	epoch_durability4	
