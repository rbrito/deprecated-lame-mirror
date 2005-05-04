/*
 *	MPEG layer 3 tables include file
 *
 *	Copyright (c) 1999 Albert L Faber
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

#ifndef LAME_TABLES_H
#define LAME_TABLES_H

/* table of huffman coding */
extern const char htESC_xlen[];
extern const unsigned char quadcode[2][16*2];

extern FLOAT window[];
extern FLOAT window_s[];

extern const char nr_of_sfb_block[6*3][4];
extern const char pretab[SBMAX_l];
extern const char s1bits[16], s2bits[16];

extern const unsigned char mdctorder[SBLIMIT];

#define IXMAX_VAL 8206  /* ix always <= 8191+15.    see count_bits() */

/* buggy Winamp decoder cannot handle values > 8191 */
/* #define IXMAX_VAL 8191 */

#define PRECALC_SIZE (IXMAX_VAL+2)

#ifdef USE_IEEE754_HACK
extern FLOAT pow43[PRECALC_SIZE*3];
# define adj43asm ((int*)&pow43[PRECALC_SIZE*2])
#else
extern FLOAT pow43[PRECALC_SIZE*2];
#endif

#define adj43 (&pow43[PRECALC_SIZE])

#define Q_MAX (256+1)
#define Q_MAX2 116 /* minimum possible number of
		      -gi->global_gain
		      + ((scalefac[] + (gi->preflag ? pretab[sfb] : 0))
		      << (gi->scalefac_scale + 1))
		      + gi->subblock_gain[gi->window[sfb]] * 8;

		      for long block, 0+((15+3)<<2) = 18*4 = 72
		      for short block, 0+(15<<2)+7*8 = 15*4+56 = 116
		   */

extern FLOAT pow20[Q_MAX+Q_MAX2];
extern FLOAT ipow20[Q_MAX+Q_MAX2];


int psymodel_init(lame_t gfc);
void iteration_init(lame_t gfc);
void init_bitstream_w(lame_t gfc);

#endif /* LAME_TABLES_H */
