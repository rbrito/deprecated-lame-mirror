#ifndef GLOBAL_FLAGS_H_INCLUDED
#define GLOBAL_FLAGS_H_INCLUDED

#include "machine.h"

typedef enum sound_file_format_e {
	sf_unknown, sf_wave, sf_aiff, sf_mp3, sf_raw
} sound_file_format;


/***********************************************************************
*
*  we are in the process of moving global flags into this struct
*
***********************************************************************/

typedef struct  {

  int allow_diff_short;
  int ATHonly;
  int noATH;
  int autoconvert;
  FLOAT cwlimit;
  int disable_reservoir;
  int experimentalX;
  int experimentalY;
  int experimentalZ;
  int fast_mode; 
  long int frameNum;
  int gtkflag;
  int bWriteVbrTag;
  int mode_gr;
  int stereo;
} global_flags;


/***********************************************************************
*
*  Global Variable External Declarations
*
***********************************************************************/
extern global_flags gf;

extern int highq;
extern sound_file_format input_format;
extern int lame_nowrite;
extern FLOAT lowpass1,lowpass2;
extern FLOAT highpass1,highpass2;
extern int no_short_blocks;
extern FLOAT resample_ratio;     /* if > 0, = input_samp/output_samp */
extern int sfb21;
extern int silent;
extern int swapbytes;
extern long totalframes;       /* frames: 0..totalframes-1 */
extern int VBR;
extern int VBR_q;
extern int VBR_min_bitrate;
extern int VBR_max_bitrate;
extern int voice_mode;
#endif
