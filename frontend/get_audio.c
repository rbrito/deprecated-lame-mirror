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
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include "lame.h"
#include "main.h"
#include "get_audio.h"
#include "portableio.h"
#include "timestatus.h"


#ifdef _WIN32
/* needed to set stdin to binary on windoze machines */
  #include <io.h>
  #include <fcntl.h>
#else
  #include <unistd.h>
#endif

#ifdef __riscos__
#include <kernel.h>
#include <sys/swis.h>
#endif


/* global data for get_audio.c. */
int count_samples_carefully;
int pcmbitwidth;
mp3data_struct mp3input_data;
unsigned int num_samples_read;             
FILE *musicin;
enum byte_order NativeByteOrder = order_unknown;


#ifdef AMIGA_MPEGA
int lame_decode_initfile(char *fullname,mp3data_struct *mp3data);
#else
int lame_decode_initfile(FILE *fd,mp3data_struct *mp3data);
#endif

/* read mp3 file until mpglib returns one frame of PCM data */
int lame_decode_fromfile(FILE *fd,short int pcm_l[],short int pcm_r[],mp3data_struct *mp3data);

/* and for Vorbis: */
int lame_decode_ogg_initfile(FILE *fd,mp3data_struct *mp3data);
int lame_decode_ogg_fromfile(FILE *fd,short int pcm_l[],short int pcm_r[],mp3data_struct *mp3data);


static int read_samples_pcm(FILE *musicin,short sample_buffer[2304],int frame_size, int samples_to_read);
static int read_samples_mp3(lame_global_flags *gfp,FILE *musicin,short int mpg123pcm[2][1152],int num_chan);
static int read_samples_ogg(lame_global_flags *gfp,FILE *musicin,short int mpg123pcm[2][1152],int num_chan);
void CloseSndFile(sound_file_format input, FILE *musicin);
FILE * OpenSndFile(lame_global_flags *gfp,char *);


/* Replacement for forward fseek(,,SEEK_CUR), because fseek() fails on pipes */

/*///
 *  Changes:
 *    - optimized to use the advised PIPE_BUF as buffer size
 *    - don't hang if fread can't read any further data
 *    - works exactly in the way of fseek (and KLEMM_06 enables the usage of fseek, if available, may be a other seekable test is better???)
 *    - error handling for wrong cases returning a -1
 *    - now at the top of the file, so you don't need to prototype it
 *    - static (all function not defined in the *.h file should be static)
 */


static int  fskip ( FILE* fp, long offset, int whence )
{
#ifndef PIPE_BUF
    char    buffer [    4096];
#else
    char    buffer [PIPE_BUF];
#endif
    size_t  read;
    
#ifdef KLEMM_06
    if ( 0 == fseek ( fp, offset, whence ) )
        return 0;
#endif	
    
    if ( whence != SEEK_CUR  ||  offset < 0 ) {
        fprintf ( stderr, "fskip problem: Mostly the return status of functions is not evaluate so it is more secure to polute <stderr>.\n" );
        return -1;
    }

    while ( offset > 0 ) {
        read      = offset > sizeof(buffer)  ?  sizeof(buffer)  :  offset;
        if ((read = fread ( buffer, (size_t)1, read, fp )) <= 0)
	    return -1;
        offset   -= read;
    }

    return 0;
}


FILE *init_outfile(char *outPath)
{
  FILE *outf;
  /* open the output file */
  if (!strcmp(outPath, "-")) {
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
    if ((outf = fopen(outPath, "wb+")) == NULL) {
      fprintf(stderr,"Could not create \"%s\".\n", outPath);
      exit(-1);
    }
#ifdef __riscos__
    /* Assign correct file type */
    for (i = 0; outPath[i]; i++) {
      if (outPath[i] == '.')
        outPath[i] = '/';
      else if (outPath[i] == '/')
        outPath[i] = '.';
    }
    if (gf.decode_only)
      SetFiletype(outPath, 0xfb1); /* Wave */
    else
      SetFiletype(outPath, 0x1ad); /* AMPEG */
#endif
  }
  return outf;
}






