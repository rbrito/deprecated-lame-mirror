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

#include <assert.h>
#include "lame.h"
#include "util.h"
#include "newmdct.h"
#include "psymodel.h"
#include "quantize.h"
#include "quantize_pvt.h"
#include "bitstream.h"
#include "VbrTag.h"


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

int lame_encode_mp3_frame(lame_global_flags *gfp,
sample_t inbuf_l[],
sample_t inbuf_r[],
char *mp3buf, int mp3buf_size)
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
  sample_t *inbuf[2];
  lame_internal_flags *gfc=gfp->internal_flags;


  typedef FLOAT8 pedata[2][2];
  pedata pe,pe_MS;
  pedata *pe_use;

  int ch,gr,mean_bits;
  int bitsPerFrame;

  int check_ms_stereo;
  FLOAT8 ms_ratio_next=0;
  FLOAT8 ms_ratio_prev=0;

  memset((char *) masking_ratio, 0, sizeof(masking_ratio));
  memset((char *) masking_MS_ratio, 0, sizeof(masking_MS_ratio));
  memset((char *) scalefac, 0, sizeof(scalefac));
  inbuf[0]=inbuf_l;
  inbuf[1]=inbuf_r;

  if (gfp->scale != 0) {
    int i;
    for (i=0 ; i<gfp->framesize; ++i) {
      inbuf_l[i] *= gfp->scale;
      if (gfc->stereo==2) inbuf_r[i] *= gfp->scale;
    }
  }
  

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
    assert(576>=FFTOFFSET);
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
	  if (gfc->stereo==2) 
	    primebuff1[i]=0;
	}else{
	  primebuff0[i]=inbuf[0][j];
	  if (gfc->stereo==2) 
	    primebuff1[i]=inbuf[1][j];
	  ++j;
	}
      }
      /* polyphase filtering / mdct */
      for ( gr = 0; gr < gfc->mode_gr; gr++ ) {
	for ( ch = 0; ch < gfc->stereo; ch++ ) {
	  gfc->l3_side.gr[gr].ch[ch].tt.block_type=SHORT_TYPE;
	}
      }
      mdct_sub48(gfc, primebuff0, primebuff1, xr, &gfc->l3_side);
    }
    
    iteration_init(gfp, &gfc->l3_side);
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





  if (gfc->psymodel) {
    /* psychoacoustic model
     * psy model has a 1 granule (576) delay that we must compensate for
     * (mt 6/99).
     */
    int ret;
    sample_t *bufp[2];  /* address of beginning of left & right granule */
    int blocktype[2];

    ms_ratio_prev=gfc->ms_ratio[gfc->mode_gr-1];
    for (gr=0; gr < gfc->mode_gr ; gr++) {

      for ( ch = 0; ch < gfc->stereo; ch++ )
	bufp[ch] = &inbuf[ch][576 + gr*576-FFTOFFSET];

      ret=L3psycho_anal( gfp,bufp, gr, 
		     &gfc->ms_ratio[gr],&ms_ratio_next,&gfc->ms_ener_ratio[gr],
		     masking_ratio, masking_MS_ratio,
		     pe[gr],pe_MS[gr],blocktype);
      if (ret!=0) return -4;

      for ( ch = 0; ch < gfc->stereo; ch++ )
	gfc->l3_side.gr[gr].ch[ch].tt.block_type=blocktype[ch];

    }
    /* at LSF there is no 2nd granule */
    gfc->ms_ratio[1]=gfc->ms_ratio[gfc->mode_gr-1];
  }else{
    for (gr=0; gr < gfc->mode_gr ; gr++)
      for ( ch = 0; ch < gfc->stereo; ch++ ) {
	gfc->l3_side.gr[gr].ch[ch].tt.block_type=NORM_TYPE;
	pe_MS[gr][ch]=pe[gr][ch]=700;
      }
  }


  /* block type flags */
  for( gr = 0; gr < gfc->mode_gr; gr++ ) {
    for ( ch = 0; ch < gfc->stereo; ch++ ) {
      gr_info *cod_info = &gfc->l3_side.gr[gr].ch[ch].tt;
      cod_info->mixed_block_flag = 0;     /* never used by this model */
      if (cod_info->block_type == NORM_TYPE )
	cod_info->window_switching_flag = 0;
      else
	cod_info->window_switching_flag = 1;
    }
  }


  /* polyphase filtering / mdct */
  mdct_sub48(gfc, inbuf[0], inbuf[1], xr, &gfc->l3_side);
  /* re-order the short blocks, for more efficient encoding below */
  for (gr = 0; gr < gfc->mode_gr; gr++) {
    for (ch = 0; ch < gfc->stereo; ch++) {
      gr_info *cod_info = &gfc->l3_side.gr[gr].ch[ch].tt;
      if (cod_info->block_type==SHORT_TYPE) {
	freorder(gfc->scalefac_band.s,xr[gr][ch]);
      }
    }
  }
  



  /* use m/s gfc->stereo? */
  check_ms_stereo =  (gfp->mode == MPG_MD_JOINT_STEREO);
  if (check_ms_stereo) {
    int gr0 = 0, gr1 = gfc->mode_gr-1;
    /* make sure block type is the same in each channel */
    check_ms_stereo =
      (gfc->l3_side.gr[gr0].ch[0].tt.block_type==gfc->l3_side.gr[gr0].ch[1].tt.block_type) &&
      (gfc->l3_side.gr[gr1].ch[0].tt.block_type==gfc->l3_side.gr[gr1].ch[1].tt.block_type);
  }
  if (gfp->force_ms) 
    gfc->mode_ext = MPG_MD_MS_LR;
  else if (check_ms_stereo) {
    /* ms_ratio = is like the ratio of side_energy/total_energy */
    FLOAT8 ms_ratio_ave;
    /*     ms_ratio_ave = .5*(ms_ratio[0] + ms_ratio[1]);*/

    ms_ratio_ave = .25*(gfc->ms_ratio[0] + gfc->ms_ratio[1]+
			 ms_ratio_prev + ms_ratio_next);
    if ( (ms_ratio_ave <.35) && (.5*(gfc->ms_ratio[0]+gfc->ms_ratio[1])<.45) )
#ifdef RH_VALIDATE_MS
        {
            int sum_pe_MS, sum_pe_LR;
            
            sum_pe_MS = pe_MS[0][0] + pe_MS[0][1] + pe_MS[1][0] + pe_MS[1][1];
            sum_pe_LR = pe   [0][0] + pe   [0][1] + pe   [1][0] + pe   [1][1];
            
            if (sum_pe_MS <= 1.07 * sum_pe_LR) {
                /* based on PE:
                 * M/S coding would not use much more bits than L/R coding 
                 */
                gfc->mode_ext = MPG_MD_MS_LR;
            }
        }
#else
            gfc->mode_ext = MPG_MD_MS_LR;
#endif
  }



  if (gfp->analysis && gfc->pinfo != NULL) {
    for ( gr = 0; gr < gfc->mode_gr; gr++ ) {
      for ( ch = 0; ch < gfc->stereo; ch++ ) {
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
  getframebits(gfp,&bitsPerFrame,&mean_bits);

  format_bitstream( gfp, bitsPerFrame, l3_enc, scalefac);

  /* copy mp3 bit buffer into array */
  mp3count = copy_buffer(mp3buf,mp3buf_size,&gfc->bs);

  if (gfp->bWriteVbrTag) AddVbrFrame(gfp);


  if (gfp->analysis && gfc->pinfo != NULL) {
    int j;
    for ( ch = 0; ch < gfc->stereo; ch++ ) {
      for ( j = 0; j < FFTOFFSET; j++ )
	gfc->pinfo->pcmdata[ch][j] = gfc->pinfo->pcmdata[ch][j+gfp->framesize];
      for ( j = FFTOFFSET; j < 1600; j++ ) {
	gfc->pinfo->pcmdata[ch][j] = inbuf[ch][j-FFTOFFSET];
      }
    }
  }
  
  updateStats( gfc );

  return mp3count;
}
