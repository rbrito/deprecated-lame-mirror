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
#include "reservoir.h"
#include "quantize-pvt.h"
#include "gtkanal.h"


#if (defined(__GNUC__) && defined(__i386__))
#define USE_GNUC_ASM
#endif
#ifdef _MSC_VER
#define USE_MSC_ASM
#endif



/*********************************************************************
 * XRPOW_FTOI is a macro to convert floats to ints.  
 * if XRPOW_FTOI(x) = nearest_int(x), then QUANTFAC(x)=adj43asm[x]
 *                                         ROUNDFAC= -0.0946
 *
 * if XRPOW_FTOI(x) = floor(x), then QUANTFAC(x)=asj43[x]   
 *                                   ROUNDFAC=0.4054
 *********************************************************************/
#ifdef USE_GNUC_ASM
#  define QUANTFAC(rx)  adj43asm[rx]
#  define ROUNDFAC -0.0946
#  define XRPOW_FTOI(src, dest) \
     asm ("fistpl %0 " : "=m"(dest) : "t"(src) : "st")
#elif defined (USE_MSC_ASM)
#  define QUANTFAC(rx)  adj43asm[rx]
#  define ROUNDFAC -0.0946
#  define XRPOW_FTOI(src, dest) do { \
     FLOAT8 src_ = (src); \
     int dest_; \
     { \
       __asm fld src_ \
       __asm fistp dest_ \
     } \
     (dest) = dest_; \
   } while (0)
#else
#  define QUANTFAC(rx)  adj43[rx]
#  define ROUNDFAC 0.4054
#  define XRPOW_FTOI(src,dest) ((dest) = (int)(src))
#endif



#undef MAXQUANTERROR


FLOAT8 calc_sfb_noise(FLOAT8 *xr, FLOAT8 *xr34, int bw, int sf)
{
  int j;
  FLOAT8 xfsf=0;
  FLOAT8 sfpow,sfpow34;

  sfpow = POW20(sf+210); /*pow(2.0,sf/4.0); */
  sfpow34  = IPOW20(sf+210); /*pow(sfpow,-3.0/4.0);*/

  for ( j=0; j < bw ; ++j) {
    int ix;
    FLOAT8 temp;

#if 0
    if (xr34[j]*sfpow34 > IXMAX_VAL) return -1;
    ix=floor( xr34[j]*sfpow34);
    temp = fabs(xr[j])- pow43[ix]*sfpow;
    temp *= temp;

    if (ix < IXMAX_VAL) {
      temp2 = fabs(xr[j])- pow43[ix+1]*sfpow;
      temp2 *=temp2;
      if (temp2<temp) {
	temp=temp2;
	++ix;
      }
    }
#else
    if (xr34[j]*sfpow34 > IXMAX_VAL) return -1;

    temp = xr34[j]*sfpow34;
    XRPOW_FTOI(temp, ix);
    XRPOW_FTOI(temp + QUANTFAC(ix), ix);
    temp = fabs(xr[j])- pow43[ix]*sfpow;
    temp *= temp;
    
#endif
    
#ifdef MAXQUANTERROR
    xfsf = Max(xfsf,temp);
#else
    xfsf += temp;
#endif
  }
#ifdef MAXQUANTERROR
  return xfsf;
#else
  return xfsf/bw;
#endif
}





