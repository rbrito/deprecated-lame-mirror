/*
 *	time status related function source file
 *
 *	Copyright (c) 1999 Mark Taylor
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "lame.h"
#ifdef BRHIST
#include "brhist.h"
#endif
#include <assert.h>
#include <time.h>

#ifdef __unix__
   #include <sys/time.h>
   #include <unistd.h>
#endif

#ifdef _WIN32
/* needed to set stdin to binary on windoze machines */
  #include <io.h>
  #include <fcntl.h>
  #include <windows.h>
#endif


#include "timestatus.h"
#include "lametime.h"
#include "main.h"

#if defined(CLOCKS_PER_SEC)
/* ANSI/ISO systems */
# define TS_CLOCKS_PER_SEC CLOCKS_PER_SEC
#elif defined(CLK_TCK)
/* Non-standard systems */
# define TS_CLOCKS_PER_SEC CLK_TCK
#elif defined(HZ)
/* Older BSD systems */
# define TS_CLOCKS_PER_SEC HZ
#else
# error no suitable value for TS_CLOCKS_PER_SEC
#endif


/* divide process time by TS_CLOCKS_PER_TIC to reduce overflow threshold */
#define TS_CLOCKS_PER_TIC ((CLOCKS_PER_SEC + 63) / 64)
#define TS_SECS_PER_TIC ((double) TS_CLOCKS_PER_TIC / TS_CLOCKS_PER_SEC)

/*********************************************************/
/* ts_real_time: real time elapsed in seconds            */
/*********************************************************/

double ts_real_time (long frame) 
{
#ifdef __unix__

   static long double  initial_time;
   long double         current_time;
   struct timeval      t;

   gettimeofday (&t, NULL);

   current_time = t.tv_sec + 1.e-6l * t.tv_usec;
   if (frame == 0)
       initial_time = current_time;

   return (double)(current_time - initial_time);

#else

   static time_t  initial_time;
   time_t         current_time;

   time (&current_time);

   if (frame == 0)
       initial_time = current_time;

   return (double) difftime (current_time, initial_time);

#endif
}

/*********************************************************/
/* ts_process_time: process time elapsed in seconds      */
/*********************************************************/
double ts_process_time(long frame) {
  static clock_t initial_tictime;
  static clock_t previous_time;
  clock_t current_time;

#if ( defined(_MSC_VER) || defined(__BORLANDC__) ) 

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

  if( current_time < previous_time ) {
				/* adjust initial_tictime for wrapped time */
				/* whether clock_t is signed or unsigned */
    initial_tictime -= ((previous_time / TS_CLOCKS_PER_TIC)
			+ (current_time - previous_time) / TS_CLOCKS_PER_TIC);
    if( current_time < 0 ) {	/* adjust if clock_t is signed */
      initial_tictime -= -(((clock_t) 1 << ((int)sizeof(clock_t) * 8 - 1))
			   / TS_CLOCKS_PER_TIC);
    }
  }
  previous_time = current_time;
  
  current_time /= TS_CLOCKS_PER_TIC; /* convert process time to tics */

  if (frame == 0) {
    initial_tictime = current_time;
  }

  return (double)((double)(current_time - initial_tictime) * TS_SECS_PER_TIC);
}

#undef TS_SECS_PER_TIC
#undef TS_CLOCKS_PER_TIC
#undef TS_CLOCKS_PER_SEC

typedef struct ts_times {
  double so_far;
  double estimated;
  double speed;
  double eta;
} ts_times;

/*********************************************************/
/* ts_calc_times: calculate time info (eta, speed, etc.) */
/*********************************************************/
void ts_calc_times(ts_times *tstime, int samp_rate, long frame, long frames,int framesize)
{
  if (frame > 0) {
    tstime->estimated = tstime->so_far * frames / frame;
    if (samp_rate * tstime->estimated > 0) {
      tstime->speed = frames * framesize / (samp_rate * tstime->estimated);
    } else {
      tstime->speed = 0;
    }
    tstime->eta = tstime->estimated - tstime->so_far;
  } else {
    tstime->estimated = 0;
    tstime->speed = 0;
    tstime->eta = 0;
  }
}

/*********************************************************/
/* timestatus: display encoding process time information */
/*********************************************************/

static void  ts_time_decompose ( const unsigned long time_in_sec, const char padded_char )
{
    unsigned long hour = time_in_sec / 3600;
    unsigned int  min  = time_in_sec / 60 % 60;
    unsigned int  sec  = time_in_sec % 60;

    if ( hour == 0 )
        fprintf ( stderr,    "   %2u:%02u%c",       min, sec, padded_char );
    else if ( hour < 100 )
        fprintf ( stderr, "%2lu:%02u:%02u%c", hour, min, sec, padded_char );
    else
        fprintf ( stderr,         "%6lu h%c", hour,           padded_char );
}

