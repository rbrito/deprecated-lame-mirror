/*
 *
 *  Place to define all lame types needed by more than one source file
 *  Also there limits are defined here.
 *
 *  General rules:
 *
 *      - C Source files have the extention ".c", Header files ".h"
 *
 *      - Header files never should generate any data or code
 *
 *      - Every Source file has a Header file to export functions, macro
 *        constants, constants and global data
 *
 *      - never define local types, constants, macros etc. in the Header file
 *
 *      - Every Source file includes it's own Header file
 *
 *      - global types are defined in lametype.h
 *
 *      - try to not include project specific includes in Header files, only
 *        system includes, if it is necessary thing twice about design
 *        errors
 *
 *      - instead of including files in Header file include them in the
 *        corresponding Source file
 *
 *      - include file must be order independend. Ever. No exceptions, no
 *        excuse
 */
 
#ifndef LAME_LAMETYPE_H
#define LAME_LAMETYPE_H

#include <stdio.h> 
#include <limits.h>


/*********************************************************************************
 *********************************************************************************
 ******  data flow direction tags for RPC (lame running on computer farms)  ******
 *********************************************************************************
 *********************************************************************************/


// Macros to mark memory transfers for RPC functions
//
// size_t fread  ( INOUT SIZE2_IS(count,size) char* buffer, OUT size_t count, OUT size_t size, INOUT FILE* fp );
// size_t fwrite ( OUT   SIZE2_IS(count,size) char* buffer, OUT size_t count, OUT size_t size, INOUT FILE* fp );
// void   memset ( IN    SIZE_IS(size)        char* buffer, OUT int fill, OUT size_t size );
//
// note: untyped data (void*) is not possible for RPC

#define IN                     /* Object is transfered from RPC system, no data is taken from the origin */
#define OUT    const           /* Object is transfered only to the RPC system and therefore never changed */
#define INOUT                  /* before remote execution data is transfered to remote host and after the */
                               /* operation the (maybe) modified data is transfered back */
#define STRING                 /* data points to a zero terminated variable length string */
#define SIZE_IS(len)           /* size of the data object, 1D array */
#define SIZE2_IS(len1,len2)    /* size of the data object, 2D array */
#define LENGTH_IS(len)
#define FIRST_IS(len)
#define LAST_IS(len)  
#define MAX_IS(len)
#define CALLBACK(fn)


/*********************************************************************************
 *********************************************************************************
 *****************************  Simple scalar types  *****************************
 *********************************************************************************
 *********************************************************************************/


/* simple scalar types */                       /* limits of the type */

/* 
 * Basic integer C types:
 * 
 * Note: be carefully with char and numerics, it can be signed or unsigned
 * 
 * char                                            CHAR_MIN   ... CHAR_MAX
 * short                                           SHRT_MIN   ... SHRT_MAX
 * int                                             INT_MIN    ... INT_MAX
 * long                                            LONG_MIN   ... LONG_MAX
 *
 * size_t   Store size of memory objects, unsigned type, may be larger than unsigned int
 * ssize_t  Store size of memory objects, signed type, may be larger than signed int
 * off_t    Store file positions, note terabyte hard disks and 64 bit file systems
 */

/* explicitly signed types */

typedef signed char             schar;          /* SCHAR_MIN  ... SCHAR_MAX  */
typedef signed short int        sshort;         /* SSHRT_MIN  ... SSHRT_MAX  */
typedef signed int              sint;           /* SINT_MIN   ... SINT_MAX   */
typedef signed long int         slong;          /* SLONG_MIN  ... SLONG_MAX  */

#ifndef SCHAR_MIN
# define SCHAR_MIN              ((schar)-128)
#endif
#ifndef SCHAR_MAX
# define SCHAR_MAX              ((schar)+127)
#endif
#define SSHRT_MIN               SHRT_MIN
#define SSHRT_MAX               SHRT_MAX
#define SINT_MIN                INT_MIN
#define SINT_MAX                INT_MAX
#define SLONG_MIN               LONG_MIN
#define SLONG_MAX               LONG_MAX


/* explicitly unsigned types */

