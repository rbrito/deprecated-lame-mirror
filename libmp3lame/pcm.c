/* $Id$ */

/*
 *  There are a lot of not tested return codes.
 *  This is currently intention to make the code more readable
 *  in the design phase.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef KLEMM_44

#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>
#include <memory.h>
#include <stdlib.h>
#include <assert.h>
#include "util.c"

typedef float         float32_t;
typedef double        float64_t;

#if 0

typedef float32_t     sample_t;

typedef struct {
    int     num_channels;
    void*   gfc;
    // ...
} lame_global_flags;

typedef enum {
    coding_MPEG_Layer_1 = 1,
    coding_MPEG_Layer_2 = 2,
    coding_MPEG_Layer_3 = 3,
    coding_MPEG_AAC     = 4,
    coding_Ogg_Vorbis   = 5,
    coding_MPEG_Layer_3_plus = 6
} coding_t;

typedef struct {
    unsigned long  Class_ID;
    long double    sample_freq_in;
    long double    sample_freq_out;
    float          lowpass_freq;
    int            scale_in;
    int            scale_out;
    sample_t**     fir;
    sample_t*      lastsamples [2];
    // ...
} resample_t;

typedef struct {
    unsigned long  Class_ID;
    unsigned int   channels_in;
    unsigned int   channels_out;
    sample_t*      mfbuf [2];
    long double    resample_ratio;
    resample_t*    resample_in;
    resample_t*    resample_out;
    size_t         frame_size;
    size_t         mf_samples_to_encode;
    size_t         mf_size;
    coding_t       coding;
    unsigned long  frame_count;
    // ...
} lame_internal_flags;

#endif

typedef lame_internal_flags  lame_t;

#define LAME_ID  0xFFF88E3B

/*========================================================================================*/

#define OUT            const
#define INOUT
#define IN

#if CHAR_BIT != 8
# error Machines with CHAR_BIT != 8 not yet supported
#endif

int  fill_buffer_resample_NEW_MORPHED (  // return code, 0 for success
//      INOUT resample_t * r,            // internal context structure
	INOUT lame_global_flags * gfp,
        IN    sample_t   * out,          // where to write the output data
        INOUT size_t     * out_req_len,  // requested output data len/really written output data len
        OUT   sample_t   * in,           // where are the input data?
        INOUT size_t     * in_avail_len, // available input data len/consumed input data len
        OUT   size_t       channel );    // number of the channel (needed for buffering)

int  lame_encode_mp3_frame (             // return code, 0 for success
        INOUT lame_global_flags*,        // internal context structure
        /*OUT*/sample_t * inbuf_l,        // data for left  channel
        /*OUT*/sample_t * inbuf_r,        // data for right channel
        IN    uint8_t  * mp3buf,         // where to write the coded data
        OUT   size_t     mp3buf_size );  // maximum size of coded data

int  lame_encode_ogg_frame (             // return code, 0 for success
        INOUT lame_global_flags,         // internal context structure
        OUT   sample_t * inbuf_l,        // data for left  channel
        OUT   sample_t * inbuf_r,        // data for right channel
        IN    uint8_t  * mp3buf,         // where to write the coded data
        OUT   size_t     mp3buf_size );  // maximum size of coded data

/*
 *    Now we need at least the following stuff:
 *    float32_t, float64_t, sample_t, int8_t, int16_t, int32_t, uint8_t, uint16_t, uint32_t, size_t, ssize_t
 *    must be defined via typedef or by #define's
 *    INLINE must be defined
 *    CHAR_BIT must be defined and must have the value 8
 */

#define BYTES1(x1)          ((((int8_t)(x1)) << 8))
#define BYTES2(x1,x2)       ((((int8_t)(x1)) << 8) + (uint8_t)(x2))
#define BYTES3(x1,x2,x3)    ((((int8_t)(x1)) << 8) + (uint8_t)(x2) + 1/256.*(uint8_t)(x3))
#define BYTES4(x1,x2,x3,x4) ((((int8_t)(x1)) << 8) + (uint8_t)(x2) + 1/256.*(uint8_t)(x3) + 1/65536.*(uint8_t)(x4))

#define FUNCTION(name,expr)  \
void  name ( sample_t* dst, const uint8_t* src, const ssize_t step, size_t len ) \
{                                                                                \
    do {                                                                         \
        *dst++ = (expr);                                                         \
        src   += step;                                                           \
    } while (--len);                                                             \
}

