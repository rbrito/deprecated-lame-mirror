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
#include "quantize_pvt.h"
#include "machine.h"
#include "tables.h"

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif


static const int max_range_short[SBMAX_s*3] = {
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    0, 0, 0
};
static const int max_range_long[SBMAX_l] = {
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 0
};

/*
  ResvFrameBegin:
  Called (repeatedly) at the beginning of a frame. Updates the maximum
  size of the reservoir, and checks to make sure main_data_begin
  was set properly by the formatter
*/

/*
 *  Background information:
 *
 *  This is the original text from the ISO standard. Because of 
 *  sooo many bugs and irritations correcting comments are added 
 *  in brackets []. A '^W' means you should remove the last word.
 *
 *  1) The following rule can be used to calculate the maximum 
 *     number of bits used for one granule [^W frame]: 
 *     At the highest possible bitrate of Layer III (320 kbps 
 *     per stereo signal [^W^W^W], 48 kHz) the frames must be of
 *     [^W^W^W are designed to have] constant length, i.e. 
 *     one buffer [^W^W the frame] length is:
 *
 *         320 kbps * 1152/48 kHz = 7680 bit = 960 byte
 *
 *     This value is used as the maximum buffer per channel [^W^W] at 
 *     lower bitrates [than 320 kbps]. At 64 kbps mono or 128 kbps 
 *     stereo the main granule length is 64 kbps * 576/48 kHz = 768 bit
 *     [per granule and channel] at 48 kHz sampling frequency. 
 *     This means that there is a maximum deviation (short time buffer 
 *     [= reservoir]) of 7680 - 2*2*768 = 4608 bits is allowed at 64 kbps. 
 *     The actual deviation is equal to the number of bytes [with the 
 *     meaning of octets] denoted by the main_data_end offset pointer. 
 *     The actual maximum deviation is (2^9-1)*8 bit = 4088 bits 
 *     [for MPEG-1 and (2^8-1)*8 bit for MPEG-2, both are hard limits].
 *     ... The xchange of buffer bits between the left and right channel
 *     is allowed without restrictions [exception: dual channel].
 *     Because of the [constructed] constraint on the buffer size
 *     main_data_end is always set to 0 in the case of bit_rate_index==14, 
 *     i.e. data rate 320 kbps per stereo signal [^W^W^W]. In this case 
 *     all data are allocated between adjacent header [^W sync] words 
 *     [, i.e. there is no buffering at all].
 */

static int
ResvFrameBegin(lame_global_flags *gfp, int *mean_bits)
{
    lame_internal_flags *gfc=gfp->internal_flags;
    III_side_info_t     *l3_side = &gfc->l3_side;
    int fullFrameBits, resvLimit, frameLength = getframebits(gfp);
/*
 *  Meaning of the variables:
 *      resvLimit: (0, 8, ..., 8*255 (MPEG-2), 8*511 (MPEG-1))
 *          Number of bits can be stored in previous frame(s) due to 
 *          counter size constaints
 *      maxmp3buf: ( ??? ... 8*1951 (MPEG-1 and 2), 8*2047 (MPEG-2.5))
 *          Number of bits allowed to encode one frame (you can take 8*511 bit 
 *          from the bit reservoir and at most 8*1440 bit from the current 
 *          frame (320 kbps, 32 kHz), so 8*1951 bit is the largest possible 
 *          value for MPEG-1 and -2)
 *       
 *          maximum allowed granule/channel size times 4 = 8*2047 bits.,
 *          so this is the absolute maximum supported by the format.
 *
 *          
 *      fullFrameBits:  maximum number of bits available for encoding
 *                      the current frame.
 *
 *      mean_bits:      target number of bits per granule.  
 *
 *      frameLength:
 *
 *      l3_side->ResvMax:   maximum allowed reservoir 
 *
 *      l3_side->ResvSize:  current reservoir size
 */
    *mean_bits = frameLength - l3_side->sideinfo_len * 8;
    l3_side->ResvMax = l3_side->maxmp3buf - frameLength;
    /* main_data_begin has 9 bits in MPEG-1, 8 bits MPEG-2 */
    resvLimit = (8*256)*gfc->mode_gr-8;
    if (l3_side->ResvMax > resvLimit)
	l3_side->ResvMax = resvLimit;
    if (l3_side->ResvMax < 0)
	l3_side->ResvMax = 0;
    assert ( 0 == l3_side->ResvMax % 8 );

    fullFrameBits = *mean_bits + Min(l3_side->ResvSize, l3_side->ResvMax);

#ifndef NOANALYSIS
    if (gfc->pinfo) {
	gfc->pinfo->mean_bits = *mean_bits / (gfc->channels_out*gfc->mode_gr);
	gfc->pinfo->resvsize  = l3_side->ResvSize;
    }
#endif

    return fullFrameBits;
}


