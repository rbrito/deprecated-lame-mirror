/*
 *	psymodel.c
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


/*
PSYCHO ACOUSTICS


This routine computes the psycho acoustics, delayed by one granule.  

Input: buffer of PCM data (1024 samples).  

This window should be centered over the 576 sample granule window.
The routine will compute the psycho acoustics for
this granule, but return the psycho acoustics computed
for the *previous* granule.  This is because the block
type of the previous granule can only be determined
after we have computed the psycho acoustics for the following
granule.  

Output:  maskings and energies for each scalefactor band.
block type, PE, and some correlation measures.  
The PE is used by CBR modes to determine if extra bits
from the bit reservoir should be used.  The correlation
measures are used to determine mid/side or regular stereo.
*/
/*
Notation:

barks:  a non-linear frequency scale.  Mapping from frequency to
        barks is given by freq2bark()

scalefactor bands: The spectrum (frequencies) are broken into 
                   SBMAX "scalefactor bands".  Thes bands
                   are determined by the MPEG ISO spec.  In
                   the noise shaping/quantization code, we allocate
                   bits among the partition bands to achieve the
                   best possible quality

partition bands:   The spectrum is also broken into about
                   64 "partition bands".  Each partition 
                   band is about .34 barks wide.  There are about 2-5
                   partition bands for each scalefactor band.

LAME computes all psycho acoustic information for each partition
band.  Then at the end of the computations, this information
is mapped to scalefactor bands.  The energy in each scalefactor
band is taken as the sum of the energy in all partition bands
which overlap the scalefactor band.  The maskings can be computed
in the same way (and thus represent the average masking in that band)
or by taking the minmum value multiplied by the number of
partition bands used (which represents a minimum masking in that band).
*/
/*
The general outline is as follows:

1. compute the energy in each partition band
2. compute the tonality in each partition band
3. compute the strength of each partion band "masker"
4. compute the masking (via the spreading function applied to each masker)
5. Modifications for mid/side masking.  

Each partition band is considiered a "masker".  The strength
of the i'th masker in band j is given by:

    s3(bark(i)-bark(j))*strength(i)

The strength of the masker is a function of the energy and tonality.
The more tonal, the less masking.  LAME uses a simple linear formula
(controlled by NMT and TMN) which says the strength is given by the
energy divided by a linear function of the tonality.
*/
/*
s3() is the "spreading function".  It is given by a formula
determined via listening tests.  

The total masking in the j'th partition band is the sum over
all maskings i.  It is thus given by the convolution of
the strength with s3(), the "spreading function."

masking(j) = sum_over_i  s3(i-j)*strength(i)  = s3 o strength

where "o" = convolution operator.  s3 is given by a formula determined
via listening tests.  It is normalized so that s3 o 1 = 1.

Note: instead of a simple convolution, LAME also has the
option of using "additive masking"

The most critical part is step 2, computing the tonality of each
partition band.  LAME has two tonality estimators.  The first
is based on the ISO spec, and measures how predictiable the
signal is over time.  The more predictable, the more tonal.
The second measure is based on looking at the spectrum of
a single granule.  The more peaky the spectrum, the more
tonal.  By most indications, the latter approach is better.

Finally, in step 5, the maskings for the mid and side
channel are possibly increased.  Under certain circumstances,
noise in the mid & side channels is assumed to also
be masked by strong maskers in the L or R channels.


Other data computed by the psy-model:

ms_ratio        side-channel / mid-channel masking ratio (for previous granule)
ms_ratio_next   side-channel / mid-channel masking ratio for this granule

percep_entropy[2]     L and R values (prev granule) of PE - A measure of how 
                      much pre-echo is in the previous granule
percep_entropy_MS[2]  mid and side channel values (prev granule) of percep_entropy
energy[4]             L,R,M,S energy in each channel, prev granule
blocktype_d[2]        block type to use for previous granule
*/




#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <math.h>

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

#include "util.h"
#include "encoder.h"
#include "psymodel.h"
#include "l3side.h"
#include "tables.h"
#include "machine.h"

/*
   L3psycho_anal.  Compute psycho acoustics.

   Data returned to the calling program must be delayed by one 
   granule. 

   This is done in two places.  
   If we do not need to know the blocktype, the copying
   can be done here at the top of the program: we copy the data for
   the last granule (computed during the last call) before it is
   overwritten with the new data.  It looks like this:
  
   0. static psymodel_data 
   1. calling_program_data = psymodel_data
   2. compute psymodel_data
    
   For data which needs to know the blocktype, the copying must be
   done at the end of this loop, and the old values must be saved:
   
   0. static psymodel_data_old 
   1. compute psymodel_data
   2. compute possible block type of this granule
   3. compute final block type of previous granule based on #2.
   4. calling_program_data = psymodel_data_old
   5. psymodel_data_old = psymodel_data
*/

/*
** FFT and FHT routines
**  Copyright 1988, 1993; Ron Mayer
**  
**  fht(fz,n);
**      Does a hartley transform of "n" points in the array "fz".
**      
** NOTE: This routine uses at least 2 patented algorithms, and may be
**       under the restrictions of a bunch of different organizations.
**       Although I wrote it completely myself; it is kind of a derivative
**       of a routine I once authored and released under the GPL, so it
**       may fall under the free software foundation's restrictions;
**       it was worked on as a Stanford Univ project, so they claim
**       some rights to it; it was further optimized at work here, so
**       I think this company claims parts of it.  The patents are
**       held by R. Bracewell (the FHT algorithm) and O. Buneman (the
**       trig generator), both at Stanford Univ.
**       If it were up to me, I'd say go do whatever you want with it;
**       but it would be polite to give credit to the following people
**       if you use this anywhere:
**           Euler     - probable inventor of the fourier transform.
**           Gauss     - probable inventor of the FFT.
**           Hartley   - probable inventor of the hartley transform.
**           Buneman   - for a really cool trig generator
**           Mayer(me) - for authoring this particular version and
**                       including all the optimizations in one package.
**       Thanks,
**       Ron Mayer; mayer@acuson.com
** and added some optimization by
**           Mather    - idea of using lookup table
**           Takehiro  - some dirty hack for speed up
*/

/* $Id$ */

#define TRI_SIZE (5-1) /* 1024 =  4**5 */

static const FLOAT costab[TRI_SIZE*2] = {
  9.238795325112867e-01, 3.826834323650898e-01,
  9.951847266721969e-01, 9.801714032956060e-02,
  9.996988186962042e-01, 2.454122852291229e-02,
  9.999811752826011e-01, 6.135884649154475e-03
};