void init_infile(lame_global_flags *gfp, char *inPath)
{
  /* open the input file */
  count_samples_carefully=0;
  pcmbitwidth=16;
  musicin=OpenSndFile(gfp,inPath);
}

void close_infile(void)
{
  CloseSndFile(input_format, musicin);
}

#ifdef KLEMM_10
void SwapBytesInWords ( short* ptr, int words )
{
    int  val;
    while ( words-- > 0 ) {
        val    = *ptr;
        *ptr++ = ((val<<8) & 0xFF00) | ((val>>8) & 0x00FF);
    }
}
#else
void SwapBytesInWords ( short* loc, int words )
{
    int i;
    short thisval;
    char *dst, *src;
    src = (char *) &thisval;
    for ( i = 0; i < words; i++ )
    {
        thisval = *loc;
        dst = (char *) loc++;
        dst[0] = src[1];
        dst[1] = src[0];
    }
}
#endif


/*****************************************************************************
*
*  Routines to determine byte order and swap bytes
*
*****************************************************************************/

enum byte_order DetermineByteOrder(void)
{
    char s[ sizeof(long) + 1 ];
    union
    {
        long longval;
        char charval[ sizeof(long) ];
    } probe;
    probe.longval = 0x41424344L;  /* ABCD in ASCII */
    strncpy( s, probe.charval, sizeof(long) );
    s[ sizeof(long) ] = '\0';
    /* fprintf( stderr, "byte order is %s\n", s ); */
    if ( strcmp(s, "ABCD") == 0 )
        return order_bigEndian;
    else
        if ( strcmp(s, "DCBA") == 0 )
            return order_littleEndian;
        else
            return order_unknown;
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
  short* p;
  int samples_read;
  int framesize,samples_to_read;
  unsigned int remaining;
  int num_channels = gfp->num_channels;

  /* NOTE: LAME can now handle arbritray size input data packets,
   * so there is no reason to read the input data in chuncks of
   * size "gfp->framesize".  EXCEPT:  the LAME graphical frame analyzer 
   * will get out of sync if we read more than framesize worth of data.
   */
    samples_to_read  = framesize  = gfp->framesize;
    assert (framesize <= 1152);

    /* if this flag has been set, then we are carefull to read
     * exactly num_samples and no more.  This is usefull for .wav and .aiff
     * files which have id3 or other tags at the end.  Note that if you
     * are using LIBSNDFILE, this is not necessary 
     */
    if ( count_samples_carefully ) {
        remaining = gfp->num_samples - Min (gfp->num_samples, num_samples_read);
        if (remaining < (unsigned int)framesize)
            samples_to_read = remaining;
    }

    switch (input_format) {
    case sf_mp1:
    case sf_mp2:
    case sf_mp3:
        samples_read  = read_samples_mp3 ( gfp, musicin, buffer, num_channels );
	break;
    case sf_ogg:
        samples_read  = read_samples_ogg ( gfp, musicin, buffer, num_channels );
	break;
    default:
        samples_read  = read_samples_pcm (      musicin, insamp, num_channels*framesize, num_channels*samples_to_read );
        samples_read /= num_channels;

        p = insamp;
        switch (num_channels) {
        case 1:
            for ( j = 0; j < framesize; j++ ) {
                buffer [0] [j] = *p++;
                buffer [1] [j] = 0;
            }
            break;
        case 2:
            for ( j = 0; j < framesize; j++ ) {
                buffer [0] [j] = *p++;
                buffer [1] [j] = *p++;
            }
            break;
        default:
	    assert (0);
	    break;
        }
    }

    /* if num_samples = MAX_U_32_NUM, then it is considered infinitely long. Don't count the samples */
    if ( gfp->num_samples != MAX_U_32_NUM) 
        num_samples_read += samples_read;
	
