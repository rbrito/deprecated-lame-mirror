/*
 *	MP3 huffman table selecting and bit counting
 *
 *	Copyright 1999-2003 Takehiro TOMINAGA
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
 *
 * $Id$
 */

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

/* log2(x). the last element is for the case when sfb is over valid range.*/
static const int log2tab[] = {
    0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5};

/*********************************************************************
 * nonlinear quantization of xr 
 * More accurate formula than the ISO formula.  Takes into account
 * the fact that we are quantizing xr -> ix, but we want ix^4/3 to be 
 * as close as possible to x^4/3.  (taking the nearest int would mean
 * ix is as close as possible to xr, which is different.)
 * From Segher Boessenkool <segher@eastsite.nl>  11/1999
 *********************************************************************/
static void
quantize_01(const FLOAT *xp, gr_info *gi, fi_union *fi, int sfb,
	    const FLOAT *xend)
{
    do {
	const FLOAT *xe = xp + gi->width[sfb];
	FLOAT istep = (1.0 - 0.4054) / IPOW20(scalefactor(gi, sfb));
	sfb++;
	if (xe > xend)
	    xe = xend;
	do {
	    (fi++)->i = *xp++ > istep ? 1:0;
	    (fi++)->i = *xp++ > istep ? 1:0;
	} while (xp < xe);
    } while (xp < xend);
}

/*
 * count how many bits we need to encode the quantized spectrums
 */
static int
count_bit_ESC( 
    const int *       ix, 
    const int * const end, 
          int         t1,
    const int         t2,
          int * const s)
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
          int * const s)
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
          int * const s)
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

/*************************************************************************
 *	choose table()
 *  Choose the Huffman table that will encode ix[begin..end] with
 *  the fewest bits.
 *  Note: This code contains knowledge about the sizes and characteristics
 *  of the Huffman tables as defined in the IS (Table B.7), and will not work
 *  with any arbitrary tables.
 *************************************************************************/
#if !HAVE_NASM
static
#endif
int choose_table_nonMMX(
    const int *       ix, 
    const int * const end,
          int * const s)
{
    int max, choice, choice2;
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
	for (choice2 = 24; choice2 < 32; choice2++)
	    if (ht[choice2].linmax >= max)
		break;

	for (choice = choice2 - 8; choice < 24; choice++)
	    if (ht[choice].linmax >= max)
		break;

	return count_bit_ESC(ix, end, choice, choice2, s);
    }
}



int
noquant_count_bits(const lame_internal_flags * const gfc, gr_info * const gi)
{
    int i, a1, a2;
    int *const ix = gi->l3_enc;

    /* Determine count1 region */
    for (i = gi->count1; i > 1; i -= 2)
	if (ix[i - 1] | ix[i - 2])
	    break;
    gi->count1 = i;

    /* Determines the number of bits to encode the quadruples. */
    a1 = a2 = 0;
    for (; i > 3; i -= 4) {
	int p;
	/* hack to check if all values <= 1 */
	if ((ix[i-1] | ix[i-2] | ix[i-3] | ix[i-4]) > 1u)
	    break;

	p = ((ix[i-4] * 2 + ix[i-3]) * 2 + ix[i-2]) * 2 + ix[i-1];
	a1 += quadcode[0][p];
	a2 += quadcode[1][p];
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
	a1 = a2;
    } else {
	gi->table_select[1] = choosetable(ix + a1, ix + a2,
					  &gi->part2_3_length);
    }
    gi->table_select[0] = choosetable(ix, ix + a1, &gi->part2_3_length);

    if (gfc->use_best_huffman == 2)
	best_huffman_divide (gfc, gi);

    return gi->part2_3_length;
}