static INLINE sample_t  read_opposite_float32 ( const uint8_t* const src )
{
    uint8_t  tmp [4];
    tmp[0] = src[3];
    tmp[1] = src[2];
    tmp[2] = src[1];
    tmp[3] = src[0];
    return *(const float32_t*)tmp;
}

static INLINE sample_t  read_opposite_float64 ( const uint8_t* const src )
{
    uint8_t  tmp [8];
    tmp[0] = src[7];
    tmp[1] = src[6];
    tmp[2] = src[5];
    tmp[3] = src[4];
    tmp[4] = src[3];
    tmp[5] = src[2];
    tmp[6] = src[1];
    tmp[7] = src[0];
    return *(const float64_t*)tmp;
}

static sample_t  read_float80_le ( const unsigned char* const src )
{
    int         sign     = src [9] & 128  ?  -1  :  +1;
    long        exponent =(src [9] & 127) * 256 +
                           src [8] - 16384 - 62;
    long double mantisse = src [7] * 72057594037927936.L +
                           src [6] * 281474976710656.L +
                           src [5] * 1099511627776.L +
                           src [4] * 4294967296.L +
                           src [3] * 16777216.L +
                           src [2] * 65536.L +
                           src [1] * 256.L +
                           src [0];
    return sign * mantisse * ldexp (1., exponent);
}

static sample_t  read_float80_be ( const unsigned char* const src )
{
    int         sign     = src [0] & 128  ?  -1  :  +1;
    long        exponent =(src [0] & 127) * 256 +
                           src [1] - 16384 - 62;
    long double mantisse = src [2] * 72057594037927936.L +
                           src [3] * 281474976710656.L +
                           src [4] * 1099511627776.L +
                           src [5] * 4294967296.L +
                           src [6] * 16777216.L +
                           src [7] * 65536.L +
                           src [8] * 256.L +
                           src [9];
    return sign * mantisse * ldexp (1., exponent);
}

FUNCTION ( copy_silence, 0 )
FUNCTION ( copy_u8     , BYTES1 (src[0]-128) )
FUNCTION ( copy_s8     , BYTES1 (src[0]    ) )
FUNCTION ( copy_s24_le , BYTES3 (src[2], src[1], src[0] ) )
FUNCTION ( copy_s24_be , BYTES3 (src[0], src[1], src[2] ) )
FUNCTION ( copy_u24_le , BYTES3 (src[2]-128, src[1], src[0] ) )
FUNCTION ( copy_u24_be , BYTES3 (src[0]-128, src[1], src[2] ) )
FUNCTION ( copy_f80_le , read_float80_le (src) )
FUNCTION ( copy_f80_be , read_float80_be (src) )

#ifndef WORDS_BIGENDIAN

/* little endian */
FUNCTION ( copy_s16_le , *(const int16_t*)src )
FUNCTION ( copy_s16_be , BYTES2 (src[0], src[1] ) )
FUNCTION ( copy_u16_le , *(const uint16_t*)src - 32768 )
FUNCTION ( copy_u16_be , BYTES2 (src[0]-128, src[1] ) )
FUNCTION ( copy_s32_le , 1/65536. * *(const int32_t*)src )
FUNCTION ( copy_s32_be , BYTES4 (src[0], src[1], src[2], src[3] ) )
FUNCTION ( copy_u32_le , 1/65536. * *(const uint32_t*)src - 32768. )
FUNCTION ( copy_u32_be , BYTES4 (src[0]-128, src[1], src[2], src[3] ) )
FUNCTION ( copy_f32_le , *(const float32_t*)src )
FUNCTION ( copy_f32_be , read_opposite_float32 (src) )
FUNCTION ( copy_f64_le , *(const float64_t*)src )
FUNCTION ( copy_f64_be , read_opposite_float64 (src) )

#else

/* big endian */
FUNCTION ( copy_s16_le , BYTES2 (src[1], src[0] ) )
FUNCTION ( copy_s16_be , *(const int16_t*)src )
FUNCTION ( copy_u16_le , BYTES2 (src[1]-128, src[0] ) )
FUNCTION ( copy_u16_be , *(const uint16_t*)src - 32768 )
FUNCTION ( copy_s32_le , BYTES4 (src[3], src[2], src[1], src[0] ) )
FUNCTION ( copy_s32_be , 1/65536. * *(const int32_t*)src )
FUNCTION ( copy_u32_le , BYTES4 (src[3]-128, src[2], src[1], src[0] ) )
FUNCTION ( copy_u32_be , 1/65536. * *(const uint32_t*)src - 32768. )
FUNCTION ( copy_f32_le , read_opposite_float32 (src) )
FUNCTION ( copy_f32_be , *(const float32_t*)src )
FUNCTION ( copy_f64_le , read_opposite_float64 (src) )
FUNCTION ( copy_f64_be , *(const float64_t*)src )

