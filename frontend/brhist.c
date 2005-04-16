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

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef BRHIST

/* basic #define's */

#ifndef BRHIST_WIDTH
# define BRHIST_WIDTH    14
#endif
#ifndef BRHIST_RES
# define BRHIST_RES      14
#endif


/* #includes */

#ifdef STDC_HEADERS
# include <stdlib.h>
# include <string.h>
#else
# ifndef HAVE_MEMCPY
#  define memcpy(d, s, n) bcopy ((s), (d), (n))
#  define memmove(d, s, n) bcopy ((s), (d), (n))
# endif
#endif


#if defined(HAVE_NCURSES_TERMCAP_H)
# include <ncurses/termcap.h>
#elif defined(HAVE_TERMCAP_H)
# include <termcap.h>
#elif defined(HAVE_TERMCAP)
# include <curses.h>
# ifdef HAVE_TERM_H
#  include <term.h>
# endif
#endif

#include "brhist.h"
#include "main.h"

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

/* Structure holding all data related to the Console I/O 
 * may be this should be a more global frontend structure. So it
 * makes sense to print all files instead with
 * printf ( "blah\n") with printf ( "blah%s\n", Console_IO.str_clreoln );
 */

typedef struct {
    FILE*   Console_fp;			/* filepointer to stream reporting information */
    FILE*   Error_fp;                   /* filepointer to stream fatal error reporting information */
    FILE*   Report_fp;                  /* filepointer to stream reports (normally a text file or /dev/null) */
#if defined(_WIN32)  &&  !defined(__CYGWIN__) 
    HANDLE  Console_Handle;
#endif
    int     disp_width;
    int     disp_height;
    char    str_up         [10];
    char    str_clreoln    [10];
    char    str_emph       [10];
    char    str_norm       [10];
    char    Console_buff [1024];
} Console_IO_t;

static Console_IO_t Console_IO;

static struct {
    int     vbr_bitrate_min_index;
    int     vbr_bitrate_max_index;
    int     kbps[BRHIST_WIDTH];
    int     hist_printed_lines;
    char    bar_asterisk[512 + 1]; /* buffer filled up with a lot of '*' to print a bar     */
    char    bar_percent[512 + 1]; /* buffer filled up with a lot of '%' to print a bar     */
    char    bar_coded[512 + 1]; /* buffer filled up with a lot of ' ' to print a bar     */
    char    bar_space[512 + 1]; /* buffer filled up with a lot of ' ' to print a bar     */
} brhist;

static int calculate_index ( const int* const array, const int len, const int value )
{
    int i;
    
    for (i = 0; i < len; i++)
        if (array[i] == value)
	    return i;
    return -1;
}