FLOAT8 calc_sfb_noise_ave(FLOAT8 *xr, FLOAT8 *xr34, int bw,int sf)
{
  int j;
  FLOAT8 xfsf=0, xfsf_p1=0, xfsf_m1=0;
  FLOAT8 sfpow34,sfpow34_p1,sfpow34_m1;
  FLOAT8 sfpow,sfpow_p1,sfpow_m1;

  sfpow = POW20(sf+210); /*pow(2.0,sf/4.0); */
  sfpow34  = IPOW20(sf+210); /*pow(sfpow,-3.0/4.0);*/

  sfpow_m1 = sfpow*.8408964153;  /* pow(2,(sf-1)/4.0) */
  sfpow34_m1 = sfpow34*1.13878863476;       /* .84089 ^ -3/4 */

  sfpow_p1 = sfpow*1.189207115;  
  sfpow34_p1 = sfpow34*0.878126080187;

  for ( j=0; j < bw ; ++j) {
    int ix;
    FLOAT8 temp,temp_p1,temp_m1;

    if (xr34[j]*sfpow34_m1 > IXMAX_VAL) return -1;

    temp = xr34[j]*sfpow34;
    XRPOW_FTOI(temp, ix);
    XRPOW_FTOI(temp + QUANTFAC(ix), ix);
    temp = fabs(xr[j])- pow43[ix]*sfpow;
    temp *= temp;

    temp_p1 = xr34[j]*sfpow34_p1;
    XRPOW_FTOI(temp_p1, ix);
    XRPOW_FTOI(temp_p1 + QUANTFAC(ix), ix);
    temp_p1 = fabs(xr[j])- pow43[ix]*sfpow_p1;
    temp_p1 *= temp_p1;
    
    temp_m1 = xr34[j]*sfpow34_m1;
    XRPOW_FTOI(temp_m1, ix);
    XRPOW_FTOI(temp_m1 + QUANTFAC(ix), ix);
    temp_m1 = fabs(xr[j])- pow43[ix]*sfpow_m1;
    temp_m1 *= temp_m1;

#ifdef MAXQUANTERROR
    xfsf = Max(xfsf,temp);
    xfsf_p1 = Max(xfsf_p1,temp_p1);
    xfsf_m1 = Max(xfsf_m1,temp_m1);
#else
    xfsf += temp;
    xfsf_p1 += temp_p1;
    xfsf_m1 += temp_m1;
#endif
  }
  if (xfsf_p1>xfsf) xfsf = xfsf_p1;
  if (xfsf_m1>xfsf) xfsf = xfsf_m1;
#ifdef MAXQUANTERROR
  return xfsf;
#else
  return xfsf/bw;
#endif
}



int find_scalefac(FLOAT8 *xr,FLOAT8 *xr34,int sfb,
		     FLOAT8 l3_xmin,int bw)
{
  FLOAT8 xfsf;
  int i,sf,sf_ok,delsf;

  /* search will range from sf:  -209 -> 45  */
  sf = -82;
  delsf = 128;

  sf_ok=10000;
  for (i=0; i<7; i++) {
    delsf /= 2;
    //      xfsf = calc_sfb_noise(xr,xr34,bw,sf);
    xfsf = calc_sfb_noise_ave(xr,xr34,bw,sf);

    if (xfsf < 0) {
      /* scalefactors too small */
      sf += delsf;
    }else{
      if (sf_ok==10000) sf_ok=sf;  
      if (xfsf > l3_xmin)  {
	/* distortion.  try a smaller scalefactor */
	sf -= delsf;
      }else{
	sf_ok = sf;
	sf += delsf;
      }
    }
  } 
  assert(sf_ok!=10000);
  //  assert(delsf==1);  /* when for loop goes up to 7 */

  return sf;
}



/*
    sfb=0..5  scalefac < 16 
    sfb>5     scalefac < 8

    ifqstep = ( cod_info->scalefac_scale == 0 ) ? 2 : 4;
    ol_sf =  (cod_info->global_gain-210.0);
    ol_sf -= 8*cod_info->subblock_gain[i];
    ol_sf -= ifqstep*scalefac[gr][ch].s[sfb][i];

*/
int compute_scalefacs_short(int sf[SBPSY_s][3],gr_info *cod_info,
int scalefac[SBPSY_s][3],unsigned int sbg[3])
{
  int maxrange,maxrange1,maxrange2,maxover;
  int sfb,i;
  int ifqstep = ( cod_info->scalefac_scale == 0 ) ? 2 : 4;

  maxover=0;
  maxrange1 = 15;
  maxrange2 = 7;


  for (i=0; i<3; ++i) {
    int maxsf1=0,maxsf2=0,minsf=1000;
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
     * 8*sbg >= maxsf1   
     */
    maxsf1 = Max(maxsf1-maxrange1*ifqstep,maxsf2-maxrange2*ifqstep);
    sbg[i]=0;
    if (minsf >0 ) sbg[i] = floor(.125*minsf + .001);
    if (maxsf1 > 0)  sbg[i]  = Max(sbg[i],maxsf1/8 + (maxsf1 % 8 != 0));
    if (sbg[i] > 7) sbg[i]=7;


    for ( sfb = 0; sfb < SBPSY_s; sfb++ ) {
      sf[sfb][i] += 8*sbg[i];

      if (sf[sfb][i] < 0) {
	maxrange = sfb < 6 ? maxrange1 : maxrange2;
	scalefac[sfb][i]=-sf[sfb][i]/ifqstep + (-sf[sfb][i]%ifqstep != 0);
	if (scalefac[sfb][i]>maxrange) scalefac[sfb][i]=maxrange;
	
	if (-(sf[sfb][i] + scalefac[sfb][i]*ifqstep) >maxover)  {
	  maxover=-(sf[sfb][i] + scalefac[sfb][i]*ifqstep);
	}
      }
    }
  }

  return maxover;
}





