/*
 * MP3 quantization
 *
 * Copyright (c) 1999 Mark Taylor
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */



#include <assert.h>
#include "util.h"
#include "l3side.h"
#include "quantize.h"
#include "reservoir.h"
#include "quantize-pvt.h"
#include "gtkanal.h"




/************************************************************************/
/*  iteration_loop()                                                    */
/************************************************************************/
void
iteration_loop( lame_global_flags *gfp, FLOAT8 pe[2][2],
                FLOAT8 ms_ener_ratio[2],  FLOAT8 xr[2][2][576],
                III_psy_ratio ratio[2][2],  int l3_enc[2][2][576],
                III_scalefac_t scalefac[2][2])
{
  III_psy_xmin l3_xmin[2];
  FLOAT8 xrpow[576];
  FLOAT8 xfsf[4][SBMAX_l];
  FLOAT8 noise[4]; /* over,max_noise,over_noise,tot_noise; */
  int targ_bits[2];
  int bitsPerFrame;
  int mean_bits,max_bits;
  int ch, gr, i, bit_rate;
  gr_info *cod_info;
  lame_internal_flags *gfc=gfp->internal_flags;
  III_side_info_t *l3_side = &gfc->l3_side;

  iteration_init(gfp,l3_side,l3_enc);
  bit_rate = bitrate_table[gfp->version][gfc->bitrate_index];
  getframebits(gfp,&bitsPerFrame, &mean_bits);
  ResvFrameBegin(gfp, l3_side, mean_bits, bitsPerFrame );

  /* quantize! */
  for ( gr = 0; gr < gfc->mode_gr; gr++ ) {

    max_bits=on_pe(gfp,pe,l3_side,targ_bits,mean_bits, gr);
    if (gfc->mode_ext==MPG_MD_MS_LR) {
      ms_convert(xr[gr], xr[gr]);
      reduce_side(targ_bits,ms_ener_ratio[gr],mean_bits,max_bits);
    }
    for (ch=0 ; ch < gfc->stereo ; ch ++) {
      cod_info = &l3_side->gr[gr].ch[ch].tt; 

      if (!init_outer_loop(gfp,xr[gr][ch],xrpow, cod_info))
        {
          /* xr contains no energy 
           * cod_info-> was initialized in init_outer_loop
           */
          memset(&scalefac[gr][ch],0,sizeof(III_scalefac_t));
          memset(l3_enc[gr][ch],0,576*sizeof(int));
          memset(xfsf,0,sizeof(xfsf));
          memset(noise,0,sizeof(noise));
        }
      else
        {
          calc_xmin(gfp,xr[gr][ch], &ratio[gr][ch], cod_info, &l3_xmin[ch]);
          memset(&scalefac[gr][ch],0,sizeof(III_scalefac_t));
          outer_loop( gfp,xr[gr][ch], targ_bits[ch], noise, &l3_xmin[ch],
                      l3_enc[gr][ch], &scalefac[gr][ch], cod_info, xfsf, ch,
                      xrpow);
        }

      best_scalefac_store(gfp,gr, ch, l3_enc, l3_side, scalefac);
      if (gfc->use_best_huffman==1) {
	best_huffman_divide(gfc, gr, ch, cod_info, l3_enc[gr][ch]);
      }
      assert((int)cod_info->part2_3_length < 4096);

      if (gfp->gtkflag) {
        set_pinfo (gfp, cod_info, &ratio[gr][ch], &scalefac[gr][ch], 
                   xr[gr][ch], xfsf, noise, gr, ch);
      }

/*#define NORES_TEST */
#ifndef NORES_TEST
      ResvAdjust(gfp,cod_info, l3_side, mean_bits );
#endif
      /* set the sign of l3_enc */
      for ( i = 0; i < 576; i++) {
        if (xr[gr][ch][i] < 0) {
          l3_enc[gr][ch][i] *= -1;
        }
      }


    } /* loop over ch */
  } /* loop over gr */

#ifdef NORES_TEST
  /* replace ResvAdjust above with this code if you do not want
     the second granule to use bits saved by the first granule.
     when combined with --nores, this is usefull for testing only */
  for ( gr = 0; gr < gfc->mode_gr; gr++ ) {
    for ( ch =  0; ch < gfc->stereo; ch++ ) {
      cod_info = &l3_side->gr[gr].ch[ch].tt;
      ResvAdjust(gfp, cod_info, l3_side, mean_bits );
    }
  }
#endif
  ResvFrameEnd(gfp,l3_side, mean_bits );
}




/*
 *  ABR_iteration_loop()
 *
 *  encode a frame with a disired average bitrate
 *
 *  mt 2000/05/31
 */
