/*
 *	lame utility library include file
 *
 *	Copyright (c) 1999 Albert L Faber
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef LAME_UTIL_H
#define LAME_UTIL_H

/***********************************************************************
*
*  Global Include Files
*
***********************************************************************/
#include "machine.h"
#include "encoder.h"
#include "lame.h"
#include "lame_global_flags.h"
#include "lame-analysis.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************************
*
*  Global Definitions
*
***********************************************************************/

#ifndef FALSE
#define         FALSE                   0
#endif

#ifndef TRUE
#define         TRUE                    (!FALSE)
#endif

#ifdef UINT_MAX
# define         MAX_U_32_NUM            UINT_MAX
#else
# define         MAX_U_32_NUM            0xFFFFFFFF
#endif

#ifndef PI
# ifdef M_PI
#  define       PI                      M_PI
# else
#  define       PI                      3.14159265358979323846
# endif
#endif


#ifdef M_LN2
# define        LOG2                    M_LN2
#else
# define        LOG2                    0.69314718055994530942
#endif

#ifdef M_LN10
# define        LOG10                   M_LN10
#else
# define        LOG10                   2.30258509299404568402
#endif


#ifdef M_SQRT2
# define        SQRT2                   M_SQRT2
#else
# define        SQRT2                   1.41421356237309504880
#endif


#define         CRC16_POLYNOMIAL        0x8005
#define		MAX_BITS		4095

#define         BUFFER_SIZE     LAME_MAXMP3BUFFER 

#define         Min(A, B)       ((A) < (B) ? (A) : (B))
#define         Max(A, B)       ((A) > (B) ? (A) : (B))

/* log/log10 approximations */
#ifdef TAKEHIRO_IEEE754_HACK
#define USE_FAST_LOG
#endif

#ifdef USE_FAST_LOG
# define LOG2_SIZE       (256)
# define LOG2_SIZE_L2    (8)
# define         FAST_LOG10(x)       (fast_log2(x)*(LOG2/LOG10))
# define         FAST_LOG(x)         (fast_log2(x)*LOG2)
# define         FAST_LOG10_X(x,y)   (fast_log2(x)*(LOG2/LOG10*(y)))
# define         FAST_LOG_X(x,y)     (fast_log2(x)*(LOG2*(y)))

extern ieee754_float32_t log_table[LOG2_SIZE*2];
inline static ieee754_float32_t fast_log2(ieee754_float32_t xx)
{
    ieee754_float32_t log2val;
    int mantisse, i;
    union {
	ieee754_float32_t f;
	int i;
    } x;

    x.f = xx;
    mantisse = x.i & 0x7FFFFF;
/*  assert(i > 0); */
    log2val = x.i >> 23;
    i = mantisse >> (23-LOG2_SIZE_L2);
    return log2val + log_table[i*2] + log_table[i*2+1]*mantisse;
}
#else
# define         FAST_LOG10(x)       log10(x)
# define         FAST_LOG(x)         log(x)
# define         FAST_LOG10_X(x,y)   (log10(x)*(y))
# define         FAST_LOG_X(x,y)     (log(x)*(y))
#endif






/***********************************************************************
*
*  Global Type Definitions
*
***********************************************************************/
/* bitstream handling structure */
typedef struct {
    int totbit;         /* bit counter of bit stream */
    int buf_byte_idx;   /* pointer to top byte in buffer */
    int buf_bit_idx;    /* pointer to top bit of top byte in buffer */
    unsigned char buf[BUFFER_SIZE];         /* bit stream buffer */
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
	int ptr;
	char buf[MAX_HEADER_LEN];
    } header[MAX_HEADER_BUF];
    int h_ptr;
    int w_ptr;
} Bit_stream_struc;

/* max scalefactor band, max(SBMAX_l, SBMAX_s*3, (SBMAX_s-3)*3+8) */
#define SFBMAX (SBMAX_s*3)

/* scalefactor band start/end positions */
typedef struct 
{
    int l[1+SBMAX_l];
    int s[1+SBMAX_s];
} scalefac_struct;


typedef struct {
    FLOAT	l[SBMAX_l];
    FLOAT	s[SBMAX_s][3];
} III_psy;

