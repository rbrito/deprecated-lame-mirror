/*{{{ #defines                        */

#define ENCDELAY      576
#define MDCTDELAY     48
#define FFTOFFSET     (MDCTDELAY + 224)
#define BLKSIZE       1024
#define HBLKSIZE      (BLKSIZE/2 + 1)
#define BLKSIZE_s     256
#define HBLKSIZE_s    (BLKSIZE_s/2 + 1)
#define MFSIZE        (3*1152 + ENCDELAY - MDCTDELAY)
#define INLINE        __inline
#define MAX_TABLES    1002
#define TAPS          32
#define WINDOW_SIZE   15.5
#define WINDOW        hanning
#define MAX_CHANNELS  2

#define MIN(a,b)      ((a) < (b) ? (a) : (b))
#define MAX(a,b)      ((a) > (b) ? (a) : (b))

#ifndef M_PIl
# define M_PIl          3.1415926535897932384626433832795029L
#endif

#define SIN             sinl
#define COS             cosl

/*}}}*/
/*{{{ object ID's                     */

#define LAME_ID         0xFFF88E3BLU
#define RESAMPLE_ID     0x52455341LU
#define PSYCHO_ID       0x50535943LU
#define BITSTREAM_ID    0x42495453LU

/*}}}*/
/*{{{ typedef's                       */

typedef float         float32_t;                   // IEEE-754 32 bit floating point
typedef double        float64_t;                   // IEEE-754 64 bit floating point
typedef long double   float80_t;                   // IEEE-854 80 bit floating point, if available
typedef float32_t     sample_t;                    // type to store one PCM sample
typedef long double   float_t;                     // temporarly results of float operations
typedef long double   double_t;                    // temporarly results of double operations
typedef long double   longdouble_t;                // temporarly results of long double operations

typedef float_t (*scalar_t)  ( const sample_t* p, const sample_t* q );
typedef float_t (*scalarn_t) ( const sample_t* p, const sample_t* q, size_t len );

typedef enum {
    coding_MPEG_Layer_1 = 1,
    coding_MPEG_Layer_2 = 2,
    coding_MPEG_Layer_3 = 3,
    coding_MPEG_AAC     = 4,
    coding_Ogg_Vorbis   = 5,
    coding_MPEG_plus    = 6
} coding_t;

typedef struct {
    unsigned long  Class_ID;                       // Class ID to recognize a resample_t object
    long double    samplefreq_in;                  // Input sample frequency in Hz
    long double    samplefreq_out;                 // requested Output sample frequency in Hz
    float          lowpass_freq;                   // lowpass frequency, this is the -6 dB point
    int            scale_in;                       // the resampling is actually done by scale_out:scale_in, so the real output
    int            scale_out;                      // frequency is samplefreq_in * scale_out / scale_in
    int            taps;                           // number of taps for every FIR resample filter
    sample_t**     fir;                            // the FIR resample filters: fir [scale_out] [taps]
    void*          firfree;                        // start address of the alloced memory for fir, is NOT equal to fir[0] because of additional alignments
    unsigned char* src_step;
    sample_t*      in_old       [MAX_CHANNELS];
    uint64_t       sample_count [MAX_CHANNELS];
    unsigned       fir_stepper  [MAX_CHANNELS];
    int            inp_stepper  [MAX_CHANNELS];
} resample_t;

typedef struct {
    int     num_channels;                          // should be normally different from LAME_ID
    void*   internal_flags;                        // the only important data in lame_global_flags: pointer to the internal falgs
    // ...
} lame_global_flags;

#ifdef __EXTERN__

struct _lame_internal_flags {
    const unsigned long  Class_ID;
};

#else

struct _lame_internal_flags {
    unsigned long  Class_ID;
    lame_global_flags* global_flags;
    struct {
        unsigned int  i387      : 1;
        unsigned int  AMD_3DNow : 1;
        unsigned int  SIMD      : 1;
        unsigned int  SIMD2     : 1;
    }              CPU_features;
    unsigned int   channels_in;
    unsigned int   channels_out;
    sample_t*      mfbuf [2];
    long double    sampfreq_in;
    long double    sampfreq_out;
    resample_t*    resample_in;
    resample_t*    resample_out;
    size_t         frame_size;
    size_t         mf_samples_to_encode;
    size_t         mf_size;
    coding_t       coding;
    unsigned long  frame_count;
    int (*analyzer_callback) ( const struct _lame_internal_flags* lame, size_t size ); // ja, ja, hier ist die struct-Notation notwendig, bedankt euch bei K&R und Konsorten
    double         ampl;
    double         last_ampl;
    size_t         mode_gr;
    // ...
};