void fht(FLOAT *fz, int n)
{
    const FLOAT *tri = costab;
    int           k4;
    FLOAT *fi, *fn, *gi;

    n <<= 1;        /* to get BLKSIZE, because of 3DNow! ASM routine */
    fn = fz + n;
    k4 = 4;
    do {
	FLOAT s1, c1;
	int   i, k1, k2, k3, kx;
	kx  = k4 >> 1;
	k1  = k4;
	k2  = k4 << 1;
	k3  = k2 + k1;
	k4  = k2 << 1;
	fi  = fz;
	gi  = fi + kx;
	do {
	    FLOAT f0,f1,f2,f3;
	    f1      = fi[0]  - fi[k1];
	    f0      = fi[0]  + fi[k1];
	    f3      = fi[k2] - fi[k3];
	    f2      = fi[k2] + fi[k3];
	    fi[k2]  = f0     - f2;
	    fi[0 ]  = f0     + f2;
	    fi[k3]  = f1     - f3;
	    fi[k1]  = f1     + f3;
	    f1      = gi[0]  - gi[k1];
	    f0      = gi[0]  + gi[k1];
	    f3      = SQRT2  * gi[k3];
	    f2      = SQRT2  * gi[k2];
	    gi[k2]  = f0     - f2;
	    gi[0 ]  = f0     + f2;
	    gi[k3]  = f1     - f3;
	    gi[k1]  = f1     + f3;
	    gi     += k4;
	    fi     += k4;
	} while (fi<fn);
	c1 = tri[0];
	s1 = tri[1];
	for (i = 1; i < kx; i++) {
	    FLOAT c2,s2;
	    c2 = 1 - (2*s1)*s1;
	    s2 = (2*s1)*c1;
	    fi = fz + i;
	    gi = fz + k1 - i;
	    do {
		FLOAT a,b,g0,f0,f1,g1,f2,g2,f3,g3;
		b       = s2*fi[k1] - c2*gi[k1];
		a       = c2*fi[k1] + s2*gi[k1];
		f1      = fi[0 ]    - a;
		f0      = fi[0 ]    + a;
		g1      = gi[0 ]    - b;
		g0      = gi[0 ]    + b;
		b       = s2*fi[k3] - c2*gi[k3];
		a       = c2*fi[k3] + s2*gi[k3];
		f3      = fi[k2]    - a;
		f2      = fi[k2]    + a;
		g3      = gi[k2]    - b;
		g2      = gi[k2]    + b;
		b       = s1*f2     - c1*g3;
		a       = c1*f2     + s1*g3;
		fi[k2]  = f0        - a;
		fi[0 ]  = f0        + a;
		gi[k3]  = g1        - b;
		gi[k1]  = g1        + b;
		b       = c1*g2     - s1*f3;
		a       = s1*g2     + c1*f3;
		gi[k2]  = g0        - a;
		gi[0 ]  = g0        + a;
		fi[k3]  = f1        - b;
		fi[k1]  = f1        + b;
		gi     += k4;
		fi     += k4;
	    } while (fi<fn);
	    c2 = c1;
	    c1 = c2 * tri[0] - s1 * tri[1];
	    s1 = c2 * tri[1] + s1 * tri[0];
        }
	tri += 2;
    } while (k4<n);
}

static const unsigned char rv_tbl[] = {
    0x00,    0x80,    0x40,    0xc0,    0x20,    0xa0,    0x60,    0xe0,
    0x10,    0x90,    0x50,    0xd0,    0x30,    0xb0,    0x70,    0xf0,
    0x08,    0x88,    0x48,    0xc8,    0x28,    0xa8,    0x68,    0xe8,
    0x18,    0x98,    0x58,    0xd8,    0x38,    0xb8,    0x78,    0xf8,
    0x04,    0x84,    0x44,    0xc4,    0x24,    0xa4,    0x64,    0xe4,
    0x14,    0x94,    0x54,    0xd4,    0x34,    0xb4,    0x74,    0xf4,
    0x0c,    0x8c,    0x4c,    0xcc,    0x2c,    0xac,    0x6c,    0xec,
    0x1c,    0x9c,    0x5c,    0xdc,    0x3c,    0xbc,    0x7c,    0xfc,
    0x02,    0x82,    0x42,    0xc2,    0x22,    0xa2,    0x62,    0xe2,
    0x12,    0x92,    0x52,    0xd2,    0x32,    0xb2,    0x72,    0xf2,
    0x0a,    0x8a,    0x4a,    0xca,    0x2a,    0xaa,    0x6a,    0xea,
    0x1a,    0x9a,    0x5a,    0xda,    0x3a,    0xba,    0x7a,    0xfa,
    0x06,    0x86,    0x46,    0xc6,    0x26,    0xa6,    0x66,    0xe6,
    0x16,    0x96,    0x56,    0xd6,    0x36,    0xb6,    0x76,    0xf6,
    0x0e,    0x8e,    0x4e,    0xce,    0x2e,    0xae,    0x6e,    0xee,
    0x1e,    0x9e,    0x5e,    0xde,    0x3e,    0xbe,    0x7e,    0xfe
};

#define ch01(index)  (buffer[chn][index])

#define ml00(f)	(window[i        ] * f(i))
#define ml10(f)	(window[i + 0x200] * f(i + 0x200))
#define ml20(f)	(window[i + 0x100] * f(i + 0x100))
#define ml30(f)	(window[i + 0x300] * f(i + 0x300))

#define ml01(f)	(window[i + 0x001] * f(i + 0x001))
#define ml11(f)	(window[i + 0x201] * f(i + 0x201))
#define ml21(f)	(window[i + 0x101] * f(i + 0x101))
#define ml31(f)	(window[i + 0x301] * f(i + 0x301))

#define ms00(f)	(window_s[i       ] * f(i + k))
#define ms10(f)	(window_s[0x7f - i] * f(i + k + 0x80))
#define ms20(f)	(window_s[i + 0x40] * f(i + k + 0x40))
#define ms30(f)	(window_s[0x3f - i] * f(i + k + 0xc0))

#define ms01(f)	(window_s[i + 0x01] * f(i + k + 0x01))
#define ms11(f)	(window_s[0x7e - i] * f(i + k + 0x81))
#define ms21(f)	(window_s[i + 0x41] * f(i + k + 0x41))
#define ms31(f)	(window_s[0x3e - i] * f(i + k + 0xc1))


static void
fft_short(lame_internal_flags * const gfc, 
	  FLOAT x_real[3][BLKSIZE_s], int chn, const sample_t *buffer[2])
{
    int           i;
    int           j;
    int           b;

    for (b = 0; b < 3; b++) {
	FLOAT *x = &x_real[b][BLKSIZE_s / 2];
	short k = (576 / 3) * (b + 1);
	j = BLKSIZE_s / 8 - 1;
	do {
	  FLOAT f0,f1,f2,f3, w;

	  i = rv_tbl[j << 2];

	  f0 = ms00(ch01); w = ms10(ch01); f1 = f0 - w; f0 = f0 + w;
	  f2 = ms20(ch01); w = ms30(ch01); f3 = f2 - w; f2 = f2 + w;

	  x -= 4;
	  x[0] = f0 + f2;
	  x[2] = f0 - f2;
	  x[1] = f1 + f3;
	  x[3] = f1 - f3;

	  f0 = ms01(ch01); w = ms11(ch01); f1 = f0 - w; f0 = f0 + w;
	  f2 = ms21(ch01); w = ms31(ch01); f3 = f2 - w; f2 = f2 + w;

	  x[BLKSIZE_s / 2 + 0] = f0 + f2;
	  x[BLKSIZE_s / 2 + 2] = f0 - f2;
	  x[BLKSIZE_s / 2 + 1] = f1 + f3;
	  x[BLKSIZE_s / 2 + 3] = f1 - f3;
	} while (--j >= 0);

	gfc->fft_fht(x, BLKSIZE_s/2);   
        /* BLKSIZE_s/2 because of 3DNow! ASM routine */
    }
}

