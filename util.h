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
 #ifdef M_PI
  #define       PI                      M_PI
 #else
  #define       PI                      3.14159265358979323846
 #endif
#endif


#ifdef M_LN2
 #define        LOG2                    M_LN2
#else
 #define        LOG2                    0.69314718055994530942
#endif

#ifdef M_LN10
 #define        LOG10                   M_LN10
#else
 #define        LOG10                   2.30258509299404568402
#endif


#ifdef M_SQRT2
 #define        SQRT2                   M_SQRT2
#else
 #define        SQRT2                   1.41421356237309504880
#endif

#define         MPEG_AUDIO_ID           1
#define		MPEG_PHASE2_LSF		0	/* 1995-07-11 SHN */

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

/* Header Information Structure */

typedef struct {
    int version;
    int lay;
    int error_protection;
    int bitrate_index;
    int sampling_frequency;
    int padding;
    int extension;
    int mode;
    int mode_ext;
    int copyright;
    int original;
    int emphasis;
} layer, *the_layer;

/* Parent Structure Interpreting some Frame Parameters in Header */

typedef struct {
    layer       *header;        /* raw header information */
    int         actual_mode;    /* when writing IS, may forget if 0 chs */
    al_table    *alloc;         /* bit allocation table read in */
    int         tab_num;        /* number of table as loaded */
    int         jsbound;        /* first band of joint stereo coding */
    int         sblimit;        /* total number of sub bands */
} frame_params;


enum byte_order { order_unknown, order_bigEndian, order_littleEndian };
extern enum byte_order NativeByteOrder;

/* "bit_stream.h" Type Definitions */

typedef struct  bit_stream_struc {
    unsigned char*		pbtOutBuf;   /* for .DLL code */
    int 			nOutBufPos;  /* for .DLL code */
    FILE        *pt;            /* pointer to bit stream device */
    unsigned char *buf;         /* bit stream buffer */
    int         buf_size;       /* size of buffer (in number of bytes) */
    unsigned long        totbit;         /* bit counter of bit stream */
    int         buf_byte_idx;   /* pointer to top byte in buffer */
    int         buf_bit_idx;    /* pointer to top bit of top byte in buffer */
    
    /* format of file in rd mode (BINARY/ASCII) */
} Bit_stream_struc;

#include "l3side.h"

/***********************************************************************
*
*  Global Variable External Declarations
*
***********************************************************************/

extern FLOAT8   s_freq[2][4];
extern int      bitrate[2][3][15];

/***********************************************************************
*
*  Global Function Prototype Declarations
*
***********************************************************************/

extern void           display_bitrates(int layr);
extern int            BitrateIndex(int, int, int,int);
extern int            SmpFrqIndex(long, int*);
extern void           *mem_alloc(unsigned long, char*);
extern void           empty_buffer(Bit_stream_struc*);
extern int            copy_buffer(char *buffer,Bit_stream_struc *bs);
extern void           init_bit_stream_w(Bit_stream_struc*);
extern void           alloc_buffer(Bit_stream_struc*, int);
extern void           desalloc_buffer(Bit_stream_struc*);
extern void           putbits(Bit_stream_struc*, unsigned int, int);

extern enum byte_order DetermineByteOrder(void);
extern void SwapBytesInWords( short *loc, int words );

extern void 
getframebits(layer *info, int *bitsPerFrame, int *mean_bits);

void timestatus(layer *info,long frameNum,long totalframes);
#endif
