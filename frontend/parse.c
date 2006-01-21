/*
 *      Command line parsing related functions
 *
 *      Copyright (c) 1999 Mark Taylor
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* $Id$ */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <ctype.h>

#ifdef STDC_HEADERS
# include <stdlib.h>
# include <string.h>
#else
# ifndef HAVE_STRCHR
#  define strrchr rindex
# endif
#endif

#ifdef __OS2__
#include <os2.h>
#define PRTYC_IDLE 1
#define PRTYC_REGULAR 2
#define PRTYD_MINIMUM -31 
#define PRTYD_MAXIMUM 31 
#endif

#include "lame.h"

#include "brhist.h"
#include "parse.h"
#include "main.h"
#include "get_audio.h"
#include "version.h"

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

#if (defined DEBUG) || (defined _DEBUG)
# define INTERNAL_OPTS 1
#else
# define INTERNAL_OPTS LAME_ALPHA_VERSION
#endif

#if INTERNAL_OPTS
# include "encoder.h"
# include "set_get.h"
# include "lame-analysis.h"
int experimentalX = 0;
int experimentalY = 0;
int experimentalZ = 0;
#endif


/* GLOBAL VARIABLES.  set by parse_args() */
/* we need to clean this up */
int keeptag=0;
int silent;                 /* Verbosity */
int disp_brhist;
float update_interval;      /* time status display interval */
int mp3_delay;              /* to adjust the number of samples truncated
                               during decode */
int mp3_delay_set;          /* user specified the value of the mp3 encoder 
                               delay to assume for decoding */

int enc_delay;
int enc_padding;
int disable_wav_header;

int startSample = 0;
int endSample = MAX_U_32_NUM;

sound_file_format input_format;
sound_file_format output_format;
int outputPCMendian=order_nativeEndian;
int in_endian=0;
int in_signed=1;
int in_bitwidth=16;
int decode_only=0;

static int ignore_tag_errors; /* Ignore errors in values passed for tags */
int print_clipping_info;      /* print info whether waveform clips */

/*
 *  Long Filename support for the WIN32 platform
 */
#ifdef WIN32
#include <winbase.h>
#include <windows.h>
static void  
dosToLongFileName( char *fn )
{
    const int MSIZE = PATH_MAX + 1 - 4;  /*  we wanna add ".mp3" later */
    WIN32_FIND_DATAA lpFindFileData;
    HANDLE h = FindFirstFileA( fn, &lpFindFileData );
    if ( h != INVALID_HANDLE_VALUE ) {
        int   a;
        char *q, *p;
        FindClose( h );
        for ( a = 0; a < MSIZE; a++ ) {
            if ( '\0' == lpFindFileData.cFileName[a] ) break;
        }
        if ( a >= MSIZE || a == 0 ) return;
        q = strrchr( fn, '\\' );
        p = strrchr( fn, '/' );
        if ( p-q > 0 ) q = p;
        if ( q == NULL ) q = strrchr(fn,':');
        if ( q == NULL ) strncpy( fn, lpFindFileData.cFileName, a );
        else {
            a += q-fn +1;
            if ( a >= MSIZE ) return;
            strncpy( ++q, lpFindFileData.cFileName, MSIZE-a );
        }
    }
}

BOOL SetPriorityClassMacro(DWORD p)
{
    HANDLE op = OpenProcess(PROCESS_ALL_ACCESS,TRUE,GetCurrentProcessId());
    return SetPriorityClass(op,p);
}

static void setWin32Priority(int Priority)
{
    if (Priority > 3) {
	SetPriorityClassMacro(HIGH_PRIORITY_CLASS);
        printf("==> Priority set to High.\n");
    }
    if (Priority < 3) {
	SetPriorityClassMacro(IDLE_PRIORITY_CLASS);
        printf("==> Priority set to Low.\n");
    }
}
#endif

#if defined(__OS2__)
/* OS/2 priority functions */
static int setOS2Priority(int Priority)
{
    int rc;

    switch(Priority) {
 
    case 0:
        rc = DosSetPriority(
             0,                      /* Scope: only one process */
             PRTYC_IDLE,             /* select priority class (idle, regular, etc) */
             0,                      /* set delta */
             0);                     /* Assume current process */
        printf("==> Priority set to 0 (Low priority).\n");
        break;

    case 1:
        rc = DosSetPriority(
             0,                      /* Scope: only one process */
             PRTYC_IDLE,             /* select priority class (idle, regular, etc) */
             PRTYD_MAXIMUM,          /* set delta */
             0);                     /* Assume current process */
        printf("==> Priority set to 1 (Medium priority).\n");
        break;

    case 2:
        rc = DosSetPriority(
             0,                      /* Scope: only one process */
             PRTYC_REGULAR,          /* select priority class (idle, regular, etc) */
             PRTYD_MINIMUM,          /* set delta */
             0);                     /* Assume current process */
        printf("==> Priority set to 2 (Regular priority).\n");
        break;
        
    case 3:
        rc = DosSetPriority(
             0,                      /* Scope: only one process */
             PRTYC_REGULAR,          /* select priority class (idle, regular, etc) */
             0,                      /* set delta */
             0);                     /* Assume current process */
        printf("==> Priority set to 3 (High priority).\n");
        break;

    case 4:
        rc = DosSetPriority(
             0,                      /* Scope: only one process */
             PRTYC_REGULAR,          /* select priority class (idle, regular, etc) */
             PRTYD_MAXIMUM,          /* set delta */
             0);                     /* Assume current process */
        printf("==> Priority set to 4 (Maximum priority). I hope you enjoy it :)\n");
        break;
     
    default:
        printf("==> Invalid priority specified! Assuming idle priority.\n");
    }
   

    return 0;
}
#endif


/************************************************************************
*
* license/version
*
* PURPOSE:  Writes version and license to the file specified by fp
*
************************************************************************/

void
print_version (FILE* const fp)
{
    fprintf(fp, "LAME version %s (%s)\n",
	    get_lame_version (), get_lame_url ());
#ifdef LIBSNDFILE
    fprintf (fp, "frontend built with libsndfile\n");
#endif

    fprintf (fp, "\n");
    if (LAME_ALPHA_VERSION)
	fprintf (fp, "warning: alpha versions should be used for testing only\n\n");
}

