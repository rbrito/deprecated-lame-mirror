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



typedef enum vbr_mode_e {
  vbr_off=0,
  vbr_mt,
  vbr_rh,
  vbr_abr,
  vbr_mtrh,
  vbr_default=vbr_rh  /* change this to change the default VBR mode of LAME */ 
} vbr_mode;





/***********************************************************************
*
*  Control Parameters set by User
*
*  instantiated by calling program
*
*  1.  call lame_init(&gf) to initialize to default values.
*  2.  override any default values, if desired 
*  3.  call lame_init_params(&gf) for more internal configuration
*
*
***********************************************************************/
typedef struct  {
  /* input description */
  unsigned long num_samples;  /* number of samples. default=2^32-1    */
  int num_channels;           /* input number of channels. default=2  */
  int in_samplerate;          /* input_samp_rate in Hz. default=44100kHz     */
  int out_samplerate;         /* output_samp_rate. default: LAME picks best value */
  float scale;                /* scale input by this amount before encoding */

  /* general control params */
  int analysis;               /* collect data for a MP3 frame analyzer?       */
  int bWriteVbrTag;           /* add Xing VBR tag?         */
  int disable_waveheader;     /* disable writing of .wav header, when *decoding* */
  int decode_only;            /* use lame/mpglib to convert mp3/ogg to wav */
  int ogg;                    /* encode to Vorbis .ogg file */

  int quality;                /* quality setting 0=best,  9=worst  default=5 */
  int mode;                   /* 0,1,2,3 = stereo,jstereo,dual channel,mono   */
  int mode_fixed;             /* user specified the mode, do not use lame's opinion of the best mode */
  int force_ms;               /* force M/S mode.  requires mode=1 */
  int free_format;            /* use free format? default=0*/

  /* set either brate>0  or compression_ratio>0, LAME will compute
   * the value of the variable not set.  Default is compression_ratio=11 */
  int brate;                  /* bitrate */
  float compression_ratio;    /* sizeof(wav file)/sizeof(mp3 file) */


  /* frame params */
  int copyright;                  /* mark as copyright. default=0 */
  int original;                   /* mark as original. default=1 */
  int error_protection;           /* use 2 bytes per frame for a CRC checksum. default=0*/
  int padding_type;               /* 0=no padding, 1=always pad, 2=adjust padding default=2*/
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




  /************************************************************************/
  /* internal variables, do not set...                                    */
  /* provided because they may be of use to calling application           */
  /************************************************************************/

  int version;                    /* 0=MPEG2  1=MPEG1 */
  int encoder_delay;
  int framesize;                  
  int frameNum;                   /* number of frames encoded */
  int totalframes;
  int lame_allocated_gfp;         /* is this struct owned by calling program or lame? */


  /* VBR tags */
  int nZeroStreamSize;
  int TotalFrameSize;
  int* pVbrFrames;
  int nVbrNumFrames;
  int nVbrFrameBufferSize;


  /****************************************************************************/
  /* more internal variables, which will not exist after lame_encode_finish() */
  /****************************************************************************/
  void *internal_flags;

} lame_global_flags;






/***********************************************************************
 *
 *  The LAME API
 *  These functions should be called, in this order, for each
 *  MP3 file to be encoded 
 *
 ***********************************************************************/


/* REQUIRED: initialize the encoder.  sets default for all encoder paramters,
 * returns -1 if some malloc()'s failed
 * otherwise returns 0
 * 
 */
lame_global_flags *lame_init(void);
/* obsolete version */
int lame_init_old(lame_global_flags *);


/* optional.  set as needed to override defaults */
/* note: these routines not yet written: */

/* set one of these.  default is compression ratio of 11.  */
int lame_set_brate(lame_global_flags *, int);
int lame_set_compression_ratio(lame_global_flags *, int);



/* REQUIRED:  sets more internal configuration based on data provided
 * above.  returns -1 if something failed.
 */