#endif

typedef struct _lame_internal_flags  lame_t;
typedef struct _lame_internal_flags  lame_internal_flags;  /* make program mind compatible with FORTRAN 4 */

/*}}}*/
/*{{{ data direction attributes       */

/*
 * These are data stream direction attributes like used in Ada83/Ada95 and in RPC
 * The data direction is seen from the caller to the calling function.
 * Examples:
 *
 *    size_t  fread  ( void INOUT* buffer, size_t items, size_t itemsize, FILE INOUT* fp );
 *    size_t  fwrite ( void OUT  * buffer, size_t items, size_t itemsize, FILE INOUT* fp );
 *    size_t  memset ( void IN   * buffer, unsigned char value, size_t size );
 *
 * Return values are implizit IN (note that here C uses the opposite attribute).
 * Arguments not transmitted via references are implizit OUT.
 */

#define OUT            /* [out]   */ const
#define INOUT          /* [inout] */
#define IN             /* [in]    */
#define OUTTR          /* [out]: data is modified like [inout], but you don't get any useful back */

/*}}}*/
/*{{{ Test some error conditions      */

#ifndef __LOC__
# define _STR2(x)      #x
# define _STR1(x)      _STR2(x)
# define __LOC__       __FILE__ "(" _STR1(__LINE__) ") : warning: "
#endif

/* The current code doesn't work on machines with non 8 bit char's in any way, so abort */

#if CHAR_BIT != 8
# pragma message ( __LOC__ "Machines with CHAR_BIT != 8 not yet supported" )
# pragma error
#endif

/*}}}*/
/*{{{ bitstream object                */

/*
 *  About handling of octetstreams:
 *  CHAR_BIT = 8: data points to a normal memory block which can be written to
 *                disk via fwrite
 *  CHAR_BIT > 8: On every char only the bits from 0...7 are used. Depending on the
 *                host I/O it can be possible that you must first pack the data
 *                stream before writing to disk.
 *  CHAR_BIT < 8: Not allowed by C
 */

#ifdef __EXTERN__

typedef struct {
    volatile const void* volatile const   data;
    const size_t                          length;
    const size_t                          size;
} octetstream_t;

#else

typedef struct {
    uint8_t*   data;
    size_t     length;
    size_t     size;
} octetstream_t;

#endif

/*
 *  Now some information how PCM data can be specified. PCM data
 *  is specified by 3 attributes: pointer, length information
 *  and attributes.
 *    - Audio is always stored in 2D arrays, which are collapsing to 1D
 *      in the case of monaural input
 *    - 2D arrays can be stored as 2D arrays or as pointers to 1D arrays.
 *    - 2D data can be stored as samples*channels or as channels*samples
 *    - This gives 4 kinds of storing PCM data:
 *      + pcm [samples][channels]       (LAME_INTERLEAVED)
 *      + pcm [channels][samples]       (LAME_CHAINED)
 *      + (*pcm) [samples]              (LAME_INDIRECT)
 *      + (*pcm) [channels]
 *    - The last I have never seen and it have a huge overhead (67% ... 200%),
 *      so the first three are implemented.
 *    - The monaural 1D cases can also be handled by the first two attributes
 */

#define  LAME_INTERLEAVED       0x10000000
#define  LAME_CHAINED           0x20000000
#define  LAME_INDIRECT          0x30000000

/*
 *  Now we need some information about the byte order of the data.
 *  There are 4 cases possible (if you are not fully support such strange
 *  Machines like the PDPs):
 *    - You know the absolute byte order of the data (LAME_LITTLE_ENDIAN, LAME_BIG_ENDIAN)
 *    - You know the byte order from the view of the current machine
 *      (LAME_NATIVE_ENDIAN, LAME_OPPOSITE_ENDIAN)
 *    - The use of LAME_OPPOSITE_ENDIAN is NOT recommended because it is
 *      is a breakable attribute, use LAME_LITTLE_ENDIAN or LAME_BIG_ENDIAN
 *      instead
 */

#define  LAME_NATIVE_ENDIAN     0x00000000
#define  LAME_OPPOSITE_ENDIAN   0x01000000
#define  LAME_LITTLE_ENDIAN     0x02000000
#define  LAME_BIG_ENDIAN        0x03000000

