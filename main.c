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

  gf=lame_init();
  if(argc==1) lame_usage(argv[0]);  /* no command-line args  */

  lame_parse_args(argc, argv);

  /* open the output file */
  if (!strcmp(gf->outPath, "-")) {
    outf = stdout;
  } else {
    if ((outf = fopen(gf->outPath, "wb")) == NULL) {
      fprintf(stderr,"Could not create \"%s\".\n", gf->outPath);
      exit(1);
    }
  }
  lame_init_infile();
  lame_init_params();
  lame_print_config();




#ifdef HAVEGTK
  if (gf->gtkflag) gtk_init (&argc, &argv);
  if (gf->gtkflag) gtkcontrol();
  else 
#endif
    {
      int samples_to_encode = gf->encoder_delay + 288;

      /* encode until we hit eof */
      do {
	iread=lame_readframe(Buffer);
	imp3=lame_encode(Buffer,mp3buffer);
	fwrite(mp3buffer,1,imp3,outf);
	samples_to_encode += iread - gf->framesize;
      } while (iread);

      /* encode until we flush internal buffers.  (Buffer=0 at this point */
      while (samples_to_encode > 0) {
	imp3=lame_encode(Buffer,mp3buffer);
	fwrite(mp3buffer,1,imp3,outf);
	samples_to_encode -= gf->framesize;
      }
    }
  
  imp3=lame_cleanup(mp3buffer);
  fwrite(mp3buffer,1,imp3,outf);
  fclose(outf);
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
