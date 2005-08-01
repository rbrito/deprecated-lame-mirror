/*
 *      encoder.h include file
 *
 *      Copyright (c) 2000 Mark Taylor
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef LAME_ENCODER_H
#define LAME_ENCODER_H

#include "lame.h"

/***********************************************************************
*
*  encoder and decoder delays
*
***********************************************************************/

/* 
 * layer III enc->dec delay:  1056 (1057?)   (observed)
 * layer  II enc->dec delay:   480  (481?)   (observed)
 *
 * polyphase 256-16             (dec or enc)        = 240
 * mdct      256+32  (9*32)     (dec or enc)        = 288
 * total:    528
 *
 * My guess is that delay of polyphase filterbank is actualy 240.5
 * (there are technical reasons for this, see postings in mp3encoder).
 * So total Encode+Decode delay = ENCDELAY + 528 + 1
 */

/* 
 * ENCDELAY. The extra encoder delay.
 * very the 1st frame is always all zero, so the "normal" dealy is framesize.
 * extra delay is for adjusting the delay to the other encoders.
 */
#define ENCDELAY      0

/* 
 * delay of the MDCT used in mdct.c
 * original ISO routines had a delay of 528!
 */
#define MDCTDELAY     48
#define FFTOFFSET     ((BLKSIZE-576)/2+MDCTDELAY)

/* FFT sizes */
#define BLKSIZE       1024
#define HBLKSIZE      (BLKSIZE/2 + 1)
#define BLKSIZE_s     256

#define MFSIZE  (FFTOFFSET + 576*2 + 512 - SBLIMIT + ENCDELAY)

/*
 * make sure there is at least one complete frame after the
 * last frame containing real data
 *
 * Using a value of 288(=576/2) would be sufficient for a 
 * a very sophisticated decoder that can decode granule-by-granule instead
 * of frame by frame.  But lets not assume this, and assume the decoder  
 * will not decode frame N unless it also has data for frame N+1
 *
 */
#define POSTDELAY	(gfc->framesize)

/*
 * Most decoders, including the one we use, have a delay of 528 samples.  
 */
#define DECDELAY      528

/* number of subbands */
#define SBLIMIT       32

/* max of parition bands bands */
#define CBANDS        64

/* number of critical bands/scale factor bands where masking is computed*/
#define SBPSY_l       21
#define SBPSY_s       12

/* total number of scalefactor bands encoded */
#define SBMAX_l       22
#define SBMAX_s       13

/* max scalefactor band, max(SBMAX_l, SBMAX_s*3, (SBMAX_s-3)*3+8) */
#define SFBMAX (SBMAX_s*3)

/* 
 * Mode Extention:
 * When we are in stereo mode, there are 4 possible methods to store these
 * two channels. The stereo modes -m? are using a subset of them.
 *
 *  -ms: MPG_MD_LR_LR
 *  -mf: MPG_MD_MS_LR
 *  -mj: all
 */
 
#define MPG_MD_LR_LR  0
#define MPG_MD_LR_I   1
#define MPG_MD_MS_LR  2
#define MPG_MD_MS_I   3

#define	MAX_BITS	4096

#define MAX_CHANNELS  2
#define MAX_GRANULES  2

/***********************************************************************
*
*  Global Include Files
*
***********************************************************************/
#include "machine.h"
#include "lame-analysis.h"
#include "gain_analysis.h"

/*
 * internal data structure definition.
 */
/* bitstream handling structure */
typedef struct {
    int totbyte;        /* byte counter of bit stream */
    int bitidx;         /* pointer to top byte in buffer */
    unsigned char buf[LAME_MAXMP3BUFFER];         /* bit stream buffer */
    /* mpeg1: buffer=511 bytes  smallest frame: 96-38(sideinfo)=58
     * max number of frames in reservoir:  8 
     * mpeg2: buffer=255 bytes.  smallest frame: 24-23bytes=1
     * with VBR, if you are encoding all silence, it is possible to
     * have 8kbs/24khz frames with 1byte of data each, which means we need
     * to buffer up to 255 headers! */
    /* also, max_header_buf has to be a power of two */
#define MAX_HEADER_BUF 256
#define MAX_HEADER_LEN 40 /* max size of header is 38 */
    struct {
	int write_timing;
	char buf[MAX_HEADER_LEN];
    } header[MAX_HEADER_BUF];
    int h_ptr;
    int w_ptr;
} bit_stream_t;

