/*
 * MP3 quantization
 *
 * Copyright (c) 1999 Mark Taylor
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
	targ_bits[ch] = tbits;
	if (ratio[ch].pe > 700.0) {
	    targ_bits[ch] = tbits * (ratio[ch].pe * (1.0/700.0));
	    if (targ_bits[ch] > mean_bits) 
		targ_bits[ch] = mean_bits;
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




static void
reduce_side(int targ_bits[2],FLOAT ms_ener_ratio,int mean_bits)
{
    int move_bits;
    FLOAT fac;

    if (targ_bits[1] < 125)
	return; /* dont reduce side channel below 125 bits */

    /*  ms_ener_ratio = 0:  allocate 66/33  mid/side  fac=.33
     *  ms_ener_ratio =.5:  allocate 50/50 mid/side   fac= 0 */
    /* 75/25 split is fac=.25 */
    /* float fac = .50*(.5-ms_ener_ratio[gr])/.5;*/
    fac = .33*(.5-ms_ener_ratio);
    if (fac<0) fac=0;
    if (fac>.25) fac=.25;

    /* number of bits to move from side channel to mid channel */
    /*    move_bits = fac*targ_bits[1];  */
    move_bits = fac*(targ_bits[0]+targ_bits[1]);

    if (move_bits > MAX_BITS - targ_bits[0])
        move_bits = MAX_BITS - targ_bits[0];

    if (targ_bits[1]-move_bits > 125) {
	targ_bits[1] -= move_bits;
	/* if mid channel already has 2x more than average, dont bother */
	/* mean_bits = bits per granule (for both channels) */
	if (targ_bits[0] < mean_bits)
	    targ_bits[0] += move_bits;
    } else {
	targ_bits[0] += targ_bits[1] - 125;
	targ_bits[1] = 125;
    }
    assert (targ_bits[0] <= MAX_BITS);
    assert (targ_bits[1] <= MAX_BITS);
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
    u -= athFloor;                                  // undo scaling
    if ( v > 1E-20 ) w = 1. + FAST_LOG10_X(v, 10.0 / o);
    if ( w < 0  )    w = 0.; 
    u *= w; 
    u += athFloor + o-p;                            // redo scaling
#undef o
#undef p
    return db2pow(u);
}

