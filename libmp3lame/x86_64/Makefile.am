## $Id$

include $(top_srcdir)/Makefile.am.global

noinst_LTLIBRARIES = liblamecpuroutines.la

##liblamecpuroutines_la_LIBADD = 
##liblamecpuroutines_la_LDFLAGS =

INCLUDES = @INCLUDES@ "-I$(top_srcdir)/mpglib" "-I$(top_builddir)"

DEFS = @DEFS@ @CONFIG_DEFS@

x86_64_sources = quantize_sub.c

if X86_64
liblamecpuroutines_la_SOURCES = $(x86_64_sources)
endif

noinst_HEADERS = lame_intrin.h

EXTRA_liblamecpuroutines_la_SOURCES = $(x86_64_sources)

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

lclint.txt: ${liblamecpuroutines_la_SOURCES} ${noinst_HEADERS}
	@lclint ${LCLINTFLAGS} ${INCLUDES} ${DEFS} ${liblamecpuroutines_la_SOURCES} 2>&1 >lclint.txt || true

lclint: lclint.txt
	more lclint.txt

#$(OBJECTS): libtool
#libtool: $(LIBTOOL_DEPS)
#	$(SHELL) $(top_builddir)/config.status --recheck
