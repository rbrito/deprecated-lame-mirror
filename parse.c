#ifndef LAMEPARSE

void lame_usage(char *) {return;}
void lame_help(char *) {return;}
void lame_parse_args(int argc, char **argv) {return;}

#else


#include "globalflags.h"
#include "util.h"
#include "id3tag.h"
#include "get_audio.h"
#include "brhist.h"
#include "version.h"



#define         MAX_NAME_SIZE           300
  char    inPath[MAX_NAME_SIZE];
  char    outPath[MAX_NAME_SIZE];


/************************************************************************
*
* usage
*
* PURPOSE:  Writes command line syntax to the file specified by #stderr#
*
************************************************************************/

void lame_usage(char *name)  /* print syntax & exit */
{
  lame_print_version(stderr);
  fprintf(stderr,"\n");
  fprintf(stderr,"USAGE   :  %s [options] <infile> [outfile]\n",name);
  fprintf(stderr,"\n<infile> and/or <outfile> can be \"-\", which means stdin/stdout.\n");
  fprintf(stderr,"\n");
  fprintf(stderr,"Try \"%s --help\" for more information\n",name);
  exit(1);
}



/************************************************************************
*
* usage
*
* PURPOSE:  Writes command line syntax to the file specified by #stdout#
*
************************************************************************/

