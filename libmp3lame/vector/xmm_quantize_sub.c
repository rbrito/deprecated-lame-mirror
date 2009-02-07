/*
 * MP3 quantization, intrinsics functions
 *
 *      Copyright (c) 2005-2006 Gabriel Bouvigne
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.     See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "lame.h"
#include "machine.h"
#include "encoder.h"
#include "util.h"
#include "lame_intrin.h"



#ifdef HAVE_XMMINTRIN_H

#include <xmmintrin.h>

typedef union {
    int32_t _i_32[4]; /* unions are initialized by its first member */
    float   _float[4];
    __m128  _m128;
} vecfloat_union;


void
init_xrpow_core_sse(gr_info * const cod_info, FLOAT xrpow[576], int upper, FLOAT * sum)
{
    int     i;
    float   tmp_max = 0;
    float   tmp_sum = 0;
    int     upper4 = (upper / 4) * 4;
    int     rest = upper-upper4;

    const vecfloat_union fabs_mask = {{ 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF }};
    const __m128 vec_fabs_mask = _mm_loadu_ps(&fabs_mask._float[0]);
    vecfloat_union vec_xrpow_max;
    vecfloat_union vec_sum;
    vecfloat_union vec_tmp;

    _mm_prefetch((char *) cod_info->xr, _MM_HINT_T0);
    _mm_prefetch((char *) xrpow, _MM_HINT_T0);

    vec_xrpow_max._m128 = _mm_set_ps1(0);
    vec_sum._m128 = _mm_set_ps1(0);

    for (i = 0; i < upper4; i += 4) {
        vec_tmp._m128 = _mm_loadu_ps(&(cod_info->xr[i])); /* load */
        vec_tmp._m128 = _mm_and_ps(vec_tmp._m128, vec_fabs_mask); /* fabs */
        vec_sum._m128 = _mm_add_ps(vec_sum._m128, vec_tmp._m128);
        vec_tmp._m128 = _mm_sqrt_ps(_mm_mul_ps(vec_tmp._m128, _mm_sqrt_ps(vec_tmp._m128)));
        vec_xrpow_max._m128 = _mm_max_ps(vec_xrpow_max._m128, vec_tmp._m128); /* retrieve max */
        _mm_storeu_ps(&(xrpow[i]), vec_tmp._m128); /* store into xrpow[] */
    }
    vec_tmp._m128 = _mm_set_ps1(0);
    switch (rest) {
        case 3: vec_tmp._float[2] = cod_info->xr[upper4+2];
        case 2: vec_tmp._float[1] = cod_info->xr[upper4+1];
        case 1: vec_tmp._float[0] = cod_info->xr[upper4+0];
            vec_tmp._m128 = _mm_and_ps(vec_tmp._m128, vec_fabs_mask); /* fabs */
            vec_sum._m128 = _mm_add_ps(vec_sum._m128, vec_tmp._m128);
            vec_tmp._m128 = _mm_sqrt_ps(_mm_mul_ps(vec_tmp._m128, _mm_sqrt_ps(vec_tmp._m128)));
            vec_xrpow_max._m128 = _mm_max_ps(vec_xrpow_max._m128, vec_tmp._m128); /* retrieve max */
            switch (rest) {
                case 3: xrpow[upper4+2] = vec_tmp._float[2];
                case 2: xrpow[upper4+1] = vec_tmp._float[1];
                case 1: xrpow[upper4+0] = vec_tmp._float[0];
                default:
                    break;
            }
        default:
            break;
    }
    tmp_sum = vec_sum._float[0] + vec_sum._float[1] + vec_sum._float[2] + vec_sum._float[3];
    {
        float ma = vec_xrpow_max._float[0] > vec_xrpow_max._float[1]
                ? vec_xrpow_max._float[0] : vec_xrpow_max._float[1];
        float mb = vec_xrpow_max._float[2] > vec_xrpow_max._float[3]
                ? vec_xrpow_max._float[2] : vec_xrpow_max._float[3];
        tmp_max = ma > mb ? ma : mb;
    }
    cod_info->xrpow_max = tmp_max;
    *sum = tmp_sum;
}

#endif	/* HAVE_XMMINTRIN_H */

