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

/* $Id$ */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>

#ifdef HAVE_LIMITS_H
# include <limits.h>
#endif

#include <stdio.h>

#ifdef STDC_HEADERS
# include <stdlib.h>
# include <string.h>
#else
# ifndef HAVE_MEMCPY
#  define memcpy(d, s, n) bcopy ((s), (d), (n))
# endif
#endif

#define         MAX_U_32_NUM            0xFFFFFFFF

#include <sys/stat.h>

#if defined(_WIN32) || defined(__CYGWIN__)
# include <io.h>
# include <fcntl.h>
#else
# include <unistd.h>
#endif

#if defined(_WIN32)
# include <sys/types.h>
#endif

#include "lame.h"
#include "main.h"
#include "get_audio.h"
#include "portableio.h"
#include "timestatus.h"

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif


/* global data for get_audio.c. */
static unsigned int num_samples_read;
static int count_samples_carefully;
static int pcmbitwidth;
static int pcmswapbytes = 0;
FILE   *musicin;
int id3v2taglen = 0;


#if defined(HAVE_MPGLIB)
static int decode_initfile(lame_t gfp, FILE * fd, mp3data_struct * mp3data);
/* read mp3 file until mpglib returns one frame of PCM data */
static int     decode_fromfile(lame_t gfp,
			       FILE * fd, short int pcm_l[], short int pcm_r[],
			       mp3data_struct * mp3data);
#endif

static int read_samples_pcm(FILE * musicin, int sample_buffer[2304],
                            int samples_to_read);
static int read_samples_mp3(lame_t gfp, FILE * const musicin,
                            short int mpg123pcm[2][1152]);
static FILE   *OpenSndFile(lame_t gfp, char *);


/* Replacement for forward fseek(,,SEEK_CUR), because fseek() fails on pipes */


static int
fskip(FILE * fp, long offset, int whence)
{
#ifndef PIPE_BUF
# define PIPE_BUF 4096
#endif
    char    buffer[PIPE_BUF];
    int     read;

    if (fseek(fp, offset, whence) == 0)
	return 0;

    if (whence != SEEK_CUR || offset < 0) {
	fprintf(stderr,
		"fskip problem: Mostly the return status of functions is not evaluate so it is more secure to polute <stderr>.\n");
	return -1;
    }

    while (offset > 0) {
        read = offset;
	if (read > PIPE_BUF)
	    read = PIPE_BUF;
        if ((read = fread(buffer, 1, read, fp)) <= 0)
            return -1;
        offset -= read;
    }

    return 0;
}


#if   defined __EMX__
# define set_stream_binary_mode(fp)     _fsetmode ( fp, "b" )
#elif defined __BORLANDC__
# define set_stream_binary_mode(fp)     setmode   (_fileno(fp),  O_BINARY )
#elif defined __CYGWIN__
# define set_stream_binary_mode(fp)     setmode   ( fileno(fp), _O_BINARY )
#elif defined _WIN32
# define set_stream_binary_mode(fp)     _setmode  (_fileno(fp), _O_BINARY )
#else
# define set_stream_binary_mode(fp)
#endif

FILE *
init_outfile(char *outPath)
{
    /* open the output file */
    if (0 == strcmp(outPath, "-")) {
	set_stream_binary_mode(stdout);
	return stdout;
    }

    return fopen(outPath, "wb+");
}

void
init_infile(lame_t gfp, char *inPath)
{
    /* open the input file */
    count_samples_carefully = 0;
    num_samples_read=0;
    pcmbitwidth=in_bitwidth;
    pcmswapbytes=swapbytes;
    musicin = OpenSndFile(gfp, inPath);
}

static off_t
get_file_size(const char* const filename)
{
    struct stat       sb;

    if ( 0 == stat ( filename, &sb ) )
        return sb.st_size;
    return (off_t) -1;
}

/************************************************************************
 *
 * get_audio()
 *
 * PURPOSE:  reads a frame of audio data from a file to the buffer,
 *   aligns the data for future processing, and separates the
 *   left and right channels
 *
 *    in: gfp
 *        buffer    output to the int buffer or 16-bit buffer
 * returns: samples read
 */
int
get_audio(lame_t gfp, int buffer[2][1152])
{
    int num_channels = lame_get_num_channels(gfp), samples_read;
    unsigned int tmp_num_samples = lame_get_num_samples(gfp);
    unsigned int samples_to_read = lame_get_framesize(gfp);
    unsigned int remaining;
    int     insamp[2 * 1152];
    int     i, j;
    short   buf_tmp16[2][1152];

    assert(samples_to_read <= 1152);

    /* 
     * NOTE: LAME can now handle arbritray size input data packets,
     * so there is no reason to read the input data in chuncks of
     * size "framesize".  EXCEPT:  the LAME graphical frame analyzer 
     * will get out of sync if we read more than framesize worth of data.
     */

    /* if this flag has been set, then we are carefull to read
     * exactly num_samples and no more.  This is useful for .wav and .aiff
     * files which have id3 or other tags at the end.  Note that if you
     * are using LIBSNDFILE, this is not necessary 
     */
    if (count_samples_carefully) {
	/* in case the input is a FIFO (at least it's reproducible with
	   a FIFO) tmp_num_samples may be 0 and therefore remaining
	   would be 0, but we need to read some samples, so don't
	   change samples_to_read to the wrong value in this case */
	remaining = tmp_num_samples - Min(tmp_num_samples, num_samples_read);
	if (samples_to_read > remaining && tmp_num_samples)
	    samples_to_read = remaining;
    }

    switch (input_format) {
    case sf_mp1:
    case sf_mp2:
    case sf_mp3:
	samples_read = read_samples_mp3(gfp, musicin, buf_tmp16);
	/* LAME mp3 output 16bit -  convert to int */
	for (j = 0; j < num_channels; j++) {
	    for (i = samples_read; --i >= 0; )
		buffer[j][i] = buf_tmp16[j][i] << (8 * sizeof(int) - 16);
	}
	if( num_channels == 1 )
	    memset(buffer[1], 0, samples_read * sizeof(int));
	break;

    default:
	samples_read
	    = read_samples_pcm(musicin, insamp, num_channels * samples_to_read)
	    / num_channels;
	if (num_channels == 2) {
	    for (i = 0; i < samples_read; i++) {
		buffer[0][i] = insamp[i*2  ];
		buffer[1][i] = insamp[i*2+1];
	    }
	} else if (num_channels == 1) {
	    memcpy(buffer[0], insamp, samples_read * sizeof(int));
	    memset(buffer[1], 0,      samples_read * sizeof(int));
	} else
	    assert(0);
    }

    /* if num_samples = MAX_U_32_NUM, then it is considered infinitely long.
       Don't count the samples */
    if (tmp_num_samples != MAX_U_32_NUM)
	num_samples_read += samples_read;

    return samples_read;
}



