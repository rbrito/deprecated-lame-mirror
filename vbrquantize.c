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
#include "tables.h"
#include "reservoir.h"
#include "quantize-pvt.h"
#ifdef HAVEGTK
#include "gtkanal.h"
#endif





FLOAT8 calc_sfb_ave_noise(FLOAT8 *xr, FLOAT8 *xr34, int stride, int bw, FLOAT8 sfpow)
{
  int j;
  FLOAT8 xfsf=0;
  FLOAT8 sfpow34 = pow(sfpow,3.0/4.0);
#define PRECALC_SIZE 8206 /* 8191+15. should never be outside this. see count_bits() */
  static FLOAT8 pow43[PRECALC_SIZE];
  static int init=0;

  if (init==0) {
    init++;
    for(j=0;j<PRECALC_SIZE;j++)
      pow43[j] = pow((FLOAT8)j, 4.0/3.0);
  }

  for ( j=0; j < stride*bw ; j += stride) {
    int ix;
    FLOAT8 temp;
    if (xr34[j] >= sfpow34*(PRECALC_SIZE  - .4054)) {
      ix=(int)( xr34[j]/sfpow34  + 0.4054);
      assert(ix >= PRECALC_SIZE);
      return -1.0;
    }else{ 
      ix=(int)( xr34[j]/sfpow34  + 0.4054);
    }
    assert(ix < PRECALC_SIZE);
    temp = fabs(xr[j])- pow43[ix]*sfpow;
    //    xfsf += temp * temp;
    temp *= temp;
    xfsf = Max(xfsf,temp);
  }
  return xfsf;
}
FLOAT8 calc_sfb_max_noise(FLOAT8 *xr, FLOAT8 *xr34, int stride, int bw, FLOAT8 sfpow)
{
  int j;
  FLOAT8 xfsf=0;
  FLOAT8 sfpow34 = pow(sfpow,3.0/4.0);
#define PRECALC_SIZE 8206 /* 8191+15. should never be outside this. see count_bits() */
  static FLOAT8 pow43[PRECALC_SIZE];
  static int init=0;

  if (init==0) {
    init++;
    for(j=0;j<PRECALC_SIZE;j++)
      pow43[j] = pow((FLOAT8)j, 4.0/3.0);
  }

  for ( j=0; j < stride*bw ; j += stride) {
    int ix;
    FLOAT8 temp;
    if (xr34[j] >= sfpow34*(PRECALC_SIZE  - .4054)) {
      ix=(int)( xr34[j]/sfpow34  + 0.4054);
      assert(ix >= PRECALC_SIZE);
      return -1.0;
    }else{ 
      ix=(int)( xr34[j]/sfpow34  + 0.4054);
    }
    assert(ix < PRECALC_SIZE);
    temp = fabs(xr[j])- pow43[ix]*sfpow;
    //    xfsf += temp * temp;
    xfsf = Max(xfsf,temp);
  }
  xfsf = xfsf / bw;
  return xfsf;
}






FLOAT8 find_scalefac(FLOAT8 *xr,FLOAT8 *xr34,int stride,int sfb,FLOAT8 l3_xmin,int bw)
{
  FLOAT8 xfsf,sfpow,sf,sf_ok,delsf;
  int i;

  /* search will range from -52.25 -> 11.25  */
  /* 31.75 */
  sf = -20.5;
  delsf = 16;

  sf_ok=1000; 
  for (i=0; i<6; i++) {  /* find  sf +/- 1 */
    //        sf=-17  + .25*i;
    sfpow = pow(2.0,sf);
    xfsf = calc_sfb_max_noise(xr,xr34,stride,bw,sfpow);

    if (xfsf < 0) {
      /* scalefactors too small */
      sf += delsf; 
    }else{

      if (stride==1 && sfb==2 && frameNum==50) {
	printf("sfb=%i q=%f  xfsf=%f   masking=%f  sf_ok=%f \n",sfb,sf,
	       10*log10(1e-20+xfsf),10*log10(1e-20+l3_xmin),sf_ok);
      }

      if (sf_ok==1000) sf_ok=sf;  
      if (xfsf > l3_xmin)  {
	/* distortion.  try a smaller scalefactor */
	sf -= delsf;
      }else{
	sf_ok=sf;
	sf += delsf;
      }
    }
    delsf /= 2;
  } 
  assert(sf_ok!=1000);

  /* NOTE: noise is not a monotone function of the sf, even though
   * the number of bits used is!  do a brute force search in the 
   * neighborhood of sf_ok: */
  sf = sf_ok + 1.5;
  do { 
    sfpow = pow(2.0,sf);
    xfsf = calc_sfb_max_noise(xr,xr34,stride,bw,sfpow);
    if (xfsf > 0) {

      if (stride==1 && sfb==2 && frameNum==50) {
	printf("bruteforce loop: sfb=%i q=%f  xfsf=%f   masking=%f   \n",sfb,sf,
	       10*log10(1e-20+xfsf),10*log10(1e-20+l3_xmin));
      }
      
      if (xfsf <= l3_xmin) return sf;
      sf -= .25;
    }
  } while (xfsf>0);
  /* we hit a scalefactor that is too small, use last value: */
  return sf+.25;
}



/************************************************************************
 *
 * VBR_iteration_loop()   
 *
 *
 ************************************************************************/
