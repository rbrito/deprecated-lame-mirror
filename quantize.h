#ifndef QUANTIZE_H
#define QUANTIZE_H
#include "util.h"
/*#include "l3side.h"
*/
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

void ABR_iteration_loop( lame_global_flags *gfp,
                     FLOAT8 pe[2][2], FLOAT8 ms_ratio[2], 
		     FLOAT8 xr_org[2][2][576], III_psy_ratio ratio[2][2],
		     int l3_enc[2][2][576], 
		     III_scalefac_t scalefac[2][2]);

void VBR_quantize(lame_global_flags *gfp,
		    FLOAT8 pe[2][2], FLOAT8 ms_ener_ratio[2],
		    FLOAT8 xr[2][2][576], III_psy_ratio ratio[2][2],
		    int l3_enc[2][2][576],
		    III_scalefac_t scalefac[2][2]);

#endif