#endif

// The lower 16 bits containing the number of channels, so 1...65535
// channels are possible

#define  LAME_SILENCE           0x00010000
#define  LAME_UINT8             0x00020000
#define  LAME_INT8              0x00030000
#define  LAME_UINT16            0x00040000
#define  LAME_INT16             0x00050000
#define  LAME_UINT24            0x00060000
#define  LAME_INT24             0x00070000
#define  LAME_UINT32            0x00080000
#define  LAME_INT32             0x00090000
#define  LAME_FLOAT             0x00100000
#define  LAME_DOUBLE            0x00110000
#define  LAME_LONGDOUBLE        0x00120000
#define  LAME_FLOAT32           0x00140000
#define  LAME_FLOAT64           0x00180000
#define  LAME_FLOAT80_ALIGN2    0x001A0000
#define  LAME_FLOAT80_ALIGN4    0x001C0000
#define  LAME_FLOAT80_ALIGN8    0x00200000

#define  LAME_NATIVE_ENDIAN     0x00000000
#define  LAME_OPPOSITE_ENDIAN   0x01000000
#define  LAME_LITTLE_ENDIAN     0x02000000
#define  LAME_BIG_ENDIAN        0x03000000

#define  LAME_INTERLEAVED       0x10000000
#define  LAME_INDIRECT          0x20000000

typedef void (*demux_t) ( sample_t* dst, const uint8_t* src, const ssize_t step, size_t len );

typedef struct {
    ssize_t  size;
    demux_t  demultiplexer_be;
    demux_t  demultiplexer_le;
} demux_info_t;

demux_info_t demux_info [] = {
    { -1, NULL        , NULL         }, /* 00: */
    {  0, copy_silence, copy_silence }, /* 01: */
    {  1, copy_u8     , copy_u8      }, /* 02: */
    {  1, copy_s8     , copy_s8      }, /* 03: */
    {  2, copy_u16_be , copy_u16_le  }, /* 04: */
    {  2, copy_s16_be , copy_s16_le  }, /* 05: */
    {  3, copy_u24_be , copy_u24_le  }, /* 06: */
    {  3, copy_s24_be , copy_s24_le  }, /* 07: */
    {  4, copy_u32_be , copy_u32_le  }, /* 08: */
    {  4, copy_s32_be , copy_s32_le  }, /* 09: */
    { -1, NULL        , NULL         }, /* 0A: */
    { -1, NULL        , NULL         }, /* 0B: */
    { -1, NULL        , NULL         }, /* 0C: */
    { -1, NULL        , NULL         }, /* 0D: */
    { -1, NULL        , NULL         }, /* 0E: */
    { -1, NULL        , NULL         }, /* 0F: */
    {  sizeof(float) , copy_f32_be , copy_f32_le  }, /* 10: */
    {  sizeof(double), copy_f64_be , copy_f64_le  }, /* 11: */
    { -1, NULL        , NULL         }, /* 12: */
    { -1, NULL        , NULL         }, /* 13: */
    {  4, copy_f32_be , copy_f32_le  }, /* 14: */
    { -1, NULL        , NULL         }, /* 15: */
    { -1, NULL        , NULL         }, /* 16: */
    { -1, NULL        , NULL         }, /* 17: */
    {  8, copy_f64_be , copy_f64_le  }, /* 18: */
    { -1, NULL        , NULL         }, /* 19: */
    { 10, copy_f80_be , copy_f80_le  }, /* 1A: */
    { -1, NULL        , NULL         }, /* 1B: */
    { 12, copy_f80_be , copy_f80_le  }, /* 1C: */
    { -1, NULL        , NULL         }, /* 1D: */
    { -1, NULL        , NULL         }, /* 1E: */
    { -1, NULL        , NULL         }, /* 1F: */
    { 16, copy_f80_be , copy_f80_le  }, /* 20: */
};
    

