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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.     See the GNU
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
#include "vbrquantize.h"
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
static int
on_pe(
    lame_global_flags *gfp,
    III_psy_ratio      ratio[2],
    int targ_bits[2],
    int mean_bits,
    int gr,
    int cbr
    )
{
    lame_internal_flags * gfc = gfp->internal_flags;
    int     extra_bits, tbits, bits;
    int     max_bits;  /* maximum allowed bits for this granule */
    int     ch;

    /* allocate targ_bits for granule */
    ResvMaxBits( gfp, mean_bits, &tbits, &extra_bits, cbr);

    max_bits = tbits + extra_bits;
    if (max_bits > MAX_BITS) /* hard limit per granule */
        max_bits = MAX_BITS;

    /* at most increase bits by 1.5*average */
    mean_bits = tbits+ mean_bits*3/4;
    if (mean_bits > MAX_BITS)
	mean_bits = MAX_BITS; 

    tbits /= gfc->channels_out;
    if (tbits > MAX_BITS)
	tbits = MAX_BITS;

    for ( bits = 0, ch = 0; ch < gfc->channels_out; ++ch ) {
	if (ratio[ch].pe > 700.0) {
	    targ_bits[ch] = tbits * (ratio[ch].pe * (1.0/700.0));
	    if (targ_bits[ch] > mean_bits) 
		targ_bits[ch] = mean_bits;
	} else
	    targ_bits[ch] = tbits;

        bits += targ_bits[ch];
    }
    if (bits > max_bits) {
        for ( ch = 0; ch < gfc->channels_out; ++ch ) {
            targ_bits[ch]
		= tbits + extra_bits * (targ_bits[ch] - tbits) / bits;
        }
    }
    return max_bits;
}




static void
reduce_side(int targ_bits[2],FLOAT ms_ener_ratio,int mean_bits,int max_bits)
{
    int move_bits;
    FLOAT fac;

    /*  ms_ener_ratio = 0:  allocate 66/33  mid/side  fac=.33  
     *  ms_ener_ratio =.5:  allocate 50/50 mid/side   fac= 0 */
    /* 75/25 split is fac=.5 */
    /* float fac = .50*(.5-ms_ener_ratio[gr])/.5;*/
    fac = .33*(.5-ms_ener_ratio)/.5;
    if (fac<0) fac=0;
    if (fac>.5) fac=.5;

    /* number of bits to move from side channel to mid channel */
    /*    move_bits = fac*targ_bits[1];  */
    move_bits = fac*.5*(targ_bits[0]+targ_bits[1]);

    if (move_bits > MAX_BITS - targ_bits[0])
        move_bits = MAX_BITS - targ_bits[0];
    if (move_bits < 0)
	move_bits = 0;

    if (targ_bits[1] >= 125) {
	/* dont reduce side channel below 125 bits */
	if (targ_bits[1]-move_bits > 125) {
	    /* if mid channel already has 2x more than average, dont bother */
	    /* mean_bits = bits per granule (for both channels) */
	    if (targ_bits[0] < mean_bits)
		targ_bits[0] += move_bits;
	    targ_bits[1] -= move_bits;
	} else {
	    targ_bits[0] += targ_bits[1] - 125;
	    targ_bits[1] = 125;
	}
    }

    move_bits=targ_bits[0]+targ_bits[1];
    if (move_bits > max_bits) {
	targ_bits[0]=(max_bits*targ_bits[0])/move_bits;
	targ_bits[1]=(max_bits*targ_bits[1])/move_bits;
    }
    assert (targ_bits[0] <= MAX_BITS);
    assert (targ_bits[1] <= MAX_BITS);
}


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


/*************************************************************************/
/*            calc_xmin                                                  */
/*************************************************************************/