/* scalefactor band start/end positions */
typedef struct 
{
    short l[1+SBMAX_l];
    short s[1+SBMAX_s];
} scalefac_struct;

/* structures for psymodel */
typedef struct {
    FLOAT	l[SBMAX_l];
    FLOAT	s[SBMAX_s][3];
} III_psy;

typedef struct {
    III_psy thm;
    III_psy en;
    FLOAT	pe;
} III_psy_ratio;

typedef struct {
    short width;
    short window;
} winfo_t;

typedef struct {
    FLOAT xr[576];
    int l3_enc[576];
    short scalefac[SFBMAX];

    char region0_count;
    char region1_count;
    int part2_length;
    int part2_3_length;
    int count1bits;
    int big_values;
    int count1;
    int table_select[3+1]; /* last one for count1 region */
    int scalefac_compress;

    int global_gain;

    int preflag;
    int scalefac_scale;
    char subblock_gain[3+1];

    int block_type;
    int mixed_block_flag;
    int sfb_lmax;
    int sfb_smin;
    int psy_lmax;
    int sfbmax;
    int psymax;
    int sfbdivide;
    int xrNumMax;
    winfo_t *wi;
    FLOAT ATHadjust;

    unsigned char slen[4];
#if SIZEOF_LONG!=8
    int dummy_for_padding[2];
#endif
} gr_info;

typedef struct {
    int sum;    /* what we have seen so far */
    int seen;   /* how many frames we have seen in this chunk */
    int want;   /* how many frames we want to collect into one chunk */
    int pos;    /* actual position in our bag */
    int size;   /* size of our bag */
    int *bag;   /* pointer to our bag */
} VBR_seek_info_t;

#ifdef HAVE_MPGLIB
#include "../mpglib/interface.h"
#endif

/*
 * encoder instance.
 */
struct lame_internal_flags {
    /* variables for subband filter and MDCT */
    union {
	FLOAT sb_smpl[MAX_CHANNELS][3][18][SBLIMIT];
	FLOAT xrwork[2][576];  /* xr^(3/4) and fabs(xr) */
    } w;

    /* side information */
    gr_info tt[MAX_GRANULES][MAX_CHANNELS];
    FLOAT maxXR[SFBMAX];
    char scfsi[MAX_CHANNELS];

    /* precalculated width information */
    winfo_t w_long[SBMAX_l];
    winfo_t w_short[SBMAX_s*3];
    winfo_t w_mixed[SBMAX_s*3-1];

    int main_data_begin;  /* in bytes */
    int ResvSize; /* in bits */
    int ResvMax;  /* in bits */
    int maxmp3buf; /* in bytes */
    int sideinfo_len;
    int mode_gr;        /* granules per frame */
    int channels_out;   /* number of channels in the output data stream (not used for decoding) */

    int decode_on_the_fly;
    int RadioGain;
    int AudiophileGain;

    replaygain_t rgdata;

    int findReplayGain;    /* find the RG value? default=1 */
    sample_t PeakSample;
    int noclipGainChange;  /* gain change required for preventing clipping */
    FLOAT noclipScale;     /* user-specified scale factor required for preventing clipping */

    FLOAT amp_filter0[SBLIMIT];
    FLOAT amp_filter1[SBLIMIT*6];
    int xrNumMax_longblock;
/*
 * Some remarks to the Class_ID field:
 * The Class ID is an Identifier for a pointer to this struct.
 * It is very unlikely that a pointer to lame_t has the same 32 bits
 * in it's structure (large and other special properties, for instance prime).
 *
 * To test that the structure is right and initialized, use:
 *   if ( gfc -> Class_ID == LAME_ID ) ...
 */
#define LAME_ID 0xFFF88E3B

