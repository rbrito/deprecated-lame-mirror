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

barks:  a non-linear frequency scale.

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
#include "tables.h"
#include "machine.h"

/*
   L3psycho_anal.  Compute psycho acoustics.

   Data returned to the calling program must be delayed by one frame.

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

    fi = fn = fz + n*2;
    do {
	FLOAT a,g0,f0,f1,g1,f2,g2,f3,g3;
	fi -= 16;
	f1      = fi[0] - fi[ 4];
	f0      = fi[0] + fi[ 4];
	f3      = fi[8] - fi[12];
	f2      = fi[8] + fi[12];
	fi[ 8]  = f0     - f2;
	fi[ 0]  = f0     + f2;
	fi[12]  = f1     - f3;
	fi[ 4]  = f1     + f3;

	f1      = fi[2+0] - fi[2+4];
	f0      = fi[2+0] + fi[2+4];
	f3      = SQRT2  * fi[2+12];
	f2      = SQRT2  * fi[2+ 8];
	fi[2+ 8]  = f0     - f2;
	fi[2+ 0]  = f0     + f2;
	fi[2+12]  = f1     - f3;
	fi[2+ 4]  = f1     + f3;

	a       = (SQRT2*0.5)*(fi[1+4] + fi[3+4]);
	f1      = fi[1+0 ]    - a;
	f0      = fi[1+0 ]    + a;
	a       = (SQRT2*0.5)*(fi[1+4] - fi[3+4]);
	g1      = fi[3+0 ]    - a;
	g0      = fi[3+0 ]    + a;
	a       = (SQRT2*0.5)*(fi[1+12] + fi[3+12]);
	f3      = fi[1+8]    - a;
	f2      = fi[1+8]    + a;
	a       = (SQRT2*0.5)*(fi[1+12] - fi[3+12]);
	g2      = fi[3+8]    + a;
	g3      = fi[3+8]    - a;
	a       = tri[0]*f2     + tri[1]*g3;
	f2      = tri[1]*f2     - tri[0]*g3;
	fi[1+ 8]  = f0        - a;
	fi[1+ 0]  = f0        + a;
	fi[3+12]  = g1        - f2;
	fi[3+ 4]  = g1        + f2;
	a       = tri[1]*g2     + tri[0]*f3;
	g2      = tri[0]*g2     - tri[1]*f3;
	fi[3+ 8]  = g0        - a;
	fi[3+ 0]  = g0        + a;
	fi[1+12]  = f1        - g2;
	fi[1+ 4]  = f1        + g2;
    } while (fi!=fz);

    tri += 2;
    k4 = 16;
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
	    c2 = 1.0 - (2*s1)*s1;
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

static const int rv_tbl[] = {
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

#define ml00(i)	(window[i] * buffer[i])
#define ms00(i)	(window_s[i] * buffer[i])

static void
fft_short(lame_internal_flags * const gfc,
	  FLOAT x[BLKSIZE_s], const sample_t *buffer)
{
    int i;
    int j = (BLKSIZE_s / 8 - 1)*4;
    x += BLKSIZE_s / 2;

    do {
	FLOAT f0,f1,f2,f3, w;

	i = rv_tbl[j];

	f0 = ms00(i     ); w = ms00(i + 128); f1 = f0 - w; f0 = f0 + w;
	f2 = ms00(i + 64); w = ms00(i + 192); f3 = f2 - w; f2 = f2 + w;

	x -= 4;
	x[0] = f0 + f2;
	x[2] = f0 - f2;
	x[1] = f1 + f3;
	x[3] = f1 - f3;

	f0 = ms00(i +  1); w = ms00(i + 129); f1 = f0 - w; f0 = f0 + w;
	f2 = ms00(i + 65); w = ms00(i + 193); f3 = f2 - w; f2 = f2 + w;

	x[BLKSIZE_s / 2 + 0] = f0 + f2;
	x[BLKSIZE_s / 2 + 2] = f0 - f2;
	x[BLKSIZE_s / 2 + 1] = f1 + f3;
	x[BLKSIZE_s / 2 + 3] = f1 - f3;
    } while ((j -= BLKSIZE/BLKSIZE_s) >= 0);

#if HAVE_NASM
    gfc->fft_fht(x, BLKSIZE_s/2);
#else
    fht(x, BLKSIZE_s/2);
#endif
}

static void
fft_long(lame_internal_flags * const gfc,
	 FLOAT x[BLKSIZE], const sample_t *buffer )
{
    int           i;
    int           j = BLKSIZE / 8 - 1;
    x += BLKSIZE / 2;

    do {
	FLOAT f0,f1,f2,f3, w;

	i = rv_tbl[j];
	f0 = ml00(i      ); w = ml00(i + 512); f1 = f0 - w; f0 = f0 + w;
	f2 = ml00(i + 256); w = ml00(i + 768); f3 = f2 - w; f2 = f2 + w;

	x -= 4;
	x[0] = f0 + f2;
	x[2] = f0 - f2;
	x[1] = f1 + f3;
	x[3] = f1 - f3;

	f0 = ml00(i +   1); w = ml00(i + 513); f1 = f0 - w; f0 = f0 + w;
	f2 = ml00(i + 257); w = ml00(i + 769); f3 = f2 - w; f2 = f2 + w;

	x[BLKSIZE / 2 + 0] = f0 + f2;
	x[BLKSIZE / 2 + 2] = f0 - f2;
	x[BLKSIZE / 2 + 1] = f1 + f3;
	x[BLKSIZE / 2 + 3] = f1 - f3;
    } while (--j >= 0);
#if HAVE_NASM
    gfc->fft_fht(x, BLKSIZE/2); /* BLKSIZE/2 because of 3DNow! ASM routine */
#else
    fht(x, BLKSIZE/2);
#endif
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
    FLOAT loudness_power = 0.0;

    /* apply weights to power in freq. bands*/
    for( i = 0; i < BLKSIZE/2; ++i )
	loudness_power += energy[i] * gfc->ATH.eql_w[i];

    return loudness_power;
}

/*************************************************************** 
 * compute interchannel masking effects
 ***************************************************************/
static void
calc_interchannel_masking(
    lame_global_flags * gfp,
    int gr
    )
{
    lame_internal_flags *gfc=gfp->internal_flags;
    FLOAT ratio = gfp->interChRatio;
    III_psy_ratio *mr = &gfc->masking_next[gr][0];

    int sb, sblock;
    FLOAT l, r;
    for ( sb = 0; sb < SBMAX_l; sb++ ) {
	l = mr[0].thm.l[sb];
	r = mr[1].thm.l[sb];
	mr[0].thm.l[sb] += r*ratio;
	mr[1].thm.l[sb] += l*ratio;
    }
    for (sb = 0; sb < SBMAX_s; sb++) {
	for (sblock = 0; sblock < 3; sblock++) {
	    l = mr[0].thm.s[sb][sblock];
	    r = mr[1].thm.s[sb][sblock];
	    mr[0].thm.s[sb][sblock] += r*ratio;
	    mr[1].thm.s[sb][sblock] += l*ratio;
	}
    }
}