static INLINE int  select_demux ( uint32_t mode, demux_t* retf, ssize_t* size )
{
    int            big    = mode >> 24;
    demux_info_t*  tabptr = demux_info + ((mode >> 16) & 0x1F);

#ifndef WORDS_BIGENDIAN
                                        // 0=little, 1=big, 2=little, 3=big
#else
    /* big endian */
    big  = (big >> 1) ^ big ^ 1;        // 0=big, 1=little, 2=little, 3=big
#endif

    *size = tabptr -> size;
    *retf = big & 1  ?  tabptr -> demultiplexer_be
                     :  tabptr -> demultiplexer_le;
    return *retf != NULL  ?  0  :  1;
}

#ifdef __EXTERN__

typedef struct {
    volatile const void* volatile const   data;
    const size_t                          length;
    const size_t                          size;
} octetstream_t; 

#else

typedef struct {
    uint8_t*   data;
    size_t     length;
    size_t     size;
} octetstream_t; 

#endif


octetstream_t*  open_octetstream ( const size_t size )
{
    octetstream_t*  ret = (octetstream_t*) calloc ( 1, sizeof(octetstream_t) );
    ret -> data = (uint8_t*) calloc ( 1, size );
    ret -> size = size;
    return ret;
}


int  resize_octetstream ( octetstream_t* const os, const size_t size )
{
    if ( size > os->size ) {
        os -> data = (uint8_t*) realloc ( os -> data, size );
        memset ( os -> data + os -> size, 0, os -> size - size );
        os -> size = size;
    }
    return 0;
}


int  close_octetstream ( octetstream_t* const os )
{
    if ( os == NULL ) 
        return 1;
    if ( os -> data != NULL ) {
        free (os -> data);
        os -> data = NULL;
        free (os);
        return 0;
    } else {
        free (os);
        return 1;
    }
}

/* 
 *  routine to feed EXACTLY one frame (lame->frame_size) worth of data to the
 *  encoding engine. All buffering, resampling, etc, handled by calling program.
 */

inline int  lame_encode_frame_NEW ( 
        lame_t*     lame,
        sample_t*   inbuf_l, 
        sample_t*   inbuf_r,
        uint8_t*    mp3buf, 
        size_t      mp3buf_size )
{
    int  ret;

    switch ( lame -> coding ) {
#ifdef HAVE_MPEG_LAYER1
    case coding_MPEG_Layer_1:
        ret = lame_encode_mp1_frame ( lame->gfp, inbuf_l, inbuf_r, mp3buf, mp3buf_size );
        break;
#endif
#ifdef HAVE_MPEG_LAYER2
    case coding_MPEG_Layer_2:
        ret = lame_encode_mp2_frame ( lame->gfp, inbuf_l, inbuf_r, mp3buf, mp3buf_size );
        break;
#endif
    case coding_MPEG_Layer_3:
        ret = lame_encode_mp3_frame ( lame->gfp, inbuf_l, inbuf_r, mp3buf, mp3buf_size );
        break;
#ifdef HAVE_MPEG_LAYER3_PLUS
    case coding_MPEG_Layer_3_plus:
        ret = lame_encode_mp3plus_frame 
	                            ( lame->gfp, inbuf_l, inbuf_r, mp3buf, mp3buf_size );
        break;
#endif
#ifdef HAVE_AAC
    case coding_MPEG_AAC:
        ret = lame_encode_aac_frame ( lame->gfp, inbuf_l, inbuf_r, mp3buf, mp3buf_size );
        break;
#endif
#ifdef HAVE_VORBIS
    case coding_Ogg_Vorbis:
        ret = lame_encode_ogg_frame ( lame->gfp, inbuf_l, inbuf_r, mp3buf, mp3buf_size );
        break;
#endif
    default:
        ret = -5;
        break;
    }
    if ( ret >= 0 )
        lame->frame_count++;
    return ret;
}