void
ABR_iteration_loop (lame_global_flags *gfp, FLOAT8 pe[2][2],
                    FLOAT8 ms_ener_ratio[2], FLOAT8 xr[2][2][576],
                    III_psy_ratio ratio[2][2], int l3_enc[2][2][576],
                    III_scalefac_t scalefac[2][2])
{
  III_psy_xmin l3_xmin;
  FLOAT8    xrpow[576];
  FLOAT8    xfsf[4][SBMAX_l];
  FLOAT8    noise[4];
  int       targ_bits[2][2];
  int       bit_rate,bitsPerFrame, mean_bits,totbits,max_frame_bits;
  int       i,ch, gr, ath_over;
  int       analog_silence_bits;
  FLOAT8    res_factor;
  gr_info  *cod_info = NULL;
  lame_internal_flags *gfc=gfp->internal_flags;
  III_side_info_t *l3_side = &gfc->l3_side;

  iteration_init(gfp,l3_side,l3_enc);

  gfc->bitrate_index = gfc->VBR_max_bitrate;
  getframebits (gfp,&bitsPerFrame, &mean_bits);
  max_frame_bits=ResvFrameBegin (gfp,l3_side, mean_bits, bitsPerFrame);

  gfc->bitrate_index = 1;
  getframebits (gfp,&bitsPerFrame, &mean_bits);
  analog_silence_bits = mean_bits/gfc->stereo;

  /* compute a target  mean_bits based on compression ratio 
   * which was set based on VBR_q  
   */
  bit_rate = gfp->VBR_mean_bitrate_kbps;
  bitsPerFrame = (bit_rate*gfp->framesize*1000)/gfp->out_samplerate;
  mean_bits = (bitsPerFrame - 8*gfc->sideinfo_len) / gfc->mode_gr;


  res_factor = .90 + .10*(11.0- gfp->compression_ratio)/(11.0-5.5);
  if (res_factor <  .90) { res_factor =  .90; }
  if (res_factor > 1.00) { res_factor = 1.00; }



  for(gr = 0; gr < gfc->mode_gr; gr++) {
    for(ch = 0; ch < gfc->stereo; ch++) {
      targ_bits[gr][ch]=res_factor*(mean_bits/gfc->stereo);
      if (pe[gr][ch]>700) {
        int add_bits=(pe[gr][ch]-700)/1.4;
 
        cod_info = &l3_side->gr[gr].ch[ch].tt;
        targ_bits[gr][ch]=res_factor*(mean_bits/gfc->stereo);
 
        /* short blocks use a little extra, no matter what the pe */
        if (cod_info->block_type==SHORT_TYPE) {
          if (add_bits<mean_bits/4) { add_bits=mean_bits/4; }
        }
        /* at most increase bits by 1.5*average */
        if (add_bits > .75*mean_bits) { add_bits=mean_bits*.75; }
        if (add_bits < 0) { add_bits=0; }
 
        targ_bits[gr][ch] += add_bits;
      }
    }
  }
  if (gfc->mode_ext==MPG_MD_MS_LR) {
    for(gr = 0; gr < gfc->mode_gr; gr++) {
      reduce_side(targ_bits[gr],ms_ener_ratio[gr],mean_bits,4095);
    }
  }

  totbits=0;
  for(gr = 0; gr < gfc->mode_gr; gr++) {
    for(ch = 0; ch < gfc->stereo; ch++) {
      if (targ_bits[gr][ch] > 4095) { targ_bits[gr][ch]=4095; }
      totbits += targ_bits[gr][ch];
    }
  }

  if (totbits > max_frame_bits) {
    for(gr = 0; gr < gfc->mode_gr; gr++) {
      for(ch = 0; ch < gfc->stereo; ch++) {
        targ_bits[gr][ch] *= ((float)max_frame_bits/(float)totbits); 
      }
    }
  }

  totbits=0;
  for(gr = 0; gr < gfc->mode_gr; gr++) {

    if (gfc->mode_ext==MPG_MD_MS_LR) { ms_convert(xr[gr], xr[gr]); }

    for(ch = 0; ch < gfc->stereo; ch++) {
      cod_info = &l3_side->gr[gr].ch[ch].tt;

      if (!init_outer_loop(gfp,xr[gr][ch],xrpow, cod_info)) {
        /* xr contains no energy 
         * cod_info was set in init_outer_loop above
         */
        memset(&scalefac[gr][ch],0,sizeof(III_scalefac_t));
        memset(l3_enc[gr][ch],0,576*sizeof(int));
        memset(noise,0,sizeof(noise));
      } else {
        ath_over = calc_xmin(gfp,xr[gr][ch],&ratio[gr][ch],cod_info,&l3_xmin);
        if (0==ath_over) {
          /* analog silence */
          targ_bits[gr][ch]=analog_silence_bits;
        }

        memset(&scalefac[gr][ch],0,sizeof(III_scalefac_t));
        outer_loop( gfp,xr[gr][ch], targ_bits[gr][ch], noise, &l3_xmin,
                    l3_enc[gr][ch], &scalefac[gr][ch], cod_info, xfsf, ch,
                    xrpow);
      }

      totbits += cod_info->part2_3_length;
      if (gfp->gtkflag) {
        set_pinfo(gfp, cod_info, &ratio[gr][ch], &scalefac[gr][ch],
                  xr[gr][ch], xfsf, noise, gr, ch);
      }
    } /* ch */
  }  /* gr */
  
  /*******************************************************************
   * find a bitrate which can handle totbits 
   *******************************************************************/
  for( gfc->bitrate_index =  gfc->VBR_min_bitrate ;
       gfc->bitrate_index <= gfc->VBR_max_bitrate;
       gfc->bitrate_index++    ) {
    getframebits (gfp,&bitsPerFrame, &mean_bits);
    max_frame_bits=ResvFrameBegin (gfp,l3_side, mean_bits, bitsPerFrame);
    if( totbits <= max_frame_bits) { break; }
  }
  assert (gfc->bitrate_index <= gfc->VBR_max_bitrate);



  /*******************************************************************
   * update reservoir status after FINAL quantization/bitrate 
   *******************************************************************/
  for (gr = 0; gr < gfc->mode_gr; gr++)
    for (ch = 0; ch < gfc->stereo; ch++) {
      cod_info = &l3_side->gr[gr].ch[ch].tt;

      best_scalefac_store(gfp,gr, ch, l3_enc, l3_side, scalefac);
      if (gfc->use_best_huffman==1 ) {
        best_huffman_divide(gfc, gr, ch, cod_info, l3_enc[gr][ch]);
      }
      if (gfp->gtkflag) {
        gfc->pinfo->LAMEmainbits[gr][ch]=cod_info->part2_3_length;
      }
      ResvAdjust (gfp,cod_info, l3_side, mean_bits);
      /* set the sign of l3_enc from the sign of xr */
      for ( i = 0; i < 576; i++) {
        if (xr[gr][ch][i] < 0) { l3_enc[gr][ch][i] *= -1; }
      }

    }
  ResvFrameEnd (gfp,l3_side, mean_bits);
}




/************************************************************************
 *
 * VBR_iteration_loop()   
 *
 * tries to find out how many bits are needed for each granule and channel
 * to get an acceptable quantization. An appropriate bitrate will then be
 * choosed for quantization.  rh 8/99                          
 *
 ************************************************************************/
