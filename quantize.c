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


/*
    OVERVIEW:

    quantize.c provides:
    - iteration_loop()
    - ABR_iteration_loop()
    - VBR_iteration_loop()
    
    they use:
    
    iteration_loop
     - iteration_init          quantize_pvt.c
     - getframebits            util.c
     - ResvFrameBegin          reservoir.c
     + on_pe
        - ResvMaxBits          takehiro.c
     - ms_convert              quantize_pvt.c
     + reduce_side
     + init_outer_loop
     - calc_xmin               quantize_pvt.c
     + outer_loop
        + bin_search_Stepsize
           - count_bits        takehiro.c
        + inner_loop
           - count_bits        takehiro.c
        - calc_noise           quantize_pvt.c
        + quant_compare
        + amp_scalefac_bands
        + loop_break
        - scale_bitcount       takehiro.c
        - scale_bitcount_lsf   takehiro.c
        + inc_scalefac_scale
        + inc_subblock_gain
           - fun_reorder       util.c
           - freorder          util.c
     - set_pinfo               quantize_pvt.c
     - best_scalefac_store     takehiro.c
     - best_huffman_divide     takehiro.c
     - ResvAdjust              reservoir.c
     - ResvFrameEnd            reservoir.c
     
    ABR_iteration_loop
     - iteration_init          quantize_pvt.c
     - getframebits            util.c
     - ResvFrameBegin          reservoir.c
     + reduce_side
     - ms_convert              quantize_pvt.c
     + init_outer_loop
     - calc_xmin               quantize_pvt.c
     + outer_loop
        + bin_search_Stepsize
           - count_bits        takehiro.c
        + inner_loop
           - count_bits        takehiro.c
        - calc_noise           quantize_pvt.c
        + quant_compare
        + amp_scalefac_bands
        + loop_break
        - scale_bitcount       takehiro.c
        - scale_bitcount_lsf   takehiro.c
        + inc_scalefac_scale
        + inc_subblock_gain
           - fun_reorder       util.c
           - freorder          util.c
     - set_pinfo               quantize_pvt.c
     - best_scalefac_store     takehiro.c
     - best_huffman_divide     takehiro.c
     - ResvAdjust              reservoir.c
     - ResvFrameEnd            reservoir.c
     
    VBR_iteration_loop
     - iteration_init          quantize_pvt.c
     - getframebits            util.c
     - ms_convert              quantize_pvt.c
     - calc_xmin               quantize_pvt.c
     - ResvFrameBegin          reservoir.c
     + init_outer_loop
     + outer_loop
        + bin_search_Stepsize
           - count_bits        takehiro.c
        + inner_loop
           - count_bits        takehiro.c
        - calc_noise           quantize_pvt.c
        + quant_compare
        + amp_scalefac_bands
        + loop_break
        - scale_bitcount       takehiro.c
        - scale_bitcount_lsf   takehiro.c
        + inc_scalefac_scale
        + inc_subblock_gain
           - fun_reorder       util.c
           - freorder          util.c
     - set_pinfo               quantize_pvt.c
     - best_scalefac_store     takehiro.c
     - best_huffman_divide     takehiro.c
     - ResvAdjust              reservoir.c
     - ResvFrameEnd            reservoir.c
    
 */



#include <assert.h>
#include "util.h"
#include "l3side.h"
#include "quantize.h"
#include "reservoir.h"
#include "quantize_pvt.h"
#include "gtkanal.h"




/************************************************************************
 *
 *  init_outer_loop()
 *  mt 6/99                                    
 *
 *  returns 0 if all energies in xr are zero, else 1                    
 *
 ************************************************************************/

int init_outer_loop (
    gr_info        *cod_info, 
    III_scalefac_t *scalefac, 
    FLOAT8          xr[576], 
    FLOAT8          xrpow[576] )
{
  int i, o=0;

  /*  initialize fresh cod_info
   */
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

  /*  fresh scalefactors are all zero
   */
  memset( scalefac, 0, sizeof(III_scalefac_t) );
  
  /*  check if there is some energy we have to quantize
   *  and calculate xrpow matching our fresh scalefactors
   */
  for (i=0; i<576; i++) {
    FLOAT8 temp = fabs( xr[i] );
    xrpow[i] = sqrt( temp * sqrt(temp) );
    o += temp > 1E-20;
  }
  
  /*  return 1 if we have something to quantize, else 0
   */
  return o > 0;
}



/************************************************************************
 *
 *  bin_search_StepSize()
 *
 *  binary step size search
 *  used by outer_loop to get a quantizer step size to start with
 *
 ************************************************************************/

typedef enum {
    BINSEARCH_NONE,
    BINSEARCH_UP, 
    BINSEARCH_DOWN
} binsearchDirection_t;

