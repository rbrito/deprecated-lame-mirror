#ifndef GLOBAL_FLAGS_H_INCLUDED
#define GLOBAL_FLAGS_H_INCLUDED

#include "machine.h"

typedef enum sound_file_format_e {
	sf_unknown, sf_wave, sf_aiff, sf_mp3, sf_raw
} sound_file_format;


/***********************************************************************
*
*  Global Variables.  Substantiated in lame.c
*
***********************************************************************/
typedef struct  {
  /* general control params */
  int gtkflag;                    /* frame analyzer? */
  int bWriteVbrTag;               /* Xing VBR tag? */
  int fast_mode;                  /* fast, very low quaity  */
  int highq;                      /* use best possible quality */
  int allow_diff_short;           /* allow blocktypes to differ between channels ? */
  int no_short_blocks;            /* disable short blocks */
  int silent;                     /* disable some status output */
  int voice_mode;
  int mode;                       /* mono, stereo, jstereo */
  int mode_fixed;                 /* use specified mode, not lame's opinion of the best mode */
  int brate;                      /* bitrate */

  /* frame params */
  int copyright;                  /* mark as copyright */
  int original;                   /* mark as original */
  int emphasis;                   /* obsolete */
  int error_protection;           /* use 2 bytes per frame for a CRC checksum */

  /* file i/o */
  sound_file_format input_format; 
  unsigned long num_samples;      /* number of samples in input file */
  int num_channels;               /* input number of channels */
  int samplerate;                 /* input_samp_rate */
  int resamplerate;               /* output_samp_rate */ 
  int autoconvert;                /* downsample stereo->mono */
  int swapbytes;                  /* force byte swapping on input file*/

  /* psycho acoustics */
  int ATHonly;                    /* only use ATH */
  int noATH;                      /* disable ATH */
  FLOAT cwlimit;                  /* predictability limit */

  /* resampling and filtering */
  int lowpassfreq;                /* freq in Hz. 0=lame choses. 1=no filter */
  int highpassfreq;               /* freq in Hz. 0=lame choses. 1=no filter */
  int lowpasswidth;               /* freq width of filter, in Hz */
  int highpasswidth;              /* freq width of filter, in Hz */
  int sfb21;

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


  /* internal variables NOT set by calling program */
  long int frameNum;              /* frame counter */
  long totalframes;               /* frames: 0..totalframes-1 (estimate)*/
  int lame_nowrite;               /* disable file writing */
  int mode_gr;                    /* granules per frame */
  int stereo;                     /* number of channels */
  int VBR_min_bitrate;       
  int VBR_max_bitrate;
  FLOAT resample_ratio;           /* input_samp_rate/output_samp_rate */
  FLOAT lowpass1,lowpass2;
  FLOAT highpass1,highpass2;

} global_flags;


extern global_flags gf;



#endif