int lame_init_params(lame_global_flags *);


/* OPTIONAL:  get the version number, in a string. of the form:  
   "3.63 (beta)" or just "3.63". */
const char *get_lame_version       ( void );
const char *get_lame_short_version ( void );

/* For final versions both functions returning the same value */
/* get_lame_short_version() aim is to ease file compare, because subnumber, date and time is NOT reported */
/* get_lame_version() aim is to ease version tracking, because subnumber, date and time is reported */

const char* get_lame_url ( void );   /* Removes the nasty LAME_URL macro */

/* OPTIONAL:  print internal lame configuration to message handler */
void lame_print_config(const lame_global_flags *);




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
//int lame_encode_buffer(lame_global_flags *,short int leftpcm[], short int rightpcm[],int num_samples, char *mp3buffer,int  mp3buffer_size);

int    lame_encode_buffer (
        lame_global_flags*  gfp,
        const short int     buffer_l [],
        const short int     buffer_r [],
        const int           nsamples,
        char* const         mp3buf,
        const int           mp3buf_size );

/* as above, but input has L & R channel data interleaved.  Note: 
 * num_samples = number of samples in the L (or R)
 * channel, not the total number of samples in pcm[]  
 */
int lame_encode_buffer_interleaved(lame_global_flags *,short int pcm[], 
int num_samples, char *mp3buffer,int  mp3buffer_size);



/* REQUIRED:  lame_encode_finish will flush the buffers and may return a 
 * final few mp3 frames.  mp3buffer should be at least 7200 bytes.
 *
 * will also write id3v1 tags (if any) into the bitstream       
 *
 * return code = number of bytes output to mp3buffer.  can be 0
 */
int lame_encode_flush(lame_global_flags *,char *mp3buffer, int size);






/* OPTIONAL:    some simple statistics
 * a bitrate histogram to visualize the distribution of used frame sizes
 * a stereo mode histogram to visualize the distribution of used stereo
 *   modes, useful in joint-stereo mode only
 *   0: LR    left-right encoded
 *   1: LR-I  left-right and intensity encoded (currently not supported)
 *   2: MS    mid-side encoded
 *   3: MS-I  mid-side and intensity encoded (currently not supported)
 *
 * attention: don't call them after lame_encode_finish
 * suggested: lame_encode_flush -> lame_*_hist -> lame_close
 */
 
void lame_bitrate_hist( 
        const lame_global_flags *const gfp, 
              int                      bitrate_count[14] );
void lame_bitrate_kbps( 
        const lame_global_flags *const gfp, 
              int                      bitrate_kbps [14] );
void lame_stereo_mode_hist( 
        const lame_global_flags *const gfp, 
              int                      stereo_mode_count[4] );

void lame_bitrate_stereo_mode_hist ( 
        const lame_global_flags*  gfp, 
        int  bitrate_stmode_count [14] [4] );


/* OPTIONAL:  lame_mp3_tags_fid will append a Xing VBR tag to
the mp3 file with file pointer fid.  These calls perform forward and
backwards seeks, so make sure fid is a real file.  Make sure
lame_encode_flush has been called, and all mp3 data has been written
to the file before calling this function.
Note: if VBR  tags are turned off by the user, or turned off
by LAME because the output is not a regular file, this call does nothing
*/
void lame_mp3_tags_fid(lame_global_flags *,FILE* fid);


/* REQUIRED:  final call to free all remaining buffers */
void lame_close(lame_global_flags *);

/* OBSOLETE:  lame_encode_finish combines lame_encode_flush
 * and lame_close in one call.  However, once this call is made,
 * the statistics routines will no longer function since there 
 * data will have been cleared */
int lame_encode_finish(lame_global_flags *,char *mp3buffer, int size);










