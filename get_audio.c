/*
 *	Get Audio routines source file
 *
 *	Copyright (c) 1999 Albert L Faber
 *
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
#include <assert.h>
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



/* global data for get_audio.c.  NOTE: get_audio.c is going
 * to be removed from the lame encoding library.  It will only
 * be part of the LAME front end, and thus does not need to be thread
 * safe */

unsigned long num_samples_read;  
int count_samples_carefully;
int pcmbitwidth;
mp3data_struct mp3input_data;





/* read mp3 file until mpglib returns one frame of PCM data */
#ifdef AMIGA_MPEGA
int lame_decode_initfile(const char *fullname,mp3data_struct *mp3data);
int lame_decode_fromfile(FILE *fd,short int pcm_l[], short int pcm_r[],mp3data_struct *mp3data);
#else
int lame_decode_initfile(FILE *fd,mp3data_struct *mp3data);
int lame_decode_fromfile(FILE *fd,short int pcm_l[],short int pcm_r[],mp3data_struct *mp3data);
#endif

/* and for Vorbis: */
int lame_decode_ogg_initfile(FILE *fd,mp3data_struct *mp3data);
int lame_decode_ogg_fromfile(FILE *fd,short int pcm_l[],short int pcm_r[],mp3data_struct *mp3data);

/* the simple lame decoder (interface to above routines) */
/* After calling lame_init(), lame_init_params() and
 * lame_init_infile(), call this routine to read the input MP3 file 
 * and output .wav data to the specified file pointer*/
/* lame_decoder will ignore the first 528 samples, since these samples
 * represent the mpglib delay (and are all 0).  skip = number of additional
 * samples to skip, to (for example) compensate for the encoder delay,
 * only used when decoding mp3 */
int lame_decoder(lame_global_flags *gfp,FILE *outf,int skip);



int read_samples_pcm(lame_global_flags *gfp,short sample_buffer[2304],int frame_size, int samples_to_read);
int read_samples_mp3(lame_global_flags *gfp,FILE *musicin,short int mpg123pcm[2][1152],int num_chan);
int read_samples_ogg(lame_global_flags *gfp,FILE *musicin,short int mpg123pcm[2][1152],int num_chan);