typedef unsigned char           uchar;          /* UCHAR_MIN  ... UCHAR_MAX  */
typedef unsigned short int      ushort;         /* USHRT_MIN  ... USHRT_MAX  */
typedef unsigned int            uint;           /* UINT_MIN   ... UINT_MAX   */
typedef unsigned long int       ulong;          /* ULONG_MIN  ... ULONG_MAX  */

#define UCHAR_MIN               ((uchar )0)
#define USHORT_MIN              ((ushort)0)
#define UINT_MIN                ((uint  )0)
#define ULONG_MIN               ((ulong )0)


/* types with a well defined size */

#if   defined _WIN32
typedef signed __int8           int8;
typedef signed __int16          int16;
typedef signed __int32          int32;
typedef signed __int64          int64;
#elif defined __C99__
typedef int8_t                  int8;
typedef int16_t                 int16;
typedef int32_t                 int32;
typedef int64_t                 int64;
#elif defined __alpha__
typedef signed char             int8;
typedef signed short int        int16;
typedef signed int              int32;
typedef signed long int         int64;
#else 
typedef signed char             int8;
typedef signed short int        int16;
typedef signed long int         int32;
typedef signed long long int    int64;
#endif

#define INT8_MIN                (-128)
#define INT8_MAX                127
#define INT16_MIN               (-32768)
#define INT16_MAX               32767
#define INT32_MIN               (-2147483648L)
#define INT32_MAX               2147483647L
#define INT64_MIN               (-9223372036854775808LL)
#define INT64_MAX               9223372036854775807LL


#if   defined _WIN32
typedef unsigned __int8         uint8;
typedef unsigned __int16        uint16;
typedef unsigned __int32        uint32;
typedef unsigned __int64        uint64;
#elif defined __C99__
typedef uint8_t                 uint8;
typedef uint16_t                uint16;
typedef uint32_t                uint32;
typedef uint64_t                uint64;
#elif defined __alpha__
typedef unsigned char           uint8;
typedef unsigned short int      uint16;
typedef unsigned int            uint32;
typedef unsigned long int       uint64;
#else
typedef unsigned char           uint8;
typedef unsigned short int      uint16;
typedef unsigned long int       uint32;
typedef unsigned long long int  uint64;
#endif

#define UINT8_MIN               0
#define UINT8_MAX               255
#define UINT16_MIN              0
#define UINT16_MAX              65535
#define UINT32_MIN              0LU
#define UINT32_MAX              4294967295LU
#define UINT64_MIN              0LLU
#define UINT64_MAX              18446744073709551615LLU


/*********************************************************************************
 *********************************************************************************
 *****************************  Floating point types  ****************************
 *********************************************************************************
 *********************************************************************************/

/*
 * float        use this type if space plays a big rôle and precision is not so very important
 * double       use this type if you need more precision than 6 decimal digits
 * long double  use this type if you need maximum precision and execution time is not so very important
 */

typedef float                   real;           /* This type should be the fastest FP type available */
typedef float                   Single;


/*********************************************************************************
 *********************************************************************************
 *********************************  Other types  *********************************
 *********************************************************************************
 *********************************************************************************/


/* Types for simple text strings (may be 16 bit for Eastern Asians) */

typedef uchar*                  string;
typedef const uchar*            cstring;

/* Project specific types */
 
typedef long double             freq_t;

typedef signed short int        sample_t;       /* SAMPLE_MIN ... SAMPLE_MAX, or -1.0...+1.0 for FP */
#define SAMPLE_MIN              ((sample_t)-32768)
#define SAMPLE_MAX              ((sample_t)+32767)

typedef sample_t                mono_t;
typedef sample_t                stereo_t  [2];
typedef sample_t                stereo1_t [1];
typedef sample_t                stereo2_t [2];
typedef sample_t                stereo3_t [3];
typedef sample_t                stereo4_t [4];
typedef sample_t                stereo5_t [5];
typedef sample_t                stereo6_t [6];

typedef uint32                  bitstream_buffer_t;
       
#define BITSTREAM_BUFFER_BITS	( sizeof(bitstream_buffer_t) * CHAR_BIT )
       

/*********************************************************************************
 *********************************************************************************
 ***********************************  enum's  ************************************
 *********************************************************************************
 *********************************************************************************/


