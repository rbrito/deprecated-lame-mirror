/*
 *	Interface to MP3 LAME encoding engine
 *
 *	Copyright (c) 1999 Mark Taylor
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#ifndef LAME_H_INCLUDE
#define LAME_H_INCLUDE
#include <stdio.h>
#define LAME_MAXMP3BUFFER 16384

typedef struct  {
  int encoder_delay;
  int framesize;
  int output_channels;
  int output_samplerate;      /* in Hz */
} lame_mp3info;





/* if no_output=1, all LAME output is disabled, and it is up to the
 * calling program to write the mp3 data (returned by lame_encode() and
 * lame_cleanup()).  In this case, the calling program is responsible 
 * for filling in the Xing VBR header data and adding the id3 tag.
 * see lame_cleanup() for details. */
void lame_init(int no_output);

/* lame_cleanup will flush the buffers and may return a final mp3 frame */
int lame_cleanup(char *mp3buf);

void lame_usage(char *);
void lame_parse_args(int, char **);
void lame_print_config(void);

/* fill lame_mp3info with data on the mp3 file */
void lame_getmp3info(lame_mp3info *);

/* read one frame of PCM data from audio input file opened by lame_parse_args*/
/* input file can be either mp3 or uncompressed audio file */
int lame_readframe(short int Buffer[2][1152]);

/* 
NOTE: for lame_encode and lame_decode, because of the bit reservoir
capability of mp3 frames, there can be a delay between the input
and output.  lame_decode/lame_encode should always return exactly
one frame of output, but it may not return any output 
until a second call with a second frame.  (or even 3 or 4 more calls)
*/

/* input 1 pcm frame, output (maybe) 1 mp3 frame. */ 
/* return code = number of bytes output in mp3buffer.  can be 0 */
int lame_encode(short int Buffer[2][1152],char *mp3buffer);



/* input 1 mp3 frame, output (maybe) 1 pcm frame.   */
int lame_decode_init(void);
int lame_decode(char *mp3buf,int len,short pcm[][1152]);

/* read mp3 file until mpglib returns one frame of PCM data */
int lame_decode_initfile(FILE *fd,int *stereo,int *samp,int *bitrate);
int lame_decode_fromfile(FILE *fd,short int mpg123pcm[2][1152]);
/*
For lame_decode_*:  return code
  -1     error
   0     ok, but need more data before outputing any samples
   n     number of samples output.  either 576 or 1152 depending on MP3 file.
*/

#endif
