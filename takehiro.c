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

#include <assert.h>
#include "util.h"
#include "l3side.h"
#include "tables.h"
#include "quantize-pvt.h"

static unsigned int largetbl[16*16];
static unsigned int table23[3*3];
static unsigned int table56[4*4];

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

int ix_max(int *ix, int *end)
{
    int max1 = 0, max2 = 0;

    while (ix < end) {
	int x1 = *ix++;
	int x2 = *ix++;
	if (max1 < x1) 
	    max1 = x1;

	if (max2 < x2) 
	    max2 = x2;
    }
    if (max1 < max2) 
	max1 = max2;
    return max1;
}








int
count_bit_ESC(int *ix, int *end, int t1, int t2, int *s)
{
    /* ESC-table is used */
    int linbits = ht[t1].xlen * 65536 + ht[t2].xlen;
    unsigned int sum = 0, sum2;

    do {
	int x = *ix++;
	int y = *ix++;

	if (x != 0) {
	    if (x > 14) {
		x = 15;
		sum += linbits;
	    }
	    x *= 16;
	}

	if (y != 0) {
	    if (y > 14) {
		y = 15;
		sum += linbits;
	    }
	    x += y;
	}

	sum += largetbl[x];
    } while (ix < end);

    sum2 = sum & 0xffff;
    sum >>= 16;

    if (sum > sum2) {
	sum = sum2;
	t1 = t2;
    }

    *s += sum;
    return t1;
}


INLINE static int
count_bit_noESC(int *ix, int *end, int t1, int *s)
{
    /* No ESC-words */
    int	sum1 = 0;
    const unsigned int xlen = ht[t1].xlen;
    const unsigned char *hlen1 = ht[t1].hlen;

    do {
	int x = ix[0] * xlen + ix[1];
	ix += 2;
	sum1 += hlen1[x];
    } while (ix < end);

    *s += sum1;
    return t1;
}



INLINE static int
count_bit_noESC_from2(int *ix, int *end, int t1, int *s)
{
    /* No ESC-words */
    unsigned int sum = 0, sum2;
    const unsigned int xlen = ht[t1].xlen;
    unsigned int *hlen;
    if (t1 == 2)
	hlen = table23;
    else
	hlen = table56;

    do {
	int x = ix[0] * xlen + ix[1];
	ix += 2;
	sum += hlen[x];
    } while (ix < end);

    sum2 = sum & 0xffff;
    sum >>= 16;

    if (sum > sum2) {
	sum = sum2;
	t1++;
    }

    *s += sum;
    return t1;
}


#ifdef MMX_choose_table
static unsigned long long table789[16*6];
static unsigned long long tableABC[16*8];
static unsigned long long tableDEF[16*16];

extern int
choose_table_from3_MMX(int *ix, int *end, int t1, int *s, unsigned long long *hlen);

INLINE static int
count_bit_noESC_from3(int *ix, int *end, int t1, int *s)
{
    /* No ESC-words */
    unsigned long long *hlen;

    if (t1 == 7)
	hlen = table789;
    else if (t1 == 10)
	hlen = tableABC;
    else
	hlen = tableDEF;

    return choose_table_from3_MMX(ix, end, t1, s, hlen);
}

#else
INLINE static int
count_bit_noESC_from3(int *ix, int *end, int t1, int *s)
{
    /* No ESC-words */
    int	sum1 = 0;
    int	sum2 = 0;
    int	sum3 = 0;
    const unsigned int xlen = ht[t1].xlen;
    const unsigned char *hlen1 = ht[t1].hlen;
    const unsigned char *hlen2 = ht[t1+1].hlen;
    const unsigned char *hlen3 = ht[t1+2].hlen;
    int t;

    do {
	int x = ix[0] * xlen + ix[1];
	ix += 2;
	sum1 += hlen1[x];
	sum2 += hlen2[x];
	sum3 += hlen3[x];
    } while (ix < end);

    t = t1;
    if (sum1 > sum2) {
	sum1 = sum2;
	t++;
    }
    if (sum1 > sum3) {
	sum1 = sum3;
	t = t1+2;
    }
    *s += sum1;

    return t;
}
#endif


/*************************************************************************/
/*	      choose table						 */
/*************************************************************************/

/*
  Choose the Huffman table that will encode ix[begin..end] with
  the fewest bits.

  Note: This code contains knowledge about the sizes and characteristics
  of the Huffman tables as defined in the IS (Table B.7), and will not work
  with any arbitrary tables.
*/

