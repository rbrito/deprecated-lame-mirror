#include "lame.h"

#ifdef HAVEGTK
#include "gtkanal.h"
#include <gtk/gtk.h>
#endif

char mp3buffer[LAME_MAXMP3BUFFER];
lame_mp3info  mp3info;


/**********************************************************************
 * read one frame and encode it 
 **********************************************************************/
int makeframe(void)
{
  int iread;
  static short int Buffer[2][1152];
  static int last_nonzero_iread=0;
  int samples_to_encode;

  iread=lame_readframe(Buffer);
  if (iread>0) {
    last_nonzero_iread=iread;
    lame_encode(Buffer,mp3buffer);
  }else{
    /* if iread=0, call lame_encode a few more times to flush buffers 
     * amount of buffering in lame_encode = mp3info.encoder_delay + last_frame 
     * and number of samples in last_frame = last_nonzero_iread
     *
     * ALSO, because of the 50% overlap, a 576 MDCT granule decodes to 
     * 1152 samples.  To synthesize the 576 samples centered under this granule
     * we need the previous granule for the first 288 samples (no problem), and
     * the next granule for the next 288 samples (not possible if this is last 
     * granule).  So we need to pad with 288 samples to make sure we can 
     * encode the 576 samples we are interested in.
     */
    samples_to_encode = mp3info.encoder_delay + last_nonzero_iread + 288;

    /* to guarantee no padding (at the risk of not encoding all data, use:*/
    /* while (samples_to_encode >= mp3info.framesize) { */       
    while (samples_to_encode > 0) {
      lame_encode(Buffer,mp3buffer);
      samples_to_encode -= mp3info.framesize;
    }


  }
  return iread;
}




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
  lame_init(0);
  if(argc==1) lame_usage(argv[0]);  /* no command-line args  */
  lame_parse_args(argc, argv); 
  lame_print_config();
  lame_getmp3info(&mp3info);

#ifdef HAVEGTK
  if (gtkflag) gtk_init (&argc, &argv);
  if (gtkflag) gtkcontrol();
  else 
#endif
    while (makeframe());


  lame_cleanup(mp3buffer);
  return 0;
}

