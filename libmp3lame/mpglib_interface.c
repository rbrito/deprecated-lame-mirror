#ifdef HAVEMPGLIB
#include "interface.h"
#include "lame.h"

#include <limits.h>
#include <stdlib.h>

static char buf[16384];
#define FSIZE 8192  
static char out[FSIZE];
MPSTR mp;
plotting_data *mpg123_pinfo=NULL;

static const int smpls[2][4]={
/* Layer x  I   II   III */
        {0,384,1152,1152}, /* MPEG-1     */
        {0,384,1152, 576}  /* MPEG-2(.5) */
};




static int is_syncword(char *header)  
// bitstreams should be 'unsigned char', ISO otherwise not guarantees a lot !!!
// bit operator on 'char' are not portable.
{
#ifdef KLEMM_08
    const unsigned char* p = header;
    
    int mpeg1  = header[0] == 0xFF  &&  (header[1] & 0xF0) == 0xF0;
    int mpeg25 = header[0] == 0xFF  &&  (header[1] & 0xF0) == 0xE0;
  
    return mpeg1 | mpeg25;
    
#else
    // Sorry, I'm not a parser, I'm a human. I can't read this. I can only hope ...
/*
unsigned int s0,s1;
s0 = (unsigned char) header[0];
s1 = (unsigned char) header[1] ;
printf(" syncword:  %2X   %2X   \n ",s0, s1);
*/

  int mpeg1=((int) ( header[0] == (char) 0xFF)) &&
    ((int) ( (header[1] & (char) 0xF0) == (char) 0xF0));
  
  int mpeg25=((int) ( header[0] == (char) 0xFF)) &&
    ((int) ( (header[1] & (char) 0xF0) == (char) 0xE0));
  
  return (mpeg1 || mpeg25);
#endif 
}


int lame_decode_init(void)
{
  InitMP3(&mp);
  memset(buf, 0, sizeof(buf));
  return 0;
}




/*
For lame_decode:  return code
  -1     error
   0     ok, but need more data before outputing any samples
   n     number of samples output.  either 576 or 1152 depending on MP3 file.
*/
int lame_decode1_headers(char *buffer,int len,
                         short pcm_l[],short pcm_r[],
                         mp3data_struct *mp3data)
{
  int size;
  int outsize=0,j,i,ret;

  mp3data->header_parsed=0;
  ret = decodeMP3(&mp,buffer,len,out,FSIZE,&size);
  if (mp.header_parsed) {
    mp3data->header_parsed=1;
    mp3data->stereo = mp.fr.stereo;
    mp3data->samplerate = freqs[mp.fr.sampling_frequency];
    if (mp.fsizeold > 0)
      /* works for free format and fixed */
      mp3data->bitrate = .5 + 8*(4+mp.fsizeold)*freqs[mp.fr.sampling_frequency]/
	(1000.0*smpls[mp.fr.lsf][mp.fr.lay]);
    else
      mp3data->bitrate = tabsel_123[mp.fr.lsf][mp.fr.lay-1][mp.fr.bitrate_index];
    
    mp3data->mode=mp.fr.mode;
    mp3data->mode_ext=mp.fr.mode_ext;
    mp3data->framesize = smpls[mp.fr.lsf][mp.fr.lay];
  }
    
  if (ret==MP3_ERR) return -1;
  
  if (ret==MP3_OK) {
    outsize = size/(2*mp.fr.stereo);
    
    for (j=0; j<mp.fr.stereo; j++)
      for (i=0; i<outsize; i++) 
	if (j==0) pcm_l[i] = ((short *) out)[mp.fr.stereo*i+j];
	else pcm_r[i] = ((short *) out)[mp.fr.stereo*i+j];
  }
  if (ret==MP3_NEED_MORE) 
    outsize=0;
  
  /*
  printf("ok, more, err:  %i %i %i  \n",MP3_OK, MP3_NEED_MORE, MP3_ERR);
  printf("ret = %i out=%i \n",ret,totsize);
  */
  return outsize;
}


/*
For lame_decode:  return code
  -1     error
   0     ok, but need more data before outputing any samples
   n     number of samples output.  Will be at most one frame of
         MPEG data.  
*/
int lame_decode1(char *buffer,int len,short pcm_l[],short pcm_r[])
{
  mp3data_struct mp3data;
  return lame_decode1_headers(buffer,len,pcm_l,pcm_r,&mp3data);
}




/*
For lame_decode:  return code
  -1     error
   0     ok, but need more data before outputing any samples
   n     number of samples output.  a multiple of 576 or 1152 depending on MP3 file.
*/
int lame_decode_headers(char *buffer,int len,short pcm_l[],short pcm_r[],
                         mp3data_struct *mp3data)
{
  int ret,totsize=0;

  do {
    ret = lame_decode1_headers(buffer,len,&pcm_l[totsize],&pcm_r[totsize],mp3data);
    if (-1==ret) return -1;
    totsize += ret;
    len=0;  /* future calls to decodeMP3 are just to flush buffers */
  } while (ret>0);
  /*
  printf("ok, more, err:  %i %i %i  \n",MP3_OK, MP3_NEED_MORE, MP3_ERR);
  printf("ret = %i out=%i \n",ret,totsize);
  */
  return totsize;
}



int lame_decode(char *buffer,int len,short pcm_l[],short pcm_r[])
{
  mp3data_struct mp3data;
  return lame_decode_headers(buffer,len,pcm_l,pcm_r,&mp3data);
}

#endif
