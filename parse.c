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

#include <assert.h>
#include "util.h"
#include "id3tag.h"
#include "get_audio.h"
#include "brhist.h"
#include "version.h"




/************************************************************************
*
* license
*
* PURPOSE:  Writes version and license to the file specified by #stdout#
*
************************************************************************/

void lame_print_license ( lame_global_flags* gfp, const char* ProgramName )  /* print version,license & exit */
{
    LAME_PRINT_VERSION1 ();
    PRINTF1 ("\n");
    PRINTF1 ("Can I use LAME in my commercial program?\n");
    PRINTF1 ("\n");
    PRINTF1 ("Yes, you can, under the restrictions of the LGPL.  In particular, you\n");
    PRINTF1 ("can include a compiled version of the LAME library (for example,\n");
    PRINTF1 ("lame.dll) with a commercial program.  Some notable requirements of\n");
    PRINTF1 ("the LGPL:\n");
    PRINTF1 ("\n");
    PRINTF1 ("1. In your program, you cannot include any source code from LAME, with\n");
    PRINTF1 ("   the exception of files whose only purpose is to describe the library\n");
    PRINTF1 ("   interface (such as lame.h).\n");
    PRINTF1 ("\n");
    PRINTF1 ("2. Any modifications of LAME must be released under the LGPL.\n");
    PRINTF1 ("   The LAME project (www.mp3dev.org) would appreciate being\n");
    PRINTF1 ("   notified of any modifications.\n");
    PRINTF1 ("\n");
    PRINTF1 ("3. You must give prominent notice that your program is:\n");
    PRINTF1 ("      A. using LAME (including version number)\n");
    PRINTF1 ("      B. LAME is under the LGPL\n");
    PRINTF1 ("      C. Provide a copy of the LGPL.  (the file COPYING contains the LGPL)\n");
    PRINTF1 ("      D. Provide a copy of LAME source, or a pointer where the LAME\n");
    PRINTF1 ("         source can be obtained (such as www.mp3dev.org)\n");
    PRINTF1 ("   An example of prominent notice would be an \"About the LAME encoding engine\"\n");
    PRINTF1 ("   button in some pull down menu within the executable of your program.\n");
    PRINTF1 ("\n");
    PRINTF1 ("4. If you determine that distribution of LAME requires a patent license,\n");
    PRINTF1 ("   you must obtain such license.\n");
    PRINTF1 ("\n");
    PRINTF1 ("\n");
    PRINTF1 ("*** IMPORTANT NOTE ***\n");
    PRINTF1 ("\n");
    PRINTF1 ("The decoding functions provided in LAME use the mpglib decoding engine which\n");
    PRINTF1 ("is under the GPL.  They may not be used by any program not released under the\n");
    PRINTF1 ("GPL unless you obtain such permission from the MPG123 project (www.mpg123.de).\n");
    PRINTF1 ("\n");
    PRINTF1 ("LAME has built-in support to read raw pcm and some wav and aiff files. More\n");
    PRINTF1 ("robust file I/O can be handled by compiling in LIBSNDFILE, but LIBSNDFILE is\n");
    PRINTF1 ("also under the GPL and may not be used by other programs not under the GPL.\n");
    PRINTF1 ("\n");
    LAME_NORMAL_EXIT ();
}



/************************************************************************
*
* usage
*
* PURPOSE:  Writes command line syntax to the file specified by #stderr#
*
************************************************************************/

void lame_usage ( lame_global_flags* gfp, const char* ProgramName )  /* print syntax & exit */
{
  LAME_PRINT_VERSION2();
  PRINTF2("\n");
  PRINTF2("usage: %s [options] <infile> [outfile]\n", ProgramName );
  PRINTF2("\n<infile> and/or <outfile> can be \"-\", which means stdin/stdout.\n");
  PRINTF2("\n");
  PRINTF2("Try \"%s --help\"     for more information\n", ProgramName );
  PRINTF2(" or \"%s --longhelp\" for a complete options list\n\n", ProgramName );
  LAME_ERROR_EXIT();
}



/************************************************************************
*
* usage
*
* PURPOSE:  Writes command line syntax to the file specified by #stdout#
*           but only the most important ones, to fit on a vt100 terminal
*
************************************************************************/

