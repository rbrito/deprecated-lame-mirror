/*
 *	Bitrate histogram source file
 *
 *	Copyright (c) 2000 Mark Taylor
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

#include <stdlib.h>
#include <string.h>
#include "brhist.h"

/* ///
 * Sorry for not using KLEMM_03, but the code was really unreadable with so much #ifdefs
 * and finally I was not sure not to unintentionally have modified the original code ...
 *
 * remove after reading (such remarks I mark with a "///")
 * changed:
 *   not all bitrates are display, only min <= x <= max and outside this range the used bitrates
 *   cursor stays at the end of the screen, so ^C works right and nice (fflush moved two lines up)
 *   WINDOWS not tested, problems may be at the line marked with "$$$" if counting the lines is wrong for Windows
 *   No cache array for bit rate string representations, printed immediately with %3u from the int array
 *   Renaming of br_min to br_kbps_min (I trapped into the pitfall that br_min in 3.87 was the index and now without renaming it was the data rate in kbps)
 *   A "bug" is that brhist_disp_line has too many arguments
 *   assert.h removed, not used
 *   some minor changes, "%#5.4g" for percentages of LR/MS, a spell error in "BRHIST_WIDTH" (last two letters were wrong), and things like that
 *   some unsigned long => int stuff
 *   brhist_disp have a second arg to select to jump back or not
 *
 * Why not adding to the cvs comments:
 *   commenting must be done while transmission (costs money, 4.8 Pf/min)
 *   I have 3 minutes time, otherwise the modem cancels the connection and the comments are lost (cvs have problems with this case)
 *   comments can be added at the right place
 */

static char  str_up        [10] = "\033[A";
static char  str_clreoln   [10];  /// static data is always zeroed, only auto data needs this, but may be a " \b" is better
static char  stderr_buff [1024];

#ifndef NOTERMCAP
#   include <termcap.h>
#endif


#ifndef BRHIST_BARMAX
#   define BRHIST_BARMAX   50
#endif
#ifndef BRHIST_WIDTH
#   define BRHIST_WIDTH    14
#endif

static struct
{
    long count_max;
    int  vbrmin;
    int  vbrmax;
    int  kbps [BRHIST_WIDTH];
    char bar  [BRHIST_BARMAX + 1];
} brhist;


#if defined(_WIN32) && !defined(__CYGWIN__) 
  #include <windows.h>
  COORD Pos;
  HANDLE CH;
  CONSOLE_SCREEN_BUFFER_INFO CSBI;
#endif

static size_t  calculate_index ( const int* const array, const size_t len, const int value )
{
    size_t  i;
    
    for ( i = 0; i < len; i++ )
        if ( array [i] == value )
	    return i;
    return -1;
}

int  brhist_init ( const lame_global_flags* const gf, const int bitrate_kbps_min, const int bitrate_kbps_max )
{
    int    unused [BRHIST_WIDTH];

#ifndef NOTERMCAP
    char term_buff[1024];
    char *termname;
    char *tp;
    char tc[10];
#endif

    lame_bitrate_hist ( gf, unused, brhist.kbps );
    
    if ( bitrate_kbps_min > bitrate_kbps_max ) {
	fprintf (stderr, "lame internal error: VBR min %d kbps > VBR max %d kbps !?\n", bitrate_kbps_min, bitrate_kbps_max );
	return -1;
    }

    /* initialize histogramming data structure */
    brhist.vbrmin    = calculate_index ( brhist.kbps, BRHIST_WIDTH, bitrate_kbps_min );
    brhist.vbrmax    = calculate_index ( brhist.kbps, BRHIST_WIDTH, bitrate_kbps_max );
    brhist.count_max = 0;

    memset (brhist.bar, '*', sizeof (brhist.bar)-1 );

#ifndef NOTERMCAP
    if ((termname = getenv("TERM")) == NULL) {
	fprintf(stderr,"can't get TERM environment string.\n");
	return -1;
    }

    if (tgetent(term_buff, termname) != 1) {
	fprintf(stderr,"can't find termcap entry: %s\n", termname);
	return -1;
    }
    
    *(tp = tc) = '\0';
    tp = tgetstr ("up", &tp);
    if (tp)
        strcpy ( str_up, tp );

    *(tp = tc) = '\0';
    tp = tgetstr ("ce", &tp);
    if (tp)
        strcpy ( str_clreoln, tp );

    setbuf ( stderr, stderr_buff );

#endif /* NOTERMCAP */

#if defined(_WIN32) && !defined(__CYGWIN__) 
    CH= GetStdHandle(STD_ERROR_HANDLE);
#endif
    return 0; /* success */
}


