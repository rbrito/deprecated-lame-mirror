
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

