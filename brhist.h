#ifdef BRHIST
#ifndef BRHIST_H_INCLUDED
#define BRHIST_H_INCLUDED

extern int disp_brhist;


void brhist_init(int br_min, int br_max);
void brhist_add_count(void);
void brhist_disp(void);
void brhist_disp_total(void);
extern long brhist_temp[15];
#endif

#endif