void
VBR_iteration_loop (lame_global_flags *gfp, FLOAT8 pe[2][2], 
                    FLOAT8 ms_ener_ratio[2], FLOAT8 xr[2][2][576],
                    III_psy_ratio ratio[2][2], int l3_enc[2][2][576],
                    III_scalefac_t scalefac[2][2])
{
  /* PLL 16/07/2000 */
#ifdef macintosh
  static plotting_data bst_pinfo;
#else
  plotting_data bst_pinfo;
#endif

  III_psy_xmin l3_xmin[2][2];
  III_scalefac_t  bst_scalefac;
  gr_info         bst_cod_info;
  int             bst_l3_enc[576]; 
  
  FLOAT8    xrpow[2][576];
  FLOAT8    noise[4];          /* over,max_noise,over_noise,tot_noise; */
  FLOAT8    xfsf[4][SBMAX_l];
  int       save_bits[2][2];
  int       bands[2][2];
  int       this_bits, dbits;
  int       used_bits=0;
  int       min_bits,max_bits,min_mean_bits=0,analog_mean_bits,Max_bits;
  int       frameBits[15];
  int       bitsPerFrame;
  int       bits;
  int       mean_bits;
  int       i,ch, gr, analog_silence;
  int       reduce_s_ch=0;
  gr_info  *cod_info = NULL;
  lame_internal_flags *gfc=gfp->internal_flags;
  III_side_info_t *l3_side = &gfc->l3_side;

  iteration_init(gfp,l3_side,l3_enc);

  /* my experiences are, that side channel reduction  
   * does more harm than good when VBR encoding
   * (Robert.Hegemann@gmx.de 2000-02-18)
   */
  if (gfc->mode_ext==MPG_MD_MS_LR && gfp->quality >= 5) { 
    reduce_s_ch=1;
  }

  /* bits for analog silence */
  gfc->bitrate_index = 1;
  getframebits (gfp,&bitsPerFrame, &mean_bits);
  analog_mean_bits = mean_bits/gfc->stereo;
  
  analog_silence=1;
  for (gr = 0; gr < gfc->mode_gr; gr++) {
    /* copy data to be quantized into xr */
    if (gfc->mode_ext==MPG_MD_MS_LR) { ms_convert(xr[gr],xr[gr]); }
    for (ch = 0; ch < gfc->stereo; ch++) {
      cod_info = &l3_side->gr[gr].ch[ch].tt;
      /* - lower masking depending on Quality setting
       * - quality control together with adjusted ATH MDCT scaling
       *   on lower quality setting allocate more noise from
       *   ATH masking, and on higher quality setting allocate
       *   less noise from ATH masking.
       * - experiments show that going more than 2dB over GPSYCHO's
       *   limits ends up in very annoying artefacts
       */
      {
        static const FLOAT8 dbQ[10]={-4.,-3.,-2.,-1.,0,.5,1.,1.5,2.,2.5};
        FLOAT8 masking_lower_db, adjust;
        assert( gfp->VBR_q <= 9 );
        assert( gfp->VBR_q >= 0 );
        
        if (cod_info->block_type==SHORT_TYPE) {
          adjust = 5/(1+exp(3.5-pe[gr][ch]/300.))-0.14;
        } else {
          adjust = 2/(1+exp(3.5-pe[gr][ch]/300.))-0.05;
        }
        if (gfc->noise_shaping==2) {
          adjust = Max(1.25,adjust);
        }
        masking_lower_db = dbQ[gfp->VBR_q]-adjust; 
        gfc->masking_lower = pow(10.0,masking_lower_db/10);
      }
      bands[gr][ch] = calc_xmin(gfp,xr[gr][ch], &ratio[gr][ch], 
                                cod_info, &l3_xmin[gr][ch]);
      if (bands[gr][ch]) {
        analog_silence = 0;
      }
    }
  }
  
  /*******************************************************************
   * how many bits are available for each bitrate?
   *******************************************************************/
  for( gfc->bitrate_index = 1;
       gfc->bitrate_index <= gfc->VBR_max_bitrate;
       gfc->bitrate_index++    ) {
    getframebits (gfp,&bitsPerFrame, &mean_bits);
    if (gfc->bitrate_index == gfc->VBR_min_bitrate) {
      /* always use at least this many bits per granule per channel */
      /* unless we detect analog silence, see below */
      min_mean_bits=mean_bits/gfc->stereo;
    }
    frameBits[gfc->bitrate_index]
     = ResvFrameBegin (gfp,l3_side, mean_bits, bitsPerFrame);
  }


  gfc->bitrate_index=gfc->VBR_max_bitrate;
  
  /*******************************************************************
   * how many bits would we use of it?
   *******************************************************************/
  for (gr = 0; gr < gfc->mode_gr; gr++) {
    int num_chan=gfc->stereo;
    
    /* determine quality based on mid channel only */
    if (reduce_s_ch) { num_chan=1; }  

    for (ch = 0; ch < num_chan; ch++) { 
      int real_bits, min_pe_bits, mdct_lines=0;
      
      /******************************************************************
       * find smallest number of bits for an allowable quantization
       ******************************************************************/
      cod_info = &l3_side->gr[gr].ch[ch].tt;
      min_bits = Max(125,min_mean_bits);

      if (gfc->mode_ext==MPG_MD_MS_LR && ch==1) { 
        min_bits = Max(min_bits,0.2*save_bits[gr][0]);
      }
      mdct_lines = init_outer_loop(gfp,xr[gr][ch],xrpow[0], cod_info);
      memset(&scalefac[gr][ch],0,sizeof(III_scalefac_t));
      if (mdct_lines == 0)
      {
        /* xr contains no energy 
         * cod_info was set in init_outer_loop above
         */
        memset(l3_enc[gr][ch],0,576*sizeof(int));
        save_bits[gr][ch] = 0;
        if (gfp->gtkflag) {
          memset(xfsf,0,sizeof(xfsf));
          set_pinfo(gfp, cod_info, &ratio[gr][ch], &scalefac[gr][ch],
                    xr[gr][ch], xfsf, noise, gr, ch);
        }
        continue; /* with next channel */
      }
      
      memcpy(xrpow[1], xrpow[0], sizeof(xrpow[0]));
      memset( &bst_scalefac, 0, sizeof(III_scalefac_t) );
      memcpy( &bst_cod_info, cod_info, sizeof(gr_info) );
      
      if (cod_info->block_type==SHORT_TYPE) {
        /* if LAME switches to short blocks then pe is
         * >= 1000 on medium surge
         * >= 3000 on big surge
         */
        min_pe_bits = (pe[gr][ch]-350) * bands[gr][ch]/39.;
      } else {
        min_pe_bits = (pe[gr][ch]-350) * bands[gr][ch]/22.;
      }
      min_pe_bits=Min(min_pe_bits,1820);

      if (analog_silence && !gfp->VBR_hard_min) {
        min_bits = analog_mean_bits;
      } else {
        min_bits = Max(min_bits,min_pe_bits);
      }
      max_bits = 1200
               + frameBits[gfc->VBR_max_bitrate]/(gfc->stereo*gfc->mode_gr);
      max_bits = Min(max_bits,4095-195*(gfc->stereo-1));
      max_bits = Max(max_bits,min_bits);
      Max_bits = max_bits;

      this_bits = min_bits+(max_bits-min_bits)/2;
      real_bits = max_bits+1;

      /* bin search to within +40 bits of optimal */
      do {
        assert(this_bits>=min_bits);
        assert(this_bits<=max_bits);

        /*
         *  OK, start with a fresh setting
         *  - scalefac  will be set up by outer_loop
         *  - l3_enc    will be set up by outer_loop
         *  + cod_info  we will restore our initialized one, see below
         */
        outer_loop( gfp,xr[gr][ch], this_bits, noise, &l3_xmin[gr][ch],
                    l3_enc[gr][ch],&scalefac[gr][ch], cod_info,
                    xfsf, ch, xrpow[1]);

        /* is quantization as good as we are looking for ?
         */
        if (noise[0] <= 0) {
          /* we now know it can be done with "real_bits"
           * and maybe we can skip some iterations
           */
          real_bits = cod_info->part2_3_length;
          /*
           * save best quantization so far
           */
          memcpy( &bst_scalefac, &scalefac[gr][ch], sizeof(III_scalefac_t)  );
          memcpy( &bst_cod_info,  cod_info,         sizeof(gr_info)         );
          memcpy(  bst_l3_enc,    l3_enc  [gr][ch], sizeof(int)*576         );
          memcpy(  xrpow[0],      xrpow[1],         sizeof(xrpow[0])        );
          if (gfp->gtkflag) {
            set_pinfo(gfp, cod_info, &ratio[gr][ch], &scalefac[gr][ch],
                      xr[gr][ch], xfsf, noise, gr, ch);
            memcpy( &bst_pinfo, gfc->pinfo, sizeof(plotting_data) );
          }
          /* try with fewer bits
           */
          max_bits = real_bits-32;
          dbits = max_bits-min_bits;
          this_bits = min_bits+dbits/2;
        } else {
          /* try with more bits
           */
          min_bits = this_bits+32;
          dbits = max_bits-min_bits;
          this_bits = min_bits+dbits/2;
          if (dbits>8) {
            memcpy( &scalefac[gr][ch], &bst_scalefac, sizeof(III_scalefac_t) );
            memcpy(  cod_info,         &bst_cod_info, sizeof(gr_info)        );
            memcpy(  xrpow[1],          xrpow[0],     sizeof(xrpow[0])       );
          }
        }
      } while (dbits>8);

      if (real_bits <= Max_bits) {
        /* restore best quantization found */
        memcpy( &scalefac[gr][ch], &bst_scalefac, sizeof(III_scalefac_t) );
        memcpy(  cod_info,         &bst_cod_info, sizeof(gr_info)        );
        memcpy(  l3_enc  [gr][ch],  bst_l3_enc,   sizeof(int)*576        );
        if (gfp->gtkflag) {
          memcpy( gfc->pinfo, &bst_pinfo, sizeof(plotting_data) );
        }
      } else {
        /* we didn't find any satisfying quantization above
         * the only thing we still need to set is the gtk info field
         */
        if (gfp->gtkflag) {
          set_pinfo(gfp, cod_info, &ratio[gr][ch], &scalefac[gr][ch],
                    xr[gr][ch], xfsf, noise, gr, ch);
        }
      }

      assert((int)cod_info->part2_3_length <= Max_bits);
      assert((int)cod_info->part2_3_length < 4096);
      save_bits[gr][ch] = cod_info->part2_3_length;
      used_bits += save_bits[gr][ch];
      
    } /* for ch */
  }  /* for gr */

  if (reduce_s_ch) {
    /* number of bits needed was found for MID channel above.  Use formula
     * (fixed bitrate code) to set the side channel bits */
    for (gr = 0; gr < gfc->mode_gr; gr++) {
      FLOAT8 fac = .33*(.5-ms_ener_ratio[gr])/.5;
      save_bits[gr][1]=((1-fac)/(1+fac))*save_bits[gr][0];
      save_bits[gr][1]=Max(analog_mean_bits,save_bits[gr][1]);
      used_bits += save_bits[gr][1];
    }
  }

  /******************************************************************
   * find lowest bitrate able to hold used bits
   ******************************************************************/
  if (analog_silence && !gfp->VBR_hard_min) {
    gfc->bitrate_index = 1;
  } else {
    gfc->bitrate_index = gfc->VBR_min_bitrate;
  }
  for( ; gfc->bitrate_index < gfc->VBR_max_bitrate; gfc->bitrate_index++ ) {
    if( used_bits <= frameBits[gfc->bitrate_index] ) { break; }
  }

  /*******************************************************************
   * calculate quantization for this bitrate
   *******************************************************************/  
  getframebits (gfp,&bitsPerFrame, &mean_bits);
  bits=ResvFrameBegin (gfp,l3_side, mean_bits, bitsPerFrame);

  for(gr = 0; gr < gfc->mode_gr; gr++) {
    for(ch = 0; ch < gfc->stereo; ch++) {
      cod_info = &l3_side->gr[gr].ch[ch].tt;
      
      if (used_bits > bits) {
        /* repartion available bits in same proportion
         */
        save_bits[gr][ch] = (save_bits[gr][ch]*frameBits[gfc->bitrate_index])
                          / used_bits;
      }
      if (used_bits > bits || (reduce_s_ch && ch == 1)) {        
        memset(&scalefac[gr][ch],0,sizeof(III_scalefac_t));
        if (!init_outer_loop(gfp,xr[gr][ch], xrpow[0], cod_info)) {
          /* xr contains no energy 
           * cod_info was set in init_outer_loop above
           */
          memset(l3_enc[gr][ch],0,sizeof(l3_enc[gr][ch]));
          memset(noise,0,sizeof(noise));
        } else {
          outer_loop( gfp,xr[gr][ch], save_bits[gr][ch], noise,
                      &l3_xmin[gr][ch], l3_enc[gr][ch], &scalefac[gr][ch],
                      cod_info, xfsf, ch, xrpow[0]);
        }
        if (gfp->gtkflag) {
          set_pinfo(gfp, cod_info, &ratio[gr][ch], &scalefac[gr][ch],
                    xr[gr][ch], xfsf, noise, gr, ch);
        }
      }
      /* update reservoir status after FINAL quantization/bitrate
       */
      best_scalefac_store(gfp,gr, ch, l3_enc, l3_side, scalefac);
      if (gfc->use_best_huffman==1) {
        best_huffman_divide(gfc, gr, ch, cod_info, l3_enc[gr][ch]);
      }
      if (gfp->gtkflag) {
        gfc->pinfo->LAMEmainbits[gr][ch]=cod_info->part2_3_length;
      }
      ResvAdjust (gfp,cod_info, l3_side, mean_bits);
      
      /* set the sign of l3_enc from the sign of xr 
       */
      for ( i = 0; i < 576; i++) {
        if (xr[gr][ch][i] < 0) { l3_enc[gr][ch][i] *= -1; }
      }
    } /* ch */
  }  /* gr */

  ResvFrameEnd (gfp,l3_side, mean_bits);
}




