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

/* $Id$ */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>


/*
 main.c is example code for how to use libmp3lame.a.  To use this library,
 you only need the library and lame.h.  All other .h files are private
 to the library.
*/
#include "lame.h"

#include "brhist.h"
#include "parse.h"
#include "main.h"
#include "get_audio.h"
#include "timestatus.h"

/* PLL 14/04/2000 */
#if macintosh
#include <console.h>
#endif

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif


/* GLOBAL VARIABLES.  set by parse_args() */
/* we need to clean this up */
sound_file_format input_format;   
int swapbytes;              /* force byte swapping   default=0*/
int silent;
int brhist;
float update_interval;      /* to use Frank's time status display */




/************************************************************************
*
* main
*
* PURPOSE:  MPEG-1,2 Layer III encoder with GPSYCHO
* psychoacoustic model.
*
************************************************************************/

int  parse_args_from_string ( lame_global_flags* const gfp, const char* p, char* inPath, char* outPath )
{   /* Quick & very Dirty */
    char*  q;
    char*  f;
    char*  r [128];
    int    c = 0;
    int    ret;
    
    if ( p == NULL || *p == '\0' )
        return 0;
	
    f = q = malloc ( strlen(p) + 1 );
    strcpy ( q, p );
    
    r[c++] = "lhama";
    while (1) {
        r [c++] = q;
	while (*q != ' ' && *q != '\0') q++;
	if (*q == '\0') break;
	*q++='\0';
    }
    r[c] = NULL;
    
    ret = parse_args ( gfp, c, r, inPath, outPath );
    free (f);
    return ret;
}



