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
static char stderr_buff[576];

#ifndef NOTERMCAP
#   include <termcap.h>
#endif


#ifndef BRHIST_BARMAX
#   define  BRHIST_BARMAX 50
#endif
#ifndef BRHIST_WIDHTMAX
#   define  BRHIST_WIDTH 16
#endif

static struct
{
    long count[BRHIST_WIDTH];
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

int brhist_init(int br_min, int br_max)
{
    int    i;

#ifndef NOTERMCAP
    char term_buff[1024];
    char *termname;
    char *tp;
    char tc[10];
#endif

    if (br_min >= br_max) {
	fprintf(stderr, "internal error... VBR max %d <= VBR min %d!?\n",
		br_min, br_max);
	return -1;
    }

    /* initialize histogramming data structure */
    brhist.vbrmin = br_min;
    brhist.vbrmax = br_max;
    brhist.count_max = 0;

    for (i = 0; i < BRHIST_WIDTH; i++) {
	int kbps;
	kbps = (br_max - br_min) * i / BRHIST_WIDTH + br_min;
	assert (kbps < 1000u );

	sprintf(brhist.kbps[i], "%3d", kbps);
	brhist.count[i] = 0;
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


void brhist_update(int column)
{
    brhist.count[column]++;
}

void brhist_disp(int frames)
{
    int   i;
    int   ppt=0;
    unsigned long  full;
    int     barlen;
    char    brppt [16];

    full =
	brhist.count_max < BRHIST_BARMAX  ?  BRHIST_BARMAX : brhist.count_max;

    for (i = 0; i < BRHIST_WIDTH; i++) {
        barlen  = (brhist.count[i]*BRHIST_BARMAX + full - 1) / full; /* round up */
        if (frames)
	    ppt = (1000lu * brhist.count[i] + frames/2) / frames;	/* round nearest */

        if ( brhist.count [i] == 0 )
            sprintf ( brppt,  " [   ]" );
        else if ( ppt < brhist.count[i]/10000 )
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


void brhist_disp_total(int frames)
{
    int i;
    double ave;

    for(i = brhist.vbrmin; i <= brhist.vbrmax; i++)
	fputc('\n', stderr);

    ave=0;
    for(i = 0; i < BRHIST_WIDTH; i++) {
	ave += (brhist.vbrmax - brhist.vbrmin) * i * brhist.count[i] / BRHIST_WIDTH;
    }

    fprintf ( stderr, "\naverage: %5.1f kbps\n", ave / frames);

    fflush(stderr);
}