void
VBR_iteration_loop_new (FLOAT8 pe[2][2], FLOAT8 ms_ener_ratio[2],
                FLOAT8 xr_org[2][2][576], III_psy_ratio *ratio,
                III_side_info_t * l3_side, int l3_enc[2][2][576],
                III_scalefac_t * scalefac, frame_params * fr_ps)
{
  III_psy_xmin l3_xmin;
  layer    *info;
  FLOAT8    xr[2][2][576];
  //  FLOAT8    masking_lower_db;
  FLOAT8    ifqstep,ol_sf,vbr_sf;
  int       stereo = fr_ps->stereo;
  int       start,end,bw,sfb, i,ch, gr, mode_gr, over;

  info = fr_ps->header;
  mode_gr = (info->version == 1) ? 2 : 1;

  convert_mdct=0;
  reduce_sidechannel=0;
  if (info->mode_ext==MPG_MD_MS_LR) {
      convert_mdct = 1;
      reduce_sidechannel=1;
  }
  /* dont bother with scfsi. */
  for ( ch = 0; ch < stereo; ch++ )
    for ( i = 0; i < 4; i++ )
      l3_side->scfsi[ch][i] = 0;

#if 0
  l3_side->resvDrain = 0;
  if ( frameNum==0 ) {
    scalefac_band_long  = &sfBandIndex[info->sampling_frequency + (info->version * 3)].l[0];
    scalefac_band_short = &sfBandIndex[info->sampling_frequency + (info->version * 3)].s[0];
    l3_side->main_data_begin = 0;
    memset((char *) &l3_xmin, 0, sizeof(l3_xmin));
    compute_ath(info,ATH_l,ATH_s);
  }



  /* Adjust allowed masking based on quality setting */
  /* db_lower varies from -10 to +8 db */
  masking_lower_db = -10 + 2*VBR_q;
  /* adjust by -6(min)..0(max) depending on bitrate */
  masking_lower = pow(10.0,masking_lower_db/10);
#endif
  masking_lower = 1;


  /* copy data to be quantized into xr */
  for(gr = 0; gr < mode_gr; gr++) {
    if (convert_mdct) ms_convert(xr[gr],xr_org[gr]);
    else memcpy(xr[gr],xr_org[gr],sizeof(FLOAT8)*2*576);   
  }

  for (gr = 0; gr < mode_gr; gr++) {
    for (ch = 0; ch < stereo; ch++) { 
      FLOAT8 xr34[576];
      gr_info *cod_info = &l3_side->gr[gr].ch[ch].tt;
      int shortblock;
      over = 0;
      shortblock = (cod_info->block_type == SHORT_TYPE);
      ifqstep = ( cod_info->scalefac_scale == 0 ) ? .5 : 1.0;

      for(i=0;i<576;i++) 	    {
	FLOAT8 temp=fabs(xr[gr][ch][i]);
	xr34[i]=sqrt(sqrt(temp)*temp);
      }

      /*init_outer_loop(xr,&l3_xmin,scalefac,gr,stereo,l3_side,ratio,ch);  */
      calc_xmin( xr, ratio, cod_info, &l3_xmin, gr, ch );

      if (shortblock) {
	for ( sfb = 0; sfb < SBPSY_s; sfb++ )  {
	  for ( i = 0; i < 3; i++ ) {
	    start = scalefac_band_short[ sfb ];
	    end   = scalefac_band_short[ sfb+1 ];
	    bw = end - start;
	    vbr_sf = find_scalefac(&xr[gr][ch][3*start+i],&xr34[3*start+i],3,sfb,
		   masking_lower*l3_xmin.s[gr][ch][sfb][i],bw);

            ol_sf =  (cod_info->global_gain-210.0)/4.0;
	    ol_sf -= 2*cod_info->subblock_gain[i];
	    ol_sf -= ifqstep*scalefac->s[gr][ch][sfb][i];

	    { 
	      FLOAT8 xfsf;
	      xfsf = calc_sfb_ave_noise(&xr[gr][ch][3*start+i],&xr34[3*start+i],
				    3,bw,pow(2.0,ol_sf));
	      if (xfsf > masking_lower*l3_xmin.s[gr][ch][sfb][i]) {
		over++;
		//		printf("xfsf, l3_xmin  %e %e \n",xfsf,l3_xmin.s[gr][ch][sfb][i]);
		//	      printf("short %i %i %i sfb=%i  %f %f  \n",frameNum,gr,ch,sfb,
		//		     ol_sf,vbr_sf);
	      }
	    }
	  }
	}
      }else{
	for ( sfb = 0; sfb < SBPSY_l; sfb++ )   {
	  start = scalefac_band_long[ sfb ];
	  end   = scalefac_band_long[ sfb+1 ];
	  bw = end - start;
	  vbr_sf = find_scalefac(&xr[gr][ch][start],&xr34[start],1,sfb,
				 masking_lower*l3_xmin.l[gr][ch][sfb],bw);
	  
	  /* compare with outer_loop scalefacs */
	  ol_sf =  (cod_info->global_gain-210.0)/4.0;
	  ol_sf -= ifqstep*scalefac->l[gr][ch][sfb];
	  if (cod_info->preflag && sfb>=11) 
	    ol_sf -= ifqstep*pretab[sfb];

	  { FLOAT8 xfsf;
          xfsf=calc_sfb_ave_noise(&xr[gr][ch][start],&xr34[start],1,bw,pow(2.0,ol_sf));
	  if (xfsf > masking_lower*l3_xmin.l[gr][ch][sfb]) {
	    over++;
	    //	    printf("xfsf, l3_xmin  %e %e \n",xfsf,l3_xmin.l[gr][ch][sfb]);
	    //	    printf("long %i %i %i sfb=%i old_sfb=%f vbr_sfb=%f \n",
	    //   frameNum,gr,ch,sfb,ol_sf,vbr_sf);
	  }
	  }

	  
	}
      } /* compute scalefactors */
      //    printf("%i %i %i new vbr over=%i  \n",frameNum,gr,ch,over);
    } /* ch */
  } /* gr */
}



