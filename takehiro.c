/*
 *	MP3 huffman table selecting and bit counting
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


#include "util.h"
#include "l3side.h"
#include "tables.h"
#include "quantize-pvt.h"
#include "globalflags.h"




struct
{
    unsigned region0_count;
    unsigned region1_count;
} subdv_table[ 23 ] =
{
{0, 0}, /* 0 bands */
{0, 0}, /* 1 bands */
{0, 0}, /* 2 bands */
{0, 0}, /* 3 bands */
{0, 0}, /* 4 bands */
{0, 1}, /* 5 bands */
{1, 1}, /* 6 bands */
{1, 1}, /* 7 bands */
{1, 2}, /* 8 bands */
{2, 2}, /* 9 bands */
{2, 3}, /* 10 bands */
{2, 3}, /* 11 bands */
{3, 4}, /* 12 bands */
{3, 4}, /* 13 bands */
{3, 4}, /* 14 bands */
{4, 5}, /* 15 bands */
{4, 5}, /* 16 bands */
{4, 6}, /* 17 bands */
{5, 6}, /* 18 bands */
{5, 6}, /* 19 bands */
{5, 7}, /* 20 bands */
{6, 7}, /* 21 bands */
{6, 7}, /* 22 bands */
};


/*************************************************************************/
/*	      ix_max							 */
/*************************************************************************/

 static int ix_max(int *ix, int *end)
{
    int max = 0;

    while (ix < end) {
	int x =	 *ix++;
	if (max < x) 
	    max = x;

	x = *ix++;
	if (max < x) 
	    max = x;
    }
    return max;
}


/*************************************************************************/
/*	      count_bit							 */
/*************************************************************************/

/*
 Function: Count the number of bits necessary to code the subregion. 
*/

static int cb_esc_buf[288];
static int *cb_esc_end;
static const int huf_tbl_noESC[15] = {
    1, 2, 5, 7, 7,10,10,13,13,13,13,13,13,13,13
};

 static int
count_bit_ESC(int *ix, int *end, int t1, int t2, int *s)
{
    /* ESC-table is used */
    int linbits1 = ht[t1].xlen;
    int linbits2 = ht[t2].xlen;
    int	sum1 = 0;
    int	sum2 = 0;

    while (ix < end) {
	int x = *ix++;
	int y = *ix++;

	if (x != 0) {
	    if (x > 14) {
		x = 15;
		sum1 += linbits1;
		sum2 += linbits2;
	    }
	    x *= 16;
	}

	if (y != 0) {
	    if (y > 14) {
		y = 15;
		sum1 += linbits1;
		sum2 += linbits2;
	    }
	    x += y;
	}

	sum1 += ht[16].hlen[x];
	sum2 += ht[24].hlen[x];
    }

    if (sum1 > sum2) {
	sum1 = sum2;
	t1 = t2;
    }

    *s += sum1;
    return t1;
}

 static int
count_bit_noESC(int *ix, int *end, unsigned int table) 
{
    /* No ESC-words */
    int	sum = 0;
    const unsigned char *hlen = ht[table].hlen;
    const unsigned int xlen = ht[table].xlen;
    unsigned int *p = cb_esc_buf;

    do {
	int x, y;
	x = *ix++;
	y = *ix++;
	x = x * xlen + y;
	*p++ = x;
	sum += hlen[x];
    } while (ix < end);

    cb_esc_end = p;
    return sum;
}



 static int
count_bit_noESC2(unsigned int table) 
{
    /* No ESC-words */
    int	sum = 0;
    int *p = cb_esc_buf;

    do {
	sum += ht[table].hlen[*p++];
    } while (p < cb_esc_end);

    return sum;
}



 static int