    unsigned long Class_ID;
    int alignment;
    int frameNum;                   /* number of frames encoded */

    sample_t mfbuf[MAX_CHANNELS][MFSIZE];
    int mf_size;
    int mf_needed;

    int channels_in;	/* number of channels in the input data stream (PCM or decoded PCM) */

    int framesize;
    int VBR_min_bitrate;            /* min bitrate index */
    int VBR_max_bitrate;            /* max bitrate index */
    int bitrate_index;
    int samplerate_index;
    int mode_ext;

    FLOAT narrowStereo;        /* stereo image narrowen factor */
    FLOAT reduce_side;         /* side channel PE reduce factor */
    FLOAT istereo_ratio;       /* intensity stereo threshold */
    FLOAT interChRatio;

    int filter_type;     /* 0=polyphase filter, 1=MDCT filter 2=FIR filter */
    int use_scalefac_scale;   /* 0 = not use  1 = use */
    int use_subblock_gain;    /* 0 = not use  1 = use */
    int noise_shaping_amp;    /* 0 = ISO model: amplify all distorted bands
				 1 = amplify within 50% of max (on db scale)
				 2 = amplify only most distorted band
				 3 = half amplify only most distorted band
				 4 = after noise shaping, try to reduce noise
				     by decreasing scalefactor
				 5 = reduce "total noise" if possible
			      */
    int substep_shaping;  /* 0 = no substep
			     1 = use substep only long
			     2 = use substep only short
			     3 = use substep all block type.
			  */
    int psymodel;         /* 2 = use psychomodel and noise shaping.
			     1 = use psychomodel but no noise shaping.
			     0 = don't use psychomodel
			  */
    int use_best_huffman;   /* 0 = no.  1=outside loop  2=inside loop(slow) */
    int use_istereo;           /* use intensity stereo */
    int mixed_blocks;          /* use mixed blocks */

    /* padding */
    int padding;        /* padding for the current frame? */
    int frac_SpF;
    int slot_lag;

    /* psymodel constants */
    FLOAT *s3_ss;
    FLOAT *s3_ll;

    FLOAT mld_l[SBMAX_l],mld_s[SBMAX_s];
    int	bo_l[SBMAX_l], bo_s[SBMAX_s], bo_l2s[SBMAX_s];
    int	npart_l;

    int	s3ind[CBANDS][2];
    int	s3ind_s[CBANDS][2];

    int	endlines_s[CBANDS];
    int	numlines_l[CBANDS];
    FLOAT rnumlines_l[CBANDS];
    FLOAT rnumlines_ls[CBANDS];
    FLOAT masking_lower;
    FLOAT masking_lower_short;
    int bytes_diff;                 /* for ABR encoding */
    FLOAT masklower_base;           /* for ABR encoding */

    /* psymodel work, (for next frame data) */
    III_psy_ratio masking_next[MAX_GRANULES][MAX_CHANNELS*2];
    char blocktype_next[MAX_GRANULES][MAX_CHANNELS*2]; /* for block type */
    char mode_ext_next;
    char is_start_sfb_l_next[MAX_GRANULES];
    char is_start_sfb_s_next[MAX_GRANULES];
    char is_start_sfb_l[MAX_GRANULES];
    char is_start_sfb_s[MAX_GRANULES];

    struct {
	/* short block tuning */
	FLOAT	subbk_ene[MAX_CHANNELS*2][12];
	FLOAT	attackthre;
	FLOAT	decay;
	int	switching_band;

	/* adjustment of Mid/Side maskings from LR maskings */
	FLOAT msfix;
	int tuneBass;
	int tuneAlto;
	int tuneTreble;
	int tuneSFB21;
    } nsPsy;

