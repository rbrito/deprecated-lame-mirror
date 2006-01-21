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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
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

#ifdef WITH_DMALLOC
# include <dmalloc.h>
#endif
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

#include "main.h"
#include "get_audio.h"

/* global data for get_audio.c. */
mp3data_struct mp3input_data; /* used by MP3 */
void *g_inputHandler;
int headingTagLen = 0;

static unsigned int num_samples_read;
static int count_samples_carefully;

#ifdef LIBSNDFILE
static int useLibsndfile;
#endif

static void Write16Bits(FILE *fp, int i)
{
    putc(i & 0xff, fp);
    putc((i >> 8) & 0xff, fp);
}

static void Write32Bits(FILE *fp, int i)
{
    Write16Bits(fp, i);
    Write16Bits(fp, i >> 16);
}

static void ReadBytes(FILE *fp, char *p, int n)
{
    memset ( p, 0, n );
    fread  ( p, 1, n, fp );
}

static unsigned int ReadByte(FILE* fp)
{
    int  result = getc (fp);
    return result == EOF  ?  0  : result;
}

static int  Read16Bits(FILE* fp)
{
    int  low  = ReadByte(fp);
    int  high = ReadByte(fp);

    return (int) ((short)((high << 8) | low));
}

static int Read32Bits(FILE* fp)
{
    int  low  = ReadByte(fp);
    int  medl = ReadByte(fp);
    int  medh = ReadByte(fp);
    int  high = ReadByte(fp);

    return (high << 24) | (medh << 16) | (medl << 8) | low;
}

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
# define set_stream_binary_mode(fp)	((void)fp)
#endif

