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

#include "machine.h"

#define HTN	34

struct huffcodetab {
    const int    xlen; 	        /* max. x-index+			*/ 
    const int    linmax;	/* max number to be stored in linbits	*/
    const short*   table;	        /* pointer to array[xlen][ylen]		*/
    const char*  hlen;	        /* pointer to array[xlen][ylen]		*/
};

extern const struct huffcodetab ht [HTN];
    /* global memory block			*/
    /* array of all huffcodtable headers	*/
    /* 0..31 Huffman code table 0..31		*/
    /* 32,33 count1-tables			*/

extern const char t32l [];
extern const char t33l [];

extern const unsigned int   largetbl    [16*16];
extern const unsigned int   table23       [3*3];
extern const unsigned int   table56       [4*4];

extern const int scfsi_band[5];

extern FLOAT window[];
extern FLOAT window_s[];

#define IXMAX_VAL 8206  /* ix always <= 8191+15.    see count_bits() */

/* buggy Winamp decoder cannot handle values > 8191 */
/* #define IXMAX_VAL 8191 */

#define PRECALC_SIZE (IXMAX_VAL+2)

extern FLOAT pow43[PRECALC_SIZE];
#ifdef TAKEHIRO_IEEE754_HACK
extern FLOAT adj43asm[PRECALC_SIZE];
#else
extern FLOAT adj43[PRECALC_SIZE];
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

extern FLOAT pow20[Q_MAX+Q_MAX2];
extern FLOAT ipow20[Q_MAX];
extern FLOAT iipow20[Q_MAX2];


#endif /* LAME_TABLES_H */
