#include <string.h>
#include <assert.h>
#include "brhist.h"
#include "util.h"

#if (defined(BRHIST) && !defined(NOTERMCAP))
#include <termcap.h>
#endif

#ifndef BRHIST_BARMAX
# define  BRHIST_BARMAX 50
#endif

unsigned long  brhist_count   [15];
unsigned long  brhist_count_max;
unsigned       brhist_vbrmin;
unsigned       brhist_vbrmax;
char           brhist_kbps    [15] [4];
char           brhist_bar     [BRHIST_BARMAX + 1];
char           brhist_up      [10] = "\033[A";
char           brhist_clreoln [10] = "";

char           stderr_buff   [576];

#ifdef _WIN32  
COORD Pos;
HANDLE CH;
CONSOLE_SCREEN_BUFFER_INFO CSBI;
#endif

void brhist_init(lame_global_flags *gfp,int br_min, int br_max)
{
  int i;
#ifndef NOTERMCAP
  char term_buff[1024];
  char *termname;
#endif
  char *tp;
  char tc[10];


  for(i = 0; i < 15; i++)
    {
      assert ( (unsigned) bitrate_table[gfp->version][i] < 1000u );
      sprintf(brhist_kbps[i], "%3d", bitrate_table[gfp->version][i]);
      brhist_count[i] = 0;
    }

  brhist_vbrmin = br_min;
  brhist_vbrmax = br_max;

  brhist_count_max = 0;

#ifdef BRHIST
  memset ( brhist_bar, '*', sizeof (brhist_bar)-1 );

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
        strcpy ( brhist_up, tp );

    *(tp = tc) = '\0';
    tp = tgetstr ("ce", &tp);
    if (tp)
        strcpy ( brhist_clreoln, tp );

    setbuf ( stderr, stderr_buff );

#endif /* NOTERMCAP */

#ifdef _WIN32  
  CH= GetStdHandle(STD_ERROR_HANDLE);
#endif

#endif
}

void brhist_add_count(int i)
{
    if ( ++brhist_count [i] > brhist_count_max )
        brhist_count_max = brhist_count [i];
}

void brhist_disp(long totalframes)
{
    unsigned       i;
    unsigned       percent;
    unsigned long  full;
    int            barlen;
    char           brpercent [16];
    
#ifdef BRHIST
    full = brhist_count_max < BRHIST_BARMAX  ?  BRHIST_BARMAX  :  brhist_count_max;

    for ( i = brhist_vbrmin; i <= brhist_vbrmax; i++ ) {
        barlen  = ( brhist_count[i]*BRHIST_BARMAX + full-1  ) / full;		/* round up */
        percent = ( 100lu * brhist_count[i] + totalframes/2 ) / totalframes;	/* round nearest */
      
        if ( brhist_count [i] == 0 )
            sprintf ( brpercent,  " [   ]" );
        else if ( percent < 100 )
            sprintf ( brpercent," [%2u%%]", percent );
        else
            sprintf ( brpercent, "[%3u%%]", percent );
          
        if ( brhist_clreoln [0] ) /* ClearEndOfLine available */
            fprintf ( stderr, "\n%s%s%.*s%s", brhist_kbps [i], brpercent, 
                      barlen, brhist_bar, brhist_clreoln );
        else
            fprintf ( stderr, "\n%s%s%.*s%*s ", brhist_kbps [i], brpercent, 
                      barlen, brhist_bar, BRHIST_BARMAX - barlen, "" );
    }
# ifdef _WIN32  
  /* fflush is not needed */
  if(GetFileType(CH)!= FILE_TYPE_PIPE)
  {
    GetConsoleScreenBufferInfo(CH, &CSBI);
    Pos.Y= CSBI.dwCursorPosition.Y-(brhist_vbrmax- brhist_vbrmin)- 1;
    Pos.X= 0;
    SetConsoleCursorPosition(CH, Pos);
  }
# else
    fputc  ( '\r', stderr );
    for ( i = brhist_vbrmin; i <= brhist_vbrmax; i++ )
        fputs ( brhist_up, stderr );
    fflush ( stderr );
# endif  
#endif
}

void brhist_disp_total(lame_global_flags *gfp)
{
  int i;
  FLOAT ave;
  /*  lame_internal_flags *gfc=gfp->internal_flags;*/

#ifdef BRHIST
  for(i = brhist_vbrmin; i <= brhist_vbrmax; i++)
    fputc('\n', stderr);
#else
  fprintf(stderr, "\n----- bitrate statistics -----\n");
  fprintf(stderr, " [kbps]      frames\n");
  for(i = brhist_vbrmin; i <= brhist_vbrmax; i++)
    {
      fprintf(stderr, "   %3d  %8ld (%.1f%%)\n",
	      bitrate_table[gfp->version][i],
	      brhist_count[i],
	      (FLOAT)brhist_count[i] / gfp->totalframes * 100.0);
    }
#endif
  ave=0;
  for(i = brhist_vbrmin; i <= brhist_vbrmax; i++)
    ave += bitrate_table[gfp->version][i]* (FLOAT)brhist_count[i] ;
  fprintf ( stderr, "\naverage: %5.1f kbps\n", ave / gfp->totalframes );

  fflush(stderr);
}
