#define MAXNOISEXX
/*
 *	MP3 quantization
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
#include "reservoir.h"
#include "quantize-pvt.h"
#ifdef HAVEGTK
#include "gtkanal.h"
#endif










/************************************************************************/
/*  iteration_loop()                                                    */
/************************************************************************/
void
iteration_loop( FLOAT8 pe[2][2], FLOAT8 ms_ener_ratio[2],
		FLOAT8 xr_org[2][2][576], III_psy_ratio *ratio,
		III_side_info_t *l3_side, int l3_enc[2][2][576],
		III_scalefac_t *scalefac, frame_params *fr_ps)
{
  III_psy_xmin l3_xmin;
  gr_info *cod_info;
  layer *info;
  int over[2];
  FLOAT8 noise[4]; /* over,max_noise,over_noise,tot_noise; */
  FLOAT8 targ_noise[4]; /* over,max_noise,over_noise,tot_noise; */
  int bitsPerFrame;
  int mean_bits;
  int stereo = fr_ps->stereo;
  int ch, gr, i, mode_gr, bit_rate;
  FLOAT8 xr[2][2][576];


  iteration_init(xr_org,l3_side,l3_enc,fr_ps,&l3_xmin);
  info = fr_ps->header;
  mode_gr = (info->version == 1) ? 2 : 1;
  bit_rate = bitrate[info->version][info->lay-1][info->bitrate_index];


  getframebits(info,stereo,&bitsPerFrame,&mean_bits);
  ResvFrameBegin( fr_ps, l3_side, mean_bits, bitsPerFrame );

  /* quantize! */

  for ( gr = 0; gr < mode_gr; gr++ ) {
    if (convert_psy) {
      /* dual channel version can quantize Mid/Side channels with L/R
       * maskings (by constantly reconstructing L/R data).  Used before we
       * we had proper mid/side maskings. */
      outer_loop_dual( xr, xr_org, mean_bits, bit_rate, over,
	       &l3_xmin,l3_enc, fr_ps, 
	       scalefac,gr,stereo, l3_side, ratio, pe, ms_ener_ratio);
    } else {
      int targ_bits[2];
      /* copy data to be quantized into xr */
      if (convert_mdct) ms_convert(xr[gr],xr_org[gr]);
      else memcpy(xr[gr],xr_org[gr],sizeof(FLOAT8)*2*576);   
      
      on_pe(pe,l3_side,targ_bits,mean_bits,stereo,gr);
      if (reduce_sidechannel) 
	reduce_side(targ_bits,ms_ener_ratio[gr],mean_bits);
      
      for (ch=0 ; ch < stereo ; ch ++) {
	outer_loop( xr, targ_bits[ch], noise, targ_noise, 0, &l3_xmin,l3_enc, 
	    fr_ps, scalefac,gr,stereo, l3_side, ratio, ms_ener_ratio[gr],ch);
	cod_info = &l3_side->gr[gr].ch[ch].tt;
	ResvAdjust( fr_ps, cod_info, l3_side, mean_bits );
      }
    }
  }

  
  /* set the sign of l3_enc */
  for ( gr = 0; gr < mode_gr; gr++ ) {
    for ( ch =  0; ch < stereo; ch++ ) {
      int *pi = &l3_enc[gr][ch][0];
      for ( i = 0; i < 576; i++) 	    {
	if ( (xr[gr][ch][i] < 0) && (pi[i] > 0) )   pi[i] *= -1;
      }
    }
  }
  ResvFrameEnd( fr_ps, l3_side, mean_bits );
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
VBR_iteration_loop (FLOAT8 pe[2][2], FLOAT8 ms_ener_ratio[2],
                FLOAT8 xr_org[2][2][576], III_psy_ratio * ratio,
                III_side_info_t * l3_side, int l3_enc[2][2][576],
                III_scalefac_t * scalefac, frame_params * fr_ps)
{
  III_psy_xmin l3_xmin;
  gr_info  *cod_info = NULL;
  layer    *info;
  int       save_bits[2][2];
  FLOAT8    noise[4];      /* over,max_noise,over_noise,tot_noise; */
  FLOAT8    targ_noise[4]; /* over,max_noise,over_noise,tot_noise; */
  int       this_bits, dbits;
  int       used_bits=0;
  int       min_bits,max_bits,min_mean_bits=0;
  int       frameBits[15];
  int       bitsPerFrame;
  int       bits;
  int       mean_bits;
  int       stereo = fr_ps->stereo;
  int       i,ch, gr, mode_gr, analog_silence;
  FLOAT8    xr[2][2][576];
  FLOAT8 xr_save[576];
  FLOAT8 masking_lower_db;

  iteration_init(xr_org,l3_side,l3_enc,fr_ps,&l3_xmin);
  info = fr_ps->header;
  mode_gr = (info->version == 1) ? 2 : 1;


  /*******************************************************************
   * how many bits are available for each bitrate?
   *******************************************************************/
  for( info->bitrate_index = 1;
       info->bitrate_index <= VBR_max_bitrate;
       info->bitrate_index++    ) {
    getframebits (info, stereo, &bitsPerFrame, &mean_bits);
    if (info->bitrate_index == VBR_min_bitrate) {
      /* always use at least this many bits per granule per channel */
      /* unless we detect analog silence, see below */
      min_mean_bits=mean_bits/stereo;
    }
    frameBits[info->bitrate_index]=
      ResvFrameBegin (fr_ps, l3_side, mean_bits, bitsPerFrame);
  }

  info->bitrate_index=VBR_max_bitrate;

  
  /*******************************************************************
   * how many bits would we use of it?
   *******************************************************************/
  analog_silence=0;
  for (gr = 0; gr < mode_gr; gr++) {
    int num_chan=stereo;
    /* determine quality based on mid channel only */
    if (reduce_sidechannel) num_chan=1;  

    /* copy data to be quantized into xr */
    if (convert_mdct) ms_convert(xr[gr],xr_org[gr]);
    else memcpy(xr[gr],xr_org[gr],sizeof(FLOAT8)*2*576);   

    for (ch = 0; ch < num_chan; ch++) { 
      /******************************************************************
       * find smallest number of bits for an allowable quantization
       ******************************************************************/
      memcpy(xr_save,xr[gr][ch],sizeof(FLOAT8)*576);   
      cod_info = &l3_side->gr[gr].ch[ch].tt;
      min_bits = Max(125,min_mean_bits);

      /* check for analolg silence */
      /* if energy < ATH, set min_bits = 125 */
      if (0==calc_xmin( xr, ratio, cod_info, &l3_xmin, gr, ch )) {
	analog_silence=1;
      	min_bits=125;
      }

      if (cod_info->block_type==SHORT_TYPE) {
	min_bits += Max(1100,pe[gr][ch]);
	min_bits=Min(min_bits,1800);
      }

      max_bits = 1200 + frameBits[VBR_max_bitrate]/(stereo*mode_gr);
      max_bits=Min(max_bits,2500);
      max_bits=Max(max_bits,min_bits);


      /** in the case we will not find any better, we allocate max_bits
       ****************************************************************/
      save_bits[gr][ch] = max_bits;

      dbits = (max_bits-min_bits)/4;
      this_bits = (max_bits+min_bits)/2;  
      /* bin search to within +/- 10 bits of optimal */
      do {
	int better;
	FLOAT8 fac;
	assert(this_bits>=min_bits);
	assert(this_bits<=max_bits);

	/* quality setting */
	/* Adjust allowed masking based on quality setting */
	/* db_lower varies from -10 to +8 db */
	masking_lower_db = -10 + 2*VBR_q;
	/* adjust by -6(min)..0(max) depending on bitrate */
	fac=(FLOAT8)(this_bits-125)/(FLOAT8)(2500-125);
	fac = 6*(fac-1);
	masking_lower_db += fac;

	masking_lower = pow(10.0,masking_lower_db/10);


	

	/* VBR will look for a quantization which has better values
	 * then those specified below.*/
	targ_noise[0]=0;          /* over */
	targ_noise[1]=0;          /* max_noise */
	targ_noise[2]=0;          /* over_noise */
	targ_noise[3]=0;          /* tot_noise */
	
	targ_noise[0]=Max(0,targ_noise[0]);
	targ_noise[2]=Max(0,targ_noise[2]);


	/* restore xr */
	memcpy(xr[gr][ch],xr_save,sizeof(FLOAT8)*576);   
	outer_loop( xr, this_bits, noise, targ_noise, 1,&l3_xmin,l3_enc, fr_ps, 
	    scalefac,gr,stereo, l3_side, ratio, ms_ener_ratio[gr], ch);

	/* is quantization as good as we are looking for ? */
	better=VBR_compare((int)targ_noise[0],targ_noise[3],targ_noise[2],
            targ_noise[1],(int)noise[0],noise[3],noise[2],noise[1]);

       if (better) {
	  save_bits[gr][ch] = this_bits;
          this_bits -= dbits;
        } else {
          this_bits += dbits;
        }
        dbits /= 2;
      } while (dbits>10) ;
      used_bits += save_bits[gr][ch];
      
    } /* for ch */
  } /* for gr */


  if (reduce_sidechannel) {
    /* number of bits needed was found for MID channel above.  Use formula
     * (fixed bitrate code) to set the side channel bits */
    for (gr = 0; gr < mode_gr; gr++) {
      FLOAT8 fac = .33*(.5-ms_ener_ratio[gr])/.5;
      save_bits[gr][1]=((1-fac)/(1+fac))*save_bits[gr][0];
      save_bits[gr][1]=Max(75,save_bits[gr][1]);
      used_bits += save_bits[gr][1];
    }
  }

  /******************************************************************
   * find lowest bitrate able to hold used bits
   ******************************************************************/
  for( info->bitrate_index =   (analog_silence ? 1 : VBR_min_bitrate );
       info->bitrate_index < VBR_max_bitrate;
       info->bitrate_index++    )
    if( used_bits <= frameBits[info->bitrate_index] ) break;

  /*******************************************************************
   * calculate quantization for this bitrate
   *******************************************************************/  
  getframebits (info, stereo, &bitsPerFrame, &mean_bits);
  bits=ResvFrameBegin (fr_ps, l3_side, mean_bits, bitsPerFrame);

  /* repartion available bits in same proportion */
  if (used_bits > bits ) {
    for( gr = 0; gr < mode_gr; gr++) {
      for(ch = 0; ch < stereo; ch++) {
	save_bits[gr][ch]=(save_bits[gr][ch]*frameBits[info->bitrate_index])/used_bits;
      }
    }
    used_bits=0;
    for( gr = 0; gr < mode_gr; gr++) {
      for(ch = 0; ch < stereo; ch++) {
	used_bits += save_bits[gr][ch];
      }
    }
  }
  assert(used_bits <= bits);

  for(gr = 0; gr < mode_gr; gr++) {
    /* copy data to be quantized into xr */
    if (convert_mdct) ms_convert(xr[gr],xr_org[gr]);
    else memcpy(xr[gr],xr_org[gr],sizeof(FLOAT8)*2*576);   
    for(ch = 0; ch < stereo; ch++) {
      outer_loop( xr, save_bits[gr][ch], noise,targ_noise,0,
		  &l3_xmin,l3_enc, fr_ps, 
		  scalefac,gr,stereo, l3_side, ratio, ms_ener_ratio[gr], ch);
    }
  }

  /*******************************************************************
   * update reservoir status after FINAL quantization/bitrate 
   *******************************************************************/
  for (gr = 0; gr < mode_gr; gr++)
    for (ch = 0; ch < stereo; ch++) {
      cod_info = &l3_side->gr[gr].ch[ch].tt;
      ResvAdjust (fr_ps, cod_info, l3_side, mean_bits);
    }

  /*******************************************************************
   * set the sign of l3_enc 
   *******************************************************************/
  for (gr = 0; gr < mode_gr; gr++)
    for (ch = 0; ch < stereo; ch++) {
      int      *pi = &l3_enc[gr][ch][0];

      for (i = 0; i < 576; i++) {
        FLOAT8    pr = xr[gr][ch][i];

        if ((pr < 0) && (pi[i] > 0))
          pi[i] *= -1;
      }
    }

  ResvFrameEnd (fr_ps, l3_side, mean_bits);
}




/************************************************************************/
/*  init_outer_loop  mt 6/99                                            */
/************************************************************************/
void init_outer_loop(
    FLOAT8 xr[2][2][576],        /*  could be L/R OR MID/SIDE */
    III_psy_xmin  *l3_xmin,   /* the allowed distortion of the scalefactor */
    III_scalefac_t *scalefac, /* scalefactors */
    int gr, int stereo, III_side_info_t *l3_side,
    III_psy_ratio *ratio, int ch)
{
  int sfb,i;
  gr_info *cod_info;  
  cod_info = &l3_side->gr[gr].ch[ch].tt;

  /* compute max allowed distortion */
  calc_xmin( xr, ratio, cod_info, l3_xmin, gr, ch );

  /* if ( info->version == 1 )
     calc_scfsi( xr[gr][ch], l3_side, &l3_xmin, ch, gr ); 
  */
  
    
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
  cod_info->big_values        = ((cod_info->block_type==SHORT_TYPE)?288:0);
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
  cod_info->address1          = 0;
  cod_info->address2          = 0;
  cod_info->address3          = 0;
  
  
  if (experimentalZ) {
    /* compute subblock gains */
    int j,b;  FLOAT8 en[3],mx;
    if ((cod_info->block_type==SHORT_TYPE) ) {
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
      /*printf("ener = %4.2f  %4.2f  %4.2f  \n",en[0],en[1],en[2]);*/
      /* pick gain so that 2^(2gain)*en[0] = 1  */
      /* gain = .5* log( 1/en[0] )/log(2) = -.5*log(en[])/log(2) */
      for (b=0; b<3; b++) {
	cod_info->subblock_gain[b]=nint2(-.5*log(en[b])/log(2.0));
	if (cod_info->subblock_gain[b] > 2) 
	  cod_info->subblock_gain[b]=2;
	if (cod_info->subblock_gain[b] < 0) 
	  cod_info->subblock_gain[b]=0;
	for (i = b; i < 576; i += 3) {
	  xr[gr][ch][i] *= 1 << (cod_info->subblock_gain[b] * 2);
	}
	for (sfb = 0; sfb < SBPSY_s; sfb++) {
	  l3_xmin->s[gr][ch][sfb][b] *= 1 << (cod_info->subblock_gain[b] * 4);
	}
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
 *  added VBR support mt 5/99
 ************************************************************************/
void outer_loop(
    FLOAT8 xr[2][2][576],        /*  could be L/R OR MID/SIDE */
    int targ_bits,
    FLOAT8 best_noise[4],
    FLOAT8 targ_noise[4],
    int sloppy,
    III_psy_xmin  *l3_xmin,   /* the allowed distortion of the scalefactor */
    int l3_enc[2][2][576],    /* vector of quantized values ix(0..575) */
    frame_params *fr_ps,
    III_scalefac_t *scalefac, /* scalefactors */
    int gr, int stereo, III_side_info_t *l3_side,
    III_psy_ratio *ratio, FLOAT8 ms_ener_ratio,
    int ch)
{

  int i, iteration;
  int status,count=0,bits_found=0;
  int scalesave_l[SBPSY_l], scalesave_s[SBPSY_l][3];
  int sfb, huff_bits;
  FLOAT8 xfsf[4][SBPSY_l];
  FLOAT8 xrpow[576],temp;
  FLOAT8 distort[4][SBPSY_l];
  int save_l3_enc[576];  
  int better;
  int over=0;
  FLOAT8 max_noise;
  FLOAT8 over_noise;
  FLOAT8 tot_noise;
  int best_over=0;
  FLOAT8 best_max_noise=0;
  FLOAT8 best_over_noise=0;
  FLOAT8 best_tot_noise=0;
  gr_info save_cod_info;
  gr_info *cod_info;  
  FLOAT8 xr_save[576];

  int compute_stepsize=1;
  int notdone=1;


  

  if (experimentalY) memcpy(xr_save,xr[gr][ch],sizeof(FLOAT8)*576);   
  cod_info = &l3_side->gr[gr].ch[ch].tt;
  init_outer_loop(xr,l3_xmin,scalefac,gr,stereo,l3_side,ratio,ch);  
  best_over = 100;
  count=0;
  for (i=0; i<576; i++) {
    if ( fabs(xr[gr][ch][i]) > 0 ) count++; 
  }
  if (count==0) {
    best_over=0;
    notdone = 0;
  }
  

  
  /* BEGIN MAIN LOOP */
  iteration = 0;
  while ( notdone  ) {
    int try_scale=0;
    iteration ++;
    
    if (compute_stepsize) {
      /* init and compute initial quantization step */
      compute_stepsize=0;
      for(i=0;i<576;i++) 	    {
	temp=fabs(xr[gr][ch][i]);
	xrpow[i]=sqrt(sqrt(temp)*temp);
      }
      bits_found=bin_search_StepSize2(targ_bits,-211.0,46,
		  l3_enc[gr][ch],xr[gr][ch],xrpow,cod_info); 
    }
    
    
    
    /* inner_loop starts with the initial quantization step computed above
     * and slowly increases until the bits < huff_bits.
     * Thus is it important not to start with too large of an inital
     * quantization step.  Too small is ok, but inner_loop will take longer 
     */
    huff_bits = targ_bits - cod_info->part2_length;
    if (huff_bits < 0) {
      if (iteration==1) {
	fprintf(stderr,"ERROR: outer_loop(): huff_bits < 0. \n");
	exit(-5);
      }else{
	/* scale factors too large, not enough bits. use previous quantizaton */
	notdone=0;
      }
    }else{
      /* if this is the first iteration, see if we can reuse the quantization
       * computed in bin_search_StepSize above */
      int real_bits;
      if (iteration==1) {
	if(bits_found>huff_bits) {
	  cod_info->quantizerStepSize+=1.0;
	  real_bits = inner_loop( xr, xrpow, l3_enc, huff_bits, cod_info, gr, ch );
	} else real_bits=bits_found;
      }
      else 
	real_bits=inner_loop( xr, xrpow, l3_enc, huff_bits, cod_info, gr, ch );
      cod_info->part2_3_length = real_bits;
    }
    
    
    if (notdone) {
      /* compute the distortion in this quantization */
      if (fast_mode) {
      	over=0;
      }else{
	/* coefficients and thresholds both l/r (or both mid/side) */
	over=calc_noise1( xr[gr][ch], l3_enc[gr][ch], cod_info, 
			  xfsf,distort, l3_xmin,gr,ch, &over_noise, 
			  &tot_noise, &max_noise, scalefac);
      }
      /* check if this quantization is better the our saved quantization */
      if (iteration == 1) better=1;
      else 
	better=quant_compare(
	     best_over,best_tot_noise,best_over_noise,best_max_noise,
                  over,     tot_noise,     over_noise,     max_noise);
      
      
      /* save data so we can restore this quantization later */    
      if (better) {
	best_over=over;
	best_max_noise=max_noise;
	best_over_noise=over_noise;
	best_tot_noise=tot_noise;
	
        /* we need not save the quantization when in sloppy mode, so skip
         */ 
        if (!sloppy) {
	for ( sfb = 0; sfb < SBPSY_l; sfb++ ) /* save scaling factors */
	  scalesave_l[sfb] = scalefac->l[gr][ch][sfb];
	
	for ( sfb = 0; sfb < SBPSY_s; sfb++ )
	  for ( i = 0; i < 3; i++ )
	    scalesave_s[sfb][i] = scalefac->s[gr][ch][sfb][i];
	
	memcpy(save_l3_enc,l3_enc[gr][ch],sizeof(l3_enc[gr][ch]));   
	memcpy(&save_cod_info,cod_info,sizeof(save_cod_info));
	
#ifdef HAVEGTK
	if (gtkflag) {
	  FLOAT ifqstep;
	  int l,start,end,bw;
	  FLOAT8 en0;
	  D192_3 *xr_s = (D192_3 *)xr[gr][ch];
	  ifqstep = ( cod_info->scalefac_scale == 0 ) ? .5 : 1.0;

	  if (cod_info->block_type == SHORT_TYPE) {
	  for ( i = 0; i < 3; i++ ) {
	    for ( sfb = 0; sfb < SBMAX_s; sfb++ )  {
	      start = scalefac_band_short[ sfb ];
	      end   = scalefac_band_short[ sfb + 1 ];
	      bw = end - start;
	      for ( en0 = 0.0, l = start; l < end; l++ ) 
		en0 += (*xr_s)[l][i] * (*xr_s)[l][i];
	      en0=Max(en0/bw,1e-20);
	      /*
	      pinfo->xfsf_s[gr][ch][3*sfb+i] =  
		pinfo->thr_s[gr][ch][3*sfb+i]*xfsf[i+1][sfb]/
		l3_xmin->s[gr][ch][sfb][i];
	      */
	      pinfo->xfsf_s[gr][ch][3*sfb+i] =  
		pinfo->en_s[gr][ch][3*sfb+i]*xfsf[i+1][sfb]/en0;
	      if (sfb < SBPSY_l) 
		pinfo->thr_s[gr][ch][3*sfb+i] =  
		  pinfo->en_s[gr][ch][3*sfb+i]*l3_xmin->s[gr][ch][sfb][i]/en0;

	      pinfo->LAMEsfb_s[gr][ch][3*sfb+i]=
		-2*cod_info->subblock_gain[i]-ifqstep*scalefac->s[gr][ch][sfb][i];
	    }
	  }
	  }else{
	  for ( sfb = 0; sfb < SBMAX_l; sfb++ )   {
	    start = scalefac_band_long[ sfb ];
	    end   = scalefac_band_long[ sfb+1 ];
	    bw = end - start;
	    for ( en0 = 0.0, l = start; l < end; l++ ) 
	      en0 += xr[gr][ch][l] * xr[gr][ch][l];
	    en0=Max(en0/bw,1e-20);
	    /* 
	    pinfo->xfsf[gr][ch][sfb] =  
	      pinfo->thr[gr][ch][sfb]*xfsf[0][sfb]/
	      l3_xmin->l[gr][ch][sfb];
	    */
	    pinfo->xfsf[gr][ch][sfb] =  
	      pinfo->en[gr][ch][sfb]*xfsf[0][sfb]/en0;
	    /*
	    printf("diff  = %f \n",10*log10(Max(pinfo->en[gr][ch][sfb],1e-20))
		   -(10*log10(en0)+150));
	    */
	    if (sfb < SBPSY_l) 
	      pinfo->thr[gr][ch][sfb] =  
		pinfo->en[gr][ch][sfb]*l3_xmin->l[gr][ch][sfb]/en0;

	    pinfo->LAMEsfb[gr][ch][sfb]=-ifqstep*scalefac->l[gr][ch][sfb];
	    if (cod_info->preflag && sfb>=11) 
	      pinfo->LAMEsfb[gr][ch][sfb]-=ifqstep*pretab[sfb];
	  }
	  }
	  pinfo->LAMEqss[gr][ch] = (cod_info->quantizerStepSize+210);
	  pinfo->over[gr][ch]=over;
	  pinfo->max_noise[gr][ch]=max_noise;
	  pinfo->tot_noise[gr][ch]=tot_noise;
	  pinfo->over_noise[gr][ch]=over_noise;
	  
	}
#endif
        } /* end of sloppy skipping */
      }
    }

    /* if no bands with distortion, we are done */
    if (experimentalX==0)
      if (over==0) notdone=0;

    /* in sloppy mode, as soon as we know we can do better than targ_noise,
       quit.  This is used for the inital VBR bin search.  Turn it off for
       final (optimal) quantization */
    if (sloppy && notdone) notdone = 
        !VBR_compare((int)targ_noise[0],targ_noise[3],targ_noise[2],
         targ_noise[1],over,tot_noise,over_noise,max_noise);



    
    
    if (notdone) {
      /* see if we should apply preemphasis */
      int pre_just_turned_on=0;
      /*
      pre_just_turned_on=
      	preemphasis(xr[gr][ch],xrpow,l3_xmin,gr,ch,l3_side,distort);
      */

      /* if we didn't just apply pre-emph, let us see if we should 
       * amplify some scale factor bands */
      if (!pre_just_turned_on) {
	notdone = amp_scalefac_bands( xr[gr][ch], xrpow, l3_xmin,
			     l3_side, scalefac, gr, ch, iteration,distort);
      }
    }
      
      
    if (notdone) {            
      /* check to make sure we have not amplified too much */
      if ( (status = loop_break(scalefac, cod_info, gr, ch)) == 0 ) {
	if ( fr_ps->header->version == 1 ) {
	  status = scale_bitcount( scalefac, cod_info, gr, ch );
	  if (status && (cod_info->scalefac_scale==0)) try_scale=1; 
	}else{
	  status = scale_bitcount_lsf( scalefac, cod_info, gr, ch );
	  if (status && (cod_info->scalefac_scale==0)) try_scale=1; 
	}
      }
      notdone = !status;
    }    
    

    
    if (try_scale && experimentalY) {
      memcpy(xr[gr][ch],xr_save,sizeof(FLOAT8)*576);   
      init_outer_loop(xr,l3_xmin,scalefac,gr,stereo,l3_side,ratio,ch);  
      compute_stepsize=1;  /* compute a new global gain */
      notdone=1;
      cod_info->scalefac_scale=1;
    }
  }    /* done with main iteration */
  
  
  /* in sloppy mode we don´t need to restore the quantization, 
   * cos we didn´t saved it and we don´t need it, as we only want
   * to know if we can do better
   */
  if (!sloppy)
  /* restore some data */
  if (count ) {
    for ( sfb = 0; sfb < SBPSY_l; sfb++ ) {
      scalefac->l[gr][ch][sfb] = scalesave_l[sfb];    
    }
    
    for ( i = 0; i < 3; i++ )
      for ( sfb = 0; sfb < SBPSY_s; sfb++ ) {
	scalefac->s[gr][ch][sfb][i] = scalesave_s[sfb][i];    
      }
    
    memcpy(l3_enc[gr][ch],save_l3_enc,sizeof(l3_enc[gr][ch]));   
    memcpy(cod_info,&save_cod_info,sizeof(save_cod_info));
    cod_info->part2_3_length += cod_info->part2_length;
    
#ifdef HAVEGTK
    if (gtkflag)
      pinfo->LAMEmainbits[gr][ch]=cod_info->part2_3_length;
#endif
  }      
  /* finish up */
  cod_info->global_gain = nint2( cod_info->quantizerStepSize + 210.0 );
  assert( cod_info->global_gain < 256 );

  best_noise[0]=best_over;
  best_noise[1]=best_max_noise;
  best_noise[2]=best_over_noise;
  best_noise[3]=best_tot_noise;
}





  










/*************************************************************************/
/*            calc_noise                                                 */
/*************************************************************************/
/*  mt 5/99:  Function: Improved calc_noise for a single channel   */
int calc_noise1( FLOAT8 xr[576], int ix[576], gr_info *cod_info,
            FLOAT8 xfsf[4][SBPSY_l], FLOAT8 distort[4][SBPSY_l],
            III_psy_xmin *l3_xmin,int gr, int ch, FLOAT8 *over_noise,
            FLOAT8 *tot_noise, FLOAT8 *max_noise, 
		    III_scalefac_t *scalefac)

{
    int start, end, sfb, l, i, over=0;
    FLOAT8 sum,step,bw;

    D192_3 *xr_s;
    I192_3 *ix_s;

    int count=0;
    FLOAT8 noise;
    *over_noise=0;
    *tot_noise=0;
    *max_noise=-999;

    xr_s = (D192_3 *) xr;
    ix_s = (I192_3 *) ix;

    step = pow( 2.0, (cod_info->quantizerStepSize) * 0.25 );

    for ( sfb = 0; sfb < cod_info->sfb_lmax; sfb++ )
    {
        start = scalefac_band_long[ sfb ];
        end   = scalefac_band_long[ sfb+1 ];
        bw = end - start;

        for ( sum = 0.0, l = start; l < end; l++ )
        {
            FLOAT8 temp;
            temp = fabs(xr[l]) - pow43[ix[l]] * step;
#ifdef MAXNOISE
	    temp = bw*temp*temp;
	    sum = Max(sum,temp);
#else
            sum += temp * temp;
#endif
	    
        }
        xfsf[0][sfb] = sum / bw;

	/* max -30db noise below threshold */
	noise = 10*log10(Max(.001,xfsf[0][sfb] / l3_xmin->l[gr][ch][sfb]));
        distort[0][sfb] = noise;
        if (noise>0) {
	  over++;
	  *over_noise += noise;
	}
	*tot_noise += noise;
	*max_noise=Max(*max_noise,noise);
	count++;

    }


    for ( i = 0; i < 3; i++ )
    {
        step = pow( 2.0, (cod_info->quantizerStepSize) * 0.25 ); 
        for ( sfb = cod_info->sfb_smax; sfb < SBPSY_s; sfb++ )
        {
            start = scalefac_band_short[ sfb ];
            end   = scalefac_band_short[ sfb+1 ];
            bw = end - start;

            for ( sum = 0.0, l = start; l < end; l++ )
            {
                FLOAT8 temp;
                temp = fabs((*xr_s)[l][i]) - pow43[(*ix_s)[l][i]] * step;
#ifdef MAXNOISE
		temp = bw*temp*temp;
		sum = Max(sum,temp);
#else
		sum += temp * temp;
#endif
            }       
            xfsf[i+1][sfb] = sum / bw;
	    /* max -30db noise below threshold */
	    noise = 10*log10(Max(.001,xfsf[i+1][sfb] / l3_xmin->s[gr][ch][sfb][i] ));
            distort[i+1][sfb] = noise;
            if (noise > 0) {
	      over++;
	      *over_noise += noise;
	    }
	    *tot_noise += noise;
	    *max_noise=Max(*max_noise,noise);
	    count++;	    
        }
    }

	if (count>1) *tot_noise /= count;
	if (over>1) *over_noise /= over;
	return over;
}








/*************************************************************************/
/*            preemphasis                                                */
/*************************************************************************/

/*
  See ISO 11172-3  section  C.1.5.4.3.4
*/
int preemphasis( FLOAT8 xr[576], FLOAT8 xrpow[576], 
     III_psy_xmin  *l3_xmin,
     int gr, int ch, III_side_info_t *l3_side, FLOAT8 distort[4][SBPSY_l] )
{
    int i, sfb, start, end, over;
    FLOAT8 ifqstep;
    gr_info *cod_info = &l3_side->gr[gr].ch[ch].tt;


    /*
      Preemphasis is switched on if in all the upper four scalefactor
      bands the actual distortion exceeds the threshold after the
      first call of the inner loop
    */
    over = 0;
    if ( cod_info->block_type != SHORT_TYPE && cod_info->preflag == 0 )
    {	
	for ( sfb = 17; sfb < SBPSY_l; sfb++ )
	    if ( distort[0][sfb]>0 ) over++;

	if (over == 4 )
	{
	    FLOAT8 t,t34;
	    cod_info->preflag = 1;
	    ifqstep = ( cod_info->scalefac_scale == 0 ) ? SQRT2 : 2.0;
	    for ( sfb = 11; sfb < cod_info->sfb_lmax; sfb++ )
	    {
		t=pow( ifqstep, (FLOAT8) pretab[sfb] );
		t34=sqrt(sqrt(t)*t);
		l3_xmin->l[gr][ch][sfb] *= t*t;
		start = scalefac_band_long[ sfb ];
		end   = scalefac_band_long[ sfb+1 ];
		for( i = start; i < end; i++ ) xr[i]*=t;
		for( i = start; i < end; i++ ) xrpow[i]*=t34;
	    }
	}
    }
    return (over == 4);
}




/*************************************************************************/
/*            amp_scalefac_bands                                         */
/*************************************************************************/

/* 
  Amplify the scalefactor bands that violate the masking threshold.
  See ISO 11172-3 Section C.1.5.4.3.5
*/
int amp_scalefac_bands( FLOAT8 xr[576], FLOAT8 xrpow[576], 
		    III_psy_xmin *l3_xmin, III_side_info_t *l3_side,
		    III_scalefac_t *scalefac,
		    int gr, int ch, int iteration, FLOAT8 distort[4][SBPSY_l])
{
    int start, end, l, sfb, i, over = 0;
    FLOAT8 ifqstep, ifqstep2, ifqstep34;
    FLOAT8 distort_thresh;
    D192_3 *xr_s;
    D192_3 *xrpow_s;
    gr_info *cod_info;
    cod_info = &l3_side->gr[gr].ch[ch].tt;

    xr_s = (D192_3 *) xr;
    xrpow_s = (D192_3 *) xrpow;

    if ( cod_info->scalefac_scale == 0 )
	ifqstep = SQRT2;
    else
	ifqstep = 2.0;


    ifqstep2 = ifqstep * ifqstep;
    ifqstep34=sqrt(sqrt(ifqstep)*ifqstep);

    /* distort_thresh = 0, unless all bands have distortion 
     * less than masking.  In that case, just amplify bands with distortion
     * within 95% of largest distortion/masking ratio */
    distort_thresh = -900;
    for ( sfb = 0; sfb < cod_info->sfb_lmax; sfb++ )
      distort_thresh = Max(1.05*distort[0][sfb],distort_thresh);
    distort_thresh=Min(distort_thresh,0.0);


    for ( sfb = 0; sfb < cod_info->sfb_lmax; sfb++ )
    {
	if ( distort[0][sfb]>distort_thresh  ) 
	{
	    over++;
	    scalefac->l[gr][ch][sfb]++;
	    start = scalefac_band_long[sfb];
	    end   = scalefac_band_long[sfb+1];
	    for ( l = start; l < end; l++ ) xr[l] *= ifqstep;
	    for ( l = start; l < end; l++ ) xrpow[l] *= ifqstep34;
	    l3_xmin->l[gr][ch][sfb] *= ifqstep2;

	}
    }



    /*
      Note that scfsi is not enabled for frames containing
      short blocks
    */

    distort_thresh = -900;
    for ( i = 0; i < 3; i++ )
      for ( sfb = cod_info->sfb_smax; sfb < 12; sfb++ ) {
	  distort_thresh = Max(1.05*distort[i+1][sfb],distort_thresh);
      }
    distort_thresh=Min(distort_thresh,0.0);


    for ( i = 0; i < 3; i++ )
      for ( sfb = cod_info->sfb_smax; sfb < 12; sfb++ ) {
            if ( distort[i+1][sfb]>distort_thresh)
            {
                over++;
                scalefac->s[gr][ch][sfb][i]++;
                start = scalefac_band_short[sfb];
                end   = scalefac_band_short[sfb+1];
                for ( l = start; l < end; l++ ) (*xr_s)[l][i] *= ifqstep;
		for ( l = start; l < end; l++ ) (*xrpow_s)[l][i] *= ifqstep34;
                l3_xmin->s[gr][ch][sfb][i] *= ifqstep2;
            }
      }
    return over;
}



int quant_compare(
int best_over,FLOAT8 best_tot_noise,FLOAT8 best_over_noise,FLOAT8 best_max_noise,
int over,FLOAT8 tot_noise, FLOAT8 over_noise, FLOAT8 max_noise)
{
  /*
    noise is given in decibals (db) relative to masking thesholds.

    over_noise:  sum of quantization noise > masking
    tot_noise:   sum of all quantization noise
    max_noise:   max quantization noise 

   */
  int better=0;

  if (experimentalX==0) {
    better = ((over < best_over) ||
	    ((over==best_over) && (over_noise<best_over_noise)) ) ;
  }

  if (experimentalX==1) 
    better = max_noise < best_max_noise;

  if (experimentalX==2) {
    better = tot_noise < best_tot_noise;
  }
  if (experimentalX==3) {
    better = (tot_noise < best_tot_noise) &&
      (max_noise < best_max_noise + 2);
  }
  if (experimentalX==4) {
    better = ( ( (0>=max_noise) && (best_max_noise>2)) ||
     ( (0>=max_noise) && (best_max_noise<0) && ((best_max_noise+2)>max_noise) && (tot_noise<best_tot_noise) ) ||
     ( (0>=max_noise) && (best_max_noise>0) && ((best_max_noise+2)>max_noise) && (tot_noise<(best_tot_noise+best_over_noise)) ) ||
     ( (0<max_noise) && (best_max_noise>-0.5) && ((best_max_noise+1)>max_noise) && ((tot_noise+over_noise)<(best_tot_noise+best_over_noise)) ) ||
     ( (0<max_noise) && (best_max_noise>-1) && ((best_max_noise+1.5)>max_noise) && ((tot_noise+over_noise+over_noise)<(best_tot_noise+best_over_noise+best_over_noise)) ) );
  }
  if (experimentalX==5) {
    better =   (over_noise <  best_over_noise)
      || ((over_noise == best_over_noise)&&(tot_noise < best_tot_noise));
  }

  return better;
}


int VBR_compare(
int best_over,FLOAT8 best_tot_noise,FLOAT8 best_over_noise,FLOAT8 best_max_noise,
int over,FLOAT8 tot_noise, FLOAT8 over_noise, FLOAT8 max_noise)
{
  /*
    noise is given in decibals (db) relative to masking thesholds.

    over_noise:  sum of quantization noise > masking
    tot_noise:   sum of all quantization noise
    max_noise:   max quantization noise 

   */
  int better=0;

  better = ((over <= best_over) &&
	    (over_noise<=best_over_noise) &&
	    (tot_noise<=best_tot_noise) &&
	    (max_noise<=best_max_noise));
  return better;
}
  