void lame_help(char *name)  /* print syntax & exit */
{
  lame_print_version(stdout);
  fprintf(stdout,"\n");
  fprintf(stdout,"USAGE   :  %s [options] <infile> [outfile]\n",name);
  fprintf(stdout,"\n<infile> and/or <outfile> can be \"-\", which means stdin/stdout.\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"OPTIONS :\n");
  fprintf(stdout,"  Input options:\n");
  fprintf(stdout,"    -r              input is raw pcm\n");
  fprintf(stdout,"    -x              force byte-swapping of input\n");
  fprintf(stdout,"    -s sfreq        sampling frequency of input file(kHz) - default 44.1kHz\n");
  fprintf(stdout,"    --mp3input      input file is a MP3 file\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"  Filter options:\n");
  fprintf(stdout,"    -k              keep ALL frequencies (disables all filters)\n");
  fprintf(stdout,"  --lowpass freq         frequency(kHz), lowpass filter cutoff above freq\n");
  fprintf(stdout,"  --lowpass-width freq   frequency(kHz) - default 15%% of lowpass freq\n");
  fprintf(stdout,"  --highpass freq        frequency(kHz), highpass filter cutoff below freq\n");
  fprintf(stdout,"  --highpass-width freq  frequency(kHz) - default 15%% of highpass freq\n");
  fprintf(stdout,"  --resample sfreq  sampling frequency of output file(kHz)- default=input sfreq\n");
  fprintf(stdout,"  --cwlimit freq    compute tonality up to freq (in kHz)\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"  Operational options:\n");
  fprintf(stdout,"    -m mode         (s)tereo, (j)oint, (f)orce or (m)ono  (default j)\n");
  fprintf(stdout,"                    force = force ms_stereo on all frames. Faster\n");
  fprintf(stdout,"    -a              downmix from stereo to mono file for mono encoding\n");
  fprintf(stdout,"    -b bitrate      set the bitrate, default 128kbps\n");
  fprintf(stdout,"    -h              higher quality, but a little slower\n");
  fprintf(stdout,"    -f              fast mode (very low quality)\n");
  fprintf(stdout,"    -d              allow channels to have different blocktypes\n");
  fprintf(stdout,"    --athonly       only use the ATH for masking\n");
  fprintf(stdout,"    --noath         disable the ATH for masking\n");
  fprintf(stdout,"    --noshort       do not use short blocks\n");
  fprintf(stdout,"    --voice         experimental voice mode\n");
  fprintf(stdout,"    --preset type   type must be phone, voice, fm, tape, hifi, cd or studio\n");
  fprintf(stdout,"                    help gives some more infos on these\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"  VBR options:\n");
  fprintf(stdout,"    -v              use variable bitrate (VBR)\n");
  fprintf(stdout,"    -V n            quality setting for VBR.  default n=%i\n",gf.VBR_q);
  fprintf(stdout,"                    0=high quality,bigger files. 9=smaller files\n");
  fprintf(stdout,"    -b bitrate      specify minimum allowed bitrate, default 32kbs\n");
  fprintf(stdout,"    -B bitrate      specify maximum allowed bitrate, default 256kbs\n");
  fprintf(stdout,"    -t              disable Xing VBR informational tag\n");
  fprintf(stdout,"    -S              don't print progress report, VBR histograms\n");
  fprintf(stdout,"    --nohist        disable VBR histogram display\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"  MP3 header/stream options:\n");
  fprintf(stdout,"    -e emp          de-emphasis n/5/c  (obsolete)\n");
  fprintf(stdout,"    -c              mark as copyright\n");
  fprintf(stdout,"    -o              mark as non-original\n");
  fprintf(stdout,"    -p              error protection.  adds 16bit checksum to every frame\n");
  fprintf(stdout,"                    (the checksum is computed correctly)\n");
  fprintf(stdout,"    --nores         disable the bit reservoir\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"  Specifying any of the following options will add an ID3 tag:\n");
  fprintf(stdout,"     --tt \"title\"     title of song (max 30 chars)\n");
  fprintf(stdout,"     --ta \"artist\"    artist who did the song (max 30 chars)\n");
  fprintf(stdout,"     --tl \"album\"     album where it came from (max 30 chars)\n");
  fprintf(stdout,"     --ty \"year\"      year in which the song/album was made (max 4 chars)\n");
  fprintf(stdout,"     --tc \"comment\"   additional info (max 30 chars)\n");
  fprintf(stdout,"                      (or max 28 chars if using the \"track\" option)\n");
  fprintf(stdout,"     --tn \"track\"     track number of the song on the CD (1 to 99)\n");
  fprintf(stdout,"                      (using this option will add an ID3v1.1 tag)\n");
  fprintf(stdout,"     --tg \"genre\"     genre of song (name or number)\n");
  fprintf(stdout,"\n");
#ifdef HAVEGTK
  fprintf(stdout,"    -g              run graphical analysis on <infile>\n");
#endif
  display_bitrates(stdout);
  exit(0);
}



/************************************************************************
*
* usage
*
* PURPOSE:  Writes presetting info to #stdout#
*
************************************************************************/

void lame_presets_info(char *name)  /* print syntax & exit */
{
  lame_print_version(stdout);
  fprintf(stdout,"\n");
  fprintf(stdout,"Presets are some shortcuts for common settings.\n");
  fprintf(stdout,"They can be combined with -v if you want VBR MP3s.\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"  --preset phone    =>  --resample      16\n");
  fprintf(stdout,"                        --highpass       0.260\n");
  /*
  fprintf(stdout,"                        --highpasswidth  0.040\n");
  */
  fprintf(stdout,"                        --lowpass        3.800\n");
  /*
  fprintf(stdout,"                        --lowpasswidth   0.300\n");
  */
  fprintf(stdout,"                        --noshort\n");
  fprintf(stdout,"                        -m   m\n");
  fprintf(stdout,"                        -b  16\n");
  fprintf(stdout,"                  plus  -b   8  \\\n");
  fprintf(stdout,"                        -B  56   > in combination with -v\n");
  fprintf(stdout,"                        -V   5  /\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"  --preset voice:   =>  --resample      24\n");
  /*
  fprintf(stdout,"                        --highpass       0.100\n");
  fprintf(stdout,"                        --highpasswidth  0.020\n");
  */
  fprintf(stdout,"                        --lowpass       11\n");
  /*
  fprintf(stdout,"                        --lowpasswidth   2\n");
  */
  fprintf(stdout,"                        --noshort\n");
  fprintf(stdout,"                        -m   m\n");
  fprintf(stdout,"                        -b  32\n");
  fprintf(stdout,"                  plus  -b   8  \\\n");
  fprintf(stdout,"                        -B  96   > in combination with -v\n");
  fprintf(stdout,"                        -V   4  /\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"  --preset fm:      =>  --resample      32\n");
  /*
  fprintf(stdout,"                        --highpass       0.030\n");
  fprintf(stdout,"                        --highpasswidth  0\n");
  */
  fprintf(stdout,"                        --lowpass       11.4\n");
  /*
  fprintf(stdout,"                        --lowpasswidth   0\n");
  */
  fprintf(stdout,"                        -m   j\n");
  fprintf(stdout,"                        -b  96\n");
  fprintf(stdout,"                  plus  -b  32  \\\n");
  fprintf(stdout,"                        -B 192   > in combination with -v\n");
  fprintf(stdout,"                        -V   4  /\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"  --preset tape:    =>  --lowpass       16\n");
  /*
  fprintf(stdout,"                        --lowpasswidth   2\n");
  fprintf(stdout,"                        --highpass       0.015\n");
  fprintf(stdout,"                        --highpasswidth  0.015\n");
  */
  fprintf(stdout,"                        -m   j\n");
  fprintf(stdout,"                        -b 128\n");
  fprintf(stdout,"                  plus  -b  32  \\\n");
  fprintf(stdout,"                        -B 192   > in combination with -v\n");
  fprintf(stdout,"                        -V   4  /\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"  --preset hifi:    =>  --lowpass       18.5\n");
  /*
  fprintf(stdout,"                        --lowpasswidth   3\n");
  */
  fprintf(stdout,"                        -h\n");
  fprintf(stdout,"                        -m   j\n");
  fprintf(stdout,"                        -b 160\n");
  fprintf(stdout,"                  plus  -b  32  \\\n");
  fprintf(stdout,"                        -B 224   > in combination with -v\n");
  fprintf(stdout,"                        -V   3  /\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"  --preset cd:      =>  --lowpass 20.5\n");
  fprintf(stdout,"                        -h\n");
  fprintf(stdout,"                        -m   s\n");
  fprintf(stdout,"                        -b 192\n");
  fprintf(stdout,"                  plus  -b  80  \\\n");
  fprintf(stdout,"                        -B 256   > in combination with -v\n");
  fprintf(stdout,"                        -V   2  /\n");
  fprintf(stdout,"\n");
  fprintf(stdout,"  --preset studio:  =>  -k\n");
  fprintf(stdout,"                        -h\n");
  fprintf(stdout,"                        -m   s\n");
  fprintf(stdout,"                        -b 256\n");
  fprintf(stdout,"                  plus  -b 112  \\\n");
  fprintf(stdout,"                        -B 320   > in combination with -v\n");
  fprintf(stdout,"                        -V   0  /\n");
  fprintf(stdout,"\n");

  exit(0);
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
  int   err = 0, i = 0;
  int autoconvert=0;

  char *programName = argv[0]; 
  int track = 0;

  inPath[0] = '\0';   
  outPath[0] = '\0';
  gf.inPath=inPath;
  gf.outPath=outPath;

  id3_inittag(&id3tag);
  id3tag.used = 0;

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
	  gf.resamplerate =  (( 1000.0 * srate ) + 0.5);
	  if (srate  < 1) {
	    fprintf(stderr,"Must specify a samplerate with --resample\n");
	    exit(1);
	  }
	}
	else if (strcmp(token, "mp3input")==0) {
	  gf.input_format=sf_mp3;
	}
	else if (strcmp(token, "voice")==0) {
	  gf.lowpassfreq=12000;
	  gf.VBR_max_bitrate_kbps=160;
	  gf.no_short_blocks=1;
	}
	else if (strcmp(token, "noshort")==0) {
	  gf.no_short_blocks=1;
	}
	else if (strcmp(token, "noath")==0) {
	  gf.noATH=1;
	}
	else if (strcmp(token, "nores")==0) {
	  gf.disable_reservoir=1;
	  gf.padding=0;
	}
	else if (strcmp(token, "athonly")==0) {
	  gf.ATHonly=1;
	}
	else if (strcmp(token, "nohist")==0) {
#ifdef BRHIST
	  disp_brhist = 0;
#endif
	}
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
 	else if (strcmp(token, "tn")==0) {
 		id3tag.used=1; argUsed = 1;
  		track = atoi(nextArg);
  		if (track < 1) { track = 1; }
  		if (track > 99) { track = 99; }
  		id3tag.track = track;
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
	else if (strcmp(token, "lowpass")==0) {
	  argUsed=1;
	  gf.lowpassfreq =  (( 1000.0 * atof( nextArg ) ) + 0.5);
	  if (gf.lowpassfreq  < 1) {
	    fprintf(stderr,"Must specify lowpass with --lowpass freq, freq >= 0.001 kHz\n");
	    exit(1);
	  }
	}
	else if (strcmp(token, "lowpass-width")==0) {
	  argUsed=1;
	  gf.lowpasswidth =  (( 1000.0 * atof( nextArg ) ) + 0.5);
	  if (gf.lowpasswidth  < 0) {
	    fprintf(stderr,"Must specify lowpass width with --lowpass-width freq, freq >= 0 kHz\n");
	    exit(1);
	  }
	}
	else if (strcmp(token, "highpass")==0) {
	  argUsed=1;
	  gf.highpassfreq =  (( 1000.0 * atof( nextArg ) ) + 0.5);
	  if (gf.highpassfreq  < 1) {
	    fprintf(stderr,"Must specify highpass with --highpass freq, freq >= 0.001 kHz\n");
	    exit(1);
	  }
	}
	else if (strcmp(token, "highpass-width")==0) {
	  argUsed=1;
	  gf.highpasswidth =  (( 1000.0 * atof( nextArg ) ) + 0.5);
	  if (gf.highpasswidth  < 0) {
	    fprintf(stderr,"Must specify highpass width with --highpass-width freq, freq >= 0 kHz\n");
	    exit(1);
	  }
	}
	else if (strcmp(token, "cwlimit")==0) {
	  argUsed=1;
	  gf.cwlimit =  atoi( nextArg );
	  if (gf.cwlimit <= 0 ) {
	    fprintf(stderr,"Must specify cwlimit in kHz\n");
	    exit(1);
	  }
	} /* some more GNU-ish options could be added
	   * version       => complete name, version and license info (normal exit)  
	   * quiet/silent  => no messages on screen
	   * brief         => few messages on screen (name, status report)
	   * verbose       => all infos to screen (brhist, internal flags/filters)
	   * o/output file => specifies output filename
	   * O             => stdout
	   * i/input file  => specifies input filename
	   * I             => stdin
	   */
	else if (strcmp(token, "help") ==0
	       ||strcmp(token, "usage")==0){
	  lame_help(programName);  /* doesn't return */
	}
	else if (strcmp(token, "preset")==0) {
	  argUsed=1;
	  if (strcmp(nextArg,"phone")==0)
	  { /* when making changes, please update help text too */
	    gf.brate = 16; 
	    gf.highpassfreq=260;
	    /* 
            gf.highpasswidth=40; 
	    gf.lowpassfreq=3700;
	    gf.lowpasswidth=300;
            */
	    gf.VBR_q=5;
	    gf.VBR_min_bitrate_kbps=8;
	    gf.VBR_max_bitrate_kbps=56;
	    gf.no_short_blocks=1;
	    /*
	    gf.resamplerate =  16000;
	    */
	    gf.mode = MPG_MD_MONO; 
	    gf.mode_fixed = 1; 
	    gf.quality = 5;
	  }
	  else if (strcmp(nextArg,"voice")==0)
	  { /* when making changes, please update help text too */
	    gf.brate = 56; 
	    /*
	    gf.highpassfreq=100;  
	    gf.highpasswidth=20;
	    gf.lowpasswidth=2000;
	    */
	    gf.lowpassfreq=11000;
	    gf.VBR_q=4;
	    gf.VBR_min_bitrate_kbps=8;
	    gf.VBR_max_bitrate_kbps=96;
	    gf.no_short_blocks=1;
	    gf.mode = MPG_MD_MONO; 
	    gf.mode_fixed = 1; 
	    gf.resamplerate =  24000; 
	    gf.quality = 5;
	  }
	  else if (strcmp(nextArg,"fm")==0)
	  { /* when making changes, please update help text too */
	    gf.brate = 96; 
	    gf.VBR_q=4;
	    gf.VBR_min_bitrate_kbps=32;
	    gf.VBR_max_bitrate_kbps=192;
	    gf.mode = MPG_MD_JOINT_STEREO; 
	    gf.mode_fixed = 1; 
	    /*gf.resamplerate =  32000; */ /* determined automatically based on bitrate & sample freq. */
	    gf.quality = 5;
	  }
	  else if (strcmp(nextArg,"tape")==0)
	  { /* when making changes, please update help text too */
	    gf.brate = 128; 
	    gf.VBR_q=4;
	    gf.VBR_min_bitrate_kbps=32;
	    gf.VBR_max_bitrate_kbps=192;
	    gf.mode = MPG_MD_JOINT_STEREO; 
	    gf.mode_fixed = 1; 
	    gf.quality = 5;
	  }
	  else if (strcmp(nextArg,"hifi")==0)
	  { /* when making changes, please update help text too */
	    gf.brate = 160; 
	    gf.VBR_q=3;
	    gf.VBR_min_bitrate_kbps=32;
	    gf.VBR_max_bitrate_kbps=224;
	    gf.mode = MPG_MD_JOINT_STEREO; 
	    gf.mode_fixed = 1; 
	    gf.quality = 2;
	  }
	  else if (strcmp(nextArg,"cd")==0)
	  { /* when making changes, please update help text too */
	    gf.brate = 192; 
	    gf.VBR_q=2;
	    gf.VBR_min_bitrate_kbps=80;
	    gf.VBR_max_bitrate_kbps=256;
	    gf.mode = MPG_MD_STEREO; 
	    gf.mode_fixed = 1; 
	    gf.quality = 2;
	  }
	  else if (strcmp(nextArg,"studio")==0)
	  { /* when making changes, please update help text too */
	    gf.brate = 256; 
	    gf.VBR_q=0;
	    gf.VBR_min_bitrate_kbps=112;
	    gf.VBR_max_bitrate_kbps=320;
	    gf.mode = MPG_MD_STEREO; 
	    gf.mode_fixed = 1; 
	    gf.quality = 2; /* should be 0, but does not work now */
	  }
	  else if (strcmp(nextArg,"help")==0)
	  {
	    lame_presets_info(programName);  /* doesn't return */
	  }
	  else
	    {
	      fprintf(stderr,"%s: --preset type, type must be phone, voice, fm, tape, hifi, cd or studio, not %s\n",
		      programName, nextArg);
	      exit(1);
	    }
	} /* --preset */
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
	case 'm':        argUsed = 1;   gf.mode_fixed = 1;
	  if (*arg == 's')
	    { gf.mode = MPG_MD_STEREO; }
	  else if (*arg == 'd')
	    { gf.mode = MPG_MD_DUAL_CHANNEL; }
	  else if (*arg == 'j')
	    { gf.mode = MPG_MD_JOINT_STEREO; }
	  else if (*arg == 'f')
	    { gf.mode = MPG_MD_JOINT_STEREO; gf.force_ms=1; }
	  else if (*arg == 'm')
	    { gf.mode = MPG_MD_MONO; }
	  else {
	    fprintf(stderr,"%s: -m mode must be s/d/j/f/m not %s\n",
		    programName, arg);
	    err = 1;
	  }
	  break;
	case 'V':        argUsed = 1;   gf.VBR = 1;  
	  if (*arg == '0')
	    { gf.VBR_q=0; }
	  else if (*arg == '1')
	    { gf.VBR_q=1; }
	  else if (*arg == '2')
	    { gf.VBR_q=2; }
	  else if (*arg == '3')
	    { gf.VBR_q=3; }
	  else if (*arg == '4')
	    { gf.VBR_q=4; }
	  else if (*arg == '5')
	    { gf.VBR_q=5; }
	  else if (*arg == '6')
	    { gf.VBR_q=6; }
	  else if (*arg == '7')
	    { gf.VBR_q=7; }
	  else if (*arg == '8')
	    { gf.VBR_q=8; }
	  else if (*arg == '9')
	    { gf.VBR_q=9; }
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
	  gf.samplerate =  (( 1000.0 * srate ) + 0.5);
	  break;
	case 'b':        
	  argUsed = 1;
	  gf.brate = atoi(arg); 
	  gf.VBR_min_bitrate_kbps=gf.brate;
	  break;
	case 'B':        
	  argUsed = 1;
	  gf.VBR_max_bitrate_kbps=atoi(arg); 
	  break;	
	case 't':  /* dont write VBR tag */
	  gf.bWriteVbrTag=0;
	  break;
	case 'r':  /* force raw pcm input file */
#ifdef LIBSNDFILE
	  fprintf(stderr,"WARNING: libsndfile may ignore -r and perform fseek's on the input.\n");
	  fprintf(stderr,"Compile without libsndfile if this is a problem.\n");
#endif
	  gf.input_format=sf_raw;
	  break;
	case 'x':  /* force byte swapping */
	  gf.swapbytes=TRUE;
	  break;
	case 'p': /* (jo) error_protection: add crc16 information to stream */
	  gf.error_protection = 1; 
	  break;
	case 'a': /* autoconvert input file from stereo to mono - for mono mp3 encoding */
	  autoconvert=1;
	  gf.mode=MPG_MD_MONO;
	  gf.mode_fixed=1;
	  break;
	case 'h': 
	  gf.quality = 2;
	  break;
	case 'k': 
	  gf.lowpassfreq=-1;
	  gf.highpassfreq=-1;
	  break;
	case 'd': 
	  gf.allow_diff_short = 1;
	  break;
	case 'v': 
	  gf.VBR = 1; 
	  break;
	case 'S': 
	  gf.silent = TRUE;
	  break;
	case 'X':        argUsed = 1;   gf.experimentalX = 0;
	  if (*arg == '0')
	    { gf.experimentalX=0; }
	  else if (*arg == '1')
	    { gf.experimentalX=1; }
	  else if (*arg == '2')
	    { gf.experimentalX=2; }
	  else if (*arg == '3')
	    { gf.experimentalX=3; }
	  else if (*arg == '4')
	    { gf.experimentalX=4; }
	  else if (*arg == '5')
	    { gf.experimentalX=5; }
	  else if (*arg == '6')
	    { gf.experimentalX=6; }
	  else {
	    fprintf(stderr,"%s: -X n must be 0-6, not %s\n",
		    programName, arg);
	    err = 1;
	  }
	  break;


	case 'Y': 
	  gf.experimentalY = TRUE;
	  break;
	case 'Z': 
	  gf.experimentalZ = TRUE;
	  break;
	case 'f': 
	  gf.quality= 9;
	  break;
#ifdef HAVEGTK
	case 'g': /* turn on gtk analysis */
	  gf.gtkflag = TRUE;
	  break;
#endif
	case 'e':        argUsed = 1;
	  if (*arg == 'n')                    gf.emphasis = 0;
	  else if (*arg == '5')               gf.emphasis = 1;
	  else if (*arg == 'c')               gf.emphasis = 3;
	  else {
	    fprintf(stderr,"%s: -e emp must be n/5/c not %s\n",
		    programName, arg);
	    err = 1;
	  }
	  break;
	case 'c':   gf.copyright = 1; break;
	case 'o':   gf.original  = 0; break;
	
	case '?':   lame_help(programName);  /* doesn't return */
	default:    fprintf(stderr,"%s: unrec option %c\n",
				programName, c);
	err = 1; break;
	}
	if(argUsed) {
	  if(arg == token)    token = "";   /* no more from token */
	  else                ++i;          /* skip arg we used */
	  arg = ""; argUsed = 0;
	}
      }
    } else {
      if(inPath[0] == '\0')       strncpy(inPath, argv[i], MAX_NAME_SIZE);
      else if(outPath[0] == '\0') strncpy(outPath, argv[i], MAX_NAME_SIZE);
      else {
	fprintf(stderr,"%s: excess arg %s\n", programName, argv[i]);
	err = 1;
      }
    }
  }  /* loop over args */



  if(err || inPath[0] == '\0') lame_usage(programName);  /* never returns */
  if (inPath[0]=='-') gf.silent=1;  /* turn off status - it's broken for stdin */
  if(outPath[0] == '\0') {
    if (inPath[0]=='-') {
      /* if input is stdin, default output is stdout */
      strcpy(outPath,"-");
    }else {
      strncpy(outPath, inPath, MAX_NAME_SIZE - 4);
      strncat(outPath, ".mp3", 4 );
    }
  }
  /* some file options not allowed with stdout */
  if (outPath[0]=='-') {
    gf.bWriteVbrTag=0; /* turn off VBR tag */
    if (id3tag.used) {
      id3tag.used=0;         /* turn of id3 tagging */
      fprintf(stderr,"id3tag ignored: id3 tagging not supported for stdout.\n");
    }
  }


  /* if user did not explicitly specify input is mp3, check file name */
  if (gf.input_format != sf_mp3)
    if (!(strcmp((char *) &inPath[strlen(inPath)-4],".mp3")))
      gf.input_format = sf_mp3;

  /* default guess for number of channels */
  if (autoconvert) gf.num_channels=2; 
  else if (gf.mode == MPG_MD_MONO) gf.num_channels=1;
  else gf.num_channels=2;
  

}



#endif