int
count_bits(const lame_internal_flags * const gfc, gr_info * const gi)
{
    /* quantize on xr^(3/4) instead of xr */
    int sfb = 0;
    fi_union *fi = (fi_union *)gi->l3_enc;
    const FLOAT *xp = xr34;
    const FLOAT *xe01 = &xp[gi->big_values];
    const FLOAT *xend = &xp[gi->count1];

    do {
	const FLOAT *xe; FLOAT istep;
	if (xp > xe01) {
	    quantize_01(xp, gi, fi, sfb, xend);
	    break;
	}

	xe = xp + gi->width[sfb];
	if (xe > xend)
	    xe = xend;
#ifdef HAVE_NASM
	if (gfc->CPU_features.AMD_3DNow) {
	    fi -= xp-xe;
	    quantize_sfb_3DN(xe, xp-xe, scalefactor(gi, sfb), &fi[0].i);
	    xp = xe;
	} else
#endif
	{
	    istep = IPOW20(scalefactor(gi, sfb));
	    do {
#ifdef TAKEHIRO_IEEE754_HACK
		double x0 = istep * xp[0] + MAGIC_FLOAT;
		double x1 = istep * xp[1] + MAGIC_FLOAT;
		xp += 2;
		if (x0 > MAGIC_FLOAT + IXMAX_VAL) x0 = MAGIC_FLOAT + IXMAX_VAL;
		if (x1 > MAGIC_FLOAT + IXMAX_VAL) x1 = MAGIC_FLOAT + IXMAX_VAL;
		fi[0].f = x0;
		fi[1].f = x1;
		fi[0].f = x0 + (adj43asm - MAGIC_INT)[fi[0].i];
		fi[1].f = x1 + (adj43asm - MAGIC_INT)[fi[1].i];
		fi[0].i -= MAGIC_INT;
		fi[1].i -= MAGIC_INT;
		fi += 2;
#else
		FLOAT x0 = *xp++ * istep;
		FLOAT x1 = *xp++ * istep;
		if (x0 > IXMAX_VAL) x0 = IXMAX_VAL;
		if (x1 > IXMAX_VAL) x1 = IXMAX_VAL;
		(fi++)->i = (int)(x0 + adj43[(int)x0]);
		(fi++)->i = (int)(x1 + adj43[(int)x1]);
#endif
	    } while (xp < xe);
	}
	sfb++;
    } while (xp < xend);

    if (gfc->noise_shaping_amp >= 3) {
	int sfb, j = 0;
	for (sfb = 0; sfb < gi->psymax && j < gi->count1; sfb++) {
	    FLOAT roundfac;
	    int l = -gi->width[sfb];
	    j -= l;
	    if (!gfc->pseudohalf[sfb])
		continue;
	    roundfac = 0.634521682242439
		/ IPOW20(scalefactor(gi, sfb) + gi->scalefac_scale);
	    do {
		if (xr34[j+l] < roundfac)
		    gi->l3_enc[j+l] = 0;
	    } while (++l < 0);
	}
    }
    return noquant_count_bits(gfc, gi);
}

/***********************************************************************
  best huffman table selection for scalefactor band
  the saved bits are kept in the bit reservoir.
 **********************************************************************/
