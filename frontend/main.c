/*
 *	Command line frontend program
 *
 *	Copyright (c) 1999 Mark Taylor
 *                    2000 Takehiro TOMIANGA
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>


#ifdef _WIN32
/* needed to set stdout to binary */
#include <io.h>
#endif
/*
 main.c is example code for how to use libmp3lame.a.  To use this library,
 you only need the library and lame.h.  All other .h files are private
 to the library.
*/
#include "lame.h"
#include "lame-id3tag.h"

#ifdef HAVEGTK
#include "lame-analysis.h"
#include <gtk/gtk.h>
#endif


#ifdef __riscos__
#include "asmstuff.h"
#endif

#include "brhist.h"
#include "parse.h"
#include "main.h"
#include "get_audio.h"
#include "timestatus.h"

/* PLL 14/04/2000 */
#if macintosh
#include <console.h>
#endif


/* I/O - not used if calling program does the i/o */
char inPath[MAX_NAME_SIZE];
FILE * musicin;             /* file pointer to input file */
sound_file_format input_format;   
int swapbytes;              /* force byte swapping   default=0*/
int totalframes;                /* frames: 0..totalframes-1 (estimate)*/
int frameNum;                   /* frame counter */

/************************************************************************
*
* main
*
* PURPOSE:  MPEG-1,2 Layer III encoder with GPSYCHO
* psychoacoustic model.
*
************************************************************************/


