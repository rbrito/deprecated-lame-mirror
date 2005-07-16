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

/* $Id$ */

#ifndef LAME_LAME_H
#define LAME_LAME_H

#include <stdio.h>
#include <stdarg.h>

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(WIN32)
#undef CDECL
#define CDECL _cdecl
#else
#define CDECL
#endif


typedef enum vbr_mode_e {
    cbr=0,
    vbr,               /* obsolete, same as vbr_mtrh */
    abr,
    vbr_max_indicator,    /* Don't use this! It's used for sanity checks. */

    vbr_default=vbr,  /* change this to change the default VBR mode of LAME */
    vbr_abr=abr,
    vbr_off=cbr
} vbr_mode;


/* MPEG modes */
typedef enum MPEG_mode_e {
  STEREO = 0,
  JOINT_STEREO,
  DUAL_CHANNEL,   /* LAME doesn't supports this! */
  MONO,
  NOT_SET,
  MAX_INDICATOR   /* Don't use this! It's used for sanity checks. */ 
} MPEG_mode;

/*asm optimizations*/
typedef enum asm_optimizations_e {
    MMX = 1,
    AMD_3DNOW = 2,
    SSE = 3
} asm_optimizations;


typedef struct lame_internal_flags *lame_t;


/***********************************************************************
 *
 *  The LAME API
 *  These functions should be called, in this order, for each
 *  MP3 file to be encoded.  See the file "API" for more documentation
 *
 ***********************************************************************/


/*
 * REQUIRED:
 * initialize the encoder.  sets default for all encoder parameters,
 * returns NULL if some malloc()'s failed
 * otherwise returns pointer to structure needed for all future
 * API calls.
 */
lame_t CDECL lame_init(void);


/*
 * OPTIONAL:
 * set as needed to override defaults
 */

/********************************************************************
 *  input stream description
 ***********************************************************************/
/* number of samples.  default = 2^32-1   */
int CDECL lame_set_num_samples(lame_t, unsigned long);
unsigned long CDECL lame_get_num_samples(lame_t);

/* input sample rate in Hz.  default = 44100hz */
int CDECL lame_set_in_samplerate(lame_t, int);
int CDECL lame_get_in_samplerate(lame_t);

/* number of channels in input stream. default=2  */
int CDECL lame_set_num_channels(lame_t, int);
int CDECL lame_get_num_channels(lame_t);

/*
  scale the input by this amount before encoding.  default=0 (disabled)
  (not used by decoding routines)
*/
int CDECL lame_set_scale(lame_t, float);
float CDECL lame_get_scale(lame_t);
    
/*
  scale the channel 0 (left) input by this amount before encoding.
    default=0 (disabled)
  (not used by decoding routines)
*/
int CDECL lame_set_scale_left(lame_t, float);
float CDECL lame_get_scale_left(lame_t);

/*
  scale the channel 1 (right) input by this amount before encoding.
    default=0 (disabled)
  (not used by decoding routines)
*/
int CDECL lame_set_scale_right(lame_t, float);
float CDECL lame_get_scale_right(lame_t);

/*
  output sample rate in Hz.  default = 0, which means LAME picks best value
  based on the amount of compression.  MPEG only allows:
  MPEG1    32, 44.1,   48khz
  MPEG2    16, 22.05,  24
  MPEG2.5   8, 11.025, 12
  (not used by decoding routines)
*/
int CDECL lame_set_out_samplerate(lame_t, int);
int CDECL lame_get_out_samplerate(lame_t);


/********************************************************************
 *  general control parameters
 ***********************************************************************/
/*
  1 = write a Xing VBR header frame.
  default = 1
  this variable must have been added by a Hungarian notation Windows programmer :-)
*/
int CDECL lame_set_bWriteVbrTag(lame_t, int);
int CDECL lame_get_bWriteVbrTag(lame_t);

/* 1=decode only.  use lame/mpglib to convert mp3/ogg to wav.  default=0 */
int CDECL lame_set_decode_only(lame_t, int);
int CDECL lame_get_decode_only(lame_t);

