/* $Id$ */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "lame.h"
#include "parse.h"
#include "lametime.h"
#include "timestatus.h"
#include "get_audio.h"
#include "rtp.h"



#define         MAX_NAME_SIZE           1000

/*

encode (via LAME) to mp3 with RTP streaming of the output.

Author:  Felix von Leitner <leitner@vim.org>

mp3rtp  ip:port:ttl  [lame encoding options]  infile outfile

example:

arecord -b 16 -s 22050 -w | ./mp3rtp 224.17.23.42:5004:2 -b 56 - /dev/null



*/


struct rtpheader RTPheader;
struct sockaddr_in rtpsi;
int     rtpsocket;

void
rtp_output (char *mp3buffer, int mp3size)
{
    sendrtp (rtpsocket, &rtpsi, &RTPheader, mp3buffer, mp3size);
    RTPheader.timestamp += 5;
    RTPheader.b.sequence++;
}

void
rtp_usage (void)
{
    fprintf (stderr,
             "usage: mp3rtp ip:port:ttl  [encoder options] <infile> <outfile>\n");
    exit (1);
}



sound_file_format input_format;
int     swapbytes;           /* force byte swapping   default=0 */
int     silent;
int     brhist;
float   update_interval;     /* to use Frank's time status display */

/************************************************************************
*
* main
*
* PURPOSE:  MPEG-1,2 Layer III encoder with GPSYCHO 
* psychoacoustic model.
*
************************************************************************/


int
main (int argc, char **argv)
{
    char    mp3buffer[LAME_MAXMP3BUFFER];
    char    inPath[MAX_NAME_SIZE];
    char    outPath[MAX_NAME_SIZE];

    int     i, port, ttl;
    char   *tmp, *Arg;
    lame_global_flags gf;
    int     iread, imp3;
    FILE   *outf;
    short int Buffer[2][1152];

    if (argc <= 2) {
        rtp_usage ();
        exit (1);
    }

    /* process args */
    Arg = argv[1];
    tmp = strchr (Arg, ':');

    if (!tmp) {
        rtp_usage ();
        exit (1);
    }
    *tmp++ = 0;
    port = atoi (tmp);
    if (port <= 0) {
        rtp_usage ();
        exit (1);
    }
    tmp = strchr (tmp, ':');
    if (!tmp) {
        rtp_usage ();
        exit (1);
    }
    *tmp++ = 0;
    ttl = atoi (tmp);
    if (tmp <= 0) {
        rtp_usage ();
        exit (1);
    }
    rtpsocket = makesocket (Arg, port, ttl, &rtpsi);
    srand (getpid () ^ time (0));
    initrtp (&RTPheader);


    /* initialize encoder */
    lame_init_old (&gf);

    /* Remove the argumets that are rtp related, and then 
     * parse the command line arguments, setting various flags in the
     * struct pointed to by 'gf'.  If you want to parse your own arguments,
     * or call libmp3lame from a program which uses a GUI to set arguments,
     * skip this call and set the values of interest in the gf struct.  
     * (see lame.h for documentation about these parameters)
     */
    for (i = 1; i < argc - 1; i++) /* remove first argument, it was for rtp */
        argv[i] = argv[i + 1];
    parse_args (&gf, argc - 1, argv, inPath, outPath);

    /* open the output file.  Filename parsed into gf.inPath */
    if (!strcmp (outPath, "-")) {
        lame_set_stream_binary_mode (outf = stdout);
    }
    else {
        if ((outf = fopen (outPath, "wb+")) == NULL) {
            fprintf (stderr, "Could not create \"%s\".\n", outPath);
            exit (1);
        }
    }


    /* open the wav/aiff/raw pcm or mp3 input file.  This call will
     * open the file with name gf.inFile, try to parse the headers and
     * set gf.samplerate, gf.num_channels, gf.num_samples.
     * if you want to do your own file input, skip this call and set
     * these values yourself.  
     */
    init_infile (&gf, inPath);


    /* Now that all the options are set, lame needs to analyze them and
     * set some more options 
     */
    i = lame_init_params (&gf);
    if (i < 0) {
        if (i == -1) {
            display_bitrates (stderr);
        }
        fprintf (stderr, "fatal error during initialization\n");
        exit (-1);
    }

    lame_print_config (&gf); /* print usefull information about options being used */

    if (update_interval < 0.)
        update_interval = 2.;


//    lame_id3v2_tag (&gf, outf); /* add ID3 version 2 tag to mp3 file */

    /* encode until we hit eof */
    do {
        /* read in 'iread' samples */
        iread = get_audio (&gf, Buffer);
        /* encode the frame */
        imp3 = lame_encode_buffer (&gf, Buffer[0], Buffer[1], iread,
                                   mp3buffer, sizeof (mp3buffer));

        fwrite (mp3buffer, 1, imp3, outf); /* write the MP3 output to file  */
        rtp_output (mp3buffer, imp3); /* write MP3 output to RTP port */
    } while (iread);


    imp3 = lame_encode_flush (&gf, mp3buffer, sizeof (mp3buffer)); /* may return one or more mp3 frame */
    fwrite (mp3buffer, 1, imp3, outf);
    rtp_output (mp3buffer, imp3);
    lame_mp3_tags_fid (&gf, outf); /* add ID3 version 1 or VBR tags to mp3 file */
    lame_close (&gf);
    fclose (outf);
    close_infile ();    /* close the sound input file */
    return 0;
}
