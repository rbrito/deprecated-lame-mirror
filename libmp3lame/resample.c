static FLOAT  std_scalar4  ( const sample_t* p, const sample_t* q )
{
    return  p[0]*q[0] + p[1]*q[1] + p[2]*q[2] + p[3]*q[3];
}

static FLOAT  std_scalar8  ( const sample_t* p, const sample_t* q )
{
    return  p[0]*q[0] + p[1]*q[1] + p[2]*q[2] + p[3]*q[3]
          + p[4]*q[4] + p[5]*q[5] + p[6]*q[6] + p[7]*q[7];
}

static FLOAT  std_scalar12 ( const sample_t* p, const sample_t* q )
{
    return  p[0]*q[0] + p[1]*q[1] + p[ 2]*q[ 2] + p[ 3]*q[ 3]
          + p[4]*q[4] + p[5]*q[5] + p[ 6]*q[ 6] + p[ 7]*q[ 7]
          + p[8]*q[8] + p[9]*q[9] + p[10]*q[10] + p[11]*q[11];
}

static FLOAT  std_scalar16 ( const sample_t* p, const sample_t* q )
{
    return  p[ 0]*q[ 0] + p[ 1]*q[ 1] + p[ 2]*q[ 2] + p[ 3]*q[ 3]
          + p[ 4]*q[ 4] + p[ 5]*q[ 5] + p[ 6]*q[ 6] + p[ 7]*q[ 7]
          + p[ 8]*q[ 8] + p[ 9]*q[ 9] + p[10]*q[10] + p[11]*q[11]
          + p[12]*q[12] + p[13]*q[13] + p[14]*q[14] + p[15]*q[15];
}

static FLOAT  std_scalar20 ( const sample_t* p, const sample_t* q )
{
    return  p[ 0]*q[ 0] + p[ 1]*q[ 1] + p[ 2]*q[ 2] + p[ 3]*q[ 3]
          + p[ 4]*q[ 4] + p[ 5]*q[ 5] + p[ 6]*q[ 6] + p[ 7]*q[ 7]
          + p[ 8]*q[ 8] + p[ 9]*q[ 9] + p[10]*q[10] + p[11]*q[11]
          + p[12]*q[12] + p[13]*q[13] + p[14]*q[14] + p[15]*q[15]
          + p[16]*q[16] + p[17]*q[17] + p[18]*q[18] + p[19]*q[19];
}


/* currently len must be a multiple of 4 and >= 8. This is necessary for SIMD1 */

static FLOAT  std_scalar ( const sample_t* p, const sample_t* q, size_t len )
{
    double sum = p[0]*q[0] + p[1]*q[1] + p[2]*q[2] + p[3]*q[3];

    assert (len >= 8);
        
    do {
        p   += 4;
        q   += 4;
        len -= 4;
        sum += p[0]*q[0] + p[1]*q[1] + p[2]*q[2] + p[3]*q[3];
    } while ( len > 4 );
    
    assert (len == 0);
    
    return sum;
}

static FLOAT  std_scalar64 ( const sample_t* p, const sample_t* q )
{
    return std_scalar ( p, q, 64 );
}

scalar_t   scalar4;
scalar_t   scalar8;
scalar_t   scalar12;
scalar_t   scalar16;
scalar_t   scalar20;
scalar_t   scalar64;
scalarn_t  scalar;

void init_scalar_functions ( lame_internal_flags *gfc )
{
    scalar4  = std_scalar4;
    scalar8  = std_scalar8;
    scalar12 = std_scalar12;
    scalar16 = std_scalar16;
    scalar20 = std_scalar20;
    scalar64 = std_scalar64;
    scalar   = std_scalar;
}


/* FIR resampling */

INLINE double sinpi ( double x )
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

INLINE double cospi ( double x )
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
    case  6: return +sin ( M_PI * (1.5+x) );
    case  7: return +cos ( M_PI * (2.0-x) );
    }
}


INLINE double sinc ( double x )
{
    if ( x == 0. )
        return 1.;

    if ( x <  0. )
        x = -x;

    return sinpi ( x ) / ( M_PI * x );
}

INLINE double blackman ( double x )
{
    if ( fabs (x) >= 1 )
        return 0.;

    x = cospi (x);
    return (0.16 * x + 0.50) * x + 0.34;        // using addition theorem of arc functions
}
 
INLINE double blackmanharris ( double x )
{
    if ( x <  0. ) x = -x;
    if ( x >= 1. ) return 0.;
}

INLINE double hanning ( double x )
{
    if ( x <  0. ) x = -x;
    if ( x >= 1. ) return 0.;
}

INLINE double hamming ( double x )
{
    if ( x <  0. ) x = -x;
    if ( x >= 1. ) return 0.;
}

double  Factorize ( const double f1, const double f2, int* x1, int* x2 )
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

