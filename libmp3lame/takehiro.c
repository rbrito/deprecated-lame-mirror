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

/* $Id$ */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include "util.h"
#include "tables.h"
#include "quantize_pvt.h"

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

#if HAVE_NASM
# define choosetable(a,b,c) gfc->choose_table(a,b,c)
#else
# define choosetable(a,b,c) choose_table_nonMMX(a,b,c)
#endif

/*********************************************************************
 * nonlinear quantization of xr 
 * More accurate formula than the ISO formula.  Takes into account
 * the fact that we are quantizing xr -> ix, but we want ix^4/3 to be 
 * as close as possible to x^4/3.  (taking the nearest int would mean
 * ix is as close as possible to xr, which is different.)
 * From Segher Boessenkool <segher@eastsite.nl>  11/1999
 * ASM optimization from 
 *    Mathew Hendry <scampi@dial.pipex.com> 11/1999
 *    Acy Stapp <AStapp@austin.rr.com> 11/1999
 *    Takehiro Tominaga <tominaga@isoternet.org> 11/1999
 * 9/00: ASM code removed in favor of IEEE754 hack.  If you need
 * the ASM code, check CVS circa Aug 2000.  
 *********************************************************************/


static int quantize_xrpow(const FLOAT *xp, gr_info *gi)
{
    /* quantize on xr^(3/4) instead of xr */
    int sfb = 0;
    fi_union *fi = (fi_union *)gi->l3_enc;

    do {
	const FLOAT *xe = xp + gi->width[sfb];
	FLOAT istep
	    = IPOW20(gi->global_gain
		     - ((gi->scalefac[sfb] + (gi->preflag ? pretab[sfb] : 0))
			<< (gi->scalefac_scale + 1))
		     - gi->subblock_gain[gi->window[sfb]] * 8);
	sfb++;
	do {
#ifdef TAKEHIRO_IEEE754_HACK
	    double x0 = istep * xp[0] + MAGIC_FLOAT;
	    double x1 = istep * xp[1] + MAGIC_FLOAT;
	    xp += 2;

	    fi[0].f = x0;
	    fi[1].f = x1;

	    if (fi[0].i >= MAGIC_INT + PRECALC_SIZE) return LARGE_BITS;
	    fi[0].f = x0 + (adj43asm - MAGIC_INT)[fi[0].i];
	    if (fi[1].i >= MAGIC_INT + PRECALC_SIZE) return LARGE_BITS;
	    fi[1].f = x1 + (adj43asm - MAGIC_INT)[fi[1].i];
	    fi[0].i -= MAGIC_INT;
	    fi[1].i -= MAGIC_INT;
	    fi += 2;
#else
	    FLOAT x1, x2;
	    int	rx1, rx2;
	    x1 = *xp++ * istep;
	    x2 = *xp++ * istep;
	    rx1 = (int)x1;
	    rx2 = (int)x2;
	    if (rx1 >= PRECALC_SIZE) return LARGE_BITS;
	    if (rx2 >= PRECALC_SIZE) return LARGE_BITS;
	    (fi++)->i = (int)(x1 + adj43[rx1]);
	    (fi++)->i = (int)(x2 + adj43[rx2]);
#endif
	} while (xp < xe);
    } while (fi < (fi_union *)&gi->l3_enc[576]);
    return 0;
}


static void quantize_xrpow_ISO(const FLOAT *xp, gr_info *gi)
{
    /* quantize on xr^(3/4) instead of xr */
    int sfb = 0;
    fi_union *fi = (fi_union *)gi->l3_enc;
    do {
	const FLOAT *xe = xp + gi->width[sfb];
	FLOAT istep
	    = IPOW20(gi->global_gain
		     - ((gi->scalefac[sfb] + (gi->preflag ? pretab[sfb] : 0))
			<< (gi->scalefac_scale + 1))
		     - gi->subblock_gain[gi->window[sfb]] * 8);
	sfb++;
	do {
#ifdef TAKEHIRO_IEEE754_HACK
	    fi[0].f = istep * xp[0] + (ROUNDFAC + MAGIC_FLOAT);
	    fi[1].f = istep * xp[1] + (ROUNDFAC + MAGIC_FLOAT);
	    xp += 2;
	    fi[0].i -= MAGIC_INT;
	    fi[1].i -= MAGIC_INT;
	    fi += 2;
#else
	    (fi++)->i = (int)(*xp++ * istep + ROUNDFAC);
	    (fi++)->i = (int)(*xp++ * istep + ROUNDFAC);
#endif
	} while (xp < xe);
    } while (fi < (fi_union *)&gi->l3_enc[576]);
}

