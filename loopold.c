/*
 *	old dual channel quantization routines
 *
 *	Copyright (c) 1999 Mark Taylor
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
#include "globalflags.h"
#include "util.h"
#include "l3side.h"
#include "quantize.h"
#include "l3bitstream.h"
#include "tables.h"
#include "reservoir.h"
#include "quantize-pvt.h"
#ifdef HAVEGTK
#include "gtkanal.h"
#endif

/************************************************************************/
/*  init_outer_loop  mt 6/99                                            */
/************************************************************************/
void init_outer_loop_dual(
    FLOAT8 xr[2][2][576],        /*  could be L/R OR MID/SIDE */
    FLOAT8 xr_org[2][2][576],
    III_psy_xmin  *l3_xmin,   /* the allowed distortion of the scalefactor */
    III_scalefac_t *scalefac, /* scalefactors */
    int gr, III_side_info_t *l3_side,
    III_psy_ratio *ratio, int ch)
{
  int sfb,i;
  gr_info *cod_info;  
  cod_info = &l3_side->gr[gr].ch[ch].tt;

  /* compute max allowed distortion */
  if (convert_psy) /* l/r thresholds, use unconverted data */
    calc_xmin( xr_org, ratio, cod_info, l3_xmin, gr, ch );
  else
    calc_xmin( xr, ratio, cod_info, l3_xmin, gr, ch );
  
  /* reset of iteration variables */
    
  for ( sfb = 0; sfb < SBMAX_l; sfb++ )
    scalefac->l[gr][ch][sfb] = 0;
  for ( sfb = 0; sfb < SBMAX_s; sfb++ )
    for ( i = 0; i < 3; i++ )
      scalefac->s[gr][ch][sfb][i] = 0;
  
  for ( i = 0; i < 4; i++ )
    cod_info->slen[i] = 0;
  cod_info->sfb_partition_table = &nr_of_sfb_block[0][0][0];
  
  cod_info->part2_3_length    = 0;
  cod_info->big_values        = ((cod_info->block_type==2)?288:0);
  cod_info->count1            = 0;
  cod_info->scalefac_compress = 0;
  cod_info->table_select[0]   = 0;
  cod_info->table_select[1]   = 0;
  cod_info->table_select[2]   = 0;
  cod_info->subblock_gain[0]  = 0;
  cod_info->subblock_gain[1]  = 0;
  cod_info->subblock_gain[2]  = 0;
  cod_info->region0_count     = 0;
  cod_info->region1_count     = 0;
  cod_info->part2_length      = 0;
  cod_info->preflag           = 0;
  cod_info->scalefac_scale    = 0;
  cod_info->quantizerStepSize = 0.0;
  cod_info->count1table_select= 0;
  cod_info->count1bits        = 0;
  
  
  if (gf.experimentalZ) {
    /* compute subblock gains */
    int j,b;  FLOAT8 en[3],mx;
    if ((cod_info->block_type ==2) ) {
      /* estimate energy within each subblock */
      for (b=0; b<3; b++) en[b]=0;
      for ( i=0,j = 0; j < 192; j++ ) {
	for (b=0; b<3; b++) {
	  en[b]+=xr[gr][ch][i]*xr[gr][ch][i];
	  i++;
	}
      }
      mx = 1e-12;
      for (b=0; b<3; b++) mx=Max(mx,en[b]);
      for (b=0; b<3; b++) en[b] = Max(en[b],1e-12)/mx;
      /* pick gain so that 2^(2gain)*en[0] = 1  */
      /* gain = .5* log( 1/en[0] )/LOG2 = -.5*log(en[])/LOG2 */
      for (b=0; b<3; b++) {
	cod_info->subblock_gain[b] = (int) (-.5*log(en[b])/LOG2 + 0.5);
	if (cod_info->subblock_gain[b] > 2) 
	  cod_info->subblock_gain[b]=2;
	if (cod_info->subblock_gain[b] < 0) 
	  cod_info->subblock_gain[b]=0;
      }
      for (i = b; i < 576; i += 3) {
	xr[gr][ch][i] *= 1 << (cod_info->subblock_gain[b] * 2);
      }
      for (sfb = 0; sfb < SBPSY_s; sfb++) {
	l3_xmin->s[gr][ch][sfb][b] *= 1 << (cod_info->subblock_gain[b] * 4);
      }
    }
  }
}




