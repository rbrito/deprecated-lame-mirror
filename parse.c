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
#include "version.h"




/************************************************************************
*
* usage
*
* PURPOSE:  Writes command line syntax to the file specified by #stdout#
*
************************************************************************/

void lame_parse_help( lame_global_flags* gfp)
{
  PRINTF1("  encoding options:\n");
  PRINTF1("    -m <mode>       (s)tereo, (j)oint, (f)orce or (m)ono  (default j)\n");
  PRINTF1("                    force = force ms_stereo on all frames.\n");
  PRINTF1("    -d              allow channels to have different blocktypes\n");
  PRINTF1("    --freeformat    produce a free format bitstream\n");
  PRINTF1("    --comp  <arg>   choose bitrate to achive a compression ratio of <arg>\n");
  PRINTF1("    --scale <arg>   scale input (multiply PCM data) by <arg>  \n");
  PRINTF1("    --athonly       only use the ATH for masking\n");
  PRINTF1("    --noath         disable the ATH for masking\n");
  PRINTF1("    --athlower x    lower the ATH x dB\n");
  PRINTF1("    --raise-smr x   0 <= x <= 1, 0 default, 1 maximum SMR\n");
  PRINTF1("    --short         use short blocks\n");
  PRINTF1("    --noshort       do not use short blocks\n");
  PRINTF1("    --voice         experimental voice mode\n");
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
  PRINTF1("    characters), or the `--add-id3v2' or `--id3v2-only' options are used.\n");
  PRINTF1("\n");
#ifdef HAVEVORBIS
  PRINTF1("    Note: All `--t*' options (except those for track and genre) work for Ogg\n");
  PRINTF1("    Vorbis output, but other ID3-specific options are ignored.\n");
  PRINTF1("\n");
#endif
}


/************************************************************************
*
* license
*
* PURPOSE:  Writes version and license to the file specified by #stdout#
*
************************************************************************/

void lame_print_license ( lame_global_flags* gfp, const char* ProgramName )
{
    lame_print_version(stdout);
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
}



/*
 * block 1 uses the best selection from block 2 and 3
 * block 2 uses lower fs and uses fsb21		 (voice: 1.8 MB, phon+ 697 KB)
 * block 3 uses higher fs and avoid fsb21 usage  (voice: 2.0 MB, phon+ 614 KB)
 */

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
/* for backward compatibility */
//int lame_parse_args ( lame_global_flags* gfp, int argc, char** argv)
//{
//}

/*
 * parse "one" argument and set the internal parameter.
 * return the how many argument it used.
 *
 * if error happen, it returns pointer to the error message's in
 * char **msg
 */

