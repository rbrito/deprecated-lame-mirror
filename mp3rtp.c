#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "lame.h"
#include "rtp.h"


/*

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
  int iread;
  int samples_to_encode;
  short int Buffer[2][1152];

  if(argc<=2) {
    rtp_usage();
    exit(1);
  }

  gf=lame_init(0,0);

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


  lame_parse_args(argc-1, &argv[1]); 
  lame_init_params();
  lame_print_config();


  samples_to_encode = gf->encoder_delay + 288;

  /* encode until we hit eof */
  do {
    iread=lame_readframe(Buffer);
    lame_encode(Buffer,mp3buffer);
    samples_to_encode += iread - gf->framesize;
  } while (iread);
  
  /* encode until we flush internal buffers.  (Buffer=0 at this point */
  while (samples_to_encode > 0) {
    lame_encode(Buffer,mp3buffer);
    samples_to_encode -= gf->framesize;
  }
  

  lame_cleanup(mp3buffer);
  return 0;
}

