/*
 *	Get Audio routines include file
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

#ifndef LAME_GET_AUDIO_H
#define LAME_GET_AUDIO_H

#include "lame.h"
#include <stdio.h>

typedef enum sound_file_format_e {
    sf_unknown, 
    sf_raw, 
    sf_wave, 
    sf_mp1 = 0x11,  /* MPEG Layer 1, aka mpg */
    sf_mp2 = 0x12,  /* MPEG Layer 2 */
    sf_mp3 = 0x13   /* MPEG Layer 3 */
} sound_file_format;

#define IS_MPEG123(x) ((x) & 0x10)

#ifdef LIBSNDFILE
/* even when with libsndfile, we cannot use it with mpeg layer 1,2,3 */
# define USE_LIBSNDFILE(x) (!IS_MPEG123(x))
#else
# define USE_LIBSNDFILE(x) 0
#endif

FILE *init_outfile (char *outPath);
void init_infile(lame_t , char *inPath);
void close_infile(void);
int get_audio(lame_t gfp, int buffer[2][1152]);
int WriteWaveHeader(FILE * const fp, const int pcmbytes,
		    const int freq, const int channels, const int bits);
extern int id3v2taglen;
extern void *g_inputHandler;
extern int in_signed;
extern int in_endian;
extern int in_bitwidth;
extern sound_file_format input_format;

#define order_littleEndian 0
#define order_bigEndian 1

#ifdef WORDS_BIGENDIAN
# define order_nativeEndian order_bigEndian
# define order_counterNativeEndian order_littleEndian
#else
# define order_nativeEndian order_littleEndian
# define order_counterNativeEndian order_bigEndian
#endif

#ifdef UINT_MAX
# define         MAX_U_32_NUM            UINT_MAX
#else
# define         MAX_U_32_NUM            0xFFFFFFFF
#endif

#ifdef LIBSNDFILE
# include <sndfile.h>
#endif /* ifdef LIBSNDFILE */
#endif /* ifndef LAME_GET_AUDIO_H */