static int
read_samples_mp3(lame_t gfp, FILE * const musicin,
		 short int mpg123pcm[2][1152])
{
    int     out;
#if defined(HAVE_MPGLIB)
    static const char type_name[] = "MP3 file";

    out = decode_fromfile(gfp, musicin, mpg123pcm[0], mpg123pcm[1], &mp3input_data);
    /*
     * out < 0:  error, probably EOF
     * out = 0:  not possible with decode_fromfile() ???
     * out > 0:  number of output samples
     */
    if (out < 0) {
        memset(mpg123pcm, 0, sizeof(**mpg123pcm) * 2 * 1152);
        return 0;
    }

    if ( lame_get_num_channels(gfp) != mp3input_data.channels)
        fprintf(stderr,
                "Error: number of channels has changed in %s - not supported\n",
                type_name);
    if ( lame_get_in_samplerate(gfp) != mp3input_data.samplerate )
        fprintf(stderr,
                "Error: sample frequency has changed in %s - not supported\n",
                type_name);

#else
    out = -1;
#endif
    return out;
}


int
WriteWaveHeader(FILE * const fp, const int pcmbytes,
                const int freq, const int channels, const int bits)
{
    int     bytes = (bits + 7) / 8;

    /* quick and dirty, but documented */
    fwrite("RIFF", 1, 4, fp); /* label */
    Write32BitsLowHigh(fp, pcmbytes + 44 - 8); /* length in bytes without header */
    fwrite("WAVEfmt ", 2, 4, fp); /* 2 labels */
    Write32BitsLowHigh(fp, 2 + 2 + 4 + 4 + 2 + 2); /* length of PCM format declaration area */
    Write16BitsLowHigh(fp, 1); /* is PCM? */
    Write16BitsLowHigh(fp, channels); /* number of channels */
    Write32BitsLowHigh(fp, freq); /* sample frequency in [Hz] */
    Write32BitsLowHigh(fp, freq * channels * bytes); /* bytes per second */
    Write16BitsLowHigh(fp, channels * bytes); /* bytes per sample time */
    Write16BitsLowHigh(fp, bits); /* bits per sample */
    fwrite("data", 1, 4, fp); /* label */
    Write32BitsLowHigh(fp, pcmbytes); /* length in bytes of raw PCM data */

    return ferror(fp) ? -1 : 0;
}




#if defined(LIBSNDFILE)

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

void
close_infile(void)
{
    SNDFILE *gs_pSndFileIn = (SNDFILE *) musicin;
    if (input == sf_mp1 || input == sf_mp2 || input == sf_mp3) {
        if (fclose(musicin) != 0) {
            fprintf(stderr, "Could not close audio input file\n");
            exit(2);
        }
    }
    else {
        if (gs_pSndFileIn) {
            if (sf_close(gs_pSndFileIn) != 0) {
                fprintf(stderr, "Could not close sound file \n");
                exit(2);
            }
        }
    }
}



