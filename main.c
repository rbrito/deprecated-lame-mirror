#include <stdlib.h>
#include <string.h>
#include "lame.h"
#include "machine.h"

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
  lame_global_flags *gf;
  FILE *outf;
#ifdef __riscos__
  int i;
#endif

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
    if ((outf = fopen(gf->outPath, "wb")) == NULL) {
      fprintf(stderr,"Could not create \"%s\".\n", gf->outPath);
      exit(1);
    }
  }

#ifdef __riscos__
  /* Assign correct file type */
  for (i = 0; gf->outPath[i]; i++)
    if (gf->outPath[i] == '.') gf->outPath[i] = '/';
  SetFiletype(gf->outPath, 0x1ad);
#endif

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

      /* encode until we hit eof */
      do {
	/* read in 'iread' samples */
	iread=lame_readframe(Buffer);

	/* check to make sure mp3buffer is big enough */
	if (LAME_MAXMP3BUFFER < 1.25*iread + 7200) 
	  fprintf(stderr,"mp3 buffer is not big enough. \n");

	/* encode */
	imp3=lame_encode_buffer(Buffer[0],Buffer[1],iread,
              mp3buffer,LAME_MAXMP3BUFFER); 

	fwrite(mp3buffer,1,imp3,outf);       /* write the MP3 output  */
      } while (iread);
    }

  imp3=lame_encode_finish(mp3buffer);   /* may return one more mp3 frame */
  fwrite(mp3buffer,1,imp3,outf);
  fclose(outf);
  lame_close_infile();            /* close the input file */
  lame_mp3_tags();                /* add id3 or VBR tags to mp3 file */
  return 0;
}