void lame_init_infile(lame_global_flags *gfp)
{
  /* open the input file */
  count_samples_carefully=0;
  pcmbitwidth=16;
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

  /* note: if input is gfp->stereo and output is mono, get_audio()
   * will return  .5*(L+R) in channel 0,  and nothing in channel 1. */
  iread = get_audio(gfp,Buffer);

  /* check to see if we overestimated/underestimated totalframes */
  if (iread==0)  gfp->totalframes = Min(gfp->totalframes,gfp->frameNum+2);
  if (gfp->frameNum > (gfp->totalframes-1)) gfp->totalframes = gfp->frameNum;

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
int get_audio(lame_global_flags *gfp,short buffer[2][1152])
{

  int		j;
  short	insamp[2304];
  int samples_read;
  int framesize,samples_to_read;
  unsigned long remaining;
  int num_channels = gfp->num_channels;

  /* NOTE: LAME can now handle arbritray size input data packets,
   * so there is no reason to read the input data in chuncks of
   * size "gfp->framesize".  EXCEPT:  the LAME graphical frame analyzer 
   * will get out of sync if we read more than framesize worth of data.
   */
  framesize = gfp->framesize;

  samples_to_read = framesize;
  if (count_samples_carefully) {
    /* if this flag has been set, then we are carefull to read
     * exactly num_samples and no more.  This is usefull for .wav and .aiff
     * files which have id3 or other tags at the end.  Note that if you
     * are using LIBSNDFILE, this is not necessary */
    remaining=gfp->num_samples-Min(gfp->num_samples,num_samples_read);
    if (remaining < (unsigned long)framesize)
      samples_to_read = remaining;
  }


  if (gfp->input_format==sf_mp1 ||
      gfp->input_format==sf_mp2 ||
      gfp->input_format==sf_mp3) {
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

  /* if num_samples = MAX_U_32_NUM, then it is considered
   * infinitely long.  Dont count the samples */
  if (gfp->num_samples!=MAX_U_32_NUM) num_samples_read += samples_read;
  return(samples_read);

}


  



int read_samples_ogg(lame_global_flags *gfp,FILE *musicin,short int mpg123pcm[2][1152],int stereo)
{
#ifdef HAVEVORBIS
  int out=0;

  out=lame_decode_ogg_fromfile(musicin,mpg123pcm[0],mpg123pcm[1],&mp3input_data);
  /* out = -1:  error, probably EOF */
  /* out = 0:   not possible with lame_decode_fromfile() */
  /* out = number of output samples */

    if ( -1 == out ) {
	memset ( mpg123pcm, 0, sizeof(**mpg123pcm)*1152*2 );
	return 0;
    }

  if (gfp->num_channels != mp3input_data.stereo) {
    ERRORF("Error: number of channels has changed in mp3 file - not supported. \n");
  }
  if (gfp->in_samplerate != mp3input_data.samplerate) {
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
  int out=0;

  out=lame_decode_fromfile(musicin,mpg123pcm[0],mpg123pcm[1],&mp3input_data);
  /* out = -1:  error, probably EOF */
  /* out = 0:   not possible with lame_decode_fromfile() */
  /* out = number of output samples */

  if ( -1 == out )
    memset ( mpg123pcm, 0, sizeof(**mpg123pcm)*1152*2 );
  if (out==-1) return 0;

  if (gfp->num_channels != mp3input_data.stereo) {
    ERRORF("Error: number of channels has changed in mp3 file - not supported. \n");
  }
  if (gfp->in_samplerate != mp3input_data.samplerate) {
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
    short int   Buffer [2] [1152];
    int         iread;
    double      wavsize;
    int         i;
    void        (*WriteFunction) (FILE* fp, char *p, int n);

    MSGF ( "\rinput:  %s%s(%g kHz, %i channel%s, ", 
           strcmp (gfp->inPath, "-")  ?  gfp->inPath  :  "<stdin>",
	   strlen (gfp->inPath) > 26  ? "\n\t" : "  ",
	   gfp->in_samplerate / 1.e3,
	   gfp->num_channels, gfp->num_channels != 1 ? "s" : "" );

    switch ( gfp -> input_format ) {
    case sf_mp3:
	skip += 528+1;   /* mp3 decoder has a 528 sample delay, plus user supplied "skip" */
	MSGF ( "MPEG-%u%s Layer %.*s", 2 - gfp->version, gfp->out_samplerate < 16000 ? ".5" : "",
	       3, "III" );
	break;
    case sf_mp2:
	skip += 240+1;  
	MSGF ( "MPEG-%u%s Layer %.*s", 2 - gfp->version, gfp->out_samplerate < 16000 ? ".5" : "",
	       2, "III" );
	break;
    case sf_mp1:
	skip += 240+1;  
	MSGF ( "MPEG-%u%s Layer %.*s", 2 - gfp->version, gfp->out_samplerate < 16000 ? ".5" : "",
	       1, "III" );
	break;
    case sf_ogg:
        MSGF ("Ogg Vorbis");
	skip = 0;	/* other formats have no delay */ /* is += 0 not better ??? */
	break;
    case sf_raw:
        MSGF ("raw PCM data");
	skip = 0;	/* other formats have no delay */ /* is += 0 not better ??? */
	break;
    case sf_wave:
        MSGF ("Microsoft WAVE");
	skip = 0;	/* other formats have no delay */ /* is += 0 not better ??? */
	break;
    case sf_aiff:
        MSGF ("SGI/Apple AIFF");
	skip = 0;	/* other formats have no delay */ /* is += 0 not better ??? */
	break;
    default:
        MSGF ("unknown");
	skip = 0;	/* other formats have no delay */ /* is += 0 not better ??? */
	assert (0);
	break;
    }
    
    MSGF ( ")\noutput: %s%s(16 bit, Microsoft WAVE)\n",
           strcmp (gfp->outPath, "-")  ?  gfp->outPath  :  "<stdout>",
	   strlen (gfp->outPath) > 45  ? "\n\t" : "  " );
	   
    if ( skip > 0 )
    	MSGF ("skipping initial %i samples (encoder+decoder delay)\n", skip );
	
    if ( ! gfp -> disable_waveheader )
    	WriteWav ( outf, 0x7FFFFFFF, gfp->in_samplerate, gfp->num_channels );
    	/* unknown size, so write maximum 32 bit signed value */
    	
    wavsize       = -skip;
    WriteFunction = gfp->swapbytes  ?  WriteBytesSwapped  :  WriteBytes;
    mp3input_data.totalframes = mp3input_data.nsamp/mp3input_data.framesize;
    
    assert ( gfp->num_channels >= 1 && gfp->num_channels <= 2 );
    
    do {
	iread    = lame_readframe (gfp, Buffer);  /* read in 'iread' samples */
	mp3input_data.framenum += iread/mp3input_data.framesize;
	wavsize += iread;
	if (!gfp->silent)
            decoder_progress(gfp,&mp3input_data);
	    
	skip -= (i = skip<iread ? skip : iread);  /* 'i' samples are to skip in this frame */

        for ( ; i < iread; i++ ) {
    	    if ( gfp -> disable_waveheader ) {
        	WriteFunction (outf,(char*)Buffer[0]+i,sizeof(short));
        	if (gfp->num_channels==2) WriteFunction (outf,(char*)Buffer[1]+i,sizeof(short));
            } else {
        	Write16BitsLowHigh (outf,Buffer[0][i]);
                if (gfp->num_channels==2) Write16BitsLowHigh (outf,Buffer[1][i]);
            }
        }
    } while ( iread );

    i = (16/8) * gfp->num_channels;
    assert ( i > 0 );
    if (wavsize < 0) {
        MSGF ("WAVE file contains 0 PCM samples\n");
        wavsize = 0;
    } else if ( wavsize > 0xFFFFFFC0/i ) {
        MSGF ("Very huge WAVE file, can't set filesize accordingly\n");
	wavsize = 0xFFFFFFC0;
    } else {
        wavsize *= i;
    }
  
    if ( ! gfp->disable_waveheader )
	if ( ! fseek (outf, 0l, SEEK_SET) ) /* if outf is seekable, rewind and adjust length */
            WriteWav (outf, (unsigned long)wavsize, gfp->in_samplerate, gfp->num_channels );
    fclose (outf);

    decoder_progress_finish (gfp);
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






void CloseSndFile(sound_file_format input,FILE *musicin)
{
  SNDFILE *gs_pSndFileIn=(SNDFILE*)musicin;
  if (input==sf_mp1 || input==sf_mp2 || input==sf_mp3) {
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
  const char* lpszFileName = gfp->inPath;
  FILE * musicin;
  SNDFILE *gs_pSndFileIn;
  SF_INFO gs_wfInfo;

  if (gfp->input_format==sf_mp1 ||
      gfp->input_format==sf_mp2 ||
      gfp->input_format==sf_mp3) {
#ifdef AMIGA_MPEGA
    if (-1==lame_decode_initfile(lpszFileName,&mp3input_data)) {
      ERRORF("Error reading headers in mp3 input file %s.\n", lpszFileName);
      LAME_ERROR_EXIT();
    }
#endif
#ifdef HAVEMPGLIB
    if ((musicin = fopen(lpszFileName, "rb")) == NULL) {
	  ERRORF("Could not find \"%s\".\n", lpszFileName);
	  LAME_ERROR_EXIT();
    }
    if (-1==lame_decode_initfile(musicin,&mp3input_data)) {
	  ERRORF("Error reading headers in mp3 input file %s.\n", lpszFileName);
	  LAME_ERROR_EXIT();
	}
#endif

    gfp->num_channels=mp3input_data.stereo;
    gfp->in_samplerate=mp3input_data.samplerate;
    gfp->num_samples=mp3input_data.nsamp;
  }else if (gfp->input_format==sf_ogg) {
#ifdef HAVEVORBIS
    if ((musicin = fopen(lpszFileName, "rb")) == NULL) {
	  ERRORF("Could not find \"%s\".\n", lpszFileName);
	  LAME_ERROR_EXIT();
    }
    if (-1==lame_decode_ogg_initfile(musicin,&mp3input_data)) {
	  ERRORF("Error reading headers in mp3 input file %s.\n", lpszFileName);
	  LAME_ERROR_EXIT();
	}
    gfp->num_channels=0;
    gfp->in_samplerate=0;
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
	
	switch ( gs_wfInfo.format & SF_FORMAT_TYPEMASK ) {
	case SF_FORMAT_WAV:  DEBUGF ("Microsoft WAV format (big endian). "    ); break;
	case SF_FORMAT_AIFF: DEBUGF ("Apple/SGI AIFF format (little endian). "); break;
	case SF_FORMAT_AU:   DEBUGF ("Sun/NeXT AU format (big endian). "      ); break;
	case SF_FORMAT_AULE: DEBUGF ("DEC AU format (little endian). "        ); break;
	case SF_FORMAT_RAW:  DEBUGF ("RAW PCM data. "                         ); break;
	case SF_FORMAT_PAF:  DEBUGF ("Ensoniq PARIS file format. "            ); break;
	case SF_FORMAT_SVX:  DEBUGF ("Amiga IFF / SVX8 / SV16 format. "       ); break;
	case SF_FORMAT_NIST: DEBUGF ("Sphere NIST format. "                   ); break;
	default:             assert (0);                                         break;
	}

        switch ( gs_wfInfo.format & SF_FORMAT_SUBMASK ) {
	case SF_FORMAT_PCM: 	  DEBUGF ("PCM data in 8, 16, 24 or 32 bits."); break;
	case SF_FORMAT_FLOAT: 	  DEBUGF ("32 bit Intel x86 floats."         ); break;
	case SF_FORMAT_ULAW: 	  DEBUGF ("U-Law encoded."                   ); break;
	case SF_FORMAT_ALAW:	  DEBUGF ("A-Law encoded."                   ); break;
	case SF_FORMAT_IMA_ADPCM: DEBUGF ("IMA ADPCM."                       ); break;
	case SF_FORMAT_MS_ADPCM:  DEBUGF ("Microsoft ADPCM."                 ); break;
	case SF_FORMAT_PCM_BE: 	  DEBUGF ("Big endian PCM data."             ); break;
	case SF_FORMAT_PCM_LE: 	  DEBUGF ("Little endian PCM data."          ); break;
	case SF_FORMAT_PCM_S8: 	  DEBUGF ("Signed 8 bit PCM."                ); break;
	case SF_FORMAT_PCM_U8: 	  DEBUGF ("Unsigned 8 bit PCM."              ); break;
	case SF_FORMAT_SVX_FIB:   DEBUGF ("SVX Fibonacci Delta encoding."    ); break;
	case SF_FORMAT_SVX_EXP:   DEBUGF ("SVX Exponential Delta encoding."  ); break;
	default:                  assert (0);                                   break;
	}

	DEBUGF("\n");
	DEBUGF("pcmbitwidth       :%d\n",gs_wfInfo.pcmbitwidth);
	DEBUGF("sections          :%d\n",gs_wfInfo.sections);
	DEBUGF("seekable          :\n",gs_wfInfo.seekable);
#endif

    gfp->num_samples = gs_wfInfo.samples;
    gfp->num_channels = gs_wfInfo.channels;
    gfp->in_samplerate = gs_wfInfo.samplerate;
    pcmbitwidth=gs_wfInfo.pcmbitwidth;
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
      if (gfp->input_format == sf_mp1 ||
          gfp->input_format == sf_mp2 ||
          gfp->input_format == sf_mp3) {
	FLOAT totalseconds = (sb.st_size*8.0/(1000.0*mp3input_data.bitrate));
	gfp->num_samples= totalseconds*gfp->in_samplerate;
	mp3input_data.nsamp = gfp->num_samples;
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
    SNDFILE * gs_pSndFileIn;
    gs_pSndFileIn = (SNDFILE *)gfp->musicin;

    samples_read=sf_read_short(gs_pSndFileIn,sample_buffer,samples_to_read);
    rcode = samples_read;

    if (8==pcmbitwidth)
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
    int samples_read;
    int iswav=(gfp->input_format==sf_wave);

    if (16==pcmbitwidth) {
      samples_read = fread(sample_buffer, 2, (unsigned int)samples_to_read, gfp->musicin);
    }else if (8==pcmbitwidth) {
      unsigned char temp[2304];
      int i;
      samples_read = fread(temp, 1, (unsigned int)samples_to_read, gfp->musicin);
      for (i=0 ; i<samples_read; ++i) {
	/* note: 8bit .wav samples are unsigned */
	sample_buffer[i]=((short int)temp[i]-127)*256;
      }
    }else{
      ERRORF("Only 8 and 16 bit input files supported \n");
      LAME_ERROR_EXIT();
    }
    if (ferror(gfp->musicin)) {
      ERRORF("Error reading input file\n");
      LAME_ERROR_EXIT();
    }



    if (16==pcmbitwidth) {
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
    }

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
        int format_tag=0;
        int channels=0;
	int block_align=0;
	int bits_per_sample=0;
        unsigned int samples_per_sec=0;
        unsigned int avg_bytes_per_sec=0;
	

	int is_wav = 0;
	long data_length = 0, file_length, subSize = 0;
	int loop_sanity = 0;

	file_length = Read32BitsHighLow(sf);

	if (Read32BitsHighLow(sf) != WAV_ID_WAVE)
		return 0;

	for (loop_sanity = 0; loop_sanity < 20; ++loop_sanity) {
		int type = Read32BitsHighLow(sf);

		if (type == WAV_ID_FMT) {
			subSize = Read32BitsLowHigh(sf);
			if (subSize < 16) {
			  /*DEBUGF(
			    "'fmt' chunk too short (only %ld bytes)!", subSize);  */
				return 0;
			}

			format_tag		= Read16BitsLowHigh(sf);
			subSize -= 2;
			channels			= Read16BitsLowHigh(sf);
			subSize -= 2;
			samples_per_sec	= Read32BitsLowHigh(sf);
			subSize -= 4;
			avg_bytes_per_sec = Read32BitsLowHigh(sf);
			subSize -= 4;
			block_align		= Read16BitsLowHigh(sf);
			subSize -= 2;
			bits_per_sample	= Read16BitsLowHigh(sf);
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
		gfp->num_channels  = channels;
		gfp->in_samplerate = samples_per_sec;
		pcmbitwidth = bits_per_sample;
		gfp->num_samples   = data_length / (channels * bits_per_sample / 8);
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
	int is_aiff = 0;
	long chunkSize = 0, subSize = 0;
	IFF_AIFF aiff_info;

	memset(&aiff_info, 0, sizeof(aiff_info));
	chunkSize = Read32BitsHighLow(sf);

	if ( Read32BitsHighLow(sf) != IFF_ID_AIFF )
		return 0;

	while ( chunkSize > 0 )
	{
		int type = Read32BitsHighLow(sf);
		chunkSize -= 4;

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
		pcmbitwidth =   aiff_info.sampleSize;
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

	int type = Read32BitsHighLow(sf);
	/*
	DEBUGF(
		"First word of input stream: %08x '%4.4s'\n", type, (char*) &type); 
	*/
	count_samples_carefully=0;
	gfp->input_format = sf_raw;

	if (type == WAV_ID_RIFF) {
		/* It's probably a WAV file */
		if (parse_wave_header(gfp,sf)) {
			gfp->input_format = sf_wave;
			count_samples_carefully=1;
		}

	} else if (type == IFF_ID_FORM) {
		/* It's probably an AIFF file */
		if (parse_aiff_header(gfp,sf)) {
			gfp->input_format = sf_aiff;
			count_samples_carefully=1;
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
  const char* inPath = gfp->inPath;
  FILE * musicin;
  struct stat sb;

  /* set the defaults from info incase we cannot determine them from file */
  gfp->num_samples=MAX_U_32_NUM;


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

  if (gfp->input_format==sf_mp1 ||
      gfp->input_format==sf_mp2 ||
      gfp->input_format==sf_mp3) {
#ifdef AMIGA_MPEGA
    if (-1==lame_decode_initfile(inPath,&mp3input_data)) {
      ERRORF("Error reading headers in mp3 input file %s.\n", inPath);
      LAME_ERROR_EXIT();
    }
#endif
#ifdef HAVEMPGLIB
    if (-1==lame_decode_initfile(musicin,&mp3input_data)) {
      ERRORF("Error reading headers in mp3 input file %s.\n", inPath);
      LAME_ERROR_EXIT();
    }
#endif
    gfp->num_channels=mp3input_data.stereo;
    gfp->in_samplerate=mp3input_data.samplerate;
    gfp->num_samples=mp3input_data.nsamp;
  }else if (gfp->input_format==sf_ogg) {
#ifdef HAVEVORBIS
    if (-1==lame_decode_ogg_initfile(musicin,&mp3input_data)) {
      ERRORF("Error reading headers in ogg input file %s.\n", inPath);
      LAME_ERROR_EXIT();
    }
    gfp->num_channels=mp3input_data.stereo;
    gfp->in_samplerate=mp3input_data.samplerate;
    gfp->num_samples=mp3input_data.nsamp;
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
      if (gfp->input_format == sf_mp1 ||
          gfp->input_format == sf_mp2 ||
          gfp->input_format == sf_mp3) {
	if (mp3input_data.bitrate>0) {
	  FLOAT totalseconds = (sb.st_size*8.0/(1000.0*mp3input_data.bitrate));
	  gfp->num_samples= totalseconds*gfp->in_samplerate;
	  mp3input_data.nsamp = gfp->num_samples;
	}
      }else{
	gfp->num_samples = sb.st_size/(2*gfp->num_channels);
      }
    }
  }
  return musicin;
}
#endif  /* LAMESNDFILE */





#ifdef HAVEMPGLIB
int check_aid(char *header) {
  int aid_header =
    (header[0]=='A' && header[1]=='i' && header[2]=='D'
     && header[3]== (char) 1);
  return aid_header;
}
int is_syncword2(char *header)
{
  int mpeg1=((int) ( header[0] == (char) 0xFF)) &&
    ((int) ( (header[1] & (char) 0xF0) == (char) 0xF0));
  
  int mpeg25=((int) ( header[0] == (char) 0xFF)) &&
    ((int) ( (header[1] & (char) 0xF0) == (char) 0xE0));
  
  return (mpeg1 || mpeg25);
}


int lame_decode_initfile(FILE *fd, mp3data_struct *mp3data)
{
#include "VbrTag.h"
  VBRTAGDATA pTagData;
  char buf[1000];
  int ret;
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
  while (!is_syncword2(buf)) {
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
  ret = lame_decode1_headers(buf,len,pcm_l,pcm_r,mp3data);
  if (-1==ret) return -1;
  if (ret>0 && !xing_header) 
    fprintf(stderr,"Oops1: first frame of mpglib output will be lost \n"); 

  /* repeat until we decode a valid mp3 header */
  while (!mp3data->header_parsed) {
    len = fread(buf,1,2,fd);
    if (len ==0 ) return -1;
    ret = lame_decode1_headers(buf,len,pcm_l,pcm_r,mp3data);
    if (-1==ret) return -1;

    if (ret>0 && !xing_header)
      fprintf(stderr,"Oops2: first frame of mpglib output will be lost \n"); 
  }

  mp3data->nsamp=MAX_U_32_NUM;
  if (xing_header && num_frames) {
    mp3data->nsamp=mp3data->framesize * num_frames;
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
  char buf[100];
  /* read until we get a valid output frame */
  while(0==ret) {
    len = fread(buf,1,100,fd);
    if (len ==0 ) return -1;
    ret = lame_decode1_headers(buf,len,pcm_l,pcm_r,mp3data);
    if (ret == -1) return -1;
  }
  return ret;
}
#endif
