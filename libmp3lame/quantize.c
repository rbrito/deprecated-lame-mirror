/*
 * MP3 quantization
 *
 * Copyright (c) 1999 Mark Taylor
 *               2003 Takehiro Tominaga
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
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

#include <math.h>
#include <assert.h>
#include <stdlib.h>
#include "bitstream.h"
#include "quantize.h"
#include "reservoir.h"
#include "quantize_pvt.h"
#include "machine.h"
#include "tables.h"

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

static const int max_range_short[SBMAX_s*3] = {
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    0, 0, 0 };

static const int max_range_long[SBMAX_l] = {
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 0 };

static const int max_range_long_lsf_pretab[SBMAX_l] = {
    7, 7, 7, 7, 7, 7, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/************************************************************************
 * allocate bits among 2 channels based on PE
 * mt 6/99
 * bugfixes rh 8/01: often allocated more than the allowed 4095 bits
 ************************************************************************/
static void
on_pe(
    lame_internal_flags *gfc,
    III_psy_ratio      ratio[2],
    int targ_bits[2],
    int mean_bits
    )
{
    int     extra_bits, tbits, bits;
    int     max_bits;  /* maximum allowed bits for this granule */
    int     ch;

    /* allocate targ_bits for granule */
    ResvMaxBits( gfc, mean_bits, &tbits, &extra_bits);

    max_bits = tbits + extra_bits;
    if (max_bits > MAX_BITS) /* hard limit per granule */
        max_bits = MAX_BITS;

    /* at most increase bits by 1.5*average */
    mean_bits = tbits+mean_bits*3/2;
    tbits /= gfc->channels_out;
    mean_bits /= gfc->channels_out;

    for ( bits = 0, ch = 0; ch < gfc->channels_out; ++ch ) {
	if (ratio[ch].ath_over == 0) {
	    targ_bits[ch] = 126;
	} else {
	    targ_bits[ch] = tbits;
	    if (ratio[ch].pe*(2.0/3) > tbits) {
		targ_bits[ch] = tbits * (ratio[ch].pe*(2.0/3)-tbits);
		if (targ_bits[ch] > mean_bits) 
		    targ_bits[ch] = mean_bits;
	    }
	}
	bits += targ_bits[ch];
    }
    if (bits > max_bits) {
        for ( ch = 0; ch < gfc->channels_out; ++ch ) {
	    targ_bits[ch]
		= tbits + extra_bits * (targ_bits[ch] - tbits) / bits;
	}
    }
}




/*************************************************************************/
/*            calc_xmin                                                  */
/*************************************************************************/

/**
 *  Robert Hegemann 2001-04-27:
 *  this adjusts the ATH, keeping the original noise floor
 *  affects the higher frequencies more than the lower ones
 */

static FLOAT athAdjust( FLOAT a, FLOAT x, FLOAT athFloor )
{
    /*  work in progress
     */
#define o 90.30873362
#define p 94.82444863
    FLOAT u = FAST_LOG10_X(x, 10.0); 
    FLOAT v = a*a;
    FLOAT w = 0.0;
    u -= athFloor;                                  /* undo scaling */
    if ( v > 1E-20 ) w = 1. + FAST_LOG10_X(v, 10.0 / o);
    if ( w < 0  )    w = 0.; 
    u *= w; 
    u += athFloor + o-p;                            /* redo scaling */
#undef o
#undef p
    return db2pow(u);
}

/*
  Calculate the allowed distortion for each scalefactor band,
  as determined by the psychoacoustic model.
  xmin(sb) = ratio(sb) * en(sb) / bw(sb)
*/
static void
calc_xmin(
    lame_global_flags *gfp,
    const III_psy_ratio * const ratio,
    const gr_info       * const gi, 
	   FLOAT        * pxmin
    )
{
    lame_internal_flags *gfc = gfp->internal_flags;
    int sfb, gsfb, j=0;

    if (gfp->ATHonly) {
	for (gsfb = 0; gsfb < gi->psy_lmax; gsfb++)
	    pxmin[gsfb]
		= athAdjust(gfc->ATH.adjust, gfc->ATH.l[gsfb], gfc->ATH.floor);

	for (sfb = gi->sfb_smin; gsfb < gi->psymax; sfb++, gsfb += 3)
	    pxmin[gsfb] = pxmin[gsfb+1] = pxmin[gsfb+2]
		= athAdjust(gfc->ATH.adjust, gfc->ATH.s[sfb], gfc->ATH.floor);

	return;
    }

    for (gsfb = 0; gsfb < gi->psy_lmax; gsfb++) {
	FLOAT en0, xmin, x;
	int width, l;
	xmin = athAdjust(gfc->ATH.adjust, gfc->ATH.l[gsfb], gfc->ATH.floor);

	width = gi->width[gsfb];
	l = width >> 1;
	en0 = 0.0;
	do {
	    en0 += gi->xr[j] * gi->xr[j]; j++;
	    en0 += gi->xr[j] * gi->xr[j]; j++;
	} while (--l > 0);

	x = ratio->en.l[gsfb];
	if (x > 0.0) {
	    x = en0 * ratio->thm.l[gsfb] / x;
	    if (xmin < x)
		xmin = x;
	}
	*pxmin++ = xmin;
    }   /* end of long block loop */

    for (sfb = gi->sfb_smin; gsfb < gi->psymax; sfb++, gsfb += 3) {
	int width, b;
	FLOAT tmpATH
	    = athAdjust(gfc->ATH.adjust, gfc->ATH.s[sfb], gfc->ATH.floor);

	width = gi->width[gsfb];
	for ( b = 0; b < 3; b++ ) {
	    FLOAT en0 = 0.0, xmin;
	    int l = width >> 1;
	    do {
		en0 += gi->xr[j] * gi->xr[j]; j++;
		en0 += gi->xr[j] * gi->xr[j]; j++;
	    } while (--l > 0);

	    xmin = ratio->en.s[sfb][b];
	    if (xmin > 0.0)
		xmin = en0 * ratio->thm.s[sfb][b] / xmin;
	    if (xmin < tmpATH)
		xmin = tmpATH;
	    *pxmin++ = xmin;
	}   /* b */
    }   /* end of short block sfb loop */
}

