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

void lame_print_license(lame_global_flags *gfp,char *name)  /* print version,license & exit */
{
  LAME_PRINT_VERSION1();
  PRINTF1("\n");
  PRINTF1("Read the file \"LICENSE\"\n");
  PRINTF1("\n");
  LAME_NORMAL_EXIT();
}



/************************************************************************
*
* usage
*
* PURPOSE:  Writes command line syntax to the file specified by #stderr#
*
************************************************************************/

void lame_usage(lame_global_flags *gfp,char *name)  /* print syntax & exit */
{
  LAME_PRINT_VERSION2();
  PRINTF2("\n");
  PRINTF2("USAGE   :  %s [options] <infile> [outfile]\n",name);
  PRINTF2("\n<infile> and/or <outfile> can be \"-\", which means stdin/stdout.\n");
  PRINTF2("\n");
  PRINTF2("Try \"%s --help\"     for more information\n",name);
  PRINTF2(" or \"%s --longhelp\" for a complete options list\n",name);
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

void lame_short_help(lame_global_flags *gfp,char *name)  /* print syntax & exit */
{
  LAME_PRINT_VERSION1(); /* prints two lines */
  PRINTF1("\n");
  PRINTF1("USAGE       :  %s [options] <infile> [outfile]\n",name);
  PRINTF1("\n<infile> and/or <outfile> can be \"-\", which means stdin/stdout.\n");
  PRINTF1("\n");
  PRINTF1("RECOMMENDED :  ");
  PRINTF1("lame -h input.mp3 output.wav\n");
  PRINTF1("\n");
  PRINTF1("OPTIONS :\n");
  PRINTF1("    -b bitrate      set the bitrate, default 128kbps\n");
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

void lame_help(lame_global_flags *gfp,char *name)  /* print syntax & exit */
{
  LAME_PRINT_VERSION1();
  PRINTF1("\n");
  PRINTF1("USAGE   :  %s [options] <infile> [outfile]\n",name);
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
  PRINTF1("    --ogg           encode to Ogg Vorbis instead of MP3\n");
  PRINTF1("    --freeformat    produce a free format bitstream\n");
  PRINTF1("    --decode        input=mp3 file, output=wav\n");
  PRINTF1("    -t              disable writing wav header when using --decode\n");
  PRINTF1("    --comp  <arg>   choose bitrate to achive a compression ratio of <arg>\n");
  PRINTF1("    --athonly       only use the ATH for masking\n");
  PRINTF1("    --noath         disable the ATH for masking\n");
  PRINTF1("    --noshort       do not use short blocks\n");
  PRINTF1("    --voice         experimental voice mode\n");
  PRINTF1("    --preset type   type must be phone, voice, fm, tape, hifi, cd or studio\n");
  PRINTF1("                    \"--preset help\" gives some more infos on these\n");
  PRINTF1("\n");
  PRINTF1("  CBR (constant bitrate, the default) options:\n");
  PRINTF1("    -h              higher quality, but a little slower.  Recommended.\n");
  PRINTF1("    -f              fast mode (lower quality)\n");
  PRINTF1("    -b <bitrate>    set the bitrate in kbs, default 128kbps\n");
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
  PRINTF1("    -b <bitrate>    specify minimum allowed bitrate, default 32kbs\n");
  PRINTF1("    -B <bitrate>    specify maximum allowed bitrate, default 256kbs\n");
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

void lame_presets_info(lame_global_flags *gfp,char *name)  /* print syntax & exit */
{
  LAME_PRINT_VERSION1();
  PRINTF1("\n");
  PRINTF1("Presets are some shortcuts for common settings.\n");
  PRINTF1("They can be combined with -v if you want VBR MP3s.\n");
  PRINTF1("\n");
  PRINTF1("                 phone   sw     am    fm  voice radio  tape  hifi   cd studio\n");
  PRINTF1("=============================================================================\n");
  PRINTF1("--resample        8.0  11.025 16.0  22.05 32.0                               \n");
  PRINTF1("--lowpass         3.2   4.800  7.2   9.95 12.3  15.0  18.5  20.24            \n");
  PRINTF1("--lowpass-width   1.0   0.500  0.5   0.88  2.0   0.0   2.0   2.20            \n");
  PRINTF1("--noshort         yes                      yes                               \n");
  PRINTF1("-h                                                           yes   yes   yes \n");
  PRINTF1("-m                  m     m      m     j     m     j     j     j     s     s \n");
  PRINTF1("-b                 16    24     32    64    56   112   128   160   192   256 \n");
  PRINTF1("-- PLUS WITH -v -------------------------------------------------------------\n");
  PRINTF1("-V                  6     5      5     5     4     4     4     3     2     0 \n");
  PRINTF1("-b                  8     8     16    24    32    64    96   112   128   160 \n");
  PRINTF1("-B                 56    64    128   160   128   256   320   320   320   320 \n");
  PRINTF1("\n");
  PRINTF1("EXAMPLES:\n");
  PRINTF1(" a) --preset fm\n");
  PRINTF1("    equals: --resample 22.05 --lowpass 9.95 --lowpass-width 0.88 -mj -b 64\n");
  PRINTF1(" b) -v --preset studio\n");
  PRINTF1("    equals: -h -m s -V 0 -b 160 -B 320\n");
  PRINTF1("\n");

  LAME_NORMAL_EXIT();
}



static void genre_list_handler(int num,const char *name,void *cookie)
{
  PRINTF1("%3d %s\n",num,name);
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
void lame_parse_args(lame_global_flags *gfp,int argc, char **argv)
{
  FLOAT srate;
  int   err = 0, i = 0;
  int autoconvert=0;
  int user_quality=-1;
  char *programName = argv[0]; 

  gfp->inPath[0] = '\0';   
  gfp->outPath[0] = '\0';
  /* turn on display options. user settings may turn them off below */
  gfp->silent=0;
  gfp->brhist_disp = 1;
  gfp->id3v1_enabled = 1;
  id3tag_init(&gfp->tag_spec);

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
	if(gfp->inPath[0] == '\0')       strncpy(gfp->inPath, argv[i],MAX_NAME_SIZE);
	else if(gfp->outPath[0] == '\0') strncpy(gfp->outPath, argv[i],MAX_NAME_SIZE);
      } 
      if (*token == '-') {
	/* GNU style */
	token++;

	if (strcmp(token, "resample")==0) {
	  argUsed=1;
	  srate = atof( nextArg );
	  if (srate <= 0) {
	    ERRORF("Must specify a samplerate with --resample\n");
	    LAME_ERROR_EXIT();
	  }
	  /* useful are 0.001 ... 384 kHz, 384 Hz ... 192000 Hz */
	  gfp -> out_samplerate =  ( srate * (srate <= 384. ? 1.e3 : 1.e0 ) + 0.5 );
	}
	else if (strcmp(token, "vbr-old")==0) {
	  gfp->VBR = vbr_rh; 
	  gfp->quality = 2;
	}
	else if (strcmp(token, "vbr-new")==0) {
	  gfp->VBR = vbr_mt; 
	  gfp->quality = 2;
	}
	else if (strcmp(token, "abr")==0) {
	  argUsed=1;
	  gfp->VBR = vbr_abr; 
	  gfp->VBR_mean_bitrate_kbps = atoi(nextArg);
	  /* values larger than 8000 are bps (like Fraunhofer), so it's strange
             to get 320000 bps MP3 when specifying 8000 bps MP3 */
	  if ( gfp -> VBR_mean_bitrate_kbps >= 8000 )
	      gfp -> VBR_mean_bitrate_kbps = ( gfp -> VBR_mean_bitrate_kbps + 500 ) / 1000;
	  gfp->VBR_mean_bitrate_kbps = Min(gfp->VBR_mean_bitrate_kbps,320); 
	  gfp->VBR_mean_bitrate_kbps = Max(gfp->VBR_mean_bitrate_kbps,8); 
	}
	else if (strcmp(token, "mp1input")==0) {
	  gfp->input_format=sf_mp1;
	}
	else if (strcmp(token, "mp2input")==0) {
	  gfp->input_format=sf_mp2;
	}
	else if (strcmp(token, "mp3input")==0) {
	  gfp->input_format=sf_mp3;
	}
	else if (strcmp(token, "ogginput")==0) {
#ifdef HAVEVORBIS
	  gfp->input_format=sf_ogg;
#else
	  ERRORF("Error: LAME not compiled with Vorbis support\n");
	  LAME_ERROR_EXIT();
#endif
	}
	else if (strcmp(token, "ogg")==0) {
#ifdef HAVEVORBIS
	  gfp->ogg=1;
#else
	  ERRORF("Error: LAME not compiled with Vorbis support\n");
	  LAME_ERROR_EXIT();
#endif

	}
	else if (strcmp(token, "voice")==0) {
	  gfp->lowpassfreq=12000;
	  gfp->VBR_max_bitrate_kbps=160;
	  gfp->no_short_blocks=1;
	}
	else if (strcmp(token, "noshort")==0) {
	  gfp->no_short_blocks=1;
	}
	else if (strcmp(token, "decode")==0) {
	  gfp->decode_only=1;
	}
	else if (strcmp(token, "noath")==0) {
	  gfp->noATH=1;
	}
	else if (strcmp(token, "nores")==0) {
	  gfp->disable_reservoir=1;
	  gfp->padding_type=0;
	}
	else if (strcmp(token, "strictly-enforce-ISO")==0) {
	  gfp->strict_ISO=1;
	}
	else if (strcmp(token, "athonly")==0) {
	  gfp->ATHonly=1;
	}
	else if (strcmp(token, "athlower")==0) {
	  argUsed=1;
	  gfp->ATHlower = atoi(nextArg);
	}
	else if (strcmp(token, "freeformat")==0) {
	  gfp->free_format=1;
	}
	else if (strcmp(token, "athshort")==0) {
	  gfp->ATHshort=1;
	}
	else if (strcmp(token, "nohist")==0) {
	  gfp->brhist_disp = 0;
	}
	/* options for ID3 tag */
 	else if (strcmp(token, "tt")==0) {
 	  argUsed=1;
 	  id3tag_set_title(&gfp->tag_spec, nextArg);
 	}
 	else if (strcmp(token, "ta")==0) {
 	  argUsed=1;
 	  id3tag_set_artist(&gfp->tag_spec, nextArg);
 	}
 	else if (strcmp(token, "tl")==0) {
 	  argUsed=1;
 	  id3tag_set_album(&gfp->tag_spec, nextArg);
 	}
 	else if (strcmp(token, "ty")==0) {
 	  argUsed=1;
 	  id3tag_set_year(&gfp->tag_spec, nextArg);
 	}
 	else if (strcmp(token, "tc")==0) {
 	  argUsed=1;
 	  id3tag_set_comment(&gfp->tag_spec, nextArg);
 	}
 	else if (strcmp(token, "tn")==0) {
 	  argUsed=1;
 	  id3tag_set_track(&gfp->tag_spec, nextArg);
 	}
 	else if (strcmp(token, "tg")==0) {
 	  argUsed=1;
 	  if (id3tag_set_genre(&gfp->tag_spec, nextArg)) {
	    ERRORF("Unknown genre: %s.  Specify genre name or number\n", nextArg);
	    LAME_ERROR_EXIT();
          }
	}
 	else if (strcmp(token, "add-id3v2")==0) {
 	  id3tag_add_v2(&gfp->tag_spec);
 	}
 	else if (strcmp(token, "id3v1-only")==0) {
 	  id3tag_v1_only(&gfp->tag_spec);
 	}
 	else if (strcmp(token, "id3v2-only")==0) {
 	  id3tag_v2_only(&gfp->tag_spec);
 	}
 	else if (strcmp(token, "space-id3v1")==0) {
 	  id3tag_space_v1(&gfp->tag_spec);
 	}
 	else if (strcmp(token, "pad-id3v2")==0) {
 	  id3tag_pad_v2(&gfp->tag_spec);
 	}
 	else if (strcmp(token, "genre-list")==0) {
 	  id3tag_genre_list(genre_list_handler, NULL);
	  LAME_NORMAL_EXIT();
 	}
	else if (strcmp(token, "lowpass")==0) {
	  double freq = atof( nextArg );
	  argUsed=1;
	  /* useful are 0.001 kHz...50 kHz, 50 Hz...50000 Hz */
	  gfp -> lowpassfreq = freq * (freq < 50. ? 1.e3 : 1.e0 ) + 0.5;
	  if ( freq < 0.001 || freq > 50000. ) {
	    ERRORF("Must specify lowpass with --lowpass freq, freq >= 0.001 kHz\n");
	    LAME_ERROR_EXIT();
	  }
	}
	else if (strcmp(token, "lowpass-width")==0) {
	  argUsed=1;
	  gfp->lowpasswidth =  (( 1000.0 * atof( nextArg ) ) + 0.5);
	  if (gfp->lowpasswidth  < 0) {
	    ERRORF("Must specify lowpass width with --lowpass-width freq, freq >= 0 kHz\n");
	    LAME_ERROR_EXIT();
	  }
	}
	else if (strcmp(token, "highpass")==0) {
	  double freq = atof( nextArg );
	  argUsed=1;
	  /* useful are 0.001 kHz...16 kHz, 16 Hz...50000 Hz */
	  gfp->highpassfreq =  freq * (freq < 16. ? 1.e3 : 1.e0 ) + 0.5;
	  if ( freq < 0.001 || freq > 50000. ) {
	    ERRORF("Must specify highpass with --highpass freq, freq >= 0.001 kHz\n");
	    LAME_ERROR_EXIT();
	  }
	}
	else if (strcmp(token, "highpass-width")==0) {
	  argUsed=1;
	  gfp->highpasswidth =  (( 1000.0 * atof( nextArg ) ) + 0.5);
	  if (gfp->highpasswidth  < 0) {
	    ERRORF("Must specify highpass width with --highpass-width freq, freq >= 0 kHz\n");
	    LAME_ERROR_EXIT();
	  }
	}
	else if (strcmp(token, "cwlimit")==0) {
          double freq = atof (nextArg);
          argUsed=1;
          /* useful are 0.001 kHz...50 kHz, 50 Hz...50000 Hz */
          /* Attention: This is stored as Kilohertz, not as Hertz (why?) */
          gfp -> cwlimit = freq * ( freq <= 50. ? 1.e0 : 1.e-3 );
          if (gfp->cwlimit <= 0 ) {
            ERRORF("Must specify cwlimit in kHz\n");
            LAME_ERROR_EXIT();
          }
	} 
	else if (strcmp(token, "comp")==0) {
	  argUsed=1;
	  gfp->compression_ratio =  atof( nextArg );
	  if (gfp->compression_ratio < 1.0 ) {
	    ERRORF("Must specify compression ratio >= 1.0\n");
	    LAME_ERROR_EXIT();
	  }
	}
	else if (strcmp(token, "nspsytune")==0) {
	  gfp->exp_nspsytune = TRUE;
	  gfp->experimentalZ = TRUE;
	}
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
	else if (strcmp(token, "version") ==0
	       ||strcmp(token, "license")==0){
	  lame_print_license(gfp,programName);  /* doesn't return */
	}
	else if (strcmp(token, "help") ==0
	       ||strcmp(token, "usage")==0){
	  lame_short_help(gfp,programName);  /* doesn't return */
	}
	else if (strcmp(token, "longhelp") ==0){
	  lame_help(gfp,programName);  /* doesn't return */
	}
	else if (strcmp(token, "preset")==0) {
	  argUsed=1;
	  if (strcmp(nextArg,"phone")==0)
	  { /* when making changes, please update help text too */
	    gfp->out_samplerate =  8000;
	    gfp->lowpassfreq=3200;
	    gfp->lowpasswidth=1000;
	    gfp->no_short_blocks=1;
	    gfp->quality = 5;
	    gfp->mode = MPG_MD_MONO; 
	    gfp->mode_fixed = 1; 
	    gfp->brate = 16; 
	    gfp->VBR_q=6;
	    gfp->VBR_min_bitrate_kbps=8;
	    gfp->VBR_max_bitrate_kbps=56;
	  }
	  else if (strcmp(nextArg,"sw")==0)
	  { /* when making changes, please update help text too */
	    gfp->out_samplerate =  11025;
	    gfp->lowpassfreq=4800;
	    gfp->lowpasswidth=500;
	    gfp->quality = 5;
	    gfp->mode = MPG_MD_MONO; 
	    gfp->mode_fixed = 1; 
	    gfp->brate = 24; 
	    gfp->VBR_q=5;
	    gfp->VBR_min_bitrate_kbps=8;
	    gfp->VBR_max_bitrate_kbps=64;
	  }
	  else if (strcmp(nextArg,"am")==0)
	  { /* when making changes, please update help text too */
	    gfp->out_samplerate =  16000;
	    gfp->lowpassfreq=7200;
	    gfp->lowpasswidth=500;
	    gfp->quality = 5;
	    gfp->mode = MPG_MD_MONO; 
	    gfp->mode_fixed = 1; 
	    gfp->brate = 32; 
	    gfp->VBR_q=5;
	    gfp->VBR_min_bitrate_kbps=16;
	    gfp->VBR_max_bitrate_kbps=128;
	  }
	  else if (strcmp(nextArg,"fm")==0)
	  { /* when making changes, please update help text too */
	    gfp->out_samplerate =  22050; 
            gfp->lowpassfreq=9950;
            gfp->lowpasswidth=880;
	    gfp->quality = 5;
	    gfp->mode = MPG_MD_JOINT_STEREO; 
	    gfp->mode_fixed = 1; 
	    gfp->brate = 64; 
	    gfp->VBR_q=5;
	    gfp->VBR_min_bitrate_kbps=24;
	    gfp->VBR_max_bitrate_kbps=160;
	  }
	  else if (strcmp(nextArg,"voice")==0)
	  { /* when making changes, please update help text too */
	    gfp->out_samplerate =  32000; 
	    gfp->lowpassfreq=12300;
	    gfp->lowpasswidth=2000;
	    gfp->no_short_blocks=1;
	    gfp->quality = 5;
	    gfp->mode = MPG_MD_MONO; 
	    gfp->mode_fixed = 1; 
	    gfp->brate = 56; 
	    gfp->VBR_q=4;
	    gfp->VBR_min_bitrate_kbps=32;
	    gfp->VBR_max_bitrate_kbps=128;
	  }
	  else if (strcmp(nextArg,"radio")==0)
	  { /* when making changes, please update help text too */
            gfp->lowpassfreq=15000;
            gfp->lowpasswidth=0;
	    gfp->quality = 5;
	    gfp->mode = MPG_MD_JOINT_STEREO; 
	    gfp->mode_fixed = 1; 
	    gfp->brate = 112; 
	    gfp->VBR_q=4;
	    gfp->VBR_min_bitrate_kbps=64;
	    gfp->VBR_max_bitrate_kbps=256;
	  }
	  else if (strcmp(nextArg,"tape")==0)
	  { /* when making changes, please update help text too */
            gfp->lowpassfreq=18500;
            gfp->lowpasswidth=2000;
	    gfp->quality = 5;
	    gfp->mode = MPG_MD_JOINT_STEREO; 
	    gfp->mode_fixed = 1; 
	    gfp->brate = 128; 
	    gfp->VBR_q=4;
	    gfp->VBR_min_bitrate_kbps=96;
	    gfp->VBR_max_bitrate_kbps=320;
	  }
	  else if (strcmp(nextArg,"hifi")==0)
	  { /* when making changes, please update help text too */
            gfp->lowpassfreq=20240;
            gfp->lowpasswidth=2200;
	    gfp->quality = 2;
	    gfp->mode = MPG_MD_JOINT_STEREO; 
	    gfp->mode_fixed = 1; 
	    gfp->brate = 160;            
	    gfp->VBR_q=3;
	    gfp->VBR_min_bitrate_kbps=112;
	    gfp->VBR_max_bitrate_kbps=320;
	  }
	  else if (strcmp(nextArg,"cd")==0)
	  { /* when making changes, please update help text too */
	    gfp->lowpassfreq=-1;
            gfp->highpassfreq=-1;
	    gfp->quality = 2;
	    gfp->mode = MPG_MD_STEREO; 
	    gfp->mode_fixed = 1; 
	    gfp->brate = 192;  
	    gfp->VBR_q=2;
	    gfp->VBR_min_bitrate_kbps=128;
	    gfp->VBR_max_bitrate_kbps=320;
	  }
	  else if (strcmp(nextArg,"studio")==0)
	  { /* when making changes, please update help text too */
	    gfp->lowpassfreq=-1;
            gfp->highpassfreq=-1;
	    gfp->quality = 2; 
	    gfp->mode = MPG_MD_STEREO; 
	    gfp->mode_fixed = 1; 
	    gfp->brate = 256; 
	    gfp->VBR_q=0;
	    gfp->VBR_min_bitrate_kbps=160;
	    gfp->VBR_max_bitrate_kbps=320;
	  }
	  else if (strcmp(nextArg,"help")==0)
	  {
	    lame_presets_info(gfp,programName);  /* doesn't return */
	  }
	  else
	    {
	      ERRORF("%s: --preset type, type must be phone, voice, fm, tape, hifi, cd or studio, not %s\n",
		      programName, nextArg);
	      LAME_ERROR_EXIT();
	    }
	} /* --preset */
	else
	  {
	    ERRORF("%s: unrec option --%s\n",
		    programName, token);
	  }
	i += argUsed;
	
      } else  while( (c = *token++) ) {
	if(*token ) arg = token;
	else                             arg = nextArg;
	switch(c) {
	case 'm':        argUsed = 1;   gfp->mode_fixed = 1;
	  if (*arg == 's')
	    { gfp->mode = MPG_MD_STEREO; }
	  else if (*arg == 'd')
	    { gfp->mode = MPG_MD_DUAL_CHANNEL; }
	  else if (*arg == 'j')
	    { gfp->mode = MPG_MD_JOINT_STEREO; }
	  else if (*arg == 'f')
	    { gfp->mode = MPG_MD_JOINT_STEREO; gfp->force_ms=1; }
	  else if (*arg == 'm')
	    { gfp->mode = MPG_MD_MONO; }
	  else {
	    ERRORF("%s: -m mode must be s/d/j/f/m not %s\n",
		    programName, arg);
	    err = 1;
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
	  srate = atof( arg );
	  /* samplerate = rint( 1000.0 * srate ); $A  */
	  gfp->in_samplerate =  (( 1000.0 * srate ) + 0.5);
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
	case 'X':        argUsed = 1;   
	  gfp->experimentalX=atoi(arg); 
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

	case 'e':        argUsed = 1;
	  if (*arg == 'n')                    gfp->emphasis = 0;
	  else if (*arg == '5')               gfp->emphasis = 1;
	  else if (*arg == 'c')               gfp->emphasis = 3;
	  else {
	    ERRORF("%s: -e emp must be n/5/c not %s\n",
		    programName, arg);
	    err = 1;
	  }
	  break;
	case 'c':   gfp->copyright = 1; break;
	case 'o':   gfp->original  = 0; break;
	
	case '?':   lame_help(gfp,programName);  /* doesn't return */
	default:    ERRORF("%s: unrec option %c\n",
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
      if(gfp->inPath[0] == '\0')       strncpy(gfp->inPath, argv[i], MAX_NAME_SIZE);
      else if(gfp->outPath[0] == '\0') strncpy(gfp->outPath, argv[i], MAX_NAME_SIZE);
      else {
	ERRORF("%s: excess arg %s\n", programName, argv[i]);
	err = 1;
      }
    }
  }  /* loop over args */

  if(err || gfp->inPath[0] == '\0') lame_usage(gfp,programName);  /* never returns */
  if (gfp->inPath[0]=='-') gfp->silent=1;  /* turn off status - it's broken for stdin */
  if(gfp->outPath[0] == '\0') {
    if (gfp->inPath[0]=='-') {
      /* if input is stdin, default output is stdout */
      strcpy(gfp->outPath,"-");
    }else {
      strncpy(gfp->outPath, gfp->inPath, MAX_NAME_SIZE - 4);
      if (gfp->decode_only) {
	strncat(gfp->outPath, ".wav", 4 );
      } else if (gfp->ogg) {
	strncat(gfp->outPath, ".ogg", 4 );
      }else{
	strncat(gfp->outPath, ".mp3", 4 );
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
  if (gfp->input_format != sf_mp1 || 
      gfp->input_format != sf_mp2 ||
      gfp->input_format != sf_mp3 ||
      gfp->input_format != sf_ogg) {
    if (!(strcmp((char *) &gfp->inPath[strlen(gfp->inPath)-4],".mpg")))
      gfp->input_format = sf_mp1;
    else if (!(strcmp((char *) &gfp->inPath[strlen(gfp->inPath)-4],".mp1")))
      gfp->input_format = sf_mp1;
    else if (!(strcmp((char *) &gfp->inPath[strlen(gfp->inPath)-4],".mp2")))
      gfp->input_format = sf_mp2;
    else if (!(strcmp((char *) &gfp->inPath[strlen(gfp->inPath)-4],".mp3")))
      gfp->input_format = sf_mp3;
    else if (!(strcmp((char *) &gfp->inPath[strlen(gfp->inPath)-4],".ogg")))
      gfp->input_format = sf_ogg;
  }
  
#if !(defined HAVEMPGLIB || defined AMIGA_MPEGA)
  if (gfp->input_format == sf_mp1 ||
      gfp->input_format == sf_mp2 ||
      gfp->input_format == sf_mp3) {
    ERRORF("Error: libmp3lame not compiled with mp3 *decoding* support \n");
    LAME_ERROR_EXIT();
  }
#endif
  /* default guess for number of channels */
  if (autoconvert) gfp->num_channels=2; 
  else if (gfp->mode == MPG_MD_MONO) gfp->num_channels=1;
  else gfp->num_channels=2;

  /* user specified a quality value.  override any defaults set above */
  if (user_quality>=0)   gfp->quality=user_quality;

  if (gfp->free_format) {
    if (gfp->brate<8) {
      ERRORF("For free format, specify a bitrate between 8 and 320kbs\n");
      LAME_ERROR_EXIT();
    }
  }

}