/* print version & license */
static int
print_license (FILE* const fp)
{
    fprintf ( fp, 
              "Can I use LAME in my commercial program?\n"
              "\n"
              "Yes, you can, under the restrictions of the LGPL.  In particular, you\n"
              "can include a compiled version of the LAME library (for example,\n"
              "lame.dll) with a commercial program.  Some notable requirements of\n"
              "the LGPL:\n"
              "\n"
              "1. In your program, you cannot include any source code from LAME, with\n"
              "   the exception of files whose only purpose is to describe the library\n"
              "   interface (such as lame.h).\n"
              "\n"
              "2. Any modifications of LAME must be released under the LGPL.\n"
              "   The LAME project (www.mp3dev.org) would appreciate being\n"
              "   notified of any modifications.\n"
              "\n"
              "3. You must give prominent notice that your program is:\n"
              "      A. using LAME (including version number)\n"
              "      B. LAME is under the LGPL\n"
              "      C. Provide a copy of the LGPL.  (the file COPYING contains the LGPL)\n"
              "      D. Provide a copy of LAME source, or a pointer where the LAME\n"
              "         source can be obtained (such as www.mp3dev.org)\n"
              "   An example of prominent notice would be an \"About the LAME encoding engine\"\n"
              "   button in some pull down menu within the executable of your program.\n"
              "\n"
              "4. If you determine that distribution of LAME requires a patent license,\n"
              "   you must obtain such license.\n"
              "\n"
              "\n"
              "*** IMPORTANT NOTE ***\n"
              "\n"
              "The decoding functions provided in LAME use the mpglib decoding engine which\n"
              "is under the GPL.  They may not be used by any program not released under the\n"
              "GPL unless you obtain such permission from the MPG123 project (www.mpg123.de).\n"
              "\n" );
    return 0;
}


/************************************************************************
*
* usage
*
* PURPOSE:  Writes command line syntax to the file specified by fp
*
************************************************************************/

int
usage(FILE* const fp, const char* ProgramName )
{
    fprintf ( fp,
              "usage: %s [options] <infile> [outfile]\n"
              "\n"
              "    <infile> and/or <outfile> can be \"-\", which means stdin/stdout.\n"
              "\n"
              "Try:\n"
              "     \"%s --help\"           for general usage information\n" 
              " or:\n"
              "     \"%s --longhelp\"\n"
              "  or \"%s -?\"              for a complete options list\n\n",
              ProgramName, ProgramName, ProgramName, ProgramName ); 
    return 0;
}


/************************************************************************
*
* usage
*
* PURPOSE:  Writes command line syntax to the file specified by fp
*           but only the most important ones, to fit on a vt100 terminal
*
************************************************************************/

static int
short_help (lame_t gfp, FILE* const fp, const char* ProgramName )  /* print short syntax help */
{
    fprintf ( fp,
              "usage: %s [options] <infile> [outfile]\n"
              "\n"
              "    <infile> and/or <outfile> can be \"-\", which means stdin/stdout.\n"
              "\n"
              "RECOMMENDED:\n"
              "    lame -h input.wav output.mp3\n"
              "\n"
              "OPTIONS:\n"
              "   -b <bitrate>      set the bitrate, default 128 kbps\n"
              "   -f                fast mode (lower quality)\n"
              "   -h                higher quality, but a little slower.  Recommended.\n"
              "   -m <mode>         (s)tereo, (j)oint, (m)ono or (a)uto\n"
              "                     default is (j)oint for stereo, (m) for mono\n"
              "   -V <n>            quality setting for VBR(quality/file size ratio).\n"
	      "                     default n=%i\n"
              "\n",
              ProgramName, lame_get_VBR_q(gfp) );
 
    return 0;
}

/************************************************************************
*
* usage
*
* PURPOSE:  Writes command line syntax to the file specified by fp
*
************************************************************************/

static void  wait_for ( FILE* const fp, int lessmode )
{
    if ( lessmode ) {
        fflush ( fp ); 
        getchar (); 
    } else {
        fprintf ( fp, "\n" );
    }
    fprintf ( fp, "\n" );
}

