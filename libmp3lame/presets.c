/*
 * presets.c -- Apply presets
 *
 * Copyright (C) 2002 Gabriel Bouvigne / Lame project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include "lame.h"

int apply_preset(lame_global_flags*  gfp, int preset)
{
    switch (preset) {
    case STANDARD: {
                lame_set_VBR(gfp, vbr_rh);

                lame_set_preset_expopts(gfp, 3);
                lame_set_quality(gfp, 2);
                lame_set_lowpassfreq(gfp, 19000);
                lame_set_mode(gfp, JOINT_STEREO);
                lame_set_VBR_min_bitrate_kbps(gfp, 128);
                return preset;
           }
    case STANDARD_FAST: {
                lame_set_VBR(gfp, vbr_mtrh);

                lame_set_preset_expopts(gfp, 3);
                lame_set_quality(gfp, 2);
                lame_set_lowpassfreq(gfp, 19000);
                lame_set_mode(gfp, JOINT_STEREO);
                lame_set_VBR_min_bitrate_kbps(gfp, 128);
                return preset;
           }
    case EXTREME: {
                lame_set_VBR(gfp, vbr_rh);

                lame_set_preset_expopts(gfp, 2);
                lame_set_quality(gfp, 2);
                lame_set_lowpassfreq(gfp, 19500);
                lame_set_mode(gfp, JOINT_STEREO);
                lame_set_VBR_min_bitrate_kbps(gfp, 128);					
                return preset;
           }
    case EXTREME_FAST: {
                lame_set_VBR(gfp, vbr_mtrh);

                lame_set_preset_expopts(gfp, 2);
                lame_set_quality(gfp, 2);
                lame_set_lowpassfreq(gfp, 19500);
                lame_set_mode(gfp, JOINT_STEREO);
                lame_set_VBR_min_bitrate_kbps(gfp, 128);					
                return preset;
           }
    case INSANE: {
                lame_set_preset_expopts(gfp, 1);
                lame_set_brate(gfp, 320);
                lame_set_quality(gfp, 2);
                lame_set_mode(gfp, JOINT_STEREO);
                lame_set_lowpassfreq(gfp, 20500);
                return preset;
           }
    case R3MIX: {
                lame_set_exp_nspsytune(gfp, lame_get_exp_nspsytune(gfp) | 1); /*nspsytune*/
                /*  lame_set_experimentalX(gfp,1); (test CVS) */

                (void) lame_set_scale( gfp, 0.98 ); /* --scale 0.98*/

                lame_set_exp_nspsytune(gfp, lame_get_exp_nspsytune(gfp) | (8 << 20));

                lame_set_VBR(gfp,vbr_mtrh); 
                lame_set_VBR_q(gfp,1);
                lame_set_quality( gfp, 2 );
                lame_set_lowpassfreq(gfp,19500);
                lame_set_mode( gfp, JOINT_STEREO );
                lame_set_ATHtype( gfp, 3 );
                lame_set_VBR_min_bitrate_kbps(gfp,96);
                return preset;
           }
    default: return 0;
    };


    return preset;
}
