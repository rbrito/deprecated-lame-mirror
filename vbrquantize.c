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
#include "util.h"
#include "l3side.h"
#include "quantize.h"
#include "l3bitstream.h"
#include "reservoir.h"
#include "quantize-pvt.h"
#ifdef HAVEGTK
#include "gtkanal.h"
#endif



#define DEBUGXX
FLOAT8 calc_sfb_ave_noise(FLOAT8 *xr, FLOAT8 *xr34, int stride, int bw, FLOAT8 sfpow)
{
  int j;
  FLOAT8 xfsf=0;
  FLOAT8 sfpow34 = pow(sfpow,3.0/4.0);

  for ( j=0; j < stride*bw ; j += stride) {
    int ix;
    FLOAT8 temp,temp2;

    /*    ix=(int)( xr34[j]/sfpow34  + 0.4054);*/
    ix=floor( xr34[j]/sfpow34);
    if (ix > IXMAX_VAL) return -1.0;

    temp = fabs(xr[j])- pow43[ix]*sfpow;
    if (ix < IXMAX_VAL) {
      temp2 = fabs(xr[j])- pow43[ix+1]*sfpow;
      if (fabs(temp2)<fabs(temp)) temp=temp2;
    }
#ifdef MAXQUANTERROR
    temp *= temp;
    xfsf = bw*Max(xfsf,temp);
#else
    xfsf += temp * temp;
#endif
  }
  return xfsf/bw;
}



FLOAT8 find_scalefac(FLOAT8 *xr,FLOAT8 *xr34,int stride,int sfb,
		     FLOAT8 l3_xmin,int bw)
{
  FLOAT8 xfsf,sfpow,sf,sf_ok,delsf;
  int sf4,sf_ok4,delsf4;
  int i;

  /* search will range from sf:  -52.25 -> 11.25  */
  /* search will range from sf4:  -209 -> 45  */
  sf = -20.5;
  sf4 = -82;
  delsf = 32;
  delsf4 = 128;

  sf_ok =10000; 
  sf_ok4=10000;
  for (i=0; i<7; i++) {
    delsf /= 2;
    delsf4 /= 2;
    sfpow = pow(2.0,sf);
    /* sfpow = pow(2.0,sf4/4.0); */
    xfsf = calc_sfb_ave_noise(xr,xr34,stride,bw,sfpow);

    if (xfsf < 0) {
      /* scalefactors too small */
      sf += delsf; 
      sf4 += delsf4;
    }else{
      if (sf_ok==10000) sf_ok=sf;  
      if (sf_ok4==10000) sf_ok4=sf4;  
      if (xfsf > l3_xmin)  {
	/* distortion.  try a smaller scalefactor */
	sf -= delsf;
	sf4 -= delsf4;
      }else{
	sf_ok=sf;
	sf_ok4 = sf4;
	sf += delsf;
	sf4 += delsf4;
      }
    }
  } 
  /* sf_ok accurate to within +/- 2*final_value_of_delsf */
  assert(sf_ok!=10000);

  /* NOTE: noise is not a monotone function of the sf, even though
   * the number of bits used is!  do a brute force search in the 
   * neighborhood of sf_ok: 
   * 
   *  sf = sf_ok + 1.75     works  1% of the time 
   *  sf = sf_ok + 1.50     works  1% of the time 
   *  sf = sf_ok + 1.25     works  2% of the time 
   *  sf = sf_ok + 1.00     works  3% of the time 
   *  sf = sf_ok + 0.75     works  9% of the time 
   *  sf = sf_ok + 0.50     0 %  (because it was tried above)
   *  sf = sf_ok + 0.25     works 39% of the time 
   *  sf = sf_ok + 0.00     works the rest of the time
   */

  sf = sf_ok + 0.75;
  sf4 = sf_ok4 + 3;

  while (sf>(sf_ok+.01)) { 
    /* sf = sf_ok + 2*delsf was tried above, skip it:  */
    if (fabs(sf-(sf_ok+2*delsf))  < .01) sf -=.25;
    if (sf4 == sf_ok4+2*delsf4) sf4 -=1;

    sfpow = pow(2.0,sf);
    /* sfpow = pow(2.0,sf4/4.0) */
    xfsf = calc_sfb_ave_noise(xr,xr34,stride,bw,sfpow);
    if (xfsf > 0) {
      if (xfsf <= l3_xmin) return sf;
    }
    sf -= .25;
    sf4 -= 1;
  }
  return sf_ok;
}


