# Makefile for LAME 3.xx
#
# LAME is reported to work under:  
# Linux (i86), NetBSD 1.3.2 (StrongARM), FreeBSD (i86)
# Compaq Alpha(OSF, Linux, Tru64 Unix), Sun Solaris, SGI IRIX,
# OS2 Warp, Macintosh PPC, BeOS, Amiga and even VC++ 
# 

# these variables are available on command line:
#
#   make NOUNIXCMD=YES            - don't use unix commands
#   make UNAME=xxxxx ARCH=xxxxx   - specify a type of host
#   make PGM=lame_exp             - specify a name of an executable file
#
# if you have mingw32-gcc, try:
#   make NOUNIXCMD=YES UNAME=MSDOS
#


ifeq ($(NOUNIXCMD),YES)
  UNAME ?= UNKNOWN
  ARCH ?= UNKNOWN_ARCH
else
  UNAME = $(shell uname)
  ARCH = $(shell uname -m)
endif

# generic defaults. OS specific options go in versious sections below
PGM ?= lame
CC = gcc
CC_OPTS =  -O
AR = ar
RANLIB = ranlib
GTK = 
GTKLIBS = 
SNDLIB = -DLAMESNDFILE
LIBSNDFILE =  
LIBS = -lm 
MAKEDEP = -M
BRHIST_SWITCH = 
LIBTERMCAP = 
RM = rm -f

##########################################################################
# -DHAVEMPGLIB compiles the mpglib *decoding* library into libmp3lame
##########################################################################
CPP_OPTS += -DHAVEMPGLIB 

##########################################################################
# -DUSE_LAYER_1/2 enables Layer1 or Layer2 *decoding* abilities 
##########################################################################
CPP_OPTS += -DUSE_LAYER_1 -DUSE_LAYER_2

##########################################################################
# floating point option:
# -DFLOAT8_is_float         most floating point variables are 4 byte
# -DFLOAT8_is_double        most floating point variables are 8 byte (default)
# NOTE: RH: 7/00:  if FLOAT8=float, it breaks resampling and VBR code 
##########################################################################
#CPP_OPTS += -DFLOAT8_is_double



##########################################################################
# Define these in the OS specific sections below to compile in support
# for the Ogg Vorbis audio format (both decoding and encoding)
# 
# VORBIS = -DHAVEVORBIS  -I ../vorbis/include
# VORBIS_LIB = -L ../vorbis/lib -lvorbis
##########################################################################

##########################################################################
# Define these in the OS specific sections below to compile in code 
# for the optional VBR bitrate histogram.  
# Requires ncurses, but libtermcap also works.  
# If you have any trouble, just dont define these
#
# BRHIST_SWITCH = -DBRHIST
# LIBTERMCAP = -lncurses
# LIBTERMCAP = -ltermcap
#
# or, to try and simulate TERMCAP (ANSI), use:
# BRHIST_SWITCH = -DBRHIST -DNOTERMCAP
#
##########################################################################


##########################################################################
# Define these in the OS specific sections below to compile in code for:
#
# SNDLIB =                no file i/o 
# SNDLIB = -DLAMESNDFILE  to use internal LAME soundfile routines 
# SNDLIB = -DLIBSNDFILE   to use Erik de Castro Lopo's libsndfile 
# http://www.zip.com.au/~erikd/libsndfile/
#
# Note: at present, libsndfile does not support input from stdin.  
#
# for example:
#  SNDLIB = -DLIBSNDFILE
#  LIBSNDFILE=-lsndfile 
#  if libsndfile is in a custom location, try:
#  LIBSNDFILE=-L $(LIBSNDHOME) -lsndfile  -I $(LIBSNDHOME)
##########################################################################


##########################################################################
# Define these in the OS specific sections below to compile in code for
# the GTK mp3 frame analyzer
#
# Requires  -DHAVEMPGLIB
# and SNDLIB = -DLAME or -DLIBSNDFILE
#
# GTK = -DHAVEGTK `gtk-config --cflags`
# GTKLIBS = `gtk-config --libs` 
#
##########################################################################




##########################################################################
# LINUX   
##########################################################################
ifeq ($(UNAME),Linux)
#  remove these lines if you dont have GTK, or dont want the GTK frame analyzer
   GTK = -DHAVEGTK `gtk-config --cflags`
   GTKLIBS = `gtk-config --libs` 
# Comment out next 2 lines if you want to remove VBR histogram capability
   BRHIST_SWITCH = -DBRHIST
   LIBTERMCAP = -lncurses
