#include "mpg123.h"
#include "mpglib.h"

#ifdef OS_AMIGAOS
#include "/lame.h"
#else
#include "../lame.h"
#endif /* OS_AMIGAOS */

#include <stdlib.h>

static char buf[16384];
#define FSIZE 8192  
static char out[FSIZE];
struct mpstr mp;

#if 0
void main(void)
{
	int size;
	char out[8192];
	int len,ret;
	

	InitMP3(&mp);

	while(1) {
		len = read(0,buf,16384);
		if(len <= 0)
			break;
		ret = decodeMP3(&mp,buf,len,out,8192,&size);
		while(ret == MP3_OK) {
			write(1,out,size);
			ret = decodeMP3(&mp,NULL,0,out,8192,&size);
		}
	}

}
#endif

int is_syncword(char *header)
{

/*
unsigned int s0,s1;
s0 = (unsigned char) header[0];
s1 = (unsigned char) header[1] ;
printf(" syncword:  %2X   %2X   \n ",s0, s1);
*/

/*
printf(" integer  %i \n",(int) ( header[0] == (char) 0xFF));
printf(" integer  %i \n",(int) ( (header[1] & (char) 0xF0) == (char) 0xF0));
*/

return 
((int) ( header[0] == (char) 0xFF)) &&
((int) ( (header[1] & (char) 0xF0) == (char) 0xF0));


}


int lame_decode_initfile(FILE *fd, int *stereo, int *samp, int *bitrate)
{
  extern int tabsel_123[2][3][16];
  int ret,size;
  size_t len;
  InitMP3(&mp);
  memset(buf, 0, sizeof(buf));
  
  /* skip RIFF type proprietary headers  */
  /* look for sync word  FFF */
  while (!is_syncword(buf)) {
    buf[0]=buf[1]; 
    if (fread(&buf[1],1,1,fd) <= 0) return -1;  /* failed */
  }
  ret = decodeMP3(&mp,buf,2,out,FSIZE,&size);

  /* read the header */
  len = fread(buf,1,8,fd);
  if (len <=0 ) return -1;
  ret = decodeMP3(&mp,buf,len,out,FSIZE,&size);

  *stereo = mp.fr.stereo;
  *samp = freqs[mp.fr.sampling_frequency];
  *bitrate = tabsel_123[mp.fr.lsf][mp.fr.lay-1][mp.fr.bitrate_index];


  /*
  printf("ret = %i NEED_MORE=%i \n",ret,MP3_NEED_MORE);
  printf("stereo = %i \n",mp.fr.stereo);
  printf("samp = %i  \n",(int)freqs[mp.fr.sampling_frequency]);
  */

  return 0;
}



int lame_decode_fromfile(FILE *fd, short pcm[][1152])
{
  int size,stereo;
  int outsize=0,j,i,ret;
  size_t len;

  size=0;
  len = fread(buf,1,64,fd);
  if (len <=0 ) return 0;
  ret = decodeMP3(&mp,buf,len,out,FSIZE,&size);

  /* read more until we get a valid output frame */
  while((ret == MP3_NEED_MORE) || !size) {
    len = fread(buf,1,100,fd);
    if (len <=0 ) return -1;
    ret = decodeMP3(&mp,buf,len,out,FSIZE,&size);
    /* if (ret ==MP3_ERR) return -1;  lets ignore errors and keep reading... */
    /*
    printf("ret = %i size= %i  %i   %i  %i \n",ret,size,
	   MP3_NEED_MORE,MP3_ERR,MP3_OK); 
    */
  }

  stereo=mp.fr.stereo;

  if (ret == MP3_OK) 
  {
    /*    write(1,out,size); */
    outsize = size/(2*(stereo));
    if ((outsize!=576) && (outsize!=1152)) {
      fprintf(stderr,"Opps: mpg123 returned more than one frame!  Cant handle this... \n");
      exit(-50);
    }

    for (j=0; j<stereo; j++)
      for (i=0; i<outsize; i++) 
	pcm[j][i] = ((short *) out)[stereo*i+j];
  }
  if (ret==MP3_ERR) return -1;
  else return outsize;
}


int lame_decode_init(void)
{
  InitMP3(&mp);
  memset(buf, 0, sizeof(buf));
  return 0;
}


int lame_decode(char *buf,int len,short pcm[][1152])
{
  int size;
  int outsize=0,j,i,ret;

  ret = decodeMP3(&mp,buf,len,out,FSIZE,&size);
  if (ret==MP3_OK) {
    /*    printf("mpg123 output one frame out=%i \n",size/4);  */
    outsize = size/(2*mp.fr.stereo);
    if (outsize > 1152) {
      fprintf(stderr,"Opps: mpg123 returned more than one frame!  shouldn't happen... \n");
      exit(-50);
    }

    for (j=0; j<mp.fr.stereo; j++)
      for (i=0; i<outsize; i++) 
	pcm[j][i] = ((short *) out)[mp.fr.stereo*i+j];
  }
  /*
  printf("ok, more, err:  %i %i %i  \n",MP3_OK, MP3_NEED_MORE, MP3_ERR);
  printf("ret = %i out=%i \n",ret,outsize);
  */
  if (ret==MP3_ERR) return -1;
  else return outsize;
}