/*************************************************************************/
/*            calc_noise                                                 */
/*************************************************************************/
/*  mt 5/99:  Function: Improved calc_noise for a single channel   */
static void
calc_noise( 
        const gr_info             * const gi,
        const FLOAT              * l3_xmin, 
              FLOAT              * distort,
              calc_noise_result   * const res )
{
    FLOAT over_noise_db = 0.0;
    FLOAT max_noise   = -20.0; /* -200 dB relative to masking */
    int sfb = 0, j = 0;

    do {
	FLOAT step
	    = POW20(gi->global_gain
		    - ((gi->scalefac[sfb] + (gi->preflag>0 ? pretab[sfb] : 0))
		       << (gi->scalefac_scale + 1))
		    - gi->subblock_gain[gi->window[sfb]] * 8);
	FLOAT noise = 0.0;
	int l = gi->width[sfb] >> 1;
	do {
	    FLOAT temp;
	    temp = fabs(gi->xr[j]) - pow43[gi->l3_enc[j]] * step; j++;
	    noise += temp * temp;
	    temp = fabs(gi->xr[j]) - pow43[gi->l3_enc[j]] * step; j++;
	    noise += temp * temp;
	} while (--l > 0);
	noise = *distort++ = noise / *l3_xmin++;

	noise = FAST_LOG10(Max(noise,1E-20));
	if (noise > 0.0) {
	    /* multiplying here is adding in dB -but can overflow */
	    /* over_noise *= noise; */
	    over_noise_db += noise;
	}
	max_noise=Max(max_noise,noise);
    } while (++sfb < gi->psymax);

    if (max_noise > 0.0 && gi->block_type == SHORT_TYPE) {
	distort -= sfb;
	max_noise = -20.0;
	over_noise_db = 0.0;
	for (sfb = gi->sfb_smin; sfb < gi->psymax; sfb += 3) {
	    FLOAT noise = 0.0, nsum = 0.0, subnoise;
	    subnoise = FAST_LOG10(distort[sfb]);
	    if (distort[sfb] > 1.0)
		noise += subnoise;
	    nsum += subnoise;

	    subnoise = FAST_LOG10(distort[sfb+1]);
	    if (distort[sfb+1] > 1.0)
		noise += subnoise;
	    nsum += subnoise;

	    subnoise = FAST_LOG10(distort[sfb+2]);
	    if (distort[sfb+2] > 1.0)
		noise += subnoise;
	    nsum += subnoise;

	    if (max_noise < nsum)
		max_noise = nsum;
	    if (noise > 0)
		over_noise_db += noise;
	}
    }
    res->over_noise  = over_noise_db;
    res->max_noise   = max_noise;
}

/************************************************************************
 *
 *      init_outer_loop()
 *  mt 6/99                                    
 *
 *  initializes xrpow
 *
 *  returns 0 if all energies in xr are zero, else 1                    
 *
 ************************************************************************/

static int 
init_outer_loop(
    lame_internal_flags *gfc,
    gr_info *const gi, 
    FLOAT xrpow[576] )
{
    FLOAT tmp, sum = 0;
    int i;

    /*  check if there is some energy we have to quantize
     *  and calculate xrpow matching our fresh scalefactors
     */
    for (i = 0; i < 576; ++i) {
	tmp = fabs (gi->xr[i]);
	sum += tmp;
	xrpow[i] = sqrt (tmp * sqrt(tmp));
    }
    /*  return 1 if we have something to quantize, else 0
     */
    if (sum > (FLOAT)1E-20) {
	int j = 0;
	if (gfc->substep_shaping & 2)
	    j = 1;

	for (i = 0; i < gi->psymax; i++)
	    gfc->pseudohalf[i] = j;
	return 1;
    }

    memset(&gi->l3_enc, 0, sizeof(int)*576);
    return 0;
}

/************************************************************************
 *
 *      bin_search_StepSize()
 *
 *  author/date??
 *
 *  binary step size search
 *  used by outer_loop to get a quantizer step size to start with
 *
 ************************************************************************/

typedef enum {
    BINSEARCH_NONE,
    BINSEARCH_UP, 
    BINSEARCH_DOWN
} binsearchDirection_t;

static int 
bin_search_StepSize(
          lame_internal_flags * const gfc,
          gr_info * const gi,
	  int             desired_rate,
	  int CurrentStep,
	  int start,
    const FLOAT          xrpow [576] ) 
{
    int nBits;
    int flag_GoneOver = 0;
    binsearchDirection_t Direction = BINSEARCH_NONE;
    gi->global_gain = start;
    desired_rate -= gi->part2_length;

    assert(CurrentStep);
    do {
	int step;
        nBits = count_bits(gfc, xrpow, gi);  

        if (CurrentStep == 1 || nBits == desired_rate)
	    break; /* nothing to adjust anymore */

        if (nBits > desired_rate) {  
            /* increase Quantize_StepSize */
            if (Direction == BINSEARCH_DOWN)
                flag_GoneOver = 1;

	    if (flag_GoneOver) CurrentStep /= 2;
            Direction = BINSEARCH_UP;
	    step = CurrentStep;
        } else {
            /* decrease Quantize_StepSize */
            if (Direction == BINSEARCH_UP)
                flag_GoneOver = 1;

	    if (flag_GoneOver) CurrentStep /= 2;
            Direction = BINSEARCH_DOWN;
	    step = -CurrentStep;
        }
	gi->global_gain += step;
    } while (gi->global_gain < 256u);

    if (gi->global_gain < 0) {
	gi->global_gain = 0;
	nBits = count_bits(gfc, xrpow, gi);
    } else if (gi->global_gain > 255) {
	gi->global_gain = 255;
	nBits = count_bits(gfc, xrpow, gi);
    } else if (nBits > desired_rate) {
	gi->global_gain++;
	nBits = count_bits(gfc, xrpow, gi);
    }
    gi->part2_3_length = nBits;
    return nBits;
}




/************************************************************************
 *
 *      trancate_smallspectrums()
 *
 *  Takehiro TOMINAGA 2002-07-21
 *
 *  trancate smaller nubmers into 0 as long as the noise threshold is allowed.
 *
 ************************************************************************/
static int
floatcompare(const FLOAT *a, const FLOAT *b)
{
    if (*a > *b) return 1;
    if (*a == *b) return 0;
    return -1;
}

static void
trancate_smallspectrums(
    lame_internal_flags *gfc,
    gr_info		* const gi,
    const FLOAT	* const l3_xmin,
    FLOAT		* work
    )
{
    int sfb, j, width;
    FLOAT distort[SFBMAX];
    calc_noise_result dummy;

    if ((!(gfc->substep_shaping & 4) && gi->block_type == SHORT_TYPE)
	|| gfc->substep_shaping & 0x80)
	return;
    calc_noise (gi, l3_xmin, distort, &dummy);
    for (j = 0; j < 576; j++) {
	FLOAT xr = 0.0;
	if (gi->l3_enc[j] != 0)
	    xr = fabs(gi->xr[j]);
	work[j] = xr;
    }

    j = 0;
    sfb = 8;
    if (gi->block_type == SHORT_TYPE)
	sfb = 6;
    do {
	FLOAT allowedNoise, trancateThreshold;
	int nsame, start;

	width = gi->width[sfb];
	j += width;
	if (distort[sfb] >= 1.0)
	    continue;

	qsort(&work[j-width], width, sizeof(FLOAT), floatcompare);
	if (work[j - 1] == 0.0)
	    continue; /* all zero sfb */

	allowedNoise = (1.0 - distort[sfb]) * l3_xmin[sfb];
	trancateThreshold = 0.0;
	start = 0;
	do {
	    FLOAT noise;
	    for (nsame = 1; start + nsame < width; nsame++)
		if (work[start + j-width] != work[start+j+nsame-width])
		    break;

	    noise = work[start+j-width] * work[start+j-width] * nsame;
	    if (allowedNoise < noise) {
		if (start != 0)
		    trancateThreshold = work[start+j-width - 1];
		break;
	    }
	    allowedNoise -= noise;
	    start += nsame;
	} while (start < width);
	if (trancateThreshold == 0.0)
	    continue;

	do {
	    if (fabs(gi->xr[j - width]) <= trancateThreshold)
		gi->l3_enc[j - width] = 0;
	} while (--width > 0);
    } while (++sfb < gi->psymax);

    gi->part2_3_length = noquant_count_bits(gfc, gi);
}