int
brhist_init(lame_t gfp, const int bitrate_kbps_min, const int bitrate_kbps_max)
{
#ifdef HAVE_TERMCAP
    char         term_buff [2048];  /* see 1) */
    const char*  term_name;
    char*        tp;
    char         tc [10];
    int          val;
#endif

    /* setup basics of brhist I/O channels */
    Console_IO.disp_width  = 80;
    Console_IO.disp_height = 25;
    brhist.hist_printed_lines = 0;
    Console_IO.Console_fp  = stderr;
    Console_IO.Error_fp    = stderr;
    Console_IO.Report_fp   = stderr;

    setvbuf(Console_IO.Console_fp, Console_IO.Console_buff, _IOFBF,
	    sizeof(Console_IO.Console_buff));

#if defined(_WIN32)  &&  !defined(__CYGWIN__) 
    Console_IO.Console_Handle = GetStdHandle(STD_ERROR_HANDLE);
#endif

    strcpy(Console_IO.str_up, "\033[A");
    
    /* some internal checks */
    if (bitrate_kbps_min > bitrate_kbps_max) {
	fprintf(Console_IO.Error_fp,
		"lame internal error: VBR min %d kbps > VBR max %d kbps.\n",
		bitrate_kbps_min, bitrate_kbps_max);
	return -1;
    }
    if (bitrate_kbps_min < 8  ||  bitrate_kbps_max > 320) {
	fprintf(Console_IO.Error_fp,
		"lame internal error: VBR min %d kbps or VBR max %d kbps out of range.\n",
		bitrate_kbps_min, bitrate_kbps_max);
	return -1;
    }

    /* initialize histogramming data structure */
    memcpy(brhist.kbps, &bitrate_table[lame_get_version(gfp)][1],
	   sizeof(int)*BRHIST_WIDTH);
    brhist.vbr_bitrate_min_index = calculate_index(brhist.kbps, BRHIST_WIDTH, bitrate_kbps_min);
    brhist.vbr_bitrate_max_index = calculate_index(brhist.kbps, BRHIST_WIDTH, bitrate_kbps_max);

    if (brhist.vbr_bitrate_min_index >= BRHIST_WIDTH  ||  
	brhist.vbr_bitrate_max_index >= BRHIST_WIDTH) {
	fprintf(Console_IO.Error_fp,
		"lame internal error: VBR min %d kbps or VBR max %d kbps not allowed.\n",
		bitrate_kbps_min, bitrate_kbps_max);
	return -1;
    }

    memset(brhist.bar_asterisk, '*', sizeof(brhist.bar_asterisk) - 1);
    memset(brhist.bar_percent, '%', sizeof(brhist.bar_percent) - 1);
    memset(brhist.bar_space, '-', sizeof(brhist.bar_space) - 1);
    memset(brhist.bar_coded, '-', sizeof(brhist.bar_space) - 1);

#ifdef HAVE_TERMCAP
    /* try to catch additional information about special console sequences */
    if ((term_name = getenv("TERM")) == NULL) {
	if (silent < 10) {
	    fprintf(Console_IO.Error_fp,
		    "LAME: Can't get \"TERM\" environment string.\n");
	}
	return -1;
    }
    if (tgetent(term_buff, term_name) != 1) {
	if (silent < 10) {
	    fprintf(Console_IO.Error_fp,
		    "LAME: Can't find termcap entry for terminal \"%s\"\n",
		    term_name);
	}
	return -1;
    }
    
    val = tgetnum ("co");
    if (val >= 40 && val <= 512)
        Console_IO.disp_width   = val;
    val = tgetnum ("li");
    if (val >= 16 && val <= 256)
        Console_IO.disp_height  = val;
        
    *(tp = tc) = '\0';
    tp = tgetstr("up", &tp);
    if (tp != NULL)
        strcpy(Console_IO.str_up, tp);

    *(tp = tc) = '\0';
    tp = tgetstr ("ce", &tp);
    if (tp != NULL)
        strcpy(Console_IO.str_clreoln, tp);

    *(tp = tc) = '\0';
    tp = tgetstr ("md", &tp);
    if (tp != NULL)
        strcpy(Console_IO.str_emph, tp);

    *(tp = tc) = '\0';
    tp = tgetstr("me", &tp);
    if (tp != NULL)
        strcpy(Console_IO.str_norm, tp);

#endif /* HAVE_TERMCAP */

    return 0;
}

static int  digits ( unsigned number )
{
    int  ret = 1;
    
    if ( number >= 100000000 ) { ret += 8; number /= 100000000; }
    if ( number >=     10000 ) { ret += 4; number /=     10000; }
    if ( number >=       100 ) { ret += 2; number /=       100; }
    if ( number >=        10 ) { ret += 1;                      }
    
    return ret;
}


static void
brhist_disp_line(int i, int br_hist_TOT, int br_hist_LR, int full, int frames)
{
    char    brppt[BRHIST_WIDTH];  /* [%] and max. 10 characters for kbps */
    int     barlen_TOT;
    int     barlen_LR;
    int     ppt  = 0;
    int     res = digits(frames) + 3 + 4 + 1;

    if (full != 0) {
        /* some problems when br_hist_TOT \approx br_hist_LR: You can't see that there are still MS frames */
        barlen_TOT = (br_hist_TOT * (Console_IO.disp_width-BRHIST_RES) + full-1 ) / full;  /* round up */
        barlen_LR  = (br_hist_LR  * (Console_IO.disp_width-BRHIST_RES) + full-1 ) / full;  /* round up */
    } else {
        barlen_TOT = barlen_LR = 0;
    }

    if (frames > 0)
        ppt = (1000 * br_hist_TOT + frames/2) / frames; /* round nearest */

    sprintf(brppt, " [%*i]", digits(frames), br_hist_TOT);

    if (Console_IO.str_clreoln[0]) /* ClearEndOfLine available */
        fprintf(Console_IO.Console_fp, "\n%3d%s %.*s%.*s%s",
                brhist.kbps[i], brppt,
                barlen_LR, brhist.bar_percent,
                barlen_TOT - barlen_LR, brhist.bar_asterisk, Console_IO.str_clreoln);
    else
        fprintf(Console_IO.Console_fp, "\n%3d%s %.*s%.*s%*s",
                brhist.kbps[i], brppt,
                barlen_LR, brhist.bar_percent,
                barlen_TOT - barlen_LR, brhist.bar_asterisk,
                Console_IO.disp_width - res - barlen_TOT, "");

    brhist.hist_printed_lines++;
}