/*
  internal algorithm selection.
  True quality is determined by the bitrate but this variable will
  effect quality by selecting expensive or cheap algorithms.
  quality=0..9.  0=best (very slow).  9=worst.
  recommended:  2     near-best quality, not too slow
                5     good quality, fast
                7     ok quality, really fast
*/
int CDECL lame_set_quality(lame_t, int);
int CDECL lame_get_quality(lame_t);

/*
  mode = 0,1,2,3 = stereo, jstereo, dual channel (not supported), mono
  default: lame picks based on compression ration and input channels
*/
int CDECL lame_set_mode(lame_t, MPEG_mode);
MPEG_mode CDECL lame_get_mode(lame_t);

/*
  force_ms.  Force M/S for all frames.  For testing only.
  default = 0 (disabled)
*/
int CDECL lame_set_force_ms(lame_t, int);
int CDECL lame_get_force_ms(lame_t);

/* perform ReplayGain analysis?  default = 0 (disabled) */
int CDECL lame_set_findReplayGain(lame_t, int);
int CDECL lame_get_findReplayGain(lame_t);

/* decode on the fly. Search for the peak sample. If the ReplayGain
 * analysis is enabled then perform the analysis on the decoded data
 * stream. default = 0 (disabled) 
 * NOTE: if this option is set the build-in decoder should not be used */
int CDECL lame_set_decode_on_the_fly(lame_t, int);
int CDECL lame_get_decode_on_the_fly(lame_t);

/* Gain change required for preventing clipping. The value is correct only if 
   peak sample searching was enabled. If negative then the waveform 
   already does not clip. The value is multiplied by 10 and rounded up. */
int CDECL lame_get_noclipGainChange(lame_t);

/* user-specified scale factor required for preventing clipping. Value is 
   correct only if peak sample searching was enabled and no user-specified
   scaling was performed. If negative then either the waveform already does
   not clip or the value cannot be determined */
float CDECL lame_get_noclipScale(lame_t);

/* use free_format?  default = 0 (disabled) */
int CDECL lame_set_free_format(lame_t, int);
int CDECL lame_get_free_format(lame_t);

/* counters for gapless encoding */
int CDECL lame_set_nogap_total(lame_t, int);
int CDECL lame_get_nogap_total(lame_t);

int CDECL lame_set_nogap_currentindex(lame_t , int);
int CDECL lame_get_nogap_currentindex(lame_t);

/*
 * OPTIONAL:
 * Set printf like error/debug/message reporting functions.
 * The second argument has to be a pointer to a function which looks like
 *   void my_debugf(const char *format, ...)
 *   {
 *        va_list  args;
 *        va_start ( args, format );
 *
 *        vfprintf ( stderr, format, args );
 *   }
 * If you use NULL as the value of the pointer in the set function, the
 * lame buildin function will be used (prints to stderr).
 * To quiet any output you have to replace the body of the example function
 * with just "return;" and use it in the set function.
 */
int CDECL lame_set_errorf(lame_t, void (*func)(const char *, ...));
int CDECL lame_set_debugf(lame_t, void (*func)(const char *, ...));
int CDECL lame_set_msgf  (lame_t, void (*func)(const char *, ...));



/* set one of brate compression ratio.  default is compression ratio of 11.  */
int CDECL lame_set_brate(lame_t, int);
int CDECL lame_get_brate(lame_t);
int CDECL lame_set_compression_ratio(lame_t, float);
float CDECL lame_get_compression_ratio(lame_t);


int CDECL lame_set_asm_optimizations(lame_t, int, int);



/********************************************************************
 *  frame params
 ***********************************************************************/
/* mark as copyright.  default=0 */
int CDECL lame_set_copyright(lame_t, int);
int CDECL lame_get_copyright(lame_t);

/* mark as original.  default=1 */
int CDECL lame_set_original(lame_t, int);
int CDECL lame_get_original(lame_t);

/* error_protection.  Use 2 bytes from each frame for CRC checksum. default=0 */
int CDECL lame_set_error_protection(lame_t, int);
int CDECL lame_get_error_protection(lame_t);