typedef enum {
    sf_unknown,
    sf_mp1,                     /* MPEG Layer 1, aka mpg */
    sf_mp2,                     /* MPEG Layer 2 */
    sf_mp3,                     /* MPEG Layer 3 */
    sf_ogg,
    sf_raw,
    sf_wave,
    sf_aiff,
    sf_wave_szip,               // see http://www.compressconsult.com/szip/
    sf_aiff_szip                // compressing -o4 -ir4 (for 16 bit stereo)
} sound_file_format_t;

typedef enum {
    vbr_off,
    vbr_mt,
    vbr_rh,
    vbr_abr,
    vbr_default = vbr_rh        /* change this to change the default VBR mode of LAME */
} vbr_mode_t;

typedef enum {
    ch_stereo,
    ch_jstereo,
    ch_dual,
    ch_mono
} channel_mode_t;

typedef enum {
    MPEG_1   = 1,
    MPEG_2   = 0,
    MPEG_2_5 = 2
} MPEG_version_t;

enum byte_order { 
    order_unknown, 
    order_bigEndian, 
    order_littleEndian 
};  


/*********************************************************************************
 *********************************************************************************
 ****************************  struct's and unions's  ****************************
 *********************************************************************************
 *********************************************************************************/


/* structure to receive extracted header */

/* toc may be NULL ?????????????????????????? */

#define NUMTOCENTRIES  100   

typedef struct {
    int32  h_id;                /* from MPEG header, 0=MPEG2, 1=MPEG1 */
    int32  samprate;            /* determined from MPEG header */
    int32  flags;               /* from Vbr header data */
    int32  frames;              /* total bit stream frames from Vbr header data */
    int32  bytes;               /* total bit stream bytes from Vbr header data*/
    int32  vbr_scale;           /* encoded vbr scale from Vbr header data*/
    uchar  toc [NUMTOCENTRIES]; /* may be NULL if toc not desired*/
} VBRTAGDATA;

/*
 *    4 bytes for Header Tag
 *    4 bytes for Header Flags
 *  100 bytes for entry (NUMTOCENTRIES)
 *    4 bytes for FRAME SIZE
 *    4 bytes for STREAM_SIZE
 *    4 bytes for VBR SCALE. a VBR quality indicator: 0=best 100=worst
 *   20 bytes for LAME tag.  for example, "LAME3.12 (beta 6)"           ????????????????????
 * ----------
 *  140 bytes
*/

#define VBRHEADERSIZE  (NUMTOCENTRIES + 4 + 4 + 4 + 4 + 4)           //  ????????????????????

struct id3tag_spec {  /* private data members */
    int          flags;
    const uchar* title;
    const uchar* artist;
    const uchar* album;
    int          year;
    const uchar* comment;
    int          track;
    int          genre;
};


typedef uint32   HUFFBITS; 

struct huffcodetab {
    size_t           xlen;      /* max. x-index+                         */
    size_t           linmax;    /* max number to be stored in linbits    */
    const HUFFBITS*  table;     /* pointer to array[xlen][ylen]          */
    const uchar*     hlen;      /* pointer to array[xlen][ylen]          */
};


#include "l3side.h"



/*********************************************************************************
 *********************************************************************************
 ************************  the three huge lame struct's  *************************
 *********************************************************************************
 *********************************************************************************/


 /*******************************
 *** data for frame analyzer ***
 *******************************/

typedef struct {

    // ...
    uint32  frameNum;
    // ...
} plotting_data;


/*******************************
 ***** internal lame flags *****
 *******************************/

typedef struct {
    plotting_data* pinfo;
    // ...
    uint    channels_out;
    float   lowpass1;
    float   lowpass2;
    float   highpass1;
    float   highpass2;
    // ...
} lame_internal_flags;


/*******************************
 ****** global lame flags ******
 *******************************/

typedef struct {
    lame_internal_flags* internal_flags; 
    // ...
    uint32  num_samples_out;
    uint    channels_in;
    freq_t  samplefreq_out;
    freq_t  samplefreq_in;
    // ...
} lame_global_flags;


 
#endif  /* LAME_LAMETYPE_H */

/* end of lametype.h */
