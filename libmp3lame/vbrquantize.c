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
#include "l3side.h"
#include "reservoir.h"
#include "quantize_pvt.h"
#include "vbrquantize.h"

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif



typedef union {
    float   f;
    int     i;
} fi_union;

#define MAGIC_FLOAT (65536*(128))
#define MAGIC_INT    0x4b000000

#ifdef TAKEHIRO_IEEE754_HACK

#define DUFFBLOCK() do { \
        xp = xr34[index] * sfpow34_p1; \
        xe = xr34[index] * sfpow34_eq; \
        xm = xr34[index] * sfpow34_m1; \
        if (xm > IXMAX_VAL)  \
            return -1; \
        xp += MAGIC_FLOAT; \
        xe += MAGIC_FLOAT; \
        xm += MAGIC_FLOAT; \
        fi[0].f = xp; \
        fi[1].f = xe; \
        fi[2].f = xm; \
        fi[0].f = xp + (adj43asm - MAGIC_INT)[fi[0].i]; \
        fi[1].f = xe + (adj43asm - MAGIC_INT)[fi[1].i]; \
        fi[2].f = xm + (adj43asm - MAGIC_INT)[fi[2].i]; \
        fi[0].i -= MAGIC_INT; \
        fi[1].i -= MAGIC_INT; \
        fi[2].i -= MAGIC_INT; \
        x0 = fabs(xr[index]); \
        xp = x0 - pow43[fi[0].i] * sfpow_p1; \
        xe = x0 - pow43[fi[1].i] * sfpow_eq; \
        xm = x0 - pow43[fi[2].i] * sfpow_m1; \
        xfsf_p1 += xp * xp; \
        xfsf_eq += xe * xe; \
        xfsf_m1 += xm * xm; \
        ++index; \
    } while(0)
#else

/*********************************************************************
 * XRPOW_FTOI is a macro to convert floats to ints.  
 * if XRPOW_FTOI(x) = nearest_int(x), then QUANTFAC(x)=adj43asm[x]
 *                                         ROUNDFAC= -0.0946
 *
 * if XRPOW_FTOI(x) = floor(x), then QUANTFAC(x)=asj43[x]   
 *                                   ROUNDFAC=0.4054
 *********************************************************************/
#  define QUANTFAC(rx)  adj43[rx]
#  define ROUNDFAC 0.4054
#  define XRPOW_FTOI(src,dest) ((dest) = (int)(src))


#endif

/*  caution: a[] will be resorted!!
 */
static int
select_kth_int(int a[], int N, int k)
{
    int i, j, l, r, v, w;
    
    l = 0;
    r = N-1;
    while (r > l) {
        v = a[r];
        i = l-1;
        j = r;
        for (;;) {
            while (a[++i] < v) /*empty*/;
            while (a[--j] > v) /*empty*/;
            if (i >= j) 
                break;
            /* swap i and j */
            w = a[i];
            a[i] = a[j];
            a[j] = w;
        }
        /* swap i and r */
        w = a[i];
        a[i] = a[r];
        a[r] = w;
        if (i >= k) 
            r = i-1;
        if (i <= k) 
            l = i+1;
    }
    return a[k];
}



/*  caution: a[] will be resorted!!
 */
static FLOAT8
select_kth(FLOAT8 a[], int N, int k)
{
    int     i, j, l, r;
    FLOAT8  v, w;

    l = 0;
    r = N - 1;
    while (r > l) {
        v = a[r];
        i = l - 1;
        j = r;
        for (;;) {
            while (a[++i] < v) /*empty */
                ;
            while (a[--j] > v) /*empty */
                ;
            if (i >= j)
                break;
            /* swap i and j */
            w = a[i];
            a[i] = a[j];
            a[j] = w;
        }
        /* swap i and r */
        w = a[i];
        a[i] = a[r];
        a[r] = w;
        if (i >= k)
            r = i - 1;
        if (i <= k)
            l = i + 1;
    }
    return a[k];
}