static void
progress_line(lame_t gfp, int full, int frames)
{
    char    rst[20] = "\0";
    int     barlen_TOT = 0, barlen_COD = 0, barlen_RST = 0;
    int     res = 1;
    int     df = 0, hour, min, sec;
    int     fsize = lame_get_framesize(gfp);
    int     srate = lame_get_out_samplerate(gfp);

    if (full < frames) {
        full = frames;
    }
    if (srate > 0) {
        df = full - frames;
        df *= fsize;
        df /= srate;
    }
    hour = df / 3600;
    df -= hour * 3600;
    min = df / 60;
    df -= min * 60;
    sec = df;
    if (full != 0) {
        if (hour > 0) {
            sprintf(rst, "%*d:%02d:%02d", digits(hour), hour, min, sec);
            res += digits(hour) + 1 + 5;
        }
        else {
            sprintf(rst, "%02d:%02d", min, sec);
            res += 5;
        }
        /* some problems when br_hist_TOT \approx br_hist_LR: You can't see that there are still MS frames */
        barlen_TOT = (full * (Console_IO.disp_width - res) + full - 1) / full; /* round up */
        barlen_COD = (frames * (Console_IO.disp_width - res) + full - 1) / full; /* round up */
        barlen_RST = barlen_TOT - barlen_COD;
        if (barlen_RST == 0) {
            sprintf(rst, "%.*s", res - 1, brhist.bar_coded);
        }
    }
    else {
        barlen_TOT = barlen_COD = barlen_RST = 0;
    }
    if (Console_IO.str_clreoln[0]) { /* ClearEndOfLine available */
        fprintf(Console_IO.Console_fp, "\n%.*s%s%.*s%s",
                barlen_COD, brhist.bar_coded,
                rst, barlen_RST, brhist.bar_space, Console_IO.str_clreoln);
    }
    else {
        fprintf(Console_IO.Console_fp, "\n%.*s%s%.*s%*s",
                barlen_COD, brhist.bar_coded,
                rst, barlen_RST, brhist.bar_space, Console_IO.disp_width - res - barlen_TOT, "");
    }
    brhist.hist_printed_lines++;
}


static int
stats_value(double x)
{
    if (x > 0.0) {
        fprintf(Console_IO.Console_fp, " %5.1f", x);
        return 6;
    }
    /*
       else {
       fprintf( Console_IO.Console_fp, "      " );
       return 6;
       }
     */
    return 0;
}

static int
stats_head(double x, const char *txt)
{
    if (x > 0.0) {
        fprintf(Console_IO.Console_fp, txt);
        return 6;
    }
    /*
       else {
       fprintf( Console_IO.Console_fp, "      " );
       return 6;
       }
     */
    return 0;
}


static void
stats_line(double *stat)
{
    int     n = 1;
    fprintf(Console_IO.Console_fp, "\n   kbps     ");
    n += 12;
    n += stats_head(stat[1], "  mono");
    n += stats_head(stat[2], "   IS ");
    n += stats_head(stat[3], "   LR ");
    n += stats_head(stat[4], "   MS ");
    fprintf(Console_IO.Console_fp, " %%    ");
    n += 6;
    n += stats_head(stat[5], " long ");
    n += stats_head(stat[6], "switch");
    n += stats_head(stat[7], " short");
    n += stats_head(stat[8], " mixed");
    n += fprintf(Console_IO.Console_fp, " %%");
    if (Console_IO.str_clreoln[0]) { /* ClearEndOfLine available */
        fprintf(Console_IO.Console_fp, "%s", Console_IO.str_clreoln);
    }
    else {
        fprintf(Console_IO.Console_fp, "%*s", Console_IO.disp_width - n, "");
    }
    brhist.hist_printed_lines++;

    n = 1;
    fprintf(Console_IO.Console_fp, "\n  %5.1f     ", stat[0]);
    n += 12;
    n += stats_value(stat[1]);
    n += stats_value(stat[2]);
    n += stats_value(stat[3]);
    n += stats_value(stat[4]);
    fprintf(Console_IO.Console_fp, "      ");
    n += 6;
    n += stats_value(stat[5]);
    n += stats_value(stat[6]);
    n += stats_value(stat[7]);
    n += stats_value(stat[8]);
    if (Console_IO.str_clreoln[0]) { /* ClearEndOfLine available */
        fprintf(Console_IO.Console_fp, "%s", Console_IO.str_clreoln);
    }
    else {
        fprintf(Console_IO.Console_fp, "%*s", Console_IO.disp_width - n, "");
    }
    brhist.hist_printed_lines++;
}