/*************************************************************************
 *
 *      loop_break()                                               
 *
 *  author/date??
 *
 *  Function: Returns zero if there is a scalefac which has not been
 *            amplified. Otherwise it returns one. 
 *
 *************************************************************************/

inline static int
loop_break(
    const gr_info * const gi
    )
{
    int sfb;
    for (sfb = 0; sfb < gi->sfbmax; sfb++)
        if (gi->scalefac[sfb] + gi->subblock_gain[gi->window[sfb]] == 0)
            return 0;

    return sfb;
}




/*************************************************************************
 *
 *      better_quant()
 *
 *  author/date??
 *
 *  several different codes to decide which quantization is better
 *
 *************************************************************************/

inline static int 
better_quant(
    lame_internal_flags	* const gfc,
    const FLOAT         * l3_xmin, 
    FLOAT               * distort,
    calc_noise_result   * const best,
    const gr_info	* const gi
    )
{
    calc_noise_result calc;
    int better;
    FLOAT new;
    /*
       noise is given in decibels (dB) relative to masking thesholds.

       over_noise:  sum of noise which exceed the threshold.
       max_noise:   max quantization noise 
     */

    calc_noise (gi, l3_xmin, distort, &calc);
    new = calc.max_noise;
    if (gi->block_type == SHORT_TYPE
	? gfc->quantcomp_method_s : gfc->quantcomp_method) {

	new = calc.over_noise;
	calc.max_noise = new;
    }

    better = new < best->max_noise;
    if (better)
	*best = calc;
    return better;
}



/*************************************************************************
 *
 *      inc_scalefac_scale()
 *
 *  Takehiro Tominaga 2000-xx-xx
 *
 *  turns on scalefac scale and adjusts scalefactors
 *
 *************************************************************************/
 
static void
inc_scalefac_scale (
    gr_info        * const gi
    )
{
    int sfb;
    for (sfb = 0; sfb < gi->sfbmax; sfb++) {
	int s = gi->scalefac[sfb];
	if (gi->preflag > 0)
	    s += pretab[sfb];
	gi->scalefac[sfb] = (s + 1) >> 1;
    }
    gi->preflag = 0;
    gi->scalefac_scale = 1;
}



/*************************************************************************
 *
 *      inc_subblock_gain()
 *
 *  Takehiro Tominaga 2000-xx-xx
 *
 *  increases the subblock gain and adjusts scalefactors
 *
 *************************************************************************/

static int 
inc_subblock_gain (
          gr_info        * const gi
    )
{
    int sfb, window;
    const int *tab = max_range_short;
    if (gi->sfb_lmax) {	/* mixed_block. */
	/* subbloc_gain can't do anything in the long block region */
	for (sfb = 0; sfb < gi->sfb_lmax; sfb++)
	    if (gi->scalefac[sfb] >= 16)
		return 1;
	tab--;
    }

    for (window = 0; window < 3; window++) {
	for (sfb = gi->sfb_lmax+window; sfb < gi->sfbmax; sfb += 3)
	    if (gi->scalefac[sfb] > tab[sfb])
		break;

	if (sfb >= gi->sfbmax)
	    continue;

        if (gi->subblock_gain[window] >= 7)
	    return 1;

	gi->subblock_gain[window]++;
	for (sfb = gi->sfb_lmax+window; sfb < gi->sfbmax; sfb += 3) {
	    int s = gi->scalefac[sfb] - (4 >> gi->scalefac_scale);
	    if (s < 0)
		s = 0;
	    gi->scalefac[sfb] = s;
        }
    }
    return 0;
}



/*************************************************************************
 *
 *          amp_scalefac_bands() 
 *
 *  Change the scalefactor value of the band where the noise is not masked.
 *  See ISO 11172-3 Section C.1.5.4.3.5
 * 
 *  We select "the band" if the distort[] is larger than "trigger",
 *  which has 3 algorithms to be calculated.
 *  
 *  method
 *    0             trigger = 1.0
 *
 *    1             trigger = max_dist^(.5)   (50% in the dB scale)
 *
 *    2             trigger = max_dist;
 *                  (select only the band with the strongest distortion)
 * where
 *  distort[] = noise/masking
 *   distort[] > 1   ==> noise is not masked (need more bits)
 *   distort[] < 1   ==> noise is masked
 *  max_dist = maximum value of distort[]
 *
 *  For algorithms 0 and 1, if max_dist < 1, then change all bands 
 *  with distort[] >= .95*max_dist.  This is to make sure we always
 *  change at least one band.
 *
 *  returns how many bits are available to store the quantized spectrum.
 *************************************************************************/
static int
amp_scalefac_bands(
    lame_internal_flags *gfc,
    gr_info  *const gi, 
    FLOAT *distort,
    int method,
    int target_bits
    )
{
    int bits, sfb;
    FLOAT trigger = 0.0;

    /* compute maximum value of distort[]  */
    for (sfb = 0; sfb < gi->sfbmax; sfb++) {
	if (trigger < distort[sfb])
	    trigger = distort[sfb];
    }

    if (method < 2) {
	if (trigger <= 1.0)
	    trigger *= 0.95;
	else if (method == 0)
	    trigger = 1.0;
	else
	    trigger = sqrt(trigger);
    }

    for (sfb = 0; sfb < gi->sfbmax; sfb++) {
	if (distort[sfb] < trigger)
	    continue;

	if (method < 3 || (gfc->pseudohalf[sfb] ^= 1))
	    gi->scalefac[sfb]++;
	if (method >= 2)
	    break;
    }

    if (loop_break(gi))
	return 0; /* all bands are already changed -> fail */

    /* not all scalefactors have been amplified.  so these 
     * scalefacs are possibly valid.  encode them: 
     */
    bits = target_bits - gfc->scale_bitcounter(gi);
    if (bits > 0)
	return bits; /* Ok, we will go with this scalefactor combination */

    /* some scalefactors are increased too much
     *      -> try to use scalefac_scale or subblock_gain.
     */
    if (gfc->use_subblock_gain && gi->block_type == SHORT_TYPE) {
	if (inc_subblock_gain(gi) || loop_break(gi))
	    return 0; /* failed */
    } else if (gfc->use_scalefac_scale && !gi->scalefac_scale)
	inc_scalefac_scale (gi);
    else
	return 0;

    return target_bits - gfc->scale_bitcounter(gi);
}