static int
long_help (lame_t gfc, FILE* const fp, const char* ProgramName, int lessmode )  /* print long syntax help */
{
    fprintf ( fp,
	      "usage: %s [options] <infile> [outfile]\n"
              "\n"
              "    <infile> and/or <outfile> can be \"-\", which means stdin/stdout.\n"
              "\n"
              "RECOMMENDED:\n"
              "    lame -h input.wav output.mp3\n"
              "\n"
              "OPTIONS:\n"
              "  Input options:\n"
              "    -r              input is raw pcm\n"
              "    -x              force byte-swapping of input\n"
              "    -s sfreq        sampling frequency of input file (kHz) - default 44.1 kHz\n"
              "    --bitwidth w    input bit width is w (default 16)\n"
              "    --signed        input is signed (default)\n"
              "    --unsigned      input is unsigned\n"
              "    --little-endian input is little-endian (default from host)\n"
              "    --big-endian    input is big-endian (default from host)\n"
              "    --mp1input      input file is a MPEG Layer I   file\n"
              "    --mp2input      input file is a MPEG Layer II  file\n"
              "    --mp3input      input file is a MPEG Layer III file\n"
 	      "    --nogap <file1> <file2> <...>\n"
 	      "                    gapless encoding for a set of contiguous files\n"
 	      "    --nogapout <dir>\n"
 	      "                    output dir for gapless encoding (must precede --nogap)\n"
 	      "    --nogaptags     allow the use of VBR tags in gapless encoding"
              , ProgramName );

    wait_for ( fp, lessmode );
    fprintf ( fp,
              "  Operational options:\n"
              "    -m <mode>       (s)tereo, (j)oint, (f)orce, or (m)ono \n"
              "                    default is (j)oint for stereo, (m) for mono\n"
              "                    force = force ms_stereo on all frames.\n"
              "    -a              downmix from stereo to mono file for mono encoding\n"
              "    --freeformat    produce a free format bitstream\n"
              "    --decode        input=mp3 file, output=wav\n"
              "    -t              disable writing wav header when using --decode\n"
              "    --comp  <arg>   choose bitrate to achive a compression ratio of <arg>\n"
              "    --scale <arg>   scale input (multiply PCM data) by <arg>\n"
	      "    --scale-l <arg> scale channel 0 (left) input (multiply PCM data) by <arg>\n"
              "    --scale-r <arg> scale channel 1 (right) input (multiply PCM data) by <arg>\n"
              );

    fprintf ( fp,
              "    --noreplaygain         disable ReplayGain analysis (default)\n"
              "    --replaygain-fast      compute RG fast but slightly inaccurately\n"
#ifdef HAVE_MPGLIB
              "    --replaygain-accurate  compute RG more accurately and find the peak sample\n"
              "    --clipdetect    enable --replaygain-accurate and print a message whether\n"
              "                    clipping occurs and how far the waveform is from full scale\n"
#endif
	);

    wait_for ( fp, lessmode );
    fprintf ( fp,
              "  Verbosity:\n"
              "    --disptime <arg>print progress report every arg seconds\n"
              "    -S              don't print progress report, VBR histograms\n"
              "    --nohist        disable VBR histogram display\n"
              "    --silent        don't print anything on screen\n"
              "    --quiet         don't print anything on screen\n"
              "    --brief         print more useful information\n"
              "    --verbose       print a lot of useful information\n"
              "\n"
              "  Noise shaping & psycho acoustic algorithms:\n"
              "    -q <arg>        <arg> = 0...9.  Default  -q 5 \n"
              "                    -q 0:  Highest quality, very slow \n"
              "                    -q 9:  Poor quality, but fast \n"
              "    -h              Same as -q 2.   Recommended.\n"
              "    -f              Same as -q 7.   Fast, ok quality\n"
	      "\n"
              "    --substep n       use pseudo substep noise shaping method\n"
	      "                      0: no\n"
	      "                      1: only on the long blocks\n"
	      "                      2: only on the short blocks\n"
	      "                      3: on all blocks\n"
              "    --sfscale [n]     try to use scalefactor scale(n=1, default) or not(n=0).\n"
              "    --sbgain [n]      try to use subblock gain (n=1, default) or not(n=0)\n"
              );

    wait_for ( fp, lessmode );
    fprintf ( fp,
"  CBR (constant bitrate, the default) options:\n"
"    -b <bitrate>    set the bitrate in kbps, default 128 kbps\n"
"    --cbr           enforce use of constant bitrate\n"
"\n"
"  ABR options:\n"
"    --abr <bitrate> specify average bitrate desired (instead of quality)\n"
"\n"
"  VBR options:\n"
"    -v              use variable bitrate (VBR)\n"
"    -V n            quality setting for VBR.  default n=%i\n"
"                    0=high quality,bigger files. 9=smaller files\n"
"    -b <bitrate>    specify minimum allowed bitrate, default  32 kbps\n"
"    -B <bitrate>    specify maximum allowed bitrate, default 320 kbps\n"
"    -t              disable writing LAME Tag\n"
              , lame_get_VBR_q(gfc) );

    wait_for ( fp, lessmode );  
    fprintf ( fp,
              "  ATH related:\n"
              "    --noath         turns ATH down to a flat noise floor\n"
              "    --athonly       use ATH only\n"
              "    --athshort      use ATH only for short blocks\n"
              "    --athlower x    lowers ATH by x dB\n"
              "    --athaa-sensitivity x  activation offset in -/+ dB for ATH auto-adjustment\n" 
              "\n"
              "  PSY related:\n"
              "   --shortthreshold x,y  short block switching threshold\n"
	      "                         x for L/R/M channel, y for S channel\n"
              "   --mixedblock    use mixed blocks(experimental)\n"
              "   --nsmsfix <arg> M/S switching tuning\n"
              "   --tune-bass x   adjust masking for  0 -  6 bark\n"
              "   --tune-alto x   adjust masking for  6 - 12 bark\n"
              "   --tune-treble x adjust masking for 12 - 18 bark\n"
              "   --tune-sfb21 x  adjust masking for 18 -    bark\n"
              "   --interch x     adjust inter-channel masking ratio\n"
              "   --is-ratio x    adjust intensity stereo usage ratio\n"
	      "   --reduce-side x   narrowen the stereo image(0.0<x<1.0)\n"
	      "   --narrowen-stereo x    narrowen the stereo image (0.0<x<1.0)\n"
            );

    wait_for ( fp, lessmode );  

    fprintf ( fp,
              "  experimental switches:\n"
              "    -X, -Y, -Z      daily changing switches. don't use\n"
            );

    wait_for ( fp, lessmode );  

    fprintf ( fp,
              "  MP3 header/stream options:\n"
              "    -e <emp>        de-emphasis n/5/c  (obsolete)\n"
              "    -c              mark as copyright\n"
              "    -o              mark as non-original\n"
              "    -p              error protection.  adds 16 bit checksum to every frame\n"
              "                    (the checksum is computed correctly)\n"
              "    --nores         disable the bit reservoir\n"
              "    --strictly-enforce-ISO   comply as much as possible to ISO MPEG spec\n"
              "    --sameblock     do not set the different block type between channel 0 and 1\n"
              "                    This may help playback problem on some BUGGY mp3 decoders\n"
              "\n"
              "  Filter options:\n"
              "    -k              keep ALL frequencies (disables all filters),\n"
              "                    Can cause ringing and twinkling\n"
              "  --lowpass <freq>        frequency(kHz), lowpass filter cutoff above freq\n"
              "  --lowpass-width <freq>  frequency(kHz) - default 15%% of lowpass freq\n"
              "  --highpass <freq>       frequency(kHz), highpass filter cutoff below freq\n"
              "  --highpass-width <freq> frequency(kHz) - default 15%% of highpass freq\n"
              "  --resample <sfreq>  sampling frequency of output file(kHz)- default=automatic\n"
               );
  
    wait_for ( fp, lessmode );
    fprintf ( fp,
              "  ID3 tag options:\n"
              "    --tt <title>    audio/song title (max 30 chars for version 1 tag)\n"
              "    --ta <artist>   audio/song artist (max 30 chars for version 1 tag)\n"
              "    --tl <album>    audio/song album (max 30 chars for version 1 tag)\n"
              "    --ty <year>     audio/song year of issue (1 to 9999)\n"
              "    --tc <comment>  user-defined text (max 30 chars for v1 tag, 28 for v1.1)\n"
              "    --tn <track>    audio/song track number (1 to 255, creates v1.1 tag)\n"
              "         <track>/<totaltrack> track number and total tracks.(creates v2 tag)\n"
              "    --tg <genre>    audio/song genre (name or number in list)\n"
              "    --add-id3v2     force addition of version 2 tag\n"
              "    --id3v1-only    add only a version 1 tag\n"
              "    --id3v2-only    add only a version 2 tag\n"
              "    --space-id3v1   pad version 1 tag with spaces instead of nulls\n"
              "    --pad-id3v2     pad version 2 tag with extra 128 bytes\n"
              "    --genre-list    print alphabetically sorted ID3 genre list and exit\n"
              "    --u8            command line arguments are in UTF-8\n"
              "    --keeptag       keep ID3 tag of input file(mp3 only)\n"
              "    --ignore-tag-errors  ignore errors in values passed for tags\n"
              "\n"
              "    Note: A version 2 tag will NOT be added unless one of the input fields\n"
              "    won't fit in a version 1 tag (e.g. the title string is longer than 30\n"
              "    characters), or the '--add-id3v2' or '--id3v2-only' options are used,\n"
              "    or output is redirected to stdout.\n"
#if defined(WIN32)
              "\n\nMS-Windows-specific options:\n"
              "    --priority <type>     sets the process priority:\n"
              "                           0,1 = Low priority (IDLE_PRIORITY_CLASS)\n"
              "                           2 = normal priority (NORMAL_PRIORITY_CLASS, default)\n"
              "                           3,4 = High priority (HIGH_PRIORITY_CLASS))\n"
#endif
#if defined(__OS2__)
              "\n\nOS/2-specific options:\n"
              "    --priority <type>     sets the process priority:\n"
              "                               0 = Low priority (IDLE, delta = 0)\n"
              "                               1 = Medium priority (IDLE, delta = +31)\n"
              "                               2 = Regular priority (REGULAR, delta = -31)\n"
              "                               3 = High priority (REGULAR, delta = 0)\n"
              "                               4 = Maximum priority (REGULAR, delta = +31)\n"
#endif

              );

#if defined(HAVE_NASM)
    wait_for ( fp, lessmode );  
    fprintf ( fp,
              "  Platform specific:\n"
              "    --noasm <instructions> disable assembly optimizations for mmx/3dnow/sse\n"
                );
    wait_for ( fp, lessmode );  
#endif

    display_bitrates ( fp );

    return 0;
}