    return samples_read;
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
    fprintf(stderr,"Error: number of channels has changed in mp3 file - not supported. \n");
  }
  if (gfp->in_samplerate != mp3input_data.samplerate) {
    fprintf(stderr,"Error: samplerate has changed in mp3 file - not supported. \n");
  }


  return out;
#else
  return -1; /* wanna read ogg without vorbis support? */
#endif
}


int read_samples_mp3(lame_global_flags *gfp,FILE *musicin,short int mpg123pcm[2][1152],int stereo)
{
#if (defined  AMIGA_MPEGA || defined HAVEMPGLIB)
  int out;

  out=lame_decode_fromfile(musicin,mpg123pcm[0],mpg123pcm[1],&mp3input_data);
  /* out = -1:  error, probably EOF */
  /* out = 0:   not possible with lame_decode_fromfile() */
  /* out = number of output samples */

  if ( -1 == out ) {
    memset ( mpg123pcm, 0, sizeof(**mpg123pcm)*1152*2 );
    return 0;
  }

  if (gfp->num_channels != mp3input_data.stereo) {
    fprintf(stderr,"Error: number of channels has changed in mp3 file - not supported. \n");
  }
  if (gfp->in_samplerate != mp3input_data.samplerate) {
    fprintf(stderr,"Error: samplerate has changed in mp3 file - not supported. \n");
  }
  return out;
#else
  return -1;
#endif
}


static int   WriteWaveHeader ( FILE* const fp, const long pcmbytes, const long double sample_freq, const int channels, const int bits )
{
    int  freq  = floor ( sample_freq + 0.5 );
    int  bytes = (bits + 7) / 8;
    
    /* quick and dirty, but documented */
    fwrite             ( "RIFF", 1, 4, fp );		// label
    Write32BitsLowHigh ( fp, pcmbytes + 44 - 8 );	// length in bytes without header
    fwrite             ( "WAVEfmt ", 2, 4, fp );        // 2 labels
    Write32BitsLowHigh ( fp, 2+2+4+4+2+2 );             // length of PCM format declaration area
    Write16BitsLowHigh ( fp, 1 );			// is PCM?
    Write16BitsLowHigh ( fp, channels );		// number of channels
    Write32BitsLowHigh ( fp, freq );			// sample frequency in [Hz]
    Write32BitsLowHigh ( fp, freq * channels * bytes );	// bytes per second
    Write16BitsLowHigh ( fp, channels * bytes );	// bytes per sample time
    Write16BitsLowHigh ( fp, bits );			// bits per sample
    fwrite             ( "data", 1, 4, fp );		// label
    Write32BitsLowHigh ( fp, pcmbytes );		// length in bytes of raw PCM data
    
    return ferror(fp)  ?  -1  :  0;
}

/* the simple lame decoder */
/* After calling lame_init(), lame_init_params() and
 * init_infile(), call this routine to read the input MP3 file
 * and output .wav data to the specified file pointer*/
/* lame_decoder will ignore the first 528 samples, since these samples
 * represent the mpglib delay (and are all 0).  skip = number of additional
 * samples to skip, to (for example) compensate for the encoder delay */