/************************************************************************
 *
 *  outer_loop ()                                                       
 *
 *  Function: The outer iteration loop controls the masking conditions
 *  of all scalefactor bands. It computes the best scalefac and global gain.
 * 
 *  mt 5/99 completely rewritten to allow for bit reservoir control,   
 *  mid/side channels with L/R or mid/side masking thresholds, 
 *  and chooses best quantization instead of last quantization when 
 *  no distortion free quantization can be found.  
 *  
 *  some code shuffle rh 9/00
 ************************************************************************/

static void
outer_loop (
    lame_global_flags	*gfp,
    gr_info		* const gi,
    const int           ch,
    const int           targ_bits,  /* maximum allowed bits */
    const III_psy_ratio * const ratio
    )
{
    lame_internal_flags *gfc=gfp->internal_flags;
    calc_noise_result best_noise_info;
    gr_info cod_info_w;
    FLOAT distort[SFBMAX], l3_xmin[SFBMAX], xrpow[576];
    int current_method, age;

    if (!init_outer_loop(gfc, gi, xrpow))
	return; /* digital silence */

    bin_search_StepSize (gfc, gi, targ_bits,
			 gfc->CurrentStep[ch], gfc->OldValue[ch], xrpow);

    gfc->CurrentStep[ch]
	= (gfc->OldValue[ch] - gi->global_gain) >= 4 ? 4 : 2;
    gfc->OldValue[ch] = gi->global_gain;

    if (gfc->psymodel < 2) 
	/* fast mode, no noise shaping, we are ready */
	return;

    /* compute the distortion in this quantization */
    /* coefficients and thresholds of ch0(L or Mid) or ch1(R or Side) */
    calc_xmin (gfp, ratio, gi, l3_xmin);
    calc_noise (gi, l3_xmin, distort, &best_noise_info);
    if (gfc->noise_shaping_stop == 0 && best_noise_info.max_noise < 0.0)
	goto quit_quantization;

    /* BEGIN MAIN LOOP */
    cod_info_w = *gi;
    current_method = 0;
    age = 3;
    for (;;) {
	/* try the new scalefactor conbination on cod_info_w */
	int huff_bits = amp_scalefac_bands(gfc, &cod_info_w, distort,
					   current_method, targ_bits);
	if (huff_bits > 0) {
	    /* adjust global_gain to fit the available bits */
	    while (count_bits(gfc, xrpow, &cod_info_w) > huff_bits
		   && ++cod_info_w.global_gain < 256u)
		;
	    /* store this scalefactor combination if it is better */
	    if (cod_info_w.global_gain != 256
		&& better_quant(gfc, l3_xmin, distort, &best_noise_info,
				&cod_info_w)) {
		*gi = cod_info_w;
		if (best_noise_info.max_noise < 0.0) {
		    if (gfc->noise_shaping_stop == 0)
			break;
		    if (current_method == 0)
			current_method++;
		}
		age = current_method*3+2;
		continue;
	    }
	} else
	    age = 0;

	/* stopping criteria */
	if (--age > 0 && cod_info_w.global_gain != 256
	    /* Check if the last scalefactor band is distorted.
	     * (makes a 10% speed increase, the files I tested were
	     * binary identical, 2000/05/20 Robert Hegemann)
	     * distort[] > 1 means noise > allowed noise
	     */
	    && !(cod_info_w.psy_lmax == SBMAX_l && distort[SBMAX_l-1] > 1.0))
	    continue;

	/* seems we cannot get a better combination.
	   restart from the best with more finer noise_amp method */
	if (current_method == gfc->noise_shaping_amp)
	    break;
	current_method++;
	cod_info_w = *gi;
	age = current_method*3+2;
    }

    assert (gi->global_gain < 256);

    /*  do the 'substep shaping'
     */
 quit_quantization:
    if (gfc->substep_shaping & 1)
	trancate_smallspectrums(gfc, gi, l3_xmin, xrpow);
}





/********************************************************************
 *
 *  calc_target_bits()
 *
 *  calculates target bits for ABR encoding
 *
 *  mt 2000/05/31
 *
 ********************************************************************/

static void 
ABR_calc_target_bits (
    lame_global_flags * gfp,
    III_psy_ratio      ratio        [2][2],
    FLOAT               ms_ener_ratio [2],
    int                  targ_bits     [2][2]
    )
{
    lame_internal_flags *gfc=gfp->internal_flags;
    FLOAT res_factor;
    int gr, ch, totbits, mean_bits;
    int analog_silence_bits, max_frame_bits;
    
    gfc->bitrate_index = gfc->VBR_max_bitrate;
    max_frame_bits = ResvFrameBegin (gfp, &mean_bits);

    gfc->bitrate_index = 1;
    mean_bits = getframebits(gfp) - gfc->sideinfo_len * 8;
    analog_silence_bits = mean_bits / (gfc->mode_gr * gfc->channels_out);

    gfc->bitrate_index = 0;
    mean_bits = getframebits(gfp) - gfc->sideinfo_len * 8;
    if (gfc->substep_shaping & 1)
	mean_bits *= 1.1;
    mean_bits /= (gfc->mode_gr * gfc->channels_out);

    /*
        res_factor is the percentage of the target bitrate that should
        be used on average.  the remaining bits are added to the
        bitreservoir and used for difficult to encode frames.  

        Since we are tracking the average bitrate, we should adjust
        res_factor "on the fly", increasing it if the average bitrate
        is greater than the requested bitrate, and decreasing it
        otherwise.  Reasonable ranges are from .9 to 1.0
        
        Until we get the above suggestion working, we use the following
        tuning:
        compression ratio    res_factor
          5.5  (256kbps)         1.0      no need for bitreservoir 
          11   (128kbps)         .93      7% held for reservoir
   
        with linear interpolation for other values.

     */
    res_factor = .93 + .07 * (11.0 - gfp->compression_ratio) / (11.0 - 5.5);
    if (res_factor <  .90)
        res_factor =  .90; 
    if (res_factor > 1.00) 
        res_factor = 1.00;

    for (gr = 0; gr < gfc->mode_gr; gr++) {
        for (ch = 0; ch < gfc->channels_out; ch++) {
	    if (ratio[gr][ch].ath_over == 0) {
		targ_bits[gr][ch] = analog_silence_bits;
		continue;
	    }

            targ_bits[gr][ch] = res_factor * mean_bits;
	    if (ratio[gr][ch].pe > 600.0) {
		int add_bits = (ratio[gr][ch].pe - 600.0);

                /* at most increase bits by 1.5*average */
                if (add_bits > mean_bits*3/2)
                    add_bits = mean_bits*3/2;
                assert(add_bits >= 0);
                targ_bits[gr][ch] += add_bits;
            }
        }
    }

    /*  sum target bits
     */
    totbits=0;
    for (gr = 0; gr < gfc->mode_gr; gr++) {
        for (ch = 0; ch < gfc->channels_out; ch++) {
            if (targ_bits[gr][ch] > MAX_BITS) 
                targ_bits[gr][ch] = MAX_BITS;
            totbits += targ_bits[gr][ch];
        }
    }

    /*  repartion target bits if needed
     */
    if (totbits > max_frame_bits) {
        for (gr = 0; gr < gfc->mode_gr; gr++) {
            for (ch = 0; ch < gfc->channels_out; ch++) {
                targ_bits[gr][ch] *= max_frame_bits; 
                targ_bits[gr][ch] /= totbits; 
            }
        }
    }
}






