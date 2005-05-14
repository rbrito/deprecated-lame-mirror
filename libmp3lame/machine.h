/*
 *	Machine dependent defines/includes for LAME.
 *
 *	Copyright (c) 1999 A.L. Faber
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

#ifndef LAME_MACHINE_H
#define LAME_MACHINE_H

#include <stdio.h>

#ifdef STDC_HEADERS
# include <stdlib.h>
# include <string.h>
#else
# ifndef HAVE_MEMCPY
#  define memcpy(d, s, n) bcopy ((s), (d), (n))
#  define memmove(d, s, n) bcopy ((s), (d), (n))
#  define memset(d, c, n) (assert((c)==0), bzero((d), (n)))
# endif
#endif

#if HAVE_INTTYPES_H
# include <inttypes.h>
#else
# if HAVE_STDINT_H
#  include <stdint.h>
# endif
#endif

#if  defined(__riscos__)  &&  defined(FPA10)
# include "ymath.h"
#else
# if defined(HAVE_TGMATH_H) && !defined(__alpha__)
#  include <tgmath.h>
# else
#  include <math.h>
# endif
#endif

#include <ctype.h>

#if defined(macintosh)
# include <types.h>
# include <stat.h>
#else
# include <sys/types.h>
# include <sys/stat.h>
#endif

#ifdef _MSC_VER
# pragma warning( disable : 4244 )
/*# pragma warning( disable : 4305 ) */
#endif

/*
 * FLOAT    for variables which require at least 32 bits
 *
 * On some machines, 64 bit will be faster than 32 bit.  Also, some math
 * routines require 64 bit float, so setting FLOAT=float will result in a
 * lot of conversions.
 */

#if ( defined(_MSC_VER) || defined(__BORLANDC__) || defined(__MINGW32__) )
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <float.h>
# define FLOAT_MAX FLT_MAX
# define u64const(x) (uint64_t)x
#else
# ifdef ALLDOUBLE
typedef double FLOAT;
#  ifdef DBL_MAX
#   define FLOAT_MAX DBL_MAX
#  endif
# else
typedef float   FLOAT;
#  ifdef FLT_MAX
#   define FLOAT_MAX FLT_MAX
#  endif
# endif
# define u64const(x) x##LL
#endif

#ifndef FLOAT_MAX
# define FLOAT_MAX 1e37 /* approx */
#endif

/* sample_t must be floating point, at least 32 bits */
typedef FLOAT     sample_t;

#ifdef USE_IEEE754_HACK
# define USE_FAST_LOG
# define MAGIC_FLOAT ((double)65536*128)
# define MAGIC_INT 0x4b000000
# define MAGIC_FLOAT2 ((double)65536/4)
# define MAGIC_INT2 0x46800000
# define FIXEDPOINT 9
# define ROUNDFAC_NEAR (ROUNDFAC-0.5)
# define signbits(x) (*((unsigned int*)&x) >> 31)
#else
# define signbits(x) (x < (FLOAT)0.)
#endif

typedef union {
    float f;
    int i;
} fi_union;

/* 
 * 3 different types of pow() functions:
 *   - table lookup
 *   - pow()
 *   - exp()   on some machines this is claimed to be faster than pow()
 */

#if (defined(SMALL_CACHE) && defined(USE_IEEE754_HACK))
static inline FLOAT IPOW20core(int x)
{
    fi_union fi;
    extern float ipow20[];

    fi.i = ((int*)ipow20)[x&15] - (x>>4)*(3*0x800000);
    return fi.f;
}

static inline FLOAT POW20core(int x)
{
    fi_union fi;
    extern float pow20[];

    fi.i = ((int*)pow20)[x&3] + (x>>2)*(0x800000);

    return fi.f;
}
# define IPOW20(x)  (assert(-Q_MAX2 <= (x) && (x) < Q_MAX), IPOW20core((x)+Q_MAX2))
# define POW20(x) (assert(-Q_MAX2 <= (x) && (x) < Q_MAX), POW20core((x)+Q_MAX2))
#else
# define IPOW20(x)  (assert(-Q_MAX2 <= (x) && (x) < Q_MAX), ipow20[(x)+Q_MAX2])
/*#define IPOW20(x)  exp( -((double)(x)-210)*.1875*LOG2 ) */
/*#define IPOW20(x)  pow(2.0,-((double)(x)-210)*.1875) */

