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
		FLOAT8 xr_org[2][2][576], III_psy_ratio ratio[2][2],
		III_side_info_t *l3_side, int l3_enc[2][2][576],
		III_scalefac_t scalefac[2][2], frame_params *fr_ps)
{
  III_psy_xmin l3_xmin[2];
  gr_info *cod_info;
  layer *info;
  int over[2];
  FLOAT8 noise[4]; /* over,max_noise,over_noise,tot_noise; */
  FLOAT8 targ_noise[4]; /* over,max_noise,over_noise,tot_noise; */
  int bitsPerFrame;
  int mean_bits;
  int ch, gr, i, bit_rate;
  FLOAT8 xr[2][576];


  iteration_init(xr_org,l3_side,l3_enc,fr_ps);
  info = fr_ps->header;
  bit_rate = bitrate[info->version][info->lay-1][info->bitrate_index];


  getframebits(info, &bitsPerFrame, &mean_bits);
  ResvFrameBegin( fr_ps, l3_side, mean_bits, bitsPerFrame );

  /* quantize! */



  for ( gr = 0; gr < gf.mode_gr; gr++ ) {
    if (convert_psy) {
      /* dual channel version can quantize Mid/Side channels with L/R
       * maskings (by constantly reconstructing L/R data).  Used before we
       * we had proper mid/side maskings. */
      outer_loop_dual( xr, xr_org[gr], mean_bits, bit_rate, over,
		       l3_xmin, l3_enc[gr], fr_ps, scalefac[gr],
		       gr, l3_side, ratio[gr], pe, ms_ener_ratio);

      /* finish up */
      for (ch=0 ; ch < gf.stereo ; ch ++ ) {
	cod_info = &l3_side->gr[gr].ch[ch].tt;
	best_scalefac_store(gr, ch, l3_side, scalefac);
#ifdef HAVEGTK
	if (gf.gtkflag)
	  pinfo->LAMEmainbits[gr][ch]=cod_info->part2_3_length;
#endif
	ResvAdjust(cod_info, l3_side, mean_bits );
	assert(cod_info->global_gain < 256 );
      }
    } else {
      int targ_bits[2];
      /* copy data to be quantized into xr */
      if (convert_mdct) ms_convert(xr, xr_org[gr]);
      else memcpy(xr, xr_org[gr],sizeof(FLOAT8)*2*576);   

      on_pe(pe,l3_side,targ_bits,mean_bits, gr);
#ifdef RH_SIDE_CBR
#warning **** YOU TURNED SIDE CHANNEL REDUCTION OFF (RH_SIDE_CBR) ****
#else
      if (reduce_sidechannel) 
	reduce_side(targ_bits,ms_ener_ratio[gr],mean_bits);
#endif      

      for (ch=0 ; ch < gf.stereo ; ch ++) {
	cod_info = &l3_side->gr[gr].ch[ch].tt;
	outer_loop( xr[ch], targ_bits[ch], noise, targ_noise, 0,
		    &l3_xmin[ch], l3_enc[gr][ch], fr_ps,
		    &scalefac[gr][ch], cod_info,
		    l3_side, &ratio[gr][ch], ms_ener_ratio[gr],gr,ch);


	best_scalefac_store(gr, ch, l3_side, scalefac);
	if (gf.use_best_huffman==1 && cod_info->block_type == NORM_TYPE) {
	  best_huffman_divide(gr, ch, cod_info, l3_enc[gr][ch]);
	}
#ifdef HAVEGTK
	if (gf.gtkflag)
	  pinfo->LAMEmainbits[gr][ch]=cod_info->part2_3_length;
#endif
	//	printf("%i %i %i bits=%i  %e %e %e %e \n",
	//          gf.frameNum,gr,ch,cod_info->part2_3_length,
	//          noise[0],noise[1],noise[2],noise[3] );

	//	printf("%i %i  sf=%i %i \n",gf.frameNum,l3_enc[gr][ch][0],
	//           scalefac[gr][ch].l[0],cod_info->global_gain);

	ResvAdjust(cod_info, l3_side, mean_bits );
      }
    }
    /* set the sign of l3_enc */
    for ( ch =  0; ch < gf.stereo; ch++ ) {
      for ( i = 0; i < 576; i++) {
	if (xr[ch][i] < 0)
	  l3_enc[gr][ch][i] *= -1;
      }
    }
  } /* loop over gr */

#if 0
  /* replace ResvAdjust above with this code if you do not want
     the second granule to use bits saved by the first granule.
     when combined with --nores, this is usefull for testing only */
  for ( gr = 0; gr < mode_gr; gr++ ) {
    for ( ch =  0; ch < stereo; ch++ ) {
	cod_info = &l3_side->gr[gr].ch[ch].tt;
	ResvAdjust( fr_ps, cod_info, l3_side, mean_bits );
    }
  }
#endif



  ResvFrameEnd(l3_side, mean_bits );
}