/********************************************************************
 *
 *  ABR_iteration_loop()
 *
 *  encode a frame with a disired average bitrate
 *
 *  mt 2000/05/31
 *
 ********************************************************************/

void 
ABR_iteration_loop(
    lame_global_flags *gfp,
    FLOAT             ms_ener_ratio[2], 
    III_psy_ratio      ratio        [2][2])
{
    lame_internal_flags *gfc=gfp->internal_flags;
    int targ_bits[2][2];
    int mean_bits;
    int gr, ch;

    ABR_calc_target_bits (gfp, ratio, ms_ener_ratio, targ_bits);
    for (gr = 0; gr < gfc->mode_gr; gr++) {
	for (ch = 0; ch < gfc->channels_out; ch++) {
	    gr_info *gi = &gfc->l3_side.tt[gr][ch];
	    outer_loop(gfp, gi, ch, targ_bits[gr][ch], &ratio[gr][ch]);
	    iteration_finish_one(gfc, gr, ch);
	    ResvAdjust(gfc, gi->part2_length + gi->part2_3_length);
	}
    }

    /*  find a bitrate which can refill the resevoir to positive size.
     */
    for (gfc->bitrate_index =  gfc->VBR_min_bitrate;
	 gfc->bitrate_index <= gfc->VBR_max_bitrate;
	 gfc->bitrate_index++) {
	if (ResvFrameBegin (gfp, &mean_bits) >= 0) break;
    }
    assert (gfc->bitrate_index <= gfc->VBR_max_bitrate);

    ResvFrameEnd (gfc, mean_bits);
}






/************************************************************************
 *
 *      iteration_loop()                                                    
 *
 *  author/date??
 *
 *  encodes one frame of MP3 data with constant bitrate
 *
 ************************************************************************/

void 
iteration_loop(
    lame_global_flags *gfp, 
    FLOAT             ms_ener_ratio[2],
    III_psy_ratio      ratio        [2][2])
{
    lame_internal_flags *gfc=gfp->internal_flags;
    int targ_bits[2];
    int mean_bits;
    int gr, ch;

    ResvFrameBegin (gfp, &mean_bits);
    for (gr = 0; gr < gfc->mode_gr; gr++) {
        /*  calculate needed bits */
        on_pe (gfc, ratio[gr], targ_bits, mean_bits);

        for (ch=0 ; ch < gfc->channels_out ; ch ++) {
	    gr_info *gi = &gfc->l3_side.tt[gr][ch]; 
	    outer_loop(gfp, gi, ch, targ_bits[ch], &ratio[gr][ch]);
	    iteration_finish_one(gfc, gr, ch);
	    ResvAdjust(gfc,
		       gi->part2_length + gi->part2_3_length
		       - mean_bits / gfc->channels_out);
        }
    }

    ResvFrameEnd (gfc, 0);
}


/************************************************************************
 *
 * VBR related code
 *  Takehiro TOMINAGA 2002-12-31
 * based on Mark and Robert's code and suggestions
 *
 ************************************************************************/

static void
bitpressure_strategy(
    gr_info *gi,
    FLOAT *pxmin
    )
{
    int sfb;
    for (sfb = 0; sfb < gi->psy_lmax; sfb++) 
	*pxmin++ *= 1.+(.029/(SBMAX_l*SBMAX_l))*(sfb*sfb+1);

    for (sfb = gi->sfb_smin; sfb < gi->psymax; sfb+=3) {
	FLOAT x = 1.+(.029/(SBMAX_s*SBMAX_s*9))*(sfb*sfb+1);
	*pxmin++ *= x;
	*pxmin++ *= x;
	*pxmin++ *= x;
    }
}

inline static  FLOAT
calc_sfb_noise_fast(const FLOAT * xr, const FLOAT * xr34, int bw, int sf)
{
    FLOAT xfsf = 0.0;
    FLOAT sfpow = POW20(sf);  /*pow(2.0,sf/4.0); */
    FLOAT sfpow34 = IPOW20(sf); /*pow(sfpow,-3.0/4.0); */

    bw >>= 1;
    do {
	fi_union fi0, fi1;
	FLOAT t0, t1;
#ifdef TAKEHIRO_IEEE754_HACK
	fi0.f = sfpow34 * xr34[0] + (ROUNDFAC + MAGIC_FLOAT);
	fi1.f = sfpow34 * xr34[1] + (ROUNDFAC + MAGIC_FLOAT);

	if (fi0.i > MAGIC_INT + IXMAX_VAL) return -1;
	if (fi1.i > MAGIC_INT + IXMAX_VAL) return -1;
	t0 = fabs(xr[0]) - (pow43 - MAGIC_INT)[fi0.i] * sfpow;
	t1 = fabs(xr[1]) - (pow43 - MAGIC_INT)[fi1.i] * sfpow;
#else
	XRPOW_FTOI(sfpow34 * xr34[0] + ROUNDFAC, fi0.i);
	XRPOW_FTOI(sfpow34 * xr34[1] + ROUNDFAC, fi1.i);
 
	if (fi0.i > IXMAX_VAL) return -1;
	if (fi1.i > IXMAX_VAL) return -1;
	t0 = fabs(xr[0]) - pow43[fi0.i] * sfpow;
	t1 = fabs(xr[1]) - pow43[fi1.i] * sfpow;
#endif
	xfsf += t0*t0 + t1*t1;
	xr34 += 2;
	xr += 2;
    } while (--bw > 0);
    return xfsf;
}