static void  display_bitrate ( FILE* const fp, const char* const version, const int div, const int index )
{
    int  i;
    
    fprintf ( fp,
              "\nMPEG-%-3s layer III sample frequencies (kHz):  %2d  %2d  %g\n"
              "bitrates (kbps):", 
              version, 32/div, 48/div, 44.1/div );
    for (i = 1; i <= 14; i++ )          /* 14 numbers are printed, not 15, and it was a bug of me */
        fprintf ( fp, " %2i", bitrate_table [index] [i] );
    fprintf ( fp, "\n" );
}

int  display_bitrates ( FILE* const fp )
{
    display_bitrate ( fp, "1"  , 1, 1 );
    display_bitrate ( fp, "2"  , 2, 0 );
    display_bitrate ( fp, "2.5", 4, 0 );
    fprintf ( fp, "\n" );
    fflush  ( fp );
    return 0;
}


/************************************************************************
*
* usage
*
* PURPOSE:  Writes presetting info to #stdout#
*
************************************************************************/


static void  presets_longinfo_dm ( FILE* msgfp )
{
    fprintf ( msgfp,
	      "\n"
	      "  --preset medium   => -V 6\n"
	      "  --preset standard => -V 4\n"
	      "  --preset extreme  => -V 2\n"
	      "  --preset insane   => -b 320\n"
	      "  --preset XXX      => --abr XXX\n"
	      "  --preset cbr XXX  => -b XXX\n"
	      "\n"
	      "  --preset phone    => --abr 16 -mm\n"
	      "  --preset phon+/lw/mw-eu/sw => --abr 24 -mm\n"
	      "  --preset mw-us    => --abr 40 -mm\n"
	      "  --preset voice    => --abr 56 -mm\n"
	      "  --preset fm/radio/tape => --abr 112\n"
	      "  --preset hifi     => --abr 160\n"
	      "  --preset cd       => --abr 192\n"
	      "  --preset studio   => --abr 256\n");
}


/*  some presets thanks to Dibrom
 */
static int  presets_set( lame_t gfp, int cbr, const char* preset_name, const char* ProgramName )
{
    int mono = 0, rate;

    fprintf(stdout,
	    "\n"
	    "The --preset switches are obsoleted and discouraged to use.\n"
	    "All the presets are only alias to the simple VBR/ABR/CBR mode\n"
	);

    if (strcmp(preset_name, "help") == 0) {
        presets_longinfo_dm( stdout );
        return -1;
    }

    if (strcmp(preset_name, "phone") == 0) {
        preset_name = "16";
        mono = 1;
    }
    if ( (strcmp(preset_name, "phon+") == 0) ||
         (strcmp(preset_name, "lw") == 0) ||
         (strcmp(preset_name, "mw-eu") == 0) ||
         (strcmp(preset_name, "sw") == 0)) {
        preset_name = "24";
        mono = 1;
    }
    if (strcmp(preset_name, "mw-us") == 0) {
        preset_name = "40";
        mono = 1;
    }
    if (strcmp(preset_name, "voice") == 0) {
        preset_name = "56";
        mono = 1;
    }
    if (strcmp(preset_name, "fm") == 0) {
        preset_name = "112";
    }
    if ( (strcmp(preset_name, "radio") == 0) ||
         (strcmp(preset_name, "tape") == 0)) {
        preset_name = "112";
    }
    if (strcmp(preset_name, "hifi") == 0) {
        preset_name = "160";
    }
    if (strcmp(preset_name, "cd") == 0) {
        preset_name = "192";
    }
    if (strcmp(preset_name, "studio") == 0) {
        preset_name = "256";
    }
    if (strcmp(preset_name, "insane") == 0) {
	preset_name = "320";
    }

    if (strcmp(preset_name, "dm-radio") == 0
	|| strcmp(preset_name, "radio") == 0) {
	lame_set_VBR(gfp, vbr_default);
	lame_set_VBR_q(gfp, 7);
	return 0;
    }

    if (strcmp(preset_name, "portable") == 0) {
	lame_set_VBR(gfp, vbr_default);
	lame_set_VBR_q(gfp, 6);
	return 0;
    }

    if (strcmp(preset_name, "dm-medium") == 0
	|| strcmp(preset_name, "medium") == 0) {
	lame_set_VBR(gfp, vbr_default);
	lame_set_VBR_q(gfp, 5);
	return 0;
    }

    if (strcmp(preset_name, "standard") == 0) {
	lame_set_VBR(gfp, vbr_default);
	lame_set_VBR_q(gfp, 4);
	return 0;
    }

    if (strcmp(preset_name, "extreme") == 0){
	lame_set_VBR(gfp, vbr_default);
	lame_set_VBR_q(gfp, 2);
	return 0;
    }

    /* Generic ABR Preset */
    rate = atoi(preset_name);
    if (8 <= rate && rate <= 320) {
	if (cbr == 1)
	    lame_set_VBR(gfp, vbr_off);
	else
	    lame_set_VBR(gfp, vbr_abr);
	
	if (mono == 1 )
	    lame_set_mode(gfp, MONO);
	
	lame_set_brate(gfp, rate);
	return 0;
    }

    fprintf(stderr,
	    "Error: You did not enter a valid profile and/or options with --preset\n"
	    "For further information try: \"%s --preset help\"\n"
	    , ProgramName);
    return -1;
}

static void genre_list_handler (int num,const char *name, void *cookie)
{
    (void)cookie;
    printf("%3d %s\n", num, name);
}


