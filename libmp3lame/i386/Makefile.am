## $Id$

AUTOMAKE_OPTIONS = foreign $(top_srcdir)/ansi2knr

DEFS = @DEFS@ @CONFIG_DEFS@ @ENCODER_DEFS@

noinst_LTLIBRARIES = liblameasmroutines.la

noinst_HEADERS = nasm.h

INCLUDES = @INCLUDES@ -I$(top_srcdir)/mpglib/ -I$(top_srcdir)/libmp3lame

SUFFIXES = .nas

EXTRA_liblameasmroutines_la_SOURCES = $(nasm_sources)

nasm_sources = \
	choose_table.nas \
	cpu_feat.nas \
	scalar.nas

CLEANFILES = \
	choose_table.lst \
	cpu_feat.lst \
	scalar.lst

if HAVE_NASM
liblameasmroutines_la_SOURCES = $(nasm_sources)
endif

EXTRA_DIST = \
	fft.nas \
	fft3dn.nas \
	fftfpu.nas \
	fftsse.nas \
	ffttbl.nas

NASM = @NASM@
NASMFLAGS=-f elf -i .
.nas.o:
	$(NASM) $(NASMFLAGS) $< -o $@ -l $<.lst