void quant_compare_dual(int better[2], int notdone[2],
FLOAT8 ms_ener_ratio,
int best_over[2],FLOAT8 best_tot_noise[2],FLOAT8 best_over_noise[2],FLOAT8 best_max_noise[2],
int over[2],FLOAT8 tot_noise[2], FLOAT8 over_noise[2], FLOAT8 max_noise[2]) 
{
  int ch;
  /*
    noise is given in decibals (db) relative to masking thesholds.

    over_noise:  sum of quantization noise > masking
    tot_noise:   sum of all quantization noise
    max_noise:   max quantization noise 

   */


  for (ch=0 ; ch < gf.stereo ; ch ++ ) {
    better[ch]=0;
    if (notdone[ch]) {
      if (convert_psy) {
	better[ch] = (over[0]+over[1]) < (best_over[0]+best_over[1]);
	if ((over[0]+over[1])==(best_over[0]+best_over[1])) {
	  better[ch] = (over_noise[0]+over_noise[1]) < (best_over_noise[0]+best_over_noise[1]);
	}
      }else{
	better[ch] = ((over[ch] < best_over[ch]) ||
		      ((over[ch]==best_over[ch]) && (over_noise[ch]<best_over_noise[ch])) ) ;
      }
    }
  }
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
 ************************************************************************/
void outer_loop_dual(
    FLOAT8 xr[2][2][576],        /*  could be L/R OR MID/SIDE */
    FLOAT8 xr_org[2][2][576],
    int mean_bits,
    int bit_rate,
    int best_over[2],
    III_psy_xmin  *l3_xmin,   /* the allowed distortion of the scalefactor */
    int l3_enc[2][2][576],    /* vector of quantized values ix(0..575) */
    frame_params *fr_ps,
    III_scalefac_t *scalefac, /* scalefactors */
    int gr, III_side_info_t *l3_side,
    III_psy_ratio *ratio, FLOAT8 pe[2][2], FLOAT8 ms_ener_ratio[2])
{
  int status[2],notdone[2]={0,0},count[2]={0,0},bits_found[2];
  int targ_bits[2],real_bits,tbits,extra_bits; 
  int scalesave_l[2][SBPSY_l], scalesave_s[2][SBPSY_l][3];
  int sfb, bits, huff_bits;
  FLOAT8 xfsf[2][4][SBPSY_l];
  FLOAT8 xrpow[2][2][576],temp;
  FLOAT8 distort[2][4][SBPSY_l];
  int save_l3_enc[2][576];  
  int i,over[2], iteration, ch, compute_stepsize;
  int better[2];
  int add_bits[2]; 
  FLOAT8 max_noise[2];
  FLOAT8 best_max_noise[2];
  FLOAT8 over_noise[2];
  FLOAT8 best_over_noise[2];
  FLOAT8 tot_noise[2];
  FLOAT8 best_tot_noise[2];
  gr_info save_cod_info[2];
  gr_info *cod_info[2];  

  cod_info[0] = &l3_side->gr[gr].ch[0].tt;
  cod_info[1] = &l3_side->gr[gr].ch[1].tt;
  for (ch=0 ; ch < gf.stereo ; ch ++) 
    best_over[ch] = 100;
      

  if (convert_mdct) ms_convert(xr[gr],xr_org[gr]);
  else memcpy(xr[gr],xr_org[gr],sizeof(FLOAT8)*2*576);   
  for (ch=0; ch<gf.stereo; ch++)
    init_outer_loop_dual(xr,xr_org,l3_xmin,scalefac,gr,l3_side,ratio,ch);  
  
  for (ch=0 ; ch < gf.stereo ; ch ++) {
    count[ch]=0;
    for (i=0; i<576; i++) {
      if ( fabs(xr[gr][ch][i]) > 0 ) count[ch]++; 
    }
    notdone[ch]=count[ch];
    if (count[ch]==0) best_over[ch]=0;
  }

  /******************************************************************
   * allocate bits for each channel 
   ******************************************************************/
  /* allocate targ_bits for granule */
  ResvMaxBits2( mean_bits, &tbits, &extra_bits, gr);
  
  for (ch=0 ; ch < gf.stereo ; ch ++ )
    targ_bits[ch]=tbits/gf.stereo;
  
  
  /* allocate extra bits from reservoir based on PE */
  bits=0;
  for (ch=0; ch<gf.stereo; ch++) {
    FLOAT8 pe_temp=pe[gr][ch];
    if (convert_psy) pe_temp=Max(pe[gr][0],pe[gr][1]);
    
    /* extra bits based on PE > 700 */
    add_bits[ch]=(pe_temp-750)/1.55;  /* 2.0; */
    
    
    /* short blocks need extra, no matter what the pe */
    if (cod_info[ch]->block_type==2) 
      if (add_bits[ch]<500) add_bits[ch]=500;
    
    if (add_bits[ch] < 0) add_bits[ch]=0;
    bits += add_bits[ch];
  }
  for (ch=0; ch<gf.stereo; ch++) {
    if (bits > extra_bits) add_bits[ch] = (extra_bits*add_bits[ch])/bits;
    targ_bits[ch] = targ_bits[ch] + add_bits[ch];
  }
  for (ch=0; ch<gf.stereo; ch++) 
    extra_bits -= add_bits[ch];

  if (reduce_sidechannel) {
    /*  ms_ener_ratio = 0:  allocate 66/33  mid/side  fac=.33  
     *  ms_ener_ratio =.5:  allocate 50/50 mid/side   fac= 0 */
    /* 75/25 split is fac=.5 */
    /* float fac = .50*(.5-ms_ener_ratio[gr])/.5;*/
    float fac = .33*(.5-ms_ener_ratio[gr])/.5;
    if (fac<0) fac=0;

    /* dont reduce side channel below 125 bits */
    if (targ_bits[1]-targ_bits[1]*fac > 125) {
      targ_bits[0] += targ_bits[1]*fac;
      targ_bits[1] -= targ_bits[1]*fac;
    }

  
    /* dont allow to many bits per channel */  
    for (ch=0; ch<gf.stereo; ch++) {
      int max_bits = Min(4095,mean_bits/2 + 1200);
      if (targ_bits[ch] > max_bits) {
	extra_bits += (targ_bits[ch] - max_bits);
	targ_bits[ch] = max_bits;
      }
    }
  }  
  
  
  /* BEGIN MAIN LOOP */
  iteration = 0;
  compute_stepsize=1;

  while ( (notdone[0] || notdone[1])  ) {
    static FLOAT8 OldValue[2] = {-30, -30};
    iteration ++;

    if (compute_stepsize) {
      /* compute initial quantization step */
      compute_stepsize=0;
      for (ch=0 ; ch < gf.stereo ; ch ++ )
	if (notdone[ch]) {
	  for(i=0;i<576;i++) 	    {
	    temp=fabs(xr[gr][ch][i]);
	    xrpow[gr][ch][i]=sqrt(sqrt(temp)*temp);
	  }
	  bits_found[ch]=bin_search_StepSize2(targ_bits[ch],OldValue[ch],
	      l3_enc[gr][ch],xr[gr][ch],xrpow[gr][ch],cod_info[ch]); 
	  OldValue[ch] = cod_info[ch]->quantizerStepSize;
	}
    }



    /* inner_loop starts with the initial quantization step computed above
     * and slowly increases until the bits < huff_bits.
     * Thus is it important not to start with too large of an inital
     * quantization step.  Too small is ok, but inner_loop will take longer 
     */
    for (ch=0 ; ch < gf.stereo ; ch ++ ) {
      if (notdone[ch]) {
	huff_bits = targ_bits[ch] - cod_info[ch]->part2_length;
	if (huff_bits < 0) {
	  if (iteration==1) {
	    fprintf(stderr,"ERROR: outer_loop(): huff_bits < 0. \n");
	    exit(-5);
	  }else{
	    /* scale factors too large, not enough bits. use previous quantizaton */
	    notdone[ch]=0;
	    over[ch]=999;
	  }
	}else{
	  /* if this is the first iteration, see if we can reuse the quantization
	   * computed in bin_search_StepSize above */
	  if (iteration==1) {
	    if(bits_found[ch]>huff_bits) {
	      cod_info[ch]->quantizerStepSize+=1.0;
	      real_bits = inner_loop( xr, xrpow[gr][ch], l3_enc, huff_bits, cod_info[ch], gr, ch );
	    } else real_bits=bits_found[ch];
	  }
	  else 
	    real_bits=inner_loop( xr, xrpow[gr][ch], l3_enc, huff_bits, cod_info[ch], gr, ch );

	  cod_info[ch]->part2_3_length = real_bits;
	}
      }
    }



    /* compute the distortion in this quantization */
    if (gf.fast_mode) {
      for (ch=0; ch<gf.stereo; ch++)
	over[ch]=0;
    }else{
      if (convert_psy) {
	/* mid/side coefficiets, l/r thresholds */
	calc_noise2( xr[gr], l3_enc[gr], cod_info, xfsf,
          distort, l3_xmin,gr,over,over_noise,tot_noise,max_noise);
      }	else {
	  /* coefficients and thresholds both l/r (or both mid/side) */
	  for (ch=0; ch<gf.stereo; ch++)
	    if (notdone[ch])
	      over[ch]=calc_noise1( xr[gr][ch], l3_enc[gr][ch], cod_info[ch], 
	    xfsf[ch],distort[ch], l3_xmin,gr,ch, &over_noise[ch], 
            &tot_noise[ch], &max_noise[ch],scalefac);
      }
    }

    /* check if this quantization is better the our saved quantization */
    if (iteration == 1) 
      for (ch=0; ch<gf.stereo; ch++) better[ch]=1;
    else quant_compare_dual(better,notdone,ms_ener_ratio[gr],
        best_over,best_tot_noise,best_over_noise,best_max_noise,
	over,tot_noise,over_noise,max_noise);

 

    /* save data so we can restore this quantization later */    
    for (ch=0 ; ch < gf.stereo ; ch ++ ) {
      if (better[ch]) {
	best_over[ch]=over[ch];
	best_over_noise[ch]=over_noise[ch];
	best_tot_noise[ch]=tot_noise[ch];
	best_max_noise[ch]=max_noise[ch];
	if (notdone[ch]) {
	  for ( sfb = 0; sfb < SBPSY_l; sfb++ ) /* save scaling factors */
	    scalesave_l[ch][sfb] = scalefac->l[gr][ch][sfb];
	  
	  for ( sfb = 0; sfb < SBPSY_s; sfb++ )
	    for ( i = 0; i < 3; i++ )
	      scalesave_s[ch][sfb][i] = scalefac->s[gr][ch][sfb][i];
	  
	  memcpy(save_l3_enc[ch],l3_enc[gr][ch],sizeof(l3_enc[gr][ch]));   
	  memcpy(&save_cod_info[ch],cod_info[ch],sizeof(save_cod_info[ch]));

#ifdef HAVEGTK
	  if (gf.gtkflag) {
	    for ( i = 0; i < 3; i++ ) {
	      for ( sfb = cod_info[ch]->sfb_smax; sfb < 12; sfb++ )  {
		pinfo->xfsf_s[gr][ch][3*sfb+i] =  
		  pinfo->thr_s[gr][ch][3*sfb+i]*xfsf[ch][i+1][sfb]/
		  (1e-20+l3_xmin->s[gr][ch][sfb][i]);
	      }
	    }
	    for ( sfb = 0; sfb < cod_info[ch]->sfb_lmax; sfb++ )   {
	      pinfo->xfsf[gr][ch][sfb] =  
		pinfo->thr[gr][ch][sfb]*xfsf[ch][0][sfb]/
		(1e-20 + l3_xmin->l[gr][ch][sfb]);
	    }
	    pinfo->over[gr][ch]=over[ch];
	    pinfo->over_noise[gr][ch]=over_noise[ch];
	  }
#endif
	  

	}
      }
    }

    /* if no bands with distortion, we are done */
    for (ch=0 ; ch < gf.stereo ; ch ++ ) 
      if (notdone[ch]) {
	if (convert_psy) 
	  notdone[ch] = (over[0] || over[1]);
	else
	  notdone[ch] = over[ch];
      }




    /* if we didn't just apply pre-emph, let us see if we should 
     * amplify some scale factor bands */
    for (ch=0 ; ch < gf.stereo ; ch ++ ) 
      if (notdone[ch]) {
	  amp_scalefac_bands( xr[gr][ch], xrpow[gr][ch], l3_xmin,
		l3_side, scalefac, gr, ch, iteration,distort[ch]);
      }
    

    /* check to make sure we have not amplified too much */
    for (ch=0 ; ch < gf.stereo ; ch ++ ) {
      if (notdone[ch]) {
	if ( (status[ch] = loop_break(scalefac, cod_info[ch], gr, ch)) == 0 ) {
	  if ( fr_ps->header->version == 1 ) {
	    status[ch] = scale_bitcount( scalefac, cod_info[ch], gr, ch );
	  }else{
	    status[ch] = scale_bitcount_lsf( scalefac, cod_info[ch], gr, ch );
	  }
        }
	notdone[ch] = !status[ch];
      }
    }
  }    /* done with main iteration */
  

  
  /* restore some data */
  for (ch=0 ; ch < gf.stereo ; ch ++ ) {
    if (count[ch]) {
      for ( sfb = 0; sfb < SBPSY_l; sfb++ ) {
	scalefac->l[gr][ch][sfb] = scalesave_l[ch][sfb];    
      }
      
      for ( i = 0; i < 3; i++ )
	for ( sfb = 0; sfb < SBPSY_s; sfb++ ) {
	  scalefac->s[gr][ch][sfb][i] = scalesave_s[ch][sfb][i];    
	}

      memcpy(l3_enc[gr][ch],save_l3_enc[ch],sizeof(l3_enc[gr][ch]));   
      memcpy(cod_info[ch],&save_cod_info[ch],sizeof(save_cod_info[ch]));
      cod_info[ch]->part2_3_length += cod_info[ch]->part2_length;

    }      
  }
  
  /* finish up */
  for (ch=0 ; ch < gf.stereo ; ch ++ ) {
    best_scalefac_store(gr, ch, l3_side, scalefac);
#ifdef HAVEGTK
    if (gf.gtkflag)
      pinfo->LAMEmainbits[gr][ch]=cod_info[ch]->part2_3_length;
#endif
    ResvAdjust(cod_info[ch], l3_side, mean_bits );
    cod_info[ch]->global_gain = cod_info[ch]->quantizerStepSize + 210.0;
    assert( cod_info[ch]->global_gain < 256 );
  }
}


/*************************************************************************/
/*            calc_noise2                                                */
/*************************************************************************/
/*   Improved version of calc_noise for dual channel.  This routine is */
/*   used when you are quantizaing mid and side channels using masking */
/*   thresholds from L and R channels.  mt 5/99 */

void calc_noise2( FLOAT8 xr[2][576], int ix[2][576], gr_info *cod_info[2],
            FLOAT8 xfsf[2][4][SBPSY_l], FLOAT8 distort[2][4][SBPSY_l],
            III_psy_xmin *l3_xmin,int gr, int over[2], 
            FLOAT8 over_noise[2], FLOAT8 tot_noise[2], FLOAT8 max_noise[2])
{
    int start, end, sfb, l, i;
    FLOAT8 sum[2],step_0,step_1,step[2],bw;

    D192_3 *xr_s[2];
    I192_3 *ix_s[2];

    static FLOAT8 pow43[PRECALC_SIZE];
    int ch;
    static int init=0;
    FLOAT8 diff[2];
    FLOAT8 noise;
    int index;
    
    if (init==0) {
      init++;
      for(i=0;i<PRECALC_SIZE;i++)
        pow43[i] = pow((FLOAT8)i, 4.0/3.0);
    }
    
    

    /* calc_noise2: we can assume block types of both channels must be the same
*/
    if (cod_info[0]->block_type != 2) {
    for (ch=0 ; ch < gf.stereo ; ch ++ ) {
      over[ch]=0;
      over_noise[ch]=0;
      tot_noise[ch]=0;
      max_noise[ch]=-999;
      step[ch] = pow( 2.0, (cod_info[ch]->quantizerStepSize) * 0.25 );
    }
    for ( sfb = 0; sfb < SBPSY_l; sfb++ ) {
      start = scalefac_band_long[ sfb ];
      end   = scalefac_band_long[ sfb+1 ];
      bw = end - start;

      for ( sum[0]=0, sum[1]=0, l = start; l < end; l++ ) {
          index=ix[0][l];
          if (index==0) {
            diff[0]=xr[0][l];
          } else {
            if (xr[0][l]<0) {
              diff[0]=xr[0][l] + pow43[index] * step[0];
            } else
              diff[0]=xr[0][l] - pow43[index] * step[0];
          }
          index=ix[1][l];
          if (index==0) {
            diff[1]=xr[1][l];
          } else {
            if (xr[1][l]<0) {
              diff[1]=xr[1][l] + pow43[index] * step[1];
            } else
              diff[1]=xr[1][l] - pow43[index] * step[1];
          }
        sum[0] += (diff[0]+diff[1])*(diff[0]+diff[1]);
        sum[1] += (diff[0]-diff[1])*(diff[0]-diff[1]);
      }      
      sum[0] *= 0.5;
      sum[1] *= 0.5;

      for (ch=0 ; ch < gf.stereo ; ch ++ ) {
        xfsf[ch][0][sfb] = sum[ch] / bw;
        noise = 10*log10(Max(.001,xfsf[ch][0][sfb]/l3_xmin->l[gr][ch][sfb]));
        distort[ch][0][sfb] =  noise;
        if (noise>0) {
          over[ch]++;
          over_noise[ch] += noise;
        }
        tot_noise[ch] += noise;
        max_noise[ch] = Max(max_noise[ch],noise);
      }

      /* if there is audible distortion in left or right channel, set flags
       * to denote distortion in both mid and side channels */
      for (ch=0 ; ch < gf.stereo ; ch ++ ) {
        distort[ch][0][sfb] = Max(distort[0][0][sfb],distort[1][0][sfb]);
      }
    }
    }

    /* calc_noise2: we can assume block types of both channels must be the same
*/
    if (cod_info[0]->block_type == 2) {
      step_0 = pow( 2.0, (cod_info[0]->quantizerStepSize) * 0.25 );
      step_1 = pow( 2.0, (cod_info[1]->quantizerStepSize) * 0.25 );

      for (ch=0 ; ch < gf.stereo ; ch ++ ) {
	over[ch] = 0;
	xr_s[ch] = (D192_3 *) xr[ch];
	ix_s[ch] = (I192_3 *) ix[ch];
      }

    for ( sfb = 0 ; sfb < SBPSY_s; sfb++ ) {
      start = scalefac_band_short[ sfb ];
      end   = scalefac_band_short[ sfb+1 ];
      bw = end - start;
      for ( i = 0; i < 3; i++ ) {           
        for (ch=0 ; ch < gf.stereo ; ch ++ ) sum[ch] = 0.0;
        for ( l = start; l < end; l++ )           {
            index=(*ix_s[0])[l][i];
            if (index==0)
              diff[0] = (*xr_s[0])[l][i];
            else {
              if ((*xr_s[0])[l][i] < 0) {
                diff[0] = (*xr_s[0])[l][i] + pow43[index] * step_0; 
              } else {
                diff[0] = (*xr_s[0])[l][i] - pow43[index] * step_0; 
              }
            }
            index=(*ix_s[1])[l][i];
            if (index==0)
              diff[1] = (*xr_s[1])[l][i];
            else {
              if ((*xr_s[1])[l][i] < 0) {
                diff[1] = (*xr_s[1])[l][i] + pow43[index] * step_1; 
              } else {
                diff[1] = (*xr_s[1])[l][i] - pow43[index] * step_1; 
              }
            }     
          sum[0] += (diff[0]+diff[1])*(diff[0]+diff[1])/(2.0);
          sum[1] += (diff[0]-diff[1])*(diff[0]-diff[1])/(2.0);
        }

        for (ch=0 ; ch < gf.stereo ; ch ++ ) {
          xfsf[ch][i+1][sfb] = sum[ch] / bw;
          noise =
10*log10(Max(.001,xfsf[ch][i+1][sfb]/l3_xmin->s[gr][ch][sfb][i]));
          distort[ch][i+1][sfb] = noise>0;
          if (noise>0) {
            over[ch]++;
            over_noise[ch] += noise;
          }
          tot_noise[ch] += noise;
          max_noise[ch]=Max(max_noise[ch],noise);
        }
        /* if there is audible distortion in left or right channel, set flags
         * to denote distortion in both mid and side channels */
        for (ch=0 ; ch < gf.stereo ; ch ++ ) 
          distort[ch][i+1][sfb] = Max
            (distort[0][i+1][sfb],distort[1][i+1][sfb]  );
      }
    }
    }
}