void set_masking_lower( int nbits )
{
	FLOAT masking_lower_db, adjust;
	
	/* quality setting */
	/* Adjust allowed masking based on quality setting */
	
#if  (RH_QUALITY_CONTROL == 1)	
#warning **** YOU ARE MODIFYING VBR QUALITY CONTROL (RH_QUALITY_CONTROL =1) ****
	/* lower masking depending on Quality setting */
	static FLOAT dbQ[10]={-9,-6,-3,-1.5,-1,-0.5,0,0.5,1,2};
	/* adjust masking, when less these Bits are used */	
	static FLOAT adB[10]={2250,2000,1750,1500,1250,1000,750,500,250,0};
	
	assert( gf.VBR_q <= 9 );
	assert( gf.VBR_q >= 0 );
	
	/* masking_lower_db varies from -9 to +2 db,
	 * based on quality setting
	 */
	masking_lower_db = dbQ[gf.VBR_q];
	
	/* adjust allowed masking by -4(min)..0(max) 
	 * depending on bitrate and quality setting
	 */
	adjust  = nbits > adB[gf.VBR_q] 
	        ? 0 
		: 2*(sin(PI*(nbits/adB[gf.VBR_q] - 0.5))-1.0);
	adjust *= reduce_sidechannel ? 0.707 : 1;
#elif (RH_QUALITY_CONTROL == 2)	
#warning **** YOU ARE MODIFYING VBR QUALITY CONTROL (RH_QUALITY_CONTROL =2) ****
	/* lower masking depending on Quality setting
	 * quality control together with adjusted ATH MDCT scaling
	 */
	static FLOAT dbQ[10]={-6,-4,-3,-2,-1,-0.5,0,0.25,0.5,1};
	
	assert( gf.VBR_q <= 9 );
	assert( gf.VBR_q >= 0 );
	
	masking_lower_db = dbQ[gf.VBR_q];	
	adjust = 0;
#elif (RH_QUALITY_CONTROL == 3)	
#warning **** YOU ARE MODIFYING VBR QUALITY CONTROL (RH_QUALITY_CONTROL =3) ****
	/* lower masking depending on Quality setting
	 * quality control together with adjusted ATH MDCT scaling
	 */
	static FLOAT dbQ[10]={-6.0,-4.5,-3.0,-1.5,0,0.3,0.6,1.0,1.5,2.0};
	
	assert( gf.VBR_q <= 9 );
	assert( gf.VBR_q >= 0 );
	
	masking_lower_db = dbQ[gf.VBR_q];	
	adjust = 0;
#else
	/* masking_lower varies from -8 to +10 db */
	masking_lower_db = -6 + 2*gf.VBR_q;
	/* adjust by -6(min)..0(max) depending on bitrate */
	adjust = (nbits-125)/(2500.0-125.0);
	adjust = 4*(adjust-1);
#endif
	masking_lower_db += adjust;
	masking_lower = pow(10.0,masking_lower_db/10);
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
                FLOAT8 xr[2][2][576], III_psy_ratio ratio[2][2],
                III_side_info_t * l3_side, int l3_enc[2][2][576],
                III_scalefac_t scalefac[2][2], frame_params * fr_ps)
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
  int       i,ch, gr, analog_silence;

  iteration_init(xr,l3_side,l3_enc,fr_ps);
  info = fr_ps->header;


  /*******************************************************************
   * how many bits are available for each bitrate?
   *******************************************************************/
  for( info->bitrate_index = 1;
       info->bitrate_index <= gf.VBR_max_bitrate;
       info->bitrate_index++    ) {
    getframebits (info, &bitsPerFrame, &mean_bits);
    if (info->bitrate_index == gf.VBR_min_bitrate) {
      /* always use at least this many bits per granule per channel */
      /* unless we detect analog silence, see below */
      min_mean_bits=mean_bits/gf.stereo;
    }
    frameBits[info->bitrate_index]=
      ResvFrameBegin (fr_ps, l3_side, mean_bits, bitsPerFrame);
  }

  info->bitrate_index=gf.VBR_max_bitrate;

  
  /*******************************************************************
   * how many bits would we use of it?
   *******************************************************************/
  analog_silence=0;
  for (gr = 0; gr < gf.mode_gr; gr++) {
    int num_chan=gf.stereo;
#ifdef  RH_SIDE_VBR
#warning **** YOU TURNED SIDE CHANNEL REDUCTION OFF (RH_SIDE_VBR) ****
#else	
    /* determine quality based on mid channel only */
    if (reduce_sidechannel) num_chan=1;  
#endif

    /* copy data to be quantized into xr */
    if (convert_mdct)
	ms_convert(xr[gr],xr[gr]);

    for (ch = 0; ch < num_chan; ch++) { 
      int real_bits;
      
      /******************************************************************
       * find smallest number of bits for an allowable quantization
       ******************************************************************/
      cod_info = &l3_side->gr[gr].ch[ch].tt;
      min_bits = Max(125,min_mean_bits);

      /* check for analolg silence */
      /* if energy < ATH, set min_bits = 125 */
      if (0==calc_xmin(xr[gr][ch], &ratio[gr][ch], cod_info, &l3_xmin)) {
	  analog_silence=1;
	  min_bits=125;
      }

      if (cod_info->block_type==SHORT_TYPE) {
	  min_bits += Max(1100,pe[gr][ch]);
	  min_bits=Min(min_bits,1800);
      }

      max_bits = 1200 + frameBits[gf.VBR_max_bitrate]/(gf.stereo*gf.mode_gr);
      max_bits=Min(max_bits,2500);
      max_bits=Max(max_bits,min_bits);


      /** in the case we will not find any better, we allocate max_bits
       ****************************************************************/
      save_bits[gr][ch] = max_bits;

      dbits = (max_bits-min_bits)/4;
      this_bits = (max_bits+min_bits)/2;
      real_bits = max_bits+1;

      /* bin search to within +/- 10 bits of optimal */
      do {
	  int better;
	  assert(this_bits>=min_bits);
	  assert(this_bits<=max_bits);

	  if( this_bits >= real_bits ){
	      /* 
	       * we already found a quantization with fewer bits
	       * so we can skip this try
	       */
	      save_bits[gr][ch] = this_bits;
	      this_bits -= dbits;
	      dbits /= 2;
	      continue; /* skips the rest of this do-while loop */
	  }

	  /* quality setting */
	  set_masking_lower( this_bits );

	  /* VBR will look for a quantization which has better values
	   * then those specified below.*/
	  targ_noise[0]=0;          /* over */
	  targ_noise[1]=0;          /* max_noise */
	  targ_noise[2]=0;          /* over_noise */
	  targ_noise[3]=0;          /* tot_noise */
	
	  targ_noise[0]=Max(0,targ_noise[0]);
	  targ_noise[2]=Max(0,targ_noise[2]);

	  outer_loop( xr[gr][ch], this_bits, noise, targ_noise, 1,
		      &l3_xmin, l3_enc[gr][ch], fr_ps,
		      &scalefac[gr][ch],
		      &l3_side->gr[gr].ch[ch].tt,
		      l3_side, &ratio[gr][ch], ms_ener_ratio[gr], gr, ch);

	  /* is quantization as good as we are looking for ? */
	  better=VBR_compare((int)targ_noise[0],targ_noise[3],targ_noise[2],
			     targ_noise[1],(int)noise[0],noise[3],noise[2],
			     noise[1]);
	  if (better) {
	      save_bits[gr][ch] = this_bits;
	      this_bits -= dbits;
	      real_bits = cod_info->part2_3_length;
	      /* 
	       * we now know it can be done with "real_bits"
	       * and maybe we can skip some iterations
	       */
	  } else {
	      this_bits += dbits;
	  }
	  dbits /= 2;
      } while (dbits>10) ;
      used_bits += save_bits[gr][ch];
      
    } /* for ch */
  } /* for gr */


