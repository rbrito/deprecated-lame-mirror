/*
 * MP3 quantization
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
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#ifdef WITH_DMALLOC
# include <dmalloc.h>
#endif
#include <assert.h>
#include <stdlib.h>

#include "encoder.h"
#include "bitstream.h"
#include "quantize.h"
#include "quantize_pvt.h"
#include "tables.h"
#include "psymodel.h"

#ifdef HAVE_NASM
extern void pow075_SSE(float *, float *, int, float*);
extern void pow075_3DN(float *, float *, int, float*);
extern void sumofsqr_3DN(const float *, int, float *);
extern void calc_noise_sub_3DN(const float *, const int *, int, int, float *);
extern void quantize_ISO_3DN(const float *, int, int, int *, int);
extern void quantize_ISO_SSE(const float *, int, int, int *);
extern void quantize_ISO_SSE2(const float *, int, int, int *);
extern float calc_sfb_noise_fast_3DN(lame_t gfc, int j, int bw, int sf);
extern float calc_sfb_noise_3DN(lame_t gfc, int j, int bw, int sf);
extern float xrmax_MMX(float *start, float *end);
#endif

#ifdef __x86_64__
# include "x86_64/quantize_sub.c"
#endif

static const char max_range_short[SBMAX_s*3] = {
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    0, 0, 0
};
static const char max_range_long[SBMAX_l] = {
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 0
};

/*
 * ResvFrameBegin:
 *  Called (repeatedly) at the beginning of a frame. Updates the maximum
 *  size of the reservoir.
 *
 * Meaning of the variables:
 *    maxmp3buf:
 *      Number of bytes allowed to encode one frame. You can take 511 byte
 *      from the bit reservoir and at most 1440 byte from the current
 *      frame (320 kbps, 32 kHz or 160kbps, 16kHz), so 1951 byte is the
 *      largest possible value of "buffer size" for MPEG-1 and -2. But the
 *      specification says another limitations. See "Background information".
 *
 *    mean_bytes:     target number of frame size (byte).
 *    gfc->ResvMax:   maximum allowed reservoir size (byte).
 *    gfc->ResvSize:  current reservoir size (byte).
 *
 * Background information:
 *  This is the "corrected" text based on the ISO standard.
 *
 *  1) The following rule can be used to calculate the maximum 
 *     number of bits used for one frame:
 *     At the highest possible bitrate of Layer III (320 kbps 
 *     48 kHz) the frames are designed to have constant length,
 *     i.e. the frame length is:
 *
 *         320 kbps * 1152/48 kHz = 7680 bit = 960 byte
 *
 *     This value is used as the maximum buffer at lower bitrates (than
 *     320 kbps). At 64 kbps mono or 128 kbps stereo the main granule
 *     length is 64 kbps * 576/48 kHz = 768 bit(per granule and channel)
 *     at 48 kHz sampling frequency. 
 *     This means that there is a maximum deviation (short time buffer 
 *     [= reservoir]) of 7680 - 2*2*768 = 4608 bits is allowed at 64 kbps. 
 *     The actual deviation is equal to the number of bytes with the 
 *     meaning of octets denoted by the main_data_end offset pointer. 
 *     The actual maximum deviation is (2^9-1)*8 bit = 4088 bits 
 *     for MPEG-1 and (2^8-1)*8 bit for MPEG-2, both are hard limits.
 *     ... The exchange of buffer bits between the left and right channel
 *     is allowed without restrictions except dual channel.
 *     Because of the constructed constraint on the buffer size
 *     main_data_end is always set to 0 in the case of bit_rate_index==14, 
 *     i.e. bitrate is 320 kbps for MPEG-1, 160kbps for MPEG-2. In this
 *     case all data are allocated between adjacent sync words, i.e. there
 *     is no buffering at all.
 */
static int
ResvFrameBegin(lame_t gfc, int mean_bytes)
{
    gfc->ResvMax = (gfc->maxmp3buf - mean_bytes)*8;
    /* main_data_begin has 9 bits in MPEG-1, 8 bits MPEG-2 */
    if (gfc->ResvMax > (8*256)*gfc->mode_gr-8)
	gfc->ResvMax = (8*256)*gfc->mode_gr-8;
    if (gfc->ResvMax < 0)
	gfc->ResvMax = 0;
    assert((gfc->ResvMax & 7) == 0);

#ifndef NOANALYSIS
    if (gfc->pinfo) {
	gfc->pinfo->mean_bits = mean_bytes*8;
	gfc->pinfo->resvsize  = gfc->ResvSize;
    }
#endif

    return mean_bytes*8 + Min(gfc->ResvSize, gfc->ResvMax);
}

/*************************************************************************
 *	calc_xmin()
 * Calculate the allowed noise threshold for each scalefactor band,
 * as determined by the psychoacoustic model.
 * xmin(sb) = ratio(sb) * en(sb)
 *************************************************************************/
static void
calc_xmin(
    lame_t gfc,
    const III_psy_ratio * const ratio,
    const gr_info       * const gi,
	   FLOAT        * xmin
    )
{
    int sfb, gsfb, j=0;
    for (gsfb = 0; gsfb < gi->psy_lmax; gsfb++) {
	FLOAT threshold = gi->ATHadjust * gfc->ATH.l[gsfb];
	FLOAT x = ratio->en.l[gsfb];
	int l = gi->wi[gsfb].width;
	j -= l;
	if (x > (FLOAT)0.0) {
	    x = ratio->thm.l[gsfb] / x;
#ifdef __x86_64__
	    sumofsqr_SSE(&gi->xr[j], l, &x);
#else
#ifdef HAVE_NASM
	    if (gfc->CPU_features.AMD_3DNow)
		sumofsqr_3DN(&gi->xr[j], l, &x);
	    else
#endif
	    {
		FLOAT en0 = (FLOAT)0.0;
		do {
		    en0 += gi->xr[j+l  ] * gi->xr[j+l  ];
		    en0 += gi->xr[j+l+1] * gi->xr[j+l+1];
		} while ((l+=2) < 0);
		x *= en0;
	    }
#endif
	    x *= gfc->masking_lower;
	    if (threshold < x)
		threshold = x;
	}
	*xmin++ = threshold;
    }   /* end of long block loop */

    for (sfb = gi->sfb_smin; gsfb < gi->psymax; sfb++, gsfb += 3) {
	FLOAT tmpATH = gi->ATHadjust * gfc->ATH.s[sfb];
	int b;
	for (b = 0; b < 3; b++) {
	    FLOAT threshold = tmpATH, x = ratio->en.s[sfb][b];
	    int l = gi->wi[gsfb].width;
	    j -= l;
	    if (x > (FLOAT)0.0) {
		x = ratio->thm.s[sfb][b] / x;
#ifdef __x86_64__
		sumofsqr_SSE(&gi->xr[j], l, &x);
#else
#ifdef HAVE_NASM
		if (gfc->CPU_features.AMD_3DNow)
		    sumofsqr_3DN(&gi->xr[j], l, &x);
		else
#endif
		{
		    FLOAT en0 = (FLOAT)0.0;
		    do {
			en0 += gi->xr[j+l  ] * gi->xr[j+l  ];
			en0 += gi->xr[j+l+1] * gi->xr[j+l+1];
		    } while ((l+=2) < 0);
		    x *= en0;
		}
#endif
		x *= gfc->masking_lower_short;
		if (threshold < x)
		    threshold = x;
	    }
	    *xmin++ = threshold;
	}   /* b */
    }   /* end of short block sfb loop */
}

/*************************************************************************
 *	calc_noise()
 * mt 5/99
 * calculate the quantization noise and store the ratio to the threshold.
 *************************************************************************/
