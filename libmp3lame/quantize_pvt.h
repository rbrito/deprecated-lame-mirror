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
#define IXMAX_VAL 8206  /* ix always <= 8191+15.    see count_bits() */

/* buggy Winamp decoder cannot handle values > 8191 */
/* #define IXMAX_VAL 8191 */

#define PRECALC_SIZE (IXMAX_VAL+2)


extern const int nr_of_sfb_block[6][3][4];
extern const int pretab[SBMAX_l];
extern const int slen1_tab[16];
extern const int slen2_tab[16];

extern const scalefac_struct sfBandIndex[9];

extern FLOAT8 pow43[PRECALC_SIZE];
#ifdef TAKEHIRO_IEEE754_HACK
extern FLOAT8 adj43asm[PRECALC_SIZE];
#else
extern FLOAT8 adj43[PRECALC_SIZE];
#endif

#define Q_MAX (256+1)
#define Q_MAX2 116 /* minimam possible number of
		      -cod_info->global_gain
		      + ((scalefac[] + (cod_info->preflag ? pretab[sfb] : 0))
		      << (cod_info->scalefac_scale + 1))
		      + cod_info->subblock_gain[cod_info->window[sfb]] * 8;

		      for long block, 0+((15+3)<<2) = 18*4 = 72
		      for short block, 0+(15<<2)+7*8 = 15*4+56 = 116
		   */

extern FLOAT8 pow20[Q_MAX+Q_MAX2];
extern FLOAT8 ipow20[Q_MAX];
extern FLOAT8 iipow20[Q_MAX2];

typedef struct calc_noise_result_t {
    FLOAT8  over_noise;      /* sum of quantization noise > masking */
    FLOAT8  tot_noise;       /* sum of all quantization noise */
    FLOAT8  max_noise;       /* max quantization noise */
    int     over_count;      /* number of quantization noise > masking */
} calc_noise_result;

int     on_pe (lame_global_flags *gfp, FLOAT8 pe[2][2], III_side_info_t * l3_side,
               int targ_bits[2], int mean_bits, int gr, int cbr);

void    reduce_side (int targ_bits[2], FLOAT8 ms_ener_ratio, int mean_bits,
                     int max_bits);


int     bin_search_StepSize (lame_internal_flags * const gfc, gr_info * const cod_info,
                             int desired_rate, const int ch,
                             const FLOAT8 xrpow[576]);

void    iteration_init (lame_global_flags *gfp);


int     calc_xmin (lame_global_flags *gfp,
                    const III_psy_ratio * const ratio,
                    const gr_info * const cod_info,
                    FLOAT8 * l3_xmin);

int     calc_noise (const lame_internal_flags * const gfc,
                    const gr_info * const cod_info,
                    const FLOAT8 * l3_xmin,
                    FLOAT8 * distort,
		    calc_noise_result * const res);

#if defined(HAVE_GTK)
void    set_frame_pinfo (lame_global_flags *gfp,
                         III_psy_ratio ratio[2][2]);
#endif




/* takehiro.c */

int     count_bits (lame_internal_flags * const gfc, const FLOAT8 * const xr,
		    gr_info * const cod_info);
int     noquant_count_bits (lame_internal_flags * const gfc,
			    gr_info * const cod_info);


void    best_huffman_divide (const lame_internal_flags * const gfc, 
                             gr_info * const cod_info);

void    best_scalefac_store (const lame_internal_flags * gfc, const int gr, const int ch,
                             III_side_info_t * const l3_side);

int     scale_bitcount (gr_info * const cod_info);
int     scale_bitcount_lsf (const lame_internal_flags *gfp,
                            gr_info * const cod_info);

void    huffman_init (lame_internal_flags * const gfc);

#define LARGE_BITS 100000

#endif /* LAME_QUANTIZE_PVT_H */

