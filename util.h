#ifndef UTIL_DOT_H
#define UTIL_DOT_H
/***********************************************************************
*
*  Global Include Files
*
***********************************************************************/
#include "machine.h"
#include "encoder.h"
#include "lame.h"
#include "gtkanal.h"

/***********************************************************************
*
*  Global Definitions
*
***********************************************************************/

/* General Definitions */
#ifndef FALSE
#define         FALSE                   0
#endif

#ifndef TRUE
#define         TRUE                    1
#endif

#define         MAX_U_32_NUM            0xFFFFFFFF

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


#define         BITS_IN_A_BYTE          8
#define         HAN_SIZE                512
#define         CRC16_POLYNOMIAL        0x8005

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


/* "bit_stream.h" Definitions */

#define         MINIMUM         4    /* Minimum size of the buffer in bytes */
#define         MAX_LENGTH      32   /* Maximum length of word written or
                                        read from bit stream */
#define         BUFFER_SIZE     LAME_MAXMP3BUFFER 

#define         Min(A, B)       ((A) < (B) ? (A) : (B))
#define         Max(A, B)       ((A) > (B) ? (A) : (B))



/***********************************************************************
*
*  Global Variable External Declarations
*
***********************************************************************/

extern int      bitrate_table[2][15];



/***********************************************************************
*
*  Global Type Definitions
*
***********************************************************************/

/* Structure for Reading Layer II Allocation Tables from File */

typedef struct {
    unsigned int    steps;
    unsigned int    bits;
    unsigned int    group;
    unsigned int    quant;
} sb_alloc, *alloc_ptr;

typedef sb_alloc        al_table[SBLIMIT][16]; 




enum byte_order { order_unknown, order_bigEndian, order_littleEndian };
extern enum byte_order NativeByteOrder;

/* "bit_stream.h" Type Definitions */

typedef struct  bit_stream_struc {
    unsigned char *buf;         /* bit stream buffer */
    int         buf_size;       /* size of buffer (in number of bytes) */
    unsigned long        totbit;         /* bit counter of bit stream */
    int         buf_byte_idx;   /* pointer to top byte in buffer */
    int         buf_bit_idx;    /* pointer to top bit of top byte in buffer */
    
    /* format of file in rd mode (BINARY/ASCII) */
} Bit_stream_struc;

#include "l3side.h"


