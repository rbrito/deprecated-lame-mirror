


typedef struct {
    long double  sample_freq_in;
    long double  sample_freq_out;
    float        lowpass_freq;
    int          scale_in;
    int          scale_out;
    sample_t**   fir;
    sample_t*    lastsamples [2];
    ... 
} resample_t;


// Creates and initialize a resample_t structure on the heap and 
// returns a pointer to it. This pointer you can add to 
// lame_internal_flags or lame_global_flags

resample_t*  open_resample (
        OUT long double  sample_freq_in,        // [Hz]
        OUT long double  sample_freq_out,       // [Hz]
        OUT double       lowpass_freq,          // [Hz] or 0 for auto mode
        OUT int          quality );             // Proposal: 0 default, 1 sample select, 2 linear interpol, 4 4-point interpolation, 20 20-point interpolation


// Destroys such a structure and frees all memory
// returns 0 for success or 1 for internal errors

int  close_resample ( INOUT resample_t* const r );


// Do the resampling

int  fill_buffer_resample (			// return code, 0 for success
        INOUT resample_t *const   r,            // internal structure
        IN    sample_t   *const   out,          // where to write the output data
        INOUT int        *const   out_req_len,  // requested output data len/really written output data len
        OUT   sample_t   *const   in,           // where are the input data?
        INOUT int        *const   in_avail_len, // available input data len/consumed input data len
        OUT   int                 channel );    // number of the channel (needed for buffering)


// returns relative frequency error for resampling, 
// 0.01 means 1%

double  resample_freq_error ( OUT resample_t* const r );
{
    return  (sample_freq_in*scale_out - sample_freq_out*scale_in) 
          / (sample_freq_out*scale_in);
}


typedef FLOAT (*scalar_t)  ( const sample_t* p, const sample_t* q );
typedef FLOAT (*scalarn_t) ( const sample_t* p, const sample_t* q, size_t len );

extern scalar_t   scalar4;
extern scalar_t   scalar8;
extern scalar_t   scalar12;
extern scalar_t   scalar16;
extern scalar_t   scalar20;
extern scalar_t   scalar24;
extern scalar_t   scalar64;
extern scalarn_t  scalar;

void init_scalar_functions ( lame_internal_flags *gfc );



/**********************************************************
 *                                                        *
 *           Functions taken from scalar.nas              *
 *                                                        *
 **********************************************************/

/*
 * The scalarxx versions with xx=04,08,12,16,20,24 are fixed size for xx element scalars
 * The scalar1n version is suitable for every non negative length
 * The scalar4n version is only suitable for positive lengths which are a multiple of 4
 *
 * The following are equivalent:
 *    scalar12 (p, q);
 *    scalar4n (p, q, 3);
 *    scalar1n (p, q, 12);
 */ 
 
float_t  scalar04_float32_i387  ( const float32_t* p, const float32_t* q );
float_t  scalar08_float32_i387  ( const float32_t* p, const float32_t* q );
float_t  scalar12_float32_i387  ( const float32_t* p, const float32_t* q );
float_t  scalar16_float32_i387  ( const float32_t* p, const float32_t* q );
float_t  scalar20_float32_i387  ( const float32_t* p, const float32_t* q );
float_t  scalar24_float32_i387  ( const float32_t* p, const float32_t* q );
float_t  scalar4n_float32_i387  ( const float32_t* p, const float32_t* q, const size_t len );
float_t  scalar1n_float32_i387  ( const float32_t* p, const float32_t* q, const size_t len );

float_t  scalar04_float32_3DNow ( const float32_t* p, const float32_t* q );
float_t  scalar08_float32_3DNow ( const float32_t* p, const float32_t* q );
float_t  scalar12_float32_3DNow ( const float32_t* p, const float32_t* q );
float_t  scalar16_float32_3DNow ( const float32_t* p, const float32_t* q );
float_t  scalar20_float32_3DNow ( const float32_t* p, const float32_t* q );
float_t  scalar24_float32_3DNow ( const float32_t* p, const float32_t* q );
float_t  scalar4n_float32_3DNow ( const float32_t* p, const float32_t* q, const size_t len );
float_t  scalar1n_float32_3DNow ( const float32_t* p, const float32_t* q, const size_t len );

float_t  scalar04_float32_SIMD  ( const float32_t* p, const float32_t* q );
float_t  scalar08_float32_SIMD  ( const float32_t* p, const float32_t* q );
float_t  scalar12_float32_SIMD  ( const float32_t* p, const float32_t* q );
float_t  scalar16_float32_SIMD  ( const float32_t* p, const float32_t* q );
float_t  scalar20_float32_SIMD  ( const float32_t* p, const float32_t* q );
float_t  scalar24_float32_SIMD  ( const float32_t* p, const float32_t* q );
float_t  scalar4n_float32_SIMD  ( const float32_t* p, const float32_t* q, const size_t len );
float_t  scalar1n_float32_SIMD  ( const float32_t* p, const float32_t* q, const size_t len );

/* end of resample.h */