/* MP3 'private extension' bit  Meaningless.  default=0 */
int CDECL lame_set_extension(lame_t, int);
int CDECL lame_get_extension(lame_t);

/* enforce strict ISO compliance.  default=0 */
int CDECL lame_set_strict_ISO(lame_t, int);
int CDECL lame_get_strict_ISO(lame_t);
 
void CDECL lame_set_forbid_diff_blocktype(lame_t, int val);

/********************************************************************
 * quantization/noise shaping 
 ***********************************************************************/

/* disable the bit reservoir. For testing only. default=0 */
int CDECL lame_set_disable_reservoir(lame_t, int);
int CDECL lame_get_disable_reservoir(lame_t);

int CDECL lame_set_tune_bass(lame_t, int);
int CDECL lame_set_tune_alto(lame_t, int);
int CDECL lame_set_tune_treble(lame_t, int);
int CDECL lame_set_tune_sfb21(lame_t, int);

void CDECL lame_set_msfix(lame_t, float);

/********************************************************************
 * VBR control
 ***********************************************************************/
/* Types of VBR.  default = vbr_off = CBR */
int CDECL lame_set_VBR(lame_t, vbr_mode);
vbr_mode CDECL lame_get_VBR(lame_t);

/* VBR quality level.  0=highest  9=lowest  */
int CDECL lame_set_VBR_q(lame_t, int);
int CDECL lame_get_VBR_q(lame_t);

/* Ignored except for VBR=vbr_abr (ABR mode) */
int CDECL lame_set_VBR_mean_bitrate_kbps(lame_t, int);
int CDECL lame_get_VBR_mean_bitrate_kbps(lame_t);

int CDECL lame_set_VBR_min_bitrate_kbps(lame_t, int);
int CDECL lame_get_VBR_min_bitrate_kbps(lame_t);

int CDECL lame_set_VBR_max_bitrate_kbps(lame_t, int);
int CDECL lame_get_VBR_max_bitrate_kbps(lame_t);

/********************************************************************
 * Filtering control
 ***********************************************************************/
/* freq in Hz to apply lowpass. Default = 0 = lame chooses.  -1 = disabled */
int CDECL lame_set_lowpassfreq(lame_t, int);
int CDECL lame_get_lowpassfreq(lame_t);
/* width of transition band, in Hz.  Default = one polyphase filter band */
int CDECL lame_set_lowpasswidth(lame_t, int);
int CDECL lame_get_lowpasswidth(lame_t);

/* freq in Hz to apply highpass. Default = 0 = lame chooses.  -1 = disabled */
int CDECL lame_set_highpassfreq(lame_t, int);
int CDECL lame_get_highpassfreq(lame_t);
/* width of transition band, in Hz.  Default = one polyphase filter band */
int CDECL lame_set_highpasswidth(lame_t, int);
int CDECL lame_get_highpasswidth(lame_t);


/********************************************************************
 * psycho acoustics and other arguments which you should not change 
 * unless you know what you are doing
 ***********************************************************************/
/* only use ATH for masking */
int CDECL lame_set_ATHonly(lame_t, int);
int CDECL lame_get_ATHonly(lame_t);

/* only use ATH for short blocks */
int CDECL lame_set_ATHshort(lame_t, int);
int CDECL lame_get_ATHshort(lame_t);

/* disable ATH */
int CDECL lame_set_noATH(lame_t, int);
int CDECL lame_get_noATH(lame_t);

/* select ATH formula shape */
int CDECL lame_set_ATHcurve(lame_t, float);
int CDECL lame_get_ATHcurve(lame_t);

/* lower ATH by this many db */
int CDECL lame_set_ATHlower(lame_t, float);
float CDECL lame_get_ATHlower(lame_t);

/* adjust (in dB) the point below which adaptive ATH level adjustment occurs */
int CDECL lame_set_athaa_sensitivity(lame_t, float);
float CDECL lame_get_athaa_sensitivity(lame_t);

/* intensity stereo usage ratio */
int CDECL lame_set_istereoRatio(lame_t, float);