int  internal_lame_encoding_pcm ( lame_t* const lame, octetstream_t* const os, const sample_t * const * const data, size_t len )
{
    size_t           i;
    double           ampl;
    double           dampl;
    int              ampl_on;
    int              ch;
    int              mf_needed;
    int              ret;
    size_t           n_in;
    size_t           n_out;
    sample_t*        mfbuf [2];
    const sample_t*  pdata [2];

    /// this should be moved to lame_init_params();
    // some sanity checks
#if ENCDELAY < MDCTDELAY
# error ENCDELAY is less than MDCTDELAY, see <encoder.h>
#endif
#if FFTOFFSET > BLKSIZE
# error FFTOFFSET is greater than BLKSIZE, see <encoder.h>
#endif

    mf_needed = BLKSIZE + lame->frame_size - FFTOFFSET;           // amount needed for FFT
    mf_needed = Max ( mf_needed, 286 + 576 * (1+lame->mode_gr) ); // amount needed for MDCT/filterbank
    assert ( mf_needed <= MFSIZE );
    ///
    
    os -> length = 0;
    pdata [0]    = data [0];
    pdata [1]    = data [1];
    mfbuf [0]    = lame->mfbuf [0];
    mfbuf [1]    = lame->mfbuf [1];

    if ( lame->ampl != lame->last_ampl ) {
        // fading
        ampl_on = 1;
	ampl    = lame->last_ampl;
	dampl   = len  ?  (lame->ampl - lame->last_ampl) / len * lame->resample_ratio /* ??? */
	               :  0;
	lame->last_ampl = lame->ampl;
    } else if ( lame->ampl != 1. ) {
        // constant amplification
        ampl_on = 1;
	ampl    = lame->last_ampl;
	dampl   = 0.;
    } else {
        // no manipulation (fastest)
        ampl_on = 0;
    }
    
    while ( len > 0 ) {

        /* copy in new samples into mfbuf, with resampling if necessary */
        if ( lame->resample_ratio != 1. )  {
            for ( ch = 0; ch < lame->channels_out; ch++ ) {
                n_in  = len;
                n_out = lame->frame_size;

                ret   = fill_buffer_resample_NEW_MORPHED ( 
                	    lame->resample_in, 
                            mfbuf [ch] + lame->mf_size, &n_out, 
                            pdata [ch]                , &n_in, 
                            ch );
                if ( ret < 0 )
                    return ret;
                pdata [ch] += n_in;
            }
        }
        else {
            n_in = n_out = Min ( lame->frame_size, len );

            for ( ch = 0; ch < lame->channels_out; ch++ ) {
                memcpy (  mfbuf [ch] + lame->mf_size, 
                          pdata [ch], 
                          n_out * sizeof (**mfbuf) );
                pdata [ch] += n_in;
            }
        }
	
	if ( ampl_on )
	    for ( i = 0; i < n_out; i++, ampl += dampl )
                for ( ch = 0; ch < lame->channels_out; ch++ )
		    mfbuf [ch] [lame->mf_size + i] *= ampl;
        
        len                        -= n_in;
        lame->mf_size              += n_out;
        lame->mf_samples_to_encode += n_out;

        // encode ONE frame if enough data available
        if ( lame->mf_size >= mf_needed ) {  
            // Enlarge octetstream buffer if (possibly) needed by (25% + 16K)
            if ( os->size < 16384 + os->length )
                resize_octetstream ( os, os->size + os->size/4 + 16384 );
            // Encode one frame
            ret = lame_encode_frame_NEW ( lame, mfbuf[0], mfbuf[1], 
                                          os->data + os->length, os->size - os->length );

            if (ret < 0) 
                return ret;
            os->length += ret;

            // shift out old samples
            lame->mf_size              -= lame->frame_size;
            lame->mf_samples_to_encode -= lame->frame_size;
            for ( ch = 0; ch < lame->channels_out; ch++ )
                memmove ( mfbuf [ch] + 0, mfbuf [ch] + lame->frame_size, lame->mf_size * sizeof (**mfbuf) );
        }
    }

    return 0;
}


static INLINE void  average ( sample_t* dst, const sample_t* src1, const sample_t* src2, size_t len )
{
    while (len--)
        *dst++ = (*src1++ + *src2++) * 0.5;
}


