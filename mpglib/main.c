#ifdef HAVEMPGLIB

#include "common.h"
#include "interface.h"

#ifdef PARENT_IS_SLASH
#include "/lame.h"
#include "/util.h"
#include "/VbrTag.h"
#else
#include "../lame.h"
#include "../util.h"
#include "../VbrTag.h"
#endif 

#include <limits.h>
#include <stdlib.h>

static char buf[16384];
#define FSIZE 8192  
static char out[FSIZE];
struct mpstr mp;
plotting_data *mpg123_pinfo=NULL;

static const int smpls[2][4]={
/* Layer x  I   II   III */
        {0,384,1152,1152}, /* MPEG-1     */
        {0,384,1152, 576}  /* MPEG-2(.5) */
};


int check_aid(char *header) {
  int aid_header =
    (header[0]=='A' && header[1]=='i' && header[2]=='D'
     && header[3]== (char) 1);
  return aid_header;
}


int is_syncword(char *header)
{

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





int lame_decode_initfile(FILE *fd, mp3data_struct *mp3data)
{
  VBRTAGDATA pTagData;
  int ret,size,framesize;
  unsigned long num_frames=0;
  int len,len2,xing_header,aid_header;
  short int pcm_l[1152],pcm_r[1152];

  lame_decode_init();

  len=4;
  if (fread(&buf,1,len,fd) == 0) return -1;  /* failed */
  aid_header = check_aid(buf);
  if (aid_header) {
    if (fread(&buf,1,2,fd) == 0) return -1;  /* failed */
    aid_header = (unsigned char) buf[0] + 256*(unsigned char)buf[1];
    fprintf(stderr,"Album ID found.  length=%i \n",aid_header);
    /* skip rest of AID, except for 6 bytes we have already read */
    fskip(fd,aid_header-6,1);

    /* read 2 more bytes to set up buffer for MP3 header check */
    if (fread(&buf,1,2,fd) == 0) return -1;  /* failed */
    len =2;
  }



  /* look for sync word  FFF */
  if (len<2) return -1;
  while (!is_syncword(buf)) {
    int i;
    for (i=0; i<len-1; i++)
      buf[i]=buf[i+1]; 
    if (fread(&buf[len-1],1,1,fd) == 0) return -1;  /* failed */
  }


  
  /* read the rest of header and enough bytes to check for Xing header */
  len2 = fread(&buf[len],1,48-len,fd);
  if (len2 ==0 ) return -1;
  len +=len2;


  /* check first 48 bytes for Xing header */
  xing_header = GetVbrTag(&pTagData,(unsigned char*)buf);
  if (xing_header && pTagData.headersize >= 48) {
    num_frames=pTagData.frames;
    
    fskip(fd,pTagData.headersize-48 ,1);
    /* look for next sync word in buffer*/
    len=2;
    buf[0]=buf[1]=0;
    while (!is_syncword(buf)) {
      buf[0]=buf[1]; 
      if (fread(&buf[1],1,1,fd) == 0) return -1;  /* fread failed */
    }
    /* read the rest of header */
    len2 = fread(&buf[2],1,2,fd);
    if (len2 ==0 ) return -1;
    len +=len2;


  } else {
    /* rewind file back what we read looking for Xing headers */
    if (fseek(fd, -44, SEEK_CUR) != 0) {
      /* backwards fseek failed.  input is probably a pipe */
    } else {
      len=4;
    }
  }


  /* now parse the current buffer looking for MP3 headers */
  size=0;
  ret = lame_decode1_headers(buf,len,pcm_l,pcm_r,mp3data);
  if (-1==ret) return -1;
  if (ret>0 && !xing_header) 
    fprintf(stderr,"Oops: first frame of mpglib output will be lost \n"); 

  /* repeat until we decode a valid mp3 header */
  while (!mp.header_parsed) {
    len = fread(buf,1,2,fd);
    if (len ==0 ) return -1;
    ret = lame_decode1_headers(buf,len,pcm_l,pcm_r,mp3data);
    if (-1==ret) return -1;

    if (ret>0 && !xing_header)
      fprintf(stderr,"Oops: first frame of mpglib output will be lost \n"); 
  }

  mp3data->nsamp=MAX_U_32_NUM;
  framesize = smpls[mp.fr.lsf][mp.fr.lay];
  if (xing_header && num_frames) {
    mp3data->nsamp=framesize * num_frames;
  }


  /*
  fprintf(stderr,"ret = %i NEED_MORE=%i \n",ret,MP3_NEED_MORE);
  fprintf(stderr,"stereo = %i \n",mp.fr.stereo);
  fprintf(stderr,"samp = %i  \n",freqs[mp.fr.sampling_frequency]);
  fprintf(stderr,"framesize = %i  \n",framesize);
  fprintf(stderr,"bitrate = %i  \n",mp3data->bitrate);
  fprintf(stderr,"num frames = %ui  \n",num_frames);
  fprintf(stderr,"num samp = %ui  \n",mp3data->nsamp);
  fprintf(stderr,"mode     = %i  \n",mp.fr.mode);
  */

  return 0;
}




/*
For lame_decode_fromfile:  return code
  -1     error
   0     ok, but need more data before outputing any samples
   n     number of samples output.  either 576 or 1152 depending on MP3 file.
*/
int lame_decode_fromfile(FILE *fd, short pcm_l[], short pcm_r[],mp3data_struct *mp3data)
{
  int ret=0,len;

  /* read until we get a valid output frame */
  while(0==ret) {
    len = fread(buf,1,100,fd);
    if (len ==0 ) return -1;
    ret = lame_decode1_headers(buf,len,pcm_l,pcm_r,mp3data);
    if (ret == -1) return -1;
  }
  return ret;
}





#endif /* HAVEMPGLIB */