/* use temporal masking effect (default = 1) */
int CDECL lame_set_interChRatio(lame_t, float);
float CDECL lame_get_interChRatio(lame_t);

/* Input PCM is emphased PCM (for instance from one of the rarely
   emphased CDs), it is STRONGLY not recommended to use this, because
   psycho does not take it into account, and last but not least many decoders
   ignore these bits */
int CDECL lame_set_emphasis(lame_t, int);
int CDECL lame_get_emphasis(lame_t);



/************************************************************************/
/* internal variables, cannot be set...                                 */
/* provided because they may be of use to calling application           */
/************************************************************************/
/* version  0=MPEG-2  1=MPEG-1  (2=MPEG-2.5)     */
int CDECL lame_get_version(lame_t);

/* encoder delay   */
int CDECL lame_get_encoder_delay(lame_t);

/*
  padding appended to the input to make sure decoder can fully decode
  all input.  Note that this value can only be calculated during the
  call to lame_encoder_flush().  Before lame_encoder_flush() has
  been called, the value of encoder_padding = 0.
*/
int CDECL lame_get_encoder_padding(lame_t);

/* size of MPEG frame */
int CDECL lame_get_framesize(lame_t);

/* number of PCM samples buffered, but not yet encoded to mp3 data. */
int CDECL lame_get_mf_samples_to_encode(lame_t);

/*
  size (bytes) of mp3 data buffered, but not yet encoded.
  this is the number of bytes which would be output by a call to 
  lame_encode_flush_nogap.  NOTE: lame_encode_flush() will return
  more bytes than this because it will encode the reamining buffered
  PCM samples before flushing the mp3 buffers.
*/
int CDECL lame_get_size_mp3buffer(lame_t);

/* number of frames encoded so far */
int CDECL lame_get_frameNum(lame_t);

/*
  lame's estimate of the total number of frames to be encoded
   only valid if calling program set num_samples
*/
int CDECL lame_get_totalframes(lame_t);

/* RadioGain value. Multiplied by 10 and rounded to the nearest. */
int CDECL lame_get_RadioGain(lame_t);

/* AudiophileGain value. Multipled by 10 and rounded to the nearest. */
int CDECL lame_get_AudiophileGain(lame_t);

/* the peak sample */
float CDECL lame_get_PeakSample(lame_t);










/*
 * REQUIRED:
 * sets more internal configuration based on data provided above.
 * returns -1 if something failed.
 */
int CDECL lame_init_params(lame_t);


/*
 * OPTIONAL:
 * get the version number, in a string. of the form:  
 * "3.63 (beta)" or just "3.63". 
 */
const char*  CDECL get_lame_version       ( void );
const char*  CDECL get_lame_short_version ( void );
const char*  CDECL get_lame_very_short_version ( void );
const char*  CDECL get_psy_version        ( void );
const char*  CDECL get_lame_url           ( void );


/*
 * OPTIONAL:
 * get the version numbers in numerical form.
 */
typedef struct {
    /* generic LAME version */
    int major;
    int minor;
    int alpha;               /* 0 if not an alpha version                  */
    int beta;                /* 0 if not a beta version                    */

    /* version of the psy model */
    int psy_major;
    int psy_minor;
    int psy_alpha;           /* 0 if not an alpha version                  */
    int psy_beta;            /* 0 if not a beta version                    */

    /* compile time features */
    const char *features;    /* Don't make assumptions about the contents! */
} lame_version_t;

void CDECL get_lame_version_numerical ( lame_version_t *const );


/*
 * OPTIONAL:
 * print internal lame configuration to message handler
 */
void CDECL lame_print_config(lame_t);

void CDECL lame_print_internals(lame_t);


