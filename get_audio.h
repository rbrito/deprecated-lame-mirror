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

#if (defined LIBSNDFILE || defined LAMESNDFILE)

void CloseSndFile(sound_file_format input, FILE *musicin);
FILE * OpenSndFile(lame_global_flags *gfp);

int get_audio(lame_global_flags *gfp,short buffer[2][1152],int stereo);



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
    FLOAT           sampleRate;
    unsigned long   sampleType;
    blockAlign      blkAlgn;
} IFF_AIFF;

extern int            aiff_read_headers(FILE*, IFF_AIFF*);
extern int            aiff_seek_to_sound_data(FILE*);
extern int            aiff_write_headers(FILE*, IFF_AIFF*);
extern int parse_wavheader(void);
extern int parse_aiff(const char fn[]);
extern void   aiff_check(const char*, IFF_AIFF*, int*);


#endif	/* ifdef LIBSNDFILE */
#endif  /* ifdef LAMESNDFILE or LIBSNDFILE */
#endif	/* ifndef LAME_GET_AUDIO_H */