INLINE 
int quant_compare(int experimentalX,
                  calc_noise_result *best,
                  calc_noise_result *calc)
{
  /*
    noise is given in decibals (db) relative to masking thesholds.

    over_noise:  sum of quantization noise > masking
    tot_noise:   sum of all quantization noise
    max_noise:   max quantization noise 

   */
  int better=0;

  switch (experimentalX) {
  default:
  case 0: better =   calc->over_count  < best->over_count
                 ||  ( calc->over_count == best->over_count
                       && calc->over_noise < best->over_noise )
                 ||  ( calc->over_count == best->over_count 
                       && calc->over_noise==best->over_noise
		       && calc->tot_noise < best->tot_noise)
	    ; break;


  case 1: better = calc->max_noise < best->max_noise; break;

  case 2: better = calc->tot_noise < best->tot_noise; break;
  
  case 3: better =  calc->tot_noise < best->tot_noise
                 && calc->max_noise < best->max_noise + 2; break;
  
  case 4: better =  (
                      (0>=calc->max_noise)
                    &&(best->max_noise>2)
                    )
                 || (
                      (0>=calc->max_noise)
                    &&(best->max_noise<0)
                    &&((best->max_noise+2)>calc->max_noise)
                    &&(calc->tot_noise<best->tot_noise)
                    )
                 || (
                      (0>=calc->max_noise)
                    &&(best->max_noise>0)
                    &&((best->max_noise+2)>calc->max_noise)
                    &&(calc->tot_noise<(best->tot_noise+best->over_noise))
                    )
                 || (
                      (0<calc->max_noise)
                    &&(best->max_noise>-0.5)
                    &&((best->max_noise+1)>calc->max_noise)
                    &&((  calc->tot_noise+calc->over_noise)
                        <(best->tot_noise+best->over_noise))
                    )
                 || (
                      (0<calc->max_noise)
                    &&(best->max_noise>-1)
                    &&((best->max_noise+1.5)>calc->max_noise)
                    &&((  calc->tot_noise+calc->over_noise+calc->over_noise)
                        <(best->tot_noise+best->over_noise+best->over_noise))
                    )
                  ; break;
     
  case 5: better =   calc->over_noise  < best->over_noise
                 ||( calc->over_noise == best->over_noise
                  && calc->tot_noise   < best->tot_noise ); break;
  
  case 6: better =     calc->over_noise  < best->over_noise
                 ||(   calc->over_noise == best->over_noise
                  &&(  calc->max_noise   < best->max_noise
                   ||( calc->max_noise  == best->max_noise
                    && calc->tot_noise  <= best->tot_noise ))); break;
  
  case 7: better =  calc->over_count < best->over_count
                 || calc->over_noise < best->over_noise; break;
  }

  return better;
}




