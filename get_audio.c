#include "util.h"
#include "get_audio.h"
#include "portableio.h"
#include "gtkanal.h"
#include "timestatus.h"

#if (defined LIBSNDFILE || defined LAMESNDFILE)

#ifdef _WIN32
/* needed to set stdin to binary on windoze machines */
#include <io.h>
#endif

#ifdef __riscos__
#include <kernel.h>
#include <sys/swis.h>
#endif


int read_samples_pcm(lame_global_flags *gfp,short sample_buffer[2304],int frame_size, int samples_to_read);
int read_samples_mp3(lame_global_flags *gfp,FILE *musicin,short int mpg123pcm[2][1152],int num_chan);
int read_samples_ogg(lame_global_flags *gfp,FILE *musicin,short int mpg123pcm[2][1152],int num_chan);


void lame_init_infile(lame_global_flags *gfp)
{
  lame_internal_flags *gfc=gfp->internal_flags;
  /* open the input file */
  gfc->count_samples_carefully=0;
  gfp->musicin=OpenSndFile(gfp);
}
void lame_close_infile(lame_global_flags *gfp)
{
  CloseSndFile(gfp->input_format,gfp->musicin);
}




/************************************************************************
*
* lame_readframe()
*
* PURPOSE:  reads a frame of audio data from a file to the buffer,
*   aligns the data for future processing, and separates the
*   left and right channels
*
*
************************************************************************/
int lame_readframe(lame_global_flags *gfp,short int Buffer[2][1152])
{
  int iread;
  lame_internal_flags *gfc=gfp->internal_flags;


  /* note: if input is gfp->stereo and output is mono, get_audio()
   * will return  .5*(L+R) in channel 0,  and nothing in channel 1. */
  iread = get_audio(gfp,Buffer,gfc->stereo);

  /* check to see if we overestimated/underestimated totalframes */
  if (iread==0)  gfp->totalframes = Min(gfp->totalframes,gfp->frameNum+2);
  if (gfp->frameNum > (gfp->totalframes-1)) gfp->totalframes = gfp->frameNum;

  /* normally, frame counter is incremented every time we encode a frame, but: */
  if (gfp->decode_only) ++gfp->frameNum;
  return iread;
}





/************************************************************************
*
* get_audio()
*
* PURPOSE:  reads a frame of audio data from a file to the buffer,
*   aligns the data for future processing, and separates the
*   left and right channels
*
*
************************************************************************/
int get_audio(lame_global_flags *gfp,short buffer[2][1152],int stereo)
{

  int		j;
  short	insamp[2304];
  int samples_read;
  int framesize,samples_to_read;
  unsigned long remaining;
  lame_internal_flags *gfc=gfp->internal_flags;
  int num_channels = gfp->num_channels;

  /* NOTE: LAME can now handle arbritray size input data packets,
   * so there is no reason to read the input data in chuncks of
   * size "gfp->framesize".  EXCEPT:  the LAME graphical frame analyzer 
   * will get out of sync if we read more than framesize worth of data.
   */
  framesize = gfp->framesize;

  samples_to_read = framesize;
  if (gfc->count_samples_carefully) {
    /* if this flag has been set, then we are carefull to read
     * exactly num_samples and no more.  This is usefull for .wav and .aiff
     * files which have id3 or other tags at the end.  Note that if you
     * are using LIBSNDFILE, this is not necessary */
    remaining=gfp->num_samples-Min(gfp->num_samples,gfc->num_samples_read);
    if (remaining < (unsigned long)framesize)
      samples_to_read = remaining;
  }


  if (gfp->input_format==sf_mp3) {
    /* decode an mp3 file for the input */
    samples_read=read_samples_mp3(gfp,gfp->musicin,buffer,num_channels);
  } else if (gfp->input_format==sf_ogg) {
    samples_read=read_samples_ogg(gfp,gfp->musicin,buffer,num_channels);
  }else{
    samples_read = read_samples_pcm(gfp,insamp,num_channels*framesize,num_channels*samples_to_read);
    samples_read /=num_channels;

    for(j=0;j<framesize;j++) {
      buffer[0][j] = insamp[num_channels*j];
      if (num_channels==2) buffer[1][j] = insamp[2*j+1];
      else buffer[1][j]=0;
    }
  }

  /* dont count things in this case to avoid overflows */
  if (gfp->num_samples!=MAX_U_32_NUM) gfc->num_samples_read += samples_read;
  return(samples_read);

}


  