typedef struct  {
  /********************************************************************/
  /* internal variables NOT set by calling program, and should not be */
  /* modified by the calling program                                  */
  /********************************************************************/
  int lame_init_params_init;      /* was lame_init_params called? */
  int lame_encode_frame_init;     
  int iteration_init_init;
  int fill_buffer_downsample_init;
  int fill_buffer_upsample_init;
  int mdct_sub48_init;
  int psymodel_init;


  int padding;                    /* padding for the current frame? */
  int mode_gr;                    /* granules per frame */
  int stereo;                     /* number of channels */
  int VBR_min_bitrate;            /* min bitrate index */
  int VBR_max_bitrate;            /* max bitrate index */
  float resample_ratio;           /* input_samp_rate/output_samp_rate */
  int bitrate_index;
  int samplerate_index;
  int mode_ext;

  /* lowpass and highpass filter control */
  float lowpass1,lowpass2;        /* normalized frequency bounds of passband */
  float highpass1,highpass2;      /* normalized frequency bounds of passband */
                                  
  /* polyphase filter (filter_type=0)  */
  int lowpass_band;          /* zero bands >= lowpass_band in the polyphase filterbank */
  int highpass_band;         /* zero bands <= highpass_band */
  int lowpass_start_band;    /* amplify bands between start */
  int lowpass_end_band;      /* and end for lowpass */
  int highpass_start_band;   /* amplify bands between start */
  int highpass_end_band;     /* and end for highpass */


  int filter_type;          /* 0=polyphase filter, 1= FIR filter 2=MDCT filter(bad)*/
  int quantization;         /* 0 = ISO formual,  1=best amplitude */
  int noise_shaping;        /* 0 = none 
                               1 = ISO AAC model
                               2 = allow scalefac_select=1  
                             */

  int noise_shaping_stop;   /* 0 = stop at over=0, all scalefacs amplified or
                                   a scalefac has reached max value
                               1 = stop when all scalefacs amplified or        
                                   a scalefac has reached max value
                               2 = stop when all scalefacs amplified 
			    */

  int psymodel;             /* 0 = none   1=gpsycho */
  int use_best_huffman;     /* 0 = no.  1=outside loop  2=inside loop(slow) */



  /* data set by get_audio.c after reading input file: */
  unsigned long num_samples_read;  
  int count_samples_carefully;
  int input_bitrate;
  int pcmbitwidth;



  /* variables used by lame.c */
  Bit_stream_struc   bs;
  III_side_info_t l3_side;
#define MFSIZE (3*1152+ENCDELAY-MDCTDELAY)
  int mf_size;
  int mf_samples_to_encode;
  short int mfbuf[2][MFSIZE];
  FLOAT8 frac_SpF;
  FLOAT8 slot_lag;
  FLOAT8 ms_ener_ratio[2];
  FLOAT8 ms_ratio[2];


  /* variables used by quantize.c */
  int OldValue[2];
  int CurrentStep;
  FLOAT8 ATH_l[SBMAX_l];
  FLOAT8 ATH_s[SBMAX_s];
  FLOAT8 masking_lower;
  FLOAT8 ATH_lower;


  /* variables used by util.c */
#define BLACKSIZE 30
#define BPC 16
  short int inbuf_old[2][BLACKSIZE];
  FLOAT blackfilt[2*BPC+1][BLACKSIZE];
  FLOAT8 itime[2];
#define OLDBUFSIZE 5
  FLOAT8 upsample_itime[2];
  short int upsample_inbuf_old[2][OLDBUFSIZE];
  int sideinfo_len;

  /* variables for newmdct.c */
  FLOAT8 sb_sample[2][2][18][SBLIMIT];
  FLOAT8 amp_lowpass[32];
  FLOAT8 amp_highpass[32];

  /* variables for bitstream.c */
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
  int ancillary_flag;
  

  /* variables for reservoir.c */
  int ResvSize; /* in bits */
  int ResvMax;  /* in bits */

  
  scalefac_struct scalefac_band;


  /* DATA FROM PSYMODEL.C */
/* The static variables "r", "phi_sav", "new", "old" and "oldest" have    */
/* to be remembered for the unpredictability measure.  For "r" and        */
/* "phi_sav", the first index from the left is the channel select and     */
/* the second index is the "age" of the data.                             */
  FLOAT8	minval[CBANDS];
  FLOAT8	nb_1[4][CBANDS], nb_2[4][CBANDS];
  FLOAT8 s3_s[CBANDS + 1][CBANDS + 1];
  FLOAT8 s3_l[CBANDS + 1][CBANDS + 1];

  III_psy_xmin thm[4];
  III_psy_xmin en[4];
  
  /* unpredictability calculation
   */
  int cw_upper_index;
  int cw_lower_index;
  FLOAT ax_sav[4][2][HBLKSIZE];
  FLOAT bx_sav[4][2][HBLKSIZE];
  FLOAT rx_sav[4][2][HBLKSIZE];
  FLOAT cw[HBLKSIZE];

  /* fft and energy calculation    */
  FLOAT wsamp_L[2][BLKSIZE];
  FLOAT energy[HBLKSIZE];
  FLOAT wsamp_S[2][3][BLKSIZE_s];
  FLOAT energy_s[3][HBLKSIZE_s];

  
  /* Scale Factor Bands    */
  FLOAT8	w1_l[SBPSY_l], w2_l[SBPSY_l];
  FLOAT8	w1_s[SBPSY_s], w2_s[SBPSY_s];
  FLOAT8 mld_l[SBPSY_l],mld_s[SBPSY_s];
  int	bu_l[SBPSY_l],bo_l[SBPSY_l] ;
  int	bu_s[SBPSY_s],bo_s[SBPSY_s] ;
  int	npart_l,npart_s;
  int	npart_l_orig,npart_s_orig;
  
  int	s3ind[CBANDS][2];
  int	s3ind_s[CBANDS][2];

  int	numlines_s[CBANDS] ;
  int	numlines_l[CBANDS];
  
  /* frame analyzer    */
  FLOAT energy_save[4][HBLKSIZE];
  FLOAT8 pe_save[4];
  FLOAT8 ers_save[4];

  /* ratios  */
  FLOAT8 pe[4];
  FLOAT8 ms_ratio_s_old,ms_ratio_l_old;
  FLOAT8 ms_ener_ratio_old;

  /* block type */
  int	blocktype_old[2];

  /* used by the frame analyzer */
  plotting_data *pinfo;


} lame_internal_flags;


/***********************************************************************
*
*  Global Function Prototype Declarations
*
***********************************************************************/

extern void           display_bitrates(FILE *out_fh);
extern int            BitrateIndex(int, int,int);
extern int            SmpFrqIndex(long, int*);
extern int            copy_buffer(char *buffer,int buffer_size,Bit_stream_struc *bs);
extern void           init_bit_stream_w(lame_internal_flags *gfc);
extern void           alloc_buffer(Bit_stream_struc*, int);
extern void           freegfc(lame_internal_flags *gfc);
extern void           reorder(int * bands, int ix[576], int ix_org[576]);


extern enum byte_order DetermineByteOrder(void);
extern void SwapBytesInWords( short *loc, int words );

extern int fill_buffer_downsample(lame_global_flags *gfp,short int *outbuf,int desired_len,
	 short int *inbuf,int len,int *num_used,int ch);

extern int fill_buffer_upsample(lame_global_flags *gfp,short int *outbuf,int desired_len,
	 short int *inbuf,int len,int *num_used,int ch);

extern void 
getframebits(lame_global_flags *gfp,int *bitsPerFrame, int *mean_bits);

#endif