/* Yes, not very good */
#define LR  0
#define MS  2

void
brhist_disp(lame_t gfp)
{
    int i, lines = 0;
    int br_hist [BRHIST_WIDTH];       /* how often a frame size was used */
    int br_sm_hist [BRHIST_WIDTH] [4];/* how often a special frame size/stereo mode commbination was used */
    int st_mode[4];
    int bl_type[6];
    int frames;                       /* total number of encoded frames */
    int most_often;                   /* usage count of the most often used frame size, but not smaller than Console_IO.disp_width-BRHIST_RES (makes this sense?) and 1 */
    double sum = 0.;
    double stat[9] = { 0 };
    int st_frames = 0;
    
    brhist.hist_printed_lines = 0;  /* printed number of lines for the brhist functionality, used to skip back the right number of lines */
	
    lame_bitrate_stereo_mode_hist(gfp, br_sm_hist);
    lame_bitrate_hist(gfp, br_hist);
    lame_stereo_mode_hist(gfp, st_mode);
    lame_block_type_hist(gfp, bl_type);

    frames = most_often = 0;
    for (i = 0; i < BRHIST_WIDTH; i++) {
        frames += br_hist[i];
        sum += br_hist[i] * brhist.kbps[i];
        if (most_often < br_hist[i])
            most_often = br_hist[i];
        if (br_hist[i])
            ++lines;
    }

    for (i = 0; i < BRHIST_WIDTH; i++) {
        int     show = br_hist[i];
        show = show && (lines > 1);
        if (show || (i >= brhist.vbr_bitrate_min_index && i <= brhist.vbr_bitrate_max_index))
            brhist_disp_line(i, br_hist[i],
			     br_sm_hist[i][LR]+br_sm_hist[i][LR+1],
			     most_often, frames);
    }

    for (i = 0; i < 4; i++) {
        st_frames += st_mode[i];
    }
    if (frames > 0) {
        stat[0] = sum / frames;
        stat[1] = 100. * (frames - st_frames) / frames;
    }
    if (st_frames > 0) {
        stat[2] = 100. * (st_mode[LR+1] + st_mode[MS+1]) / st_frames;
        stat[3] = 100. * st_mode[LR] / st_frames;
        stat[4] = 100. * st_mode[MS] / st_frames;
    }
    if (bl_type[5] > 0) {
        stat[5] = 100. * bl_type[0] / bl_type[5];
        stat[6] = 100. * (bl_type[1] + bl_type[3]) / bl_type[5];
        stat[7] = 100. * bl_type[2] / bl_type[5];
        stat[8] = 100. * bl_type[4] / bl_type[5];
    }
    progress_line(gfp, lame_get_totalframes(gfp), frames);
    stats_line(stat);

    fputs ( "\r", Console_IO.Console_fp );
    fflush ( Console_IO.Console_fp );   /* fflush is ALSO needed for Windows! */
}

void brhist_jump_back( void )
{
#if defined(_WIN32)  &&  !defined(__CYGWIN__) 
    if ( GetFileType (Console_IO.Console_Handle) != FILE_TYPE_PIPE ) {
        COORD                       Pos;
        CONSOLE_SCREEN_BUFFER_INFO  CSBI;
	
        GetConsoleScreenBufferInfo ( Console_IO.Console_Handle, &CSBI );
        Pos.Y = CSBI.dwCursorPosition.Y - brhist.hist_printed_lines ;
        Pos.X = 0;
        SetConsoleCursorPosition ( Console_IO.Console_Handle, Pos );
    }
#else
    while ( brhist.hist_printed_lines-- > 0 )
        fputs ( Console_IO.str_up, Console_IO.Console_fp );
#endif
}