int read_samples_ogg(lame_global_flags *gfp,FILE *musicin,short int mpg123pcm[2][1152],int stereo)
{
#ifdef HAVEVORBIS
  int j,out=0;
  lame_internal_flags *gfc=gfp->internal_flags;
  mp3data_struct mp3data;

  out=lame_decode_ogg_fromfile(musicin,mpg123pcm[0],mpg123pcm[1],&mp3data);
  /* out = -1:  error, probably EOF */
  /* out = 0:   not possible with lame_decode_fromfile() */
  /* out = number of output samples */

  if (out==-1) {
    for ( j = 0; j < 1152; j++ ) {
      mpg123pcm[0][j] = 0;
      mpg123pcm[1][j] = 0;
    }
  }

  if (out==-1) return 0;

  gfp->brate=mp3data.bitrate;
  if (gfp->num_channels != mp3data.stereo) {
    ERRORF("Error: number of channels has changed in mp3 file - not supported. \n");
  }
  if (gfp->in_samplerate != mp3data.samplerate) {
    ERRORF("Error: samplerate has changed in mp3 file - not supported. \n");
  }


  return out;
#else
  return -1; /* wanna read ogg without vorbis support? */
#endif
}


int read_samples_mp3(lame_global_flags *gfp,FILE *musicin,short int mpg123pcm[2][1152],int stereo)
{
#if (defined  AMIGA_MPEGA || defined HAVEMPGLIB)
  int j,out=0;
  lame_internal_flags *gfc=gfp->internal_flags;
  mp3data_struct mp3data;

  out=lame_decode_fromfile(musicin,mpg123pcm[0],mpg123pcm[1],&mp3data);
  /* out = -1:  error, probably EOF */
  /* out = 0:   not possible with lame_decode_fromfile() */
  /* out = number of output samples */

  if (out==-1) {
    for ( j = 0; j < 1152; j++ ) {
      mpg123pcm[0][j] = 0;
      mpg123pcm[1][j] = 0;
    }
  }


  if (gfc->pinfo != NULL) {
    int ch;
    /* add a delay of framesize-DECDELAY, which will make the total delay
     * exactly one frame, so we can sync MP3 output with WAV input */
    for ( ch = 0; ch < stereo; ch++ ) {
      for ( j = 0; j < gfp->framesize-DECDELAY; j++ )
	gfc->pinfo->pcmdata2[ch][j] = gfc->pinfo->pcmdata2[ch][j+gfp->framesize];
      for ( j = 0; j < gfp->framesize; j++ )
	gfc->pinfo->pcmdata2[ch][j+gfp->framesize-DECDELAY] = mpg123pcm[ch][j];
    }

  gfc->pinfo->frameNum123 = gfp->frameNum-1;
  gfc->pinfo->frameNum = gfp->frameNum;
  }

  if (out==-1) return 0;

  gfp->brate=mp3data.bitrate;
  if (gfp->num_channels != mp3data.stereo) {
    ERRORF("Error: number of channels has changed in mp3 file - not supported. \n");
  }
  if (gfp->in_samplerate != mp3data.samplerate) {
    ERRORF("Error: samplerate has changed in mp3 file - not supported. \n");
  }
  return out;

#endif
}





void WriteWav(FILE *f,long bytes,int srate,int ch){
  /* quick and dirty */
  fwrite("RIFF",1,4,f);               /*  0-3 */
  Write32BitsLowHigh(f,bytes+44-8);
  fwrite("WAVEfmt ",1,8,f);           /*  8-15 */
  Write32BitsLowHigh(f,16);
  Write16BitsLowHigh(f,1);
  Write16BitsLowHigh(f,ch);
  Write32BitsLowHigh(f,srate);
  Write32BitsLowHigh(f,srate*ch*2);
  Write16BitsLowHigh(f,4);
  Write16BitsLowHigh(f,16);
  fwrite("data",1,4,f);               /* 36-39 */
  Write32BitsLowHigh(f,bytes);
}

/* the simple lame decoder */
/* After calling lame_init(), lame_init_params() and
 * lame_init_infile(), call this routine to read the input MP3 file
 * and output .wav data to the specified file pointer*/
/* lame_decoder will ignore the first 528 samples, since these samples
 * represent the mpglib delay (and are all 0).  skip = number of additional
 * samples to skip, to (for example) compensate for the encoder delay */