#define WAV_ID_RIFF 0x46464952 /* "RIFF" */
#define WAV_ID_WAVE 0x45564157 /* "WAVE" */
#define WAV_ID_FMT  0x20746d66 /* "fmt " */
#define WAV_ID_DATA 0x61746164 /* "data" */
#define WAV_ID_LIST 0x5453494c /* "LIST" */
#define WAV_ID_INFO 0x4f464e49 /* "INFO" */
#define WAV_ID_INAM 0x4d414e49 /* "INAM" */
#define WAV_ID_IART 0x54524149 /* "IART" */
#define WAV_ID_IGNR 0x524e4749 /* "IGNR" */
#define WAV_ID_IPRD 0x44525049 /* "IPRD" */
#define WAV_ID_ITRK 0x6b727469 /* "itrk" */

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
    int subSize = Read32Bits(sf);
    if (Read32Bits(sf) != WAV_ID_INFO) {
	fskip(sf, subSize, SEEK_CUR);
	return;
    }

    subSize -= 4;
    while (subSize && !feof(sf)) {
	int type = Read32Bits(sf);
	char buf[INFO_SIZE+1];
	unsigned int length = Read32Bits(sf);
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

    file_length = Read32Bits(sf);

    if (Read32Bits(sf) != WAV_ID_WAVE)
	return 0;

    for (loop_sanity = 0; loop_sanity < 20; ++loop_sanity) {
	int type = Read32Bits(sf);
	if (feof(sf))
	    break;

        if (type == WAV_ID_FMT) {
	    subSize = Read32Bits(sf);

	    if (subSize < 16) {
		/* too short 'fmt ' chunk */
		return 0;
	    }
	    subSize -= 16;

	    format_tag = Read16Bits(sf);
	    channels = Read16Bits(sf);
	    samples_per_sec = Read32Bits(sf);
	    avg_bytes_per_sec = Read32Bits(sf);
	    block_align = Read16Bits(sf);
	    bits_per_sample = Read16Bits(sf);

	    if (subSize > 0 && fskip(sf, subSize, SEEK_CUR) != 0)
		return 0;
        }
        else if (type == WAV_ID_DATA) {
            subSize = Read32Bits(sf);
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
	    subSize = Read32Bits(sf);
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
	if (lame_get_in_samplerate(gfp) < 0)
	    lame_set_in_samplerate(gfp, samples_per_sec);
	in_bitwidth = bits_per_sample;
	lame_set_num_samples(
	    gfp, data_length / (channels * ((bits_per_sample+7) / 8)));

	if (fskip(sf, (long) data_offset, SEEK_SET) == 0)
	    return 1;
    }
    return 0;
}



/************************************************************************
 *
 * parse_file_header
 *
 * Read the header from a bytestream.  Try to determine whether
 * it's a WAV file without rewinding, since rewind doesn't work on
 * pipes and there's a good chance we're reading
 * from stdin (otherwise we'd probably be using libsndfile).
 *
 * When this function returns, the file offset will be positioned at the
 * beginning of the sound data.
 *
 ************************************************************************/

static void
parse_file_header(lame_t gfp, FILE * sf)
{
    int type = Read32Bits(sf);
    count_samples_carefully = 0;
    input_format = sf_raw;

    if (type == WAV_ID_RIFF) {
	/* It's probably a WAV file */
	if (parse_wave_header(gfp, sf)) {
	    input_format = sf_wave;
	    count_samples_carefully = 1;
	} else {
	    /* It's probably a WAV file, but we do not know how to handle it */
#ifdef LIBSNDFILE
	    /* use libsndfile */
	    input_format = sf_unknown;
	    useLibsndfile = 1;
#endif
	}
    }

    if (input_format == sf_raw) {
        /* input file is unsupported WAVE format or corrupt WAVE file.
	 * Assume it's as raw PCM.  Since the audio data is assumed to begin
	 * at byte zero, this will unfortunately require seeking.
         */
	if (fseek(sf, 0L, SEEK_SET) != 0) {
	    /* ignore errors */
	}
	fprintf(stderr, "Warning: corrupt or unsupported WAVE format\n"); 
    }
}





static off_t
get_file_size(const char* const filename)
{
    struct stat       sb;

    if ( 0 == stat ( filename, &sb ) )
        return sb.st_size;
    return (off_t) -1;
}


#ifdef HAVE_MPGLIB
static int decode_initfile(lame_t gfp, FILE * fd, mp3data_struct * mp3data);

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
    unsigned char buf[100];
    int     ret;
    int     len, aid_header;
    short   pcm_l[1152], pcm_r[1152];
    int freeformat = 0;

    memset(mp3data, 0, sizeof(mp3data_struct));
    if (lame_decode_init(gfp) < 0)
	return -1;

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
	headingTagLen = ftell(fd);
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
decode_fromfile(lame_t gfp, FILE * fd, float pcm_l[], float pcm_r[],
		mp3data_struct * mp3data)
{
    int     ret, len = 0;
    unsigned char buf[1024];

    /* first see if we still have data buffered in the decoder: */
    ret = lame_decode1_headers_noclip(gfp, buf, len, pcm_l, pcm_r, mp3data);
    if (ret > 0) return ret;

    /* read until we get a valid output frame */
    do {
	len = fread(buf, 1, 1024, fd);
	ret = lame_decode1_headers_noclip(gfp, buf, len, pcm_l, pcm_r, mp3data);
	if (ret == -1) {
	    return ret;
	}
	if (len == 0) {
	    /* we are done reading the file, but check for buffered data */
	    if (ret<=0) {
		return -1;  /* done with file */
	    }
	    break;
	}
    } while (ret == 0);
    return ret;
}
#endif /* defined(HAVE_MPGLIB) */

static int
read_samples_mp3(lame_t gfp, float mpg123pcm[2][1152])
{
#ifndef HAVE_MPGLIB
    return -1;
#else
    int     out;

    out = decode_fromfile(gfp, g_inputHandler, mpg123pcm[0], mpg123pcm[1],
			  &mp3input_data);
    /*
     * out < 0:  error, probably EOF
     * out = 0:  not possible with decode_fromfile() ???
     * out > 0:  number of output samples
     */
    if (out < 0) {
	memset(mpg123pcm, 0, sizeof(float) * 2 * 1152);
	return 0;
    }

    if (lame_get_num_channels(gfp) != mp3input_data.channels)
	fprintf(stderr,
		"Error: number of channels has changed in an MP3 file - not supported\n");
    if (lame_get_in_samplerate(gfp) != mp3input_data.samplerate)
	fprintf(stderr,
		"Error: sample frequency has changed in an MP3 file - not supported\n");
    return out;
#endif
}

/************************************************************************
unpack_read_samples - read and unpack signed low-to-high byte or unsigned
                      single byte input. (used for read_samples function)
                      Output integers are stored in the native byte order
                      (little or big endian).  -jd
  in: samples_to_read
      bytes_per_sample
      swap_order    - set for high-to-low byte order input stream
 out: sample_buffer  (must be allocated up to samples_to_read upon call)
returns: number of samples read
*/
static int
unpack_read_samples( const int samples_to_read, const int bytes_per_sample,
		     float *sample_buffer)
{
    int samples_read;
    int i;
    float *op;			/* output pointer */
    unsigned char *ip = (unsigned char *) sample_buffer; /* input pointer */
#define B (sizeof(int) * 8)

#define GA_URS_IFLOOP(ga_urs_bps) \
    if (bytes_per_sample == ga_urs_bps) \
	for (i = samples_read * bytes_per_sample; (i -= bytes_per_sample) >=0;)

    samples_read = fread(ip, bytes_per_sample,
			 samples_to_read, g_inputHandler);
    op = sample_buffer + samples_read;

    GA_URS_IFLOOP(1)
	*--op = (ip[i] ^ 0x80)<<(B-8) | 0x7f<<(B-16);/* convert from unsigned*/

    if (in_endian == order_littleEndian) {
	GA_URS_IFLOOP(2)
	    *--op = ip[i]<<(B-16) | ip[i+1]<<(B-8);
	GA_URS_IFLOOP(3)
	    *--op = ip[i]<<(B-24) | ip[i+1]<<(B-16) | ip[i+2]<<(B-8);
	GA_URS_IFLOOP(4)
	    *--op = ip[i]<<(B-32) | ip[i+1]<<(B-24) | ip[i+2]<<(B-16) | ip[i+3] << (B-8);
    } else {
	GA_URS_IFLOOP(2)
	    *--op = ip[i]<<(B-8) | ip[i+1]<<(B-16);
	GA_URS_IFLOOP(3)
	    *--op = ip[i]<<(B-8) | ip[i+1]<<(B-16) | ip[i+2]<<(B-24);
	GA_URS_IFLOOP(4)
	    *--op = ip[i]<<(B-8) | ip[i+1]<<(B-16) | ip[i+2]<<(B-24) | ip[i+3]<<(B-32);
    }
#undef GA_URS_IFLOOP
#undef B
    return samples_read;
}

/************************************************************************
*
* read_samples_pcm()
*
* PURPOSE:  reads the PCM samples from a file to the buffer
*
*  SEMANTICS:
* Reads #samples_read# number of shorts from g_inputHandler filepointer
* into #sample_buffer[]#.  Returns the number of samples read.
*
************************************************************************/

static int
read_samples_pcm(float sample_buffer[1152*2], int samples_to_read)
{
    int samples_read;

    assert((32 == in_bitwidth) || (24 == in_bitwidth) || (16 == in_bitwidth)
	   || (in_bitwidth == 8));

    samples_read = unpack_read_samples(samples_to_read, in_bitwidth/8,
				       sample_buffer);

    if (ferror((FILE *)g_inputHandler)) {
        fprintf(stderr, "Error reading input file\n");
        exit(1);
    }

    return samples_read;
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
 ***********************************************************************/
int
get_audio(lame_t gfp, float buffer[2][1152])
{
    int num_channels = lame_get_num_channels(gfp), samples_read;
    unsigned int tmp_num_samples = lame_get_num_samples(gfp);
    unsigned int samples_to_read = lame_get_framesize(gfp);
    unsigned int remaining;
    float    tmpbuf[2 * 1152];
    int     i;

    assert(samples_to_read <= 1152);

    /* 
     * NOTE: LAME can now handle arbritray size input data packets,
     * so there is no reason to read the input data in chuncks of
     * size "framesize".  EXCEPT:  the LAME graphical frame analyzer 
     * will get out of sync if we read more than framesize worth of data.
     */

    /* if this flag has been set, then we are carefull to read
     * exactly num_samples and no more.  This is useful for .wav
     * files which have some tags at the end.  Note that if you
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

    if (IS_MPEG123(input_format)) {
	int j;
	samples_read = read_samples_mp3(gfp, buffer);
	for (j = 0; j < num_channels; j++) {
	    for (i = samples_read; --i >= 0; )
		buffer[j][i] *= 65536.0;
	}
	if (num_channels == 1)
	    memset(buffer[1], 0, samples_read * sizeof(int));
    } else {
	float     *insamp = tmpbuf;
#ifdef LIBSNDFILE
	if (useLibsndfile) {
	    samples_read
		= sf_read_float(g_inputHandler, insamp,
				num_channels * samples_to_read);
	} else 
#endif
	{
	    samples_read
		= read_samples_pcm(insamp, num_channels * samples_to_read);
	}

	if (num_channels == 2) {
	    samples_read /= 2;
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



int
WriteWaveHeader(FILE * const fp, const int pcmbytes,
                const int freq, const int channels, const int bits)
{
    int     bytes = (bits + 7) / 8;

    /* quick and dirty, but documented */
    fwrite("RIFF", 1, 4, fp); /* label */
    Write32Bits(fp, pcmbytes + 44 - 8); /* length in bytes without header */
    fwrite("WAVEfmt ", 2, 4, fp); /* 2 labels */
    Write32Bits(fp, 2 + 2 + 4 + 4 + 2 + 2); /* length of PCM format declaration area */
    Write16Bits(fp, 1); /* format ID 1, linear PCM */
    Write16Bits(fp, channels); /* number of channels */
    Write32Bits(fp, freq); /* sample frequency in [Hz] */
    Write32Bits(fp, freq * channels * bytes); /* bytes per second */
    Write16Bits(fp, channels * bytes); /* bytes per sample time */
    Write16Bits(fp, bits); /* bits per sample */
    fwrite("data", 1, 4, fp); /* label */
    Write32Bits(fp, pcmbytes); /* length in bytes of raw PCM data */

    return ferror(fp) ? -1 : 0;
}




void
close_infile(void)
{
#ifdef LIBSNDFILE
    if (useLibsndfile) {
	if (g_inputHandler) {
	    if (sf_close(g_inputHandler) != 0) {
		fprintf(stderr, "Could not close sound file \n");
		exit(2);
	    }
	}
	return;
    }
#endif
    if (fclose(g_inputHandler) != 0) {
	fprintf(stderr, "Could not close audio input file\n");
	exit(2);
    }
}



static void
OpenSndFile(lame_t gfp, char *inPath)
{
    FILE   *fp = NULL;
#ifdef LIBSNDFILE
    SNDFILE *gs_pSndFileIn;
    SF_INFO gs_wfInfo;
    useLibsndfile = 1;
    if (input_format != sf_unknown) {
	useLibsndfile = 0;
    }
#endif

    /* set the defaults from info in case we cannot determine them from file */
    lame_set_num_samples( gfp, MAX_U_32_NUM );

    if (!strcmp(inPath, "-")) {
	fp = stdin;
	set_stream_binary_mode(stdin); /* Read from standard input. */
#ifdef LIBSNDFILE
	/* seems libsndfile has some problem with stdin... */
	useLibsndfile = 0;
#endif
    }
    else {
	if ((fp = fopen(inPath, "rb")) == NULL) {
	    fprintf(stderr, "Could not find \"%s\".\n", inPath);
	    exit(1);
	}
    }

#ifdef HAVE_MPGLIB
    if (IS_MPEG123(input_format)) {
        if (!(fp = fopen(inPath, "rb"))) {
            fprintf(stderr, "Could not find \"%s\".\n", inPath);
            exit(1);
        }
	if (decode_initfile(gfp, fp, &mp3input_data) < 0) {
	    fprintf(stderr, "Error reading headers in mp3 input file %s.\n",
                    inPath);
            exit(1);
	}
	if (lame_set_num_channels(gfp, mp3input_data.channels) < 0) {
            fprintf( stderr, "Unsupported number of channels: %ud\n",
		     mp3input_data.channels);
	    exit(1);
	}
	lame_set_in_samplerate(gfp, mp3input_data.samplerate);
	lame_set_num_samples(gfp, mp3input_data.nsamp);
    }
#endif

#ifdef LIBSNDFILE
    /* Try to open the sound file with libsndfile */
    /* set some defaults in case input is raw PCM */
    gs_wfInfo.seekable = 1;
    gs_wfInfo.samplerate = lame_get_in_samplerate(gfp);
    gs_wfInfo.channels = lame_get_num_channels(gfp);

    if (in_bitwidth == 8) {
	if (in_signed)
	    gs_wfInfo.format = SF_FORMAT_PCM_S8;
	else
	    gs_wfInfo.format = SF_FORMAT_PCM_U8;
    } else {
	if (!in_signed) {
	    fprintf(stderr,
		    "Unsigned input only supported with bitwidth 8\n");
	    exit(1);
	}
	if (in_endian == order_littleEndian)
	    gs_wfInfo.format = SF_ENDIAN_LITTLE;
	else
	    gs_wfInfo.format = SF_ENDIAN_BIG;
    }
#endif /* LIBSNDFILE */

    if (input_format == sf_wave) {
	parse_file_header(gfp, fp);
	if (input_format == sf_wave)
	    in_endian = order_littleEndian;
    }
    if (input_format == sf_raw && silent < 10) {
	/* assume raw PCM */
	fprintf(stderr, "Assuming raw pcm input file");
	if (in_endian == order_littleEndian)
	    fprintf(stderr, " : Little Endian\n");
	else
	    fprintf(stderr, " : Big Endian\n");
    }

#ifdef LIBSNDFILE
    if (useLibsndfile) {
	g_inputHandler = gs_pSndFileIn = sf_open(inPath, SFM_READ, &gs_wfInfo);

	/* Check result */
	if (!gs_pSndFileIn) {
	    sf_perror(gs_pSndFileIn);
	    fprintf(stderr, "Could not open sound file \"%s\".\n", inPath);
	    exit(1);
	}

	lame_set_num_samples(gfp, gs_wfInfo.frames);
	if (lame_set_num_channels(gfp, gs_wfInfo.channels) < 0) {
	    fprintf(stderr, "Unsupported number of channels: %ud\n",
		    gs_wfInfo.channels);
	    exit(1);
	}
	lame_set_in_samplerate(gfp, gs_wfInfo.samplerate);
    }
#endif /* LIBSNDFILE */

    if (lame_get_num_samples(gfp) == MAX_U_32_NUM) {
	/* try to figure out num_samples */
	double  flen = get_file_size(inPath);
	unsigned long tmp_num_samples;

	if (flen >= 0) {
	    /* try file size, assume 2 bytes per sample */
	    int bps = 2*lame_get_num_channels(gfp);
	    if (IS_MPEG123(input_format) && mp3input_data.bitrate > 0) {
		/* if input is mp3, use its bitrate */
		bps = mp3input_data.bitrate * (1000/8);
		flen *= lame_get_in_samplerate(gfp);
	    }
	    tmp_num_samples = flen / bps;
	    lame_set_num_samples(gfp, tmp_num_samples);
	    if (IS_MPEG123(input_format)
		&& mp3input_data.nsamp == MAX_U_32_NUM)
		mp3input_data.nsamp = tmp_num_samples;
	}
    }

    if (fp) {
#ifdef LIBSNDFILE
	if (useLibsndfile) {
	    fclose(fp);
	} else
#endif
	{
	    g_inputHandler = fp;
	}
    }
#ifdef LIBSNDFILE
    if (silent < 10) {
	if (useLibsndfile)
	    fprintf(stderr, "use libsndfile for file input\n");
	else
	    fprintf(stderr, "use internal audio input routine\n");
    }
#endif
}

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
    OpenSndFile(gfp, inPath);
}

/* end of get_audio.c */