static  FLOAT8
calc_sfb_noise(const FLOAT8 * xr, const FLOAT8 * xr34, unsigned int bw, int sf)
{
    unsigned int j;
    fi_union fi;
    FLOAT8  temp;
    FLOAT8  xfsf = 0;
    FLOAT8  sfpow, sfpow34;

    sfpow = POW20(sf);  /*pow(2.0,sf/4.0); */
    sfpow34 = IPOW20(sf); /*pow(sfpow,-3.0/4.0); */

    for (j = 0; j < bw; ++j) {
        temp = sfpow34 * xr34[j];
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




static  FLOAT8
calc_sfb_noise_mq(const FLOAT8 * xr, const FLOAT8 * xr34, int bw, int sf, int mq)
{
    int     j, k;
    fi_union fi;
    FLOAT8  temp, scratch[192];
    FLOAT8  sfpow, sfpow34, xfsfm = 0, xfsf = 0;

    sfpow = POW20(sf);  /*pow(2.0,sf/4.0); */
    sfpow34 = IPOW20(sf); /*pow(sfpow,-3.0/4.0); */

    for (j = 0; j < bw; ++j) {
        temp = xr34[j] * sfpow34;
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
        temp *= temp;

        scratch[j] = temp;
        if (xfsfm < temp)
            xfsfm = temp;
        xfsf += temp;
    }
    if (mq == 1)
        return bw * select_kth(scratch, bw, bw * 13 / 16);

    xfsf /= bw;
    for (k = 1, j = 0; j < bw; ++j) {
        if (scratch[j] > xfsf) {
            xfsfm += scratch[j];
            ++k;
        }
    }
    return xfsfm / k * bw;
}


static const FLOAT8 facm1 = .8408964153; /* pow(2,(sf-1)/4.0) */
static const FLOAT8 facp1 = 1.189207115;
static const FLOAT8 fac34m1 = 1.13878863476; /* .84089 ^ -3/4 */
static const FLOAT8 fac34p1 = 0.878126080187;

static  FLOAT8
calc_sfb_noise_ave(const FLOAT8 * xr, const FLOAT8 * xr34, int bw, int sf)
{
    double  xp;
    double  xe;
    double  xm;
#ifdef TAKEHIRO_IEEE754_HACK
    double  x0;
    unsigned int index = 0;
#endif
    int     xx[3], j;
    fi_union *fi = (fi_union *) xx;
    FLOAT8  sfpow34_eq, sfpow34_p1, sfpow34_m1;
    FLOAT8  sfpow_eq, sfpow_p1, sfpow_m1;
    FLOAT8  xfsf_eq = 0, xfsf_p1 = 0, xfsf_m1 = 0;

    sfpow_eq = POW20(sf); /*pow(2.0,sf/4.0); */
    sfpow_m1 = sfpow_eq * facm1;
    sfpow_p1 = sfpow_eq * facp1;

    sfpow34_eq = IPOW20(sf); /*pow(sfpow,-3.0/4.0); */
    sfpow34_m1 = sfpow34_eq * fac34m1;
    sfpow34_p1 = sfpow34_eq * fac34p1;

#ifdef TAKEHIRO_IEEE754_HACK
    /*
     *  loop unrolled into "Duff's Device".   Robert Hegemann
     */
    j = (bw + 3) / 4;
    switch (bw % 4) {
    default:
    case 0:
        do {
            DUFFBLOCK();
    case 3:
            DUFFBLOCK();
    case 2:
            DUFFBLOCK();
    case 1:
            DUFFBLOCK();
        } while (--j);
    }
#else
    for (j = 0; j < bw; ++j) {

        if (xr34[j] * sfpow34_m1 > IXMAX_VAL)
            return -1;

        xe = xr34[j] * sfpow34_eq;
        XRPOW_FTOI(xe, fi[0].i);
        XRPOW_FTOI(xe + QUANTFAC(fi[0].i), fi[0].i);
        xe = fabs(xr[j]) - pow43[fi[0].i] * sfpow_eq;
        xe *= xe;

        xp = xr34[j] * sfpow34_p1;
        XRPOW_FTOI(xp, fi[0].i);
        XRPOW_FTOI(xp + QUANTFAC(fi[0].i), fi[0].i);
        xp = fabs(xr[j]) - pow43[fi[0].i] * sfpow_p1;
        xp *= xp;

        xm = xr34[j] * sfpow34_m1;
        XRPOW_FTOI(xm, fi[0].i);
        XRPOW_FTOI(xm + QUANTFAC(fi[0].i), fi[0].i);
        xm = fabs(xr[j]) - pow43[fi[0].i] * sfpow_m1;
        xm *= xm;

        xfsf_eq += xe;
        xfsf_p1 += xp;
        xfsf_m1 += xm;
    }
#endif

    if (xfsf_eq < xfsf_p1)
        xfsf_eq = xfsf_p1;
    if (xfsf_eq < xfsf_m1)
        xfsf_eq = xfsf_m1;
    return xfsf_eq;
}



inline int
find_scalefac(
    const FLOAT8 * xr, const FLOAT8 * xr34, FLOAT8 l3_xmin, int bw)
{
    FLOAT8  xfsf;
    int     i, sf, sf_ok, delsf;

    /* search will range from sf:  -209 -> 45  */
    sf = 128;
    delsf = 128;

    sf_ok = 10000;
    for (i = 0; i < 7; ++i) {
        delsf /= 2;
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

inline int
find_scalefac_mq(const FLOAT8 * xr, const FLOAT8 * xr34, FLOAT8 l3_xmin,
		 int bw, int mq)
{
    FLOAT8  xfsf;
    int     i, sf, sf_ok, delsf;

    /* search will range from sf:  -209 -> 45  */
    sf = 128;
    delsf = 128;

    sf_ok = 10000;
    for (i = 0; i < 7; ++i) {
        delsf /= 2;
        xfsf = calc_sfb_noise_mq(xr, xr34, bw, sf, mq);

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

inline int
find_scalefac_ave(
    const FLOAT8 * xr, const FLOAT8 * xr34, FLOAT8 l3_xmin, int bw)
{
    FLOAT8  xfsf;
    int     i, sf, sf_ok, delsf;

    /* search will range from sf:  -209 -> 45  */
    sf = 128;
    delsf = 128;

    sf_ok = 10000;
    for (i = 0; i < 7; ++i) {
        delsf /= 2;
        xfsf = calc_sfb_noise_ave(xr, xr34, bw, sf);

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
calc_scalefac(FLOAT8 l3_xmin, int bw, FLOAT8 preset_tune)
{
    FLOAT8 const c = (preset_tune > 0 ? preset_tune : 5.799142446); /* 10 * 10^(2/3) * log10(4/3) */
    return 210 + (int) (c * log10(l3_xmin / bw) - .5);
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

    ifqstep = ( cod_info->scalefac_scale == 0 ) ? 2 : 4;
    ol_sf =  (cod_info->global_gain-210.0);
    ol_sf -= 8*cod_info->subblock_gain[i];
    ol_sf -= ifqstep*scalefac[gr][ch].s[sfb][i];

*/
inline int
compute_scalefacs_short(int sf[], const gr_info * cod_info, int scalefac[],
			int *sbg)
{
    const int maxrange1 = 15, maxrange2 = 7;
    int     maxrange, maxover = 0;
    int     sfb, i;
    int     ifqstep = (cod_info->scalefac_scale == 0) ? 2 : 4;

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
        sbg[i] = 0;
        if (minsf > 0)
            sbg[i] = floor(.125 * minsf + .001);
        if (maxsf1 > 0)
            sbg[i] = Max(sbg[i], (maxsf1 / 8 + (maxsf1 % 8 != 0)));
        if (sbg[i] > 7)
            sbg[i] = 7;

        for (sfb = 0; sfb < SBPSY_s; ++sfb) {
            sf[sfb*3+i] += 8 * sbg[i];

            if (sf[sfb*3+i] < 0) {
                maxrange = sfb < 6 ? maxrange1 : maxrange2;

                scalefac[sfb*3+i]
                    = -sf[sfb*3+i] / ifqstep + (-sf[sfb*3+i] % ifqstep != 0);

                if (scalefac[sfb*3+i] > maxrange)
                    scalefac[sfb*3+i] = maxrange;

                if (maxover < -(sf[sfb*3+i] + scalefac[sfb*3+i] * ifqstep))
                    maxover = -(sf[sfb*3+i] + scalefac[sfb*3+i] * ifqstep);
            }
        }
        scalefac[sfb*3+i] = 0;
    }

    return maxover;
}



/*
	  ifqstep = ( cod_info->scalefac_scale == 0 ) ? 2 : 4;
	  ol_sf =  (cod_info->global_gain-210.0);
	  ol_sf -= ifqstep*scalefac[gr][ch].l[sfb];
	  if (cod_info->preflag && sfb>=11) 
	  ol_sf -= ifqstep*pretab[sfb];
*/
inline int
compute_scalefacs_long_lsf(int *sf, const gr_info * cod_info, int *scalefac)
{
    const int *max_range = max_range_long;
    int     ifqstep = (cod_info->scalefac_scale == 0) ? 2 : 4;
    int     sfb;
    int     maxover;

    if (cod_info->preflag) {
        max_range = max_range_long_lsf_pretab;
        for (sfb = 11; sfb < SBPSY_l; ++sfb)
            sf[sfb] += pretab[sfb] * ifqstep;
    }

    maxover = 0;
    for (sfb = 0; sfb < SBPSY_l; ++sfb) {

        if (sf[sfb] < 0) {
            /* ifqstep*scalefac >= -sf[sfb], so round UP */
            scalefac[sfb] = -sf[sfb] / ifqstep + (-sf[sfb] % ifqstep != 0);
            if (scalefac[sfb] > max_range[sfb])
                scalefac[sfb] = max_range[sfb];

            /* sf[sfb] should now be positive: */
            if (-(sf[sfb] + scalefac[sfb] * ifqstep) > maxover) {
                maxover = -(sf[sfb] + scalefac[sfb] * ifqstep);
            }
        }
    }
    scalefac[sfb] = 0;  /* sfb21 */

    return maxover;
}





/*
	  ifqstep = ( cod_info->scalefac_scale == 0 ) ? 2 : 4;
	  ol_sf =  (cod_info->global_gain-210.0);
	  ol_sf -= ifqstep*scalefac[gr][ch].l[sfb];
	  if (cod_info->preflag && sfb>=11) 
	  ol_sf -= ifqstep*pretab[sfb];
*/
inline int
compute_scalefacs_long(int *sf, const gr_info * cod_info, int *scalefac)
{
    int     ifqstep = (cod_info->scalefac_scale == 0) ? 2 : 4;
    int     sfb;
    int     maxover;

    if (cod_info->preflag) {
        for (sfb = 11; sfb < SBPSY_l; ++sfb)
            sf[sfb] += pretab[sfb] * ifqstep;
    }

    maxover = 0;
    for (sfb = 0; sfb < SBPSY_l; ++sfb) {

        if (sf[sfb] < 0) {
            /* ifqstep*scalefac >= -sf[sfb], so round UP */
            scalefac[sfb] = -sf[sfb] / ifqstep + (-sf[sfb] % ifqstep != 0);
            if (scalefac[sfb] > max_range_long[sfb])
                scalefac[sfb] = max_range_long[sfb];

            /* sf[sfb] should now be positive: */
            if (-(sf[sfb] + scalefac[sfb] * ifqstep) > maxover) {
                maxover = -(sf[sfb] + scalefac[sfb] * ifqstep);
            }
        }
    }
    scalefac[sfb] = 0;  /* sfb21 */

    return maxover;
}








/***********************************************************************
 *
 *      calc_short_block_vbr_sf()
 *      calc_long_block_vbr_sf()
 *
 *  Mark Taylor 2000-??-??
 *  Robert Hegemann 2000-10-25 made functions of it
 *
 ***********************************************************************/
#define MAX_SF_DELTA 4

/* a variation for vbr-mtrh */
static void
block_sf(const lame_internal_flags * gfc, const FLOAT8 * l3_xmin,
	 const FLOAT8 * xr34_orig, int * vbrsf, gr_info *gi)
{
    int     sfb, j;
    int     scalefac_criteria;

    scalefac_criteria = gfc->VBR->quality;
    /* map quant_comp settings to internal selections */
    if (gi->block_type != NORM_TYPE)
	scalefac_criteria = gfc->gfp->quant_comp_short;

    j = 0;
    for (sfb = 0; sfb < gi->psymax; sfb++) {
        const int width = gi->width[sfb];
        switch (scalefac_criteria) {
        case 0:
            /*  the slower and better mode to use at higher quality
             */
            vbrsf[sfb] =
                find_scalefac_ave(&gi->xr[j], &xr34_orig[j], l3_xmin[sfb], width);
            break;
        case 1:
        case 2:
            /*  maxnoise mode to use at higher quality
             */
            vbrsf[sfb]
		= find_scalefac_mq(&gi->xr[j], &xr34_orig[j], l3_xmin[sfb],
				   width, 2 - scalefac_criteria);
            break;
        case 3:
            /*  the faster and sloppier mode to use at lower quality
             */
            vbrsf[sfb] = find_scalefac(&gi->xr[j], &xr34_orig[j], l3_xmin[sfb], width);
            break;
        case 4:
        default:
            /*  the fastest
             */
            vbrsf[sfb] =
                calc_scalefac(l3_xmin[sfb], width, gfc->presetTune.quantcomp_adjust_mtrh);
            break;
        }
	j += width;
    }
}


static void
short_block_sf(const lame_internal_flags * gfc, const FLOAT8 * l3_xmin,
               int * vbrsf, int *vbrmin, int *vbrmax, gr_info *gi)
{
    int     sfb, b, i;
    int     vbrmean, vbrmn, vbrmx, vbrclip;
    int     sf_cache[SBMAX_s];

    *vbrmax = -10000;
    *vbrmin = +10000;

    for (b = 0; b < 3; ++b) {
        /*  smoothing
         */
        switch (gfc->VBR->smooth) {
        default:
        case 0:
            /*  get max value
             */
            for (sfb = b + gi->sfb_lmax; sfb < gi->sfbmax; sfb += 3) {
                if (*vbrmax < vbrsf[sfb])
                    *vbrmax = vbrsf[sfb];
                if (*vbrmin > vbrsf[sfb])
                    *vbrmin = vbrsf[sfb];
            }
            break;

        case 1:
            /*  make working copy, get min value, select_kth_int will reorder!
             */
            vbrmn = +1000;
            vbrmx = -1000;
	    i = 0;
            for (sfb = b + gi->sfb_lmax; sfb < gi->sfbmax; sfb += 3) {
                sf_cache[i++] = vbrsf[sfb];
                if (vbrmn > vbrsf[sfb])
                    vbrmn = vbrsf[sfb];
                if (vbrmx < vbrsf[sfb])
                    vbrmx = vbrsf[sfb];
            }
            if (*vbrmin > vbrmn)
                *vbrmin = vbrmn;

            /*  find median value, take it as mean 
             */
            vbrmean = select_kth_int(sf_cache, i, (i + 1) / 2u);

            /*  cut peaks
             */
            vbrclip = vbrmean + (vbrmean - vbrmn);
            for (sfb = b + gi->sfb_lmax; sfb < gi->sfbmax; sfb += 3) {
                if (vbrsf[sfb] > vbrclip)
                    vbrsf[sfb] = vbrclip;
            }
            if (vbrmx > vbrclip)
                vbrmx = vbrclip;
            if (*vbrmax < vbrmx)
                *vbrmax = vbrmx;
            break;

        case 2:
            vbrclip = vbrsf[gi->sfb_lmax+b+3] + MAX_SF_DELTA;
            if (vbrsf[gi->sfb_lmax+b] > vbrclip)
                vbrsf[gi->sfb_lmax+b] = vbrclip;
            if (*vbrmax < vbrsf[gi->sfb_lmax+b])
                *vbrmax = vbrsf[gi->sfb_lmax+b];
            if (*vbrmin > vbrsf[gi->sfb_lmax+b])
                *vbrmin = vbrsf[gi->sfb_lmax+b];
            for (sfb = gi->sfb_lmax+3+b; sfb < gi->sfbmax-3; sfb += 3) {
                vbrclip = vbrsf[sfb-3] + MAX_SF_DELTA;
                if (vbrsf[sfb] > vbrclip)
                    vbrsf[sfb] = vbrclip;
                vbrclip = vbrsf[sfb + 3] + MAX_SF_DELTA;
                if (vbrsf[sfb] > vbrclip)
                    vbrsf[sfb] = vbrclip;
                if (*vbrmax < vbrsf[sfb])
                    *vbrmax = vbrsf[sfb];
                if (*vbrmin > vbrsf[sfb])
                    *vbrmin = vbrsf[sfb];
            }
            vbrclip = vbrsf[sfb - 3] + MAX_SF_DELTA;
            if (vbrsf[sfb] > vbrclip)
                vbrsf[sfb] = vbrclip;
            if (*vbrmax < vbrsf[sfb])
                *vbrmax = vbrsf[sfb];
            if (*vbrmin > vbrsf[sfb])
                *vbrmin = vbrsf[sfb];
            break;
        }
    }
}


static void
long_block_sf(const lame_internal_flags * gfc, const FLOAT8 * l3_xmin,
              int * vbrsf, int *vbrmin, int *vbrmax, gr_info *gi)
{
    int     sfb;
    int     vbrmean, vbrmn, vbrmx, vbrclip;
    int     sf_cache[SBMAX_l];

    switch (gfc->VBR->smooth) {
    default:
    case 0:
        /*  get max value
         */
        *vbrmin = *vbrmax = vbrsf[0];
        for (sfb = 1; sfb < gi->sfb_lmax; ++sfb) {
            if (*vbrmax < vbrsf[sfb])
                *vbrmax = vbrsf[sfb];
            if (*vbrmin > vbrsf[sfb])
                *vbrmin = vbrsf[sfb];
        }
        break;

    case 1:
        /*  make working copy, get min value, select_kth_int will reorder!
         */
	vbrmn = +1000;
	vbrmx = -1000;
        for (sfb = 0; sfb < gi->sfb_lmax; ++sfb) {
            sf_cache[sfb] = vbrsf[sfb];
            if (vbrmn > vbrsf[sfb])
                vbrmn = vbrsf[sfb];
            if (vbrmx < vbrsf[sfb])
                vbrmx = vbrsf[sfb];
        }
        /*  find median value, take it as mean 
         */
        vbrmean = select_kth_int(sf_cache, gi->sfb_lmax, (gi->sfb_lmax+1) / 2);

        /*  cut peaks
         */
        vbrclip = vbrmean + (vbrmean - vbrmn);
        for (sfb = 0; sfb < gi->sfb_lmax; ++sfb) {
            if (vbrsf[sfb] > vbrclip)
                vbrsf[sfb] = vbrclip;
        }
        if (vbrmx > vbrclip)
            vbrmx = vbrclip;
        *vbrmin = vbrmn;
        *vbrmax = vbrmx;
        break;

    case 2:
        vbrclip = vbrsf[1] + MAX_SF_DELTA;
        if (vbrsf[0] > vbrclip)
            vbrsf[0] = vbrclip;
        *vbrmin = *vbrmax = vbrsf[0];
        for (sfb = 1; sfb < gi->sfb_lmax - 1; ++sfb) {
            vbrclip = vbrsf[sfb - 1] + MAX_SF_DELTA;
            if (vbrsf[sfb] > vbrclip)
                vbrsf[sfb] = vbrclip;
            vbrclip = vbrsf[sfb + 1] + MAX_SF_DELTA;
            if (vbrsf[sfb] > vbrclip)
                vbrsf[sfb] = vbrclip;
            if (*vbrmax < vbrsf[sfb])
                *vbrmax = vbrsf[sfb];
            if (*vbrmin > vbrsf[sfb])
                *vbrmin = vbrsf[sfb];
        }
        vbrclip = vbrsf[sfb-1] + MAX_SF_DELTA;
        if (vbrsf[sfb] > vbrclip)
            vbrsf[sfb] = vbrclip;
        if (*vbrmax < vbrsf[sfb])
            *vbrmax = vbrsf[sfb];
        if (*vbrmin > vbrsf[sfb])
            *vbrmin = vbrsf[sfb];
        break;
    }
}



/******************************************************************
 *
 *  short block scalefacs
 *
 ******************************************************************/

static void
short_block_scalefacs(const lame_internal_flags * gfc, gr_info * cod_info,
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
    for (sfb = 0; sfb < cod_info->psymax; ++sfb) {
	v0 = (vbrmax - vbrsf[sfb]) - (4 * 14 + 2 * max_range_short[sfb]);
	v1 = (vbrmax - vbrsf[sfb]) - (4 * 14 + 4 * max_range_short[sfb]);
	if (maxover0 < v0)
	    maxover0 = v0;
	if (maxover1 < v1)
	    maxover1 = v1;
    }

    if (gfc->noise_shaping == 2)
        /* allow scalefac_scale=1 */
        mover = Min(maxover0, maxover1);
    else
        mover = maxover0;

    vbrmax -= mover;
    maxover0 -= mover;
    maxover1 -= mover;

    if (maxover0 == 0)
        cod_info->scalefac_scale = 0;
    else if (maxover1 == 0)
        cod_info->scalefac_scale = 1;

    cod_info->global_gain = vbrmax;

    if (cod_info->global_gain < 0) {
        cod_info->global_gain = 0;
    }
    else if (cod_info->global_gain > 255) {
        cod_info->global_gain = 255;
    }
    for (sfb = 0; sfb < SBMAX_s; ++sfb) {
        for (b = 0; b < 3; ++b) {
            vbrsf[sfb*3+b] -= vbrmax;
        }
    }
    assert(cod_info->global_gain < 256);
    maxover =
        compute_scalefacs_short(vbrsf, cod_info, cod_info->scalefac, cod_info->subblock_gain);

    assert(maxover <= 0);

    /* adjust global_gain so at least 1 subblock gain = 0 */
    minsfb = 999;       /* prepare for minimum search */
    for (b = 0; b < 3; ++b)
        if (minsfb > cod_info->subblock_gain[b])
            minsfb = cod_info->subblock_gain[b];

    if (minsfb > cod_info->global_gain / 8)
        minsfb = cod_info->global_gain / 8;

    vbrmax -= 8 * minsfb;
    cod_info->global_gain -= 8 * minsfb;

    for (b = 0; b < 3; ++b)
        cod_info->subblock_gain[b] -= minsfb;

    *VBRmax = vbrmax;
}



/******************************************************************
 *
 *  long block scalefacs
 *
 ******************************************************************/

static void
long_block_scalefacs(const lame_internal_flags * gfc, gr_info * cod_info,
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

    for (sfb = 0; sfb < cod_info->psymax; ++sfb) {
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
    if (gfc->noise_shaping == 2) {
        /* allow scalefac_scale=1 */
        mover = Min(mover, maxover1);
        mover = Min(mover, maxover1p);
    }

    vbrmax -= mover;
    maxover0 -= mover;
    maxover0p -= mover;
    maxover1 -= mover;
    maxover1p -= mover;

    if (maxover0 <= 0) {
        cod_info->scalefac_scale = 0;
        cod_info->preflag = 0;
        vbrmax -= maxover0;
    }
    else if (maxover0p <= 0) {
        cod_info->scalefac_scale = 0;
        cod_info->preflag = 1;
        vbrmax -= maxover0p;
    }
    else if (maxover1 == 0) {
        cod_info->scalefac_scale = 1;
        cod_info->preflag = 0;
    }
    else if (maxover1p == 0) {
        cod_info->scalefac_scale = 1;
        cod_info->preflag = 1;
    }
    else {
        assert(0);      /* this should not happen */
    }

    cod_info->global_gain = vbrmax;
    if (cod_info->global_gain < 0) {
        cod_info->global_gain = 0;
    }
    else if (cod_info->global_gain > 255)
        cod_info->global_gain = 255;

    for (sfb = 0; sfb < SBMAX_l; ++sfb)
        vbrsf[sfb] -= vbrmax;

    if (gfc->mode_gr == 2)
        maxover = compute_scalefacs_long(vbrsf, cod_info, cod_info->scalefac);
    else
        maxover = compute_scalefacs_long_lsf(vbrsf, cod_info, cod_info->scalefac);

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
block_xr34(const lame_internal_flags * gfc, const gr_info * cod_info,
	   const FLOAT8 * xr34_orig, FLOAT8 * xr34)
{
    int     sfb, j = 0, sfbmax, *scalefac = cod_info->scalefac;

    /* even though there is no scalefactor for sfb12/sfb21
     * subblock gain affects upper frequencies too, that's why
     * we have to go up to SBMAX_s/SBMAX_l
     */
    sfbmax = cod_info->psymax;
    if (sfbmax == 35 || sfbmax == 36) /* short block / mixed? case */
	sfbmax += 3;
    if (sfbmax == 21) /* long block case */
        sfbmax += 1;

    for (sfb = 0; sfb < sfbmax; ++sfb) {
	FLOAT8 fac;
	int s =
	    (((*scalefac++) + (cod_info->preflag ? pretab[sfb] : 0))
	     << (cod_info->scalefac_scale + 1))
	    + cod_info->subblock_gain[cod_info->window[sfb]] * 8;
	int l = cod_info->width[sfb];

	if (s == 0) {/* just copy */
	    memcpy(&xr34[j], &xr34_orig[j], sizeof(FLOAT8)*l);
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
VBR_noise_shaping(lame_internal_flags * gfc, FLOAT8 * xr34orig, int minbits, int maxbits,
                  FLOAT8 * l3_xmin, int gr, int ch)
{
    int vbrsf[SFBMAX];
    int vbrsf2[SFBMAX];
    gr_info *cod_info;
    FLOAT8  xr34[576];
    int     shortblock, ret;
    int     vbrmin, vbrmax, vbrmin2, vbrmax2;
    int     M = 6;
    int     count = M;

    cod_info = &gfc->l3_side.tt[gr][ch];
    shortblock = (cod_info->block_type == SHORT_TYPE);

    block_sf(gfc, l3_xmin, xr34orig, vbrsf2, cod_info);

    if (shortblock) {
        short_block_sf(gfc, l3_xmin, vbrsf2, &vbrmin2, &vbrmax2, cod_info);
    }
    else {
        long_block_sf(gfc, l3_xmin, vbrsf2, &vbrmin2, &vbrmax2, cod_info);
    }
    memcpy(vbrsf, vbrsf2, sizeof(vbrsf));
    vbrmin = vbrmin2;
    vbrmax = vbrmax2;

    M = (vbrmax - vbrmin) / 2;
    if ( M > 16 ) M = 16;
    if ( M <  1 ) M = 1;
    count = M;

    do {
        --count;

        if (shortblock) {
            short_block_scalefacs(gfc, cod_info, vbrsf, &vbrmax);
        }
        else {
            long_block_scalefacs(gfc, cod_info, vbrsf, &vbrmax);
        }

	/* encode scalefacs */
	if (gfc->mode_gr == 2)
	    ret = scale_bitcount(cod_info);
	else
	    ret = scale_bitcount_lsf(gfc, cod_info);

	if (ret != 0) {
	    ret = -1;
	} else {
	    /* quantize xr34 */
	    block_xr34(gfc, cod_info, xr34orig, xr34);
	    cod_info->part2_3_length = count_bits(gfc, xr34, cod_info);

	    if (cod_info->part2_3_length >= LARGE_BITS)
		ret = -2;
	    else if (gfc->use_best_huffman == 2)
		best_huffman_divide(gfc, cod_info);
	}

        if (vbrmin == vbrmax)
            break;
        else if (cod_info->part2_3_length < minbits) {
            int     i;
            vbrmax = vbrmin2 + (vbrmax2 - vbrmin2) * count / M;
            vbrmin = vbrmin2;
	    for (i = 0; i < cod_info->psymax; ++i) {
		vbrsf[i] = vbrmin2 + (vbrsf2[i] - vbrmin2) * count / M;
            }
        }
        else if (cod_info->part2_3_length > maxbits) {
            int     i;
            vbrmax = vbrmax2;
            vbrmin = vbrmax2 + (vbrmin2 - vbrmax2) * count / M;
	    for (i = 0; i < cod_info->psymax; ++i) {
		vbrsf[i] = vbrmax2 + (vbrsf2[i] - vbrmax2) * count / M;
            }
        }
        else
            break;
    } while (ret != -1);

    if (ret == -1)      /* Houston, we have a problem */
        return -1;

    if (gfc->substep_shaping & 2) {
	FLOAT8 xrtmp[576];
	trancate_smallspectrums(gfc, cod_info, l3_xmin, xrtmp);
    }

    if (cod_info->part2_3_length < minbits - cod_info->part2_length) {
        bin_search_StepSize (gfc, cod_info, minbits, ch, xr34);
    }
    if (cod_info->part2_3_length > maxbits - cod_info->part2_length) {
	bin_search_StepSize (gfc, cod_info, maxbits, ch, xr34);
    }
    assert (cod_info->global_gain < 256u);

    if (cod_info->part2_3_length + cod_info->part2_length >= LARGE_BITS) 
	return -2; /* Houston, we have a problem */

    return 0;
}