count_bit_short_ESC(int *ix, int *end, int t1, int t2, int *s)
{
    /* ESC-table is used */
    int linbits1 = ht[t1].xlen;
    int linbits2 = ht[t2].xlen;
    int	sum1 = 0;
    int	sum2 = 0;

    do {
	int i;
	for (i = 0; i < 3; i++) {
	    int y = *(ix + 3);
	    int x = *ix++;

	    if (x != 0) {
		if (x > 14) {
		    x = 15;
		    sum1 += linbits1;
		    sum2 += linbits2;
		}
		x *= 16;
	    }

	    if (y != 0) {
		if (y > 14) {
		    y = 15;
		    sum1 += linbits1;
		    sum2 += linbits2;
		}
		x += y;
	    }

	    sum1 += ht[16].hlen[x];
	    sum2 += ht[24].hlen[x];
	}
	ix += 3;
    } while (ix < end);

    if (sum1 > sum2)  {
	sum1 = sum2;
	t1 = t2;
    }

    *s += sum1;
    return t1;
}



 static int
count_bit_short_noESC(int *ix, int *end, unsigned int table) 
{
    /* No ESC-words */
    int	sum = 0;
    const unsigned char *hlen = ht[table].hlen;
    const unsigned int xlen = ht[table].xlen;
    unsigned int *p = cb_esc_buf;

    do {
	int i;
	for (i = 0; i < 3; i++) {
	    int y = *(ix + 3);
	    int x = *ix++;
	    x = x * xlen + y;
	    *p++ = x;
	    sum += hlen[x];
	}
	ix += 3;
    } while (ix < end);

    cb_esc_end = p;
    return sum;
}



/*************************************************************************/
/*	      new_choose table						 */
/*************************************************************************/

/*
  Choose the Huffman table that will encode ix[begin..end] with
  the fewest bits.

  Note: This code contains knowledge about the sizes and characteristics
  of the Huffman tables as defined in the IS (Table B.7), and will not work
  with any arbitrary tables.
*/

static int choose_table(int *ix, int *end, int *s)
{
    int max;
    int choice0, sum0;
    int choice1, sum1;

    max = ix_max(ix, end);

    if (max > IXMAX_VAL) {
        *s = LARGE_BITS;
        return -1;
    }

    if (max <= 15)  {
	if (max == 0) {
	    return 0;
	}
	/* try tables with no linbits */
	choice0 = huf_tbl_noESC[max - 1];
	sum0 = count_bit_noESC(ix, end, choice0);
	choice1 = choice0;

	switch (choice0) {
	case 7:
	case 10:
	    choice1++;
	    sum1 = count_bit_noESC2(choice1);
	    if (sum0 > sum1) {
		sum0 = sum1;
		choice0 = choice1;
	    }
	    /*fall*/
	case 2:
	case 5:
	    choice1++;
	    sum1 = count_bit_noESC2(choice1);
	    if (sum0 > sum1) {
		sum0 = sum1;
		choice0 = choice1;
	    }
	    break;

	case 13:
	    sum1 = count_bit_noESC2(14);
	    if (sum0 > sum1) {
		sum0 = sum1;
		choice0 = 16;
	    }

	    choice1 = 15;
	    sum1 = count_bit_noESC2(choice1);
	    if (sum0 > sum1) {
		sum0 = sum1;
		choice0 = choice1;
	    }
	    break;

	default:
	    break;
	}
	*s += sum0;
    } else {
	/* try tables with linbits */
	max -= 15;

	for (choice1 = 24; choice1 < 32; choice1++) {
	    if ((int)ht[choice1].linmax >= max) {
		break;
	    }
	}

	for (choice0 = choice1 - 8; choice0 < 24; choice0++) {
	    if ((int)ht[choice0].linmax >= max) {
		break;
	    }
	}

	choice0 = count_bit_ESC(ix, end, choice0, choice1, s);
    }

    return choice0;
}