int lame_decoder(lame_global_flags *gfp, FILE *outf,int skip, char *inPath, char *outPath)
{
    short int   Buffer [2] [1152];
    int         iread;
    double      wavsize;
    int         i;
    void        (*WriteFunction) (FILE* fp, char *p, int n);



    fprintf(stderr, "\rinput:  %s%s(%g kHz, %i channel%s, ", 
           strcmp (inPath, "-")  ?  inPath  :  "<stdin>",
	   strlen (inPath) > 26  ? "\n\t" : "  ",
	   gfp->in_samplerate / 1.e3,
	   gfp->num_channels, gfp->num_channels != 1 ? "s" : "" );

    switch ( input_format ) {
    case sf_mp3:
	skip += 528+1;   /* mp3 decoder has a 528 sample delay, plus user supplied "skip" */
	fprintf(stderr, "MPEG-%u%s Layer %.*s", 2 - gfp->version, gfp->out_samplerate < 16000 ? ".5" : "",
	       3, "III" );
	break;
    case sf_mp2:
	skip += 240+1;  
	fprintf(stderr, "MPEG-%u%s Layer %.*s", 2 - gfp->version, gfp->out_samplerate < 16000 ? ".5" : "",
	       2, "III" );
	break;
    case sf_mp1:
	skip += 240+1;  
	fprintf(stderr, "MPEG-%u%s Layer %.*s", 2 - gfp->version, gfp->out_samplerate < 16000 ? ".5" : "",
	       1, "III" );
	break;
    case sf_ogg:
        fprintf(stderr, "Ogg Vorbis");
	skip = 0;	/* other formats have no delay */ /* is += 0 not better ??? */
	break;
    case sf_raw:
        fprintf(stderr, "raw PCM data");
	mp3input_data.nsamp = gfp->num_samples;
	mp3input_data.framesize = 1152;
	skip = 0;	/* other formats have no delay */ /* is += 0 not better ??? */
	break;
    case sf_wave:
        fprintf(stderr, "Microsoft WAVE");
	mp3input_data.nsamp = gfp->num_samples;
	mp3input_data.framesize = 1152;
	skip = 0;	/* other formats have no delay */ /* is += 0 not better ??? */
	break;
    case sf_aiff:
        fprintf(stderr, "SGI/Apple AIFF");
	mp3input_data.nsamp = gfp->num_samples;
	mp3input_data.framesize = 1152;
	skip = 0;	/* other formats have no delay */ /* is += 0 not better ??? */
	break;
    default:
        fprintf(stderr, "unknown");
	mp3input_data.nsamp = gfp->num_samples;
	mp3input_data.framesize = 1152;
	skip = 0;	/* other formats have no delay */ /* is += 0 not better ??? */
	assert (0);
	break;
    }
    
    fprintf(stderr,  ")\noutput: %s%s(16 bit, Microsoft WAVE)\n",
           strcmp (outPath, "-")  ?  outPath  :  "<stdout>",
	   strlen (outPath) > 45  ? "\n\t" : "  " );
	   
    if ( skip > 0 )
    	fprintf(stderr, "skipping initial %i samples (encoder+decoder delay)\n", skip );
	
    if ( ! gfp -> disable_waveheader )
    	WriteWaveHeader ( outf, 0x7FFFFFFF, gfp->in_samplerate, gfp->num_channels, 16 );
    	/* unknown size, so write maximum 32 bit signed value */
    	
    wavsize       = -skip;
    WriteFunction = swapbytes  ?  WriteBytesSwapped  :  WriteBytes;
    mp3input_data.totalframes = 
      mp3input_data.nsamp/mp3input_data.framesize;
    
    assert ( gfp->num_channels >= 1 && gfp->num_channels <= 2 );
    
    do {
	iread=get_audio(gfp,Buffer);  /* read in 'iread' samples */
	mp3input_data.framenum += iread/mp3input_data.framesize;
	wavsize += iread;
	if (!silent)
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
    if (wavsize <= 0) {
        fprintf(stderr, "WAVE file contains 0 PCM samples\n");
        wavsize = 0;
    } else if ( wavsize > 0xFFFFFFD0/i ) {
        fprintf(stderr, "Very huge WAVE file, can't set filesize accordingly\n");
	wavsize = 0xFFFFFFD0;
    } else {
        wavsize *= i;
    }
  
    if ( ! gfp->disable_waveheader )
	if ( ! fseek (outf, 0l, SEEK_SET) ) /* if outf is seekable, rewind and adjust length */
            WriteWaveHeader (outf, (unsigned long)wavsize, gfp->in_samplerate, gfp->num_channels, 16 );
    fclose (outf);

    decoder_progress_finish (gfp);
    return 0;
}






#if defined(LIBSNDFILE)

#if 0    /* currently disabled */
# include "sndfile.h"   // prototype for sf_get_lib_version()
void print_sndlib_version(FILE *fp)
{
    char  tmp [80];
    sf_get_lib_version ( tmp, sizeof (tmp) );
    fprintf ( fp, "Input handled by %s  (http://www.zip.com.au/~erikd/libsndfile/)\n", tmp );
}
#endif

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
      fprintf(stderr,"Could not close audio input file\n");
      exit(2);
    }