int bin_search_StepSize (
    lame_internal_flags *gfc,
    gr_info             *cod_info,
    int                  desired_rate, 
    int                  start, 
    int                 *ix, 
    FLOAT8               xrspow[576]) 
{
    int nBits;
    int CurrentStep;
    int flag_GoneOver = 0;
    int StepSize      = start;

    binsearchDirection_t Direction = BINSEARCH_NONE;
    assert(gfc->CurrentStep);
    CurrentStep = gfc->CurrentStep;

    do {
        cod_info->global_gain = StepSize;
        nBits = count_bits (gfc, cod_info, xrspow, ix);  

        if (CurrentStep == 1 ) break; /* nothing to adjust anymore */
    
        if (flag_GoneOver) CurrentStep /= 2;
 
        if (nBits > desired_rate) {  
            /* increase Quantize_StepSize */
            if (Direction == BINSEARCH_DOWN && !flag_GoneOver) {
                flag_GoneOver = 1;
                CurrentStep  /= 2; /* late adjust */
            }
            Direction = BINSEARCH_UP;
            StepSize += CurrentStep;
            if (StepSize > 255) break;
        }
        else if (nBits < desired_rate) {
            /* decrease Quantize_StepSize */
            if (Direction == BINSEARCH_UP && !flag_GoneOver) {
                flag_GoneOver = 1;
                CurrentStep  /= 2; /* late adjust */
            }
            Direction = BINSEARCH_DOWN;
            StepSize -= CurrentStep;
            if (StepSize < 0) break;
        }
        else break; /* nBits == desired_rate;; most unlikely to happen.*/
    } while (1); /* For-ever, break is adjusted. */

    CurrentStep = abs(start - StepSize);
    
    if (CurrentStep >= 4) 
        CurrentStep = 4;
    else
        CurrentStep = 2;

    gfc->CurrentStep = CurrentStep;

    return nBits;
}




/*************************************************************************** 
 *
 *         inner_loop ()                                                     
 *
 *  The code selects the best global gain for a particular set of scalefacs 
 *
 ***************************************************************************/ 
 
int inner_loop (
    lame_internal_flags *gfc,
    gr_info             *cod_info,
    FLOAT8               xrpow[576],
    int                  l3_enc[576], 
    int                  max_bits )
{
    int bits;
    assert( max_bits >= 0 );
    cod_info->global_gain--;

    do {
        cod_info->global_gain++;
        bits = count_bits (gfc, cod_info, xrpow, l3_enc);
    }
    while ( bits > max_bits );

    return bits;
}



/*************************************************************************
 *
 *   loop_break()                                               
 *
 *   Function: Returns zero if there is a scalefac which has not been
 *   amplified. Otherwise it returns one. 
 *
 *************************************************************************/

INLINE 
int loop_break ( 
    gr_info        *cod_info,
    III_scalefac_t *scalefac ) 
{
    int i;
    u_int sfb;

    for ( sfb = 0; sfb < cod_info->sfb_lmax; sfb++ )
        if ( scalefac->l[sfb] == 0 )
            return 0;

    for ( sfb = cod_info->sfb_smax; sfb < SBPSY_s; sfb++ )
        for ( i = 0; i < 3; i++ ) 
            if ( scalefac->s[sfb][i]+cod_info->subblock_gain[i] == 0 )
                return 0;

    return 1;
}




/*************************************************************************
 *
 *   quant_compare()                                               
 *
 *************************************************************************/

INLINE 
int quant_compare (
    int                experimentalX,
    calc_noise_result *best,
    calc_noise_result *calc )
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
                   ||( calc->over_count == best->over_count
                    && calc->over_noise  < best->over_noise )
                   ||( calc->over_count == best->over_count 
                    && calc->over_noise == best->over_noise
                    && calc->tot_noise   < best->tot_noise  ); break;


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



/*************************************************************************
 *
 *            amp_scalefac_bands() 
 *        
 *  Amplify the scalefactor bands that violate the masking threshold.
 *  See ISO 11172-3 Section C.1.5.4.3.5
 *
 *************************************************************************/