int choose_table(int *ix, int *end, int *s)
{
    unsigned int max;
    int choice, choice2;
    static const int huf_tbl_noESC[16] = {
	1, 2, 5, 7, 7,10,10,13,13,13,13,13,13,13,13
    };

    max = ix_max(ix, end);

    switch (max) {
    case 0:
	return (int)max;

    case 1:
	return count_bit_noESC(ix, end, 1, s);

    case 2:
    case 3:
	return count_bit_noESC_from2(ix, end, huf_tbl_noESC[max - 1], s);

    case 4: case 5: case 6:
    case 7: case 8: case 9:
    case 10: case 11: case 12:
    case 13: case 14: case 15:
	choice = count_bit_noESC_from3(ix, end, huf_tbl_noESC[max - 1], s);
	if (choice == 14) {
	    choice = 16;
	}
	return choice;

    default:
	/* try tables with linbits */
	if (max > IXMAX_VAL) {
	    *s = LARGE_BITS;
	    return -1;
	}
	max -= 15;
	for (choice2 = 24; choice2 < 32; choice2++) {
	    if (ht[choice2].linmax >= max) {
		break;
	    }
	}

	for (choice = choice2 - 8; choice < 24; choice++) {
	    if (ht[choice].linmax >= max) {
		break;
	    }
	}
	return count_bit_ESC(ix, end, choice, choice2, s);
    }
}

/*************************************************************************/
/*	      count_bit							 */
/*************************************************************************/

/*
 Function: Count the number of bits necessary to code the subregion. 
*/


static const int huf_tbl_noESC[15] = {
    1, 2, 5, 7, 7,10,10,13,13,13,13,13,13,13,13
};

int bigv_short_ISO;


