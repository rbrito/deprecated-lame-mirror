#include "lame.h"

#include "lame-analysis.h"
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
  lame_global_flags gf;  

  lame_init(&gf);
  if(argc==1)  lame_usage(&gf,argv[0]);  /* no command-line args  */

  lame_parse_args(&gf,argc, argv); 
  gf.analysis=1;

  init_infile(&gf);
  lame_init_params(&gf);
  lame_print_config(&gf);


  gtk_init (&argc, &argv);
  gtkcontrol(&gf);

  lame_encode_finish(&gf,mp3buffer,sizeof(mp3buffer));
  close_infile(&gf);
  return 0;
}