/*************************************************************** 
 * compute M/S thresholds from Johnston & Ferreira 1992 ICASSP paper
 * when the masking threshold of L/R channels are almost same
 * (the sound is positioned almost center), more masking is produced
 * because you cannot detect the sound position properly.
 ***************************************************************/
/*************************************************************** 
 * Adjust M/S maskings if user set "msfix"
 * Naoki Shibata 2000
 * 
 * The noise in L-ch (Lerr) with MS coding mode will be
 *      Lerr = 1/sqrt(2) (Merr + Serr)
 *           = 1/sqrt(2) (thM + thS) ... (1)   (worst case)
 *        or = 1/2       (thM + thS) ... (2)   (typical case)
 * Because Merr and Serr will have no correlation(white noise)
 * where Merr and Serr are the noise in M=1/sqrt(2)(L+R) and S=1/sqrt(2)(L-R).
 *
 * and, with LR coding mode,
 *      Lerr = thL                   ... (3)
 * Therefore,
 *      alpha (thM+thS) < thL
 * where alpha is a constant, between 1/sqrt(2) and 1/2
 * And with same way,
 *      alpha (thM+thS) < thR
 * Therefore,
 *      alpha (thM+thS) < min(thR, thL)
 ***************************************************************/

static void
msfix_l(
    lame_internal_flags *gfc,
    int gr
    )
{
    int sb;
    III_psy_ratio *mr = &gfc->masking_next[gr][0];
    for (sb = 0; sb < SBMAX_l; sb++) {
	/* use this fix if L & R masking differs by 2db (*1.58) or less */
	FLOAT rside,rmid,mld;
	if (mr[0].thm.l[sb] > 1.58*mr[1].thm.l[sb]
	 || mr[1].thm.l[sb] > 1.58*mr[0].thm.l[sb])
	{
	    FLOAT thmLR, thmM, thmS, x;
	    thmLR = Min(mr[0].thm.l[sb], mr[1].thm.l[sb]);
	    thmM = mr[2].thm.l[sb];
	    thmS = mr[3].thm.l[sb];
	    x = Min(thmM, mr[2].en.l[sb]) + Min(thmS, mr[3].en.l[sb]);

	    if (thmLR*gfc->nsPsy.msfix < x) {
		x = thmLR / x * gfc->nsPsy.msfix;
		thmM *= x;
		thmS *= x;
		mr[2].thm.l[sb] = thmM;
		mr[3].thm.l[sb] = thmS;
	    }
	    continue;
	}

	mld = gfc->mld_l[sb] * mr[3].en.l[sb];
	rmid = Max(mr[2].thm.l[sb], Min(mr[3].thm.l[sb], mld));

	mld = gfc->mld_l[sb] * mr[2].en.l[sb];
	rside = Max(mr[3].thm.l[sb], Min(mr[2].thm.l[sb], mld));

	mr[2].thm.l[sb] = rmid;
	mr[3].thm.l[sb] = rside;
    }
}

static void
msfix_s(
    lame_internal_flags *gfc,
    int gr
    )
{
    int sb, sblock;
    III_psy_ratio *mr = &gfc->masking_next[gr][0];
    for (sb = 0; sb < SBMAX_s; sb++) {
	for (sblock = 0; sblock < 3; sblock++) {
	    FLOAT rside,rmid,mld;
	    if (mr[0].thm.s[sb][sblock] > 1.58 * mr[1].thm.s[sb][sblock]
	     || mr[1].thm.s[sb][sblock] > 1.58 * mr[0].thm.s[sb][sblock])
	    {
		FLOAT thmLR, thmM, thmS, x;
		thmLR = Min(mr[0].thm.s[sb][sblock], mr[1].thm.s[sb][sblock]);
		thmM = mr[2].thm.s[sb][sblock];
		thmS = mr[3].thm.s[sb][sblock];
		x = Min(thmM, mr[2].en.s[sb][sblock])
		    + Min(thmS, mr[3].en.s[sb][sblock]);

		if (thmLR*gfc->nsPsy.msfix < x) {
		    x = thmLR / x * gfc->nsPsy.msfix;
		    mr[2].thm.s[sb][sblock] = thmM*x;
		    mr[3].thm.s[sb][sblock] = thmS*x;
		}
		continue;
	    }

	    mld = gfc->mld_s[sb] * mr[3].en.s[sb][sblock];
	    rmid = Max(mr[2].thm.s[sb][sblock],
		       Min(mr[3].thm.s[sb][sblock], mld));

	    mld = gfc->mld_s[sb] * mr[2].en.s[sb][sblock];
	    rside = Max(mr[3].thm.s[sb][sblock],
			Min(mr[2].thm.s[sb][sblock],mld));

	    mr[2].thm.s[sb][sblock] = rmid;
	    mr[3].thm.s[sb][sblock] = rside;
	}
    }
}

static void
compute_masking_s(
    lame_internal_flags *gfc,
    FLOAT fft_s[BLKSIZE_s],
    III_psy_ratio *mr,
    int sblock
    )
{
    int j, b, sb;
    FLOAT eb[CBANDS];
    FLOAT ecb, enn, thmm;
    
    ecb = fft_s[0] * fft_s[0] * 2.0f;
    j = 1;
    for (b = 0; b < gfc->npart_s; b++) {
	while (j < gfc->endlines_s[b]) {
	    FLOAT re = fft_s[j];
	    FLOAT im = fft_s[BLKSIZE_s-j];
	    ecb += re * re + im * im;
	    j++;
	}
	assert(j <= BLKSIZE_s);
	eb[b] = ecb;
	ecb = 0.0;
    }

    enn = thmm = 0.0;
    sb = j = b = 0;
    for (;;b++) {
	int kk = gfc->s3ind_s[b][0];
	FLOAT ecb = gfc->s3_ss[j++] * eb[kk++];
	while (kk <= gfc->s3ind_s[b][1])
	    ecb += gfc->s3_ss[j++] * eb[kk++];
	enn  += eb[b];
	thmm += ecb;
	if (b != gfc->bo_s[sb])
	    continue;

	enn  -= eb[b] * 0.5;
	thmm -= ecb * 0.5;
	thmm *= gfc->masking_lower;
	if (thmm < gfc->ATH.s_avg[sb] * gfc->ATH.adjust)
	    thmm = gfc->ATH.s_avg[sb] * gfc->ATH.adjust;
	mr->en .s[sb][sblock] = enn;
	mr->thm.s[sb][sblock] = thmm;
	enn  = eb[b] * 0.5;
	thmm = ecb * 0.5;
	sb++;
	if (sb == SBMAX_s)
	    break;
	if (b == gfc->bo_s[sb]) {
	    if (thmm < gfc->ATH.s_avg[sb] * gfc->ATH.adjust)
		thmm = gfc->ATH.s_avg[sb] * gfc->ATH.adjust;
	    mr->en .s[sb][sblock] = enn;
	    mr->thm.s[sb][sblock] = thmm;
	    break;
	}
    }
}