int lame_decoder(lame_global_flags *gfp,FILE *outf,int skip)
{
  short int Buffer[2][1152];
  int iread;
  long wavsize=2147483647L;  /* max for a signed long */

  if (gfp->input_format==sf_mp3) {
    /* mp3 decoder has a 528 sample delay, plus user supplied "skip" */
    skip+=528;
    MSGF("input:    %s %.1fkHz MPEG%i %i channel LayerIII\n",
	  (strcmp(gfp->inPath, "-")? gfp->inPath : "stdin"),
	  gfp->in_samplerate/1000.0,2-gfp->version,gfp->num_channels);

  }else{
    /* other formats have no delay */
    skip=0;
    MSGF("input:    %s %.1fkHz %i channel\n",
	  (strcmp(gfp->inPath, "-")? gfp->inPath : "stdin"),
	  gfp->in_samplerate/1000.0,gfp->num_channels);
  }

  MSGF("output:   %s (wav format)\n",
	  (strcmp(gfp->outPath, "-")? gfp->outPath : "stdout"));
  if (skip>0)
    MSGF("skipping initial %i samples (encoder + decoder delay)\n",skip);
  WriteWav(outf,wavsize,gfp->in_samplerate,gfp->num_channels);
  wavsize=-skip;
  do {
    int i;
    /* read in 'iread' samples */
    iread=lame_readframe(gfp,Buffer);
    wavsize += iread;
    if (!gfp->silent)
      decoder_progress(gfp);
    for (i=0; i<iread; ++i) {
      if (skip) {
	--skip;
      }else{
	Write16BitsLowHigh(outf,Buffer[0][i]);
	if (gfp->num_channels==2)
	  Write16BitsLowHigh(outf,Buffer[1][i]);
      }
    }
  } while (iread);
  if (wavsize<0) wavsize=0;
  wavsize *= 2*gfp->num_channels;
  decoder_progress_finish(gfp);
  /* if outf is seekable, rewind and adjust length */
  if (!fseek(outf,0,SEEK_SET))
    WriteWav(outf,wavsize,gfp->in_samplerate,gfp->num_channels);
  fclose(outf);

  return 0;
}


#endif  /* LAMESNDFILE or LIBSNDFILE */




#ifdef LIBSNDFILE
/*
** Copyright (C) 1999 Albert Faber
**
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#include <stdio.h>




void CloseSndFile(sound_file_format input,FILE *musicin)
{
  SNDFILE *gs_pSndFileIn=(SNDFILE*)musicin;
  if (input==sf_mp3) {
#ifndef AMIGA_MPEGA
    if (fclose(musicin) != 0){
      ERRORF("Could not close audio input file\n");
      LAME_FATAL_EXIT();
    }
#endif
  }else{
	if (gs_pSndFileIn)
	{
		if (sf_close(gs_pSndFileIn) !=0)
		{
			ERRORF("Could not close sound file \n");
			LAME_FATAL_EXIT();
		}
	}
  }
}



FILE * OpenSndFile(lame_global_flags *gfp)
{
  lame_internal_flags *gfc=gfp->internal_flags;
  const char* lpszFileName = gfp->inPath;
  FILE * musicin;
  SNDFILE *gs_pSndFileIn;
  SF_INFO gs_wfInfo;

  gfc->input_bitrate=0;
  if (gfp->input_format==sf_mp3) {
    mp3data_struct mp3data;
#ifdef AMIGA_MPEGA
    if (-1==lame_decode_initfile(lpszFileName,&mp3data)) {
      ERRORF("Error reading headers in mp3 input file %s.\n", lpszFileName);
      LAME_ERROR_EXIT();
    }
#endif
#ifdef HAVEMPGLIB
    if ((musicin = fopen(lpszFileName, "rb")) == NULL) {
	  ERRORF("Could not find \"%s\".\n", lpszFileName);
	  LAME_ERROR_EXIT();
    }
    if (-1==lame_decode_initfile(musicin,&mp3data)) {
	  ERRORF("Error reading headers in mp3 input file %s.\n", lpszFileName);
	  LAME_ERROR_EXIT();
	}
#endif

    gfp->num_channels=mp3data.stereo;
    gfp->in_samplerate=mp3data.samplerate;
    gfc->input_bitrate=mp3data.bitrate;
    gfp->num_samples=mp3data.nsamp;
  }else if (gfp->input_format==sf_ogg) {
#ifdef HAVEVORBIS
    if ((musicin = fopen(lpszFileName, "rb")) == NULL) {
	  ERRORF("Could not find \"%s\".\n", lpszFileName);
	  LAME_ERROR_EXIT();
    }
    if (-1==lame_decode_ogg_initfile(musicin,&mp3data)) {
	  ERRORF("Error reading headers in mp3 input file %s.\n", lpszFileName);
	  LAME_ERROR_EXIT();
	}
    gfp->num_channels=0;
    gfp->in_samplerate=0;
    gfc->input_bitrate=0;
    gfp->num_samples=0;
#else
    ERRORF("LAME not compiled with libvorbis support.\n");
    LAME_ERROR_EXIT();
#endif


  } else {

    /* Try to open the sound file */
    /* set some defaults incase input is raw PCM */
    gs_wfInfo.seekable=(gfp->input_format!=sf_raw);  /* if user specified -r, set to not seekable */
    gs_wfInfo.samplerate=gfp->in_samplerate;
    gs_wfInfo.pcmbitwidth=16;
    gs_wfInfo.channels=gfp->num_channels;
    if (DetermineByteOrder()==order_littleEndian) {
      if (gfp->swapbytes) gs_wfInfo.format=SF_FORMAT_RAW_BE;
      else gs_wfInfo.format=SF_FORMAT_RAW_LE;
    } else {
      if (gfp->swapbytes) gs_wfInfo.format=SF_FORMAT_RAW_LE;
      else gs_wfInfo.format=SF_FORMAT_RAW_BE;
    }

    gs_pSndFileIn=sf_open_read(lpszFileName,&gs_wfInfo);
    musicin = (SNDFILE *) gs_pSndFileIn;

        /* Check result */
	if (gs_pSndFileIn==NULL)
	{
	        sf_perror(gs_pSndFileIn);
		ERRORF("Could not open sound file \"%s\".\n", lpszFileName);
		LAME_ERROR_EXIT();
	}

    if ((gs_wfInfo.format==SF_FORMAT_RAW_LE) ||
	(gs_wfInfo.format==SF_FORMAT_RAW_BE))
      gfp->input_format=sf_raw;