static int choose_table_short(int *ix, int *end, int * s)
{
    int max;
    int choice0, sum0;
    int choice1, sum1;

    max = ix_max(ix, end);

    if (max > IXMAX_VAL) {
        *s = LARGE_BITS;
        return -1;
    }

    if (max <= 15)  {
	if (max == 0) {
	    return 0;
	}
	/* try tables with no linbits */
	choice0 = huf_tbl_noESC[max - 1];
	sum0 = count_bit_short_noESC(ix, end, choice0);
	choice1 = choice0;

	switch (choice0) {
	case 7:
	case 10:
	    choice1++;
	    sum1 = count_bit_noESC2(choice1);
	    if (sum0 > sum1) {
		sum0 = sum1;
		choice0 = choice1;
	    }
	    /*fall*/
	case 2:
	case 5:
	    choice1++;
	    sum1 = count_bit_noESC2(choice1);
	    if (sum0 > sum1) {
		sum0 = sum1;
		choice0 = choice1;
	    }
	    break;

	case 13:
	    sum1 = count_bit_noESC2(14);
	    if (sum0 > sum1) {
		sum0 = sum1;
		choice0 = 16;
	    }

	    choice1 = 15;
	    sum1 = count_bit_noESC2(choice1);
	    if (sum0 > sum1) {
		sum0 = sum1;
		choice0 = choice1;
	    }
	    break;

	default:
	    break;
	}
	*s += sum0;
    } else {
	/* try tables with linbits */
	max -= 15;
	for (choice1 = 24; choice1 < 32; choice1++) {
	    if ((int)ht[choice1].linmax >= max) {
		break;
	    }
	}

	for (choice0 = choice1 - 8; choice0 < 24; choice0++) {
	    if ((int)ht[choice0].linmax >= max) {
		break;
	}
	}
	choice0 = count_bit_short_ESC(ix, end, choice0, choice1, s);
    }

    return choice0;
}



static int count_bits_long(int ix[576], gr_info *gi)
{
    int i, a1, a2;
    int bits = 0;

    i=576;
    for (; i > 1; i -= 2) 
	if (ix[i - 1] | ix[i - 2])
	    break;

    /* Determines the number of bits to encode the quadruples. */
    gi->count1 = i;
    a1 = a2 = 0;
    for (; i > 3; i -= 4) {
	int p;
	if ((unsigned int)(ix[i-1] | ix[i-2] | ix[i-3] | ix[i-4]) > 1)
	    break;

	p = ((ix[i-4] * 2 + ix[i-3]) * 2 + ix[i-2]) * 2 + ix[i-1];
	a1 += ht[32].hlen[p];
	a2 += ht[33].hlen[p];
    }

    bits = a1;
    gi->count1table_select = 0;
    if (a1 > a2) {
	bits = a2;
	gi->count1table_select = 1;
    }

    gi->count1bits = bits;
    gi->big_values = i;
    if (i == 0)
	return bits;

    if (gi->block_type == NORM_TYPE) {
	int index;
	int scfb_anz = 0;

	while (scalefac_band.l[++scfb_anz] < i) 
	    ;
	index = subdv_table[scfb_anz].region0_count;
	while (scalefac_band.l[index + 1] > i)
	    index--;
	gi->region0_count = index;

	index = subdv_table[scfb_anz].region1_count;
	while (scalefac_band.l[index + gi->region0_count + 2] > i)
	    index--;
	gi->region1_count = index;

	a1 = scalefac_band.l[gi->region0_count + 1];
	a2 = scalefac_band.l[index + gi->region0_count + 2];
	gi->table_select[2] = choose_table(ix + a2, ix + i, &bits);

    } else {
	gi->region0_count = 7;
	/*gi->region1_count = SBPSY_l - 7 - 1;*/
	gi->region1_count = SBMAX_l -1 - 7 - 1;
	a1 = scalefac_band.l[7 + 1];
	a2 = i;
	if (a1 > a2) {
	    a1 = a2;
	}
    }

    /* Count the number of bits necessary to code the bigvalues region. */
    gi->table_select[0] = choose_table(ix, ix + a1, &bits);
    gi->table_select[1] = choose_table(ix + a1, ix + a2, &bits);
    return bits;
}