/*
  Calculate the allowed distortion for each scalefactor band,
  as determined by the psychoacoustic model.
  xmin(sb) = ratio(sb) * en(sb) / bw(sb)

  returns number of sfb's with energy > ATH
*/
static void
calc_xmin(
    lame_global_flags *gfp,
    const III_psy_ratio * const ratio,
    const gr_info       * const cod_info, 
	   FLOAT        * pxmin
    )
{
    lame_internal_flags *gfc = gfp->internal_flags;
    int sfb, gsfb, j=0;
    const FLOAT *xr = cod_info->xr;

    for (gsfb = 0; gsfb < cod_info->psy_lmax; gsfb++) {
	FLOAT en0, xmin;
	int width, l;
	xmin = athAdjust(gfc->ATH.adjust, gfc->ATH.l[gsfb], gfc->ATH.floor);

	width = cod_info->width[gsfb];
	l = width >> 1;
	en0 = 0.0;
	do {
	    en0 += xr[j] * xr[j]; j++;
	    en0 += xr[j] * xr[j]; j++;
	} while (--l > 0);

	if (!gfp->ATHonly) {
	    FLOAT x = ratio->en.l[gsfb];
	    if (x > 0.0) {
		x = en0 * ratio->thm.l[gsfb] / x;
		if (xmin < x)
		    xmin = x;
	    }
	}
	*pxmin++ = xmin;
    }   /* end of long block loop */

    for (sfb = cod_info->sfb_smin; gsfb < cod_info->psymax; sfb++, gsfb += 3) {
	int width, b;
	FLOAT tmpATH
	    = athAdjust(gfc->ATH.adjust, gfc->ATH.s[sfb], gfc->ATH.floor);

	width = cod_info->width[gsfb];
	for ( b = 0; b < 3; b++ ) {
	    FLOAT en0 = 0.0, xmin;
	    int l = width >> 1;
	    do {
		en0 += xr[j] * xr[j]; j++;
		en0 += xr[j] * xr[j]; j++;
	    } while (--l > 0);

	    xmin = tmpATH;
	    if (!gfp->ATHonly && !gfp->ATHshort) {
		FLOAT x = ratio->en.s[sfb][b];
		if (x > 0.0)
		    x = en0 * ratio->thm.s[sfb][b] / x;
		if (xmin < x) 
		    xmin = x;
	    }
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
        const gr_info             * const cod_info,
        const FLOAT              * l3_xmin, 
              FLOAT              * distort,
              calc_noise_result   * const res )
{
    int sfb, l, over=0;
    FLOAT over_noise_db = 0;
    FLOAT tot_noise_db  = 0; /*    0 dB relative to masking */
    FLOAT max_noise = -20.0; /* -200 dB relative to masking */
    int j = 0;
    const int *ix = cod_info->l3_enc;
    const int *scalefac = cod_info->scalefac;

    for (sfb = 0; sfb < cod_info->psymax; sfb++) {
	int s =
	    cod_info->global_gain
	    - (((*scalefac++) + (cod_info->preflag ? pretab[sfb] : 0))
	       << (cod_info->scalefac_scale + 1))
	    - cod_info->subblock_gain[cod_info->window[sfb]] * 8;
	FLOAT step = POW20(s);
	FLOAT noise = 0.0;

	l = cod_info->width[sfb] >> 1;
	do {
	    FLOAT temp;
	    temp = fabs(cod_info->xr[j]) - pow43[ix[j]] * step; j++;
	    noise += temp * temp;
	    temp = fabs(cod_info->xr[j]) - pow43[ix[j]] * step; j++;
	    noise += temp * temp;
	} while (--l > 0);
	noise = *distort++ = noise / *l3_xmin++;

	noise = FAST_LOG10(Max(noise,1E-20));
	/* multiplying here is adding in dB, but can overflow */
	//tot_noise *= Max(noise, 1E-20);
	tot_noise_db += noise;

	if (noise > 0.0) {
	    over++;
	    /* multiplying here is adding in dB -but can overflow */
	    //over_noise *= noise;
	    over_noise_db += noise;
	}
	max_noise=Max(max_noise,noise);
    }

    if (cod_info->block_type == SHORT_TYPE) {
	distort -= sfb;
	over = 0;
	max_noise = -20.0;
	over_noise_db = 0.0;
	for (sfb = cod_info->sfb_smin; sfb < cod_info->psymax; sfb += 3) {
	    FLOAT noise = 0.0;
	    if (distort[sfb] > 1.0)
		noise += FAST_LOG10(distort[sfb]);
	    if (distort[sfb+1] > 1.0)
		noise += FAST_LOG10(distort[sfb+1]);
	    if (distort[sfb+2] > 1.0)
		noise += FAST_LOG10(distort[sfb+2]);
	    if (max_noise < noise)
		max_noise = noise;
	    if (noise > 0) {
		over++;
		over_noise_db += noise;
	    }
	}
    }
    res->over_count = over;
    res->tot_noise   = tot_noise_db;
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
    gr_info *const cod_info, 
    FLOAT xrpow[576] )
{
    FLOAT tmp, sum = 0;
    int i;

    /*  check if there is some energy we have to quantize
     *  and calculate xrpow matching our fresh scalefactors
     */
    for (i = 0; i < 576; ++i) {
        tmp = fabs (cod_info->xr[i]);
	sum += tmp;
        xrpow[i] = sqrt (tmp * sqrt(tmp));
    }
    /*  return 1 if we have something to quantize, else 0
     */
    if (sum > (FLOAT)1E-20) {
	int j = 0;
	if (gfc->substep_shaping & 2)
	    j = 1;

	for (i = 0; i < cod_info->psymax; i++)
	    gfc->pseudohalf[i] = j;
	return 1;
    }

    memset(&cod_info->l3_enc, 0, sizeof(int)*576);
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
          gr_info * const cod_info,
	  int             desired_rate,
	  int CurrentStep,
	  int start,
    const FLOAT          xrpow [576] ) 
{
    int nBits;
    int flag_GoneOver = 0;
    binsearchDirection_t Direction = BINSEARCH_NONE;
    cod_info->global_gain = start;
    desired_rate -= cod_info->part2_length;

    assert(CurrentStep);
    do {
	int step;
        nBits = count_bits(gfc, xrpow, cod_info);  

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
	cod_info->global_gain += step;
    } while (cod_info->global_gain < 256u);

    if (cod_info->global_gain < 0) {
	cod_info->global_gain = 0;
	nBits = count_bits(gfc, xrpow, cod_info);
    } else if (cod_info->global_gain > 255) {
	cod_info->global_gain = 255;
	nBits = count_bits(gfc, xrpow, cod_info);
    } else if (nBits > desired_rate) {
	cod_info->global_gain++;
	nBits = count_bits(gfc, xrpow, cod_info);
    }
    cod_info->part2_3_length = nBits;
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

//	printf("%e %e %e\n",
//	       trancateThreshold/l3_xmin[sfb],
//	       trancateThreshold/(l3_xmin[sfb]*start),
//	       trancateThreshold/(l3_xmin[sfb]*(start+width))
//	    );
//	if (trancateThreshold > 1000*l3_xmin[sfb]*start)
//	    trancateThreshold = 1000*l3_xmin[sfb]*start;

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
    const gr_info        * const cod_info)
{
    int sfb;

    for (sfb = 0; sfb < cod_info->sfbmax; sfb++)
        if (cod_info->scalefac[sfb]
	    + cod_info->subblock_gain[cod_info->window[sfb]] == 0)
            return 0;

    return 1;
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
    /*
       noise is given in decibels (dB) relative to masking thesholds.

       over_noise:  ??? (the previous comment is fully wrong)
       tot_noise:   ??? (the previous comment is fully wrong)
       max_noise:   max quantization noise 
     */

    calc_noise (gi, l3_xmin, distort, &calc);
    switch (gi->block_type != NORM_TYPE
	    ? gfc->quantcomp_method_s : gfc->quantcomp_method) {
        default:
        case 0:
	    better = calc.over_count  < best->over_count
               ||  ( calc.over_count == best->over_count  &&
                     calc.over_noise  < best->over_noise )
               ||  ( calc.over_count == best->over_count  &&
                     calc.over_noise == best->over_noise  &&
                     calc.tot_noise   < best->tot_noise  ); 
	    break;

        case 2:	    /* for backward compatibility, pass through */
        case 4:
        case 5:
        case 7:
        case 8:
        case 9:
        case 1:
	    better = calc.max_noise < best->max_noise; 
	    break;

        case 3:
	    better = (calc.tot_noise < best->tot_noise)
		&&   (calc.max_noise < best->max_noise);
	    break;

        case 6:
	    better =  calc.over_noise  < best->over_noise
		||  ( calc.over_noise == best->over_noise
		      && ( calc.max_noise   < best->max_noise  
			   || ( calc.max_noise  == best->max_noise
			     && calc.tot_noise  <= best->tot_noise )
			  ));
	    break;

        case 10:
	    better = (calc.over_noise < best->over_noise)
		&&   (calc.max_noise < best->max_noise);
	    break;
    }   
    if (better)
	*best = calc;
    return better;
}



/*************************************************************************
 *
 *          amp_scalefac_bands() 
 *
 *  author/date??
 *        
 *  Amplify the scalefactor bands that violate the masking threshold.
 *  See ISO 11172-3 Section C.1.5.4.3.5
 * 
 *  distort[] = noise/masking
 *  distort[] > 1   ==> noise is not masked
 *  distort[] < 1   ==> noise is masked
 *  max_dist = maximum value of distort[]
 *  
 *  Three algorithms:
 *  noise_shaping_amp
 *        0             Amplify all bands with distort[]>1.
 *
 *        1             Amplify all bands with distort[] >= max_dist^(.5);
 *                     ( 50% in the db scale)
 *
 *        2             Amplify first band with distort[] >= max_dist;
 *                       
 *
 *  For algorithms 0 and 1, if max_dist < 1, then amplify all bands 
 *  with distort[] >= .95*max_dist.  This is to make sure we always
 *  amplify at least one band.  
 * 
 *
 *************************************************************************/
static void 
amp_scalefac_bands(
    lame_internal_flags *gfc,
    gr_info  *const cod_info, 
    FLOAT *distort,
    FLOAT xrpow[576] )
{
    int j, sfb;
    FLOAT ifqstep34, trigger;

    if (cod_info->scalefac_scale == 0) {
	ifqstep34 = 1.29683955465100964055; /* 2**(.75*.5)*/
    } else {
	ifqstep34 = 1.68179283050742922612;  /* 2**(.75*1) */
    }

    /* compute maximum value of distort[]  */
    trigger = 0;
    for (sfb = 0; sfb < cod_info->sfbmax; sfb++) {
	if (trigger < distort[sfb])
	    trigger = distort[sfb];
    }

    switch (gfc->noise_shaping_amp) {
    case 2:
	/* amplify exactly 1 band */
	break;

    case 1:
	/* amplify bands within 50% of max (on db scale) */
	if (trigger>1.0)
	    trigger = sqrt(trigger);
	else
	    trigger *= .95;
	break;

    case 0:
    default:
	/* ISO algorithm.  amplify all bands with distort>1 */
	if (trigger>1.0)
	    trigger=1.0;
	else
	    trigger *= .95;
	break;
    }

    j = 0;
    for (sfb = 0; sfb < cod_info->sfbmax; sfb++) {
	int width = cod_info->width[sfb];
	int l;
	j += width;
	if (distort[sfb] < trigger)
	    continue;

	if (gfc->substep_shaping & 2) {
	    gfc->pseudohalf[sfb] = !gfc->pseudohalf[sfb];
	    if (!gfc->pseudohalf[sfb] && gfc->noise_shaping_amp==2)
		return;
	}
	cod_info->scalefac[sfb]++;
	for (l = -width; l < 0; l++)
	    xrpow[j+l] *= ifqstep34;

	if (gfc->noise_shaping_amp==2)
	    return;
    }
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
    gr_info        * const cod_info, 
    FLOAT                 xrpow[576] )
{
    int l, j, sfb;
    const FLOAT ifqstep34 = 1.29683955465100964055;

    j = 0;
    for (sfb = 0; sfb < cod_info->sfbmax; sfb++) {
	int width = cod_info->width[sfb];
        int s = cod_info->scalefac[sfb];
	if (cod_info->preflag)
	    s += pretab[sfb];
	j += width;
        if (s & 1) {
            s++;
            for (l = -width; l < 0; l++) 
                xrpow[j+l] *= ifqstep34;
        }
        cod_info->scalefac[sfb] = s >> 1;
    }
    cod_info->preflag = 0;
    cod_info->scalefac_scale = 1;
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
    const lame_internal_flags        * const gfc,
          gr_info        * const cod_info,
          FLOAT                 xrpow[576] )
{
    int sfb, window;
    int * const scalefac = cod_info->scalefac;

    /* subbloc_gain can't do anything in the long block region */
    for (sfb = 0; sfb < cod_info->sfb_lmax; sfb++) {
	if (scalefac[sfb] >= 16)
	    return 1;
    }

    for (window = 0; window < 3; window++) {
	int s1, s2, l, j;
        s1 = s2 = 0;

        for (sfb = cod_info->sfb_lmax+window;
	     sfb < cod_info->sfbdivide; sfb += 3) {
            if (s1 < scalefac[sfb])
		s1 = scalefac[sfb];
        }
	for (; sfb < cod_info->sfbmax; sfb += 3) {
	    if (s2 < scalefac[sfb])
		s2 = scalefac[sfb];
        }

        if (s1 < 16 && s2 < 8)
            continue;

        if (cod_info->subblock_gain[window] >= 7)
            return 1;

        /* even though there is no scalefactor for sfb12
         * subblock gain affects upper frequencies too, that's why
         * we have to go up to SBMAX_s
         */
        cod_info->subblock_gain[window]++;
	j = gfc->scalefac_band.l[cod_info->sfb_lmax];
	for (sfb = cod_info->sfb_lmax+window;
	     sfb < cod_info->sfbmax; sfb += 3) {
	    FLOAT amp;
	    int width = cod_info->width[sfb], s = scalefac[sfb];

	    assert(s >= 0);
	    s = s - (4 >> cod_info->scalefac_scale);
            if (s >= 0) {
		scalefac[sfb] = s;
		j += width*3;
		continue;
	    }

	    scalefac[sfb] = 0;
	    amp = IPOW20(210 + (s << (cod_info->scalefac_scale + 1)));
	    j += width * (window+1);
	    for (l = -width; l < 0; l++) {
		xrpow[j+l] *= amp;
            }
	    j += width*(2-window);
        }
	{
	    FLOAT8 amp = IPOW20(210 - 8);
	    j += cod_info->width[sfb] * (window+1);
	    for (l = -cod_info->width[sfb]; l < 0; l++)
		xrpow[j+l] *= amp;
        }
    }
    return 0;
}



/********************************************************************
 *
 *      balance_noise()
 *
 *  Takehiro Tominaga /date??
 *  Robert Hegemann 2000-09-06: made a function of it
 *
 *  amplifies scalefactor bands, 
 *   - if all are already amplified returns 0
 *   - if some bands are amplified too much:
 *      * try to increase scalefac_scale
 *      * if already scalefac_scale was set
 *          try on short blocks to increase subblock gain
 *
 ********************************************************************/
inline static int 
balance_noise (
    lame_internal_flags *const gfc,
    gr_info        * const cod_info,
    FLOAT         * distort,
    FLOAT         xrpow[576] )
{
    int status;
    
    amp_scalefac_bands ( gfc, cod_info, distort, xrpow);

    /* check to make sure we have not amplified too much 
     * loop_break returns 0 if there is an unamplified scalefac
     * scale_bitcount returns 0 if no scalefactors are too large
     */
    if (loop_break (cod_info)) 
        return 0; /* all bands amplified */
    
    /* not all scalefactors have been amplified.  so these 
     * scalefacs are possibly valid.  encode them: 
     */
    if (gfc->mode_gr == 2)
        status = scale_bitcount (cod_info);
    else 
        status = scale_bitcount_lsf (gfc, cod_info);

    if (!status)
        return 1; /* amplified some bands not exceeding limits */
    
    /*  some scalefactors are too large. */
    if (gfc->use_scalefac_scale && !cod_info->scalefac_scale) {
	/*  lets try setting scalefac_scale=1 */
	inc_scalefac_scale (cod_info, xrpow);
    } else if (gfc->use_subblock_gain && cod_info->block_type == SHORT_TYPE) {
	/* try to use subblcok gain */
	if (inc_subblock_gain (gfc, cod_info, xrpow) || loop_break (cod_info))
	    return 0;
    } else
	return 0;

    if (gfc->mode_gr == 2)
	return !scale_bitcount (cod_info);
    else 
	return !scale_bitcount_lsf (gfc, cod_info);
}



/************************************************************************
 *
 *  outer_loop ()                                                       
 *
 *  Function: The outer iteration loop controls the masking conditions  
 *  of all scalefactorbands. It computes the best scalefac and global gain.
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
    gr_info		* const cod_info,
    const int           ch,
    const int           targ_bits,  /* maximum allowed bits */
    const III_psy_ratio * const ratio
    )
{
    lame_internal_flags *gfc=gfp->internal_flags;
    calc_noise_result best_noise_info;
    gr_info cod_info_w;
    FLOAT distort[SFBMAX], l3_xmin[SFBMAX];
    FLOAT xrpow[576];
    int huff_bits;

    if (!init_outer_loop(gfc, cod_info, xrpow))
	return; /* digital silence */

    bin_search_StepSize (gfc, cod_info, targ_bits,
			 gfc->CurrentStep[ch], gfc->OldValue[ch], xrpow);

    gfc->CurrentStep[ch]
	= (gfc->OldValue[ch] - cod_info->global_gain) >= 4 ? 4 : 2;
    gfc->OldValue[ch] = cod_info->global_gain;

    if (gfc->psymodel < 2) 
	/* fast mode, no noise shaping, we are ready */
	return;

    /* compute the distortion in this quantization */
    /* coefficients and thresholds both l/r (or both mid/side) */
    calc_xmin (gfp, ratio, cod_info, l3_xmin);
    calc_noise (cod_info, l3_xmin, distort, &best_noise_info);
    cod_info_w = *cod_info;

    /* BEGIN MAIN LOOP */
    do {
	/******************************************************************/
	/* stopping criterion */
	/******************************************************************/
	/* if no bands with distortion, we are done */
	if (gfc->noise_shaping_stop == 0 && best_noise_info.over_count == 0)
	    break;

	/* Check if the last scalefactor band is distorted.
	 * (makes a 10% speed increase, the files I tested were
	 * binary identical, 2000/05/20 Robert Hegemann)
	 * distort[] > 1 means noise > allowed noise
	 */
	if (cod_info_w.psy_lmax == SBMAX_l && distort[SBMAX_l-1] > 1.0)
	    break;

	/* try the new scalefactor conbination on cod_info_w */
	if (balance_noise (gfc, &cod_info_w, distort, xrpow) == 0)
	    break;

        huff_bits = targ_bits - cod_info_w.part2_length;
	if (huff_bits <= 0)
            break;

        /* this loop starts with the initial quantization step computed above
         * and slowly increases until the bits < huff_bits.
         * Thus it is important not to start with too large of an inital
         * quantization step.  Too small is ok, but the loop will take longer
         */
	while (count_bits(gfc, xrpow, &cod_info_w) > huff_bits
	       && ++cod_info_w.global_gain < 256u)
	    ;
	if (cod_info_w.global_gain == 256)
	    break;

        /* save data so we can restore this quantization later,
	 * if the newer scalefactor combination is better
	 */
        /* compute the distortion in this quantization and
	   compare it with "BEST" result */
	if (better_quant(gfc, l3_xmin, distort, &best_noise_info, &cod_info_w))
	    *cod_info = cod_info_w;
    }
    while (cod_info_w.global_gain <= 255u);

    assert (cod_info->global_gain < 256);

    /*  do the 'substep shaping'
     */
    if (gfc->substep_shaping & 1)
	trancate_smallspectrums(gfc, cod_info, l3_xmin, xrpow);
}





/************************************************************************
 *
 *      iteration_finish_one()
 *
 *  Robert Hegemann 2000-09-06
 *
 *  update reservoir status after FINAL quantization/bitrate 
 *
 ************************************************************************/

static void 
iteration_finish_one (
    lame_internal_flags *gfc,
    int gr, int ch, int adjbits)
{
    gr_info *gi = &gfc->l3_side.tt[gr][ch];

    /*  try some better scalefac storage
     */
    best_scalefac_store (gfc, gr, ch);

    /*  best huffman_divide may save some bits too
     */
    if (gfc->use_best_huffman == 1) 
	best_huffman_divide (gfc, gi);

    /*  update reservoir status after FINAL quantization/bitrate
     */
    ResvAdjust (gfc, gi->part2_length + gi->part2_3_length - adjbits);
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
                int add_bits = (ratio[gr][ch].pe - 600.0) / 1.4;

                /* short blocks use a little extra, no matter what the pe */
                if (gfc->l3_side.tt[gr][ch].block_type == SHORT_TYPE
                    && add_bits < mean_bits/2)
		    add_bits = mean_bits/2;

                /* at most increase bits by 1.5*average */
                if (add_bits > mean_bits*3/2)
                    add_bits = mean_bits*3/2;
                assert(add_bits >= 0);
                targ_bits[gr][ch] += add_bits;
            }
        }
	if (gfc->mode_ext & MPG_MD_MS_LR)
	    reduce_side(targ_bits[gr], ms_ener_ratio[gr], mean_bits * STEREO);
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
        for(gr = 0; gr < gfc->mode_gr; gr++) {
            for(ch = 0; ch < gfc->channels_out; ch++) {
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
    int       targ_bits[2][2];
    int       mean_bits;
    int       ch, gr;

    ABR_calc_target_bits (gfp, ratio, ms_ener_ratio, targ_bits);
    /*  encode granules
     */
    for (gr = 0; gr < gfc->mode_gr; gr++) {
        for (ch = 0; ch < gfc->channels_out; ch++) {
	    gr_info *cod_info = &gfc->l3_side.tt[gr][ch];
	    outer_loop(gfp, cod_info, ch, targ_bits[gr][ch], &ratio[gr][ch]);
	    iteration_finish_one(gfc, gr, ch, 0);
        } /* ch */
    }  /* gr */

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
    int    targ_bits[2];
    int    mean_bits;
    int    gr, ch;

    ResvFrameBegin (gfp, &mean_bits);

    for (gr = 0; gr < gfc->mode_gr; gr++) {
        /*  calculate needed bits */
        on_pe (gfc, ratio[gr], targ_bits, mean_bits);
        if (gfc->mode_ext & MPG_MD_MS_LR)
            reduce_side (targ_bits, ms_ener_ratio[gr], mean_bits);

        for (ch=0 ; ch < gfc->channels_out ; ch ++) {
	    gr_info *cod_info = &gfc->l3_side.tt[gr][ch]; 
	    outer_loop (gfp, cod_info, ch, targ_bits[ch], &ratio[gr][ch]);
            assert (cod_info->part2_3_length <= MAX_BITS);
	    iteration_finish_one(gfc, gr, ch, mean_bits / gfc->channels_out);
        } /* for ch */
    }    /* for gr */

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
	*pxmin++ *= 1.+(.029/(SBMAX_l*SBMAX_l))*sfb*sfb;

    for (sfb = gi->sfb_smin; sfb < gi->psymax; sfb+=3) {
	*pxmin++ *= 1.+(.029/(SBMAX_s*SBMAX_s*9))*sfb*sfb;
	*pxmin++ *= 1.+(.029/(SBMAX_s*SBMAX_s*9))*sfb*sfb;
	*pxmin++ *= 1.+(.029/(SBMAX_s*SBMAX_s*9))*sfb*sfb;
    }
}

static  FLOAT
calc_sfb_noise(const FLOAT * xr, const FLOAT * xr34, int bw, int sf)
{
    FLOAT xfsf = 0.0;
    FLOAT sfpow = POW20(sf);  /*pow(2.0,sf/4.0); */
    FLOAT sfpow34 = IPOW20(sf); /*pow(sfpow,-3.0/4.0); */

    bw >>= 1;
    do {
	fi_union fi0, fi1;
	FLOAT t1, t2;
#ifdef TAKEHIRO_IEEE754_HACK
	fi0.f = sfpow34 * xr34[0] + (ROUNDFAC + MAGIC_FLOAT);
	fi1.f = sfpow34 * xr34[1] + (ROUNDFAC + MAGIC_FLOAT);

	if (fi0.i > MAGIC_INT + IXMAX_VAL) return -1;
	if (fi1.i > MAGIC_INT + IXMAX_VAL) return -1;
	t1 = fabs(xr[0]) - (pow43 - MAGIC_INT)[fi0.i] * sfpow;
	t2 = fabs(xr[1]) - (pow43 - MAGIC_INT)[fi1.i] * sfpow;
#else
        XRPOW_FTOI(sfpow34 * xr34[0] + ROUNDFAC, fi0.i);
        XRPOW_FTOI(sfpow34 * xr34[1] + ROUNDFAC, fi1.i);

	if (fi0.i > IXMAX_VAL) return -1;
	if (fi1.i > IXMAX_VAL) return -1;
	t1 = fabs(xr[0]) - pow43[fi0.i] * sfpow;
	t2 = fabs(xr[1]) - pow43[fi1.i] * sfpow;
#endif
        xfsf += t1*t1 + t2*t2;
	xr34 += 2;
	xr += 2;
    } while (--bw > 0);
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



static const int max_range_short[SBMAX_s*3] = {
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    0, 0, 0 };

static const int max_range_long[SBMAX_l] =
    { 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 0 };

static const int max_range_long_lsf_pretab[SBMAX_l] =
    { 7, 7, 7, 7, 7, 7, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

/***********************************************************************
 *
 *      block_sf()
 *
 *  set the gain of each scalefactor band
 *
 ***********************************************************************/
static int
block_sf(const lame_internal_flags * gfc, const FLOAT * l3_xmin,
	 const FLOAT * xr34_orig, int * vbrsf, gr_info *gi)
{
    int     sfb, j, vbrmax;
    int     scalefac_method = gfc->quantcomp_method;

    if (gi->block_type == SHORT_TYPE)
	scalefac_method = gfc->quantcomp_method_s;
    j = 0;
    vbrmax = -10000;
    for (sfb = 0; sfb < gi->psymax; sfb++) {
        const int width = gi->width[sfb];
	int gain;
        if (scalefac_method >= 4) {
            gain = 210 + (int) (9.0 * log10(l3_xmin[sfb] / width) - .5);
	} else {
            gain = find_scalefac(&gi->xr[j], &xr34_orig[j], l3_xmin[sfb], width);
        }
	j += width;
	vbrsf[sfb] = gain;
	if (vbrmax < gain)
	    vbrmax = gain;
    }
    return vbrmax;
}



static void
set_scalefactor_values(gr_info *gi, int const max_range[],
		       int vbrmax, int vbrsf[])
{
    int ifqstep = 1 << (1 + gi->scalefac_scale);
    int sfb;
    for (sfb = 0; sfb < gi->psymax; sfb++) {
	/* ifqstep*scalefac >= -sf[sfb], so round UP */
	gi->scalefac[sfb]
	    = (vbrmax - vbrsf[sfb] - gi->subblock_gain[gi->window[sfb]]*8
	       - (gi->preflag ? pretab[sfb] * ifqstep : 0)
	       + ifqstep - 1) / ifqstep;
	if (gi->scalefac[sfb] < 0)
	    gi->scalefac[sfb] = 0;
	assert(gi->scalefac[sfb] <= max_range[sfb]);
    }
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
        maxov0p = Min(vbrsf[sfb] + 2*(max_rangep[sfb] + pretab[sfb]),
			maxov0p);
        maxov1p = Min(vbrsf[sfb] + 4*(max_rangep[sfb] + pretab[sfb]),
			maxov1p);
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
    int     sfb, j, *scalefac = gi->scalefac;
    for (j = sfb = 0; j < 576; ++sfb) {
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
 *    part of this code is based on Robert's VBR code,
 *    but now completely new
 *
 ***********************************************************************/
static int
VBR_noise_shaping(
    lame_internal_flags * gfc, FLOAT * xr34orig,
    FLOAT * l3_xmin, int gr, int ch)
{
    int vbrsf[SFBMAX];
    gr_info *gi;
    FLOAT  xr34[576];
    int     vbrmax;

    gi = &gfc->l3_side.tt[gr][ch];

    vbrmax = block_sf(gfc, l3_xmin, xr34orig, vbrsf, gi);
    if (gi->block_type == SHORT_TYPE) {
	short_block_scalefacs(gfc, gi, vbrsf, vbrmax);
    }
    else {
	long_block_scalefacs(gfc, gi, vbrsf, vbrmax);
    }

    /* encode scalefacs */
    if (gfc->mode_gr == 2)
	scale_bitcount(gi);
    else
	scale_bitcount_lsf(gfc, gi);

    /* quantize xr34 */
    block_xr34(gfc, gi, xr34orig, xr34);
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
		gr_info *cod_info = &gfc->l3_side.tt[gr][ch];
		if (init_outer_loop(gfc, cod_info, xrpow) == 0)
		    continue; /* digital silence */

		while (VBR_noise_shaping(gfc, xrpow,
					 l3_xmin[gr][ch], gr, ch) < 0) {
		    bitpressure_strategy(cod_info, l3_xmin[gr][ch]);
		}

		if (gfc->substep_shaping & 1)
		    trancate_smallspectrums(gfc, cod_info,
					    l3_xmin[gr][ch], xrpow);

		used_bits += cod_info->part2_3_length + cod_info->part2_length;
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
	    iteration_finish_one(gfc, gr, ch, 0);
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
              gr_info        * const cod_info,
        const III_psy_ratio  * const ratio, 
        const int                    gr,
        const int                    ch )
{
    lame_internal_flags *gfc=gfp->internal_flags;
    int sfb, sfb2;
    int j,i,l,start,end,bw;
    FLOAT en0,en1;
    FLOAT ifqstep = ( cod_info->scalefac_scale == 0 ) ? .5 : 1.0;
    int* scalefac = cod_info->scalefac;

    FLOAT l3_xmin[SFBMAX], xfsf[SFBMAX];
    calc_noise_result noise;

    calc_xmin (gfp, ratio, cod_info, l3_xmin);
    calc_noise (cod_info, l3_xmin, xfsf, &noise);

    j = 0;
    sfb2 = cod_info->sfb_lmax;
    if (cod_info->block_type != SHORT_TYPE)
	sfb2 = 22;
    for (sfb = 0; sfb < sfb2; sfb++) {
	start = gfc->scalefac_band.l[ sfb ];
	end   = gfc->scalefac_band.l[ sfb+1 ];
	bw = end - start;
	for ( en0 = 0.0; j < end; j++ ) 
	    en0 += cod_info->xr[j] * cod_info->xr[j];
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

	/* there is no scalefactor bands >= SBPSY_l */
	gfc->pinfo->LAMEsfb[gr][ch][sfb] = 0;
	if (cod_info->preflag && sfb>=11) 
	    gfc->pinfo->LAMEsfb[gr][ch][sfb] = -ifqstep*pretab[sfb];

	if (sfb<SBPSY_l) {
	    assert(scalefac[sfb]>=0); /* scfsi should be decoded by caller side*/
	    gfc->pinfo->LAMEsfb[gr][ch][sfb] -= ifqstep*scalefac[sfb];
	}
    } /* for sfb */

    if (cod_info->block_type == SHORT_TYPE) {
	sfb2 = sfb;
        for (sfb = cod_info->sfb_smin; sfb < SBMAX_s; sfb++ )  {
            start = gfc->scalefac_band.s[ sfb ];
            end   = gfc->scalefac_band.s[ sfb + 1 ];
            bw = end - start;
            for ( i = 0; i < 3; i++ ) {
                for ( en0 = 0.0, l = start; l < end; l++ ) {
                    en0 += cod_info->xr[j] * cod_info->xr[j];
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

                /* there is no scalefactor bands >= SBPSY_s */
                gfc->pinfo->LAMEsfb_s[gr][ch][3*sfb+i]
		    = -2.0*cod_info->subblock_gain[i];
                if (sfb < SBPSY_s) {
                    gfc->pinfo->LAMEsfb_s[gr][ch][3*sfb+i] -=
						ifqstep*scalefac[sfb2];
                }
		sfb2++;
            }
        }
    } /* block type short */
    gfc->pinfo->LAMEqss     [gr][ch] = cod_info->global_gain;
    gfc->pinfo->LAMEmainbits[gr][ch] = cod_info->part2_3_length + cod_info->part2_length;
    gfc->pinfo->LAMEsfbits  [gr][ch] = cod_info->part2_length;

    gfc->pinfo->over      [gr][ch] = noise.over_count;
    gfc->pinfo->max_noise [gr][ch] = noise.max_noise * 10.0;
    gfc->pinfo->over_noise[gr][ch] = noise.over_noise * 10.0;
    gfc->pinfo->tot_noise [gr][ch] = noise.tot_noise * 10.0;
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
            gr_info *cod_info = &gfc->l3_side.tt[gr][ch];
	    int scalefac_sav[SFBMAX];
	    memcpy(scalefac_sav, cod_info->scalefac, sizeof(scalefac_sav));

	    /* reconstruct the scalefactors in case SCFSI was used 
	     */
            if (gr == 1) {
		int sfb;
		for (sfb = 0; sfb < cod_info->sfb_lmax; sfb++) {
		    if (cod_info->scalefac[sfb] < 0) /* scfsi */
			cod_info->scalefac[sfb] = gfc->l3_side.tt[0][ch].scalefac[sfb];
		}
	    }

	    set_pinfo (gfp, cod_info, &ratio[gr][ch], gr, ch);
	    memcpy(cod_info->scalefac, scalefac_sav, sizeof(scalefac_sav));
	} /* for ch */
    }    /* for gr */
}
#endif /* ifdef HAVE_GTK */