#ifndef  RH_SIDE_VBR	
  if (reduce_sidechannel) {
    /* number of bits needed was found for MID channel above.  Use formula
     * (fixed bitrate code) to set the side channel bits */
    for (gr = 0; gr < gf.mode_gr; gr++) {
      FLOAT8 fac = .33*(.5-ms_ener_ratio[gr])/.5;
      save_bits[gr][1]=((1-fac)/(1+fac))*save_bits[gr][0];
      save_bits[gr][1]=Max(125,save_bits[gr][1]);
      used_bits += save_bits[gr][1];
    }
  }
#endif

  /******************************************************************
   * find lowest bitrate able to hold used bits
   ******************************************************************/
  for( info->bitrate_index =   (analog_silence ? 1 : gf.VBR_min_bitrate );
       info->bitrate_index < gf.VBR_max_bitrate;
       info->bitrate_index++    )
    if( used_bits <= frameBits[info->bitrate_index] ) break;

  /*******************************************************************
   * calculate quantization for this bitrate
   *******************************************************************/  
  getframebits (info, &bitsPerFrame, &mean_bits);
  bits=ResvFrameBegin (fr_ps, l3_side, mean_bits, bitsPerFrame);

  /* repartion available bits in same proportion */
  if (used_bits > bits ) {
    for( gr = 0; gr < gf.mode_gr; gr++) {
      for(ch = 0; ch < gf.stereo; ch++) {
	save_bits[gr][ch]=(save_bits[gr][ch]*frameBits[info->bitrate_index])/used_bits;
      }
    }
    used_bits=0;
    for( gr = 0; gr < gf.mode_gr; gr++) {
      for(ch = 0; ch < gf.stereo; ch++) {
	used_bits += save_bits[gr][ch];
      }
    }
  }
  assert(used_bits <= bits);

  for(gr = 0; gr < gf.mode_gr; gr++) {
    for(ch = 0; ch < gf.stereo; ch++) {
      /* quality setting */
      set_masking_lower( save_bits[gr][ch] );
      
      outer_loop( xr[gr][ch], save_bits[gr][ch], noise,targ_noise,0,
		  &l3_xmin, l3_enc[gr][ch], fr_ps, 
		  &scalefac[gr][ch],
		  &l3_side->gr[gr].ch[ch].tt,
		  l3_side, &ratio[gr][ch], ms_ener_ratio[gr], gr,ch);
    }
  }

  /*******************************************************************
   * update reservoir status after FINAL quantization/bitrate 
   *******************************************************************/
  for (gr = 0; gr < gf.mode_gr; gr++)
    for (ch = 0; ch < gf.stereo; ch++) {
      cod_info = &l3_side->gr[gr].ch[ch].tt;
      best_scalefac_store(gr, ch, l3_side, scalefac);
      if (cod_info->block_type == NORM_TYPE) {
	best_huffman_divide(gr, ch, cod_info, l3_enc[gr][ch]);
      }
#ifdef HAVEGTK
      if (gf.gtkflag)
	pinfo->LAMEmainbits[gr][ch]=cod_info->part2_3_length;
#endif
      ResvAdjust (cod_info, l3_side, mean_bits);
    }

  /*******************************************************************
   * set the sign of l3_enc 
   *******************************************************************/
  for (gr = 0; gr < gf.mode_gr; gr++)
    for (ch = 0; ch < gf.stereo; ch++) {
      int      *pi = &l3_enc[gr][ch][0];

      for (i = 0; i < 576; i++) {
        FLOAT8    pr = xr[gr][ch][i];

        if ((pr < 0) && (pi[i] > 0))
          pi[i] *= -1;
      }
    }

  ResvFrameEnd (l3_side, mean_bits);
}