#ifdef _DEBUG_SND_FILE
	DEBUGF("\n\nSF_INFO structure\n");
	DEBUGF("samplerate        :%d\n",gs_wfInfo.samplerate);
	DEBUGF("samples           :%d\n",gs_wfInfo.samples);
	DEBUGF("channels          :%d\n",gs_wfInfo.channels);
	DEBUGF("pcmbitwidth       :%d\n",gs_wfInfo.pcmbitwidth);
	DEBUGF("format            :");

	/* new formats from sbellon@sbellon.de  1/2000 */
        if ((gs_wfInfo.format&SF_FORMAT_TYPEMASK)==SF_FORMAT_WAV)
	  DEBUGF("Microsoft WAV format (big endian). ");
	if ((gs_wfInfo.format&SF_FORMAT_TYPEMASK)==SF_FORMAT_AIFF)
	  DEBUGF("Apple/SGI AIFF format (little endian). ");
	if ((gs_wfInfo.format&SF_FORMAT_TYPEMASK)==SF_FORMAT_AU)
	  DEBUGF("Sun/NeXT AU format (big endian). ");
	if ((gs_wfInfo.format&SF_FORMAT_TYPEMASK)==SF_FORMAT_AULE)
	  DEBUGF("DEC AU format (little endian). ");
	if ((gs_wfInfo.format&SF_FORMAT_TYPEMASK)==SF_FORMAT_RAW)
	  DEBUGF("RAW PCM data. ");
	if ((gs_wfInfo.format&SF_FORMAT_TYPEMASK)==SF_FORMAT_PAF)
	  DEBUGF("Ensoniq PARIS file format. ");
	if ((gs_wfInfo.format&SF_FORMAT_TYPEMASK)==SF_FORMAT_SVX)
	  DEBUGF("Amiga IFF / SVX8 / SV16 format. ");
	if ((gs_wfInfo.format&SF_FORMAT_TYPEMASK)==SF_FORMAT_NIST)
	  DEBUGF("Sphere NIST format. ");

	if ((gs_wfInfo.format&SF_FORMAT_SUBMASK)==SF_FORMAT_PCM)
	  DEBUGF("PCM data in 8, 16, 24 or 32 bits.");
	if ((gs_wfInfo.format&SF_FORMAT_SUBMASK)==SF_FORMAT_FLOAT)
	  DEBUGF("32 bit Intel x86 floats.");
	if ((gs_wfInfo.format&SF_FORMAT_SUBMASK)==SF_FORMAT_ULAW)
	  DEBUGF("U-Law encoded.");
	if ((gs_wfInfo.format&SF_FORMAT_SUBMASK)==SF_FORMAT_ALAW)
	  DEBUGF("A-Law encoded.");
	if ((gs_wfInfo.format&SF_FORMAT_SUBMASK)==SF_FORMAT_IMA_ADPCM)
	  DEBUGF("IMA ADPCM.");
	if ((gs_wfInfo.format&SF_FORMAT_SUBMASK)==SF_FORMAT_MS_ADPCM)
	  DEBUGF("Microsoft ADPCM.");
	if ((gs_wfInfo.format&SF_FORMAT_SUBMASK)==SF_FORMAT_PCM_BE)
	  DEBUGF("Big endian PCM data.");
	if ((gs_wfInfo.format&SF_FORMAT_SUBMASK)==SF_FORMAT_PCM_LE)
	  DEBUGF("Little endian PCM data.");
	if ((gs_wfInfo.format&SF_FORMAT_SUBMASK)==SF_FORMAT_PCM_S8)
	  DEBUGF("Signed 8 bit PCM.");
	if ((gs_wfInfo.format&SF_FORMAT_SUBMASK)==SF_FORMAT_PCM_U8)
	  DEBUGF("Unsigned 8 bit PCM.");
	if ((gs_wfInfo.format&SF_FORMAT_SUBMASK)==SF_FORMAT_SVX_FIB)
	  DEBUGF("SVX Fibonacci Delta encoding.");
	if ((gs_wfInfo.format&SF_FORMAT_SUBMASK)==SF_FORMAT_SVX_EXP)
	  DEBUGF("SVX Exponential Delta encoding.");




	DEBUGF("\n");
	DEBUGF("pcmbitwidth       :%d\n",gs_wfInfo.pcmbitwidth);
	DEBUGF("sections          :%d\n",gs_wfInfo.sections);
	DEBUGF("seekable          :\n",gs_wfInfo.seekable);