#endif
  }else{
	if (gs_pSndFileIn)
	{
		if (sf_close(gs_pSndFileIn) !=0)
		{
			fprintf(stderr,"Could not close sound file \n");
			exit(2);
		}
	}
  }
}



FILE * OpenSndFile(lame_global_flags *gfp, char *inPath)
{
  char* lpszFileName = inPath;
  FILE * musicin;
  SNDFILE *gs_pSndFileIn;
  SF_INFO gs_wfInfo;

  if (input_format==sf_mp1 ||
      input_format==sf_mp2 ||
      input_format==sf_mp3) {
#ifdef AMIGA_MPEGA
    if (-1==lame_decode_initfile(lpszFileName,&mp3input_data)) {
      fprintf(stderr,"Error reading headers in mp3 input file %s.\n", lpszFileName);
      exit(1);
    }
#endif
#ifdef HAVEMPGLIB
    if ((musicin = fopen(lpszFileName, "rb")) == NULL) {
	  fprintf(stderr,"Could not find \"%s\".\n", lpszFileName);
	  exit(1);
    }
    if (-1==lame_decode_initfile(musicin,&mp3input_data)) {
	  fprintf(stderr,"Error reading headers in mp3 input file %s.\n", lpszFileName);
	  exit(1);
    }
#endif

    gfp->num_channels=mp3input_data.stereo;
    gfp->in_samplerate=mp3input_data.samplerate;
    gfp->num_samples=mp3input_data.nsamp;
  }else if (input_format==sf_ogg) {
#ifdef HAVEVORBIS
    if ((musicin = fopen(lpszFileName, "rb")) == NULL) {
	  fprintf(stderr,"Could not find \"%s\".\n", lpszFileName);
	  exit(1);
    }
    if (-1==lame_decode_ogg_initfile(musicin,&mp3input_data)) {
	  fprintf(stderr,"Error reading headers in mp3 input file %s.\n", lpszFileName);
	  exit(1);
	}
    gfp->num_channels=0;
    gfp->in_samplerate=0;
    gfp->num_samples=0;
#else
    fprintf(stderr,"LAME not compiled with libvorbis support.\n");
    exit(1);
#endif


  } else {

    /* Try to open the sound file */
    /* set some defaults incase input is raw PCM */
    gs_wfInfo.seekable=(input_format!=sf_raw);  /* if user specified -r, set to not seekable */
    gs_wfInfo.samplerate=gfp->in_samplerate;
    gs_wfInfo.pcmbitwidth=16;
    gs_wfInfo.channels=gfp->num_channels;
    if (DetermineByteOrder()==order_littleEndian) {
      if (swapbytes) gs_wfInfo.format=SF_FORMAT_RAW_BE;
      else gs_wfInfo.format=SF_FORMAT_RAW_LE;
    } else {
      if (swapbytes) gs_wfInfo.format=SF_FORMAT_RAW_LE;
      else gs_wfInfo.format=SF_FORMAT_RAW_BE;
    }

    gs_pSndFileIn=sf_open_read(lpszFileName,&gs_wfInfo);
    musicin = (SNDFILE *) gs_pSndFileIn;

        /* Check result */
	if (gs_pSndFileIn==NULL)
	{
	        sf_perror(gs_pSndFileIn);
		fprintf(stderr,"Could not open sound file \"%s\".\n", lpszFileName);
		exit(1);
	}

    if ((gs_wfInfo.format==SF_FORMAT_RAW_LE) ||
	(gs_wfInfo.format==SF_FORMAT_RAW_BE))
      input_format=sf_raw;

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
      if (input_format == sf_mp1 ||
          input_format == sf_mp2 ||
          input_format == sf_mp3) {
	double totalseconds = (sb.st_size*8.0/(1000.0*mp3input_data.bitrate));
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

static int  read_samples_pcm ( FILE* const musicin, short sample_buffer [2304], int frame_size /* unused */, int samples_to_read )
{
    int  i;
    int  samples_read;

    samples_read = sf_read_short ( (SNDFILE*)musicin, sample_buffer, samples_to_read );

    switch (pcmbitwidth) {
    case  8:
        for ( i = 0; i < samples_read; i++ )
            sample_buffer [i] <<= 8;
	break;
    case 16:
        break;
    default:
        assert (0);
        break;
    }

    return sample_read;
}


#else  /* defined(LIBSNDFILE) */

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

int read_samples_pcm(FILE *musicin,short sample_buffer[2304], int frame_size,int samples_to_read)
{
    int samples_read;
    int iswav=(input_format==sf_wave);

    if (16==pcmbitwidth) {
      samples_read = fread(sample_buffer, 2, (unsigned int)samples_to_read, musicin);
    }else if (8==pcmbitwidth) {
      char temp[2304];
      int i;
      samples_read = fread(temp, 1, (unsigned int)samples_to_read, musicin);
      for (i=0 ; i<samples_read; ++i) {
	/* note: 8bit .wav samples are unsigned */
	sample_buffer[i]=((short int)temp[i]-127)*256;
      }
    }else{
      fprintf(stderr,"Only 8 and 16 bit input files supported \n");
      exit(1);
    }
    if (ferror(musicin)) {
      fprintf(stderr,"Error reading input file\n");
      exit(1);
    }



    if (16==pcmbitwidth) {
      if ( NativeByteOrder == order_unknown )
	{
	  NativeByteOrder = DetermineByteOrder();
	  if ( NativeByteOrder == order_unknown )
	    {
	      fprintf(stderr,"byte order not determined\n" );
	      exit(1);
	    }
	}
      /* intel=littleEndian */
      if (!iswav && ( NativeByteOrder == order_littleEndian ))
	SwapBytesInWords( sample_buffer, samples_read );
      
      if (iswav && ( NativeByteOrder == order_bigEndian ))
	SwapBytesInWords( sample_buffer, samples_read );
      
      if (swapbytes)
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
aiff_check2(char *file_name, IFF_AIFF *pcm_aiff_data)
{
	if (pcm_aiff_data->sampleType != IFF_ID_SSND) {
	   fprintf(stderr,"Sound data is not PCM in \"%s\".\n", file_name);
	   exit(1);
	}

#define         BITS_IN_A_BYTE          8
	if (pcm_aiff_data->sampleSize != sizeof(short) * BITS_IN_A_BYTE) {
		fprintf(stderr,"Sound data is not %d bits in \"%s\".\n",
				(unsigned int) sizeof(short) * BITS_IN_A_BYTE, file_name);
		exit(1);
	}

	if (pcm_aiff_data->numChannels != 1 &&
		pcm_aiff_data->numChannels != 2) {
	   fprintf(stderr,"Sound data is not mono or stereo in \"%s\".\n",
			   file_name);
	   exit(1);
	}

	if (pcm_aiff_data->blkAlgn.blockSize != 0) {
	   fprintf(stderr,"Block size is not %d bytes in \"%s\".\n",
			   0, file_name);
	   exit(1);
	}

	if (pcm_aiff_data->blkAlgn.offset != 0) {
	   fprintf(stderr,"Block offset is not %d bytes in \"%s\".\n",
			   0, file_name);
	   exit(1);
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
	input_format = sf_raw;

	if (type == WAV_ID_RIFF) {
		/* It's probably a WAV file */
		if (parse_wave_header(gfp,sf)) {
			input_format = sf_wave;
			count_samples_carefully=1;
		}

	} else if (type == IFF_ID_FORM) {
		/* It's probably an AIFF file */
		if (parse_aiff_header(gfp,sf)) {
			input_format = sf_aiff;
			count_samples_carefully=1;
		}
	}
	if (input_format==sf_raw) {
	  /*
	  ** Assume it's raw PCM.	 Since the audio data is assumed to begin
	  ** at byte zero, this will unfortunately require seeking.
	  */
	  if (fseek(sf, 0L, SEEK_SET) != 0) {
	    /* ignore errors */
	  }
	  input_format = sf_raw;
	}
}



void CloseSndFile(sound_file_format input,FILE * musicin)
{
  if (fclose(musicin) != 0){
    fprintf(stderr,"Could not close audio input file\n");
    exit(2);
  }
}





FILE * OpenSndFile(lame_global_flags *gfp, char *inPath)
{
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
      fprintf(stderr,"Could not find \"%s\".\n", inPath);
      exit(1);
    }
  }

  if (input_format==sf_mp1 ||
      input_format==sf_mp2 ||
      input_format==sf_mp3) {
#ifdef AMIGA_MPEGA
    if (-1==lame_decode_initfile(inPath,&mp3input_data)) {
      fprintf(stderr,"Error reading headers in mp3 input file %s.\n", inPath);
      exit(1);
    }
#endif
#ifdef HAVEMPGLIB
    if (-1==lame_decode_initfile(musicin,&mp3input_data)) {
      fprintf(stderr,"Error reading headers in mp3 input file %s.\n", inPath);
      exit(1);
    }
#endif
    gfp->num_channels=mp3input_data.stereo;
    gfp->in_samplerate=mp3input_data.samplerate;
    gfp->num_samples=mp3input_data.nsamp;
  }else if (input_format==sf_ogg) {
#ifdef HAVEVORBIS
    if (-1==lame_decode_ogg_initfile(musicin,&mp3input_data)) {
      fprintf(stderr,"Error reading headers in ogg input file %s.\n", inPath);
      exit(1);
    }
    gfp->num_channels=mp3input_data.stereo;
    gfp->in_samplerate=mp3input_data.samplerate;
    gfp->num_samples=mp3input_data.nsamp;
#else
    fprintf(stderr,"LAME not compiled with libvorbis support.\n");
    exit(1);
#endif
 }else{
   if (input_format != sf_raw) {
     parse_file_header(gfp,musicin);
   }

   if (input_format==sf_raw) {
     /* assume raw PCM */
     fprintf(stderr,"Assuming raw pcm input file");
     if (swapbytes)
       fprintf(stderr," : Forcing byte-swapping\n");
     else
       fprintf(stderr,"\n");
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
      if (input_format == sf_mp1 ||
          input_format == sf_mp2 ||
          input_format == sf_mp3) {
	if (mp3input_data.bitrate>0) {
	  double totalseconds = (sb.st_size*8.0/(1000.0*mp3input_data.bitrate));
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
#endif  /* defined(LIBSNDFILE) */





#if defined(HAVEMPGLIB)
static int check_aid ( const char* header ) 
{
    return  0 == strncmp ( header, "AiD\1", 4 );
}

/*
 * Please check this and don't kill me if there's a bug
 * This is a (nearly?) complete header analysis for a MPEG-1/2/2.5 Layer I, II or III
 * data stream
 */

static int is_syncword_mp123 ( const void* const headerptr )
{
    const unsigned char* const  p         = headerptr;
    static const char           abl2 [16] = { 0, 7, 7, 7, 0, 7, 0, 0, 0, 0, 0, 8, 8, 8, 8, 8 };
    
    if ( (p[0] & 0xFF) != 0xFF ) return 0;   // first 8 bits must be '1'
    if ( (p[1] & 0xE0) != 0xE0 ) return 0;   // next 3 bits are also
    if ( (p[1] & 0x18) == 0x08 ) return 0;   // no MPEG-1, -2 or -2.5
    if ( (p[1] & 0x06) == 0x00 ) return 0;   // no Layer I, II and III
    if ( (p[2] & 0xF0) == 0xF0 ) return 0;   // bad bitrate
    if ( (p[2] & 0x0C) == 0x0C ) return 0;   // no sample frequency with (32,44.1,48)/(1,2,4)    
    if ( (p[1] & 0x06) == 0x04 )             // illegal Layer II bitrate/Channel Mode comb
        if ( abl2 [p[2] >> 4]  &  (1 << (p[3] >> 6)) )
            return 0;
    return 1;
}

static int is_syncword_mp3 ( const void* const headerptr )
{
    const unsigned char* const  p = headerptr;
    
    if ( (p[0] & 0xFF) != 0xFF ) return 0;   // first 8 bits must be '1'
    if ( (p[1] & 0xE0) != 0xE0 ) return 0;   // next 3 bits are also
    if ( (p[1] & 0x18) == 0x08 ) return 0;   // no MPEG-1, -2 or -2.5
    if ( (p[1] & 0x06) != 0x02 ) return 0;   // no Layer III (can be merged with 'next 3 bits are also' test, but don't do this, this decreases readability)
    if ( (p[2] & 0xF0) == 0xF0 ) return 0;   // bad bitrate
    if ( (p[2] & 0x0C) == 0x0C ) return 0;   // no sample frequency with (32,44.1,48)/(1,2,4)    
    return 1;
}


int lame_decode_initfile(FILE *fd, mp3data_struct *mp3data)
{
  VBRTAGDATA pTagData;
  char buf[1000];
  int ret;
  int num_frames=0;
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
    fskip ( fd, aid_header-6, SEEK_CUR );

    /* read 2 more bytes to set up buffer for MP3 header check */
    if (fread(&buf,1,2,fd) == 0) return -1;  /* failed */
    len =2;
  }



  /* look for sync word  FFF */
  if (len<4) return -1;                /* is_sync_word no checks the next 4 byte, call for fix this line */
  while (!is_syncword_mp123(buf)) {
    int i;
    for (i=0; i<len-1; i++)
      buf[i]=buf[i+1]; 
    if (fread(&buf[len-1],1,1,fd) == 0) return -1;  /* failed */
  }


  
  /* read the rest of header and enough bytes to check for Xing header */
  len2 = fread(&buf[len],1,48-len,fd);
  if (len2 ==0 ) return -1;            // what happens when only 1 byte is read ???
  len +=len2;


  /* check first 48 bytes for Xing header */
  xing_header = GetVbrTag(&pTagData,(unsigned char*)buf);

  if (xing_header && pTagData.headersize >= 48) {
    num_frames=pTagData.frames;
    fprintf(stderr,"\rXing VBR header dectected.  MP3 file has %i frames\n",num_frames);
    
    fskip ( fd, pTagData.headersize-48 ,SEEK_CUR );
    
    /* 
     *  pfk: This is a fix, please check this. The is_sync now needs all 4 bytes !!! 
     *  This code can be wrong !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     */
    
    /* look for next sync word in buffer*/
    len = 4;
    buf[0] = buf[1] = buf[2] = buf[3] = 0;
    
    while ( !is_syncword_mp123(buf) ) {
        buf[0] = buf[1];
        buf[1] = buf[2];
        buf[2] = buf[3];
        if (fread ( buf+3, 1, 1, fd ) != 1 )
            return -1;  /* fread failed */
    }
    /* read the rest of header */
    // len2 = fread(&buf[2],1,2,fd);
    // if (len2 ==0 ) return -1;
    // wrong, if ( len != 2) is right
    // len +=len2;


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
#endif /* defined(HAVEMPGLIB) */

/* end of get_audio.c */
