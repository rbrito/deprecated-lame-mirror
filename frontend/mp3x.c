/* $Id$ */

#ifdef HAVE_CONFIG_H
# include <config.h>
#elif defined(HAVE_CONFIG_MS_H)
# include <configMS.h>
#endif

#include "lame.h"

#include <stdio.h>

#include "lame-analysis.h"
#include <gtk/gtk.h>
#include "parse.h"
#include "get_audio.h"
#include "gtkanal.h"
#include "lametime.h"

#include "main.h"
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
int main(int argc, char **argv)
{
  char mp3buffer[LAME_MAXMP3BUFFER];
  lame_global_flags gf;  
  char outPath[MAX_NAME_SIZE];
  char inPath[MAX_NAME_SIZE];
  int ret;

  lame_init_old(&gf);
  if(argc <=1 ) {
    usage(&gf, stderr, argv[0]);  /* no command-line args  */
    return -1;
  }
  ret = parse_args(&gf,argc, argv, inPath, outPath); 
  if (ret < 0)
    return ret == -2 ? 0 : 1;
  
  gf.analysis=1;

  init_infile(&gf,inPath);
  lame_init_params(&gf);
  lame_print_config(&gf);


  gtk_init (&argc, &argv);
  gtkcontrol(&gf,inPath);

  lame_encode_finish(&gf,mp3buffer,sizeof(mp3buffer));
  close_infile();
  return 0;
}

