#include <stdio.h>
#include <math.h>
#include "resample.h"

float a1 [128];
float a2 [128];

int main ( int argc, char** argv )
{
    int     i;
    double  x;

#if 0    
    for ( i = -1000; i <= 1000; i++ ) {
        x = 0.001 * i;
	printf ( "%5d %12.6f\n", i, 
                 hanning  ( x )
               );
    }
    printf ("%%%%\n");
    
    for ( i = -1000; i <= 1000; i++ ) {
        x = 0.001 * i;
	printf ( "%5d %12.6f\n", i, 
                 hamming  ( x )
	       );
    }
    printf ("%%%%\n");
    
    for ( i = -1000; i <= 1000; i++ ) {
        x = 0.001 * i;
	printf ( "%5d %12.6f\n", i, 
                 blackman ( x )
	       );
    }
    printf ("%%%%\n");

    for ( i = -1000; i <= 1000; i++ ) {
        x = 0.001 * i;
	printf ( "%5d %12.6f\n", i, 
                 blackmanharris_nuttall ( x )
	       );
    }
    printf ("%%%%\n");

    for ( i = -1000; i <= 1000; i++ ) {
        x = 0.001 * i;
	printf ( "%5d %12.6f\n", i, 
                 blackmanharris_min4    ( x )
	       );
    }
    printf ("%%%%\n");
#else


    for ( i = 0; i < 24; i++ ) {
        a1 [i] = sin(i)+0.2*sin(1.8*i)+log(2+i);
	a2 [i] = cos(0.1*i);
    }
    printf ( "%20.12f\n", (double)scalar24_float32       ( a1, a2 ) );
    printf ( "%20.12f\n", (double)scalar24_float32_i387  ( a1, a2 ) );
    printf ( "%20.12f\n", (double)scalar24_float32_3DNow ( a1, a2 ) );
    
#endif    
    
    return 0;
}

/* end of scalartest.c */