void lame_short_help ( lame_global_flags* gfp, const char* ProgramName )  /* print syntax & exit */
{
  LAME_PRINT_VERSION1(); /* prints two lines */
  PRINTF1("\n");
  PRINTF1("usage: %s [options] <infile> [outfile]\n", ProgramName );
  PRINTF1("\n<infile> and/or <outfile> can be \"-\", which means stdin/stdout.\n");
  PRINTF1("\n");
  PRINTF1("RECOMMENDED :  ");
  PRINTF1("lame -h input.wav output.mp3\n");
  PRINTF1("\n");
  PRINTF1("OPTIONS :\n");
  PRINTF1("    -b bitrate      set the bitrate, default 128 kbps\n");
  PRINTF1("    -f              fast mode (lower quality)\n");
  PRINTF1("    -h              higher quality, but a little slower.  Recommended.\n");
  PRINTF1("    -k              keep ALL frequencies (disables all filters)\n");
  PRINTF1("                    Can cause ringing and twinkling\n");
  PRINTF1("    -m mode         (s)tereo, (j)oint, (f)orce or (m)ono  (default j)\n");
  PRINTF1("                    force = force ms_stereo on all frames.\n");
  PRINTF1("    -V n            quality setting for VBR.  default n=%i\n",gfp->VBR_q);
  PRINTF1("\n");
  PRINTF1("    --preset type   type must be phone, voice, fm, tape, hifi, cd or studio\n");
  PRINTF1("                    \"--preset help\" gives some more infos on these\n");
  PRINTF1("\n");
  PRINTF1("    --longhelp      full list of options\n");
  PRINTF1("\n");
 
  LAME_NORMAL_EXIT();
}

/************************************************************************
*
* usage
*
* PURPOSE:  Writes command line syntax to the file specified by #stdout#
*
************************************************************************/

