## $Id$

include $(top_srcdir)/Makefile.am.global

.PHONY: test

SUBDIRS = mpglib libmp3lame frontend Dll debian doc include misc dshow ACM mac

CLEANFILES = testcase.new.mp3

EXTRA_DIST = \
	API \
	DEFINES \
	HACKING \
	INSTALL.configure \
	LICENSE \
	Makefile.B32 \
	Makefile.DJGPP \
	Makefile.MSVC \
	Makefile.unix \
	Makefile.am.global \
	PRESETS.draft \
	README.B32 \
	README.DJGPP \
	README.WINGTK \
	STYLEGUIDE \
	USAGE \
	configMS.h \
	lame.bat \
	lame.dsw \
	lame.spec.in \
	lame.spec \
	lame_projects_vc6.dsp \
	lame_vc6.dsw \
	testcase.mp3 \
	testcase.wav

#
# The differences depend on the used processor architecture, the used
# compiler and the used options for the compiler, so make test may
# show some differences. You should only be concerned if you are a
# LAME developer and the number of differences change after you
# modified the source.
#
testcase.new.mp3: testcase.wav frontend/lame$(EXEEXT)
	time frontend/lame$(EXEEXT) --nores -h $(top_srcdir)/testcase.wav testcase.new.mp3 || $(RM_F) testcase.new.mp3

test: testcase.mp3 testcase.new.mp3
	cmp -l testcase.new.mp3 $(top_srcdir)/testcase.mp3 | wc -l

testupdate: testcase.mp3 testcase.new.mp3
	cp testcase.new.mp3 testcase.mp3

testg: frontend/mp3x$(EXEEXT) $(top_srcdir)/../test/castanets.wav
	frontend/mp3x$(EXEEXT) -h $(top_srcdir)/../test/castanets.wav

update:
	cd $(top_srcdir) && CVS_RSH=ssh cvs -z3 -q update -dAP || true

diff:
	cd $(top_srcdir) && CVS_RSH=ssh cvs -z3 diff -u || true

frontend/lame$(EXEEXT):
	$(MAKE) $(MAKEFLAGS)

frontend/mp3x$(EXEEXT): frontend/lame$(EXEEXT)