static FLOAT
calc_noise(
    lame_t gfc, const gr_info * const gi, const FLOAT rxmin[], FLOAT distort[])
{
    FLOAT max_noise = (FLOAT)1.0;
    int sfb = 0, j = 0, l;

    do {
	FLOAT noise;
	if (j > gi->big_values)
	    break;
	noise = *distort;
	l = gi->wi[sfb].width;
	j -= l;
	if (noise < (FLOAT)0.0) {
	    int sf = 255;
	    if (gi->scalefac[sfb] >= 0)
		sf = scalefactor(gi, sfb);
#ifdef HAVE_NASM
	    if (gfc->CPU_features.AMD_3DNow) {
		calc_noise_sub_3DN(&absxr[j], &gi->l3_enc[j], l, sf, &noise);
	    } else
#endif
	    {
		FLOAT step = POW20(sf);
		noise = (FLOAT)0.0;
		do {
		    FLOAT temp;
		    temp = absxr[j+l] - pow43[gi->l3_enc[j+l]] * step; l++;
		    noise += temp * temp;
		    temp = absxr[j+l] - pow43[gi->l3_enc[j+l]] * step; l++;
		    noise += temp * temp;
		} while (l < 0);
	    }
	    *distort = (noise *= *rxmin);
	}
	max_noise = Max(max_noise, noise);
    } while (rxmin++, distort++, ++sfb < gi->psymax);

    for (;sfb < gi->psymax; rxmin++, distort++, sfb++) {
	FLOAT noise;
	if (j > gi->count1)
	    break;
	noise = *distort;
	l = gi->wi[sfb].width;
	j -= l;
	if (noise < (FLOAT)0.0) {
	    FLOAT step = (FLOAT)0.0;
	    if (gi->scalefac[sfb] >= 0)
		step = POW20(scalefactor(gi, sfb)) * pow43[1];
	    noise = (FLOAT)0.0;
	    do {
		FLOAT t0 = absxr[j+l], t1 = absxr[j+l+1];
		if (gi->l3_enc[j+l  ]) t0 -= step;
		noise += t0*t0;
		if (gi->l3_enc[j+l+1]) t1 -= step;
		noise += t1*t1;
	    } while ((l+=2) < 0);
	    *distort = (noise *= *rxmin);
	}
	max_noise = Max(max_noise, noise);
    }

    for (;sfb < gi->psymax; rxmin++, distort++, sfb++) {
	FLOAT noise = *distort;
	l = gi->wi[sfb].width;
	j -= l;
	if (noise < (FLOAT)0.0) {
#ifdef __x86_64__
	    noise = *rxmin;
	    sumofsqr_SSE(&absxr[j], l, &noise);
#else
#ifdef HAVE_NASM
	    if (gfc->CPU_features.AMD_3DNow) {
		noise = *rxmin;
		sumofsqr_3DN(&absxr[j], l, &noise);
	    } else
#endif
	    {
		noise = (FLOAT)0.0;
		do {
		    FLOAT t0 = absxr[j+l], t1 = absxr[j+l+1];
		    noise += t0*t0 + t1*t1;
		} while ((l += 2) < 0);
		noise *= *rxmin;
	    }
#endif
	    *distort = noise;
	}
	max_noise = Max(max_noise, noise);
    }

    if (max_noise > (FLOAT)1.0 && gi->block_type == SHORT_TYPE) {
	distort -= sfb;
	max_noise = (FLOAT)1.0;
	for (sfb = gi->sfb_smin; sfb < gi->psymax; sfb += 3) {
	    FLOAT noise = 1.0;
	    if (distort[sfb  ] > 1.0) noise  = distort[sfb  ];
	    if (distort[sfb+1] > 1.0) noise *= distort[sfb+1];
	    if (distort[sfb+2] > 1.0) noise *= distort[sfb+2];
	    max_noise += FAST_LOG10(noise);
	}
    }
    return max_noise;
}

static FLOAT
quantEstimate(
    lame_t gfc, const gr_info * const gi, const FLOAT rxmin[], FLOAT distort[])
{
    FLOAT noise = calc_noise(gfc, gi, rxmin, distort);

    if (noise <= (FLOAT)1.0) {
	return (FLOAT) (-(LARGE_BITS - gi->part2_3_length - gi->part2_length));
    }
    return noise;
}

static FLOAT
calc_noise_allband(
    lame_t gfc,
    const gr_info * const gi,
    const FLOAT * const xmin,
    FLOAT distort[],
    FLOAT rxmin[]
    )
{
    int sfb;
    for (sfb = 0; sfb < gi->psymax; sfb++) {
	distort[sfb] = -1.0;
	rxmin[sfb] = 1.0/xmin[sfb];
    }
    return calc_noise(gfc, gi, rxmin, distort);
}

/************************************************************************
 *
 *	init_bitalloc()
 *  mt 6/99
 *
 *  initializes xr34 and gfc->pseudohalf[]
 *  and returns 0 if all energies in xr are zero, else non-zero.
 ************************************************************************/

#ifdef USE_IEEE754_HACK
static const int pow075_table0[] = {
/*  (32/ 9)^.75, (64/ 9)^.75, (128/ 9)^.75,(256/ 9)^.75 */
    0x0A5B6EB1,  0x116B28F5,  0x1D4B6220,  0x314468B9,
/*  (32/11)^.75, (64/11)^.75, (128/11)^.75,(256/11)^.75 */
    0x08E8F61B,  0x0EFC1ABC,  0x1933886F,  0x2A622451,
/*  (32/14)^.75, (64/14)^.75, (128/14)^.75,(256/14)^.75 */
    0x076F8F2F,  0x0C816474,  0x1508142B,  0x235EE7B5,
};
#if 0
static const char pow075_table1[] = {
    9,  /* 1.62500 < mantissa < 2.00000 => -(44/512) < x-1 < (64/512) */
    11, /* 1.28125 < mantissa < 1.62500 => -(61/512) < x-1 < (60/512) */
    14  /* 1.00000 < mantissa < 1.28125 => -(64/512) < x-1 < (62/512) */
};
#endif
static void pow075sub(fi_union *p)
{
    int mantissa, exponent;
    int adj, res, x;

    exponent = p->i >> 23;
    if (exponent <= 0)
	return; /* de-normalized number or zero or minus number */

    x = mantissa = (p->i & 0x7fffff) | 0x800000;
    mantissa *= 9;
    adj = exponent & 3;
    if (mantissa < 0x00A40000) /* 1.28125 * (1<<23) */
	mantissa += x*3, adj+=4;
    if (mantissa < 0x00D00000) /* 1.625 * (1<<23) */
	mantissa += x*2, adj+=4;

    mantissa -= 1 << 27;

    /* when x \simeq 1, x^(3/4) = 1 + (x-1)*3/4 - ... */
    x = mantissa * 3;
    res = (1 << 29) + x;
    x = ((x * 4) * (uint64_t)mantissa) >> 32;
    res -= x;
    x = ((int)((x * (uint64_t)0x6AAAAAAB) >> 32) * (uint64_t)mantissa) >> 32;
    res += x << 5;
    x = (((x*9) * (uint64_t)mantissa) >> 32) << 6;
    res -= x;
    p->f = ((uint64_t)res * pow075_table0[adj]) >> 32;
    p->i += ((exponent>>2)*3 - 32*3 - 55 + 32) << 23;
}

static void pow075(lame_t gfc, float *xr, float end, float *pmax)
{
    int i, max = 0;
    for (i = 0; i < end; i++) {
	int tmp = ((fi_union*)xr)[i].i & 0x7fffffff;
	if (max < tmp)
	    max = tmp;
	((fi_union*)absxr)[i].i = ((fi_union*)xr34)[i].i = tmp;
	pow075sub((fi_union*)&xr34[i]);
    }
    ((fi_union*)pmax)->i = max;
}
#else
static void pow075(lame_t gfc, float *xr, float end, float *pmax)
{
    int i;
    float max = 0.0;
    for (i = 0; i < end; i++) {
	float tmp = fabs(xr[i]);
	if (max < tmp)
	    max = tmp;
	absxr[i] = tmp;
	xr34[i] = sqrt(tmp * sqrt(tmp));
    }
    *pmax = max;
}
#endif

