/*
 *	MP3 quantization
 *
 *	Copyright (c) 1999 Mark Taylor
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
#include "reservoir.h"
#include "quantize_pvt.h"
#include "vbrquantize.h"
#include "tables.h"

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

static  FLOAT
calc_sfb_noise(const FLOAT * xr, const FLOAT * xr34, int bw, int sf)
{
    int j;
    fi_union fi;
    FLOAT  xfsf = 0.0;
    FLOAT  sfpow, sfpow34;

    sfpow = POW20(sf);  /*pow(2.0,sf/4.0); */
    sfpow34 = IPOW20(sf); /*pow(sfpow,-3.0/4.0); */

    for (j = 0; j < bw; ++j) {
        double temp = sfpow34 * xr34[j];
        if (temp > IXMAX_VAL)
            return -1;

#ifdef TAKEHIRO_IEEE754_HACK
        temp += MAGIC_FLOAT;
        fi.f = temp;
        fi.f = temp + (adj43asm - MAGIC_INT)[fi.i];
        fi.i -= MAGIC_INT;
#else
        XRPOW_FTOI(temp, fi.i);
        XRPOW_FTOI(temp + QUANTFAC(fi.i), fi.i);
#endif

        temp = fabs(xr[j]) - pow43[fi.i] * sfpow;
        xfsf += temp * temp;
    }
    return xfsf;
}




inline int
find_scalefac(
    const FLOAT * xr, const FLOAT * xr34, FLOAT l3_xmin, int bw)
{
    FLOAT  xfsf;
    int     i, sf, sf_ok, delsf;

    /* search will range from sf:  -209 -> 45  */
    /* from the spec, we can use -321 -> 45 */
    sf = 128;
    delsf = 128;

    sf_ok = 10000;
    for (i = 0; i < 7; ++i) {
        delsf >>= 1;
        xfsf = calc_sfb_noise(xr, xr34, bw, sf);

        if (xfsf < 0) {
            /* scalefactors too small */
            sf += delsf;
        }
        else {
            if (xfsf > l3_xmin) {
                /* distortion.  try a smaller scalefactor */
                sf -= delsf;
            }
            else {
                sf_ok = sf;
                sf += delsf;
            }
        }
    }

    /*  returning a scalefac without distortion, if possible
     */
    if (sf_ok <= 255)
        return sf_ok;
    return sf;
}



/**
 *  Robert Hegemann 2001-05-01
 *  calculates quantization step size determined by allowed masking
 */
inline int
calc_scalefac(FLOAT l3_xmin, int bw)
{
    return 210 + (int) (9.0 * log10(l3_xmin / bw) - .5);
}



static const int max_range_short[SBMAX_s*3] = {
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    0, 0, 0 };

static const int max_range_long[SBMAX_l] =
    { 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 0 };

static const int max_range_long_lsf_pretab[SBMAX_l] =
    { 7, 7, 7, 7, 7, 7, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };



/*
    sfb=0..5  scalefac < 16 
    sfb>5     scalefac < 8

    ifqstep = ( gi->scalefac_scale == 0 ) ? 2 : 4;
    ol_sf =  (gi->global_gain-210.0);
    ol_sf -= 8*gi->subblock_gain[i];
    ol_sf -= ifqstep*scalefac[gr][ch].s[sfb][i];

*/
inline int
compute_scalefacs_short(int sf[], gr_info * const gi)
{
    const int maxrange1 = 15, maxrange2 = 7;
    int     maxrange, maxover = 0;
    int     sfb, i;
    int     ifqstep = (gi->scalefac_scale == 0) ? 2 : 4;

    for (i = 0; i < 3; ++i) {
        int     maxsf1 = 0, maxsf2 = 0, minsf = 1000;
        /* see if we should use subblock gain */
        for (sfb = 0; sfb < 6; ++sfb) { /* part 1 */
            if (maxsf1 < -sf[sfb*3+i])
                maxsf1 = -sf[sfb*3+i];
            if (minsf > -sf[sfb*3+i])
                minsf = -sf[sfb*3+i];
        }
        for (; sfb < SBPSY_s; ++sfb) { /* part 2 */
            if (maxsf2 < -sf[sfb*3+i])
                maxsf2 = -sf[sfb*3+i];
            if (minsf > -sf[sfb*3+i])
                minsf = -sf[sfb*3+i];
        }

        /* boost subblock gain as little as possible so we can
         * reach maxsf1 with scalefactors 
         * 8*sbg >= maxsf1   
         */
        maxsf1 = Max(maxsf1 - maxrange1 * ifqstep, maxsf2 - maxrange2 * ifqstep);
        gi->subblock_gain[i] = 0;
        if (minsf > 0)
            gi->subblock_gain[i] = floor(.125 * minsf + .001);
        if (maxsf1 > 0)
            gi->subblock_gain[i]
		= Max(gi->subblock_gain[i], (maxsf1 / 8 + (maxsf1 % 8 != 0)));
        if (gi->subblock_gain[i] > 7)
            gi->subblock_gain[i] = 7;

        for (sfb = 0; sfb < SBPSY_s; ++sfb) {
            sf[sfb*3+i] += 8 * gi->subblock_gain[i];

            if (sf[sfb*3+i] < 0) {
                maxrange = sfb < 6 ? maxrange1 : maxrange2;

                gi->scalefac[sfb*3+i]
                    = -sf[sfb*3+i] / ifqstep + (-sf[sfb*3+i] % ifqstep != 0);

                if (gi->scalefac[sfb*3+i] > maxrange)
                    gi->scalefac[sfb*3+i] = maxrange;

                if (maxover < -(sf[sfb*3+i] + gi->scalefac[sfb*3+i] * ifqstep))
                    maxover = -(sf[sfb*3+i] + gi->scalefac[sfb*3+i] * ifqstep);
            }
        }
        gi->scalefac[sfb*3+i] = 0;
    }

    return maxover;
}



/*
	  ifqstep = ( gi->scalefac_scale == 0 ) ? 2 : 4;
	  ol_sf =  (gi->global_gain-210.0);
	  ol_sf -= ifqstep*scalefac[gr][ch].l[sfb];
	  if (gi->preflag && sfb>=11) 
	  ol_sf -= ifqstep*pretab[sfb];
*/
inline int
compute_scalefacs_long_lsf(int *sf, gr_info * const gi)
{
    const int *max_range = max_range_long;
    int     ifqstep = (gi->scalefac_scale == 0) ? 2 : 4;
    int     sfb;
    int     maxover;

    if (gi->preflag) {
        max_range = max_range_long_lsf_pretab;
        for (sfb = 11; sfb < SBPSY_l; ++sfb)
            sf[sfb] += pretab[sfb] * ifqstep;
    }

    maxover = 0;
    for (sfb = 0; sfb < SBPSY_l; ++sfb) {

        if (sf[sfb] < 0) {
            /* ifqstep*scalefac >= -sf[sfb], so round UP */
            gi->scalefac[sfb] = -sf[sfb] / ifqstep + (-sf[sfb] % ifqstep != 0);
            if (gi->scalefac[sfb] > max_range[sfb])
                gi->scalefac[sfb] = max_range[sfb];

            /* sf[sfb] should now be positive: */
            if (-(sf[sfb] + gi->scalefac[sfb] * ifqstep) > maxover) {
                maxover = -(sf[sfb] + gi->scalefac[sfb] * ifqstep);
            }
        }
    }
    gi->scalefac[sfb] = 0;  /* sfb21 */

    return maxover;
}