FLOAT8 max_range_short[2][SBPSY_s]={
  {21.5, 21.5, 21.5, 21.5, 21.5, 21.5 , 17.5,  17.5,  17.5,  17.5,  17.5,  17.5 },
  {29, 29, 29, 29, 29, 29 ,  21,    21,    21,    21,    21,     21 }
};

/*
    sfb=0..5  scalefac < 16 
    sfb>5     scalefac < 8

    ifqstep = ( cod_info->scalefac_scale == 0 ) ? .5 : 1.0;
    ol_sf =  (cod_info->global_gain-210.0)/4.0;
    ol_sf -= 2*cod_info->subblock_gain[i];
    ol_sf -= ifqstep*scalefac[gr][ch].s[sfb][i];

*/
FLOAT8 compute_scalefacs_short(FLOAT8 sf[SBPSY_s][3],gr_info *cod_info,
int scalefac[SBPSY_s][3],int sbg[3])
{
  FLOAT8 maxrange,maxrange1,maxrange2,maxover;
  int sfb,i;
  int ifqstep_inv = ( cod_info->scalefac_scale == 0 ) ? 2 : 1;

  maxover=0;
  maxrange1 = 15.0/ifqstep_inv;
  maxrange2 = 7.0/ifqstep_inv;
  for (i=0; i<3; ++i) {
    FLOAT8 maxsf1=0,maxsf2=0,minsf=1000;
    sbg[i]=0;
    /* see if we should use subblock gain */
    for ( sfb = 0; sfb < SBPSY_s; sfb++ ) {
      if (sfb < 6) {
	if (-sf[sfb][i]>maxsf1) maxsf1 = -sf[sfb][i];
      } else {
	if (-sf[sfb][i]>maxsf2) maxsf2 = -sf[sfb][i];
      }
      if (-sf[sfb][i]<minsf) minsf = -sf[sfb][i];
    }

    /* boost subblock gain as little as possible so we can
     * reach maxsf1 with scalefactors 
     * 2*sbg >= maxsf1   
     */
    maxsf1 = Max(maxsf1-maxrange1,maxsf2-maxrange2);

    if (minsf >0 ) sbg[i] = floor(minsf/2 + .001);
    if (maxsf1 > 0)  sbg[i]  = Max(sbg[i],ceil(maxsf1/2 - .0001));
    if (sbg[i] > 7) sbg[i]=7;

    for ( sfb = 0; sfb < SBPSY_s; sfb++ ) {
      /* ifqstep*scalefac + 2*subblock_gain >= -sf[sfb] */
      /* sf * ifqstep_inv = 3.0       scalefac=3.0 */
      /* sf * ifqstep_inv = 3.25-4.0  scalefac=4.0 */
      sf[sfb][i] += 2*sbg[i];

      if (sf[sfb][i] < 0) {
	maxrange = sfb < 6 ? maxrange1 : maxrange2;
	scalefac[sfb][i]=ceil( -sf[sfb][i]*ifqstep_inv - .0001);
	if (scalefac[sfb][i]>maxrange) scalefac[sfb][i]=maxrange;
	
	if (-(sf[sfb][i] + scalefac[sfb][i]*ifqstep_inv) >maxover)  {
	  maxover=-sf[sfb][i];
	}
      }
    }
  }
  return maxover;
}



FLOAT8 max_range_long[SBPSY_l]=
{15,   15,  15,  15,  15,  15,  15,  15,  15,  15,  15,   7,   7,   7,   7,   7,   7,   7,    7,    7,    7};