static int
block_type_set(
    lame_global_flags * gfp,
    int blocktype_old,
    int useshort_current,
    int useshort_next
    )
{
    /* update the blocktype of the previous granule, since it depends on what
     * happend in this granule */
    if (useshort_current)
	return SHORT_TYPE;

    assert(blocktype_old != START_TYPE);
    if (useshort_next) {
	if (blocktype_old == SHORT_TYPE)
	    return SHORT_TYPE;
	return START_TYPE;
    }
    if (blocktype_old == SHORT_TYPE)
	return STOP_TYPE;
    return NORM_TYPE;
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
#define I2LIMIT 23  /* as in if(i>24) -> changed 23 */ 
#define MLIMIT  15  /* as in if(m<15) */ 

static FLOAT ma_max_i1;
static FLOAT ma_max_i2;
static FLOAT ma_max_m;


#ifdef TAKEHIRO_IEEE754_HACK
static inline int trancate(FLOAT x)
{
    union {
	float f;
	int i;
    } fi;
    fi.f = x+(0.5+65536*128);
    return fi.i - 0x4b000001;
}
#else /* TAKEHIRO_IEEE754_HACK */
# define trancate(x) (int)x
#endif /* TAKEHIRO_IEEE754_HACK */

void
init_mask_add_max_values(
    lame_internal_flags * const gfc
    )
{
    int i;
    ma_max_i1 = db2pow((I1LIMIT+1)/16.0*10.0);
    ma_max_i2 = db2pow((I2LIMIT+1)/16.0*10.0);
    ma_max_m  = db2pow(MLIMIT);
    for (i = 0; i < gfc->npart_l; i++)
	gfc->ATH.cb[i] *= ma_max_m;

    ma_max_m = 1.0 / ma_max_m;
}






/* addition of simultaneous masking   Naoki Shibata 2000/7 */

static const FLOAT table1[] = {
    3.3246 *3.3246 ,3.23837*3.23837,3.15437*3.15437,3.00412*3.00412,2.86103*2.86103,2.65407*2.65407,2.46209*2.46209,2.284  *2.284  ,
    2.11879*2.11879,1.96552*1.96552,1.82335*1.82335,1.69146*1.69146,1.56911*1.56911,1.46658*1.46658,1.37074*1.37074,1.31036*1.31036,
    1.25264*1.25264,1.20648*1.20648,1.16203*1.16203,1.12765*1.12765,1.09428*1.09428,1.0659 *1.0659 ,1.03826*1.03826,1.01895*1.01895,
    1.0
};

static const FLOAT table2[] = {
    1.33352*1.33352,1.35879*1.35879,1.38454*1.38454,1.39497*1.39497,1.40548*1.40548,1.3537 *1.3537 ,1.30382*1.30382,1.22321*1.22321,
    1.14758*1.14758,
    1.0
};

static const FLOAT table3[] = {
    2.35364*2.35364,2.29259*2.29259,2.23313*2.23313,2.12675*2.12675,2.02545*2.02545,1.87894*1.87894,1.74303*1.74303,1.61695*1.61695,
    1.49999*1.49999,1.39148*1.39148,1.29083*1.29083,1.19746*1.19746,1.11084*1.11084,1.03826*1.03826
};

inline static FLOAT
mask_add_samebark(FLOAT m1, FLOAT m2)
{
    FLOAT m = m1 + m2;

    if (m2 > m1) {
	if (m2 >= m1*ma_max_i1)
	    return m; /* 43% of the total */
	m1 = m2/m1;
    } else {
	if (m1 >= m2*ma_max_i1)
	    return m; /* 43% of the total */
	m1 = m1/m2;
    }

    /* 22% of the total */
    return m * table2[trancate(FAST_LOG10_X(m1, 16.0))];
}

inline static FLOAT
mask_add(FLOAT m1, FLOAT m2, int k, int b, lame_internal_flags * const gfc)
{
    int i;
    FLOAT ratio;

    assert((unsigned int)(k-b+3) > 3+3);

    if (m2 > m1) {
	if (m2 >= m1*ma_max_i2)
	    return m1+m2;
	ratio = m2/m1;
    } else {
	if (m1 >= m2*ma_max_i2)
	    return m1+m2;
	ratio = m1/m2;
    }

    /* Should always be true, just checking */
    assert(m1>0.0);
    assert(m2>0.0);
    assert(gfc->ATH.cb[k]>0.0);


    m1 += m2;
    i = trancate(FAST_LOG10_X(ratio, 16.0));
    m2 = gfc->ATH.cb[k] * gfc->ATH.adjust;

    /* 10% of total */
    if (m1 >= m2)
	return m1*table1[i];

    /* 3% of the total */
    /* Originally if (m > 0) { */
    m2 *= ma_max_m;
    if (m1 > m2) {
	FLOAT f, r;

	f = 1.0;
	if (i <= 13) f = table3[i];
	r = FAST_LOG10_X(m1 / m2, 10.0/15.0);
	return m1 * ((table1[i]-f)*r+f);
    }

    /* very rare case */
    if (i > 13) return m1;
    return m1*table3[i];
}



/* pow(x, r) * pow(y, 1-r) */
#define NS_INTERP2(x, y, r)  pow((x)/(y),(r))*(y)



static inline FLOAT NS_INTERP(FLOAT x, FLOAT y, FLOAT r)
{
    if (r==1.0)
	return x;
    if (y==0.0)
	return y;
    return NS_INTERP2(x,y,r);
}



static void nsPsy2dataRead(
    FILE *fp,
    FLOAT *eb2,
    FLOAT *eb,
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
    lame_internal_flags *gfc,
    III_psy_ratio *mr
    )
{
    FLOAT pe_s;
    const static FLOAT regcoef_s[] = {
	11.8, /* this value is tuned only for 44.1kHz... */
	13.6,
	17.2,
	32,
	46.5,
	51.3,
	57.5,
	67.1,
	71.5,
	84.6,
	97.6,
	130,
	255.8
    };
    int sb, sblock;

    pe_s = 1236.28/4;
    sb = SBMAX_s - 1;
    if (!gfc->sfb21_extra)
	sb--;

    do {
	for (sblock=0;sblock<3;sblock++) {
	    FLOAT x = mr->thm.s[sb][sblock];
	    if (mr->en.s[sb][sblock] <= x)
		continue;

	    mr->ath_over++;
	    if (mr->en.s[sb][sblock] > x*1e10)
		pe_s += regcoef_s[sb] * (10.0 * LOG10);
	    else
		pe_s += regcoef_s[sb] * FAST_LOG10(mr->en.s[sb][sblock] / x);
	}
    } while (--sb >= 0);

    return pe_s;
}

static FLOAT
pecalc_l(
    lame_internal_flags *gfc,
    III_psy_ratio *mr
    )
{
    FLOAT pe_l;
    const static FLOAT regcoef_l[] = {
	6.8, /* this value is tuned only for 44.1kHz... */
	5.8,
	5.8,
	6.4,
	6.5,
	9.9,
	12.1,
	14.4,
	15,
	18.9,
	21.6,
	26.9,
	34.2,
	40.2,
	46.8,
	56.5,
	60.7,
	73.9,
	85.7,
	93.4,
	126.1,
	241.3
    };
    int sb = SBMAX_l - 1;

    if (!gfc->sfb21_extra)
	sb--;
    pe_l = 1124.23/4;
    do {
	FLOAT x = mr->thm.l[sb];
	if (mr->en.l[sb] <= x)
	    continue;

	mr->ath_over++;

	if (mr->en.l[sb] > x*1e10)
	    pe_l += regcoef_l[sb] * (10.0 * LOG10);
	else
	    pe_l += regcoef_l[sb] * FAST_LOG10(mr->en.l[sb] / x);
    } while (--sb >= 0);

    return pe_l;
}




static void
psycho_analysis_short(
    lame_global_flags * gfp,
    const sample_t *buffer[2],
    FLOAT sbsmpl[2][2*1152],
    int gr
    )
{
    lame_internal_flags *gfc=gfp->internal_flags;
    int current_is_short = 0;
    int first_attack_position[4];
    FLOAT attack_adjust[4];

    /* usual variables like loop indices, etc..    */
    int numchn, chn;
    int i, j, sblock;

    numchn = gfc->channels_out;
    if (gfp->mode == JOINT_STEREO) numchn=4;

    /*************************************************************** 
     * determine the block type (window type)
     ***************************************************************/
    for (chn=0; chn<numchn; chn++) {
	FLOAT attackThreshold;
/*
  use subband filtered samples to determine block type switching.

  1 character = 1 sample (in subband) = 32 samples (in original)

                                          =============stop=============
                  ===short====      ===short====
            ===short====      ===short====
      ===short====      ===short====
<----------------><----------------><----------------><---------------->
subbk_ene         000000111111222222333333444444555555
mp3x display               <------LONG------>
                           <=ST=><=ST=><=ST=>

================long================
                  ================long================


                                          =============stop=============
                                    ===short====
                              ===short====
                        ===short====
=============start============


                                    ================long================
                        =============stop=============
=============start============
*/
	if (chn < 2) {
	    for (i = 0; i < 3; i++) {
		FLOAT y = 1.0;
		for (j = 0; j < 6; j++) {
		    int k = i*6+j, band;
		    FLOAT x = 0.0;
// for (band = gfc->nsPsy.switching_highpass; ...
		    for (band = 10; band < SBLIMIT; band++)
			x += fabs(sbsmpl[chn][gr*576+k*32+mdctorder[band]]);
		    if (y < x) y = x;
		}
		gfc->nsPsy.subbk_ene[chn][i] = gfc->nsPsy.subbk_ene[chn][i+3];
		gfc->nsPsy.subbk_ene[chn][i+3] = y;
	    }
	} else if (chn == 2) {
	    for (i = 0; i < 3; i++) {
		FLOAT y0 = 1.0, y1 = 1.0;
		for (j = 0; j < 6; j++) {
		    int k = i*6+j, band;
		    FLOAT x0 = 0.0, x1 = 0.0;
// for (band = gfc->nsPsy.switching_highpass; ...
		    for (band = 10; band < SBLIMIT; band++) {
			FLOAT l = sbsmpl[0][gr*576+k*32+mdctorder[band]];
			FLOAT r = sbsmpl[1][gr*576+k*32+mdctorder[band]];
			x0 += fabs(l + r);
			x1 += fabs(l - r);
		    }
		    if (y0 < x0) y0 = x0;
		    if (y1 < x1) y1 = x1;
		}
		gfc->nsPsy.subbk_ene[2][i] = gfc->nsPsy.subbk_ene[2][i+3];
		gfc->nsPsy.subbk_ene[3][i] = gfc->nsPsy.subbk_ene[3][i+3];
		gfc->nsPsy.subbk_ene[2][i+3] = y0;
		gfc->nsPsy.subbk_ene[3][i+3] = y1;
	    }
	}

	/* calculate energies of each sub-shortblocks */
#if defined(HAVE_GTK)
	if (gfc->pinfo) {
	    memcpy(gfc->pinfo->ers[gr][chn], gfc->ers_save[gr][chn],
		   sizeof(gfc->ers_save[0][0]));
	    gfc->ers_save[gr][chn][0]
		= gfc->nsPsy.subbk_ene[chn][2] / gfc->nsPsy.subbk_ene[chn][1];
	    gfc->ers_save[gr][chn][1]
		= gfc->nsPsy.subbk_ene[chn][3] / gfc->nsPsy.subbk_ene[chn][2];
	    gfc->ers_save[gr][chn][2]
		= gfc->nsPsy.subbk_ene[chn][4] / gfc->nsPsy.subbk_ene[chn][3];
	}
#endif
	/* compare energies between sub-shortblocks */
	attackThreshold = (chn == 3)
	    ? gfc->nsPsy.attackthre_s : gfc->nsPsy.attackthre;

	/* initialize the flag representing
	 * "short block may be needed but not calculated"
	 */
	gfc->masking_next[gr][chn].en.s[0][0] = -1.0;
	gfc->useshort_next[gr][chn] = NORM_TYPE;
	for (i=0;i<3;i++) {
	    /* calculate energies of each sub-shortblocks */
	    if (gfc->nsPsy.subbk_ene[chn][i+2]
		> attackThreshold * gfc->nsPsy.subbk_ene[chn][i+1]) {
		gfc->useshort_next[gr][chn] = SHORT_TYPE;
		current_is_short += (1 << chn);
		first_attack_position[chn] = i;
		attack_adjust[chn]
		    = gfc->nsPsy.subbk_ene[chn][i+1]
		    / gfc->nsPsy.subbk_ene[chn][i+2];
		break;
	    }
	}
    }

    if (!current_is_short)
	return;

    for (chn=0; chn<numchn; chn++) {
	/* fft and energy calculation   */
	FLOAT wsamp_S[2][3][BLKSIZE_s];

	/* compute masking thresholds for short blocks */
	for (sblock = 0; sblock < 3; sblock++) {
	    if (chn < 2)
		fft_short( gfc, wsamp_S[chn][sblock],
			   &buffer[chn][(576/3) * (sblock+1)]);
	    else if (chn == 2 && (current_is_short & 12)) {
		for (j = 0; j < BLKSIZE_s; j++) {
		    FLOAT l = wsamp_S[0][sblock][j];
		    FLOAT r = wsamp_S[1][sblock][j];
		    wsamp_S[0][sblock][j] = (l+r)*(SQRT2*0.5);
		    wsamp_S[1][sblock][j] = (l-r)*(SQRT2*0.5);
		}
	    }

	    if (!gfc->useshort_next[gr][chn])
		continue;

	    compute_masking_s(gfc, wsamp_S[chn&1][sblock],
			      &gfc->masking_next[gr][chn], sblock);

	    if (sblock == first_attack_position[chn]) {
		int sb;
		for (sb = 0; sb < SBMAX_s; sb++) 
		    gfc->masking_next[gr][chn].thm.s[sb][sblock]
			*= attack_adjust[chn];
	    }
	}
    } /* end loop over chn */

    if (current_is_short & 12)
	gfc->useshort_next[gr][2] = gfc->useshort_next[gr][3] = SHORT_TYPE;

    return;
}

/* in some scalefactor band, 
   we can use masking threshold value of long block */
static void
partially_convert_l2s(
    lame_internal_flags *gfc,
    FLOAT *eb,
    int gr,
    int chn
    )
{
    III_psy_ratio *mr = &gfc->masking_next[gr][chn];
    int sfb, b;
    for (sfb = 0; sfb < SBMAX_s; sfb++) {
	FLOAT x = (mr->en.s[sfb][0] + mr->en.s[sfb][1] + mr->en.s[sfb][2])
	    * (1.0/4.0);
	if (x > mr->en.s[sfb][0]
	    || x > mr->en.s[sfb][1]
	    || x > mr->en.s[sfb][2])
	    continue;

//	printf("ch %d, sf %d\n", chn, sfb);
	if (sfb == 0) {
	    b = 0;
	    x = 0.0;
	} else {
	    b = gfc->bo_l2s[sfb-1];
	    x = gfc->nb_1[chn][b++] * 0.5;
	}
	for (; b < gfc->bo_l2s[sfb]; b++) {
	    x += gfc->nb_1[chn][b];
	}
	x += .5*gfc->nb_1[chn][b];

	x *= gfc->masking_lower * ((double)BLKSIZE_s / BLKSIZE);
//	printf("%e %e %e -> %e",
//	       mr->thm.s[sfb][0], mr->thm.s[sfb][1], mr->thm.s[sfb][2], x);
	if (x < gfc->ATH.s_avg[sfb] * gfc->ATH.adjust)
	    continue;
//	printf("(%e)\n", gfc->ATH.s_avg[sfb] * gfc->ATH.adjust);
//	printf("%e %e %e\n",
//	       mr->en.s[sfb][0], mr->en.s[sfb][1], mr->en.s[sfb][2]);
	if (mr->thm.s[sfb][0] > x)
	    mr->thm.s[sfb][0] = x;
	if (mr->thm.s[sfb][1] > x)
	    mr->thm.s[sfb][1] = x;
	if (mr->thm.s[sfb][2] > x)
	    mr->thm.s[sfb][2] = x;
    }
}

static void
L3psycho_anal_ns(
    lame_global_flags * gfp,
    const sample_t *buffer[2],
    int useshort_old[4],
    int gr
    )
{
    lame_internal_flags *gfc=gfp->internal_flags;

    /* fft and energy calculation   */
    FLOAT wsamp_L[2][BLKSIZE];

    /* usual variables like loop indices, etc..    */
    int numchn, chn;
    FLOAT pcfact;

    /*********************************************************************
     * compute the long block masking ratio
     *********************************************************************/
    numchn = gfc->channels_out;
    if (gfp->mode == JOINT_STEREO) numchn=4;

    switch (gfp->VBR) {
    case cbr:
	pcfact = gfc->ResvMax == 0
	    ? 0 : ((FLOAT)gfc->ResvSize)/gfc->ResvMax*0.5;
	break;
    case vbr: {
	static const FLOAT pcQns[10]={1.0,1.0,1.0,0.8,0.6,0.5,0.4,0.3,0.2,0.1};
	pcfact = pcQns[gfp->VBR_q];
	break;
    }
    case abr:
    default:
	pcfact = 1.0;
    }

    for (chn=0; chn<numchn; chn++) {
	FLOAT fftenergy[HBLKSIZE];
	/* convolution   */
	FLOAT eb[CBANDS], max[CBANDS], avg[CBANDS];
	FLOAT enn, thmm;
	III_psy_ratio *mr = &gfc->masking_next[gr][chn];
#define eb2 fftenergy
	static const FLOAT tab[] = {
	    1.0    /0.11749, 0.79433/0.11749, 0.63096/0.11749, 0.63096/0.11749,
	    0.63096/0.11749, 0.63096/0.11749, 0.63096/0.11749, 0.25119/0.11749
	};
	int b, i, j;
	FLOAT *spread;

	if (chn < 2)
	    fft_long ( gfc, wsamp_L[chn], buffer[chn]);
	else if (chn == 2) {
	    /* FFT data for mid and side channel is derived from L & R */
	    for (j = 0; j < BLKSIZE; j++) {
		FLOAT l = wsamp_L[0][j];
		FLOAT r = wsamp_L[1][j];
		wsamp_L[0][j] = (l+r)*(FLOAT)(SQRT2*0.5);
		wsamp_L[1][j] = (l-r)*(FLOAT)(SQRT2*0.5);
	    }
	}
	/*********************************************************************
	 *    Calculate the energy and the tonality of each partition.
	 *********************************************************************/
	{
	    FLOAT *p = wsamp_L[chn & 1], ebb, m;
	    ebb = p[0] * p[0];
	    eb[0] = ebb;

	    ebb += ebb;
	    max[0] = fftenergy[0] = ebb;
	    avg[0] = ebb * gfc->rnumlines_l[0];

	    for (b = j = 1; b<gfc->npart_l; b++) {
		fftenergy[j] = m = ebb = p[j]*p[j] + p[BLKSIZE-j]*p[BLKSIZE-j];
		j++;
		for (i = gfc->numlines_l[b] - 1; i > 0; --i) {
		    FLOAT el;
		    fftenergy[j] = el = p[j]*p[j] + p[BLKSIZE-j]*p[BLKSIZE-j];
		    j++;
		    ebb += el;
		    if (m < el)
			m = el;
		}
		eb[b] = ebb * 0.5f;
		max[b] = m;
		avg[b] = ebb * gfc->rnumlines_l[b];
	    }
	}
	/*********************************************************************
	 * compute loudness approximation (used for ATH auto-level adjustment) 
	 *********************************************************************/
	if (gfp->athaa_loudapprox == 2 && chn < 2) {/*no loudness for mid/side ch*/
	    gfc->loudness_next[gr][chn]
		= psycho_loudness_approx(fftenergy, gfc) * .5f;
	}
	/* total energy */
	{
	    FLOAT totalenergy=0.0;
	    for (j = 11; j < gfc->npart_l; j++)
		totalenergy += eb[j];

	    /* there is a one granule delay.  Copy maskings computed last call
	     * into masking_ratio to return to calling program.
	     */
	    gfc->tot_ener_next[gr][chn] = totalenergy;
	}
#ifdef HAVE_GTK
	if (gfp->analysis)
	    memcpy(gfc->energy_save[gr][chn], fftenergy, sizeof(fftenergy));
#endif
	if (gfc->nsPsy.pass1fp)
	    nsPsy2dataRead(gfc->nsPsy.pass1fp, eb2, eb, chn, gfc->npart_l);
	else {
	    FLOAT m,a;
	    a = avg[0] + avg[1];
	    if (a != 0.0) {
		m = max[0]; if (m < max[1]) m = max[1];
		a *= 3.0/2.0;
		m = (m-a) / a * gfc->rnumlines_ls[0];
		a = eb[0];
		if (m < sizeof(tab)/sizeof(tab[0]))
		    a *= tab[trancate(m)];
	    }
	    eb2[0] = a;

	    for (b = 1; b < gfc->npart_l-1; b++) {
		a = avg[b-1] + avg[b] + avg[b+1];
		if (a != 0.0) {
		    m = max[b-1];
		    if (m < max[b  ]) m = max[b];
		    if (m < max[b+1]) m = max[b+1];
		    m = (m-a) / a * gfc->rnumlines_ls[b];
		    a = eb[b];
		    if (m < sizeof(tab)/sizeof(tab[0]))
			a *= tab[trancate(m)];
		}
		eb2[b] = a;
	    }

	    a = avg[b-1] + avg[b];
	    if (a != 0.0) {
		m = max[b-1];
		if (m < max[b]) m = max[b];
		a *= 3.0/2.0;
		m = (m-a) / a * gfc->rnumlines_ls[b];
		a = eb[b];
		if (m < sizeof(tab)/sizeof(tab[0]))
		    a *= tab[trancate(m)];
	    }
	    eb2[b] = a;
	}

	/*********************************************************************
	 *      convolve the partitioned energy and unpredictability
	 *      with the spreading function, s3_l[b][k]
	 ******************************************************************* */
	b = j = 0;
	spread = gfc->s3_ll;
	enn = thmm = 0.0;
	for (;; b++ ) {
	    /* convolve the partitioned energy with the spreading function */
	    FLOAT ecb, tmp;
	    int kk = gfc->s3ind[b][0];
	    spread -= kk;
// calculate same bark masking 1st
	    ecb = spread[b] * eb2[b];

	    for (kk = 1; kk <= 3; kk++) {
		int k2;

		k2 = b + kk;
		if (k2 <= gfc->s3ind[b][1] && eb2[k2] != 0.0)
		    ecb = mask_add_samebark(ecb, spread[k2] * eb2[k2]);

		k2 = b - kk;
		if (k2 >= gfc->s3ind[b][0] && eb2[k2] != 0.0)
		    ecb = mask_add_samebark(ecb, spread[k2] * eb2[k2]);
	    }
	    for (kk = gfc->s3ind[b][0]; kk <= gfc->s3ind[b][1]; kk++) {
		if ((unsigned int)(kk - b + 3) <= 6 || eb2[kk] == 0.0)
		    continue;
		ecb = mask_add(ecb, spread[kk] * eb2[kk], kk, b, gfc);
	    }
	    spread += gfc->s3ind[b][1] + 1;

	    /****   long block pre-echo control   ****/
	    /* dont use long block pre-echo control if previous granule was 
	     * a short block.  This is to avoid the situation:   
	     * frame0:  quiet (very low masking)  
	     * frame1:  surge  (triggers short blocks)
	     * frame2:  regular frame.  looks like pre-echo when compared to 
	     *          frame0, but all pre-echo was in frame1.
	     */
	    if (useshort_old[chn] == SHORT_TYPE)
		tmp = ecb; /* or Min(ecb, rpelev*gfc->nb_1[chn][b]); ? */
	    else
		tmp = NS_INTERP(Min(ecb,
				    Min(rpelev  * gfc->nb_1[chn][b],
					rpelev2 * gfc->nb_2[chn][b])),
				ecb, pcfact);

	    gfc->nb_2[chn][b] = gfc->nb_1[chn][b];
	    gfc->nb_1[chn][b] = ecb;

	    enn  += eb [b];
	    thmm += tmp;
	    if (b != gfc->bo_l[j])
		continue;

	    if (j == SBMAX_l - 1)
		break;

	    enn  -= .5*eb[b];
	    thmm -= .5*tmp;
	    thmm *= gfc->masking_lower;
	    if (thmm < gfc->ATH.l_avg[j] * gfc->ATH.adjust)
		thmm = gfc->ATH.l_avg[j] * gfc->ATH.adjust;

	    mr->en .l[j] = enn;
	    mr->thm.l[j] = thmm;

	    enn  =  eb[b] * 0.5;
	    thmm = tmp * 0.5;
	    j++;
	    if (b == gfc->bo_l[j])
		break;
	}

	thmm *= gfc->masking_lower;
	if (thmm < gfc->ATH.l_avg[SBMAX_l-1] * gfc->ATH.adjust)
	    thmm = gfc->ATH.l_avg[SBMAX_l-1] * gfc->ATH.adjust;

	mr->en .l[SBMAX_l-1] = enn;
	mr->thm.l[SBMAX_l-1] = thmm;

	if (mr->en.s[0][0] >= 0.0) {
	    partially_convert_l2s(gfc, eb, gr, chn);
	    continue;
	}

	/* short block may be needed but not calculated */
	/* calculate it from converting from long */
	b = j = 0;
	enn = thmm = 0.0;
	for (;; b++ ) {
	    FLOAT tmp = gfc->nb_1[chn][b];
	    enn  += eb[b];
	    thmm += tmp;
	    if (b != gfc->bo_l2s[j])
		continue;

	    if (j == SBMAX_s - 1)
		break;

	    enn  -= .5*eb[b];
	    thmm -= .5*tmp;
	    thmm *= ((double)BLKSIZE_s / BLKSIZE) * gfc->masking_lower;
	    enn  *= ((double)BLKSIZE_s / BLKSIZE);
	    if (thmm < gfc->ATH.s_avg[j] * gfc->ATH.adjust)
		thmm = gfc->ATH.s_avg[j] * gfc->ATH.adjust;

	    mr->thm.s[j][0] = mr->thm.s[j][1] = mr->thm.s[j][2] = thmm;
	    mr->en .s[j][0] = mr->en .s[j][1] = mr->en .s[j][2] = enn;

	    enn  =  eb[b] * 0.5;
	    thmm = tmp * 0.5;
	    j++;
	    if (b == gfc->bo_l2s[j])
		break;
	}

	if (thmm < gfc->ATH.s_avg[j] * gfc->ATH.adjust)
	    thmm = gfc->ATH.s_avg[j] * gfc->ATH.adjust;

	mr->thm.s[j][0] = mr->thm.s[j][1] = mr->thm.s[j][2] = thmm;
	mr->en .s[j][0] = mr->en .s[j][1] = mr->en .s[j][2] = enn;
    }
}

/*
 * auto-adjust of ATH, useful for low volume
 * Gabriel Bouvigne 3 feb 2001
 *
 * modifies some values in
 *   gfp->internal_flags->ATH
 *   (gfc->ATH)
 */
static void
adjust_ATH(
    lame_global_flags* const  gfp
    )
{
    lame_internal_flags* const  gfc = gfp->internal_flags;
    int gr;
    FLOAT max_pow, max_pow_alt;
    FLOAT max_val;

    if (gfc->ATH.use_adjust == 0 || gfp->athaa_loudapprox == 0) {
        gfc->ATH.adjust = 1.0;	/* no adjustment */
        return;
    }
    
    switch( gfp->athaa_loudapprox ) {
    case 1:
                                /* flat approximation for loudness (squared) */
        max_pow = 0;
        for ( gr = 0; gr < gfc->mode_gr; ++gr ) {
	    max_pow = Max( max_pow, gfc->tot_ener_next[gr][0] );
	    if (gfc->channels_out == 2 && max_pow < gfc->tot_ener_next[gr][1])
                max_pow = gfc->tot_ener_next[gr][1];
	}
        max_pow *= 0.25/ 5.6e13; /* scale to 0..1 (5.6e13), and tune (0.25) */
        break;
    
    case 2:                     /* jd - 2001 mar 12, 27, jun 30 */
    {				/* loudness based on equal loudness curve; */
                                /* use granule with maximum combined loudness*/
	FLOAT gr2_max = gfc->loudness_next[1][0];
        max_pow = gfc->loudness_next[0][0];
        if( gfc->channels_out == 2 ) {
            max_pow += gfc->loudness_next[0][1];
	    gr2_max += gfc->loudness_next[1][1];
	} else {
	    max_pow += max_pow;
	    gr2_max += gr2_max;
	}
	if( gfc->mode_gr == 2 ) {
	    max_pow = Max( max_pow, gr2_max );
	}
	max_pow *= 0.5;		/* max_pow approaches 1.0 for full band noise*/
        break;
    }

    default:
        assert(0);
    }

                                /* jd - 2001 mar 31, jun 30 */
                                /* user tuning of ATH adjustment region */
    max_pow_alt = max_pow;
    max_pow *= gfc->ATH.aa_sensitivity_p;

    /*  adjust ATH depending on range of maximum value
     */
    switch ( gfc->ATH.use_adjust ) {

    case  1:
        max_val = sqrt( max_pow ); /* GB's original code requires a maximum */
        max_val *= 32768;          /*  sample or loudness value up to 32768 */

                                /* by Gabriel Bouvigne */
        if      (0.5 < max_val / 32768) {       /* value above 50 % */
                gfc->ATH.adjust = 1.0;         /* do not reduce ATH */
        }
        else if (0.3 < max_val / 32768) {       /* value above 30 % */
                gfc->ATH.adjust *= 0.955;      /* reduce by ~0.2 dB */
                if (gfc->ATH.adjust < 0.3)     /* but ~5 dB in maximum */
                    gfc->ATH.adjust = 0.3;            
        }
        else {                                  /* value below 30 % */
                gfc->ATH.adjust *= 0.93;       /* reduce by ~0.3 dB */
                if (gfc->ATH.adjust < 0.01)    /* but 20 dB in maximum */
                    gfc->ATH.adjust = 0.01;
        }
        break;

    case  2:   
        max_val = Min( max_pow, 1.0 ) * 32768; /* adapt for RH's adjust */

      {                         /* by Robert Hegemann */
        /*  this code reduces slowly the ATH (speed of 12 dB per second)
         */
	FLOAT x;
        //x = Max (640, 320*(int)(max_val/320));
        x = Max (32, 32*(int)(max_val/32));
        x = x/32768;
        gfc->ATH.adjust *= gfc->ATH.decay;
        if (gfc->ATH.adjust < x)       /* but not more than f(x) dB */
            gfc->ATH.adjust = x;
      }
        break;

    case  3:
      {                         /* jd - 2001 feb27, mar12,20, jun30, jul22 */
                                /* continuous curves based on approximation */
                                /* to GB's original values. */
        FLOAT adj_lim_new;
                                /* For an increase in approximate loudness, */
                                /* set ATH adjust to adjust_limit immediately*/
                                /* after a delay of one frame. */
                                /* For a loudness decrease, reduce ATH adjust*/
                                /* towards adjust_limit gradually. */
                                /* max_pow is a loudness squared or a power. */
        if( max_pow > 0.03125) { /* ((1 - 0.000625)/ 31.98) from curve below */
            if( gfc->ATH.adjust >= 1.0) {
                gfc->ATH.adjust = 1.0;
            } else {
                                /* preceding frame has lower ATH adjust; */
                                /* ascend only to the preceding adjust_limit */
                                /* in case there is leading low volume */
                if( gfc->ATH.adjust < gfc->ATH.adjust_limit) {
                    gfc->ATH.adjust = gfc->ATH.adjust_limit;
                }
            }
            gfc->ATH.adjust_limit = 1.0;
        } else {                /* adjustment curve */
                                /* about 32 dB maximum adjust (0.000625) */
            adj_lim_new = 31.98 * max_pow + 0.000625;
            if( gfc->ATH.adjust >= adj_lim_new) { /* descend gradually */
                gfc->ATH.adjust *= adj_lim_new * 0.075 + 0.925;
                if( gfc->ATH.adjust < adj_lim_new) { /* stop descent */
                    gfc->ATH.adjust = adj_lim_new;
                }
            } else {            /* ascend */
                if( gfc->ATH.adjust_limit >= adj_lim_new) {
                    gfc->ATH.adjust = adj_lim_new;
                } else {        /* preceding frame has lower ATH adjust; */
                                /* ascend only to the preceding adjust_limit */
                    if( gfc->ATH.adjust < gfc->ATH.adjust_limit) {
                        gfc->ATH.adjust = gfc->ATH.adjust_limit;
                    }
                }
            }
            gfc->ATH.adjust_limit = adj_lim_new;
        }
      }
        break;
        
    default:
        assert(0);
        break;
    }   /* switch */
}

/* psychoacoustic model
 * psymodel has a 1 frame (576*mode_gr) delay that we must compensate for
 */
void
psycho_analysis(
    lame_global_flags * gfp,
    const sample_t *buffer[2],
    FLOAT ms_ener_ratio_d[2],
    III_psy_ratio masking_d[2][2],
    FLOAT sbsmpl[2][2*1152]
    )
{
    int gr, ch;
    int blocktype_old[MAX_CHANNELS*2];
    lame_internal_flags *gfc=gfp->internal_flags;

    /* address of beginning of left & right granule */
    const sample_t *bufp[MAX_CHANNELS];

    for (ch = 0; ch < gfc->channels_out; ch++) {
	blocktype_old[ch] = blocktype_old[ch+2]
	    = gfc->l3_side.tt[gfc->mode_gr-1][ch].block_type;
    }
    /* next frame data -> current frame data (aging) */
    adjust_ATH(gfp);
    gfc->mode_ext = gfc->mode_ext_next;
    if (gfc->mode_ext & MPG_MD_MS_LR) {
	for (gr=0; gr < gfc->mode_gr ; gr++) {
	    FLOAT e0 = gfc->tot_ener_next[gr][2] + gfc->tot_ener_next[gr][3];
	    if (e0 > 0.0)
		e0 = gfc->tot_ener_next[gr][3] / e0;
	    ms_ener_ratio_d[gr] = e0;

	    for (ch = 0; ch < 2; ch++) {
		masking_d[gr][ch] = gfc->masking_next[gr][ch + 2];
		gfc->l3_side.tt[gr][ch].block_type = NORM_TYPE;
		if (gfc->useshort_next[gr][ch]) {
		    gfc->l3_side.tt[gr][ch].block_type = SHORT_TYPE;
		}
	    }
	}
    } else {
	for (gr=0; gr < gfc->mode_gr ; gr++) {
	    for (ch = 0; ch < gfc->channels_out; ch++) {
		masking_d[gr][ch] = gfc->masking_next[gr][ch];
		gfc->l3_side.tt[gr][ch].block_type = NORM_TYPE;
		if (gfc->useshort_next[gr][ch]) {
		    gfc->l3_side.tt[gr][ch].block_type = SHORT_TYPE;
		}
	    }
	}
    }

    /* calculate next frame data */
    for (gr=0; gr < gfc->mode_gr ; gr++) {
	int numchn;
	for (ch = 0; ch < gfc->channels_out; ch++)
	    bufp[ch] = &buffer[ch][576*(gr + gfc->mode_gr) - FFTOFFSET];

	psycho_analysis_short(gfp, bufp, sbsmpl, gr);
	if (gr == 0) {
	    L3psycho_anal_ns(gfp, bufp, blocktype_old, gr);
	} else {
	    L3psycho_anal_ns(gfp, bufp, gfc->useshort_next[0], gr);
	}

	numchn = gfc->channels_out;

	/*********************************************************************
	 * other masking effect
	 *********************************************************************/
	if (gfp->interChRatio != 0.0)
	    calc_interchannel_masking(gfp, gr);

	if (gfp->mode == JOINT_STEREO) {
	    if (!gfc->useshort_next[gr][ch]) 
		msfix_l(gfc, gr);

	    msfix_s(gfc, gr);
	    numchn = 4;
	}
	/*********************************************************************
	 * compute the value of PE to return
	 *********************************************************************/
	for (ch=0;ch<numchn;ch++) {
	    III_psy_ratio *mr = &gfc->masking_next[gr][ch];
	    mr->ath_over = 0;
	    if (gfc->useshort_next[gr][ch])
		mr->pe = pecalc_s(gfc, mr);
	    else
		mr->pe = pecalc_l(gfc, mr);
	}
    }

    /* determine MS/LR in the next frame */
    gfc->mode_ext_next = MPG_MD_LR_LR;
    if (gfp->mode == JOINT_STEREO) {
	FLOAT diff_pe = 0.0;
	for (gr = 0; gr < gfc->mode_gr; gr++)
	    diff_pe
		+= gfc->masking_next[gr][2].pe
		+  gfc->masking_next[gr][3].pe
		-  gfc->masking_next[gr][0].pe
		-  gfc->masking_next[gr][1].pe;

	/* based on PE: M/S coding would not use much more bits than L/R */
	if (diff_pe <= 0.0 || gfp->force_ms) {
	    gfc->mode_ext_next = MPG_MD_MS_LR;
	    for (gr = 0; gr < gfc->mode_gr; gr++) {
		gfc->useshort_next[gr][0] = gfc->useshort_next[gr][2];
		gfc->useshort_next[gr][1] = gfc->useshort_next[gr][3];
	    }
	    /* LR -> MS case */
	    if (gfc->mode_ext_next != gfc->mode_ext
	     && (gfc->l3_side.tt[gfc->mode_gr-1][0].block_type
		 | gfc->l3_side.tt[gfc->mode_gr-1][1].block_type))
		gfc->l3_side.tt[gfc->mode_gr-1][0].block_type
		    = gfc->l3_side.tt[gfc->mode_gr-1][1].block_type
		    = SHORT_TYPE;
	} else if (gfc->mode_ext_next != gfc->mode_ext
		   && (gfc->useshort_next[0][0] | gfc->useshort_next[0][1]))
	    gfc->useshort_next[0][0] = gfc->useshort_next[0][1] = SHORT_TYPE;
    }

    /* determine current frame block type (long/start/short/stop) */
    for (ch = 0; ch < gfc->channels_out; ch++) {
	if (gfc->mode_gr == 1)
	    gfc->l3_side.tt[0][ch].block_type
		= block_type_set(gfp,
				 blocktype_old[ch],
				 gfc->l3_side.tt[0][ch].block_type,
				 gfc->useshort_next[0][ch]);
	else {
	    gfc->l3_side.tt[0][ch].block_type
		= block_type_set(gfp,
				 blocktype_old[ch],
				 gfc->l3_side.tt[0][ch].block_type,
				 gfc->l3_side.tt[1][ch].block_type);

	    gfc->l3_side.tt[1][ch].block_type
		= block_type_set(gfp,
				 gfc->l3_side.tt[0][ch].block_type,
				 gfc->l3_side.tt[1][ch].block_type,
				 gfc->useshort_next[0][ch]);
	}
    }
    if (gfc->mode_ext & MPG_MD_MS_LR) {
	assert(
	    gfc->l3_side.tt[0][0].block_type
	    == gfc->l3_side.tt[0][1].block_type);
	assert(
	    gfc->l3_side.tt[gfc->mode_gr-1][0].block_type
	    == gfc->l3_side.tt[gfc->mode_gr-1][1].block_type);
    }
}