/************************************************************************/
/*  init_outer_loop  mt 6/99                                            */
/************************************************************************/
void init_outer_loop(
    FLOAT8 xr[576],        /*  could be L/R OR MID/SIDE */
    III_scalefac_t *scalefac, /* scalefactors */
    gr_info *cod_info,
    III_side_info_t *l3_side)
{
  int i;

  /* reset of iteration variables */
  memset(scalefac, 0, sizeof(III_scalefac_t));

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
  cod_info->global_gain       = 210;
  cod_info->count1table_select= 0;
  cod_info->count1bits        = 0;
  
  
  if (gf.experimentalZ) {
    /* compute subblock gains */
    int j,b;  FLOAT8 en[3],mx;
    if ((cod_info->block_type==SHORT_TYPE) ) {
      /* estimate energy within each subblock */
      for (b=0; b<3; b++) en[b]=0;
      for ( i=0,j = 0; j < 192; j++ ) {
	for (b=0; b<3; b++) {
	  en[b]+=xr[i] * xr[i];
	  i++;
	}
      }
      mx = 1e-12;
      for (b=0; b<3; b++) mx=Max(mx,en[b]);
      for (b=0; b<3; b++) en[b] = Max(en[b],1e-12)/mx;
      /*printf("ener = %4.2f  %4.2f  %4.2f  \n",en[0],en[1],en[2]);*/
      /* pick gain so that 2^(2gain)*en[0] = 1  */
      /* gain = .5* log( 1/en[0] )/LOG2 = -.5*log(en[])/LOG2 */
      for (b=0; b<3; b++) {
	cod_info->subblock_gain[b] = (int)(-.5*log(en[b])/LOG2 + 0.5);
	if (cod_info->subblock_gain[b] > 2) 
	  cod_info->subblock_gain[b]=2;
	if (cod_info->subblock_gain[b] < 0) 
	  cod_info->subblock_gain[b]=0;
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
    FLOAT8 xr[576],        
    int targ_bits,
    FLOAT8 best_noise[4],
    FLOAT8 targ_noise[4],
    int sloppy,
    III_psy_xmin *l3_xmin,   /* the allowed distortion of the scalefactor */
    int l3_enc[576],         /* vector of quantized values ix(0..575) */
    frame_params *fr_ps,
    III_scalefac_t *scalefac, /* scalefactors */
    gr_info *cod_info,
    III_side_info_t *l3_side,
    III_psy_ratio *ratio, FLOAT8 ms_ener_ratio,
    int gr,int ch)
{
  int i, iteration;
  int status,count=0,bits_found=0;
  III_scalefac_t scalefac_w;
  int huff_bits;
  FLOAT8 xrpow[576],temp;
  int l3_enc_w[576]; 
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
  FLOAT8 xfsf[4][SBPSY_l];
  FLOAT8 distort[4][SBPSY_l];

  int compute_stepsize=1;
  int notdone=1;

  /* compute max allowed distortion */
  calc_xmin(xr, ratio, cod_info, l3_xmin);

  init_outer_loop(xr, &scalefac_w, cod_info,l3_side);
  best_over = 100;
  count=0;
  for (i=0; i<576; i++) {
    if ( fabs(xr[i]) > 0 ) count++; 
  }
  if (count==0) {
    /* cod_info was set in init_outer_loop above */
    memset(scalefac,0,sizeof(III_scalefac_t));
    memset(l3_enc,0,576*sizeof(int));
    best_noise[0]=0;
    best_noise[1]=0;
    best_noise[2]=0;
    best_noise[3]=0;
    return;
  }
  

  
  /* BEGIN MAIN LOOP */
  iteration = 0;
  while ( notdone  ) {
    static FLOAT8 OldValue[2] = {180, 180};
    int try_scale=0;
    iteration ++;

    if (compute_stepsize) {
      /* init and compute initial quantization step */
      compute_stepsize=0;
      for (i=0;i<576;i++) {
	temp=fabs(xr[i]);
	xrpow[i]=sqrt(sqrt(temp)*temp);
      }
      bits_found=bin_search_StepSize2(targ_bits,OldValue[ch],
				      l3_enc_w,xr,xrpow,cod_info);
      OldValue[ch] = cod_info->global_gain;
    }


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
	  real_bits = inner_loop(xrpow, l3_enc_w, huff_bits, cod_info);
	} else real_bits=bits_found;
      }
      else 
	real_bits=inner_loop(xrpow, l3_enc_w, huff_bits, cod_info);
      cod_info->part2_3_length = real_bits;

      /* compute the distortion in this quantization */
      if (gf.noise_shaping==0) {
      	over=0;
      }else{
	/* coefficients and thresholds both l/r (or both mid/side) */
	over=calc_noise1( xr, l3_enc_w, cod_info, 
			  xfsf,distort, l3_xmin, &scalefac_w, &over_noise, 
			  &tot_noise, &max_noise);

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
	
	memcpy(scalefac, &scalefac_w, sizeof(III_scalefac_t));
	memcpy(l3_enc,l3_enc_w,sizeof(int)*576);
	memcpy(&save_cod_info,cod_info,sizeof(save_cod_info));

#ifdef HAVEGTK
	if (gf.gtkflag) {
	  int sfb;
	  FLOAT ifqstep;
	  int l,start,end,bw;
	  FLOAT8 en0;
	  D192_3 *xr_s = (D192_3 *)xr;
	  ifqstep = ( cod_info->scalefac_scale == 0 ) ? .5 : 1.0;
	  
	  if (cod_info->block_type == SHORT_TYPE) {
	    for ( i = 0; i < 3; i++ ) {
	      for ( sfb = 0; sfb < SBPSY_s; sfb++ )  {
		start = scalefac_band.s[ sfb ];
		end   = scalefac_band.s[ sfb + 1 ];
		bw = end - start;
		for ( en0 = 0.0, l = start; l < end; l++ ) 
		  en0 += (*xr_s)[l][i] * (*xr_s)[l][i];
		en0=Max(en0/bw,1e-20);
		
		/* conversion to FFT units */
		en0 = ratio->en.s[sfb][i]/en0;
		
		pinfo->xfsf_s[gr][ch][3*sfb+i] =  xfsf[i+1][sfb]*en0;
		pinfo->thr_s[gr][ch][3*sfb+i] = ratio->thm.s[sfb][i];
		pinfo->en_s[gr][ch][3*sfb+i] = ratio->en.s[sfb][i]; 
		
		pinfo->LAMEsfb_s[gr][ch][3*sfb+i]=
		  -2*cod_info->subblock_gain[i]-ifqstep*scalefac_w.s[sfb][i];
	      }
	    }
	  }else{
	    for ( sfb = 0; sfb < SBPSY_l; sfb++ )   {
	      start = scalefac_band.l[ sfb ];
	      end   = scalefac_band.l[ sfb+1 ];
	      bw = end - start;
	      for ( en0 = 0.0, l = start; l < end; l++ ) 
		en0 += xr[l] * xr[l];
	      en0=Max(en0/bw,1e-20);
	      /*
		printf("diff  = %f \n",10*log10(Max(ratio[gr][ch].en.l[sfb],1e-20))
		-(10*log10(en0)+150));
	      */
	      
	      /* convert to FFT units */
	      en0 =   ratio->en.l[sfb]/en0;
	      
	      pinfo->xfsf[gr][ch][sfb] =  xfsf[0][sfb]*en0;
	      pinfo->thr[gr][ch][sfb] = ratio->thm.l[sfb];
	      pinfo->en[gr][ch][sfb] = ratio->en.l[sfb];
	      
	      pinfo->LAMEsfb[gr][ch][sfb]=-ifqstep*scalefac_w.l[sfb];
	      if (cod_info->preflag && sfb>=11) 
		pinfo->LAMEsfb[gr][ch][sfb]-=ifqstep*pretab[sfb];
	    }
	  }
	  pinfo->LAMEqss[gr][ch] = cod_info->global_gain;
	  pinfo->over[gr][ch]=over;
	  pinfo->max_noise[gr][ch]=max_noise;
	  pinfo->tot_noise[gr][ch]=tot_noise;
	  pinfo->over_noise[gr][ch]=over_noise;
	}
#endif
      }
    }
    
    /* if no bands with distortion, we are done */
    if (gf.noise_shaping_stop==0)
      if (over==0) notdone=0;

    /* in sloppy mode, as soon as we know we can do better than targ_noise,
       quit.  This is used for the inital VBR bin search.  Turn it off for
       final (optimal) quantization */
    if (sloppy && notdone) notdone = 
        !VBR_compare((int)targ_noise[0],targ_noise[3],targ_noise[2],
         targ_noise[1],over,tot_noise,over_noise,max_noise);



    if (notdone) {
	amp_scalefac_bands( xrpow, cod_info, &scalefac_w, distort);
	/* check to make sure we have not amplified too much */
	/* loop_break returns 0 if there is an unamplified scalefac */
	/* scale_bitcount returns 0 if no scalefactors are too large */
	if ( (status = loop_break(&scalefac_w, cod_info)) == 0 ) {
	    if ( fr_ps->header->version == 1 ) {
		status = scale_bitcount(&scalefac_w, cod_info);
	    }else{
		status = scale_bitcount_lsf(&scalefac_w, cod_info);
	    }
	    if (status && (cod_info->scalefac_scale==0)) try_scale=1; 
	}
	notdone = !status;
    }



    if (try_scale && gf.experimentalY && (sloppy || !gf.VBR)) {
      init_outer_loop(xr, &scalefac_w, cod_info,l3_side);
      compute_stepsize=1;  /* compute a new global gain */
      notdone=1;
      cod_info->scalefac_scale=1;
    }
  }    /* done with main iteration */

  /* in sloppy mode we don´t need to restore the quantization, 
   * cos we didn´t saved it and we don´t need it, as we only want
   * to know if we can do better
   */
  if (count) {
    memcpy(cod_info,&save_cod_info,sizeof(save_cod_info));
    cod_info->part2_3_length += cod_info->part2_length;
  }
  /* finish up */
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
		 III_psy_xmin *l3_xmin, III_scalefac_t *scalefac,
		 FLOAT8 *over_noise,
		 FLOAT8 *tot_noise, FLOAT8 *max_noise)
{
    int start, end, sfb, l, i, over=0;
    FLOAT8 sum,step,bw;
#if RH_ATH
    FLOAT8 ath_max;
#endif

    int count=0;
    FLOAT8 noise;
    *over_noise=0;
    *tot_noise=0;
    *max_noise=-999;

    for ( sfb = 0; sfb < cod_info->sfb_lmax; sfb++ ) {
	FLOAT8 step;
	int s = scalefac->l[sfb];

	if (cod_info->preflag)
	    s += pretab[sfb];

	s = cod_info->global_gain - (s << (cod_info->scalefac_scale + 1));
	step = POW20(s);

	start = scalefac_band.l[ sfb ];
        end   = scalefac_band.l[ sfb+1 ];
        bw = end - start;

#if RH_ATH
        ath_max = 0;
#endif
        for ( sum = 0.0, l = start; l < end; l++ )
        {
            FLOAT8 temp;
            temp = fabs(xr[l]) - pow43[ix[l]] * step;
#ifdef MAXNOISE
	    temp = bw*temp*temp;
	    sum = Max(sum,temp);
#elif RH_ATH
#warning **** YOU ARE USING A DIFFERENT NOISE CALCULATION (RH_ATH) ****
	    temp = temp*temp;
            sum += temp;
	    ath_max = Max( ath_max, temp/ATH_mdct_long[l] );
#else
            sum += temp * temp;
#endif
	    
        }
        xfsf[0][sfb] = sum / bw;

	/* max -30db noise below threshold */
#if RH_ATH
	noise = 10*log10(Max(.001,Min(ath_max,xfsf[0][sfb]/l3_xmin->l[sfb])));
#else
	noise = 10*log10(Max(.001,xfsf[0][sfb] / l3_xmin->l[sfb]));
#endif
        distort[0][sfb] = noise;
        if (noise>0) {
	  over++;
	  *over_noise += noise;
	}
	*tot_noise += noise;
	*max_noise=Max(*max_noise,noise);
	count++;

    }


    for ( i = 0; i < 3; i++ ) {
        for ( sfb = cod_info->sfb_smax; sfb < SBPSY_s; sfb++ ) {
	    int s;

	    s = (scalefac->s[sfb][i] << (cod_info->scalefac_scale + 1))
		+ cod_info->subblock_gain[i] * 8;
	    s = cod_info->global_gain - s;

	    step = POW20(s);
	    start = scalefac_band.s[ sfb ];
	    end   = scalefac_band.s[ sfb+1 ];
            bw = end - start;
#if RH_ATH
	    ath_max = 0;
#endif
	    for ( sum = 0.0, l = start; l < end; l++ ) {
		FLOAT8 temp;
		temp = fabs(xr[l * 3 + i]) - pow43[ix[l * 3 + i]] * step;
#ifdef MAXNOISE
		temp = bw*temp*temp;
		sum = Max(sum,temp);
#elif RH_ATH
		temp = temp*temp;
		sum += temp;
		ath_max = Max( ath_max, temp/ATH_mdct_short[l] );
#else
		sum += temp * temp;
#endif
            }       
	    xfsf[i+1][sfb] = sum / bw;
	    /* max -30db noise below threshold */
#if RH_ATH
	    noise = 10*log10(Max(.001,Min(ath_max,xfsf[i+1][sfb]/l3_xmin->s[sfb][i])));
#else
	    noise = 10*log10(Max(.001,xfsf[i+1][sfb] / l3_xmin->s[sfb][i] ));
#endif
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
/*            amp_scalefac_bands                                         */
/*************************************************************************/

/* 
  Amplify the scalefactor bands that violate the masking threshold.
  See ISO 11172-3 Section C.1.5.4.3.5
*/
void amp_scalefac_bands(FLOAT8 xrpow[576], 
			gr_info *cod_info,
			III_scalefac_t *scalefac,
			FLOAT8 distort[4][SBPSY_l])
{
    int start, end, l, sfb, i;
    FLOAT8 ifqstep34;
    FLOAT8 distort_thresh;

    if ( cod_info->scalefac_scale == 0 )
	ifqstep34 = 1.29683955465100964055;
    else
	ifqstep34 = 1.68179283050742922612;

    /* distort_thresh = 0, unless all bands have distortion 
     * less than masking.  In that case, just amplify bands with distortion
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
    distort_thresh=Min(distort_thresh * 1.05, 0.0);



    for ( sfb = 0; sfb < cod_info->sfb_lmax; sfb++ ) {
	if ( distort[0][sfb]>distort_thresh  ) {
	    scalefac->l[sfb]++;
	    start = scalefac_band.l[sfb];
	    end   = scalefac_band.l[sfb+1];
	    for ( l = start; l < end; l++ )
		xrpow[l] *= ifqstep34;
	}
    }


    for ( i = 0; i < 3; i++ ) {
	for ( sfb = cod_info->sfb_smax; sfb < 12; sfb++ ) {
            if ( distort[i+1][sfb]>distort_thresh) {
                scalefac->s[sfb][i]++;
                start = scalefac_band.s[sfb];
                end   = scalefac_band.s[sfb+1];
		for (l = start; l < end; l++)
		    xrpow[l * 3 + i] *= ifqstep34;
            }
	}
    }
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

  if (gf.experimentalX==0) {
    better = ((over < best_over) ||
	      ((over==best_over) && (over_noise<=best_over_noise)) ) ;
	      //	    ((over==best_over) && (over_noise<best_over_noise)) ) ;
  }

  if (gf.experimentalX==1) 
    better = max_noise < best_max_noise;

  if (gf.experimentalX==2) {
    better = tot_noise < best_tot_noise;
  }
  if (gf.experimentalX==3) {
    better = (tot_noise < best_tot_noise) &&
      (max_noise < best_max_noise + 2);
  }
  if (gf.experimentalX==4) {
    better = ( ( (0>=max_noise) && (best_max_noise>2)) ||
     ( (0>=max_noise) && (best_max_noise<0) && ((best_max_noise+2)>max_noise) && (tot_noise<best_tot_noise) ) ||
     ( (0>=max_noise) && (best_max_noise>0) && ((best_max_noise+2)>max_noise) && (tot_noise<(best_tot_noise+best_over_noise)) ) ||
     ( (0<max_noise) && (best_max_noise>-0.5) && ((best_max_noise+1)>max_noise) && ((tot_noise+over_noise)<(best_tot_noise+best_over_noise)) ) ||
     ( (0<max_noise) && (best_max_noise>-1) && ((best_max_noise+1.5)>max_noise) && ((tot_noise+over_noise+over_noise)<(best_tot_noise+best_over_noise+best_over_noise)) ) );
  }
  if (gf.experimentalX==5) {
    better =   (over_noise <  best_over_noise)
      || ((over_noise == best_over_noise)&&(tot_noise < best_tot_noise));
  }
  if (gf.experimentalX==6) {
    better = (over_noise < best_over_noise)
           ||( (over_noise == best_over_noise)
             &&( (max_noise < best_max_noise)
               ||( (max_noise == best_max_noise)
                 &&(tot_noise <= best_tot_noise)
                 )
               ) 
	     );
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
  







