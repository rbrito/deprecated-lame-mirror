/* $Id$ */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <math.h>
#include <assert.h>

#include "resample.h"

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

/* FIR resampling */

double  sinpi ( double x )
{
    x -= (int)floor(x) & ~1;

    switch ( (int)(4.*x) ) {
    default: assert (0);
    case  0: return +sin ( M_PI * (0.0+x) );
    case  1: return +cos ( M_PI * (0.5-x) );
    case  2: return +cos ( M_PI * (x-0.5) );
    case  3: return +sin ( M_PI * (1.0-x) );
    case  4: return -sin ( M_PI * (x-1.0) );
    case  5: return -cos ( M_PI * (1.5-x) );
    case  6: return -cos ( M_PI * (x-1.5) );
    case  7: return -sin ( M_PI * (2.0-x) );
    }
}

double  cospi ( double x )
{
    x -= (int)floor(x) & ~1;

    switch ( (int)(4.*x) ) {
    default: assert (0);
    case  0: return +cos ( M_PI * (x-0.0) );
    case  1: return +sin ( M_PI * (0.5-x) );
    case  2: return -sin ( M_PI * (x-0.5) );
    case  3: return -cos ( M_PI * (1.0-x) );
    case  4: return -cos ( M_PI * (x-1.0) );
    case  5: return -sin ( M_PI * (1.5-x) );
    case  6: return +sin ( M_PI * (x-1.5) );
    case  7: return +cos ( M_PI * (2.0-x) );
    }
}


double  sinc ( double x )
{
    if ( x == 0. )
        return 1.;

    if ( x <  0. )
        x = -x;

    return sinpi ( x ) / ( M_PI * x );
}

double  hanning ( double x )
{
    if ( fabs (x) >= 1 )
        return 0.;

    x = cospi (0.5 * x);
    return x * x;
}

double  hamming ( double x )
{
    if ( fabs (x) >= 1 )
        return 0.;

    x = cospi (x);
    return 0.54 + 0.46 * x;
}

double  blackman ( double x )
{
    if ( fabs (x) >= 1 )
        return 0.;

    x = cospi (x);
    return (0.16 * x + 0.50) * x + 0.34;        // using addition theorem of arc functions
}

double  blackman1 ( double x )
{
    if ( fabs (x) >= 1 )
        return 0.;

    x += 1.;
    return 0.42 - 0.50*cospi(x) + 0.08*cospi(2*x);
}
 
double  blackman2 ( double x )
{
    if ( fabs (x) >= 1 )
        return 0.;

    x += 1.;
    return 0.375 - 0.50*cospi(x) + 0.125*cospi(2*x);
}

double  blackmanharris_nuttall ( double x )
{
    if ( fabs (x) >= 1 )
        return 0.;

    x += 1.;
    return (10 - 15*cospi (x) + 6*cospi (2*x) - cospi (3*x) ) * (1./32);
}

double  blackmanharris_min4 ( double x )
{
    if ( fabs (x) >= 1 )
        return 0.;

    x += 1.;
    return 0.355768 - 0.487396*cospi (x) + 0.144232*cospi (2*x) - 0.012604*cospi (3*x);
}

/*

  Blackman-Harris windows, which have the general equation:

           w(n) = a0 - a1 cos(2pn/N) + a2 cos(4pn/N) - a3 cos(6pn/N).

  When Nuttall set the ak coefficients to the "minimum 4-term" values of a0 = 0.355768, a1 =
  0.487396, a2 = 0.144232, and a3 = 0.012604, he obtained the window and the frequency
  response shown in Figure 8. The minimum-4-term window loses some frequency
  resolution because it has a wide main lobe, but the first side lobe drops to -93 db.

  If you require fast side-lobe rolloff in an application, consider Nuttall's Equation (30). It
  applies the following coefficients to the general equation above: a0 = 10/32, a1 = 15/32, a2 =
  6/32, and a3 = 1/32. Figure 8 also includes the window and frequency response for Nuttall's
  Equation (30). This window has a first side-lobe level of -61 dB and a significant side-lobe
  rolloff of -42 dB/octave.

  The first investigators of windowing functions determined that window functions should
  have zero values at their boundaries and so should their successive derivatives. If a
  window's kth derivative is zero at the boundaries, the peaks of the window's side lobes will
  decay at a rate of 6(k+2) dB/octave. T&MW.

*/                