inline static  FLOAT
calc_sfb_noise(const FLOAT * xr, const FLOAT * xr34, int bw, int sf)
{
    FLOAT xfsf = 0.0;
    FLOAT sfpow = POW20(sf);  /*pow(2.0,sf/4.0); */
    FLOAT sfpow34 = IPOW20(sf); /*pow(sfpow,-3.0/4.0); */
 
    bw >>= 1;
    do {
	fi_union fi0, fi1;
	FLOAT t0, t1;
#ifdef TAKEHIRO_IEEE754_HACK
	fi0.f = (t0 = sfpow34 * xr34[0] + (ROUNDFAC + MAGIC_FLOAT));
	fi1.f = (t1 = sfpow34 * xr34[1] + (ROUNDFAC + MAGIC_FLOAT));

	if (fi0.i > MAGIC_INT + IXMAX_VAL) return -1.0;
	if (fi1.i > MAGIC_INT + IXMAX_VAL) return -1.0;
	fi0.f = t0 + (adj43asm - MAGIC_INT)[fi0.i];
	fi1.f = t1 + (adj43asm - MAGIC_INT)[fi1.i];
	t0 = fabs(xr[0]) - (pow43 - MAGIC_INT)[fi0.i] * sfpow;
	t1 = fabs(xr[1]) - (pow43 - MAGIC_INT)[fi1.i] * sfpow;
#else
	fi0.i = (int) (t0 = sfpow34 * xr34[0]);
        fi1.i = (int) (t1 = sfpow34 * xr34[1]);
	if (fi0.i > IXMAX_VAL) return -1.0;
	if (fi1.i > IXMAX_VAL) return -1.0;

	t0 = fabs(xr[0]) - pow43[(int)(t0 + adj43[fi0.i])] * sfpow;
	t1 = fabs(xr[1]) - pow43[(int)(t1 + adj43[fi1.i])] * sfpow;
#endif
	xfsf += t0*t0 + t1*t1;
	xr34 += 2;
	xr += 2;
    } while (--bw > 0);
    return xfsf;
}

/* the find_scalefac* routines calculate
 * a quantization step size which would
 * introduce as much noise as is allowed.
 * The larger the step size the more
 * quantization noise we'll get. The
 * scalefactors are there to lower the
 * global step size, allowing limited
 * differences in quantization step sizes
 * per band (shaping the noise).
 */
inline int
find_scalefac(
    const FLOAT * xr, const FLOAT * xr34, FLOAT l3_xmin, int bw)
{
    int sf, sf_ok, delsf;

    /* search will range from sf:  -209 -> 45  */
    /* from the spec, we can use -321 -> 45 */
    sf = 128;
    sf_ok = 10000;
    for (delsf = 64; delsf != 0; delsf >>= 1) {
	FLOAT xfsf = calc_sfb_noise_fast(xr, xr34, bw, sf);

	if (xfsf > l3_xmin) {
	    /* distortion.  try a smaller scalefactor */
	    sf -= delsf;
	}
	else {
	    if (xfsf >= 0)
		sf_ok = sf;
	    sf += delsf;
	}
    }

    if (sf_ok <= 255)
	return sf_ok;
    return sf;
}



static void
set_scalefactor_values(gr_info *gi, int const max_range[],
		       int vbrmax, int vbrsf[])
{
    int ifqstep = 1 << (1 + gi->scalefac_scale), sfb = 0;
    do {
	int s = (vbrmax - vbrsf[sfb] - gi->subblock_gain[gi->window[sfb]]*8
		 + ifqstep - 1) >> (1 + gi->scalefac_scale);
	if (gi->preflag > 0)
	    s -= pretab[sfb];
	if (s < 0)
	    s = 0;
	gi->scalefac[sfb] = s;
	assert(s <= max_range[sfb]);
    } while (++sfb < gi->psymax);
}

/******************************************************************
 *
 *  short block scalefacs
 *
 ******************************************************************/

static void
short_block_scalefacs(const lame_internal_flags * gfc, gr_info * gi,
		      int *vbrsf, int vbrmax
    )
{
    int sfb, b, newmax0, newmax1;
    int maxov0[3], maxov1[3];

    maxov0[0] = maxov0[1] = maxov0[2]
	= maxov1[0] = maxov1[1] = maxov1[2]
	= newmax0 = newmax1
	= vbrmax;

    for (sfb = 0; sfb < gi->psymax; ) {
	for (b = 0; b < 3; b++) {
	    maxov0[b] = Min(vbrsf[sfb] + 2*max_range_short[sfb], maxov0[b]);
	    maxov1[b] = Min(vbrsf[sfb] + 4*max_range_short[sfb], maxov1[b]);
	    sfb++;
	}
    }

    /* should we use subblcok_gain ? */
    for (b = 0; b < 3; b++) {
	if (newmax0 > maxov0[b] + 7*8)
	    newmax0 = maxov0[b] + 7*8;
	if (newmax1 > maxov1[b] + 7*8)
	    newmax1 = maxov1[b] + 7*8;
    }
    if (newmax0 <= newmax1) {
	vbrmax = newmax0;
	gi->scalefac_scale = 0;
    } else {
	vbrmax = newmax1;
	gi->scalefac_scale = 1;
	memcpy(maxov0, maxov1, sizeof(maxov0));
    }
    if (vbrmax < 0)
	vbrmax = 0;
    if (vbrmax > 255)
	vbrmax = 255;
    gi->global_gain = vbrmax;

    for (b = 0; b < 3; b++) {
	int sbg = (vbrmax - maxov0[b] + 7) / 8;
	if (sbg < 0)
	    sbg = 0;
	assert(sbg <= 7);
	gi->subblock_gain[b] = sbg;
    }
    set_scalefactor_values(gi, max_range_short, vbrmax, vbrsf);
}

/******************************************************************
 *
 *  long block scalefacs
 *
 ******************************************************************/
static void
long_block_scalefacs(const lame_internal_flags * gfc, gr_info * gi,
		     int *vbrsf, int vbrmax
    )
{
    int     sfb;
    int     maxov0, maxov1, maxov0p, maxov1p;
    const int *max_rangep
	= gfc->mode_gr == 2 ? max_range_long : max_range_long_lsf_pretab;

    /* we can use 4 strategies.
       [scalefac_scale = (0,1), pretab = (0,1)] */
    maxov0  = maxov1 = maxov0p = maxov1p = vbrmax;

    /* find out the distance from most important scalefactor band
       (which wants the largest scalefactor value)
       to largest possible range */
    for (sfb = 0; sfb < gi->psymax; ++sfb) {
	maxov0  = Min(vbrsf[sfb] + 2*max_range_long[sfb], maxov0);
	maxov1  = Min(vbrsf[sfb] + 4*max_range_long[sfb], maxov1);
	maxov0p = Min(vbrsf[sfb] + 2*(max_rangep[sfb] + pretab[sfb]), maxov0p);
	maxov1p = Min(vbrsf[sfb] + 4*(max_rangep[sfb] + pretab[sfb]), maxov1p);
    }

    if (maxov0 == vbrmax) {
        gi->scalefac_scale = 0;
        gi->preflag = 0;
    }
    else if (maxov0p == vbrmax) {
        gi->scalefac_scale = 0;
        gi->preflag = 1;
    }
    else if (maxov1 == vbrmax) {
        gi->scalefac_scale = 1;
        gi->preflag = 0;
    }
    else if (maxov1p == vbrmax) {
        gi->scalefac_scale = 1;
        gi->preflag = 1;
    } else {
        gi->scalefac_scale = 1;
	if (maxov1 <= maxov1p) {
	    gi->preflag = 0;
	    vbrmax = maxov1;
	} else {
	    gi->preflag = 1;
	    vbrmax = maxov1p;
	}
    }

    if (vbrmax < 0)
        vbrmax = 0;
    if (vbrmax > 255)
        vbrmax = 255;
    gi->global_gain = vbrmax;

    if (!gi->preflag)
	max_rangep = max_range_long;

    set_scalefactor_values(gi, max_rangep, vbrmax, vbrsf);
}


