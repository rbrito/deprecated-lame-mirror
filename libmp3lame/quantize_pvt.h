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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef LAME_QUANTIZE_PVT_H
#define LAME_QUANTIZE_PVT_H

typedef struct calc_noise_result_t {
    FLOAT  over_noise;      /* sum of quantization noise > masking */
    FLOAT  tot_noise;       /* sum of all quantization noise */
    FLOAT  max_noise;       /* max quantization noise */
    int    over_count;      /* number of quantization noise > masking */
} calc_noise_result;

/* takehiro.c */

int     count_bits (lame_internal_flags * const gfc, const FLOAT * const xr,
		    gr_info * const cod_info);
int     noquant_count_bits (lame_internal_flags * const gfc,
			    gr_info * const cod_info);


void    best_huffman_divide (const lame_internal_flags * const gfc, 
                             gr_info * const cod_info);

void    best_scalefac_store (lame_internal_flags * const gfc,
			     const int gr, const int ch );

int     scale_bitcount (gr_info * const cod_info);
int     scale_bitcount_lsf (gr_info * const cod_info);

#define LARGE_BITS 100000

typedef union {
    float f;
    int i;
} fi_union;

/*********************************************************************
 * XRPOW_FTOI is a macro to convert floats to ints.  
 * if XRPOW_FTOI(x) = nearest_int(x), then QUANTFAC(x)=adj43asm[x]
 *                                         ROUNDFAC= -0.0946
 *
 * if XRPOW_FTOI(x) = floor(x), then QUANTFAC(x)=asj43[x]   
 *                                   ROUNDFAC=0.4054
 *********************************************************************/
#ifdef TAKEHIRO_IEEE754_HACK
# define MAGIC_FLOAT (65536*128)
# define MAGIC_INT 0x4b000000
# define ROUNDFAC -0.0946
#else
# define XRPOW_FTOI(src,dest) ((dest) = (int)(src))
# define ROUNDFAC 0.4054
#endif

#endif /* LAME_QUANTIZE_PVT_H */
