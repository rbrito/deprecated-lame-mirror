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
    int         bstype;  
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


  long int frameNum;              /* frame counter */
  long totalframes;               /* frames: 0..totalframes-1 (estimate)*/
  int encoder_delay;
  int framesize;                  
  int version;                    /* 0=MPEG2  1=MPEG1 */
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



  /* variables used by lame.c */
  Bit_stream_struc   bs;
  III_side_info_t l3_side;
#define MFSIZE (1152+1152+ENCDELAY-MDCTDELAY)
  short int mfbuf[2][MFSIZE];
  int mf_size;
  int mf_samples_to_encode;
  FLOAT8 frac_SpF;
  FLOAT8 slot_lag;
  FLOAT8 ms_ener_ratio[2];
  FLOAT8 ms_ratio[2];

  /* variables used by quantize.c */
  int OldValue[2];
  int CurrentStep;

  /* variables used by util.c */
#define BLACKSIZE 30
  short int inbuf_old[2][BLACKSIZE];
  FLOAT8 blackfilt[BLACKSIZE];
  FLOAT8 itime[2];
#define OLDBUFSIZE 5
  FLOAT8 upsample_itime[2];
  short int upsample_inbuf_old[2][OLDBUFSIZE];
  int sideinfo_len;

  /* variables for newmdct.c */
  FLOAT8 sb_sample[2][2][18][SBLIMIT];
  FLOAT8 mm[16][SBLIMIT - 1];

  /* variables for bitstream.c */
#define MAX_HEADER_BUF 32 /* 511 / 21 = 24, 255/13 = 19 */
#define MAX_HEADER_LEN 40 /* max size of header is 38 */
struct {
    int write_timing;
    int ptr;
    char buf[MAX_HEADER_LEN];
} header[MAX_HEADER_BUF];
     int h_ptr;
     int w_ptr;

  
     struct scalefac_struct scalefac_band;


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
extern void           desalloc_buffer(Bit_stream_struc*);
extern void           putbits(Bit_stream_struc *bs,unsigned int val,int N);
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