/*
 * input pcm data, output (maybe) mp3 frames.
 * This routine handles all buffering, resampling and filtering for you.
 * 
 * return code     number of bytes output in mp3buf. Can be 0 
 *                 -1:  mp3buf was too small
 *                 -2:  malloc() problem
 *                 -3:  lame_init_params() not called
 *                 -4:  psycho acoustic problems 
 *
 * The required mp3buf_size can be computed from num_samples, 
 * samplerate and encoding rate, but here is a worst case estimate:
 *
 * mp3buf_size in bytes = 1.25*num_samples + 7200
 *
 * I think a tighter bound could be:  (mt, March 2000)
 * MPEG1:
 *    num_samples*(bitrate/8)/samplerate + 4*1152*(bitrate/8)/samplerate + 512
 * MPEG2:
 *    num_samples*(bitrate/8)/samplerate + 4*576*(bitrate/8)/samplerate + 256
 *
 * but test first if you use that!
 *
 * set mp3buf_size = 0 and LAME will not check if mp3buf_size is
 * large enough.
 *
 * NOTE:
 * if lame_t->num_channels=2, but lame_t->mode = 3 (mono), the L & R channels
 * will be averaged into the L channel before encoding only the L channel
 * This will overwrite the data in buffer_l[] and buffer_r[].
 * 
*/
int CDECL lame_encode_buffer (
        lame_t,
        const short int     buffer_l [],   /* PCM data for left channel     */
        const short int     buffer_r [],   /* PCM data for right channel    */
        const int           nsamples,      /* number of samples per channel */
        unsigned char*      mp3buf,        /* pointer to encoded MP3 stream */
        const int           mp3buf_size ); /* number of valid octets in this
                                              stream                        */

/*
 * as above, but input has L & R channel data interleaved.
 * NOTE: 
 * num_samples = number of samples in the L (or R)
 * channel, not the total number of samples in pcm[]  
 */
int CDECL lame_encode_buffer_interleaved(
        lame_t,
        short int           pcm[],         /* PCM data for left and right
                                              channel, interleaved          */
        int                 num_samples,   /* number of samples per channel,
                                              _not_ number of samples in
                                              pcm[]                         */
        unsigned char*      mp3buf,        /* pointer to encoded MP3 stream */
        int                 mp3buf_size ); /* number of valid octets in this
                                              stream                        */


/* as lame_encode_buffer, but for 'float's.
 * !! NOTE: !! data must still be scaled to be in the same range as 
 * short int, +/- 32768  
 */
int CDECL lame_encode_buffer_float(
        lame_t,
        const float     buffer_l [],   /* PCM data for left channel     */
        const float     buffer_r [],   /* PCM data for right channel    */
        const int       nsamples,      /* number of samples per channel */
        unsigned char*  mp3buf,        /* pointer to encoded MP3 stream */
        const int       mp3buf_size ); /* number of valid octets in this
					  stream                        */


/* as lame_encode_buffer, for 'float's.
 * !! NOTE: !! data must be scaled to be in the same range as 
 * short int, +/- 2^31
 */
int CDECL lame_encode_buffer_float2(
        lame_t,
        const float     buffer_l [],   /* PCM data for left channel     */
        const float     buffer_r [],   /* PCM data for right channel    */
        const int       nsamples,      /* number of samples per channel */
        unsigned char*  mp3buf,        /* pointer to encoded MP3 stream */
        const int       mp3buf_size ); /* number of valid octets in this
					  stream                        */


/* as lame_encode_buffer, but for long's 
 * !! NOTE: !! data must still be scaled to be in the same range as 
 * short int, +/- 32768  
 *
 * This scaling was a mistake (doesn't allow one to exploit full
 * precision of type 'long'.  Use lame_encode_buffer_long2() instead.
 *
 */
int CDECL lame_encode_buffer_long(
        lame_t,
        const long     buffer_l [],   /* PCM data for left channel     */
        const long     buffer_r [],   /* PCM data for right channel    */
        const int      nsamples,      /* number of samples per channel */
        unsigned char* mp3buf,        /* pointer to encoded MP3 stream */
        const int      mp3buf_size ); /* number of valid octets in this
                                              stream                        */

/* Same as lame_encode_buffer_long(), but with correct scaling. 
 * !! NOTE: !! data must still be scaled to be in the same range as  
 * type 'long'.   Data should be in the range:  +/- 2^(8*size(long)-1)
 *
 */