#endif

    gfp->num_samples = gs_wfInfo.samples;
    gfp->num_channels = gs_wfInfo.channels;
    gfp->in_samplerate = gs_wfInfo.samplerate;
    gfc->pcmbitwidth=gs_wfInfo.pcmbitwidth;
  }

  if (gfp->num_samples==MAX_U_32_NUM) {
    struct stat sb;
#ifdef __riscos__
    _kernel_swi_regs reg;
#endif

    /* try to figure out num_samples */
#ifndef __riscos__
    if (0==stat(lpszFileName,&sb)) {
#else /* __riscos__ */
    reg.r[0]=17;
    reg.r[1]=(int) lpszFileName;
    _kernel_swi(OS_File,&reg,&reg);
    if (reg.r[0] == 1) {
      sb.st_size=reg.r[4];
#endif /* __riscos__ */

      /* try file size, assume 2 bytes per sample */
      if (gfp->input_format == sf_mp3) {
	FLOAT totalseconds = (sb.st_size*8.0/(1000.0*gfc->input_bitrate));
	gfp->num_samples= totalseconds*gfp->in_samplerate;
      }else{
	gfp->num_samples = sb.st_size/(2*gfp->num_channels);
      }
    }
  }


  return musicin;
}


/************************************************************************
*
* read_samples()
*
* PURPOSE:  reads the PCM samples from a file to the buffer
*
*  SEMANTICS:
* Reads #samples_read# number of shorts from #musicin# filepointer
* into #sample_buffer[]#.  Returns the number of samples read.
*
************************************************************************/

int read_samples_pcm(lame_global_flags *gfp,short sample_buffer[2304],int frame_size,int samples_to_read)
{
    int 		samples_read;
    int			rcode;
    lame_internal_flags *gfc=gfp->internal_flags;
    SNDFILE * gs_pSndFileIn;
    gs_pSndFileIn = (SNDFILE *)gfp->musicin;

    samples_read=sf_read_short(gs_pSndFileIn,sample_buffer,samples_to_read);
    rcode = samples_read;

    if (8==gfc->pcmbitwidth)
      for (; samples_read > 0; sample_buffer[--samples_read] *= 256);

    return(rcode);
}


#endif /* ifdef LIBSNDFILE */
#ifdef LAMESNDFILE

/************************************************************************
 ************************************************************************
 ************************************************************************
 ************************************************************************
 ************************************************************************
 ************************************************************************
 *
 * OLD ISO/LAME routines follow.  Used if you dont have LIBSNDFILE
 * or for stdin/stdout support
 *
 ************************************************************************
 ************************************************************************
 ************************************************************************
 ************************************************************************
 ************************************************************************
 ************************************************************************/

/* Replacement for forward fseek(,,SEEK_CUR), because fseek() fails on pipes */
int fskip(FILE *sf,long num_bytes,int dummy)
{
  char data[1024];
  int nskip = 0;
  while (num_bytes > 0) {
    nskip = (num_bytes>1024) ? 1024 : num_bytes;
    num_bytes -= fread(data,(size_t)1,(size_t)nskip,sf);
  }
  /* return 0 if last read was successful */
  return num_bytes;
}


/************************************************************************
*
* read_samples()
*
* PURPOSE:  reads the PCM samples from a file to the buffer
*
*  SEMANTICS:
* Reads #samples_read# number of shorts from #musicin# filepointer
* into #sample_buffer[]#.  Returns the number of samples read.
*
************************************************************************/

int read_samples_pcm(lame_global_flags *gfp,short sample_buffer[2304], int frame_size,int samples_to_read)
{
    lame_internal_flags *gfc=gfp->internal_flags;
    int samples_read;
    int iswav=(gfp->input_format==sf_wave);

    if (16==gfc->pcmbitwidth) {
      samples_read = fread(sample_buffer, 2, (unsigned int)samples_to_read, gfp->musicin);
    }else if (8==gfc->pcmbitwidth) {
      signed char temp[2304];
      int i;
      samples_read = fread(temp, 1, (unsigned int)samples_to_read, gfp->musicin);
      for (i=0 ; i<samples_read; ++i)
	sample_buffer[i]=(short int)temp[i]*256;
    }else{
      ERRORF("Only 8 and 16 bit input files supported \n");
      LAME_ERROR_EXIT();
    }
    if (ferror(gfp->musicin)) {
      ERRORF("Error reading input file\n");
      LAME_ERROR_EXIT();
    }


    /*
       Samples are big-endian. If this is a little-endian machine
       we must swap
     */
    if ( NativeByteOrder == order_unknown )
      {
	NativeByteOrder = DetermineByteOrder();
	if ( NativeByteOrder == order_unknown )
	  {
	    ERRORF("byte order not determined\n" );
	    LAME_ERROR_EXIT();
	  }
      }
    /* intel=littleEndian */
    if (!iswav && ( NativeByteOrder == order_littleEndian ))
      SwapBytesInWords( sample_buffer, samples_read );

    if (iswav && ( NativeByteOrder == order_bigEndian ))
      SwapBytesInWords( sample_buffer, samples_read );

    if (gfp->swapbytes==TRUE)
      SwapBytesInWords( sample_buffer, samples_read );


    return samples_read;
}