/*********************************************************************
 *
 * decoding 
 *
 * a simple interface to mpglib, part of mpg123, is also included if
 * libmp3lame is compiled with HAVEMPGLIB
 *
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

/* same as lame_decode, and also returns mp3 header data */
int lame_decode_headers(char *mp3buf,int len,short pcm_l[],short pcm_r[],
mp3data_struct *mp3data);

/* same as lame_decode, but returns at most one frame */
int lame_decode1(char *mp3buf,int len,short pcm_l[],short pcm_r[]);

/* same as lame_decode1, but returns at most one frame and mp3 header data */
int lame_decode1_headers(char *mp3buf,int len,short pcm_l[],short pcm_r[],
mp3data_struct *mp3data);

/* Also usefull for decoding is the ability to parse Xing VBR headers: */
#define NUMTOCENTRIES 100
typedef struct
{
  int		h_id;			/* from MPEG header, 0=MPEG2, 1=MPEG1 */
  int		samprate;		/* determined from MPEG header */
  int		flags;			/* from Vbr header data */
  int		frames;			/* total bit stream frames from Vbr header data */
  int		bytes;			/* total bit stream bytes from Vbr header data*/
  int		vbr_scale;		/* encoded vbr scale from Vbr header data*/
  unsigned char	toc[NUMTOCENTRIES];	/* may be NULL if toc not desired*/
  int           headersize;             /* size of VBR header, in bytes */
}   VBRTAGDATA;

int GetVbrTag(VBRTAGDATA *pTagData,  unsigned char *buf);








/*********************************************************************
 *
 * id3tag stuff
 *
 *********************************************************************/

/*
 * id3tag.h -- Interface to write ID3 version 1 and 2 tags.
 *
 * Copyright (C) 2000 Don Melton.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

/* utility to obtain alphabetically sorted list of genre names with numbers */
extern void id3tag_genre_list(void (*handler)(int, const char *, void *),
    void *cookie);

extern void id3tag_init(lame_global_flags *gfp);

/* force addition of version 2 tag */
extern void id3tag_add_v2(lame_global_flags *gfp);
/* add only a version 1 tag */
extern void id3tag_v1_only(lame_global_flags *gfp);
/* add only a version 2 tag */
extern void id3tag_v2_only(lame_global_flags *gfp);
/* pad version 1 tag with spaces instead of nulls */
extern void id3tag_space_v1(lame_global_flags *gfp);
/* pad version 2 tag with extra 128 bytes */
extern void id3tag_pad_v2(lame_global_flags *gfp);

extern void id3tag_set_title(lame_global_flags *gfp, const char *title);
extern void id3tag_set_artist(lame_global_flags *gfp, const char *artist);
extern void id3tag_set_album(lame_global_flags *gfp, const char *album);
extern void id3tag_set_year(lame_global_flags *gfp, const char *year);
extern void id3tag_set_comment(lame_global_flags *gfp, const char *comment);
extern void id3tag_set_track(lame_global_flags *gfp, const char *track);

/* return non-zero result if genre name or number is invalid */
extern int id3tag_set_genre(lame_global_flags *gfp, const char *genre);


/* moved to lametime.* */

/***********************************************************************
*
*  list of valid bitrates & samplerates.
*  first index: 0:  mpeg2 values  (samplerates < 32khz) 
*               1:  mpeg1 values  (samplerates >= 32khz)
***********************************************************************/
extern const int      bitrate_table[2][16];
extern const int      samplerate_table[2][3];



/* maximum size of mp3buffer needed if you encode at most 1152 samples for
   each call to lame_encode_buffer.  see lame_encode_buffer() below  
   (LAME_MAXMP3BUFFER is now obsolete)  */
#define LAME_MAXMP3BUFFER 16384


/* MPEG modes */
#define         MPG_MD_STEREO           0
#define         MPG_MD_JOINT_STEREO     1
#define         MPG_MD_DUAL_CHANNEL     2   /* not supported by LAME */
#define         MPG_MD_MONO             3





#if defined(__cplusplus)
}
#endif
#endif