static FILE   *
OpenSndFile(lame_t gfp, char *inPath)
{
    char   *lpszFileName = inPath;
    FILE   *musicin;
    SNDFILE *gs_pSndFileIn;
    SF_INFO gs_wfInfo;

    if (input_format == sf_mp1 ||
        input_format == sf_mp2 || input_format == sf_mp3) {
#ifdef HAVE_MPGLIB
        if ((musicin = fopen(lpszFileName, "rb")) == NULL) {
            fprintf(stderr, "Could not find \"%s\".\n", lpszFileName);
            exit(1);
        }
        if (-1 == decode_initfile(gfp, musicin, &mp3input_data)) {
            fprintf(stderr, "Error reading headers in mp3 input file %s.\n",
                    lpszFileName);
            exit(1);
        }
#endif

        if( -1 == lame_set_num_channels(gfp, mp3input_data.channels) ) {
            fprintf( stderr,
                     "Unsupported number of channels: %ud\n",
                     mp3input_data.channels);
            exit( 1 );
        }
        lame_set_in_samplerate(gfp, mp3input_data.samplerate );
        lame_set_num_samples(gfp, mp3input_data.nsamp );
    }
    else {

        /* Try to open the sound file */
        /* set some defaults incase input is raw PCM */
        gs_wfInfo.seekable = (input_format != sf_raw); /* if user specified -r, set to not seekable */
        gs_wfInfo.samplerate = lame_get_in_samplerate(gfp);
        gs_wfInfo.pcmbitwidth = in_bitwidth;
        gs_wfInfo.channels = lame_get_num_channels(gfp);

	if (in_bitwidth == 8) {
	    if (in_signed)
		gs_wfInfo.format = SF_FORMAT_RAW_S8;
	    else
		gs_wfInfo.format = SF_FORMAT_RAW_U8;

	} else {
	    if (!in_signed) {
		fputs("Unsigned input only supported with bitwidth 8\n", stderr);
		exit(1);
	    }
	    if (in_endian != order_unknown) {
		if (in_endian == order_littleEndian)
		    gs_wfInfo.format = SF_FORMAT_RAW_LE;
		else
		    gs_wfInfo.format = SF_FORMAT_RAW_BE;
	    } else {
#ifndef WORDS_BIGENDIAN
		/* little endian */
		if (swapbytes)
		    gs_wfInfo.format = SF_FORMAT_RAW_BE;
		else
		    gs_wfInfo.format = SF_FORMAT_RAW_LE;
#else
		if (swapbytes)
		    gs_wfInfo.format = SF_FORMAT_RAW_LE;
		else
		    gs_wfInfo.format = SF_FORMAT_RAW_BE;
#endif
	    }
	}

        gs_pSndFileIn = sf_open_read(lpszFileName, &gs_wfInfo);
        musicin = (SNDFILE *) gs_pSndFileIn;

        /* Check result */
        if (gs_pSndFileIn == NULL) {
            sf_perror(gs_pSndFileIn);
            fprintf(stderr, "Could not open sound file \"%s\".\n",
                    lpszFileName);
            exit(1);
        }

        if ((gs_wfInfo.format == SF_FORMAT_RAW_LE) ||
            (gs_wfInfo.format == SF_FORMAT_RAW_BE) ||
	    (gs_wfInfo.format == SF_FORMAT_RAW_S8) ||
	    (gs_wfInfo.format == SF_FORMAT_RAW_U8))
	    input_format = sf_raw;

#ifdef _DEBUG_SND_FILE
        DEBUGF("\n\nSF_INFO structure\n");
        DEBUGF("samplerate        :%d\n", gs_wfInfo.samplerate);
        DEBUGF("samples           :%d\n", gs_wfInfo.samples);
        DEBUGF("channels          :%d\n", gs_wfInfo.channels);
        DEBUGF("pcmbitwidth       :%d\n", gs_wfInfo.pcmbitwidth);
        DEBUGF("format            :");

        /* new formats from sbellon@sbellon.de  1/2000 */

        switch (gs_wfInfo.format & SF_FORMAT_TYPEMASK) {
        case SF_FORMAT_WAV:
            DEBUGF("Microsoft WAV format (big endian). ");
            break;
        case SF_FORMAT_AIFF:
            DEBUGF("Apple/SGI AIFF format (little endian). ");
            break;
        case SF_FORMAT_AU:
            DEBUGF("Sun/NeXT AU format (big endian). ");
            break;
        case SF_FORMAT_AULE:
            DEBUGF("DEC AU format (little endian). ");
            break;
        case SF_FORMAT_RAW:
            DEBUGF("RAW PCM data. ");
            break;
        case SF_FORMAT_PAF:
            DEBUGF("Ensoniq PARIS file format. ");
            break;
        case SF_FORMAT_SVX:
            DEBUGF("Amiga IFF / SVX8 / SV16 format. ");
            break;
        case SF_FORMAT_NIST:
            DEBUGF("Sphere NIST format. ");
            break;
        default:
            assert(0);
            break;
        }

        switch (gs_wfInfo.format & SF_FORMAT_SUBMASK) {
        case SF_FORMAT_PCM:
            DEBUGF("PCM data in 8, 16, 24 or 32 bits.");
            break;
        case SF_FORMAT_FLOAT:
            DEBUGF("32 bit Intel x86 floats.");
            break;
        case SF_FORMAT_ULAW:
            DEBUGF("U-Law encoded.");
            break;
        case SF_FORMAT_ALAW:
            DEBUGF("A-Law encoded.");
            break;
        case SF_FORMAT_IMA_ADPCM:
            DEBUGF("IMA ADPCM.");
            break;
        case SF_FORMAT_MS_ADPCM:
            DEBUGF("Microsoft ADPCM.");
            break;
        case SF_FORMAT_PCM_BE:
            DEBUGF("Big endian PCM data.");
            break;
        case SF_FORMAT_PCM_LE:
            DEBUGF("Little endian PCM data.");
            break;
        case SF_FORMAT_PCM_S8:
            DEBUGF("Signed 8 bit PCM.");
            break;
        case SF_FORMAT_PCM_U8:
            DEBUGF("Unsigned 8 bit PCM.");
            break;
        case SF_FORMAT_SVX_FIB:
            DEBUGF("SVX Fibonacci Delta encoding.");
            break;
        case SF_FORMAT_SVX_EXP:
            DEBUGF("SVX Exponential Delta encoding.");
            break;
        default:
            assert(0);
            break;
        }

        DEBUGF("\n");
        DEBUGF("pcmbitwidth       :%d\n", gs_wfInfo.pcmbitwidth);
        DEBUGF("sections          :%d\n", gs_wfInfo.sections);
        DEBUGF("seekable          :\n", gs_wfInfo.seekable);
#endif

        lame_set_num_samples(gfp, gs_wfInfo.samples );
        if( -1 == lame_set_num_channels(gfp, gs_wfInfo.channels ) ) {
            fprintf( stderr,
                     "Unsupported number of channels: %ud\n",
                     gs_wfInfo.channels );
            exit( 1 );
        }
        lame_set_in_samplerate(gfp, gs_wfInfo.samplerate );
        pcmbitwidth = gs_wfInfo.pcmbitwidth;
    }

    if (lame_get_num_samples(gfp) == MAX_U_32_NUM) {
        /* try to figure out num_samples */
        double  flen = get_file_size( lpszFileName );

        if (flen >= 0) {
            /* try file size, assume 2 bytes per sample */
            if (input_format == sf_mp1 ||
                input_format == sf_mp2 || input_format == sf_mp3) {
		if (mp3input_data.bitrate>0) {
		    double  totalseconds =
			(flen * 8.0 / (1000.0 * mp3input_data.bitrate));
		    unsigned long tmp_num_samples =
			totalseconds * lame_get_in_samplerate(gfp);
		    
		    lame_set_num_samples(gfp, tmp_num_samples );
		    mp3input_data.nsamp = tmp_num_samples;
		}
            }
	    else {
		lame_set_num_samples(
		    gfp, flen/(pcmbitwidth/8 * lame_get_num_channels(gfp)));
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

static int
read_samples_pcm(FILE * const musicin, int sample_buffer[2304],
                 int samples_to_read)
{
    int     i;
    int     samples_read;

    samples_read =
        sf_read_int((SNDFILE *) musicin, sample_buffer, samples_to_read);

    switch (pcmbitwidth) {
    case 8:
        for (i = 0; i < samples_read; i++)
            sample_buffer[i] <<= (8 * sizeof(int) - 8);
        break;
    case 16:
        for (i = 0; i < samples_read; i++)
            sample_buffer[i] <<= (8 * sizeof(int) - 16);
        break;
    case 24:
        for (i = 0; i < samples_read; i++)
            sample_buffer[i] <<= (8 * sizeof(int) - 24);
	break;
    case 32:
	break;
    default:
        fprintf(stderr, "Only 8, 16, 24 and 32 bit input files supported \n");
        exit(1);
    }

    return samples_read;
}


#else /* defined(LIBSNDFILE) */

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
unpack_read_samples - read and unpack signed low-to-high byte or unsigned
                      single byte input. (used for read_samples function)
                      Output integers are stored in the native byte order
                      (little or big endian).  -jd
  in: samples_to_read
      bytes_per_sample
      swap_order    - set for high-to-low byte order input stream
 i/o: pcm_in
 out: sample_buffer  (must be allocated up to samples_to_read upon call)
returns: number of samples read
*/
static int
unpack_read_samples( const int samples_to_read, const int bytes_per_sample,
		     const int swap_order, int *sample_buffer, FILE *pcm_in )
{
    int samples_read;
    int i;
    int *op;			/* output pointer */
    unsigned char *ip = (unsigned char *) sample_buffer; /* input pointer */
    const int b = sizeof(int) * 8;

#define GA_URS_IFLOOP( ga_urs_bps ) \
    if( bytes_per_sample == ga_urs_bps ) \
	for( i = samples_read * bytes_per_sample; (i -= bytes_per_sample) >=0;)
    

    samples_read = fread( sample_buffer, bytes_per_sample, 
			  samples_to_read, pcm_in);
    op = sample_buffer + samples_read;

    GA_URS_IFLOOP( 1 )
	*--op = (ip[i] ^ 0x80)<<(b-8) | 0x7f<<(b-16);/* convert from unsigned*/
    if( swap_order == 0 ) {
	GA_URS_IFLOOP( 2 )
	    *--op = ip[i]<<(b-16) | ip[i+1]<<(b-8); 
	GA_URS_IFLOOP( 3 )
	    *--op = ip[i]<<(b-24) | ip[i+1]<<(b-16) | ip[i+2]<<(b-8);
	GA_URS_IFLOOP( 4 )
	    *--op = ip[i]<<(b-32) | ip[i+1]<<(b-24) | ip[i+2]<<(b-16) | ip[i+3] << (b-8);
    } else {
	GA_URS_IFLOOP( 2 )
	    *--op = ip[i]<<(b-8) | ip[i+1]<<(b-16); 
	GA_URS_IFLOOP( 3 )
	    *--op = ip[i]<<(b-8) | ip[i+1]<<(b-16) | ip[i+2]<<(b-24);
	GA_URS_IFLOOP( 4 )
	    *--op = ip[i]<<(b-8) | ip[i+1]<<(b-16) | ip[i+2]<<(b-24) | ip[i+3]<<(b-32);
    }
#undef GA_URS_IFLOOP
    return( samples_read );
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

static int
read_samples_pcm(FILE * musicin, int sample_buffer[2304],
                 int samples_to_read)
{
    int     samples_read;
    int     iswav = (input_format == sf_wave);
    int     hi_lo_order;	/* byte order of input stream */

    if( (32 == pcmbitwidth) || (24 == pcmbitwidth) || (16 == pcmbitwidth) ) {
	/* assume only recognized wav files are */
	/*  in little endian byte order */
	hi_lo_order = (!iswav == !pcmswapbytes);
        samples_read = unpack_read_samples(samples_to_read, pcmbitwidth/8, 
					   hi_lo_order,sample_buffer, musicin);
    } else if( 8 == pcmbitwidth ) {
	samples_read = unpack_read_samples( samples_to_read, 1, 0,
					    sample_buffer, musicin );
    } else {
        fprintf(stderr, "Only 8, 16, 24 and 32 bit input files supported \n");
        exit(1);
    }
    if (ferror(musicin)) {
        fprintf(stderr, "Error reading input file\n");
        exit(1);
    }

    return samples_read;
}



/* AIFF Definitions */

#define IFF_ID_FORM 0x464f524d /* "FORM" */
#define IFF_ID_AIFF 0x41494646 /* "AIFF" */
#define IFF_ID_AIFC 0x41494643 /* "AIFC" */
#define IFF_ID_COMM 0x434f4d4d /* "COMM" */
#define IFF_ID_SSND 0x53534e44 /* "SSND" */
#define IFF_ID_MPEG 0x4d504547 /* "MPEG" */

#define IFF_ID_NONE 0x4e4f4e45 /* "NONE" */ /* AIFF-C data format */
#define IFF_ID_2CBE 0x74776f73 /* "twos" */ /* AIFF-C data format */
#define IFF_ID_2CLE 0x736f7774 /* "sowt" */ /* AIFF-C data format */

#define WAV_ID_RIFF 0x52494646 /* "RIFF" */
#define WAV_ID_WAVE 0x57415645 /* "WAVE" */
#define WAV_ID_FMT  0x666d7420 /* "fmt " */
#define WAV_ID_DATA 0x64617461 /* "data" */
#define WAV_ID_LIST 0x4c495354 /* "LIST" */
#define WAV_ID_INFO 0x494e464f /* "INFO" */
#define WAV_ID_INAM 0x494e414d /* "INAM" */
#define WAV_ID_IART 0x49415254 /* "IART" */
#define WAV_ID_IGNR 0x49474e52 /* "IGNR" */
#define WAV_ID_IPRD 0x49505244 /* "IPRD" */
#define WAV_ID_ITRK 0x6974726b /* "itrk" */

#define INFO_SIZE 256

/**********************************************
 * Pull RIFF LIST INFO headers out of WAV files
 * and set their matching id3 tags
 * INFO headers are used by Turtle Beach's Audiotron 
 * itrk is their extension for Track Number
 * 
 * refer to 
 * http://www.saettler.com/RIFFMCI/riffmci.html
 * http://www.turtlebeach.com/site/kb_ftp/1166005.asp
 *********************************************/

static void
SetIDTagsFromRiffTags(lame_t gfp, FILE * sf)
{
    int subSize = Read32BitsLowHigh(sf);
    if (Read32BitsHighLow(sf) != WAV_ID_INFO) {
	fskip(sf, subSize, SEEK_CUR);
	return;
    }

    subSize -= 4;
    while (subSize && !feof(sf)) {
	int type = Read32BitsHighLow(sf);
	char buf[INFO_SIZE+1];
	unsigned int length = Read32BitsLowHigh(sf);
	if (length > INFO_SIZE)
	    return; /* XXX ugly FIX ME */
	ReadBytes(sf, buf, (length+1) & ~1); /* resolve word padding */
	buf[length] = 0;
	subSize -= 4 + ((length+1) & ~1);
	switch (type) {
	case WAV_ID_INAM:
	    /* Track Name */
	    id3tag_set_title(gfp, strdup(buf));
	    break;

	case WAV_ID_IART:
	    /* Artist Name */
	    id3tag_set_artist(gfp, strdup(buf));
	    break;

	case WAV_ID_IGNR:
	    /* Genre Name */
	    if (id3tag_set_genre(gfp,strdup(buf)))
		fprintf(stderr,"Unknown genre: %s.  Specify genre name or number\n", buf);
	    break;

	case WAV_ID_IPRD:
	    /* Title */
	    id3tag_set_album(gfp, strdup(buf));
	    break;

	case WAV_ID_ITRK:
	    id3tag_set_track(gfp, strdup(buf));
	    break;
	}
    }
}

/*****************************************************************************
 *
 * Read Microsoft Wave headers
 *
 * By the time we get here the first 32-bits of the file have already been
 * read, and we're pretty sure that we're looking at a WAV file.
 *
 *****************************************************************************/
static int
parse_wave_header(lame_t gfp, FILE * sf)
{
    int     format_tag = 0;
    int     channels = 0;
    int     block_align = 0;
    int     bits_per_sample = 0;
    int     samples_per_sec = 0;
    int     avg_bytes_per_sec = 0;
    unsigned long data_offset = MAX_U_32_NUM;

    int     is_wav = 0;
    long    data_length = 0, file_length, subSize = 0;
    int     loop_sanity = 0;

    file_length = Read32BitsHighLow(sf);

    if (Read32BitsHighLow(sf) != WAV_ID_WAVE)
	return 0;

    for (loop_sanity = 0; loop_sanity < 20; ++loop_sanity) {
	int type = Read32BitsHighLow(sf);
	if (feof(sf))
	    break;

        if (type == WAV_ID_FMT) {
	    subSize = Read32BitsLowHigh(sf);

	    if (subSize < 16) {
		/* too short 'fmt ' chunk */
		return 0;
	    }
	    subSize -= 16;

	    format_tag = Read16BitsLowHigh(sf);
	    channels = Read16BitsLowHigh(sf);
	    samples_per_sec = Read32BitsLowHigh(sf);
	    avg_bytes_per_sec = Read32BitsLowHigh(sf);
	    block_align = Read16BitsLowHigh(sf);
	    bits_per_sample = Read16BitsLowHigh(sf);

	    if (subSize > 0 && fskip(sf, subSize, SEEK_CUR) != 0)
		return 0;
        }
        else if (type == WAV_ID_DATA) {
            subSize = Read32BitsLowHigh(sf);
	    if (data_offset != MAX_U_32_NUM) {
		/* multiple 'data' chunk */
		return 0;
	    }
	    data_offset = ftell(sf);

            data_length = subSize;
            is_wav = 1;

	    if (fskip(sf, (long) subSize, SEEK_CUR) != 0)
		return 0;
	}
        else if (type == WAV_ID_LIST) {
	    SetIDTagsFromRiffTags(gfp, sf);
	}
	else {
	    /* unknown chunk */
	    subSize = Read32BitsLowHigh(sf);
	    if (fskip(sf, (long) subSize, SEEK_CUR) != 0)
		return 0;
        }
    }

    if (format_tag != 1)
	return 0; /* oh no! non-supported format  */


    if (is_wav) {
        /* make sure the header is sane */
	if (lame_set_num_channels(gfp, channels) == -1) {
	    fprintf(stderr, "Unsupported number of channels: %ud\n", channels);
	    exit(1);
	}
	lame_set_in_samplerate(gfp, samples_per_sec);
	pcmbitwidth = bits_per_sample;
	lame_set_num_samples(
	    gfp, data_length / (channels * ((bits_per_sample+7) / 8)));

	if (fskip(sf, (long) data_offset, SEEK_SET) == 0)
	    return 1;
    }
    return 0;
}



/************************************************************************
* aiff_check2
*
* PURPOSE:	Checks AIFF header information to make sure it is valid.
*	        returns 0 on success, 1 on errors
************************************************************************/

typedef struct  blockAlign_struct {
    unsigned long   offset;
    unsigned long   blockSize;
} blockAlign;

typedef struct  IFF_AIFF_struct {
    short           numChannels;
    unsigned long   numSampleFrames;
    short           sampleSize;
    double          sampleRate;
    unsigned long   sampleType;
    blockAlign      blkAlgn;
} IFF_AIFF;

static int
aiff_check2(IFF_AIFF * const pcm_aiff_data)
{
    if (pcm_aiff_data->sampleType != IFF_ID_SSND) {
        fprintf(stderr, "Input Sound data is not PCM\n");
        return 1;
    }
    if (pcm_aiff_data->sampleSize != sizeof(short) * CHAR_BIT) {
        fprintf(stderr, "Input Sound data is not %i bits\n",
                sizeof(short) * CHAR_BIT);
        return 1;
    }
    if (pcm_aiff_data->numChannels != 1 && pcm_aiff_data->numChannels != 2) {
        fprintf(stderr, "Input Sound data is not mono or stereo\n");
        return 1;
    }
    if (pcm_aiff_data->blkAlgn.blockSize != 0) {
        fprintf(stderr, "Block size of Input Sound data is not 0 bytes\n");
        return 1;
    }

    return 0;
}

/*****************************************************************************
 *
 *	Read Audio Interchange File Format (AIFF) headers.
 *
 *	By the time we get here the first 32 bits of the file have already been
 *	read, and we're pretty sure that we're looking at an AIFF file.
 *
 *****************************************************************************/

static int
parse_aiff_header(lame_t gfp, FILE * sf)
{
    int     is_aiff = 0;
    long    chunkSize = 0, subSize = 0, typeID = 0, dataType = 0;
    IFF_AIFF aiff_info;

    memset(&aiff_info, 0, sizeof(aiff_info));
    chunkSize = Read32BitsHighLow(sf);

    typeID = Read32BitsHighLow(sf);
    if ((typeID != IFF_ID_AIFF)&&(typeID != IFF_ID_AIFC))
        return 0;

    while (chunkSize > 0) {
        int     type = Read32BitsHighLow(sf);
        chunkSize -= 4;

        /* DEBUGF(
           "found chunk type %08x '%4.4s'\n", type, (char*)&type); */

        /* don't use a switch here to make it easier to use 'break' for SSND */
        if (type == IFF_ID_COMM) {
            subSize = Read32BitsHighLow(sf);
            chunkSize -= subSize;

            aiff_info.numChannels = Read16BitsHighLow(sf);
            subSize -= 2;
            aiff_info.numSampleFrames = Read32BitsHighLow(sf);
            subSize -= 4;
            aiff_info.sampleSize = Read16BitsHighLow(sf);
            subSize -= 2;
            aiff_info.sampleRate = ReadIeeeExtendedHighLow(sf);
            subSize -= 10;

            if (typeID == IFF_ID_AIFC) {
                dataType = Read32BitsHighLow(sf);
                subSize -= 4;

                if ((dataType != IFF_ID_2CLE) && 
                    (dataType != IFF_ID_2CBE) &&
                    (dataType != IFF_ID_NONE))
                    return 0;

                if (aiff_info.sampleSize == 16)
                  pcmswapbytes = (!swapbytes == (dataType == IFF_ID_2CLE));
            }
            
            if (fskip(sf, (long) subSize, SEEK_CUR) != 0)
                return 0;
        }
        else if (type == IFF_ID_SSND) {
            subSize = Read32BitsHighLow(sf);
            chunkSize -= subSize;

            aiff_info.blkAlgn.offset = Read32BitsHighLow(sf);
            subSize -= 4;
            aiff_info.blkAlgn.blockSize = Read32BitsHighLow(sf);
            subSize -= 4;

            if (fskip(sf, (long) aiff_info.blkAlgn.offset, SEEK_CUR) != 0)
                return 0;

            aiff_info.sampleType = IFF_ID_SSND;
            is_aiff = 1;

            /* We've found the audio data. Read no further! */
            break;

        }
        else {
            subSize = Read32BitsHighLow(sf);
            chunkSize -= subSize;

            if (fskip(sf, (long) subSize, SEEK_CUR) != 0)
                return 0;
        }
    }

    /* DEBUGF("Parsed AIFF %d\n", is_aiff); */
    if (is_aiff) {
        /* make sure the header is sane */
        if (aiff_check2(&aiff_info))
            return 0;
        if( -1 == lame_set_num_channels( gfp, aiff_info.numChannels ) ) {
            fprintf( stderr,
                     "Unsupported number of channels: %ud\n",
                     aiff_info.numChannels );
            exit( 1 );
        }
        lame_set_in_samplerate( gfp, aiff_info.sampleRate );
        pcmbitwidth = aiff_info.sampleSize;
        lame_set_num_samples( gfp, aiff_info.numSampleFrames );
    }
    return is_aiff;
}



/************************************************************************
*
* parse_file_header
*
* Read the header from a bytestream.  Try to determine whether
* it's a WAV file or AIFF without rewinding, since rewind
* doesn't work on pipes and there's a good chance we're reading
* from stdin (otherwise we'd probably be using libsndfile).
*
* When this function returns, the file offset will be positioned at the
* beginning of the sound data.
*
************************************************************************/

static void
parse_file_header(lame_t gfp, FILE * sf)
{
    int type = Read32BitsHighLow(sf);
    count_samples_carefully = 0;
    input_format = sf_raw;

    if (type == WAV_ID_RIFF) {
        /* It's probably a WAV file */
        if (parse_wave_header(gfp, sf)) {
	    input_format = sf_wave;
	    count_samples_carefully = 1;
        } else {
	    fprintf(stderr, "Warning: corrupt or unsupported WAVE format\n"); 
        }
    }
    else if (type == IFF_ID_FORM) {
	/* It's probably an AIFF file */
	if (parse_aiff_header(gfp, sf)) {
	    input_format = sf_aiff;
	    count_samples_carefully = 1;
	}
    }

    if (input_format == sf_raw) {
        /*
	 * Assume it's raw PCM.  Since the audio data is assumed to begin
	 * at byte zero, this will unfortunately require seeking.
         */
	if (fseek(sf, 0L, SEEK_SET) != 0) {
	    /* ignore errors */
        }
	input_format = sf_raw;
    }
}



void
close_infile(void)
{
    if (fclose(musicin) != 0) {
        fprintf(stderr, "Could not close audio input file\n");
        exit(2);
    }
}





static FILE   *
OpenSndFile(lame_t gfp, char *inPath)
{
    FILE   *musicin;

    /* set the defaults from info incase we cannot determine them from file */
    lame_set_num_samples( gfp, MAX_U_32_NUM );


    if (!strcmp(inPath, "-")) {
	musicin = stdin;
        set_stream_binary_mode(stdin); /* Read from standard input. */
    }
    else {
        if ((musicin = fopen(inPath, "rb")) == NULL) {
            fprintf(stderr, "Could not find \"%s\".\n", inPath);
            exit(1);
        }
    }

    if (input_format == sf_mp1 ||
        input_format == sf_mp2 || input_format == sf_mp3) {
#ifdef HAVE_MPGLIB
        if (-1 == decode_initfile(gfp, musicin, &mp3input_data)) {
            fprintf(stderr, "Error reading headers in mp3 input file %s.\n",
                    inPath);
            exit(1);
        }
#endif
        if( -1 == lame_set_num_channels( gfp, mp3input_data.channels) ) {
            fprintf( stderr,
                     "Unsupported number of channels: %ud\n",
                     mp3input_data.channels);
            exit( 1 );
        }
	lame_set_in_samplerate( gfp, mp3input_data.samplerate );
	lame_set_num_samples( gfp, mp3input_data.nsamp );
    }
    else {
        if (input_format != sf_raw) {
            parse_file_header(gfp, musicin);
        }

        if (input_format == sf_raw) {
            /* assume raw PCM */
            fprintf(stderr, "Assuming raw pcm input file");
            if (swapbytes)
                fprintf(stderr, " : Forcing byte-swapping\n");
            else
                fprintf(stderr, "\n");
            pcmswapbytes = swapbytes;
        }
    }


    if (lame_get_num_samples( gfp ) == MAX_U_32_NUM && musicin != stdin) {

        double  flen = get_file_size(inPath); /* try to figure out num_samples */

        if (flen >= 0) {
            /* try file size, assume 2 bytes per sample */
            if (input_format == sf_mp1 ||
                input_format == sf_mp2 || input_format == sf_mp3) {

                if (mp3input_data.bitrate > 0) {
                    double  totalseconds =
                        (flen * 8.0 / (1000.0 * mp3input_data.bitrate));
                    unsigned long tmp_num_samples =
                        totalseconds * lame_get_in_samplerate( gfp );

                    lame_set_num_samples( gfp, tmp_num_samples );
                    mp3input_data.nsamp = tmp_num_samples;
                }
            }
            else {
                lame_set_num_samples(
		    gfp, flen / (2 * lame_get_num_channels( gfp )) );
            }
        }
    }
    return musicin;
}
#endif /* defined(LIBSNDFILE) */





#if defined(HAVE_MPGLIB)
static int
check_aid(const unsigned char *header)
{
    return 0 == memcmp(header, "AiD\1", 4);
}

/*
 * Please check this and don't kill me if there's a bug
 * This is a (nearly?) complete header analysis for a MPEG-1/2/2.5 Layer I, II or III
 * data stream
 */
static int
is_syncword_mp123(const void *const headerptr)
{
    const unsigned char *const p = headerptr;
    static const char abl2[16] =
        { 0, 7, 7, 7, 0, 7, 0, 0, 0, 0, 0, 8, 8, 8, 8, 8 };

    if (p[0] != 0xFF)
        return 0;       /* first 8 bits must be '1' */
    if ((p[1] & 0xE0) != 0xE0)
        return 0;       /* next 3 bits are also */
    if ((p[1] & 0x18) == 0x08)
        return 0;       /* no MPEG-1, -2 or -2.5 */
    if (!(((p[1] & 0x06) == 0x03*2 && input_format == sf_mp1)
	  || ((p[1] & 0x06) == 0x02*2 && input_format == sf_mp2)
	  || ((p[1] & 0x06) == 0x01*2 && input_format == sf_mp3)))
	return 0; /* imcompatible layer with input file format */
    if ((p[2] & 0xF0) == 0xF0)
        return 0;       /* bad bitrate */
    if ((p[2] & 0x0C) == 0x0C)
        return 0;       /* no sample frequency with (32,44.1,48)/(1,2,4)     */
    if ((p[1] & 0x18) == 0x18 && (p[1] & 0x06) == 0x04
	&& abl2[p[2] >> 4] & (1 << (p[3] >> 6)))
	return 0;
    if ((p[3] & 3) == 2)
	return 0;       /* reserved enphasis mode */

    return 1;
}

static int
decode_initfile(lame_t gfp, FILE * fd, mp3data_struct * mp3data)
{
    /*  VBRTAGDATA pTagData; */
    /* int xing_header,len2,num_frames; */
    unsigned char buf[100];
    int     ret;
    int     len, aid_header;
    short int pcm_l[1152], pcm_r[1152];
    int freeformat = 0;

    memset(mp3data, 0, sizeof(mp3data_struct));
    lame_decode_init(gfp);

    len = 4;
    if (fread(buf, len, 1, fd) != 1)
        return -1;      /* failed */
    if (buf[0] == 'I' && buf[1] == 'D' && buf[2] == '3') {
	len = 6;
	if (fread(&buf, len, 1, fd) != 1)
	    return -1;      /* failed */
	buf[2] &= 127; buf[3] &= 127; buf[4] &= 127; buf[5] &= 127;
	len = (buf[2] << 21) + (buf[3] << 14) + (buf[4] << 7) + buf[5];
	fskip(fd, len, SEEK_CUR);
	id3v2taglen = ftell(fd);
	len = 4;
	if (fread(&buf, len, 1, fd) != 1)
	    return -1;      /* failed */
    }
    aid_header = check_aid(buf);
    if (aid_header) {
	if (fread(&buf, 1, 2, fd) != 2)
            return -1;  /* failed */
        aid_header = (unsigned char) buf[0] + 256 * (unsigned char) buf[1];
        fprintf(stderr, "Album ID found.  length=%i \n", aid_header);
        /* skip rest of AID, except for 6 bytes we have already read */
        fskip(fd, aid_header - 6, SEEK_CUR);

        /* read 4 more bytes to set up buffer for MP3 header check */
	if (fread(&buf, len, 1, fd) != 1)
	    return -1;      /* failed */
    }

    /* look for valid 8 byte MPEG header  */
    len = 4;
    while (!is_syncword_mp123(buf)) {
        int     i;
        for (i = 0; i < len - 1; i++)
            buf[i] = buf[i + 1];
        if (fread(buf + len - 1, 1, 1, fd) != 1)
            return -1;  /* failed */
    }

    if ((buf[2] & 0xf0)==0) {
	fprintf(stderr,"Input file is freeformat.\n");
	freeformat = 1;
    }

    /* now parse the current buffer looking for MP3 headers.    */
    /* (as of 11/00: mpglib modified so that for the first frame where  */
    /* headers are parsed, no data will be decoded.   */
    /* However, for freeformat, we need to decode an entire frame, */
    /* so mp3data->bitrate will be 0 until we have decoded the first */
    /* frame.  Cannot decode first frame here because we are not */
    /* yet prepared to handle the output. */
    ret = lame_decode1_headersB(gfp, buf, len, pcm_l, pcm_r, mp3data,&enc_delay,&enc_padding);
    if (-1 == ret)
        return -1;

    /* repeat until we decode a valid mp3 header.  */
    while (!mp3data->header_parsed) {
        len = fread(buf, 1, sizeof(buf), fd);
        if (len != sizeof(buf))
            return -1;
        ret = lame_decode1_headersB(gfp, buf, len, pcm_l, pcm_r, mp3data,&enc_delay,&enc_padding);
        if (-1 == ret)
            return -1;
    }

    if (mp3data->bitrate==0 && !freeformat) {
	fprintf(stderr, "fail to sync...\n");
	return decode_initfile(gfp, fd, mp3data);
    }

    if (mp3data->totalframes > 0) {
        /* mpglib found a Xing VBR header and computed nsamp & totalframes */
    }
    else {
	/* set as unknown.  Later, we will take a guess based on file size
	 * ant bitrate */
        mp3data->nsamp = MAX_U_32_NUM;
    }


    /*
       fprintf(stderr,"ret = %i NEED_MORE=%i \n",ret,MP3_NEED_MORE);
       fprintf(stderr,"channels = %i \n",mp.fr.channels);
       fprintf(stderr,"samp = %i  \n",freqs[mp.fr.sampling_frequency]);
       fprintf(stderr,"bitrate = %i  \n",mp3data->bitrate);
       fprintf(stderr,"num frames = %ui  \n",num_frames);
       fprintf(stderr,"num samp = %ui  \n",mp3data->nsamp);
       fprintf(stderr,"mode     = %i  \n",mp.fr.mode);
     */

    return 0;
}

/*
For decode_fromfile:  return code
  -1     error
   n     number of samples output.  either 576 or 1152 depending on MP3 file.


For lame_decode1_headers():  return code
  -1     error
   0     ok, but need more data before outputing any samples
   n     number of samples output.  either 576 or 1152 depending on MP3 file.
*/
static int
decode_fromfile(lame_t gfp, FILE * fd, short pcm_l[], short pcm_r[],
		mp3data_struct * mp3data)
{
    int     ret, len = 0;
    unsigned char buf[1024];

    /* first see if we still have data buffered in the decoder: */
    ret = lame_decode1_headers(gfp, buf, len, pcm_l, pcm_r, mp3data);
    if (ret != 0) return ret;

    /* read until we get a valid output frame */
    do {
        len = fread(buf, 1, 1024, fd);
        if (len == 0) {
	    /* we are done reading the file, but check for buffered data */
	    ret = lame_decode1_headers(gfp, buf, len, pcm_l, pcm_r, mp3data);
	    if (ret<=0) {
                return -1;  /* done with file */
            }
	    break;
	}

        ret = lame_decode1_headers(gfp, buf, len, pcm_l, pcm_r, mp3data);
        if (ret == -1) {
            return ret;
        }
    } while (ret == 0);
    return ret;
}
#endif /* defined(HAVE_MPGLIB) */

/* end of get_audio.c */
