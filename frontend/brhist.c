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
#include <assert.h>
#include "brhist.h"



static char str_up[10]={"\033[A"};
static char str_clreoln[10]={'\0',};
static char stderr_buff[1024];

#ifndef NOTERMCAP
#   include <termcap.h>
#endif


#ifndef BRHIST_BARMAX
#   define  BRHIST_BARMAX 50
#endif
#ifndef BRHIST_WIDHT
#   define  BRHIST_WIDTH 14
#endif

static struct
{
    long count_max;
    int  vbrmin;
    int  vbrmax;
    char kbps[BRHIST_WIDTH][4];
    char bar[BRHIST_BARMAX + 1];
} brhist;


#if defined(_WIN32) && !defined(__CYGWIN__) 
  #include <windows.h>
  COORD Pos;
  HANDLE CH;
  CONSOLE_SCREEN_BUFFER_INFO CSBI;
#endif

int brhist_init(lame_global_flags *gf, int br_min, int br_max)
{
    int    i;
    int br_hist[14];
    int br_kbps[14];

#ifndef NOTERMCAP
    char term_buff[1024];
    char *termname;
    char *tp;
    char tc[10];
#endif

    lame_bitrate_hist( gf, br_hist, br_kbps );
    
    if (br_min >= br_max) {
	fprintf(stderr, "internal error... VBR max %d <= VBR min %d!?\n",
		br_min, br_max);
	return -1;
    }

    /* initialize histogramming data structure */
    brhist.vbrmin = 0;
    brhist.vbrmax = 13;
    brhist.count_max = 0;

    for (i = 0; i < BRHIST_WIDTH; i++) {
	sprintf(brhist.kbps[i], "%3d", br_kbps[i]);
    }

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



void brhist_disp(lame_global_flags *gf)
{
    int   i;
    int   ppt=0;
    unsigned long  full = 0;
    int     barlen;
    char    brppt [16];
    int br_hist[14];
    int br_kbps[14];
    int frames = 0;
    
    lame_bitrate_hist(gf, br_hist, br_kbps);
    
    for (i = 0; i < BRHIST_WIDTH; i++) {
        frames += br_hist[i];
        if (full < br_hist[i]) full = br_hist[i];
    }

    full =
	full < BRHIST_BARMAX  ?  BRHIST_BARMAX : full;
    full =
	full < 1  ?  1 : full;

    for (i = 0; i < BRHIST_WIDTH; i++) {
        barlen  = (br_hist[i]*BRHIST_BARMAX + full - 1) / full; /* round up */
        if (frames)
	    ppt = (1000lu * br_hist[i] + frames/2) / frames;	/* round nearest */

        if ( br_hist [i] == 0 )
            sprintf ( brppt,  " [   ]" );
        else if ( ppt < br_hist[i]/10000 )
            sprintf ( brppt," [%%..]" );
        else if ( ppt < 10 )
            sprintf ( brppt," [%%.%1u]", ppt%10 );
        else if ( ppt < 995 )
            sprintf ( brppt, " [%2u%%]", (ppt+5)/10 );
        else
            sprintf ( brppt, "[%3u%%]", (ppt+5)/10 );
          
        if ( str_clreoln [0] ) /* ClearEndOfLine available */
            fprintf ( stderr, "\n%s%s%.*s%s", brhist.kbps [i], brppt, 
                      barlen, brhist.bar, str_clreoln );
        else
            fprintf ( stderr, "\n%s%s%.*s%*s ", brhist.kbps [i], brppt, 
                      barlen, brhist.bar, BRHIST_BARMAX - barlen, "" );
    }
#if defined(_WIN32) && !defined(__CYGWIN__) 
    /* fflush is not needed */
    if (GetFileType(CH)!= FILE_TYPE_PIPE){
	  GetConsoleScreenBufferInfo(CH, &CSBI);
	  Pos.Y= CSBI.dwCursorPosition.Y-(BRHIST_WIDTH)- 1;
	  Pos.X= 0;
	  SetConsoleCursorPosition(CH, Pos);
    }
#else
    fputc  ( '\r', stderr );
    for ( i = 0; i < BRHIST_WIDTH; i++ )
        fputs ( str_up, stderr );
    fflush ( stderr );
#endif
}


void brhist_disp_total(lame_global_flags *gf)
{
    int i;
    double ave;
    int br_hist[14];
    int br_kbps[14];
    int st_mode[4], st_frames = 0;
    int frames = 0;
    
    lame_stereo_mode_hist(gf, st_mode);
    lame_bitrate_hist(gf, br_hist, br_kbps);
    
    for (i = 0; i < BRHIST_WIDTH; i++) 
        frames += br_hist[i];

    for(i = brhist.vbrmin; i <= brhist.vbrmax; i++)
	fputc('\n', stderr);

    ave=0;
    for(i = 0; i < BRHIST_WIDTH; i++) {
	ave += br_kbps[i] * br_hist[i];
    }

    for (i = 0; i < 4; i++) {
        st_frames += st_mode[i];
    }

    fprintf ( stderr, "\naverage: %5.1f kbps", ave / frames);

    if (st_frames > 0) {
        double lr = st_mode[0] * 100. / st_frames;
        double ms = st_mode[2] * 100. / st_frames;
        fprintf ( stderr, "   LR: %5.1f%%   MS: %5.1f%%\n", lr, ms );
    }
    
    fflush(stderr);
    
}