static void
fft_long(lame_internal_flags * const gfc,
	 FLOAT x[BLKSIZE], int chn, const sample_t *buffer[2] )
{
    int           i;
    int           jj = BLKSIZE / 8 - 1;
    x += BLKSIZE / 2;

    do {
      FLOAT f0,f1,f2,f3, w;

      i = rv_tbl[jj];
      f0 = ml00(ch01); w = ml10(ch01); f1 = f0 - w; f0 = f0 + w;
      f2 = ml20(ch01); w = ml30(ch01); f3 = f2 - w; f2 = f2 + w;

      x -= 4;
      x[0] = f0 + f2;
      x[2] = f0 - f2;
      x[1] = f1 + f3;
      x[3] = f1 - f3;

      f0 = ml01(ch01); w = ml11(ch01); f1 = f0 - w; f0 = f0 + w;
      f2 = ml21(ch01); w = ml31(ch01); f3 = f2 - w; f2 = f2 + w;

      x[BLKSIZE / 2 + 0] = f0 + f2;
      x[BLKSIZE / 2 + 2] = f0 - f2;
      x[BLKSIZE / 2 + 1] = f1 + f3;
      x[BLKSIZE / 2 + 3] = f1 - f3;
    } while (--jj >= 0);

    gfc->fft_fht(x, BLKSIZE/2);
    /* BLKSIZE/2 because of 3DNow! ASM routine */
}


/* psycho_loudness_approx
   jd - 2001 mar 12
in:  energy   - BLKSIZE/2 elements of frequency magnitudes ^ 2
     gfp      - uses out_samplerate, ATHtype (also needed for ATHformula)
returns: loudness^2 approximation, a positive value roughly tuned for a value
         of 1.0 for signals near clipping.
notes:   When calibrated, feeding this function binary white noise at sample
         values +32767 or -32768 should return values that approach 3.
         ATHformula is used to approximate an equal loudness curve.
future:  Data indicates that the shape of the equal loudness curve varies
         with intensity.  This function might be improved by using an equal
         loudness curve shaped for typical playback levels (instead of the
         ATH, that is shaped for the threshold).  A flexible realization might
         simply bend the existing ATH curve to achieve the desired shape.
         However, the potential gain may not be enough to justify an effort.
*/
static FLOAT
psycho_loudness_approx( FLOAT *energy, lame_internal_flags *gfc )
{
    int i;
    FLOAT loudness_power;

/* tuned for output level (sensitive to energy scale) */
#define vo_scale (1./( 14752 ))

    loudness_power = 0.0;
    /* apply weights to power in freq. bands*/
    for( i = 0; i < BLKSIZE/2; ++i )
	loudness_power += energy[i] * gfc->ATH.eql_w[i];
    loudness_power = loudness_power;
    loudness_power *= (vo_scale * vo_scale / (BLKSIZE/2));

    return loudness_power;
}

static void
compute_ffts(
    lame_global_flags *gfp,
    FLOAT fftenergy[HBLKSIZE],
    FLOAT (*fftenergy_s)[HBLKSIZE_s],
    FLOAT (*wsamp_l)[BLKSIZE],
    FLOAT (*wsamp_s)[3][BLKSIZE_s],
    int gr_out,
    int chn,
    const sample_t *buffer[2]
    )
{
    int b, j;
    lame_internal_flags *gfc=gfp->internal_flags;
    if (chn<2) {
	fft_long ( gfc, *wsamp_l, chn, buffer);
	fft_short( gfc, *wsamp_s, chn, buffer);
    }
    /* FFT data for mid and side channel is derived from L & R */
    else if (chn == 2) {
	for (j = BLKSIZE-1; j >=0 ; --j) {
	    FLOAT l = wsamp_l[0][j];
	    FLOAT r = wsamp_l[1][j];
	    wsamp_l[0][j] = (l+r)*(FLOAT)(SQRT2*0.5);
	    wsamp_l[1][j] = (l-r)*(FLOAT)(SQRT2*0.5);
	}
	for (b = 2; b >= 0; --b) {
	    for (j = BLKSIZE_s-1; j >= 0 ; --j) {
		FLOAT l = wsamp_s[0][b][j];
		FLOAT r = wsamp_s[1][b][j];
		wsamp_s[0][b][j] = (l+r)*(FLOAT)(SQRT2*0.5);
		wsamp_s[1][b][j] = (l-r)*(FLOAT)(SQRT2*0.5);
	    }
	}
    }

    /*********************************************************************
     *  compute energies
     *********************************************************************/
    fftenergy[0]  = wsamp_l[0][0] * wsamp_l[0][0];
    for (j=BLKSIZE/2-1; j >= 0; --j) {
	FLOAT re = (*wsamp_l)[BLKSIZE/2-j];
	FLOAT im = (*wsamp_l)[BLKSIZE/2+j];
	fftenergy[BLKSIZE/2-j] = (re * re + im * im) * 0.5f;
    }
    for (b = 2; b >= 0; --b) {
	fftenergy_s[b][0]  = (*wsamp_s)[b][0];
	fftenergy_s[b][0] *=  fftenergy_s [b][0];
	for (j=BLKSIZE_s/2-1; j >= 0; --j) {
	    FLOAT re = (*wsamp_s)[b][BLKSIZE_s/2-j];
	    FLOAT im = (*wsamp_s)[b][BLKSIZE_s/2+j];
	    fftenergy_s[b][BLKSIZE_s/2-j] = (re * re + im * im) * 0.5f;
	}
    }
    /* total energy */
    {FLOAT8 totalenergy=0.0;
    for (j=11;j < HBLKSIZE; j++)
	totalenergy += fftenergy[j];

    gfc->tot_ener[chn] = totalenergy;
    }

#if defined(HAVE_GTK)
    if (gfp->analysis) {
	for (j=0; j<HBLKSIZE ; j++) {
	    gfc->pinfo->energy[gr_out][chn][j]=gfc->energy_save[chn][j];
	    gfc->energy_save[chn][j]=fftenergy[j];
	}
    }
#endif
    /*********************************************************************
     * compute loudness approximation (used for ATH auto-level adjustment) 
     *********************************************************************/
    if (gfp->athaa_loudapprox == 2 && chn < 2) {/*no loudness for mid/side ch*/
	gfc->loudness_sq[gr_out][chn] = gfc->loudness_sq_save[chn];
	gfc->loudness_sq_save[chn]
	    = psycho_loudness_approx(fftenergy, gfc);
    }
}

/*************************************************************** 
 * compute interchannel masking effects
 ***************************************************************/
static void
calc_interchannel_masking(
    lame_global_flags * gfp,
    FLOAT8 ratio
    )
{
    lame_internal_flags *gfc=gfp->internal_flags;
    int sb, sblock;
    FLOAT8 l, r;
    for ( sb = 0; sb < SBMAX_l; sb++ ) {
	l = gfc->thm[0].l[sb];
	r = gfc->thm[1].l[sb];
	gfc->thm[0].l[sb] += r*ratio;
	gfc->thm[1].l[sb] += l*ratio;
    }
    for ( sb = 0; sb < SBMAX_s; sb++ ) {
	for ( sblock = 0; sblock < 3; sblock++ ) {
	    l = gfc->thm[0].s[sb][sblock];
	    r = gfc->thm[1].s[sb][sblock];
	    gfc->thm[0].s[sb][sblock] += r*ratio;
	    gfc->thm[1].s[sb][sblock] += l*ratio;
	}
    }
}



