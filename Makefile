# Makefile for LAME 3.xx
#
# LAME is reported to work under:  
# Linux (i86), NetBSD 1.3.2 (StrongARM), FreeBSD (i86)
# Compaq Alpha(OSF, Linux, Tru64 Unix), Sun Solaris, SGI IRIX,
# OS2 Warp, Macintosh PPC, BeOS, Amiga and even VC++ 
# 
UNAME = $(shell uname)
ARCH = $(shell uname -m)


# defaults:
PGM = lame
CC = gcc
CC_OPTS = -O   	
GTK = 
GTKLIBS = 
SNDLIB = 
LIBSNDFILE =
LIBS = -lm 
MAKEDEP = -M
BRHIST_SWITCH = 
LIBTERMCAP = 
RM = rm -f

##########################################################################
# Define these to produce a VBR bitrate histogram.  Requires ncurses,
# but libtermcap also works.  
# If you have any trouble, just dont define these
##########################################################################
#BRHIST_SWITCH = -DBRHIST
#LIBTERMCAP = -lncurses
#LIBTERMCAP = -ltermcap


##########################################################################
# define these to use Erik de Castro Lopo's libsndfile 
# http://www.zip.com.au/~erikd/libsndfile/
# otherwise LAME can only read 16bit wav, aiff and pcm inputfiles.
# Note: at present, libsndfile does not support input from stdin.  
##########################################################################
#SNDLIB = -DLIBSNDFILE
#LIBSNDFILE=-lsndfile 
# if libsndfile is in a custom location, try:
#LIBSNDFILE=-L $(LIBSNDHOME) -lsndfile  -I $(LIBSNDHOME)


##########################################################################
# define these to use compile in support for the GTK mp3 frame analyzer
##########################################################################
#GTK = -DHAVEGTK `gtk-config --cflags`
#GTKLIBS = `gtk-config --libs` 


##########################################################################
# LINUX   
##########################################################################
ifeq ($(UNAME),Linux)
#  remove if you do not have GTK or do not want the GTK frame analyzer
   GTK = -DHAVEGTK `gtk-config --cflags`
   GTKLIBS = `gtk-config --libs` 
# Comment out next 2 lines if you want to remove VBR histogram capability
   BRHIST_SWITCH = -DBRHIST
   LIBTERMCAP = -lncurses

#  for debugging:
   CC_OPTS =  -UNDEBUG -O  -Wall -g -DABORTFP 


#  for lots of debugging:
#   CC_OPTS =  -DDEBUG -UNDEBUG  -O -Wall -g -DABORTFP -DNO_LEONID


# suggested for gcc-2.7.x
#   CC_OPTS =    -O3 -fomit-frame-pointer -funroll-loops -ffast-math \
#                    -finline-functions 

# these options were suggested with egcs-990524
#   CC = egcs   #egcc for Debian systems
#   CC_OPTS =    -O9 -fomit-frame-pointer -march=pentium \
#                   -ffast-math -funroll-loops 

# these options were suggested with gcc-2.95.2
#   FEATURES = -DAACS3 -DAAC_TMN_NMT -DRH_masking 
#   CC_OPTS = $(FEATURES) -Wall -O9 -fomit-frame-pointer -march=pentium \
#	-fno-strength-reduce -finline-functions \
#	-ffast-math -malign-double -mfancy-math-387 \
#	-funroll-loops 



##########################################################################
# LINUX on Digital/Compaq Alpha CPUs
##########################################################################
ifeq ($(ARCH),alpha)
# double is faster than float on Alpha
CC_OPTS =       -O4 -Wall -fomit-frame-pointer -ffast-math -funroll-loops \
                -mfp-regs -fschedule-insns -fschedule-insns2 \
                -finline-functions \
                -DFLOAT=double
# standard Linux libm
LIBS	=	-lm  
# optimized libffm (free fast math library)
#LIBS	=	-lffm  
# Compaq's fast math library
#LIBS    =       -lcpml 
endif
endif



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
   PGM = lame.exe
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




# 10/99 added -D__NO_MATH_INLINES to fix a bug in *all* versions of
# gcc 2.8+ as of 10/99.  