int max_range_short[SBPSY_s]=
{15, 15, 15, 15, 15, 15 ,  7,    7,    7,    7,   7,     7 };
int max_range_long[SBPSY_l]=
{15,   15,  15,  15,  15,  15,  15,  15,  15,  15,  15,   7,   7,   7,   7,   7,   7,   7,    7,    7,    7};


/*
	  ifqstep = ( cod_info->scalefac_scale == 0 ) ? 2 : 4;
	  ol_sf =  (cod_info->global_gain-210.0);
	  ol_sf -= ifqstep*scalefac[gr][ch].l[sfb];
	  if (cod_info->preflag && sfb>=11) 
	  ol_sf -= ifqstep*pretab[sfb];
*/
int compute_scalefacs_long(int sf[SBPSY_l],gr_info *cod_info,int scalefac[SBPSY_l])
{
  int sfb;
  int maxover;
  int ifqstep = ( cod_info->scalefac_scale == 0 ) ? 2 : 4;
  

  if (cod_info->preflag)
    for ( sfb = 11; sfb < SBPSY_l; sfb++ ) 
      sf[sfb] += pretab[sfb]*ifqstep;


  maxover=0;
  for ( sfb = 0; sfb < SBPSY_l; sfb++ ) {

    if (sf[sfb]<0) {
      /* ifqstep*scalefac >= -sf[sfb], so round UP */
      scalefac[sfb]=-sf[sfb]/ifqstep  + (-sf[sfb] % ifqstep != 0);
      if (scalefac[sfb] > max_range_long[sfb]) scalefac[sfb]=max_range_long[sfb];
      
      /* sf[sfb] should now be positive: */
      if (  -(sf[sfb] + scalefac[sfb]*ifqstep)  > maxover) {
	maxover = -(sf[sfb] + scalefac[sfb]*ifqstep);
      }
    }
  }

  return maxover;
}
  
  






/************************************************************************
 *
 * quantize and encode with the given scalefacs and global gain
 *
 * compute scalefactors, l3_enc, and return number of bits needed to encode
 *
 *
 ************************************************************************/
void
VBR_quantize_granule(lame_global_flags *gfp,
                FLOAT8 xr34[576], int l3_enc[576],
		     III_psy_ratio *ratio,  
                III_scalefac_t *scalefac, int gr, int ch)
{
  lame_internal_flags *gfc=gfp->internal_flags;
  int status;
  gr_info *cod_info;  
  III_side_info_t * l3_side;
  l3_side = &gfc->l3_side;
  cod_info = &l3_side->gr[gr].ch[ch].tt;


  /* encode scalefacs */
  if ( gfp->version == 1 ) 
    status=scale_bitcount(scalefac, cod_info);
  else
    status=scale_bitcount_lsf(scalefac, cod_info);

  if (status!=0) {
    return;
  }
  
  /* quantize xr34 */
  cod_info->part2_3_length = count_bits(gfp,l3_enc,xr34,cod_info);
  if (cod_info->part2_3_length >= LARGE_BITS) return;
  cod_info->part2_3_length += cod_info->part2_length;


  if (gfc->use_best_huffman==1) {
    best_huffman_divide(gfc, gr, ch, cod_info, l3_enc);
  }
  return;
}
  

















/************************************************************************
 *
 * VBR_noise_shaping()
 *
 * compute scalefactors, l3_enc, and return number of bits needed to encode
 *
 * return code:    0   scalefactors were found with all noise < masking
 *
 *               n>0   scalefactors required too many bits.  global gain
 *                     was decreased by n
 *                     If n is large, we should probably recompute scalefacs
 *                     with a lower quality.
 *
 *               n<0   scalefactors used less than minbits.
 *                     global gain was increased by n.  
 *                     If n is large, might want to recompute scalefacs
 *                     with a higher quality setting?
 *
 ************************************************************************/
