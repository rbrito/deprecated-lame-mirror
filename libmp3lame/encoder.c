/*
 *	LAME MP3 encoding engine
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>

#include "lame.h"
#include "util.h"
#include "newmdct.h"
#include "psymodel.h"
#include "quantize.h"
#include "tables.h"
#include "bitstream.h"
#include "VbrTag.h"

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif


/* Robert Hegemann - 2002-10-24
 * sparsing of mid side channels
 * 2DO: replace mld by something with finer resolution
 */
static void
ms_sparsing(lame_internal_flags* gfc, int gr)
{
    int sfb, i, j = 0, width;

    for (sfb = 0; sfb < gfc->l3_side.tt[gr][0].sfbmax; ++sfb) {
	FLOAT threshold;
	width = gfc->l3_side.tt[gr][0].width[sfb];
	if (sfb < gfc->l3_side.tt[gr][0].sfb_lmax)
	    threshold
		= db2pow(-(gfc->sparseA - gfc->mld_l[sfb]*gfc->sparseB));
	else {
	    int sfb2 = (sfb - gfc->l3_side.tt[gr][0].sfb_lmax) / 3;
	    threshold
		= db2pow(-(gfc->sparseA - gfc->mld_s[sfb2]*gfc->sparseB));
	}

	i = j + width;
	do {
            FLOAT *m = gfc->l3_side.tt[gr][0].xr+j;
            FLOAT *s = gfc->l3_side.tt[gr][1].xr+j;
            FLOAT m02 = m[0] * m[0];
            FLOAT m12 = m[1] * m[1];
            FLOAT s02 = s[0] * s[0];
            FLOAT s12 = s[1] * s[1];
            if ( s02 < m02*threshold && s12 < m12*threshold ) { 
                m[0] += s[0]; s[0] = 0;
                m[1] += s[1]; s[1] = 0;
            }
            if ( m02 < s02*threshold && m12 < s12*threshold ) { 
                s[0] += m[0]; m[0] = 0;
                s[1] += m[1]; m[1] = 0;
            }
        } while ((j += 2) < i);
    }
}

static void
conv_istereo(lame_internal_flags* gfc, gr_info *gi, int sfb, int i)
{
    for (; i != 576; sfb++) {
	FLOAT lsum = 1e-30, rsum = 1e-30;
	int j = i + gi->width[sfb];
	do {
	    FLOAT l = gi[0].xr[i];
	    FLOAT r = gi[1].xr[i];
	    gi[0].xr[i] = l+r;
	    gi[1].xr[i] = 0.0;
	    lsum += fabs(l);
	    rsum += fabs(r);
	} while (++i < j);

	if (sfb > gi[0].psymax)
	    continue;
	lsum = lsum / (lsum+rsum);
	j = 3;
	if (lsum < 0.5) {
	    if (lsum < 0.211324865 * 0.5)
		j = 0;
	    else if (lsum < (0.366025404 + 0.211324865) * 0.5)
		j = 1;
	    else if (lsum < (0.5 + 0.366025404) * 0.5)
		j = 2;
	} else {
	    if (lsum > 1.0-0.211324865 * 0.5)
		j = 6;
	    else if (lsum > 1.0-(0.366025404 + 0.211324865) * 0.5)
		j = 5;
	    else if (lsum > 1.0-(0.5 + 0.366025404) * 0.5)
		j = 4;
	}
	gi[1].scalefac[sfb] = j;
    }
    gfc->scale_bitcounter(&gi[1]);
}


/***********************************************************************
 *
 *  some simple statistics
 *
 *  bitrate index 0: free bitrate -> not allowed in VBR mode
 *  : bitrates, kbps depending on MPEG version
 *  bitrate index 15: forbidden
 *
 *  mode_ext:
 *  0:  LR
 *  1:  LR-i
 *  2:  MS
 *  3:  MS-i
 *
 ***********************************************************************/