/*************************************************************************/
/*	      ix_max							 */
/*************************************************************************/

static int 
ix_max(const int *ix, const int *end)
{
    int max1 = 0, max2 = 0;

    do {
	int x1 = *ix++;
	int x2 = *ix++;
	if (max1 < x1) 
	    max1 = x1;

	if (max2 < x2) 
	    max2 = x2;
    } while (ix < end);
    if (max1 < max2) 
	max1 = max2;
    return max1;
}








static int
count_bit_ESC( 
    const int *       ix, 
    const int * const end, 
          int         t1,
    const int         t2,
          int * const s )
{
    /* ESC-table is used */
    int linbits = ht[t1].xlen * 65536 + ht[t2].xlen;
    int sum = 0, sum2;

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


inline static int
count_bit_noESC(const int * ix, const int * const end, int * const s)
{
    /* No ESC-words */
    int	sum1 = 0;
    const char *hlen1 = ht[1].hlen;

    do {
	int x = ix[0] * 2 + ix[1];
	ix += 2;
	sum1 += hlen1[x];
    } while (ix < end);

    *s += sum1;
    return 1;
}



inline static int
count_bit_noESC_from2(
    const int *       ix, 
    const int * const end,
          int         t1,
          int * const s )
{
    /* No ESC-words */
    unsigned int sum = 0, sum2;
    const int xlen = ht[t1].xlen;
    const unsigned int *hlen;
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


inline static int
count_bit_noESC_from3(
    const int *       ix, 
    const int * const end,
          int         t1,
          int * const s )
{
    /* No ESC-words */
    int	sum1 = 0;
    int	sum2 = 0;
    int	sum3 = 0;
    const int xlen = ht[t1].xlen;
    const char *hlen1 = ht[t1].hlen;
    const char *hlen2 = ht[t1+1].hlen;
    const char *hlen3 = ht[t1+2].hlen;
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

#if !HAVE_NASM
static
#endif
int choose_table_nonMMX(
    const int *       ix, 
    const int * const end,
          int * const s )
{
    int max;
    int choice, choice2;
    static const int huf_tbl_noESC[] = {
	1, 2, 5, 7, 7,10,10,13,13,13,13,13,13,13,13
    };

    max = ix_max(ix, end);

    switch (max) {
    case 0:
	return max;

    case 1:
	return count_bit_noESC(ix, end, s);

    case 2:
    case 3:
	return count_bit_noESC_from2(ix, end, huf_tbl_noESC[max - 1], s);

    case 4: case 5: case 6:
    case 7: case 8: case 9:
    case 10: case 11: case 12:
    case 13: case 14: case 15:
	return count_bit_noESC_from3(ix, end, huf_tbl_noESC[max - 1], s);

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
int noquant_count_bits(
          lame_internal_flags * const gfc, 
          gr_info * const gi
	  )
{
    int i, a1, a2;
    int *const ix = gi->l3_enc;
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
	/* hack to check if all values <= 1 */
	if ((unsigned int)(ix[i-1] | ix[i-2] | ix[i-3] | ix[i-4]) > 1)
	    break;

	p = ((ix[i-4] * 2 + ix[i-3]) * 2 + ix[i-2]) * 2 + ix[i-1];
	a1 += t32l[p];
	a2 += t33l[p];
    }

    gi->count1table_select = 0;
    if (a1 > a2) {
	a1 = a2;
	gi->count1table_select = 1;
    }

    gi->part2_3_length = gi->count1bits = a1;
    gi->big_values = i;
    if (i == 0)
	return a1;

    if (gi->block_type == NORM_TYPE) {
	assert(i <= 576); /* bv_scf has 576 entries (0..575) */
        a1 = gi->region0_count = gfc->bv_scf[i-2];
	a2 = gi->region1_count = gfc->bv_scf[i-1];

	assert(a1+a2+2 < SBPSY_l);
        a2 = gfc->scalefac_band.l[a1 + a2 + 2];
	if (a2 < i)
	    gi->table_select[2] = choosetable(ix + a2, ix + i,
					      &gi->part2_3_length);
	else
	    a2 = i;
    } else {
	a1 = gi->region0_count;
	a2 = i;
    }
    a1 = gfc->scalefac_band.l[a1 + 1];

    /* have to allow for the case when bigvalues < region0 < region1 */
    /* (and region0, region1 are ignored) */
    assert( a1 > 0 );
    assert( a2 > 0 );

    if (a1 >= a2) {
	gi->table_select[0] = choosetable(ix, ix + a2,
					  &gi->part2_3_length);
    } else {
	gi->table_select[0] = choosetable(ix, ix + a1,
					  &gi->part2_3_length);
	gi->table_select[1] = choosetable(ix + a1, ix + a2,
					  &gi->part2_3_length);
    }
    if (gfc->use_best_huffman == 2)
	best_huffman_divide (gfc, gi);

    return gi->part2_3_length;
}

int count_bits(
          lame_internal_flags * const gfc, 
    const FLOAT  * const xrpow,
          gr_info * const gi
	  )
{
    int i;

    if (gfc->quantization) {
	i = quantize_xrpow(xrpow, gi);
	if (i)
	    return i;
    } else
	quantize_xrpow_ISO(xrpow, gi);

    if (gfc->substep_shaping & 2) {
	int sfb, j = 0;
	int *const ix = gi->l3_enc;
	// 0.634521682242439 = 0.5946*2**(.5*0.1875)
	for (sfb = 0; sfb < gi->sfbmax; sfb++) {
	    int width = gi->width[sfb];
	    int l;
	    const FLOAT roundfac = 0.634521682242439
		/ IPOW20(gi->global_gain
			 - ((gi->scalefac[sfb] + (gi->preflag ? pretab[sfb]:0))
			    << (gi->scalefac_scale + 1))
			 - gi->subblock_gain[gi->window[sfb]] * 8
			 + gi->scalefac_scale);
	    j += width;
	    if (!gfc->pseudohalf[sfb])
		continue;
	    for (l = -width; l < 0; l++)
		if (xrpow[j+l] < roundfac)
		    ix[j+l] = 0;
	}
    }
    return noquant_count_bits(gfc, gi);
}

/***********************************************************************
  re-calculate the best scalefac_compress using scfsi
  the saved bits are kept in the bit reservoir.
 **********************************************************************/


inline static void
recalc_divide_init(
    const lame_internal_flags * const gfc,
          gr_info         *cod_info,
          int     * const ix,
          int             r01_bits[],
          int             r01_div [],
          int             r0_tbl  [],
          int             r1_tbl  [] )
{
    int r0, r1, bigv, r0t, r1t, bits;

    bigv = cod_info->big_values;

    for (r0 = 0; r0 <= 7 + 15; r0++) {
	r01_bits[r0] = LARGE_BITS;
    }

    for (r0 = 0; r0 < 16; r0++) {
	int a1 = gfc->scalefac_band.l[r0 + 1], r0bits;
	if (a1 >= bigv)
	    break;
	r0bits = 0;
	r0t = choosetable(ix, ix + a1, &r0bits);

	for (r1 = 0; r1 < 8; r1++) {
	    int a2 = gfc->scalefac_band.l[r0 + r1 + 2];
	    if (a2 >= bigv)
		break;

	    bits = r0bits;
	    r1t = choosetable(ix + a1, ix + a2, &bits);
	    if (r01_bits[r0 + r1] > bits) {
		r01_bits[r0 + r1] = bits;
		r01_div[r0 + r1] = r0;
		r0_tbl[r0 + r1] = r0t;
		r1_tbl[r0 + r1] = r1t;
	    }
	}
    }
}

inline static int
recalc_divide_sub(
    const lame_internal_flags * const gfc,
          gr_info         *gi,
    const int     * const ix,
    const int             r01_bits[],
    const int             r01_div [],
    const int             r0_tbl  [],
    const int             r1_tbl  [] )
{
    int bits, r2, a2, bigv, r2t, flag = 0;

    bigv = gi->big_values;

    for (r2 = 2; r2 < SBMAX_l + 1; r2++) {
	a2 = gfc->scalefac_band.l[r2];
	if (a2 >= bigv) 
	    break;

	bits = r01_bits[r2 - 2] + gi->count1bits;
	if (gi->part2_3_length <= bits)
	    break;

	r2t = choosetable(ix + a2, ix + bigv, &bits);
	if (gi->part2_3_length <= bits)
	    continue;

	gi->part2_3_length = bits;
	gi->region0_count = r01_div[r2 - 2];
	gi->region1_count = r2 - 2 - r01_div[r2 - 2];
	gi->table_select[0] = r0_tbl[r2 - 2];
	gi->table_select[1] = r1_tbl[r2 - 2];
	gi->table_select[2] = r2t;
	flag = 1;
    }
    return flag;
}




void best_huffman_divide(
    const lame_internal_flags * const gfc,
    gr_info * const gi)
{
    int i, a1, a2;
    gr_info cod_info2;
    int * const ix = gi->l3_enc;

    int r01_bits[7 + 15 + 1];
    int r01_div[7 + 15 + 1];
    int r0_tbl[7 + 15 + 1];
    int r1_tbl[7 + 15 + 1];

    if (gi->big_values == 0)
	return;

    if (gi->block_type == NORM_TYPE) {
	recalc_divide_init(gfc, gi, ix, r01_bits,r01_div,r0_tbl,r1_tbl);
	recalc_divide_sub(gfc, gi, ix, r01_bits,r01_div,r0_tbl,r1_tbl);
    }

    i = gi->big_values;
    if ((unsigned int)(ix[i-2] | ix[i-1]) > 1)
	return;

    i = gi->count1 + 2;
    if (i > 576)
	return;

    cod_info2 = *gi;
    cod_info2.count1 = i;
    cod_info2.big_values -= 2;

    a1 = a2 = 0;
    for (; i > cod_info2.big_values; i -= 4) {
	int p = ((ix[i-4] * 2 + ix[i-3]) * 2 + ix[i-2]) * 2 + ix[i-1];
	a1 += t32l[p];
	a2 += t33l[p];
    }

    cod_info2.count1table_select = 0;
    if (a1 > a2) {
	a1 = a2;
	cod_info2.count1table_select = 1;
    }

    cod_info2.count1bits = a1;

    if (cod_info2.block_type == NORM_TYPE) {
	if (recalc_divide_sub(gfc, &cod_info2, ix, r01_bits,r01_div,r0_tbl,r1_tbl))
	    *gi = cod_info2;
    } else {
	/* Count the number of bits necessary to code the bigvalues region. */
	cod_info2.part2_3_length = a1;
	a1 = gfc->scalefac_band.l[gi->region0_count+1];
	if (a1 > i) {
	    a1 = i;
	}
	if (a1 > 0)
	    cod_info2.table_select[0] =
		choosetable(ix, ix + a1, &cod_info2.part2_3_length);
	if (i > a1)
	    cod_info2.table_select[1] =
		choosetable(ix + a1, ix + i, &cod_info2.part2_3_length);
	if (gi->part2_3_length > cod_info2.part2_3_length)
	    *gi = cod_info2;
    }
}

static const int slen1_n[16] = { 1, 1, 1, 1, 8, 2, 2, 2, 4, 4, 4, 8, 8, 8,16,16 };
static const int slen2_n[16] = { 1, 2, 4, 8, 1, 2, 4, 8, 2, 4, 8, 2, 4, 8, 4, 8 };
const int slen1_tab [16] = { 0, 0, 0, 0, 3, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4 };
const int slen2_tab [16] = { 0, 1, 2, 3, 0, 1, 2, 3, 1, 2, 3, 1, 2, 3, 2, 3 };

static void
scfsi_calc(
    int ch,
    III_side_info_t *l3_side
    )
{
    int i, s1, s2, c1, c2;
    int sfb;
    gr_info *gi = &l3_side->tt[1][ch];
    gr_info *g0 = &l3_side->tt[0][ch];

    for (i = 0; i < (sizeof(scfsi_band) / sizeof(int)) - 1; i++) {
	for (sfb = scfsi_band[i]; sfb < scfsi_band[i + 1]; sfb++) {
	    if (g0->scalefac[sfb] != gi->scalefac[sfb]
		&& gi->scalefac[sfb] >= 0)
		break;
	}
	if (sfb == scfsi_band[i + 1]) {
	    for (sfb = scfsi_band[i]; sfb < scfsi_band[i + 1]; sfb++) {
		gi->scalefac[sfb] = -1;
	    }
	    l3_side->scfsi[ch][i] = 1;
	}
    }

    s1 = c1 = 0;
    for (sfb = 0; sfb < 11; sfb++) {
	if (gi->scalefac[sfb] == -1)
	    continue;
	c1++;
	if (s1 < gi->scalefac[sfb])
	    s1 = gi->scalefac[sfb];
    }

    s2 = c2 = 0;
    for (; sfb < SBPSY_l; sfb++) {
	if (gi->scalefac[sfb] == -1)
	    continue;
	c2++;
	if (s2 < gi->scalefac[sfb])
	    s2 = gi->scalefac[sfb];
    }

    for (i = 0; i < 16; i++) {
	if (s1 < slen1_n[i] && s2 < slen2_n[i]) {
	    int c = slen1_tab[i] * c1 + slen2_tab[i] * c2;
	    if (gi->part2_length > c) {
		gi->part2_length = c;
		gi->scalefac_compress = i;
	    }
	}
    }
}

/*
Find the optimal way to store the scalefactors.
Only call this routine after final scalefactors have been
chosen and the channel/granule will not be re-encoded.
 */
void best_scalefac_store(
    lame_internal_flags * const gfc,
    const int             gr,
    const int             ch
    )
{
    /* use scalefac_scale if we can */
    III_side_info_t * const l3_side = &gfc->l3_side;
    gr_info *gi = &l3_side->tt[gr][ch];
    int sfb,i,j,l;
    int recalc = 0;

    /* remove scalefacs from bands with ix=0.  This idea comes
     * from the AAC ISO docs.  added mt 3/00 */
    /* check if l3_enc=0 */
    j = 0;
    for ( sfb = 0; sfb < gi->sfbmax; sfb++ ) {
	int width = gi->width[sfb];
	j += width;
	for (l = -width; l < 0; l++)
	    if (gi->l3_enc[l+j]!=0)
		break;
	if (l==0)
	    gi->scalefac[sfb] = recalc = -2; // anything goes.
    }

    if (!gi->scalefac_scale && !gi->preflag) {
	int s = 0;
	for (sfb = 0; sfb < gi->sfbmax; sfb++)
	    if (gi->scalefac[sfb] > 0)
		s |= gi->scalefac[sfb];

	if (!(s & 1) && s != 0) {
	    for (sfb = 0; sfb < gi->sfbmax; sfb++)
		if (gi->scalefac[sfb] > 0)
		    gi->scalefac[sfb] >>= 1;

	    gi->scalefac_scale = recalc = 1;
	}
    }

    if (!gi->preflag && gi->block_type != SHORT_TYPE) {
	for (sfb = 11; sfb < SBPSY_l; sfb++)
	    if (gi->scalefac[sfb] < pretab[sfb] && gi->scalefac[sfb] != -2)
		break;
	if (sfb == SBPSY_l) {
	    for (sfb = 11; sfb < SBPSY_l; sfb++)
		if (gi->scalefac[sfb] > 0)
		    gi->scalefac[sfb] -= pretab[sfb];

	    gi->preflag = recalc = 1;
	}
    }

    for ( i = 0; i < 4; i++ )
	l3_side->scfsi[ch][i] = 0;

    if (gfc->mode_gr==2 && gr == 1
	&& l3_side->tt[0][ch].block_type != SHORT_TYPE
	&& gi->block_type != SHORT_TYPE) {
      	scfsi_calc(ch, l3_side);
    } else if (recalc)
	gfc->scale_bitcounter(gi);
}


/* number of bits used to encode scalefacs */

/* 18*slen1_tab[i] + 18*slen2_tab[i] */
static const int scale_short[16] = {
    0, 18, 36, 54, 54, 36, 54, 72, 54, 72, 90, 72, 90, 108, 108, 126 };

/* 17*slen1_tab[i] + 18*slen2_tab[i] */
static const int scale_mixed[16] = {
    0, 18, 36, 54, 51, 35, 53, 71, 52, 70, 88, 69, 87, 105, 104, 122 };

/* 11*slen1_tab[i] + 10*slen2_tab[i] */
static const int scale_long[16] = {
    0, 10, 20, 30, 33, 21, 31, 41, 32, 42, 52, 43, 53, 63, 64, 74 };


/*************************************************************************/
/*            scale_bitcount                                             */
/*************************************************************************/

/* Also calculates the number of bits necessary to code the scalefactors. */
int
scale_bitcount(gr_info * const gi)
{
    int k, sfb, max_slen1 = 0, max_slen2 = 0;
    int *tab, *scalefac = gi->scalefac;

    /* maximum values */
    if ( gi->block_type == SHORT_TYPE ) {
	tab = scale_short;
	if (gi->mixed_block_flag)
	    tab = scale_mixed;
    } else {
        tab = scale_long;
	if (!gi->preflag) {
	    for ( sfb = 11; sfb < SBPSY_l; sfb++ )
		if (scalefac[sfb] < pretab[sfb])
		    break;

	    if (sfb == SBPSY_l) {
		gi->preflag = 1;
		for ( sfb = 11; sfb < SBPSY_l; sfb++ )
		    scalefac[sfb] -= pretab[sfb];
	    }
	}
    }

    for (sfb = 0; sfb < gi->sfbdivide; sfb++)
	if (max_slen1 < scalefac[sfb])
	    max_slen1 = scalefac[sfb];

    for (; sfb < gi->sfbmax; sfb++)
	if (max_slen2 < scalefac[sfb])
	    max_slen2 = scalefac[sfb];

    /* from Takehiro TOMINAGA <tominaga@isoternet.org> 10/99
     * loop over *all* posible values of scalefac_compress to find the
     * one which uses the smallest number of bits.  ISO would stop
     * at first valid index */
    gi->part2_length = LARGE_BITS;
    for ( k = 0; k < 16; k++ ) {
	if (max_slen1 < slen1_n[k] && max_slen2 < slen2_n[k]
	    && gi->part2_length > tab[k]) {
	    gi->part2_length=tab[k];
	    gi->scalefac_compress=k;
	}
    }
    return gi->part2_length;
}



/*************************************************************************/
/*            scale_bitcount_lsf                                         */
/*************************************************************************/

/* counts the number of bits to encode the scalefacs but for MPEG 2,2.5
 * Lower sampling frequencies  (24, 22.05 and 16 kHz.)
 *
 * This is reverse-engineered from section 2.4.3.2 of the MPEG2 IS,
 * "Audio Decoding Layer III"
 */

/*
  table of largest scalefactor values for MPEG2
*/
static const int max_range_sfac_tab[6][4] = {
    { 15, 15, 7,  7},
    { 15, 15, 7,  0},
    { 7,  3,  0,  0},
    { 15, 31, 31, 0},
    { 7,  7,  7,  0},
    { 3,  3,  0,  0}
};

static const int log2tab[] = {0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4};

int
scale_bitcount_lsf(gr_info * const gi)
{
    int table_number, row_in_table, partition, nr_sfb, window, i, sfb;
    const int *partition_table;
    int *scalefac = gi->scalefac;

    /*
      Set partition table. Note that should try to use table one,
      but do not yet...
    */
    table_number = gi->preflag * 2;
    for ( i = 0; i < 4; i++ )
	gi->slen[i] = 0;

    if ( gi->block_type == SHORT_TYPE ) {
	row_in_table = 1;
	partition_table = &nr_of_sfb_block[table_number][row_in_table][0];
	for ( sfb = 0, partition = 0; partition < 4; partition++ ) {
	    nr_sfb = partition_table[ partition ] / 3;
	    for ( i = 0; i < nr_sfb; i++, sfb++ )
		for ( window = 0; window < 3; window++ )
		    if (gi->slen[partition] < scalefac[sfb*3+window])
			gi->slen[partition] = scalefac[sfb*3+window];
	}
    } else {
	row_in_table = 0;
	partition_table = &nr_of_sfb_block[table_number][row_in_table][0];
	for ( sfb = 0, partition = 0; partition < 4; partition++ ) {
	    nr_sfb = partition_table[ partition ];
	    for ( i = 0; i < nr_sfb; i++, sfb++ )
		if (gi->slen[partition] < scalefac[sfb])
		    gi->slen[partition] = scalefac[sfb];
	}
    }

    gi->part2_length = LARGE_BITS;
    for (partition = 0; partition < 4; partition++ ) {
	if (gi->slen[partition] > max_range_sfac_tab[table_number][partition])
	    return gi->part2_length; /* fail */
	gi->slen[partition] = log2tab[gi->slen[partition]];
    }

    gi->sfb_partition_table = nr_of_sfb_block[table_number][row_in_table];
    /* set scalefac_compress */
    switch ( table_number ) {
    case 0:
	gi->scalefac_compress
	    = gi->slen[0]*80 + gi->slen[1]*16 + gi->slen[2]*4 + gi->slen[3];
	break;

    case 2:
	gi->scalefac_compress = 500 + gi->slen[0]*3 + gi->slen[1];
	break;
/*
    case 1:
	gi->scalefac_compress
	    = 400 + gi->slen[0]*20 + gi->slen[1]*4 + gi->slen[2];
	break;
*/
    default:
	/* ERRORF(gfc, "intensity stereo not implemented yet\n" ); */
	assert(0);
	break;
    }

    assert( gi->sfb_partition_table );
    gi->part2_length=0;
    for ( partition = 0; partition < 4; partition++ )
	gi->part2_length
	    += gi->slen[partition] * gi->sfb_partition_table[partition];

    return gi->part2_length;
}