int
VBR_noise_shaping
(
 lame_global_flags *gfp,
 FLOAT8 xr[576], FLOAT8 xr34orig[576], III_psy_ratio *ratio,
 int l3_enc[576], int digital_silence, int minbits, int maxbits,
 III_scalefac_t *scalefac, III_psy_xmin *l3_xmin,
 int gr,int ch)
{
  lame_internal_flags *gfc=gfp->internal_flags;
  int       start,end,bw,sfb,l, i,j, vbrmax;
  III_scalefac_t vbrsf;
  III_scalefac_t save_sf;
  int maxover0,maxover1,maxover0p,maxover1p,maxover,mover;
  int ifqstep;
  III_side_info_t * l3_side;
  gr_info *cod_info;  
  FLOAT8 xr34[576];
  int shortblock;
  int global_gain_adjust=0;

  l3_side = &gfc->l3_side;
  cod_info = &l3_side->gr[gr].ch[ch].tt;
  shortblock = (cod_info->block_type == SHORT_TYPE);
  
  memcpy(xr34, xr34orig, sizeof(xr34));

  
#define MAX_SF_DELTA 4  
  vbrmax=-10000;
  if (shortblock) {
    for ( j=0, sfb = 0; sfb < SBMAX_s; sfb++ )  {
      for ( i = 0; i < 3; i++ ) {
	start = gfc->scalefac_band.s[ sfb ];
	end   = gfc->scalefac_band.s[ sfb+1 ];
	bw = end - start;
	vbrsf.s[sfb][i] = find_scalefac(&xr[j],&xr34[j],sfb,
					l3_xmin->s[sfb][i],bw);
	j += bw;
      }
    }

    for ( sfb = 0; sfb < SBMAX_s; sfb++ )  {
      for ( i = 0; i < 3; i++ ) {
	if (sfb>0) 
	  vbrsf.s[sfb][i] = Min(vbrsf.s[sfb-1][i]+MAX_SF_DELTA,vbrsf.s[sfb][i]);
	if (sfb< SBMAX_s-1) 
	  vbrsf.s[sfb][i] = Min(vbrsf.s[sfb+1][i]+MAX_SF_DELTA,vbrsf.s[sfb][i]);
      }
    }

    for ( j=0, sfb = 0; sfb < SBMAX_s; sfb++ )  {
      for ( i = 0; i < 3; i++ ) {
	if (vbrsf.s[sfb][i]>vbrmax) vbrmax=vbrsf.s[sfb][i];
      }
    }

  }else{
    for ( sfb = 0; sfb < SBMAX_l; sfb++ )   {
      start = gfc->scalefac_band.l[ sfb ];
      end   = gfc->scalefac_band.l[ sfb+1 ];
      bw = end - start;
      vbrsf.l[sfb] = find_scalefac(&xr[start],&xr34[start],sfb,
				   l3_xmin->l[sfb],bw);

    }

    for ( sfb = 0; sfb < SBMAX_l; sfb++ )   {
      if (sfb>0) 
	vbrsf.l[sfb] = Min(vbrsf.l[sfb-1]+MAX_SF_DELTA,vbrsf.l[sfb]);
      if (sfb< SBMAX_l-1) 
	vbrsf.l[sfb] = Min(vbrsf.l[sfb+1]+MAX_SF_DELTA,vbrsf.l[sfb]);

    }

    for ( sfb = 0; sfb < SBMAX_l; sfb++ )   
      if (vbrsf.l[sfb]>vbrmax) vbrmax = vbrsf.l[sfb];


  } /* compute needed scalefactors */



  /* save a copy of vbrsf, incase we have to recomptue scalefacs */
  memcpy(&save_sf,&vbrsf,sizeof(III_scalefac_t));


  do { 

  memset(scalefac,0,sizeof(III_scalefac_t));
    
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
	maxover0 = Max(maxover0,(vbrmax - vbrsf.s[sfb][i]) - (4*14 + 2*max_range_short[sfb]) );
	maxover1 = Max(maxover1,(vbrmax - vbrsf.s[sfb][i]) - (4*14 + 4*max_range_short[sfb]) );
      }
    }
    if (gfc->noise_shaping==2)
      /* allow scalefac_scale=1 */
      mover = Min(maxover0,maxover1);
    else
      mover = maxover0; 


    vbrmax -= mover;
    maxover0 -= mover;
    maxover1 -= mover;

    if (maxover0==0) 
      cod_info->scalefac_scale = 0;
    else if (maxover1==0)
      cod_info->scalefac_scale = 1;


    /* sf =  (cod_info->global_gain-210.0) */
    cod_info->global_gain = vbrmax +210;
    assert(cod_info->global_gain < 256);
    if (cod_info->global_gain>255) cod_info->global_gain=255;
    
    for ( sfb = 0; sfb < SBPSY_s; sfb++ ) {
      for ( i = 0; i < 3; i++ ) {
	vbrsf.s[sfb][i]-=vbrmax;
      }
    }
    maxover=compute_scalefacs_short(vbrsf.s,cod_info,scalefac->s,cod_info->subblock_gain);
    assert(maxover <=0);
    {
      /* adjust global_gain so at least 1 subblock gain = 0 */
      int minsfb=999;
      for (i=0; i<3; i++) minsfb = Min(minsfb,cod_info->subblock_gain[i]);
      minsfb = Min(cod_info->global_gain/8,minsfb);
      vbrmax -= 8*minsfb; 
      cod_info->global_gain -= 8*minsfb;
      for (i=0; i<3; i++) cod_info->subblock_gain[i] -= minsfb;
    }
    
    
    
    /* quantize xr34[] based on computed scalefactors */
    ifqstep = ( cod_info->scalefac_scale == 0 ) ? 2 : 4;
    for ( j=0, sfb = 0; sfb < SBPSY_s; sfb++ ) {
      start = gfc->scalefac_band.s[ sfb ];
      end   = gfc->scalefac_band.s[ sfb+1 ];
      for (i=0; i<3; i++) {
	int ifac;
	FLOAT8 fac;
	ifac = (8*cod_info->subblock_gain[i]+ifqstep*scalefac->s[sfb][i]);
	if (ifac+210<Q_MAX) 
	  fac = 1/IPOW20(ifac+210);
	else
	  fac = pow(2.0,.75*ifac/4.0);
	for ( l = start; l < end; l++ ) 
	  xr34[j++]*=fac;
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
      maxover0 = Max(maxover0,(vbrmax - vbrsf.l[sfb]) - 2*max_range_long[sfb] );
      maxover0p = Max(maxover0,(vbrmax - vbrsf.l[sfb]) - 2*(max_range_long[sfb]+pretab[sfb]) );
      maxover1 = Max(maxover1,(vbrmax - vbrsf.l[sfb]) - 4*max_range_long[sfb] );
      maxover1p = Max(maxover1,(vbrmax - vbrsf.l[sfb]) - 4*(max_range_long[sfb]+pretab[sfb]));
    }
    mover = Min(maxover0,maxover0p);
    if (gfc->noise_shaping==2) {
      /* allow scalefac_scale=1 */
      mover = Min(mover,maxover1);
      mover = Min(mover,maxover1p);
    }


    vbrmax -= mover;
    maxover0 -= mover;
    maxover0p -= mover;
    maxover1 -= mover;
    maxover1p -= mover;


    if (maxover0<=0) {
      cod_info->scalefac_scale = 0;
      cod_info->preflag=0;
      vbrmax -= maxover0;
    } else if (maxover0p<=0) {
      cod_info->scalefac_scale = 0;
      cod_info->preflag=1;
      vbrmax -= maxover0p;
    } else if (maxover1==0) {
      cod_info->scalefac_scale = 1;
      cod_info->preflag=0;
    } else if (maxover1p==0) {
      cod_info->scalefac_scale = 1;
      cod_info->preflag=1;
    } else {
      ERRORF("error vbrquantize.c...\n");
      LAME_ERROR_EXIT();
    }

    
    /* sf =  (cod_info->global_gain-210.0) */
    cod_info->global_gain = vbrmax +210;
    assert(cod_info->global_gain < 256);
    if (cod_info->global_gain>255) cod_info->global_gain=255;

    
    for ( sfb = 0; sfb < SBPSY_l; sfb++ )   
      vbrsf.l[sfb] -= vbrmax;
    
    
    maxover=compute_scalefacs_long(vbrsf.l,cod_info,scalefac->l);
    assert(maxover <=0);
    
    
    /* quantize xr34[] based on computed scalefactors */
    ifqstep = ( cod_info->scalefac_scale == 0 ) ? 2 : 4;
    for ( sfb = 0; sfb < SBPSY_l; sfb++ ) {
      int ifac;
      FLOAT8 fac;
      ifac = ifqstep*scalefac->l[sfb];
      if (cod_info->preflag)
	ifac += ifqstep*pretab[sfb];

      if (ifac+210<Q_MAX) 
	fac = 1/IPOW20(ifac+210);
      else
	fac = pow(2.0,.75*ifac/4.0);

      start = gfc->scalefac_band.l[ sfb ];
      end   = gfc->scalefac_band.l[ sfb+1 ];
      for ( l = start; l < end; l++ ) {
    	xr34[l]*=fac;
      }
    }
  } 

  VBR_quantize_granule(gfp,xr34,l3_enc,ratio,scalefac,gr,ch);




  if (cod_info->part2_3_length < minbits) {
    /* decrease global gain, recompute scale factors */
    if (digital_silence) break;  
    //    if (cod_info->part2_3_length-cod_info->part2_length== 0) break;
    if (vbrmax+210 ==0 ) break;
    


    --vbrmax;
    --global_gain_adjust;
    memcpy(&vbrsf,&save_sf,sizeof(III_scalefac_t));
    memcpy(xr34, xr34orig, sizeof(xr34));
  }

  } while ((cod_info->part2_3_length < minbits));



  while (cod_info->part2_3_length > Min(maxbits,4095)) {
    /* increase global gain, keep exisiting scale factors */
    ++cod_info->global_gain;
    if (cod_info->global_gain > 255) 
      ERRORF("%ld impossible to encode this frame! bits=%d\n",
	      gfp->frameNum,cod_info->part2_3_length);
    VBR_quantize_granule(gfp,xr34,l3_enc,ratio,scalefac,gr,ch);

    ++global_gain_adjust;
  }


  return global_gain_adjust;
}