void amp_scalefac_bands(
    lame_global_flags *gfp, 
    gr_info           *cod_info, 
    III_scalefac_t    *scalefac,
    FLOAT8             xrpow[576], 
    FLOAT8             distort[4][SBMAX_l] )
{
  int start, end, l,i,j, max_ind[4]={0,0,0,0};
  u_int sfb;
  FLOAT8 ifqstep34,distort_thresh[4]={1E-20,1E-20,1E-20,1E-20};
  lame_internal_flags *gfc = (lame_internal_flags *)gfp->internal_flags;

  if ( cod_info->scalefac_scale == 0 ) {
    ifqstep34 = 1.29683955465100964055; /* 2**(.75*.5)*/
  } else {
    ifqstep34 = 1.68179283050742922612;  /* 2**(.75*1) */
  }
  /* distort[] = noise/masking.  Compute distort_thresh so that:
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



/*************************************************************************
 *
 *  inc_scalefac_scale()
 *
 *************************************************************************/
 
void inc_scalefac_scale(
    lame_internal_flags *gfc, 
    gr_info             *cod_info, 
    III_scalefac_t      *scalefac,
    FLOAT8               xrpow[576] )
{
  int start, end, l,i,j;
  unsigned int sfb;
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



/*************************************************************************
 *
 *  inc_subblock_gain()
 *
 *************************************************************************/
 
void inc_subblock_gain(
    lame_internal_flags *gfc,
    gr_info             *cod_info,
    III_scalefac_t      *scalefac,
    FLOAT8               xrpow[576] )
{
  int start, end, l,i;
  int sfb;

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



/************************************************************************
 *
 *  outer_loop ()                                                       
 *
 *  Function: The outer iteration loop controls the masking conditions  
 *  of all scalefactorbands. It computes the best scalefac and          
 *  global gain. This module calls the inner iteration loop             
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
    gr_info           *cod_info,
    int                ch, 
    int                targ_bits,   /* maximum allowed bits */
    FLOAT8             xr[576],     /* magnitudes of spectral values */
    III_psy_xmin      *l3_xmin,     /* allowed distortion of the scalefactor */
    III_scalefac_t    *scalefac,    /* scalefactors */
    FLOAT8             xrpow[576],  /* coloured magnitudes of spectral values */
    int                l3_enc[576], /* vector of quantized values ix(0..575) */
    FLOAT8             best_noise[4] )
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
  lame_internal_flags *gfc = (lame_internal_flags *)gfp->internal_flags;

  int notdone=1;

  noise_info.over_count = 100;
  noise_info.tot_count = 100;
  noise_info.max_noise = 0;
  noise_info.tot_noise = 0;
  noise_info.over_noise = 0;
  best_noise_info = noise_info;


  bits_found = bin_search_StepSize (gfc, cod_info, targ_bits, gfc->OldValue[ch],
                                    l3_enc_w, xrpow);
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
          real_bits = inner_loop(gfc, cod_info, xrpow, l3_enc_w, huff_bits);
        } else {
          real_bits=bits_found;
        }
      } else {
        real_bits=inner_loop(gfc, cod_info, xrpow, l3_enc_w, huff_bits);
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
      amp_scalefac_bands( gfp, cod_info, scalefac, xrpow, distort);

      /* check to make sure we have not amplified too much */
      /* loop_break returns 0 if there is an unamplified scalefac */
      /* scale_bitcount returns 0 if no scalefactors are too large */
      status = loop_break (cod_info, scalefac);
      if ( status == 0 ) {
        /* not all scalefactors have been amplified.  so these 
         * scalefacs are possibly valid.  encode them: */
        if ( gfp->version == 1 ) {
          status = scale_bitcount (cod_info, scalefac);
        } else {
          status = scale_bitcount_lsf (cod_info, scalefac);
        }
        if (status) {
          /*  some scalefactors are too large.  lets try setting
           * scalefac_scale=1 */
          if (gfc->noise_shaping > 1 && !cod_info->scalefac_scale) {
            inc_scalefac_scale (gfc, cod_info, scalefac, xrpow);
            status = 0;
          } else {
            if (cod_info->block_type == SHORT_TYPE
               && gfp->experimentalZ && gfc->noise_shaping > 1) {
                inc_subblock_gain (gfc, cod_info, scalefac, xrpow);
                status = loop_break (cod_info, scalefac);
            }
          }
          if (!status) {
            if ( gfp->version == 1 ) {
              status = scale_bitcount (cod_info, scalefac);
            } else {
              status = scale_bitcount_lsf (cod_info, scalefac);
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
  memcpy (cod_info, &save_cod_info, sizeof(save_cod_info) );
  memcpy (scalefac, &save_scalefac, sizeof(III_scalefac_t));
  if (gfp->VBR==vbr_rh) {
    /* restore for reuse on next try */
    memcpy (xrpow, save_xrpow, sizeof(save_xrpow));
  }
  cod_info->part2_3_length += cod_info->part2_length;


  /* finish up */
  assert( cod_info->global_gain < 256 );

  best_noise[0] = best_noise_info.over_count;
  best_noise[1] = best_noise_info.max_noise;
  best_noise[2] = best_noise_info.over_noise;
  best_noise[3] = best_noise_info.tot_noise;
}




/************************************************************************
 *
 *  on_pe()
 *
 * allocate bits among 2 channels based on PE
 * mt 6/99
 *
 ************************************************************************/

int on_pe (
    lame_global_flags *gfp,
    FLOAT8 pe[2][2],
    III_side_info_t *l3_side,
    int targ_bits[2],
    int mean_bits, 
    int gr )
{
  lame_internal_flags *gfc=gfp->internal_flags;
  gr_info *cod_info;
  int extra_bits,tbits,bits;
  int add_bits[2]; 
  int ch;
  int max_bits;  /* maximum allowed bits for this granule */

  /* allocate targ_bits for granule */
  ResvMaxBits(gfp, mean_bits, &tbits, &extra_bits);
  max_bits=tbits+extra_bits;

  bits=0;

  for (ch=0 ; ch < gfc->channels ; ch ++) {
    /******************************************************************
     * allocate bits for each channel 
     ******************************************************************/
    cod_info = &l3_side->gr[gr].ch[ch].tt;
    
    targ_bits[ch]=Min(4095,tbits/gfc->channels);
    
    add_bits[ch]=(pe[gr][ch]-750)/1.4;
    /* short blocks us a little extra, no matter what the pe */
    if (cod_info->block_type==SHORT_TYPE) {
      if (add_bits[ch]<mean_bits/4) add_bits[ch]=mean_bits/4;
    }

    /* at most increase bits by 1.5*average */
    if (add_bits[ch] > .75*mean_bits) add_bits[ch]=mean_bits*.75;
    if (add_bits[ch] < 0) add_bits[ch]=0;

    if ((targ_bits[ch]+add_bits[ch]) > 4095) 
      add_bits[ch]=Max(0,4095-targ_bits[ch]);

    bits += add_bits[ch];
  }
  if (bits > extra_bits)
    for (ch=0 ; ch < gfc->channels ; ch ++) {
      add_bits[ch] = (extra_bits*add_bits[ch])/bits;
    }

  for (ch=0 ; ch < gfc->channels ; ch ++) {
    targ_bits[ch] = targ_bits[ch] + add_bits[ch];
    extra_bits -= add_bits[ch];
  }
  return max_bits;
}



/************************************************************************
 *
 *  reduce_side()
 *
 *  allocate bits between mid and side channel
 *
 ************************************************************************/

void reduce_side (
    int targ_bits[2],
    FLOAT8 ms_ener_ratio,
    int mean_bits,
    int max_bits )
{
  int move_bits;
  FLOAT fac;


  /*  ms_ener_ratio = 0:  allocate 66/33  mid/side  fac=.33  
   *  ms_ener_ratio =.5:  allocate 50/50 mid/side   fac= 0 
   * 75/25 split is fac=.5 */
  /* float fac = .50*(.5-ms_ener_ratio[gr])/.5;*/
  fac = .33*(.5-ms_ener_ratio)/.5;
  if (fac< 0) fac=0;
  if (fac>.5) fac=.5;
  
  /* number of bits to move from side channel to mid channel */
  /*    move_bits = fac*targ_bits[1];  */
  move_bits = fac*.5*(targ_bits[0]+targ_bits[1]);  

  if ((move_bits + targ_bits[0]) > 4095) {
    move_bits = 4095 - targ_bits[0];
  }
  if (move_bits<0) move_bits=0;
    
  if (targ_bits[1] >= 125) {
    /* dont reduce side channel below 125 bits */
    if (targ_bits[1]-move_bits > 125) {

      /* if mid channel already has 2x more than average, dont bother */
      /* mean_bits = bits per granule (for both channels) */
      if (targ_bits[0] < mean_bits)
        targ_bits[0] += move_bits;
      targ_bits[1] -= move_bits;
    } else {
      targ_bits[0] += targ_bits[1] - 125;
      targ_bits[1] = 125;
    }
  }
    
  move_bits=targ_bits[0]+targ_bits[1];
  if (move_bits > max_bits) {
    targ_bits[0]=(max_bits*targ_bits[0])/move_bits;
    targ_bits[1]=(max_bits*targ_bits[1])/move_bits;
  }
}


/************************************************************************
 *
 *  iteration_loop()                                                    
 *
 ************************************************************************/

void iteration_loop (
    lame_global_flags *gfp, 
    FLOAT8 pe[2][2],
    FLOAT8 ms_ener_ratio[2],  
    FLOAT8 xr[2][2][576],
    III_psy_ratio ratio[2][2],  
    int l3_enc[2][2][576],
    III_scalefac_t scalefac[2][2] )
{
  III_psy_xmin l3_xmin[2];
  FLOAT8 xrpow[576];
  FLOAT8 noise[4]; /* over,max_noise,over_noise,tot_noise; */
  int targ_bits[2];
  int bitsPerFrame;
  int mean_bits,max_bits;
  int ch, gr, i, bit_rate;
  gr_info *cod_info;
  lame_internal_flags *gfc = (lame_internal_flags*)gfp->internal_flags;
  III_side_info_t *l3_side = &gfc->l3_side;

  iteration_init(gfp,l3_side,l3_enc);
  bit_rate = index_to_bitrate [gfp->version] [gfc->bitrate_index];
  getframebits(gfp,&bitsPerFrame, &mean_bits);
  ResvFrameBegin(gfp, l3_side, mean_bits, bitsPerFrame );

  /* quantize! */
  for ( gr = 0; gr < gfc->mode_gr; gr++ ) {

    max_bits=on_pe(gfp,pe,l3_side,targ_bits,mean_bits, gr);
    if (gfc->mode_ext==MPG_MD_MS_LR) {
      ms_convert(xr[gr], xr[gr]);
      reduce_side(targ_bits,ms_ener_ratio[gr],mean_bits,max_bits);
    }
    for (ch=0 ; ch < gfc->channels ; ch ++) {
      cod_info = &l3_side->gr[gr].ch[ch].tt; 

      if (!init_outer_loop( cod_info, &scalefac[gr][ch], xr[gr][ch], xrpow )) {
        /*  xr contains no energy 
         *  cod_info and scalefac was initialized in init_outer_loop
         *  l3_enc, our encoding data, will be quantized to zero
         */
        memset(l3_enc[gr][ch],0,sizeof(l3_enc[gr][ch]));
      }
      else {
        /*  xr contains energy we will have to encode 
         *  cod_info and scalefac was initialized in init_outer_loop
         *  calculate the masking abilities
         *  find some good quantization in outer_loop 
         */
        calc_xmin ( gfp, xr[gr][ch], &ratio[gr][ch], cod_info, &l3_xmin[ch] );
        outer_loop( gfp, cod_info, ch, targ_bits[ch], xr[gr][ch], &l3_xmin[ch],
                    &scalefac[gr][ch], xrpow, l3_enc[gr][ch], noise );
      }

      if (gfp->gtkflag) {
        set_pinfo (gfp, cod_info, &ratio[gr][ch], &scalefac[gr][ch], 
                   xr[gr][ch], l3_enc[gr][ch], gr, ch);
      }

      best_scalefac_store(gfc, l3_side, scalefac, l3_enc,gr, ch);
      if (gfc->use_best_huffman==1) {
        best_huffman_divide(gfc, cod_info, l3_enc[gr][ch], gr, ch);
      }
      assert((int)cod_info->part2_3_length < 4096);

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
    for ( ch =  0; ch < gfc->channels; ch++ ) {
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
void ABR_iteration_loop (
    lame_global_flags *gfp, 
    FLOAT8             pe[2][2],
    FLOAT8             ms_ener_ratio[2], 
    FLOAT8             xr[2][2][576],
    III_psy_ratio      ratio[2][2], 
    int                l3_enc[2][2][576],
    III_scalefac_t     scalefac[2][2] )
{
  III_psy_xmin l3_xmin;
  FLOAT8    xrpow[576];
  FLOAT8    noise[4];
  int       targ_bits[2][2];
  int       bit_rate,bitsPerFrame, mean_bits,totbits,max_frame_bits;
  int       i,ch, gr, ath_over;
  int       analog_silence_bits;
  FLOAT8    res_factor;
  gr_info  *cod_info = NULL;
  lame_internal_flags *gfc = (lame_internal_flags*)gfp->internal_flags;
  III_side_info_t *l3_side = &gfc->l3_side;

  iteration_init(gfp,l3_side,l3_enc);

  gfc->bitrate_index = gfc->VBR_max_bitrate;
  getframebits (gfp,&bitsPerFrame, &mean_bits);
  max_frame_bits=ResvFrameBegin (gfp,l3_side, mean_bits, bitsPerFrame);

  gfc->bitrate_index = 1;
  getframebits (gfp,&bitsPerFrame, &mean_bits);
  analog_silence_bits = mean_bits/gfc->channels;

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
    for(ch = 0; ch < gfc->channels; ch++) {
      targ_bits[gr][ch]=(int)(res_factor*(mean_bits/gfc->channels));
      if (pe[gr][ch]>700) {
        int add_bits=(int)((pe[gr][ch]-700)/1.4);
 
        cod_info = &l3_side->gr[gr].ch[ch].tt;
        targ_bits[gr][ch]=(int)(res_factor*(mean_bits/gfc->channels));
 
        /* short blocks use a little extra, no matter what the pe */
        if (cod_info->block_type==SHORT_TYPE) {
          if (add_bits<mean_bits/4) { add_bits=mean_bits/4; }
        }
        /* at most increase bits by 1.5*average */
        if (add_bits > mean_bits*3/4) { add_bits=mean_bits*3/4; }
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
    for(ch = 0; ch < gfc->channels; ch++) {
      if (targ_bits[gr][ch] > 4095) { targ_bits[gr][ch]=4095; }
      totbits += targ_bits[gr][ch];
    }
  }

  if (totbits > max_frame_bits) {
    for(gr = 0; gr < gfc->mode_gr; gr++) {
      for(ch = 0; ch < gfc->channels; ch++) {
        targ_bits[gr][ch] = targ_bits[gr][ch]*max_frame_bits/totbits; 
      }
    }
  }

  totbits=0;
  for(gr = 0; gr < gfc->mode_gr; gr++) {

    if (gfc->mode_ext==MPG_MD_MS_LR) { ms_convert(xr[gr], xr[gr]); }

    for(ch = 0; ch < gfc->channels; ch++) {
      cod_info = &l3_side->gr[gr].ch[ch].tt;

      if (!init_outer_loop(cod_info,&scalefac[gr][ch],xr[gr][ch],xrpow)) {
        /*  xr contains no energy 
         *  cod_info and scalefac was initialized in init_outer_loop
         *  l3_enc, our encoding data, will be quantized to zero
         */
        memset(l3_enc[gr][ch],0,576*sizeof(int));
      } 
      else {
        /*  xr contains energy we will have to encode 
         *  cod_info and scalefac was initialized in init_outer_loop
         *  calculate the masking abilities
         *  find some good quantization in outer_loop 
         */
        ath_over = calc_xmin(gfp,xr[gr][ch],&ratio[gr][ch],cod_info,&l3_xmin);
        if (0==ath_over) {
          /* analog silence */
          targ_bits[gr][ch]=analog_silence_bits;
        }

        outer_loop( gfp, cod_info, ch, targ_bits[gr][ch], xr[gr][ch], &l3_xmin,
                    &scalefac[gr][ch], xrpow, l3_enc[gr][ch], noise );
      }

      totbits += cod_info->part2_3_length;
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
    for (ch = 0; ch < gfc->channels; ch++) {
      cod_info = &l3_side->gr[gr].ch[ch].tt;

      if (gfp->gtkflag) {
        set_pinfo(gfp, cod_info, &ratio[gr][ch], &scalefac[gr][ch],
                  xr[gr][ch], l3_enc[gr][ch], gr, ch);
      }
      best_scalefac_store(gfc, l3_side, scalefac, l3_enc, gr, ch);
      if (gfc->use_best_huffman==1 ) {
        best_huffman_divide(gfc, cod_info, l3_enc[gr][ch], gr, ch);
      }
      ResvAdjust (gfp, cod_info, l3_side, mean_bits);
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

void VBR_iteration_loop (
    lame_global_flags *gfp, 
    FLOAT8             pe[2][2],
    FLOAT8             ms_ener_ratio[2], 
    FLOAT8             xr[2][2][576],
    III_psy_ratio      ratio[2][2], 
    int                l3_enc[2][2][576],
    III_scalefac_t     scalefac[2][2] )
{
  III_psy_xmin l3_xmin[2][2];
  III_scalefac_t  bst_scalefac;
  gr_info         bst_cod_info;
  int             bst_l3_enc[576];
  FLOAT8          bst_xrpow[576]; 
  
  FLOAT8    xrpow[576];
  FLOAT8    noise[4];          /* over,max_noise,over_noise,tot_noise; */
  int       save_bits[2][2];
  int       bands[2][2];
  int       this_bits, dbits;
  int       used_bits=0;
  int       min_bits,max_bits,min_mean_bits=0,analog_mean_bits,Max_bits;
  int       frameBits[15];
  int       bitsPerFrame;
  int       bits;
  int       mean_bits, real_bits, min_pe_bits;
  int       i, ch, num_chan, gr, analog_silence;
  int       reduce_s_ch=0;
  gr_info  *cod_info = NULL;
  lame_internal_flags *gfc = (lame_internal_flags*)gfp->internal_flags;
  III_side_info_t *l3_side = &gfc->l3_side;

  iteration_init(gfp,l3_side,l3_enc);

  /* my experiences are, that side channel reduction  
   * does more harm than good when VBR encoding
   * (Robert.Hegemann@gmx.de 2000-02-18)
   */
  if (gfc->mode_ext==MPG_MD_MS_LR && gfp->quality >= 5) { 
    reduce_s_ch=1;
    num_chan=1;
  } else {
    num_chan=gfc->channels;
  }

  /* bits for analog silence */
  gfc->bitrate_index = 1;
  getframebits (gfp,&bitsPerFrame, &mean_bits);
  analog_mean_bits = mean_bits/gfc->channels;
  
  analog_silence=1;
  for (gr = 0; gr < gfc->mode_gr; gr++) {
    /* copy data to be quantized into xr */
    if (gfc->mode_ext==MPG_MD_MS_LR) { ms_convert(xr[gr],xr[gr]); }
    for (ch = 0; ch < gfc->channels; ch++) {
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
      min_mean_bits=mean_bits/gfc->channels;
    }
    frameBits[gfc->bitrate_index]
     = ResvFrameBegin (gfp,l3_side, mean_bits, bitsPerFrame);
  }


  gfc->bitrate_index=gfc->VBR_max_bitrate;
  
  /*******************************************************************
   * how many bits would we use of it?
   *******************************************************************/
  for (gr = 0; gr < gfc->mode_gr; gr++) {
    for (ch = 0; ch < num_chan; ch++) { 
      /******************************************************************
       * find smallest number of bits for an allowable quantization
       ******************************************************************/
      cod_info = &l3_side->gr[gr].ch[ch].tt;
      if (!init_outer_loop(cod_info,&scalefac[gr][ch],xr[gr][ch],xrpow)) {
        /*  xr contains no energy 
         *  cod_info and scalefac was initialized in init_outer_loop
         *  l3_enc, our encoding data, will be quantized to zero
         */
        memset(l3_enc[gr][ch],0,sizeof(l3_enc[gr][ch]));
        save_bits[gr][ch] = 0;
        continue; /* with next channel */
      }
      
      memcpy( &bst_cod_info, cod_info, sizeof(gr_info)        );
      memset( &bst_scalefac, 0,        sizeof(III_scalefac_t) );
      memcpy( &bst_xrpow,    xrpow,    sizeof(bst_xrpow)      );
      
      /*  base amount of minimum bits
       */
      min_bits = Max(125,min_mean_bits);

      if (gfc->mode_ext==MPG_MD_MS_LR && ch==1) { 
        min_bits = Max(min_bits,save_bits[gr][0]/5);
      }
      
      /*  bit skeleton based on PE
       */
      min_pe_bits = (int)pe[gr][ch];
      
      if (cod_info->block_type==SHORT_TYPE) {
        /*  if LAME switches to short blocks then pe is
         *  >= 1000 on medium surge
         *  >= 3000 on big surge
         */
        min_pe_bits = (min_pe_bits-350) * bands[gr][ch]/39;
      } else {
        min_pe_bits = (min_pe_bits-350) * bands[gr][ch]/22;
      }
      if (gfc->mode_ext==MPG_MD_MS_LR && ch==1) {
        /*  side channel will use a lower bit skeleton based on PE
         */ 
        FLOAT8 fac  = .33*(.5-ms_ener_ratio[gr])/.5;
        min_pe_bits = (int)(min_pe_bits*((1-fac)/(1+fac)));
      }
      min_pe_bits = Min(min_pe_bits,(1820*gfp->out_samplerate/44100));

      /*  determine final minimum bits
       */
      if (analog_silence && !gfp->VBR_hard_min) {
        min_bits = analog_mean_bits;
      } else {
        min_bits = Max(min_bits,min_pe_bits);
      }
      
      /*  maximum allowed bits
       */
      max_bits = frameBits[gfc->VBR_max_bitrate]/(gfc->channels*gfc->mode_gr);
      max_bits = Min(1200+max_bits,4095-195*(gfc->channels-1));
      max_bits = Max(max_bits,min_bits);
      Max_bits = max_bits;
      
      assert(Max_bits < 4096);

      /*  start our search in the middle of nowhere
       */
      this_bits = min_bits+(max_bits-min_bits)/2;
      
      /*  real_bits is larger as max_bits so we can check to see if
       *  we found a better quantization
       */
      real_bits = max_bits+1;

      /*  search within round about 40 bits of optimal
       */
      do {
        assert(this_bits>=min_bits);
        assert(this_bits<=max_bits);

        outer_loop( gfp, cod_info, ch, this_bits, xr[gr][ch], &l3_xmin[gr][ch],
                    &scalefac[gr][ch], xrpow, l3_enc[gr][ch], noise );

        /*  is quantization as good as we are looking for ?
         *  in this case: is no scalefactor band distorted?
         */
        if (noise[0] <= 0) {
          /*  now we know it can be done with "real_bits"
           *  and maybe we can skip some iterations
           */
          real_bits = cod_info->part2_3_length;
          
          /*  store best quantization so far
           */
          memcpy( &bst_cod_info,  cod_info,         sizeof(gr_info)         );
          memcpy( &bst_scalefac, &scalefac[gr][ch], sizeof(III_scalefac_t)  );
          memcpy(  bst_xrpow,     xrpow,            sizeof(bst_xrpow)       );
          memcpy(  bst_l3_enc,    l3_enc  [gr][ch], sizeof(bst_l3_enc)      );
          
          /*  try with fewer bits
           */
          max_bits  = real_bits-32;
          dbits     = max_bits-min_bits;
          this_bits = min_bits+dbits/2;
        } 
        else {
          /*  try with more bits
           */
          min_bits  = this_bits+32;
          dbits     = max_bits-min_bits;
          this_bits = min_bits+dbits/2;
          
          if (dbits>8) {
            /*  start again with best quantization so far
             */
            memcpy(  cod_info,         &bst_cod_info, sizeof(gr_info)        );
            memcpy( &scalefac[gr][ch], &bst_scalefac, sizeof(III_scalefac_t) );
            memcpy(  xrpow,             bst_xrpow,    sizeof(xrpow)          );
          }
        }
      } while (dbits>8);

      if (real_bits <= Max_bits) {
        /*  restore best quantization found
         */
        memcpy(  cod_info,         &bst_cod_info, sizeof(gr_info)        );
        memcpy( &scalefac[gr][ch], &bst_scalefac, sizeof(III_scalefac_t) );
        memcpy(  l3_enc  [gr][ch],  bst_l3_enc,   sizeof(bst_l3_enc)     );
      }
      assert((int)cod_info->part2_3_length <= Max_bits);
      save_bits[gr][ch] = cod_info->part2_3_length;
      used_bits += save_bits[gr][ch];
      
    } /* for ch */
  }  /* for gr */

  if (reduce_s_ch) {
    /*  number of bits needed was found for MID channel above.  Use formula
     *  (fixed bitrate code) to set the side channel bits */
    for (gr = 0; gr < gfc->mode_gr; gr++) {
      FLOAT8 fac = .33*(.5-ms_ener_ratio[gr])/.5;
      save_bits[gr][1] = (int)(((1-fac)/(1+fac))*save_bits[gr][0]);
      save_bits[gr][1] = Max(analog_mean_bits,save_bits[gr][1]);
      used_bits += save_bits[gr][1];
    }
  }

  /******************************************************************
   * find lowest bitrate able to hold used bits
   ******************************************************************/
  if (analog_silence && !gfp->VBR_hard_min) {
    /*  we detected analog silence and the user did not specify 
     *  any hard framesize limit, so start with smallest possible frame
     */
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
    for(ch = 0; ch < gfc->channels; ch++) {
      cod_info = &l3_side->gr[gr].ch[ch].tt;
      
      if (used_bits > bits) {
        /* repartion available bits in same proportion
         */
        save_bits[gr][ch] *= frameBits[gfc->bitrate_index];
        save_bits[gr][ch] /= used_bits;
      }
      if (used_bits > bits || (reduce_s_ch && ch == 1)) {        
        /*  we had to repartion the available bits
         *  or we have to encode the side channel now due to quality=5
         */
        if (!init_outer_loop(cod_info,&scalefac[gr][ch],xr[gr][ch],xrpow)) {
          /*  xr contains no energy 
           *  cod_info and scalefac were initialized in init_outer_loop
           *  l3_enc, our encoding data, will be quantized to zero
           */
          memset(l3_enc[gr][ch],0,sizeof(l3_enc[gr][ch]));
        }
        else {
          /*  xr contains energy we will have to encode 
           *  cod_info and scalefac were initialized in init_outer_loop
           *  masking abilities were previously calculated
           *  find some good quantization in outer_loop 
           */
          outer_loop( gfp, cod_info, ch, save_bits[gr][ch], xr[gr][ch], 
                      &l3_xmin[gr][ch], &scalefac[gr][ch], xrpow,
                      l3_enc[gr][ch], noise );
        }
      }
      /*  update the frame analyzer information
       *  it looks like scfsi will not be handled correct:
       *    often there are undistorted bands which show up
       *    as distorted ones in the frame analyzer!!
       *  that's why we do it before:
       *  - best_scalefac_store
       *  - best_huffman_divide
       */
      if (gfp->gtkflag) {
        set_pinfo( gfp, cod_info, &ratio[gr][ch], &scalefac[gr][ch],
                   xr[gr][ch], l3_enc[gr][ch], gr, ch);
      }
      /*  try some better scalefac storage
       */
      best_scalefac_store(gfc, l3_side, scalefac, l3_enc, gr, ch);
      
      /*  best huffman_divide may save some bits too
       */
      if (gfc->use_best_huffman==1) {
        best_huffman_divide(gfc, cod_info, l3_enc[gr][ch], gr, ch);
      }      
      /*  update reservoir status after FINAL quantization/bitrate
       */
      ResvAdjust (gfp,cod_info, l3_side, mean_bits);
      
      /*  set the sign of l3_enc from the sign of xr 
       */
      for ( i = 0; i < 576; i++) {
        if (xr[gr][ch][i] < 0) { l3_enc[gr][ch][i] *= -1; }
      }
    } /* ch */
  }  /* gr */

  ResvFrameEnd (gfp,l3_side, mean_bits);
}