/*
 *  The next attribute is the data type of the input data.
 *  There are currently 2 kinds of input data:
 *    - C based:
 *          LAME_{SHORT,INT,LONG}
 *          LAME_{FLOAT,DOUBLE,LONGDOUBLE}
 *    - Binary representation based:
 *          LAME_{UINT,INT}{8,16,24,32}
 *          LAME_{A,U}LAW
 *          LAME_FLOAT{32,64,80_ALIGN{2,4,8}}
 *
 *  Don't use the C based for external data.
 */

#define  LAME_SILENCE           0x00010000
#define  LAME_UINT8             0x00020000
#define  LAME_INT8              0x00030000
#define  LAME_UINT16            0x00040000
#define  LAME_INT16             0x00050000
#define  LAME_UINT24            0x00060000
#define  LAME_INT24             0x00070000
#define  LAME_UINT32            0x00080000
#define  LAME_INT32             0x00090000
#define  LAME_FLOAT32           0x00140000
#define  LAME_FLOAT64           0x00180000
#define  LAME_FLOAT80_ALIGN2    0x001A0000
#define  LAME_FLOAT80_ALIGN4    0x001C0000
#define  LAME_FLOAT80_ALIGN8    0x00200000
#define  LAME_SHORT             0x00210000
#define  LAME_INT               0x00220000
#define  LAME_LONG              0x00230000
#define  LAME_FLOAT             0x00240000
#define  LAME_DOUBLE            0x00250000
#define  LAME_LONGDOUBLE        0x00260000
#define  LAME_ALAW              0x00310000
#define  LAME_ULAW              0x00320000

/*
 *  The last attribute is the number of input channels. Currently
 *  1...65535 channels are possible, but only 1 and 2 are supported.
 *  So matrixing or MPEG-2 MultiChannelSupport are not a big problem.
 *
 *  Note: Some people use the word 'stereo' for 2 channel stereophonic sound.
 *        But stereophonic sound is a collection of ALL methods to restore the
 *        stereophonic sound field starting from 2 channels up to audio
 *        holography.
 */

#define LAME_MONO               0x00000001
#define LAME_STEREO             0x00000002
#define LAME_STEREO_2_CHANNELS  0x00000002
#define LAME_STEREO_3_CHANNELS  0x00000003
#define LAME_STEREO_4_CHANNELS  0x00000004
#define LAME_STEREO_5_CHANNELS  0x00000005
#define LAME_STEREO_6_CHANNELS  0x00000006
#define LAME_STEREO_7_CHANNELS  0x00000007
#define LAME_STEREO_65535_CHANNELS\
                                0x0000FFFF


extern scalar_t   scalar4;
extern scalar_t   scalar8;
extern scalar_t   scalar12;
extern scalar_t   scalar16;
extern scalar_t   scalar20;
extern scalar_t   scalar24;
extern scalar_t   scalar32;
extern scalarn_t  scalar4n;
extern scalarn_t  scalar1n;

float_t  scalar04_float32_i387  ( const float32_t* p, const float32_t* q );
float_t  scalar08_float32_i387  ( const float32_t* p, const float32_t* q );
float_t  scalar12_float32_i387  ( const float32_t* p, const float32_t* q );
float_t  scalar16_float32_i387  ( const float32_t* p, const float32_t* q );
float_t  scalar20_float32_i387  ( const float32_t* p, const float32_t* q );
float_t  scalar24_float32_i387  ( const float32_t* p, const float32_t* q );
float_t  scalar32_float32_i387  ( const float32_t* p, const float32_t* q );
float_t  scalar4n_float32_i387  ( const float32_t* p, const float32_t* q, const size_t len );
float_t  scalar1n_float32_i387  ( const float32_t* p, const float32_t* q, const size_t len );

float_t  scalar04_float32_3DNow ( const float32_t* p, const float32_t* q );
float_t  scalar08_float32_3DNow ( const float32_t* p, const float32_t* q );
float_t  scalar12_float32_3DNow ( const float32_t* p, const float32_t* q );
float_t  scalar16_float32_3DNow ( const float32_t* p, const float32_t* q );
float_t  scalar20_float32_3DNow ( const float32_t* p, const float32_t* q );
float_t  scalar24_float32_3DNow ( const float32_t* p, const float32_t* q );
float_t  scalar32_float32_3DNow ( const float32_t* p, const float32_t* q );
float_t  scalar4n_float32_3DNow ( const float32_t* p, const float32_t* q, const size_t len );
float_t  scalar1n_float32_3DNow ( const float32_t* p, const float32_t* q, const size_t len );