int CDECL lame_encode_buffer_long2(
        lame_t,
        const long     buffer_l [],   /* PCM data for left channel     */
        const long     buffer_r [],   /* PCM data for right channel    */
        const int      nsamples,      /* number of samples per channel */
        unsigned char* mp3buf,        /* pointer to encoded MP3 stream */
        const int      mp3buf_size ); /* number of valid octets in this
                                         stream                        */

/* as lame_encode_buffer, but for int's 
 * !! NOTE: !! input should be scaled to the maximum range of 'int'
 * If int is 4 bytes, then the values should range from
 * +/- 2147483648.  
 *
 * This routine does not (and cannot, without loosing precision) use
 * the same scaling as the rest of the lame_encode_buffer() routines.
 * 
 */   
int CDECL lame_encode_buffer_int(
        lame_t,
        const int      buffer_l [],   /* PCM data for left channel     */
        const int      buffer_r [],   /* PCM data for right channel    */
        const int      nsamples,      /* number of samples per channel */
        unsigned char* mp3buf,        /* pointer to encoded MP3 stream */
        const int      mp3buf_size ); /* number of valid octets in this
                                         stream                        */





/*
 * REQUIRED:
 * lame_encode_flush will flush the intenal PCM buffers, padding with 
 * 0's to make sure the final frame is complete, and then flush
 * the internal MP3 buffers, and thus may return a 
 * final few mp3 frames.  'mp3buf' should be at least 7200 bytes long
 * to hold all possible emitted data.
 *
 * will also write id3v1 tags (if any) into the bitstream       
 *
 * return code = number of bytes output to mp3buf. Can be 0
 */
int CDECL lame_encode_flush(
        lame_t,
        unsigned char* mp3buf, /* pointer to encoded MP3 stream         */
        int            size);  /* number of valid octets in this stream */

/*
 * OPTIONAL:
 * lame_encode_flush_nogap will flush the internal mp3 buffers and pad
 * the last frame with ancillary data so it is a complete mp3 frame.
 * 
 * 'mp3buf' should be at least 7200 bytes long
 * to hold all possible emitted data.
 *
 * After a call to this routine, the outputed mp3 data is complete, but
 * you may continue to encode new PCM samples and write future mp3 data
 * to a different file.  The two mp3 files will play back with no gaps
 * if they are concatenated together.
 *
 * This routine will NOT write id3v1 tags into the bitstream.
 *
 * return code = number of bytes output to mp3buf. Can be 0
 */
int CDECL lame_encode_flush_nogap(
        lame_t,
        unsigned char* mp3buf, /* pointer to encoded MP3 stream         */
        int            size);  /* number of valid octets in this stream */

/*
 * OPTIONAL:
 * Normally, this is called by lame_init_params().  It writes id3v2 and
 * Xing headers into the front of the bitstream, and sets frame counters
 * and bitrate histogram data to 0.  You can also call this after 
 * lame_encode_flush_nogap().  
 */
int CDECL lame_init_bitstream(lame_t);



/*
 * OPTIONAL:    some simple statistics
 *
 * a bitrate histogram to visualize the distribution of used frame sizes
 * there are 14 possible bitrate indices, 0 has the special meaning 
 * "free format" which is not possible to mix with VBR and 15 is forbidden
 * anyway. One has to weight them to calculate the average bitrate in kbps
 *
 * a stereo mode histogram to visualize the distribution of used stereo
 * modes, useful in joint-stereo mode only
 *   0: LR    left-right encoded
 *   1: LR-I  left-right and intensity encoded (currently not supported)
 *   2: MS    mid-side encoded
 *   3: MS-I  mid-side and intensity encoded (currently not supported)
 *
 *  4: number of encoded frames
 *
 * attention: don't call them after lame_encode_finish
 * suggested: lame_encode_flush -> lame_*_hist -> lame_close
 */

void CDECL lame_bitrate_hist(lame_t, int bitrate_count[14]);
void CDECL lame_stereo_mode_hist(lame_t, int stereo_mode_count[4]);
void CDECL lame_block_type_hist(lame_t, int btype_count[6]);

