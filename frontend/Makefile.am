## $Id$

AUTOMAKE_OPTIONS = foreign $(top_srcdir)/ansi2knr

bin_PROGRAMS = lame @WITH_MP3RTP@ @WITH_MP3X@
EXTRA_PROGRAMS = mp3rtp mp3x

brhist_sources = brhist.c brhist.h

EXTRA_DIST = $(brhist_sources) README.Win32 mp3x.dsp mp3x.dsw amiga_mpega.c

DEFS = @DEFS@ @CONFIG_DEFS@

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
mp3x_SOURCES = mp3x.c gtkanal.c gpkplotting.c \
	$(common_sources) $(brhist_sources)
else
lame_SOURCES = main.c $(common_sources)
mp3rtp_SOURCES = mp3rtp.c rtp.c $(common_sources)
mp3x_SOURCES = mp3x.c gtkanal.c gpkplotting.c $(common_sources)
endif

CFLAGS = @CFLAGS@ @GTK_CFLAGS@ @FRONTEND_CFLAGS@
LDFLAGS = @LDFLAGS@ @FRONTEND_LDFLAGS@

INCLUDES = -I$(top_srcdir)/libmp3lame -I$(top_srcdir)/include -I$(top_srcdir)

LDADD = @LDADD@ @FRONTEND_LDADD@ \
	$(top_srcdir)/libmp3lame/libmp3lame.la

mp3x_LDADD = $(LDADD) @GTK_LIBS@

CLEANFILES = lclint.txt

LCLINTFLAGS= +posixlib +showsummary +showalluses +whichlib +forcehints -Dlint

lclint.txt: ${lame_SOURCES} ${noinst_HEADERS}
	@lclint ${LCLINTFLAGS} ${INCLUDES} ${DEFS} ${lame_SOURCES} 2>&1 >lclint.txt || true

lclint: lclint.txt
	more lclint.txt