static int
VBR_maxnoise(
    gr_info *gi,
    FLOAT * xr34,
    FLOAT * l3_xmin,
    int sfb2)
{
    int sfb, j = 0;
    for (sfb = 0; sfb < gi->psymax; ++sfb) {
	int width = gi->width[sfb];
	if (sfb >= sfb2
	    && calc_sfb_noise(
		&gi->xr[j], &xr34[j], width,
		gi->global_gain
		- ((gi->scalefac[sfb] + (gi->preflag>0 ? pretab[sfb] : 0))
		   << (gi->scalefac_scale + 1))
		- gi->subblock_gain[gi->window[sfb]] * 8) > l3_xmin[sfb])
	    return sfb;
	j += width;
    }
    return -1;
}

static void
VBR_2nd_bitalloc(
    lame_internal_flags * gfc,
    gr_info *gi,
    FLOAT * xr34,
    FLOAT * l3_xmin)
{
    /* note: we cannot use calc_noise() because l3_enc[] is not calculated
       at this point */
    gr_info gi_w = *gi;
    int sfb = 0, endflag = 0;
    for (;;) {
	sfb = VBR_maxnoise(&gi_w, xr34, l3_xmin, sfb);
	if (sfb >= 0) {
	    if (sfb >= gi->sfbmax) {
		endflag |= 1;
		if (endflag == 3 || gi_w.global_gain == 0)
		    return;
		gi_w.global_gain--;
		sfb = 0;
	    } else {
		gi_w.scalefac[sfb]++;
		if (loop_break(&gi_w)
		    || gfc->scale_bitcounter(&gi_w) > MAX_BITS)
		    return;
	    }
	} else {
	    *gi = gi_w;
	    gi_w.global_gain++;
	    endflag |= 2;
	    if (endflag == 3 || gi_w.global_gain > 255)
		return;
	}
    }
}

/************************************************************************
 *
 *  VBR_noise_shaping()
 *    part of this code is based on Robert's VBR code,
 *    but now completely new
 *
 ***********************************************************************/
static int
VBR_noise_shaping(
    lame_internal_flags * gfc, FLOAT * xr34,
    FLOAT * l3_xmin, int gr, int ch)
{
    int vbrsf[SFBMAX], vbrmax, sfb, j;
    gr_info *gi = &gfc->l3_side.tt[gr][ch];

    sfb = j = 0;
    vbrmax = -10000;
    do {
	int width = gi->width[sfb], gain;
	gain = find_scalefac(&gi->xr[j], &xr34[j], l3_xmin[sfb], width);
	j += width;
	vbrsf[sfb] = gain;
	if (vbrmax < gain)
	    vbrmax = gain;
    } while (++sfb < gi->psymax);

    if (gi->block_type == SHORT_TYPE)
	short_block_scalefacs(gfc, gi, vbrsf, vbrmax);
    else
	long_block_scalefacs(gfc, gi, vbrsf, vbrmax);

    /* ensure there's no noise */
    VBR_2nd_bitalloc(gfc, gi, xr34, l3_xmin);

    /* encode scalefacs */
    gfc->scale_bitcounter(gi);

    /* quantize xr34 */
    gi->part2_3_length = count_bits(gfc, xr34, gi);

    if (gi->part2_3_length + gi->part2_length >= MAX_BITS)
	return -2;

    if (gfc->substep_shaping & 2) {
	FLOAT xrtmp[576];
	trancate_smallspectrums(gfc, gi, l3_xmin, xrtmp);
    }

    if (gfc->use_best_huffman == 2)
	best_huffman_divide(gfc, gi);

    assert (gi->global_gain < 256u);
    assert(gi->part2_3_length + gi->part2_length < MAX_BITS);

    return 0;
}


void 
VBR_iteration_loop (
    lame_global_flags *gfp,
    III_psy_ratio ratio[2][2]
    )
{
    lame_internal_flags *gfc=gfp->internal_flags;
    FLOAT l3_xmin[2][2][SFBMAX];

    FLOAT xrpow[576];
    int mean_bits, max_frame_bits, used_bits;
    int ch, gr, ath_over = 0;

    for (gr = 0; gr < gfc->mode_gr; gr++) {
        for (ch = 0; ch < gfc->channels_out; ch++) {
      	    calc_xmin (gfp, &ratio[gr][ch],
		       &gfc->l3_side.tt[gr][ch], l3_xmin[gr][ch]);
	    ath_over += ratio[gr][ch].ath_over;
        }
    }

    gfc->bitrate_index = gfc->VBR_max_bitrate;
    max_frame_bits = ResvFrameBegin (gfp, &mean_bits);

 VBRloop_restart:
    {
	used_bits = 0;
	for (gr = 0; gr < gfc->mode_gr; gr++) {
	    for (ch = 0; ch < gfc->channels_out; ch++) {
		gr_info *gi = &gfc->l3_side.tt[gr][ch];
		if (init_outer_loop(gfc, gi, xrpow) == 0)
		    continue; /* digital silence */

		while (VBR_noise_shaping(gfc, xrpow,
					 l3_xmin[gr][ch], gr, ch) < 0) {
		    bitpressure_strategy(gi, l3_xmin[gr][ch]);
		}

		used_bits += gi->part2_3_length + gi->part2_length;
		if (used_bits > max_frame_bits) {
		    for (gr = 0; gr < gfc->mode_gr; gr++) 
			for (ch = 0; ch < gfc->channels_out; ch++) 
			    bitpressure_strategy(&gfc->l3_side.tt[gr][ch],
						 l3_xmin[gr][ch]);
		    goto VBRloop_restart;
		}
	    }
	}
    }

    for (gr = 0; gr < gfc->mode_gr; gr++) {
        for (ch = 0; ch < gfc->channels_out; ch++) {
	    gr_info *gi = &gfc->l3_side.tt[gr][ch];
	    if (gfc->substep_shaping & 1)
		trancate_smallspectrums(gfc, gi, l3_xmin[gr][ch], xrpow);
	    iteration_finish_one(gfc, gr, ch);
	    ResvAdjust (gfc, gi->part2_length + gi->part2_3_length);
	} /* for ch */
    }    /* for gr */

    /*  find a bitrate which can refill the resevoir to positive size.
     */
    gfc->bitrate_index = gfc->VBR_min_bitrate;
    if (!ath_over && !gfp->VBR_hard_min)
	gfc->bitrate_index = 1;
    for (; gfc->bitrate_index <= gfc->VBR_max_bitrate; gfc->bitrate_index++)
	if (ResvFrameBegin (gfp, &mean_bits) >= 0)
	    break;
    assert (gfc->bitrate_index <= gfc->VBR_max_bitrate);

    ResvFrameEnd (gfc, mean_bits);
}