void lame_help ( lame_global_flags* gfp, const char* ProgramName )  /* print syntax & exit */
{
  LAME_PRINT_VERSION1();
  PRINTF1("\n");
  PRINTF1("usage: %s [options] <infile> [outfile]\n", ProgramName );
  PRINTF1("\n<infile> and/or <outfile> can be \"-\", which means stdin/stdout.\n");
  PRINTF1("\n");
  PRINTF1("RECOMMENDED :  ");
  PRINTF1("lame -h input.mp3 output.wav\n");
  PRINTF1("\n");
  PRINTF1("OPTIONS :\n");
  PRINTF1("  Input options:\n");
  PRINTF1("    -r              input is raw pcm\n");
  PRINTF1("    -x              force byte-swapping of input\n");
  PRINTF1("    -s sfreq        sampling frequency of input file(kHz) - default 44.1kHz\n");
  PRINTF1("    --mp1input      input file is a MPEG LayerI   file\n");
  PRINTF1("    --mp2input      input file is a MPEG LayerII  file\n");
  PRINTF1("    --mp3input      input file is a MPEG LayerIII file\n");
  PRINTF1("    --ogginput      input file is a Ogg Vorbis file\n");
  PRINTF1("\n");
  PRINTF1("  Operational options:\n");
  PRINTF1("    -m <mode>       (s)tereo, (j)oint, (f)orce or (m)ono  (default j)\n");
  PRINTF1("                    force = force ms_stereo on all frames.\n");
  PRINTF1("    -a              downmix from stereo to mono file for mono encoding\n");
  PRINTF1("    -d              allow channels to have different blocktypes\n");
  PRINTF1("    -S              don't print progress report, VBR histograms\n");
  PRINTF1("    --disptime <arg>print progress report every arg seconds \n");
  PRINTF1("    --ogg           encode to Ogg Vorbis instead of MP3\n");
  PRINTF1("    --freeformat    produce a free format bitstream\n");
  PRINTF1("    --decode        input=mp3 file, output=wav\n");
  PRINTF1("    -t              disable writing wav header when using --decode\n");
  PRINTF1("    --comp  <arg>   choose bitrate to achive a compression ratio of <arg>\n");
  PRINTF1("    --scale <arg>   scale input (multiply PCM data) by <arg>  \n");
  PRINTF1("    --athonly       only use the ATH for masking\n");
  PRINTF1("    --noath         disable the ATH for masking\n");
  PRINTF1("    --athlower x    lower the ATH x dB\n");
  PRINTF1("    --raise-smr x   0 <= x <= 1, 0 default, 1 maximum SMR\n");
  PRINTF1("    --short         use short blocks\n");
  PRINTF1("    --noshort       do not use short blocks\n");
  PRINTF1("    --voice         experimental voice mode\n");
  PRINTF1("    --preset type   type must be phone, voice, fm, tape, hifi, cd or studio\n");
  PRINTF1("                    \"--preset help\" gives some more infos on these\n");
  PRINTF1("\n");
  PRINTF1("  CBR (constant bitrate, the default) options:\n");
  PRINTF1("    -h              higher quality, but a little slower.  Recommended.\n");
  PRINTF1("    -f              fast mode (lower quality)\n");
  PRINTF1("    -b <bitrate>    set the bitrate in kbps, default 128 kbps\n");
  PRINTF1("\n");
  PRINTF1("  ABR options:\n");
  PRINTF1("    --abr <bitrate> specify average bitrate desired (instead of quality)\n");
  PRINTF1("\n");
  PRINTF1("  VBR options:\n");
  PRINTF1("    -v              use variable bitrate (VBR)\n");
  PRINTF1("    --vbr-old       use old variable bitrate (VBR) routine\n");
  PRINTF1("    --vbr-new       use new variable bitrate (VBR) routine\n");
  PRINTF1("    -V n            quality setting for VBR.  default n=%i\n",gfp->VBR_q);
  PRINTF1("                    0=high quality,bigger files. 9=smaller files\n");
  PRINTF1("    -b <bitrate>    specify minimum allowed bitrate, default  32 kbps\n");
  PRINTF1("    -B <bitrate>    specify maximum allowed bitrate, default 320 kbps\n");
  PRINTF1("    -F              strictly enforce the -b option, for use with players that\n");
  PRINTF1("                    do not support low bitrate mp3 (Apex AD600-A DVD/mp3 player)\n");
  PRINTF1("    -t              disable writing Xing VBR informational tag\n");
  PRINTF1("    --nohist        disable VBR histogram display\n");
  PRINTF1("\n");
  PRINTF1("  MP3 header/stream options:\n");
  PRINTF1("    -e <emp>        de-emphasis n/5/c  (obsolete)\n");
  PRINTF1("    -c              mark as copyright\n");
  PRINTF1("    -o              mark as non-original\n");
  PRINTF1("    -p              error protection.  adds 16bit checksum to every frame\n");
  PRINTF1("                    (the checksum is computed correctly)\n");
  PRINTF1("    --nores         disable the bit reservoir\n");
  PRINTF1("    --strictly-enforce-ISO   comply as much as possible to ISO MPEG spec\n");
  PRINTF1("\n");
  PRINTF1("  Filter options:\n");
  PRINTF1("    -k              keep ALL frequencies (disables all filters),\n");
  PRINTF1("                    Can cause ringing and twinkling\n");
  PRINTF1("  --lowpass <freq>        frequency(kHz), lowpass filter cutoff above freq\n");
  PRINTF1("  --lowpass-width <freq>  frequency(kHz) - default 15%% of lowpass freq\n");
  PRINTF1("  --highpass <freq>       frequency(kHz), highpass filter cutoff below freq\n");
  PRINTF1("  --highpass-width <freq> frequency(kHz) - default 15%% of highpass freq\n");
  PRINTF1("  --resample <sfreq>  sampling frequency of output file(kHz)- default=automatic\n");
  PRINTF1("  --cwlimit <freq>    compute tonality up to freq (in kHz) default 8.8717\n");
  PRINTF1("\n");
  PRINTF1("  ID3 tag options:\n");
  PRINTF1("    --tt <title>    audio/song title (max 30 chars for version 1 tag)\n");
  PRINTF1("    --ta <artist>   audio/song artist (max 30 chars for version 1 tag)\n");
  PRINTF1("    --tl <album>    audio/song album (max 30 chars for version 1 tag)\n");
  PRINTF1("    --ty <year>     audio/song year of issue (1 to 9999)\n");
  PRINTF1("    --tc <comment>  user-defined text (max 30 chars for v1 tag, 28 for v1.1)\n");
  PRINTF1("    --tn <track>    audio/song track number (1 to 99, creates v1.1 tag)\n");
  PRINTF1("    --tg <genre>    audio/song genre (name or number in list)\n");
  PRINTF1("    --add-id3v2     force addition of version 2 tag\n");
  PRINTF1("    --id3v1-only    add only a version 1 tag\n");
  PRINTF1("    --id3v2-only    add only a version 2 tag\n");
  PRINTF1("    --space-id3v1   pad version 1 tag with spaces instead of nulls\n");
  PRINTF1("    --pad-id3v2     pad version 2 tag with extra 128 bytes\n");
  PRINTF1("    --genre-list    print alphabetically sorted ID3 genre list and exit\n");
  PRINTF1("\n");
  PRINTF1("    Note: A version 2 tag will NOT be added unless one of the input fields\n");
  PRINTF1("    won't fit in a version 1 tag (e.g. the title string is longer than 30\n");
  PRINTF1("    characters), or the `--add-id3v2' or `--id3v2-only' options are used,\n");
  PRINTF1("    or output is redirected to stdout.\n");
  PRINTF1("\n");
#ifdef HAVEVORBIS
  PRINTF1("    Note: All `--t*' options (except those for track and genre) work for Ogg\n");
  PRINTF1("    Vorbis output, but other ID3-specific options are ignored.\n");
  PRINTF1("\n");
#endif
#ifdef HAVEGTK
  PRINTF1("    -g              run graphical analysis on <infile>\n");
#endif
  DISPLAY_BITRATES1();
  LAME_NORMAL_EXIT();
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

/*
 * block 1 uses the best selection from block 2 and 3
 * block 2 uses lower fs and uses fsb21		 (voice: 1.8 MB, phon+ 697 KB)
 * block 3 uses higher fs and avoid fsb21 usage  (voice: 2.0 MB, phon+ 614 KB)
 */


const preset_t Presets [] = {
   // name       fs     fu    fo    dfo shrt qual  mode              cbr vbr_mode/min/max
#if 1
    { "phone" ,  8000, 125,  3400,    0,  1,  5, MPG_MD_MONO        ,  16,  6,   8,  24 },  // phone standard 300-3400
    { "phon+" , 11025, 100,  4000,    0,  1,  5, MPG_MD_MONO        ,  24,  4,  16,  32 },  // phon
    { "lw"    , 11025,  -1,  4000,    0,  0,  5, MPG_MD_MONO        ,  24,  3,  16,  56 },  // LW
    { "mw-eu" , 11025,  -1,  4000,    0,  0,  5, MPG_MD_MONO        ,  24,  3,  16,  56 },  // MW i
    { "mw-us" , 16000,  -1,  7500,    0,  0,  5, MPG_MD_MONO        ,  40,  3,  24, 112 },  // MW i
    { "sw"    , 11025,  -1,  4000,    0,  0,  5, MPG_MD_MONO        ,  24,  3,  16,  56 },  // SW
    { "fm"    , 32000,  -1, 15000,    0,  0,  5, MPG_MD_JOINT_STEREO, 112,  3,  80, 256 },
    { "voice" , 24000,  -1, 12000,    0,  1,  5, MPG_MD_MONO        ,  56,  4,  40, 112 },
    { "radio" ,    -1,  -1, 15000,    0,  0,  5, MPG_MD_JOINT_STEREO, 128,  3,  96, 256 },
    { "tape"  ,    -1,  -1, 18000,  900,  0,  5, MPG_MD_JOINT_STEREO, 128,  3,  96, 256 },
    { "hifi"  ,    -1,  -1, 18000,  900,  0,  2, MPG_MD_JOINT_STEREO, 160,  2, 112, 320 },
    { "cd"    ,    -1,  -1,    -1,   -1,  0,  2, MPG_MD_STEREO      , 192,  1, 128, 320 },
    { "studio",    -1,  -1,    -1,   -1,  0,  2, MPG_MD_STEREO      , 256,  0, 160, 320 },
#elif 0
    { "phone" ,  8000, 125,  3400,    0,  1,  5, MPG_MD_MONO        ,  16,  6,   8,  24 },  // phone standard 300-3400
    { "phon+" ,  8000, 100,  4000,    0,  1,  5, MPG_MD_MONO        ,  24,  4,  16,  32 },  // phone theoretical limits
    { "lw"    , 11025,  -1,  4000,    0,  0,  5, MPG_MD_MONO        ,  24,  3,  16,  56 },  // LW           use df= 9 kHz
    { "mw-eu" , 11025,  -1,  4000,    0,  0,  5, MPG_MD_MONO        ,  24,  3,  16,  56 },  // MW in europe use df= 9 kHz
    { "mw-us" , 16000,  -1,  7500,    0,  0,  5, MPG_MD_MONO        ,  40,  3,  24, 112 },  // MW in USA    use df=20 kHz
    { "sw"    , 11025,  -1,  4000,    0,  0,  5, MPG_MD_MONO        ,  24,  3,  16,  56 },  // SW           use df=10 kHz
    { "fm"    , 32000,  -1, 15000,    0,  0,  5, MPG_MD_JOINT_STEREO, 112,  3,  80, 256 },
    { "voice" , 24000,  -1, 12000,    0,  1,  5, MPG_MD_MONO        ,  56,  4,  40, 112 },
    { "radio" ,    -1,  -1, 15000,    0,  0,  5, MPG_MD_JOINT_STEREO, 128,  3,  96, 256 },
    { "tape"  ,    -1,  -1, 18000,  900,  0,  5, MPG_MD_JOINT_STEREO, 128,  3,  96, 256 },
    { "hifi"  ,    -1,  -1, 18000,  900,  0,  2, MPG_MD_JOINT_STEREO, 160,  2, 112, 320 },
    { "cd"    ,    -1,  -1,    -1,   -1,  0,  2, MPG_MD_STEREO      , 192,  1, 128, 320 },
    { "studio",    -1,  -1,    -1,   -1,  0,  2, MPG_MD_STEREO      , 256,  0, 160, 320 },
#else
    { "phone" ,  8000, 125,  3400,    0,  1,  5, MPG_MD_MONO        ,  16,  6,   8,  24 },  // phon
    { "phon+" , 11025, 100,  4000,    0,  1,  5, MPG_MD_MONO        ,  24,  4,  16,  32 },  // phon
    { "lw"    , 11025,  -1,  4000,    0,  0,  5, MPG_MD_MONO        ,  24,  3,  16,  56 },  // LW
    { "mw-eu" , 11025,  -1,  4000,    0,  0,  5, MPG_MD_MONO        ,  24,  3,  16,  56 },  // MW i
    { "mw-us" , 16000,  -1,  7500,    0,  0,  5, MPG_MD_MONO        ,  40,  3,  24, 112 },  // MW i
    { "sw"    , 11025,  -1,  4000,    0,  0,  5, MPG_MD_MONO        ,  24,  3,  16,  56 },  // SW
    { "fm"    , 32000,  -1, 15000,    0,  0,  5, MPG_MD_JOINT_STEREO, 112,  3,  80, 256 },
    { "voice" , 32000,  -1, 12000,    0,  1,  5, MPG_MD_MONO        ,  56,  4,  40, 112 },
    { "radio" ,    -1,  -1, 15000,    0,  0,  5, MPG_MD_JOINT_STEREO, 128,  3,  96, 256 },
    { "tape"  ,    -1,  -1, 18000,  900,  0,  5, MPG_MD_JOINT_STEREO, 128,  3,  96, 256 },
    { "hifi"  ,    -1,  -1, 18000,  900,  0,  2, MPG_MD_JOINT_STEREO, 160,  2, 112, 320 },
    { "cd"    ,    -1,  -1,    -1,   -1,  0,  2, MPG_MD_STEREO      , 192,  1, 128, 320 },
    { "studio",    -1,  -1,    -1,   -1,  0,  2, MPG_MD_STEREO      , 256,  0, 160, 320 },
#endif    
};


void lame_presets_info ( lame_global_flags* gfp, const char* ProgramName )  /* print syntax & exit */
{
    size_t  i;

    PRINTF1 ("\n");    
    LAME_PRINT_VERSION1 ();
    PRINTF1 ("\n");
    PRINTF1 ("Presets are some shortcuts for common settings.\n");
    PRINTF1 ("They can be combined with -v if you want VBR MP3s.\n");
    
    PRINTF1 ("\n                ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        PRINTF1 ( strlen(Presets[i].name) <= 4 ? "%5s " : " %-5s", Presets[i].name );
    PRINTF1 ("\n=================");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        PRINTF1 ( "======" );
    PRINTF1 ("\n--resample      ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        if ( Presets[i].resample < 0 )
            PRINTF1 ( "      " );
        else
            PRINTF1 ( "%6.3g",  Presets[i].resample*1.e-3 );
    PRINTF1 ("\n--highpass      ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        if ( Presets[i].highpass_freq < 0 )
            PRINTF1 ( "      " );
        else
            PRINTF1 ( "%6.3g",  Presets[i].highpass_freq*1.e-3 );
    PRINTF1 ("\n--lowpass       ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        if ( Presets[i].lowpass_freq < 0 )
            PRINTF1 ( "      " );
        else
            PRINTF1 ( "%6.3g",  Presets[i].lowpass_freq*1.e-3 );
    PRINTF1 ("\n--lowpass-width ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        if ( Presets[i].lowpass_width < 0 )
            PRINTF1 ( "      " );
        else
            PRINTF1 ( "%6.3g",  Presets[i].lowpass_width*1.e-3 );
    PRINTF1 ("\n--noshort       ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        switch ( Presets[i].no_short_blocks ) {
        case  1: PRINTF1 ( "   yes" ); break;
        case  0: PRINTF1 ( "    no" ); break;
        case -1: PRINTF1 ( "      " ); break;
        default: assert (0);           break;
        }
    PRINTF1 ("\n                ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        switch ( Presets[i].mode ) {
        case MPG_MD_MONO:         PRINTF1 ("   -mm"); break;
        case MPG_MD_JOINT_STEREO: PRINTF1 ("   -mj"); break;
        case MPG_MD_STEREO:       PRINTF1 ("   -ms"); break;
        case -1:                  PRINTF1 ("      "); break;
        default:                  assert (0);         break;
        }
    PRINTF1 ("\n                ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        switch ( Presets[i].quality ) {
	case  2: PRINTF1 ("    -h"); break;
	case  5: PRINTF1 ("      "); break;
	case  7: PRINTF1 ("    -f"); break;
	default: assert (0);         break;
    }
    PRINTF1 ("\n-b              ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        PRINTF1 ( "%6u", Presets[i].cbr );
    PRINTF1 ("\n-- PLUS WITH -v ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        PRINTF1 ( "------" );
    PRINTF1 ("-\n-V              ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        PRINTF1 ( "%6u", Presets[i].vbr_mode );
    PRINTF1 ("\n-b              ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        PRINTF1 ( "%6u", Presets[i].vbr_min );
    PRINTF1 ("\n-B              ");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        PRINTF1 ( "%6u", Presets[i].vbr_max );
    PRINTF1 ("\n----------------");
    for ( i = 0; i < sizeof(Presets)/sizeof(*Presets); i++)
        PRINTF1 ( "------" );
  
    PRINTF1 ("-\nEXAMPLES:\n");
    PRINTF1 (" a) --preset fm\n");
    PRINTF1 ("    equal to: -mj -b112 --resample 32 --lowpass 15 --lowpass-width 0\n");
    PRINTF1 (" b) -v --preset studio\n");
    PRINTF1 ("    equals to: -h -ms -V0 -b160 -B320\n");

    LAME_NORMAL_EXIT ();
}


void lame_presets_setup ( lame_global_flags* gfp, const char* preset_name, const char* ProgramName )
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
	    return;
	}

    lame_presets_info ( gfp, ProgramName );
    assert (0);
}


static void genre_list_handler (int num,const char *name,void *cookie)
{
    PRINTF1 ("%3d %s\n", num, name);
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
    default: ERRORF ("Illegal resample frequency: %.3f kHz\n", freq );
             return 0;
    }
}

/* Ugly, NOT final version */

#define T_IF(str)          if ( 0 == local_strcasecmp (token,str) ) {
#define T_ELIF(str)        } else if ( 0 == local_strcasecmp (token,str) ) {
#define T_ELIF2(str1,str2) } else if ( 0 == local_strcasecmp (token,str1)  ||  0 == local_strcasecmp (token,str2) ) {
#define T_ELSE             } else {
#define T_END              }

void lame_parse_args ( lame_global_flags* gfp, int argc, char** argv )
{
    int         err;
    int         i;
    int         autoconvert  = 0;
    int         user_quality = -1;
    double      freq;
    const char* ProgramName  = argv[0]; 

    gfp -> inPath [0] = '\0';   
    gfp -> outPath[0] = '\0';
    /* turn on display options. user settings may turn them off below */
    gfp -> silent        = 0;
    gfp -> brhist_disp   = 1;
    gfp -> id3v1_enabled = 1;
    id3tag_init (&gfp->tag_spec);

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
	        if (gfp -> inPath [0] == '\0')
		    strncpy (gfp->inPath, argv[i],MAX_NAME_SIZE);
	        else 
		if (gfp -> outPath[0] == '\0') 
		    strncpy (gfp->outPath, argv[i],MAX_NAME_SIZE);
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
		    gfp->input_format=sf_mp1;
		
		T_ELIF ("mp2input")
		    gfp->input_format=sf_mp2;
		
		T_ELIF ("mp3input")
		    gfp->input_format=sf_mp3;
		
		T_ELIF ("ogginput")
#ifdef HAVEVORBIS
		    gfp->input_format=sf_ogg;
#else
		    ERRORF("Error: LAME not compiled with Vorbis support\n");
		    LAME_ERROR_EXIT();
#endif
		T_ELIF ("ogg")
#ifdef HAVEVORBIS
		    gfp->ogg=1;
#else
		    ERRORF("Error: LAME not compiled with Vorbis support\n");
		    LAME_ERROR_EXIT();
#endif
		T_ELIF ("phone")
                    lame_presets_setup ( gfp, token, ProgramName );
                    
		T_ELIF ("voice")
                    lame_presets_setup ( gfp, token, ProgramName );
                    
		T_ELIF ("radio")
                    lame_presets_setup ( gfp, token, ProgramName );
                    
		T_ELIF ("tape")
                    lame_presets_setup ( gfp, token, ProgramName );
                    
		T_ELIF ("cd")
                    lame_presets_setup ( gfp, token, ProgramName );
                    
		T_ELIF ("studio")
                    lame_presets_setup ( gfp, token, ProgramName );
                    
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
		
		T_ELIF ("raise-smr")
		    argUsed=1;
		    gfp->raiseSMR = atof(nextArg);
                    if (gfp->raiseSMR < 0.0) gfp->raiseSMR = 0.0;
                    if (gfp->raiseSMR > 1.0) gfp->raiseSMR = 1.0;

		T_ELIF ("scale")
		    argUsed=1;
		    gfp->scale = atof(nextArg);
		
		T_ELIF ("freeformat")
		    gfp->free_format=1;
		
		T_ELIF ("athshort")
		    gfp->ATHshort=1;
		
		T_ELIF ("nohist")
		    gfp->brhist_disp = 0;
		
		/* options for ID3 tag */
		T_ELIF ("tt")
		    argUsed=1;
		    id3tag_set_title(&gfp->tag_spec, nextArg);
		
		T_ELIF ("ta")
		    argUsed=1;
		    id3tag_set_artist(&gfp->tag_spec, nextArg);
		
		T_ELIF ("tl")
		    argUsed=1;
		    id3tag_set_album(&gfp->tag_spec, nextArg);
		
		T_ELIF ("ty")
		    argUsed=1;
		    id3tag_set_year(&gfp->tag_spec, nextArg);
		
		T_ELIF ("tc")
		    argUsed=1;
		    id3tag_set_comment(&gfp->tag_spec, nextArg);
		
		T_ELIF ("tn")
		    argUsed=1;
		    id3tag_set_track(&gfp->tag_spec, nextArg);
		
		T_ELIF ("tg")
		    argUsed=1;
		    if (id3tag_set_genre(&gfp->tag_spec, nextArg)) {
			ERRORF("Unknown genre: %s.  Specify genre name or number\n", nextArg);
			LAME_ERROR_EXIT();
		    }
		
		T_ELIF ("add-id3v2")
		    id3tag_add_v2(&gfp->tag_spec);
		
		T_ELIF ("id3v1-only")
		    id3tag_v1_only(&gfp->tag_spec);
		
		T_ELIF ("id3v2-only")
		    id3tag_v2_only(&gfp->tag_spec);
		
		T_ELIF ("space-id3v1")
		    id3tag_space_v1(&gfp->tag_spec);
		
		T_ELIF ("pad-id3v2")
		    id3tag_pad_v2(&gfp->tag_spec);
		
		T_ELIF ("genre-list")
		    id3tag_genre_list(genre_list_handler, NULL);
		    LAME_NORMAL_EXIT();
		
		T_ELIF ("lowpass")
		    freq = atof( nextArg );
		    argUsed=1;
		    /* useful are 0.001 kHz...50 kHz, 50 Hz...50000 Hz */
		    gfp -> lowpassfreq = freq * (freq < 50. ? 1.e3 : 1.e0 ) + 0.5;
		    if ( freq < 0.001 || freq > 50000. ) {
			ERRORF("Must specify lowpass with --lowpass freq, freq >= 0.001 kHz\n");
			LAME_ERROR_EXIT();
		    }
		
		T_ELIF ("lowpass-width")
		    argUsed=1;
		    gfp->lowpasswidth =  1000.0 * atof( nextArg ) + 0.5;
		    if (gfp->lowpasswidth  < 0) {
			ERRORF("Must specify lowpass width with --lowpass-width freq, freq >= 0 kHz\n");
			LAME_ERROR_EXIT();
		    }
		
		T_ELIF ("highpass")
		    freq = atof( nextArg );
		    argUsed=1;
		    /* useful are 0.001 kHz...16 kHz, 16 Hz...50000 Hz */
		    gfp->highpassfreq =  freq * (freq < 16. ? 1.e3 : 1.e0 ) + 0.5;
		    if ( freq < 0.001 || freq > 50000. ) {
			ERRORF("Must specify highpass with --highpass freq, freq >= 0.001 kHz\n");
			LAME_ERROR_EXIT();
		    }
		
		T_ELIF ("highpass-width")
		    argUsed=1;
		    gfp->highpasswidth =  1000.0 * atof( nextArg ) + 0.5;
		    if (gfp->highpasswidth  < 0) {
			ERRORF("Must specify highpass width with --highpass-width freq, freq >= 0 kHz\n");
			LAME_ERROR_EXIT();
		    }
		
		T_ELIF ("cwlimit")
		    freq = atof (nextArg);
		    argUsed=1;
		    /* useful are 0.001 kHz...50 kHz, 50 Hz...50000 Hz */
		    gfp -> cwlimit = freq * ( freq <= 50. ? 1.e3 : 1.e0 );
		    if (gfp->cwlimit <= 0 ) {
			ERRORF("Must specify cwlimit with --cwlimit freq, freq >= 0.001 kHz\n");
			LAME_ERROR_EXIT();
		    }
		 
		T_ELIF ("comp")
		    argUsed=1;
		    gfp->compression_ratio =  atof( nextArg );
		    if (gfp->compression_ratio < 1.0 ) {
			ERRORF("Must specify compression ratio >= 1.0\n");
			LAME_ERROR_EXIT();
		    }
		
		T_ELIF ("nspsytune")
		    gfp->exp_nspsytune = TRUE;
		    gfp->experimentalZ = TRUE;
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
		    lame_print_license (gfp,ProgramName);  /* doesn't return */
		
		T_ELIF2 ("help", "usage")
		    lame_short_help (gfp,ProgramName);  /* doesn't return */
		
		T_ELIF ("longhelp")
		    lame_help (gfp,ProgramName);  /* doesn't return */
		
		T_ELIF ("preset")
		    argUsed = 1;
		    lame_presets_setup ( gfp, nextArg, ProgramName );
                T_ELIF ("disptime")
                    argUsed = 1;
		    gfp -> update_interval = atof (nextArg);
		T_ELSE
		    ERRORF ("%s: unrec option --%s\n", ProgramName, token);
		    
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
			default : ERRORF ("%s: -m mode must be s/d/j/f/m not %s\n", ProgramName, arg);
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
			freq = atof( arg );
			gfp->in_samplerate = ( 1000.0 * freq ) + 0.5;
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
#ifdef LIBSNDFILE
			ERRORF("WARNING: libsndfile may ignore -r and perform fseek's on the input.\n"
			       "Compile without libsndfile if this is a problem.\n");
#endif
			gfp->input_format=sf_raw;
			break;
		    case 'x':  /* force byte swapping */
			gfp->swapbytes=TRUE;
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
			gfp->silent = TRUE;
			break;
		    case 'X':        
			argUsed = 1;   
			gfp->experimentalX = atoi(arg); 
			break;
		    case 'Y': 
			gfp->experimentalY = TRUE;
			break;
		    case 'Z': 
			gfp->experimentalZ = TRUE;
			break;
		    case 'f': 
			gfp->quality= 7;
			break;
		    case 'g': /* turn on gtk analysis */
			gfp->gtkflag = TRUE;
			break;
			
		    case 'e':        
			argUsed = 1;
			
			switch (*arg) {
			case 'n': gfp -> emphasis = 0; break;
			case '5': gfp -> emphasis = 1; break;
			case 'c': gfp -> emphasis = 3; break;
			default : ERRORF ("%s: -e emp must be n/5/c not %s\n", ProgramName, arg );
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
		        lame_help(gfp,ProgramName);  /* doesn't return */
		        
		    default:    
		        ERRORF("%s: unrec option %c\n", ProgramName, c);
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
	    if (gfp->inPath [0] == '\0')       
	        strncpy(gfp->inPath , argv[i], MAX_NAME_SIZE);
	    else 
	    if (gfp->outPath[0] == '\0') 
	        strncpy(gfp->outPath, argv[i], MAX_NAME_SIZE);
	    else {
		ERRORF("%s: excess arg %s\n", ProgramName, argv[i]);
		err = 1;
	    }
	}
    }  /* loop over args */
    
    if ( err || gfp->inPath[0] == '\0' ) 
        lame_usage (gfp,ProgramName);  /* never returns */
	
    if ( gfp->inPath[0] == '-' ) 
        gfp -> silent = 1;  /* turn off status - it's broken for stdin */
	
    if ( gfp->outPath[0] == '\0' ) {
	if ( gfp->inPath[0] == '-' ) {
	    /* if input is stdin, default output is stdout */
	    strcpy(gfp->outPath,"-");
	} else {
	    strncpy(gfp->outPath, gfp->inPath, MAX_NAME_SIZE - 4);
	    if (gfp->decode_only) {
	        strncat (gfp->outPath, ".wav", 4 );
	    } else if (gfp->ogg) {
		strncat (gfp->outPath, ".ogg", 4 );
	    } else {
		strncat (gfp->outPath, ".mp3", 4 );
	    }
	}
    }
    /* some file options not allowed with stdout */
    if (gfp->outPath[0]=='-') {
	gfp->bWriteVbrTag=0; /* turn off VBR tag */
	if (gfp->id3v1_enabled) {
	    gfp->id3v1_enabled=0;     /* turn off ID3 version 1 tagging */
	    ERRORF("ID3 version 1 tagging not supported for stdout.\n");
	    ERRORF("Converting any ID3 tag data to version 2 format.\n");
	    id3tag_v2_only(&gfp->tag_spec);
	}
    }
    
    /* if user did not explicitly specify input is mp3, check file name */
    if (gfp -> input_format == sf_unknown)
	gfp -> input_format = filename_to_type ( gfp -> inPath );
    
#if !(defined HAVEMPGLIB || defined AMIGA_MPEGA)
    if ( gfp->input_format == sf_mp1 ||
	 gfp->input_format == sf_mp2 ||
	 gfp->input_format == sf_mp3) {
	ERRORF("Error: libmp3lame not compiled with mpg123 *decoding* support \n");
	LAME_ERROR_EXIT();
    }
#endif

#if !(defined HAVEVORBIS)
    if ( gfp->input_format == sf_ogg ) {
        ERRORF("Error: LAME not compiled with Vorbis support\n");
        LAME_ERROR_EXIT();
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
	    ERRORF("For free format, specify a bitrate between 8 and 320 (550) kbps\n");
	    LAME_ERROR_EXIT();
	}
    }
    
}