    /**
     *  ATH related stuff, if something new ATH related has to be added,
     *  please plugg it here into the ATH_t struct
     */
    struct {
	FLOAT l[SBMAX_l];     /* ATH for sfbs in long blocks */
	FLOAT s[SBMAX_s];     /* ATH for sfbs in short blocks */
	FLOAT cb[CBANDS];     /* ATH for convolution bands */
	FLOAT l_avg[SBMAX_l];
	FLOAT s_avg[SBMAX_s];

	/* factor for tuning the (sample power) point below which adaptive
	 * threshold of hearing adjustment occurs */
	FLOAT aa_decay;
	FLOAT adjust[MAX_CHANNELS*2]; /* ATH lowering factor based on peak volume */
	FLOAT eql_w[CBANDS];  /* equal loudness weights (based on ATH) */
    } ATH;

    /* quantization */
    short OldValue[MAX_CHANNELS];
    short CurrentStep[MAX_CHANNELS];
    scalefac_struct scalefac_band;

    char bv_scf[576];
    char pseudohalf[SFBMAX];

    /* default cutoff(the max sfb where we should do noise-shaping) */
    char cutoff_sfb_l;
    char cutoff_sfb_s;

    /* variables for bitstream.c */
    bit_stream_t bs;

    /* optional ID3 tags, used in id3tag.c  */
    struct {
	/* private data members */
	int flags;
	const char *title;
	const char *artist;
	const char *album;
	const char *track;
	const char *comment;
	int year;
	int tracknum;
	int genre;
	int utf8;
    }  tag_spec;
    uint16_t nMusicCRC;

    /* VBR tags. */
    VBR_seek_info_t seekTable; /* used for Xing VBR header */

    struct {
	void (*msgf)  (const char *format, ...);
	void (*debugf)(const char *format, ...);
	void (*errorf)(const char *format, ...);
    } report;

    /* variables used for resampling */
    /* BPC = maximum number of filter convolution windows to precompute */
#define BPC 320
    struct {
	FLOAT itime[MAX_CHANNELS];
	sample_t *inbuf_old[MAX_CHANNELS];
	FLOAT *blackfilt;
	int filter_l;
	int bpc;
	FLOAT ratio;      /* input_samp_rate/output_samp_rate */
    } resample;

    int (*scale_bitcounter)(gr_info * const gi);
#if HAVE_NASM
    /* functions to replace with CPU feature optimized one in takehiro.c */
    int (*ix_max)(const int *ix, const int *end);
    void (*fft_fht)(FLOAT *, int);

    /* CPU features */
    struct {
	unsigned int  MMX       : 1; /* Pentium MMX, Pentium II...IV, K6, K6-2,
					K6-III, Athlon */
	unsigned int  MMX2      : 1; /* Pentium III, IV, M, Athlon, K8 */
	unsigned int  AMD_3DNow : 1; /* K6-2, K6-III, Athlon, K8 */
	unsigned int  AMD_E3DNow: 1; /* Athlon, K8               */
	unsigned int  SSE      : 1; /* Pentium III, Pentium 4    */
	unsigned int  SSE2     : 1; /* Pentium 4, M, K8          */
    } CPU_features;
    struct {
	int mmx;
	int amd3dnow;
	int sse;
    } asm_optimizations;
#endif

#ifdef BRHIST
    /* simple statistics */
    int bitrate_stereoMode_Hist[16][4];
    int	bitrate_blockType_Hist [16][4+1];/*norm/start/stop/short/mixed(short)*/
#endif
#ifndef NOANALYSIS
    /* used by the frame analyzer */
    plotting_data *pinfo;
#endif