#  uncomment to use LIBSNDFILE
#   SNDLIB = -DLIBSNDFILE
#   LIBSNDFILE=-lsndfile 

# uncomment to compile in Vorbis support
#   VORBIS = -DHAVEVORBIS -I/home/mt/mp3/vorbis/include
#   VORBIS_LIB = -L/home/mt/mp3/vorbis/lib -lvorbis


# suggested for gcc-2.7.x
   CC_OPTS =  -O3 -fomit-frame-pointer -funroll-loops -ffast-math  -finline-functions -Wall
#  CC_OPTS =  -O9 -fomit-frame-pointer -fno-strength-reduce -mpentiumpro -ffast-math -finline-functions -funroll-loops -Wall -malign-double -g -march=pentiumpro -mfancy-math-387 -pipe 

#  for debugging:
#  CC_OPTS =  -UNDEBUG -O -Wall -ggdb -DABORTFP

#  for lots of debugging:
#   CC_OPTS =  -DDEBUG -UNDEBUG  -O -Wall -g -DABORTFP 


#  special noise calculation
#  CPP_OPTS += -DRH_AMP
# these options for gcc-2.95.2 to produce fast code
#   CC_OPTS = \
#	-Wall -O9 -fomit-frame-pointer -march=pentium \
#	-finline-functions -fexpensive-optimizations \
#	-funroll-loops -funroll-all-loops -pipe -fschedule-insns2 \
#	-fstrength-reduce \
#	-malign-double -mfancy-math-387 -ffast-math 


##########################################################################
# LINUX on Digital/Compaq Alpha CPUs
##########################################################################
ifeq ($(ARCH),alpha)

################################################################
#### Check if 'ccc' is in our path
####   if not, use 'gcc'
################################################################
ifeq ($(shell which ccc 2>/dev/null | grep -c ccc),0)

# double is faster than float on Alpha
CC_OPTS =       -O4 -Wall -fomit-frame-pointer -ffast-math -funroll-loops \
                -mfp-regs -fschedule-insns -fschedule-insns2 \
                -finline-functions \
#                -DFLOAT=double
# add "-mcpu=21164a -Wa,-m21164a" to optimize for 21164a (ev56) CPU

################################################################
#### else, use 'ccc'
################################################################
else

# Compaq's C Compiler
CC = ccc

################################################################
#### set 'CC_OPTS = -arch host -tune host' to generate/tune instructions for this machine
####     'CC_OPTS += -migrate -fast -inline speed -unroll 0' tweak to run as fast as possible :)
####     'CC_OPTS += -w0 -Wall' set warning and linking flags
################################################################
CC_OPTS = -arch host -tune host
CC_OPTS += -migrate -fast -inline speed -unroll 0
CC_OPTS += -w0 -Wall


################################################################
#### to debug, uncomment
################################################################
# For Debugging
#CC_OPTS += -g3

################################################################
#### define __DECALPHA__ (i was getting re-declaration warnings
####   in machine.h
################################################################
# Define DEC Alpha
CPP_OPTS += -D__DECALPHA__

# standard Linux libm
#LIBS	=	-lm
# optimized libffm (free fast math library)
#LIBS	=	-lffm
# Compaq's fast math library
LIBS    =       -lcpml
endif  #  gcc or ccc?
endif  #  alpha 
endif  #  linux



##########################################################################
# FreeBSD
##########################################################################
ifeq ($(UNAME),FreeBSD)
#  remove if you do not have GTK or do not want the GTK frame analyzer
   GTK = -DHAVEGTK `gtk12-config --cflags`
   GTKLIBS = `gtk12-config --libs` 
# Comment out next 2 lines if you want to remove VBR histogram capability
   BRHIST_SWITCH = -DBRHIST
   LIBTERMCAP = -lncurses

endif

##########################################################################
# OpenBSD
##########################################################################
ifeq ($(UNAME),OpenBSD)
#  remove if you do not have GTK or do not want the GTK frame analyzer
   GTK = -DHAVEGTK `gtk12-config --cflags`
   GTKLIBS = `gtk12-config --libs` 
# Comment out next 2 lines if you want to remove VBR histogram capability
   BRHIST_SWITCH = -DBRHIST
   LIBTERMCAP = -lncurses
endif


##########################################################################
# SunOS
##########################################################################
ifeq ($(UNAME),SunOS) 
   CC = cc
   CC_OPTS = -O -xCC  	
   MAKEDEP = -xM
endif


