#include <string.h>
#include "brhist.h"
#include "util.h"

#ifdef OS_AMIGAOS
#else
#ifdef BRHIST
#include <termcap.h>
#endif
#endif


#define BRHIST_BARMAX 50
long brhist_count[15];
int brhist_vbrmin;
int brhist_vbrmax;
long brhist_max;
char brhist_bps[15][5];
char brhist_backcur[200];
char brhist_bar[BRHIST_BARMAX+10];
char brhist_spc[BRHIST_BARMAX+1];

char stderr_buff[BUFSIZ];


#ifdef OS_AMIGAOS
/* tgetstr */
char *
tgetstr(char id[2], char **area)
{
      char *result;
      result = NULL;
      if (strncmp(id, "up", 2) == 0) {
              result = "\033[A";
      }
      *area = result;
      return result;
}

int
tgetent(char *buff, char *name)
{
      return 1;
}
#endif /* OS_AMIGAOS */





void brhist_init(lame_global_flags *gfp,int br_min, int br_max)
{
  int i;
  char term_buff[1024];
  char *termname;
  char *tp;
  char tc[10];


  for(i = 0; i < 15; i++)
    {
      sprintf(brhist_bps[i], "%3d", bitrate_table[gfp->version][i]);
      brhist_count[i] = 0;
    }

  brhist_vbrmin = br_min;
  brhist_vbrmax = br_max;

  brhist_max = 0;

#ifdef BRHIST
  memset(&brhist_bar[0], '*', BRHIST_BARMAX);
  brhist_bar[BRHIST_BARMAX] = '\0';
  memset(&brhist_spc[0], ' ', BRHIST_BARMAX);
  brhist_spc[BRHIST_BARMAX] = '\0';
  brhist_backcur[0] = '\0';

  if ((termname = getenv("TERM")) == NULL)
    {
      fprintf(stderr, "can't get TERM environment string.\n");
      gfp->brhist_disp = 0;
      return;
    }

  if (tgetent(term_buff, termname) != 1)
    {
      fprintf(stderr, "can't find termcap entry: %s\n", termname);
      gfp->brhist_disp = 0;
      return;
    }

  tc[0] = '\0';
  tp = &tc[0];
  tp=tgetstr("up", &tp);
  brhist_backcur[0] = '\0';
  for(i = br_min-1; i <= br_max; i++)
    strcat(brhist_backcur, tp);
  setbuf(stderr, stderr_buff);
#endif
}

void brhist_add_count(int i)
{
  ++brhist_count[i];
  if (brhist_count[i] > brhist_max)
    brhist_max = brhist_count[i];
}

void brhist_disp(long totalframes)
{
  int i;
  long full;
  int barlen;
  char brpercent[10];
#ifdef BRHIST
  full = (brhist_max < BRHIST_BARMAX) ? BRHIST_BARMAX : brhist_max;
  fputc('\n', stderr);
  for(i = brhist_vbrmin; i <= brhist_vbrmax; i++)
    {
      barlen = (brhist_count[i]*BRHIST_BARMAX+full-1) / full;
      fputs(brhist_bps[i], stderr);
      sprintf(brpercent,"[%3i%%]",(int)(100*brhist_count[i]/totalframes));
      fputs(brpercent, stderr);

      fputs(&brhist_bar[BRHIST_BARMAX - barlen], stderr);
      fputs(&brhist_spc[barlen], stderr);
      fputc('\n', stderr);
    }
  fputs(brhist_backcur, stderr);
  fflush(stderr);
#endif
}

void brhist_disp_total(lame_global_flags *gfp)
{
  int i;
  FLOAT ave;
  lame_internal_flags *gfc=gfp->internal_flags;

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
	      (FLOAT)brhist_count[i] / gfc->totalframes * 100.0);
    }
#endif
  ave=0;
  for(i = brhist_vbrmin; i <= brhist_vbrmax; i++)
    ave += bitrate_table[gfp->version][i]*
      (FLOAT)brhist_count[i] / gfc->totalframes;
  fprintf(stderr, "\naverage: %2.0f kbs\n",ave);

  fflush(stderr);
}




