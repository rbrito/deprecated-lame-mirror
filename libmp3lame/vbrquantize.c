/*
 *	MP3 quantization
 *
 *	Copyright (c) 1999 Mark Taylor
 *                2004 Robert Hegemann
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

extern FLOAT8 adj43[PRECALC_SIZE];

extern void
        trancate_smallspectrums(lame_internal_flags * gfc,
                                gr_info * const gi, const FLOAT8 * const l3_xmin, FLOAT8 * work);

/*  Remarks on optimizing compilers:
 *
 *  the MSVC compiler may get into aliasing problems when accessing
 *  memory through the fi_union. declaring it volatile does the trick here
 *
 *  the calc_sfb_noise_* functions are not inlined because the intel compiler
 *  optimized executeables won't work as expected anymore
 */

#ifdef _MSC_VER
#define VOLATILE volatile
#else
#define VOLATILE
#endif

typedef VOLATILE union {
    float   f;
    int     i;
} fi_union;


#define valid_sf(sf) (sf>=0?(sf<=255?sf:255):0)



/*  caution: a[] will be resorted!!
 */
static int
select_kth_int(int a[], int N, int k)
{
    int     i, j, l, r, v, w;

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



/*  caution: a[] will be resorted!!
 */
static  FLOAT8
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

inline  FLOAT8
max_x34(const FLOAT8 * xr34, unsigned int bw)
{
    FLOAT8  xfsf = 0;
    int     j = bw >> 1;
    int     remaining = j % 2;
    assert(bw >= 0);
    for (j >>= 1; j > 0; --j) {
        if (xfsf < xr34[0])
            xfsf = xr34[0];
        if (xfsf < xr34[1])
            xfsf = xr34[1];
        if (xfsf < xr34[2])
            xfsf = xr34[2];
        if (xfsf < xr34[3])
            xfsf = xr34[3];
        xr34 += 4;
    }
    if (remaining) {
        if (xfsf < xr34[0])
            xfsf = xr34[0];
        if (xfsf < xr34[1])
            xfsf = xr34[1];
    }
    return xfsf;
}

inline int
find_lowest_scalefac(const FLOAT8 xr34)
{
    FLOAT8  xfsf;
    int     sf = 128, sf_ok = 10000, delsf = 128, i;
    for (i = 0; i < 8; ++i) {
        delsf >>= 1;
        xfsf = IPOW20(sf) * xr34;
        if (xfsf <= IXMAX_VAL) {
            sf_ok = sf;
            sf -= delsf;
        }
        else {
            sf += delsf;
        }
    }
    if (sf_ok < 255) {
        sf = sf_ok;
    }
    return sf;
}


#define MAGIC_FLOAT (65536*(128))
#define MAGIC_INT    0x4b000000


#ifdef TAKEHIRO_IEEE754_HACK
#   define ROUNDFAC -0.0946
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


#define BLOCK_IEEE754_x34_4 \
	                x0 += MAGIC_FLOAT; fi[0].f = x0;    \
	                x1 += MAGIC_FLOAT; fi[1].f = x1;    \
	                x2 += MAGIC_FLOAT; fi[2].f = x2;    \
	                x3 += MAGIC_FLOAT; fi[3].f = x3;    \
	                fi[0].f = x0 + adj43asm[fi[0].i-MAGIC_INT];  \
	                fi[1].f = x1 + adj43asm[fi[1].i-MAGIC_INT];  \
    	            fi[2].f = x2 + adj43asm[fi[2].i-MAGIC_INT];  \
        	        fi[3].f = x3 + adj43asm[fi[3].i-MAGIC_INT];  \
	                l3[0] = fi[0].i-MAGIC_INT;               \
	                l3[1] = fi[1].i-MAGIC_INT;               \
	                l3[2] = fi[2].i-MAGIC_INT;               \
	                l3[3] = fi[3].i-MAGIC_INT;

#define BLOCK_IEEE754_x34_2 \
	                x0 += MAGIC_FLOAT; fi[0].f = x0;    \
	                x1 += MAGIC_FLOAT; fi[1].f = x1;    \
	                fi[0].f = x0 + adj43asm[fi[0].i-MAGIC_INT];  \
	                fi[1].f = x1 + adj43asm[fi[1].i-MAGIC_INT];  \
	                l3[0] = fi[0].i-MAGIC_INT;               \
	                l3[1] = fi[1].i-MAGIC_INT;


#define BLOCK_PLAIN_C_x34_4 \
    	            XRPOW_FTOI(x0, l3[0]);    \
	                XRPOW_FTOI(x1, l3[1]);    \
    	            XRPOW_FTOI(x2, l3[2]);    \
	                XRPOW_FTOI(x3, l3[3]);    \
	                x0 += QUANTFAC(l3[0]);    \
                    x1 += QUANTFAC(l3[1]);    \
                    x2 += QUANTFAC(l3[2]);    \
                    x3 += QUANTFAC(l3[3]);    \
                    XRPOW_FTOI(x0, l3[0]);    \
    	            XRPOW_FTOI(x1, l3[1]);    \
	                XRPOW_FTOI(x2, l3[2]);    \
    	            XRPOW_FTOI(x3, l3[3]);

#define BLOCK_PLAIN_C_x34_2 \
    	            XRPOW_FTOI(x0, l3[0]);    \
	                XRPOW_FTOI(x1, l3[1]);    \
	                x0 += QUANTFAC(l3[0]);    \
                    x1 += QUANTFAC(l3[1]);    \
 	                XRPOW_FTOI(x0, l3[0]);    \
    	            XRPOW_FTOI(x1, l3[1]);


#define BLOCK_IEEE754_ISO_4 \
	                x0 += (ROUNDFAC + MAGIC_FLOAT); fi[0].f = x0;   \
	                x1 += (ROUNDFAC + MAGIC_FLOAT); fi[1].f = x1;   \
	                x2 += (ROUNDFAC + MAGIC_FLOAT); fi[2].f = x2;   \
	                x3 += (ROUNDFAC + MAGIC_FLOAT); fi[3].f = x3;   \
	                l3[0] = fi[0].i-MAGIC_INT;                      \
	                l3[1] = fi[1].i-MAGIC_INT;                      \
	                l3[2] = fi[2].i-MAGIC_INT;                      \
	                l3[3] = fi[3].i-MAGIC_INT;

#define BLOCK_IEEE754_ISO_2 \
	                x0 += (ROUNDFAC + MAGIC_FLOAT); fi[0].f = x0;   \
	                x1 += (ROUNDFAC + MAGIC_FLOAT); fi[1].f = x1;   \
	                l3[0] = fi[0].i-MAGIC_INT;                      \
	                l3[1] = fi[1].i-MAGIC_INT;

#define BLOCK_PLAIN_C_ISO_4 \
	            if (compareval0 > xr34[0]) {                \
	                l3[0] = 0;                              \
	            } else if (compareval1 > xr34[0]) {         \
	                l3[0] = 1;                              \
	            } else {                                    \
	                l3[0] = x0 + ROUNDFAC;                  \
	            }                                           \
	            if (compareval0 > xr34[1]) {           \
	                l3[1] = 0;                            \
	            } else if (compareval1 > xr34[1]) {    \
	                l3[1] = 1;                            \
	            } else {                                    \
	                l3[1] = x1 + ROUNDFAC;                \
	            }                                           \
	            if (compareval0 > xr34[2]) {           \
	                l3[2] = 0;                            \
	            } else if (compareval1 > xr34[2]) {    \
	                l3[2] = 1;                            \
	            } else {                                    \
	                l3[2] = x2 + ROUNDFAC;                \
	            }                                           \
	            if (compareval0 > xr34[3]) {           \
	                l3[3] = 0;                            \
	            } else if (compareval1 > xr34[3]) {    \
	                l3[3] = 1;                            \
	            } else {                                    \
	                l3[3] = x3 + ROUNDFAC;                \
	            }                                           \


#define BLOCK_PLAIN_C_ISO_2 \
	            if (compareval0 > xr34[0]) {           \
	                l3[0] = 0;                            \
	            } else if (compareval1 > xr34[0]) {    \
	                l3[0] = 1;                            \
	            } else {                                    \
	                l3[0] = x0 + ROUNDFAC;                \
	            }                                           \
	            if (compareval0 > xr34[1]) {           \
	                l3[1] = 0;                            \
	            } else if (compareval1 > xr34[1]) {    \
	                l3[1] = 1;                            \
	            } else {                                    \
	                l3[1] = x1 + ROUNDFAC;                \
	            }                                           \


#define ERRDELTA_4 \
    x0 = fabs(xr[0]) - sfpow * pow43[l3[0]]; \
    x1 = fabs(xr[1]) - sfpow * pow43[l3[1]]; \
    x2 = fabs(xr[2]) - sfpow * pow43[l3[2]]; \
    x3 = fabs(xr[3]) - sfpow * pow43[l3[3]];
#define ERRDELTA_2 \
    x0 = fabs(xr[0]) - sfpow * pow43[l3[0]]; \
    x1 = fabs(xr[1]) - sfpow * pow43[l3[1]];



/*  do call the calc_sfb_noise_* functions only with sf values 
 *  for which holds: sfpow34*xr34 <= IXMAX_VAL
 */

static  FLOAT8
calc_sfb_noise_x34(const FLOAT8 * xr, const FLOAT8 * xr34, unsigned int bw, int sf)
{
    const int SF = valid_sf(sf);
    const FLOAT8 sfpow = POW20(SF); /*pow(2.0,sf/4.0); */
    const FLOAT8 sfpow34 = IPOW20(SF); /*pow(sfpow,-3.0/4.0); */

    FLOAT8  xfsf = 0;
    fi_union fi[4];
    int     l3[4];
    int     j = bw >> 1;
    int     remaining = j % 2;
    assert(bw >= 0);
    for (j >>= 1; j > 0; --j) {
        double  x0 = sfpow34 * xr34[0];
        double  x1 = sfpow34 * xr34[1];
        double  x2 = sfpow34 * xr34[2];
        double  x3 = sfpow34 * xr34[3];
        assert(x0 <= IXMAX_VAL && x1 <= IXMAX_VAL && x2 <= IXMAX_VAL && x3 <= IXMAX_VAL);
#ifdef TAKEHIRO_IEEE754_HACK
        BLOCK_IEEE754_x34_4
#else
        BLOCK_PLAIN_C_x34_4
#endif
            ERRDELTA_4 xfsf += x0 * x0 + x1 * x1 + x2 * x2 + x3 * x3;

        xr += 4;
        xr34 += 4;
    }
    if (remaining) {
        double  x0 = sfpow34 * xr34[0];
        double  x1 = sfpow34 * xr34[1];
        assert(x0 <= IXMAX_VAL && x1 <= IXMAX_VAL);
#ifdef TAKEHIRO_IEEE754_HACK
        BLOCK_IEEE754_x34_2
#else
        BLOCK_PLAIN_C_x34_2
#endif
            ERRDELTA_2 xfsf += x0 * x0 + x1 * x1;
    }
    return xfsf;
}


static  FLOAT8
calc_sfb_noise_ISO(const FLOAT8 * xr, const FLOAT8 * xr34, unsigned int bw, int sf)
{
    const int SF = valid_sf(sf);
    const FLOAT8 sfpow = POW20(SF); /*pow(2.0,sf/4.0); */
    const FLOAT8 sfpow34 = IPOW20(SF); /*pow(sfpow,-3.0/4.0); */

#ifndef TAKEHIRO_IEEE754_HACK
    const FLOAT8 compareval0 = (1.0 - 0.4054) / sfpow34;
    const FLOAT8 compareval1 = (2.0 - 0.4054) / sfpow34;
#endif

    FLOAT8  xfsf = 0;
    fi_union fi[4];
    int     l3[4];
    int     j = bw >> 1;
    int     remaining = j % 2;
    assert(bw >= 0);
    for (j >>= 1; j > 0; --j) {
        double  x0 = sfpow34 * xr34[0];
        double  x1 = sfpow34 * xr34[1];
        double  x2 = sfpow34 * xr34[2];
        double  x3 = sfpow34 * xr34[3];
        assert(x0 <= IXMAX_VAL && x1 <= IXMAX_VAL && x2 <= IXMAX_VAL && x3 <= IXMAX_VAL);
#ifdef TAKEHIRO_IEEE754_HACK
        BLOCK_IEEE754_ISO_4
#else
        BLOCK_PLAIN_C_ISO_4
#endif
            ERRDELTA_4 xfsf += x0 * x0 + x1 * x1 + x2 * x2 + x3 * x3;

        xr += 4;
        xr34 += 4;
    }
    if (remaining) {
        double  x0 = sfpow34 * xr34[0];
        double  x1 = sfpow34 * xr34[1];
        assert(x0 <= IXMAX_VAL && x1 <= IXMAX_VAL);
#ifdef TAKEHIRO_IEEE754_HACK
        BLOCK_IEEE754_ISO_2
#else
        BLOCK_PLAIN_C_ISO_2
#endif
            ERRDELTA_2 xfsf += x0 * x0 + x1 * x1;
    }
    return xfsf;
}



static  FLOAT8
calc_sfb_noise_mq_x34(const FLOAT8 * xr, const FLOAT8 * xr34, int bw, int sf, int mq)
{
    const int SF = valid_sf(sf);
    FLOAT8  scratch_[192];
    FLOAT8 *scratch = scratch_;
    const FLOAT8 sfpow = POW20(SF); /*pow(2.0,sf/4.0); */
    const FLOAT8 sfpow34 = IPOW20(SF); /*pow(sfpow,-3.0/4.0); */

    FLOAT8  xfsfm = 0, xfsf = 0;
    fi_union fi[4];
    int     l3[4];
    int     k, j = bw >> 1;
    int     remaining = j % 2;
    assert(bw >= 0);
    for (j >>= 1; j > 0; --j) {
        double  x0 = sfpow34 * xr34[0];
        double  x1 = sfpow34 * xr34[1];
        double  x2 = sfpow34 * xr34[2];
        double  x3 = sfpow34 * xr34[3];
        assert(x0 <= IXMAX_VAL && x1 <= IXMAX_VAL && x2 <= IXMAX_VAL && x3 <= IXMAX_VAL);
#ifdef TAKEHIRO_IEEE754_HACK
        BLOCK_IEEE754_x34_4
#else
        BLOCK_PLAIN_C_x34_4
#endif
            ERRDELTA_4 scratch[0] = x0 * x0;
        scratch[1] = x1 * x1;
        scratch[2] = x2 * x2;
        scratch[3] = x3 * x3;
        if (xfsfm < scratch[0])
            xfsfm = scratch[0];
        if (xfsfm < scratch[1])
            xfsfm = scratch[1];
        if (xfsfm < scratch[2])
            xfsfm = scratch[2];
        if (xfsfm < scratch[3])
            xfsfm = scratch[3];
        xfsf += scratch[0] + scratch[1] + scratch[2] + scratch[3];
        scratch += 4;
        xr += 4;
        xr34 += 4;
    }
    if (remaining) {
        double  x0 = sfpow34 * xr34[0];
        double  x1 = sfpow34 * xr34[1];
        assert(x0 <= IXMAX_VAL && x1 <= IXMAX_VAL);
#ifdef TAKEHIRO_IEEE754_HACK
        BLOCK_IEEE754_x34_2
#else
        BLOCK_PLAIN_C_x34_2
#endif
            ERRDELTA_2 scratch[0] = x0 * x0;
        scratch[1] = x1 * x1;
        if (xfsfm < scratch[0])
            xfsfm = scratch[0];
        if (xfsfm < scratch[1])
            xfsfm = scratch[1];
        xfsf += scratch[0] + scratch[1];
    }
    if (mq == 1) {
        return bw * select_kth(scratch_, bw, bw * 13 / 16);
    }
    xfsf /= bw;
    for (k = 1, j = 0; j < bw; ++j) {
        if (scratch_[j] > xfsf) {
            xfsfm += scratch_[j];
            ++k;
        }
    }
    return xfsfm / k * bw;
}


static  FLOAT8
calc_sfb_noise_mq_ISO(const FLOAT8 * xr, const FLOAT8 * xr34, int bw, int sf, int mq)
{
    const int SF = valid_sf(sf);
    FLOAT8  scratch_[192];
    FLOAT8 *scratch = scratch_;
    const FLOAT8 sfpow = POW20(SF); /*pow(2.0,sf/4.0); */
    const FLOAT8 sfpow34 = IPOW20(SF); /*pow(sfpow,-3.0/4.0); */
#ifndef TAKEHIRO_IEEE754_HACK
    const FLOAT8 compareval0 = (1.0 - 0.4054) / sfpow34;
    const FLOAT8 compareval1 = (2.0 - 0.4054) / sfpow34;
#endif

    FLOAT8  xfsfm = 0, xfsf = 0;
    fi_union fi[4];
    int     l3[4];
    int     k, j = bw >> 1;
    int     remaining = j % 2;
    assert(bw >= 0);
    for (j >>= 1; j > 0; --j) {
        double  x0 = sfpow34 * xr34[0];
        double  x1 = sfpow34 * xr34[1];
        double  x2 = sfpow34 * xr34[2];
        double  x3 = sfpow34 * xr34[3];
        assert(x0 <= IXMAX_VAL && x1 <= IXMAX_VAL && x2 <= IXMAX_VAL && x3 <= IXMAX_VAL);
#ifdef TAKEHIRO_IEEE754_HACK
        BLOCK_IEEE754_ISO_4
#else
        BLOCK_PLAIN_C_ISO_4
#endif
            ERRDELTA_4 scratch[0] = x0 * x0;
        scratch[1] = x1 * x1;
        scratch[2] = x2 * x2;
        scratch[3] = x3 * x3;
        if (xfsfm < scratch[0])
            xfsfm = scratch[0];
        if (xfsfm < scratch[1])
            xfsfm = scratch[1];
        if (xfsfm < scratch[2])
            xfsfm = scratch[2];
        if (xfsfm < scratch[3])
            xfsfm = scratch[3];
        xfsf += scratch[0] + scratch[1] + scratch[2] + scratch[3];
        scratch += 4;
        xr += 4;
        xr34 += 4;
    }
    if (remaining) {
        double  x0 = sfpow34 * xr34[0];
        double  x1 = sfpow34 * xr34[1];
        assert(x0 <= IXMAX_VAL && x1 <= IXMAX_VAL);
#ifdef TAKEHIRO_IEEE754_HACK
        BLOCK_IEEE754_ISO_2
#else
        BLOCK_PLAIN_C_ISO_2
#endif
            ERRDELTA_2 scratch[0] = x0 * x0;
        scratch[1] = x1 * x1;
        if (xfsfm < scratch[0])
            xfsfm = scratch[0];
        if (xfsfm < scratch[1])
            xfsfm = scratch[1];
        xfsf += scratch[0] + scratch[1];
    }
    if (mq == 1) {
        return bw * select_kth(scratch_, bw, bw * 13 / 16);
    }
    xfsf /= bw;
    for (k = 1, j = 0; j < bw; ++j) {
        if (scratch_[j] > xfsf) {
            xfsfm += scratch_[j];
            ++k;
        }
    }
    return xfsfm / k * bw;
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


#define FIND_BODY( FUNC ) \
    int sf = 128, sf_ok = 10000, delsf = 128, i;                \
    for (i = 0; i < 8; ++i) {                                   \
        delsf >>= 1;                                            \
        if (sf < sf_min) {                                      \
            /* scalefactors too small */                        \
            sf += delsf;                                        \
        }                                                       \
        else {                                                  \
            if (FUNC > l3_xmin) {                               \
                /* distortion.  try a smaller scalefactor */    \
                sf -= delsf;                                    \
            }                                                   \
            else {                                              \
                sf_ok = sf;                                     \
                sf += delsf;                                    \
            }                                                   \
        }                                                       \
    }                                                           \
    /*  returning a scalefac without distortion, if possible    \
     */                                                         \
    if (sf_ok <= 255)                                           \
        sf = sf_ok;                                             \
    return sf;


inline int
find_scalefac_x34(const FLOAT8 * xr, const FLOAT8 * xr34, FLOAT8 l3_xmin, int bw, int sf_min)
{
    FIND_BODY(calc_sfb_noise_x34(xr, xr34, bw, sf))
}

inline int
find_scalefac_ISO(const FLOAT8 * xr, const FLOAT8 * xr34, FLOAT8 l3_xmin, int bw, int sf_min)
{
    FIND_BODY(calc_sfb_noise_ISO(xr, xr34, bw, sf))
}

inline int
find_scalefac_mq_x34(const FLOAT8 * xr, const FLOAT8 * xr34, FLOAT8 l3_xmin, int bw, int mq,
                     int sf_min)
{
    FIND_BODY(calc_sfb_noise_mq_x34(xr, xr34, bw, sf, mq))
}

inline int
find_scalefac_mq_ISO(const FLOAT8 * xr, const FLOAT8 * xr34, FLOAT8 l3_xmin, int bw, int mq,
                     int sf_min)
{
    FIND_BODY(calc_sfb_noise_mq_ISO(xr, xr34, bw, sf, mq))
}

inline int
find_scalefac_ave_x34(const FLOAT8 * xr, const FLOAT8 * xr34, FLOAT8 l3_xmin, int bw, int sf_min)
{
    int     sf = 128, sf_ok = 10000, delsf = 128, i;
    for (i = 0; i < 8; ++i) {
        delsf >>= 1;
        if (sf <= sf_min) {
            sf += delsf;
        }
        else {
            if ((sf < 255 && calc_sfb_noise_x34(xr, xr34, bw, sf + 1) > l3_xmin)
                || calc_sfb_noise_x34(xr, xr34, bw, sf) > l3_xmin
                || calc_sfb_noise_x34(xr, xr34, bw, sf - 1) > l3_xmin) {
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
    if (sf_ok <= 255) {
        sf = sf_ok;
    }
    return sf;
}


inline int
find_scalefac_ave_ISO(const FLOAT8 * xr, const FLOAT8 * xr34, FLOAT8 l3_xmin, int bw, int sf_min)
{
    int     sf = 128, sf_ok = 10000, delsf = 128, i;
    for (i = 0; i < 8; ++i) {
        delsf >>= 1;
        if (sf <= sf_min) {
            sf += delsf;
        }
        else {
            if ((sf < 255 && calc_sfb_noise_ISO(xr, xr34, bw, sf + 1) > l3_xmin)
                || calc_sfb_noise_ISO(xr, xr34, bw, sf) > l3_xmin
                || calc_sfb_noise_ISO(xr, xr34, bw, sf - 1) > l3_xmin) {
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
    if (sf_ok <= 255) {
        sf = sf_ok;
    }
    return sf;
}


/**
 *  Robert Hegemann 2001-05-01
 *  calculates quantization step size determined by allowed masking
 */
inline int
calc_scalefac(FLOAT8 l3_xmin, int bw, int sfmin)
{
    FLOAT8 const c = 5.799142446; /* 10 * 10^(2/3) * log10(4/3) */
    int     sf = 210 + (int) (c * log10(l3_xmin / bw) - .5);
    if (sf < sfmin) {
        return sfmin;
    }
    if (sf <= 255) {
        return sf;
    }
    return 255;
}



static const int max_range_short[SBMAX_s * 3] = {
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    0, 0, 0
};

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
compute_scalefacs_short(int sf[], const gr_info * cod_info, const int *vbrsfmin)
{
    const int maxrange1 = 15, maxrange2 = 7;
    int     maxrange, maxover = 0;
    int     sfb, i;
    const int ifqstep = (cod_info->scalefac_scale == 0) ? 2 : 4;
    const int ifqstepShift = (cod_info->scalefac_scale == 0) ? 1 : 2;
    int    *scalefac = cod_info->scalefac;
    int    *sbg = cod_info->subblock_gain;

    for (i = 0; i < 3; ++i) {
        int     maxsf1 = 0, maxsf2 = 0, minsf = 1000, maxsfmin = 0;
        /* see if we should use subblock gain */
        for (sfb = 0; sfb < 6; ++sfb) { /* part 1 */
            int     v = -sf[sfb * 3 + i];
            if (maxsf1 < v)
                maxsf1 = v;
            if (maxsfmin < vbrsfmin[sfb * 3 + i])
                maxsfmin = vbrsfmin[sfb * 3 + i];
            if (minsf > v)
                minsf = v;
        }
        for (; sfb < SBPSY_s; ++sfb) { /* part 2 */
            int     v = -sf[sfb * 3 + i];
            if (maxsf2 < v)
                maxsf2 = v;
            if (maxsfmin < vbrsfmin[sfb * 3 + i])
                maxsfmin = vbrsfmin[sfb * 3 + i];
            if (minsf > v)
                minsf = v;
        }

        /* boost subblock gain as little as possible so we can
         * reach maxsf1 with scalefactors 
         * 8*sbg >= maxsf1   
         */
        {
            int     m1 = maxsf1 - (maxrange1 << ifqstepShift);
            int     m2 = maxsf2 - (maxrange2 << ifqstepShift);

            maxsf1 = Max(m1, m2);
        }
        if (minsf > 0) {
            sbg[i] = minsf >> 3;
        }
        else {
            sbg[i] = 0;
        }
        if (maxsf1 > 0) {
            int     m1 = sbg[i];
            int     m2 = (maxsf1 + 7) >> 3;
            sbg[i] = Max(m1, m2);
        }
        if (sbg[i] > 0 && (sbg[i] << 3) > cod_info->global_gain - maxsfmin) {
            sbg[i] = (cod_info->global_gain - maxsfmin) >> 3;
        }
        if (sbg[i] > 7)
            sbg[i] = 7;

        for (sfb = 0; sfb < SBPSY_s; ++sfb) {
            int     j = sfb * 3 + i;
            sf[j] += sbg[i] << 3;

            if (sf[j] < 0) {
                int     m;
                maxrange = sfb < 6 ? maxrange1 : maxrange2;

                scalefac[j] = (ifqstep - 1 - sf[j]) >> ifqstepShift;

                if (scalefac[j] > maxrange)
                    scalefac[j] = maxrange;
                m = cod_info->global_gain - (sbg[i] << 3) - vbrsfmin[j];
                if (scalefac[j] > 0 && (scalefac[j] << ifqstepShift) > m) {
                    scalefac[j] = m >> ifqstepShift;
                }
                m = -(sf[j] + (scalefac[j] << ifqstepShift));
                if (maxover < m) {
                    maxover = m;
                }
            }
            else {
                scalefac[j] = 0;
            }
        }
        scalefac[SBPSY_s * 3 + i] = 0;
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
compute_scalefacs_long_lsf(int *sf, const gr_info * cod_info, const int *vbrsfmin)
{
    const int *max_range = max_range_long;
    const int ifqstep = (cod_info->scalefac_scale == 0) ? 2 : 4;
    const int ifqstepShift = (cod_info->scalefac_scale == 0) ? 1 : 2;
    int    *scalefac = cod_info->scalefac;
    int     sfb;
    int     maxover;

    if (cod_info->preflag) {
        max_range = max_range_long_lsf_pretab;
        for (sfb = 11; sfb < SBPSY_l; ++sfb)
            sf[sfb] += pretab[sfb] << ifqstepShift;
    }

    maxover = 0;
    for (sfb = 0; sfb < SBPSY_l; ++sfb) {

        if (sf[sfb] < 0) {
            int     m;
            /* ifqstep*scalefac >= -sf[sfb], so round UP */
            scalefac[sfb] = (ifqstep - 1 - sf[sfb]) >> ifqstepShift;
            if (scalefac[sfb] > max_range[sfb])
                scalefac[sfb] = max_range[sfb];

            m = cod_info->global_gain - ((cod_info->preflag ? pretab[sfb] : 0) << ifqstepShift) -
                vbrsfmin[sfb];
            if (scalefac[sfb] > 0 && (scalefac[sfb] << ifqstepShift) > m) {
                scalefac[sfb] = m >> ifqstepShift;
            }

            m = -(sf[sfb] + (scalefac[sfb] << ifqstepShift));
            if (maxover < m) {
                maxover = m;
            }
        }
        else {
            scalefac[sfb] = 0;
        }
    }
    scalefac[SBPSY_l] = 0; /* sfb21 */

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
compute_scalefacs_long(int *sf, const gr_info * cod_info, const int *vbrsfmin)
{
    const int ifqstep = (cod_info->scalefac_scale == 0) ? 2 : 4;
    const int ifqstepShift = (cod_info->scalefac_scale == 0) ? 1 : 2;
    int    *scalefac = cod_info->scalefac;
    int     sfb;
    int     maxover;

    if (cod_info->preflag) {
        for (sfb = 11; sfb < SBPSY_l; ++sfb) {
            sf[sfb] += pretab[sfb] << ifqstepShift;
        }
    }

    maxover = 0;
    for (sfb = 0; sfb < SBPSY_l; ++sfb) {

        if (sf[sfb] < 0) {
            int     m;
            /* ifqstep*scalefac >= -sf[sfb], so round UP */
            scalefac[sfb] = (ifqstep - 1 - sf[sfb]) >> ifqstepShift;
            if (scalefac[sfb] > max_range_long[sfb])
                scalefac[sfb] = max_range_long[sfb];

            m = cod_info->global_gain - ((cod_info->preflag ? pretab[sfb] : 0) << ifqstepShift) -
                vbrsfmin[sfb];
            if (scalefac[sfb] > 0 && (scalefac[sfb] << ifqstepShift) > m) {
                scalefac[sfb] = m >> ifqstepShift;
            }

            m = -(sf[sfb] + (scalefac[sfb] << ifqstepShift));
            if (maxover < m) {
                maxover = m;
            }
        }
        else {
            scalefac[sfb] = 0;
        }
    }
    scalefac[SBPSY_l] = 0; /* sfb21 */

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
block_sf(const lame_internal_flags * gfc, const gr_info * gi, const FLOAT8 l3_xmin[576],
         const FLOAT8 xr34_orig[576], int vbrsf[SFBMAX], int vbrsfmin[SFBMAX])
{
    const int psymax = gi->psymax;
    const int max_nonzero_coeff = gi->max_nonzero_coeff;
    int     sf = 0;
    int     sfb, j;
    int     scalefac_criteria;

    scalefac_criteria = gfc->VBR->quality;
    /* map quant_comp settings to internal selections */
    if (gi->block_type != NORM_TYPE)
        scalefac_criteria = gfc->gfp->quant_comp_short;

    j = 0;
    for (sfb = 0; sfb < psymax; ++sfb) {
        int     sf_min, width = gi->width[sfb];

        if (j + width > max_nonzero_coeff) {
            width = max_nonzero_coeff - j + 1;
        }
        sf_min = find_lowest_scalefac(max_x34(&xr34_orig[j], width));

        switch (scalefac_criteria) {
        case 0:
            /*  the slower and better mode to use at higher quality
             */
            if (gfc->quantization) {
                vbrsf[sfb] =
                    find_scalefac_ave_x34(&gi->xr[j], &xr34_orig[j], l3_xmin[sfb], width, sf_min);
            }
            else {
                vbrsf[sfb] =
                    find_scalefac_ave_ISO(&gi->xr[j], &xr34_orig[j], l3_xmin[sfb], width, sf_min);
            }
            break;
        case 1:
        case 2:
            /*  maxnoise mode to use at higher quality
             */
            if (gfc->quantization) {
                vbrsf[sfb] = find_scalefac_mq_x34(&gi->xr[j], &xr34_orig[j], l3_xmin[sfb],
                                                  width, 2 - scalefac_criteria, sf_min);
            }
            else {
                vbrsf[sfb] = find_scalefac_mq_ISO(&gi->xr[j], &xr34_orig[j], l3_xmin[sfb],
                                                  width, 2 - scalefac_criteria, sf_min);
            }
            break;
        case 3:
            /*  the faster and sloppier mode to use at lower quality
             */
            if (gfc->quantization) {
                vbrsf[sfb] =
                    find_scalefac_x34(&gi->xr[j], &xr34_orig[j], l3_xmin[sfb], width, sf_min);
            }
            else {
                vbrsf[sfb] =
                    find_scalefac_ISO(&gi->xr[j], &xr34_orig[j], l3_xmin[sfb], width, sf_min);
            }
            break;
        case 4:
        default:
            /*  the fastest
             */
            vbrsf[sfb] = calc_scalefac(l3_xmin[sfb], width, sf_min);
            break;
        }
        if (sf < vbrsf[sfb]) {
            sf = vbrsf[sfb];
        }
        vbrsfmin[sfb] = sf_min;
        if (width < gi->width[sfb]) {
            for (++sfb; sfb < psymax; ++sfb) {
                vbrsf[sfb] = sf;
                vbrsfmin[sfb] = 0;
            }
            break;
        }
        j += width;
    }
}

/*
 * See below function for comments
 */
static int
short_block_sf(const lame_internal_flags * gfc, const gr_info * gi, int vbrsf[SFBMAX])
{
    int     sfb, b, i;
    int     vbrmean, vbrmn, vbrmx, vbrclip;
    int     sf_cache[SBMAX_s];

    int     vbrmax = 0;

    for (b = 0; b < 3; ++b) {
        /*  smoothing
         */
        switch (gfc->VBR->smooth) {
        default:
        case 0:
            /*  get max value
             */
            for (sfb = b + gi->sfb_lmax; sfb < gi->sfbmax; sfb += 3) {
                if (vbrmax < vbrsf[sfb])
                    vbrmax = vbrsf[sfb];
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
            if (vbrmax < vbrmx)
                vbrmax = vbrmx;
            break;

        case 2:
            vbrclip = vbrsf[gi->sfb_lmax + b + 3] + MAX_SF_DELTA;
            if (vbrsf[gi->sfb_lmax + b] > vbrclip)
                vbrsf[gi->sfb_lmax + b] = vbrclip;
            if (vbrmax < vbrsf[gi->sfb_lmax + b])
                vbrmax = vbrsf[gi->sfb_lmax + b];
            for (sfb = gi->sfb_lmax + 3 + b; sfb < gi->sfbmax - 3; sfb += 3) {
                vbrclip = vbrsf[sfb - 3] + MAX_SF_DELTA;
                if (vbrsf[sfb] > vbrclip)
                    vbrsf[sfb] = vbrclip;
                vbrclip = vbrsf[sfb + 3] + MAX_SF_DELTA;
                if (vbrsf[sfb] > vbrclip)
                    vbrsf[sfb] = vbrclip;
                if (vbrmax < vbrsf[sfb])
                    vbrmax = vbrsf[sfb];
            }
            vbrclip = vbrsf[sfb - 3] + MAX_SF_DELTA;
            if (vbrsf[sfb] > vbrclip)
                vbrsf[sfb] = vbrclip;
            if (vbrmax < vbrsf[sfb])
                vbrmax = vbrsf[sfb];
            break;
        }
    }
    return vbrmax;
}





/* smooth scalefactors
 *
 * The find_scalefac* routines calculate
 * scalefactors which introduce as much
 * noise as it is allowed. Because of the
 * *flawed* psy model, at many test samples
 * some scalefactors where too high. That's
 * why the idea of limiting came into play
 * to give a somehow smoother noise
 * distribution over the scalefactor bands.
 * According to Robert, this improved on
 * samples with tonality problems a lot,
 * but is only a workaround against some
 * psymodel limitations.
 */
static int
long_block_sf(const lame_internal_flags * gfc, const gr_info * gi, int vbrsf[SFBMAX])
{
    int     sfb;
    int     vbrmean, vbrmn, vbrmx, vbrclip;
    int     sf_cache[SBMAX_l];
    int     vbrmax = 0;

    switch (gfc->VBR->smooth) {
    default:
    case 0:
        /* no smoothing */

        /*  get max value
         */
        for (sfb = 0; sfb < gi->sfb_lmax; ++sfb) {
            if (vbrmax < vbrsf[sfb])
                vbrmax = vbrsf[sfb];
        }
        break;

    case 1:
        /* cut high scalefactors based on median */

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
        vbrmean = select_kth_int(sf_cache, gi->sfb_lmax, (gi->sfb_lmax + 1) / 2);

        /*  cut peaks
         */
        vbrclip = vbrmean + (vbrmean - vbrmn);
        for (sfb = 0; sfb < gi->sfb_lmax; ++sfb) {
            if (vbrsf[sfb] > vbrclip)
                vbrsf[sfb] = vbrclip;
        }
        if (vbrmx > vbrclip)
            vbrmx = vbrclip;
        vbrmax = vbrmx;
        break;

    case 2:
        /* limit neighboring factors by a fixed delta */

        vbrclip = vbrsf[1] + MAX_SF_DELTA;
        if (vbrsf[0] > vbrclip)
            vbrsf[0] = vbrclip;
        vbrmax = vbrsf[0];
        for (sfb = 1; sfb < gi->sfb_lmax - 1; ++sfb) {
            vbrclip = vbrsf[sfb - 1] + MAX_SF_DELTA;
            if (vbrsf[sfb] > vbrclip)
                vbrsf[sfb] = vbrclip;
            vbrclip = vbrsf[sfb + 1] + MAX_SF_DELTA;
            if (vbrsf[sfb] > vbrclip)
                vbrsf[sfb] = vbrclip;
            if (vbrmax < vbrsf[sfb])
                vbrmax = vbrsf[sfb];
        }
        vbrclip = vbrsf[sfb - 1] + MAX_SF_DELTA;
        if (vbrsf[sfb] > vbrclip)
            vbrsf[sfb] = vbrclip;
        if (vbrmax < vbrsf[sfb])
            vbrmax = vbrsf[sfb];
        break;
    }
    return vbrmax;
}



inline int
checkScalefactor(const gr_info * cod_info, const int vbrsfmin[SFBMAX])
{
    int     sfb;
    for (sfb = 0; sfb < cod_info->psymax; ++sfb) {
        const int s = ((cod_info->scalefac[sfb] + (cod_info->preflag ? pretab[sfb] : 0))
                       << (cod_info->scalefac_scale + 1))
            + cod_info->subblock_gain[cod_info->window[sfb]] * 8;

        if (cod_info->global_gain - s < vbrsfmin[sfb]) {
            return 0;
        }
    }
    return 1;
}

/******************************************************************
 *
 *  short block scalefacs
 *
 ******************************************************************/

static void
short_block_scalefacs(const lame_internal_flags * gfc, gr_info * cod_info,
                      int vbrsf[SFBMAX], const int vbrsfmin[SFBMAX], int vbrmax)
{
    int     sfb, b;
    int     maxover0, maxover1, mover;
    int     v0, v1, vm0, vm1, maxminsfb = 0;
    maxover0 = 0;
    maxover1 = 0;
    for (sfb = 0; sfb < cod_info->psymax; ++sfb) {
        if (vbrsf[sfb] < vbrsfmin[sfb])
            vbrsf[sfb] = vbrsfmin[sfb];
        v0 = (vbrmax - vbrsf[sfb]) - (4 * 14 + 2 * max_range_short[sfb]);
        v1 = (vbrmax - vbrsf[sfb]) - (4 * 14 + 4 * max_range_short[sfb]);
        if (maxover0 < v0)
            maxover0 = v0;
        if (maxover1 < v1)
            maxover1 = v1;
        if (maxminsfb < vbrsfmin[sfb])
            maxminsfb = vbrsfmin[sfb];
    }
    for (vm0 = 0, vm1 = 0, sfb = 0; sfb < cod_info->psymax; ++sfb) {
        int     a = (vbrmax - vbrsfmin[sfb]) - 2 * max_range_long[sfb];
        int     b = (vbrmax - vbrsfmin[sfb]) - 4 * max_range_long[sfb];
        if (a <= 0)
            vm0 = Min(vm0, a);
        if (b <= 0)
            vm1 = Min(vm1, b);
    }

    if (gfc->noise_shaping == 2)
        /* allow scalefac_scale=1 */
        mover = Min(maxover0, maxover1);
    else
        mover = maxover0;

    vbrmax -= mover;
    maxover0 -= mover;
    maxover1 -= mover;

    if (maxover0 == 0) {
        cod_info->scalefac_scale = 0;
    }
    else if (maxover1 == 0) {
        cod_info->scalefac_scale = 1;
    }

    if (vbrmax < maxminsfb)
        vbrmax = maxminsfb;

    cod_info->global_gain = vbrmax;

    if (cod_info->global_gain < 0) {
        cod_info->global_gain = 0;
    }
    else if (cod_info->global_gain > 255) {
        cod_info->global_gain = 255;
    }
    for (sfb = 0; sfb < SBMAX_s; ++sfb) {
        for (b = 0; b < 3; ++b) {
            vbrsf[sfb * 3 + b] -= vbrmax;
        }
    }
    assert(cod_info->global_gain < 256);
    compute_scalefacs_short(vbrsf, cod_info, vbrsfmin);
    /*assert(checkScalefactor(cod_info, vbrsfmin));*/
}



/******************************************************************
 *
 *  long block scalefacs
 *
 ******************************************************************/

static void
long_block_scalefacs(const lame_internal_flags * gfc, gr_info * cod_info,
                     int vbrsf[SFBMAX], const int vbrsfmin[SFBMAX], int vbrmax)
{
    const int *max_rangep;
    int     sfb;
    int     maxover0, maxover1, maxover0p, maxover1p, mover;
    int     v0, v1, v0p, v1p, vm0p, vm1p, vm0, vm1, maxminsfb = 0;

    max_rangep = gfc->mode_gr == 2 ? max_range_long : max_range_long_lsf_pretab;

    maxover0 = 0;
    maxover1 = 0;
    maxover0p = 0;      /* pretab */
    maxover1p = 0;      /* pretab */

    for (sfb = 0; sfb < cod_info->psymax; ++sfb) {
        if (vbrsf[sfb] < vbrsfmin[sfb])
            vbrsf[sfb] = vbrsfmin[sfb];
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
        if (maxminsfb < vbrsfmin[sfb])
            maxminsfb = vbrsfmin[sfb];
    }
    for (vm0p = 1, vm1p = 1, sfb = 0; sfb < cod_info->psymax; ++sfb) {
        int     a = (vbrmax - vbrsfmin[sfb]) - 2 * pretab[sfb];
        int     b = (vbrmax - vbrsfmin[sfb]) - 4 * max_rangep[sfb];
        if (a <= 0)
            vm0p = 0;
        if (b <= 0)
            vm1p = 0;
    }
    for (vm0 = 0, vm1 = 0, sfb = 0; sfb < cod_info->psymax; ++sfb) {
        int     a = (vbrmax - vbrsfmin[sfb]) - 2 * (max_range_long[sfb] + 1);
        int     b = (vbrmax - vbrsfmin[sfb]) - 4 * (max_range_long[sfb] + 1);
        if (a <= 0)
            vm0 = Min(vm0, a);
        if (b <= 0)
            vm1 = Min(vm1, b);
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


    if (maxover0 == 0) {
        cod_info->scalefac_scale = 0;
        cod_info->preflag = 0;
    }
    else if (maxover0p == 0 && vm0p) {
        cod_info->scalefac_scale = 0;
        cod_info->preflag = 1;
    }
    else if (maxover1 == 0 || ((maxover0p == 0) && !vm0p)) {
        cod_info->scalefac_scale = 1;
        cod_info->preflag = 0;
    }
    else if (maxover1p == 0) {
        cod_info->scalefac_scale = 1;
        cod_info->preflag = vm1p;
    }
    else {
        assert(0);      /* this should not happen */
    }

    if (vbrmax < maxminsfb)
        vbrmax = maxminsfb;

    cod_info->global_gain = vbrmax;
    if (cod_info->global_gain < 0) {
        cod_info->global_gain = 0;
    }
    else if (cod_info->global_gain > 255)
        cod_info->global_gain = 255;

    for (sfb = 0; sfb < SBMAX_l; ++sfb)
        vbrsf[sfb] -= vbrmax;

    if (gfc->mode_gr == 2)
        compute_scalefacs_long(vbrsf, cod_info, vbrsfmin);
    else
        compute_scalefacs_long_lsf(vbrsf, cod_info, vbrsfmin);
    /*assert(checkScalefactor(cod_info, vbrsfmin));*/
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




#define BODY_1 \
	/* 0.634521682242439 = 0.5946*2**(.5*0.1875) */                                     \
	const FLOAT8 roundfac = 0.634521682242439                                           \
                    / IPOW20(cod_info->global_gain+cod_info->scalefac_scale);           \
    const int max_nonzero_coeff = cod_info->max_nonzero_coeff;                          \
    const int *scalefac = cod_info->scalefac;                                           \
    int     sfb, j = 0, sfbmax;                                                         \
    fi_union fi[4];\
    int* l3 = cod_info->l3_enc;                                                 \
    sfbmax = cod_info->psymax;                                                          \
                                                                                        \
    for (sfb = 0; sfb < sfbmax; ++sfb) {                                                \
        const int s =  ((scalefac[sfb] + (cod_info->preflag ? pretab[sfb] : 0))         \
                 << (cod_info->scalefac_scale + 1))                                     \
                 + cod_info->subblock_gain[cod_info->window[sfb]] * 8;                  \
        int remaining;                                                                  \
        int l = cod_info->width[sfb];                                                   \
        const int notpseudohalf = !(gfc->pseudohalf[sfb]&&(gfc->substep_shaping & 2));  \
                                                                                        \
        FLOAT8 fac = IIPOW20(s);                                                        \
        FLOAT8 jstep = fac*istep;                                                       \
        if ( l >= max_nonzero_coeff-j+1 ) {                                             \
            l = max_nonzero_coeff-j+1;                                                  \
        }                                                                               \
        assert( l >= 0 );                                                               \
        j += l; \
        l >>= 1;                                                                        \
        remaining = l%2;                                                                \
                                                                                        \
        for ( l >>= 1; l > 0; --l ) {                                                   \
            xr34[0] = fac * xr34_orig[0];                                               \
            xr34[1] = fac * xr34_orig[1];                                               \
            xr34[2] = fac * xr34_orig[2];                                               \
            xr34[3] = fac * xr34_orig[3];                                               \
            {                                                                           \
                double x0 = jstep * xr34_orig[0];                                       \
                double x1 = jstep * xr34_orig[1];                                       \
                double x2 = jstep * xr34_orig[2];                                       \
                double x3 = jstep * xr34_orig[3];                                       \
                assert ( x0 <= IXMAX_VAL                                                \
                  && x1 <= IXMAX_VAL                                                    \
                  && x2 <= IXMAX_VAL                                                    \
                  && x3 <= IXMAX_VAL );                                                 \

#define BODY_2 \
                    if ( notpseudohalf ) {                                              \
                        /*  most often the case */                                      \
                    }                                                                   \
                    else {                                                              \
                        if ( xr34[0] > roundfac ) {} else {                             \
                            l3[0] = 0;                                                \
                        }                                                               \
                        if ( xr34[1] > roundfac ) {} else {                             \
                            l3[1] = 0;                                                \
                        }                                                               \
                        if ( xr34[2] > roundfac ) {} else {                             \
                            l3[2] = 0;                                                \
                        }                                                               \
                        if ( xr34[3] > roundfac ) {} else {                             \
                            l3[3] = 0;                                                \
                        }                                                               \
                    }                                                                   \
                    l3 += 4;                                                        \
                    xr34 += 4;                                                          \
                    xr34_orig +=4;                                                      \
                /*}                                                                     \
                else return 0;*/                                                        \
            }                                                                           \
        }                                                                               \
        if ( remaining ) {                                                              \
            xr34[0] = fac * xr34_orig[0];                                               \
            xr34[1] = fac * xr34_orig[1];                                               \
            {                                                                           \
                double x0 = jstep * xr34_orig[0];                                       \
                double x1 = jstep * xr34_orig[1];                                       \
                assert ( x0 <= IXMAX_VAL                                                    \
                  && x1 <= IXMAX_VAL );                                                \

#define BODY_3 \
                    if ( notpseudohalf ) {                                              \
                        /*  most often the case */                                      \
                    }                                                                   \
                    else {                                                              \
                        if ( xr34[0] > roundfac ) {} else {                             \
                            l3[0] = 0;                                                \
                        }                                                               \
                        if ( xr34[1] > roundfac ) {} else {                             \
                            l3[1] = 0;                                                \
                        }                                                               \
                    }                                                                   \
                    l3 += 2;                                                        \
                    xr34 += 2;                                                          \
                    xr34_orig +=2;                                                      \
                /*}                                                                     \
                else return 0;*/                                                        \
            }                                                                           \
        }                                                                               \
        if ( j <= max_nonzero_coeff ) {                                                 \
            /*  most often the case */                                                  \
        }                                                                               \
        else {                                                                          \
            break;                                                                      \
        }                                                                               \
    }                                                                                   \
    if ( j < 576 ) {                                                            \
        int n = 576-j;                                                          \
        memset( xr34, 0, n*sizeof(FLOAT8) );                                    \
        memset( l3, 0, n*sizeof(int) );                                     \
    }                                                                           \
    return 1;                                                                   \


#ifdef TAKEHIRO_IEEE754_HACK

static int
quantize_x34(const lame_internal_flags * gfc, gr_info * const cod_info,
             const FLOAT8 xr34_orig[576], FLOAT8 xr34[576])
{
    const FLOAT8 istep = IPOW20(cod_info->global_gain);
BODY_1 BLOCK_IEEE754_x34_4 BODY_2 BLOCK_IEEE754_x34_2 BODY_3}

static int
quantize_ISO(const lame_internal_flags * gfc, gr_info * const cod_info,
             const FLOAT8 xr34_orig[576], FLOAT8 xr34[576])
{
    const FLOAT8 istep = IPOW20(cod_info->global_gain);
BODY_1 BLOCK_IEEE754_ISO_4 BODY_2 BLOCK_IEEE754_ISO_2 BODY_3}

#else

static int
quantize_x34(const lame_internal_flags * gfc, gr_info * const cod_info,
             const FLOAT8 xr34_orig[576], FLOAT8 xr34[576])
{
    const FLOAT8 istep = IPOW20(cod_info->global_gain);
BODY_1 BLOCK_PLAIN_C_x34_4 BODY_2 BLOCK_PLAIN_C_x34_2 BODY_3}

static int
quantize_ISO(const lame_internal_flags * gfc, gr_info * const cod_info,
             const FLOAT8 xr34_orig[576], FLOAT8 xr34[576])
{
    const FLOAT8 istep = IPOW20(cod_info->global_gain);
    const FLOAT8 compareval0 = (1.0 - 0.4054) / istep;
    const FLOAT8 compareval1 = (2.0 - 0.4054) / istep;
BODY_1 BLOCK_PLAIN_C_ISO_4 BODY_2 BLOCK_PLAIN_C_ISO_2 BODY_3}

#endif

inline int
quantizeAndCountBits(lame_internal_flags * gfc, gr_info * cod_info, const FLOAT8 xr34_orig[576],
                     FLOAT8 xr34[576])
{
    int     x = 0;
    if (gfc->quantization) {
        x = quantize_x34(gfc, cod_info, xr34_orig, xr34);
    }
    else {
        x = quantize_ISO(gfc, cod_info, xr34_orig, xr34);
    }
    if (x) {
        cod_info->part2_3_length = noquant_count_bits(gfc, cod_info);
    }
    else {
        cod_info->part2_3_length = LARGE_BITS;
    }
    return cod_info->part2_3_length;
}


inline int
tryScalefacColor(lame_internal_flags * const gfc, gr_info * cod_info, int vbrsf[SFBMAX],
                 const int vbrsf2[SFBMAX], const int vbrsfmin[SFBMAX], int I, int M, int target,
                 FLOAT8 xr34[576], const FLOAT8 xr34orig[576])
{
    FLOAT8  xrpow_max = cod_info->xrpow_max;
    int     vbrmax, i, nbits;

    for (vbrmax = 0, i = 0; i < cod_info->psymax; ++i) {
        vbrsf[i] = target + (vbrsf2[i] - target) * I / M;
        if (vbrsf[i] < vbrsfmin[i])
            vbrsf[i] = vbrsfmin[i];
        if (vbrsf[i] > 255)
            vbrsf[i] = 255;
        if (vbrmax < vbrsf[i])
            vbrmax = vbrsf[i];
    }
    if (cod_info->block_type == SHORT_TYPE) {
        short_block_scalefacs(gfc, cod_info, vbrsf, vbrsfmin, vbrmax);
    }
    else {
        long_block_scalefacs(gfc, cod_info, vbrsf, vbrsfmin, vbrmax);
    }
    if (checkScalefactor(cod_info, vbrsfmin) == 0)
        return LARGE_BITS;
    if (gfc->mode_gr == 2) {
        scale_bitcount(cod_info);
    }
    else {
        scale_bitcount_lsf(gfc, cod_info);
    }
    nbits = quantizeAndCountBits(gfc, cod_info, xr34orig, xr34);
    cod_info->xrpow_max = xrpow_max;
    return nbits;
}

static void
searchScalefacColor(lame_internal_flags * const gfc, gr_info * cod_info, int sfwork[SFBMAX],
                    const int sfcalc[SFBMAX], const int vbrsfmin[SFBMAX], int minimize, int bits,
                    FLOAT8 xr34[576], FLOAT8 xr34orig[576])
{
    int     nbits, last, i, ok = -1, l = 0, r, vbrmin = 255, vbrmax = 0, M, target;
    for (i = 0; i < cod_info->psymax; ++i) {
        if (vbrmin > sfcalc[i])
            vbrmin = sfcalc[i];
        if (vbrmax < sfcalc[i])
            vbrmax = sfcalc[i];
    }
    M = vbrmax - vbrmin;

    if (M == 0)
        return;

    target = minimize ? vbrmin : vbrmax;
    for (l = 0, r = M, last = i = M / 2; l <= r; i = (l + r) / 2) {
        nbits =
            tryScalefacColor(gfc, cod_info, sfwork, sfcalc, vbrsfmin, i, M, target, xr34, xr34orig);
        if (minimize) {
            if (nbits > bits) {
                ok = i;
                r = i - 1;
            }
            else {
                l = i + 1;
            }
        }
        else {
            if (nbits < bits) {
                ok = i;
                l = i + 1;
            }
            else {
                r = i - 1;
            }
        }
        last = i;
    }
    if (last != ok) {
        if (ok == -1)
            ok = 0;
        nbits =
            tryScalefacColor(gfc, cod_info, sfwork, sfcalc, vbrsfmin, ok, M, target, xr34,
                             xr34orig);
    }
}


inline int
tryGlobalStepsize(lame_internal_flags * const gfc, gr_info * cod_info, const int sfwork[SFBMAX],
                  const int vbrsfmin[SFBMAX], int delta, FLOAT8 xr34[576],
                  const FLOAT8 xr34orig[576])
{
    FLOAT8  xrpow_max = cod_info->xrpow_max;
    int     sftemp[SFBMAX], vbrmax, i, nbits;
    for (vbrmax = 0, i = 0; i < cod_info->psymax; ++i) {
        sftemp[i] = sfwork[i] + delta;
        if (sftemp[i] < vbrsfmin[i])
            sftemp[i] = vbrsfmin[i];
        if (sftemp[i] > 255)
            sftemp[i] = 255;
        if (vbrmax < sftemp[i])
            vbrmax = sftemp[i];
    }
    if (cod_info->block_type == 2) {
        short_block_scalefacs(gfc, cod_info, sftemp, vbrsfmin, vbrmax);
    }
    else {
        long_block_scalefacs(gfc, cod_info, sftemp, vbrsfmin, vbrmax);
    }
    if (checkScalefactor(cod_info, vbrsfmin) == 0)
        return LARGE_BITS;
    if (gfc->mode_gr == 2) {
        scale_bitcount(cod_info);
    }
    else {
        scale_bitcount_lsf(gfc, cod_info);
    }
    nbits = quantizeAndCountBits(gfc, cod_info, xr34orig, xr34);
    cod_info->xrpow_max = xrpow_max;
    return nbits;
}

static void
searchGlobalStepsize(lame_internal_flags * const gfc, gr_info * cod_info, const int sfwork[SFBMAX],
                     const int vbrsfmin[SFBMAX], int minimize, int target, FLOAT8 xr34[576],
                     const FLOAT8 xr34orig[576])
{
    const int gain = cod_info->global_gain;
    int     curr = gain;
    int     gain_ok = 1024;
    int     nbits = LARGE_BITS;

    int     l, r;

    assert(gain >= 0);

    if (minimize) {
        r = gain;
        l = 0;
    }
    else {
        r = 512;
        l = gain;
    }
    while (l <= r) {
        curr = (l + r) >> 1;
        nbits = tryGlobalStepsize(gfc, cod_info, sfwork, vbrsfmin, curr - gain, xr34, xr34orig);
        if (cod_info->part2_length >= LARGE_BITS || nbits >= LARGE_BITS) {
            l = curr + 1;
            continue;
        }
        if (nbits + cod_info->part2_length < target) {
            if (minimize) {
                l = curr + 1;
                if (gain_ok == 1024)
                    gain_ok = curr;
            }
            else {
                r = curr - 1;
                gain_ok = curr;
            }
        }
        else {
            if (minimize) {
                r = curr - 1;
                gain_ok = curr;
            }
            else {
                l = curr + 1;
                if (gain_ok == 1024)
                    gain_ok = curr;
            }
        }
    }
    if (gain_ok != curr) {
        curr = gain_ok;
        nbits = tryGlobalStepsize(gfc, cod_info, sfwork, vbrsfmin, curr - gain, xr34, xr34orig);
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
    FLOAT8  xr34[576];
    int     sfwork[SFBMAX];
    int     sfcalc[SFBMAX];
    int     vbrsfmin[SFBMAX];
    int     vbrmax;
    int     didmaxize = 0;
    gr_info *cod_info = &gfc->l3_side.tt[gr][ch];

    block_sf(gfc, cod_info, l3_xmin, xr34orig, sfcalc, vbrsfmin);
    memcpy(sfwork, sfcalc, SFBMAX * sizeof(int));
    if (cod_info->block_type == SHORT_TYPE) {
        vbrmax = short_block_sf(gfc, cod_info, sfwork);
    }
    else {
        vbrmax = long_block_sf(gfc, cod_info, sfwork);
    }
    if (cod_info->block_type == SHORT_TYPE) {
        short_block_scalefacs(gfc, cod_info, sfwork, vbrsfmin, vbrmax);
    }
    else {
        long_block_scalefacs(gfc, cod_info, sfwork, vbrsfmin, vbrmax);
    }
    if (gfc->mode_gr == 2) {
        if (0 != scale_bitcount(cod_info))
            /*  this should not happen due to the way the scalefactors are selected
             */
            return -1;
    }
    else {
        if (0 != scale_bitcount_lsf(gfc, cod_info))
            /*  this should not happen due to the way the scalefactors are selected
             */
            return -1;
    }
    quantizeAndCountBits(gfc, cod_info, xr34orig, xr34);
    if (!didmaxize && cod_info->part2_3_length < minbits - cod_info->part2_length) {
        searchScalefacColor(gfc, cod_info, sfwork, sfcalc, vbrsfmin, 1, minbits, xr34, xr34orig);
    }
    if (cod_info->part2_3_length > maxbits - cod_info->part2_length) {
        didmaxize = 1;
        searchScalefacColor(gfc, cod_info, sfwork, sfcalc, vbrsfmin, 0, maxbits, xr34, xr34orig);
    }
    if (!didmaxize && cod_info->part2_3_length < minbits - cod_info->part2_length) {
        searchGlobalStepsize(gfc, cod_info, sfwork, vbrsfmin, 1, minbits, xr34, xr34orig);
    }
    if (cod_info->part2_3_length > maxbits - cod_info->part2_length) {
        searchGlobalStepsize(gfc, cod_info, sfwork, vbrsfmin, 0, maxbits, xr34, xr34orig);
    }
    if (gfc->substep_shaping & 2) {
        FLOAT8  xrtmp[576];
        trancate_smallspectrums(gfc, cod_info, l3_xmin, xrtmp);
    }
    if (gfc->use_best_huffman == 2) {
        best_huffman_divide(gfc, cod_info);
    }
    assert(cod_info->global_gain < 256u);

    if (cod_info->part2_3_length + cod_info->part2_length >= LARGE_BITS) {
        return -2;      /* Houston, we have a problem */
    }
    return 0;
}