##########################################################################
# SGI
##########################################################################
ifeq ($(UNAME),IRIX64) 
   CC = cc
   CC_OPTS = -O3 -woff all 

#optonal:
#   GTK = -DHAVEGTK `gtk-config --cflags`
#   GTKLIBS = `gtk-config --libs`
#   BRHIST_SWITCH = -DBRHIST
#   LIBTERMCAP = -lncurses

endif
ifeq ($(UNAME),IRIX) 
   CC = cc
   CC_OPTS = -O3 -woff all 
endif



##########################################################################
# Compaq Alpha running Dec Unix (OSF)
##########################################################################
ifeq ($(UNAME),OSF1)
   CC = cc
   CC_OPTS = -fast -O3 -std -g3 -non_shared
endif

##########################################################################
# BeOS
##########################################################################
ifeq ($(UNAME),BeOS)
   CC = $(BE_C_COMPILER)
   LIBS =
ifeq ($(ARCH),BePC)
   CC_OPTS = -O9 -fomit-frame-pointer -march=pentium \
   -mcpu=pentium -ffast-math -funroll-loops \
   -fprofile-arcs -fbranch-probabilities
else
   CC_OPTS = -opt all
   MAKEDEP = -make
endif
endif

###########################################################################
# MOSXS (Rhapsody PPC)
###########################################################################
ifeq ($(UNAME),Rhapsody)
   CC = cc
   LIBS =
   CC_OPTS = -O9 -ffast-math -funroll-loops -fomit-frame-pointer
   MAKEDEP = -make 
   
endif
##########################################################################
# OS/2
##########################################################################
# Properly installed EMX runtime & development package is a prerequisite.
# tools I used: make 3.76.1, uname 1.12, sed 2.05, PD-ksh 5.2.13
#
##########################################################################
ifeq ($(UNAME),OS/2)
   SHELL=sh	
   CC = gcc
   CC_OPTS = -O3
   PGM ?= lame.exe
   LIBS =

# I use the following for slightly better performance on my Pentium-II
# using pgcc-2.91.66:
#   CC_OPTS = -O6 -ffast-math -funroll-loops -mpentiumpro -march=pentiumpro

# Comment out next 2 lines if you want to remove VBR histogram capability
   BRHIST_SWITCH = -DBRHIST
   LIBTERMCAP = -ltermcap

# Uncomment & inspect the 2 GTK lines to use MP3x GTK frame analyzer.
# Properly installed XFree86/devlibs & GTK+ is a prerequisite.
# The following works for me using Xfree86/OS2 3.3.5 and GTK+ 1.2.3:
#   GTK = -DHAVEGTK -IC:/XFree86/include/gtk12 -Zmt -D__ST_MT_ERRNO__ -IC:/XFree86/include/glib12 -IC:/XFree86/include
#   GTKLIBS = -LC:/XFree86/lib -Zmtd -Zsysv-signals -Zbin-files -lgtk12 -lgdk12 -lgmodule -lglib12 -lXext -lX11 -lshm -lbsd -lsocket -lm
endif

###########################################################################
# MSDOS/Windows
###########################################################################
ifeq ($(UNAME),MSDOS)
  RM =
  PGM ?= lame.exe
endif

###########################################################################
# AmigaOS
###########################################################################
# Type 'Make ARCH=PPC' for PowerUP and 'Make ARCH=WOS' for WarpOS
#
###########################################################################
ifeq ($(UNAME),AmigaOS)
	CC = gcc -noixemul
	CC_OPTS = -O3 -ffast-math -funroll-loops -m68020-60 -m68881
	BRHIST_SWITCH = -DBRHIST -DNOTERMCAP
	MAKEDEP = -MM
	ifeq ($(ARCH),WOS)
		CC = ppc-amigaos-gcc -warpup
		CC_OPTS = -O3 -ffast-math -fomit-frame-pointer -funroll-loops \
		-mmultiple -mcpu=603e
		AR = ppc-amigaos-ar
		RANLIB = ppc-amigaos-ranlib
		LIBS =
	endif
	ifeq ($(ARCH),PPC)
		CC = ppc-amigaos-gcc
		CC_OPTS = -O3 -ffast-math -fomit-frame-pointer -funroll-loops \
		-mmultiple -mcpu=603e
		AR = ppc-amigaos-ar
		RANLIB = ppc-amigaos-ranlib
		LIBS =
	endif
endif
















# 10/99 added -D__NO_MATH_INLINES to fix a bug in *all* versions of
# gcc 2.8+ as of 10/99.  

