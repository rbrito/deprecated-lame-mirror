#ifndef BRHIST_H_INCLUDED
#define BRHIST_H_INCLUDED


#include "lame.h"
void brhist_init(lame_global_flags *gfp,int br_min, int br_max);
void brhist_add_count(int i);
void brhist_disp(long );
void brhist_disp_total(lame_global_flags *gfp);
#endif

