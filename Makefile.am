## $Id$

SUBDIRS = mpglib libmp3lame frontend Dll doc include misc dshow

CLEANFILES = testcase.new.mp3
AUTOMAKE_OPTIONS = foreign ansi2knr

EXTRA_DIST = \
	API HACKING \
	INSTALL.configure \
	LICENSE \
	Lame.vbs \
	Makefile.B32 \
	Makefile.DJGPP \
	Makefile.MSVC \
	PRESETS.draft \
	README.B32 \
	README.DJGPP \
	STYLEGUIDE \
	USAGE \
	auenc \
	lame.bat \
	lame.spec.in \
	lame4dos.bat \
	mlame \
	mlame_corr.c \
	testcase.mp3 \
	testcase.wav

#
#  testcase.mp3 is a 2926 byte file.  The first number output by
#  wc is the number of bytes which differ between new output
#  and 'official' results.  
#
#  Because of compiler options and effects of roundoff, the 
#  number of bytes which are different may not be zero, but
#  should be at most 30.
#
testcase.new.mp3: testcase.wav frontend/lame
	time frontend/lame --nores -h testcase.wav testcase.new.mp3

test: testcase.mp3 testcase.new.mp3
	cmp -l testcase.new.mp3 testcase.mp3 | wc -l

testg: frontend/mp3x ../test/castanets.wav
	frontend/mp3x -h ../test/castanets.wav

update:
	CVS_RSH=ssh cvs -z3 -q update -d -A -P || true

diff:
	CVS_RSH=ssh cvs -z3 diff -u || true

frontend/lame:
	$(MAKE) $(MAKEFLAGS)

frontend/mp3x: frontend/lame

