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
  long int frameNum;              /* frame counter */
  long totalframes;               /* frames: 0..totalframes-1 (estimate)*/
  int gtkflag;                    /* frame analyzer? */
  int mode_gr;                    /* granules per frame */
  int stereo;                     /* number of channels */
  int bWriteVbrTag;               /* Xing VBR tag? */
  sound_file_format input_format; 
  int fast_mode;                  /* fast, very low quaity  */
  int highq;                      /* use best possible quality */
  int allow_diff_short;           /* allow blocktypes to differ between channels ? */
  int no_short_blocks;            /* disable short blocks */
  int silent;                     /* disable some status output */
  int voice_mode;

  /* file i/o */
  int autoconvert;                /* downsample stereo->mono */
  int lame_nowrite;               /* disable file writing */
  int swapbytes;                  /* force byte swapping on input file*/

  /* psycho acoustics */
  int ATHonly;                    /* only use ATH */
  int noATH;                      /* disable ATH */
  FLOAT cwlimit;                  /* predictability limit */

  /* resampling and filtering */
  FLOAT resample_ratio;           /* input_samp_rate/output_samp_rate */
  FLOAT lowpass1,lowpass2;
  FLOAT highpass1,highpass2;
  int sfb21;

  /* quantization/noise shaping */
  int disable_reservoir;          /* use bit reservoir? */
  int experimentalX;            
  int experimentalY;
  int experimentalZ;

  /* VBR control */
  int VBR;
  int VBR_q;
  int VBR_min_bitrate;
  int VBR_max_bitrate;

} global_flags;


extern global_flags gf;



#endif