/*
  ResvMaxBits
  returns targ_bits:  target number of bits to use for 1 granule
         extra_bits:  amount extra available from reservoir
  Mark Taylor 4/99
*/
static int
ResvMaxBits(lame_internal_flags *gfc, int mean_bits, int *extra_bits)
{
    int ResvSize = gfc->l3_side.ResvSize, ResvMax = gfc->l3_side.ResvMax;
    if (ResvSize*10 >= ResvMax*6) {
	int add_bits = 0;
	if (ResvSize*10 >= ResvMax*9) {
	    /* reservoir is filled over 90% -> forced to use excessed bits */
	    add_bits = ResvSize - (ResvMax * 9) / 10;
	    mean_bits += add_bits;
	    gfc->substep_shaping |= 0x80;
	}
	/* max amount from the reservoir we are allowed to use. ISO says 60% */
	ResvSize = (ResvMax*6)/10 - add_bits;
	if (ResvSize < 0) ResvSize = 0;
    } else {
	/* build up reservoir.  this builds the reservoir a little slower
	 * than FhG.  It could simple be mean_bits/15, but this was rigged
	 * to always produce 100 (the old value) at 128kbs */
	/*    *targ_bits -= (int) (mean_bits/15.2);*/
	if (!(gfc->substep_shaping & 1))
	    mean_bits -= mean_bits/10;
	gfc->substep_shaping &= 0x7f;
    }

    *extra_bits = ResvSize;
    return mean_bits;
}

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
    int extra_bits, tbits, bits, ch;
    int max_bits;  /* maximum allowed bits for this granule */

    /* allocate targ_bits for granule */
    tbits = ResvMaxBits( gfc, mean_bits, &extra_bits);

    max_bits = tbits + extra_bits;
    if (max_bits > MAX_BITS) /* hard limit per granule */
	max_bits = MAX_BITS;

    /* at most increase bits by 1.5*average */
    mean_bits = tbits+mean_bits*3/2;
    tbits /= gfc->channels_out;
    mean_bits /= gfc->channels_out;

    for (bits = 0, ch = 0; ch < gfc->channels_out; ch++) {
	if (ratio[ch].ath_over == 0) {
	    targ_bits[ch] = 126;
	} else {
	    targ_bits[ch] = tbits;
	    if (ratio[ch].pe*(2.0/3) > tbits) {
		targ_bits[ch] = ratio[ch].pe*tbits/700;
		if (targ_bits[ch] > mean_bits) 
		    targ_bits[ch] = mean_bits;
	    }
	}
	bits += targ_bits[ch];
    }
    if (bits > max_bits) {
	for (ch = 0; ch < gfc->channels_out; ch++) {
	    targ_bits[ch]
		= tbits + extra_bits * (targ_bits[ch] - tbits) / bits;
	}
    } else if (bits < tbits) {
	tbits = tbits - bits / gfc->channels_out;
	for (ch = 0; ch < gfc->channels_out; ch++)
	    targ_bits[ch] += tbits;
    }
}




/*************************************************************************
 * calc_xmin
 * Calculate the allowed distortion for each scalefactor band,
 * as determined by the psychoacoustic model.
 * xmin(sb) = ratio(sb) * en(sb) / bw(sb)
 *************************************************************************/