int lame_parse_arg(lame_global_flags* gfp, int argc, char** argv, char **msg)
{
    int         user_quality = -1;
    double      freq;

    char   c;
    char*  token;
    char*  arg;
    char*  nextArg;
    int    argUsed;

    token = argv[0];
    if (*token++ == '-') {
	argUsed = 1;
	nextArg = 1 < argc  ?  argv[1]  :  "";
	if (*token == '-') { /* GNU style */
	    token++;

	    T_IF ("resample")
		argUsed++;
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
		argUsed++;
		gfp->VBR = vbr_abr; 
		gfp->VBR_mean_bitrate_kbps = atoi(nextArg);
		/* values larger than 8000 are bps (like Fraunhofer), so it's strange to get 320000 bps MP3 when specifying 8000 bps MP3 */
		if ( gfp -> VBR_mean_bitrate_kbps >= 8000 )
		    gfp -> VBR_mean_bitrate_kbps = ( gfp -> VBR_mean_bitrate_kbps + 500 ) / 1000;
		gfp->VBR_mean_bitrate_kbps = Min(gfp->VBR_mean_bitrate_kbps, 320); 
		gfp->VBR_mean_bitrate_kbps = Max(gfp->VBR_mean_bitrate_kbps,   8); 
		
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
		argUsed++;
	        gfp->ATHlower = atoi(nextArg);
		
	    T_ELIF ("raise-smr")
		argUsed++;
	        gfp->raiseSMR = atof(nextArg);
		if (gfp->raiseSMR < 0.0) gfp->raiseSMR = 0.0;
		if (gfp->raiseSMR > 1.0) gfp->raiseSMR = 1.0;

	    T_ELIF ("scale")
		argUsed++;
		gfp->scale = atof(nextArg);
		
	    T_ELIF ("freeformat")
		gfp->free_format=1;
		
	    T_ELIF ("athshort")
		gfp->ATHshort=1;
		
	    /* options for ID3 tag */
	    T_ELIF ("tt")
		argUsed++;
		id3tag_set_title(&gfp->tag_spec, nextArg);
		
	    T_ELIF ("ta")
		argUsed++;
	        id3tag_set_artist(&gfp->tag_spec, nextArg);

	    T_ELIF ("tl")
		argUsed++;
		id3tag_set_album(&gfp->tag_spec, nextArg);
		
	    T_ELIF ("ty")
		argUsed++;
		id3tag_set_year(&gfp->tag_spec, nextArg);
		
	    T_ELIF ("tc")
		argUsed++;
		id3tag_set_comment(&gfp->tag_spec, nextArg);
		
	    T_ELIF ("tn")
		argUsed++;
		id3tag_set_track(&gfp->tag_spec, nextArg);
		
	    T_ELIF ("tg")
	        argUsed++;
	        if (id3tag_set_genre(&gfp->tag_spec, nextArg)) {
		    argUsed = 0;
		    *msg = "Unknown genre: Specify genre name or number\n";
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
		
	    T_ELIF ("lowpass")
		freq = atof( nextArg );
	        argUsed++;
		/* useful are 0.001 kHz...50 kHz, 50 Hz...50000 Hz */
		gfp -> lowpassfreq = freq * (freq < 50. ? 1.e3 : 1.e0 ) + 0.5;
		if ( freq < 0.001 || freq > 50000. ) {
		    *msg = "Must specify lowpass with --lowpass freq, freq >= 0.001 kHz\n";
		    argUsed = 0;
		}
		
	    T_ELIF ("lowpass-width")
		argUsed++;
		gfp->lowpasswidth =  1000.0 * atof( nextArg ) + 0.5;
		if (gfp->lowpasswidth  < 0) {
		    *msg = "Must specify lowpass width with --lowpass-width freq, freq >= 0 kHz\n";
		    argUsed = 0;
		}

	    T_ELIF ("highpass")
		freq = atof( nextArg );
	        argUsed++;
		/* useful are 0.001 kHz...16 kHz, 16 Hz...50000 Hz */
		gfp->highpassfreq =  freq * (freq < 16. ? 1.e3 : 1.e0 ) + 0.5;
		if ( freq < 0.001 || freq > 50000. ) {
		    *msg = "Must specify highpass with --highpass freq, freq >= 0.001 kHz\n";
		    argUsed = 0;
		}
		
	    T_ELIF ("highpass-width")
		argUsed++;
		gfp->highpasswidth =  1000.0 * atof( nextArg ) + 0.5;
		if (gfp->highpasswidth  < 0) {
		    *msg = "Must specify highpass width with --highpass-width freq, freq >= 0 kHz\n";
		    argUsed = 0;
		}
		
	    T_ELIF ("cwlimit")
		freq = atof (nextArg);
	        argUsed++;
		/* useful are 0.001 kHz...50 kHz, 50 Hz...50000 Hz */
		gfp -> cwlimit = freq * ( freq <= 50. ? 1.e3 : 1.e0 );
		if (gfp->cwlimit <= 0 ) {
		    *msg = "Must specify cwlimit with --cwlimit freq, freq >= 0.001 kHz\n";
		    argUsed = 0;
		}

	    T_ELIF ("comp")
		argUsed++;
	        gfp->compression_ratio =  atof( nextArg );
		if (gfp->compression_ratio < 1.0 ) {
		    *msg = "Must specify compression ratio >= 1.0\n";
		    argUsed = 0;
		}
		
	    T_ELIF ("nspsytune")
		gfp->exp_nspsytune = TRUE;
	        gfp->experimentalZ = TRUE;
		gfp->experimentalX = 1;

	    T_ELSE
		argUsed = 0;

	    T_END
	} else {
	    c = *token++;
	    arg = *token ? token : nextArg;
	    argUsed = 1;
	    switch (c) {
	    case 'm':
		argUsed++;
		gfp -> mode_fixed = 1;

		switch ( *arg ) {
		case 's': gfp -> mode = MPG_MD_STEREO;       break;
		case 'd': gfp -> mode = MPG_MD_DUAL_CHANNEL; break;
		case 'f': gfp -> force_ms = 1;               /* fall through */
		case 'j': gfp -> mode = MPG_MD_JOINT_STEREO; break;
		case 'm': gfp -> mode = MPG_MD_MONO;         break;
		default :
		    *msg = " -m mode must be s/d/j/f/m\n";
		    break;
		}
		break;

	    case 'V':
		argUsed++;
		/* to change VBR default look in lame.h */
		if (gfp->VBR == vbr_off) gfp->VBR = vbr_default;  
		gfp->VBR_q = atoi(arg);
		if (gfp->VBR_q <0) gfp->VBR_q=0;
		if (gfp->VBR_q >9) gfp->VBR_q=9;
		gfp->quality = 2;
		break;
	    case 'q':
		argUsed++;
		user_quality = atoi(arg);
		if (user_quality<0) user_quality=0;
		if (user_quality>9) user_quality=9;
		break;
	    case 'b':        
		argUsed++;
		gfp->brate = atoi(arg); 
		gfp->VBR_min_bitrate_kbps=gfp->brate;
		break;
	    case 'B':        
		argUsed++;
		gfp->VBR_max_bitrate_kbps=atoi(arg); 
		break;	
	    case 'F':        
		gfp->VBR_hard_min=1;
		break;	
	    case 't':  /* dont write VBR tag */
		gfp->bWriteVbrTag=0;
		gfp->disable_waveheader=1;
		break;
	    case 'p': /* (jo) error_protection: add crc16 information to stream */
		gfp->error_protection = 1; 
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
	    case 'X':        
		argUsed++;
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

	    case 'e':        
		argUsed++;
		switch (*arg) {
		case 'n': gfp -> emphasis = 0; break;
		case '5': gfp -> emphasis = 1; break;
		case 'c': gfp -> emphasis = 3; break;
		default :
		    *msg = "-e emp must be n/5/c\n";
		    argUsed = 0;
		}
		break;
	    case 'c':
		gfp->copyright = 1;
		break;
	    case 'o':   
		gfp->original  = 0; 
		break;

	    default:
		argUsed = 0;
		break;
	    }
	}
    }

    /* user specified a quality value.  override any defaults set above */
    if (user_quality >= 0)
        gfp->quality = user_quality;

    return argUsed;
}