CC_SWITCHES = -DNDEBUG -D__NO_MATH_INLINES $(CC_OPTS) $(SNDLIB) $(GTK) \
$(BRHIST_SWITCH) $(VORBIS) 
c_sources_s = \
        brhist.c \
	bitstream.c \
	fft.c \
	get_audio.c \
        id3tag.c \
	ieeefloat.c \
        lame.c \
        newmdct.c \
        parse.c \
	portableio.c \
	psymodel.c \
	quantize.c \
	quantize-pvt.c \
	vbrquantize.c \
	reservoir.c \
	tables.c \
	takehiro.c \
	timestatus.c \
	util.c \
	vorbis_interface.c \
        VbrTag.c \
        version.c \
        mpglib/common.c \
        mpglib/dct64_i386.c \
        mpglib/decode_i386.c \
        mpglib/layer1.c \
        mpglib/layer2.c \
        mpglib/layer3.c \
        mpglib/tabinit.c \
        mpglib/interface.c \
        mpglib/main.c 

ifeq ($(UNAME),MSDOS)
  c_sources = $(subst /,\\,$(c_sources_s))
else
  c_sources = $(c_sources_s)
endif

OBJ = $(c_sources:.c=.o)
DEP = $(c_sources:.c=.d)

gtk_sources = gtkanal.c gpkplotting.c
gtk_obj = $(gtk_sources:.c=.o)
gtk_dep = $(gtk_sources:.c=.d)



NASM = nasm
ASFLAGS=-f elf -i i386/
%.o: %.nas
	$(NASM) $(ASFLAGS) $< -o $@
%.o: %.s
	gcc -c $< -o $@

## use MMX extension. you need nasm and MMX supported CPU.
#CC_SWITCHES += -DMMX_choose_table
#OBJ += i386/choose_table.o

%.o: %.c 
	$(CC) $(CPP_OPTS) $(CC_SWITCHES) -c $< -o $@

%.d: %.c
  ifeq ($(NOUNIXCMD),YES)
	$(CC) $(MAKEDEP)  $(CPP_OPTS) $(CC_SWITCHES)  $< > $@
  else
	$(SHELL) -ec '$(CC) $(MAKEDEP)  $(CPP_OPTS) $(CC_SWITCHES)  $< | sed '\''s;$*.o;& $@;g'\'' > $@'
  endif

all: $(PGM)

$(PGM):	main.o $(gtk_obj) libmp3lame.a 
	$(CC) $(CC_OPTS) -o $(PGM)  main.o $(gtk_obj) -L. -lmp3lame $(LIBS) $(LIBSNDFILE) $(GTKLIBS) $(LIBTERMCAP) $(VORBIS_LIB)

mp3x:	mp3x.o $(gtk_obj) libmp3lame.a
	$(CC) -o mp3x mp3x.o $(gtk_obj) $(OBJ) $(LIBS) $(LIBSNDFILE) $(GTKLIBS) $(LIBTERMCAP) $(VORBIS_LIB)

mp3rtp:	rtp.o mp3rtp.o libmp3lame.a
	$(CC) -o mp3rtp mp3rtp.o rtp.o   $(OBJ) $(LIBS) $(LIBSNDFILE) $(GTKLIBS) $(LIBTERMCAP) $(VORBIS_LIB)

libmp3lame.a:  $(OBJ) Makefile
	$(AR) cr libmp3lame.a  $(OBJ) 
	$(RANLIB) libmp3lame.a


clean:
  ifeq ($(UNAME),MSDOS)
	-del *.o
	-del *.d
	-del *.a
	-del mpglib\*.o
	-del mpglib\*.d
	-del $(PGM)
  else
	-$(RM) $(gtk_obj) $(OBJ) $(DEP) $(PGM) main.o rtp.o mp3rtp mp3rtp.o \
         mp3x.o mp3x libmp3lame.a 
  endif


tags: TAGS

TAGS: ${c_sources}
	etags -T ${c_sources}

ifneq ($(MAKECMDGOALS),clean)
  -include $(DEP)
endif


#
#  testcase.mp3 is a 2926 byte file.  The first number output by
#  wc is the number of bytes which differ between new output
#  and 'official' results.  
#
#  Because of compilier options and effects of roundoff, the 
#  number of bytes which are different may not be zero, but
#  should be at most 30.
#
test: $(PGM)
	./lame  --nores -h testcase.wav testcase.new.mp3
	cmp -l testcase.new.mp3 testcase.mp3 | wc

testg: $(PGM)
	./lame -g -h ../test/castanets.wav