/*************************************************************** 
 * compute M/S thresholds from Johnston & Ferreira 1992 ICASSP paper
 ***************************************************************/
static void
msfix1(
    lame_internal_flags *gfc
    )
{
    int sb, sblock;
    FLOAT8 rside,rmid,mld;
#define chmid 2
#define chside 3
    for ( sb = 0; sb < SBMAX_l; sb++ ) {
	/* use this fix if L & R masking differs by 2db or less */
	/* if db = 10*log10(x2/x1) < 2 */
	/* if (x2 < 1.58*x1) { */
	if (gfc->thm[0].l[sb] > 1.58*gfc->thm[1].l[sb]
	 || gfc->thm[1].l[sb] > 1.58*gfc->thm[0].l[sb])
	    continue;

	mld = gfc->mld_l[sb]*gfc->en[chside].l[sb];
	rmid = Max(gfc->thm[chmid].l[sb], Min(gfc->thm[chside].l[sb],mld));

	mld = gfc->mld_l[sb]*gfc->en[chmid].l[sb];
	rside = Max(gfc->thm[chside].l[sb], Min(gfc->thm[chmid].l[sb],mld));
	gfc->thm[chmid].l[sb]=rmid;
	gfc->thm[chside].l[sb]=rside;
    }

    for ( sb = 0; sb < SBMAX_s; sb++ ) {
	for ( sblock = 0; sblock < 3; sblock++ ) {
	    if (gfc->thm[0].s[sb][sblock] > 1.58*gfc->thm[1].s[sb][sblock]
	     || gfc->thm[1].s[sb][sblock] > 1.58*gfc->thm[0].s[sb][sblock])
		continue;

	    mld = gfc->mld_s[sb]*gfc->en[chside].s[sb][sblock];
	    rmid = Max(gfc->thm[chmid].s[sb][sblock],
		       Min(gfc->thm[chside].s[sb][sblock],mld));

	    mld = gfc->mld_s[sb]*gfc->en[chmid].s[sb][sblock];
	    rside = Max(gfc->thm[chside].s[sb][sblock],
			Min(gfc->thm[chmid].s[sb][sblock],mld));

	    gfc->thm[chmid].s[sb][sblock]=rmid;
	    gfc->thm[chside].s[sb][sblock]=rside;
	}
    }
}

/*************************************************************** 
 * Adjust M/S maskings if user set "msfix"
 ***************************************************************/
/* Naoki Shibata 2000 */
static void
ns_msfix(
    lame_internal_flags *gfc,
    FLOAT8 msfix,
    FLOAT8 athadjust
    )
{
    int sb, sblock;
    FLOAT8 msfix2 = msfix;
    FLOAT8 athlower = pow(10, athadjust);
    if (gfc->presetTune.use)
	msfix2 = gfc->presetTune.ms_maskadjust;

    msfix *= 2.0;
    msfix2 *= 2.0;
    for ( sb = 0; sb < SBMAX_l; sb++ ) {
	FLOAT8 thmL,thmR,thmM,thmS,ath;
	ath  = (gfc->ATH.cb[gfc->bm_l[sb]])*athlower;
	thmL = Max(gfc->thm[0].l[sb],ath);
	thmR = Max(gfc->thm[1].l[sb],ath);
	thmM = Max(gfc->thm[2].l[sb],ath);
	thmS = Max(gfc->thm[3].l[sb],ath);

	if (thmL*msfix < thmM+thmS) {
	    FLOAT8 f = thmL * msfix2 / (thmM+thmS);
	    thmM *= f;
	    thmS *= f;
	}
	if (thmR*msfix < thmM+thmS) {
	    FLOAT8 f = thmR * msfix2 / (thmM+thmS);
	    thmM *= f;
	    thmS *= f;
	}
	gfc->thm[2].l[sb] = Min(thmM,gfc->thm[2].l[sb]);
	gfc->thm[3].l[sb] = Min(thmS,gfc->thm[3].l[sb]);
    }

    for ( sb = 0; sb < SBMAX_s; sb++ ) {
	for ( sblock = 0; sblock < 3; sblock++ ) {
	    FLOAT8 thmL,thmR,thmM,thmS,ath;
	    ath  = (gfc->ATH.cb[gfc->bm_s[sb]])*athlower;
	    thmL = Max(gfc->thm[0].s[sb][sblock],ath);
	    thmR = Max(gfc->thm[1].s[sb][sblock],ath);
	    thmM = Max(gfc->thm[2].s[sb][sblock],ath);
	    thmS = Max(gfc->thm[3].s[sb][sblock],ath);

	    if (thmL*msfix < thmM+thmS) {
		FLOAT8 f = thmL*msfix / (thmM+thmS);
		thmM *= f;
		thmS *= f;
	    }
	    if (thmR*msfix < thmM+thmS) {
		FLOAT8 f = thmR*msfix / (thmM+thmS);
		thmM *= f;
		thmS *= f;
	    }
	    gfc->thm[2].s[sb][sblock] = Min(gfc->thm[2].s[sb][sblock],thmM);
	    gfc->thm[3].s[sb][sblock] = Min(gfc->thm[3].s[sb][sblock],thmS);
	}
    }
}

static FLOAT8 calc_mixed_ratio(
    lame_internal_flags *gfc,
    int chn
    )
{
    int sb;
    FLOAT8 m0 = 1.0;
    for (sb = 0; sb < 8; sb++) {
	if (gfc->en[chn].l[sb] > gfc->thm[chn].l[sb]
	    && gfc->thm[chn].l[sb] > 0.0) {
	    m0 *= gfc-> en[chn].l[sb] / gfc->thm[chn].l[sb];
	}
    }

    for (sb = 0; sb < 3; sb++) {
	if (gfc->en[chn].s[sb][0] > gfc->thm[chn].s[sb][0]
	    && gfc->thm[chn].s[sb][0] > 0.0) {
	    m0 *= gfc->thm[chn].s[sb][0] / gfc-> en[chn].s[sb][0];
	}
	if (gfc->en[chn].s[sb][1] > gfc->thm[chn].s[sb][1]
	    && gfc->thm[chn].s[sb][1] > 0.0) {
	    m0 *= gfc->thm[chn].s[sb][1] / gfc-> en[chn].s[sb][1];
	}
	if (gfc->en[chn].s[sb][2] > gfc->thm[chn].s[sb][2]
	    && gfc->thm[chn].s[sb][2] > 0.0) {
	    m0 *= gfc->thm[chn].s[sb][2] / gfc-> en[chn].s[sb][2];
	}
    }
    return m0;
}

/* longblock threshold calculation (part 2) */
static void convert_partition2scalefac_l(
    lame_internal_flags *gfc,
    FLOAT8 *eb,
    FLOAT8 *thr,
    int chn
    )
{
    FLOAT8 enn, thmm;
    int sb, b;
    enn = thmm = 0.0;
    sb = b = 0;
    for (;;) {
	while (b < gfc->bo_l[sb]) {
	    enn  += eb[b];
	    thmm += thr[b];
	    b++;
	}

	if (sb == SBMAX_l - 1)
	    break;

	gfc->en [chn].l[sb] = enn  + 0.5 * eb [b];
	gfc->thm[chn].l[sb] = thmm + 0.5 * thr[b];

	enn  = 0.5 *  eb[b];
	thmm = 0.5 * thr[b];
	b++;
	sb++;
    }

    gfc->en [chn].l[SBMAX_l-1] = enn;
    gfc->thm[chn].l[SBMAX_l-1] = thmm;
}