/************************************************************************/
/*  init_outer_loop  mt 6/99                                            */
/*  returns 0 if all energies in xr are zero, else 1                    */
/************************************************************************/
int init_outer_loop(lame_global_flags *gfp, 
    FLOAT8 xr[576], FLOAT8 xrpow[576],     /*  could be L/R OR MID/SIDE */
    gr_info *cod_info)
{
  int i, o=0;

  cod_info->part2_3_length    = 0;
  cod_info->big_values        = 0;
  cod_info->count1            = 0;
  cod_info->global_gain       = 210;
  cod_info->scalefac_compress = 0;
  /* window_switching_flag */
  /* block_type            */
  /* mixed_block_flag      */
  cod_info->table_select[0]   = 0;
  cod_info->table_select[1]   = 0;
  cod_info->table_select[2]   = 0;
  cod_info->subblock_gain[0]  = 0;
  cod_info->subblock_gain[1]  = 0;
  cod_info->subblock_gain[2]  = 0;
  cod_info->region0_count     = 0;
  cod_info->region1_count     = 0;
  cod_info->preflag           = 0;
  cod_info->scalefac_scale    = 0;
  cod_info->count1table_select= 0;
  cod_info->part2_length      = 0;
  /* sfb_lmax              */
  /* sfb_smax              */
  cod_info->count1bits        = 0;  
  cod_info->sfb_partition_table = &nr_of_sfb_block[0][0][0];
  cod_info->slen[0] = 0;
  cod_info->slen[1] = 0;
  cod_info->slen[2] = 0;
  cod_info->slen[3] = 0;

  /*
   *  check if there is some energy we have to quantize
   *  if so, then return 1 else 0
   */
  for (i=0; i<576; i++) {
    FLOAT8 temp=fabs(xr[i]);
    xrpow[i]=sqrt(sqrt(temp)*temp);
    o += temp>1E-20;
  }
  
  return o>0;
}