/*
	  ifqstep = ( gi->scalefac_scale == 0 ) ? 2 : 4;
	  ol_sf =  (gi->global_gain-210.0);
	  ol_sf -= ifqstep*scalefac[gr][ch].l[sfb];
	  if (gi->preflag && sfb>=11) 
	  ol_sf -= ifqstep*pretab[sfb];
*/
inline int
compute_scalefacs_long(int *sf, gr_info * const gi)
{
    int     ifqstep = (gi->scalefac_scale == 0) ? 2 : 4;
    int     sfb;
    int     maxover;

    if (gi->preflag) {
        for (sfb = 11; sfb < SBPSY_l; ++sfb)
            sf[sfb] += pretab[sfb] * ifqstep;
    }

    maxover = 0;
    for (sfb = 0; sfb < SBPSY_l; ++sfb) {

        if (sf[sfb] < 0) {
            /* ifqstep*scalefac >= -sf[sfb], so round UP */
            gi->scalefac[sfb] = -sf[sfb] / ifqstep + (-sf[sfb] % ifqstep != 0);
            if (gi->scalefac[sfb] > max_range_long[sfb])
                gi->scalefac[sfb] = max_range_long[sfb];

            /* sf[sfb] should now be positive: */
            if (maxover < -(sf[sfb] + gi->scalefac[sfb] * ifqstep)) {
                maxover = -(sf[sfb] + gi->scalefac[sfb] * ifqstep);
            }
        }
    }
    gi->scalefac[sfb] = 0;  /* sfb21 */

    return maxover;
}








/***********************************************************************
 *
 *      block_sf()
 *
 *  set the gain of each scalefactor band
 *
 ***********************************************************************/
static void
block_sf(const lame_internal_flags * gfc, const FLOAT * l3_xmin,
	 int *vbrmin, int *vbrmax,
	 const FLOAT * xr34_orig, int * vbrsf, gr_info *gi)
{
    int     sfb, j;
    int     scalefac_method = gfc->quantcomp_method;

    if (gi->block_type == SHORT_TYPE)
	scalefac_method = gfc->quantcomp_method_s;
    j = 0;
    *vbrmin =  10000;
    *vbrmax = -10000;
    for (sfb = 0; sfb < gi->psymax; sfb++) {
        const int width = gi->width[sfb];
	int gain;
        if (scalefac_method >= 4) {
            gain = calc_scalefac(l3_xmin[sfb], width);
	} else {
            gain = find_scalefac(&gi->xr[j], &xr34_orig[j], l3_xmin[sfb], width);
        }
	j += width;
	vbrsf[sfb] = gain;
	if (*vbrmax < gain)
	    *vbrmax = gain;
	if (*vbrmin > gain)
	    *vbrmin = gain;
    }
}



/******************************************************************
 *
 *  short block scalefacs
 *
 ******************************************************************/

static void
short_block_scalefacs(const lame_internal_flags * gfc, gr_info * gi,
		      int *vbrsf,
                      int *VBRmax)
{
    int     sfb, b;
    int     maxover, maxover0, maxover1, mover;
    int     v0, v1;
    int     minsfb;
    int     vbrmax = *VBRmax;

    maxover0 = 0;
    maxover1 = 0;
    for (sfb = 0; sfb < gi->psymax; ++sfb) {
	v0 = (vbrmax - vbrsf[sfb]) - (4 * 14 + 2 * max_range_short[sfb]);
	v1 = (vbrmax - vbrsf[sfb]) - (4 * 14 + 4 * max_range_short[sfb]);
	if (maxover0 < v0)
	    maxover0 = v0;
	if (maxover1 < v1)
	    maxover1 = v1;
    }
    mover = maxover0;
    vbrmax -= mover;
    maxover0 -= mover;
    maxover1 -= mover;

    if (maxover0 == 0)
        gi->scalefac_scale = 0;
    else if (maxover1 == 0)
        gi->scalefac_scale = 1;

    gi->global_gain = vbrmax;

    if (gi->global_gain < 0) {
        gi->global_gain = 0;
    }
    else if (gi->global_gain > 255) {
        gi->global_gain = 255;
    }
    for (sfb = 0; sfb < SBMAX_s; ++sfb) {
        for (b = 0; b < 3; ++b) {
            vbrsf[sfb*3+b] -= vbrmax;
        }
    }
    assert(gi->global_gain < 256);
    maxover = compute_scalefacs_short(vbrsf, gi);//, gi->subblock_gain);

    assert(maxover <= 0);

    /* adjust global_gain so at least 1 subblock gain = 0 */
    minsfb = 999;       /* prepare for minimum search */
    for (b = 0; b < 3; ++b)
        if (minsfb > gi->subblock_gain[b])
            minsfb = gi->subblock_gain[b];

    if (minsfb > gi->global_gain / 8)
        minsfb = gi->global_gain / 8;

    vbrmax -= 8 * minsfb;
    gi->global_gain -= 8 * minsfb;

    for (b = 0; b < 3; ++b)
        gi->subblock_gain[b] -= minsfb;

    *VBRmax = vbrmax;
}