void CDECL lame_bitrate_stereo_mode_hist(
    lame_t, int bitrate_stmode_count[14][4]);

void CDECL lame_bitrate_block_type_hist(
    lame_t, int bitrate_btype_count[14][6]);

/*
 * OPTIONAL:
 * lame_mp3_tags_fid will append a Xing VBR tag to the mp3 file with file
 * pointer fid.  These calls perform forward and backwards seeks, so make
 * sure fid is a real file.  Make sure lame_encode_flush has been called,
 * and all mp3 data has been written to the file before calling this
 * function.
 * NOTE:
 * if VBR tags are turned off by the user, or turned off by LAME because
 * the output is not a regular file, this call does nothing
*/
int CDECL lame_mp3_tags_fid(lame_t, FILE* fid);


/*
 * REQUIRED:
 * final call to free all remaining buffers
 */
int  CDECL lame_close (lame_t);


/*********************************************************************
 *
 * decoding 
 *
 * a simple interface to mpglib, part of mpg123, is also included if
 * libmp3lame is compiled with HAVE_MPGLIB
 *
 *********************************************************************/
typedef struct {
  int header_parsed;   /* 1 if header was parsed and following data was
                          computed                                       */
  int channels;        /* number of channels                             */
  int samplerate;      /* sample rate                                    */
  int bitrate;         /* bitrate                                        */
  int mode;            /* mp3 frame type                                 */
  int mode_ext;        /* mp3 frame type                                 */
  int framesize;       /* number of samples per mp3 frame                */

  /* this data is only computed if mpglib detects a Xing VBR header */
  unsigned long nsamp; /* number of samples in mp3 file.                 */
  int totalframes;     /* total number of frames in mp3 file             */

  /* this data is not currently computed by the mpglib routines */
  int framenum;        /* frames decoded counter                         */
} mp3data_struct;


/* structure to receive extracted header (VBR header) */
#define NUMTOCENTRIES 100

#define FRAMES_FLAG     0x0001
#define BYTES_FLAG      0x0002
#define TOC_FLAG        0x0004
#define VBR_SCALE_FLAG  0x0008

/* toc may be NULL*/
typedef struct
{
    int h_id;		/* from MPEG header, 0=MPEG2, 1=MPEG1 */
    int samprate;	/* determined from MPEG header */
    int flags;		/* from Vbr header data */
    int frames;		/* total bit stream frames from Vbr header data */
    int bytes;		/* total bit stream bytes from Vbr header data*/
    int vbr_scale;	/* encoded vbr scale from Vbr header data*/
    unsigned char toc[NUMTOCENTRIES];	/* may be NULL if toc not desired*/
    int headersize;	/* size of VBR header, in bytes */
    int enc_delay;	/* encoder delay */
    int enc_padding;	/* encoder paddign added at end of stream */
}   VBRTAGDATA;

/* required call to initialize decoder */
int CDECL lame_decode_init(lame_t);

/*********************************************************************
 * input 1 mp3 frame, output (maybe) pcm data.  
 *
 *  nout = lame_decode(mp3buf,len,pcm_l,pcm_r);
 *
 * input:  
 *    len          :  number of bytes of mp3 data in mp3buf
 *    mp3buf[len]  :  mp3 data to be decoded
 *
 * output:
 *    nout:  -1    : decoding error
 *            0    : need more data before we can complete the decode 
 *           >0    : returned 'nout' samples worth of data in pcm_l,pcm_r
 *    pcm_l[nout]  : left channel data
 *    pcm_r[nout]  : right channel data 
 *    
 *********************************************************************/
int CDECL lame_decode(
    lame_t,
    unsigned char *  mp3buf,
    int              len,
    short            pcm_l[],
    short            pcm_r[] );

/* same as lame_decode, and also returns mp3 header data */
int CDECL lame_decode_headers(
    lame_t,
    unsigned char*   mp3buf,
    int              len,
    short            pcm_l[],
    short            pcm_r[],
    mp3data_struct*  mp3data );