int count_bits_long(lame_internal_flags *gfc, int ix[576], gr_info *gi)
{
    int i, a1, a2;
    int bits = 0;

    i=576;
    /* Determine count1 region */
    for (; i > 1; i -= 2) 
	if (ix[i - 1] | ix[i - 2])
	    break;
    gi->count1 = i;


    /* Determines the number of bits to encode the quadruples. */
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
    if (i == 0 )
	return bits;

    if (gi->block_type == SHORT_TYPE) {
      if (bigv_short_ISO) {
	/* old ISO formula */
	bits=0;
	gi->count1bits=0;
	gi->count1=576;
	gi->big_values=576;
      }

      a1=3*gfc->scalefac_band.s[3];
      if (a1 > gi->big_values) a1 = gi->big_values;
      a2 = gi->big_values;

    }else if (gi->block_type == NORM_TYPE) {
	int index;
	int scfb_anz = 0;

	while (gfc->scalefac_band.l[++scfb_anz] < i) 
	    ;
	index = subdv_table[scfb_anz].region0_count;
	while (gfc->scalefac_band.l[index + 1] > i)
	    index--;
	gi->region0_count = index;

	index = subdv_table[scfb_anz].region1_count;
	while (gfc->scalefac_band.l[index + gi->region0_count + 2] > i)
	    index--;
	gi->region1_count = index;

	a1 = gfc->scalefac_band.l[gi->region0_count + 1];
	a2 = gfc->scalefac_band.l[index + gi->region0_count + 2];
	gi->table_select[2] = choose_table(ix + a2, ix + i, &bits);

    } else {
	gi->region0_count = 7;
	/*gi->region1_count = SBPSY_l - 7 - 1;*/
	gi->region1_count = SBMAX_l -1 - 7 - 1;
	a1 = gfc->scalefac_band.l[7 + 1];
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
  lame_internal_flags *gfc=gfp->internal_flags;

  int bits=0,i;
  /* since quantize_xrpow uses table lookup, we need to check this first: */
  FLOAT8 w = (IXMAX_VAL) / IPOW20(cod_info->global_gain);
  for ( i = 0; i < 576; i++ )  {
    if (xr[i] > w)
      return LARGE_BITS;
  }
  if (gfc->quantization) 
    quantize_xrpow(xr, ix, cod_info);
  else
    quantize_xrpow_ISO(xr, ix, cod_info);



  if (cod_info->block_type==SHORT_TYPE) {
    int ix_reorder[576];
    reorder(gfc->scalefac_band.s,ix_reorder,ix);
    /*
    int obits;
    bigv_short_ISO=1;
    obits=count_bits_long(gfc, ix_reorder, cod_info);
    */
    bigv_short_ISO=0;
    bits=count_bits_long(gfc, ix_reorder, cod_info);
    /*
    printf("new bits = %i  saving=%i   percentage=%f\n",bits,obits-bits,
            (obits-bits)/(.1+bits));
    */
  }else{
    bits=count_bits_long(gfc, ix, cod_info);
  }
  return bits;

}

/***********************************************************************
  re-calculate the best scalefac_compress using scfsi
  the saved bits are kept in the bit reservoir.
 **********************************************************************/


INLINE void
recalc_divide_init(lame_internal_flags *gfc, gr_info cod_info, int gr, int ch, int *ix,
int r01_bits[],int r01_div[],int r0_tbl[],int r1_tbl[])
{
    int r0, r1, bigv, r0t, r1t, bits;

    bigv = cod_info.big_values;

    for (r0 = 0; r0 <= 7 + 15; r0++) {
	r01_bits[r0] = LARGE_BITS;
    }

    for (r0 = 0; r0 < 16; r0++) {
	int a1 = gfc->scalefac_band.l[r0 + 1], r0bits;
	if (a1 >= bigv)
	    break;
	r0bits = cod_info.part2_length;
	r0t = choose_table(ix, ix + a1, &r0bits);

	for (r1 = 0; r1 < 8; r1++) {
	    int a2 = gfc->scalefac_band.l[r0 + r1 + 2];
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

INLINE void
recalc_divide_sub(lame_internal_flags *gfc,gr_info cod_info2, int gr, int ch, gr_info *gi, int *ix,
int r01_bits[],int r01_div[],int r0_tbl[],int r1_tbl[])
{
    int bits, r2, a2, bigv, r2t;

    bigv = cod_info2.big_values;

    for (r2 = 2; r2 < SBMAX_l + 1; r2++) {
	a2 = gfc->scalefac_band.l[r2];
	if (a2 >= bigv) 
	    break;

	bits = r01_bits[r2 - 2] + cod_info2.count1bits;
	if (gi->part2_3_length <= bits)
	    break;

	r2t = choose_table(ix + a2, ix + bigv, &bits);
	if (gi->part2_3_length <= bits)
	    continue;

	memcpy(gi, &cod_info2, sizeof(gr_info));
	gi->part2_3_length = bits;
	gi->region0_count = r01_div[r2 - 2];
	gi->region1_count = r2 - 2 - r01_div[r2 - 2];
	gi->table_select[0] = r0_tbl[r2 - 2];
	gi->table_select[1] = r1_tbl[r2 - 2];
	gi->table_select[2] = r2t;
    }
}

void best_huffman_divide(lame_internal_flags *gfc, int gr, int ch, gr_info *gi, int *ix)
{
    int i, a1, a2;
    gr_info cod_info2;

    int r01_bits[7 + 15 + 1];
    int r01_div[7 + 15 + 1];
    int r0_tbl[7 + 15 + 1];
    int r1_tbl[7 + 15 + 1];
    memcpy(&cod_info2, gi, sizeof(gr_info));

    if (gi->block_type == NORM_TYPE) {
	recalc_divide_init(gfc, cod_info2, gr, ch, ix,r01_bits,r01_div,r0_tbl,r1_tbl);
	recalc_divide_sub(gfc,cod_info2, gr, ch, gi, ix,r01_bits,r01_div,r0_tbl,r1_tbl);
    }

    i = cod_info2.big_values;
    if (i == 0 || (unsigned int)(ix[i-2] | ix[i-1]) > 1)
	return;

    i = gi->count1 + 2;
    if (i > 576)
	return;

    /* Determines the number of bits to encode the quadruples. */
    memcpy(&cod_info2, gi, sizeof(gr_info));
    cod_info2.count1 = i;
    a1 = a2 = 0;

    assert(i <= 576);
    
    for (; i > cod_info2.big_values; i -= 4) {
	int p = ((ix[i-4] * 2 + ix[i-3]) * 2 + ix[i-2]) * 2 + ix[i-1];
	a1 += ht[32].hlen[p];
	a2 += ht[33].hlen[p];
    }
    cod_info2.big_values = i;

    cod_info2.count1table_select = 0;
    if (a1 > a2) {
	a1 = a2;
	cod_info2.count1table_select = 1;
    }

    cod_info2.count1bits = a1;
    cod_info2.part2_3_length = a1 + cod_info2.part2_length;

    if (cod_info2.block_type == NORM_TYPE)
	recalc_divide_sub(gfc, cod_info2, gr, ch, gi, ix,r01_bits,r01_div,r0_tbl,r1_tbl);
    else {
	/* Count the number of bits necessary to code the bigvalues region. */
	a1 = gfc->scalefac_band.l[7 + 1];
	if (a1 > i) {
	    a1 = i;
	}
	cod_info2.table_select[0] =
	    choose_table(ix, ix + a1, (int *)&cod_info2.part2_3_length);
	cod_info2.table_select[1] =
	    choose_table(ix + a1, ix + i, (int *)&cod_info2.part2_3_length);
	if (gi->part2_3_length > cod_info2.part2_3_length)
	    memcpy(gi, &cod_info2, sizeof(gr_info));
    }
}

void
scfsi_calc(int ch,
	   III_side_info_t *l3_side,
	   III_scalefac_t scalefac[2][2])
{
    int i, s1, s2, c1, c2;
    int sfb;
    gr_info *gi = &l3_side->gr[1].ch[ch].tt;

    static const int scfsi_band[5] = { 0, 6, 11, 16, 21 };

    static const int slen1_n[16] = { 0, 1, 1, 1, 8, 2, 2, 2, 4, 4, 4, 8, 8, 8,16,16 };
    static const int slen2_n[16] = { 0, 2, 4, 8, 1, 2, 4, 8, 2, 4, 8, 2, 4, 8, 4, 8 };

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
  lame_internal_flags *gfc=gfp->internal_flags;

    /* use scalefac_scale if we can */
    gr_info *gi = &l3_side->gr[gr].ch[ch].tt;
    u_int sfb,i,l,start,end;

    /* remove scalefacs from bands with ix=0.  This idea comes
     * from the AAC ISO docs.  added mt 3/00 */
    /* check if l3_enc=0 */
    for ( sfb = 0; sfb < gi->sfb_lmax; sfb++ ) {
      if (scalefac[gr][ch].l[sfb]>0) { 
	start = gfc->scalefac_band.l[ sfb ];
	end   = gfc->scalefac_band.l[ sfb+1 ];
	for ( l = start; l < end; l++ ) if (l3_enc[gr][ch][l]!=0) break;
	if (l==end) scalefac[gr][ch].l[sfb]=0;
      }
    }
    for ( i = 0; i < 3; i++ ) {
      for ( sfb = gi->sfb_smax; sfb < SBPSY_s; sfb++ ) {
	if (scalefac[gr][ch].s[sfb][i]>0) {
	  start = gfc->scalefac_band.s[ sfb ];
	  end   = gfc->scalefac_band.s[ sfb+1 ];
	  for ( l = start; l < end; l++ ) 
	    if (l3_enc[gr][ch][3*l+i]!=0) break;
	  if (l==end) scalefac[gr][ch].s[sfb][i]=0;
        }
      }
    }


    gi->part2_3_length -= gi->part2_length;
    if (!gi->scalefac_scale && !gi->preflag) {
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
	    if (gfc->mode_gr == 2) {
	        scale_bitcount(&scalefac[gr][ch], gi);
	    } else {
		scale_bitcount_lsf(&scalefac[gr][ch], gi);
	    }
	}
    }


    for ( i = 0; i < 4; i++ )
      l3_side->scfsi[ch][i] = 0;

    if (gfc->mode_gr==2 && gr == 1
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

void huffman_init()
{
    int i;

    for (i = 0; i < 16*16; i++) {
	largetbl[i] = (((int)ht[16].hlen[i]) << 16) + ht[24].hlen[i];
    }

    for (i = 0; i < 3*3; i++) {
	table23[i] = (((int)ht[2].hlen[i]) << 16) + ht[3].hlen[i];
    }

    for (i = 0; i < 4*4; i++) {
	table56[i] = (((int)ht[5].hlen[i]) << 16) + ht[6].hlen[i];
    }
#ifdef MMX_choose_table
    for (i = 0; i < 6; i++) {
	int j;
	for (j = 0; j < 6; j++) {
	    table789[i*16+j] =
		(((long long)ht[7].hlen[i*6+j]) << 32) +
		(((long long)ht[8].hlen[i*6+j]) << 16) +
		(((long long)ht[9].hlen[i*6+j]));
	}
    }

    for (i = 0; i < 8; i++) {
	int j;
	for (j = 0; j < 8; j++) {
	    tableABC[i*16+j] =
		(((long long)ht[10].hlen[i*8+j]) << 32) +
		(((long long)ht[11].hlen[i*8+j]) << 16) +
		(((long long)ht[12].hlen[i*8+j]));
	}
    }

    for (i = 0; i < 16*16; i++) {
	tableDEF[i] =
	    (((long long)ht[13].hlen[i]) << 32) +
	    (((long long)ht[14].hlen[i]) << 16) +
	    (((long long)ht[15].hlen[i]));
    }
#endif
}