/*
  Calculate the allowed distortion for each scalefactor band,
  as determined by the psychoacoustic model.
  xmin(sb) = ratio(sb) * en(sb) / bw(sb)

  returns number of sfb's with energy > ATH
*/
int calc_xmin( 
        lame_global_flags *gfp,
        const III_psy_ratio * const ratio,
	const gr_info       * const cod_info, 
	      FLOAT        * pxmin
    )
{
    lame_internal_flags *gfc = gfp->internal_flags;
    int sfb, gsfb, j=0, ath_over=0;
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
	if (en0 > xmin) ath_over++;

	if (!gfp->ATHonly) {
	    FLOAT x = ratio->en.l[gsfb];
	    if (x > 0.0) {
		x = en0 * ratio->thm.l[gsfb] / x;
		if (xmin < x)
		    xmin = x;
	    }
	}
	*pxmin++ = xmin * gfc->nsPsy.longfact[gsfb];
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
	    if (en0 > tmpATH) ath_over++;

	    xmin = tmpATH;
	    if (!gfp->ATHonly && !gfp->ATHshort) {
		FLOAT x = ratio->en.s[sfb][b];
		if (x > 0.0)
		    x = en0 * ratio->thm.s[sfb][b] / x;
		if (xmin < x) 
		    xmin = x;
	    }
	    *pxmin++ = xmin * gfc->nsPsy.shortfact[sfb];
	}   /* b */
    }   /* end of short block sfb loop */

    return ath_over;
}







/*************************************************************************/
/*            calc_noise                                                 */
/*************************************************************************/

// -oo dB  =>  -1.00
// - 6 dB  =>  -0.97
// - 3 dB  =>  -0.80
// - 2 dB  =>  -0.64
// - 1 dB  =>  -0.38
//   0 dB  =>   0.00
// + 1 dB  =>  +0.49
// + 2 dB  =>  +1.06
// + 3 dB  =>  +1.68
// + 6 dB  =>  +3.69
// +10 dB  =>  +6.45