typedef struct {
    III_psy thm;
    III_psy en;
    FLOAT	pe;
    int	ath_over;
} III_psy_ratio;

typedef struct {
    FLOAT xr[576];
    int l3_enc[576];
    int scalefac[SFBMAX];

    int part2_length;
    int part2_3_length;
    int big_values;
    int count1;
    int global_gain;
    int scalefac_compress;
    int block_type;
    int mixed_block_flag;
    int table_select[3];
    int subblock_gain[3+1];
    int region0_count;
    int region1_count;
    int preflag;
    int scalefac_scale;
    int count1table_select;

    int sfb_lmax;
    int sfb_smin;
    int psy_lmax;
    int sfbmax;
    int psymax;
    int sfbdivide;
    int xrNumMax;
    int width[SFBMAX];
    int window[SFBMAX];
    int count1bits;
    /* added for LSF */
    int slen[4];
    int dummy_for_padding[3];
} gr_info;

/* Layer III side information. */
typedef struct {
    gr_info tt[2][2];
    int main_data_begin; 
    int private_bits;
    int ResvSize; /* in bits */
    int ResvMax;  /* in bits */
    int maxmp3buf; /* in bits */
    int sideinfo_len;
    int scfsi[2][4];
} III_side_info_t;

typedef struct 
{
    int sum;    /* what we have seen so far */
    int seen;   /* how many frames we have seen in this chunk */
    int want;   /* how many frames we want to collect into one chunk */
    int pos;    /* actual position in our bag */
    int size;   /* size of our bag */
    int *bag;   /* pointer to our bag */
} VBR_seek_info_t;


/* Guest structure, only temporarly here */

#define MAX_CHANNELS  2

typedef struct {
    unsigned long  Class_ID;        /* Class ID to recognize a resample_t
                                       object */
    FLOAT   sample_freq_in;  /* Input sample frequency in Hz */
    FLOAT   sample_freq_out; /* requested Output sample frequency in Hz */
    FLOAT   lowpass_freq;    /* lowpass frequency, this is the -6 dB
                                       point */
    int            scale_in;        /* the resampling is actually done by
                                       scale_out: */
    int            scale_out;       /* frequency is
				       samplefreq_in * scale_out / scal */
    int            taps;            /* number of taps for every FIR resample
                                       filter */

    sample_t**     fir;             /* the FIR resample filters:
                                         fir [scale_out] [taps */
    void*          firfree;         /* start address of the alloced memory for
                                       fir, */
    unsigned char* src_step;
    sample_t*      in_old       [MAX_CHANNELS];
    unsigned       fir_stepper  [MAX_CHANNELS];
    int            inp_stepper  [MAX_CHANNELS];

} resample_t;


/********************************************************************
 * internal variables NOT set by calling program, and should not be *
 * modified by the calling program                                  *
 ********************************************************************/
struct lame_internal_flags {
    /* most used variables */
    FLOAT xrwork[2][576];  /* xr^(3/4) and fabs(xr) */

    /* side information */
    III_side_info_t l3_side;

    /* variables for subband filter and MDCT */
    FLOAT sb_sample[2][3][18][SBLIMIT];
    FLOAT amp_filter[SBLIMIT];
    int xrNumMax_longblock;

/*  
 * Some remarks to the Class_ID field:
 * The Class ID is an Identifier for a pointer to this struct.
 * It is very unlikely that a pointer to lame_global_flags has the same 32 bits
 * in it's structure (large and other special properties, for instance prime).
 *
 * To test that the structure is right and initialized, use:
 *   if ( gfc -> Class_ID == LAME_ID ) ...
 * Other remark:
 *   If you set a flag to 0 for uninit data and 1 for init data, the right test
 *   should be "if (flag == 1)" and NOT "if (flag)". Unintended modification
 *   of this element will be otherwise misinterpreted as an init.
 */

#ifndef  MFSIZE
# define MFSIZE  ( 3*1152 + ENCDELAY - MDCTDELAY )
#endif
    sample_t     mfbuf [2] [MFSIZE];

#  define  LAME_ID   0xFFF88E3B
    unsigned long Class_ID;
    int alignment;