int main(int argc, char **argv)
{
  static const char *mode_names[4] = { "stereo", "j-stereo", "dual-ch", "single-ch" };

  char mp3buffer[LAME_MAXMP3BUFFER];
  short int Buffer[2][1152];
  int iread,imp3;
  lame_global_flags gf;
  FILE *outf;
  int i;

#if macintosh
  argc = ccommand(&argv);
#endif

  extern void main_CRC_init (void);

  main_CRC_init ();  /* ulgy, it's C, not Ada, C++ or Modula */
  
  /* initialize libmp3lame */
  input_format=sf_unknown;
  if (lame_init(&gf)<0) {
    fprintf(stderr,"fatal error during initialization\n");
    exit(-1);
  }
  if(argc==1)
    usage(&gf,argv[0]);  /* no command-line args, print usage, exit  */

  /* parse the command line arguments, setting various flags in the
   * struct 'gf'.  If you want to parse your own arguments,
   * or call libmp3lame from a program which uses a GUI to set arguments,
   * skip this call and set the values of interest in the gf struct.
   * (see lame.h for documentation about these parameters)
   */
  {
    parse_args(&gf,argc, argv);
  }

  /* Mostly it is not useful to use the same input and output name.
     This test is very easy and buggy and don't recognize different names
     assigning the same file
   */
  if ( 0 != strcmp ( "-"      , gf.outPath )  && 
       0 == strcmp ( inPath, gf.outPath ) ) {
      fprintf(stderr,"Input file and Output file are the same. Abort.\n" );
      exit(-1);
  }

  /* open the wav/aiff/raw pcm or mp3 input file.  This call will
   * open the file with name gf.inFile, try to parse the headers and
   * set gf.samplerate, gf.num_channels, gf.num_samples.
   * if you want to do your own file input, skip this call and set
   * samplerate, num_channels and num_samples yourself.
   */
  
  init_infile(&gf);

  /* Now that all the options are set, lame needs to analyze them and
   * set some more internal options and check for problems
   */
  i = lame_init_params(&gf);

  /* estimate total frames.  must be done after setting sampling rate so
   * we know the framesize.  */
  {
    double resample_ratio=1;
    if (gf.out_samplerate != gf.in_samplerate) 
      resample_ratio = (double)gf.in_samplerate / (double)gf.out_samplerate;

    totalframes=0;
    if(input_format == sf_mp1 && gf.decode_only)
      totalframes = (gf.num_samples+383)/384;
    else
      if(input_format == sf_mp2 && gf.decode_only)
	totalframes = (gf.num_samples+1151)/1152;
      else
	totalframes = 2+ gf.num_samples/(resample_ratio * gf.framesize);
  }

  if (i<0)  {
    if (i == -1) {
      display_bitrates(stderr);
    }
    fprintf(stderr,"fatal error during initialization\n");
    exit(-1);
  }

#ifdef BRHIST
  if (gf.silent || gf.VBR==vbr_off) {
    gf.brhist_disp=0;  /* turn of VBR historgram */
  }

  if (gf.brhist_disp) {
    if (brhist_init(gf.VBR_min_bitrate_kbps,gf.VBR_max_bitrate_kbps)) {
      /* fall to initialize */
      gf.brhist_disp = 0;
    }
  }
#endif

#ifdef HAVEVORBIS
  if (gf.ogg) {
    lame_encode_ogg_init(&gf);
    //    gfc->filter_type = -1;   /* vorbis claims not to need filters */
    gf.VBR=vbr_off;            /* ignore lame's various VBR modes */
  }
#endif

  if (gf.gtkflag) {
    fprintf(stderr,"Analyzing %s \n",inPath);
  }
  else {
    fprintf(stderr,"Encoding %s to %s\n",
	    (strcmp(inPath, "-")? inPath : "<stdin>"),
	    (strcmp(gf.outPath, "-")? gf.outPath : "<stdout>"));
    if (gf.ogg) {
      fprintf(stderr,"Encoding as %.1f kHz VBR Ogg Vorbis \n",
	      gf.out_samplerate/1000.0);
    }else
    if (gf.VBR==vbr_mt || gf.VBR==vbr_rh || gf.VBR==vbr_mtrh)
      fprintf(stderr,"Encoding as %.1f kHz VBR(q=%i) %s MPEG-%g LayerIII (%4.1fx estimated) qval=%i\n",
	      gf.out_samplerate/1000.0,
	      gf.VBR_q,mode_names[gf.mode],
              2-gf.version+0.5*(gf.out_samplerate<16000),
              gf.compression_ratio, gf.quality);
    else
    if (gf.VBR==vbr_abr)
      fprintf(stderr,"Encoding as %.1f kHz average %d kbps %s MPEG-%g LayerIII (%4.1fx) qval=%i\n",
	      gf.out_samplerate/1000.0,
	      gf.VBR_mean_bitrate_kbps,mode_names[gf.mode],
              2-gf.version+0.5*(gf.out_samplerate<16000),
              gf.compression_ratio,gf.quality);
    else {
      fprintf(stderr,"Encoding as %.1f kHz %d kbps %s MPEG-%g LayerIII (%4.1fx)  qval=%i\n",
	      gf.out_samplerate/1000.0,gf.brate,
	      mode_names[gf.mode],
              2-gf.version+0.5*(gf.out_samplerate<16000),
              gf.compression_ratio,gf.quality);
    }
  }

  if (!gf.decode_only)
    lame_print_config(&gf);   /* print usefull information about options being used */


  if (!gf.gtkflag) {
    /* open the output file */
    if (!strcmp(gf.outPath, "-")) {
#ifdef __EMX__
      _fsetmode(stdout,"b");
#elif (defined  __BORLANDC__)
      setmode(_fileno(stdout), O_BINARY);
#elif (defined  __CYGWIN__)
      setmode(fileno(stdout), _O_BINARY);
#elif (defined _WIN32)
      _setmode(_fileno(stdout), _O_BINARY);
#endif
      outf = stdout;
    } else {
      if ((outf = fopen(gf.outPath, "wb+")) == NULL) {
	fprintf(stderr,"Could not create \"%s\".\n", gf.outPath);
	exit(-1);
      }
    }
#ifdef __riscos__
    /* Assign correct file type */
    for (i = 0; gf.outPath[i]; i++) {
      if (gf.outPath[i] == '.')
        gf.outPath[i] = '/';
      else if (gf.outPath[i] == '/')
        gf.outPath[i] = '.';
    }
    if (gf.decode_only)
      SetFiletype(gf.outPath, 0xfb1); /* Wave */
    else
      SetFiletype(gf.outPath, 0x1ad); /* AMPEG */
#endif
  }


  if (gf.gtkflag) {

#ifdef HAVEGTK
    gtk_init (&argc, &argv);
    gtkcontrol(&gf);
#else
    fprintf(stderr,"Error: lame not compiled with GTK support \n");
#endif

  } else if (gf.decode_only) {

    /* decode an mp3 file to a .wav */
    lame_decoder(&gf,outf,gf.encoder_delay);

  } else {

      /* encode until we hit eof */
      do {
	/* read in 'iread' samples */
	iread = readframe(&gf,Buffer);


	/********************** status display  *****************************/
	if (!gf.silent) {
	  if (gf.update_interval>0) {
	    timestatus_klemm(&gf);
	  }else{
	    int mod = gf.version == 0 ? 100 : 50;
	    if (frameNum % mod==0) {
	      timestatus(gf.out_samplerate,frameNum,totalframes,gf.framesize);
#ifdef BRHIST
	      if (gf.brhist_disp)
		brhist_disp(totalframes);
#endif
	    }
	  }
	}

	/* encode */
	imp3=lame_encode_buffer(&gf,Buffer[0],Buffer[1],iread,
              mp3buffer,(int)sizeof(mp3buffer));
	frameNum++;

#ifdef BRHIST
	/* update VBR histogram data */
	if (gf.brhist_disp)
	  brhist_update(imp3*8);
#endif
	/* was our output buffer big enough? */
	if (imp3<0) {
	  if (imp3==-1) fprintf(stderr,"mp3 buffer is not big enough... \n");
	  else fprintf(stderr,"mp3 internal error:  error code=%i\n",imp3);
	  exit(-1);
	}

	/* imp3 is not negative, but fwrite needs an unsigned here */
	if (fwrite(mp3buffer,1,(unsigned int)imp3,outf) != (size_t)imp3) {
	  fprintf(stderr,"Error writing mp3 output \n");
	  exit(-1);
	}
      } while (iread);
      imp3=lame_encode_finish(&gf,mp3buffer,(int)sizeof(mp3buffer));   /* may return one more mp3 frame */
      if (imp3<0) {
	if (imp3==-1) fprintf(stderr,"mp3 buffer is not big enough... \n");
	else fprintf(stderr,"mp3 internal error:  error code=%i\n",imp3);
	exit(-1);
      }

      if (!gf.silent) {
	timestatus(gf.out_samplerate,frameNum,totalframes,gf.framesize);
#ifdef BRHIST
	if (gf.brhist_disp) {
	  brhist_update(imp3);
	  brhist_disp(totalframes);
	  brhist_disp_total(totalframes);
	}
#endif
	timestatus_finish();
      }

      /* imp3 is not negative, but fwrite needs an unsigned here */
      fwrite(mp3buffer,1,(unsigned int)imp3,outf);
      lame_mp3_tags_fid(&gf,outf);       /* add ID3 version 1 or VBR tags to mp3 file */
      fclose(outf);
    }
  close_infile(&gf);            /* close the input file */
  return 0;
}