/************************************************************************/
/*  outer_loop                                                         */
/************************************************************************/
/*  Function: The outer iteration loop controls the masking conditions  */
/*  of all scalefactorbands. It computes the best scalefac and          */
/*  global gain. This module calls the inner iteration loop             
 * 
 *  mt 5/99 completely rewritten to allow for bit reservoir control,   
 *  mid/side channels with L/R or mid/side masking thresholds, 
 *  and chooses best quantization instead of last quantization when 
 *  no distortion free quantization can be found.  
 *  
 *  added VBR support mt 5/99
 ************************************************************************/
void outer_loop(
    lame_global_flags *gfp,
    FLOAT8 xr[576],        
    int targ_bits,
    FLOAT8 best_noise[4],
    III_psy_xmin *l3_xmin,   /* the allowed distortion of the scalefactor */
    int l3_enc[576],         /* vector of quantized values ix(0..575) */
    III_scalefac_t *scalefac, /* scalefactors */
    gr_info *cod_info,
    FLOAT8 xfsf[4][SBMAX_l],
    int ch, 
    FLOAT8 xrpow[576])
{
  III_scalefac_t save_scalefac;
  gr_info save_cod_info;
  FLOAT8 save_xrpow[576];
  FLOAT8 xfsf_w[4][SBMAX_l];
  FLOAT8 distort[4][SBMAX_l];
  calc_noise_result noise_info;
  calc_noise_result best_noise_info;
  int l3_enc_w[576]; 
  int iteration;
  int status,bits_found=0;
  int huff_bits;
  int better;
  int over=0;
  lame_internal_flags *gfc=gfp->internal_flags;

  int notdone=1;

  noise_info.over_count = 100;
  noise_info.tot_count = 100;
  noise_info.max_noise = 0;
  noise_info.tot_noise = 0;
  noise_info.over_noise = 0;
  best_noise_info = noise_info;


  bits_found=bin_search_StepSize2(gfp,targ_bits,gfc->OldValue[ch],
                                  l3_enc_w,xrpow,cod_info);
  gfc->OldValue[ch] = cod_info->global_gain;


  /* BEGIN MAIN LOOP */
  iteration = 0;
  while ( notdone  ) {
    iteration += 1;


    /* inner_loop starts with the initial quantization step computed above
     * and slowly increases until the bits < huff_bits.
     * Thus it is important not to start with too large of an inital
     * quantization step.  Too small is ok, but inner_loop will take longer 
     */
    huff_bits = targ_bits - cod_info->part2_length;
    if (huff_bits < 0) {
      assert(iteration != 1);
      /* scale factors too large, not enough bits. use previous quantizaton */
      notdone=0;
    } else {
      /* if this is the first iteration, see if we can reuse the quantization
       * computed in bin_search_StepSize above */
      int real_bits;

      if (iteration==1) {
        if(bits_found>huff_bits) {
          cod_info->global_gain++;
          real_bits = inner_loop(gfp,xrpow, l3_enc_w, huff_bits, cod_info);
        } else {
          real_bits=bits_found;
        }
      } else {
        real_bits=inner_loop(gfp,xrpow, l3_enc_w, huff_bits, cod_info);
      }


      cod_info->part2_3_length = real_bits;

      /* compute the distortion in this quantization */
      if (gfc->noise_shaping==0) {
        over=0;
      } else {
        /* coefficients and thresholds both l/r (or both mid/side) */
        over = calc_noise( gfp,xr, l3_enc_w, cod_info, xfsf_w,distort,
                           l3_xmin, scalefac, &noise_info);
      }


      /* check if this quantization is better the our saved quantization */
      if (iteration == 1) {
        better=1;
      } else { 
        better=quant_compare(gfp->experimentalX, &best_noise_info, &noise_info);
      }
      /* save data so we can restore this quantization later */    
      if (better) {
        best_noise_info = noise_info;
        memcpy(&save_scalefac, scalefac, sizeof(III_scalefac_t));
        memcpy(&save_cod_info, cod_info, sizeof(save_cod_info) );
        memcpy( l3_enc,        l3_enc_w, sizeof(int)*576       );
        if (gfp->VBR==vbr_rh) {
          /* store for later reuse */
          memcpy(save_xrpow, xrpow, sizeof(save_xrpow));
        }

        if (gfp->gtkflag) {
          memcpy(xfsf, xfsf_w, sizeof(xfsf_w));
        }
      }
    }




    /* do early stopping on noise_shaping_stop = 0
     * otherwise stop only if tried to amplify all bands
     * or after noise_shaping_stop iterations are no distorted bands
     */ 

    if (gfc->noise_shaping_stop < iteration) 
    {
      /* if no bands with distortion and -X0, we are done */
      if ( 0==gfp->experimentalX
       && (0==over || best_noise_info.over_count==0))
      {
        notdone=0;
      }
      /* do at least 7 tries and stop 
       * if our best quantization so far had no distorted bands
       * this gives us more possibilities for our different quant_compare modes
       */
      if (iteration>7 && best_noise_info.over_count==0) {
        notdone=0;
      }
    }
    
    if (notdone) {
      amp_scalefac_bands( gfp, xrpow, cod_info, scalefac, distort);

      /* check to make sure we have not amplified too much */
      /* loop_break returns 0 if there is an unamplified scalefac */
      /* scale_bitcount returns 0 if no scalefactors are too large */
      status = loop_break(scalefac, cod_info);
      if ( status == 0 ) {
        /* not all scalefactors have been amplified.  so these 
         * scalefacs are possibly valid.  encode them: */
        if ( gfp->version == 1 ) {
          status = scale_bitcount(scalefac, cod_info);
        } else {
          status = scale_bitcount_lsf(scalefac, cod_info);
        }
        if (status) {
          /*  some scalefactors are too large.  lets try setting
           * scalefac_scale=1 */
          if (gfc->noise_shaping > 1 && !cod_info->scalefac_scale) {
            inc_scalefac_scale(gfp, scalefac, cod_info, xrpow);
            status = 0;
          } else {
            if (cod_info->block_type == SHORT_TYPE
               && gfp->experimentalZ && gfc->noise_shaping > 1) {
                inc_subblock_gain(gfp, scalefac, cod_info, xrpow);
                status = loop_break(scalefac, cod_info);
            }
          }
          if (!status) {
            if ( gfp->version == 1 ) {
              status = scale_bitcount(scalefac, cod_info);
            } else {
              status = scale_bitcount_lsf(scalefac, cod_info);
            }
          }
        } /* status != 0 */
      } /* status == 0 */
      notdone = !status;
    }

    /* Check if the last scalefactor band is distorted.
     * in VBR mode we can't get rid of the distortion, so quit now
     * and VBR mode will try again with more bits.  
     * (makes a 10% speed increase, the files I tested were
     * binary identical, 2000/05/20 Robert.Hegemann@gmx.de)
     * NOTE: distort[] = changed to:  noise/allowed noise
     * so distort[] > 1 means noise > allowed noise
     */
    if (gfp->VBR==vbr_rh || iteration>100) {
      if (cod_info->block_type == SHORT_TYPE) {
        if ((distort[1][SBMAX_s-1] > 1)
          ||(distort[2][SBMAX_s-1] > 1)
          ||(distort[3][SBMAX_s-1] > 1)) { notdone=0; }
      } else {
        if (distort[0][SBMAX_l-1] > 1) { notdone=0; }
      }
    }

  }    /* done with main iteration */
  memcpy(scalefac, &save_scalefac, sizeof(III_scalefac_t));
  memcpy(cod_info,&save_cod_info,sizeof(save_cod_info));
  if (gfp->VBR==vbr_rh) {
    /* restore for reuse on next try */
    memcpy(xrpow, save_xrpow, sizeof(save_xrpow));
  }
  cod_info->part2_3_length += cod_info->part2_length;


  /* finish up */
  assert( cod_info->global_gain < 256 );

  best_noise[0]=best_noise_info.over_count;
  best_noise[1]=best_noise_info.max_noise;
  best_noise[2]=best_noise_info.over_noise;
  best_noise[3]=best_noise_info.tot_noise;
}





  














