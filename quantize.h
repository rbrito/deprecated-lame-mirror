#ifndef LOOP_DOT_H
#define LOOP_DOT_H
#include "util.h"
#include "l3side.h"

/**********************************************************************
 *   date   programmers                comment                        *
 * 25. 6.92  Toshiyuki Ishino          Ver 1.0                        *
 * 29.10.92  Masahiro Iwadare          Ver 2.0                        *
 * 17. 4.93  Masahiro Iwadare          Updated for IS Modification    *
 *                                                                    *
 *********************************************************************/

extern int cont_flag;


extern int pretab[];

void iteration_loop( lame_global_flags *gfp,
                     FLOAT8 pe[2][2], FLOAT8 ms_ratio[2], 
		     FLOAT8 xr_org[2][2][576], III_psy_ratio ratio[2][2],
		     int l3_enc[2][2][576], 
		     III_scalefac_t scalefac[2][2]);

void VBR_iteration_loop( lame_global_flags *gfp,
                     FLOAT8 pe[2][2], FLOAT8 ms_ratio[2], 
		     FLOAT8 xr_org[2][2][576], III_psy_ratio ratio[2][2],
		     int l3_enc[2][2][576], 
		     III_scalefac_t scalefac[2][2]);

void VBR_quantize(lame_global_flags *gfp,
		    FLOAT8 pe[2][2], FLOAT8 ms_ener_ratio[2],
		    FLOAT8 xr[2][2][576], III_psy_ratio ratio[2][2],
		    int l3_enc[2][2][576],
		    III_scalefac_t scalefac[2][2]);



#define maximum(A,B) ( (A) > (B) ? (A) : (B) )
#define minimum(A,B) ( (A) < (B) ? (A) : (B) )
#define signum( A ) ( (A) > 0 ? 1 : -1 )


extern int bit_buffer[50000];

#endif