    int lame_encode_frame_init;
    int iteration_init_init;
    int fill_buffer_resample_init;

    int padding;          /* padding for the current frame? */
    int mode_gr;          /* granules per frame */
    int channels_in;	/* number of channels in the input data stream (PCM or decoded PCM) */
    int channels_out;     /* number of channels in the output data stream (not used for decoding) */
    resample_t*  resample_in;   /* context for coding (PCM=>MP3) resampling */
    resample_t*  resample_out;	/* context for decoding (MP3=>PCM) resampling */
    FLOAT samplefreq_in;
    FLOAT samplefreq_out;
    FLOAT resample_ratio;           /* input_samp_rate/output_samp_rate */

    lame_global_flags* gfp;     /* needed as long as the frame encoding functions must access gfp (all needed information can be added to gfc) */
    int          mf_samples_to_encode;
    int          mf_size;
    int VBR_min_bitrate;            /* min bitrate index */
    int VBR_max_bitrate;            /* max bitrate index */
    int bitrate_index;
    int samplerate_index;
    int mode_ext;


    /* lowpass and highpass filter control */
    FLOAT lowpass1,lowpass2;   /* normalized frequency bounds of passband */
    FLOAT highpass1,highpass2; /* normalized frequency bounds of passband */

    FLOAT narrowStereo;        /* stereo image narrowen factor */
    FLOAT reduce_side;         /* side channel PE reduce factor */

    int filter_type;          /* 0=polyphase filter, 1= FIR filter 2=MDCT filter(bad)*/
    int use_scalefac_scale;   /* 0 = not use  1 = use */
    int use_subblock_gain;    /* 0 = not use  1 = use */
    int noise_shaping_amp;    /* 0 = ISO model: amplify all distorted bands
				 1 = amplify within 50% of max (on db scale)
				 2 = amplify only most distorted band
				 3 = half amplify only most distorted band
				 4 = do not stop when all scalefacs amplified
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

    /* used for padding */
    int frac_SpF;
    int slot_lag;

    /* side channel sparsing */
    int   sparsing;
    FLOAT sparseA;
    FLOAT sparseB;

    /* intensity stereo threshold */
    FLOAT istereo_ratio;

    /* psymodel */
    FLOAT nb_1[4][CBANDS];
    FLOAT *s3_ss;
    FLOAT *s3_ll;
    FLOAT decay;

    FLOAT mld_l[SBMAX_l],mld_s[SBMAX_s];
    int	bo_l[SBMAX_l], bo_s[SBMAX_s], bo_l2s[SBMAX_s];
    int	npart_l,npart_s;

    int	s3ind[CBANDS][2];
    int	s3ind_s[CBANDS][2];

    int	endlines_s[CBANDS];
    int	numlines_l[CBANDS];
    FLOAT rnumlines_l[CBANDS];
    FLOAT rnumlines_ls[CBANDS];
    FLOAT masking_lower;

    /* for next frame data */
    III_psy_ratio masking_next[2][MAX_CHANNELS*2];
    FLOAT loudness_next[2][MAX_CHANNELS];  /* loudness^2 approx. per granule and channel */
    int blocktype_next[2][MAX_CHANNELS*2]; /* for block type */
    int mode_ext_next;
    int is_start_sfb_l[2];
    int is_start_sfb_s[2];
    int is_start_sfb_l_next[2];
    int is_start_sfb_s_next[2];

    struct {
	/* short block tuning */
	FLOAT	attackthre;
	FLOAT	attackthre_s;
	FLOAT	subbk_ene[MAX_CHANNELS*2][6];
	int	switching_band;

	/* adjustment of Mid/Side maskings from LR maskings */
	FLOAT msfix;
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
	FLOAT adjust;     /* lowering based on peak volume, 1 = no lowering */
	FLOAT adjust_limit;   /* limit for dynamic ATH adjust */
	FLOAT eql_w[BLKSIZE/2];/* equal loudness weights (based on ATH) */
	/* factor for tuning the (sample power) point below which adaptive
	 * threshold of hearing adjustment occurs 
	 */
	FLOAT aa_sensitivity_p;
    } ATH;

    /* quantization */
    int OldValue[2];
    int CurrentStep[2];
    scalefac_struct scalefac_band;

