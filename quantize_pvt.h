/*
 *	quantize_pvt include file
 *
 *	Copyright (c) 1999 Takehiro TOMINAGA
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

#ifndef LAME_QUANTIZE_PVT_H
#define LAME_QUANTIZE_PVT_H

#include "l3side.h"
#define IXMAX_VAL 8206 /* ix always <= 8191+15.    see count_bits() */
#define PRECALC_SIZE (IXMAX_VAL+2)


extern const int     nr_of_sfb_block [6] [3] [4];
extern const char             pretab [SBMAX_l];
extern const char             slen1_tab [16];
extern const char             slen2_tab [16];

extern const scalefac_struct sfBandIndex[9];

extern FLOAT8 pow43[PRECALC_SIZE];
extern FLOAT8 adj43[PRECALC_SIZE];
extern FLOAT8 adj43asm[PRECALC_SIZE];

#define Q_MAX 330

extern FLOAT8 pow20[Q_MAX];
extern FLOAT8 ipow20[Q_MAX];

typedef struct calc_noise_result_t {
	int over_count;		/* number of quantization noise > masking */
	int tot_count;		/* all */
	FLOAT8 over_noise;	/* sum of quantization noise > masking */
	FLOAT8 tot_noise;	/* sum of all quantization noise */
	FLOAT8 max_noise;	/* max quantization noise */
} calc_noise_result;

void compute_ath(lame_global_flags *gfp,FLOAT8 ATH_l[SBPSY_l],FLOAT8 ATH_s[SBPSY_l]);
void ms_convert(FLOAT8 xr[2][576],FLOAT8 xr_org[2][576]);
int on_pe(lame_global_flags *gfp,FLOAT8 pe[2][2],III_side_info_t *l3_side,
int targ_bits[2],int mean_bits, int gr);
void reduce_side(int targ_bits[2],FLOAT8 ms_ener_ratio,int mean_bits,int max_bits);


void iteration_init( lame_global_flags *gfp, III_side_info_t *l3_side);


int calc_xmin( lame_global_flags *gfp,FLOAT8 xr[576],
                III_psy_ratio *ratio,
	            gr_info *cod_info,
                III_psy_xmin *l3_xmin);

int calc_noise( lame_global_flags *gfp, FLOAT8 xr[576],
                 int ix[576],
                 gr_info *cod_info,
                 FLOAT8 xfsf[4][SBMAX_l], 
		 FLOAT8 distort[4][SBMAX_l],
                 III_psy_xmin *l3_xmin,
		 III_scalefac_t *,
                 calc_noise_result *);

void set_pinfo ( lame_global_flags *gfp,
                 gr_info           *cod_info,
                 III_psy_ratio     *ratio, 
                 III_scalefac_t    *scalefac,
                 FLOAT8             xr    [576],        
                 int                l3_enc[576],        
                 int                gr,
                 int                ch );

#ifdef ASM_QUANTIZE
void quantize_xrpow_ASM( FLOAT8 xr[576],
               int  ix[576],
               int );

void quantize_xrpow_ISO_ASM( FLOAT8 xr[576],
               int  ix[576],
               int);
#else
void quantize_xrpow( FLOAT8 xr[576],
               int  ix[576],
               gr_info *cod_info );

void quantize_xrpow_ISO( FLOAT8 xr[576],
               int  ix[576],
               gr_info *cod_info );
#endif



/* takehiro.c */

int count_bits (lame_internal_flags *gfc, int  *ix, FLOAT8 xr[576],
                gr_info *cod_info);


void best_huffman_divide(lame_internal_flags *gfc, int gr, int ch, gr_info *cod_info, int *ix);

void best_scalefac_store(lame_internal_flags *gfc, int gr, int ch,
			 int l3_enc[2][2][576],
			 III_side_info_t *l3_side,
			 III_scalefac_t scalefac[2][2]);

int scale_bitcount (III_scalefac_t *scalefac, gr_info *cod_info);

int scale_bitcount_lsf (III_scalefac_t *scalefac, gr_info *cod_info);

void huffman_init (lame_internal_flags *gfp );

#define LARGE_BITS 100000

#endif /* LAME_QUANTIZE_PVT_H */

