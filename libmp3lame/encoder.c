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
    int sfb, i, j = 0;
    int width;

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
init_gr_info(
    lame_internal_flags *gfc,
    gr_info *const cod_info)
{
    int sfb, j;
    /*  initialize fresh cod_info
     */
    cod_info->part2_3_length      = 0;
    cod_info->big_values          = 0;
    cod_info->count1              = 0;
    cod_info->global_gain         = 210;
    cod_info->scalefac_compress   = 0;
    /* mixed_block_flag, block_type was set in psymodel.c */
    cod_info->table_select [0]    = 0;
    cod_info->table_select [1]    = 0;
    cod_info->table_select [2]    = 0;
    cod_info->subblock_gain[0]    = 0;
    cod_info->subblock_gain[1]    = 0;
    cod_info->subblock_gain[2]    = 0;
    cod_info->subblock_gain[3]    = 0;    // this one is always 0
    cod_info->region0_count       = 0;
    cod_info->region1_count       = 0;
    cod_info->preflag             = 0;
    cod_info->scalefac_scale      = 0;
    cod_info->count1table_select  = 0;
    cod_info->part2_length        = 0;
    cod_info->sfb_lmax        = SBPSY_l;
    cod_info->sfb_smin        = SBPSY_s;
    cod_info->psy_lmax        = gfc->sfb21_extra ? SBMAX_l : SBPSY_l;
    cod_info->psymax          = cod_info->psy_lmax;
    cod_info->sfbmax          = cod_info->sfb_lmax;
    cod_info->sfbdivide       = 11;
    for (sfb = 0; sfb < SBMAX_l; sfb++) {
	cod_info->width[sfb]
	    = gfc->scalefac_band.l[sfb+1] - gfc->scalefac_band.l[sfb];
	cod_info->window[sfb] = 3; // which is always 0.
    }

    if (cod_info->block_type != NORM_TYPE) {
	cod_info->region0_count = 7;
	cod_info->region1_count = SBMAX_l -1 - 7 - 1;
	if (cod_info->block_type == SHORT_TYPE) {
	    FLOAT ixwork[576];
	    FLOAT *ix;

	    if (gfc->mode_gr == 1)
		cod_info->region0_count = 5;
	    cod_info->sfb_smin        = 0;
	    cod_info->sfb_lmax        = 0;
	    if (cod_info->mixed_block_flag) {
		/*
		 *  MPEG-1:      sfbs 0-7 long block, 3-12 short blocks 
		 *  MPEG-2(.5):  sfbs 0-5 long block, 3-12 short blocks
		 */ 
		cod_info->sfb_smin    = 3;
		cod_info->sfb_lmax    = gfc->mode_gr*2 + 4;
	    }
	    cod_info->psymax
		= cod_info->sfb_lmax
		+ 3*((gfc->sfb21_extra ? SBMAX_s : SBPSY_s) - cod_info->sfb_smin);
	    cod_info->sfbmax
		= cod_info->sfb_lmax + 3*(SBPSY_s - cod_info->sfb_smin);
	    cod_info->sfbdivide   = cod_info->sfbmax - 18;
	    cod_info->psy_lmax    = cod_info->sfb_lmax;
	    /* re-order the short blocks, for more efficient encoding below */
	    /* By Takehiro TOMINAGA */
	    /*
	      Within each scalefactor band, data is given for successive
	      time windows, beginning with window 0 and ending with window 2.
	      Within each window, the quantized values are then arranged in
	      order of increasing frequency...
	    */
	    j = cod_info->sfb_lmax;
	    ix = &cod_info->xr[gfc->scalefac_band.l[j]];
	    memcpy(ixwork, cod_info->xr, sizeof(ixwork));
	    for (sfb = cod_info->sfb_smin; sfb < SBMAX_s; sfb++) {
		int start = gfc->scalefac_band.s[sfb];
		int end   = gfc->scalefac_band.s[sfb + 1];
		int window, l;
		cod_info->width[j] = cod_info->width[j+1] = cod_info->width[j + 2]
		    = end - start;
		cod_info->window[j  ] = 0;
		cod_info->window[j+1] = 1;
		cod_info->window[j+2] = 2;
		j += 3;
		for (window = 0; window < 3; window++) {
		    for (l = start; l < end; l++) {
			*ix++ = ixwork[3*l+window];
		    }
		}
	    }
	}
    }
    cod_info->count1bits          = 0;  
    cod_info->sfb_partition_table = nr_of_sfb_block[0][0];
    cod_info->slen[0]             = 0;
    cod_info->slen[1]             = 0;
    cod_info->slen[2]             = 0;
    cod_info->slen[3]             = 0;

    /*  fresh scalefactors are all zero
     */
    memset(cod_info->scalefac, 0, sizeof(cod_info->scalefac));
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

typedef FLOAT chgrdata[2][2];

int  lame_encode_mp3_frame (				// Output
	lame_global_flags* const  gfp,			// Context
	sample_t*                 inbuf_l,              // Input
	sample_t*                 inbuf_r,              // Input
	unsigned char*            mp3buf, 		// Output
	int                    mp3buf_size )		// Output
{
    int mp3count;
    III_psy_ratio masking[2][2];  /*pointer to selected maskings*/
    const sample_t *inbuf[2];
    lame_internal_flags *gfc=gfp->internal_flags;
    FLOAT ms_ener_ratio[2];
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
	    for ( gr = 0; gr < gfc->mode_gr; gr++ ) {
		gfc->l3_side.tt[gr][ch].block_type = NORM_TYPE;
	    }
	    subband(gfc, primebuff[ch]-1152, sbsmpl[ch]);
	    memcpy(gfc->sb_sample[ch][1], sbsmpl,
		   sizeof(gfc->sb_sample[ch][0])*gfc->mode_gr);
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

	if (gfc->psymodel) {
	    inbuf[0]=primebuff[0] - gfp->framesize;
	    inbuf[1]=primebuff[1] - gfp->framesize;
	    psycho_analysis(gfp, inbuf, ms_ener_ratio, masking, sbsmpl);
	}
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

    if (gfc->psymodel) {
#ifdef HAVE_GTK
	if (gfc->pinfo)
	    memcpy(gfc->pinfo->energy, gfc->energy_save,
		   sizeof(gfc->energy_save));
#endif
	psycho_analysis(gfp, inbuf, ms_ener_ratio, masking, sbsmpl);
    } else {
	memset(masking, 0, sizeof(masking));
	for (gr=0; gr < gfc->mode_gr ; gr++)
	    for ( ch = 0; ch < gfc->channels_out; ch++ ) {
		gfc->l3_side.tt[gr][ch].block_type=NORM_TYPE;
		gfc->l3_side.tt[gr][ch].mixed_block_flag=0;
		masking[gr][ch].pe = 700.0;
	    }
	gfc->ATH.adjust = 1.0;	/* no adjustment */
	gfc->mode_ext = MPG_MD_LR_LR;
	if (gfp->mode == JOINT_STEREO)
	    gfc->mode_ext = MPG_MD_MS_LR;
    }

    /* polyphase filtering / mdct */
    for ( ch = 0; ch < gfc->channels_out; ch++ ) {
	mdct_sub48(gfc, ch);
	/* aging subband filetr output */
	memcpy(gfc->sb_sample[ch][0], gfc->sb_sample[ch][gfc->mode_gr],
	       sizeof(gfc->sb_sample[ch][0]));
	memcpy(gfc->sb_sample[ch][1], sbsmpl[ch],
	       sizeof(gfc->sb_sample[ch][0])*gfc->mode_gr);

	for (gr = 0; gr < gfc->mode_gr; gr++)
	    init_gr_info(gfc, &gfc->l3_side.tt[gr][ch]);
    }

    /* bit and noise allocation */
    if (gfc->mode_ext & MPG_MD_MS_LR) {
	/* convert from L/R <-> Mid/Side */
	for (gr = 0; gr < gfc->mode_gr; gr++) {
	    int i;
	    for (i = 0; i < 576; ++i) {
		FLOAT l, r;
		l = gfc->l3_side.tt[gr][0].xr[i];
		r = gfc->l3_side.tt[gr][1].xr[i];
		gfc->l3_side.tt[gr][0].xr[i] = (l+r) * (FLOAT)(SQRT2*0.5);
		gfc->l3_side.tt[gr][1].xr[i] = (l-r) * (FLOAT)(SQRT2*0.5);
	    }
	    if (gfc->sparsing)
		ms_sparsing(gfc, gr);
	}
    }
#if defined(HAVE_GTK)
    /* copy data for MP3 frame analyzer */
    if (gfc->pinfo) {
	for ( gr = 0; gr < gfc->mode_gr; gr++ ) {
	    gfc->pinfo->ms_ener_ratio[gr] = ms_ener_ratio[gr];
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

    if (gfp->VBR != vbr) {
	static const FLOAT fircoef[9] = {
	    -0.0207887 *5,	-0.0378413*5,	-0.0432472*5,	-0.031183*5,
	    7.79609e-18*5,	 0.0467745*5,	 0.10091*5,	0.151365*5,
	    0.187098*5
	};
	int i;
	FLOAT f = 0.0;
	for (i=0;i<18;i++)
	    gfc->nsPsy.pefirbuf[i] = gfc->nsPsy.pefirbuf[i+1];
	for (gr = 0; gr < gfc->mode_gr; gr++)
	    for ( ch = 0; ch < gfc->channels_out; ch++ )
		f += masking[gr][ch].pe;
	gfc->nsPsy.pefirbuf[18] = f;

	f = gfc->nsPsy.pefirbuf[9];
	for (i=0;i<9;i++)
	    f += (gfc->nsPsy.pefirbuf[i]+gfc->nsPsy.pefirbuf[18-i])
		* fircoef[i];

	f = (670*5*gfc->mode_gr*gfc->channels_out)/f;
	for ( gr = 0; gr < gfc->mode_gr; gr++ ) {
	    for ( ch = 0; ch < gfc->channels_out; ch++ ) {
		masking[gr][ch].pe *= f;
	    }
	}
    }

    switch (gfp->VBR){ 
    default:
    case cbr:
	iteration_loop( gfp, ms_ener_ratio, masking);
	break;
    case vbr:
	VBR_iteration_loop( gfp, masking);
	break;
    case abr:
	ABR_iteration_loop( gfp, ms_ener_ratio, masking);
	break;
    }

    /*  write the frame to the bitstream  */
    format_bitstream(gfp);

    /* copy mp3 bit buffer into array */
    mp3count = copy_buffer(gfc,mp3buf,mp3buf_size,1);

    if (gfp->bWriteVbrTag) AddVbrFrame(gfp);

#if defined(HAVE_GTK)
    if (gfc->pinfo != NULL)
	set_frame_pinfo (gfp, masking);
#endif

#ifdef BRHIST
    updateStats( gfc );
#endif

  return mp3count;
}