    char bv_scf[576];
    int pseudohalf[SFBMAX];

    /* default cutoff(the max sfb where we should do noise-shaping) */
    int cutoff_sfb_l;
    int cutoff_sfb_s;

    /* variables for bitstream.c */
    Bit_stream_struc   bs;
    int ancillary_flag;

    /* optional ID3 tags, used in id3tag.c  */
    struct id3tag_spec {
	/* private data members */
	int flags;
	const char *title;
	const char *artist;
	const char *album;
	int year;
	const char *comment;
	int track;
	int genre;
    }  tag_spec;
    uint16_t nMusicCRC;

    unsigned int crcvalue;
    VBR_seek_info_t VBR_seek_table; /* used for Xing VBR header */
  
    int nogap_total;
    int nogap_current;  

    struct {
	void (*msgf)  (const char *format, va_list ap);
	void (*debugf)(const char *format, va_list ap);
	void (*errorf)(const char *format, va_list ap);
    } report;

  /* variables used by util.c */
  /* BPC = maximum number of filter convolution windows to precompute */
#define BPC 320
    sample_t *inbuf_old [2];
    sample_t *blackfilt [2*BPC+1];
    FLOAT itime[2];

    int (*scale_bitcounter)(gr_info * const gi);
#if HAVE_NASM
    /* functions to replace with CPU feature optimized one in takehiro.c */
    int (*choose_table)(const int *ix, const int *end, int *s);
    void (*fft_fht)(FLOAT *, int);

    /* CPU features */
    struct {
	unsigned int  MMX       : 1; /* Pentium MMX, Pentium II...IV, K6, K6-2,
                                    K6-III, Athlon */
	unsigned int  AMD_3DNow : 1; /* K6-2, K6-III, Athlon, Opteron */
	unsigned int  AMD_E3DNow: 1; /* Athlon, Opteron          */
	unsigned int  SSE      : 1; /* Pentium III, Pentium 4    */
	unsigned int  SSE2     : 1; /* Pentium 4, K8             */
    } CPU_features;
#endif

#ifdef BRHIST
  /* simple statistics */
  int   bitrate_stereoMode_Hist [16] [4+1];
  int	bitrate_blockType_Hist  [16] [4+1+1];/*norm/start/short/stop/mixed(short)/sum*/
#endif
#ifndef NOANALYSIS
  /* used by the frame analyzer */
  plotting_data *pinfo;
#endif
};





/***********************************************************************
*
*  Global Function Prototype Declarations
*
***********************************************************************/
void	freegfc(lame_internal_flags * const gfc);
int	BitrateIndex(int, int);
void	disable_FPE(void);

void fill_buffer(lame_global_flags *gfp,
		 sample_t *mfbuf[2],
		 sample_t *in_buffer[2],
		 int nsamples, int *n_in, int *n_out);

int  fill_buffer_resample (
        lame_global_flags *gfp,
        sample_t*  outbuf,
        int        desired_len,
        sample_t*  inbuf,
        int        len,
        int*       num_used,
        int        channels );

/* same as lame_decode1 (look in lame.h), but returns 
   unclipped raw floating-point samples. It is declared
   here, not in lame.h, because it returns LAME's 
   internal type sample_t. No more than 1152 samples 
   per channel are allowed. */
int lame_decode1_unclipped(
     unsigned char*  mp3buf,
     int             len,
     sample_t        pcm_l[],
     sample_t        pcm_r[] );




extern int  has_MMX   ( void );
extern int  has_3DNow ( void );
extern int  has_E3DNow( void );
extern int  has_SSE   ( void );
extern int  has_SSE2  ( void );



/***********************************************************************
*
*  Macros about Message Printing and Exit
*
***********************************************************************/
extern void lame_errorf(const lame_internal_flags *gfc, const char *, ...);
extern void lame_debugf(const lame_internal_flags *gfc, const char *, ...);
extern void lame_msgf  (const lame_internal_flags *gfc, const char *, ...);
#define DEBUGF  lame_debugf
#define ERRORF	lame_errorf
#define MSGF	lame_msgf


#ifdef __cplusplus
}
#endif

#endif /* LAME_UTIL_H */