static void
compute_masking_s(
    lame_internal_flags *gfc,
    FLOAT (*fftenergy_s)[HBLKSIZE_s],
    FLOAT8 *eb,
    FLOAT8 *thr,
    int chn,
    int sblock
    )
{
    int j, b;
    for (j = b = 0; b < gfc->npart_s; b++) {
	FLOAT ecb = fftenergy_s[sblock][j++];
	int kk = gfc->numlines_s[b];
	while (--kk > 0)
	    ecb += fftenergy_s[sblock][j++];
	eb[b] = ecb;
    }
    for (j = b = 0; b < gfc->npart_s; b++) {
	int kk = gfc->s3ind_s[b][0];
	FLOAT8 ecb = gfc->s3_ss[j++] * eb[kk++];
	while (kk <= gfc->s3ind_s[b][1])
	    ecb += gfc->s3_ss[j++] * eb[kk++];

	thr[b] = Min( ecb, rpelev_s  * gfc->nb_s1[chn][b] );
	if (gfc->blocktype_old[chn & 1] == SHORT_TYPE ) {
	    thr[b] = Min(thr[b], rpelev2_s * gfc->nb_s2[chn][b]);
	}
	thr[b] = Max( thr[b], 1e-37 );
	gfc->nb_s2[chn][b] = gfc->nb_s1[chn][b];
	gfc->nb_s1[chn][b] = ecb;
    }
}

static void
block_type_set(
    lame_global_flags * gfp,
    int *uselongblock,
    int *blocktype_d,
    int *blocktype
    )
{
    lame_internal_flags *gfc=gfp->internal_flags;
    int chn;

    if (gfp->short_blocks == short_block_coupled
	/* force both channels to use the same block type */
	/* this is necessary if the frame is to be encoded in ms_stereo.  */
	/* But even without ms_stereo, FhG  does this */
	&& !(uselongblock[0] && uselongblock[1]))
        uselongblock[0] = uselongblock[1] = 0;

    /* update the blocktype of the previous granule, since it depends on what
     * happend in this granule */
    for (chn=0; chn<gfc->channels_out; chn++) {
	blocktype[chn] = NORM_TYPE;
	/* disable short blocks */
	if (gfp->short_blocks == short_block_dispensed)
	    uselongblock[chn]=1;
	if (gfp->short_blocks == short_block_forced)
	    uselongblock[chn]=0;

	if (uselongblock[chn]) {
	    /* no attack : use long blocks */
	    assert( gfc->blocktype_old[chn] != START_TYPE );
	    if (gfc->blocktype_old[chn] == SHORT_TYPE)
		blocktype[chn] = STOP_TYPE;
	} else {
	    /* attack : use short blocks */
	    blocktype[chn] = SHORT_TYPE;
	    if (gfc->blocktype_old[chn] == NORM_TYPE) {
		int oldblocktype = START_TYPE;
		if (calc_mixed_ratio(gfc, chn) > 1000) {
		    blocktype[chn] = -SHORT_TYPE;
		    oldblocktype = -oldblocktype;
		}
		gfc->blocktype_old[chn] = oldblocktype;
	    }
	    if (gfc->blocktype_old[chn] == STOP_TYPE)
		gfc->blocktype_old[chn] = SHORT_TYPE;
	}

	blocktype_d[chn] = gfc->blocktype_old[chn];  /* value returned to calling program */
	gfc->blocktype_old[chn] = blocktype[chn];    /* save for next call to l3psy_anal */
    }
}

/* mask_add optimization */
/* init the limit values used to avoid computing log in mask_add when it is not necessary */

/* For example, with i = 10*log10(m2/m1)/10*16         (= log10(m2/m1)*16)
 *
 * abs(i)>8 is equivalent (as i is an integer) to
 * abs(i)>=9
 * i>=9 || i<=-9
 * equivalent to (as i is the biggest integer smaller than log10(m2/m1)*16 
 * or the smallest integer bigger than log10(m2/m1)*16 depending on the sign of log10(m2/m1)*16)
 * log10(m2/m1)>=9/16 || log10(m2/m1)<=-9/16
 * exp10 is strictly increasing thus this is equivalent to
 * m2/m1 >= 10^(9/16) || m2/m1<=10^(-9/16) which are comparisons to constants
 */


#define I1LIMIT 8   /* as in if(i>8)  */ 
#define I2LIMIT 24  /* as in if(i>24) */ 
#define MLIMIT  15  /* as in if(m<15) */ 

static FLOAT8 ma_max_i1;
static FLOAT8 ma_min_i1;
static FLOAT8 ma_max_i2;
static FLOAT8 ma_min_i2;
static FLOAT8 ma_max_m;



void init_mask_add_max_values(void)
{
    ma_max_i1 = pow(10,(I1LIMIT+1)/16.0);
    ma_min_i1 = pow(10,-(I1LIMIT+1)/16.0);
    ma_max_i2 = pow(10,(I2LIMIT+1)/16.0);
    ma_min_i2 = pow(10,-(I2LIMIT+1)/16.0);
    ma_max_m  = pow(10,(MLIMIT)/10.0);
}






/* addition of simultaneous masking   Naoki Shibata 2000/7 */
inline static FLOAT8 mask_add(FLOAT8 m1,FLOAT8 m2,int k,int b, lame_internal_flags * const gfc)
{
  static const FLOAT8 table1[] = {
    3.3246 *3.3246 ,3.23837*3.23837,3.15437*3.15437,3.00412*3.00412,2.86103*2.86103,2.65407*2.65407,2.46209*2.46209,2.284  *2.284  ,
    2.11879*2.11879,1.96552*1.96552,1.82335*1.82335,1.69146*1.69146,1.56911*1.56911,1.46658*1.46658,1.37074*1.37074,1.31036*1.31036,
    1.25264*1.25264,1.20648*1.20648,1.16203*1.16203,1.12765*1.12765,1.09428*1.09428,1.0659 *1.0659 ,1.03826*1.03826,1.01895*1.01895,
    1
  };

  static const FLOAT8 table2[] = {
    1.33352*1.33352,1.35879*1.35879,1.38454*1.38454,1.39497*1.39497,1.40548*1.40548,1.3537 *1.3537 ,1.30382*1.30382,1.22321*1.22321,
    1.14758*1.14758
  };

  static const FLOAT8 table3[] = {
    2.35364*2.35364,2.29259*2.29259,2.23313*2.23313,2.12675*2.12675,2.02545*2.02545,1.87894*1.87894,1.74303*1.74303,1.61695*1.61695,
    1.49999*1.49999,1.39148*1.39148,1.29083*1.29083,1.19746*1.19746,1.11084*1.11084,1.03826*1.03826
  };


  int i;
  FLOAT8 m;
  FLOAT ratio;

  if (m1 == 0) return m2;

  ratio = m2/m1;

  if (b < 0) b = -b;

  /*i = abs(10*log10(m2 / m1)/10*16);
  m = 10*log10((m1+m2)/gfc->ATH.cb[k]);*/


  /* Should always be true, just checking */
  assert(m1>=0);
  assert(m2>=0);
  assert(gfc->ATH.cb[k]>=0);


  if (b <= 3) {  /* approximately, 1 bark = 3 partitions */
    /* 65% of the cases */

    /* originally 'if(i > 8)' */
    if(ratio>=ma_max_i1 || ratio <=ma_min_i1) {
      /* 43% of the total */
      return m1+m2;
    }

    /* 22% of the total */
    i = fabs(FAST_LOG10(ratio))*16;
    if (i > I1LIMIT)
      return m1+m2; /* just in case  8.99999 <= log10(m2/m1)*16 < 9 */

    return (m1+m2)*table2[i];
  }


  /* m<15 equ log10((m1+m2)/gfc->ATH.cb[k])<1.5
   * equ (m1+m2)/gfc->ATH.cb[k]<10^1.5
   * equ (m1+m2)<10^1.5 * gfc->ATH.cb[k]
   */

  if((m1+m2)<ma_max_m*gfc->ATH.cb[k])  {
    /* 3% of the total */
    i = fabs(FAST_LOG10(ratio))*16;

    /* Originally if (m > 0) { */
    if(m1+m2>gfc->ATH.cb[k]) {
      FLOAT8 f=1.0,r;

      if (i > 24) return m1+m2;

      if (i > 13) f = 1; else f = table3[i];

      m = 10*FAST_LOG10((m1+m2)/gfc->ATH.cb[k]);
      r = (m-0)/15;

      return (m1+m2)*(table1[i]*r+f*(1-r));
    }

    if (i > 13) return m1+m2;

    return (m1+m2)*table3[i];
  }


  /* orginally 'if (i > 24) {' */
  if(ratio>=ma_max_i2 || ratio <=ma_min_i2) {
    /* 22% of total */
    return m1+m2;
  }

  
  /* 10% of total */
  i = fabs(FAST_LOG10(ratio))*16;
  if (i > I2LIMIT)
    return m1+m2; /* just in case... */

  return (m1+m2)*table1[i];
}



