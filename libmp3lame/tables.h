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

struct huffcodetab {
    const int    xlen; 	        /* max. x-index+			*/ 
    const int    linmax;	/* max number to be stored in linbits	*/
    const short* table;	        /* pointer to array[xlen][ylen]		*/
    const char*  hlen;	        /* pointer to array[xlen][ylen]		*/
};

/* array of all huffcodtable headers, Huffman code table 0..31 */
extern const struct huffcodetab ht[];

extern const unsigned char quadcode[2][16*2];

extern const unsigned int   largetbl    [16*16];
extern const unsigned int   table13       [2*2];
extern const uint64_t table7B89[];
extern const uint64_t table7B89[];
extern const uint64_t table5967[];
extern const uint64_t tableDxEF[];

extern const int scfsi_band[5];

extern FLOAT window[];
extern FLOAT window_s[];

extern const int nr_of_sfb_block[6*3][4];
extern const int pretab[SBMAX_l];
extern const int s1bits[16], s2bits[16];

extern const scalefac_struct sfBandIndex[9];
extern const int mdctorder[SBLIMIT];

#define IXMAX_VAL 8206  /* ix always <= 8191+15.    see count_bits() */

/* buggy Winamp decoder cannot handle values > 8191 */
/* #define IXMAX_VAL 8191 */

#define PRECALC_SIZE (IXMAX_VAL+2)

#ifdef TAKEHIRO_IEEE754_HACK
extern FLOAT pow43[PRECALC_SIZE*3];
# define adj43asm (&pow43[PRECALC_SIZE*2])
#else
extern FLOAT pow43[PRECALC_SIZE*2];
#endif

#define adj43 (&pow43[PRECALC_SIZE])

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
extern FLOAT ipow20[Q_MAX+Q_MAX2];


int psymodel_init(lame_t gfc);
void iteration_init(lame_t gfc);
void init_bit_stream_w(lame_t gfc);

#endif /* LAME_TABLES_H */