void
VBR_quantize(lame_global_flags *gfp,
                FLOAT8 pe[2][2], FLOAT8 ms_ener_ratio[2],
                FLOAT8 xr[2][2][576], III_psy_ratio ratio[2][2],
                int l3_enc[2][2][576],
                III_scalefac_t scalefac[2][2])
{
  III_psy_xmin l3_xmin[2][2];
  lame_internal_flags *gfc=gfp->internal_flags;
  int minbits,maxbits,max_frame_bits,totbits,gr,ch,i,bits_ok;
  int bitsPerFrame,mean_bits;
  int analog_silence;
  FLOAT8 qadjust;
  III_side_info_t * l3_side;
  gr_info *cod_info;  
  int digital_silence[2][2];
  FLOAT8 masking_lower_db=0;
  FLOAT8 xr34[2][2][576];
  // static const FLOAT8 dbQ[10]={-6.0,-5.0,-4.0,-3.0, -2.0, -1.0, -.25, .5, 1.25, 2.0};
  /* from quantize.c VBR algorithm */
  /*static const FLOAT8 dbQ[10]=
   {-5.5,-4.25,-3.0,-2.50, -1.75, -.75, -.5, -.25, .25, .75};*/
  static const FLOAT8 dbQ[10]=
  {-6.06,-4.4,-2.9,-1.57, -0.4, 0.61, 1.45, 2.13, 2.65, 3.0};
  
  qadjust=0;   /* start with -1 db quality improvement over quantize.c VBR */

  l3_side = &gfc->l3_side;
  gfc->ATH_vbrlower = (4-gfp->VBR_q)*4.0; 
  if (gfc->ATH_vbrlower < 0) gfc->ATH_vbrlower=0;
  iteration_init(gfp,l3_side,l3_enc);


  /* now find out: if the frame can be considered analog silent
   *               if each granule can be considered digital silent
   * and calculate l3_xmin and the fresh xr34 array
   */

  assert( gfp->VBR_q <= 9 );
  assert( gfp->VBR_q >= 0 );
  analog_silence=1;
  for (gr = 0; gr < gfc->mode_gr; gr++) {
    /* copy data to be quantized into xr */
    if (gfc->mode_ext==MPG_MD_MS_LR) {
      ms_convert(xr[gr],xr[gr]);
    }
    for (ch = 0; ch < gfc->stereo; ch++) {
      /* if in the following sections the quality would not be adjusted
       * then we would only have to call calc_xmin once here and
       * could drop subsequently calls (rh 2000/07/17)
       */
      int over_ath;
      cod_info = &l3_side->gr[gr].ch[ch].tt;
      cod_info->part2_3_length=LARGE_BITS;
      
      /* quality setting */
      masking_lower_db = dbQ[gfp->VBR_q];
      if (pe[gr][ch]>750) {
        masking_lower_db -= Min(10,4*(pe[gr][ch]-750.)/750.);
      }
      gfc->masking_lower = pow(10.0,masking_lower_db/10);
      
      /* masking thresholds */
      over_ath = calc_xmin(gfp,xr[gr][ch],ratio[gr]+ch,cod_info,l3_xmin[gr]+ch);
      
      /* if there are bands with more energy than the ATH 
       * then we say the frame is not analog silent */
      if (over_ath) {
        analog_silence = 0;
      }
      
      /* if there is no line with more energy than 1e-20
       * then this granule is considered to be digital silent
       * plus calculation of xr34 */
      digital_silence[gr][ch] = 1;
      for(i=0;i<576;i++) {
        FLOAT8 temp=fabs(xr[gr][ch][i]);
        xr34[gr][ch][i]=sqrt(sqrt(temp)*temp);
        digital_silence[gr][ch] &= temp < 1E-20;
      }
    } /* ch */
  }  /* gr */

  
  /* compute minimum allowed bits from minimum allowed bitrate */
  if (analog_silence) {
    gfc->bitrate_index=1;
  } else {
    gfc->bitrate_index=gfc->VBR_min_bitrate;
  }
  getframebits(gfp,&bitsPerFrame, &mean_bits);
  minbits = (mean_bits/gfc->stereo);

  /* compute maximum allowed bits from max allowed bitrate */
  gfc->bitrate_index=gfc->VBR_max_bitrate;
  getframebits(gfp,&bitsPerFrame, &mean_bits);
  max_frame_bits = ResvFrameBegin(gfp,l3_side, mean_bits, bitsPerFrame);
  maxbits=2.5*(mean_bits/gfc->stereo);

  {
  /* compute a target  mean_bits based on compression ratio 
   * which was set based on VBR_q  
   */
  int bit_rate = gfp->out_samplerate*16*gfc->stereo/(1000.0*gfp->compression_ratio);
  bitsPerFrame = (bit_rate*gfp->framesize*1000)/gfp->out_samplerate;
  mean_bits = (bitsPerFrame - 8*gfc->sideinfo_len) / gfc->mode_gr;
  }


  minbits = Max(minbits,125);
  minbits=Max(minbits,.40*(mean_bits/gfc->stereo));
  maxbits=Min(maxbits,2.5*(mean_bits/gfc->stereo));







  /* 
   * loop over all ch,gr, encoding anything with bits > .5*(max_frame_bits/4)
   *
   * If a particular granule uses way too many bits, it will be re-encoded
   * on the next iteration of the loop (with a lower quality setting).  
   * But granules which dont use
   * use too many bits will not be re-encoded.
   *
   * minbits:  minimum allowed bits for 1 granule 1 channel
   * maxbits:  maximum allowwed bits for 1 granule 1 channel
   * max_frame_bits:  maximum allowed bits for entire frame
   * (max_frame_bits/4)   estimate of average bits per granule per channel
   * 
   */

  do {
  
    totbits=0;
    for (gr = 0; gr < gfc->mode_gr; gr++) {
      int minbits_lr[2];
      minbits_lr[0]=minbits;
      minbits_lr[1]=minbits;

#if 0
      if (gfc->mode_ext==MPG_MD_MS_LR) {
        FLOAT8 fac;
        fac = .33*(.5-ms_ener_ratio[gr])/.5;
        if (fac<0) fac=0;
        if (fac>.5) fac=.5;
        minbits_lr[0] = (1+fac)*minbits;
        minbits_lr[1] = Max(125,(1-fac)*minbits);
      }
#endif


      for (ch = 0; ch < gfc->stereo; ch++) { 
        int adjusted,shortblock;
        cod_info = &l3_side->gr[gr].ch[ch].tt;
        
        /* ENCODE this data first pass, and on future passes unless it uses
         * a very small percentage of the max_frame_bits  */
        if (cod_info->part2_3_length > (max_frame_bits/(2*gfc->stereo*gfc->mode_gr))) {
  
          shortblock = (cod_info->block_type == SHORT_TYPE);
  
          /* Adjust allowed masking based on quality setting */
          if (qadjust!=0 /*|| shortblock*/) {
            masking_lower_db = dbQ[gfp->VBR_q] + qadjust;

            /*
            if (shortblock) masking_lower_db -= 4;
            */
     
            if (pe[gr][ch]>750)
              masking_lower_db -= Min(10,4*(pe[gr][ch]-750.)/750.);
            gfc->masking_lower = pow(10.0,masking_lower_db/10);
            calc_xmin( gfp,xr[gr][ch], ratio[gr]+ch, cod_info, l3_xmin[gr]+ch);
          }
          
          /* digital silent granules do not need the full round trip,
           * but this can be optimized later on
           */
          adjusted = VBR_noise_shaping (gfp,xr[gr][ch],xr34[gr][ch],
                                        ratio[gr]+ch,l3_enc[gr][ch],
                                        digital_silence[gr][ch],
                                        minbits_lr[ch],
                                        maxbits,scalefac[gr]+ch,
                                        l3_xmin[gr]+ch,gr,ch);
          if (adjusted>10) {
            /* global_gain was changed by a large amount to get bits < maxbits */
            /* quality is set to high.  we could set bits = LARGE_BITS
             * to force re-encoding.  But most likely the other channels/granules
             * will also use too many bits, and the entire frame will
             * be > max_frame_bits, forcing re-encoding below.
             */
            // cod_info->part2_3_bits = LARGE_BITS;
          }
        }
        totbits += cod_info->part2_3_length;
      }
    }
    bits_ok=1;
    if (totbits>max_frame_bits) {
      /* lower quality */
      qadjust += Max(.125,Min(1,(totbits-max_frame_bits)/300.0));
      /* adjusting minbits and maxbits is necessary too
       * cos lowering quality is not enough in rare cases
       * when each granule still needs almost maxbits, it wont fit */ 
      minbits = Max(125,minbits*0.975);
      maxbits = Max(minbits,maxbits*0.975);
      //      DEBUGF("%i totbits>max_frame_bits   totbits=%i  maxbits=%i \n",gfp->frameNum,totbits,max_frame_bits);
      //      DEBUGF("next masking_lower_db = %f \n",masking_lower_db + qadjust);
      bits_ok=0;
    }
    
  } while (!bits_ok);
  

  /* find optimal scalefac storage.  Cant be done above because
   * might enable scfsi which breaks the interation loops */
  totbits=0;
  for (gr = 0; gr < gfc->mode_gr; gr++) {
    for (ch = 0; ch < gfc->stereo; ch++) {
      best_scalefac_store(gfp,gr, ch, l3_enc, l3_side, scalefac);
      totbits += l3_side->gr[gr].ch[ch].tt.part2_3_length;
    }
  }



  
  if (gfp->gtkflag) {
    for (gr = 0; gr < gfc->mode_gr; gr++) {
      for (ch = 0; ch < gfc->stereo; ch++) {
        III_psy_xmin l3_xmin;
        calc_noise_result noise_info;
        FLOAT8 noise[4];
        FLOAT8 xfsf[4][SBMAX_l];
        FLOAT8 distort[4][SBMAX_l];

        cod_info = &l3_side->gr[gr].ch[ch].tt;

        /* recompute allowed noise with no 'masking_lower' for
         * frame analyzer */
        gfc->masking_lower=1.0;
        cod_info = &l3_side->gr[gr].ch[ch].tt;
        calc_xmin( gfp,xr[gr][ch], &ratio[gr][ch], cod_info, &l3_xmin);

        calc_noise( gfp, xr[gr][ch], l3_enc[gr][ch], cod_info, 
                    xfsf,distort, &l3_xmin, &scalefac[gr][ch], &noise_info);
        noise[0] = noise_info.over_count;
        noise[1] = noise_info.max_noise;
        noise[2] = noise_info.over_noise;
        noise[3] = noise_info.tot_noise;

        set_pinfo (gfp, cod_info, &ratio[gr][ch], &scalefac[gr][ch], xr[gr][ch], xfsf, noise, gr, ch);
      }
    }
  }


  

  if (analog_silence && !gfp->VBR_hard_min) {
    gfc->bitrate_index = 1;
  } else {
    gfc->bitrate_index = gfc->VBR_min_bitrate;
  }
  for( ; gfc->bitrate_index < gfc->VBR_max_bitrate; gfc->bitrate_index++ ) {

    getframebits (gfp,&bitsPerFrame, &mean_bits);
    maxbits = ResvFrameBegin(gfp,l3_side, mean_bits, bitsPerFrame);
    if (totbits <= maxbits) break;
  }
  if (gfc->bitrate_index == gfc->VBR_max_bitrate) {
    getframebits (gfp,&bitsPerFrame, &mean_bits);
    maxbits = ResvFrameBegin(gfp,l3_side, mean_bits, bitsPerFrame);
  }

  //  DEBUGF("%i total_bits=%i max_frame_bits=%i index=%i  \n",gfp->frameNum,totbits,max_frame_bits,gfc->bitrate_index);

  for (gr = 0; gr < gfc->mode_gr; gr++) {
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
  }
  ResvFrameEnd (gfp,l3_side, mean_bits);



}



