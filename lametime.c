/*
 *   Routines dealing with CPU time and Real time
 */

/*
 * name:        GetCPUTime ( void )
 *
 * description: returns CPU time used by the process
 * input:       none
 * output:      time in seconds
 * known bugs:  may not work in SMP and RPC
 * conforming:  ANSI C
 *
 * There is some old difficult to read code at the end of this file.
 * Can someone integrate this into this function (if useful)?
 */

#include <assert.h>
#include <time.h>

double GetCPUTime ( void )
{
    clock_t  t;

#if defined(_MSC_VER)  ||  defined(__BORLANDC__)    
    t = clock ();
#else
    t = clock ();
#endif    
    return t / (double) CLOCKS_PER_SEC;
}


/*
 * name:        GetRealTime ( void )
 *
 * description: returns real (human) time elapsed relative to a fixed time (mostly 1970-01-01 00:00:00)
 * input:       none
 * output:      time in seconds
 * known bugs:  bad precission with time()
 */

#if defined(__unix__)  ||  defined(SVR4)  ||  defined(BSD)

# include <sys/time.h>
# include <unistd.h>

double GetRealTime ( void )			/* conforming:  SVr4, BSD 4.3 */
{
    struct timeval  t;

    if ( 0 != gettimeofday (&t, NULL) )
        assert (0);
    return t.tv_sec + 1.e-6 * t.tv_usec;
} 

#elif defined(WIN16)  ||  defined(WIN32)

# include <stdio.h>
# include <sys/types.h>
# include <sys/timeb.h>

double GetRealTime ( void )			/* conforming:  Win 95, Win NT */
{
    struct _timeb  t;
    
    _ftime ( &t );
    return t.time + 1.e-3 * t.millitm;
}

#else

double GetRealTime ( void )			/* conforming:  SVr4, SVID, POSIX, X/OPEN, BSD 4.3 */
{						/* BUT NOT GUARANTEED BY ANSI */
    time_t  t;
    
    t = time (NULL);
    return (double) t;
} 

#endif
                              



/* End of lametime.c */
