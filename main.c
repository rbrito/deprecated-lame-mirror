#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#ifdef _WIN32
/* needed to set stdout to binary */
#include <io.h>
#endif
#include "lame.h"
#include "util.h"

#ifdef HAVEGTK
#include "gtkanal.h"
#include <gtk/gtk.h>
#endif

#ifdef __riscos__
#include "asmstuff.h"
#endif




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

  char mp3buffer[LAME_MAXMP3BUFFER];
  short int Buffer[2][1152];
  int iread,imp3;
  lame_global_flags gf;
  FILE *outf;
#ifdef __riscos__
  int i;
#endif


  if (lame_init(&gf)<0) LAME_ERROR_EXIT();       /* initialize libmp3lame */
  if(argc==1) lame_usage(&gf,argv[0]);  /* no command-line args, print usage, exit  */

  /* parse the command line arguments, setting various flags in the
   * struct 'gf'.  If you want to parse your own arguments,
   * or call libmp3lame from a program which uses a GUI to set arguments,
   * skip this call and set the values of interest in the gf struct.
   * (see lame.h for documentation about these parameters)
   */
#ifdef ONLYVORBIS
  gf.ogg=1;
#endif
  lame_parse_args(&gf,argc, argv);



  /* open the wav/aiff/raw pcm or mp3 input file.  This call will
   * open the file with name gf.inFile, try to parse the headers and
   * set gf.samplerate, gf.num_channels, gf.num_samples.
   * if you want to do your own file input, skip this call and set
   * these values yourself.
   */
  lame_init_infile(&gf);

  /* Now that all the options are set, lame needs to analyze them and
   * set some more options
   */
  if (lame_init_params(&gf)<0)  LAME_ERROR_EXIT();

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
      if ((outf = fopen(gf.outPath, "wb")) == NULL) {
	ERRORF("Could not create \"%s\".\n", gf.outPath);
	LAME_ERROR_EXIT();
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
    ERRORF("Error: lame not compiled with GTK support \n");
#endif

  } else if (gf.decode_only) {

    /* decode an mp3 file to a .wav */
    lame_decoder(&gf,outf,gf.encoder_delay);

  } else {

      /* encode until we hit eof */
      do {
	/* read in 'iread' samples */
	iread=lame_readframe(&gf,Buffer);


	/* encode */
	imp3=lame_encode_buffer(&gf,Buffer[0],Buffer[1],iread,
              mp3buffer,(int)sizeof(mp3buffer));

	/* was our output buffer big enough? */
	if (imp3<0) {
	  if (imp3==-1) ERRORF("mp3 buffer is not big enough... \n");
	  else ERRORF("mp3 internal error:  error code=%i\n",imp3);
	  LAME_ERROR_EXIT();
	}

	/* imp3 is not negative, but fwrite needs an unsigned here */
	if (fwrite(mp3buffer,1,(unsigned int)imp3,outf) != imp3) {
	  ERRORF("Error writing mp3 output \n");
	  LAME_ERROR_EXIT();
	}
      } while (iread);
      imp3=lame_encode_finish(&gf,mp3buffer,(int)sizeof(mp3buffer));   /* may return one more mp3 frame */
      if (imp3<0) {
	if (imp3==-1) ERRORF("mp3 buffer is not big enough... \n");
	else ERRORF("mp3 internal error:  error code=%i\n",imp3);
	LAME_ERROR_EXIT();
      }
      /* imp3 is not negative, but fwrite needs an unsigned here */
      fwrite(mp3buffer,1,(unsigned int)imp3,outf);
      fclose(outf);
      lame_mp3_tags(&gf);                /* add id3 or VBR tags to mp3 file */
    }
  lame_close_infile(&gf);            /* close the input file */
  return 0;
}