#define POW20(x) (assert(-Q_MAX2 <= (x) && (x) < Q_MAX), pow20[(x)+Q_MAX2])
/*#define POW20(x)  pow(2.0,((double)(x)-210)*.25) */
/*#define POW20(x)  exp( ((double)(x)-210)*(.25*LOG2) ) */

#endif

# define db2pow(x) (exp((x)*(LOG10*0.1)))
/*#define db2pow(x) pow(10.0, (x)*0.1) */
/*#define db2pow(x) pow(10.0, (x)/10.0) */

/* log/log10 approximations */
#ifdef USE_FAST_LOG
# define LOG2_SIZE_L2    (8)
# define LOG2_SIZE       (1 << LOG2_SIZE_L2)
# define FAST_LOG10(x)       (fast_log2(x)*(FLOAT)(LOG2/LOG10/(1<<23)))
# define FAST_LOG(x)         (fast_log2(x)*(FLOAT)LOG2/(1<<23))
# define FAST_LOG10_X(x,y)   (fast_log2(x)*(FLOAT)(LOG2/LOG10*(y)/(1<<23)))
# define FAST_LOG_X(x,y)     (fast_log2(x)*(FLOAT)(LOG2*(y)/(1<<23)))

# define FASTLOG fast_log2
# define FASTLOG_TO_10 (FLOAT)(LOG2/LOG10/(1<<23))

extern int log_table[LOG2_SIZE*2];
inline static int fast_log2(ieee754_float32_t xx)
{
    int mantisse, *p, i;
    fi_union x;

    x.f = xx;
    i = x.i;
    mantisse = i & 0x7FFF;
    p = (int*)((char*)log_table + ((i >> (23 - LOG2_SIZE_L2 - 3))
				   & (((1<<LOG2_SIZE_L2) - 1)*sizeof(int)*2)));
    return i + p[0] + ((unsigned int)(p[1] * mantisse) >> 16);
}
#else
# define         FAST_LOG10(x)       log10(x)
# define         FAST_LOG(x)         log(x)
# define         FAST_LOG10_X(x,y)   (log10(x)*(y))
# define         FAST_LOG_X(x,y)     (log(x)*(y))

# define         FASTLOG log10 /* really ? */
# define         FASTLOG_TO_10 (FLOAT)1.0
#endif

/***********************************************************************
*
*  Global Definitions
*
***********************************************************************/

#ifndef FALSE
#define         FALSE                   0
#endif

#ifndef TRUE
#define         TRUE                    (!FALSE)
#endif

#ifdef UINT_MAX
# define         MAX_U_32_NUM            UINT_MAX
#else
# define         MAX_U_32_NUM            0xFFFFFFFF
#endif

#ifndef PI
# ifdef M_PI
#  define       PI                      (FLOAT)M_PI
# else
#  define       PI                      (FLOAT)3.14159265358979323846
# endif
#endif

#ifdef M_LN2
# define        LOG2                    (FLOAT)M_LN2
#else
# define        LOG2                    (FLOAT)0.69314718055994530942
#endif

#ifdef M_LN10
# define        LOG10                   (FLOAT)M_LN10
#else
# define        LOG10                   (FLOAT)2.30258509299404568402
#endif

#ifdef M_SQRT2
# define        SQRT2                   (FLOAT)M_SQRT2
#else
# define        SQRT2                   (FLOAT)1.41421356237309504880
#endif

#define         Min(A, B)       ((A) < (B) ? (A) : (B))
#define         Max(A, B)       ((A) > (B) ? (A) : (B))

#endif

/* end of machine.h */