/************************************************************************
*
* parse_args
*
* PURPOSE:  Sets encoding parameters to the specifications of the
* command line.  Default settings are used for parameters
* not specified in the command line.
*
* If the input file is in WAVE or AIFF format, the sampling frequency is read
* from the AIFF header.
*
* The input and output filenames are read into #inpath# and #outpath#.
*
************************************************************************/

/* would use real "strcasecmp" but it isn't portable */
static int local_strcasecmp ( const char* s1, const char* s2 )
{
    unsigned char c1;
    unsigned char c2;
    
    do {
        c1 = tolower(*s1);
        c2 = tolower(*s2);
        if (!c1) {
            break;
        }
        ++s1;
        ++s2;
    } while (c1 == c2);
    return c1 - c2;
}


/*
 * LAME is a simple frontend which just uses the file extension
 * to determine the file type.  Trying to analyze the file
 * contents is well beyond the scope of LAME and should not be added.
 */
static int filename_to_type ( const char* FileName )
{
    int len = strlen (FileName);
    if (len >= 4) {
	FileName += len-4;
	if ( 0 == local_strcasecmp ( FileName, ".mpg" ) ) return sf_mp1;
	if ( 0 == local_strcasecmp ( FileName, ".mp1" ) ) return sf_mp1;
	if ( 0 == local_strcasecmp ( FileName, ".mp2" ) ) return sf_mp2;
	if ( 0 == local_strcasecmp ( FileName, ".mp3" ) ) return sf_mp3;
	if ( 0 == local_strcasecmp ( FileName, ".wav" ) ) return sf_wave;
	if ( 0 == local_strcasecmp ( FileName, ".raw" ) ) return sf_raw;
    }
    return sf_unknown;
}

static int resample_rate ( double freq )
{
    if (freq >= 1.e3) freq *= 1.e-3;

    switch ( (int)freq ) {
    case  8: return  8000;
    case 11: return 11025;
    case 12: return 12000;
    case 16: return 16000;
    case 22: return 22050;
    case 24: return 24000;
    case 32: return 32000;
    case 44: return 44100;
    case 48: return 48000;
    default: fprintf(stderr,"Illegal resample frequency: %.3f kHz\n", freq );
             return 0;
    }
}

/* Ugly, NOT final version */

#define T_IF(str)            if ( 0 == local_strcasecmp (token,str) )
#define T_ELIF(str)          else if ( 0 == local_strcasecmp (token,str) )
#define T_ELIF_INTERNAL(str) else if (INTERNAL_OPTS && (0 == local_strcasecmp (token,str)))
#define T_ELIF2(str1,str2)   else if ( 0 == local_strcasecmp (token,str1)  ||  0 == local_strcasecmp (token,str2) )
#define T_ELSE               else

