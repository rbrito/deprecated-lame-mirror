#ifndef GLOBAL_FLAGS_H_INCLUDED
#define GLOBAL_FLAGS_H_INCLUDED

#include "machine.h"

typedef enum sound_file_format_e {
	sf_unknown, sf_wave, sf_aiff, sf_mp3, sf_raw
} sound_file_format;

/***********************************************************************
*
*  Global Variable External Declarations
*
***********************************************************************/

extern int allow_diff_short;
extern int ATHonly;
extern int autoconvert;
extern int disable_reservoir;
extern int experimentalX;
extern int experimentalY;
extern int experimentalZ;
extern int fast_mode; 
extern long int frameNum;
extern int gtkflag;
extern int g_bWriteVbrTag;
extern int gpsycho;
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
extern int psyModel;
#endif
