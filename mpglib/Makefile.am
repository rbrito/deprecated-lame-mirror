## $Id$

AUTOMAKE_OPTIONS = foreign $(top_srcdir)/ansi2knr

INCLUDES = @INCLUDES@ -I$(top_srcdir)/libmp3lame

EXTRA_DIST = README

noinst_LTLIBRARIES = libmpgdecoder.la

libmpgdecoder_la_SOURCES = common.c \
	dct64_i386.c \
	decode_i386.c \
	interface.c \
	layer1.c \
	layer2.c \
	layer3.c \
	tabinit.c

noinst_HEADERS = common.h \
	dct64_i386.h \
	decode_i386.h \
	huffman.h \
	interface.h \
	l2tables.h \
	layer1.h \
	layer2.h \
	layer3.h \
	mpg123.h \
	mpglib.h \
	tabinit.h

LCLINTFLAGS= +posixlib +showsummary +showalluses +whichlib +forcehints

lclint.txt: ${libmpgdecoder_la_SOURCES} ${noinst_HEADERS}
	@lclint ${LCLINTFLAGS} ${INCLUDES} ${DEFS} ${libmpgdecoder_la_SOURCES} 2>&1 >lclint.txt || true

lclint: lclint.txt
	more lclint.txt
