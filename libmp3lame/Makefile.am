## $Id$

include $(top_srcdir)/Makefile.am.global

SUBDIRS = i386 ppc x86_64

lib_LTLIBRARIES = libmp3lame.la

if HAVE_NASM
nasm_ldadd = $(top_builddir)/libmp3lame/@CPUTYPE@/liblameasmroutines.la
else
nasm_ldadd =
endif

if LIB_WITH_DECODER
decoder_ldadd = $(top_builddir)/mpglib/libmpgdecoder.la
else
decoder_ldadd =
endif

libmp3lame_la_LIBADD = $(nasm_ldadd) $(decoder_ldadd)
libmp3lame_la_LDFLAGS = -version-info @LIB_MAJOR_VERSION@:@LIB_MINOR_VERSION@ \
			-no-undefined

INCLUDES = @INCLUDES@ -I$(top_srcdir)/mpglib -I$(top_builddir)

DEFS = @DEFS@ @CONFIG_DEFS@

EXTRA_DIST = libmp3lame_vc6.dsp

libmp3lame_la_SOURCES = \
	bitstream.c \
	gain_analysis.c \
        tags.c \
        lame.c \
        newmdct.c \
	psymodel.c \
	quantize.c \
	set_get.c \
	tables.c \
	takehiro.c \
	util.c \
	mpglib_interface.c

noinst_HEADERS= \
	tags.h \
	bitstream.h \
	gain_analysis.h \
	encoder.h \
	lame-analysis.h \
	machine.h \
	newmdct.h \
	psymodel.h \
	quantize.h  \
	quantize_pvt.h \
	set_get.h \
	tables.h \
	util.h \
	version.h

CLEANFILES = lclint.txt

LCLINTFLAGS= \
	+posixlib \
	+showsummary \
	+showalluses \
	+whichlib \
	+forcehints \
	-fixedformalarray \
	+matchanyintegral \
	-Dlint

lclint.txt: ${libmp3lame_la_SOURCES} ${noinst_HEADERS}
	@lclint ${LCLINTFLAGS} ${INCLUDES} ${DEFS} ${libmp3lame_la_SOURCES} 2>&1 >lclint.txt || true

lclint: lclint.txt
	more lclint.txt

#$(OBJECTS): libtool
#libtool: $(LIBTOOL_DEPS)
#	$(SHELL) $(top_builddir)/config.status --recheck