int count_bits(lame_global_flags *gfp,int *ix, FLOAT8 *xr, gr_info *cod_info)  
{
  int bits=0,i;
  /* since quantize_xrpow uses table lookup, we need to check this first: */
  FLOAT8 w = (IXMAX_VAL) / IPOW20(cod_info->global_gain);
  for ( i = 0; i < 576; i++ )  {
    if (xr[i] > w)
      return LARGE_BITS;
  }
  if (gfp->quantization) 
    quantize_xrpow(xr, ix, cod_info);
  else
    quantize_xrpow_ISO(xr, ix, cod_info);



  if (cod_info->block_type==SHORT_TYPE) {
    cod_info->table_select[0] = choose_table_short(ix, ix + 36, &bits);
    cod_info->table_select[1] = choose_table_short(ix + 36, ix + 576, &bits);
  }else{
    bits=count_bits_long(ix, cod_info);
  }
  return bits;

}

/***********************************************************************
  re-calculate the best scalefac_compress using scfsi
  the saved bits are kept in the bit reservoir.
 **********************************************************************/

static int r01_bits[7 + 15 + 1];
static int r01_div[7 + 15 + 1];
static int r0_tbl[7 + 15 + 1];
static int r1_tbl[7 + 15 + 1];
static gr_info cod_info;

static INLINE void
recalc_divide_init(int gr, int ch, int *ix)
{
    int r0, r1, bigv, r0t, r1t, bits;

    bigv = cod_info.big_values;

    for (r0 = 0; r0 <= 7 + 15; r0++) {
	r01_bits[r0] = LARGE_BITS;
    }

    for (r0 = 0; r0 < 16; r0++) {
	int a1 = scalefac_band.l[r0 + 1], r0bits;
	if (a1 >= bigv)
	    break;
	r0bits = cod_info.part2_length;
	r0t = choose_table(ix, ix + a1, &r0bits);

	for (r1 = 0; r1 < 8; r1++) {
	    int a2 = scalefac_band.l[r0 + r1 + 2];
	    if (a2 >= bigv)
		break;

	    bits = r0bits;
	    r1t = choose_table(ix + a1, ix + a2, &bits);
	    if (r01_bits[r0 + r1] > bits) {
		r01_bits[r0 + r1] = bits;
		r01_div[r0 + r1] = r0;
		r0_tbl[r0 + r1] = r0t;
		r1_tbl[r0 + r1] = r1t;
	    }
	}
    }
}

static INLINE void
recalc_divide_sub(int gr, int ch, gr_info *gi, int *ix)
{
    int bits, r2, a2, bigv, r2t;

    bigv = cod_info.big_values;

    for (r2 = 2; r2 < SBMAX_l + 1; r2++) {
	a2 = scalefac_band.l[r2];
	if (a2 >= bigv) 
	    break;

	bits = r01_bits[r2 - 2] + cod_info.count1bits;
	if (gi->part2_3_length <= bits)
	    break;

	r2t = choose_table(ix + a2, ix + bigv, &bits);
	if (gi->part2_3_length <= bits)
	    continue;

	memcpy(gi, &cod_info, sizeof(gr_info));
	gi->part2_3_length = bits;
	gi->region0_count = r01_div[r2 - 2];
	gi->region1_count = r2 - 2 - r01_div[r2 - 2];
	gi->table_select[0] = r0_tbl[r2 - 2];
	gi->table_select[1] = r1_tbl[r2 - 2];
	gi->table_select[2] = r2t;
    }
}