static inline FLOAT8 NS_INTERP(FLOAT8 x, FLOAT8 y, FLOAT8 r)
{
    /* was pow((x),(r))*pow((y),1-(r))*/
    if(r==1.0)
        return x;              /* 99.7% of the time */ 
    if(y!=0.0)
        return pow(x/y,r)*y;   /* rest of the time */ 
    return 0.0;                /* never happens */ 
}



static void nsPsy2dataRead(
    FILE *fp,
    FLOAT8 *eb2,
    FLOAT8 *eb,
    int chn,
    int npart_l
    )
{
    int b;
    for(;;) {
	static const char chname[] = {'L','R','M','S'};
	char c;

	fscanf(fp, "%c",&c);
	for (b=0; b < npart_l; b++) {
	    double e;
	    fscanf(fp, "%lf",&e);
	    eb2[b] = e;
	}

	if (feof(fp)) abort();
	if (c == chname[chn]) break;
	abort();
    }

    eb2[62] = eb2[61];
    for (b=0; b < npart_l; b++ )
	eb2[b] = eb2[b] * eb[b];
}

static FLOAT
pecalc_s(
    III_psy_ratio *mr,
    FLOAT8 masking_lower
    )
{
    FLOAT8 pe_s;
    const static FLOAT8 regcoef_s[] = {
	0, 0, 0, /* I don't know why there're 0 -tt- */
	0.434542,25.0738,
	0, 0, 0,
	19.5442,19.7486,60,100,0
    };
    int sb, sblock;

    pe_s = 1236.28/4;
    for(sblock=0;sblock<3;sblock++) {
	for ( sb = 0; sb < SBMAX_s; sb++ ) {
	    FLOAT x;
	    if (regcoef_s[sb] == 0.0
		|| mr->thm.s[sb][sblock] <= 0.0
		|| mr->en.s[sb][sblock]
		<= (x = mr->thm.s[sb][sblock] * masking_lower))
		continue;

	    if (mr->en.s[sb][sblock] > x*1e10)
		pe_s += regcoef_s[sb] * (10.0 * LOG10);
	    else
		pe_s += regcoef_s[sb] * FAST_LOG10(mr->en.s[sb][sblock] / x);
	}
    }
    return pe_s;
}

static FLOAT
pecalc_l(
    III_psy_ratio *mr,
    FLOAT8 masking_lower
    )
{
    FLOAT8 pe_l;
    const static FLOAT8 regcoef_l[] = {
	10.0583,10.7484,7.29006,16.2714,6.2345,4.09743,3.05468,3.33196,
	2.54688, 3.68168,5.83109,2.93817,-8.03277,-10.8458,8.48777,
	9.13182,2.05212,8.6674,50.3937,73.267,97.5664,0
    };
    int sb;

    pe_l = 1124.23/4;
    for ( sb = 0; sb < SBMAX_l; sb++ ) {
	FLOAT x;
	if (mr->thm.l[sb] <= 0.0
	    || mr->en.l[sb] <= (x = mr->thm.l[sb]*masking_lower))
	    continue;

	if (mr->en.l[sb] > x*1e10)
	    pe_l += regcoef_l[sb] * (10.0 * LOG10);
	else
	    pe_l += regcoef_l[sb] * FAST_LOG10(mr->en.l[sb] / x);
    }

    return pe_l;
}