#ifdef BRHIST
/*2DO rh 20021015
I thought BRHIST was only for the frontend, so that clients
may use these stats, even if it's only a Windows DLL
I'll extend the stats for block types used
*/
static void
updateStats( lame_internal_flags * const gfc )
{
    int gr, ch;
    assert ( gfc->bitrate_index < 16u );
    assert ( gfc->mode_ext      <  4u );
    
    /* count bitrate indices */
    gfc->bitrate_stereoMode_Hist [gfc->bitrate_index] [4] ++;
    gfc->bitrate_stereoMode_Hist [15] [4] ++;
    
    /* count 'em for every mode extension in case of 2 channel encoding */
    if (gfc->channels_out == 2) {
        gfc->bitrate_stereoMode_Hist [gfc->bitrate_index] [gfc->mode_ext]++;
        gfc->bitrate_stereoMode_Hist [15] [gfc->mode_ext]++;
    }
    for (gr = 0; gr < gfc->mode_gr; ++gr) {
        for (ch = 0; ch < gfc->channels_out; ++ch) {
            int bt = gfc->l3_side.tt[gr][ch].block_type;
            int mf = gfc->l3_side.tt[gr][ch].mixed_block_flag;
            if (mf) bt = 4;
            gfc->bitrate_blockType_Hist [gfc->bitrate_index] [bt] ++;
	    gfc->bitrate_blockType_Hist [gfc->bitrate_index] [ 5] ++;
            gfc->bitrate_blockType_Hist [15] [bt] ++;
	    gfc->bitrate_blockType_Hist [15] [ 5] ++;
        }
    }
}
#endif


static void
init_gr_info(lame_internal_flags *gfc, int gr, int ch)
{
    int sfb, j;
    gr_info *gi = &gfc->l3_side.tt[gr][ch];

    gi->part2_3_length      = 0;
    gi->big_values          = 0;
    gi->count1              = 0;
    gi->global_gain         = 210;
    gi->scalefac_compress   = 0;
    /* mixed_block_flag, block_type was set in psymodel.c */
    gi->table_select [0]    = 0;
    gi->table_select [1]    = 0;
    gi->table_select [2]    = 0;
    gi->subblock_gain[0]    = 0;
    gi->subblock_gain[1]    = 0;
    gi->subblock_gain[2]    = 0;
    gi->subblock_gain[3]    = 0;    /* this one is always 0 */
    gi->region0_count       = 0;
    gi->region1_count       = 0;
    gi->preflag             = 0;
    gi->scalefac_scale      = 0;
    gi->count1table_select  = 0;
    gi->part2_length        = 0;
    gi->sfbdivide           = 11;
    j = gfc->cutoff_sfb_l;
    if (ch & 1)
	j = gfc->is_start_sfb_l[gr];
    gi->psymax = gi->psy_lmax = j;
    gi->sfbmax = gi->sfb_lmax = SBPSY_l;
    gi->sfb_smin              = SBPSY_s;
    for (sfb = 0; sfb < SBMAX_l; sfb++) {
	gi->width[sfb]
	    = gfc->scalefac_band.l[sfb+1] - gfc->scalefac_band.l[sfb];
	gi->window[sfb] = 3; /* subblockgain[3] is always 0. */
    }

    if (gi->block_type != NORM_TYPE) {
	gi->region0_count = 7;
	if (gi->block_type == SHORT_TYPE) {
	    FLOAT ixwork[576];
	    FLOAT *ix;

	    if (gfc->mode_gr == 1)
		gi->region0_count = 5;
	    gi->sfb_smin        = 0;
	    gi->sfb_lmax        = 0;
	    if (gi->mixed_block_flag) {
		/*
		 *  MPEG-1:      sfbs 0-7 long block, 3-12 short blocks 
		 *  MPEG-2(.5):  sfbs 0-5 long block, 3-12 short blocks
		 */ 
		gi->sfb_smin    = 3;
		gi->sfb_lmax    = gfc->mode_gr*2 + 4;
	    }
	    j = gfc->cutoff_sfb_s;
	    if (ch & 1)
		j = gfc->is_start_sfb_s[gr];
	    gi->psymax = gi->sfb_lmax + 3*(j - gi->sfb_smin);
	    gi->sfbmax = gi->sfb_lmax + 3*(SBPSY_s - gi->sfb_smin);
	    gi->sfbdivide   = gi->sfbmax - 18;
	    gi->psy_lmax    = gi->sfb_lmax;
	    /* re-order the short blocks, for more efficient encoding below */
	    /* By Takehiro TOMINAGA */
	    /*
	      Within each scalefactor band, data is given for successive
	      time windows, beginning with window 0 and ending with window 2.
	      Within each window, the quantized values are then arranged in
	      order of increasing frequency...
	    */
	    j = gi->sfb_lmax;
	    ix = &gi->xr[gfc->scalefac_band.l[j]];
	    memcpy(ixwork, gi->xr, sizeof(ixwork));
	    for (sfb = gi->sfb_smin; sfb < SBMAX_s; sfb++) {
		int start = gfc->scalefac_band.s[sfb];
		int end   = gfc->scalefac_band.s[sfb + 1];
		int window, l;
		for (window = 0; window < 3; window++) {
		    for (l = start; l < end; l++)
			*ix++ = ixwork[3*l+window];
		    gi->width [j] = end - start;
		    gi->window[j] = window;
		    j++;
		}
	    }
	}
	gi->region1_count = SBMAX_l - 2 - gi->region0_count;
    } else {
	/* analog silence detection in pseudo sfb 22 */
	if (gfc->scalefac_band.l[SBMAX_l-1] < 576-100) {
	    int j0 = (576+gfc->scalefac_band.l[SBMAX_l-1])/2, j;
	    FLOAT power = 0.0;
	    for (j = j0; j < 576; j++)
		power += gi->xr[j] * gi->xr[j];
	    if (power < gfc->ATH.adjust * gfc->ATH.l[SBMAX_l-1]) {
		for (j = j0; j < 576; j++)
		    gi->xr[j] = 0;
	    }
	}
    }
    gi->count1bits          = 0;
    gi->slen[0]             = 0;
    gi->slen[1]             = 0;
    gi->slen[2]             = 0;
    gi->slen[3]             = 0;
    memset(gi->scalefac, 0, sizeof(gi->scalefac));
}



