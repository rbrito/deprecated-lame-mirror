#ifndef L3PSY_DOT_H_
#define L3PSY_DOT_H_

/* l3psy.c */
#include "l3side.h"
void L3psycho_anal( short int *buffer[2], 
		    int gr , layer *info,
		    FLOAT8 sfreq, int check_ms, 
		    FLOAT8 *ms_ratio, 
		    FLOAT8 *ms_ratio_next, 
		    FLOAT8 *ms_ener_ratio, 
		    III_psy_ratio *,
		    III_psy_ratio *,
		    FLOAT8 pe[2], FLOAT8 pe_MS[2], 
                    int blocktype_d[2]); 
#endif