float_t  scalar04_float32_SIMD  ( const float32_t* p, const float32_t* q );
float_t  scalar08_float32_SIMD  ( const float32_t* p, const float32_t* q );
float_t  scalar12_float32_SIMD  ( const float32_t* p, const float32_t* q );
float_t  scalar16_float32_SIMD  ( const float32_t* p, const float32_t* q );
float_t  scalar20_float32_SIMD  ( const float32_t* p, const float32_t* q );
float_t  scalar24_float32_SIMD  ( const float32_t* p, const float32_t* q );
float_t  scalar32_float32_SIMD  ( const float32_t* p, const float32_t* q );
float_t  scalar4n_float32_SIMD  ( const float32_t* p, const float32_t* q, const size_t len );
float_t  scalar1n_float32_SIMD  ( const float32_t* p, const float32_t* q, const size_t len );

float_t  scalar04_float32       ( const float32_t* p, const float32_t* q );
float_t  scalar08_float32       ( const float32_t* p, const float32_t* q );
float_t  scalar12_float32       ( const float32_t* p, const float32_t* q );
float_t  scalar16_float32       ( const float32_t* p, const float32_t* q );
float_t  scalar20_float32       ( const float32_t* p, const float32_t* q );
float_t  scalar24_float32       ( const float32_t* p, const float32_t* q );
float_t  scalar32_float32       ( const float32_t* p, const float32_t* q );
float_t  scalar4n_float32       ( const float32_t* p, const float32_t* q, const size_t len );
float_t  scalar1n_float32       ( const float32_t* p, const float32_t* q, const size_t len );


/*{{{ some prototypes                 */

octetstream_t*  octetstream_open   ( const size_t  size );
int             octetstream_resize ( octetstream_t* const  os, const size_t  size );
int             octetstream_close  ( octetstream_t* const  os );

int  lame_encode_pcm (
        lame_t* const   lame,
        octetstream_t*  os,
        const void*     pcm,
        size_t          len,
        uint32_t        flags );
        
int  lame_encode_pcm_flush (
        lame_t*        const  lame,
        octetstream_t* const  os );

int  LEGACY_lame_encode_buffer (
        void* const    gfp,
        const int16_t  buffer_l [],
        const int16_t  buffer_r [],
        const size_t   nsamples,
        void* const    mp3buf,
        const size_t   mp3buf_size );
        
int  LEGACY_lame_encode_buffer_interleaved (
        void* const    gfp,
        const int16_t  buffer [],
        size_t         nsamples,
        void* const    mp3buf,
        const size_t   mp3buf_size );
        
int  LEGACY_lame_encode_flush (
        void* const    gfp,
        void* const    mp3buf,
        const size_t   mp3buf_size );
        
resample_t*  resample_open (
        OUT long double  sampfreq_in,    // [Hz]
        OUT long double  sampfreq_out,   // [Hz]
        OUT double       lowpass_freq,   // [Hz] or <0 for auto mode
        OUT int          quality );      // Proposal: 0 default, 1 sample select, 2 linear interpol, 4 4-point interpolation, 32 32-point interpolation
        
int  resample_buffer (                          // return code, 0 for success
        INOUT resample_t *const   r,            // internal structure
        IN    sample_t   *const   out,          // where to write the output data
        INOUT size_t     *const   out_req_len,  // requested output data len/really written output data len
        OUT   sample_t   *const   in,           // where are the input data?
        INOUT size_t     *const   in_avail_len, // available input data len/consumed input data len
        OUT   size_t              channel );    // number of the channel (needed for buffering)

int  resample_close ( INOUT resample_t* const r );

void         init_scalar_functions   ( OUT lame_t* const lame );
long double  unround_Samplefrequency ( OUT long double freq );

int  lame_encode_mp3_frame (             // return code, 0 for success
        INOUT lame_global_flags*,        // internal context structure
        OUTTR sample_t * inbuf_l,        // data for left  channel
        OUTTR sample_t * inbuf_r,        // data for right channel
        IN    uint8_t  * mp3buf,         // where to write the coded data
        OUT   size_t     mp3buf_size );  // maximum size of coded data

int  lame_encode_ogg_frame (             // return code, 0 for success
        INOUT lame_global_flags,         // internal context structure
        OUT   sample_t * inbuf_l,        // data for left  channel
        OUT   sample_t * inbuf_r,        // data for right channel
        IN    uint8_t  * mp3buf,         // where to write the coded data
        OUT   size_t     mp3buf_size );  // maximum size of coded data

/*}}}*/

/* end of pcm.h */