int  parse_args (lame_t gfp, int argc, char** argv, 
		 char* const inPath, char* const outPath,
		 char **nogap_inPath, int *num_nogap)
{
    int         err = 0;
    int         input_file=0;  /* set to 1 if we parse an input file name */
    int         i;
    int         autoconvert  = 0;
    double      val;
    int         nogap=0;
    int         nogap_tags=0;  /* set to 1 to use VBR tags in NOGAP mode */
    int		count_nogap=0;
    const char* ProgramName=argv[0]; 

    inPath [0] = '\0';   
    outPath[0] = '\0';
    /* turn on display options. user settings may turn them off below */
    silent   = 0;
    disp_brhist = 1;
    ignore_tag_errors = 0;
    enc_padding=-1;
    enc_delay=-1;
    mp3_delay = 0;   
    mp3_delay_set=0;
    disable_wav_header=0;
    print_clipping_info = 0;

    /* process args */
    for (i = err = 0; ++i < argc && !err;) {
	char   c;
	char*  token;
	char*  arg;
	char*  nextArg = i+1 < argc  ?  argv[i+1]  :  "";
	int    argUsed = 0;

	token = argv[i];
	if (*token++ != '-' || *token == 0) {
	    if (nogap) {
		if (num_nogap && count_nogap < *num_nogap) {
		    strncpy(nogap_inPath[count_nogap++], argv[i], PATH_MAX+1);
                    input_file=1;
		} else {
		    /* sorry, calling program did not allocate enough space */
		    fprintf(stderr,"Error: 'nogap option'.  Calling program does not allow nogap option, or\nyou have exceeded maximum number of input files for the nogap option\n");
		    *num_nogap=-1;
		    return -1;
		}
            } else {
		/* normal options:   inputfile  [outputfile], and
                   either one can be a '-' for stdin/stdout */
		if (inPath [0] == '\0') {
		    strncpy(inPath , argv[i], PATH_MAX + 1);
                    input_file=1;
                } else {
                    if (outPath[0] == '\0') 
                        strncpy(outPath, argv[i], PATH_MAX + 1);
                    else {
			fprintf(stderr,"%s: excess arg %s\n", ProgramName, argv[i]);
			err = 1;
                    }
		}
	    }
	    continue;
	}

	if (*token == '-') { /* GNU style */
	    token++;

	    T_IF ("resample") {
		argUsed = 1;
		lame_set_out_samplerate(gfp, resample_rate(atof(nextArg)));
	    }
	    T_ELIF ("cbr") {
		lame_set_VBR(gfp, vbr_off);
	    }
	    T_ELIF ("abr") {
		argUsed=1;
		lame_set_VBR(gfp,vbr_abr); 
		lame_set_VBR_mean_bitrate_kbps(gfp,atoi(nextArg));
		/* values larger than 8000 are bps (like Fraunhofer),
		   so it's strange to get 320000 bps MP3 when specifying 8000 bps MP3 */
		if ( lame_get_VBR_mean_bitrate_kbps(gfp) >= 8000 )
		    lame_set_VBR_mean_bitrate_kbps(gfp,( lame_get_VBR_mean_bitrate_kbps(gfp) + 500 ) / 1000);

		lame_set_VBR_mean_bitrate_kbps(gfp,Min(lame_get_VBR_mean_bitrate_kbps(gfp), 320)); 
		lame_set_VBR_mean_bitrate_kbps(gfp,Max(lame_get_VBR_mean_bitrate_kbps(gfp),8)); 
	    }
	    T_ELIF ("bitwidth") {
		argUsed=1;
		in_bitwidth=atoi(nextArg);
	    }
	    T_ELIF ("signed") {
		in_signed=1;
	    }
	    T_ELIF ("unsigned") {
		in_signed=0;
	    }
	    T_ELIF ("little-endian") {
		outputPCMendian = in_endian = order_littleEndian;
	    }
	    T_ELIF ("big-endian") {
		outputPCMendian = in_endian = order_bigEndian;
	    }
	    T_ELIF ("mp1input") {
		input_format=sf_mp1;
	    }
	    T_ELIF ("mp2input") {
		input_format=sf_mp2;
	    }
	    T_ELIF ("mp3input") {
		input_format=sf_mp3;
	    }
	    T_ELIF ("keeptag") {
		keeptag=1;
	    }
	    T_ELIF_INTERNAL ("mixedblock") {
		lame_set_use_mixed_blocks( gfp, 2);
	    }
	    T_ELIF_INTERNAL ("allshort") {
		lame_set_short_threshold(gfp, 0.0f);
	    }
	    T_ELIF_INTERNAL ("shortthreshold") {
		if (sscanf(nextArg, "%lf", &val) == 1) {
		    lame_set_short_threshold(gfp, val);
		    argUsed=1;
		}
	    }
#ifdef HAVE_MPGLIB
	    T_ELIF ("replaygain-accurate") {
		lame_set_decode_on_the_fly(gfp,1);
		lame_set_findReplayGain(gfp,1);
	    }
	    T_ELIF ("clipdetect") {
		print_clipping_info = 1;
		lame_set_decode_on_the_fly(gfp,1);
	    }
	    T_ELIF ("decode") {
		decode_only = 1;
	    }
	    T_ELIF ("decode-mp3delay") {
		mp3_delay = atoi( nextArg );
		mp3_delay_set=1;
		argUsed=1;
	    }
#endif
	    T_ELIF_INTERNAL ("noath") {
		lame_set_noATH( gfp, 1 );
	    }
	    T_ELIF_INTERNAL ("nores") {
		lame_set_disable_reservoir(gfp,1);
	    }
	    T_ELIF ("sameblock") {
		lame_set_forbid_diff_blocktype(gfp, 1);
	    }
	    T_ELIF ("strictly-enforce-ISO") {
		lame_set_strict_ISO(gfp,1);
	    }
	    T_ELIF_INTERNAL ("athonly") {
		lame_set_ATHonly( gfp, 1 );
	    }
	    T_ELIF_INTERNAL ("athlower") {
		argUsed=1;
		lame_set_ATHlower( gfp, atof( nextArg ) );
	    }
	    T_ELIF_INTERNAL ("athcurve") {
		argUsed=1;
		lame_set_ATHcurve( gfp, atof( nextArg ) );
	    }
	    T_ELIF_INTERNAL ("athaa-sensitivity") {
		argUsed=1;
		lame_set_athaa_sensitivity( gfp, atof(nextArg) );
	    }
	    T_ELIF ("scale") {
		argUsed=1;
		lame_set_scale( gfp, atof(nextArg) );
	    }
	    T_ELIF ("noasm") {
		argUsed=1;
		if (!strcmp(nextArg, "mmx")) 
		    lame_set_asm_optimizations( gfp, MMX, 0 );
		if (!strcmp(nextArg, "3dnow")) 
		    lame_set_asm_optimizations( gfp, AMD_3DNOW, 0 );
		if (!strcmp(nextArg, "sse")) 
		    lame_set_asm_optimizations( gfp, SSE, 0 );
	    }
	    T_ELIF ("scale-l") {
		argUsed=1;
		lame_set_scale_left( gfp, atof(nextArg) );
	    }
	    T_ELIF ("scale-r") {
		argUsed=1;
		lame_set_scale_right( gfp, atof(nextArg) );
	    }
	    T_ELIF ("replaygain-fast") {
		lame_set_findReplayGain(gfp,1);
	    }
	    T_ELIF ("noreplaygain") {
		lame_set_findReplayGain(gfp,0);
	    }
	    T_ELIF ("freeformat") {
		lame_set_free_format(gfp,1);
	    }
	    T_ELIF_INTERNAL ("athshort") {
		lame_set_ATHshort( gfp, 1 );
	    }
	    T_ELIF ("nohist") {
		disp_brhist = 0;
	    }
#if defined(__OS2__)
	    T_ELIF ("priority") {
		argUsed=1;
		setOS2Priority(atoi(nextArg));
	    }
#endif
#if defined(WIN32)
	    T_ELIF ("priority") {
		argUsed=1;
		setWin32Priority(atoi(nextArg));
	    }
#endif
	    /* options for ID3 tag */
	    T_ELIF ("tt") {
		argUsed=1;
		id3tag_set_title(gfp, nextArg);
	    }
	    T_ELIF ("ta") {
		argUsed=1;
		id3tag_set_artist(gfp, nextArg);
	    }
	    T_ELIF ("tl") {
		argUsed=1;
		id3tag_set_album(gfp, nextArg);
	    }
	    T_ELIF ("ty") {
		argUsed=1;
		id3tag_set_year(gfp, nextArg);
	    }
	    T_ELIF ("tc") {
		argUsed=1;
		id3tag_set_comment(gfp, nextArg);
	    }
	    T_ELIF ("tn") {
		argUsed=1;
		if( 0 == ignore_tag_errors ) {
		    if (nextArg && *nextArg) {
			int num = atoi(nextArg);
			if ( num < 0 || num > 255 ) {
			    if( silent < 10 ) {
				fprintf(stderr, "The track number has to be between 0 and 255.\n");
			    }
			    return -1;
			}
		    }
		}
		id3tag_set_track(gfp, nextArg);
	    }    
	    T_ELIF ("tg") {
		argUsed=1;
		if ( id3tag_set_genre(gfp, nextArg) ) {
		    if ( ignore_tag_errors ) {
			if( silent < 10 ) {
			    fprintf(stderr, "Unknown genre: '%s'.  Setting to 'Other'\n", nextArg);
			}
			id3tag_set_genre(gfp, "other");
		    } else {
			fprintf(stderr, "Unknown genre: '%s'.  Specify genre name or number\n", nextArg);
			return -1;
		    }
		}
	    }
	    T_ELIF ("add-id3v2") {
		id3tag_add_v2(gfp);
	    }
            T_ELIF ("u8") {
		id3tag_u8(gfp);
	    }
	    T_ELIF ("id3v1-only") {
		id3tag_v1_only(gfp);
	    }
	    T_ELIF ("id3v2-only") {
		id3tag_v2_only(gfp);
	    }
	    T_ELIF ("space-id3v1") {
		id3tag_space_v1(gfp);
	    }
	    T_ELIF ("pad-id3v2") {
		id3tag_pad_v2(gfp);
	    }
	    T_ELIF ("genre-list") {
		id3tag_genre_list(genre_list_handler, NULL);
		return -2;
	    }
	    T_ELIF ("lowpass") {
		val     = atof( nextArg );
		argUsed = 1;
		/* useful are 0.001 kHz...50 kHz, 50 Hz...50000 Hz */
		if ( val < 0.001 || val > 50000. ) {
		    fprintf(stderr,"Must specify lowpass with --lowpass freq, freq >= 0.001 kHz\n");
		    return -1;
		}
		lame_set_lowpassfreq(gfp,val * (val < 50. ? 1000 : 1 ) + 0.5);
	    }
	    T_ELIF ("lowpass-width") {
		argUsed=1;
		val = 1000.0 * atof( nextArg ) + 0.5;
		if (val  < 0) {
		    fprintf(stderr,"Must specify lowpass width with --lowpass-width freq, freq >= 0 kHz\n");
		    return -1;
		}
		lame_set_lowpasswidth(gfp,val); 
	    }
	    T_ELIF ("highpass") {
		val = atof( nextArg );
		argUsed=1;
		/* useful are 0.001 kHz...16 kHz, 16 Hz...50000 Hz */
		if ( val < 0.001 || val > 50000. ) {
		    fprintf(stderr,"Must specify highpass with --highpass freq, freq >= 0.001 kHz\n");
		    return -1;
		}
		lame_set_highpassfreq(gfp, val * (val < 16. ? 1.e3 : 1.e0 ) + 0.5);
	    }
	    T_ELIF ("highpass-width") {
		argUsed=1;
		val = 1000.0 * atof( nextArg ) + 0.5;
		if (val   < 0) {
		    fprintf(stderr,"Must specify highpass width with --highpass-width freq, freq >= 0 kHz\n");
		    return -1;
		}
		lame_set_highpasswidth(gfp,val);
	    }
	    T_ELIF ("comp") {
		argUsed=1;
		val = atof( nextArg );
		if (val  < 1.0 ) {
		    fprintf(stderr,"Must specify compression ratio >= 1.0\n");
		    return -1;
		}
		lame_set_compression_ratio(gfp,val);
	    }
	    T_ELIF_INTERNAL ("reduce-side") {
		argUsed=1;
		lame_set_reduceSide( gfp, atof(nextArg ) );
	    }
	    T_ELIF_INTERNAL ("interch") {
		argUsed=1;
		lame_set_interChRatio( gfp, atof(nextArg ) );
	    }
	    T_ELIF_INTERNAL ("is-ratio") {
		argUsed=1;
		lame_set_istereoRatio( gfp, atof(nextArg ) );
	    }
	    T_ELIF_INTERNAL ("substep") {
		argUsed=1;
		lame_set_substep( gfp, atoi(nextArg) );
	    }
	    T_ELIF_INTERNAL ("sfscale") {
		int i = 1;
		if (sscanf(nextArg, "%d", &i) == 1)
		    argUsed=1;
		lame_set_sfscale(gfp, i);
	    }
	    T_ELIF_INTERNAL ("sbgain") {
		int i = 1;
		if (sscanf(nextArg, "%d", &i) == 1)
		    argUsed=1;
		lame_set_subblock_gain( gfp, i);
	    }
	    T_ELIF_INTERNAL ("narrowen-stereo") {
		argUsed = 1;
		lame_set_narrowenStereo(gfp, atof(nextArg));
	    }
	    T_ELIF ("nspsytune") {
		fprintf(stderr, "note: nspsytune is defaulted and no need to specify the option.\n");
	    }
	    T_ELIF_INTERNAL ("nsmsfix") {
		argUsed=1;
		lame_set_msfix( gfp, atof(nextArg) );
	    }
	    T_ELIF ("tune-bass") {
		argUsed=1;
		lame_set_tune_bass(gfp, (int)(atof( nextArg ) * 4.0));
	    }
	    T_ELIF ("tune-alto") {
		argUsed=1;
		lame_set_tune_alto(gfp, (int)(atof( nextArg ) * 4.0));
	    }
	    T_ELIF ("tune-treble") {
		argUsed=1;
		lame_set_tune_treble(gfp, (int)(atof( nextArg ) * 4.0));
	    }
	    T_ELIF ("tune-sfb21") {
		argUsed=1;
		lame_set_tune_sfb21(gfp, (int)(atof( nextArg ) * 4.0));
	    }
	    /* some more GNU-ish options could be added
	     * brief         => few messages on screen (name, status report)
	     * o/output file => specifies output filename
	     * O             => stdout
	     * i/input file  => specifies input filename
	     * I             => stdin
	     */
	    T_ELIF2 ("quiet", "silent") {
		silent = 10;    /* on a scale from 1 to 10 be very silent */
	    }
	    T_ELIF ("brief") {
		silent = -5;     /* print few info on screen */
	    }
	    T_ELIF ("verbose") {
		silent = -10;    /* print a lot on screen */
	    }
	    T_ELIF ("ignore-tag-errors") {
		ignore_tag_errors = 1;
	    }
	    T_ELIF2 ("version", "license") {
		print_license (stdout);
		return -2;
	    }
	    T_ELIF2 ("help", "usage") {
		short_help ( gfp, stdout, ProgramName );
		return -2;
	    }
	    T_ELIF ("longhelp") {
		long_help ( gfp, stdout, ProgramName, 0 /* lessmode=NO */ );
		return -2;
	    }
	    T_ELIF ("?") {
#ifdef __unix__
		FILE* fp = popen ("less -Mqc", "w");
		long_help ( gfp, fp, ProgramName, 0 /* lessmode=NO */ );
		pclose (fp);
#else           
		long_help ( gfp, stdout, ProgramName, 1 /* lessmode=YES */ );
#endif              
		return -2;
	    }
	    T_ELIF2 ("preset", "alt-preset") {
		int fast = 0, cbr = 0;
		for (;;) {
		    if (!fast && strcmp(nextArg, "fast") == 0)
			fast = 1;
		    else if (!cbr && strcmp(nextArg, "cbr")  == 0)
			cbr = 1;
		    else
			break;
		    argUsed++;
		    nextArg = i+argUsed+1 < argc  ?  argv[i+argUsed+1]  :  "";
		}

		if (presets_set (gfp, cbr, nextArg, ProgramName ) < 0)
		    return -1;
		    argUsed++;
	    }
	    T_ELIF ("disptime") {
		argUsed = 1;
		update_interval = atof (nextArg);
	    }
	    T_ELIF ("nogaptags") {
		nogap_tags=1;
	    }
	    T_ELIF ("nogapout") {
		strcpy(outPath, nextArg);
		argUsed = 1;
	    }
	    T_ELIF ("nogap") {
		nogap=1;
	    }
	    T_ELIF ("start") {
		
	    }
	    T_ELIF ("end") {
		
	    }
	    T_ELSE {
		fprintf(stderr,"%s: unrec option --%s\n", ProgramName, token);
	    }
	    i += argUsed;
	    continue;
	}
	while ( (c = *token++) != '\0' ) {
	    arg = *token ? token : nextArg;
	    switch (c) {
	    case 'm':
		argUsed           = 1;
		switch ( *arg ) {
		case 's': lame_set_mode( gfp, STEREO       );
		    break;
		case 'd': lame_set_mode( gfp, DUAL_CHANNEL );
		    fprintf( stderr,
			     "%s: dual channel is not supported yet, the result (perhaps stereo)\n"
			     "  may not be what you expect\n",
			     ProgramName );
		    break;
		case 'f': lame_set_force_ms(gfp,1);
		    /* FALLTHROUGH */
		case 'j': lame_set_mode( gfp, JOINT_STEREO );
		    break;
		case 'm': lame_set_mode( gfp, MONO         );
		    break;
		default:
		    fprintf(stderr,"%s: -m mode must be s/d/j/f/m not %s\n", ProgramName, arg);
		    err = 1;
		    break;
		}
		break;
	    case 'V':        argUsed = 1;
		lame_set_VBR(gfp, vbr);
		lame_set_VBR_q(gfp, atoi(arg));
		if (lame_get_VBR_q(gfp) <0) lame_set_VBR_q(gfp,0);
		break;
	    case 'v': 
		lame_set_VBR(gfp, vbr);
		break;
	    case 'q':        argUsed = 1;
		lame_set_quality(gfp, atoi( arg ));
		break;
	    case 'f': 
		lame_set_quality( gfp, 7 );
		break;
	    case 'h': 
		lame_set_quality( gfp, 2 );
		break;
	    case 's':
		argUsed = 1;
		val = atof( arg );
		if (val <= 192) val *= 1000.0f;
		lame_set_in_samplerate(gfp, (int)(val + 0.5));
		break;
	    case 'b':        
		argUsed = 1;
		lame_set_brate(gfp,atoi(arg)); 
		lame_set_VBR_min_bitrate_kbps(gfp,lame_get_brate(gfp));
		break;
	    case 'B':        
		argUsed = 1;
		lame_set_VBR_max_bitrate_kbps(gfp,atoi(arg)); 
		break;  
	    case 't':  /* dont write VBR tag */
		lame_set_bWriteVbrTag( gfp, 0 );
		disable_wav_header=1;
		break;
	    case 'T':  /* do write VBR tag */
		lame_set_bWriteVbrTag( gfp, 1 );
		nogap_tags=1;
		disable_wav_header=0;
		break;
	    case 'r':  /* force raw pcm input file */
		input_format=sf_raw;
		break;
	    case 'x':  /* force byte swapping */
		outputPCMendian ^= 1;
		in_endian ^= 1;
		break;
	    case 'p': /* error_protection: add crc16 information to stream */
		lame_set_error_protection(gfp,1);
		break;
	    case 'a': /* autoconvert input file from stereo to mono - for mono mp3 encoding */
		autoconvert=1;
		lame_set_mode( gfp, MONO );
		break;
	    case 'k': 
		lame_set_lowpassfreq(gfp,1000000);
		lame_set_highpassfreq(gfp,0);
		break;
	    case 'S': 
		silent = 1;
		break;
	    case 'X':        
		argUsed = 1;   
		experimentalX = atoi(arg);
		break;
	    case 'Y': 
		experimentalY = 1;
		break;
	    case 'Z': 
		experimentalZ = 1;
		break;
	    case 'e':
		argUsed = 1;
		switch (*arg) {
		case 'n': lame_set_emphasis( gfp, 0 ); break;
		case '5': lame_set_emphasis( gfp, 1 ); break;
		case 'c': lame_set_emphasis( gfp, 3 ); break;
		default : fprintf(stderr,"%s: -e emp must be n/5/c not %s\n", ProgramName, arg );
		    err = 1;
		    break;
		}
		break;
	    case 'c':   
		lame_set_copyright(gfp,1);
		break;
	    case 'o':   
		lame_set_original(gfp,0);
		break;
	    case '?':   
		long_help ( gfp, stderr, ProgramName, 0 /* LESSMODE=NO */);
		return -1;
	    default:    
		fprintf(stderr,"%s: unrec option %c\n", ProgramName, c);
		err = 1; 
		break;
	    }
	    if (argUsed) {
		if(arg == token)    token = "";   /* no more from token */
		else                ++i;          /* skip arg we used */
		arg = ""; argUsed = 0;
	    }
        }
    }  /* loop over args */
    
    if ( err  ||  !input_file ) {
        usage (stderr, ProgramName );
        return -1;
    }
        
    if ( inPath[0] == '-' ) {
	silent = (silent <= 1 ? 1 : silent);
	if (keeptag) {
	    keeptag = 0;
	    fprintf(stderr,
		    "sorry, keeptag does not work when input is stdin.\n");
	}
    }
#ifdef WIN32
    else
        dosToLongFileName( inPath );
#endif
    
    if ( outPath[0] == '\0' && count_nogap == 0) {
        if ( inPath[0] == '-' ) {
	    /* if input is stdin, default output is stdout */
	    strcpy(outPath,"-");
	} else {
	    strncpy(outPath, inPath, PATH_MAX + 1 - 4);
	    if (decode_only) {
		strncat (outPath, ".wav", 4 );
	    } else {
		strncat (outPath, ".mp3", 4 );
	    }
        }
    }
    
    /* disable VBR tags with nogap unless the VBR tags are forced */
    if (nogap && lame_get_bWriteVbrTag(gfp) && nogap_tags==0) {
	fprintf(stderr,"Note: Disabling VBR Xing/Info tag since it interferes with --nogap\n");
	lame_set_bWriteVbrTag( gfp, 0 );
    }

    /* some file options not allowed with stdout */
    if (outPath[0]=='-') {
	lame_set_bWriteVbrTag( gfp, 0 ); /* turn off VBR tag */
    }

    /* if user did not explicitly specify input is mp3, check file name */
    if (input_format == sf_unknown)
	input_format = filename_to_type ( inPath );

#ifndef HAVE_MPGLIB
    if (IS_MPEG123(input_format)) {
	fprintf(stderr,"Error: libmp3lame not compiled with mpg123 *decoding* support \n");
	return -1;
    }
#endif

    /* default guess for number of channels */
    if (autoconvert || lame_get_mode(gfp) != MONO)
	lame_set_num_channels(gfp, 2); 
    else
	lame_set_num_channels(gfp, 1);

    if (lame_get_free_format(gfp)) {
	if (lame_get_brate(gfp) < 8 || lame_get_brate(gfp) > 640) {
	    fprintf(stderr,"For free format, specify a bitrate between 8 and 640 kbps\n");
	    fprintf(stderr,"with the -b <bitrate> option\n");
	    return -1;
	}
    }
    if (num_nogap) *num_nogap=count_nogap;

    return 0;
}

/* end of parse.c */

