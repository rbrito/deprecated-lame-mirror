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
  lame_init(1);
  
  if(argc==1)  lame_usage(argv[0]);  /* no command-line args  */
  else lame_parse_args(argc, argv); 
  lame_print_config();

  gf.gtkflag=1;
  gtk_init (&argc, &argv);
  gtkcontrol();

  lame_cleanup(mp3buffer);
  return 0;
}

