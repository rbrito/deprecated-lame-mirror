## $Id$

AUTOMAKE_OPTIONS = foreign $(top_srcdir)/ansi2knr

SUBDIRS = i386

lib_LTLIBRARIES = libmp3lame.la

if HAVE_NASM
nasm_ldadd = $(top_srcdir)/libmp3lame/@CPUTYPE@/liblameasmroutines.la
else
nasm_ldadd =
endif

if LIB_WITH_DECODER
decoder_ldadd = $(top_srcdir)/mpglib/libmpgdecoder.la
else
decoder_ldadd =
endif

libmp3lame_la_LIBADD = $(nasm_ldadd) $(decoder_ldadd)
libmp3lame_la_LDFLAGS = -version-info @LIB_MAJOR_VERSION@:@LIB_MINOR_VERSION@

INCLUDES = @INCLUDES@ -I$(top_srcdir)/mpglib/ -I$(top_srcdir)

EXTRA_DIST = \
	debugscalefac.c \
	libmp3lame.dsp

libmp3lame_la_SOURCES = \
        VbrTag.c \
	bitstream.c \
	encoder.c \
	fft.c \
        id3tag.c \
        lame.c \
        newmdct.c \
	psymodel.c \
	quantize.c \
	quantize_pvt.c \
	reservoir.c \
	tables.c \
	takehiro.c \
	util.c \
	vbrquantize.c \
	version.c \
	mpglib_interface.c \
	vorbis_interface.c

noinst_HEADERS= \
	VbrTag.h \
	bitstream.h \
	encoder.h \
	fft.h \
	id3tag.h \
	l3side.h \
	lame-analysis.h \
	machine.h \
	newmdct.h \
	psymodel.h \
	quantize.h  \
	quantize_pvt.h \
	reservoir.h \
	tables.h \
	util.h \
	version.h