int  lame_encode_pcm ( 
        lame_t* const   lame,
        octetstream_t*  os,
        const void*     pcm,
        size_t          len,
        uint32_t        flags )
{
    sample_t*           data [2];
    demux_t             retf;
    ssize_t             size;
    const uint8_t*      p;
    const uint8_t* const*  q;
    int                 ret;
    size_t              channels_in = flags & 0xFFFF;
    
    select_demux ( flags, &retf, &size );
    
    data[0] = (sample_t*) calloc (sizeof(sample_t), len);
    data[1] = (sample_t*) calloc (sizeof(sample_t), len);

    if ( (flags & 0xF0000000) == LAME_INTERLEAVED ) {
        p = (const uint8_t*) pcm;
        switch ( lame->channels_out ) {
        case 1:
            switch ( channels_in ) {
            case 1:
                retf ( data[0], p+0*size, 1*size, len );
                break;
            case 2:
                retf ( data[0], p+0*size, 2*size, len );
                retf ( data[1], p+1*size, 2*size, len );
                average ( data[0], data[0], data[1], len );
                memset ( data[1], 0, sizeof(sample_t)*len );
                break;
            default:
                return -1;
                break;
            }
            break;
        case 2:
            switch ( channels_in ) {
            case 1:
                retf ( data[0], p+0*size, 1*size, len );
                memcpy ( data[1], data[0], sizeof(sample_t)*len );
                break;
            case 2:
                retf ( data[0], p+0*size, 2*size, len );
                retf ( data[1], p+1*size, 2*size, len );
                break;
            default:
                return -1;
                break;
            }
            break;
        default:
            return -1;
        }
    } 
    else if ( (flags & 0xF0000000) == LAME_INDIRECT ) {
        q = (const uint8_t* const*) pcm;
        switch ( lame->channels_out ) {
        case 1:
            switch ( channels_in ) {
            case 1:
                retf ( data[0], q[0], 1*size, len );
                break;
            case 2:
                retf ( data[0], q[0], 2*size, len );
                retf ( data[1], q[1], 2*size, len );
                average ( data[0], data[0], data[1], len );
                memset ( data[1], 0, sizeof(sample_t)*len );
                break;
            default:
                return -1;
                break;
            }
            break;
        case 2:
            switch ( channels_in ) {
            case 1:
                retf ( data[0], q[0], 1*size, len );
                memcpy ( data[1], data[0], sizeof(sample_t)*len );
                break;
            case 2:
                retf ( data[0], q[0], 2*size, len );
                retf ( data[1], q[1], 2*size, len );
                break;
            default:
                return -1;
                break;
            }
            break;
        default:
            return -1;
        }
    } else {
        return -1;
    }
    
    ret = internal_lame_encoding_pcm ( lame, os, data, len );
    
    free ( data[0] );
    free ( data[1] );
    return ret;
}                      

                                                                                                                                                                 
static lame_t*  pointer2lame ( void* const handle )
{
    lame_t*             lame = (lame_t*)handle;
    lame_global_flags*  gfp  = (lame_global_flags*)handle;

    if ( lame == NULL )
        return NULL;
    if ( lame->Class_ID == LAME_ID )
        return lame;

    if ( gfp->num_channels == 1  ||  gfp->num_channels == 2 ) {
        lame = (lame_t*) (gfp->internal_flags);
    
        if ( lame == NULL )
            return NULL;
        if ( lame->Class_ID == LAME_ID )
            return lame;
    }
    return NULL;
}


int  LEGACY_lame_encode_buffer (
        void* const    gfp,
        const int16_t  buffer_l [],
        const int16_t  buffer_r [],
        const size_t   nsamples,
        void* const    mp3buf,
        const size_t   mp3buf_size )
{
    const int16_t*  pcm [2];
    octetstream_t*  os;
    int             ret;
    lame_t*         lame;
    
    lame    = pointer2lame (gfp);
    os      = open_octetstream (mp3buf_size);
    pcm [0] = buffer_l;
    pcm [1] = buffer_r;
    
    ret     = lame_encode_pcm ( lame, os, pcm, nsamples, 
                                LAME_INDIRECT | LAME_NATIVE_ENDIAN | LAME_INT16 | 
                                lame->channels_in ); 
    
    memcpy ( mp3buf, os->data, os->length );
    close_octetstream (os);
    return ret;
}


int  LEGACY_lame_encode_buffer_interleaved (
        void* const    gfp,
        const int16_t  buffer [],
        size_t         nsamples,
        void* const    mp3buf,
        const size_t   mp3buf_size )
{
    octetstream_t*  os;
    int             ret;
    lame_t*         lame;
    
    lame    = pointer2lame (gfp);
    os      = open_octetstream (mp3buf_size);
    
    ret     = lame_encode_pcm ( lame, os, buffer, nsamples, 
                                LAME_INTERLEAVED | LAME_NATIVE_ENDIAN | LAME_INT16 | 
                                lame->channels_in ); 
    
    memcpy ( mp3buf, os->data, os->length );
    close_octetstream (os);
    return ret;
}