double  Factorize ( const long double f1, const long double f2, int* x1, int* x2 )
{
    unsigned     i;
    long         ltmp;
    long double  ftmp;
    double       minerror = 1.;
    double       abserror = 1.;
    double       error;

    assert ( f1 > 0. );
    assert ( f2 > 0. );
    assert ( x1 != NULL );
    assert ( x2 != NULL );

    for ( i = 1; i <= MAX_TABLES; i++) {
        ftmp  = f2 * i / f1;
        ltmp  = (long) ( ftmp + 0.5 );
        error = fabs ( ltmp/ftmp - 1.);
        if ( error < minerror ) {
            *x1      = i;
            *x2      = (int)ltmp;
            minerror = error * 0.999999999;
            abserror = ltmp/ftmp - 1.;
        }
    }
    return abserror;
}

////////////////////////////////////////////////////////////////////////

float_t  scalar04_float32 ( const sample_t* p, const sample_t* q )
{
    return  p[0]*q[0] + p[1]*q[1] + p[2]*q[2] + p[3]*q[3];
}

float_t  scalar08_float32 ( const sample_t* p, const sample_t* q )
{
    return  p[0]*q[0] + p[1]*q[1] + p[2]*q[2] + p[3]*q[3]
          + p[4]*q[4] + p[5]*q[5] + p[6]*q[6] + p[7]*q[7];
}

float_t  scalar12_float32 ( const sample_t* p, const sample_t* q )
{
    return  p[0]*q[0] + p[1]*q[1] + p[ 2]*q[ 2] + p[ 3]*q[ 3]
          + p[4]*q[4] + p[5]*q[5] + p[ 6]*q[ 6] + p[ 7]*q[ 7]
          + p[8]*q[8] + p[9]*q[9] + p[10]*q[10] + p[11]*q[11];
}

float_t  scalar16_float32 ( const sample_t* p, const sample_t* q )
{
    return  p[ 0]*q[ 0] + p[ 1]*q[ 1] + p[ 2]*q[ 2] + p[ 3]*q[ 3]
          + p[ 4]*q[ 4] + p[ 5]*q[ 5] + p[ 6]*q[ 6] + p[ 7]*q[ 7]
          + p[ 8]*q[ 8] + p[ 9]*q[ 9] + p[10]*q[10] + p[11]*q[11]
          + p[12]*q[12] + p[13]*q[13] + p[14]*q[14] + p[15]*q[15];
}

float_t  scalar20_float32 ( const sample_t* p, const sample_t* q )
{
    return  p[ 0]*q[ 0] + p[ 1]*q[ 1] + p[ 2]*q[ 2] + p[ 3]*q[ 3]
          + p[ 4]*q[ 4] + p[ 5]*q[ 5] + p[ 6]*q[ 6] + p[ 7]*q[ 7]
          + p[ 8]*q[ 8] + p[ 9]*q[ 9] + p[10]*q[10] + p[11]*q[11]
          + p[12]*q[12] + p[13]*q[13] + p[14]*q[14] + p[15]*q[15]
          + p[16]*q[16] + p[17]*q[17] + p[18]*q[18] + p[19]*q[19];
}

float_t  scalar24_float32 ( const sample_t* p, const sample_t* q )
{
    return  p[ 0]*q[ 0] + p[ 1]*q[ 1] + p[ 2]*q[ 2] + p[ 3]*q[ 3]
          + p[ 4]*q[ 4] + p[ 5]*q[ 5] + p[ 6]*q[ 6] + p[ 7]*q[ 7]
          + p[ 8]*q[ 8] + p[ 9]*q[ 9] + p[10]*q[10] + p[11]*q[11]
          + p[12]*q[12] + p[13]*q[13] + p[14]*q[14] + p[15]*q[15]
          + p[16]*q[16] + p[17]*q[17] + p[18]*q[18] + p[19]*q[19]
          + p[20]*q[20] + p[21]*q[21] + p[22]*q[22] + p[23]*q[23];
}