/* same as lame_decode, but returns at most one frame */
int CDECL lame_decode1(
    lame_t,
    unsigned char*  mp3buf,
    int             len,
    short           pcm_l[],
    short           pcm_r[] );

/* same as lame_decode1, but returns at most one frame and mp3 header data */
int CDECL lame_decode1_headers(
    lame_t,
    unsigned char*   mp3buf,
    int              len,
    short            pcm_l[],
    short            pcm_r[],
    mp3data_struct*  mp3data );

/* same as lame_decode1_headers, but returns float values */
int CDECL lame_decode1_headers_noclip(
    lame_t,
    unsigned char*  mp3buf,
    int             len,
    float           pcm_l[],
    float           pcm_r[],
    mp3data_struct*  mp3data );

/* same as lame_decode1_headers, but also returns enc_delay and enc_padding
   from VBR Info tag, (-1 if no info tag was found) */
int CDECL lame_decode1_headersB(
    lame_t,
    unsigned char*   mp3buf,
    int              len,
    short            pcm_l[],
    short            pcm_r[],
    mp3data_struct*  mp3data,
    int              *enc_delay,
    int              *enc_padding );


/* cleanup call to exit decoder  */
int CDECL lame_decode_exit(lame_t);



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
extern void id3tag_genre_list(
        void (*handler)(int, const char *, void *),
        void*  cookie);

extern void id3tag_init(lame_t);

/* command line is in UTF-8 */
extern void id3tag_u8       (lame_t);

/* force addition of version 2 tag */
extern void id3tag_add_v2   (lame_t);

/* add only a version 1 tag */
extern void id3tag_v1_only  (lame_t);

/* add only a version 2 tag */
extern void id3tag_v2_only  (lame_t);

/* pad version 1 tag with spaces instead of nulls */
extern void id3tag_space_v1 (lame_t);

/* pad version 2 tag with extra 128 bytes */
extern void id3tag_pad_v2   (lame_t);

extern void id3tag_set_title  (lame_t, const char* title);
extern void id3tag_set_artist (lame_t, const char* artist);
extern void id3tag_set_album  (lame_t, const char* album);
extern void id3tag_set_year   (lame_t, const char* year);
extern void id3tag_set_comment(lame_t, const char* comment);
extern void id3tag_set_track  (lame_t, const char* track);

/* return non-zero result if genre name or number is invalid */
extern int id3tag_set_genre   (lame_t, const char* genre);

/***********************************************************************
*
*  list of valid bitrates [kbps] & sample frequencies [Hz].
*  first index: 0: MPEG-2   values  (sample frequencies 16...24 kHz)
*               1: MPEG-1   values  (sample frequencies 32...48 kHz)
*               2: MPEG-2.5 values  (sample frequencies  8...12 kHz)
***********************************************************************/
extern const int      bitrate_table    [3] [16];
extern const int      samplerate_table [3] [ 4];

#define NORM_TYPE     0
#define SHORT_TYPE    3 /* in the spec, 2 */
#define START_TYPE    1
#define STOP_TYPE     2 /* in the spec, 3 */



/* maximum size of mp3buffer needed if you encode at most 1152 samples for
   each call to lame_encode_buffer.  see lame_encode_buffer() below  
   (LAME_MAXMP3BUFFER is now obsolete)  */
#define LAME_MAXMP3BUFFER   16384


typedef enum {
    LAME_OKAY             =   0,
    LAME_NOERROR          =   0,
    LAME_GENERICERROR     =  -1,
    LAME_NOTINIT          =  -3,
    LAME_INSUFFICIENTBUF  =  -5,
    LAME_NOMEM            = -10,
    LAME_BADBITRATE       = -11,
    LAME_BADSAMPFREQ      = -12,
    LAME_INTERNALERROR    = -13,

    LAME_SEEKERROR    = -80,
    LAME_WRITEERROR   = -81,
    LAME_FILETOOLARGE = -82
} lame_errorcodes_t;

#if defined(__cplusplus)
}
#endif
#endif /* LAME_LAME_H */