void best_huffman_divide(int gr, int ch, gr_info *gi, int *ix)
{
    int i, a1, a2;

    memcpy(&cod_info, gi, sizeof(gr_info));

    if (gi->block_type == NORM_TYPE) {
	recalc_divide_init(gr, ch, ix);
	recalc_divide_sub(gr, ch, gi, ix);
    }

    i = cod_info.big_values;
    if (i == 0 || i == 576 || (unsigned int)(ix[i-2] | ix[i-1]) > 1)
	return;

    memcpy(&cod_info, gi, sizeof(gr_info));

    /* Determines the number of bits to encode the quadruples. */
    i = (cod_info.count1 += 2);
    a1 = a2 = 0;
    for (; i > cod_info.big_values; i -= 4) {
	int p = ((ix[i-4] * 2 + ix[i-3]) * 2 + ix[i-2]) * 2 + ix[i-1];
	a1 += ht[32].hlen[p];
	a2 += ht[33].hlen[p];
    }
    cod_info.big_values = i;

    cod_info.count1table_select = 0;
    if (a1 > a2) {
	a1 = a2;
	cod_info.count1table_select = 1;
    }

    cod_info.count1bits = a1;
    cod_info.part2_3_length = a1 + cod_info.part2_length;

    if (cod_info.block_type == NORM_TYPE)
	recalc_divide_sub(gr, ch, gi, ix);
    else {
	/* Count the number of bits necessary to code the bigvalues region. */
	a1 = scalefac_band.l[7 + 1];
	if (a1 > i) {
	    a1 = i;
	}
	cod_info.table_select[0] =
	    choose_table(ix, ix + a1, &cod_info.part2_3_length);
	cod_info.table_select[1] =
	    choose_table(ix + a1, ix + i, &cod_info.part2_3_length);
	if (gi->part2_3_length > cod_info.part2_3_length)
	    memcpy(gi, &cod_info, sizeof(gr_info));
    }
}

static void
scfsi_calc(int ch,
	   III_side_info_t *l3_side,
	   III_scalefac_t scalefac[2][2])
{
    int i, s1, s2, c1, c2;
    int sfb;
    gr_info *gi = &l3_side->gr[1].ch[ch].tt;

    const int scfsi_band[5] = { 0, 6, 11, 16, 21 };

    const int slen1_n[16] = { 0, 1, 1, 1, 8, 2, 2, 2, 4, 4, 4, 8, 8, 8,16,16 };
    const int slen2_n[16] = { 0, 2, 4, 8, 1, 2, 4, 8, 2, 4, 8, 2, 4, 8, 4, 8 };

    const int slen1_tab[16] = { 0, 0, 0, 0, 3, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4 };
    const int slen2_tab[16] = { 0, 1, 2, 3, 0, 1, 2, 3, 1, 2, 3, 1, 2, 3, 2, 3 };

    for (i = 0; i < 4; i++) 
	l3_side->scfsi[ch][i] = 0;

    for (i = 0; i < (int)(sizeof(scfsi_band) / sizeof(int)) - 1; i++) {
	for (sfb = scfsi_band[i]; sfb < scfsi_band[i + 1]; sfb++) {
	    if (scalefac[0][ch].l[sfb] != scalefac[1][ch].l[sfb])
		break;
	}
	if (sfb == scfsi_band[i + 1]) {
	    for (sfb = scfsi_band[i]; sfb < scfsi_band[i + 1]; sfb++) {
		scalefac[1][ch].l[sfb] = -1;
	    }
	    l3_side->scfsi[ch][i] = 1;
	}
    }

    s1 = c1 = 0;
    for (sfb = 0; sfb < 11; sfb++) {
	if (scalefac[1][ch].l[sfb] < 0)
	    continue;
	c1++;
	if (s1 < scalefac[1][ch].l[sfb])
	    s1 = scalefac[1][ch].l[sfb];
    }

    s2 = c2 = 0;
    for (; sfb < SBPSY_l; sfb++) {
	if (scalefac[1][ch].l[sfb] < 0)
	    continue;
	c2++;
	if (s2 < scalefac[1][ch].l[sfb])
	    s2 = scalefac[1][ch].l[sfb];
    }
    for (i = 0; i < 16; i++) {
	if (s1 < slen1_n[i] && s2 < slen2_n[i]) {
	    int c = slen1_tab[i] * c1 + slen2_tab[i] * c2;
	    if ((int)gi->part2_length > c) {
		gi->part2_length = c;
		gi->scalefac_compress = i;
	    }
	}
    }
}

