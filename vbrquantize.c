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
FLOAT8 compute_scalefacs_short(FLOAT8 vbrsf[SBPSY_s][3],gr_info *cod_info,
int scalefac[SBPSY_s][3],int sbg[3])
{
  FLOAT8 maxrange,maxrange1,maxrange2,maxover;
  FLOAT8 sf[SBPSY_s][3];
  int sfb,i;
  int ifqstep_inv = ( cod_info->scalefac_scale == 0 ) ? 2 : 1;

  /* make a working copy of the desired scalefacs */
  memcpy(sf,vbrsf,sizeof(sf));

  maxover=0;
  maxrange1 = 15.0/ifqstep_inv;
  maxrange2 = 7.0/ifqstep_inv;
  for (i=0; i<3; ++i) {
    FLOAT8 maxsf1=0,maxsf2=0;
    sbg[i]=0;
    /* see if we should use subblock gain */
    for ( sfb = 0; sfb < SBPSY_s; sfb++ ) {
      if (sfb < 6) {
	if (-sf[sfb][i]>maxsf1) maxsf1 = -sf[sfb][i];
      } else {
	if (-sf[sfb][i]>maxsf2) maxsf2 = -sf[sfb][i];
      }
    }

    /* boost subblock gain as little as possible so we can
     * reach maxsf1 with scalefactors */
    maxsf1 = Max(maxsf1-maxrange1,maxsf2-maxrange2);
    if (maxsf1 > 0) {
      sbg[i]  = floor(maxsf1/2 + .001);
      if (sbg[i] + cod_info->subblock_gain[i] > 7)
	sbg[i] = 7-cod_info->subblock_gain[i];
      assert(sbg[i]+cod_info->subblock_gain[i] <= 7);
    }

    for ( sfb = 0; sfb < SBPSY_s; sfb++ ) {
      /* ifqstep*scalefac + 2*subblock_gain >= -sf[sfb] */
      /* sf * ifqstep_inv = 3.0       scalefac=3.0 */
      /* sf * ifqstep_inv = 3.25-4.0  scalefac=4.0 */
      scalefac[sfb][i]=floor( -sf[sfb][i]*ifqstep_inv -2*sbg[i] +.75 + .0001);
      
      maxrange = (sfb<6) ? maxrange1 : maxrange2;
      if (maxrange + sf[sfb][i] > maxover) maxover = maxrange+sf[sfb][i];

      if (scalefac[sfb][i] > maxrange) scalefac[sfb][i]=maxrange;
    }

  }
  return maxover;
}



FLOAT8 max_range_long[2][SBPSY_l]={
    {7.5, 7.5, 7.5, 7.5, 7.5, 7.5, 7.5, 7.5, 7.5, 7.5, 7.5, 4, 4, 4, 4, 4.5, 4.5, 5,  5,  5,  5.5},
    {15,   15,  15,  15,  15,  15,  15,  15,  15,  15,  15, 9, 9, 9, 9, 10,  10, 11, 11, 11, 20}
};