/* AIFF Definitions */

#define IFF_ID_FORM 0x464f524d /* "FORM" */
#define IFF_ID_AIFF 0x41494646 /* "AIFF" */
#define IFF_ID_COMM 0x434f4d4d /* "COMM" */
#define IFF_ID_SSND 0x53534e44 /* "SSND" */
#define IFF_ID_MPEG 0x4d504547 /* "MPEG" */


#define WAV_ID_RIFF 0x52494646 /* "RIFF" */
#define WAV_ID_WAVE 0x57415645 /* "WAVE" */
#define WAV_ID_FMT  0x666d7420 /* "fmt " */
#define WAV_ID_DATA 0x64617461 /* "data" */

typedef struct fmt_chunk_data_struct {
	short	format_tag;			 /* Format category */
	u_short channels;			 /* Number of channels */
	u_long	samples_per_sec;	 /* Sampling rate */
	u_long	avg_bytes_per_sec;	 /* For buffer estimation */
	u_short block_align;		 /* Data block size */
	u_short bits_per_sample;	 /* for PCM data, anyway... */
} fmt_chunk_data;







/*****************************************************************************
 *
 *	Read Microsoft Wave headers
 *
 *	By the time we get here the first 32-bits of the file have already been
 *	read, and we're pretty sure that we're looking at a WAV file.
 *
 *****************************************************************************/

static int
parse_wave_header(lame_global_flags *gfp,FILE *sf)
{
    lame_internal_flags *gfc=gfp->internal_flags;
	fmt_chunk_data wave_info;
	int is_wav = 0;
	long data_length = 0, file_length, subSize = 0;
	int loop_sanity = 0;

	memset(&wave_info, 0, sizeof(wave_info));

	file_length = Read32BitsHighLow(sf);

	if (Read32BitsHighLow(sf) != WAV_ID_WAVE)
		return 0;

	for (loop_sanity = 0; loop_sanity < 20; ++loop_sanity) {
		u_int type = Read32BitsHighLow(sf);

		if (type == WAV_ID_FMT) {
			subSize = Read32BitsLowHigh(sf);
			if (subSize < 16) {
			  /*DEBUGF(
			    "'fmt' chunk too short (only %ld bytes)!", subSize);  */
				return 0;
			}

			wave_info.format_tag		= Read16BitsLowHigh(sf);
			subSize -= 2;
			wave_info.channels			= Read16BitsLowHigh(sf);
			subSize -= 2;
			wave_info.samples_per_sec	= Read32BitsLowHigh(sf);
			subSize -= 4;
			wave_info.avg_bytes_per_sec = Read32BitsLowHigh(sf);
			subSize -= 4;
			wave_info.block_align		= Read16BitsLowHigh(sf);
			subSize -= 2;
			wave_info.bits_per_sample	= Read16BitsLowHigh(sf);
			subSize -= 2;

			/* DEBUGF("   skipping %d bytes\n", subSize); */

			if (subSize > 0) {
				if (fskip(sf, (long)subSize, SEEK_CUR) != 0 )
					return 0;
			};

		} else if (type == WAV_ID_DATA) {
			subSize = Read32BitsLowHigh(sf);
			data_length = subSize;
			is_wav = 1;
			/* We've found the audio data.	Read no further! */
			break;

		} else {
			subSize = Read32BitsLowHigh(sf);
			if (fskip(sf, (long) subSize, SEEK_CUR) != 0 ) return 0;
		}
	}

	if (is_wav) {
		/* make sure the header is sane */
		gfp->num_channels  = wave_info.channels;
		gfp->in_samplerate = wave_info.samples_per_sec;
		gfc->pcmbitwidth = wave_info.bits_per_sample;
		gfp->num_samples   = data_length / (wave_info.channels * wave_info.bits_per_sample / 8);
	}
	return is_wav;
}



/************************************************************************
*
* aiff_check
*
* PURPOSE:	Checks AIFF header information to make sure it is valid.
*			Exits if not.
*
************************************************************************/

