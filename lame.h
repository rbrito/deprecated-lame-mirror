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


typedef enum sound_file_format_e {
	sf_unknown, sf_wave, sf_aiff, sf_mp3, sf_raw
} sound_file_format;


/***********************************************************************
*
*  Global Variables.  
*
*  substantiated in lame.c
*
*  Initilized and default values set by gf=lame_init()
*  gf is a pointer to this struct, which the user may use to 
*  override any of the default values
*
*  a call to lame_set_params() is also needed
*
***********************************************************************/
typedef struct  {
  /* input file description */
  unsigned long num_samples;  /* number of samples         */
  int num_channels;           /* input number of channels  */
  int samplerate;             /* input_samp_rate           */

  /* general control params */
  int gtkflag;                /* frame analyzer?       */
  int bWriteVbrTag;           /* Xing VBR tag?         */
  int fast_mode;              /* fast, very low quaity */
  int highq;                  /* use best possible quality  */
  int allow_diff_short;       /* allow blocktypes to differ between channels ? */
  int no_short_blocks;        /* disable short blocks       */
  int silent;                 /* disable some status output */
  int mode;                       /* mono, stereo, jstereo */
  int mode_fixed;                 /* use specified mode, not lame's opinion of the best mode */
  int brate;                      /* bitrate */

  /* frame params */
  int copyright;                  /* mark as copyright */
  int original;                   /* mark as original */
  int emphasis;                   /* obsolete */
  int error_protection;           /* use 2 bytes per frame for a CRC checksum */


  /* resampling and filtering */
  int resamplerate;               /* output_samp_rate.   */ 
  int lowpassfreq;                /* freq in Hz. 0=lame choses. -1=no filter */
  int highpassfreq;               /* freq in Hz. 0=lame choses. -1=no filter */
  int lowpasswidth;               /* freq width of filter, in Hz (default=15%)*/
  int highpasswidth;              /* freq width of filter, in Hz (default=15%)*/
  int sfb21;                      /* soon to be obsolete */

  /* quantization/noise shaping */
  int disable_reservoir;          /* use bit reservoir? */
  int experimentalX;            
  int experimentalY;
  int experimentalZ;

  /* VBR control */
  int VBR;
  int VBR_q;
  int VBR_min_bitrate_kbps;
  int VBR_max_bitrate_kbps;


  /* psycho acoustics */
  int ATHonly;                    /* only use ATH */
  int noATH;                      /* disable ATH */
  float cwlimit;                  /* predictability limit */

  /* not yet coded: */
  int filter_type;          /* 0=MDCT filter, 1= (expensive) filters */
  int noise_shaping;        /* 0 = none 
                               1 = ISO model
                               2 = allow multiple scalefacs to hit maximum
                               3 = allow scalefac_select=1  
                             */

  int noise_shaping_stop;   /* 0 = stop at over=0. 
                               1=stop when all scalefacs amplified  
			    */

  int psymodel;             /* 0 = none   1=gpsycho */
  int ms_masking;           /* 0 = no,  1 = use mid/side maskings */
  int use_best_huffman;     /* 0 = no.  1=outside loop  2=inside loop(slow) */


  /* 
map commandline options to quality settings:

--qual=9 (worst)
 -f          filter_type=0
             noise_shaping=0
             psymodel=0

--qual=?
default      filter_type=0
             noise_shaping=1
	     noise_shaping_stop=0
             psymodel=1
             ms_masking=0
             use_best_huffman=0

--qual=?
-h           filter_type=0
             noise_shaping=2
	     noise_shaping_stop=0
             psymodel=1
             ms_masking=1
             use_best_huffman=1

--qual=0 (best, very slow)
             filter_type=1
             noise_shaping=3
	     noise_shaping_stop=1
             psymodel=1
             ms_masking=1
             use_best_huffman=2

  */
              


  /* input file reading - not used if you handle your own i/o */
  sound_file_format input_format;   
  int swapbytes;              /* force byte swapping   default=0*/



  /*******************************************************************/
  /* internal variables NOT set by calling program, but which might  */
  /* be of interest to the calling program                           */
  /*******************************************************************/
  long int frameNum;              /* frame counter */
  long totalframes;               /* frames: 0..totalframes-1 (estimate)*/
  int encoder_delay;
  int framesize;
  int lame_nowrite;               /* user will take care of output */
  int lame_noread;                /* user will take care of input */
  int mode_gr;                    /* granules per frame */
  int stereo;                     /* number of channels */
  int VBR_min_bitrate;       
  int VBR_max_bitrate;
  float resample_ratio;           /* input_samp_rate/output_samp_rate */
  float lowpass1,lowpass2;
  float highpass1,highpass2;

} lame_global_flags;



/* if no_output=1, all LAME output is disabled, and it is up to the
 * calling program to write the mp3 data (returned by lame_encode() and
 * lame_cleanup()).  In this case, the calling program is responsible 
 * for filling in the Xing VBR header data and adding the id3 tag.
 * see lame_cleanup() for details. */
lame_global_flags *lame_init(int no_output, int no_input);

/* lame_cleanup will flush the buffers and may return a final mp3 frame */
int lame_cleanup(char *mp3buf);

void lame_usage(char *);
void lame_parse_args(int, char **);
void lame_init_params(void);
void lame_print_config(void);


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
#ifdef AMIGA_MPEGA
int lame_decode_initfile(const char *fullname,int *stereo,int *samp,int *bitrate, unsigned long *nsamp);
#else
int lame_decode_initfile(FILE *fd,int *stereo,int *samp,int *bitrate, unsigned long *nsamp);
#endif
int lame_decode_fromfile(FILE *fd,short int mpg123pcm[2][1152]);
/*
For lame_decode_*:  return code
  -1     error
   0     ok, but need more data before outputing any samples
   n     number of samples output.  either 576 or 1152 depending on MP3 file.
*/

extern int force_ms;

#endif