/*
	  sfb=0..10  scalefac < 16 
	  sfb>10     scalefac < 8
		
	  ifqstep = ( cod_info->scalefac_scale == 0 ) ? .5 : 1.0;
	  ol_sf =  (cod_info->global_gain-210.0)/4.0;
	  ol_sf -= ifqstep*scalefac[gr][ch].l[sfb];
	  if (cod_info->preflag && sfb>=11) 
	  ol_sf -= ifqstep*pretab[sfb];
*/
FLOAT8 compute_scalefacs_long(FLOAT8 vbrsf[SBPSY_l],gr_info *cod_info,int scalefac[SBPSY_l])
{
  int sfb;
  FLOAT8 sf[SBPSY_l];
  FLOAT8 maxrange,maxover;
  int ifqstep_inv = ( cod_info->scalefac_scale == 0 ) ? 2 : 1;

  /* make a working copy of the desired scalefacs */
  memcpy(sf,vbrsf,SBPSY_l*sizeof(FLOAT8));

  cod_info->preflag=0;
  for ( sfb = 11; sfb < SBPSY_l; sfb++ ) {
    if (sf[sfb] + pretab[sfb]/ifqstep_inv > 0) break;
  }
  if (sfb==SBPSY_l) {
    cod_info->preflag=1;
    for ( sfb = 11; sfb < SBPSY_l; sfb++ ) 
      sf[sfb] += pretab[sfb]/ifqstep_inv;
  }

  maxover=0;
  for ( sfb = 0; sfb < SBPSY_l; sfb++ ) {
    if (sfb < 11) maxrange = 15.0/ifqstep_inv;
    else maxrange = 7.0/ifqstep_inv;

    /* ifqstep*scalefac >= -sf[sfb] */
    scalefac[sfb]=floor( -sf[sfb]*ifqstep_inv  +.75 + .0001)   ;
    if (scalefac[sfb] > maxrange) scalefac[sfb]=maxrange;
    sf[sfb] += (FLOAT8)scalefac[sfb]/(FLOAT8)ifqstep_inv;

    /* sf[sfb] should now be positive: */
    if (-sf[sfb] > maxover) maxover = -sf[sfb];

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
  III_psy_xmin l3_xmin[2][2];
  FLOAT8    masking_lower_db;
  int       start,end,bw,sfb, i,ch, gr, over;
  III_psy_xmin vbrsf;
  FLOAT8 maxover0,maxover1,maxover,vbrmax,vbrmin;
  III_side_info_t * l3_side;
  
  l3_side = &gfc->l3_side;

  iteration_init(gfp,l3_side,l3_enc);

  /* Adjust allowed masking based on quality setting */
  /* db_lower varies from -10 to +8 db */
  masking_lower_db = -10 + 2*gfp->VBR_q;
  /* adjust by -6(min)..0(max) depending on bitrate */
  masking_lower = pow(10.0,masking_lower_db/10);
  masking_lower = 1;


  for (gr = 0; gr < gfc->mode_gr; gr++) {
    if (convert_mdct)
      ms_convert(xr[gr],xr[gr]);
    for (ch = 0; ch < gfc->stereo; ch++) { 
      FLOAT8 xr34[576];
      gr_info *cod_info = &l3_side->gr[gr].ch[ch].tt;
      int shortblock;
      over = 0;
      shortblock = (cod_info->block_type == SHORT_TYPE);

      for(i=0;i<576;i++) {
	FLOAT8 temp=fabs(xr[gr][ch][i]);
	xr34[i]=sqrt(sqrt(temp)*temp);
      }

      calc_xmin( gfp,xr[gr][ch], &ratio[gr][ch], cod_info, &l3_xmin[gr][ch]);

      vbrmax=-10000;
      vbrmin=10000;
      if (shortblock) {
	for ( sfb = 0; sfb < SBPSY_s; sfb++ )  {
	  for ( i = 0; i < 3; i++ ) {
	    start = gfc->scalefac_band.s[ sfb ];
	    end   = gfc->scalefac_band.s[ sfb+1 ];
	    bw = end - start;
	    vbrsf.s[sfb][i] = find_scalefac(&xr[gr][ch][3*start+i],&xr34[3*start+i],3,sfb,
		   masking_lower*l3_xmin[gr][ch].s[sfb][i],bw);
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
	  		 masking_lower*l3_xmin[gr][ch].l[sfb],bw);
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
	int sbg[3];
	maxover0=0;
	maxover1=0;
	for ( sfb = 0; sfb < SBPSY_s; sfb++ ) {
	  for ( i = 0; i < 3; i++ ) {
	    maxover0 = Max(maxover0,(vbrmax - vbrsf.s[sfb][i]) - max_range_short[0][sfb] );
	    maxover1 = Max(maxover1,(vbrmax - vbrsf.s[sfb][i]) - max_range_short[1][sfb] );
	        printf("range=%f    this=%f  maxover1=%f  \n",max_range_short[1][sfb],
	                   (vbrmax - vbrsf.s[sfb][i]),maxover1);
	  }
	}
	if (maxover0==0) 
	  cod_info->scalefac_scale = 0;
	else if (maxover1==0)
	  cod_info->scalefac_scale = 1;
	else {
	  cod_info->scalefac_scale = 1;
          vbrmax -= maxover1;
	}


	/* sf =  (cod_info->global_gain-210.0)/4.0; */
	cod_info->global_gain = floor(4*vbrmax +210 + .5);

	for ( sfb = 0; sfb < SBPSY_s; sfb++ ) {
	  for ( i = 0; i < 3; i++ ) {
	    vbrsf.s[sfb][i]-=vbrmax;
	  }
	}


	/* first see if we should turn on subblock gain.
	 * does not depend on scalefac_scale */
	for (i=0; i<3; ++i) {
	  FLOAT8 minsf=1000;
	  for ( sfb = 0; sfb < SBPSY_s; sfb++ ) {
	    if (-vbrsf.s[sfb][i]<minsf) minsf = -vbrsf.s[sfb][i];
	  }
	  /* minsf = 0-1.75: no subblock gain
	   * minsf = 2-3.75: subblock gain = 1
           * minsf = 4-5.76: subblock_gain = 2  etc.... */
	  if (minsf > 0) {
	    cod_info->subblock_gain[i] = floor(minsf/2 + .001);
	    if (cod_info->subblock_gain[i] > 7)
	      cod_info->subblock_gain[i]=7;
	    for ( sfb = 0; sfb < SBPSY_s; sfb++ ) {
	      vbrsf.s[sfb][i] += 2*cod_info->subblock_gain[i];
	      assert(vbrsf.s[sfb][i] <= 0);
	    }
	  }
	}

	maxover=compute_scalefacs_short(vbrsf.s,cod_info,scalefac[gr][ch].s,sbg);
	if (maxover > 0) {
	  printf("not enought (short) maxover=%f \n",maxover);
	}
	for (i=0; i<3; ++i) {
	  cod_info->subblock_gain[i] += sbg[i];
	  assert(cod_info->subblock_gain[i] <= 7);
	}

      }else{
	/******************************************************************
	 *
	 *  long block scalefacs
	 *
	 ******************************************************************/
      /* sf =  (cod_info->global_gain-210.0)/4.0; */
      cod_info->global_gain = floor(4*vbrmax +210 + .5);

	for ( sfb = 0; sfb < SBPSY_l; sfb++ )   
	  vbrsf.l[sfb] -= vbrmax;

	/* can we get away with scalefac_scale=0? */
	cod_info->scalefac_scale = 0;
	maxover=compute_scalefacs_long(vbrsf.l,cod_info,scalefac[gr][ch].l);
	if (maxover > 0) {
	  cod_info->scalefac_scale = 1;
	  maxover=compute_scalefacs_long(vbrsf.l,cod_info,scalefac[gr][ch].l);
	  if (maxover > 0) {
	    /* what do we do now? */
	    printf("not enought (long) maxover=%f \n",maxover);
	  }
	}
      } 
    } /* ch */
  } /* gr */
}