static int 
init_bitalloc(lame_t gfc, gr_info *const gi)
{
    FLOAT max = 0.0;
    int end = gi->xrNumMax;

    /*  check if there is some energy we have to quantize
     *  and calculate xr34(gfc->xrwork[0]) matching our fresh scalefactors */
#ifdef __x86_64__
    if (end) {
	end = (end + 7) & (~7);
	pow075_SSE(gi->xr, xr34, end, &max);
    }
#else
#ifdef HAVE_NASM
    if (gfc->CPU_features.SSE) {
	if (end) {
	    end = (end + 7) & (~7);
	    pow075_SSE(gi->xr, xr34, end, &max);
	}
    } else if (gfc->CPU_features.AMD_3DNow) {
	if (end) {
	    end = (end + 3) & (~3);
	    pow075_3DN(gi->xr, xr34, end, &max);
	}
    } else
#endif
    {
	pow075(gfc, gi->xr, end, &max);
    }
#endif
    memset(&xr34[end], 0, sizeof(FLOAT)*(576-end));
    memset(&absxr[end], 0, sizeof(FLOAT)*(576-end));

    if (max > (FLOAT)1e-14) {
	int i, j = 0;
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
quantize_ISO(lame_t gfc, gr_info *gi)
{
    /* quantize on xr^(3/4) instead of xr */
    fi_union *fi = (fi_union *)gi->l3_enc;
    FLOAT istep = IPOW20(gi->global_gain);
    const FLOAT *xp = xr34;
    const FLOAT *xend = &xr34[gi->big_values];
    if (!gi->count1)
	return;
#ifdef __x86_64__
    {
	int len = (gi->count1+15)&(~15);
	fi += len;
	xp += len;
	quantize_ISO_SSE2(xp, -len, gi->global_gain, &fi[0].i);
	xend = &xr34[gi->count1];
    }
#else
#ifdef HAVE_NASM
    if (gfc->CPU_features.AMD_3DNow) {
	fi += gi->big_values;
	xp += gi->big_values;
	quantize_ISO_3DN(xp, -gi->big_values, gi->global_gain, &fi[0].i,
			 gi->count1 - gi->big_values);
	xend = &xr34[gi->count1];
    } else if (gfc->CPU_features.SSE) {
	int len = (gi->count1+15)&(~15);
	fi += len;
	xp += len;
	if (gfc->CPU_features.SSE2) {
	    quantize_ISO_SSE2(xp, -len, gi->global_gain, &fi[0].i);
	} else {
	    quantize_ISO_SSE(xp, -len, gi->global_gain, &fi[0].i);
	}
	xend = &xr34[gi->count1];
    } else
#endif
    {
	while (xp < xend) {
#ifdef USE_IEEE754_HACK
	    fi[0].f = istep * xp[0] + (ROUNDFAC_NEAR + MAGIC_FLOAT);
	    fi[1].f = istep * xp[1] + (ROUNDFAC_NEAR + MAGIC_FLOAT);
	    fi[2].f = istep * xp[2] + (ROUNDFAC_NEAR + MAGIC_FLOAT);
	    fi[3].f = istep * xp[3] + (ROUNDFAC_NEAR + MAGIC_FLOAT);
	    xp += 4;
	    fi[0].i -= MAGIC_INT;
	    fi[1].i -= MAGIC_INT;
	    fi[2].i -= MAGIC_INT;
	    fi[3].i -= MAGIC_INT;
	    fi += 4;
#else
	    (fi++)->i = (int)(*xp++ * istep + ROUNDFAC);
	    (fi++)->i = (int)(*xp++ * istep + ROUNDFAC);
#endif
	}
	xend = &xr34[gi->count1];
#ifdef USE_IEEE754_HACK
	{
	    fi_union thre;
	    thre.f = (FLOAT)(1.0-ROUNDFAC) / istep;
	    while (xp < xend) {
		(fi++)->i = ((int*)xp)[0] > thre.i ? 1:0;
		(fi++)->i = ((int*)xp)[1] > thre.i ? 1:0;
		xp += 2;
	    }
	}
#else
	istep = (FLOAT)(1.0-ROUNDFAC) / istep;
	while (xp < xend) {
	    (fi++)->i = *xp++ > istep ? 1:0;
	    (fi++)->i = *xp++ > istep ? 1:0;
	}
#endif
    }
#endif
    if (gfc->noise_shaping_amp >= 3) {
	istep = (FLOAT)0.634521682242439 / IPOW20(gi->global_gain);
	xp = xr34;
	fi = (fi_union *)gi->l3_enc;
	while (xp < xend) {
	    if (*xp++ < istep) fi[0].i = 0;
	    if (*xp++ < istep) fi[1].i = 0;
	    if (*xp++ < istep) fi[2].i = 0;
	    if (*xp++ < istep) fi[3].i = 0;
	    fi += 4;
	}
    }
}

static void
init_global_gain(
    lame_t gfc, gr_info * const gi, int desired_rate, int CurrentStep)
{
    int nbits, flag_GoneOver = 4;
    assert(CurrentStep);
    desired_rate -= gi->part2_length;
    do {
	if (flag_GoneOver & 4) {
	    gi->big_values = gi->count1 = gi->xrNumMax;
	    flag_GoneOver -= 4;
	}
	quantize_ISO(gfc, gi);
	nbits = noquant_count_bits(gfc, gi);

	if (CurrentStep == 1 || nbits == desired_rate)
	    break; /* nothing to adjust anymore */

	if (nbits > desired_rate) {  
	    /* increase Quantize_StepSize */
	    flag_GoneOver |= 1;

	    if (flag_GoneOver==3) CurrentStep >>= 1;
	    gi->global_gain += CurrentStep;
	} else {
	    /* decrease Quantize_StepSize */
	    flag_GoneOver |= 2 | 4;

	    if (flag_GoneOver == 7) CurrentStep >>= 1;
	    gi->global_gain -= CurrentStep;
	}
    } while ((unsigned int)gi->global_gain < MAX_GLOBAL_GAIN+1);

    if (gi->global_gain < 0) {
	gi->global_gain = 0;
	quantize_ISO(gfc, gi);
	noquant_count_bits(gfc, gi);
    } else if (gi->global_gain > MAX_GLOBAL_GAIN) {
	gi->global_gain = MAX_GLOBAL_GAIN;
	gi->big_values = gi->count1 = gi->xrNumMax;
	quantize_ISO(gfc, gi);
	noquant_count_bits(gfc, gi);
    } else if (nbits > desired_rate) {
	gi->global_gain++;
	quantize_ISO(gfc, gi);
	noquant_count_bits(gfc, gi);
    }
}




/************************************************************************
 *
 *      truncate_smallspectrums()
 *
 *  Takehiro TOMINAGA 2002-07-21
 *
 *  truncate smal nubmers into 0 as long as the noise threshold is allowed.
 *
 ************************************************************************/
static int
floatcompare(const void *v1, const void *v2)
{
#if ALLDOUBLE
    const FLOAT *a = v1, *b = v2;
    if (*a > *b) return 1;
    if (*a == *b) return 0;
    return -1;
#else
    return *(int *)v1 - *(int *)v2;
#endif
}

static void
truncate_smallspectrums(lame_t gfc, gr_info* const gi, const FLOAT* const xmin)
{
    int sfb, j;
    FLOAT distort[SFBMAX], work[192]; /* 192 is the width of the widest scale factor band(sfb 21 of 48kHz) */
    assert(gi->psymax != 0);

    for (sfb = 0; sfb < gi->psymax; sfb++) {
	work[sfb] = (FLOAT)1.0;
	distort[sfb] = (FLOAT)-1.0;
    }
    calc_noise(gfc, gi, work, distort);

    j = sfb = 0;
    do {
	FLOAT threshold;
	int nsame, start, width2, k = j, width = gi->wi[sfb].width;
	j -= width;

	if (distort[sfb] >= xmin[sfb])
	    continue;

	width2 = 0;
	do {
	    if (gi->l3_enc[k] != 0)
		work[width2++] = absxr[k];
	} while (++k < j);
	if (width2 == 0)
	    continue; /* already all zero */

	qsort(work, width2, sizeof(FLOAT), floatcompare);
	threshold = xmin[sfb] - distort[sfb];
	start = 0;
	do {
	    for (nsame = 1; start+nsame < width2; nsame++)
		if (work[start] != work[start+nsame])
		    break;

	    threshold -= work[start] * work[start] * nsame;
	} while (threshold > (FLOAT)0.0 && (start += nsame) < width2);
	if (start == 0)
	    continue;
	assert(start <= width2);

	threshold = work[start-1];
	do {
	    if (absxr[j + width] <= threshold)
		gi->l3_enc[j + width] = 0;
	} while (++width < 0);
    } while (++sfb < gi->psymax);
    noquant_count_bits(gfc, gi);
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
	if (!(gi->scalefac[sfb] | gi->subblock_gain[gi->wi[sfb].window]))
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
inc_scalefac_scale(gr_info * const gi, FLOAT distort[])
{
    int sfb = 0;
    do {
	int s = gi->scalefac[sfb], s0;
	if (gi->preflag > 0)
	    s += pretab[sfb];
	s0 = s;
	gi->scalefac[sfb] = s = (s + 1) >> 1;
	if (s*2 != s0)
	    distort[sfb] = (FLOAT)-1.0;
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
    int sfb, subwin;
    const char *tab = max_range_short;
    if (gi->sfb_lmax) {	/* mixed_block. */
	/* subbloc_gain can't do anything in the long block region */
	for (sfb = 0; sfb < gi->sfb_lmax; sfb++)
	    if (gi->scalefac[sfb] >= 16)
		return 1;
	tab--;
    }

    for (subwin = 0; subwin < 3; subwin++) {
	for (sfb = gi->sfb_lmax+subwin; sfb < gi->psymax; sfb += 3)
	    if (gi->scalefac[sfb] > tab[sfb])
		break;

	if (sfb >= gi->psymax)
	    continue;

	if (gi->subblock_gain[subwin+1] >= MAX_SUBBLOCK_GAIN)
	    return 1;

	gi->subblock_gain[subwin+1]++;
	for (sfb = gi->sfb_lmax+subwin; sfb < gi->psymax; sfb += 3) {
	    int s;
	    if (gi->scalefac[sfb] < 0)
		continue;
	    s = gi->scalefac[sfb] - (4 >> gi->scalefac_scale);
	    if (s < 0) {
		distort[sfb] = (FLOAT)-1.0;
		s = 0;
	    }
	    if (sfb < gi->sfbmax)
		gi->scalefac[sfb] = s;
	}
    }
    return 0;
}

static int 
inc_subblock_gain2(lame_t gfc, gr_info * const gi, int subwin)
{
    int sfb, ret = -1;
    gr_info gi_w;

    gi_work_copy(&gi_w, gi);
    gi_w.subblock_gain[subwin]++;
    for (sfb = gi_w.sfb_lmax+subwin-1; sfb < gi_w.psymax; sfb += 3) {
	int s;
	if (gi_w.scalefac[sfb] < 0)
	    continue;
	s = gi_w.scalefac[sfb] - (4 >> gi_w.scalefac_scale);
	if (s < 0) {
	    if (ret < 0)
		ret = sfb;
	    s = 0;
	}
	if (sfb < gi_w.sfbmax)
	    gi_w.scalefac[sfb] = s;
	if (IPOW20(scalefactor(&gi_w, sfb)) >= gfc->maxXR[sfb]) {
	    return -2;
	}
    }
    gi_work_copy(gi, &gi_w);
    return ret;
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
 *    1             trigger = sqrt(max_dist)   (50% in the dB scale)
 *
 *    2             trigger = max_dist;
 *                  (select only the band with the strongest noise)
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
amp_scalefac_bands(lame_t gfc, gr_info *const gi, FLOAT *distort,
		   FLOAT trigger, int method, int target_bits)
{
    int bits, sfb, sfbmax = gi->sfbmax;
    if (sfbmax > gi->psymax)
	sfbmax = gi->psymax;

    if (gi->block_type == SHORT_TYPE || trigger <= (FLOAT)1.0) {
	trigger = distort[0];
	for (sfb = 1; sfb < sfbmax; sfb++) {
	    if (trigger < distort[sfb])
		trigger = distort[sfb];
	}
    }

    if (method < 2) {
	if (trigger <= (FLOAT)1.0)
	    trigger *= (FLOAT)0.95;
	else if (method == 0)
	    trigger = (FLOAT)1.0;
	else
	    trigger = sqrt(trigger);
    }

    for (sfb = 0; sfb < sfbmax; sfb++) {
	if (distort[sfb] < trigger)
	    continue;

	if (method < 3 || (gfc->pseudohalf[sfb] ^= 1))
	    gi->scalefac[sfb]++;
	distort[sfb] = (FLOAT)-1.0;
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
    if (gi->block_type == SHORT_TYPE && gfc->use_subblock_gain) {
	if (inc_subblock_gain(gi, distort) || loop_break(gi))
	    return 0; /* failed */
    } else if (gfc->use_scalefac_scale && !gi->scalefac_scale)
	inc_scalefac_scale(gi, distort);
    else
	return 0;

    return target_bits - gfc->scale_bitcounter(gi);
}


inline static FLOAT
calc_sfb_noise_fast(lame_t gfc, int j, int bw, int sf)
{
    FLOAT xfsf = (FLOAT)0.0;
    FLOAT sfpow = POW20(sf);  /*pow(2.0,sf/4.0); */
    FLOAT sfpow34 = IPOW20(sf); /*pow(sfpow,-3.0/4.0); */

    do {
	FLOAT t0, t1;
#ifdef USE_IEEE754_HACK
	fi_union fi0, fi1;
	fi0.f = sfpow34 * xr34[j+bw  ] + (ROUNDFAC_NEAR + MAGIC_FLOAT);
	fi1.f = sfpow34 * xr34[j+bw+1] + (ROUNDFAC_NEAR + MAGIC_FLOAT);

	assert(fi0.i <= MAGIC_INT + IXMAX_VAL
	       && fi1.i <= MAGIC_INT + IXMAX_VAL);
	t0 = absxr[j+bw  ] - (pow43 - MAGIC_INT)[fi0.i] * sfpow;
	t1 = absxr[j+bw+1] - (pow43 - MAGIC_INT)[fi1.i] * sfpow;
#else
	int i0, i1;
	i0 = (int)(sfpow34 * xr34[j+bw  ] + ROUNDFAC);
	i1 = (int)(sfpow34 * xr34[j+bw+1] + ROUNDFAC);

	assert(i0 <= IXMAX_VAL && i1 <= IXMAX_VAL);
	t0 = absxr[j+bw  ] - pow43[i0] * sfpow;
	t1 = absxr[j+bw+1] - pow43[i1] * sfpow;
#endif
	xfsf += t0*t0 + t1*t1;
    } while ((bw += 2) < 0);
    return xfsf;
}

inline static FLOAT
calc_sfb_noise(lame_t gfc, int j, int bw, int sf)
{
    FLOAT xfsf, sfpow, sfpow34;
#ifdef HAVE_NASM
    if (gfc->CPU_features.AMD_3DNow)
	return calc_sfb_noise_3DN(gfc, j, bw, sf);
#endif
    xfsf = (FLOAT)0.0;
    sfpow = POW20(sf);  /*pow(2.0,sf/4.0) */
    sfpow34 = IPOW20(sf); /*pow(sfpow,-3.0/4.0)=pow(2.0,-sf*0.1875) */

    do {
	FLOAT t0, t1;
	int i0, i1;
#ifdef USE_IEEE754_HACK
	fi_union fi0, fi1;
	fi0.f = sfpow34 * xr34[j+bw  ] + (FLOAT)MAGIC_FLOAT2;
	fi1.f = sfpow34 * xr34[j+bw+1] + (FLOAT)MAGIC_FLOAT2;
	i0 = fi0.i;
	i1 = fi1.i;
	assert(((i0 - (MAGIC_INT2 + (IXMAX_VAL<<FIXEDPOINT) + 1))
		& (i1 - (MAGIC_INT2 + (IXMAX_VAL<<FIXEDPOINT) + 1))) < 0);
	i0 += adj43asm[(i0 >> FIXEDPOINT) - (MAGIC_INT2 >> FIXEDPOINT)];
	i1 += adj43asm[(i1 >> FIXEDPOINT) - (MAGIC_INT2 >> FIXEDPOINT)];
	t0 = absxr[j+bw  ] - pow43[i0 >> FIXEDPOINT] * sfpow;
	t1 = absxr[j+bw+1] - pow43[i1 >> FIXEDPOINT] * sfpow;
#else
	i0 = (int) (t0 = sfpow34 * xr34[j+bw  ]);
	i1 = (int) (t1 = sfpow34 * xr34[j+bw+1]);
	assert(i0 <= IXMAX_VAL && i1 <= IXMAX_VAL);

	t0 = absxr[j+bw  ] - pow43[(int)(t0 + adj43[i0])] * sfpow;
	t1 = absxr[j+bw+1] - pow43[(int)(t1 + adj43[i1])] * sfpow;
#endif
	xfsf += t0*t0 + t1*t1;
    } while ((bw += 2) < 0);
    return xfsf;
}

static int
adjust_global_gain(lame_t gfc, gr_info *gi, FLOAT *distort, int huffbits)
{
    fi_union *fi = (fi_union *)gi->l3_enc;
    int sfb = 0, j = 0, end = gi->xrNumMax;

    do {
	int bw = gi->wi[sfb].width;
	FLOAT istep;
	j -= bw;
	if (!signbits(distort[sfb]))
	    continue;

	if (gi->count1 < j)
	    gi->count1 = j;
#ifdef HAVE_NASM
	if (gfc->CPU_features.AMD_3DNow) {
	    quantize_sfb_3DN(&xr34[j], bw, scalefactor(gi, sfb), &gi->l3_enc[j]);
	} else
#endif
	{
	    istep = IPOW20(scalefactor(gi, sfb));
	    do {
#ifdef USE_IEEE754_HACK
		int i0, i1;
		fi[j+bw  ].f = istep * xr34[j+bw  ] + MAGIC_FLOAT2;
		fi[j+bw+1].f = istep * xr34[j+bw+1] + MAGIC_FLOAT2;
		i0 = fi[j+bw  ].i;
		i1 = fi[j+bw+1].i;
		if (i0 > MAGIC_INT2 + IXMAX_VAL) i0 = MAGIC_INT2 + IXMAX_VAL;
		if (i1 > MAGIC_INT2 + IXMAX_VAL) i1 = MAGIC_INT2 + IXMAX_VAL;
		fi[j+bw  ].i = (i0 + adj43asm[(i0 >> FIXEDPOINT) - (MAGIC_INT2 >> FIXEDPOINT)])
						  >> FIXEDPOINT;
		fi[j+bw+1].i = (i1 + adj43asm[(i1 >> FIXEDPOINT) - (MAGIC_INT2 >> FIXEDPOINT)])
						  >> FIXEDPOINT;
#else
		FLOAT x0 = istep * xr34[j+bw  ];
		FLOAT x1 = istep * xr34[j+bw+1];
		if (x0 > IXMAX_VAL) x0 = IXMAX_VAL;
		if (x1 > IXMAX_VAL) x1 = IXMAX_VAL;
		fi[j+bw  ].i = (int)(x0 + adj43[(int)x0]);
		fi[j+bw+1].i = (int)(x1 + adj43[(int)x1]);
#endif
	    } while ((bw += 2) < 0);
	}

	if (!gfc->pseudohalf[sfb])
	    continue;
	istep = (FLOAT)0.634521682242439
	    / IPOW20(scalefactor(gi, sfb) + gi->scalefac_scale);
	bw = gi->wi[sfb].width;
	do {
	    if (xr34[j+bw] < istep)
		fi[j+bw].i = 0;
	} while (++bw < 0);
    } while (++sfb < gi->psymax && j < end);

    if (noquant_count_bits(gfc, gi) > huffbits)
	return (++gi->global_gain) - (MAX_GLOBAL_GAIN+1);

    return 0;
}

static void
CBR_2nd_bitalloc(lame_t gfc, gr_info *gi, FLOAT distort[])
{
    int sfb, j = 0, flag = 0;
    gr_info gi_w;
    gi_w = *gi;
    for (sfb = 0; sfb < gi_w.psymax; sfb++) {
	int width = gi_w.wi[sfb].width;
	distort[sfb] = (FLOAT)0.0;
	j -= width;
	if (gi_w.scalefac[sfb] > 0) {
	    FLOAT noiseold = calc_sfb_noise(gfc, j, width,
					    scalefactor((&gi_w), sfb));
	    while (--gi_w.scalefac[sfb] >= 0
		   && calc_sfb_noise(gfc, j, width, scalefactor((&gi_w), sfb))
		   <= noiseold)
		;
	    gi_w.scalefac[sfb]++;
	    if (gi_w.scalefac[sfb] != gi->scalefac[sfb]) {
		flag = 1;
		distort[sfb] = (FLOAT)-1.0;
	    }
	}
    }
    if (!flag)
	return;

    gfc->scale_bitcounter(&gi_w);
    if (adjust_global_gain(gfc, &gi_w, distort, gi_w.part2_3_length))
	return;
    assert(gi_w.part2_length + gi_w.part2_3_length
	   <= gi->part2_length + gi->part2_3_length);
    gi_work_l3_copy(gi, &gi_w);
}

static int
noise_in_sfb21(gr_info *gi, FLOAT distort[], FLOAT threshold)
{
    int sfb;
    for (sfb = gi->sfbmax; sfb < gi->psymax; sfb++)
	if (distort[sfb] >= threshold)
	    return 1;
    return 0;
}

/************************************************************************
 *
 *  CBR_1st_bitalloc ()
 *
 *  find the best scalefactor and global gain combination
 *  as long as it satisfy the bitrate condition.
 ************************************************************************/
static void
CBR_1st_bitalloc (
    lame_t gfc,
    gr_info		* const gi,
    const int           ch,
    const int           targ_bits,  /* maximum allowed bits */
    const III_psy_ratio * const ratio
    )
{
    gr_info gi_w;
    FLOAT distort[SFBMAX], xmin[SFBMAX], rxmin[SFBMAX], bestNoise, newNoise;
    int current_method, age;

    if (!init_bitalloc(gfc, gi))
	return; /* digital silence */

    gi->global_gain = gfc->OldValue[ch];
    init_global_gain(gfc, gi, targ_bits, gfc->CurrentStep[ch]);

    gfc->CurrentStep[ch] = (gfc->OldValue[ch] - gi->global_gain) >= 4 ? 4 : 2;
    gfc->OldValue[ch] = gi->global_gain;

    if (gfc->psymodel < 2) 
	return; /* fast mode, no noise shaping, we are ready */

    /* compute the noise */
    calc_xmin (gfc, ratio, gi, xmin);
    newNoise = bestNoise = calc_noise_allband(gfc, gi, xmin, distort, rxmin);
    if (noise_in_sfb21(gi, distort, bestNoise))
	goto quit_quantization;
    current_method = 0;
    age = 3;
    if (bestNoise < (FLOAT)1.0) {
	if (gfc->noise_shaping_amp == 0)
	    goto quit_quantization;
	current_method = 1;
	age = 5;
    }

    /* BEGIN MAIN LOOP */
    gi_w = *gi;
    for (;;) {
	/* try the new scalefactor conbination on gi_w */
	int huff_bits = amp_scalefac_bands(gfc, &gi_w, distort, newNoise,
					   current_method, targ_bits);
	if (huff_bits >= 3) { /* we need at least 3 bits to encode "not all zero" */
	    /* adjust global_gain to fit the available bits */
	    if (adjust_global_gain(gfc, &gi_w, distort, huff_bits)) {
		int sfb;
		while (count_bits(gfc, &gi_w) > huff_bits
		       && (unsigned int)++gi_w.global_gain < MAX_GLOBAL_GAIN+1)
		    ;
		for (sfb = 0; sfb < gi->psymax; sfb++)
		    distort[sfb] = (FLOAT)-1.0;
	    }
	    if ((gfc->mode_ext & 1) && ch == 1
		&& ((gi->block_type != SHORT_TYPE
		     && gfc->scalefac_band.l[gi->psymax-1] > gi->count1)
		    || (gfc->scalefac_band.s[gi->psymax/3]*3
			+ gi->wi[gi->psymax-1].width) > gi->count1)) {
		/* some scalefac band which we do not want to be i-stereo is
		   all zero (treat as i-stereo). */
		calc_noise(gfc, &gi_w, rxmin, distort);
		distort[gi->psymax-1] = (FLOAT)20.0; /* some large value */
		goto nextTry;
	    }

	    /* store this scalefactor combination if it is better */
	    if (gi_w.global_gain != MAX_GLOBAL_GAIN+1
		&& bestNoise
		> (newNoise = quantEstimate(gfc, &gi_w, rxmin, distort))) {
		bestNoise = newNoise;
		gi_work_l3_copy(gi, &gi_w);
		if (bestNoise < (FLOAT)1.0 && current_method == 0) {
		    if (gfc->noise_shaping_amp == 0)
			break;
		    current_method++;
		}
		age = current_method*3+2;
		continue;
	    }
	} else
	    age = 0;
    nextTry:
	/* stopping criteria */
	if (--age > 0 && gi_w.global_gain != MAX_GLOBAL_GAIN+1
	    && !noise_in_sfb21(&gi_w, distort, bestNoise))
	    continue;

	/* seems we cannot get a better combination.
	   restart from the best with more finer noise_amp method */
	if (current_method == gfc->noise_shaping_amp)
	    break;
	current_method++;
	gi_work_l3_copy(&gi_w, gi);
	age = current_method*3+2;
	newNoise = bestNoise;
    }
    assert((unsigned int)gi->global_gain <= (unsigned int)MAX_GLOBAL_GAIN);

    if (gfc->noise_shaping_amp >= 4)
	CBR_2nd_bitalloc(gfc, gi, distort);
 quit_quantization:
    if ((gfc->substep_shaping & 0x80) == 0
	&& ((gi->block_type == SHORT_TYPE && (gfc->substep_shaping & 2))
	 || (gi->block_type != SHORT_TYPE && (gfc->substep_shaping & 1))))
	truncate_smallspectrums(gfc, gi, xmin);
}


/************************************************************************
 * allocate bits among 2 channels based on PE (no multichannel support)
 ************************************************************************/
static void
CBR_bitalloc(
    lame_t gfc,
    III_psy_ratio ratio[MAX_CHANNELS],
    int min_bits,
    int max_bits,
    FLOAT factor,
    const int gr
    )
{
    int bits = gfc->tt[gr][gfc->channels_out-1].part2_length;
    int ch, adjustBits, targ_bits[MAX_CHANNELS];

    /* allocate minimum bits to encode part2_length (for i-stereo) */
    min_bits -= bits;
    max_bits -= bits; assert(max_bits >= 0);

    /* estimate how many bits we need */
    adjustBits = 0;
    for (ch = 0; ch < gfc->channels_out; ch++) {
	gr_info *gi = &gfc->tt[gr][ch];
	bits = 0;
	if (gi->psymax > 0)
	    bits = (int)(ratio[ch].pe*factor);
	if ((gi->block_type == SHORT_TYPE && (gfc->substep_shaping & 2))
	 || (gi->block_type != SHORT_TYPE && (gfc->substep_shaping & 1)))
	    bits *= (FLOAT)1.1;

	/* START/STOP block cannot use "huffman block division hack" to
	 * improve coding efficiency. */
	if (gi->block_type == START_TYPE || gi->block_type == STOP_TYPE)
	    bits *= (FLOAT)1.1;

	adjustBits += (targ_bits[ch] = ++bits); /* avoid zero division */
    }

    bits = 0;
    if (adjustBits > max_bits)
	bits = max_bits - adjustBits; /* reduce */
    else if (adjustBits < min_bits)
	bits = min_bits - adjustBits; /* increase */

    if (bits) {
	int ch0new = targ_bits[0] + ((FLOAT)bits * targ_bits[0]) / adjustBits;
	if (gfc->channels_out == 2)
	    targ_bits[1] += bits - (ch0new - targ_bits[0]);
	targ_bits[0] = ch0new;
    }

    for (ch = 0; ch < gfc->channels_out; ch++) {
	CBR_1st_bitalloc(gfc, &gfc->tt[gr][ch], ch,
			 targ_bits[ch] + gfc->tt[gr][ch].part2_length,
			 &ratio[ch]);
 	gfc->ResvSize -= iteration_finish_one(gfc, gr, ch);
    }
}


/************************************************************************
 *
 *      CBR_iteration_loop()
 *
 *  encodes one frame of MP3 data with constant bitrate
 *
 ************************************************************************/
void 
CBR_iteration_loop(lame_t gfc, III_psy_ratio ratio[MAX_GRANULES][MAX_CHANNELS])
{
    int gr, mean_bits = getframebytes(gfc);
    FLOAT factor;

    ResvFrameBegin(gfc, mean_bits);
    mean_bits *= 8/gfc->mode_gr;
    factor = (mean_bits/gfc->channels_out+300)*(FLOAT)(1.0/1300.0);

    for (gr = 0; gr < gfc->mode_gr; gr++) {
	/*  calculate needed bits */
	int min_bits = mean_bits, max_bits = gfc->ResvSize;
	gfc->substep_shaping &= 0x7f;
	if (max_bits*10 >= gfc->ResvMax*6) {
	    int add_bits = 0;
	    if (max_bits*10 >= gfc->ResvMax*9)
		gfc->substep_shaping |= 0x80;
	    if (max_bits*10 >= gfc->ResvMax*9
		|| (gfc->substep_shaping & 1)) {
		/* reservoir is filled over 90% -> forced to use reservoir */
		add_bits = max_bits - (gfc->ResvMax * 6) / 10;
		min_bits += add_bits;
	    }
	    /* max amount from the reservoir we are allowed to use. */
	    /* ISO says 60%, but we can tune it */
	    max_bits = (gfc->ResvMax*6)/10 - add_bits;
	    if (max_bits < 0) max_bits = 0;
	} else if (!(gfc->substep_shaping & 1)) {
	    /* build up reservoir.  this builds the reservoir a little slower
	     * than FhG.  It could simple be mean_bits/15, but this was rigged
	     * to always produce 100 (the old value) at 128kbs */
	    /*    min_bits -= (int) (mean_bits/15.2);*/
	    min_bits -= mean_bits/10;
	}

	gfc->ResvSize += mean_bits;

	max_bits += mean_bits;
	if (max_bits > mean_bits*2)    /* limit 2*average (need tuning) */
	    max_bits = mean_bits*2;
	if (min_bits > max_bits)
	    min_bits = max_bits;
	/* check hard limit per granule (by spec) */
        if (gfc->strict_ISO) {
	    if (max_bits > MAX_BITS)
		max_bits = MAX_BITS;
	    if (min_bits > MAX_BITS)
		min_bits = MAX_BITS;
	}

	CBR_bitalloc(gfc, ratio[gr], min_bits, max_bits, factor, gr);
    }
    assert(gfc->ResvSize >= 0);
}

/************************************************************************
 *
 * VBR related code
 *  Takehiro TOMINAGA 2002-12-31
 * based on Mark and Robert's code and suggestions
 *
 ************************************************************************/
static void
bitpressure_strategy(gr_info *gi, FLOAT *xmin)
{
    int sfb;
    for (sfb = 0; sfb < gi->psy_lmax; sfb++) 
	*xmin++ *= (FLOAT)1.+(FLOAT)(.029/(SBMAX_l*SBMAX_l))*(sfb*sfb+1);

    if (sfb != gi->psymax) {
	for (sfb = gi->sfb_smin; sfb < gi->psymax; sfb += 3) {
	    FLOAT x = (FLOAT)1.+(FLOAT)(.029/(SBMAX_s*SBMAX_s*9))*(sfb*sfb+1);
	    *xmin++ *= x;
	    *xmin++ *= x;
	    *xmin++ *= x;
	}
    }
}

/* find_scalefac() calculates a quantization step size which would
 * introduce as much noise as allowed (= as less bits as possible). */
inline static int
find_scalefac(lame_t gfc, int j, FLOAT xmin, int bw, FLOAT maxXR,
	      int sfmin, int sf)
{
    int sf_ok = sfmin, delsf = 8, endflag = 0;

    assert(sf >= sfmin);
    do {
	FLOAT xfsf;
	if (IPOW20(sf) > maxXR) /* pow(sf,0.1875) < maxXR / IXMAX */
	    goto illegal_sf;
#ifdef HAVE_NASM
	if (gfc->CPU_features.AMD_3DNow)
	    xfsf = calc_sfb_noise_fast_3DN(gfc, j, bw, sf);
	else
#endif
	    xfsf = calc_sfb_noise_fast(gfc, j, bw, sf);

	if (xfsf > xmin) {
	    /* there's noticible noise. try a smaller scalefactor */
	    endflag |= 1;
	    if (endflag == 3)
		delsf >>= 1;
	    sf -= delsf;
	    if (sf < sfmin) {
		sf = sfmin;
		endflag = 3;
	    }
	} else {
	    assert(xfsf >= (FLOAT)0.0);
	    if (sf >= MAX_GLOBAL_GAIN+1) {
		return MAX_GLOBAL_GAIN+1;
	    }
	    sf_ok = sf;
	illegal_sf:
	    endflag |= 2;
	    if (endflag == 3)
		delsf >>= 1;
	    sf += delsf;
	    if (sf > MAX_GLOBAL_GAIN+1) {
		sf = MAX_GLOBAL_GAIN+1;
		endflag = 3;
	    }
	}
    } while (delsf != 0);

    return sf_ok;
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
	for (b = 0; b < 3; b++, sfb++) {
	    if (gi->scalefac[sfb] > MAX_GLOBAL_GAIN)
		continue;
	    maxov[0][b] = Min(gi->scalefac[sfb] + 2*max_range_short[sfb],
			      maxov[0][b]);
	    maxov[1][b] = Min(gi->scalefac[sfb] + 4*max_range_short[sfb],
			      maxov[1][b]);
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
    if (vbrmax > MAX_GLOBAL_GAIN)
	vbrmax = MAX_GLOBAL_GAIN;

    for (b = 0; b < 3; b++) {
	int sbg = (vbrmax - maxov[(int)gi->scalefac_scale][b] + 7) >> 3;
	if (sbg < 0)
	    sbg = 0;
	if (sbg > MAX_SUBBLOCK_GAIN)
	    sbg = MAX_SUBBLOCK_GAIN;
	gi->subblock_gain[b+1] = sbg;
    }

    b = vbrmax >> 3;
    if (b > gi->subblock_gain[1])
	b = gi->subblock_gain[1];
    if (b > gi->subblock_gain[2])
	b = gi->subblock_gain[2];
    if (b > gi->subblock_gain[3])
	b = gi->subblock_gain[3];

    gi->global_gain = vbrmax-b*8;
    gi->subblock_gain[1] -= b;
    gi->subblock_gain[2] -= b;
    gi->subblock_gain[3] -= b;
}


static void
long_block_scalefacs(lame_t gfc, gr_info * gi, int vbrmax)
{
    int sfb, maxov0, maxov1, maxov0p, maxov1p;

    /* we can use 4 strategies. [scalefac_scale = (0,1), pretab = (0,1)] */
    maxov0  = maxov1 = maxov0p = maxov1p = vbrmax;

    /* find out the distance from most important scalefactor band
       (which wants the largest scalefactor value)
       to largest possible range */
    for (sfb = 0; sfb < gi->psymax; ++sfb) {
	if (gi->scalefac[sfb] > MAX_GLOBAL_GAIN)
	    continue;
	maxov0  = Min(gi->scalefac[sfb] + 2*max_range_long[sfb], maxov0);
	maxov1  = Min(gi->scalefac[sfb] + 4*max_range_long[sfb], maxov1);
	maxov0p = Min(gi->scalefac[sfb] + 2*max_range_long[sfb] + 2*pretab[sfb],
		      maxov0p);
	maxov1p = Min(gi->scalefac[sfb] + 4*max_range_long[sfb]+4*pretab[sfb],
		      maxov1p);
    }

    if (gfc->mode_gr == 1)
	maxov1p = maxov0p = 10000;

    gi->preflag = 0;
    gi->scalefac_scale = 0;
    if (maxov0 == vbrmax) {
    }
    else if (maxov0p == vbrmax && gfc->mode_gr != 1) {
	gi->preflag = 1;
    }
    else {
	gi->scalefac_scale = 1;
	if (maxov1 == vbrmax) {
	}
	else if (maxov1p == vbrmax && gfc->mode_gr != 1) {
	    gi->preflag = 1;
	} else {
	    vbrmax = maxov1;
	}
    }
    if (vbrmax < 0)
	vbrmax = 0;
    if (vbrmax > MAX_GLOBAL_GAIN)
	vbrmax = MAX_GLOBAL_GAIN;
    gi->global_gain = vbrmax;
}

static int
noisesfb(lame_t gfc, gr_info *gi, FLOAT * xmin, int startsfb)
{
    int sfb, j = 0;
    for (sfb = 0; sfb < gi->psymax; sfb++) {
	int width = gi->wi[sfb].width, s;
	j -= width;
	if (sfb < startsfb || gi->scalefac[sfb] < 0)
	    continue;

	s = scalefactor(gi, sfb);
	assert(IPOW20(s) <= gfc->maxXR[sfb]);
	if (calc_sfb_noise(gfc, j, width, s) > xmin[sfb])
	    return sfb;
    }
    return -1;
}

static int
VBR_2nd_bitalloc(lame_t gfc, gr_info *gi, FLOAT * xmin)
{
    /* note: we cannot use calc_noise() because l3_enc[] is not calculated
       at this point */
    gr_info gi_w;
    int endflag, j;
    const char *tab;
    int gainbase = gi->global_gain + 1 + 2*gi->scalefac_scale, sfb;

    j = sfb = 0;
    do {
	int s = gi->scalefac[sfb];
	int width = gi->wi[sfb].width;
	j -= width;
	if (s > MAX_GLOBAL_GAIN) {
	    gi->scalefac[sfb] = SCALEFAC_ANYTHING_GOES;
	    continue;
	}

	s = (gainbase - s - gi->subblock_gain[gi->wi[sfb].window]*8)
	    >> (1 + gi->scalefac_scale);
	if (gi->preflag > 0)
	    s -= pretab[sfb];
	if (s < 0)
	    s = 0;
	gi->scalefac[sfb] = s;

	while (IPOW20(scalefactor(gi, sfb)) > gfc->maxXR[sfb]) {
	    if (gi->scalefac[sfb] == 0)
		return -2;
	    gi->scalefac[sfb]--;
	}
    } while (++sfb < gi->psymax);

    gi_w = *gi;
    endflag = sfb = 0;
    tab = max_range_long;
    if (gi->block_type == SHORT_TYPE) {
	tab = max_range_short;
	if (gi->mixed_block_flag)
	    tab++;
    }
    for (;;) {
	sfb = noisesfb(gfc, &gi_w, xmin, sfb);
	if (sfb < 0) { /* no noise */
	    gi_work_copy(gi, &gi_w);
	    if (endflag == 1 || gi_w.global_gain >= MAX_GLOBAL_GAIN)
		return 0;
	    gi_w.global_gain++;
	    endflag = 2;
	} else if (sfb < gi->sfbmax) { /* noise at "sfb" */
	    gi_w.scalefac[sfb]++;
	    if (loop_break(&gi_w))
		return 0;
	    if (gi_w.scalefac[sfb] > tab[sfb]
		|| IPOW20(scalefactor(&gi_w, sfb)) > gfc->maxXR[sfb]) {
		/* oh, we can't reduce the noise in this band */
		gi_w.scalefac[sfb]--;
		sfb++;
		endflag = 1; /* forbid to increase global_gain */
	    }
	} else { /* noise in sfb21 */
	    int win = gi_w.wi[sfb].window;
	    if (!win || gi_w.subblock_gain[win] >= MAX_SUBBLOCK_GAIN
		|| (sfb = inc_subblock_gain2(gfc, &gi_w, win)) == -2) {
		if (endflag == 2 || gi_w.global_gain == 0)
		    return 0;
		endflag = 1;
		gi_w.global_gain--;
		sfb = 0;
	    }
	}
    }
}

static void
VBR_3rd_bitalloc(lame_t gfc, gr_info *gi, FLOAT * xmin)
{
    /*
     * This code is basically based on the same idea of CBR_2nd_bitalloc().
     * But we cannot use calc_noise() because l3_enc[] is not calculated
     * at this point.
     */
    int sfb, j;
    for (j = sfb = 0; sfb < gi->psymax; sfb++) {
	int width = gi->wi[sfb].width;
	j -= width;
	while (gi->scalefac[sfb] > 0) {
	    gi->scalefac[sfb]--;
	    if (calc_sfb_noise(gfc, j, width, scalefactor(gi, sfb))
		> xmin[sfb]) {
		gi->scalefac[sfb]++;
		break;
	    }
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
VBR_noise_shaping(lame_t gfc, gr_info *gi, FLOAT * xmin)
{
    int vbrmax, sfb, j, gain, sfmin;
    sfb = j = 0;
    vbrmax = -10000;
    gain = gi->global_gain;

    /* search range of sf.
       on shoft blocks, it is large because of subblock_gain */
    sfmin = -7*4;
    if (gi->block_type == SHORT_TYPE)
	sfmin = -7*8-7*4;
    do {
	FLOAT maxXR;
	int width = gi->wi[sfb].width;
	j -= width;
#ifdef HAVE_NASM
	if (gfc->CPU_features.MMX) {
	    maxXR = xrmax_MMX(&xr34[j+width], &xr34[j]);
	} else
#endif
	{
	    int i = width;
	    maxXR = (FLOAT)0.0;
	    do {
		if (maxXR < xr34[i+j])
		    maxXR = xr34[i+j];
		if (maxXR < xr34[i+j+1])
		    maxXR = xr34[i+j+1];
	    } while ((i += 2) < 0);
	}
	gi->scalefac[sfb] = MAX_GLOBAL_GAIN+1;
	gfc->maxXR[sfb] = FLOAT_MAX;
	if (maxXR > MINIMUM_XR) {
	    maxXR = IXMAX_VAL / maxXR;
	    gain = find_scalefac(gfc, j, xmin[sfb], width, maxXR, sfmin, gain);
	    if (gain <= MAX_GLOBAL_GAIN) {
		gi->scalefac[sfb] = gain;
		gfc->maxXR[sfb] = maxXR;
		if (vbrmax < gain)
		    vbrmax = gain;
	    } else
		gain = MAX_GLOBAL_GAIN;
	}
    } while (++sfb < gi->psymax);

    if (vbrmax == -10000)
	vbrmax = MAX_GLOBAL_GAIN;

    if (gi->block_type == SHORT_TYPE)
	short_block_scalefacs(gi, vbrmax);
    else
	long_block_scalefacs(gfc, gi, vbrmax);

    /* set scalefac[] as they satisfy the noise threshold */
    if (VBR_2nd_bitalloc(gfc, gi, xmin))
	return -2;

    /* reduce bitrate if possible */
    VBR_3rd_bitalloc(gfc, gi, xmin);

    /* quantize */
    gi->big_values = gi->count1 = gi->xrNumMax;
    count_bits(gfc, gi);

    /* encode scalefacs */
    gfc->scale_bitcounter(gi);

    if ((gfc->strict_ISO && gi->part2_3_length + gi->part2_length > MAX_BITS)
	|| ((1+(gi->block_type == SHORT_TYPE)) & gfc->substep_shaping)) {
	truncate_smallspectrums(gfc, gi, xmin);
    }

    assert((unsigned int)gi->global_gain <= (unsigned int)MAX_GLOBAL_GAIN);

    return 0;
}


void 
VBR_iteration_loop(lame_t gfc, III_psy_ratio ratio[MAX_GRANULES][MAX_CHANNELS])
{
    FLOAT xmin[MAX_GRANULES][MAX_CHANNELS][SFBMAX];
    gr_info gi_backup[MAX_GRANULES][MAX_CHANNELS];
    int max_frame_bits, used_bits, ch, gr, mean_bytes;
    int retry = 0;
    memcpy(gi_backup, gfc->tt, sizeof(gi_backup));

    gfc->bitrate_index = gfc->VBR_max_bitrate;
    max_frame_bits = ResvFrameBegin(gfc, getframebytes(gfc));

    used_bits = 0;
    for (gr = 0; gr < gfc->mode_gr; gr++) {
	int used_bits_gr = 0;
	for (ch = 0; ch < gfc->channels_out; ch++) {
	    gr_info *gi = &gfc->tt[gr][ch];
	    if (init_bitalloc(gfc, gi)) {
		int next;
		calc_xmin(gfc, &ratio[gr][ch], gi, xmin[gr][ch]);
		gi->global_gain = gfc->OldValue[ch];
		while (VBR_noise_shaping(gfc, gi, xmin[gr][ch])) {
		    if (++retry > 5)
			goto do_CBR_encoding;
		    bitpressure_strategy(gi, xmin[gr][ch]);
		}
		next = gi->global_gain - Max(gi->scalefac[0]<<(gi->scalefac_scale+1), 0);
		if (next < -116)
		    next = -116;
		gfc->OldValue[ch] = next;
	    }
	    used_bits_gr += iteration_finish_one(gfc, gr, ch);
	}
	if (gfc->strict_ISO && used_bits_gr > MAX_BITS) {
	    /* this is very rare case, but may happen */
	    if (++retry > 5)
		goto do_CBR_encoding;
	    for (ch = 0; ch < gfc->channels_out; ch++)
		bitpressure_strategy(&gfc->tt[gr][ch], xmin[gr][ch]);
	    gr--;
	    used_bits_gr = 0;
	}
	used_bits += used_bits_gr;
    }

    if (used_bits > max_frame_bits) {
	used_bits = 0;
	for (gr = 0; gr < gfc->mode_gr; gr++) {
	    for (ch = 0; ch < gfc->channels_out; ch++) {
		gr_info *gi = &gfc->tt[gr][ch];
		truncate_smallspectrums(gfc, gi, xmin[gr][ch]);
		used_bits += gi->part2_3_length + gi->part2_length;
	    }
	}
	if (used_bits > max_frame_bits) {
	do_CBR_encoding:
	    /* oops, still we cannot ... */
	    memcpy(gfc->tt, gi_backup, sizeof(gi_backup));
	    gfc->bitrate_index = gfc->VBR_max_bitrate;
	    CBR_iteration_loop(gfc, ratio);
	    return;
	}
    }

    gfc->ResvSize -= used_bits;

    gfc->bitrate_index = gfc->VBR_min_bitrate;
    do {
	mean_bytes = getframebytes(gfc);
	if (ResvFrameBegin(gfc, mean_bytes) >= 0)
	    break;
    } while (++gfc->bitrate_index <= gfc->VBR_max_bitrate);
    assert(gfc->bitrate_index <= gfc->VBR_max_bitrate);

    gfc->ResvSize += mean_bytes*8;
}



/********************************************************************
 *
 *  ABR_iteration_loop()
 *
 *  encode a frame with a desired average bitrate
 *  (VBR encoding with changing noise threshold. 2004/Nov/21 tt)
 *
 ********************************************************************/
void 
ABR_iteration_loop(lame_t gfc, III_psy_ratio ratio[MAX_GRANULES][MAX_CHANNELS])
{
    int bytes;
    FLOAT threshold, newlower;
    /* encode */
    VBR_iteration_loop(gfc, ratio);

    /* adjust threshold for the next frame */
    bytes = bitrate_table[gfc->version][gfc->bitrate_index]
	- gfc->mean_bitrate_kbps;
    gfc->bytes_diff += bytes;

    threshold = (FLOAT)0.2 * gfc->mean_bitrate_kbps * gfc->frameNum;
    newlower = gfc->masking_lower_short;
    if (bytes > 0
	&& (gfc->bytes_diff > threshold
	    || (gfc->bytes_diff > 0
		&& newlower < gfc->masklower_base*(FLOAT)2.0))) {
	newlower *= (FLOAT)1.01;
    } else {
	if (gfc->bytes_diff < -threshold) {
	    newlower *= (FLOAT)0.97;
	} else if (gfc->bytes_diff < -threshold*(FLOAT)0.5) {
	    newlower *= (FLOAT)0.99;
	}
	if (newlower < gfc->masklower_base) {
	    newlower = gfc->masklower_base;
	}
    }
    if (newlower > gfc->masklower_base * (FLOAT)4.0)
	newlower = gfc->masklower_base * (FLOAT)4.0;
    gfc->masking_lower = gfc->masking_lower_short = newlower;
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
 ************************************************************************/
static void
set_pinfo(lame_t gfc, gr_info *gi, const III_psy_ratio *ratio, int gr, int ch)
{
    int i, j, end, sfb, sfb2, over;
    FLOAT en0, en1, tot_noise=(FLOAT)0.0, over_noise=(FLOAT)0.0, max_noise;
    FLOAT ifqstep = (FLOAT)0.5 * (1+gi->scalefac_scale);
    FLOAT xmin[SFBMAX], distort[SFBMAX], dummy[SFBMAX];

    init_bitalloc(gfc, gi);
    calc_xmin (gfc, ratio, gi, xmin);
    max_noise = calc_noise_allband(gfc, gi, xmin, distort, dummy);

    over = j = 0;
    for (sfb2 = 0; sfb2 < gi->psy_lmax; sfb2++) {
	end = j - gi->wi[sfb2].width;
	for (en0 = (FLOAT)0.0; j < end; j++) 
	    en0 += gi->xr[j] * gi->xr[j];
	en0=Max(en0, (FLOAT)1e-20);
	/* convert to MDCT units */
	gfc->pinfo->  en[gr][ch][sfb2] = FFT2MDCT*en0;
	gfc->pinfo-> thr[gr][ch][sfb2] = FFT2MDCT*xmin[sfb2];
	gfc->pinfo->xfsf[gr][ch][sfb2] = FFT2MDCT*xmin[sfb2]*distort[sfb2];
	en1 = FAST_LOG10(distort[sfb2]);
	if (en1 > (FLOAT)0.0) {
	    over++;
	    over_noise += en1;
	}
	tot_noise += en1;

	gfc->pinfo->LAMEsfb[gr][ch][sfb2] = 0;
	if (gi->preflag > 0 && sfb2>=11)
	    gfc->pinfo->LAMEsfb[gr][ch][sfb2] = -ifqstep*pretab[sfb2];
	if (gi->scalefac[sfb2]>=0)
	    gfc->pinfo->LAMEsfb[gr][ch][sfb2] -= ifqstep*gi->scalefac[sfb2];
    } /* for sfb */

    for (sfb = gi->sfb_smin; sfb2 < gi->psymax; sfb++) {
	for (i = 0; i < 3; i++, sfb2++) {
	    end = j - gi->wi[sfb2].width;
	    for (en0 = (FLOAT)0.0; j < end; j++)
		en0 += gi->xr[j] * gi->xr[j];

	    en0 = Max(en0, (FLOAT)1e-20);
	    /* convert to MDCT units */
	    gfc->pinfo->  en_s[gr][ch][3*sfb+i] = FFT2MDCT*en0;
	    gfc->pinfo-> thr_s[gr][ch][3*sfb+i] = FFT2MDCT*xmin[sfb2];
	    gfc->pinfo->xfsf_s[gr][ch][3*sfb+i] = FFT2MDCT*xmin[sfb2]*distort[sfb2];
	    en1 = FAST_LOG10(distort[sfb2]);
	    if (en1 > (FLOAT)0.0) {
		over++;
		over_noise += en1;
	    }
	    tot_noise += distort[sfb2];

	    gfc->pinfo->LAMEsfb_s[gr][ch][3*sfb+i]
		= -2*gi->subblock_gain[1+i]
		- ifqstep*gi->scalefac[sfb2];
	}
    } /* block type short */

    gfc->pinfo->LAMEqss     [gr][ch] = gi->global_gain;
    gfc->pinfo->LAMEmainbits[gr][ch] = gi->part2_3_length + gi->part2_length;
    gfc->pinfo->LAMEsfbits  [gr][ch] = gi->part2_length;

    gfc->pinfo->over      [gr][ch] = over;
    gfc->pinfo->max_noise [gr][ch] = FAST_LOG10(max_noise) * (FLOAT)10.0;
    gfc->pinfo->over_noise[gr][ch] = over_noise * (FLOAT)10.0;
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
set_frame_pinfo(lame_t gfc, III_psy_ratio ratio[MAX_GRANULES][MAX_CHANNELS])
{
    int gr, ch;

    /* for every granule and channel common data */
    memset(gfc->pinfo->  en_s, 0, sizeof(gfc->pinfo->  en_s));
    memset(gfc->pinfo-> thr_s, 0, sizeof(gfc->pinfo-> thr_s));
    memset(gfc->pinfo->xfsf_s, 0, sizeof(gfc->pinfo->xfsf_s));
    memset(gfc->pinfo->    en, 0, sizeof(gfc->pinfo->    en));
    memset(gfc->pinfo->   thr, 0, sizeof(gfc->pinfo->   thr));
    memset(gfc->pinfo->  xfsf, 0, sizeof(gfc->pinfo->  xfsf));

    /* copy data for MP3 frame analyzer */
    for (ch = 0; ch < gfc->channels_out; ch++) {
	int j;
	for (j = 0; j < gfc->framesize + 448; j++)
	    gfc->pinfo->pcmdata[ch][j] = gfc->mfbuf[ch][j];
	gfc->pinfo->athadjust[ch] = gfc->tt[0][ch].ATHadjust;
    }

    for (gr = 0; gr < gfc->mode_gr; gr++) {
	if (gfc->mode == JOINT_STEREO) {
	    gfc->pinfo->ms_ratio[gr] = gfc->pinfo->ms_ratio_next[gr];
	    gfc->pinfo->ms_ratio_next[gr]
		= gfc->masking_next[gr][2].pe + gfc->masking_next[gr][3].pe
		- gfc->masking_next[gr][0].pe - gfc->masking_next[gr][1].pe;
	}
	for (ch = 0; ch < gfc->channels_out; ch++) {
	    gr_info *gi = &gfc->tt[gr][ch];
	    int scalefac_sav[SFBMAX], sfb;

	    gfc->pinfo->blocktype[gr][ch] = gi->block_type;
	    gfc->pinfo->pe[gr][ch] = ratio[gr][ch].pe;
	    memcpy(gfc->pinfo->xr[gr][ch], gi->xr, sizeof(gi->xr));
	    memcpy(scalefac_sav, gi->scalefac, sizeof(scalefac_sav));

	    /* reconstruct the scalefactors in case SCFSI was used */
	    for (sfb = 0; sfb < gi->psymax; sfb++) {
		if (gi->scalefac[sfb] == SCALEFAC_SCFSI_FLAG)
		    gi->scalefac[sfb] = gfc->tt[0][ch].scalefac[sfb];
		if (gi->scalefac[sfb] == SCALEFAC_ANYTHING_GOES)
		    gi->scalefac[sfb] = 0;
	    }

	    set_pinfo(gfc, gi, &ratio[gr][ch], gr, ch);
	    memcpy(gi->scalefac, scalefac_sav, sizeof(scalefac_sav));
	} /* for ch */
    }    /* for gr */
}
#endif /* #ifndef NOANALYSIS */
