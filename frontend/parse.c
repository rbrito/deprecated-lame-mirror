/*
 *	Command line parsing related functions
 *
 *	Copyright (c) 1999 Mark Taylor
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include "lame.h"

#include "brhist.h"
#include "parse.h"
#include "main.h"
#include "get_audio.h"
#include "version.h"

/************************************************************************
*
* license
*
* PURPOSE:  Writes version and license to the file specified by fp
*
************************************************************************/

static int  lame_version_print ( FILE* const fp )
{
/* 
   fprintf ( fp, "%sLAME%s version %s    (%s)\n\n", Console_IO.str_emph, Console_IO.str_norm, get_lame_version (), get_lame_url () );
    ^- disabled as long as there is no proper solution for Console_IO, RH 
 */
   fprintf ( fp, "LAME version %s    (%s)\n\n", get_lame_version (), get_lame_url () );

   return 0;
}

int  print_license ( const lame_global_flags* gfp, FILE* const fp, const char* ProgramName )  /* print version & license */
{
    lame_version_print ( fp );
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

int  usage ( const lame_global_flags* gfp, FILE* const fp, const char* ProgramName )  /* print general syntax */
{
    lame_version_print ( fp );
    fprintf ( fp,
              "usage: %s [options] <infile> [outfile]\n"
              "\n"
              "    <infile> and/or <outfile> can be \"-\", which means stdin/stdout.\n"
              "\n"
              "Try \"%s --help\"     for more information\n" 
              " or \"%s --longhelp\" or \"%s --?\" for a complete options list\n\n",
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

int  short_help ( const lame_global_flags* gfp, FILE* const fp, const char* ProgramName )  /* print short syntax help */
{
    lame_version_print ( fp );
    fprintf ( fp,
              "usage: %s [options] <infile> [outfile]\n"
              "\n"
              "    <infile> and/or <outfile> can be \"-\", which means stdin/stdout.\n"
              "\n"
              "RECOMMENDED:\n"
              "    lame -h input.wav output.mp3\n"
              "\n"
              "OPTIONS:\n"
              "    -b bitrate      set the bitrate, default 128 kbps\n"
              "    -f              fast mode (lower quality)\n"
              "    -h              higher quality, but a little slower.  Recommended.\n"
              "    -k              keep ALL frequencies (disables all filters)\n"
              "                    Can cause ringing and twinkling\n"
              "    -m mode         (s)tereo, (j)oint, (f)orce or (m)ono  (default j)\n"
              "                    force = force ms_stereo on all frames.\n"
              "    -V n            quality setting for VBR.  default n=%i\n"
              "\n"
              "    --preset type   type must be phone, voice, fm, tape, hifi, cd or studio\n"
              "                    \"--preset help\" gives some more infos on these\n"
              "\n"
              "    --longhelp      full list of options\n"
              "\n",
              ProgramName, gfp->VBR_q );
 
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

int  long_help ( const lame_global_flags* gfp, FILE* const fp, const char* ProgramName, int lessmode )  /* print long syntax help */
{
    lame_version_print ( fp );
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
              "    --mp1input      input file is a MPEG Layer I   file\n"
              "    --mp2input      input file is a MPEG Layer II  file\n"
              "    --mp3input      input file is a MPEG Layer III file\n"
              "    --ogginput      input file is a Ogg Vorbis file",
              ProgramName );

    wait_for ( fp, lessmode );
    fprintf ( fp,
              "  Operational options:\n"
              "    -m <mode>       (s)tereo, (j)oint, (f)orce or (m)ono  (default j)\n"
              "                    force = force ms_stereo on all frames.\n"
              "    -a              downmix from stereo to mono file for mono encoding\n"
              "    -d              allow channels to have different blocktypes\n"
              "    -S              don't print progress report, VBR histograms\n"
              "    --disptime <arg>print progress report every arg seconds\n"
              "    --ogg           encode to Ogg Vorbis instead of MP3\n"
              "    --freeformat    produce a free format bitstream\n"
              "    --decode        input=mp3 file, output=wav\n"
              "    -t              disable writing wav header when using --decode\n"
              "    --comp  <arg>   choose bitrate to achive a compression ratio of <arg>\n"
              "    --scale <arg>   scale input (multiply PCM data) by <arg>\n"
              "    --athonly       only use the ATH for masking\n"
              "    --noath         disable the ATH for masking\n"
              "    --athlower x    lower the ATH x dB\n"
              "    --short         use short blocks\n"
              "    --noshort       do not use short blocks\n"
              "    --voice         experimental voice mode\n"
              "    --preset type   type must be phone, voice, fm, tape, hifi, cd or studio\n"
              "                    \"--preset help\" gives some more infos on these" );
  
    wait_for ( fp, lessmode );
    fprintf ( fp,
              "  CBR (constant bitrate, the default) options:\n"
              "    -h              higher quality, but a little slower.  Recommended.\n"
              "    -f              fast mode (lower quality)\n"
              "    -b <bitrate>    set the bitrate in kbps, default 128 kbps\n"
              "\n"
              "  ABR options:\n"
              "    --abr <bitrate> specify average bitrate desired (instead of quality)\n"
              "\n"
              "  VBR options:\n"
              "    -v              use variable bitrate (VBR)\n"
              "    --vbr-old       use old variable bitrate (VBR) routine\n"
              "    --vbr-new       use new variable bitrate (VBR) routine\n"
              "    -V n            quality setting for VBR.  default n=%i\n"
              "                    0=high quality,bigger files. 9=smaller files\n"
              "    -b <bitrate>    specify minimum allowed bitrate, default  32 kbps\n"
              "    -B <bitrate>    specify maximum allowed bitrate, default 320 kbps\n"
              "    -F              strictly enforce the -b option, for use with players that\n"
              "                    do not support low bitrate mp3 (Apex AD600-A DVD/mp3 player)\n"
              "    -t              disable writing Xing VBR informational tag\n"
              "    --nohist        disable VBR histogram display", gfp->VBR_q );
  
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
              "\n"
              "  Filter options:\n"
              "    -k              keep ALL frequencies (disables all filters),\n"
              "                    Can cause ringing and twinkling\n"
              "  --lowpass <freq>        frequency(kHz), lowpass filter cutoff above freq\n"
              "  --lowpass-width <freq>  frequency(kHz) - default 15%% of lowpass freq\n"
              "  --highpass <freq>       frequency(kHz), highpass filter cutoff below freq\n"
              "  --highpass-width <freq> frequency(kHz) - default 15%% of highpass freq\n"
              "  --resample <sfreq>  sampling frequency of output file(kHz)- default=automatic\n"
              "  --cwlimit <freq>    compute tonality up to freq (in kHz) default 8.8717" );
  
    wait_for ( fp, lessmode );
    fprintf ( fp,
              "  ID3 tag options:\n"
              "    --tt <title>    audio/song title (max 30 chars for version 1 tag)\n"
              "    --ta <artist>   audio/song artist (max 30 chars for version 1 tag)\n"
              "    --tl <album>    audio/song album (max 30 chars for version 1 tag)\n"
              "    --ty <year>     audio/song year of issue (1 to 9999)\n"
              "    --tc <comment>  user-defined text (max 30 chars for v1 tag, 28 for v1.1)\n"
              "    --tn <track>    audio/song track number (1 to 99, creates v1.1 tag)\n"
              "    --tg <genre>    audio/song genre (name or number in list)\n"
              "    --add-id3v2     force addition of version 2 tag\n"
              "    --id3v1-only    add only a version 1 tag\n"
              "    --id3v2-only    add only a version 2 tag\n"
              "    --space-id3v1   pad version 1 tag with spaces instead of nulls\n"
              "    --pad-id3v2     pad version 2 tag with extra 128 bytes\n"
              "    --genre-list    print alphabetically sorted ID3 genre list and exit\n"
              "\n"
              "    Note: A version 2 tag will NOT be added unless one of the input fields\n"
              "    won't fit in a version 1 tag (e.g. the title string is longer than 30\n"
              "    characters), or the `--add-id3v2' or `--id3v2-only' options are used,\n"
              "    or output is redirected to stdout."
#if defined(HAVEVORBIS)
              "\n\n"
              "    Note: All `--t*' options (except those for track and genre) work for Ogg\n"
              "    Vorbis output, but other ID3-specific options are ignored."
#endif              
#if defined(HAVEGTK)
              "\n\n"
              "    -g              run graphical analysis on <infile>"
#endif
              );

    wait_for ( fp, lessmode );  
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
    for (i = 1; i < 15; i++ )
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


typedef struct {
    const char* name;			// name of preset
    long        resample;		// resample frequency in Hz, or -1 for no resampling
    short       highpass_freq;		// highpass frequency in Hz, or -1 for no highpass filtering
    short       lowpass_freq;		// lowpass frequency in Hz, or -1 for no lowpass filtering
    short       lowpass_width;		// lowpass width in Hz
    signed char no_short_blocks;	// use of short blocks, 1: no, 0: yes
    signed char quality;		// quality, the same as -f or -h
    signed char mode;			// channel mode (mono, stereo, joint)
    short       cbr;			// CBR data rate in kbps (8...320)
    signed char vbr_mode;		// VBR mode (0...9)
    short       vbr_min;		// minimum VBR rate in kbps ( 8...256)
    short       vbr_max;		// maximum VBR rate in kbps (16...320)
} preset_t;

const preset_t Presets [] = {
   // name       fs     fu    fo    dfo shrt qual  mode              cbr vbr_mode/min/max
    { "phone" ,  8000, 125,  3400,    0,  1,  5, MPG_MD_MONO        ,  16,  6,   8,  24 },  // phone standard 300-3400
    { "phon+" , 11025, 100,  4000,    0,  1,  5, MPG_MD_MONO        ,  24,  4,  16,  32 },  // phone theoretical limits
    { "lw"    , 11025,  -1,  4000,    0,  0,  5, MPG_MD_MONO        ,  24,  3,  16,  56 },  // LW
    { "mw-eu" , 11025,  -1,  4000,    0,  0,  5, MPG_MD_MONO        ,  24,  3,  16,  56 },  // MW in europe
    { "mw-us" , 16000,  -1,  7500,    0,  0,  5, MPG_MD_MONO        ,  40,  3,  24, 112 },  // MW in U.S.A.
    { "sw"    , 11025,  -1,  4000,    0,  0,  5, MPG_MD_MONO        ,  24,  3,  16,  56 },  // SW
    { "fm"    , 32000,  -1, 15000,    0,  0,  5, MPG_MD_JOINT_STEREO, 112,  3,  80, 256 },
    { "voice" , 24000,  -1, 12000,    0,  1,  5, MPG_MD_MONO        ,  56,  4,  40, 112 },
    { "radio" ,    -1,  -1, 15000,    0,  0,  5, MPG_MD_JOINT_STEREO, 128,  3,  96, 256 },
    { "tape"  ,    -1,  -1, 18000,  900,  0,  5, MPG_MD_JOINT_STEREO, 128,  3,  96, 256 },
    { "hifi"  ,    -1,  -1, 18000,  900,  0,  2, MPG_MD_JOINT_STEREO, 160,  2, 112, 320 },
    { "cd"    ,    -1,  -1,    -1,   -1,  0,  2, MPG_MD_STEREO      , 192,  1, 128, 320 },
    { "studio",    -1,  -1,    -1,   -1,  0,  2, MPG_MD_STEREO      , 256,  0, 160, 320 },
};


static int  presets_info ( const lame_global_flags* gfp, FILE* const fp, const char* ProgramName )  /* print possible combination */
{
    size_t  i;

    fprintf ( fp, "\n");    
    lame_version_print ( fp );
    
    fprintf ( fp, "Presets are some shortcuts for common settings.\n");
    fprintf ( fp, "They can be combined with -v if you want VBR MP3s.\n");
    
    fprintf ( fp, "\n                ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        fprintf ( fp,  strlen(Presets[i].name) <= 4 ? "%5s " : " %-5s", Presets[i].name );
    fprintf ( fp, "\n=================");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        fprintf ( fp,  "======" );
    fprintf ( fp, "\n--resample      ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        if ( Presets[i].resample < 0 )
            fprintf ( fp,  "      " );
        else
            fprintf ( fp,  "%6.3g",  Presets[i].resample*1.e-3 );
    fprintf ( fp, "\n--highpass      ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        if ( Presets[i].highpass_freq < 0 )
            fprintf ( fp,  "      " );
        else
            fprintf ( fp,  "%6.3g",  Presets[i].highpass_freq*1.e-3 );
    fprintf ( fp, "\n--lowpass       ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        if ( Presets[i].lowpass_freq < 0 )
            fprintf ( fp,  "      " );
        else
            fprintf ( fp,  "%6.3g",  Presets[i].lowpass_freq*1.e-3 );
    fprintf ( fp, "\n--lowpass-width ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        if ( Presets[i].lowpass_width < 0 )
            fprintf ( fp,  "      " );
        else
            fprintf ( fp,  "%6.3g",  Presets[i].lowpass_width*1.e-3 );
    fprintf ( fp, "\n--noshort       ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        switch ( Presets[i].no_short_blocks ) {
        case  1: fprintf ( fp,  "   yes" ); break;
        case  0: fprintf ( fp,  "    no" ); break;
        case -1: fprintf ( fp,  "      " ); break;
        default: assert (0);                break;
        }
    fprintf ( fp, "\n                ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        switch ( Presets[i].mode ) {
        case MPG_MD_MONO:         fprintf ( fp, "   -mm"); break;
        case MPG_MD_JOINT_STEREO: fprintf ( fp, "   -mj"); break;
        case MPG_MD_STEREO:       fprintf ( fp, "   -ms"); break;
        case -1:                  fprintf ( fp, "      "); break;
        default:                  assert (0);              break;
        }
    fprintf ( fp, "\n                ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        switch ( Presets[i].quality ) {
	case  2: fprintf ( fp, "    -h"); break;
	case  5: fprintf ( fp, "      "); break;
	case  7: fprintf ( fp, "    -f"); break;
	default: assert (0);              break;
    }
    fprintf ( fp, "\n-b              ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        fprintf ( fp,  "%6u", Presets[i].cbr );
    fprintf ( fp, "\n-- PLUS WITH -v ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        fprintf ( fp,  "------" );
    fprintf ( fp, "-\n-V              ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        fprintf ( fp,  "%6u", Presets[i].vbr_mode );
    fprintf ( fp, "\n-b              ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        fprintf ( fp,  "%6u", Presets[i].vbr_min );
    fprintf ( fp, "\n-B              ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        fprintf ( fp,  "%6u", Presets[i].vbr_max );
    fprintf ( fp, "\n----------------");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        fprintf ( fp,  "------" );
  
    fprintf ( fp, "-\nEXAMPLES:\n");
    fprintf ( fp, " a) --preset fm\n");
    fprintf ( fp, "    equal to: -mj -b112 --resample 32 --lowpass 15 --lowpass-width 0\n");
    fprintf ( fp, " b) -v --preset studio\n");
    fprintf ( fp, "    equals to: -h -ms -V0 -b160 -B320\n");
 
    return 0;
}


static int  presets_setup ( lame_global_flags* gfp, const char* preset_name, const char* ProgramName )
{
    size_t  i;

    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++ )
        if ( 0 == strncmp (preset_name, Presets[i].name, strlen (preset_name) ) ) {
            if ( Presets[i].resample >= 0 )
	        gfp -> out_samplerate   = Presets[i].resample;
	    if ( Presets[i].highpass_freq >= 0 )
	        gfp -> highpassfreq     = Presets[i].highpass_freq,
	        gfp -> highpasswidth    = 0;
	    gfp -> lowpassfreq          = Presets[i].lowpass_freq;
	    gfp -> lowpasswidth         = Presets[i].lowpass_width;
	    gfp -> no_short_blocks      = Presets[i].no_short_blocks;
	    gfp -> quality              = Presets[i].quality;
	    gfp -> mode                 = Presets[i].mode;
	    gfp -> brate                = Presets[i].cbr;
	    gfp -> VBR_q                = Presets[i].vbr_mode;
	    gfp -> VBR_min_bitrate_kbps = Presets[i].vbr_min;
	    gfp -> VBR_max_bitrate_kbps = Presets[i].vbr_max;
	    gfp -> mode_fixed           = 1; 
	    return 0;
	}

    presets_info ( gfp, stderr, ProgramName );
    return -1;
}


static void genre_list_handler (int num,const char *name,void *cookie)
{
    printf ("%3d %s\n", num, name);
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


// buggy: Should analyze file contents instead of directory contents (file name)

static int filename_to_type ( const char* FileName )
{
    size_t len = strlen (FileName);
    
    if ( len < 4 ) return sf_unknown;

    FileName += len-4;
    if ( 0 == local_strcasecmp ( FileName, ".mpg" ) ) return sf_mp1;
    if ( 0 == local_strcasecmp ( FileName, ".mp1" ) ) return sf_mp1;
    if ( 0 == local_strcasecmp ( FileName, ".mp2" ) ) return sf_mp2;
    if ( 0 == local_strcasecmp ( FileName, ".mp3" ) ) return sf_mp3;
    if ( 0 == local_strcasecmp ( FileName, ".ogg" ) ) return sf_ogg;
    if ( 0 == local_strcasecmp ( FileName, ".wav" ) ) return sf_wave;
    if ( 0 == local_strcasecmp ( FileName, ".aif" ) ) return sf_aiff;
    if ( 0 == local_strcasecmp ( FileName, ".raw" ) ) return sf_raw;
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

#define T_IF(str)          if ( 0 == local_strcasecmp (token,str) ) {
#define T_ELIF(str)        } else if ( 0 == local_strcasecmp (token,str) ) {
#define T_ELIF2(str1,str2) } else if ( 0 == local_strcasecmp (token,str1)  ||  0 == local_strcasecmp (token,str2) ) {
#define T_ELSE             } else {
#define T_END              }

int  parse_args ( lame_global_flags* gfp, int argc, char** argv, char* const inPath, char* const outPath )
{
    int         err;
    int         i;
    int         autoconvert  = 0;
    int         user_quality = -1;
    double      val;
    const char* ProgramName  = argv[0]; 

    inPath [0] = '\0';   
    outPath[0] = '\0';
    /* turn on display options. user settings may turn them off below */
    silent   = 0;
    brhist   = 1;
    id3tag_init (gfp);

    /* process args */
    for ( i = 0, err = 0; ++i < argc  &&  !err; ) {
	char   c;
	char*  token;
	char*  arg;
	char*  nextArg;
	int    argUsed;
    
	token = argv[i];
	if ( *token++ == '-' ) {
            argUsed = 0;
	    nextArg = i+1 < argc  ?  argv[i+1]  :  "";
	    
            if (! *token) { /* The user wants to use stdin and/or stdout. */
	        if (inPath [0] == '\0')
		    strncpy (inPath, argv[i],MAX_NAME_SIZE);
	        else 
		if (outPath[0] == '\0') 
		    strncpy (outPath, argv[i],MAX_NAME_SIZE);
            } 
	    if (*token == '-') { /* GNU style */
		token++;

		T_IF ("resample")
		    argUsed = 1;
		    gfp -> out_samplerate = resample_rate ( atof (nextArg) );
		
		T_ELIF ("vbr-old")
		    gfp->VBR = vbr_rh; 
		    gfp->quality = 2;
		
		T_ELIF ("vbr-new")
		    gfp->VBR = vbr_mt; 
		    gfp->quality = 2;
		
		T_ELIF ("vbr-mtrh")
		    gfp->VBR = vbr_mtrh; 
		    gfp->quality = 2;
		
		T_ELIF ("abr")
		    argUsed=1;
		    gfp->VBR = vbr_abr; 
		    gfp->VBR_mean_bitrate_kbps = atoi(nextArg);
		    /* values larger than 8000 are bps (like Fraunhofer), so it's strange to get 320000 bps MP3 when specifying 8000 bps MP3 */
		    if ( gfp -> VBR_mean_bitrate_kbps >= 8000 )
			gfp -> VBR_mean_bitrate_kbps = ( gfp -> VBR_mean_bitrate_kbps + 500 ) / 1000;
		    gfp->VBR_mean_bitrate_kbps = Min(gfp->VBR_mean_bitrate_kbps, 320); 
		    gfp->VBR_mean_bitrate_kbps = Max(gfp->VBR_mean_bitrate_kbps,   8); 
		
		T_ELIF ("mp1input")
		    input_format=sf_mp1;
		
		T_ELIF ("mp2input")
		    input_format=sf_mp2;
		
		T_ELIF ("mp3input")
		    input_format=sf_mp3;
		
		T_ELIF ("ogginput")
#if defined(HAVEVORBIS)
		    input_format=sf_ogg;
#else
		    fprintf(stderr,"Error: LAME not compiled with Vorbis support\n");
		    return -1;
#endif
		T_ELIF ("ogg")
#if defined(HAVEVORBIS)
		    gfp->ogg=1;
#else
		    fprintf(stderr,"Error: LAME not compiled with Vorbis support\n");
		    return -1;
#endif
		T_ELIF ("phone")
                    if (presets_setup ( gfp, token, ProgramName ) < 0)
		        return -1;
                    
		T_ELIF ("voice")
                    if (presets_setup ( gfp, token, ProgramName ) < 0)
		        return -1;
                    
		T_ELIF ("radio")
                    if (presets_setup ( gfp, token, ProgramName ) < 0)
		        return -1;
                    
		T_ELIF ("tape")
                    if (presets_setup ( gfp, token, ProgramName ) < 0)
		        return -1;
                    
		T_ELIF ("cd")
                    if (presets_setup ( gfp, token, ProgramName ) < 0)
		        return -1;
                    
		T_ELIF ("studio")
                    if (presets_setup ( gfp, token, ProgramName ) < 0)
		        return -1;
                    
		T_ELIF ("noshort")
		    gfp->no_short_blocks=1;
		
		T_ELIF ("short")
		    gfp->no_short_blocks=0;
		
		T_ELIF ("decode")
		    gfp->decode_only=1;
		
		T_ELIF ("noath")
		    gfp->noATH=1;
		
		T_ELIF ("nores")
		    gfp->disable_reservoir=1;
		    gfp->padding_type=0;
		
		T_ELIF ("strictly-enforce-ISO")
		    gfp->strict_ISO=1;
		
		T_ELIF ("athonly")
		    gfp->ATHonly=1;
		
		T_ELIF ("athlower")
		    argUsed=1;
		    gfp->ATHlower = atoi(nextArg);
		
		T_ELIF ("scale")
		    argUsed=1;
		    gfp->scale = atof(nextArg);
		
		T_ELIF ("freeformat")
		    gfp->free_format=1;
		
		T_ELIF ("athshort")
		    gfp->ATHshort=1;
		
		T_ELIF ("nohist")
		    brhist = 0;
		
		/* options for ID3 tag */
		T_ELIF ("tt")
		    argUsed=1;
		    id3tag_set_title(gfp, nextArg);
		
		T_ELIF ("ta")
		    argUsed=1;
		    id3tag_set_artist(gfp, nextArg);
		
		T_ELIF ("tl")
		    argUsed=1;
		    id3tag_set_album(gfp, nextArg);
		
		T_ELIF ("ty")
		    argUsed=1;
		    id3tag_set_year(gfp, nextArg);
		
		T_ELIF ("tc")
		    argUsed=1;
		    id3tag_set_comment(gfp, nextArg);
		
		T_ELIF ("tn")
		    argUsed=1;
		    id3tag_set_track(gfp, nextArg);
		
		T_ELIF ("tg")
		    argUsed=1;
		    if (id3tag_set_genre(gfp, nextArg)) {
			fprintf(stderr,"Unknown genre: %s.  Specify genre name or number\n", nextArg);
			return -1;
		    }
		
		T_ELIF ("add-id3v2")
		    id3tag_add_v2(gfp);
		
		T_ELIF ("id3v1-only")
		    id3tag_v1_only(gfp);
		
		T_ELIF ("id3v2-only")
		    id3tag_v2_only(gfp);
		
		T_ELIF ("space-id3v1")
		    id3tag_space_v1(gfp);
		
		T_ELIF ("pad-id3v2")
		    id3tag_pad_v2(gfp);
		
		T_ELIF ("genre-list")
		    id3tag_genre_list(genre_list_handler, NULL);
		    return -2;
		
		T_ELIF ("lowpass")
		    val     = atof( nextArg );
		    argUsed = 1;
		    /* useful are 0.001 kHz...50 kHz, 50 Hz...50000 Hz */
		    gfp -> lowpassfreq = val * (val < 50. ? 1.e3 : 1.e0 ) + 0.5;
		    if ( val < 0.001 || val > 50000. ) {
			fprintf(stderr,"Must specify lowpass with --lowpass freq, freq >= 0.001 kHz\n");
			return -1;
		    }
		
		T_ELIF ("lowpass-width")
		    argUsed=1;
		    gfp->lowpasswidth =  1000.0 * atof( nextArg ) + 0.5;
		    if (gfp->lowpasswidth  < 0) {
			fprintf(stderr,"Must specify lowpass width with --lowpass-width freq, freq >= 0 kHz\n");
			return -1;
		    }
		
		T_ELIF ("highpass")
		    val = atof( nextArg );
		    argUsed=1;
		    /* useful are 0.001 kHz...16 kHz, 16 Hz...50000 Hz */
		    gfp->highpassfreq =  val * (val < 16. ? 1.e3 : 1.e0 ) + 0.5;
		    if ( val < 0.001 || val > 50000. ) {
			fprintf(stderr,"Must specify highpass with --highpass freq, freq >= 0.001 kHz\n");
			return -1;
		    }
		
		T_ELIF ("highpass-width")
		    argUsed=1;
		    gfp->highpasswidth =  1000.0 * atof( nextArg ) + 0.5;
		    if (gfp->highpasswidth  < 0) {
			fprintf(stderr,"Must specify highpass width with --highpass-width freq, freq >= 0 kHz\n");
			return -1;
		    }
		
		T_ELIF ("cwlimit")
		    val = atof (nextArg);
		    argUsed=1;
		    /* useful are 0.001 kHz...50 kHz, 50 Hz...50000 Hz */
		    gfp -> cwlimit = val * ( val <= 50. ? 1.e3 : 1.e0 );
		    if (gfp->cwlimit <= 0 ) {
			fprintf(stderr,"Must specify cwlimit with --cwlimit freq, freq >= 0.001 kHz\n");
			return -1;
		    }
		 
		T_ELIF ("comp")
		    argUsed=1;
		    gfp->compression_ratio =  atof( nextArg );
		    if (gfp->compression_ratio < 1.0 ) {
			fprintf(stderr,"Must specify compression ratio >= 1.0\n");
			return -1;
		    }
		
		T_ELIF ("nspsytune")
		    gfp->exp_nspsytune = 1;
		    gfp->experimentalZ = 1;
		    gfp->experimentalX = 1;
		
		/* some more GNU-ish options could be added
		 * version       => complete name, version and license info (normal exit)  
		 * quiet/silent  => no messages on screen
		 * brief         => few messages on screen (name, status report)
		 * verbose       => all infos to screen (brhist, internal flags/filters)
		 * o/output file => specifies output filename
		 * O             => stdout
		 * i/input file  => specifies input filename
		 * I             => stdin
		 */
		T_ELIF2 ("version", "license")
		    print_license ( gfp, stdout, ProgramName );
		    return -2;
		
		T_ELIF2 ("help", "usage")
		    short_help ( gfp, stdout, ProgramName );
		    return -2;
		
		T_ELIF ("longhelp")
		    long_help ( gfp, stdout, ProgramName, 0 /* lessmode=NO */ );
		    return -2;
		    
		T_ELIF ("?")
#ifdef __unix__
                    FILE* fp = popen ("less -Mqc", "w");
		    long_help ( gfp, fp, ProgramName, 0 /* lessmode=NO */ );
		    pclose (fp);
#else		
		    long_help ( gfp, stdout, ProgramName, 1 /* lessmode=YES */ );
#endif		    
		    return -2;
		
		T_ELIF ("preset")
		    argUsed = 1;
		    if (presets_setup ( gfp, nextArg, ProgramName ) < 0)
		        return -1;
		    
                T_ELIF ("disptime")
                    argUsed = 1;
		    update_interval = atof (nextArg);
		    
		T_ELSE
		    fprintf(stderr,"%s: unrec option --%s\n", ProgramName, token);
		    
		T_END
		
		i += argUsed;
		
	    } else {
		while ( (c = *token++) != '\0' ) {
		    arg = *token ? token : nextArg;
		    switch (c) {
		    case 'm':        
			argUsed           = 1;   
			gfp -> mode_fixed = 1;
			
			switch ( *arg ) {
			case 's': gfp -> mode = MPG_MD_STEREO;       break;
			case 'd': gfp -> mode = MPG_MD_DUAL_CHANNEL; break;
			case 'f': gfp -> force_ms = 1;               /* fall through */
			case 'j': gfp -> mode = MPG_MD_JOINT_STEREO; break;
			case 'm': gfp -> mode = MPG_MD_MONO;         break;
			default : fprintf(stderr,"%s: -m mode must be s/d/j/f/m not %s\n", ProgramName, arg);
			    err = 1;
			    break;
			}
			break;
	    
		    case 'V':        argUsed = 1;
			/* to change VBR default look in lame.h */
			if (gfp->VBR == vbr_off) gfp->VBR = vbr_default;  
			gfp->VBR_q = atoi(arg);
			if (gfp->VBR_q <0) gfp->VBR_q=0;
			if (gfp->VBR_q >9) gfp->VBR_q=9;
			gfp->quality = 2;
			break;
		    case 'q':        argUsed = 1; 
			user_quality = atoi(arg);
			if (user_quality<0) user_quality=0;
			if (user_quality>9) user_quality=9;
			break;
		    case 's':
			argUsed = 1;
			val = atof( arg );
			gfp->in_samplerate = val * ( val <= 192 ? 1.e3 : 1.e0 ) + 0.5;
			break;
		    case 'b':        
			argUsed = 1;
			gfp->brate = atoi(arg); 
			gfp->VBR_min_bitrate_kbps=gfp->brate;
			break;
		    case 'B':        
			argUsed = 1;
			gfp->VBR_max_bitrate_kbps=atoi(arg); 
			break;	
		    case 'F':        
			gfp->VBR_hard_min=1;
			break;	
		    case 't':  /* dont write VBR tag */
			gfp->bWriteVbrTag=0;
			gfp->disable_waveheader=1;
			break;
		    case 'r':  /* force raw pcm input file */
#if defined(LIBSNDFILE)
			fprintf(stderr,"WARNING: libsndfile may ignore -r and perform fseek's on the input.\n"
			       "Compile without libsndfile if this is a problem.\n");
#endif
			input_format=sf_raw;
			break;
		    case 'x':  /* force byte swapping */
			swapbytes=1;
			break;
		    case 'p': /* (jo) error_protection: add crc16 information to stream */
			gfp->error_protection = 1; 
			break;
		    case 'a': /* autoconvert input file from stereo to mono - for mono mp3 encoding */
			autoconvert=1;
			gfp->mode=MPG_MD_MONO;
			gfp->mode_fixed=1;
			break;
		    case 'h': 
			gfp->quality = 2;
			break;
		    case 'k': 
			gfp->lowpassfreq=-1;
			gfp->highpassfreq=-1;
			break;
		    case 'd': 
			gfp->allow_diff_short = 1;
			break;
		    case 'v': 
			/* to change VBR default look in lame.h */
			gfp->VBR = vbr_default; 
			gfp->quality = 2;
			break;
		    case 'S': 
			silent = 1;
			break;
		    case 'X':        
			argUsed = 1;   
			gfp->experimentalX = atoi(arg); 
			break;
		    case 'Y': 
			gfp->experimentalY = 1;
			break;
		    case 'Z': 
			gfp->experimentalZ = 1;
			break;
		    case 'f': 
			gfp->quality= 7;
			break;
#if defined(HAVEGTK)
		    case 'g': /* turn on gtk analysis */
			gfp->analysis = 1;
			break;
#endif			
		    case 'e':        
			argUsed = 1;
			
			switch (*arg) {
			case 'n': gfp -> emphasis = 0; break;
			case '5': gfp -> emphasis = 1; break;
			case 'c': gfp -> emphasis = 3; break;
			default : fprintf(stderr,"%s: -e emp must be n/5/c not %s\n", ProgramName, arg );
			    err = 1;
			    break;
			}
			break;
		    case 'c':   
		        gfp->copyright = 1; 
		        break;
		    case 'o':   
		        gfp->original  = 0; 
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
	    }	
	} else {
	    if (inPath [0] == '\0')       
	        strncpy(inPath , argv[i], MAX_NAME_SIZE);
	    else 
	    if (outPath[0] == '\0') 
	        strncpy(outPath, argv[i], MAX_NAME_SIZE);
	    else {
		fprintf(stderr,"%s: excess arg %s\n", ProgramName, argv[i]);
		err = 1;
	    }
	}
    }  /* loop over args */
    
    if ( err  ||  inPath[0] == '\0' ) {
        usage ( gfp, stderr, ProgramName );
        return -1;
    }
	
    if ( inPath[0] == '-' ) 
        silent = 1;  /* turn off status - it's broken for stdin */
	
    if ( outPath[0] == '\0' ) {
	if ( inPath[0] == '-' ) {
	    /* if input is stdin, default output is stdout */
	    strcpy(outPath,"-");
	} else {
	    strncpy(outPath, inPath, MAX_NAME_SIZE - 4);
	    if (gfp->decode_only) {
	        strncat (outPath, ".wav", 4 );
	    } else if (gfp->ogg) {
		strncat (outPath, ".ogg", 4 );
	    } else {
		strncat (outPath, ".mp3", 4 );
	    }
	}
    }
    /* some file options not allowed with stdout */
    if (outPath[0]=='-') {
	gfp->bWriteVbrTag=0; /* turn off VBR tag */
    }
    
    /* if user did not explicitly specify input is mp3, check file name */
    if (input_format == sf_unknown)
	input_format = filename_to_type ( inPath );
    
#if !(defined HAVEMPGLIB || defined AMIGA_MPEGA)
    if ( input_format == sf_mp1 ||
	 input_format == sf_mp2 ||
	 input_format == sf_mp3) {
	fprintf(stderr,"Error: libmp3lame not compiled with mpg123 *decoding* support \n");
	return -1;
    }
#endif

#if !(defined HAVEVORBIS)
    if ( input_format == sf_ogg ) {
        fprintf(stderr,"Error: LAME not compiled with Vorbis support\n");
	return -1;
    }
#endif
    /* default guess for number of channels */
    if (autoconvert) 
        gfp -> num_channels = 2; 
    else if (gfp->mode == MPG_MD_MONO) 
        gfp -> num_channels = 1;
    else 
        gfp -> num_channels = 2;
    
    /* user specified a quality value.  override any defaults set above */
    if (user_quality >= 0)   
        gfp -> quality = user_quality;
    
    if ( gfp->free_format ) {
	if ( gfp -> brate < 8  ||  gfp -> brate > 550 ) {
	    fprintf(stderr,"For free format, specify a bitrate between 8 and 320 (550) kbps\n");
	    return -1;
	}
    }
    return 0;
}

/* end of parse.c */