static void
aiff_check2(const char *file_name, IFF_AIFF *pcm_aiff_data)
{
	if (pcm_aiff_data->sampleType != IFF_ID_SSND) {
	   ERRORF("Sound data is not PCM in \"%s\".\n", file_name);
	   LAME_ERROR_EXIT();
	}

	if (pcm_aiff_data->sampleSize != sizeof(short) * BITS_IN_A_BYTE) {
		ERRORF("Sound data is not %d bits in \"%s\".\n",
				(unsigned int) sizeof(short) * BITS_IN_A_BYTE, file_name);
		LAME_ERROR_EXIT();
	}

	if (pcm_aiff_data->numChannels != 1 &&
		pcm_aiff_data->numChannels != 2) {
	   ERRORF("Sound data is not mono or stereo in \"%s\".\n",
			   file_name);
	   LAME_ERROR_EXIT();
	}

	if (pcm_aiff_data->blkAlgn.blockSize != 0) {
	   ERRORF("Block size is not %d bytes in \"%s\".\n",
			   0, file_name);
	   LAME_ERROR_EXIT();
	}

	if (pcm_aiff_data->blkAlgn.offset != 0) {
	   ERRORF("Block offset is not %d bytes in \"%s\".\n",
			   0, file_name);
	   LAME_ERROR_EXIT();
	}
}

/*****************************************************************************
 *
 *	Read Audio Interchange File Format (AIFF) headers.
 *
 *	By the time we get here the first 32-bits of the file have already been
 *	read, and we're pretty sure that we're looking at an AIFF file.
 *
 *****************************************************************************/

static int
parse_aiff_header(lame_global_flags *gfp,FILE *sf)
{
    lame_internal_flags *gfc=gfp->internal_flags;
	int is_aiff = 0;
	long chunkSize = 0, subSize = 0;
	IFF_AIFF aiff_info;

	memset(&aiff_info, 0, sizeof(aiff_info));
	chunkSize = Read32BitsHighLow(sf);

	if ( Read32BitsHighLow(sf) != IFF_ID_AIFF )
		return 0;

	while ( chunkSize > 0 )
	{
		u_int type = 0;
		chunkSize -= 4;

		type = Read32BitsHighLow(sf);

		/* DEBUGF(
			"found chunk type %08x '%4.4s'\n", type, (char*)&type); */

		/* don't use a switch here to make it easier to use 'break' for SSND */
		if (type == IFF_ID_COMM) {
			subSize = Read32BitsHighLow(sf);
			chunkSize -= subSize;

			aiff_info.numChannels	  = Read16BitsHighLow(sf);
			subSize -= 2;
			aiff_info.numSampleFrames = Read32BitsHighLow(sf);
			subSize -= 4;
			aiff_info.sampleSize	  = Read16BitsHighLow(sf);
			subSize -= 2;
			aiff_info.sampleRate	  = ReadIeeeExtendedHighLow(sf);
			subSize -= 10;

			if (fskip(sf, (long) subSize, SEEK_CUR) != 0 )
				return 0;

		} else if (type == IFF_ID_SSND) {
			subSize = Read32BitsHighLow(sf);
			chunkSize -= subSize;

			aiff_info.blkAlgn.offset	= Read32BitsHighLow(sf);
			subSize -= 4;
			aiff_info.blkAlgn.blockSize = Read32BitsHighLow(sf);
			subSize -= 4;

			if (fskip(sf, (long) aiff_info.blkAlgn.offset, SEEK_CUR) != 0 )
				return 0;

			aiff_info.sampleType = IFF_ID_SSND;
			is_aiff = 1;

			/* We've found the audio data.	Read no further! */
			break;

		} else {
			subSize = Read32BitsHighLow(sf);
			chunkSize -= subSize;

			if (fskip(sf, (long) subSize, SEEK_CUR) != 0 )
				return 0;
		}
	}

	/* DEBUGF("Parsed AIFF %d\n", is_aiff); */
	if (is_aiff) {
		/* make sure the header is sane */
		aiff_check2("name", &aiff_info);
		gfp->num_channels  = aiff_info.numChannels;
		gfp->in_samplerate = aiff_info.sampleRate;
		gfc->pcmbitwidth =   aiff_info.sampleSize;
		gfp->num_samples   = aiff_info.numSampleFrames;
	}
	return is_aiff;
}



/************************************************************************
*
* parse_file_header
*
* PURPOSE: Read the header from a bytestream.  Try to determine whether
*		   it's a WAV file or AIFF without rewinding, since rewind
*		   doesn't work on pipes and there's a good chance we're reading
*		   from stdin (otherwise we'd probably be using libsndfile).
*
* When this function returns, the file offset will be positioned at the
* beginning of the sound data.
*
************************************************************************/

