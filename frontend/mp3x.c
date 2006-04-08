/* $Id$ */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <gtk/gtk.h>

#include "lame.h"

#include "encoder.h"
#include "lame-analysis.h"
#include "parse.h"
#include "get_audio.h"
#include "gtkanal.h"
#include "lametime.h"
#include "set_get.h"

#include "main.h"

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

extern plotting_data Pinfo[];

/************************************************************************
*
* mp3x main, to display the internal parameters of LAME.
*
************************************************************************/
int main(int argc, char **argv)
{
    unsigned char mp3buffer[LAME_MAXMP3BUFFER];
    lame_t gf;
    char outPath[PATH_MAX + 1];
    char inPath[PATH_MAX + 1];
    int ret;

    print_version (stdout);
    gf=lame_init();
    if(argc <=1 ) {
	usage(stderr, argv[0]);  /* no command-line args  */
	return -1;
    }
    ret = parse_args(gf,argc, argv, inPath, outPath,NULL,NULL); 
    if (ret < 0)
	return ret == -2 ? 0 : 1;

    lame_set_analysis(gf, Pinfo);
    init_infile(gf,inPath);
    lame_init_params(gf);
    lame_set_findReplayGain(gf, 0);
    lame_print_config(gf);

    gtk_init (&argc, &argv);
    gtkcontrol(gf,inPath);

    lame_encode_flush(gf, mp3buffer, sizeof(mp3buffer));
    lame_close(gf);
    close_infile();
    return 0;
}
