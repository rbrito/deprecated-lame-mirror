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

#include <string.h>
#include <assert.h>
#include "brhist.h"
#include "util.h"



static char str_up[10]={"\033[A"};
static char str_clreoln[10]={'\0',};
static char stderr_buff[576];


#if (defined(BRHIST) && !defined(NOTERMCAP))
#include <termcap.h>
#endif


/* Should this moved to lame_internal_flags? pfk */


#if defined(_WIN32) && !defined(__CYGWIN__) 
  COORD Pos;
  HANDLE CH;
  CONSOLE_SCREEN_BUFFER_INFO CSBI;
#endif

void brhist_init(lame_global_flags *gfp,int br_min, int br_max)
{
  int    i;
  PBRHST brhist=&gfp->brhist;


#ifdef BRHIST
  #ifndef NOTERMCAP
    char term_buff[1024];
    char *termname;
    char *tp;
    char tc[10];
  #endif
#endif

  /* initialize histogramming data structure */
  memset(&gfp->brhist,0,sizeof(BRHST));

  for(i = 0; i < 15; i++)
    {
      assert ( (unsigned) bitrate_table[gfp->version][i] < 1000u );
      sprintf(brhist->kbps[i], "%3d", bitrate_table[gfp->version][i]);
      gfp->brhist.count[i] = 0;
    }

  brhist->vbrmin = 1;
  brhist->vbrmax = br_max;

  brhist->count_max = 0;

#ifdef BRHIST
  memset ( brhist->bar, '*', sizeof (brhist->bar)-1 );

#ifndef NOTERMCAP
  if ((termname = getenv("TERM")) == NULL)
    {
      ERRORF("can't get TERM environment string.\n");
      gfp->brhist_disp = 0;
      return;
    }

  if (tgetent(term_buff, termname) != 1)
    {
      ERRORF("can't find termcap entry: %s\n", termname);
      gfp->brhist_disp = 0;
      return;
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

#endif
}

void brhist_add_count( lame_global_flags *gfp, int i)
{
    if ( ++(gfp->brhist.count[i]) > gfp->brhist.count_max )
        gfp->brhist.count_max = gfp->brhist.count [i];
}

void brhist_disp ( lame_global_flags *gfp )
{
#ifdef BRHIST
    u_int   i;
    u_int   ppt=0;
    u_long  full;
    int     barlen;
    char    brppt [16];
    
    full = gfp->brhist.count_max < BRHIST_BARMAX  ?  BRHIST_BARMAX  :  gfp->brhist.count_max;

    for ( i = gfp->brhist.vbrmin; i <= gfp->brhist.vbrmax; i++ ) {
        barlen  = ( gfp->brhist.count[i]*BRHIST_BARMAX + full-1  ) / full;		/* round up */

        if ( gfp->totalframes )
          ppt = ( 1000lu * gfp->brhist.count[i] + gfp->totalframes/2 ) / gfp->totalframes;	/* round nearest */
      
        if ( gfp->brhist.count [i] == 0 )
            sprintf ( brppt,  " [   ]" );
        else if ( ppt < gfp->brhist.count[i]/10000 )
            sprintf ( brppt," [%%..]" );
        else if ( ppt < 10 )
            sprintf ( brppt," [%%.%1u]", ppt%10 );
        else if ( ppt < 995 )
            sprintf ( brppt, " [%2u%%]", (ppt+5)/10 );
        else
            sprintf ( brppt, "[%3u%%]", (ppt+5)/10 );
          
        if ( str_clreoln [0] ) /* ClearEndOfLine available */
            fprintf ( stderr, "\n%s%s%.*s%s", gfp->brhist.kbps [i], brppt, 
                      barlen, gfp->brhist.bar, str_clreoln );
        else
            fprintf ( stderr, "\n%s%s%.*s%*s ", gfp->brhist.kbps [i], brppt, 
                      barlen, gfp->brhist.bar, BRHIST_BARMAX - barlen, "" );
    }
#if defined(_WIN32) && !defined(__CYGWIN__) 
  /* fflush is not needed */
  if(GetFileType(CH)!= FILE_TYPE_PIPE)
  {
    GetConsoleScreenBufferInfo(CH, &CSBI);
    Pos.Y= CSBI.dwCursorPosition.Y-(gfp->brhist.vbrmax- gfp->brhist.vbrmin)- 1;
    Pos.X= 0;
    SetConsoleCursorPosition(CH, Pos);
  }
# else
    fputc  ( '\r', stderr );
    for ( i = gfp->brhist.vbrmin; i <= gfp->brhist.vbrmax; i++ )
        fputs ( str_up, stderr );
    fflush ( stderr );
# endif  
#endif
}

void brhist_disp_total(lame_global_flags *gfp)
{
  unsigned int i;
  FLOAT ave;
  /*  lame_internal_flags *gfc=gfp->internal_flags;*/

#ifdef BRHIST
  for(i = gfp->brhist.vbrmin; i <= gfp->brhist.vbrmax; i++)
    fputc('\n', stderr);
#else
  fprintf(stderr, "\n----- bitrate statistics -----\n");
  fprintf(stderr, " [kbps]      frames\n");
  for(i = gfp->brhist.vbrmin; i <= gfp->brhist.vbrmax; i++)
    {
      fprintf(stderr, "   %3d  %8ld (%.1f%%)\n",
	      bitrate_table[gfp->version][i],
	      gfp->brhist.count[i],
	      (FLOAT)gfp->brhist.count[i] / gfp->totalframes * 100.0);
    }
#endif
  ave=0;
  for(i = gfp->brhist.vbrmin; i <= gfp->brhist.vbrmax; i++)
    ave += bitrate_table[gfp->version][i]* (FLOAT)gfp->brhist.count[i] ;
  fprintf ( stderr, "\naverage: %5.1f kbps\n", ave / gfp->totalframes );

  fflush(stderr);
}