inline static void
recalc_divide_init(
    const lame_internal_flags * const gfc,
          gr_info         *gi,
          int             r01_bits[],
          int             r01_div [],
          int             r0_tbl  [],
          int             r1_tbl  [] )
{
    int r0;
    for (r0 = 0; r0 <= 7 + 15; r0++)
	r01_bits[r0] = LARGE_BITS;

    for (r0 = 0; r0 < 16; r0++) {
	int a1, r0bits, r1, r0t, r1t, bits;
	if (gfc->scalefac_band.l[r0 + 2] >= gi->big_values)
	    break;
	r0bits = 0;
	a1 = gfc->scalefac_band.l[r0 + 1];
	r0t = choosetable(gi->l3_enc, &gi->l3_enc[a1], &r0bits);

	for (r1 = 0; r1 < 8; r1++) {
	    int a2 = gfc->scalefac_band.l[r0 + r1 + 2];
	    if (a2 >= gi->big_values)
		break;

	    bits = r0bits;
	    r1t = choosetable(&gi->l3_enc[a1], &gi->l3_enc[a2], &bits);
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
    const int             r01_bits[],
    const int             r01_div [],
    const int             r0_tbl  [],
    const int             r1_tbl  [] )
{
    int bits, r2, a2, r2t, old = gi->part2_3_length;
    for (r2 = 0; r2 < SBMAX_l - 1; r2++) {
	bits = r01_bits[r2] + gi->count1bits;
	if (gi->part2_3_length <= bits)
	    break;
	a2 = gfc->scalefac_band.l[r2+2];
	if (a2 >= gi->big_values)
	    break;

	r2t = choosetable(&gi->l3_enc[a2], &gi->l3_enc[gi->big_values], &bits);
	if (gi->part2_3_length <= bits)
	    continue;

	gi->part2_3_length = bits;
	gi->region0_count = r01_div[r2];
	gi->region1_count = r2 - r01_div[r2];
	gi->table_select[0] = r0_tbl[r2];
	gi->table_select[1] = r1_tbl[r2];
	gi->table_select[2] = r2t;
    }
    return gi->part2_3_length - old;
}

void
best_huffman_divide(const lame_internal_flags * const gfc, gr_info * const gi)
{
    int i, a1, a2;
    gr_info gi_w;
    int * const ix = gi->l3_enc;

    int r01_bits[7 + 15 + 1];
    int r01_div[7 + 15 + 1];
    int r0_tbl[7 + 15 + 1];
    int r1_tbl[7 + 15 + 1];

    if (gi->big_values == 0)
	return;

    if (gi->block_type == NORM_TYPE) {
	recalc_divide_init(gfc, gi, r01_bits,r01_div,r0_tbl,r1_tbl);
	recalc_divide_sub(gfc, gi, r01_bits,r01_div,r0_tbl,r1_tbl);
    }

    i = gi->big_values;
    if ((unsigned int)(ix[i-2] | ix[i-1]) > 1)
	return;

    i = gi->count1 + 2;
    if (i > 576)
	return;

    gi_w = *gi;
    gi_w.count1 = i;
    gi_w.big_values -= 2;

    a1 = a2 = 0;
    for (; i > gi_w.big_values; i -= 4) {
	int p = ((ix[i-4] * 2 + ix[i-3]) * 2 + ix[i-2]) * 2 + ix[i-1];
	a1 += quadcode[0][p];
	a2 += quadcode[1][p];
    }

    gi_w.count1table_select = 0;
    if (a1 > a2) {
	a1 = a2;
	gi_w.count1table_select = 1;
    }

    gi_w.count1bits = a1;

    if (gi_w.block_type == NORM_TYPE) {
	if (recalc_divide_sub(gfc, &gi_w, r01_bits,r01_div,r0_tbl,r1_tbl))
	    *gi = gi_w;
    } else {
	/* Count the number of bits necessary to code the bigvalues region. */
	gi_w.part2_3_length = a1;
	a1 = gfc->scalefac_band.l[gi->region0_count+1];
	if (a1 > i)
	    a1 = i;

	if (a1 > 0)
	    gi_w.table_select[0] =
		choosetable(ix, ix + a1, &gi_w.part2_3_length);
	if (i > a1)
	    gi_w.table_select[1] =
		choosetable(ix + a1, ix + i, &gi_w.part2_3_length);
	if (gi->part2_3_length > gi_w.part2_3_length)
	    *gi = gi_w;
    }
}

/***********************************************************************
  re-calculate the best scalefac_compress using scfsi
  the saved bits are kept in the bit reservoir.
 **********************************************************************/
static void
scfsi_calc(int ch, III_side_info_t *l3_side)
{
    int i, s1, s2, c1, c2, sfb;
    gr_info *gi = &l3_side->tt[1][ch];
    gr_info *g0 = &l3_side->tt[0][ch];
    s1 = 0;
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
	    s1 = l3_side->scfsi[ch][i] = 1;
	}
    }

    if (!s1) return;
    s1 = c1 = 0;
    for (sfb = 0; sfb < 11; sfb++) {
	if (gi->scalefac[sfb] == -1)
	    continue;
	c1++;
	if (s1 < gi->scalefac[sfb])
	    s1 = gi->scalefac[sfb];
    }
    s1 = log2tab[s1];

    s2 = c2 = 0;
    for (; sfb < SBPSY_l; sfb++) {
	if (gi->scalefac[sfb] == -1)
	    continue;
	c2++;
	if (s2 < gi->scalefac[sfb])
	    s2 = gi->scalefac[sfb];
    }
    s2 = log2tab[s2];

    for (i = 0; i < 16; i++) {
	if (s1 <= s1bits[i] && s2 <= s2bits[i]) {
	    int c = s1bits[i] * c1 + s2bits[i] * c2;
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
static void
best_scalefac_store(
    lame_internal_flags * const gfc,
    const int             gr,
    const int             ch
    )
{
    /* use scalefac_scale if we can */
    III_side_info_t * const l3_side = &gfc->l3_side;
    gr_info *gi = &l3_side->tt[gr][ch];
    int sfb,j,recalc=0;

    memset(l3_side->scfsi[ch], 0, sizeof(l3_side->scfsi[ch]));

    /* remove scalefacs from bands with all ix=0.
     * This idea comes from the AAC ISO docs.  added mt 3/00 */
    j = 0;
    for (sfb = 0; sfb < gi->psymax; sfb++) {
	int l = -gi->width[sfb];
	j -= l;
	do {
	    if (gi->l3_enc[l+j]!=0)
		break;
	} while (++l < 0);
	if (l==0)
	    gi->scalefac[sfb] = recalc = -2; /* anything goes. */
    }

    if (gi->psymax > gi->sfbmax && gi->block_type != SHORT_TYPE
	&& gi->scalefac[gi->sfbmax] == -2) {
	int minsfb = 15;
	for (sfb = 0; sfb < gi->psymax; sfb++)
	    if (gi->scalefac[sfb] >= 0
		&& minsfb > gi->scalefac[sfb] + (gi->preflag>0 ? pretab[sfb]:0))
		minsfb = gi->scalefac[sfb] + (gi->preflag>0 ? pretab[sfb]:0);
	if (minsfb != 0) {
	    gr_info gi_w = *gi;
	    if (gi->global_gain - (minsfb << (gi->scalefac_scale+1)) < 0)
		minsfb = gi->global_gain >> (gi->scalefac_scale+1);
	    gi->global_gain -= minsfb << (gi->scalefac_scale+1);
	    for (sfb = 0; sfb < gi->psymax; sfb++)
		if (gi->scalefac[sfb] != -2)
		    gi->scalefac[sfb] -= minsfb-(gi->preflag>0 ? pretab[sfb]:0);
	    if (gi->preflag > 0)
		gi->preflag = 0;
	    gfc->scale_bitcounter(gi);
	    if (gi->part2_length > gi_w.part2_length)
		*gi = gi_w;
	}
    }

    if (!gi->scalefac_scale && !gi->preflag) {
	int s = 0;
	for (sfb = 0; sfb < gi->psymax; sfb++)
	    if (gi->scalefac[sfb] > 0)
		s |= gi->scalefac[sfb];

	if (!(s & 1) && s != 0) {
	    for (sfb = 0; sfb < gi->psymax; sfb++)
		if (gi->scalefac[sfb] > 0)
		    gi->scalefac[sfb] >>= 1;

	    gi->scalefac_scale = recalc = 1;
	}
    }

    if (gfc->mode_gr == 2 && !gi->preflag && gi->block_type != SHORT_TYPE) {
	for (sfb = 11; sfb < gi->psymax; sfb++)
	    if (gi->scalefac[sfb] < pretab[sfb] && gi->scalefac[sfb] != -2)
		break;
	if (sfb == gi->psymax) {
	    for (sfb = 11; sfb < gi->psymax; sfb++)
		if (gi->scalefac[sfb] > 0)
		    gi->scalefac[sfb] -= pretab[sfb];

	    gi->preflag = recalc = 1;
	}
    }

    if (gfc->mode_gr==2 && gr == 1
	&& l3_side->tt[0][ch].block_type != SHORT_TYPE
	&& gi->block_type != SHORT_TYPE) {
	scfsi_calc(ch, l3_side);
    } else if (recalc)
	gfc->scale_bitcounter(gi);
}


/************************************************************************
 *
 *      iteration_finish_one()
 *
 *  Robert Hegemann 2000-09-06
 *
 ************************************************************************/
int
iteration_finish_one(lame_internal_flags *gfc, int gr, int ch)
{
    gr_info *gi = &gfc->l3_side.tt[gr][ch];
    /*  try some better scalefac storage  */
    best_scalefac_store (gfc, gr, ch);

    /*  best huffman_divide may save some bits too */
    if (gfc->use_best_huffman == 1)
	best_huffman_divide (gfc, gi);

    return gi->part2_length + gi->part2_3_length;
}

/*************************************************************************
 *	scale_bitcount()
 * calculates the number of bits necessary to code the scalefactors.
 *************************************************************************/

/* 18*s1bits[i] + 18*s2bits[i] */
static const int scale_short[16] = {
    0, 18, 36, 54, 54, 36, 54, 72, 54, 72, 90, 72, 90, 108, 108, 126 };

/* 17*s1bits[i] + 18*s2bits[i] */
static const int scale_mixed[16] = {
    0, 18, 36, 54, 51, 35, 53, 71, 52, 70, 88, 69, 87, 105, 104, 122 };

/* 11*s1bits[i] + 10*s2bits[i] */
static const int scale_long[16] = {
    0, 10, 20, 30, 33, 21, 31, 41, 32, 42, 52, 43, 53, 63, 64, 74 };
int
scale_bitcount(gr_info * const gi)
{
    int k, sfb, s1, s2;
    const int *tab;

    /* maximum values */
    if (gi->block_type == SHORT_TYPE) {
	tab = scale_short;
	if (gi->mixed_block_flag)
	    tab = scale_mixed;
    } else {
	tab = scale_long;
	if (!gi->preflag) {
	    for (sfb = 11; sfb < gi->psymax; sfb++)
		if (gi->scalefac[sfb] < pretab[sfb])
		    break;

	    if (sfb == gi->psymax) {
		gi->preflag = 1;
		for (sfb = 11; sfb < gi->psymax; sfb++)
		    gi->scalefac[sfb] -= pretab[sfb];
	    }
	}
    }

    s1 = s2 = 0;
    for (sfb = 0; sfb < gi->sfbdivide; sfb++)
	if (s1 < gi->scalefac[sfb])
	    s1 = gi->scalefac[sfb];
    for (; sfb < gi->sfbmax; sfb++)
	if (s2 < gi->scalefac[sfb])
	    s2 = gi->scalefac[sfb];
    s1 = log2tab[s1];
    s2 = log2tab[s2];

    /* from Takehiro TOMINAGA <tominaga@isoternet.org> 10/99
     * loop over *all* posible values of scalefac_compress to find the
     * one which uses the smallest number of bits.  ISO would stop
     * at first valid index */
    gi->part2_length = LARGE_BITS;
    for (k = 0; k < 16; k++) {
	if (gi->part2_length > tab[k] && s1 <= s1bits[k] && s2 <= s2bits[k]) {
	    gi->part2_length = tab[k];
	    gi->scalefac_compress = k;
	}
    }
    return gi->part2_length;
}

/*************************************************************************
 *	scale_bitcount_lsf()
 *
 * counts the number of bits to encode the scalefacs for MPEG 2 and 2.5
 * Lower sampling frequencies  (lower than 24kHz.)
 *
 * This is reverse-engineered from section 2.4.3.2 of the MPEG2 IS,
 * "Audio Decoding Layer III"
 ************************************************************************/
/* table of largest scalefactor values for MPEG2 */
static const int max_range_sfac_tab[6][4] = {
    { 4, 4, 3, 3}, { 4, 4, 3, 0}, { 3, 2, 0, 0},
    { 4, 5, 5, 0}, { 3, 3, 3, 0}, { 2, 2, 0, 0}
};

int
scale_bitcount_lsf(gr_info * const gi)
{
    int part, i, sfb, table_type, tableID;

    table_type = 0;
    if (gi->preflag < 0)
	table_type = 3; /* intensity stereo */

    tableID = table_type * 3;
    if (gi->block_type == SHORT_TYPE) {
	tableID++;
	if (gi->mixed_block_flag)
	    tableID++;
    }

    sfb = i = 0;
    for (part = 0; part < 4; part++) {
	int m = 0, sfbend = sfb + nr_of_sfb_block[tableID][part];
	for (; sfb < sfbend; sfb++)
	    if (m < gi->scalefac[sfb])
		m = gi->scalefac[sfb];
	gi->slen[part] = m = log2tab[m];

	if (m > max_range_sfac_tab[table_type][part])
	    return (gi->part2_length = LARGE_BITS); /* fail */
	i += m * nr_of_sfb_block[tableID][part];
    }
    gi->part2_length = i;

    if (table_type == 0) {
	/* try to use table type 1 and 2(preflag) */
	int slen2[4];
	sfb = i = 0;
	for (part = 0; part < 4; part++) {
	    int m = 0, sfbend = sfb + nr_of_sfb_block[3+tableID][part];
	    for (; sfb < sfbend; sfb++)
		if (m < gi->scalefac[sfb])
		    m = gi->scalefac[sfb];
	    slen2[part] = m = log2tab[m];

	    if (m > max_range_sfac_tab[1][part]) {
		i = LARGE_BITS;
		break;
	    }
	    i += m * nr_of_sfb_block[3+tableID][part];
	}
	if (gi->part2_length > i) {
	    gi->part2_length = i;
	    tableID += 3;
	    memcpy(gi->slen, slen2, sizeof(slen2));
	}

	if ((tableID == 3 || tableID == 0) && gi->preflag == 0) {
	    /* not SHORT block and not i-stereo of channel 1 */
	    for (part = 0; part < 4; part++) {
		int m = 0, sfbend = sfb + nr_of_sfb_block[6+tableID][part];
		for (; sfb < sfbend; sfb++) {
		    if (gi->scalefac[sfb] < pretab[sfb])
			m = LARGE_BITS;
		    if (m < gi->scalefac[sfb]-pretab[sfb])
			m = gi->scalefac[sfb]-pretab[sfb];
		}
		slen2[part] = m = log2tab[m];

		if (m > max_range_sfac_tab[2][part]) {
		    i = LARGE_BITS;
		    break;
		}
		i += m * nr_of_sfb_block[6+tableID][part];
	    }
	    if (gi->part2_length > i) {
		gi->part2_length = i;
		tableID = 6;
		memcpy(gi->slen, slen2, sizeof(slen2));
	    }
	}
    }
    gi->scalefac_compress = tableID;
    return gi->part2_length;
}
