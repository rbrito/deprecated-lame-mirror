#include "lame.h"

#include "gtkanal.h"
#include <gtk/gtk.h>




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
  lame_global_flags *gf;  

  gf=lame_init();

  if(argc==1)  lame_usage(argv[0]);  /* no command-line args  */

  lame_parse_args(argc, argv); 
  lame_init_infile();
  lame_init_params();
  lame_print_config();

  gf->gtkflag=1;
  gtk_init (&argc, &argv);
  gtkcontrol();

  lame_encode_finish(mp3buffer);
  lame_close_infile();
  return 0;
}

