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






#if 0 /**********************************************************************************/

#if   defined(CLOCKS_PER_SEC)			/* ANSI/ISO systems */
# define TS_CLOCKS_PER_SEC   CLOCKS_PER_SEC
#elif defined(CLK_TCK)				/* Non-standard systems */
# define TS_CLOCKS_PER_SEC   CLK_TCK
#elif defined(HZ)				/* Older BSD systems */
# define TS_CLOCKS_PER_SEC   HZ
#else
# error No suitable value for TS_CLOCKS_PER_SEC
#endif


/* divide process time by TS_CLOCKS_PER_TIC to reduce overflow threshold */
#define TS_CLOCKS_PER_TIC  ((CLOCKS_PER_SEC + 63) / 64)
#define TS_SECS_PER_TIC    ((double) TS_CLOCKS_PER_TIC / TS_CLOCKS_PER_SEC)

/*********************************************************/
/* ts_process_time: process time elapsed in seconds      */
/*********************************************************/
double ts_process_time(long frame) 
{
  static clock_t initial_tictime;
  static clock_t previous_time;
  clock_t current_time;

#if defined(_MSC_VER) || defined(__BORLANDC__)

  { static HANDLE hProcess;
    FILETIME Ignored1, Ignored2, KernelTime, UserTime;

    if ( frame==0 ) {
      hProcess = GetCurrentProcess();
    }
        
    /* GetProcessTimes() always fails under Win9x */
    if (GetProcessTimes(hProcess, &Ignored1, &Ignored2, &KernelTime, &UserTime)) {
      LARGE_INTEGER Kernel;
      LARGE_INTEGER User;

      Kernel.LowPart  = KernelTime.dwLowDateTime;
      Kernel.HighPart = KernelTime.dwHighDateTime;
      User.LowPart    = UserTime.dwLowDateTime;
      User.HighPart   = UserTime.dwHighDateTime;

      current_time = (clock_t)((double)(Kernel.QuadPart + User.QuadPart) * TS_CLOCKS_PER_SEC / 10000000);
    } else {
      current_time = clock();
	}
  }
#else
  current_time = clock();
#endif

  if( current_time < previous_time ) {                                        /* Should be moved to lametime.c */
				/* adjust initial_tictime for wrapped time */
				/* whether clock_t is signed or unsigned */
    initial_tictime -= ((previous_time / TS_CLOCKS_PER_TIC)
			+ (current_time - previous_time) / TS_CLOCKS_PER_TIC);
    if( current_time < 0 ) {	/* adjust if clock_t is signed */
      initial_tictime -= -(((clock_t) 1 << (sizeof(clock_t) * 8 - 1))
			   / TS_CLOCKS_PER_TIC);
    }
  }
  previous_time = current_time;
  
  current_time /= TS_CLOCKS_PER_TIC; /* convert process time to tics */

  if (frame == 0) {
    initial_tictime = current_time;
  }

  return (double)(current_time - initial_tictime) * TS_SECS_PER_TIC;
}

#endif /**********************************************************************************/