/*
	  sfb=0..10  scalefac < 16 
	  sfb>10     scalefac < 8
		
	  ifqstep = ( cod_info->scalefac_scale == 0 ) ? .5 : 1.0;
	  ol_sf =  (cod_info->global_gain-210.0)/4.0;
	  ol_sf -= ifqstep*scalefac[gr][ch].l[sfb];
	  if (cod_info->preflag && sfb>=11) 
	  ol_sf -= ifqstep*pretab[sfb];
*/
FLOAT8 compute_scalefacs_long(FLOAT8 sf[SBPSY_l],gr_info *cod_info,int scalefac[SBPSY_l])
{
  int sfb;
  FLOAT8 maxover;
  FLOAT8 ifqstep_inv = ( cod_info->scalefac_scale == 0 ) ? 2 : 1;
  
  if (cod_info->preflag)
    for ( sfb = 11; sfb < SBPSY_l; sfb++ ) 
      sf[sfb] += pretab[sfb]/ifqstep_inv;

  maxover=0;
  for ( sfb = 0; sfb < SBPSY_l; sfb++ ) {

    if (sf[sfb]<0) {
      /* ifqstep*scalefac >= -sf[sfb] */
      scalefac[sfb]=ceil( -sf[sfb]*ifqstep_inv  - .0001)   ;
      if (scalefac[sfb] > max_range_long[sfb]) scalefac[sfb]=max_range_long[sfb];
      
      /* sf[sfb] should now be positive: */
      if (  -(sf[sfb] + scalefac[sfb]/ifqstep_inv)  > maxover) {
	maxover = -sf[sfb];
      }
    }
  }
  return maxover;
}
  
  



/************************************************************************
 *
 * VBR_noise_shapping()
 *
 *
 ************************************************************************/
