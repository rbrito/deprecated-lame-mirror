#include "lame.h"

#ifdef HAVEGTK
#include "gtkanal.h"
#include <gtk/gtk.h>
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
  lame_global_flags *gf;
  FILE *outf;



  gf=lame_init();                  /* initialize libmp3lame */
  if(argc==1) lame_usage(argv[0]);  /* no command-line args, print usage, exit  */

  /* parse the command line arguments, setting various flags in the
   * struct pointed to by 'gf'.  If you want to parse your own arguments,
   * or call libmp3lame from a program which uses a GUI to set arguments,
   * skip this call and set the values of interest in the gf-> struct.  
   * (see lame.h for documentation about these parameters)
   */
  lame_parse_args(argc, argv);


  /* open the MP3 output file */
  if (!strcmp(gf->outPath, "-")) {
    outf = stdout;
  } else {
    if ((outf = fopen(gf->outPath, "wb")) == NULL) {
      fprintf(stderr,"Could not create \"%s\".\n", gf->outPath);
      exit(1);
    }
  }

  /* open the wav/aiff/raw pcm or mp3 input file.  This call will
   * open the file with name gf->inFile, try to parse the headers and
   * set gf->samplerate, gf->num_channels, gf->num_samples.
   * if you want to do your own file input, skip this call and set
   * these values yourself.  
   */
  lame_init_infile();

  /* Now that all the options are set, lame needs to analyze them and
   * set some more options 
   */
  lame_init_params();
  lame_print_config();   /* print usefull information about options being used */




#ifdef HAVEGTK
  if (gf->gtkflag) gtk_init (&argc, &argv);
  if (gf->gtkflag) gtkcontrol();
  else 
#endif
    {
      int samples_to_encode = gf->encoder_delay + 288;

      /* encode until we hit eof */
      do {
	/* read in gf->framesize samples.  If you are doing your own file input
	 * replace this by a call to fill Buffer with exactly gf->framesize sampels */
	iread=lame_readframe(Buffer);  
	imp3=lame_encode(Buffer,mp3buffer);  /* encode the frame */
	fwrite(mp3buffer,1,imp3,outf);       /* write the MP3 output  */
	samples_to_encode += iread - gf->framesize;
      } while (iread);

      /* encode until we flush internal buffers.  (Buffer=0 at this point */
      while (samples_to_encode > 0) {
	imp3=lame_encode(Buffer,mp3buffer);
	fwrite(mp3buffer,1,imp3,outf);
	samples_to_encode -= gf->framesize;
      }
    }
  
  imp3=lame_encode_finish(mp3buffer);   /* may return one more mp3 frame */
  fwrite(mp3buffer,1,imp3,outf);  
  fclose(outf);
  lame_close_infile();            /* close the input file */
  lame_mp3_tags();                /* add id3 or VBR tags to mp3 file */
  return 0;
}



/* The reason for this line:
 *
 *       int samples_to_encode = gf.encoder_delay + 288;
 * 
 * is:  
 *
 * amount of buffering in lame_encode = gf.encoder_delay 
 *
 * ALSO, because of the 50% overlap, a 576 MDCT granule decodes to 
 * 1152 samples.  To synthesize the 576 samples centered under this granule
 * we need the previous granule for the first 288 samples (no problem), and
 * the next granule for the next 288 samples (not possible if this is last 
 * granule).  So we need to pad with 288 samples to make sure we can 
 * encode the 576 samples we are interested in.
 */
