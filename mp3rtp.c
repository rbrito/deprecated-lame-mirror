#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "lame.h"
#include "rtp.h"


/*

encode (via LAME) to mp3 with RTP streaming of the output.

Author:  Felix von Leitner <leitner@vim.org>

mp3rtp  ip:port:ttl  [lame encoding options]  infile outfile

example:

arecord -b 16 -s 22050 -w | ./mp3rtp 224.17.23.42:5004:2 -b 56 - /dev/null



*/


struct rtpheader RTPheader;
struct sockaddr_in rtpsi;
int rtpsocket;

void rtp_output(char *mp3buffer,int mp3size)
{
  sendrtp(rtpsocket,&rtpsi,&RTPheader,mp3buffer,mp3size);
  RTPheader.timestamp+=5;
  RTPheader.b.sequence++;
}

void rtp_usage(void) {
    fprintf(stderr,"usage: mp3rtp ip:port:ttl  [encoder options] <infile> <outfile>\n");
    exit(1);
}



char mp3buffer[LAME_MAXMP3BUFFER];


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

  int port,ttl;
  char *tmp,*Arg;
  lame_global_flags *gf;
  int iread,imp3;
  FILE *outf;
  int samples_to_encode;
  short int Buffer[2][1152];

  if(argc<=2) {
    rtp_usage();
    exit(1);
  }

  /* process args */
  Arg = argv[1];
  tmp=strchr(Arg,':');

  if (!tmp) {
    rtp_usage();
    exit(1);
  }
  *tmp++=0;
  port=atoi(tmp);
  if (port<=0) {
    rtp_usage();
    exit(1);
  }
  tmp=strchr(tmp,':');
  if (!tmp) {
    rtp_usage();
    exit(1);
  }
  *tmp++=0;
  ttl=atoi(tmp);
  if (tmp<=0) {
    rtp_usage();
    exit(1);
  }
  rtpsocket=makesocket(Arg,port,ttl,&rtpsi);
  srand(getpid() ^ time(0));
  initrtp(&RTPheader);


  /* initialize encoder */
  gf=lame_init();

  /* parse the rest of the options (for LAME), including input and output file names */
  lame_parse_args(argc-1, &argv[1]); 

  /* open the output file */
  if (!strcmp(gf->outPath, "-")) {
    outf = stdout;
  } else {
    if ((outf = fopen(gf->outPath, "wb")) == NULL) {
      fprintf(stderr,"Could not create \"%s\".\n", gf->outPath);
      exit(1);
    }
  }

  /* open the input file and parse headers if possible */
  lame_init_infile();

  lame_init_params();
  lame_print_config();
  samples_to_encode = gf->encoder_delay + 288;
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
  

  imp3=lame_cleanup(mp3buffer);
  fwrite(mp3buffer,1,imp3,outf);
  return 0;
}