/************************************************************************
*
* encodeframe()           Layer 3
*
* encode a single frame
*
************************************************************************
lame_encode_frame()


                       gr 0            gr 1
inbuf:           |--------------|---------------|-------------|
MDCT output:  |--------------|---------------|-------------|

FFT's                    <---------1024---------->
                                         <---------1024-------->



    inbuf = buffer of PCM data size=MP3 framesize
    encoder acts on inbuf[ch][0], but output is delayed by MDCTDELAY
    so the MDCT coefficints are from inbuf[ch][-MDCTDELAY]

    psy-model FFT has a 1 granule delay, so we feed it data for the 
    next granule.
    FFT is centered over granule:  224+576+224
    So FFT starts at:   576-224-MDCTDELAY

    MPEG2:  FFT ends at:  BLKSIZE+576-224-MDCTDELAY
    MPEG1:  FFT ends at:  BLKSIZE+2*576-224-MDCTDELAY    (1904)

    FFT starts at 576-224-MDCTDELAY (304)  = 576-FFTOFFSET

*/

int  lame_encode_mp3_frame (				/* Output */
	lame_global_flags* const  gfp,			/* Context */
	sample_t*                 inbuf_l,              /* Input */
	sample_t*                 inbuf_r,              /* Input */
	unsigned char*            mp3buf, 		/* Output */
	int                    mp3buf_size )		/* Output */
{
    int mp3count;
    III_psy_ratio masking[2][2];  /*pointer to selected maskings*/
    const sample_t *inbuf[2];
    lame_internal_flags *gfc=gfp->internal_flags;
    FLOAT sbsmpl[MAX_CHANNELS][2*1152];

    int ch,gr;

    if (!gfc->lame_encode_frame_init) {
	/* prime the MDCT/polyphase filterbank with a short block */
	int i,j;
	sample_t primebuff[2][286+1152+576];
	memset(gfc->sb_sample, 0, sizeof(gfc->sb_sample));
	gfc->lame_encode_frame_init = 1;
	for (i=0, j=0; i<286+576*(1+gfc->mode_gr); ++i) {
	    if (i<576*gfc->mode_gr) {
		primebuff[0][i]=0;
		primebuff[1][i]=0;
	    }else{
		primebuff[0][i]=inbuf_l[j];
		if (gfc->channels_out==2) 
		    primebuff[1][i]=inbuf_r[j];
		++j;
	    }
	}
	/* polyphase filtering / mdct */
	for ( ch = 0; ch < gfc->channels_out; ch++ ) {
	    inbuf[ch] = primebuff[ch] - gfp->framesize + 286;
	    for ( gr = 0; gr < gfc->mode_gr; gr++ ) {
		gfc->l3_side.tt[gr][ch].block_type = NORM_TYPE;
	    }
	    subband(gfc, inbuf[ch], sbsmpl[ch]);
	    memcpy(gfc->sb_sample[ch][1], sbsmpl,
		   sizeof(gfc->sb_sample[ch][0])*gfc->mode_gr);
	    memset(gfc->sb_sample[ch][0], 0, sizeof(gfc->sb_sample[ch][0]));
	    mdct_sub48(gfc, ch);
	}

	/* check FFT will not use a negative starting offset */
#if 576 < FFTOFFSET
# error FFTOFFSET greater than 576: FFT uses a negative offset
#endif
	/* check if we have enough data for FFT */
	assert(gfc->mf_size>=(BLKSIZE+gfp->framesize*2-FFTOFFSET));
	/* check if we have enough data for polyphase filterbank */
	/* it needs 1152 samples + 286 samples ignored for one granule */
	/*          1152+576+286 samples for two granules */
	assert(gfc->mf_size>=(286+576*(1+gfc->mode_gr)));

	if (gfc->psymodel)
	    psycho_analysis(gfp, inbuf, masking, sbsmpl);
    }

    /********************** padding *****************************/
    /* padding method as described in 
     * "MPEG-Layer3 / Bitstream Syntax and Decoding"
     * by Martin Sieler, Ralph Sperschneider
     *
     * note: there is no padding for the very first frame
     *
     * Robert.Hegemann@gmx.de 2000-06-22
     */
    gfc->padding = FALSE;
    if ((gfc->slot_lag -= gfc->frac_SpF) < 0) {
	gfc->slot_lag += gfp->out_samplerate;
	gfc->padding = TRUE;
    }

    inbuf[0]=inbuf_l;
    inbuf[1]=inbuf_r;

    /* subband filtering in the next frame */
    /* to determine long/short swithcing in psymodel */
    for ( ch = 0; ch < gfc->channels_out; ch++ )
	subband(gfc, inbuf[ch], sbsmpl[ch]);

    if (gfc->psymodel)
	psycho_analysis(gfp, inbuf, masking, sbsmpl);
    else
	memset(masking, 0, sizeof(masking));

    /* polyphase filtering / mdct */
    for (ch = 0; ch < gfc->channels_out; ch++) {
	mdct_sub48(gfc, ch);
	/* aging subband filetr output */
	memcpy(gfc->sb_sample[ch][0], gfc->sb_sample[ch][gfc->mode_gr],
	       sizeof(gfc->sb_sample[ch][0]));
	memcpy(gfc->sb_sample[ch][1], sbsmpl[ch],
	       sizeof(gfc->sb_sample[ch][0])*gfc->mode_gr);

	for (gr = 0; gr < gfc->mode_gr; gr++)
	    init_gr_info(gfc, gr, ch);
    }

    /* channel conversion */
    if (gfc->narrowStereo != 0.0) {
	/* narrown_stereo */
	for (gr = 0; gr < gfc->mode_gr; gr++) {
	    gr_info *gi = &gfc->l3_side.tt[gr][0];
	    int i;
	    for (i = 0; i < 576; i++) {
		FLOAT d = (gi[0].xr[i]-gi[1].xr[i]) * gfc->narrowStereo;
		gi[0].xr[i] -= d;
		gi[1].xr[i] += d;
	    }
	}
    }

    if (gfc->mode_ext & MPG_MD_MS_LR) {
	/* convert from L/R -> Mid/Side */
	if (gfc->mode_ext == MPG_MD_MS_I) {
	    int i = 0;
	    for (gr = 0; gr < gfc->mode_gr; gr++) {
		if (gfc->l3_side.tt[gr][0].block_type == SHORT_TYPE)
		    i += gfc->is_start_sfb_s[gr];
		else
		    i += gfc->is_start_sfb_l[gr];
	    }
	    if (i == 0)
		gfc->mode_ext = MPG_MD_MS_LR;
	}

	for (gr = 0; gr < gfc->mode_gr; gr++) {
	    gr_info *gi = &gfc->l3_side.tt[gr][0];
	    int sfb = gfc->is_start_sfb_l[gr];
	    int end = gfc->scalefac_band.l[sfb];
	    int i;
	    if (gi->block_type == SHORT_TYPE) {
		sfb = gfc->is_start_sfb_s[gr];
		end = gfc->scalefac_band.s[sfb]*3;
		sfb *= 3;
	    }
	    for (i = 0; i < end; i++) {
		FLOAT l = gi[0].xr[i];
		FLOAT r = gi[1].xr[i];
		gi[0].xr[i] = (l+r) * (FLOAT)(SQRT2*0.5);
		gi[1].xr[i] = (l-r) * (FLOAT)(SQRT2*0.5);
	    }
	    if (gfc->mode_ext & 1)
		conv_istereo(gfc, gi, sfb, end);
	    if (gfc->sparsing)
		ms_sparsing(gfc, gr);
	}
    } else if (gfc->mode_ext & 1) {
	for (gr = 0; gr < gfc->mode_gr; gr++) {
	    gr_info *gi = &gfc->l3_side.tt[gr][0];
	    int sfb = gfc->is_start_sfb_l[gr];
	    int end = gfc->scalefac_band.l[sfb];
	    if (gi->block_type == SHORT_TYPE) {
		sfb = gfc->is_start_sfb_s[gr];
		end = gfc->scalefac_band.s[sfb]*3;
		sfb *= 3;
	    }
	    conv_istereo(gfc, gi, sfb, end);
	}
    }

#ifdef HAVE_GTK
    /* copy data for MP3 frame analyzer */
    if (gfc->pinfo) {
	for ( gr = 0; gr < gfc->mode_gr; gr++ ) {
	    gfc->pinfo->ms_ener_ratio[gr] = 0.0;
	    for ( ch = 0; ch < gfc->channels_out; ch++ ) {
		gfc->pinfo->blocktype[gr][ch]
		    =gfc->l3_side.tt[gr][ch].block_type;
		gfc->pinfo->pe[gr][ch]=masking[gr][ch].pe;
		memcpy(gfc->pinfo->xr[gr][ch], &gfc->l3_side.tt[gr][ch].xr,
		       sizeof(gfc->pinfo->xr[gr][ch]));
	    }
	    if (gfp->mode == JOINT_STEREO) {
		gfc->pinfo->ms_ratio[gr] = gfc->pinfo->ms_ratio_next[gr];
		gfc->pinfo->ms_ratio_next[gr]
		    = gfc->masking_next[gr][2].pe
		    + gfc->masking_next[gr][3].pe
		    - gfc->masking_next[gr][0].pe
		    - gfc->masking_next[gr][1].pe;
	    }
	}
	for ( ch = 0; ch < gfc->channels_out; ch++ ) {
	    int j;
	    for ( j = 0; j < FFTOFFSET; j++ )
		gfc->pinfo->pcmdata[ch][j]
		    = gfc->pinfo->pcmdata[ch][j+gfp->framesize];
	    for ( j = FFTOFFSET; j < 1600; j++ ) {
		gfc->pinfo->pcmdata[ch][j] = inbuf[ch][j-FFTOFFSET];
	    }
	}
    }
#endif

    /* bit and noise allocation */
    switch (gfp->VBR){ 
    default:
    case cbr:
	iteration_loop(gfp, masking);
	break;
    case vbr:
	VBR_iteration_loop(gfp, masking);
	break;
    case abr:
	ABR_iteration_loop(gfp, masking);
	break;
    }

    /*  write the frame to the bitstream  */
    format_bitstream(gfp);

    /* copy mp3 bit buffer into array */
    mp3count = copy_buffer(gfc,mp3buf,mp3buf_size,1);

    if (gfp->bWriteVbrTag) AddVbrFrame(gfp);

#if defined(HAVE_GTK)
    if (gfc->pinfo)
	set_frame_pinfo (gfc, masking);
#endif

#ifdef BRHIST
    updateStats( gfc );
#endif

  return mp3count;
}