/******************************************************************
 *
 *  long block scalefacs
 *
 ******************************************************************/

static void
long_block_scalefacs(const lame_internal_flags * gfc, gr_info * gi,
		     int *vbrsf,
                     int *VBRmax)
{
    const int *max_rangep;
    int     sfb;
    int     maxover, maxover0, maxover1, maxover0p, maxover1p, mover;
    int     v0, v1, v0p, v1p;
    int     vbrmax = *VBRmax;

    max_rangep
      = gfc->mode_gr == 2 ? max_range_long : max_range_long_lsf_pretab;

    maxover0 = 0;
    maxover1 = 0;
    maxover0p = 0;      /* pretab */
    maxover1p = 0;      /* pretab */

    for (sfb = 0; sfb < gi->psymax; ++sfb) {
        v0 = (vbrmax - vbrsf[sfb]) - 2 * max_range_long[sfb];
        v1 = (vbrmax - vbrsf[sfb]) - 4 * max_range_long[sfb];
        v0p = (vbrmax - vbrsf[sfb]) - 2 * (max_rangep[sfb] + pretab[sfb]);
        v1p = (vbrmax - vbrsf[sfb]) - 4 * (max_rangep[sfb] + pretab[sfb]);
        if (maxover0 < v0)
            maxover0 = v0;
        if (maxover1 < v1)
            maxover1 = v1;
        if (maxover0p < v0p)
            maxover0p = v0p;
        if (maxover1p < v1p)
            maxover1p = v1p;
    }

    mover = Min(maxover0, maxover0p);
    vbrmax -= mover;
    maxover0 -= mover;
    maxover0p -= mover;
    maxover1 -= mover;
    maxover1p -= mover;

    if (maxover0 <= 0) {
        gi->scalefac_scale = 0;
        gi->preflag = 0;
        vbrmax -= maxover0;
    }
    else if (maxover0p <= 0) {
        gi->scalefac_scale = 0;
        gi->preflag = 1;
        vbrmax -= maxover0p;
    }
    else if (maxover1 == 0) {
        gi->scalefac_scale = 1;
        gi->preflag = 0;
    }
    else if (maxover1p == 0) {
        gi->scalefac_scale = 1;
        gi->preflag = 1;
    }
    else {
        assert(0);      /* this should not happen */
    }

    gi->global_gain = vbrmax;
    if (gi->global_gain < 0) {
        gi->global_gain = 0;
    }
    else if (gi->global_gain > 255)
        gi->global_gain = 255;

    for (sfb = 0; sfb < SBMAX_l; ++sfb)
        vbrsf[sfb] -= vbrmax;

    if (gfc->mode_gr == 2)
        maxover = compute_scalefacs_long(vbrsf, gi);
    else
        maxover = compute_scalefacs_long_lsf(vbrsf, gi);

    assert(maxover <= 0);

    *VBRmax = vbrmax;
}



/***********************************************************************
 *
 *  quantize xr34 based on scalefactors
 *
 *  block_xr34      
 *
 *  Mark Taylor 2000-??-??
 *  Robert Hegemann 2000-10-20 made functions of them
 *
 ***********************************************************************/

static void
block_xr34(const lame_internal_flags * gfc, const gr_info * gi,
	   const FLOAT * xr34_orig, FLOAT * xr34)
{
    int     sfb, j = 0, sfbmax, *scalefac = gi->scalefac;

    /* even though there is no scalefactor for sfb12
     * subblock gain affects upper frequencies too, that's why
     * we have to go up to SBMAX_s
     */
    sfbmax = gi->sfbmax;
    if (sfbmax == 35 || sfbmax == 36) /* short (including mixed) block case */
	sfbmax += 3;
    if (sfbmax == 21) /* long block case */
        sfbmax += 1;

    for (sfb = 0; sfb < sfbmax; ++sfb) {
	FLOAT fac;
	int s =
	    (((*scalefac++) + (gi->preflag ? pretab[sfb] : 0))
	     << (gi->scalefac_scale + 1))
	    + gi->subblock_gain[gi->window[sfb]] * 8;
	int l = gi->width[sfb];

	if (s == 0) {/* just copy */
	    memcpy(&xr34[j], &xr34_orig[j], sizeof(FLOAT)*l);
	    j += l;
	    continue;
	}

	fac = IIPOW20(s);
	l >>= 1;
	do {
	    xr34[j] = xr34_orig[j] * fac; ++j;
	    xr34[j] = xr34_orig[j] * fac; ++j;
	} while (--l > 0);
    }
}