    /* input description */
    unsigned long num_samples; /* number of samples. default=2^32-1 */
    int in_samplerate;         /* input_samp_rate in Hz. default=44.1 kHz */
    int out_samplerate;        /* output_samp_rate.
				  default: LAME picks best value 
				  at least not used for MP3 decoding:
				  Remember 44.1 kHz MP3s and AC97 */
    FLOAT scale;               /* scale input by this amount before encoding
				  at least not used for MP3 decoding */
    FLOAT scale_left;          /* scale input of channel 0 (left) by this
				  amount before encoding */
    FLOAT scale_right;         /* scale input of channel 1 (right) by this
				  amount before encoding */

    /* general control params */
    int bWriteVbrTag;          /* add Xing VBR tag? */

    int quality;               /* quality setting 0=best, 9=worst  default=5 */
    MPEG_mode mode;            /* see enum in lame.h
				  default = LAME picks best value */
    int force_ms;              /* force M/S mode. */
    int free_format;           /* use free format? default=0 */

    /*
     * set either mean_bitrate_kbps>0  or compression_ratio>0,
     * LAME will compute the value of the variable not set.
     * Default is compression_ratio = 11.025
     */
    int mean_bitrate_kbps;      /* bitrate */
    FLOAT compression_ratio;    /* sizeof(wav file)/sizeof(mp3 file) */

    /* frame params */
    int copyright;              /* mark as copyright. default=0           */
    int original;               /* mark as original. default=1            */
    int error_protection;       /* use 2 bytes per frame for a CRC
				   checksum. default=0                    */
    int extension;              /* the MP3 'private extension' bit.
				   Meaningless                            */
    int emphasis;               /* Input PCM is emphased PCM (for
				   instance from one of the rarely
				   emphased CDs), it is STRONGLY not
				   recommended to use this, because
				   psycho does not take it into account,
				   and last but not least many decoders
				   don't care about these bits          */
    int version;                /* 0=MPEG-2/2.5  1=MPEG-1               */
    int strict_ISO;             /* enforce ISO spec as much as possible */
    int forbid_diff_type;	/* forbid different block type */

    /* quantization/noise shaping */
    int disable_reservoir;      /* use bit reservoir? */

    /* VBR control */
    vbr_mode VBR;
    int VBR_q;
    int VBR_min_bitrate_kbps;
    int VBR_max_bitrate_kbps;

    /* lowpass and highpass filter control */
    int lowpassfreq;   /* freq in Hz. 0=lame choses, -1=no filter */
    int highpassfreq;  /* freq in Hz. 0=lame choses. -1=no filter */
    int lowpasswidth;  /* freq width of filter, in Hz (default=15%) */
    int highpasswidth; /* freq width of filter, in Hz (default=15%) */
    FLOAT lowpass1,lowpass2;   /* normalized frequency bounds of passband */
    FLOAT highpass1,highpass2; /* normalized frequency bounds of passband */

    /*
     * psycho acoustics and other arguments which you should not change 
     * unless you know what you are doing
     */
    int ATHonly;                    /* only use ATH                         */
    int ATHshort;                   /* only use ATH for short blocks        */
    int noATH;                      /* disable ATH                          */
    FLOAT ATHcurve;                 /* change ATH formula 4 shape           */
    FLOAT ATHlower;                 /* lower ATH by this many db            */

    int encoder_padding;  /* number of samples of padding appended to input */

#ifdef HAVE_MPGLIB
    PMPSTR pmp;
    PMPSTR pmp_replaygain;
#endif

    int nogap_total;
    int nogap_current;

    int nsamples;
    sample_t *in_buffer;
};

#ifdef HAVE_MPGLIB
/* same as lame_decode1 (look in lame.h), but returns 
   unclipped raw floating-point samples. It is declared
   here, not in lame.h, because it returns LAME's 
   internal type sample_t. No more than 1152 samples 
   per channel are allowed. */
int decode1_unclipped(
    PMPSTR pmp,
    unsigned char*  mp3buf,
    int             len,
    sample_t        pcm_l[],
    sample_t        pcm_r[] );
#endif

int decode_init_for_replaygain(lame_t gfc);

#endif /* LAME_ENCODER_H */