CC_SWITCHES = -DNDEBUG -D__NO_MATH_INLINES $(CC_OPTS) $(SNDLIB) $(GTK) $(BRHIST_SWITCH)
c_sources = \
	filters.c \
	formatBitstream.c \
	fft.c \
	get_audio.c \
	l3bitstream.c \
        id3tag.c \
	ieeefloat.c \
        lame.c \
        newmdct.c \
	portableio.c \
	psymodel.c \
	quantize.c \
	quantize-pvt.c \
	vbrquantize.c \
	loopold.c \
	reservoir.c \
	tables.c \
	takehiro.c \
	timestatus.c \
	util.c \
        VbrTag.c \
        version.c \
        gtkanal.c \
        gpkplotting.c \
        mpglib/common.c \
        mpglib/dct64_i386.c \
        mpglib/decode_i386.c \
        mpglib/layer3.c \
        mpglib/tabinit.c \
        mpglib/interface.c \
        mpglib/main.c 

OBJ = $(c_sources:.c=.o)
DEP = $(c_sources:.c=.d)



%.o: %.c 
	$(CC) $(CC_SWITCHES) -c $< -o $@

%.d: %.c
	$(SHELL) -ec '$(CC) $(MAKEDEP)  $(CC_SWITCHES)  $< | sed '\''s;$*.o;& $@;g'\'' > $@'

$(PGM):	main.o $(OBJ) Makefile 
	$(CC) -o $(PGM)  main.o $(OBJ) $(LIBS) $(LIBSNDFILE) $(GTKLIBS) $(LIBTERMCAP)

#$(PGM):	main.o libmp3lame.a Makefile 
#	$(CC) -o $(PGM)  main.o -L. -lmp3lame $(LIBS) $(LIBSNDFILE) $(GTKLIBS) $(LIBTERMCAP)

mp3x:	mp3x.o $(OBJ) Makefile 
	$(CC) -o mp3x mp3x.o  $(OBJ) $(LIBS) $(LIBSNDFILE) $(GTKLIBS) $(LIBTERMCAP)

mp3rtp:	rtp.o mp3rtp.o $(OBJ) Makefile 
	$(CC) -o mp3rtp mp3rtp.o rtp.o   $(OBJ) $(LIBS) $(LIBSNDFILE) $(GTKLIBS) $(LIBTERMCAP)

# compile mp3rtp using the mp3lame library:
#mp3rtp:	rtp.o mp3rtp.o libmp3lame.a Makefile 
#	$(CC) -o mp3rtp mp3rtp.o rtp.o  -L. -lmp3lame $(LIBS) $(LIBSNDFILE) $(GTKLIBS) $(LIBTERMCAP)

libmp3lame.a:  $(OBJ) Makefile
	ar cr libmp3lame.a  $(OBJ) 

clean:
	-$(RM) $(OBJ) $(DEP) $(PGM) main.o rtp.o mp3rtp mp3rtp.o mp3x.o mp3x libmp3lame.a \
                      mp3resample.o mp3resample	

tags: TAGS

TAGS: ${c_sources}
	etags -T ${c_sources}

ifneq ($(MAKECMDGOALS),clean)
  -include $(DEP)
endif


test25: $(PGM)
	./lame  ../test/castanets.wav
	cmp -l ../test/castanets.wav.mp3 ../test/castanets.ref25.mp3 | head
test25h: $(PGM)
	./lame  -h  ../test/castanets.wav
	cmp -l ../test/castanets.wav.mp3 ../test/castanets.ref25h.mp3 | head
test24: $(PGM)
	./lame  ../test/castanets.wav
	cmp -l ../test/castanets.wav.mp3 ../test/castanets.ref24.mp3 | head
test24h: $(PGM)
	./lame  -h  ../test/castanets.wav
	cmp -l ../test/castanets.wav.mp3 ../test/castanets.ref24h.mp3 | head

testr: $(PGM)
	./lame  --nores -h  ../test/castanets.wav
	cmp -l ../test/castanets.wav.mp3 ../test/castanets.refr.mp3 | wc
testv: $(PGM)
	./lame  -v  ../test/castanets.wav
	cmp -l ../test/castanets.wav.mp3 ../test/castanets.refv.mp3 | wc
testg: $(PGM)
	./lame -h -g ../test/castanets.wav