/************************************************************************
 *
 *  VBR_noise_shaping()
 *
 *  may result in a need of too many bits, then do it CBR like
 *
 *  Robert Hegemann 2000-10-25
 *
 ***********************************************************************/
int
VBR_noise_shaping(
    lame_internal_flags * gfc, FLOAT * xr34orig, int minbits, int maxbits,
    FLOAT * l3_xmin, int gr, int ch)
{
    int vbrsf[SFBMAX];
    int vbrsf2[SFBMAX];
    gr_info *gi;
    FLOAT  xr34[576];
    int     ret;
    int     vbrmin, vbrmax, vbrmin2, vbrmax2;
    int     M, count;

    gi = &gfc->l3_side.tt[gr][ch];

    block_sf(gfc, l3_xmin, &vbrmin2, &vbrmax2, xr34orig, vbrsf2, gi);

    memcpy(vbrsf, vbrsf2, sizeof(vbrsf));
    vbrmin = vbrmin2;
    vbrmax = vbrmax2;

    M = (vbrmax - vbrmin) / 2;
    if ( M > 16 ) M = 16;
    if ( M <  1 ) M = 1;
    count = M;

    do {
        --count;

        if (gi->block_type == SHORT_TYPE) {
            short_block_scalefacs(gfc, gi, vbrsf, &vbrmax);
        }
        else {
            long_block_scalefacs(gfc, gi, vbrsf, &vbrmax);
        }

	/* encode scalefacs */
	if (gfc->mode_gr == 2)
	    ret = scale_bitcount(gi);
	else
	    ret = scale_bitcount_lsf(gfc, gi);

	if (ret != 0) {
	    ret = -1;
	} else {
	    /* quantize xr34 */
	    block_xr34(gfc, gi, xr34orig, xr34);
	    gi->part2_3_length = count_bits(gfc, xr34, gi);

	    if (gi->part2_3_length >= LARGE_BITS)
		ret = -2;
	    else if (gfc->use_best_huffman == 2)
		best_huffman_divide(gfc, gi);
	}

        if (vbrmin == vbrmax)
            break;
        else if (gi->part2_3_length < minbits) {
            int     i;
            vbrmax = vbrmin2 + (vbrmax2 - vbrmin2) * count / M;
            vbrmin = vbrmin2;
	    for (i = 0; i < gi->psymax; ++i) {
		vbrsf[i] = vbrmin2 + (vbrsf2[i] - vbrmin2) * count / M;
            }
        }
        else if (gi->part2_3_length > maxbits) {
            int     i;
            vbrmax = vbrmax2;
            vbrmin = vbrmax2 + (vbrmin2 - vbrmax2) * count / M;
	    for (i = 0; i < gi->psymax; ++i) {
		vbrsf[i] = vbrmax2 + (vbrsf2[i] - vbrmax2) * count / M;
            }
        }
        else
            break;
    } while (ret != -1);

    if (ret == -1)      /* Houston, we have a problem */
        return -1;

    if (gfc->substep_shaping & 2) {
	FLOAT xrtmp[576];
	trancate_smallspectrums(gfc, gi, l3_xmin, xrtmp);
    }

    if (gi->part2_3_length < minbits - gi->part2_length) {
        bin_search_StepSize (gfc, gi, minbits, ch, xr34);
    }
    if (gi->part2_3_length > maxbits - gi->part2_length) {
	bin_search_StepSize (gfc, gi, maxbits, ch, xr34);
    }
    assert (gi->global_gain < 256u);

    if (gi->part2_3_length + gi->part2_length >= LARGE_BITS) 
	return -2; /* Houston, we have a problem */

    return 0;
}