int L3psycho_anal_ns( lame_global_flags * gfp,
                    const sample_t *buffer[2], int gr_out, 
                    FLOAT8 *ms_ratio,
                    FLOAT8 *ms_ratio_next,
		    III_psy_ratio masking_ratio[2][2],
		    III_psy_ratio masking_MS_ratio[2][2],
		    FLOAT8 percep_entropy[2],FLOAT8 percep_MS_entropy[2], 
		    FLOAT8 energy[4], 
                    int blocktype_d[2])
{
/* to get a good cache performance, one has to think about
 * the sequence, in which the variables are used.  
 * (Note: these static variables have been moved to the gfc-> struct,
 * and their order in memory is layed out in util.h)
 */
    lame_internal_flags *gfc=gfp->internal_flags;

    /* fft and energy calculation   */
    FLOAT wsamp_L[2][BLKSIZE];
    FLOAT wsamp_S[2][3][BLKSIZE_s];

    /* block type  */
    int blocktype[2],uselongblock[4];

    /* usual variables like loop indices, etc..    */
    int numchn, chn;
    int b, i, j, k;
    int	sb,sblock;

    /* variables used for --nspsytune */
    FLOAT ns_hpfsmpl[2][576];
    FLOAT pcfact;

    numchn = gfc->channels_out;
    /* chn=2 and 3 = Mid and Side channels */
    if (gfp->mode == JOINT_STEREO) numchn=4;

    if (gfp->VBR==vbr_off) pcfact = gfc->ResvMax == 0 ? 0 : ((FLOAT)gfc->ResvSize)/gfc->ResvMax*0.5;
    else if (gfp->VBR == vbr_rh  ||  gfp->VBR == vbr_mtrh  ||  gfp->VBR == vbr_mt) {
	static const FLOAT pcQns[10]={1.0,1.0,1.0,0.8,0.6,0.5,0.4,0.3,0.2,0.1};
	pcfact = pcQns[gfp->VBR_q];
    } else pcfact = 1.0;

    /**********************************************************************
     *  Apply HPF of fs/4 to the input signal.
     *  This is used for attack detection / handling.
     **********************************************************************/
    /* Don't copy the input buffer into a temporary buffer */
    /* unroll the loop 2 times */
    for(chn=0;chn<gfc->channels_out;chn++) {
	static const FLOAT fircoef[] = {
	    -8.65163e-18*2, -0.00851586*2, -6.74764e-18*2, 0.0209036*2,
	    -3.36639e-17*2, -0.0438162 *2, -1.54175e-17*2, 0.0931738*2,
	    -5.52212e-17*2, -0.313819  *2
	};

	/* apply high pass filter of fs/4 */
	const sample_t * const firbuf = &buffer[chn][576-350-NSFIRLEN+192];
	for (i=0;i<576;i++) {
	    FLOAT sum1, sum2;
	    sum1 = firbuf[i + 10];
	    sum2 = 0.0;
	    for (j=0;j<(NSFIRLEN-1)/2;j+=2) {
		sum1 += fircoef[j  ] * (firbuf[i+j  ]+firbuf[i+NSFIRLEN-j  ]);
		sum2 += fircoef[j+1] * (firbuf[i+j+1]+firbuf[i+NSFIRLEN-j-1]);
	    }
	    ns_hpfsmpl[chn][i] = sum1 + sum2;
	}
	masking_ratio    [gr_out] [chn]  .en  = gfc -> en  [chn];
	masking_ratio    [gr_out] [chn]  .thm = gfc -> thm [chn];
	if (numchn > 2) {
	    /* MS maskings  */
	    //percep_MS_entropy         [chn-2]     = gfc -> pe  [chn]; 
	    masking_MS_ratio [gr_out] [chn].en  = gfc -> en  [chn+2];
	    masking_MS_ratio [gr_out] [chn].thm = gfc -> thm [chn+2];
	}
    }

    for (chn=0; chn<numchn; chn++) {
	FLOAT (*wsamp_l)[BLKSIZE];
	FLOAT (*wsamp_s)[3][BLKSIZE_s];
	FLOAT en_subshort[12];
	FLOAT attack_intensity[12];
	int ns_uselongblock = 1;
	FLOAT attackThreshold;
	FLOAT8 max[CBANDS],avg[CBANDS];
	int ns_attacks[4] = {0};
	FLOAT fftenergy[HBLKSIZE];
	FLOAT fftenergy_s[3][HBLKSIZE_s];
	/* convolution   */
	FLOAT8 eb[CBANDS],eb2[CBANDS];
	FLOAT8 thr[CBANDS];
	static const FLOAT8 tab[] = {
	    1.0,     0.79433, 0.63096, 0.63096,
	    0.63096, 0.63096, 0.63096, 0.25119,
	    0.11749
	};

	/*************************************************************** 
	 * determine the block type (window type)
	 ***************************************************************/
	/* calculate energies of each sub-shortblocks */
	for (i=0; i<3; i++) {
	    en_subshort[i] = gfc->nsPsy.last_en_subshort[chn][i+6];
	    attack_intensity[i]
		= en_subshort[i] / gfc->nsPsy.last_en_subshort[chn][i+4];
	}

	if (chn == 2) {
	    for(i=0;i<576;i++) {
		FLOAT l, r;
		l = ns_hpfsmpl[0][i];
		r = ns_hpfsmpl[1][i];
		ns_hpfsmpl[0][i] = l+r;
		ns_hpfsmpl[1][i] = l-r;
	    }
	}
	{
	    FLOAT *pf = ns_hpfsmpl[chn & 1];
	    for (i=0;i<9;i++) {
		FLOAT *pfe = pf + 576/9, p = 10.;
		for (; pf < pfe; pf++)
		    if (p < fabs(*pf))
			p = fabs(*pf);

		gfc->nsPsy.last_en_subshort[chn][i] = en_subshort[i+3] = p;
		attack_intensity[i+3] = p / en_subshort[i+3-2];
	    }
	}
#if defined(HAVE_GTK)
	if (gfp->analysis) {
	    FLOAT x = attack_intensity[0];
	    for (i=1;i<12;i++) 
		if (x < attack_intensity[i])
		    x = attack_intensity[i];
	    gfc->pinfo->ers[gr_out][chn] = gfc->ers_save[chn];
	    gfc->ers_save[chn] = x;
	}
#endif
	/* compare energies between sub-shortblocks */
	attackThreshold = (chn == 3)
	    ? gfc->nsPsy.attackthre_s : gfc->nsPsy.attackthre;
	for (i=0;i<12;i++) 
	    if (!ns_attacks[i/3] && attack_intensity[i] > attackThreshold)
		ns_attacks[i/3] = (i % 3)+1;

	if (ns_attacks[0] && gfc->nsPsy.last_attacks[chn])
	    ns_attacks[0] = 0;

	if (gfc->nsPsy.last_attacks[chn] == 3 ||
	    ns_attacks[0] + ns_attacks[1] + ns_attacks[2] + ns_attacks[3]) {
	    ns_uselongblock = 0;

	    if (ns_attacks[1] && ns_attacks[0]) ns_attacks[1] = 0;
	    if (ns_attacks[2] && ns_attacks[1]) ns_attacks[2] = 0;
	    if (ns_attacks[3] && ns_attacks[2]) ns_attacks[3] = 0;
	}
	uselongblock[chn] = ns_uselongblock;

	/* there is a one granule delay.  Copy maskings computed last call
	 * into masking_ratio to return to calling program.
	 */
	energy[chn]=gfc->tot_ener[chn];

	/*********************************************************************
	 *  compute FFTs
	 *********************************************************************/
	wsamp_s = wsamp_S+(chn & 1);
	wsamp_l = wsamp_L+(chn & 1);
	compute_ffts(gfp, fftenergy, fftenergy_s,
		     wsamp_l, wsamp_s, gr_out, chn, buffer);

	/* compute masking thresholds for short blocks */
	for (sblock = 0; sblock < 3; sblock++) {
	    FLOAT8 enn, thmm;
	    compute_masking_s(gfc, fftenergy_s, eb, thr, chn, sblock);
	    b = -1;
	    for (sb = 0; sb < SBMAX_s; sb++) {
		enn = thmm = 0.0;
		while (++b < gfc->bo_s[sb]) {
		    enn  += eb[b];
		    thmm += thr[b];
		}
		enn  += 0.5 * eb[b];
		thmm += 0.5 * thr[b];
		gfc->en [chn].s[sb][sblock] = enn;

		/****   short block pre-echo control   ****/
#define NS_PREECHO_ATT0 0.8
#define NS_PREECHO_ATT1 0.6
#define NS_PREECHO_ATT2 0.3

		thmm *= NS_PREECHO_ATT0;
		if (ns_attacks[sblock] >= 2 || ns_attacks[sblock+1] == 1) {
		    int idx = (sblock != 0) ? sblock-1 : 2;
		    double p = NS_INTERP(gfc->thm[chn].s[sb][idx],
					 thmm, NS_PREECHO_ATT1*pcfact);
		    thmm = Min(thmm,p);
		}

		if (ns_attacks[sblock] == 1) {
		    int idx = (sblock != 0) ? sblock-1 : 2;
		    double p = NS_INTERP(gfc->thm[chn].s[sb][idx],
					 thmm,NS_PREECHO_ATT2*pcfact);
		    thmm = Min(thmm,p);
		} else if ((sblock != 0 && ns_attacks[sblock-1] == 3)
			|| (sblock == 0 && gfc->nsPsy.last_attacks[chn] == 3)) {
		    int idx = (sblock != 2) ? sblock+1 : 0;
		    double p = NS_INTERP(gfc->thm[chn].s[sb][idx],
					 thmm,NS_PREECHO_ATT2*pcfact);
		    thmm = Min(thmm,p);
		}

		/* pulse like signal detection for fatboy.wav and so on */
		enn = en_subshort[sblock*3+3] + en_subshort[sblock*3+4]
		    + en_subshort[sblock*3+5];
		if (en_subshort[sblock*3+5]*6 < enn) {
		    thmm *= 0.5;
		    if (en_subshort[sblock*3+4]*6 < enn)
			thmm *= 0.5;
		}

		gfc->thm[chn].s[sb][sblock] = thmm;
	    }
	}
	gfc->nsPsy.last_attacks[chn] = ns_attacks[2];

	/*********************************************************************
	 *    Calculate the energy and the tonality of each partition.
	 *********************************************************************/
	for (b = j = 0; b<gfc->npart_l; b++) {
	    FLOAT ebb,m;
	    m = ebb = fftenergy[j++];
	    for (i = gfc->numlines_l[b] - 1; i > 0; --i) {
		FLOAT8 el = fftenergy[j++];
		ebb += el;
		if (m < el)
		    m = el;
	    }
	    eb[b] = ebb;
	    max[b] = m;
	    avg[b] = ebb * gfc->rnumlines_l[b];
	}

	if (gfc->nsPsy.pass1fp)
	    nsPsy2dataRead(gfc->nsPsy.pass1fp, eb2, eb, chn, gfc->npart_l);
	else {
	    FLOAT8 m,a;
	    a = avg[0] + avg[1];
	    if (a != 0.0) {
		m = max[0]; if (m < max[1]) m = max[1];
		a = 20.0 * (m*2.0-a)
		    / (a*(gfc->numlines_l[0] + gfc->numlines_l[1] - 1));
		k = (int) a;
		if (k > sizeof(tab)/sizeof(tab[0]) - 1)
		    k = sizeof(tab)/sizeof(tab[0]) - 1;
		a = eb[0] * tab[k];
	    }
	    eb2[0] = a;

	    for (b = 1; b < gfc->npart_l-1; b++) {
		a = avg[b-1] + avg[b] + avg[b+1];
		if (a != 0.0) {
		    m = max[b-1];
		    if (m < max[b  ]) m = max[b];
		    if (m < max[b+1]) m = max[b+1];
		    a = 20.0 * (m*3.0-a)
			/ (a*(gfc->numlines_l[b-1] + gfc->numlines_l[b] + gfc->numlines_l[b+1] - 1));
		    k = (int) a;
		    if (k > sizeof(tab)/sizeof(tab[0]) - 1)
			k = sizeof(tab)/sizeof(tab[0]) - 1;
		    a = eb[b] * tab[k];
		}
		eb2[b] = a;
	    }

	    a = avg[gfc->npart_l-2] + avg[gfc->npart_l-1];
	    if (a != 0.0) {
		m = max[gfc->npart_l-2];
		if (m < max[gfc->npart_l-1])
		    m = max[gfc->npart_l-1];

		a = 20.0 * (m*2.0-a)
		    / (a*(gfc->numlines_l[gfc->npart_l-2] + gfc->numlines_l[gfc->npart_l-1] - 1));
		k = (int) a;
		if (k > sizeof(tab)/sizeof(tab[0]) - 1)
		    k = sizeof(tab)/sizeof(tab[0]) - 1;
		a = eb[b] * tab[k];
	    }
	    eb2[b] = a;
	}

	/*********************************************************************
	 *      convolve the partitioned energy and unpredictability
	 *      with the spreading function, s3_l[b][k]
	 ******************************************************************* */
	k = 0;
	for ( b = 0;b < gfc->npart_l; b++ ) {
	    FLOAT8 ecb;
	    /* convolve the partitioned energy with the spreading function */
	    int kk = gfc->s3ind[b][0];
	    ecb = gfc->s3_ll[k++] * eb2[kk];
	    while (++kk <= gfc->s3ind[b][1])
		ecb = mask_add(ecb, gfc->s3_ll[k++] * eb2[kk], kk, kk-b, gfc);

	    ecb *= 0.158489319246111; // pow(10,-0.8)

	    /****   long block pre-echo control   ****/
	    /* dont use long block pre-echo control if previous granule was 
	     * a short block.  This is to avoid the situation:   
	     * frame0:  quiet (very low masking)  
	     * frame1:  surge  (triggers short blocks)
	     * frame2:  regular frame.  looks like pre-echo when compared to 
	     *          frame0, but all pre-echo was in frame1.
	     */
	    /* chn=0,1   L and R channels
	       chn=2,3   S and M channels.
	    */

	    if (gfc->blocktype_old[chn & 1] == SHORT_TYPE)
		thr[b] = ecb; /* Min(ecb, rpelev*gfc->nb_1[chn][b]); */
	    else
		thr[b] = NS_INTERP(Min(ecb,
				       Min(rpelev*gfc->nb_1[chn][b],
					   rpelev2*gfc->nb_2[chn][b])),
				   ecb, pcfact);

	    gfc->nb_2[chn][b] = gfc->nb_1[chn][b];
	    gfc->nb_1[chn][b] = ecb;
	}

	/* compute masking thresholds for long blocks */
	convert_partition2scalefac_l(gfc, eb, thr, chn);

    } /* end loop over chn */

    if (gfp->interChRatio != 0.0)
	calc_interchannel_masking(gfp, gfp->interChRatio);

    if (gfp->mode == JOINT_STEREO) {
	FLOAT8 msfix;
	msfix1(gfc);
	msfix = gfp->msfix;
	if (gfc->ATH.adjust >= gfc->presetTune.athadjust_switch_level)
	    msfix = gfc->nsPsy.athadjust_msfix;

	if (msfix != 0.0)
	    ns_msfix(gfc, msfix, gfp->ATHlower);
    }

    /*************************************************************** 
     * determine final block type
     ***************************************************************/
    block_type_set(gfp, uselongblock, blocktype_d, blocktype);

    /*********************************************************************
     * compute the value of PE to return ... no delay and advance
     *********************************************************************/
    for(chn=0;chn<numchn;chn++) {
	FLOAT8 *ppe;
	int type;
	III_psy_ratio *mr;

	if (chn > 1) {
	    ppe = percep_MS_entropy - 2;
	    type = NORM_TYPE;
	    if (blocktype_d[0] == SHORT_TYPE || blocktype_d[1] == SHORT_TYPE)
		type = SHORT_TYPE;
	    mr = &masking_MS_ratio[gr_out][chn-2];
	} else {
	    ppe = percep_entropy;
	    type = blocktype_d[chn];
	    mr = &masking_ratio[gr_out][chn];
	}

	if (type == SHORT_TYPE)
	    ppe[chn] = pecalc_s(mr, gfc->masking_lower);
	else
	    ppe[chn] = pecalc_l(mr, gfc->masking_lower);

#if defined(HAVE_GTK)
	if (gfp->analysis) gfc->pinfo->pe[gr_out][chn] = ppe[chn];
#endif
    }
    return 0;
}