static void
calc_xmin(
    lame_internal_flags *gfc,
    const III_psy_ratio * const ratio,
    const gr_info       * const gi, 
	   FLOAT        * pxmin
    )
{
    int sfb, gsfb, j=0;
    for (gsfb = 0; gsfb < gi->psy_lmax; gsfb++) {
	FLOAT xmin = gfc->ATH.adjust * gfc->ATH.l[gsfb];
	FLOAT en0 = 0.0, x;
	int l = gi->width[gsfb];
	l >>= 1;
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
	FLOAT tmpATH = gfc->ATH.adjust * gfc->ATH.s[sfb];
	int width = gi->width[gsfb], b;

	for (b = 0; b < 3; b++) {
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
static FLOAT
calc_noise(const gr_info  * const gi, const FLOAT rxmin[], FLOAT distort[])
{
    FLOAT max_noise = 0.0;
    int sfb = 0, j = 0, l;

    do {
	FLOAT noise;
	if (j > gi->count1)
	    break;
	noise = *distort;
	l = -gi->width[sfb];
	j -= l;
	if (noise < 0.0) {
	    FLOAT step = POW20(scalefactor(gi, sfb));
	    noise = 0.0;
	    do {
		FLOAT temp;
		temp = fabs(gi->xr[j+l]) - pow43[gi->l3_enc[j+l]] * step; l++;
		noise += temp * temp;
		temp = fabs(gi->xr[j+l]) - pow43[gi->l3_enc[j+l]] * step; l++;
		noise += temp * temp;
	    } while (l < 0);
	    *distort = noise = noise * *rxmin;
	}
	max_noise=Max(max_noise,noise);
    } while (rxmin++, distort++, ++sfb < gi->psymax);

    for (;sfb < gi->psymax; rxmin++, distort++, sfb++) {
	FLOAT noise = *distort;
	l = -gi->width[sfb];
	j -= l;
	if (noise < 0.0) {
	    noise = 0.0;
	    do {
		FLOAT t0 = gi->xr[j+l], t1 = gi->xr[j+l+1];
		noise += t0*t0 + t1*t1;
	    } while ((l += 2) < 0);
	    *distort = noise = noise * *rxmin;
	}
	max_noise=Max(max_noise,noise);
    }

    if (max_noise > 1.0 && gi->block_type == SHORT_TYPE) {
	distort -= sfb;
	max_noise = 0.0;
	for (sfb = gi->sfb_smin; sfb < gi->psymax; sfb += 3) {
	    FLOAT noise = distort[sfb] * distort[sfb+1] * distort[sfb+2];
	    if (max_noise < noise)
		max_noise = noise;
	}
    }
    return Max(max_noise, 1e-20);
}

static FLOAT
calc_noise_allband(
    const gr_info * const gi,
    const FLOAT * const l3_xmin,
    FLOAT distort[],
    FLOAT rxmin[]
    )
{
    int sfb;
    for (sfb = 0; sfb < gi->psymax; sfb++) {
	distort[sfb] = -1.0;
	rxmin[sfb] = 1.0/l3_xmin[sfb];
    }
    return calc_noise(gi, rxmin, distort);
}

/************************************************************************
 *
 *	init_bitalloc()
 *  mt 6/99
 *
 *  initializes xrpow
 *  and returns 0 if all energies in xr are zero, else non-zero.
 ************************************************************************/

static int 
init_bitalloc(lame_internal_flags *gfc, gr_info *const gi, FLOAT xrpow[576])
{
    FLOAT tmp, sum = 0.0;
    int i, end = gi->xrNumMax;
#ifdef HAVE_NASM
    if (gfc->CPU_features.SIMD) {
	extern void pow075_SSE(float *, float *, int, float*);
	if (end) {
	    end = (end + 7) & (~7);
	    pow075_SSE(gi->xr, xrpow, end, &sum);
	}
    } else
#endif
    {
	/*  check if there is some energy we have to quantize
	 *  and calculate xrpow matching our fresh scalefactors */
	for (i = 0; i < end; i++) {
	    tmp = fabs (gi->xr[i]);
	    sum += tmp;
	    xrpow[i] = sqrt (tmp * sqrt(tmp));
	}
    }
    memset(&xrpow[end], 0, sizeof(FLOAT)*(576-end));
    /*  return 1 if we have something to quantize, else 0 */
    if (sum > (FLOAT)1e-20) {
	int j = 0;
	if (gfc->noise_shaping_amp >= 3)
	    j = 1;

	for (i = 0; i < gi->psymax; i++)
	    gfc->pseudohalf[i] = j;
	return gi->psymax;
    }

    return 0;
}

/************************************************************************
 *	init_global_gain()
 *
 * initialize the global_gain using binary search.
 * used by CBR_1st_bitalloc to get a quantizer step size to start with
 ************************************************************************/
static void
init_global_gain(
    lame_internal_flags * const gfc,
    gr_info * const gi,
    int desired_rate,
    int CurrentStep,
    const FLOAT xrpow [576])
{
    int nbits, flag_GoneOver = 0;
    assert(CurrentStep);
    gi->big_values = gi->count1 = gi->xrNumMax;
    desired_rate -= gi->part2_length;
    do {
	if (flag_GoneOver & 2)
	    gi->big_values = gi->count1 = gi->xrNumMax;
	nbits = count_bits(gfc, xrpow, gi);

	if (CurrentStep == 1 || nbits == desired_rate)
	    break; /* nothing to adjust anymore */

	if (nbits > desired_rate) {  
	    /* increase Quantize_StepSize */
	    flag_GoneOver |= 1;

	    if (flag_GoneOver==3) CurrentStep >>= 1;
	    gi->global_gain += CurrentStep;
	} else {
	    /* decrease Quantize_StepSize */
	    flag_GoneOver |= 2;

	    if (flag_GoneOver==3) CurrentStep >>= 1;
	    gi->global_gain -= CurrentStep;
	}
    } while (gi->global_gain < 256u);

    if (gi->global_gain < 0) {
	gi->global_gain = 0;
	nbits = count_bits(gfc, xrpow, gi);
    } else if (gi->global_gain > 255) {
	gi->global_gain = 255;
	gi->big_values = gi->count1 = gi->xrNumMax;
	nbits = count_bits(gfc, xrpow, gi);
    } else if (nbits > desired_rate) {
	gi->global_gain++;
	nbits = count_bits(gfc, xrpow, gi);
    }
    gi->part2_3_length = nbits;
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
    FLOAT distort[SFBMAX], dummy[SFBMAX];

    for (sfb = 0; sfb < gi->psymax; sfb++) {
	dummy[sfb] = 1.0;
	distort[sfb] = -1.0;
    }

    calc_noise(gi, dummy, distort);
    for (j = 0; j < gi->xrNumMax; j++) {
	FLOAT xr = 0.0;
	if (gi->l3_enc[j] != 0)
	    xr = fabs(gi->xr[j]);
	work[j] = xr;
    }

    j = sfb = 0;
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

	allowedNoise = l3_xmin[sfb] - distort[sfb];
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
 *  Returns zero if there is a scalefac which has not been amplified.
 *  Otherwise it returns non-zero.
 *
 *************************************************************************/

inline static int
loop_break(const gr_info * const gi)
{
    int sfb = gi->psymax-1;
    do {
	if (gi->scalefac[sfb] + gi->subblock_gain[gi->window[sfb]] == 0)
	    return 0;
    } while (--sfb >= 0);
    return sfb;
}




/*************************************************************************
 *
 *      inc_scalefac_scale()
 *
 *  turns on scalefac scale and adjusts scalefactors
 *
 *************************************************************************/
 
static void
inc_scalefac_scale (gr_info * const gi, FLOAT distort[])
{
    int sfb = 0;
    do {
	int s = gi->scalefac[sfb], s0;
	if (gi->preflag)
	    s += pretab[sfb];
	s0 = s;
	gi->scalefac[sfb] = s = (s + 1) >> 1;
	if (s*2 != s0)
	    distort[sfb] = -1.0;
    } while (++sfb < gi->psymax);
    gi->preflag = 0;
    gi->scalefac_scale = 1;
}



/*************************************************************************
 *
 *      inc_subblock_gain()
 *
 *  increases the subblock gain and adjusts scalefactors
 *  and returns zero if success.
 *************************************************************************/

static int 
inc_subblock_gain(gr_info * const gi, FLOAT distort[])
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
	for (sfb = gi->sfb_lmax+window; sfb < gi->psymax; sfb += 3)
	    if (gi->scalefac[sfb] > tab[sfb])
		break;

	if (sfb >= gi->psymax)
	    continue;

	if (gi->subblock_gain[window] >= 7)
	    return 1;

	gi->subblock_gain[window]++;
	for (sfb = gi->sfb_lmax+window; sfb < gi->psymax; sfb += 3) {
	    int s = gi->scalefac[sfb] - (4 >> gi->scalefac_scale);
	    distort[sfb] = -1.0;
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
 *  Change the scalefactor value of the band where its noise is not masked.
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
    FLOAT trigger,
    int method,
    int target_bits
    )
{
    int bits, sfb, sfbmax = gi->sfbmax;
    if (sfbmax > gi->psymax)
	sfbmax = gi->psymax;

    if (method < 2) {
	if (trigger <= 1.0)
	    trigger *= 0.95;
	else if (method == 0)
	    trigger = 1.0;
	else
	    trigger = sqrt(trigger);
    }

    for (sfb = 0; sfb < sfbmax; sfb++) {
	if (distort[sfb] < trigger)
	    continue;

	if (method < 3 || (gfc->pseudohalf[sfb] ^= 1))
	    gi->scalefac[sfb]++;
	distort[sfb] = -1.0;
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
	if (inc_subblock_gain(gi, distort) || loop_break(gi))
	    return 0; /* failed */
    } else if (gfc->use_scalefac_scale && !gi->scalefac_scale)
	inc_scalefac_scale(gi, distort);
    else
	return 0;

    return target_bits - gfc->scale_bitcounter(gi);
}



inline static FLOAT
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
	fi0.f = sfpow34 * xr34[0] + (ROUNDFAC_NEAR + MAGIC_FLOAT);
	fi1.f = sfpow34 * xr34[1] + (ROUNDFAC_NEAR + MAGIC_FLOAT);

	if (fi0.i > MAGIC_INT + IXMAX_VAL) return -1;
	if (fi1.i > MAGIC_INT + IXMAX_VAL) return -1;
	t0 = fabs(xr[0]) - (pow43 - MAGIC_INT)[fi0.i] * sfpow;
	t1 = fabs(xr[1]) - (pow43 - MAGIC_INT)[fi1.i] * sfpow;
#else
	fi0.i = (int)(sfpow34 * xr34[0] + ROUNDFAC);
	fi1.i = (int)(sfpow34 * xr34[1] + ROUNDFAC);

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

inline static FLOAT
calc_sfb_noise(const FLOAT * xr, const FLOAT * xr34, int bw, int sf)
{
    FLOAT xfsf = 0.0;
    FLOAT sfpow = POW20(sf);  /*pow(2.0,sf/4.0); */
    FLOAT sfpow34 = IPOW20(sf); /*pow(sfpow,-3.0/4.0); */

    bw >>= 1;
    do {
	fi_union fi0, fi1;
#ifdef TAKEHIRO_IEEE754_HACK
	double t0, t1;
	fi0.f = (t0 = sfpow34 * xr34[0] + MAGIC_FLOAT);
	fi1.f = (t1 = sfpow34 * xr34[1] + MAGIC_FLOAT);
	if (fi0.i > MAGIC_INT + IXMAX_VAL) return -1.0;
	if (fi1.i > MAGIC_INT + IXMAX_VAL) return -1.0;
	fi0.f = t0 + (adj43asm - MAGIC_INT)[fi0.i];
	fi1.f = t1 + (adj43asm - MAGIC_INT)[fi1.i];
	t0 = fabs(xr[0]) - (pow43 - MAGIC_INT)[fi0.i] * sfpow;
	t1 = fabs(xr[1]) - (pow43 - MAGIC_INT)[fi1.i] * sfpow;
#else
	FLOAT t0, t1;
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

static int
adjust_global_gain(
    lame_internal_flags *gfc,
    FLOAT *xp,
    gr_info *gi, FLOAT *distort, int huffbits)
{
    fi_union *fi = (fi_union *)gi->l3_enc;
    FLOAT *xend = &xp[gi->xrNumMax];
    int sfb = 0;

    do {
	int width = gi->width[sfb];
	FLOAT istep;
	xp += width;
	fi += width;
	if (distort[sfb] > 0.0)
	    continue;

	width = -width;
	istep = IPOW20(scalefactor(gi, sfb));
	if (gi->count1 < fi - (fi_union*)gi->l3_enc)
	    gi->count1 = fi - (fi_union*)gi->l3_enc;
	do {
#ifdef TAKEHIRO_IEEE754_HACK
	    double x0 = istep * xp[width  ] + MAGIC_FLOAT;
	    double x1 = istep * xp[width+1] + MAGIC_FLOAT;

	    fi[width  ].f = x0;
	    fi[width+1].f = x1;

	    if (fi[width  ].i >= MAGIC_INT + PRECALC_SIZE) return LARGE_BITS;
	    fi[width  ].f = x0 + (adj43asm - MAGIC_INT)[fi[width  ].i];
	    if (fi[width+1].i >= MAGIC_INT + PRECALC_SIZE) return LARGE_BITS;
	    fi[width+1].f = x1 + (adj43asm - MAGIC_INT)[fi[width+1].i];
	    fi[width  ].i -= MAGIC_INT;
	    fi[width+1].i -= MAGIC_INT;
#else
	    FLOAT x1 = xp[width  ] * istep;
	    FLOAT x2 = xp[width+1] * istep;
	    int rx1 = (int)x1;
	    int rx2 = (int)x2;
	    if (rx1 >= PRECALC_SIZE) return LARGE_BITS;
	    if (rx2 >= PRECALC_SIZE) return LARGE_BITS;
	    fi[width  ].i = (int)(x1 + adj43[rx1]);
	    fi[width+1].i = (int)(x2 + adj43[rx2]);
#endif
	} while ((width += 2) < 0);
	if (!gfc->pseudohalf[sfb])
	    continue;
	istep = 0.634521682242439
	    / IPOW20(scalefactor(gi, sfb) + gi->scalefac_scale);
	for (width = -gi->width[sfb]; width < 0; width++)
	    if (xp[width] < istep)
		fi[width].i = 0;
    } while (++sfb < gi->psymax && xp < xend);

    if (noquant_count_bits(gfc, gi) > huffbits)
	return (++gi->global_gain) - 256;

    return 0;
}

static void
CBR_2nd_bitalloc(
    lame_internal_flags * gfc,
    gr_info *gi,
    FLOAT * xr34,
    FLOAT distort[])
{
    int sfb, j = 0;
    for (sfb = 0; sfb < gi->psymax; sfb++) {
	FLOAT noisenew, noiseold;
	int width = gi->width[sfb];
	distort[sfb] = 0.0;
	if (gi->scalefac[sfb] > 0) {
	    int s0 = gi->scalefac[sfb];
	    noiseold = calc_sfb_noise(&gi->xr[j], &xr34[j], width,
				      scalefactor(gi, sfb));
	    do {
		if (--gi->scalefac[sfb] < 0)
		    break;
		noisenew = calc_sfb_noise(&gi->xr[j], &xr34[j], width,
					  scalefactor(gi, sfb));
	    } while (noisenew <= noiseold);
	    gi->scalefac[sfb]++;
	    if (gi->scalefac[sfb] != s0)
		distort[sfb] = -1.0;
	}
	j += width;
    }
    gfc->scale_bitcounter(gi);
    adjust_global_gain(gfc, xr34, gi, distort, gi->part2_3_length);
}

/************************************************************************
 *
 *  CBR_1st_bitalloc ()
 *
 *  find the best scalefactor and global gain combination
 *  as long as it satisfy the bitrate condition.
 ************************************************************************/

static int
noise_in_sfb21(gr_info *gi, FLOAT distort[], FLOAT threshold)
{
    int sfb;
    for (sfb = gi->sfbmax; sfb < gi->psymax; sfb++)
	if (distort[sfb] >= threshold)
	    return 1;
    return 0;
}

static void
CBR_1st_bitalloc (
    lame_internal_flags *gfc,
    gr_info		* const gi,
    const int           ch,
    const int           targ_bits,  /* maximum allowed bits */
    const III_psy_ratio * const ratio
    )
{
    gr_info gi_w;
    FLOAT distort[SFBMAX], l3_xmin[SFBMAX], rxmin[SFBMAX], bestNoise, newNoise;
    align16 FLOAT xrpow[576];
    int current_method, age;

    if (!init_bitalloc(gfc, gi, xrpow))
	return; /* digital silence */

    gi->global_gain = gfc->OldValue[ch];
    init_global_gain(gfc, gi, targ_bits, gfc->CurrentStep[ch], xrpow);

    gfc->CurrentStep[ch] = (gfc->OldValue[ch] - gi->global_gain) >= 4 ? 4 : 2;
    gfc->OldValue[ch] = gi->global_gain;

    if (gfc->psymodel < 2) 
	return; /* fast mode, no noise shaping, we are ready */

    /* compute the distortion in this quantization */
    /* coefficients and thresholds of ch0(L or Mid) or ch1(R or Side) */
    calc_xmin (gfc, ratio, gi, l3_xmin);

    newNoise = bestNoise = calc_noise_allband(gi, l3_xmin, distort, rxmin);
    current_method = 0;
    age = 3;
    if (bestNoise < 1.0) {
	if (gfc->noise_shaping_stop == 0)
	    goto quit_quantization;
	current_method = 1;
	age = 5;
    }
    if (noise_in_sfb21(gi, distort, bestNoise))
	goto quit_quantization;

    /* BEGIN MAIN LOOP */
    gi_w = *gi;
    for (;;) {
	/* try the new scalefactor conbination on gi_w */
	int huff_bits = amp_scalefac_bands(gfc, &gi_w, distort, newNoise,
					   current_method, targ_bits);
	if (huff_bits > 0) {
	    /* adjust global_gain to fit the available bits */
	    if (adjust_global_gain(gfc, xrpow, &gi_w, distort, huff_bits)) {
		int sfb;
		while (count_bits(gfc, xrpow, &gi_w) > huff_bits
		       && ++gi_w.global_gain < 256u)
		    ;
		for (sfb = 0; sfb < gi->psymax; sfb++)
		    distort[sfb] = -1.0;
	    }

	    /* store this scalefactor combination if it is better */
	    if (gi_w.global_gain != 256
	     && bestNoise > (newNoise = calc_noise(&gi_w, rxmin, distort))) {
		bestNoise = newNoise;
		*gi = gi_w;
		if (bestNoise < 1.0) {
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
	if (--age > 0 && gi_w.global_gain != 256
	    && !noise_in_sfb21(&gi_w, distort, bestNoise))
	    continue;

	/* seems we cannot get a better combination.
	   restart from the best with more finer noise_amp method */
	if (current_method == gfc->noise_shaping_amp)
	    break;
	current_method++;
	gi_w = *gi;
	age = current_method*3+2;
    }
    assert (gi->global_gain < 256);

    CBR_2nd_bitalloc(gfc, gi, xrpow, distort);
 quit_quantization:
    if (gi->psymax != 0
	&& !(gi->block_type != SHORT_TYPE && !(gfc->substep_shaping & 1))
	&& !(gi->block_type == SHORT_TYPE && !(gfc->substep_shaping & 2)))
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
    III_psy_ratio     ratio        [2][2],
    int               targ_bits    [2][2]
    )
{
    lame_internal_flags *gfc=gfp->internal_flags;
    FLOAT res_factor;
    int gr, ch, totbits, mean_bits;
    int analog_silence_bits, max_frame_bits;
    
    gfc->bitrate_index = gfc->VBR_max_bitrate;
    max_frame_bits = ResvFrameBegin (gfp, &mean_bits);

    gfc->bitrate_index = 1;
    mean_bits = getframebits(gfp) - gfc->l3_side.sideinfo_len * 8;
    analog_silence_bits = mean_bits / (gfc->mode_gr * gfc->channels_out);

    gfc->bitrate_index = 0;
    mean_bits = getframebits(gfp) - gfc->l3_side.sideinfo_len * 8;
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

    /* sum of target bits */
    totbits=0;
    for (gr = 0; gr < gfc->mode_gr; gr++) {
	for (ch = 0; ch < gfc->channels_out; ch++) {
	    if (targ_bits[gr][ch] > MAX_BITS) 
		targ_bits[gr][ch] = MAX_BITS;
	    totbits += targ_bits[gr][ch];
	}
    }

    /* repartion target bits if needed */
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
    III_psy_ratio      ratio        [2][2])
{
    lame_internal_flags *gfc=gfp->internal_flags;
    int targ_bits[2][2], mean_bits, gr, ch;

    ABR_calc_target_bits (gfp, ratio, targ_bits);
    for (gr = 0; gr < gfc->mode_gr; gr++) {
	for (ch = 0; ch < gfc->channels_out; ch++) {
	    gr_info *gi = &gfc->l3_side.tt[gr][ch];
	    CBR_1st_bitalloc(gfc, gi, ch, targ_bits[gr][ch], &ratio[gr][ch]);
	    iteration_finish_one(gfc, gr, ch);
	    gfc->l3_side.ResvSize -= gi->part2_length + gi->part2_3_length;
	}
    }

    /*  find a bitrate which can refill the resevoir to positive size.
     */
    gfc->bitrate_index = gfc->VBR_min_bitrate;
    for (; gfc->bitrate_index <= gfc->VBR_max_bitrate; gfc->bitrate_index++)
	if (ResvFrameBegin(gfp, &mean_bits) >= 0)
	    break;
    assert(gfc->bitrate_index <= gfc->VBR_max_bitrate);

    gfc->l3_side.ResvSize += mean_bits;
}






/************************************************************************
 *
 *      iteration_loop()
 *
 *  encodes one frame of MP3 data with constant bitrate
 *
 ************************************************************************/

void 
iteration_loop(
    lame_global_flags *gfp, 
    III_psy_ratio      ratio        [2][2])
{
    lame_internal_flags *gfc=gfp->internal_flags;
    int targ_bits[2], mean_bits, gr, ch;

    ResvFrameBegin (gfp, &mean_bits);
    mean_bits /= gfc->mode_gr;

    for (gr = 0; gr < gfc->mode_gr; gr++) {
	/*  calculate needed bits */
	on_pe(gfc, ratio[gr], targ_bits, mean_bits);

	for (ch=0; ch < gfc->channels_out; ch ++) {
	    gr_info *gi = &gfc->l3_side.tt[gr][ch];
	    CBR_1st_bitalloc(gfc, gi, ch, targ_bits[ch], &ratio[gr][ch]);
	    iteration_finish_one(gfc, gr, ch);
	    gfc->l3_side.ResvSize += mean_bits / gfc->channels_out
		- gi->part2_length - gi->part2_3_length;
	}
    }
}


/************************************************************************
 *
 * VBR related code
 *  Takehiro TOMINAGA 2002-12-31
 * based on Mark and Robert's code and suggestions
 *
 ************************************************************************/
static void
bitpressure_strategy(gr_info *gi, FLOAT *pxmin)
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

/* find_scalefac() calculates a quantization step size which would
 * introduce as much noise as allowed (= as less bits as possible). */
inline static int
find_scalefac(const FLOAT * xr, const FLOAT * xr34, FLOAT l3_xmin, int bw,
	      int shortflag, int sf)
{
    int sf_ok = 10000, delsf = 8, sfmin = -7*2, endflag = 0;
    /* search range of sf.
       on shoft blocks, it is large because of subblock_gain */
    if (shortflag)
	sfmin = -7*4-7*2;

    do {
	FLOAT xfsf = calc_sfb_noise_fast(xr, xr34, bw, sf);
	if (xfsf > l3_xmin) {
	    /* distortion.  try a smaller scalefactor */
	    endflag |= 1;
	    if (endflag == 3)
		delsf >>= 1;
	    sf -= delsf;
	    if (sf < sfmin) {
		sf = sfmin;
		endflag = 3;
	    }
	} else {
	    if (xfsf >= 0.0)
		sf_ok = sf;
	    endflag |= 2;
	    if (endflag == 3)
		delsf >>= 1;
	    sf += delsf;
	    if (sf > 255) {
		sf = 255;
		endflag = 3;
	    }
	}
    } while (delsf != 0);

    if (sfmin <= sf_ok && sf_ok < 256)
	sf = sf_ok;
    return sf;
}



/* {long|short}_block_scalefacs() calculates global_gain and
    prepare scalefac[] to satisfy the spec (and the noise threshold). */
static void
short_block_scalefacs(gr_info * gi, int vbrmax)
{
    int sfb, b, newmax1, maxov[2][3];

    maxov[0][0] = maxov[0][1] = maxov[0][2]
	= maxov[1][0] = maxov[1][1] = maxov[1][2]
	= newmax1 = vbrmax;

    for (sfb = 0; sfb < gi->psymax; ) {
	for (b = 0; b < 3; b++) {
	    maxov[0][b] = Min(gi->scalefac[sfb] + 2*max_range_short[sfb],
			      maxov[0][b]);
	    maxov[1][b] = Min(gi->scalefac[sfb] + 4*max_range_short[sfb],
			      maxov[1][b]);
	    sfb++;
	}
    }

    /* should we use subblcok_gain ? */
    for (b = 0; b < 3; b++) {
	if (vbrmax > maxov[0][b] + 7*8)
	    vbrmax = maxov[0][b] + 7*8;
	if (newmax1 > maxov[1][b] + 7*8)
	    newmax1 = maxov[1][b] + 7*8;
    }
    gi->scalefac_scale = 0;
    if (vbrmax > newmax1) {
	vbrmax = newmax1;
	gi->scalefac_scale = 1;
    }
    if (vbrmax < 0)
	vbrmax = 0;
    if (vbrmax > 255)
	vbrmax = 255;
    gi->global_gain = vbrmax;

    for (b = 0; b < 3; b++) {
	int sbg = (vbrmax - maxov[gi->scalefac_scale][b] + 7) / 8;
	if (sbg < 0)
	    sbg = 0;
	assert(sbg <= 7);
	gi->subblock_gain[b] = sbg;
    }
}


static void
long_block_scalefacs(const lame_internal_flags *gfc, gr_info * gi, int vbrmax)
{
    int sfb, maxov0, maxov1, maxov0p, maxov1p;

    /* we can use 4 strategies. [scalefac_scale = (0,1), pretab = (0,1)] */
    maxov0  = maxov1 = maxov0p = maxov1p = vbrmax;

    /* find out the distance from most important scalefactor band
       (which wants the largest scalefactor value)
       to largest possible range */
    for (sfb = 0; sfb < gi->psymax; ++sfb) {
	maxov0  = Min(gi->scalefac[sfb] + 2*max_range_long[sfb], maxov0);
	maxov1  = Min(gi->scalefac[sfb] + 4*max_range_long[sfb], maxov1);
	maxov0p = Min(gi->scalefac[sfb] + 2*(max_range_long[sfb]+pretab[sfb]),
		      maxov0p);
	maxov1p = Min(gi->scalefac[sfb] + 4*(max_range_long[sfb]+pretab[sfb]),
		      maxov1p);
    }

    if (gfc->mode_gr == 1)
	maxov1p = maxov0p = 10000;

    gi->preflag = 0;
    if (maxov0 == vbrmax) {
	gi->scalefac_scale = 0;
    }
    else if (maxov0p == vbrmax) {
	gi->scalefac_scale = 0;
	gi->preflag = 1;
    }
    else if (maxov1 == vbrmax) {
	gi->scalefac_scale = 1;
    }
    else if (maxov1p == vbrmax) {
	gi->scalefac_scale = 1;
	gi->preflag = 1;
    } else {
	gi->scalefac_scale = 1;
	if (maxov1 > maxov1p) {
	    gi->preflag = 1;
	    vbrmax = maxov1p;
	} else {
	    vbrmax = maxov1;
	}
    }
    if (vbrmax < 0)
	vbrmax = 0;
    if (vbrmax > 255)
	vbrmax = 255;
    gi->global_gain = vbrmax;
}

static void
set_scalefactor_values(gr_info *gi)
{
    int ifqstep = (1 << (1 + gi->scalefac_scale)) - 1, sfb = 0;
    do {
	int s = (gi->global_gain - gi->scalefac[sfb]
		 - gi->subblock_gain[gi->window[sfb]]*8 + ifqstep)
		     >> (1 + gi->scalefac_scale);
	if (gi->preflag)
	    s -= pretab[sfb];
	if (s < 0)
	    s = 0;
	gi->scalefac[sfb] = s;
    } while (++sfb < gi->psymax);
}


static int
noisesfb(gr_info *gi, FLOAT * xr34, FLOAT * l3_xmin, int startsfb)
{
    int sfb, j = 0;
    for (sfb = 0; sfb < gi->psymax; sfb++) {
	int width = gi->width[sfb];
	if (sfb >= startsfb) {
	    FLOAT noise = calc_sfb_noise(&gi->xr[j], &xr34[j], width,
					 scalefactor(gi, sfb));
	    if (noise > l3_xmin[sfb])
		return sfb;
	    if (noise < 0.0)
		return -1;
	}
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
	sfb = noisesfb(&gi_w, xr34, l3_xmin, sfb);
	if (sfb >= 0) {
	    if (sfb >= gi->sfbmax) { /* noise in sfb21 */
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

static int
VBR_3rd_bitalloc(gr_info *gi, FLOAT * xr34, FLOAT * l3_xmin)
{
    /* note: we cannot use calc_noise() because l3_enc[] is not calculated
       at this point */
    int sfb, j, r = 0;
    for (j = sfb = 0; sfb < gi->psymax; sfb++) {
	if (calc_sfb_noise(&gi->xr[j], &xr34[j], gi->width[sfb],
			   scalefactor(gi, sfb)) < 0) {
	    if (gi->scalefac[sfb] == 0 || r < 0)
		r = -2;
	    else {
		r = -1;
		l3_xmin[sfb] *= 1.0029;
	    }
	}
	j += gi->width[sfb];
    }
    if (r)
	return r;

    for (j = sfb = 0; sfb < gi->psymax; sfb++) {
	while (gi->scalefac[sfb] > 0) {
	    gi->scalefac[sfb]--;
	    if (calc_sfb_noise(&gi->xr[j], &xr34[j], gi->width[sfb],
			       scalefactor(gi, sfb)) > l3_xmin[sfb]) {
		gi->scalefac[sfb]++;
		break;
	    }
	}
	j += gi->width[sfb];
    }
    return 0;
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
    lame_internal_flags * gfc,
    gr_info *gi,
    FLOAT * xr34,
    FLOAT * l3_xmin)
{
    int vbrmax, sfb, j;

    sfb = j = 0;
    vbrmax = -10000;
    do {
	int width = gi->width[sfb], gain;
	gain = find_scalefac(&gi->xr[j], &xr34[j], l3_xmin[sfb], width,
			     gi->block_type == SHORT_TYPE, gi->global_gain);
	j += width;
	gi->scalefac[sfb] = gain;
	if (vbrmax < gain)
	    vbrmax = gain;
    } while (++sfb < gi->psymax);

    if (gi->block_type == SHORT_TYPE)
	short_block_scalefacs(gi, vbrmax);
    else
	long_block_scalefacs(gfc, gi, vbrmax);
    set_scalefactor_values(gi);

    /* ensure there's no noise */
    VBR_2nd_bitalloc(gfc, gi, xr34, l3_xmin);

    /* reduce bitrate if possible */
    j = VBR_3rd_bitalloc(gi, xr34, l3_xmin);
    if (j)
	return j;

    /* quantize xr34 */
    gi->big_values = gi->count1 = gi->xrNumMax;
    gi->part2_3_length = count_bits(gfc, xr34, gi);
    if (gi->part2_3_length >= LARGE_BITS)
	return -2;

    /* encode scalefacs */
    gfc->scale_bitcounter(gi);
    assert(gi->part2_length < LARGE_BITS);

    if (gi->part2_3_length > MAX_BITS) {
	FLOAT xrtmp[576];
	trancate_smallspectrums(gfc, gi, l3_xmin, xrtmp);
    }

    if (gfc->use_best_huffman == 2)
	best_huffman_divide(gfc, gi);

    if (gi->part2_3_length + gi->part2_length > MAX_BITS)
	return -2;

    assert(gi->global_gain < 256u);

    return 0;
}


void 
VBR_iteration_loop(lame_global_flags *gfp, III_psy_ratio ratio[2][2])
{
    lame_internal_flags *gfc=gfp->internal_flags;
    align16 FLOAT xrpow[576];
    FLOAT l3_xmin[2][2][SFBMAX];
    int mean_bits, max_frame_bits, used_bits;
    int ch, gr, ath_over = 0;

    for (gr = 0; gr < gfc->mode_gr; gr++) {
	for (ch = 0; ch < gfc->channels_out; ch++) {
     	    calc_xmin (gfc, &ratio[gr][ch],
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
		if (!init_bitalloc(gfc, gi, xrpow))
		    continue; /* digital silence */

		gi->global_gain = gfc->OldValue[ch];
		for (;;) {
		    int ret
			= VBR_noise_shaping(gfc, gi, xrpow, l3_xmin[gr][ch]);
		    if (ret == 0)
			break;
		    if (ret == -2)
			bitpressure_strategy(gi, l3_xmin[gr][ch]);
		}

		gfc->OldValue[ch] = gi->global_gain;
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
	    if (gi->psymax != 0
		&& !(gi->block_type != SHORT_TYPE && !(gfc->substep_shaping & 1))
		&& !(gi->block_type == SHORT_TYPE && !(gfc->substep_shaping & 2)))
		trancate_smallspectrums(gfc, gi, l3_xmin[gr][ch], xrpow);
	    iteration_finish_one(gfc, gr, ch);
	    gfc->l3_side.ResvSize -= gi->part2_length + gi->part2_3_length;
	}
    }

    /*  find a bitrate which can refill the resevoir to positive size. */
    gfc->bitrate_index = 1;
    if (ath_over || gfp->VBR_hard_min)
	gfc->bitrate_index = gfc->VBR_min_bitrate;
    for (; gfc->bitrate_index <= gfc->VBR_max_bitrate; gfc->bitrate_index++)
	if (ResvFrameBegin (gfp, &mean_bits) >= 0)
	    break;
    assert (gfc->bitrate_index <= gfc->VBR_max_bitrate);

    gfc->l3_side.ResvSize += mean_bits;
}



#ifndef NOANALYSIS
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
static void
set_pinfo (
    lame_internal_flags *gfc,
         gr_info        * const gi,
    const III_psy_ratio * const ratio, 
    const int           gr,
    const int           ch )
{
    int i, j, end, bw, sfb, sfb2, over;
    FLOAT en0, en1, tot_noise=0.0, over_noise=0.0, max_noise;
    FLOAT ifqstep = 0.5 * (1+gi->scalefac_scale);
    FLOAT l3_xmin[SFBMAX], distort[SFBMAX], dummy[SFBMAX];

    calc_xmin (gfc, ratio, gi, l3_xmin);
    max_noise = calc_noise_allband(gi, l3_xmin, distort, dummy);

    over = j = 0;
    for (sfb2 = 0; sfb2 < gi->psy_lmax; sfb2++) {
	bw = gi->width[sfb2];
	end = j + bw;
	for (en0 = 0.0; j < end; j++) 
	    en0 += gi->xr[j] * gi->xr[j];
	en0=Max(en0, 1e-20);
	/* convert to MDCT units */
	en1=1e15/bw;  /* scaling so it shows up on FFT plot */
	gfc->pinfo->  en[gr][ch][sfb2] = en1*en0;
	gfc->pinfo-> thr[gr][ch][sfb2] = en1*l3_xmin[sfb2];
	gfc->pinfo->xfsf[gr][ch][sfb2] = en1*l3_xmin[sfb2]*distort[sfb2];
	en1 = FAST_LOG10(distort[sfb2]);
	if (en1 > 0.0) {
	    over++;
	    over_noise += en1;
	}
	tot_noise += en1;

	gfc->pinfo->LAMEsfb[gr][ch][sfb2] = 0;
	if (gi->preflag && sfb2>=11)
	    gfc->pinfo->LAMEsfb[gr][ch][sfb2] = -ifqstep*pretab[sfb2];
	if (gi->scalefac[sfb2]>=0)
	    gfc->pinfo->LAMEsfb[gr][ch][sfb2] -= ifqstep*gi->scalefac[sfb2];
    } /* for sfb */

    for (sfb = gi->sfb_smin; sfb2 < gi->psymax; sfb++) {
	for (i = 0; i < 3; i++, sfb2++) {
	    bw = gi->width[sfb2];
	    end = j + bw;
	    for (en0 = 0.0; j < end; j++)
		en0 += gi->xr[j] * gi->xr[j];

	    en0 = Max(en0, 1e-20);
	    /* convert to MDCT units */
	    en1=1e15/bw;  /* scaling so it shows up on FFT plot */
	    gfc->pinfo->  en_s[gr][ch][3*sfb+i] = en1*en0;
	    gfc->pinfo-> thr_s[gr][ch][3*sfb+i] = en1*l3_xmin[sfb2];
	    gfc->pinfo->xfsf_s[gr][ch][3*sfb+i] = en1*l3_xmin[sfb2]*distort[sfb2];
	    en1 = FAST_LOG10(distort[sfb2]);
	    if (en1 > 0.0) {
		over++;
		over_noise += en1;
	    }
	    tot_noise += distort[sfb2];

	    gfc->pinfo->LAMEsfb_s[gr][ch][3*sfb+i]
		= -2.0*gi->subblock_gain[i] - ifqstep*gi->scalefac[sfb2];
	}
    } /* block type short */

    gfc->pinfo->LAMEqss     [gr][ch] = gi->global_gain;
    gfc->pinfo->LAMEmainbits[gr][ch] = gi->part2_3_length + gi->part2_length;
    gfc->pinfo->LAMEsfbits  [gr][ch] = gi->part2_length;

    gfc->pinfo->over      [gr][ch] = over;
    gfc->pinfo->max_noise [gr][ch] = FAST_LOG10(max_noise) * 10.0;
    gfc->pinfo->over_noise[gr][ch] = over_noise * 10.0;
    gfc->pinfo->tot_noise [gr][ch] = tot_noise;
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

void
set_frame_pinfo(lame_internal_flags *gfc, III_psy_ratio   ratio    [2][2])
{
    int gr, ch;

    /* for every granule and channel common data */
    memset(gfc->pinfo->  en_s, 0, sizeof(gfc->pinfo->  en_s));
    memset(gfc->pinfo-> thr_s, 0, sizeof(gfc->pinfo-> thr_s));
    memset(gfc->pinfo->xfsf_s, 0, sizeof(gfc->pinfo->xfsf_s));
    memset(gfc->pinfo->    en, 0, sizeof(gfc->pinfo->    en));
    memset(gfc->pinfo->   thr, 0, sizeof(gfc->pinfo->   thr));
    memset(gfc->pinfo->  xfsf, 0, sizeof(gfc->pinfo->  xfsf));

    for (gr = 0; gr < gfc->mode_gr; gr ++) {
        for (ch = 0; ch < gfc->channels_out; ch ++) {
            gr_info *gi = &gfc->l3_side.tt[gr][ch];
	    int scalefac_sav[SFBMAX];
	    memcpy(scalefac_sav, gi->scalefac, sizeof(scalefac_sav));

	    /* reconstruct the scalefactors in case SCFSI was used */
	    if (gr == 1) {
		int sfb;
		for (sfb = 0; sfb < gi->sfb_lmax; sfb++) {
		    if (gi->scalefac[sfb] < 0) /* scfsi */
			gi->scalefac[sfb] = gfc->l3_side.tt[0][ch].scalefac[sfb];
		}
	    }

	    set_pinfo (gfc, gi, &ratio[gr][ch], gr, ch);
	    memcpy(gi->scalefac, scalefac_sav, sizeof(scalefac_sav));
	} /* for ch */
    }    /* for gr */
}
#endif /* #ifndef NOANALYSIS */
