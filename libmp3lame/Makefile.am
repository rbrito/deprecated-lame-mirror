### Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign

lib_LIBRARIES = libmp3lame.a

libmp3lame_a_SOURCES = \
	bitstream.c \
	encoder.c \
	fft.c \
        id3tag.c \
        lame.c \
        newmdct.c \
	psymodel.c \
	quantize.c \
	quantize_pvt.c \
	vbrquantize.c \
	reservoir.c \
	tables.c \
	takehiro.c \
	util.c \
        VbrTag.c \
        version.c

INCLUDES = -I../include
include_HEADERS = ../include/lame.h \
	../include/lame-id3tag.h \
	../include/lame-analysis.h
