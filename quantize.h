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



/*
 *  vbrquantize.c
 */

void VBR_quantize(lame_global_flags *gfp,
		    FLOAT8 pe[2][2], FLOAT8 ms_ener_ratio[2],
		    FLOAT8 xr[2][2][576], III_psy_ratio ratio[2][2],
		    int l3_enc[2][2][576],
		    III_scalefac_t scalefac[2][2]);


    /*  used by VBR_quantize() and VBR_iteration_loop() */ 

int VBR_noise_shaping(
                    lame_global_flags *gfp,
                    FLOAT8             xr[576], 
                    FLOAT8             xr34orig[576], 
                    III_psy_ratio     *ratio,
                    int                l3_enc[576], 
                    int                digital_silence, 
                    int                minbits, 
                    int                maxbits,
                    III_scalefac_t    *scalefac, 
                    III_psy_xmin      *l3_xmin,
                    int                gr,
                    int                ch);

#endif