/*************************************************************************/
/*            amp_scalefac_bands                                         */
/*************************************************************************/

/* 
  Amplify the scalefactor bands that violate the masking threshold.
  See ISO 11172-3 Section C.1.5.4.3.5
*/
#ifndef RH_AMP
void amp_scalefac_bands(lame_global_flags *gfp, FLOAT8 xrpow[576], 
                        gr_info *cod_info, III_scalefac_t *scalefac,
                        FLOAT8 distort[4][SBMAX_l])
{
  int start, end, l,i,j;
  u_int sfb;
  FLOAT8 ifqstep34,distort_thresh;
  lame_internal_flags *gfc=gfp->internal_flags;

  if ( cod_info->scalefac_scale == 0 ) {
    ifqstep34 = 1.29683955465100964055; /* 2**(.75*.5)*/
  } else {
    ifqstep34 = 1.68179283050742922612;  /* 2**(.75*1) */
  }
  /* distort[] = noise/masking.  Comput distort_thresh so that:
   * distort_thresh = 1, unless all bands have distort < 1
   * In that case, just amplify bands with distortion
   * within 95% of largest distortion/masking ratio */
  distort_thresh = -900;
  for ( sfb = 0; sfb < cod_info->sfb_lmax; sfb++ ) {
    distort_thresh = Max(distort[0][sfb],distort_thresh);
  }

  for ( sfb = cod_info->sfb_smax; sfb < 12; sfb++ ) {
    for ( i = 0; i < 3; i++ ) {
      distort_thresh = Max(distort[i+1][sfb],distort_thresh);
    }
  }
  if (distort_thresh>1.0)
    distort_thresh=1.0;
  else
    distort_thresh *= .95;


  for ( sfb = 0; sfb < cod_info->sfb_lmax; sfb++ ) {
    if ( distort[0][sfb]>distort_thresh  ) {
      scalefac->l[sfb]++;
      start = gfc->scalefac_band.l[sfb];
      end   = gfc->scalefac_band.l[sfb+1];
      for ( l = start; l < end; l++ ) {
        xrpow[l] *= ifqstep34;
      }
    }
  }
    

  for ( j=0,sfb = cod_info->sfb_smax; sfb < 12; sfb++ ) {
    start = gfc->scalefac_band.s[sfb];
    end   = gfc->scalefac_band.s[sfb+1];
    for ( i = 0; i < 3; i++ ) {
      int j2 = j;
      if ( distort[i+1][sfb]>distort_thresh) {
        scalefac->s[sfb][i]++;
        for (l = start; l < end; l++) {
          xrpow[j2++] *= ifqstep34;
        }
      }
      j += end-start;
    }
  }
}
#else
void amp_scalefac_bands(lame_global_flags *gfp, FLOAT8 xrpow[576], 
                        gr_info *cod_info, III_scalefac_t *scalefac,
                        FLOAT8 distort[4][SBMAX_l])
{
  int start, end, l,i,j, max_ind[4]={0,0,0,0};
  u_int sfb;
  FLOAT8 ifqstep34,distort_thresh[4]={1E-20,1E-20,1E-20,1E-20};
  lame_internal_flags *gfc=gfp->internal_flags;

  if ( cod_info->scalefac_scale == 0 ) {
    ifqstep34 = 1.29683955465100964055; /* 2**(.75*.5)*/
  } else {
    ifqstep34 = 1.68179283050742922612;  /* 2**(.75*1) */
  }
  /* distort[] = noise/masking.  Comput distort_thresh so that:
   * distort_thresh = 1, unless all bands have distort < 1
   * In that case, just amplify bands with distortion
   * within 95% of largest distortion/masking ratio */
  for ( sfb = 0; sfb < cod_info->sfb_lmax; sfb++ ) {
    if (distort[0][sfb] >= distort_thresh[0]) {
      distort_thresh[0]  = distort[0][sfb];
      max_ind[0] = sfb;
    }
  }
  if (distort_thresh[0]>1.0) {
    distort_thresh[0] = 1.0;
  } else {
    distort_thresh[0] = pow(distort_thresh[0], 1.05);
  }
  for ( i = 1; i < 4; i++ ) {
    for ( sfb = cod_info->sfb_smax; sfb < 12; sfb++ ) {
      if (distort[i][sfb] >= distort_thresh[i]) {
        distort_thresh[i]  = distort[i][sfb];
        max_ind[i] = sfb;
      }
    }
    if (distort_thresh[i]>1.0) {
      distort_thresh[i] = 1.0;
    } else {
      distort_thresh[i] = pow(distort_thresh[i], 1.05);
    }
  }
  
  for ( sfb = 0; sfb < cod_info->sfb_lmax; sfb++ ) {
    if ( distort[0][sfb] > distort_thresh[0]
      &&(sfb==max_ind[0] || !gfp->experimentalY) )
    {
      scalefac->l[sfb]++;
      start = gfc->scalefac_band.l[sfb];
      end   = gfc->scalefac_band.l[sfb+1];
      for ( l = start; l < end; l++ ) {
        xrpow[l] *= ifqstep34;
      }
    }
  }
    
  for ( j=0,sfb = cod_info->sfb_smax; sfb < 12; sfb++ ) {
    start = gfc->scalefac_band.s[sfb];
    end   = gfc->scalefac_band.s[sfb+1];
    for ( i = 0; i < 3; i++ ) {
      int j2 = j;
      if ( distort[i+1][sfb] > distort_thresh[i+1]
        &&(sfb==max_ind[i+1] || !gfp->experimentalY))
      {
        scalefac->s[sfb][i]++;
        for (l = start; l < end; l++) {
          xrpow[j2++] *= ifqstep34;
        }
      }
      j += end-start;
    }
  }
}
#endif

