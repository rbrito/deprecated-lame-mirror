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


void CloseSndFile(sound_file_format input, FILE *musicin);
FILE * OpenSndFile(lame_global_flags *gfp);

int get_audio(lame_global_flags *gfp,short buffer[2][1152]);

#ifdef HAVEMPGLIB
int lame_decoder(lame_global_flags *gfp,FILE *outf,int skip);
#endif

#ifdef LIBSNDFILE

/* INCLUDE the sound library header file */
/* (you should have this in your system includes, or alternatively in project dir) */
#include "sndfile.h"
/* One byte alignment for WIN32 should already be part of sndfile.h, */
/* so no longer needed here (if not, get newer libsndfile) */


#else
/*****************************************************************
 * LAME/ISO built in audio file I/O routines 
 *******************************************************************/
#include "portableio.h"
#include "ieeefloat.h"


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

extern int            aiff_read_headers(FILE*, IFF_AIFF*);
extern int            aiff_seek_to_sound_data(FILE*);
extern int            aiff_write_headers(FILE*, IFF_AIFF*);
extern int parse_wavheader(void);
extern int parse_aiff(const char fn[]);
extern void   aiff_check(const char*, IFF_AIFF*, int*);

enum byte_order { order_unknown, order_bigEndian, order_littleEndian };
enum byte_order NativeByteOrder;

enum byte_order DetermineByteOrder(void);
void SwapBytesInWords( short *loc, int words );

void init_infile(lame_global_flags *);
int readframe(lame_global_flags *,short int Buffer[2][1152]);
void close_infile(lame_global_flags *);


#endif	/* ifdef LIBSNDFILE */
#endif	/* ifndef LAME_GET_AUDIO_H */
