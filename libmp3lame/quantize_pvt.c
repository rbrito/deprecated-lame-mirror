/*
 *	quantize_pvt source file
 *
 *	Copyright (c) 1999 Takehiro TOMINAGA
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
#include "lame-analysis.h"
#include "tables.h"
#include "reservoir.h"
#include "quantize_pvt.h"

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif


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
    calc_noise (gfc, cod_info, l3_xmin, xfsf, &noise);

    j = 0;
    sfb2 = cod_info->sfb_lmax;
    if (cod_info->block_type != SHORT_TYPE && !cod_info->mixed_block_flag)
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

    gfc->masking_lower = 1.0;

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

