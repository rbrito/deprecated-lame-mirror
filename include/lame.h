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
#ifndef LAME_LAME_H
#define LAME_LAME_H
#include <stdio.h>
#if defined(__cplusplus)
extern "C" {
#endif

/* maximum size of mp3buffer needed if you encode at most 1152 samples for
   each call to lame_encode_buffer.  see lame_encode_buffer() below  
   (LAME_MAXMP3BUFFER is now obsolete)  */
#define LAME_MAXMP3BUFFER 16384


typedef enum vbr_mode_e {
  vbr_off=0,
  vbr_mt,
  vbr_rh,
  vbr_abr,
  vbr_mtrh,
  vbr_default=vbr_rh  /* change this to change the default VBR mode of LAME */ 
} vbr_mode;


/* MPEG Header Definitions - Mode Values */

#define         MPG_MD_STEREO           0
#define         MPG_MD_JOINT_STEREO     1
#define         MPG_MD_DUAL_CHANNEL     2
#define         MPG_MD_MONO             3

/* Mode Extention */

#define         MPG_MD_LR_LR             0
#define         MPG_MD_LR_I              1
#define         MPG_MD_MS_LR             2
#define         MPG_MD_MS_I              3


struct id3tag_spec
{
    /* private data members */
    int flags;
    const char *title;
    const char *artist;
    const char *album;
    int year;
    const char *comment;
    int track;
    int genre;
};

/***********************************************************************
*
*  Control Parameters set by User
*
*  substantiated by calling program
*
*  Initilized and default values set by lame_init(&gf)
*
*
***********************************************************************/
typedef struct  {
  /* input file description */
  unsigned long num_samples;  /* number of samples. default=2^32-1    */
  int num_channels;           /* input number of channels. default=2  */
  int in_samplerate;          /* input_samp_rate. default=44.1kHz     */
  int out_samplerate;         /* output_samp_rate. (usually determined automatically)   */ 
  float scale;                /* scale input by this amount */

  /* general control params */
  int analysis;               /* run frame analyzer?       */
  int bWriteVbrTag;           /* add Xing VBR tag?         */
  int disable_waveheader;     /* disable writing of .wav header, when *decoding* */
  int decode_only;            /* use lame/mpglib to convert mp3 to wav */
  int ogg;                    /* encode to Vorbis .ogg file */

  int quality;                /* quality setting 0=best,  9=worst  */
  int mode;                       /* 0,1,2,3 stereo,jstereo,dual channel,mono */
  int mode_fixed;                 /* use specified the mode, do not use lame's opinion of the best mode */
  int force_ms;                   /* force M/S mode.  requires mode=1 */
  int brate;                      /* bitrate */
  float compression_ratio;          /* user specified compression ratio, instead of brate */
  int free_format;                /* use free format? */

  /* frame params */
  int copyright;                  /* mark as copyright. default=0 */
  int original;                   /* mark as original. default=1 */
  int error_protection;           /* use 2 bytes per frame for a CRC checksum. default=0*/
  int padding_type;               /* 0=no padding, 1=always pad, 2=adjust padding */
  int extension;                  /* the MP3 'private extension' bit.  meaningless */
  int strict_ISO;                 /* enforce ISO spec as much as possible */

  /* quantization/noise shaping */
  int disable_reservoir;          /* use bit reservoir? */
  int experimentalX;            
  int experimentalY;
  int experimentalZ;
  int exp_nspsytune;

  /* VBR control */
  vbr_mode VBR;
  int VBR_q;
  int VBR_mean_bitrate_kbps;
  int VBR_min_bitrate_kbps;
  int VBR_max_bitrate_kbps;
  int VBR_hard_min;             /* strictly enforce VBR_min_bitrate*/
                                /* normaly, it will be violated for analog silence */


  /* resampling and filtering */
  int lowpassfreq;                /* freq in Hz. 0=lame choses. -1=no filter */
  int highpassfreq;               /* freq in Hz. 0=lame choses. -1=no filter */
  int lowpasswidth;               /* freq width of filter, in Hz (default=15%)*/
  int highpasswidth;              /* freq width of filter, in Hz (default=15%)*/


  /* Note: outPath must be set if you want Xing VBR or ID3 version 1 tags written */
#define         MAX_NAME_SIZE           1000
  char outPath[MAX_NAME_SIZE];



  /* optional ID3 tags  */
  int id3v1_enabled;
  struct id3tag_spec tag_spec;

  /* psycho acoustics and other aguments which you should not change 
   * unless you know what you are doing  */
  int ATHonly;                    /* only use ATH */
  int ATHshort;                   /* only use ATH for short blocks */
  int noATH;                      /* disable ATH */
  int ATHlower;                   /* lower ATH by this many db */
  int cwlimit;                    /* predictability limit */
  int allow_diff_short;           /* allow blocktypes to differ between channels ? */
  int no_short_blocks;            /* disable short blocks       */
  int emphasis;                   /* obsolete */

  float raiseSMR;  /* 0...1 : 0 leave SMR, 1 maximize SMR */



  /************************************************************************/
  /* internal variables, do not set... */
  /* provided because they may be of use to some applications   */
  /************************************************************************/

  int version;                    /* 0=MPEG2  1=MPEG1 */
  int encoder_delay;
  int framesize;                  


  /* VBR tags */
  int nZeroStreamSize;
  int TotalFrameSize;
  int* pVbrFrames;
  int nVbrNumFrames;
  int nVbrFrameBufferSize;


  /************************************************************************/
  /* more internal variables, which will not exist after lame_encode_finish() */
  /************************************************************************/
  void *internal_flags;

} lame_global_flags;






/*

The LAME API

 */


/* REQUIRED: initialize the encoder.  sets default for all encoder paramters,
 * returns -1 if some malloc()'s failed
 * otherwise returns 0
 * 
 */
int lame_init(lame_global_flags *);




/*********************************************************************
 * command line argument parsing & option setting.  Only supported
 * if libmp3lame compiled with LAMEPARSE defined 
 *********************************************************************/
void lame_print_license(lame_global_flags* gfp, const char* ProgramName );

/* OPTIONAL: get the version number, in a string. of the form:  "3.63 (beta)" or 
   just "3.63".  Max allows length is 20 characters  */
void lame_version(lame_global_flags *, char *);
void lame_print_version ( FILE* fp );





/* REQUIRED:  sets more internal configuration based on data provided
 * above.  returns -1 if something failed.
 */
int lame_init_params(lame_global_flags *);





/* OPTONAL:  print internal lame configuration on stderr*/
void lame_print_config(lame_global_flags *);

/* input pcm data, output (maybe) mp3 frames.
 * This routine handles all buffering, resampling and filtering for you.
 * 
 * leftpcm[]       array of 16bit pcm data, left channel
 * rightpcm[]      array of 16bit pcm data, right channel
 * num_samples     number of samples in leftpcm[] and rightpcm[] (if stereo)
 * mp3buffer       pointer to buffer where mp3 output is written
 * mp3buffer_size  size of mp3buffer, in bytes
 * return code     number of bytes output in mp3buffer.  can be 0 
 *                 -1:  mp3buffer was too small
 *                 -2:  malloc() problem
 *                 -3:  lame_init_params() not called
 *                 -4:  psycho acoustic problems 
 *                 -5:  ogg cleanup encoding error
 *                 -6:  ogg frame encoding error
 *
 * The required mp3buffer_size can be computed from num_samples, 
 * samplerate and encoding rate, but here is a worst case estimate:
 *
 * mp3buffer_size in bytes = 1.25*num_samples + 7200
 *
 * I think a tighter bound could be:  (mt, March 2000)
 * MPEG1:
 *    num_samples*(bitrate/8)/samplerate + 4*1152*(bitrate/8)/samplerate + 512
 * MPEG2:
 *    num_samples*(bitrate/8)/samplerate + 4*576*(bitrate/8)/samplerate + 256
 *
 * but test first if you use that!
 *
 * set mp3buffer_size = 0 and LAME will not check if mp3buffer_size is
 * large enough.
 *
 * NOTE: if gfp->num_channels=2, but gfp->mode = 3 (mono), the L & R channels
 * will be averaged into the L channel before encoding only the L channel
 * This will overwrite the data in leftpcm[] and rightpcm[].
 * 
*/
int lame_encode_buffer(lame_global_flags *,short int leftpcm[], short int rightpcm[],int num_samples,
char *mp3buffer,int  mp3buffer_size);

/* as above, but input has L & R channel data interleaved.  Note: 
 * num_samples = number of samples in the L (or R)
 * channel, not the total number of samples in pcm[]  
 */
int lame_encode_buffer_interleaved(lame_global_flags *,short int pcm[], 
int num_samples, char *mp3buffer,int  mp3buffer_size);



/* REQUIRED:  lame_encode_finish will flush the buffers and may return a 
 * final few mp3 frames.  mp3buffer should be at least 7200 bytes.
 *
 * will also write id3 tags (if any) into the bitstream       
 *
 * return code = number of bytes output to mp3buffer.  can be 0
 */
int lame_encode_finish(lame_global_flags *,char *mp3buffer, int size);


/* OPTIONAL:  lame_mp3_tags_fid will append a Xing VBR tag to
the mp3 file with file pointer fid.  These calls perform forward and
backwards seeks, so make sure fid is a real file.
Note: if VBR  tags are turned off by the user, or turned off
by LAME because the output is not a regular file, this call does nothing
*/
void lame_mp3_tags_fid(lame_global_flags *,FILE* fid);





/*********************************************************************
 * a simple interface to mpglib, part of mpg123, is also included if
 * libmp3lame is compiled with HAVEMPGLIB
 *********************************************************************/
typedef struct {
  int header_parsed;   /* 1 if header was parsed and following data was computed: */
  int stereo;          /* number of channels */
  int samplerate;      /* sample rate */
  int bitrate;         /* bitrate */
  int mode;               /* mp3 frame type */
  int mode_ext;           /* mp3 frame type */
  int framesize;          /* number of samples per mp3 frame */

  /* this data is not currently computed by the mpglib routines: */
  unsigned long nsamp;    /* number of samples in mp3 file.  */
  int framenum;           /* frames decoded counter */
  int totalframes;        /* total number of frames in mp3 file */
} mp3data_struct;


/* required call to initialize decoder */
int lame_decode_init(void);

/*********************************************************************
 * input 1 mp3 frame, output (maybe) pcm data.  
 * lame_decode return code:  -1: error.  0: need more data.  n>0: size of pcm output
 *********************************************************************/
int lame_decode(char *mp3buf,int len,short pcm_l[],short pcm_r[]);

/* same as lame_decode, but returns mp3 header data */
int lame_decode_headers(char *mp3buf,int len,short pcm_l[],short pcm_r[],
mp3data_struct *mp3data);

/* same as lame_decode, but returns at most one frame */
int lame_decode1(char *mp3buf,int len,short pcm_l[],short pcm_r[]);

/* same as lame_decode1, but returns at most one frame and mp3 header data */
int lame_decode1_headers(char *mp3buf,int len,short pcm_l[],short pcm_r[],
mp3data_struct *mp3data);


/***********************************************************************
*
*  Global Variable External Declarations
*
***********************************************************************/

extern const int      bitrate_table[2][16];
extern const int      samplerate_table[2][3];



#if defined(__cplusplus)
}
#endif


#endif
