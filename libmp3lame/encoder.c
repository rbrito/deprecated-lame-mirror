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

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

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

typedef FLOAT8 pedata[2][2];

int  lame_encode_mp3_frame (				// Output
	lame_global_flags* const  gfp,			// Context
	sample_t*                 inbuf_l,              // Input
	sample_t*                 inbuf_r,              // Input
	unsigned char*            mp3buf, 		// Output
	size_t                    mp3buf_size )		// Output
{
#ifdef macintosh /* PLL 14/04/2000 */
  static FLOAT8 xr[2][2][576];
  static int l3_enc[2][2][576];
#else
  FLOAT8 xr[2][2][576];
  int l3_enc[2][2][576];
#endif
  int mp3count;
  III_psy_ratio masking_ratio[2][2];    /*LR ratios */
  III_psy_ratio masking_MS_ratio[2][2]; /*MS ratios */
  III_psy_ratio (*masking)[2][2];  /*LR ratios and MS ratios*/
  III_scalefac_t scalefac[2][2];
  const sample_t *inbuf[2];
  lame_internal_flags *gfc=gfp->internal_flags;

  pedata pe,pe_MS;
  pedata *pe_use;

  int ch,gr,mean_bits;
  int bitsPerFrame;

  int check_ms_stereo;
  FLOAT8 ms_ratio_next = 0.;
  FLOAT8 ms_ratio_prev = 0.;

  /* end of variable definition */
  

  memset((char *) masking_ratio, 0, sizeof(masking_ratio));
  memset((char *) masking_MS_ratio, 0, sizeof(masking_MS_ratio));
  memset((char *) scalefac, 0, sizeof(scalefac));
  inbuf[0]=inbuf_l;
  inbuf[1]=inbuf_r;

  /* Now another nasty example about programming:
   * A lot of lame functions destroy data, so you can't trust
   * in that data is not modified. A lot of prophylactic copying is done.
   */
   

  gfc->mode_ext = MPG_MD_LR_LR;

  if (gfc->lame_encode_frame_init==0 )  {
    gfc->lame_encode_frame_init=1;
    
    /* padding method as described in 
     * "MPEG-Layer3 / Bitstream Syntax and Decoding"
     * by Martin Sieler, Ralph Sperschneider
     *
     * note: there is no padding for the very first frame
     *
     * Robert.Hegemann@gmx.de 2000-06-22
     */
        
    gfc->frac_SpF = ((gfp->version+1)*72000L*gfp->brate) % gfp->out_samplerate;
    gfc->slot_lag  = gfc->frac_SpF;
    
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

    /* prime the MDCT/polyphase filterbank with a short block */
    { 
      int i,j;
      sample_t primebuff0[286+1152+576];
      sample_t primebuff1[286+1152+576];
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
	  gfc->l3_side.gr[gr].ch[ch].tt.block_type=SHORT_TYPE;
	}
      }
      mdct_sub48(gfc, primebuff0, primebuff1, xr);
    }
    
    iteration_init(gfp);
    
    /*  prepare for ATH auto adjustment:
     *  we want to decrease the ATH by 12 dB per second
     */ {
        FLOAT8 frame_duration = 576. * gfc->mode_gr / gfp->out_samplerate;
        gfc->ATH->decay = pow(10., -12./10. * frame_duration);
        gfc->ATH->adjust = 1.0;
    }
  }


  /********************** padding *****************************/
  switch (gfp->padding_type) {
  case 0:
    gfc->padding=0;
    break;
  case 1:
    gfc->padding=1;
    break;
  case 2:
  default:
    if (gfp->VBR!=vbr_off) {
      gfc->padding=0;
    } else {
      if (gfp->disable_reservoir) {
	gfc->padding = 0;
	/* if the user specified --nores, dont very gfc->padding either */
	/* tiny changes in frac_SpF rounding will cause file differences */
      }else{
        /* padding method as described in 
         * "MPEG-Layer3 / Bitstream Syntax and Decoding"
         * by Martin Sieler, Ralph Sperschneider
         *
         * note: there is no padding for the very first frame
         *
         * Robert.Hegemann@gmx.de 2000-06-22
         */

        gfc->slot_lag -= gfc->frac_SpF;
        if (gfc->slot_lag < 0) {
          gfc->slot_lag += gfp->out_samplerate;
          gfc->padding = 1;
        } else {
          gfc->padding = 0;
        }
      } /* reservoir enabled */
    }
  }

    
    /*  auto-adjust of ATH, useful for low volume
     *  Gabriel Bouvigne 3 feb 2001
     */
    if (gfc->ATH->use_adjust) {
        FLOAT8 max_val = 0;
        int i;
        
        /*  check maximum level in left channel
         */
        for (i = 0; i < gfp->framesize; i++) {
            if (max_val < fabs(inbuf_l[i]))
                max_val = fabs(inbuf_l[i]);
        }
        /*  and in right channel (if not mono) 
         */
        for (i = 0; gfc->channels_out == 2 && i < gfp->framesize; i++) {
            if (max_val < fabs(inbuf_r[i]))
                max_val = fabs(inbuf_r[i]);
        }
        
        /*  adjust ATH depending on range of maximum value
         */
        if (vbr_mtrh == gfp->VBR) {
            /*  this code reduces slowly the ATH (speed of 12 dB per second)
             *  with some supporting stages to limit the reduction
             *    640  ->  ~17 dB
             *         :
             *  32640  ->  ~0.01 dB
             */
            FLOAT8 
            x = Max (640, 320*(int)(max_val/320));
            x = x/32768;
            gfc->ATH->adjust *= gfc->ATH->decay;
            if (gfc->ATH->adjust < x)       /* but not more than f(x) dB */
                gfc->ATH->adjust = x;
        }
        else {
            if      (0.5 < max_val / 32768) {       /* value above 50 % */
                    gfc->ATH->adjust = 1.0;         /* do not reduce ATH */
            }
            else if (0.3 < max_val / 32768) {       /* value above 30 % */
                    gfc->ATH->adjust *= 0.955;      /* reduce by ~0.2 dB */
                    if (gfc->ATH->adjust < 0.3)     /* but ~5 dB in maximum */
                        gfc->ATH->adjust = 0.3;            
            }
            else {                                  /* value below 30 % */
                    gfc->ATH->adjust *= 0.93;       /* reduce by ~0.3 dB */
                    if (gfc->ATH->adjust < 0.01)    /* but 20 dB in maximum */
                        gfc->ATH->adjust = 0.01;
            }
        }
    }


  if (gfc->psymodel) {
    /* psychoacoustic model
     * psy model has a 1 granule (576) delay that we must compensate for
     * (mt 6/99).
     */
    int ret;
    const sample_t *bufp[2];  /* address of beginning of left & right granule */
    int blocktype[2];

    ms_ratio_prev=gfc->ms_ratio[gfc->mode_gr-1];
    for (gr=0; gr < gfc->mode_gr ; gr++) {

      for ( ch = 0; ch < gfc->channels_out; ch++ )
	bufp[ch] = &inbuf[ch][576 + gr*576-FFTOFFSET];

      if (gfc->nsPsy.use) {
	ret=L3psycho_anal_ns( gfp, bufp, gr, 
			      &gfc->ms_ratio[gr],&ms_ratio_next,&gfc->ms_ener_ratio[gr],
			      masking_ratio, masking_MS_ratio,
			      pe[gr],pe_MS[gr],blocktype);
      } else {
	ret=L3psycho_anal( gfp, bufp, gr, 
			   &gfc->ms_ratio[gr],&ms_ratio_next,&gfc->ms_ener_ratio[gr],
			   masking_ratio, masking_MS_ratio,
			   pe[gr],pe_MS[gr],blocktype);
      }
      if (ret!=0) return -4;

      for ( ch = 0; ch < gfc->channels_out; ch++ )
	gfc->l3_side.gr[gr].ch[ch].tt.block_type=blocktype[ch];

    }
    /* at LSF there is no 2nd granule */
    gfc->ms_ratio[1]=gfc->ms_ratio[gfc->mode_gr-1];
  }else{
    for (gr=0; gr < gfc->mode_gr ; gr++)
      for ( ch = 0; ch < gfc->channels_out; ch++ ) {
	gfc->l3_side.gr[gr].ch[ch].tt.block_type=NORM_TYPE;
	pe_MS[gr][ch]=pe[gr][ch]=700;
      }
  }


  /* block type flags */
  for( gr = 0; gr < gfc->mode_gr; gr++ ) {
    for ( ch = 0; ch < gfc->channels_out; ch++ ) {
      gr_info *cod_info = &gfc->l3_side.gr[gr].ch[ch].tt;
      cod_info->mixed_block_flag = 0;     /* never used by this model */
      if (cod_info->block_type == NORM_TYPE )
	cod_info->window_switching_flag = 0;
      else
	cod_info->window_switching_flag = 1;
    }
  }


  /* polyphase filtering / mdct */
  mdct_sub48(gfc, inbuf[0], inbuf[1], xr);
  /* re-order the short blocks, for more efficient encoding below */
  for (gr = 0; gr < gfc->mode_gr; gr++) {
    for (ch = 0; ch < gfc->channels_out; ch++) {
      gr_info *cod_info = &gfc->l3_side.gr[gr].ch[ch].tt;
      if (cod_info->block_type==SHORT_TYPE) {
	freorder(gfc->scalefac_band.s,xr[gr][ch]);
      }
    }
  }
  

  /* use m/s gfc->channels_out? */
  check_ms_stereo =  (gfp->mode == MPG_MD_JOINT_STEREO);
  if (check_ms_stereo) {
    int gr0 = 0, gr1 = gfc->mode_gr-1;
    /* make sure block type is the same in each channel */
    check_ms_stereo =
      (gfc->l3_side.gr[gr0].ch[0].tt.block_type==gfc->l3_side.gr[gr0].ch[1].tt.block_type) &&
      (gfc->l3_side.gr[gr1].ch[0].tt.block_type==gfc->l3_side.gr[gr1].ch[1].tt.block_type);
  }
  
  /* Here will be selected MS or LR coding of the 2 stereo channels */

  assert (  gfc->mode_ext == MPG_MD_LR_LR );
  gfc->mode_ext = MPG_MD_LR_LR;
  
  if (gfp->force_ms) {
    gfc->mode_ext = MPG_MD_MS_LR;
  } else if (check_ms_stereo) {
    /* ms_ratio = is like the ratio of side_energy/total_energy */
    /*     ms_ratio_ave =  .50 * (ms_ratio[0] + ms_ratio[1]);*/

    
        /* [0] and [1] are the results for the two granules in MPEG-1,
         * in MPEG-2 it's only a faked averaging of the same value
         * _prev is the value of the last granule of the previous frame
         * _next is the value of the first granule of the next frame
         */
        FLOAT8  ms_ratio_ave1 = 0.25 * ( gfc->ms_ratio[0] + gfc->ms_ratio[1] + ms_ratio_prev + ms_ratio_next );
        FLOAT8  ms_ratio_ave2 = 0.50 * ( gfc->ms_ratio[0] + gfc->ms_ratio[1] );
        FLOAT8  threshold1    = 0.35;
        FLOAT8  threshold2    = 0.45;

	if (gfp->mode_automs) {
	  if ( gfp->compression_ratio < 11.025 ) {
            /* 11.025 => 1, 6.3 => 0 */
            double thr = (gfp->compression_ratio - 6.3) / (11.025 - 6.3);
            if (thr<0) thr=0;
            threshold1   *= thr;
            threshold2   *= thr;
	  }
	}

        if ((ms_ratio_ave1 < threshold1  &&  ms_ratio_ave2 < threshold2) || gfc->nsPsy.use) {
            int  sum_pe_MS = pe_MS[0][0] + pe_MS[0][1] + pe_MS[1][0] + pe_MS[1][1];
            int  sum_pe_LR = pe   [0][0] + pe   [0][1] + pe   [1][0] + pe   [1][1];
            
            /* based on PE: M/S coding would not use much more bits than L/R coding */

	    if (sum_pe_MS <= 1.07 * sum_pe_LR && !gfc->nsPsy.use) gfc->mode_ext = MPG_MD_MS_LR;
	    if (sum_pe_MS <= 1.00 * sum_pe_LR &&  gfc->nsPsy.use) gfc->mode_ext = MPG_MD_MS_LR;
        }


  }



  if (gfp->analysis && gfc->pinfo != NULL) {
    for ( gr = 0; gr < gfc->mode_gr; gr++ ) {
      for ( ch = 0; ch < gfc->channels_out; ch++ ) {
	gfc->pinfo->ms_ratio[gr]=gfc->ms_ratio[gr];
	gfc->pinfo->ms_ener_ratio[gr]=gfc->ms_ener_ratio[gr];
	gfc->pinfo->blocktype[gr][ch]=
	  gfc->l3_side.gr[gr].ch[ch].tt.block_type;
	memcpy(gfc->pinfo->xr[gr][ch],xr[gr][ch],sizeof(xr[gr][ch]));
	/* if MS stereo, switch to MS psy data */
	if (gfc->mode_ext==MPG_MD_MS_LR) {
	  gfc->pinfo->pe[gr][ch]=gfc->pinfo->pe[gr][ch+2];
	  gfc->pinfo->ers[gr][ch]=gfc->pinfo->ers[gr][ch+2];
	  memcpy(gfc->pinfo->energy[gr][ch],gfc->pinfo->energy[gr][ch+2],
		 sizeof(gfc->pinfo->energy[gr][ch]));
	}
      }
    }
  }




  /* bit and noise allocation */
  if (MPG_MD_MS_LR == gfc->mode_ext) {
    masking = &masking_MS_ratio;    /* use MS masking */
    pe_use = &pe_MS;
  } else {
    masking = &masking_ratio;    /* use LR masking */
    pe_use = &pe;
  }


  if (gfc->nsPsy.use && (gfp->VBR == vbr_off || gfp->VBR == vbr_abr)) {
    static FLOAT fircoef[19] = {
      -0.0207887,-0.0378413,-0.0432472,-0.031183,
      7.79609e-18,0.0467745,0.10091,0.151365,
      0.187098,0.2,0.187098,0.151365,
      0.10091,0.0467745,7.79609e-18,-0.031183,
      -0.0432472,-0.0378413,-0.0207887,
    };
    int i;
    FLOAT8 f;

    for(i=0;i<18;i++) gfc->nsPsy.pefirbuf[i] = gfc->nsPsy.pefirbuf[i+1];

    i=0;
    gfc->nsPsy.pefirbuf[18] = 0;
    for ( gr = 0; gr < gfc->mode_gr; gr++ ) {
      for ( ch = 0; ch < gfc->channels_out; ch++ ) {
	gfc->nsPsy.pefirbuf[18] += (*pe_use)[gr][ch];
	i++;
      }
    }

    gfc->nsPsy.pefirbuf[18] = gfc->nsPsy.pefirbuf[18] / i;
    f = 0;
    for(i=0;i<19;i++) f += gfc->nsPsy.pefirbuf[i] * fircoef[i];

    for ( gr = 0; gr < gfc->mode_gr; gr++ ) {
      for ( ch = 0; ch < gfc->channels_out; ch++ ) {
	(*pe_use)[gr][ch] *= 670 / f;
      }
    }
  }

  switch (gfp->VBR){ 
  default:
  case vbr_off:
    iteration_loop( gfp,*pe_use, gfc->ms_ener_ratio, xr, *masking, l3_enc, scalefac);
    break;
  case vbr_mt:
    VBR_quantize( gfp,*pe_use, gfc->ms_ener_ratio, xr, *masking, l3_enc, scalefac);
    break;
  case vbr_rh:
  case vbr_mtrh:
    VBR_iteration_loop( gfp,*pe_use, gfc->ms_ener_ratio, xr, *masking, l3_enc, scalefac);
    break;
  case vbr_abr:
    ABR_iteration_loop( gfp,*pe_use, gfc->ms_ener_ratio, xr, *masking, l3_enc, scalefac);
    break;
  }

  /*  write the frame to the bitstream  */
  getframebits(gfp, &bitsPerFrame, &mean_bits);

  format_bitstream( gfp, bitsPerFrame, l3_enc, scalefac);

  /* copy mp3 bit buffer into array */
  mp3count = copy_buffer(mp3buf,mp3buf_size,&gfc->bs);

  if (gfp->bWriteVbrTag) AddVbrFrame(gfp);


  if (gfp->analysis && gfc->pinfo != NULL) {
    int j;
    for ( ch = 0; ch < gfc->channels_out; ch++ ) {
      for ( j = 0; j < FFTOFFSET; j++ )
	gfc->pinfo->pcmdata[ch][j] = gfc->pinfo->pcmdata[ch][j+gfp->framesize];
      for ( j = FFTOFFSET; j < 1600; j++ ) {
	gfc->pinfo->pcmdata[ch][j] = inbuf[ch][j-FFTOFFSET];
      }
    }
    set_frame_pinfo (gfp, xr, *masking, l3_enc, scalefac);
  }
  
  updateStats( gfc );

  return mp3count;
}
