#include "globalflags.h"
#include "util.h"
#include "id3tag.h"
#include "get_audio.h"
#include "brhist.h"


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
  fprintf(stderr,"\n");
  fprintf(stderr,"USAGE   :  %s [options] <infile> [outfile]\n",name);
  fprintf(stderr,"\n<infile> and/or <outfile> can be \"-\", which means stdin/stdout.\n");
  fprintf(stderr,"\n");
  fprintf(stderr,"OPTIONS :\n");
  fprintf(stderr,"    -m mode         (s)tereo, (j)oint, (f)orce or (m)ono  (default j)\n");
  fprintf(stderr,"                    force = force ms_stereo on all frames. Faster\n");
  fprintf(stderr,"    -b bitrate      set the bitrate, default 128kbps\n");
  fprintf(stderr,"    -s sfreq        sampling frequency of input file(kHz) - default 44.1kHz\n");
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
  fprintf(stderr,"    -V n            quality setting for VBR.  default n=%i\n",gf.VBR_q);
  fprintf(stderr,"                    0=high quality,bigger files. 9=smaller files\n");
  fprintf(stderr,"    -b bitrate      specify minimum allowed bitrate, default 32kbs\n");
  fprintf(stderr,"    -B bitrate      specify maximum allowed bitrate, default 256kbs\n");
  fprintf(stderr,"    -t              disable Xing VBR informational tag\n");
  fprintf(stderr,"    --nohist        disable VBR histogram display\n");
  fprintf(stderr,"\n");
  fprintf(stderr,"    -h              higher quality, but a little slower\n");
  fprintf(stderr,"    -f              fast mode (very low quality)\n");
  fprintf(stderr,"    -k              keep ALL frequencies (disables all filters)\n");
  fprintf(stderr,"    -d              allow channels to have different blocktypes\n");
  fprintf(stderr,"  --athonly         only use the ATH for masking\n");
  fprintf(stderr,"  --noath           disable the ATH for masking\n");
  fprintf(stderr,"  --noshort         do not use short blocks\n");
  fprintf(stderr,"  --nores           disable the bit reservoir\n");
  fprintf(stderr,"  --cwlimit freq    compute tonality up to freq (in kHz)\n");
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
  int   err = 0, i = 0;
  int autoconvert=0;

  char *programName = argv[0]; 

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
	  gf.highq = TRUE;
	  break;
	case 'k': 
	  gf.sfb21=0;  
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
	  gf.fast_mode = 1;
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
	case 'c':       gf.copyright = 1; break;
	case 'o':       gf.original  = 0; break;
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
  if (inPath[0]=='-') {
    /* setup standard output on some machines. */
#ifdef __EMX__
    _fsetmode(stdout,"b");
#elif (defined  __BORLANDC__)
    setmode(_fileno(stdout), O_BINARY);
#elif (defined  __CYGWIN__)
    setmode(fileno(stdout), _O_BINARY);
#elif (defined _WIN32)
    _setmode(_fileno(stdout), _O_BINARY);
#endif
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