// What we need as Info?
//    gfp -> out_samplerate   (should be moved to gfc)
//    gfp -> in_samplerate    (should be moved to gfc)
//    gfc -> itime
//    gfc -> inbuf_old
//    gfc -> blackfilt

int  fill_buffer_resample_NEW_MORPHED (
       lame_global_flags * const gfp,   // resample_t* const r
       sample_t          * const out,
       size_t            * const out_req_len,
       const sample_t    * const in,
       size_t            * const in_avail_len,
       size_t              const channel )
{
    lame_internal_flags * const gfc   = gfp->internal_flags;
    double     ratio       = (double) gfp->in_samplerate / gfp->out_samplerate;
    int        len         = *in_avail_len;
    int        desired_len = *out_req_len;
    double     offset;
    double     sum;
    double     fcn;
    double     intratio;
    double     time0;
    sample_t*  inbuf_old;
    int        bpc;         // number of convolution functions to pre-compute
    int        blacksize;
    int        filter_l;
    int        filter_l2;
    int        i;
    int        j;
    int        j2;
    int        joff;
    int        k;
    int        y;
    
    bpc       = gfp->out_samplerate / gcd ( gfp->out_samplerate, gfp->in_samplerate );
    if ( bpc > BPC ) 
        bpc   = BPC;

    intratio  = fabs (ratio - floor (0.5 + ratio)) < 1.e-4;
    fcn       = 0.90 / ratio;
    if ( fcn > 0.90 ) 
        fcn   = 0.90;
    filter_l  = 19 + intratio;
    filter_l2 = filter_l / 2;
    blacksize = filter_l + 1;  // size of data needed for FIR
  
    if ( gfc -> inbuf_old [0] == NULL ) {
        gfc -> inbuf_old [0] = calloc ( blacksize, sizeof (gfc->inbuf_old[0][0]) );
        gfc -> inbuf_old [1] = calloc ( blacksize, sizeof (gfc->inbuf_old[0][0]) );
        for ( i = 0; i <= 2 * bpc; i++ )
            gfc->blackfilt [i] = calloc ( blacksize, sizeof (gfc->blackfilt[0][0]) );

        gfc->itime [0] = 0;
        gfc->itime [1] = 0;

        // precompute blackman filter coefficients
        for ( j = 0; j <= 2*bpc; j++ ) {
            offset = (j-bpc) / (2.*bpc);
            sum    = 0.;
            for ( i = 0; i <= filter_l; i++ ) 
                sum += 
                gfc->blackfilt [j] [i]  = blackman ( i, offset, fcn, filter_l );
            for ( i = 0; i <= filter_l; i++ ) 
                gfc->blackfilt [j] [i] /= sum;
        }
    }
  
    inbuf_old = gfc->inbuf_old [channel];

    // time of j'th element in in  = itime + j/ifreq;
    // time of k'th element in out = j/ofreq
    j = 0;
    for ( k = 0; k < desired_len; k++ ) {
        time0 = k * ratio;       // time of k'th output sample
        j     = floor ( time0 - gfc->itime [channel]  );

        // channeleck if we need more input data
        if ( (filter_l + j - filter_l2) >= len ) 
            break;

        // blackman filter.  by default, window centered at j+.5(filter_l%2)
        // but we want a window centered at time0.
        offset = time0 - gfc->itime [channel] - (j + 0.5 * (filter_l%2) );
        joff   = floor ( offset * 2 * bpc + bpc + 0.5 );
        sum    = 0.;
        
        for ( i = 0 ; i <= filter_l ; i++) {
            j2   = i + j - filter_l2;
            y    = j2 < 0  ?  inbuf_old [blacksize + j2]  :  in [j2];
            sum += y * gfc->blackfilt [joff] [i];
        }
        out [k] = sum;
    }

    // k = number of samples added to out
    // last k sample used data from j..j+filter_l/2
    // remove in_avail_len samples from in:
    // *in_avail_len = Min ( len, j+filter_l/2 )
    
    *in_avail_len         = Min ( len, filter_l + j - filter_l2 );
    gfc->itime [channel] += *in_avail_len - k * ratio;
    for ( i = 0; i < blacksize; i++ )
        inbuf_old [i]     = in [*in_avail_len + i - blacksize];
    *out_req_len          = k;
    return 0;
}

#endif /* KLEMM_44 */

/* end of pcm.c */