int main(int argc, char **argv)
{
    static const char *mode_names [2] [4] = { 
        { "stereo", "j-stereo", "dual-ch", "single-ch" },
        { "stereo", "force-ms", "dual-ch", "single-ch" }
    };
    int  ret;
    unsigned char  mp3buffer [LAME_MAXMP3BUFFER];
  short int Buffer[2][1152];
  int iread,imp3;
  lame_global_flags *gf;
  FILE *outf;
  char outPath[MAX_NAME_SIZE];
  char inPath[MAX_NAME_SIZE];
  int i;

#if macintosh
  argc = ccommand(&argv);
#endif

  /* initialize libmp3lame */
  input_format=sf_unknown;
  if (NULL== (gf=lame_init()) ) {
    fprintf(stderr,"fatal error during initialization\n");
    return 1;
  }
  if (argc <= 1) {
      usage ( gf, stderr, argv[0] );  /* no command-line args, print usage, exit  */
      return 1;
  }

  /* parse the command line arguments, setting various flags in the
   * struct 'gf'.  If you want to parse your own arguments,
   * or call libmp3lame from a program which uses a GUI to set arguments,
   * skip this call and set the values of interest in the gf struct.
   * (see the file API and lame.h for documentation about these parameters)
   */
    parse_args_from_string ( gf, getenv("LAMEOPT"), inPath, outPath );
    ret = parse_args ( gf, argc, argv, inPath, outPath );
    if ( ret < 0 )
        return ret == -2 ? 0 : 1;
  if ( update_interval < 0. )
    update_interval = 2.;


  /* Mostly it is not useful to use the same input and output name.
     This test is very easy and buggy and don't recognize different names
     assigning the same file
   */
  if ( 0 != strcmp ( "-"   , outPath )  && 
       0 == strcmp ( inPath, outPath ) ) {
      fprintf(stderr,"Input file and Output file are the same. Abort.\n" );
      return 1;
  }

  /* open the wav/aiff/raw pcm or mp3 input file.  This call will
   * open the file, try to parse the headers and
   * set gf.samplerate, gf.num_channels, gf.num_samples.
   * if you want to do your own file input, skip this call and set
   * samplerate, num_channels and num_samples yourself.
   */
  init_infile ( gf, inPath );
  if ((outf = init_outfile (outPath, gf->decode_only)) == NULL ) {
      fprintf (stderr, "Can't init outfile '%s'\n", outPath);
      return -42;
  }

  /* Now that all the options are set, lame needs to analyze them and
   * set some more internal options and check for problems
   */
  i = lame_init_params(gf);
  if (i<0)  {
    if (i == -1) {
      display_bitrates(stderr);
    }
    fprintf(stderr,"fatal error during initialization\n");
    return 1;
  }

  if (silent || gf->VBR==vbr_off) {
    brhist = 0;  /* turn off VBR histogram */
  }

#ifdef BRHIST
  if (brhist) {
    if (brhist_init(gf, gf->VBR_min_bitrate_kbps,gf->VBR_max_bitrate_kbps)) {
      /* fail to initialize */
      brhist = 0;
    }
  }
  else {
    brhist_init(gf, 128, 128 );   // Dirty hack
  }
#endif


#ifdef HAVE_VORBIS
  if (gf->ogg) {
    lame_encode_ogg_init(gf);
    gf->VBR=vbr_off;            /* ignore lame's various VBR modes */
  }
#endif
  if (!gf->decode_only) {
    lame_print_config(gf);   /* print useful information about options being used */

    fprintf ( stderr, "Encoding %s%s to %s\n",
	      strcmp(inPath, "-") ? inPath  : "<stdin>",
	      strlen (inPath)+strlen(outPath) < 66 ? "" : "\n     ",
	      strcmp(outPath,"-") ? outPath : "<stdout>" );
	    
    fprintf ( stderr, "Encoding as %g kHz ", 1.e-3 * gf->out_samplerate );
    
    if ( gf->ogg ) {
        fprintf ( stderr, "VBR Ogg Vorbis\n" );
    } else { 
        const char* appendix = "";
      
        switch ( gf->VBR ) {
        case vbr_mt:
        case vbr_rh:
        case vbr_mtrh:
            appendix = "ca. ";
	    fprintf ( stderr, "VBR(q=%i)", gf->VBR_q );
	    break;
        case vbr_abr:
	    fprintf ( stderr, "average %d kbps", gf->VBR_mean_bitrate_kbps );
            break;
        default:
	    fprintf ( stderr, "%3d kbps", gf->brate );
	    break;
        }
        fprintf ( stderr, " %s MPEG-%u%s Layer III (%s%gx) qval=%i\n", 
                  mode_names [gf->force_ms] [gf->mode],
	  	  2 - gf->version, 
		  gf->out_samplerate < 16000 ? ".5" : "",
                  appendix, 
		  0.1 * (int)(10.*gf->compression_ratio + 0.5), 
		  gf->quality );
    }
  }



  if (gf->decode_only) {

    /* decode an mp3 file to a .wav */
    lame_decoder(gf,outf,gf->encoder_delay,inPath,outPath);

  } else {

      /* encode until we hit eof */
      do {
	/* read in 'iread' samples */
	iread = get_audio(gf,Buffer);


	/********************** status display  *****************************/
	if (!silent) {
	  if (update_interval>0) {
	    timestatus_klemm(gf);
	  }else{
	    if (0==gf->frameNum % 50) {
#ifdef BRHIST
          brhist_jump_back();
#endif
	      timestatus(gf->out_samplerate,gf->frameNum,gf->totalframes,gf->framesize);
#ifdef BRHIST
	      if (brhist)
		brhist_disp(gf);
#endif
	    }
	  }
	}

	/* encode */
	imp3=lame_encode_buffer(gf,Buffer[0],Buffer[1],iread,
              mp3buffer,sizeof(mp3buffer));

	/* was our output buffer big enough? */
	if (imp3<0) {
	  if (imp3==-1) fprintf(stderr,"mp3 buffer is not big enough... \n");
	  else fprintf(stderr,"mp3 internal error:  error code=%i\n",imp3);
	  return 1;
	}

	if (fwrite(mp3buffer,1,imp3,outf) != imp3) {
	  fprintf(stderr,"Error writing mp3 output \n");
	  return 1;
	}
      } while (iread);
      imp3=lame_encode_flush(gf,mp3buffer,sizeof(mp3buffer));   /* may return one more mp3 frame */
      if (imp3<0) {
	if (imp3==-1) fprintf(stderr,"mp3 buffer is not big enough... \n");
	else fprintf(stderr,"mp3 internal error:  error code=%i\n",imp3);
	return 1;

      }

      if (!silent) {
#ifdef BRHIST
	brhist_jump_back();
#endif
	timestatus(gf->out_samplerate,gf->frameNum,gf->totalframes,gf->framesize);
#ifdef BRHIST
	if (brhist) {
	  brhist_disp(gf);
	}
	brhist_disp_total(gf);
#endif
	timestatus_finish();
      }

      fwrite(mp3buffer,1,imp3,outf);
      lame_mp3_tags_fid(gf,outf);       /* add VBR tags to mp3 file */
      lame_close(gf);
      fclose(outf);
    }
  close_infile();            /* close the input file */
  return 0;
}
