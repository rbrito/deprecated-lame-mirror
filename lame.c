/*
 *	LAME MP3 encoding engine
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

#ifdef HAVEGTK
#include "gtkanal.h"
#include <gtk/gtk.h>
#endif
#include "lame.h"
#include "util.h"
#include "globalflags.h"
#include "psymodel.h"
#include "newmdct.h"
#include "filters.h"
#include "quantize.h"
#include "l3bitstream.h"
#include "formatBitstream.h"
#include "version.h"
#include "VbrTag.h"
#include "id3tag.h"
#include "get_audio.h"



/* Global flags.  defined extern in globalflags.h */
/* default values set in lame_init() */
int allow_diff_short;
int ATHonly;
int autoconvert;
int disable_reservoir;
int experimentalX;
int experimentalY;
int experimentalZ;
int force_ms;
int fast_mode;
long int frameNum;
int gtkflag;
int g_bWriteVbrTag;
int gpsycho;
int highq;
int no_short_blocks;
sound_file_format input_format;
int lame_nowrite;
FLOAT lowpass1,lowpass2;
FLOAT highpass1,highpass2;
FLOAT resample_ratio;
int sfb21;
int silent;
int swapbytes;           /* force byte swapping */
long totalframes;        /* frames: 0..totalframes-1 */
int VBR;
int VBR_q;
int VBR_min_bitrate;   /* 32kbs */
int VBR_max_bitrate;  /* 256kbs */
int voice_mode;
int psyModel;	/* 0=patched Model 2, other=Model2 */



/* Global variable definitions for "musicin.c" */
ID3TAGDATA id3tag;
struct stat sb;
Bit_stream_struc   bs;
III_side_info_t l3_side;
frame_params fr_ps;
char    inPath[MAX_NAME_SIZE];
char    outPath[MAX_NAME_SIZE];
int target_bitrate;
char               *programName;
unsigned long num_samples;
static layer info;

#ifdef BRHIST
#include <string.h>
#include <termcap.h>
void brhist_init(int br_min, int br_max);
void brhist_add_count(void);
void brhist_disp(void);
void brhist_disp_total(void);
extern long brhist_temp[15];
int disp_brhist = 1;
#endif

#define DFLT_LAY        3      /* default encoding layer is III */
#define DFLT_MOD        'j'    /* default mode is stereo */
#define DFLT_SFQ        44.1   /* default input sampling rate is 44.1 kHz */
#define DFLT_EMP        'n'    /* default de-emphasis is none */
#define DFLT_EXT        ".mp3" /* default output file extension */


/************************************************************************
*
* usage
*
* PURPOSE:  Writes command line syntax to the file specified by #stderr#
*
************************************************************************/