#ifdef HAVE_GTK
/************************************************************************
 *
 *  set_pinfo()
 *
 *  updates plotting data    
 *
 *  Mark Taylor 2000-??-??                
 *
 *  Robert Hegemann: moved noise/distortion calc into it
 *
 ************************************************************************/

static
void set_pinfo (
        lame_global_flags *gfp,
              gr_info        * const gi,
        const III_psy_ratio  * const ratio, 
        const int                    gr,
        const int                    ch )
{
    lame_internal_flags *gfc=gfp->internal_flags;
    int sfb, sfb2;
    int j,i,l,start,end,bw;
    FLOAT en0,en1;
    FLOAT ifqstep = ( gi->scalefac_scale == 0 ) ? .5 : 1.0;

    FLOAT l3_xmin[SFBMAX], xfsf[SFBMAX];
    calc_noise_result noise;

    calc_xmin (gfp, ratio, gi, l3_xmin);
    calc_noise (gi, l3_xmin, xfsf, &noise);

    j = 0;
    sfb2 = gi->sfb_lmax;
    if (gi->block_type != SHORT_TYPE)
	sfb2 = 22;
    for (sfb = 0; sfb < sfb2; sfb++) {
	start = gfc->scalefac_band.l[ sfb ];
	end   = gfc->scalefac_band.l[ sfb+1 ];
	bw = end - start;
	for ( en0 = 0.0; j < end; j++ ) 
	    en0 += gi->xr[j] * gi->xr[j];
	en0/=bw;
	/* convert to MDCT units */
	en1=1e15;  /* scaling so it shows up on FFT plot */
	gfc->pinfo->  en[gr][ch][sfb] = en1*en0;
	gfc->pinfo->xfsf[gr][ch][sfb] = en1*l3_xmin[sfb]*xfsf[sfb]/bw;

	if (ratio->en.l[sfb]>0 && !gfp->ATHonly)
	    en0 = en0/ratio->en.l[sfb];
	else
	    en0=0.0;

	gfc->pinfo->thr[gr][ch][sfb] =
	    en1*Max(en0*ratio->thm.l[sfb],gfc->ATH.l[sfb]);

	gfc->pinfo->LAMEsfb[gr][ch][sfb] = 0;
	if (gi->preflag>0 && sfb>=11)
	    gfc->pinfo->LAMEsfb[gr][ch][sfb] = -ifqstep*pretab[sfb];

	assert(gi->scalefac[sfb]>=0); /*scfsi should be decoded by the caller*/
	gfc->pinfo->LAMEsfb[gr][ch][sfb] -= ifqstep*gi->scalefac[sfb];
    } /* for sfb */

    if (gi->block_type == SHORT_TYPE) {
	sfb2 = sfb;
        for (sfb = gi->sfb_smin; sfb < SBMAX_s; sfb++ )  {
            start = gfc->scalefac_band.s[ sfb ];
            end   = gfc->scalefac_band.s[ sfb + 1 ];
            bw = end - start;
            for ( i = 0; i < 3; i++ ) {
                for ( en0 = 0.0, l = start; l < end; l++ ) {
                    en0 += gi->xr[j] * gi->xr[j];
                    j++;
                }
                en0=Max(en0/bw,1e-20);
                /* convert to MDCT units */
                en1=1e15;  /* scaling so it shows up on FFT plot */

                gfc->pinfo->  en_s[gr][ch][3*sfb+i] = en1*en0;
                gfc->pinfo->xfsf_s[gr][ch][3*sfb+i] = en1*l3_xmin[sfb2]*xfsf[sfb2]/bw;
                if (ratio->en.s[sfb][i]>0)
                    en0 = en0/ratio->en.s[sfb][i];
                else
                    en0=0.0;
                if (gfp->ATHonly || gfp->ATHshort)
                    en0=0;

                gfc->pinfo->thr_s[gr][ch][3*sfb+i] = 
                        en1*Max(en0*ratio->thm.s[sfb][i],gfc->ATH.s[sfb]);

                gfc->pinfo->LAMEsfb_s[gr][ch][3*sfb+i]
		    = -2.0*gi->subblock_gain[i] - ifqstep*gi->scalefac[sfb2];
		sfb2++;
            }
        }
    } /* block type short */
    gfc->pinfo->LAMEqss     [gr][ch] = gi->global_gain;
    gfc->pinfo->LAMEmainbits[gr][ch] = gi->part2_3_length + gi->part2_length;
    gfc->pinfo->LAMEsfbits  [gr][ch] = gi->part2_length;

    gfc->pinfo->over      [gr][ch] = -1;
    gfc->pinfo->max_noise [gr][ch] = noise.max_noise * 10.0;
    gfc->pinfo->over_noise[gr][ch] = noise.over_noise * 10.0;
    gfc->pinfo->tot_noise [gr][ch] = -1.0;
}


/************************************************************************
 *
 *  set_frame_pinfo()
 *
 *  updates plotting data for a whole frame  
 *
 *  Robert Hegemann 2000-10-21                          
 *
 ************************************************************************/

void set_frame_pinfo( 
        lame_global_flags *gfp,
        III_psy_ratio   ratio    [2][2])
{
    lame_internal_flags *gfc=gfp->internal_flags;
    int                   ch;
    int                   gr;

    /* for every granule and channel patch l3_enc and set info
     */
    for (gr = 0; gr < gfc->mode_gr; gr ++) {
        for (ch = 0; ch < gfc->channels_out; ch ++) {
            gr_info *gi = &gfc->l3_side.tt[gr][ch];
	    int scalefac_sav[SFBMAX];
	    memcpy(scalefac_sav, gi->scalefac, sizeof(scalefac_sav));

	    /* reconstruct the scalefactors in case SCFSI was used 
	     */
            if (gr == 1) {
		int sfb;
		for (sfb = 0; sfb < gi->sfb_lmax; sfb++) {
		    if (gi->scalefac[sfb] < 0) /* scfsi */
			gi->scalefac[sfb] = gfc->l3_side.tt[0][ch].scalefac[sfb];
		}
	    }

	    set_pinfo (gfp, gi, &ratio[gr][ch], gr, ch);
	    memcpy(gi->scalefac, scalefac_sav, sizeof(scalefac_sav));
	} /* for ch */
    }    /* for gr */
}
#endif /* ifdef HAVE_GTK */
