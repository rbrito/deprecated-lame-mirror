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
#include "quantize_pvt.h"
#include "bitstream.h"
#include "VbrTag.h"
#include "vbrquantize.h"

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
		= pow(10.0, -(gfc->sparseA - gfc->mld_l[sfb]*gfc->sparseB)/10.);
	else {
	    int sfb2 = (sfb - gfc->l3_side.tt[gr][0].sfb_lmax) / 3;
	    threshold
		= pow(10.0, -(gfc->sparseA - gfc->mld_s[sfb2]*gfc->sparseB)/10.);
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



/*
 * auto-adjust of ATH, useful for low volume
 * Gabriel Bouvigne 3 feb 2001
 *
 * modifies some values in
 *   gfp->internal_flags->ATH
 *   (gfc->ATH)
 */
static void
adjust_ATH( lame_global_flags* const  gfp,
            FLOAT              tot_ener[2][4] )
{
    lame_internal_flags* const  gfc = gfp->internal_flags;
    int gr, channel;
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
        for ( gr = 0; gr < gfc->mode_gr; ++gr ) 
            for ( channel = 0; channel < gfc->channels_out; ++channel ) 
                max_pow = Max( max_pow, tot_ener[gr][channel] );
        max_pow *= 0.25/ 5.6e13; /* scale to 0..1 (5.6e13), and tune (0.25) */
        break;
    
    case 2:                     /* jd - 2001 mar 12, 27, jun 30 */
    {				/* loudness based on equal loudness curve; */
                                /* use granule with maximum combined loudness*/
	FLOAT gr2_max = gfc->loudness_sq[1][0];
        max_pow = gfc->loudness_sq[0][0];
        if( gfc->channels_out == 2 ) {
            max_pow += gfc->loudness_sq[0][1];
	    gr2_max += gfc->loudness_sq[1][1];
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
    if (gfc->presetTune.use)
        max_pow_alt *= gfc->presetTune.athadjust_safe_athaasensitivity;

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
init_outer_loop(
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
    if (cod_info->block_type == SHORT_TYPE) {
	FLOAT ixwork[576];
	FLOAT *ix;

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
	ix = &cod_info->xr[gfc->scalefac_band.l[cod_info->sfb_lmax]];
	memcpy(ixwork, cod_info->xr, 576*sizeof(FLOAT));
	for (sfb = cod_info->sfb_smin; sfb < SBMAX_s; sfb++) {
	    int start = gfc->scalefac_band.s[sfb];
	    int end   = gfc->scalefac_band.s[sfb + 1];
	    int window, l;
	    for (window = 0; window < 3; window++) {
		for (l = start; l < end; l++) {
		    *ix++ = ixwork[3*l+window];
		}
	    }
	}

	j = cod_info->sfb_lmax;
	for (sfb = cod_info->sfb_smin; sfb < SBMAX_s; sfb++) {
	    cod_info->width[j] = cod_info->width[j+1] = cod_info->width[j + 2]
		= gfc->scalefac_band.s[sfb+1] - gfc->scalefac_band.s[sfb];
	    cod_info->window[j  ] = 0;
	    cod_info->window[j+1] = 1;
	    cod_info->window[j+2] = 2;
	    j += 3;
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
  III_psy_ratio masking_LR[2][2];    /*LR masking & energy */
  III_psy_ratio masking_MS[2][2]; /*MS masking & energy */
  III_psy_ratio (*masking)[2][2];  /*pointer to selected maskings*/
  const sample_t *inbuf[2];
  lame_internal_flags *gfc=gfp->internal_flags;

  FLOAT tot_ener[2][4];
  FLOAT ms_ener_ratio[2]={.5,.5};
  chgrdata pe,pe_MS;
  chgrdata *pe_use;

  int ch,gr;

  inbuf[0]=inbuf_l;
  inbuf[1]=inbuf_r;

  if (gfc->lame_encode_frame_init==0 ) {
      /* prime the MDCT/polyphase filterbank with a short block */
      int i,j;
      sample_t primebuff0[286+1152+576];
      sample_t primebuff1[286+1152+576];
      gfc->lame_encode_frame_init=1;
      for (i=0, j=0; i<286+576*(1+gfc->mode_gr); ++i) {
	  if (i<576*gfc->mode_gr) {
	      primebuff0[i]=0;
	      if (gfc->channels_out==2) 
		  primebuff1[i]=0;
	  }else{
	      primebuff0[i]=inbuf[0][j];
	      if (gfc->channels_out==2) 
		  primebuff1[i]=inbuf[1][j];
	      ++j;
	  }
      }
      /* polyphase filtering / mdct */
      for ( gr = 0; gr < gfc->mode_gr; gr++ ) {
	  for ( ch = 0; ch < gfc->channels_out; ch++ ) {
	      gfc->l3_side.tt[gr][ch].block_type=SHORT_TYPE;
	  }
      }
      mdct_sub48(gfc, primebuff0, 0);
      mdct_sub48(gfc, primebuff1, 1);

      /* check FFT will not use a negative starting offset */
#if 576 < FFTOFFSET
# error FFTOFFSET greater than 576: FFT uses a negative offset
#endif
      /* check if we have enough data for FFT */
      assert(gfc->mf_size>=(BLKSIZE+gfp->framesize-FFTOFFSET));
      /* check if we have enough data for polyphase filterbank */
      /* it needs 1152 samples + 286 samples ignored for one granule */
      /*          1152+576+286 samples for two granules */
      assert(gfc->mf_size>=(286+576*(1+gfc->mode_gr)));
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


  if (gfc->psymodel) {
    /* psychoacoustic model
     * psy model has a 1 granule (576) delay that we must compensate for
     * (mt 6/99).
     */
    const sample_t *bufp[2]; /* address of beginning of left & right granule */
    int blocktype[2];

    for (gr=0; gr < gfc->mode_gr ; gr++) {
	for (ch = 0; ch < gfc->channels_out; ch++)
	    bufp[ch] = &inbuf[ch][576 + gr*576-FFTOFFSET];

	L3psycho_anal_ns( gfp, bufp, gr, 
			  masking_LR, masking_MS,
			  pe[gr],pe_MS[gr],tot_ener[gr],blocktype);

	if (gfp->mode == JOINT_STEREO) {
	    ms_ener_ratio[gr] = tot_ener[gr][2]+tot_ener[gr][3];
	    if (ms_ener_ratio[gr]>0)
		ms_ener_ratio[gr] = tot_ener[gr][3]/ms_ener_ratio[gr];
	}

	/* block type flags */
	for ( ch = 0; ch < gfc->channels_out; ch++ ) {
	    gr_info *cod_info = &gfc->l3_side.tt[gr][ch];
	    cod_info->block_type=blocktype[ch];
	    cod_info->mixed_block_flag = 0;
	    if (cod_info->block_type != NORM_TYPE) {
		if (gfp->mixed_blocks == 1
		    || (gfp->mixed_blocks == 2 && cod_info->block_type < 0))
		    cod_info->mixed_block_flag = 1;

		if (cod_info->block_type < 0)
		    cod_info->block_type = -cod_info->block_type;
	    }
	}
    }
  }else{
    memset((char *) masking_LR, 0, sizeof(masking_LR));
    memset((char *) masking_MS, 0, sizeof(masking_MS));
    for (gr=0; gr < gfc->mode_gr ; gr++)
      for ( ch = 0; ch < gfc->channels_out; ch++ ) {
	gfc->l3_side.tt[gr][ch].block_type=NORM_TYPE;
	gfc->l3_side.tt[gr][ch].mixed_block_flag=0;
	pe_MS[gr][ch]=pe[gr][ch]=700;
      }
  }



  /* auto-adjust of ATH, useful for low volume */
  adjust_ATH( gfp, tot_ener );



  /* polyphase filtering / mdct */
  for ( ch = 0; ch < gfc->channels_out; ch++ ) {
      mdct_sub48(gfc, inbuf[ch], ch);
      for (gr = 0; gr < gfc->mode_gr; gr++) {
	  init_outer_loop(gfc, &gfc->l3_side.tt[gr][ch]);
      }
  }

  /* Here will be selected MS or LR coding of the 2 stereo channels */
  gfc->mode_ext = MPG_MD_LR_LR;
  
  if (gfp->force_ms) {
      gfc->mode_ext = MPG_MD_MS_LR;
  } else if (gfp->mode == JOINT_STEREO) {
      /* [0] and [1] are the results for the two granules in MPEG-1,
       * in MPEG-2 it's only a faked averaging of the same value
       */
      FLOAT diff_pe = 0;
      for ( gr = 0; gr < gfc->mode_gr; gr++ )
	for ( ch = 0; ch < gfc->channels_out; ch++ )
	    diff_pe += pe_MS[gr][ch] - pe[gr][ch];

      /* based on PE: M/S coding would not use much more bits than L/R */
      if (diff_pe <= 0.0)
	  gfc->mode_ext = MPG_MD_MS_LR;
  }


  /* bit and noise allocation */
  if (gfc->mode_ext == MPG_MD_MS_LR) {
      gr_info *gi0 = &gfc->l3_side.tt[0][0];
      gr_info *gi1 = &gfc->l3_side.tt[gfc->mode_gr-1][0];
      masking = &masking_MS;    /* use MS masking */
      pe_use = &pe_MS;

      if (gi0[0].mixed_block_flag != gi0[1].mixed_block_flag)
	  gi0[0].mixed_block_flag = gi0[1].mixed_block_flag = 0;

      if (gi1[0].mixed_block_flag != gi1[1].mixed_block_flag)
	  gi1[0].mixed_block_flag = gi1[1].mixed_block_flag = 0;

      if (gi0[0].block_type != gi0[1].block_type) {
	  int type = gi0[0].block_type;
	  if (type == NORM_TYPE)
	      type = gi0[1].block_type;
	  gi0[0].block_type = gi0[1].block_type = type;
      }
      if (gi1[0].block_type != gi1[1].block_type) {
	  int type = gi1[0].block_type;
	  if (type == NORM_TYPE)
	      type = gi1[1].block_type;
	  gi1[0].block_type = gi1[1].block_type = type;
      }

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
  } else {
      masking = &masking_LR;    /* use LR masking */
      pe_use = &pe;
  }


#if defined(HAVE_GTK)
  /* copy data for MP3 frame analyzer */
  if (gfp->analysis && gfc->pinfo != NULL) {
    for ( gr = 0; gr < gfc->mode_gr; gr++ ) {
      for ( ch = 0; ch < gfc->channels_out; ch++ ) {
	gfc->pinfo->ms_ratio[gr]=ms_ener_ratio[gr];
	gfc->pinfo->ms_ener_ratio[gr]=ms_ener_ratio[gr];
	gfc->pinfo->blocktype[gr][ch]=gfc->l3_side.tt[gr][ch].block_type;
	gfc->pinfo->pe[gr][ch]=(*pe_use)[gr][ch];
	memcpy(gfc->pinfo->xr[gr][ch], &gfc->l3_side.tt[gr][ch].xr,
	       sizeof(FLOAT)*576);
      }
    }
  }
#endif

  if (gfp->VBR == vbr_off || gfp->VBR == vbr_abr) {
    static FLOAT fircoef[9] = {
	-0.0207887 *5,	-0.0378413*5,	-0.0432472*5,	-0.031183*5,
	7.79609e-18*5,	 0.0467745*5,	 0.10091*5,	0.151365*5,
	0.187098*5
    };

    int i;
    FLOAT f;

    for(i=0;i<18;i++) gfc->nsPsy.pefirbuf[i] = gfc->nsPsy.pefirbuf[i+1];

    f = 0.0;
    for ( gr = 0; gr < gfc->mode_gr; gr++ )
      for ( ch = 0; ch < gfc->channels_out; ch++ )
	  f += (*pe_use)[gr][ch];
    gfc->nsPsy.pefirbuf[18] = f;

    f = gfc->nsPsy.pefirbuf[9];
    for (i=0;i<9;i++)
	f += (gfc->nsPsy.pefirbuf[i]+gfc->nsPsy.pefirbuf[18-i]) * fircoef[i];

    f = (670*5*gfc->mode_gr*gfc->channels_out)/f;
    for ( gr = 0; gr < gfc->mode_gr; gr++ ) {
      for ( ch = 0; ch < gfc->channels_out; ch++ ) {
	(*pe_use)[gr][ch] *= f;
      }
    }
  }

  switch (gfp->VBR){ 
  default:
  case vbr_off:
    iteration_loop( gfp,*pe_use,ms_ener_ratio, *masking);
    break;
  case vbr_mt:
  case vbr_rh:
  case vbr_mtrh:
    VBR_iteration_loop( gfp,*pe_use,ms_ener_ratio, *masking);
    break;
  case vbr_abr:
    ABR_iteration_loop( gfp,*pe_use,ms_ener_ratio, *masking);
    break;
  }

  /*  write the frame to the bitstream  */
  format_bitstream(gfp);

  /* copy mp3 bit buffer into array */
  mp3count = copy_buffer(gfc,mp3buf,mp3buf_size,1);




  if (gfp->bWriteVbrTag) AddVbrFrame(gfp);


#if defined(HAVE_GTK)
  if (gfp->analysis && gfc->pinfo != NULL) {
    for ( ch = 0; ch < gfc->channels_out; ch++ ) {
      int j;
      for ( j = 0; j < FFTOFFSET; j++ )
	gfc->pinfo->pcmdata[ch][j] = gfc->pinfo->pcmdata[ch][j+gfp->framesize];
      for ( j = FFTOFFSET; j < 1600; j++ ) {
	gfc->pinfo->pcmdata[ch][j] = inbuf[ch][j-FFTOFFSET];
      }
    }
    set_frame_pinfo (gfp, *masking);
  }
#endif
  
#ifdef BRHIST
  updateStats( gfc );
#endif

  return mp3count;
}