void
VBR_noise_shapping (lame_global_flags *gfp,
                FLOAT8 pe[2][2], FLOAT8 ms_ener_ratio[2],
                FLOAT8 xr[2][2][576], III_psy_ratio ratio[2][2],
                int l3_enc[2][2][576],
                III_scalefac_t scalefac[2][2])
{
  lame_internal_flags *gfc=gfp->internal_flags;
  FLOAT8    masking_lower_db;
  int       start,end,bw,sfb,l, i,ch, gr, bits;
  III_psy_xmin vbrsf;
  FLOAT8 maxover0,maxover1,maxover0p,maxover1p,maxover,vbrmax,vbrmin;
  FLOAT8 ifqstep;
  III_side_info_t * l3_side;
  gr_info *cod_info;  

  l3_side = &gfc->l3_side;
  iteration_init(gfp,l3_side,l3_enc);

  /* Adjust allowed masking based on quality setting */
  /* db_lower varies from -10 to +8 db */
  masking_lower_db = -10 + 2*gfp->VBR_q;
  /* adjust by -6(min)..0(max) depending on bitrate */
  masking_lower = pow(10.0,masking_lower_db/10);
  masking_lower = 1;


  bits = 0; 
  for (gr = 0; gr < gfc->mode_gr; gr++) {
    if (convert_mdct)
      ms_convert(xr[gr],xr[gr]);
    for (ch = 0; ch < gfc->stereo; ch++) { 
      FLOAT8 xr34[576];
      III_psy_xmin l3_xmin;

      int shortblock;
      cod_info = &l3_side->gr[gr].ch[ch].tt;
      shortblock = (cod_info->block_type == SHORT_TYPE);

      for(i=0;i<576;i++) {
	FLOAT8 temp=fabs(xr[gr][ch][i]);
	xr34[i]=sqrt(sqrt(temp)*temp);
      }

      calc_xmin( gfp,xr[gr][ch], &ratio[gr][ch], cod_info, &l3_xmin);

      vbrmax=-10000;
      vbrmin=10000;
      if (shortblock) {
	for ( sfb = 0; sfb < SBPSY_s; sfb++ )  {
	  for ( i = 0; i < 3; i++ ) {
	    start = gfc->scalefac_band.s[ sfb ];
	    end   = gfc->scalefac_band.s[ sfb+1 ];
	    bw = end - start;
	    vbrsf.s[sfb][i] = find_scalefac(&xr[gr][ch][3*start+i],&xr34[3*start+i],3,sfb,
		   masking_lower*l3_xmin.s[sfb][i],bw);
	    if (vbrsf.s[sfb][i]>vbrmax) vbrmax=vbrsf.s[sfb][i];
	    if (vbrsf.s[sfb][i]<vbrmin) vbrmin=vbrsf.s[sfb][i];
	  }
	}
      }else{
	for ( sfb = 0; sfb < SBPSY_l; sfb++ )   {
	  start = gfc->scalefac_band.l[ sfb ];
	  end   = gfc->scalefac_band.l[ sfb+1 ];
	  bw = end - start;
	  vbrsf.l[sfb] = find_scalefac(&xr[gr][ch][start],&xr34[start],1,sfb,
	  		 masking_lower*l3_xmin.l[sfb],bw);
	  if (vbrsf.l[sfb]>vbrmax) vbrmax = vbrsf.l[sfb];
	  if (vbrsf.l[sfb]<vbrmin) vbrmin = vbrsf.l[sfb];
	}

      } /* compute needed scalefactors */





      if (shortblock) {
	/******************************************************************
	 *
	 *  short block scalefacs
	 *
	 ******************************************************************/
	maxover0=0;
	maxover1=0;
	for ( sfb = 0; sfb < SBPSY_s; sfb++ ) {
	  for ( i = 0; i < 3; i++ ) {
	    maxover0 = Max(maxover0,(vbrmax - vbrsf.s[sfb][i]) - max_range_short[0][sfb] );
	    maxover1 = Max(maxover1,(vbrmax - vbrsf.s[sfb][i]) - max_range_short[1][sfb] );
	  }
	}
	if (maxover0==0) 
	  cod_info->scalefac_scale = 0;
	else if (maxover1==0)
	  cod_info->scalefac_scale = 1;
	else {
	  //	  printf("%i  extreme case (short)! maxover1=%f\n",(int)gfc->frameNum,maxover1);
	  cod_info->scalefac_scale = 1;
          vbrmax -= maxover1;
	}


	/* sf =  (cod_info->global_gain-210.0)/4.0; */
	cod_info->global_gain = floor(4*vbrmax +210 + .5);
	if (cod_info->global_gain>255) cod_info->global_gain=255;

	for ( sfb = 0; sfb < SBPSY_s; sfb++ ) {
	  for ( i = 0; i < 3; i++ ) {
	    vbrsf.s[sfb][i]-=vbrmax;
	  }
	}

	maxover=compute_scalefacs_short(vbrsf.s,cod_info,scalefac[gr][ch].s,cod_info->subblock_gain);
	if (maxover > 0) {
	  printf("not enought (short) maxover=%f \n",maxover);
	  exit(-1);
	}


	/* quantize xr34[] based on computed scalefactors */
	ifqstep = ( cod_info->scalefac_scale == 0 ) ? .5 : 1;
	for ( sfb = 0; sfb < SBPSY_s; sfb++ ) {
	  start = gfc->scalefac_band.s[ sfb ];
	  end   = gfc->scalefac_band.s[ sfb+1 ];
	  for (i=0; i<3; i++) {
	    FLOAT8 fac;
	    fac = (2*cod_info->subblock_gain[i]+ifqstep*scalefac[gr][ch].s[sfb][i]);
	    fac = pow(2.0,.75*fac);
	    for ( l = start; l < end; l++ ) 
	      xr34[3*l +i]*=fac;
	  }
	}



      }else{
	/******************************************************************
	 *
	 *  long block scalefacs
	 *
	 ******************************************************************/
	maxover0=0;
	maxover1=0;
	maxover0p=0;
	maxover1p=0;
	for ( sfb = 0; sfb < SBPSY_l; sfb++ ) {
	  maxover0 = Max(maxover0,(vbrmax - vbrsf.l[sfb]) - .5*max_range_long[sfb] );
	  maxover0p = Max(maxover0,(vbrmax - vbrsf.l[sfb]) - .5*(max_range_long[sfb]+pretab[sfb]) );
	  maxover1 = Max(maxover1,(vbrmax - vbrsf.l[sfb]) - max_range_long[sfb] );
	  maxover1p = Max(maxover1,(vbrmax - vbrsf.l[sfb]) - (max_range_long[sfb]+pretab[sfb]));
	}
        if (maxover0==0) {
	  cod_info->scalefac_scale = 0;
         } else if (maxover0p==0) {
            cod_info->scalefac_scale = 0;
            cod_info->preflag=1;
        } else if (maxover1==0) {
	  cod_info->scalefac_scale = 1;
        } else if (maxover1p==0) {
            cod_info->scalefac_scale = 1;
            cod_info->preflag=1;
	} else {
	  //	  printf("%i  extreme case (long)! maxover1=%f\n",(int)gfc->frameNum,maxover1);
	  cod_info->scalefac_scale = 1;
          vbrmax -= maxover1;
	}


	/* sf =  (cod_info->global_gain-210.0)/4.0; */
	cod_info->global_gain = floor(4*vbrmax +210 + .0001);
	if (cod_info->global_gain>255) cod_info->global_gain=255;

	for ( sfb = 0; sfb < SBPSY_l; sfb++ )   
	  vbrsf.l[sfb] -= vbrmax;

	maxover=compute_scalefacs_long(vbrsf.l,cod_info,scalefac[gr][ch].l);
	if (maxover > 0) {
	    printf("not enought (long) maxover=%f \n",maxover);
	    exit(-1);
	}

	/* quantize xr34[] based on computed scalefactors */
	ifqstep = ( cod_info->scalefac_scale == 0 ) ? .5 : 1;
	for ( sfb = 0; sfb < SBPSY_l; sfb++ ) {
          FLOAT8 fac;
	  fac = ifqstep*(scalefac[gr][ch].l[sfb]+cod_info->preflag);
	  assert(fac >= -vbrsf.l[sfb]);
	  fac=pow(2.0,.75*fac);
	  start = gfc->scalefac_band.l[ sfb ];
	  end   = gfc->scalefac_band.l[ sfb+1 ];
	  for ( l = start; l < end; l++ ) 
	    xr34[l]*=fac;
	}
      } 



      /* encode scalefacs */
      if ( gfc->version == 1 ) 
	scale_bitcount(&scalefac[gr][ch], cod_info);
      else
	scale_bitcount_lsf(&scalefac[gr][ch], cod_info);

      /* quantize xr34 */
      cod_info->part2_3_length = count_bits(gfp,l3_enc[gr][ch],xr34,cod_info);
      cod_info->part2_3_length += cod_info->part2_length;
      assert((int)count_bits != LARGE_BITS);

      if (gfc->use_best_huffman==1 && cod_info->block_type != SHORT_TYPE) {
	best_huffman_divide(gfc, gr, ch, cod_info, l3_enc[gr][ch]);
      }
      printf("bits for this xr34 = %i \n",cod_info->part2_3_length);



#ifdef HAVEGTK
      if (gfp->gtkflag) {
	FLOAT8 noise[4];
	FLOAT8 xfsf[4][SBPSY_l];
	FLOAT8 distort[4][SBPSY_l];

	noise[0]=calc_noise1( gfp, xr[gr][ch], l3_enc[gr][ch], cod_info, 
		  xfsf,distort, &l3_xmin, &scalefac[gr][ch], 
                  &noise[2],&noise[3],&noise[1]);

	set_pinfo (gfp, cod_info, &ratio[gr][ch], &scalefac[gr][ch], xr[gr][ch], xfsf, noise, gr, ch);
      }
#endif
    } /* ch */
  } /* gr */


  /* cant do this above since it may turn on scfsi and set
   * scalefacs = -1 in second granule  */
  for (gr = 0; gr < gfc->mode_gr; gr++){
    for (ch = 0; ch < gfc->stereo; ch++) {
      cod_info = &l3_side->gr[gr].ch[ch].tt;
      best_scalefac_store(gfp,gr, ch, l3_enc, l3_side, scalefac);
      bits+=cod_info->part2_3_length;
#ifdef HAVEGTK
      if (gfp->gtkflag)
	pinfo->LAMEmainbits[gr][ch]=cod_info->part2_3_length;
#endif
    }
  }
  

  
  /*******************************************************************
   * how many bits are available for each bitrate?
   *******************************************************************/
  { int bitsPerFrame,mean_bits;
  for( gfc->bitrate_index = 1;
       gfc->bitrate_index <= gfc->VBR_max_bitrate;
       gfc->bitrate_index++    ) {
    getframebits (gfp,&bitsPerFrame, &mean_bits);
    if (ResvFrameBegin(gfp,l3_side, mean_bits, bitsPerFrame)>=bits) break;
  }
  printf("bits=%i  needed index=%i \n",bits,gfc->bitrate_index);
  assert(gfc->bitrate_index <= gfc->VBR_max_bitrate);

  for (gr = 0; gr < gfc->mode_gr; gr++)
    for (ch = 0; ch < gfc->stereo; ch++) {
      cod_info = &l3_side->gr[gr].ch[ch].tt;
      ResvAdjust (gfp,cod_info, l3_side, mean_bits);

      /*******************************************************************
       * set the sign of l3_enc from the sign of xr
       *******************************************************************/
      for ( i = 0; i < 576; i++) {
        if (xr[gr][ch][i] < 0) l3_enc[gr][ch][i] *= -1;
      }
    }
  ResvFrameEnd (gfp,l3_side, mean_bits);
  }
}