/*  mt 5/99:  Function: Improved calc_noise for a single channel   */
void calc_noise( 
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
    lame_global_flags *gfp,
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
	lame_internal_flags *gfc=gfp->internal_flags;
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

int 
bin_search_StepSize(
          lame_internal_flags * const gfc,
          gr_info * const cod_info,
	  int             desired_rate,
    const int             ch,
    const FLOAT          xrpow [576] ) 
{
    int nBits;
    int CurrentStep = gfc->CurrentStep[ch];
    int flag_GoneOver = 0;
    int start = gfc->OldValue[ch];
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
    gfc->CurrentStep[ch] = (start - cod_info->global_gain >= 4) ? 4 : 2;
    gfc->OldValue[ch] = cod_info->global_gain;
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
floatcompare(FLOAT *a, FLOAT *b)
{
    if (*a > *b) return 1;
    if (*a == *b) return 0;
    return -1;
}

void
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
    lame_global_flags *gfp,
    gr_info  *const cod_info, 
    FLOAT *distort,
    FLOAT xrpow[576] )
{
    lame_internal_flags *gfc=gfp->internal_flags;
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
	    trigger = pow(trigger, .5);
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
	j = gfc->scalefac_band.l[cod_info->sfb_lmax]
	    + cod_info->width[cod_info->sfb_lmax] * window;
	for (sfb = cod_info->sfb_lmax+window;
	     sfb < cod_info->sfbmax; sfb += 3) {
	    FLOAT amp;
	    int width = cod_info->width[sfb], s = scalefac[sfb];

	    j += width;
	    if (s < 0)
                continue; /* ???? */
	    s = s - (4 >> cod_info->scalefac_scale);
            if (s >= 0) {
		scalefac[sfb] = s;
		continue;
	    }

	    scalefac[sfb] = 0;
	    amp = IPOW20(210 + (s << (cod_info->scalefac_scale + 1)));
	    for (l = -width; l < 0; l++) {
		xrpow[j+l] *= amp;
            }
        }
	{
	    FLOAT8 amp = IPOW20(210 - 8);
	    j += cod_info->width[sfb];
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
    lame_global_flags  *const gfp,
    gr_info        * const cod_info,
    FLOAT         * distort,
    FLOAT         xrpow[576] )
{
    lame_internal_flags *const gfc = gfp->internal_flags;
    int status;
    
    amp_scalefac_bands ( gfp, cod_info, distort, xrpow);

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
    
    /*  some scalefactors are too large.
     *  lets try setting scalefac_scale=1 
     */
    if (gfc->use_scalefac_scale) {
	memset(&gfc->pseudohalf, 0, sizeof(gfc->pseudohalf));
	if (!cod_info->scalefac_scale) {
	    inc_scalefac_scale (cod_info, xrpow);
	    status = 0;
	} else {
	    if (cod_info->block_type == SHORT_TYPE ) {
		status = inc_subblock_gain (gfc, cod_info, xrpow)
		    || loop_break (cod_info);
	    }
	}
    }

    if (!status) {
        if (gfc->mode_gr == 2)
            status = scale_bitcount (cod_info);
        else 
            status = scale_bitcount_lsf (gfc, cod_info);
    }
    return !status;
}



/************************************************************************
 *
 *  outer_loop ()                                                       
 *
 *  Function: The outer iteration loop controls the masking conditions  
 *  of all scalefactorbands. It computes the best scalefac and          
 *  global gain. This module calls the inner iteration loop             
 * 
 *  mt 5/99 completely rewritten to allow for bit reservoir control,   
 *  mid/side channels with L/R or mid/side masking thresholds, 
 *  and chooses best quantization instead of last quantization when 
 *  no distortion free quantization can be found.  
 *  
 *  added VBR support mt 5/99
 *
 *  some code shuffle rh 9/00
 ************************************************************************/

static int 
outer_loop (
    lame_global_flags	*gfp,
    gr_info		* const cod_info,
    const FLOAT	* const l3_xmin,  /* allowed distortion */
    FLOAT		xrpow[576], /* coloured magnitudes of spectral */
    const int           ch,
    const int           targ_bits )  /* maximum allowed bits */
{
    lame_internal_flags *gfc=gfp->internal_flags;
    gr_info cod_info_w;
    FLOAT distort[SFBMAX];
    calc_noise_result best_noise_info;
    int huff_bits;
    int age;

    bin_search_StepSize (gfc, cod_info, targ_bits, ch, xrpow);

    if (gfc->psymodel < 2) 
	/* fast mode, no noise shaping, we are ready */
	return 100; /* default noise_info.over_count */

    /* compute the distortion in this quantization */
    /* coefficients and thresholds both l/r (or both mid/side) */
    calc_noise (cod_info, l3_xmin, distort, &best_noise_info);
    cod_info_w = *cod_info;
    age = 0;

    /* BEGIN MAIN LOOP */
    do {
	/******************************************************************/
	/* stopping criterion */
	/******************************************************************/
	/* if no bands with distortion and -X0, we are done */
	if (gfc->quantcomp_method == 0 && gfc->noise_shaping_stop == 0
	    && best_noise_info.over_count == 0)
	    break;

	/* Check if the last scalefactor band is distorted.
	 * (makes a 10% speed increase, the files I tested were
	 * binary identical, 2000/05/20 Robert.Hegemann@gmx.de)
	 * distort[] > 1 means noise > allowed noise
	 */
	if (gfc->sfb21_extra) {
	    if (distort[cod_info_w.sfbmax] > 1.0)
		break;
	    if (cod_info_w.block_type == SHORT_TYPE
		&& (distort[cod_info_w.sfbmax+1] > 1.0
		    || distort[cod_info_w.sfbmax+2] > 1.0))
		    break;
	}

	/* try the new scalefactor conbination on cod_info_w */
	if (balance_noise (gfp, &cod_info_w, distort, xrpow) == 0)
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
	if (better_quant(gfc, l3_xmin, distort, &best_noise_info,
			 &cod_info_w)) {
	    *cod_info = cod_info_w;
	    age = 0;
        }
        else {
	    /* allow up to 3 unsuccesful tries in serial, then stop 
	     * if our best quantization so far had no distorted bands. This
	     * gives us more possibilities for different better_quant modes.
	     * Much more than 3 makes not a big difference, it is only slower.
	     */
	    if (++age > 3 && best_noise_info.over_count == 0)
		break;
	}
    }
    while (cod_info_w.global_gain < 255u);

    assert (cod_info->global_gain < 256);

    /*  do the 'substep shaping'
     */
    if (gfc->substep_shaping & 1)
	trancate_smallspectrums(gfc, cod_info, l3_xmin, xrpow);

    return best_noise_info.over_count;
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
    int gr, int ch)
{
    gr_info *cod_info = &gfc->l3_side.tt[gr][ch];

    /*  try some better scalefac storage
     */
    best_scalefac_store (gfc, gr, ch);

    /*  best huffman_divide may save some bits too
     */
    if (gfc->use_best_huffman == 1) 
	best_huffman_divide (gfc, cod_info);

    /*  update reservoir status after FINAL quantization/bitrate
     */
    ResvAdjust (gfc, cod_info);
}



/************************************************************************
 *
 *      get_framebits()   
 *
 *  Robert Hegemann 2000-09-05
 *
 *  calculates
 *  * how many bits are available for analog silent granules
 *  * how many bits to use for the lowest allowed bitrate
 *  * how many bits each bitrate would provide
 *
 ************************************************************************/

static void 
get_framebits (
    lame_global_flags *gfp,
    int     * const analog_mean_bits,
    int     * const min_mean_bits,
    int             frameBits[15] )
{
    lame_internal_flags *gfc=gfp->internal_flags;
    int bitsPerFrame, i;
    
    /*  always use at least this many bits per granule per channel 
     *  unless we detect analog silence, see below 
     */
    gfc->bitrate_index = gfc->VBR_min_bitrate;
    bitsPerFrame = getframebits(gfp);
    *min_mean_bits = (bitsPerFrame - gfc->sideinfo_len * 8) / (gfc->mode_gr*gfc->channels_out);

    /*  bits for analog silence 
     */
    gfc->bitrate_index = 1;
    bitsPerFrame = getframebits(gfp);
    *analog_mean_bits = (bitsPerFrame - gfc->sideinfo_len * 8) / (gfc->mode_gr*gfc->channels_out);

    for (i = 1; i <= gfc->VBR_max_bitrate; i++) {
        gfc->bitrate_index = i;
        frameBits[i] = ResvFrameBegin (gfp, &bitsPerFrame);
    }
}



/*********************************************************************
 *
 *      VBR_prepare()
 *
 *  2000-09-04 Robert Hegemann
 *
 *  * calculates allowed/adjusted quantization noise amounts
 *  * detects analog silent frames
 *
 *  some remarks:
 *  - lower masking depending on Quality setting
 *  - quality control together with adjusted ATH MDCT scaling
 *    on lower quality setting allocate more noise from
 *    ATH masking, and on higher quality setting allocate
 *    less noise from ATH masking.
 *  - experiments show that going more than 2dB over GPSYCHO's
 *    limits ends up in very annoying artefacts
 *
 *********************************************************************/

/* RH: this one needs to be overhauled sometime */
 
static int 
VBR_prepare (
          lame_global_flags *gfp,
          FLOAT          ms_ener_ratio [2], 
          III_psy_ratio   ratio         [2][2], 
          FLOAT	  l3_xmin       [2][2][SFBMAX],
          int             frameBits     [16],
          int            *analog_mean_bits,
          int            *min_mean_bits,
          int             min_bits      [2][2],
          int             max_bits      [2][2]
    )
{
    lame_internal_flags *gfc=gfp->internal_flags;
    int     gr, ch;
    int     analog_silence = 1;
    int     avg, mxb, bits = 0;

    gfc->bitrate_index = gfc->VBR_max_bitrate;
    avg = ResvFrameBegin (gfp, &avg) / gfc->mode_gr;

    get_framebits (gfp, analog_mean_bits, min_mean_bits, frameBits);

    for (gr = 0; gr < gfc->mode_gr; gr++) {
        mxb = on_pe (gfp, ratio[gr], max_bits[gr], avg, gr, 0);
        if (gfc->mode_ext & MPG_MD_MS_LR)
            reduce_side (max_bits[gr], ms_ener_ratio[gr], avg, mxb);

        for (ch = 0; ch < gfc->channels_out; ++ch) {
            gr_info *cod_info = &gfc->l3_side.tt[gr][ch];
      	    if (calc_xmin (gfp, &ratio[gr][ch], cod_info, l3_xmin[gr][ch]))
                analog_silence = 0;

            min_bits[gr][ch] = 126;
            bits += max_bits[gr][ch];
        }
    }
    for (gr = 0; gr < gfc->mode_gr; gr++) {
        for (ch = 0; ch < gfc->channels_out; ch++) {
	    if (bits > frameBits[gfc->VBR_max_bitrate]) {
                max_bits[gr][ch] *= frameBits[gfc->VBR_max_bitrate];
                max_bits[gr][ch] /= bits;
            }
            if (min_bits[gr][ch] > max_bits[gr][ch]) 
                min_bits[gr][ch] = max_bits[gr][ch];
        } /* for ch */
    }  /* for gr */

    *min_mean_bits = Max(*min_mean_bits, 126);

    return analog_silence;
}


static void
bitpressure_strategy(
    lame_internal_flags * gfc,
    FLOAT l3_xmin[2][2][SFBMAX],
    int min_bits[2][2],  
    int max_bits[2][2] )  
{
    int gr, ch, sfb;
    for (gr = 0; gr < gfc->mode_gr; gr++) {
        for (ch = 0; ch < gfc->channels_out; ch++) {
	    gr_info *gi = &gfc->l3_side.tt[gr][ch];
	    FLOAT *pxmin = l3_xmin[gr][ch];
	    for (sfb = 0; sfb < gi->psy_lmax; sfb++) 
		*pxmin++ *= 1.+.029*sfb*sfb/SBMAX_l/SBMAX_l;

	    if (gi->block_type == SHORT_TYPE) {
		for (sfb = gi->sfb_smin; sfb < SBMAX_s; sfb++) {
		    *pxmin++ *= 1.+.029*sfb*sfb/SBMAX_s/SBMAX_s;
		    *pxmin++ *= 1.+.029*sfb*sfb/SBMAX_s/SBMAX_s;
		    *pxmin++ *= 1.+.029*sfb*sfb/SBMAX_s/SBMAX_s;
		}
	    }
            max_bits[gr][ch] = Max(min_bits[gr][ch], 0.9*max_bits[gr][ch]);
        }
    }
}

/************************************************************************
 *
 *      VBR_iteration_loop()   
 *
 *  tries to find out how many bits are needed for each granule and channel
 *  to get an acceptable quantization. An appropriate bitrate will then be
 *  choosed for quantization.  rh 8/99                          
 *
 *  Robert Hegemann 2000-09-06 rewrite
 *
 ************************************************************************/

void 
VBR_iteration_loop (
    lame_global_flags *gfp,
    FLOAT             ms_ener_ratio[2],
    III_psy_ratio ratio[2][2])
{
    lame_internal_flags *gfc=gfp->internal_flags;
    FLOAT l3_xmin[2][2][SFBMAX];
  
    FLOAT    xrpow[576];
    int       frameBits[15];
    int       save_bits[2][2];
    int       used_bits, used_bits2;
    int       bits;
    int       min_bits[2][2], max_bits[2][2];
    int       analog_mean_bits, min_mean_bits;
    int       mean_bits;
    int       ch, gr, analog_silence;

    analog_silence = VBR_prepare (gfp, ms_ener_ratio, ratio, 
                                  l3_xmin, frameBits, &analog_mean_bits,
                                  &min_mean_bits, min_bits, max_bits);

    /*---------------------------------*/
    for(;;) {  
    
    /*  quantize granules with lowest possible number of bits
     */
    
    used_bits = 0;
    used_bits2 = 0;
   
    for (gr = 0; gr < gfc->mode_gr; gr++) {
        for (ch = 0; ch < gfc->channels_out; ch++) {
            int ret; 
	    gr_info *cod_info = &gfc->l3_side.tt[gr][ch];
      
            /*  init_outer_loop sets up xrpow
             */
            ret = init_outer_loop(gfp, cod_info, xrpow);
            if (ret == 0 || max_bits[gr][ch] == 0) {
                /*  xr contains no energy 
                 *  l3_enc, our encoding data, will be quantized to zero
                 */
                save_bits[gr][ch] = 0;
                continue; /* with next channel */
            }

	    ret = VBR_noise_shaping (gfc, xrpow,
				     min_bits[gr][ch], max_bits[gr][ch], 
				     l3_xmin[gr][ch], gr, ch );
	    if (ret < 0)
		cod_info->part2_3_length = 100000;

	    /*  do the 'substep shaping'
	     */
	    if (gfc->substep_shaping & 1) {
		trancate_smallspectrums(gfc, cod_info,
					l3_xmin[gr][ch], xrpow);
	    }

            ret = cod_info->part2_3_length + cod_info->part2_length;
            used_bits += ret;
            save_bits[gr][ch] = Min(MAX_BITS, ret);
            used_bits2 += Min(MAX_BITS, ret);
        } /* for ch */
    }    /* for gr */

    /*  find lowest bitrate able to hold used bits
     */
     if (analog_silence && !gfp->VBR_hard_min) 
	 /*  we detected analog silence and the user did not specify 
	  *  any hard framesize limit, so start with smallest possible frame
	  */
	 gfc->bitrate_index = 1;
     else
	 gfc->bitrate_index = gfc->VBR_min_bitrate;

    for( ; gfc->bitrate_index < gfc->VBR_max_bitrate; gfc->bitrate_index++) {
        if (used_bits <= frameBits[gfc->bitrate_index]) break; 
    }
    bits = ResvFrameBegin (gfp, &mean_bits);

    if (used_bits <= bits) break;

    bitpressure_strategy( gfc, l3_xmin, min_bits, max_bits );

    }   /* breaks adjusted */
    /*--------------------------------------*/

    for (gr = 0; gr < gfc->mode_gr; gr++) {
        for (ch = 0; ch < gfc->channels_out; ch++) {
	    iteration_finish_one(gfc, gr, ch);
	} /* for ch */
    }    /* for gr */
    ResvFrameEnd (gfc, mean_bits);
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
calc_target_bits (
    lame_global_flags * gfp,
    III_psy_ratio      ratio        [2][2],
    FLOAT               ms_ener_ratio [2],
    int                  targ_bits     [2][2],
    int                 *analog_silence_bits,
    int                 *max_frame_bits )
{
    lame_internal_flags *gfc=gfp->internal_flags;
    FLOAT res_factor;
    int gr, ch, totbits, mean_bits;
    
    gfc->bitrate_index = gfc->VBR_max_bitrate;
    *max_frame_bits = ResvFrameBegin (gfp, &mean_bits);

    gfc->bitrate_index = 1;
    mean_bits = getframebits(gfp) - gfc->sideinfo_len * 8;
    *analog_silence_bits = mean_bits / (gfc->mode_gr * gfc->channels_out);

    mean_bits  = gfp->VBR_mean_bitrate_kbps * gfp->framesize * 1000;
    if (gfc->substep_shaping & 1)
	mean_bits *= 1.09;
    mean_bits /= gfp->out_samplerate;
    mean_bits -= gfc->sideinfo_len*8;
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
            targ_bits[gr][ch] = res_factor * mean_bits;

            if (ratio[gr][ch].pe > 700.0) {
                int add_bits = (ratio[gr][ch].pe - 700.0) / 1.4;

                gr_info *cod_info = &gfc->l3_side.tt[gr][ch];
                targ_bits[gr][ch] = res_factor * mean_bits;

                /* short blocks use a little extra, no matter what the pe */
                if (cod_info->block_type == SHORT_TYPE) {
                    if (add_bits < mean_bits/2)
                        add_bits = mean_bits/2;
                }
                /* at most increase bits by 1.5*average */
                if (add_bits > mean_bits*3/2)
                    add_bits = mean_bits*3/2;
                else
                if (add_bits < 0) 
                    add_bits = 0;

                targ_bits[gr][ch] += add_bits;
            }
        }
    }
    if (gfc->mode_ext & MPG_MD_MS_LR)
	for (gr = 0; gr < gfc->mode_gr; gr++)
	    reduce_side(targ_bits[gr], ms_ener_ratio[gr],
			mean_bits * STEREO, MAX_BITS);

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
    if (totbits > *max_frame_bits) {
        for(gr = 0; gr < gfc->mode_gr; gr++) {
            for(ch = 0; ch < gfc->channels_out; ch++) {
                targ_bits[gr][ch] *= *max_frame_bits; 
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
    FLOAT    l3_xmin[SFBMAX];
    FLOAT    xrpow[576];
    int       targ_bits[2][2];
    int       mean_bits, max_frame_bits;
    int       ch, gr, ath_over;
    int       analog_silence_bits;

    calc_target_bits (gfp, ratio, ms_ener_ratio, targ_bits, 
                      &analog_silence_bits, &max_frame_bits);
    
    /*  encode granules
     */
    for (gr = 0; gr < gfc->mode_gr; gr++) {
        for (ch = 0; ch < gfc->channels_out; ch++) {
	    gr_info *cod_info = &gfc->l3_side.tt[gr][ch];

            /*  init_outer_loop sets up xrpow
             */
            if (init_outer_loop(gfp, cod_info, xrpow)) {
                /*  xr contains energy we will have to encode 
                 *  calculate the masking abilities
                 *  find some good quantization in outer_loop 
                 */
                ath_over = calc_xmin (gfp, &ratio[gr][ch], cod_info, l3_xmin);
                if (0 == ath_over) /* analog silence */
                    targ_bits[gr][ch] = analog_silence_bits;

                outer_loop (gfp, cod_info, l3_xmin,
                            xrpow, ch, targ_bits[gr][ch]);
            }
	    iteration_finish_one(gfc, gr, ch);
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
    FLOAT l3_xmin[SFBMAX];
    FLOAT xrpow[576];
    int    targ_bits[2];
    int    mean_bits, max_bits;
    int    gr, ch;

    ResvFrameBegin (gfp, &mean_bits);

    /* quantize! */
    for (gr = 0; gr < gfc->mode_gr; gr++) {

        /*  calculate needed bits
         */
        max_bits = on_pe (gfp, ratio[gr], targ_bits, mean_bits, gr, gr);
        if (gfc->mode_ext & MPG_MD_MS_LR)
            reduce_side (targ_bits, ms_ener_ratio[gr], mean_bits, max_bits);
        
        for (ch=0 ; ch < gfc->channels_out ; ch ++) {
	    gr_info *cod_info = &gfc->l3_side.tt[gr][ch]; 

            /*  init_outer_loop sets up xrpow
             */
            if (init_outer_loop(gfp, cod_info, xrpow)) {
                /*  xr contains energy we will have to encode 
                 *  calculate the masking abilities
                 *  find some good quantization in outer_loop 
                 */
                calc_xmin (gfp, &ratio[gr][ch], cod_info, l3_xmin);
                outer_loop (gfp, cod_info, l3_xmin, xrpow, ch, targ_bits[ch]);
            }
            assert (cod_info->part2_3_length <= MAX_BITS);

	    iteration_finish_one(gfc, gr, ch);
        } /* for ch */
    }    /* for gr */

    ResvFrameEnd (gfc, mean_bits);
}
