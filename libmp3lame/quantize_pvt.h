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

#define scalefactor(gi, sfb) \
    ((gi)->global_gain - (gi)->subblock_gain[(gi)->wi[(sfb)].window]*8) \
     - (((gi)->scalefac[(sfb)] + (((gi)->preflag > 0) ? pretab[(sfb)] : 0)) \
        << ((gi)->scalefac_scale + 1))

#define xr34 gfc->w.xrwork[0]
#define absxr gfc->w.xrwork[1]

/* takehiro.c */

int count_bits (lame_t gfc, gr_info * const gi);
int noquant_count_bits (lame_t gfc, gr_info * const gi);
int iteration_finish_one (lame_t gfc, int gr, int ch);
int scale_bitcount (gr_info * const gi);
int scale_bitcount_lsf (gr_info * const gi);

#if HAVE_NASM
int ix_max(const int *ix, const int *end);
void quantize_sfb_3DN(const FLOAT *, int, int, int *);
#endif

#define LARGE_BITS	(100000)
#define SCALEFAC_SCFSI_FLAG    (-1)
#define SCALEFAC_ANYTHING_GOES (-2)

#define ROUNDFAC ((FLOAT)0.4054)

#endif /* LAME_QUANTIZE_PVT_H */