static void  brhist_disp_line ( const lame_global_flags* const gf, int i, int br_hist, int full, int frames )
{
    char    brppt [16];
    int     barlen;
    int     ppt  = 0;
    
    barlen  = (br_hist*BRHIST_BARMAX + full-1 ) / full;      /* round up */
    if (frames > 0)
        ppt = (1000lu * br_hist + frames/2) / frames;        /* round nearest */

    if ( br_hist == 0 )
        sprintf ( brppt,  " [   ]" );
    else if ( ppt < br_hist/10000 )
        sprintf ( brppt," [%%..]" );
    else if ( ppt <  10 )
        sprintf ( brppt," [%%.%1u]", ppt );
    else if ( ppt < 995 )
        sprintf ( brppt, " [%2u%%]", (ppt+5)/10 );
    else
        sprintf ( brppt, "[%3u%%]", (ppt+5)/10 );
          
    if ( str_clreoln [0] ) /* ClearEndOfLine available */
        fprintf ( stderr, "\n%3d%s%.*s%s", brhist.kbps [i], brppt, 
                  barlen, brhist.bar, str_clreoln );
    else
        fprintf ( stderr, "\n%3d%s%.*s%*s ", brhist.kbps [i], brppt, 
                  barlen, brhist.bar, BRHIST_BARMAX - barlen, "" );
}

void  brhist_disp ( const lame_global_flags* const gf, const int jump_back )
{
    int   i;
    int   br_hist [BRHIST_WIDTH];
    int   br_kbps [BRHIST_WIDTH];
    int   frames;
    int   full;
    int   lines_on_screen = 0;
    
    lame_bitrate_hist(gf, br_hist, br_kbps);
    
    frames = full = 0;
    for (i = 0; i < BRHIST_WIDTH; i++) {
        frames += br_hist[i];
        if (full < br_hist[i]) full = br_hist[i];
    }

    full =
	full < BRHIST_BARMAX  ?  BRHIST_BARMAX : full;
    full =
	full < 1  ?  1 : full;

    i = 0;
    for (; i < brhist.vbrmin; i++) 
        if ( br_hist [i] ) {
            brhist_disp_line ( gf, i, br_hist [i], full, frames );
    	    lines_on_screen++;
	}
    for (; i <= brhist.vbrmax; i++) {
        brhist_disp_line ( gf, i, br_hist [i], full, frames );
	lines_on_screen++;
    }
    for (; i < BRHIST_WIDTH; i++)
        if ( br_hist [i] ) {
            brhist_disp_line ( gf, i, br_hist [i], full, frames );
	    lines_on_screen++;
	}
    
#if defined(_WIN32) && !defined(__CYGWIN__) 
    /* fflush is not needed */
    if (GetFileType(CH)!= FILE_TYPE_PIPE){
	  GetConsoleScreenBufferInfo(CH, &CSBI);
	  Pos.Y= CSBI.dwCursorPosition.Y - lines_on_screen - 1;  /* $$$ */
	  Pos.X= 0;
	  SetConsoleCursorPosition(CH, Pos);
    }
#else
    fputs ( "\r", stderr );
    fflush ( stderr );
    if ( jump_back )
        while ( lines_on_screen-- > 0 )
            fputs ( str_up, stderr );
#endif
}


void  brhist_disp_total ( const lame_global_flags* const gf )
{
    int i;
    double sum;
    int br_hist [BRHIST_WIDTH];
    int br_kbps [BRHIST_WIDTH];
    int st_mode [4];
    int st_frames = 0;
    int br_frames = 0;
    
    lame_stereo_mode_hist(gf, st_mode);    /// is this information only a summary, or is this available for every framesize ?
    lame_bitrate_hist(gf, br_hist, br_kbps);
    
    for (i = 0; i < BRHIST_WIDTH; i++) 
        br_frames += br_hist[i];

    sum=0;
    for (i = 0; i < BRHIST_WIDTH; i++) {
	sum += br_kbps[i] * br_hist[i];
    }

    for (i = 0; i < 4; i++) {
        st_frames += st_mode[i];
    }

    fprintf ( stderr, "\naverage: %5.1f kbps", sum / br_frames);

    if (st_frames > 0) {
        double lr = st_mode[0] * 100. / st_frames;
        double ms = st_mode[2] * 100. / st_frames;
        fprintf ( stderr, "   LR: %#5.4g%%   MS: %#5.4g%%\n", lr, ms );
    }
    
    fflush(stderr);
    
}

/* end of brhist.c */