void
brhist_disp_total(lame_t gfp)
{
    int i;
    int br_hist [BRHIST_WIDTH];
    int st_mode [4];
    int bl_type [6];
    int st_frames = 0;
    int br_frames = 0;
    double sum = 0.;
    
    lame_stereo_mode_hist (gfp, st_mode );
    lame_bitrate_hist     (gfp, br_hist );
    lame_block_type_hist  (gfp, bl_type );
    
    for (i = 0; i < BRHIST_WIDTH; i++) {
        br_frames += br_hist[i];
	sum       += br_hist[i] * brhist.kbps[i];
    }

    for (i = 0; i < 4; i++) {
        st_frames += st_mode[i];
    }

    if (br_frames == 0)
	return;

    fprintf ( Console_IO.Console_fp, "\naverage: %5.1f kbps\n", sum / br_frames);

    if (st_frames > 0) {
	fprintf ( Console_IO.Console_fp, "   LR: %d (%#5.4g%%)", st_mode[LR], 100. * st_mode[LR] / st_frames );
	fprintf ( Console_IO.Console_fp, "   MS: %d (%#5.4g%%)", st_mode[MS], 100. * st_mode[MS] / st_frames );

	fprintf ( Console_IO.Console_fp, "\n LR+i: %d (%#5.4g%%)", st_mode[LR+1], 100. * st_mode[LR+1] / st_frames );
	fprintf ( Console_IO.Console_fp, " MS+i: %d (%#5.4g%%)", st_mode[MS+1], 100. * st_mode[MS+1] / st_frames );
    }
    fprintf ( Console_IO.Console_fp, "\n" );

    if (bl_type[5] > 0) {
	if (silent <= -5 && silent > -10) {
	    fprintf ( Console_IO.Console_fp, "block type");
	    fprintf ( Console_IO.Console_fp,  " long: %#4.3f", 100. * bl_type[NORM_TYPE] / bl_type[5] );
	    fprintf ( Console_IO.Console_fp, " start: %#4.3f", 100. * bl_type[START_TYPE] / bl_type[5] );
	    fprintf ( Console_IO.Console_fp, " short: %#4.3f", 100. * bl_type[SHORT_TYPE] / bl_type[5] );
	    fprintf ( Console_IO.Console_fp,  " stop: %#4.3f", 100. * bl_type[STOP_TYPE] / bl_type[5] );
	    fprintf ( Console_IO.Console_fp, " mixed: %#4.3f", 100. * bl_type[4] / bl_type[5] );
	    fprintf ( Console_IO.Console_fp, " (%%)\n" );
	}
	else if (silent <= -10) {
	    fprintf ( Console_IO.Console_fp, "block types   granules   percent\n" );
	    fprintf ( Console_IO.Console_fp, "      long: % 10d  % 8.3f%%\n", bl_type[NORM_TYPE], 100. * bl_type[NORM_TYPE] / bl_type[5] );
	    fprintf ( Console_IO.Console_fp, "     start: % 10d  % 8.3f%%\n", bl_type[START_TYPE], 100. * bl_type[START_TYPE] / bl_type[5] );
	    fprintf ( Console_IO.Console_fp, "     short: % 10d  % 8.3f%%\n", bl_type[SHORT_TYPE], 100. * bl_type[SHORT_TYPE] / bl_type[5] );
	    fprintf ( Console_IO.Console_fp, "      stop: % 10d  % 8.3f%%\n", bl_type[STOP_TYPE], 100. * bl_type[STOP_TYPE] / bl_type[5] );
	    fprintf ( Console_IO.Console_fp, "     mixed: % 10d  % 8.3f%%\n", bl_type[4], 100. * bl_type[4] / bl_type[5] );
	}
    }
    fflush  ( Console_IO.Console_fp );
}

/* 
 * 1)
 *
 * Taken from Termcap_Manual.html:
 *
 * With the Unix version of termcap, you must allocate space for the description yourself and pass
 * the address of the space as the argument buffer. There is no way you can tell how much space is
 * needed, so the convention is to allocate a buffer 2048 characters long and assume that is
 * enough.  (Formerly the convention was to allocate 1024 characters and assume that was enough.
 * But one day, for one kind of terminal, that was not enough.)
 */

#endif /* ifdef BRHIST */