void best_scalefac_store(lame_global_flags *gfp,int gr, int ch,
			 int l3_enc[2][2][576],
			 III_side_info_t *l3_side,
			 III_scalefac_t scalefac[2][2])
{
    /* use scalefac_scale if we can */
    gr_info *gi = &l3_side->gr[gr].ch[ch].tt;

    /* remove scalefacs from bands with ix=0.  This idea comes
     * from the AAC ISO docs.  added mt 3/00 */
    int sfb,i,l,start,end;
    /* check if l3_enc=0 */
    for ( sfb = 0; sfb < gi->sfb_lmax; sfb++ ) {
      if (scalefac[gr][ch].l[sfb]>0) { 
	start = scalefac_band.l[ sfb ];
	end   = scalefac_band.l[ sfb+1 ];
	for ( l = start; l < end; l++ ) if (l3_enc[gr][ch][l]!=0) break;
	if (l==end) scalefac[gr][ch].l[sfb]=0;
      }
    }
    for ( i = 0; i < 3; i++ ) {
      for ( sfb = gi->sfb_smax; sfb < SBPSY_s; sfb++ ) {
	if (scalefac[gr][ch].s[sfb][i]>0) {
	  start = scalefac_band.s[ sfb ];
	  end   = scalefac_band.s[ sfb+1 ];
	  for ( l = start; l < end; l++ ) 
	    if (l3_enc[gr][ch][3*l+i]!=0) break;
	  if (l==end) scalefac[gr][ch].s[sfb][i]=0;
        }
      }
    }


    gi->part2_3_length -= gi->part2_length;
    if (!gi->scalefac_scale && !gi->preflag) {
	u_int sfb;
	int b, s = 0;
	for (sfb = 0; sfb < gi->sfb_lmax; sfb++) {
	    s |= scalefac[gr][ch].l[sfb];
	}

	for (sfb = gi->sfb_smax; sfb < SBPSY_s; sfb++) {
	    for (b = 0; b < 3; b++) {
		s |= scalefac[gr][ch].s[sfb][b];
	    }
	}

	if (!(s & 1) && s != 0) {
	    for (sfb = 0; sfb < gi->sfb_lmax; sfb++) {
		scalefac[gr][ch].l[sfb] /= 2;
	    }
	    for (sfb = gi->sfb_smax; sfb < SBPSY_s; sfb++) {
		for (b = 0; b < 3; b++) {
		    scalefac[gr][ch].s[sfb][b] /= 2;
		}
	    }

	    gi->scalefac_scale = 1;
	    gi->part2_length = 99999999;
	    if (gfp->mode_gr == 2) {
	        scale_bitcount(&scalefac[gr][ch], gi);
	    } else {
		scale_bitcount_lsf(&scalefac[gr][ch], gi);
	    }
	}
    }

    if (gfp->mode_gr==2 && gr == 1
	&& l3_side->gr[0].ch[ch].tt.block_type != SHORT_TYPE
	&& l3_side->gr[1].ch[ch].tt.block_type != SHORT_TYPE
	&& l3_side->gr[0].ch[ch].tt.scalefac_scale
	== l3_side->gr[1].ch[ch].tt.scalefac_scale
	&& l3_side->gr[0].ch[ch].tt.preflag
	== l3_side->gr[1].ch[ch].tt.preflag) {
      	scfsi_calc(ch, l3_side, scalefac);
    }
    gi->part2_3_length += gi->part2_length;
}