void parse_file_header(lame_global_flags *gfp,FILE *sf)
{
  lame_internal_flags *gfc=gfp->internal_flags;

	u_int type = 0;
	type = Read32BitsHighLow(sf);
	/*
	DEBUGF(
		"First word of input stream: %08x '%4.4s'\n", type, (char*) &type); 
	*/
	gfc->count_samples_carefully=0;
	gfp->input_format = sf_raw;

	if (type == WAV_ID_RIFF) {
		/* It's probably a WAV file */
		if (parse_wave_header(gfp,sf)) {
			gfp->input_format = sf_wave;
			gfc->count_samples_carefully=1;
		}

	} else if (type == IFF_ID_FORM) {
		/* It's probably an AIFF file */
		if (parse_aiff_header(gfp,sf)) {
			gfp->input_format = sf_aiff;
			gfc->count_samples_carefully=1;
		}
	}
	if (gfp->input_format==sf_raw) {
	  /*
	  ** Assume it's raw PCM.	 Since the audio data is assumed to begin
	  ** at byte zero, this will unfortunately require seeking.
	  */
	  if (fseek(sf, 0L, SEEK_SET) != 0) {
	    /* ignore errors */
	  }
	  gfp->input_format = sf_raw;
	}
}



void CloseSndFile(sound_file_format input,FILE * musicin)
{
  if (fclose(musicin) != 0){
    ERRORF("Could not close audio input file\n");
    LAME_FATAL_EXIT();
  }
}





FILE * OpenSndFile(lame_global_flags *gfp)
{
  lame_internal_flags *gfc=gfp->internal_flags;
  const char* inPath = gfp->inPath;
  FILE * musicin;
  struct stat sb;

  /* set the defaults from info incase we cannot determine them from file */
  gfp->num_samples=MAX_U_32_NUM;
  gfc->input_bitrate=0;


  if (!strcmp(inPath, "-")) {
    /* Read from standard input. */
#ifdef __EMX__
    _fsetmode(stdin,"b");
#elif (defined  __BORLANDC__)
    setmode(_fileno(stdin), O_BINARY);
#elif (defined  __CYGWIN__)
    setmode(fileno(stdin), _O_BINARY);
#elif (defined _WIN32)
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    musicin = stdin;
  } else {
    if ((musicin = fopen(inPath, "rb")) == NULL) {
      ERRORF("Could not find \"%s\".\n", inPath);
      LAME_ERROR_EXIT();
    }
  }

  if (gfp->input_format==sf_mp3) {
    mp3data_struct mp3data;
#ifdef AMIGA_MPEGA
    if (-1==lame_decode_initfile(inPath,&mp3data)) {
      ERRORF("Error reading headers in mp3 input file %s.\n", inPath);
      LAME_ERROR_EXIT();
    }
#endif
#ifdef HAVEMPGLIB
    if (-1==lame_decode_initfile(musicin,&mp3data)) {
      ERRORF("Error reading headers in mp3 input file %s.\n", inPath);
      LAME_ERROR_EXIT();
    }
#endif
    gfp->num_channels=mp3data.stereo;
    gfp->in_samplerate=mp3data.samplerate;
    gfc->input_bitrate=mp3data.bitrate;
    gfp->num_samples=mp3data.nsamp;
  }else if (gfp->input_format==sf_ogg) {
#ifdef HAVEVORBIS
    mp3data_struct mp3data;
    if (-1==lame_decode_ogg_initfile(musicin,&mp3data)) {
      ERRORF("Error reading headers in ogg input file %s.\n", inPath);
      LAME_ERROR_EXIT();
    }
    gfp->num_channels=mp3data.stereo;
    gfp->in_samplerate=mp3data.samplerate;
    gfc->input_bitrate=mp3data.bitrate;
    gfp->num_samples=mp3data.nsamp;
#else
    ERRORF("LAME not compiled with libvorbis support.\n");
    LAME_ERROR_EXIT();
#endif
 }else{
   if (gfp->input_format != sf_raw) {
     parse_file_header(gfp,musicin);
   }

   if (gfp->input_format==sf_raw) {
     /* assume raw PCM */
     MSGF("Assuming raw pcm input file");
     if (gfp->swapbytes==TRUE)
       MSGF(" : Forcing byte-swapping\n");
     else
       MSGF("\n");
   }
 }

  if (gfp->num_samples==MAX_U_32_NUM && musicin != stdin) {
#ifdef __riscos__
    _kernel_swi_regs reg;
#endif

    /* try to figure out num_samples */
#ifndef __riscos__
    if (0==stat(inPath,&sb)) {
#else
    reg.r[0]=17;
    reg.r[1]=(int) inPath;
    _kernel_swi(OS_File,&reg,&reg);
    if (reg.r[0] == 1) {
      sb.st_size=reg.r[4];
#endif

      /* try file size, assume 2 bytes per sample */
      if (gfp->input_format == sf_mp3) {
	if (gfc->input_bitrate>0) {
	  FLOAT totalseconds = (sb.st_size*8.0/(1000.0*gfc->input_bitrate));
	  gfp->num_samples= totalseconds*gfp->in_samplerate;
	}
      }else{
	gfp->num_samples = sb.st_size/(2*gfp->num_channels);
      }
    }
  }
  return musicin;
}
#endif  /* LAMESNDFILE */


