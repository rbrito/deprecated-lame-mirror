## $Id$

AUTOMAKE_OPTIONS = foreign $(top_srcdir)/ansi2knr

bin_PROGRAMS = lame @WITH_MP3RTP@ @WITH_MP3X@
EXTRA_PROGRAMS = mp3rtp mp3x

DEFS = @DEFS@ @CONFIG_DEFS@

brhist_sources = brhist.c brhist.h

EXTRA_DIST = $(brhist_sources) README.Win32 mp3x.dsp mp3x.dsw

common_sources = \
	get_audio.c \
	ieeefloat.c \
	lametime.c \
	parse.c \
	portableio.c \
	timestatus.c

noinst_HEADERS = get_audio.h \
	gtkanal.h \
	gpkplotting.h \
	ieeefloat.h \
	lametime.h \
	main.h \
	parse.h \
	portableio.h \
	timestatus.h

EXTRA_lame_SOURCES = $(brhist_sources)
EXTRA_mp3rtp_SOURCES = $(brhist_sources)
EXTRA_mp3x_SOURCES = $(brhist_sources)

if WITH_BRHIST
lame_SOURCES = main.c $(common_sources) $(brhist_sources)
mp3rtp_SOURCES = mp3rtp.c rtp.c $(common_sources) $(brhist_sources)
mp3x_SOURCES = gtkanal.c gpkplotting.c $(common_sources) $(brhist_sources)
else
lame_SOURCES = main.c $(common_sources)
mp3rtp_SOURCES = mp3rtp.c rtp.c $(common_sources)
mp3x_SOURCES = gtkanal.c gpkplotting.c $(common_sources) $(brhist_sources)
endif

CFLAGS = @CFLAGS@ @GTK_CFLAGS@ @FRONTEND_CFLAGS@
LDFLAGS = @LDFLAGS@ @FRONTEND_LDFLAGS@

INCLUDES = -I$(top_srcdir)/libmp3lame -I$(top_srcdir)/include

LDADD = @LDADD@ @FRONTEND_LDADD@ \
	$(top_srcdir)/libmp3lame/libmp3lame.la

mp3x_LDADD = $(LDADD) @GTK_LIBS@