void inc_scalefac_scale(lame_global_flags *gfp, III_scalefac_t *scalefac,
                        gr_info *cod_info, FLOAT8 xrpow[576])
{
  int start, end, l,i,j;
  int sfb;
  lame_internal_flags *gfc=gfp->internal_flags;
  const FLOAT8 ifqstep34 = 1.29683955465100964055;

  for ( sfb = 0; sfb < cod_info->sfb_lmax; sfb++ ) {
    int s = scalefac->l[sfb];
    if (cod_info->preflag) {
       s += pretab[sfb];
    }
    if (s & 1) {
      s++;
      start = gfc->scalefac_band.l[sfb];
      end   = gfc->scalefac_band.l[sfb+1];
      for ( l = start; l < end; l++ ) {
        xrpow[l] *= ifqstep34;
      }
    }
    scalefac->l[sfb] = s >> 1;
    cod_info->preflag = 0;
  }

  for ( j=0,sfb = cod_info->sfb_smax; sfb < 12; sfb++ ) {
    start = gfc->scalefac_band.s[sfb];
    end   = gfc->scalefac_band.s[sfb+1];
    for ( i = 0; i < 3; i++ ) {
      int j2=j;
      if (scalefac->s[sfb][i] & 1) {
        scalefac->s[sfb][i]++;
        for (l = start; l < end; l++) {
          xrpow[j2++] *= ifqstep34;
        }
      }
      scalefac->s[sfb][i] >>= 1;
      j += end-start;
    }
  }
  cod_info->scalefac_scale = 1;
}

void inc_subblock_gain(lame_global_flags *gfp, III_scalefac_t *scalefac,
                       gr_info *cod_info, FLOAT8 xrpow[576])
{
  int start, end, l,i;
  int sfb;
  lame_internal_flags *gfc=gfp->internal_flags;

  fun_reorder(gfc->scalefac_band.s,xrpow);

  for ( i = 0; i < 3; i++ ) {
    if (cod_info->subblock_gain[i] >= 7) {
      continue;
    }
    for ( sfb = cod_info->sfb_smax; sfb < 12; sfb++ ) {
      if (scalefac->s[sfb][i] >= 8) {
        break;
      }
    }
    if (sfb == 12) {
      continue;
    }
    for ( sfb = cod_info->sfb_smax; sfb < 12; sfb++ ) {
      if (scalefac->s[sfb][i] >= 2) {
        scalefac->s[sfb][i] -= 2;
      } else {
        FLOAT8 amp = pow(2.0, 0.75*(2 - scalefac->s[sfb][i]));
        scalefac->s[sfb][i] = 0;
        start = gfc->scalefac_band.s[sfb];
        end   = gfc->scalefac_band.s[sfb+1];
        for (l = start; l < end; l++) {
          xrpow[l * 3 + i] *= amp;
        }
      }
    }
    {
      FLOAT8 amp = pow(2.0, 0.75*2);
      start = gfc->scalefac_band.s[sfb];
      for (l = start; l < 192; l++) {
        xrpow[l * 3 + i] *= amp;
      }
    }
    cod_info->subblock_gain[i]++;
  }

  freorder(gfc->scalefac_band.s,xrpow);
}