float_t  scalar32_float32 ( const sample_t* p, const sample_t* q )
{
    return  p[ 0]*q[ 0] + p[ 1]*q[ 1] + p[ 2]*q[ 2] + p[ 3]*q[ 3]
          + p[ 4]*q[ 4] + p[ 5]*q[ 5] + p[ 6]*q[ 6] + p[ 7]*q[ 7]
          + p[ 8]*q[ 8] + p[ 9]*q[ 9] + p[10]*q[10] + p[11]*q[11]
          + p[12]*q[12] + p[13]*q[13] + p[14]*q[14] + p[15]*q[15]
          + p[16]*q[16] + p[17]*q[17] + p[18]*q[18] + p[19]*q[19]
          + p[20]*q[20] + p[21]*q[21] + p[22]*q[22] + p[23]*q[23]
          + p[24]*q[24] + p[25]*q[25] + p[26]*q[26] + p[27]*q[27]
          + p[28]*q[28] + p[29]*q[29] + p[30]*q[30] + p[31]*q[31];
}

float_t  scalar4n_float32 ( const sample_t* p, const sample_t* q, size_t len )
{
    double sum = p[0]*q[0] + p[1]*q[1] + p[2]*q[2] + p[3]*q[3];

    while (--len) {
        p   += 4;
        q   += 4;
        sum += p[0]*q[0] + p[1]*q[1] + p[2]*q[2] + p[3]*q[3];
    }
    return sum;
}

float_t  scalar1n_float32 ( const sample_t* p, const sample_t* q, size_t len )
{
    double sum = 0.;
    
    if (len & 1) sum += p[0]*q[0]            , p += 1, q += 1;
    if (len & 2) sum += p[0]*q[0] + p[1]*q[1], p += 2, q += 2;
    len >>= 2;
    while (len--) {
        sum += p[0]*q[0] + p[1]*q[1] + p[2]*q[2] + p[3]*q[3];
        p   += 4;
        q   += 4;
    }
    return sum;
}


scalar_t   scalar04;
scalar_t   scalar08;
scalar_t   scalar12;
scalar_t   scalar16;
scalar_t   scalar20;
scalar_t   scalar24;
scalarn_t  scalar4n;
scalarn_t  scalar1n;


void init_scalar_functions ( const lame_internal_flags* const gfc )
{
    scalar04 = scalar04_float32;
    scalar08 = scalar08_float32;
    scalar12 = scalar12_float32;
    scalar16 = scalar16_float32;
    scalar20 = scalar20_float32;
    scalar24 = scalar24_float32;
    scalar32 = scalar32_float32;
    scalar4n = scalar4n_float32;
    scalar1n = scalar1n_float32;

    if ( gfc -> CPU_features_i387 ) {    
        scalar04 = scalar04_float32_i387;
        scalar08 = scalar08_float32_i387;
        scalar12 = scalar12_float32_i387;
        scalar16 = scalar16_float32_i387;
        scalar20 = scalar20_float32_i387;
        scalar24 = scalar24_float32_i387;
        scalar32 = scalar32_float32_i387;
        scalar4n = scalar4n_float32_i387;
        scalar1n = scalar1n_float32_i387;
    }
    
    if ( gfc -> CPU_features_3DNow ) {
        scalar04 = scalar04_float32_3DNow;
        scalar08 = scalar08_float32_3DNow;
        scalar12 = scalar12_float32_3DNow;
        scalar16 = scalar16_float32_3DNow;
        scalar20 = scalar20_float32_3DNow;
        scalar24 = scalar24_float32_3DNow;
        scalar32 = scalar32_float32_3DNow;
        scalar4n = scalar4n_float32_3DNow;
        scalar1n = scalar1n_float32_3DNow;
    }

    if ( gfc -> CPU_features_SIMD ) {
        scalar04 = scalar04_float32_SIMD;
        scalar08 = scalar08_float32_SIMD;
        scalar12 = scalar12_float32_SIMD;
        scalar16 = scalar16_float32_SIMD;
        scalar20 = scalar20_float32_SIMD;
        scalar24 = scalar24_float32_SIMD;
        scalar32 = scalar32_float32_SIMD;
        scalar4n = scalar4n_float32_SIMD;
        scalar1n = scalar1n_float32_SIMD;
    }

}

/* end of resample.c */