void timestatus ( unsigned long samp_rate, 
                  unsigned long frameNum,
                  unsigned long totalframes,
                  int           framesize )
{
    ts_times  real_time;
    ts_times  process_time;
    int       percent;
    unsigned  dropped_frames = samp_rate <= 16000  || samp_rate == 32000  ?  2  :  1;  
                               /* ugly and nasty work around, only obscuring another bug? */
                               /* values of 1...3 are possible, why ??? */

    real_time   .so_far = ts_real_time    (frameNum);
    process_time.so_far = ts_process_time (frameNum);

    if ( frameNum == 0 ) {
        fputs ( "    Frame          |  CPU time/estim | REAL time/estim | play/CPU |    ETA  \n"
                "     0/       ( 0%)|    0:00/     :  |    0:00/     :  |    .    x|     :   \r", stderr );
        return;
    }  

    ts_calc_times ( &real_time   , samp_rate, frameNum, totalframes, framesize );
    ts_calc_times ( &process_time, samp_rate, frameNum, totalframes, framesize );

    if ( frameNum < totalframes - dropped_frames ) {
        percent = (int) (100.0 * frameNum / (totalframes - dropped_frames) + 0.5 );
    } else {
        percent = 100;
    }

    fprintf ( stderr, "\r%6ld/%-6ld", frameNum, totalframes - dropped_frames );
    fprintf ( stderr, percent < 100 ? " (%2d%%)|" : "(%3.3d%%)|", percent );
    ts_time_decompose ( (unsigned long)process_time.so_far   , '/' );
    ts_time_decompose ( (unsigned long)process_time.estimated, '|' );
    ts_time_decompose ( (unsigned long)real_time   .so_far   , '/' );
    ts_time_decompose ( (unsigned long)real_time   .estimated, '|' );
    fprintf ( stderr, process_time.speed <= 9999.9999 ? "%9.4fx|" : "%9.3ex|", process_time.speed );
    ts_time_decompose ( (unsigned long)real_time   .eta      , ' ' );
    fflush ( stderr );
}


void timestatus_finish(void)
{
  fprintf(stderr, "\n");
  fflush(stderr);
}


void timestatus_klemm(lame_global_flags *gfp)
{
  /* variables used for the status display */
  static double  last_time = 0.0;

  if (!silent) {
    if ( frameNum ==  0  ||  
	 frameNum == 10  ||
	 ( GetRealTime () - last_time >= update_interval  ||
	   GetRealTime ()             <  update_interval )) {
      timestatus ( gfp -> out_samplerate, 
		   frameNum, 
		   totalframes, 
		   gfp -> framesize );
#ifdef BRHIST
      if ( brhist )
	brhist_disp ( totalframes );
#endif
      last_time = GetRealTime ();  /* from now! disp_time seconds */
    }
  }
}




#if defined LIBSNDFILE || defined LAMESNDFILE

/* these functions are used in get_audio.c */

void decoder_progress ( lame_global_flags* gfp, mp3data_struct *mp3data )
{
#if 0
    static int  last_total = -1;
    static int  last_kbps  = -1;
    static int  last_frame = -1;

    if ( (frameNum & 255 ) == 1 ) {
        fprintf ( stderr, "\rFrame#%6lu/%-6lu %3u kbps        ", frameNum, totalframes, gfp -> brate );
        last_frame = -1;
    } 
    else if ( last_kbps != gfp -> brate ) {
        fprintf ( stderr, "\rFrame#%6lu/%-6lu %3u", frameNum, totalframes, gfp -> brate );
        last_frame = -1;
    } 
    else if ( last_total != totalframes ) {
        fprintf ( stderr, "\rFrame#%6lu/%-6lu", frameNum, totalframes );
        last_frame = -1;
    } 
    else {
        if ( last_frame > 0  &&  last_frame/10 == frameNum/10 )
            fprintf ( stderr, "\b%lu", frameNum % 10 );
        else if ( last_frame > 0  &&  last_frame/100 == frameNum/100 )
            fprintf ( stderr, "\b\b%02lu", frameNum % 100 );
        else
            fprintf ( stderr, "\rFrame#%6lu", frameNum ),
        last_frame = frameNum;
    }
	      
    last_total = totalframes;
    last_kbps  = gfp -> brate;
#endif

  fprintf(stderr, "\rFrame#%6i/%-6i %3i kbps        ",mp3data->framenum, 
     mp3data->totalframes, mp3data->bitrate );
  if (mp3data->mode==MPG_MD_JOINT_STEREO)
    fprintf ( stderr, ".... %s ...." ,
       MPG_MD_MS_LR==mp3data->mode_ext ? "M " : " S" );

}

void decoder_progress_finish ( lame_global_flags* gfp )
{
    fprintf ( stderr, "\n" );
}

#endif