void lame_usage(char *name)  /* print syntax & exit */
{
  fprintf(stderr,"\n");
  fprintf(stderr,"USAGE   :  %s [options] <infile> [outfile]\n",name);
  fprintf(stderr,"\n<infile> and/or <outfile> can be \"-\", which means stdin/stdout.\n");
  fprintf(stderr,"\n");
  fprintf(stderr,"OPTIONS :\n");
  fprintf(stderr,"    -m mode         (s)tereo, (j)oint, (f)orce or (m)ono  (default %c)\n",DFLT_MOD);
  fprintf(stderr,"                    force = force ms_stereo on all frames. Faster\n");
  fprintf(stderr,"    -b bitrate      set the bitrate, default 128kbps\n");
  fprintf(stderr,"    -s sfreq        sampling frequency of input file(kHz) - default %4.1f\n",DFLT_SFQ);
  fprintf(stderr,"  --resample sfreq  sampling frequency of output file(kHz)- default=input sfreq\n");
  fprintf(stderr,"  --mp3input        input file is a MP3 file\n");
  fprintf(stderr,"  --voice           experimental voice mode\n");
  fprintf(stderr,"\n");
  fprintf(stderr,"  --lowpass freq         frequency(kHz), lowpass filter cutoff above freq\n");
  fprintf(stderr,"  --lowpass-width freq   frequency(kHz) - default 15%% of lowpass freq\n");
  fprintf(stderr,"  --highpass freq        frequency(kHz), highpass filter cutoff below freq\n");
  fprintf(stderr,"  --highpass-width freq  frequency(kHz) - default 15%% of highpass freq\n");
  fprintf(stderr,"\n");
  fprintf(stderr,"    -v              use variable bitrate (VBR)\n");
  fprintf(stderr,"    -V n            quality setting for VBR.  default n=%i\n",VBR_q);
  fprintf(stderr,"                    0=high quality,bigger files. 9=smaller files\n");
  fprintf(stderr,"    -b bitrate      specify minimum allowed bitrate, default 32kbs\n");
  fprintf(stderr,"    -B bitrate      specify maximum allowed bitrate, default 256kbs\n");
  fprintf(stderr,"    -t              disable Xing VBR informational tag\n");
  fprintf(stderr,"    --nohist        disable VBR histogram display\n");
  fprintf(stderr,"\n");
  fprintf(stderr,"    -h              use (maybe) quality improvements\n");
  fprintf(stderr,"    -f              fast mode (low quality)\n");
  fprintf(stderr,"    -k              disable sfb=21 cutoff\n");
  fprintf(stderr,"    -d              allow channels to have different blocktypes\n");
  fprintf(stderr,"  --athonly         only use the ATH for masking\n");
  fprintf(stderr,"  --noshort         do not use short blocks\n");
  fprintf(stderr,"  --nores           disable the bit reservoir\n");
  fprintf(stderr,"\n");
  fprintf(stderr,"    -r              input is raw pcm\n");
  fprintf(stderr,"    -x              force byte-swapping of input\n");
  fprintf(stderr,"    -a              downmix from stereo to mono file for mono encoding\n");
  fprintf(stderr,"    -e emp          de-emphasis n/5/c  (obsolete)\n");
  fprintf(stderr,"    -p              error protection.  adds 16bit checksum to every frame\n");
  fprintf(stderr,"                    (the checksum is computed correctly)\n");
  fprintf(stderr,"    -c              mark as copyright\n");
  fprintf(stderr,"    -o              mark as non-original\n");
  fprintf(stderr,"    -S              don't print progress report, VBR histograms\n");
  fprintf(stderr,"\n");
  fprintf(stderr,"  Specifying any of the following options will add an ID3 tag:\n");
  fprintf(stderr,"     --tt \"title\"     title of song (max 30 chars)\n");
  fprintf(stderr,"     --ta \"artist\"    artist who did the song (max 30 chars)\n");
  fprintf(stderr,"     --tl \"album\"     album where it came from (max 30 chars)\n");
  fprintf(stderr,"     --ty \"year\"      year in which the song/album was made (max 4 chars)\n");
  fprintf(stderr,"     --tc \"comment\"   additional info (max 30 chars)\n");
  fprintf(stderr,"     --tg \"genre\"     genre of song (name or number)\n");
  fprintf(stderr,"\n");
#ifdef HAVEGTK
  fprintf(stderr,"    -g              run graphical analysis on <infile>\n");
#endif
  display_bitrates(2);
  exit(1);
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
void lame_parse_args(int argc, char **argv)
{
  FLOAT srate;
  int   brate,VBR_max_bitrate_kbps=0;
  layer *info = fr_ps.header;
  int   err = 0, i = 0;
  int mode_given=0;
  int num_channels;   /* number of channels of INPUT file */
  int stereo;           /* number of channels of MP3 file */
  int samplerate,resamplerate=0;
  int lowpassrate=0, highpassrate=0; /* initialized to use defaults */
  int lowpasswidth=-1, highpasswidth=-1; /* initialized to use defaults */
  int framesize;
  FLOAT compression_ratio;

  /* preset defaults */
  programName = argv[0]; 
  inPath[0] = '\0';   outPath[0] = '\0';
  info->lay = DFLT_LAY;
  switch(DFLT_MOD) {
  case 's': info->mode = MPG_MD_STEREO; info->mode_ext = MPG_MD_LR_LR; break;
  case 'd': info->mode = MPG_MD_DUAL_CHANNEL; info->mode_ext=MPG_MD_LR_LR; break;
  case 'j': info->mode = MPG_MD_JOINT_STEREO; info->mode_ext=MPG_MD_LR_LR; break;
  case 'm': info->mode = MPG_MD_MONO; info->mode_ext = MPG_MD_LR_LR; break;
  default:
    fprintf(stderr, "%s: Bad mode dflt %c\n", programName, DFLT_MOD);
    abort();
  }
  samplerate = 1000*DFLT_SFQ;
  if((info->sampling_frequency = SmpFrqIndex((long)samplerate, &info->version)) < 0) {
    fprintf(stderr, "%s: bad sfrq default %.2f\n", programName, DFLT_SFQ);
    abort();
  }

  info->bitrate_index = 9;
  brate = 0;
  switch(DFLT_EMP) {
  case 'n': info->emphasis = 0; break;
  case '5': info->emphasis = 1; break;
  case 'c': info->emphasis = 3; break;
  default: 
    fprintf(stderr, "%s: Bad emph dflt %c\n", programName, DFLT_EMP);
    abort();
  }
  info->copyright = 0;
  info->original = 1;
  info->error_protection = FALSE;
  
#ifndef _BLADEDLL
  id3_inittag(&id3tag);
  id3tag.used = 0;
#endif

  /* process args */
  while(++i<argc && err == 0) {
    char c, *token, *arg, *nextArg;
    int  argUsed;
    
    token = argv[i];
    if(*token++ == '-') {
      if(i+1 < argc) nextArg = argv[i+1];
      else           nextArg = "";
      argUsed = 0;
      if (! *token) {
	/* The user wants to use stdin and/or stdout. */
	if(inPath[0] == '\0')       strncpy(inPath, argv[i],MAX_NAME_SIZE);
	else if(outPath[0] == '\0') strncpy(outPath, argv[i],MAX_NAME_SIZE);
      } 
      if (*token == '-') {
	/* GNU style */
	token++;

	if (strcmp(token, "resample")==0) {
	  argUsed=1;
	  srate = atof( nextArg );
	  /* samplerate = rint( 1000.0 * srate ); $A  */
	  resamplerate =  (( 1000.0 * srate ) + 0.5);
	  if (srate  < 1) {
	    fprintf(stderr,"Must specify samplerate with --resample\n");
	    exit(1);
	  }
	}
	else if (strcmp(token, "mp3input")==0) {
	  input_format=sf_mp3;
	}
	else if (strcmp(token, "voice")==0) {
	  voice_mode=1;
	  no_short_blocks=1;
	}
	else if (strcmp(token, "noshort")==0) {
	  no_short_blocks=1;
	}
	else if (strcmp(token, "nores")==0) {
	  disable_reservoir=1;
	}
	else if (strcmp(token, "athonly")==0) {
	  ATHonly=1;
	}
	else if (strcmp(token, "nohist")==0) {
#ifdef BRHIST
	  disp_brhist = 0;
#endif
	}
#ifndef _BLADEDLL
	/* options for ID3 tag */
 	else if (strcmp(token, "tt")==0) {
 		id3tag.used=1;      argUsed = 1;
  		strncpy(id3tag.title, nextArg, 30);
 		}
 	else if (strcmp(token, "ta")==0) {
 		id3tag.used=1; argUsed = 1;
  		strncpy(id3tag.artist, nextArg, 30);
 		}
 	else if (strcmp(token, "tl")==0) {
 		id3tag.used=1; argUsed = 1;
  		strncpy(id3tag.album, nextArg, 30);
 		}
 	else if (strcmp(token, "ty")==0) {
 		id3tag.used=1; argUsed = 1;
  		strncpy(id3tag.year, nextArg, 4);
 		}
 	else if (strcmp(token, "tc")==0) {
 		id3tag.used=1; argUsed = 1;
  		strncpy(id3tag.comment, nextArg, 30);
 		}
 	else if (strcmp(token, "tg")==0) {
		argUsed = strtol (nextArg, &token, 10);
		if (nextArg==token) {
		  /* Genere was given as a string, so it's number*/
		  for (argUsed=0; argUsed<=genre_last; argUsed++) {
		    if (!strcmp (genre_list[argUsed], nextArg)) { break; }
		  }
 		}
		if (argUsed>genre_last) { 
		  argUsed=255; 
		  fprintf(stderr,"Unknown genre: %s.  Specifiy genre number \n", nextArg);
		}
	        argUsed &= 255; c=(char)(argUsed);

 		id3tag.used=1; argUsed = 1;
  		strncpy(id3tag.genre, &c, 1);
	       }
#endif
	else if (strcmp(token, "lowpass")==0) {
	  argUsed=1;
	  lowpassrate =  (( 1000.0 * atof( nextArg ) ) + 0.5);
	  if (lowpassrate  < 1) {
	    fprintf(stderr,"Must specify lowpass with --lowpass freq, freq >= 0.001 kHz\n");
	    exit(1);
	  }
	}
	else if (strcmp(token, "lowpass-width")==0) {
	  argUsed=1;
	  lowpasswidth =  (( 1000.0 * atof( nextArg ) ) + 0.5);
	  if (lowpasswidth  < 0) {
	    fprintf(stderr,"Must specify lowpass width with --lowpass-width freq, freq >= 0 kHz\n");
	    exit(1);
	  }
	}
	else if (strcmp(token, "highpass")==0) {
	  argUsed=1;
	  highpassrate =  (( 1000.0 * atof( nextArg ) ) + 0.5);
	  if (highpassrate  < 1) {
	    fprintf(stderr,"Must specify highpass with --highpass freq, freq >= 0.001 kHz\n");
	    exit(1);
	  }
	}
	else if (strcmp(token, "highpass-width")==0) {
	  argUsed=1;
	  highpasswidth =  (( 1000.0 * atof( nextArg ) ) + 0.5);
	  if (highpasswidth  < 0) {
	    fprintf(stderr,"Must specify highpass width with --highpass-width freq, freq >= 0 kHz\n");
	    exit(1);
	  }
	}
	else if (strcmp(token, "psy-model")==0) {
	  argUsed=1;
	  psyModel =  atoi( nextArg );
	  if (psyModel < 0 || psyModel > 2) {
	    fprintf(stderr,"Must specify psychoacoustic model with --psy-model n, n=0,1,2\n");
	    exit(1);
	  }
	}
	else
	  {
	    fprintf(stderr,"%s: unrec option --%s\n",
		    programName, token);
	  }
	i += argUsed;
	
      } else  while( (c = *token++) ) {
	if(*token ) arg = token;
	else                             arg = nextArg;
	switch(c) {
	case 'm':        argUsed = 1;
	  mode_given=1;
	  if (*arg == 's')
	    { info->mode = MPG_MD_STEREO; info->mode_ext = MPG_MD_LR_LR; }
	  else if (*arg == 'd')
	    { info->mode = MPG_MD_DUAL_CHANNEL; info->mode_ext=MPG_MD_LR_LR; }
	  else if (*arg == 'j')
	    { info->mode = MPG_MD_JOINT_STEREO; info->mode_ext=MPG_MD_LR_LR; }
	  else if (*arg == 'f')
	    { info->mode = MPG_MD_JOINT_STEREO; force_ms=1; info->mode_ext=MPG_MD_LR_LR; }
	  else if (*arg == 'm')
	    { info->mode = MPG_MD_MONO; info->mode_ext = MPG_MD_LR_LR; }
	  else {
	    fprintf(stderr,"%s: -m mode must be s/d/j/f/m not %s\n",
		    programName, arg);
	    err = 1;
	  }
	  break;
	case 'V':        argUsed = 1;   VBR = 1;  
	  if (*arg == '0')
	    { VBR_q=0; }
	  else if (*arg == '1')
	    { VBR_q=1; }
	  else if (*arg == '2')
	    { VBR_q=2; }
	  else if (*arg == '3')
	    { VBR_q=3; }
	  else if (*arg == '4')
	    { VBR_q=4; }
	  else if (*arg == '5')
	    { VBR_q=5; }
	  else if (*arg == '6')
	    { VBR_q=6; }
	  else if (*arg == '7')
	    { VBR_q=7; }
	  else if (*arg == '8')
	    { VBR_q=8; }
	  else if (*arg == '9')
	    { VBR_q=9; }
	  else {
	    fprintf(stderr,"%s: -V n must be 0-9 not %s\n",
		    programName, arg);
	    err = 1;
	  }
	  break;
	  
	case 's':
	  argUsed = 1;
	  srate = atof( arg );
	  /* samplerate = rint( 1000.0 * srate ); $A  */
	  samplerate =  (( 1000.0 * srate ) + 0.5);
	  break;
	case 'b':        
	  argUsed = 1;
	  brate = atoi(arg); 
	  break;
	case 'B':        
	  argUsed = 1;
	  VBR_max_bitrate_kbps=atoi(arg); 
	  break;	
case 't':  /* dont write VBR tag */
		g_bWriteVbrTag=0;
	  break;
	case 'r':  /* force raw pcm input file */
#ifdef LIBSNDFILE
	  fprintf(stderr,"WARNING: libsndfile may ignore -r and perform fseek's on the input.\n");
	  fprintf(stderr,"Compile without libsndfile if this is a problem.\n");
#endif
	  input_format=sf_raw;
	  break;
	case 'x':  /* force byte swapping */
	  swapbytes=TRUE;
	  break;
	case 'p': /* (jo) error_protection: add crc16 information to stream */
	  info->error_protection = 1; 
	  break;
	case 'a': /* autoconvert input file from stereo to mono - for mono mp3 encoding */
	  autoconvert = TRUE;
	  break;
	case 'h': 
	  highq = TRUE;
	  break;
	case 'k': 
	  sfb21 = FALSE;
	  break;
	case 'd': 
	  allow_diff_short = 1;
	  break;
	case 'v': 
	  VBR = 1; 
	  break;
	case 'S': 
	  silent = TRUE;
	  break;
	case 'X':        argUsed = 1;   experimentalX = 0;
	  if (*arg == '0')
	    { experimentalX=0; }
	  else if (*arg == '1')
	    { experimentalX=1; }
	  else if (*arg == '2')
	    { experimentalX=2; }
	  else if (*arg == '3')
	    { experimentalX=3; }
	  else if (*arg == '4')
	    { experimentalX=4; }
	  else if (*arg == '5')
	    { experimentalX=5; }
	  else if (*arg == '6')
	    { experimentalX=6; }
	  else {
	    fprintf(stderr,"%s: -X n must be 0-6 not %s\n",
		    programName, arg);
	    err = 1;
	  }
	  break;


	case 'Y': 
	  experimentalY = TRUE;
	  break;
	case 'Z': 
	  experimentalZ = TRUE;
	  break;
	case 'f': 
	  fast_mode = 1;
	  break;
#ifdef HAVEGTK
	case 'g': /* turn on gtk analysis */
	  gtkflag = TRUE;
	  break;
#endif
	case 'e':        argUsed = 1;
	  if (*arg == 'n')                    info->emphasis = 0;
	  else if (*arg == '5')               info->emphasis = 1;
	  else if (*arg == 'c')               info->emphasis = 3;
	  else {
	    fprintf(stderr,"%s: -e emp must be n/5/c not %s\n",
		    programName, arg);
	    err = 1;
	  }
	  break;
	case 'c':       info->copyright = 1; break;
	case 'o':       info->original  = 0; break;
	default:        fprintf(stderr,"%s: unrec option %c\n",
				programName, c);
	err = 1; break;
	}
	if(argUsed) {
	  if(arg == token)    token = "";   /* no more from token */
	  else                ++i;          /* skip arg we used */
	  arg = ""; argUsed = 0;
	}
      }
    }
    else {
      if(inPath[0] == '\0')       strncpy(inPath, argv[i], MAX_NAME_SIZE);
      else if(outPath[0] == '\0') strncpy(outPath, argv[i], MAX_NAME_SIZE);
      else {
	fprintf(stderr,"%s: excess arg %s\n", programName, argv[i]);
	err = 1;
      }
    }
  }

  /* Do not write VBR tag if VBR flag is not specified */
  if (g_bWriteVbrTag==1 && VBR==0) g_bWriteVbrTag=0;

  if(err || inPath[0] == '\0') lame_usage(programName);  /* never returns */
  if (inPath[0]=='-') silent=1;  /* turn off status - it's broken for stdin */
  if(outPath[0] == '\0') {
    if (inPath[0]=='-') {
      /* if input is stdin, default output is stdout */
      strcpy(outPath,"-");
    }else {
      strncpy(outPath, inPath, MAX_NAME_SIZE - strlen(DFLT_EXT));
      strncat(outPath, DFLT_EXT, strlen(DFLT_EXT) );
    }
  }
  /* some file options not allowed with stdout */
  if (outPath[0]=='-') {
    g_bWriteVbrTag=0; /* turn off VBR tag */
    if (id3tag.used) {
      id3tag.used=0;         /* turn of id3 tagging */
      fprintf(stderr,"id3tag ignored: id3 tagging not supported for stdout.\n");
    }
  }


  /* if user did not explicitly specify input is mp3, check file name */
  if (input_format != sf_mp3)
    if (!(strcmp((char *) &inPath[strlen(inPath)-4],".mp3")))
      input_format = sf_mp3;

  /* default guess for number of channels */
  if (autoconvert) num_channels=2; 
  else if (info->mode == MPG_MD_MONO) num_channels=1;
  else num_channels=2;
  
#ifdef _BLADEDLL
  num_samples=0;
#else
  /* open the input file */
  OpenSndFile(inPath,info,samplerate,num_channels);  
  /* if GetSndSampleRate is non zero, use it to overwrite the default */
  if (GetSndSampleRate()) samplerate=GetSndSampleRate();
  if (GetSndChannels()) num_channels=GetSndChannels();
  num_samples = GetSndSamples();
#endif

  if (autoconvert==TRUE) {
    info->mode = MPG_MD_MONO; 
    info->mode_ext = MPG_MD_LR_LR;
  }
  if (num_channels==1) {
    autoconvert = FALSE;  /* avoid 78rpm emulation mode from downmixing mono file! */
    info->mode = MPG_MD_MONO;
    info->mode_ext = MPG_MD_LR_LR;
  }      
  /* if user specified mono and there are 2 channels, autoconvert */
  if ((num_channels==2) && (info->mode == MPG_MD_MONO))
    autoconvert = TRUE;
  
  if ((num_channels==1) && (info->mode!=MPG_MD_MONO)) {
    fprintf(stderr,"Error: mono input, stereo output not supported. \n");
    exit(1);
  }
  if (autoconvert==TRUE) {
    fprintf(stderr, "Autoconverting from stereo to mono. Setting encoding to mono mode.\n");
  }
  stereo=2;
  if (info->mode == MPG_MD_MONO) stereo=1;


  /* set the output sampling rate, and resample options if necessary 
     samplerate = input sample rate
     resamplerate = ouput sample rate
  */
  if (resamplerate==0) {
    /* user did not specify output sample rate */
    resamplerate=samplerate;   /* default */

    /* if resamplerate is not valid, find a valid value */
    if (SmpFrqIndex((long)resamplerate, &info->version) < 0) {
	if (resamplerate>48000) resamplerate=48000;
	else if (resamplerate>44100) resamplerate=44100;
	else if (resamplerate>32000) resamplerate=32000;
	else if (resamplerate>24000) resamplerate=24000;
	else if (resamplerate>22050) resamplerate=22050;
	else resamplerate=16000;
    }

    if (brate>0) {
      compression_ratio = resamplerate*16*stereo/(1000.0*brate);
      if (!VBR && compression_ratio > 13 ) {
	/* automatic downsample, if possible */
	resamplerate = (10*1000.0*brate)/(16*stereo);
	if (resamplerate<16000) resamplerate=16000;
	else if (resamplerate<22050) resamplerate=22050;
	else if (resamplerate<24000) resamplerate=24000;
	else if (resamplerate<32000) resamplerate=32000;
	else if (resamplerate<44100) resamplerate=44100;
	else resamplerate=48000;
      }
    }
  }


  info->sampling_frequency = SmpFrqIndex((long)resamplerate, &info->version);
  if( info->sampling_frequency < 0) {
    display_bitrates(2);
    exit(1);
  }





  if ( brate == 0 ) {
    info->bitrate_index=9;
    brate = bitrate[info->version][info->lay-1][info->bitrate_index];
  } else {
    if( (info->bitrate_index = BitrateIndex(info->lay, brate, info->version,resamplerate)) < 0) {
      display_bitrates(2);
      exit(1);
    }
    /* a bit rate was specified.  for VBR, take this to be the minimum */
    VBR_min_bitrate=info->bitrate_index;
  }
  if (info->bitrate_index==14) VBR=0;  /* dont bother with VBR at 320kbs */
  compression_ratio = resamplerate*16*stereo/(1000.0*brate);

  resample_ratio=1;
  if (resamplerate != samplerate) resample_ratio = (FLOAT)samplerate/(FLOAT)resamplerate;
  if (resample_ratio!=1) {
    fprintf(stderr,"Resampling:  input=%iHz  output=%iHz\n",
	    (int)samplerate,(int)resamplerate);
  }


#ifndef _BLADEDLL
  /* estimate total frames.  must be done after setting sampling rate so
   * we know the framesize.  */
  totalframes=0;
  framesize = (info->version==0) ? 576 : 1152;
  if (num_samples == MAX_U_32_NUM) { 
    stat(inPath,&sb);  /* try file size, assume 2 bytes per sample */
    if (input_format == sf_mp3) {
      FLOAT totalseconds = (sb.st_size*8.0/(1000.0*GetSndBitrate()));
      FLOAT framespersecond =  (FLOAT)samplerate/(FLOAT)framesize;
      totalframes = 1+(totalseconds*framespersecond);
    }else{
      totalframes = 2+(sb.st_size/(2*framesize*num_channels));
    }
  }else{
    totalframes = 2+ num_samples/(resample_ratio*framesize);
  }
#endif /* _BLADEDLL */





  /* 44.1kHz at 56kbs/channel: compression factor of 12.6 
     44.1kHz at 64kbs/channel: compression factor of 11.025 
     44.1kHz at 80kbs/channel: compression factor of 8.82 
     22.05kHz at 24kbs:  14.7 
     22.05kHz at 32kbs:  11.025
     22.05kHz at 40kbs:  8.82 
     16kHz at 16kbs:  16.0 
     16kHz at 24kbs:  10.7 
    
     compression_ratio 
        11                                .70?
        12                   sox resample .66
        14.7                 sox resample .45
                 
  */
  /* if the user did not specify a large VBR_min_bitrate (=brate),
   * compression_ratio not well defined.  take a guess: */
  if (VBR && compression_ratio>11) {
    compression_ratio = 11.025;               /* like 44kHz/64kbs per channel */
    if (VBR_q <= 4) compression_ratio=8.82;   /* like 44kHz/80kbs per channel */
    if (VBR_q <= 3) compression_ratio=7.35;   /* like 44kHz/96kbs per channel */
  }


  /* At higher quality (lower compression) use STEREO instead of JSTEREO.
   * (unless the user explicitly specified a mode ) */
  if ( (!mode_given) && (info->mode !=MPG_MD_MONO)) {
    if (compression_ratio <= 8 ) {
      info->mode = MPG_MD_STEREO; info->mode_ext = MPG_MD_LR_LR;
    }
  }


  /* Should we disable the scalefactor band 21 cutoff? */
  /* NOTE: dont *enable* it here, since that could override the users' 
   * -k option, which disables sfb21 cufoff */
  /* remove coefficients in scalefactor band 21 (12 for short blocks)
   * FhG does this for bps <= 128kbs, so we will too.  
   * This amounds to a 16kHz low-pass filter.  If that offends you, you
   * probably should not be encoding at 128kbs!
   * There is no ratio[21] or xfsf[21], so when these coefficients are
   * included they are just quantized as is.  mt 5/99
   */
  /* disable sfb21 cutoff for low amounts of compression */
  if (compression_ratio<9.0) sfb21=0;

  if (brate/stereo <= 32  ) {
    /* high compression, low bitrates, lets use a filter */
    sfb21=0; /* not needed */
    lowpass1=.85;
    lowpass2=.99;
    if (compression_ratio > 15.5) {
      lowpass1=.35;  /* good for 16kHz 16kbs compression = 16*/
      lowpass2=.50;
    }else if (compression_ratio > 14) {
      lowpass1=.40;  /* good for 22kHz 24kbs compression = 14.7*/
      lowpass2=.55;
    }else {
      lowpass1=.55;  /* good for 16kHz 24kbs compression = 10.7*/
      lowpass2=.70;
    }
  }

  /* 14.5khz = .66  16kHz = .73 */
  /*
  lowpass1 = .73-.10;
  lowpass2 = .73+.05;
  */
  
  /* apply user driven filters, may override above calculations 
   */

  if ( highpassrate > 0 ) {
    highpass1 = 2.0*highpassrate/resamplerate; /* will always be >=0 */
    if ( highpasswidth >= 0 ) {
      highpass2 = 2.0*(highpassrate+highpasswidth)/resamplerate;
    } else {
      highpass2 = 1.15*2.0*highpassrate/resamplerate; /* 15% above on default */
    }
    if ( highpass1 == highpass2 ) { /* ensure highpass1 < highpass2 */
      highpass2 += 1E-6;
    }
    highpass1 = Min( 1, highpass1 );
    highpass2 = Min( 1, highpass2 );
  }
  if ( lowpassrate > 0 ) {
    lowpass2 = 2.0*lowpassrate/resamplerate; /* will always be >=0 */
    if ( lowpasswidth >= 0 ) { 
      lowpass1 = 2.0*(lowpassrate-lowpasswidth)/resamplerate;
      if ( lowpass1 < 0 ) { /* has to be >= 0 */
	lowpass1 = 0;
      }
    } else {
      lowpass1 = 0.85*2.0*lowpassrate/resamplerate; /* 15% below on default */
    }
    if ( lowpass1 == lowpass2 ) { /* ensure lowpass1 < lowpass2 */
      lowpass2 += 1E-6;
    }
    lowpass1 = Min( 1, lowpass1 );
    lowpass2 = Min( 1, lowpass2 );
  }
  /* printf("%g %g %g %g\n", highpass1, highpass2, lowpass1, lowpass2 );
   */
  if ( highpass2>0 || lowpass1>0 ) {
    sfb21 = 0;
  }

  /* choose a max bitrate for VBR */
  if (VBR) {
    /* if the user didn't specify VBR_max_bitrate: */
    if (0==VBR_max_bitrate_kbps) {
      /* default max bitrate is 256kbs */
      /* we do not normally allow 320bps frams with VBR, unless: */
      if (info->bitrate_index==13) VBR_max_bitrate=14;  
      if (VBR_q == 0) VBR_max_bitrate=14;   /* allow 320kbs */
      if (VBR_q >= 4) VBR_max_bitrate=12;   /* max = 224kbs */
      if (VBR_q >= 8) VBR_max_bitrate=9;    /* low quality, max = 128kbs */
      if (voice_mode) VBR_max_bitrate=10;
    }else{
      if( (VBR_max_bitrate  = BitrateIndex(info->lay, VBR_max_bitrate_kbps, info->version,resamplerate)) < 0) {
	display_bitrates(2);
	exit(1);
      }
    }
  }


  if (VBR) highq=1;                    /* always use highq with VBR */
  /* dont allow forced mid/side stereo for mono output */
  if (info->mode == MPG_MD_MONO) force_ms=0;  

  if (gtkflag) {
    lame_nowrite=1;    /* disable all file output */
    g_bWriteVbrTag=0;  /* disable Xing VBR tag */
  }

  if (info->version==0) {
    g_bWriteVbrTag=0;      /* no MPEG2 Xing VBR tags yet */
  }


  /* open the output file */
  /* if gtkflag, no output.  but set outfile = stdout */
  open_bit_stream_w(&bs, outPath, BUFFER_SIZE,lame_nowrite);  

  /* copy some header information */
  fr_ps.actual_mode = info->mode;
  fr_ps.stereo = (info->mode == MPG_MD_MONO) ? 1 : 2;


#ifdef BRHIST
  if (VBR) {
    if (disp_brhist)
      brhist_init(1, 14);
  } else
    disp_brhist = 0;
#endif

  if (g_bWriteVbrTag)
    {
      /* Write initial VBR Header to bitstream */
      InitVbrTag(&bs,info->version-1,info->mode,info->sampling_frequency);
      /* flush VBR header frame to mp3 file */
      if (!lame_nowrite) 
	  {
		write_buffer(&bs);
		empty_buffer(&bs);
	  }
      /* note: if lame_nowrite==0, we do not empty the buffer, the VBR header will
       * remain in the bit buffer and the first mp3 frame will be
       * appeneded.  this way, lame_encode() will return to the calling
       * program the VBR header along with the first mp3 frame */
    }

  return;
}




/************************************************************************
 *
 * print_config
 *
 * PURPOSE:  Prints the encoding parameters used
 *
 ************************************************************************/
void lame_print_config(void)
{
  layer *info = fr_ps.header;
  char *mode_names[4] = { "stereo", "j-stereo", "dual-ch", "single-ch" };
  FLOAT compression=
    (FLOAT)(fr_ps.stereo*16*s_freq[info->version][info->sampling_frequency])/
    (FLOAT)(bitrate[info->version][info->lay-1][info->bitrate_index]);

  if (gtkflag) {
    fprintf(stderr, "Analyzing %s \n",inPath);
  }
  else {
    fprintf(stderr, "Encoding %s to %s\n",
	    (strcmp(inPath, "-")? inPath : "stdin"),
	    (strcmp(outPath, "-")? outPath : "stdout"));
    if (highpass2>0.0)
      fprintf(stderr, "Highpass filter: cutoff below %g Hz, increasing upto %g Hz\n",
              highpass1*s_freq[info->version][info->sampling_frequency]*500, 
	      highpass2*s_freq[info->version][info->sampling_frequency]*500);
    if (lowpass1>0.0)
      fprintf(stderr, "Lowpass filter: cutoff above %g Hz, decreasing from %g Hz\n",
              lowpass2*s_freq[info->version][info->sampling_frequency]*500, 
	      lowpass1*s_freq[info->version][info->sampling_frequency]*500);
    if (VBR)
      fprintf(stderr, "Encoding as %.1fkHz VBR(q=%i) %s MPEG%i LayerIII file\n",
	      s_freq[info->version][info->sampling_frequency],
	      VBR_q,mode_names[info->mode],2-info->version);
    else
      fprintf(stderr, "Encoding as %.1f kHz %d kbps %s MPEG%i LayerIII file  (%4.1fx)\n",
	      s_freq[info->version][info->sampling_frequency],
	      bitrate[info->version][info->lay-1][info->bitrate_index],
	      mode_names[info->mode],2-info->version,compression);
  }
  fflush(stderr);
}



 
#ifdef BRHIST

#define BRHIST_BARMAX 50

long brhist_count[15];
long brhist_temp[15];
int brhist_vbrmin;
int brhist_vbrmax;
long brhist_max;
char brhist_bps[15][5];
char brhist_backcur[200];
char brhist_bar[BRHIST_BARMAX+10];
char brhist_spc[BRHIST_BARMAX+1];

char stderr_buff[BUFSIZ];


void brhist_init(int br_min, int br_max)
{
  int i;
  char term_buff[1024];
  layer *info = fr_ps.header;
  char *termname;
  char *tp;
  char tc[10];

  for(i = 0; i < 15; i++)
    {
      sprintf(brhist_bps[i], "%3d:", bitrate[info->version][info->lay-1][i]);
      brhist_count[i] = 0;
      brhist_temp[i] = 0;
    }

  brhist_vbrmin = br_min;
  brhist_vbrmax = br_max;

  brhist_max = 0;

  memset(&brhist_bar[0], '*', BRHIST_BARMAX);
  brhist_bar[BRHIST_BARMAX] = '\0';
  memset(&brhist_spc[0], ' ', BRHIST_BARMAX);
  brhist_spc[BRHIST_BARMAX] = '\0';
  brhist_backcur[0] = '\0';

  if ((termname = getenv("TERM")) == NULL)
    {
      fprintf(stderr, "can't get TERM environment string.\n");
      disp_brhist = 0;
      return;
    }

  if (tgetent(term_buff, termname) != 1)
    {
      fprintf(stderr, "can't find termcap entry: %s\n", termname);
      disp_brhist = 0;
      return;
    }

  tc[0] = '\0';
  tp = &tc[0];
  tp=tgetstr("up", &tp);
  brhist_backcur[0] = '\0';
  for(i = br_min-1; i <= br_max; i++)
    strcat(brhist_backcur, tp);
  setbuf(stderr, stderr_buff);
}

void brhist_add_count(void)
{
  int i;

  for(i = brhist_vbrmin; i <= brhist_vbrmax; i++)
    {
      brhist_count[i] += brhist_temp[i];
      if (brhist_count[i] > brhist_max)
	brhist_max = brhist_count[i];
      brhist_temp[i] = 0;
    }
}

void brhist_disp(void)
{
  int i;
  long full;
  int barlen;

  full = (brhist_max < BRHIST_BARMAX) ? BRHIST_BARMAX : brhist_max;
  fputc('\n', stderr);
  for(i = brhist_vbrmin; i <= brhist_vbrmax; i++)
    {
      barlen = (brhist_count[i]*BRHIST_BARMAX+full-1) / full;
      fputs(brhist_bps[i], stderr);
      fputs(&brhist_bar[BRHIST_BARMAX - barlen], stderr);
      fputs(&brhist_spc[barlen], stderr);
      fputc('\n', stderr);
    }
  fputs(brhist_backcur, stderr);
  fflush(stderr);
}

void brhist_disp_total(void)
{
  int i;
  FLOAT ave;
  layer *info = fr_ps.header;

  for(i = brhist_vbrmin; i <= brhist_vbrmax; i++)
    fputc('\n', stderr);

  ave=0;
  for(i = brhist_vbrmin; i <= brhist_vbrmax; i++)
    ave += bitrate[info->version][info->lay-1][i]*
      (FLOAT)brhist_count[i] / totalframes;
  fprintf(stderr, "\naverage: %2.0f kbs\n",ave);
    
#if 0
  fprintf(stderr, "----- bitrate statistics -----\n");
  fprintf(stderr, " [kbps]      frames\n");
  for(i = brhist_vbrmin; i <= brhist_vbrmax; i++)
    {
      fprintf(stderr, "   %3d  %8ld (%.1f%%)\n",
	      bitrate[info->version][info->lay-1][i],
	      brhist_count[i],
	      (FLOAT)brhist_count[i] / totalframes * 100.0);
    }
#endif
  fflush(stderr);
}

#endif










/************************************************************************
* 
* encodeframe()
*
************************************************************************/
int lame_encode(short int Buffer[2][1152],char *mpg123bs)
{
  static unsigned long frameBits;
  static unsigned long bitsPerSlot;
  static FLOAT8 frac_SpF;
  static FLOAT8 slot_lag;
  static int mode_gr;
  static int old_bitrate;
  static unsigned long sentBits = 0;
  static int samplesPerFrame;
#define EXTRADELAY (1152+ENCDELAY-MDCTDELAY)
#define MFSIZE (EXTRADELAY+1152)
  static short int mfbuf[2][MFSIZE];
  FLOAT8 xr[2][2][576];
  int l3_enc[2][2][576];
  int mpg123count;
  III_psy_ratio masking_ratio;    /*LR ratios */
  III_psy_ratio masking_MS_ratio; /*MS ratios */
  III_psy_ratio *masking;  /*LR ratios and MS ratios*/
  III_scalefac_t scalefac;

  typedef FLOAT8 pedata[2][2];
  pedata pe,pe_MS;
  pedata *pe_use;

  int ch,gr,mean_bits;
  int bitsPerFrame;
  static int frame_buffered=0;
  int stereo;
  layer *info;
  int i;
  int check_ms_stereo;
  static FLOAT8 ms_ratio[2]={0,0};
  FLOAT8 ms_ratio_next=0;
  FLOAT8 ms_ratio_prev=0;

  stereo = fr_ps.stereo;
  info = fr_ps.header;
  info->mode_ext = MPG_MD_LR_LR; 
  /* use m/s stereo? */
  check_ms_stereo =   ((info->mode == MPG_MD_JOINT_STEREO) && 
		       (info->version == 1) &&
		       (stereo==2) );
  if (frameNum==0) old_bitrate=info->bitrate_index;
  info->bitrate_index = old_bitrate;

  if (frameNum==0)  {    
    FLOAT8 avg_slots_per_frame;
    FLOAT8 sampfreq =   s_freq[info->version][info->sampling_frequency];
    int bit_rate = bitrate[info->version][info->lay-1][info->bitrate_index];

    sentBits = 0;
    memset((char *) mfbuf, 0, sizeof(mfbuf));
    /* Figure average number of 'slots' per frame. */
    /* Bitrate means TOTAL for both channels, not per side. */
    bitsPerSlot = 8;
    samplesPerFrame = info->version == 1 ? 1152 : 576;
    avg_slots_per_frame = (bit_rate*samplesPerFrame) /
           (sampfreq* bitsPerSlot);
    frac_SpF  = avg_slots_per_frame - (int) avg_slots_per_frame;
    slot_lag  = -frac_SpF;
    info->padding = 1;
    if (fabs(frac_SpF) < 1e-9) info->padding = 0;
    mode_gr = (info->version == 1) ? 2 : 1;  /* mode_gr = 2 */

    assert(MFSIZE>=samplesPerFrame+EXTRADELAY);  
    /* check enougy space to store data needed by FFT */
    assert(MFSIZE>=BLKSIZE+samplesPerFrame-FFTOFFSET);     
    /* check FFT will not use data behond what we read in */
    assert(samplesPerFrame+EXTRADELAY>=BLKSIZE+samplesPerFrame-FFTOFFSET); 
    /* check FFT will not use a negative starting offset */
    assert(576>=FFTOFFSET);
  }


  /* shift in new samples, delayed by EXTRADELAY */
   for (ch=0; ch<stereo; ch++)
     for (i=0; i<EXTRADELAY; i++)
       mfbuf[ch][i]=mfbuf[ch][i+samplesPerFrame];
   for (ch=0; ch<stereo; ch++)
     for (i=0; i<samplesPerFrame; i++)
       mfbuf[ch][i+EXTRADELAY]=Buffer[ch][i];

   /* just buffer the first frame, and return */
   if (frameNum==0 && !frame_buffered) {
     frame_buffered=1;
     return 0;
   }else
     /* reset, in case we encode a second file after this one */
     frame_buffered=0;  



  /********************** padding *****************************/
  if (VBR) {
  /* leave info_padding as it was set above */
  } else {
    if (frac_SpF != 0) {
      if (slot_lag > (frac_SpF-1.0) ) {
	slot_lag -= frac_SpF;
	info->padding = 0;
      }
      else {
	info->padding = 1;
	slot_lag += (1-frac_SpF);
      }
    }
  }

#ifndef _BLADEDLL
  /********************** status display  *****************************/
  if (!gtkflag && !silent) {
    int mod = info->version == 0 ? 100 : 20;
    if (frameNum%mod==0) {
      timestatus(info,frameNum,totalframes);
#ifdef BRHIST
      if (disp_brhist)
	{
	  brhist_add_count();
	  brhist_disp();
	}
#endif
    }
  }

#endif /* _BLADEDLL */



  /***************************** Layer 3 **********************************

                       gr 0            gr 1
mfbuf:           |--------------|---------------|-------------|
MDCT output:    |--------------|---------------|-------------|

FFT's                      <---------1024---------->
                                           <---------1024-------->
    


    mfbuf = large buffer of PCM data
    encoder acts on mfbuf[ch][0], but output is delayed by MDCTDELAY
    so the MDCT coefficints are from mfbuf[ch][-MDCTDELAY]

    psy-model FFT has a 1 granule day, so we feed it data for the next granule.
    FFT is centered over granule:  224+576+224  
    So FFT starts at:   576-224-MDCTDELAY
   
    MPEG2:  FFT ends at:  BLKSIZE+576-224-MDCTDELAY
    MPEG1:  FFT ends at:  BLKSIZE+2*576-224-MDCTDELAY    ( 2 granules) 

    To perform the FFT, mfbuf should contain at least BLKSIZE+2*576+224-MDCTDELAY
    data, and 576-224-MDCTDELAY >= 0.  
   */
  if (!fast_mode) {  
    /* psychoacoustic model 
     * psy model has a 1 granule (576) delay that we must compensate for 
     * (mt 6/99). 
     */
    short int *bufp[2];  /* address of beginning of left & right granule */
    int blocktype[2];

    ms_ratio_prev=ms_ratio[mode_gr-1];
    for (gr=0; gr < mode_gr ; gr++) {

      for ( ch = 0; ch < stereo; ch++ )
	bufp[ch] = &mfbuf[ch][576 + gr*576-FFTOFFSET];

      L3psycho_anal( bufp, stereo, gr, info,
         s_freq[info->version][info->sampling_frequency] * 1000.0,
	 check_ms_stereo,&ms_ratio[gr],&ms_ratio_next,
         masking_ratio.l[gr], masking_ratio.s[gr], 
         masking_MS_ratio.l[gr], masking_MS_ratio.s[gr], 
         pe[gr],pe_MS[gr],blocktype);

      for ( ch = 0; ch < stereo; ch++ ) 
	l3_side.gr[gr].ch[ch].tt.block_type=blocktype[ch];

    }
  }else{
    for (gr=0; gr < mode_gr ; gr++) 
      for ( ch = 0; ch < stereo; ch++ ) {
	l3_side.gr[gr].ch[ch].tt.block_type=NORM_TYPE;
	pe[gr][ch]=700;
      }
  }


  /* block type flags */
  for( gr = 0; gr < mode_gr; gr++ ) {
    for ( ch = 0; ch < stereo; ch++ ) {
      gr_info *cod_info = &l3_side.gr[gr].ch[ch].tt;
      cod_info->mixed_block_flag = 0;     /* never used by this model */
      if (cod_info->block_type == NORM_TYPE )
	cod_info->window_switching_flag = 0;
      else
	cod_info->window_switching_flag = 1;
    }
  }

  /* polyphase filtering / mdct */
  mdct_sub48(mfbuf[0], mfbuf[1], xr, stereo, &l3_side, mode_gr);

  /* lowpass MDCT filtering */
  if (sfb21 || voice_mode || lowpass1>0 || highpass2>0) 
    filterMDCT(xr,&l3_side,&fr_ps);


  if (check_ms_stereo) {
    /* make sure block type is the same in each channel */
    check_ms_stereo = 
      (l3_side.gr[0].ch[0].tt.block_type==l3_side.gr[0].ch[1].tt.block_type) &&
      (l3_side.gr[1].ch[0].tt.block_type==l3_side.gr[1].ch[1].tt.block_type);
  }
  if (check_ms_stereo) {
    /* ms_ratio = is like the ratio of side_energy/total_energy */
    FLOAT8 ms_ratio_ave;
    /*     ms_ratio_ave = .5*(ms_ratio[0] + ms_ratio[1]);*/
    ms_ratio_ave = .25*(ms_ratio[0] + ms_ratio[1]+ 
			 ms_ratio_prev + ms_ratio_next);
    if (( ms_ratio_ave <.35) || force_ms )
           info->mode_ext = MPG_MD_MS_LR;
  }


#ifdef HAVEGTK
  if (gtkflag) { 
    int j;
    for ( gr = 0; gr < mode_gr; gr++ ) {
      for ( ch = 0; ch < stereo; ch++ ) {
	pinfo->ms_ratio[gr]=ms_ratio[gr];
	pinfo->blocktype[gr][ch]=
	  l3_side.gr[gr].ch[ch].tt.block_type;
	for ( j = 0; j < 576; j++ ) pinfo->xr[gr][ch][j]=xr[gr][ch][j];
	/* if MS stereo, switch to MS psy data */
	if (highq && (info->mode_ext==MPG_MD_MS_LR)) {
	  pinfo->pe[gr][ch]=pinfo->pe[gr][ch+2];
	  pinfo->ers[gr][ch]=pinfo->ers[gr][ch+2];
	  memcpy(pinfo->energy[gr][ch],pinfo->energy[gr][ch+2],
		 sizeof(pinfo->energy[gr][ch]));

	  memcpy(pinfo->thr[gr][ch],pinfo->thr[gr][ch+2],
		 sizeof(pinfo->thr[gr][ch]));
	  memcpy(pinfo->en[gr][ch],pinfo->en[gr][ch+2],
		 sizeof(pinfo->en[gr][ch]));
	  memcpy(pinfo->thr_s[gr][ch],pinfo->thr_s[gr][ch+2],
		 sizeof(pinfo->thr_s[gr][ch]));
	  memcpy(pinfo->en_s[gr][ch],pinfo->en_s[gr][ch+2],
		 sizeof(pinfo->en_s[gr][ch]));
	}
      }
    }
  }
#endif




  /* bit and noise allocation */
  if ((MPG_MD_MS_LR == info->mode_ext) && highq) {
    masking = &masking_MS_ratio;    /* use MS masking */
    pe_use=&pe_MS;
  } else {
    masking = &masking_ratio;    /* use LR masking */
    pe_use=&pe;
  }

  if (VBR) {
    VBR_iteration_loop( *pe_use, ms_ratio, xr, masking, &l3_side, l3_enc, 
		    &scalefac, &fr_ps);
  }else{
    iteration_loop( *pe_use, ms_ratio, xr, masking, &l3_side, l3_enc, 
		    &scalefac, &fr_ps);
  }
  /*
  VBR_iteration_loop_new( *pe_use, ms_ratio, xr, masking, &l3_side, l3_enc, 
			  &scalefac, &fr_ps);
  */



#ifdef BRHIST
  brhist_temp[info->bitrate_index]++;
#endif


  /*  write the frame to the bitstream  */
  old_bitrate = info->bitrate_index;
  getframebits(info,stereo,&bitsPerFrame,&mean_bits);
  III_format_bitstream( bitsPerFrame, &fr_ps, l3_enc, &l3_side, 
			&scalefac, &bs);


  frameBits = bs.totbit - sentBits;

  
  if ( frameBits % bitsPerSlot )   /* a program failure */
    fprintf( stderr, "Sent %ld bits = %ld slots plus %ld\n",
	     frameBits, frameBits/bitsPerSlot,
	     frameBits%bitsPerSlot );
  sentBits += frameBits;

  /* copy mp3 bit buffer into array */
  mpg123count = copy_buffer(mpg123bs,&bs);
  if (!lame_nowrite) write_buffer(&bs);  /* ouput to mp3 file */
  empty_buffer(&bs);  /* empty buffer */

  if (g_bWriteVbrTag) AddVbrFrame((int)(sentBits/8));

#ifdef HAVEGTK 
  if (gtkflag) { 
    int j;
    int framesize = (info->version==0) ? 576 : 1152;
    for ( ch = 0; ch < stereo; ch++ ) {
      for ( j = 0; j < FFTOFFSET; j++ )
	pinfo->pcmdata[ch][j] = pinfo->pcmdata[ch][j+framesize];
      for ( j = FFTOFFSET; j < 1600; j++ ) {
	pinfo->pcmdata[ch][j] = mfbuf[ch][j-FFTOFFSET];
      }
    }
  }
#endif
  frameNum++;

  return mpg123count;
}

#ifndef _BLADEDLL
int lame_readframe(short int Buffer[2][1152])
{
  int iread,ofreq;

  /* note: if input is stereo and output is mono, get_audio() 
   * will  .5*(L+R) in channel 0,  and nothing in channel 1. */
  /* DOWNSAMPLE, ETC */
#ifdef HAVEGTK
  if (gtkflag) {
    ofreq=1000*s_freq[info.version][info.sampling_frequency];  /* output */
    pinfo->frameNum = frameNum;
    pinfo->sampfreq=ofreq;
    pinfo->framesize=(fr_ps.header->version==0) ? 576 : 1152;
    pinfo->stereo = fr_ps.stereo;
  }
#endif
  if (resample_ratio!=1) {
    /* input frequency =  output freq*resample_ratio; */
    iread=get_audio_resample(Buffer,resample_ratio,fr_ps.stereo,fr_ps.header);
  }else{
    iread = get_audio(Buffer,fr_ps.stereo,fr_ps.header);
  }

  /* check to see if we overestimated/underestimated totalframes */
  if (iread==0)  totalframes = Min(totalframes,frameNum+2);
  if (frameNum > (totalframes-1)) totalframes = frameNum;
  return iread;
}
#endif /* _BLADEDLL */

/* initialize mp3 encoder */
void lame_init(int nowrite)
{

#ifdef __FreeBSD__
#include <floatingpoint.h>
  {
  /* seet floating point mask to the Linux default */
  fp_except_t mask;
  mask=fpgetmask();
  /* if bit is set, we get SIGFPE on that error! */
  fpsetmask(mask & ~(FP_X_INV|FP_X_DZ));
  /*  fprintf(stderr,"FreeBSD mask is 0x%x\n",mask); */
  }
#endif
#ifdef ABORTFP
  {
#include <fpu_control.h>
  unsigned int mask;
  _FPU_GETCW(mask);
  /* Set the Linux mask to abort on most FPE's */
  /* if bit is set, we _mask_ SIGFPE on that error! */
  /*  mask &= ~( _FPU_MASK_IM | _FPU_MASK_ZM | _FPU_MASK_OM | _FPU_MASK_UM );*/
   mask &= ~( _FPU_MASK_IM | _FPU_MASK_ZM | _FPU_MASK_OM );
  _FPU_SETCW(mask);
  }
#endif


#ifndef _BLADEDLL
  fprintf(stderr,"LAME version %s (www.sulaco.org/mp3) \n",get_lame_version());
  fprintf(stderr,"GPSYCHO: GPL psycho-acoustic model version %s. \n",get_psy_version());
#ifdef LIBSNDFILE
  fprintf(stderr,"Input handled by libsndfile (www.zip.com.au/~erikd/libsndfile)\n");
#endif
#endif /* _BLADEDLL */

  /* Global flags.  set defaults here */
  allow_diff_short=0;
  ATHonly=0;
  autoconvert=FALSE;
  disable_reservoir=0;
  experimentalX = 0;
  experimentalY = 0;
  experimentalZ = 0;
  fast_mode=0;
  frameNum=0;
  g_bWriteVbrTag=1;
  gtkflag=0;
  gpsycho=1;
  highq=0;
  input_format=sf_unknown;
  lame_nowrite=nowrite;
  lowpass1=0;
  lowpass2=0;
  highpass1=0;
  highpass2=0;
  no_short_blocks=0;
  resample_ratio=1;
  sfb21=1;
  silent=0;
  swapbytes=0;
  totalframes=0;
  VBR=0;
  VBR_q=4;
  VBR_min_bitrate=1;   /* 32kbs */
  VBR_max_bitrate=13;  /* 256kbs */
  voice_mode=0;
  psyModel=2; /* GPSYCHO is Model2 */


  /* Clear info structure */
  memset(&info,0,sizeof(info));
  memset(&bs, 0, sizeof(Bit_stream_struc));
  memset(&fr_ps, 0, sizeof(frame_params));
  memset(&l3_side,0x00,sizeof(III_side_info_t));

  force_ms=FALSE;
  
  fr_ps.header = &info;
  fr_ps.tab_num = -1;             /* no table loaded */
  fr_ps.alloc = NULL;
  info.version = MPEG_AUDIO_ID;   /* =1   Default: MPEG-1 */
  info.extension = 0;       

  InitFormatBitStream();

}

void lame_getmp3info(lame_mp3info *mp3info)
{
  mp3info->encoder_delay = ENCDELAY;
  mp3info->framesize = (fr_ps.header->version==0) ? 576 : 1152;
  mp3info->output_channels = fr_ps.stereo;
  mp3info->output_samplerate = (int)
     1000*s_freq[fr_ps.header->version][fr_ps.header->sampling_frequency];
}



#ifndef _BLADEDLL
int lame_cleanup(char *mpg123bs)
{
  int mpg123count;
  frameNum--;
  if (!gtkflag && !silent) {
      timestatus(&info,frameNum,totalframes);
#ifdef BRHIST
      if (disp_brhist)
	{
	  brhist_add_count();
	  brhist_disp();
	}
#endif
#ifdef BRHIST
      if (disp_brhist)
	brhist_disp_total();
#endif
      fprintf(stderr,"\n");
      fflush(stderr);
  }


  CloseSndFile();


  III_FlushBitstream();
  mpg123count = copy_buffer(mpg123bs,&bs);

  if (!lame_nowrite)
  {
    write_buffer(&bs);  /* ouput to mp3 file */
    empty_buffer(&bs);  /* empty buffer */
    fclose(bs.pt);    /* Close the file */
    desalloc_buffer(&bs);    /* Deallocate all buffers */
  
    if (g_bWriteVbrTag)
	{
	  /* Calculate relative quality of VBR stream  
	   * 0=best, 100=worst */
		int nQuality=VBR_q*100/9;
		/* Write Xing header again */
		PutVbrTag(outPath,nQuality);
	}

    
    /* write an ID3 tag  */
    if(id3tag.used) {
      id3_buildtag(&id3tag);
      id3_writetag(outPath, &id3tag);
    }
  }
  return mpg123count;
}
#endif /* _BLADEDLL */
