/*
 *	psymodel.c
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

#include "util.h"
#include "encoder.h"
#include "psymodel.h"
#include "l3side.h"
#include <assert.h>
#include "tables.h"
#include "fft.h"

#ifdef M_LN10
#define		LN_TO_LOG10		(M_LN10/10)
#else
#define         LN_TO_LOG10             0.2302585093
#endif


void L3para_read( FLOAT8 sfreq, int numlines[CBANDS],int numlines_s[CBANDS], int partition_l[HBLKSIZE],
		  FLOAT8 minval[CBANDS], 
		  FLOAT8 s3_l[CBANDS + 1][CBANDS + 1],
		  FLOAT8 s3_s[CBANDS + 1][CBANDS + 1],
		  FLOAT8 SNR_s[CBANDS],
		  int bu_l[SBPSY_l], int bo_l[SBPSY_l],
		  FLOAT8 w1_l[SBPSY_l], FLOAT8 w2_l[SBPSY_l],
		  int bu_s[SBPSY_s], int bo_s[SBPSY_s],
		  FLOAT8 w1_s[SBPSY_s], FLOAT8 w2_s[SBPSY_s] );
									


void L3psycho_anal( lame_global_flags *gfp,
                    short int *buffer[2],int gr_out , 
                    FLOAT8 *ms_ratio,
                    FLOAT8 *ms_ratio_next,
		    FLOAT8 *ms_ener_ratio,
		    III_psy_ratio masking_ratio[2][2],
		    III_psy_ratio masking_MS_ratio[2][2],
		    FLOAT8 percep_entropy[2],FLOAT8 percep_MS_entropy[2], 
                    int blocktype_d[2])
{
  lame_internal_flags *gfc=gfp->internal_flags;

/* to get a good cache performance, one has to think about
 * the sequence, in which the variables are used.  
 * (Note: these static variables have been moved to the gfc-> struct,
 * and their order in memory is layed out in util.h)
 */
  

  /* fft and energy calculation   */
  FLOAT (*wsamp_l)[BLKSIZE];
  FLOAT (*wsamp_s)[3][BLKSIZE_s];
  FLOAT tot_ener[4];

  /* convolution   */
  FLOAT8 eb[CBANDS];
  FLOAT8 cb[CBANDS];
  FLOAT8 thr[CBANDS];
  
  /* ratios    */
  FLOAT8 ms_ratio_l=0,ms_ratio_s=0;

  /* block type  */
  int blocktype[2],uselongblock[2];
  
  /* usual variables like loop indices, etc..    */
  int numchn, chn, samplerate;
  int   b, i, j, k;
  int	sb,sblock;
  FLOAT cwlimit;

  /*  use a simplified spreading function: */
    /*#define NEWS3  */ 
#if 1
    /* AAC values, results in more masking over MP3 values */
# define TMN 18
# define NMT 6
#else
    /* MP3 values */
# define TMN 29
# define NMT 6
#endif



  if(gfc->psymodel_init==0) {
    FLOAT8	SNR_s[CBANDS];
    int	partition_l[HBLKSIZE];
    gfc->psymodel_init=1;


    samplerate = gfp->out_samplerate;
    switch(gfp->out_samplerate){
    case 32000: break;
    case 44100: break;
    case 48000: break;
    case 16000: break;
    case 22050: break;
    case 24000: break;
    case  8000: samplerate *= 2; break;  /* kludge so mpeg2.5 uses mpeg2 tables  for now */
    case 11025: samplerate *= 2; break;
    case 12000: samplerate *= 2; break;
    default:    fprintf(stderr,"error, invalid sampling frequency: %d Hz\n",
            gfp->out_samplerate);
      exit(-1);
    }

    gfc->ms_ener_ratio_old=.25;
    gfc->blocktype_old[0]=STOP_TYPE;
    gfc->blocktype_old[1]=STOP_TYPE;
    gfc->blocktype_old[0]=SHORT_TYPE;
    gfc->blocktype_old[1]=SHORT_TYPE;

    for (i=0; i<4; ++i) {
      for (j=0; j<CBANDS; ++j) {
	gfc->nb_1[i][j]=1e20;
	gfc->nb_2[i][j]=1e20;
      }
      for ( sb = 0; sb < SBPSY_l; sb++ ) {
	gfc->en[i].l[sb] = 1e20;
	gfc->thm[i].l[sb] = 1e20;
      }
      for (j=0; j<3; ++j) {
	for ( sb = 0; sb < SBPSY_s; sb++ ) {
	  gfc->en[i].s[sb][j] = 1e20;
	  gfc->thm[i].s[sb][j] = 1e20;
	}
      }
    }



    
    /*  gfp->cwlimit = sfreq*j/1024.0;  */
    gfc->cw_lower_index=6;
    if (gfp->cwlimit>0) 
      cwlimit=gfp->cwlimit;
    else
      cwlimit=8.8717;
    gfc->cw_upper_index = cwlimit*1000.0*1024.0/((FLOAT8)samplerate);
    gfc->cw_upper_index=Min(HBLKSIZE-4,gfc->cw_upper_index);      /* j+3 < HBLKSIZE-1 */
    gfc->cw_upper_index=Max(6,gfc->cw_upper_index);

    for ( j = 0; j < HBLKSIZE; j++ )
      gfc->cw[j] = 0.4;
    
    /* setup stereo demasking thresholds */
    /* formula reverse enginerred from plot in paper */
    for ( sb = 0; sb < SBPSY_s; sb++ ) {
      FLOAT8 mld = 1.25*(1-cos(PI*sb/SBPSY_s))-2.5;
      gfc->mld_s[sb] = pow(10.0,mld);
    }
    for ( sb = 0; sb < SBPSY_l; sb++ ) {
      FLOAT8 mld = 1.25*(1-cos(PI*sb/SBPSY_l))-2.5;
      gfc->mld_l[sb] = pow(10.0,mld);
    }
    
    for (i=0;i<HBLKSIZE;i++) partition_l[i]=-1;

    L3para_read( (FLOAT8) samplerate,gfc->numlines_l,gfc->numlines_s,
          partition_l,gfc->minval,gfc->s3_l,gfc->s3_s,SNR_s,gfc->bu_l,
          gfc->bo_l,gfc->w1_l,gfc->w2_l, gfc->bu_s,gfc->bo_s,
          gfc->w1_s,gfc->w2_s );
    
    
    /* npart_l_orig   = number of partition bands before convolution */
    /* npart_l  = number of partition bands after convolution */
    gfc->npart_l_orig=0; gfc->npart_s_orig=0;
    for (i=0;i<HBLKSIZE;i++) 
      if (partition_l[i]>gfc->npart_l_orig) gfc->npart_l_orig=partition_l[i];
    gfc->npart_l_orig++;

    for (i=0;gfc->numlines_s[i]>=0;i++)
      ;
    gfc->npart_s_orig = i;
    
    gfc->npart_l=gfc->bo_l[SBPSY_l-1]+1;
    gfc->npart_s=gfc->bo_s[SBPSY_s-1]+1;

    /* MPEG2 tables are screwed up 
     * the mapping from paritition bands to scalefactor bands will use
     * more paritition bands than we have.  
     * So we will not compute these fictitious partition bands by reducing
     * npart_l below.  */
    if (gfc->npart_l > gfc->npart_l_orig) {
      gfc->npart_l=gfc->npart_l_orig;
      gfc->bo_l[SBPSY_l-1]=gfc->npart_l-1;
      gfc->w2_l[SBPSY_l-1]=1.0;
    }
    if (gfc->npart_s > gfc->npart_s_orig) {
      gfc->npart_s=gfc->npart_s_orig;
      gfc->bo_s[SBPSY_s-1]=gfc->npart_s-1;
      gfc->w2_s[SBPSY_s-1]=1.0;
    }
    
    
    
    for (i=0; i<gfc->npart_l; i++) {
      for (j = 0; j < gfc->npart_l_orig; j++) {
	if (gfc->s3_l[i][j] != 0.0)
	  break;
      }
      gfc->s3ind[i][0] = j;
      
      for (j = gfc->npart_l_orig - 1; j > 0; j--) {
	if (gfc->s3_l[i][j] != 0.0)
	  break;
      }
      gfc->s3ind[i][1] = j;
    }


    for (i=0; i<gfc->npart_s; i++) {
      for (j = 0; j < gfc->npart_s_orig; j++) {
	if (gfc->s3_s[i][j] != 0.0)
	  break;
      }
      gfc->s3ind_s[i][0] = j;
      
      for (j = gfc->npart_s_orig - 1; j > 0; j--) {
	if (gfc->s3_s[i][j] != 0.0)
	  break;
      }
      gfc->s3ind_s[i][1] = j;
    }
    
    
    /*  
      #include "debugscalefac.c"
    */
    



#define rpelev 2
#define rpelev2 16

    /* compute norm_l, norm_s instead of relying on table data */
    for ( b = 0;b < gfc->npart_l; b++ ) {
      FLOAT8 norm=0;
      for ( k = gfc->s3ind[b][0]; k <= gfc->s3ind[b][1]; k++ ) {
	norm += gfc->s3_l[b][k];
      }
      for ( k = gfc->s3ind[b][0]; k <= gfc->s3ind[b][1]; k++ ) {
	gfc->s3_l[b][k] *= exp(-LN_TO_LOG10 * NMT) / norm;
      }
      /*printf("%i  norm=%f  norm_l=%f \n",b,1/norm,norm_l[b]);*/
    }

    /* MPEG1 SNR_s data is given in db, convert to energy */
    if (gfp->version == 1) {
      for ( b = 0;b < gfc->npart_s; b++ ) {
	SNR_s[b]=exp( (FLOAT8) SNR_s[b] * LN_TO_LOG10 );
      }
    }

    for ( b = 0;b < gfc->npart_s; b++ ) {
      FLOAT8 norm=0;
      for ( k = gfc->s3ind_s[b][0]; k <= gfc->s3ind_s[b][1]; k++ ) {
	norm += gfc->s3_s[b][k];
      }
      for ( k = gfc->s3ind_s[b][0]; k <= gfc->s3ind_s[b][1]; k++ ) {
	gfc->s3_s[b][k] *= SNR_s[b] / norm;
      }
      /*printf("%i  norm=%f  norm_s=%f \n",b,1/norm,norm_l[b]);*/
    }
    
    init_fft();
  }
  /************************* End of Initialization *****************************/
  


  
  
  numchn = gfc->stereo;
  /* chn=2 and 3 = Mid and Side channels */
  if (gfp->mode == MPG_MD_JOINT_STEREO) numchn=4;
  for (chn=0; chn<numchn; chn++) {
  
    wsamp_s = gfc->wsamp_S+(chn & 1);
    wsamp_l = gfc->wsamp_L+(chn & 1);


    if (chn<2) {    
      /**********************************************************************
       *  compute FFTs
       **********************************************************************/
      fft_long ( *wsamp_l, chn, buffer);
      fft_short( *wsamp_s, chn, buffer); 
      
      /* LR maskings  */
      percep_entropy[chn] = gfc->pe[chn]; 
      masking_ratio[gr_out][chn].thm = gfc->thm[chn];
      masking_ratio[gr_out][chn].en = gfc->en[chn];
    }else{
      /* MS maskings  */
      percep_MS_entropy[chn-2] = gfc->pe[chn]; 
      masking_MS_ratio[gr_out][chn-2].en = gfc->en[chn];
      masking_MS_ratio[gr_out][chn-2].thm = gfc->thm[chn];
      
      if (chn == 2)
      {
        for (j = BLKSIZE-1; j >=0 ; --j)
        {
          FLOAT l = gfc->wsamp_L[0][j];
          FLOAT r = gfc->wsamp_L[1][j];
          gfc->wsamp_L[0][j] = (l+r)*(FLOAT)(SQRT2*0.5);
          gfc->wsamp_L[1][j] = (l-r)*(FLOAT)(SQRT2*0.5);
        }
        for (b = 2; b >= 0; --b)
        {
          for (j = BLKSIZE_s-1; j >= 0 ; --j)
          {
            FLOAT l = gfc->wsamp_S[0][b][j];
            FLOAT r = gfc->wsamp_S[1][b][j];
            gfc->wsamp_S[0][b][j] = (l+r)*(FLOAT)(SQRT2*0.5);
            gfc->wsamp_S[1][b][j] = (l-r)*(FLOAT)(SQRT2*0.5);
          }
        }
      }
    }

    /**********************************************************************
     *  compute energies
     **********************************************************************/
    
    
    
    gfc->energy[0]  = (*wsamp_l)[0];
    gfc->energy[0] *= gfc->energy[0];
    
    tot_ener[chn] = gfc->energy[0]; /* sum total energy at nearly no extra cost */
    
    for (j=BLKSIZE/2-1; j >= 0; --j)
    {
      FLOAT re = (*wsamp_l)[BLKSIZE/2-j];
      FLOAT im = (*wsamp_l)[BLKSIZE/2+j];
      gfc->energy[BLKSIZE/2-j] = (re * re + im * im) * (FLOAT)0.5;

      if (BLKSIZE/2-j > 10)
	tot_ener[chn] += gfc->energy[BLKSIZE/2-j];
    }
    for (b = 2; b >= 0; --b)
    {
      gfc->energy_s[b][0]  = (*wsamp_s)[b][0];
      gfc->energy_s[b][0] *=  gfc->energy_s [b][0];
      for (j=BLKSIZE_s/2-1; j >= 0; --j)
      {
        FLOAT re = (*wsamp_s)[b][BLKSIZE_s/2-j];
        FLOAT im = (*wsamp_s)[b][BLKSIZE_s/2+j];
        gfc->energy_s[b][BLKSIZE_s/2-j] = (re * re + im * im) * (FLOAT)0.5;
      }
    }


  if(gfp->gtkflag) {
    for (j=0; j<HBLKSIZE ; j++) {
      gfc->pinfo->energy[gr_out][chn][j]=gfc->energy_save[chn][j];
      gfc->energy_save[chn][j]=gfc->energy[j];
    }
  }
    
    /**********************************************************************
     *    compute unpredicatability of first six spectral lines            * 
     **********************************************************************/
    for ( j = 0; j < gfc->cw_lower_index; j++ )
      {	 /* calculate unpredictability measure cw */
	FLOAT an, a1, a2;
	FLOAT bn, b1, b2;
	FLOAT rn, r1, r2;
	FLOAT numre, numim, den;

	a2 = gfc-> ax_sav[chn][1][j];
	b2 = gfc-> bx_sav[chn][1][j];
	r2 = gfc-> rx_sav[chn][1][j];
	a1 = gfc-> ax_sav[chn][1][j] = gfc-> ax_sav[chn][0][j];
	b1 = gfc-> bx_sav[chn][1][j] = gfc-> bx_sav[chn][0][j];
	r1 = gfc-> rx_sav[chn][1][j] = gfc-> rx_sav[chn][0][j];
	an = gfc-> ax_sav[chn][0][j] = (*wsamp_l)[j];
	bn = gfc-> bx_sav[chn][0][j] = j==0 ? (*wsamp_l)[0] : (*wsamp_l)[BLKSIZE-j];  
	rn = gfc-> rx_sav[chn][0][j] = sqrt(gfc->energy[j]);

	{ /* square (x1,y1) */
	  if( r1 != 0 ) {
	    numre = (a1*b1);
	    numim = (a1*a1-b1*b1)*(FLOAT)0.5;
	    den = r1*r1;
	  } else {
	    numre = 1;
	    numim = 0;
	    den = 1;
	  }
	}
	
	{ /* multiply by (x2,-y2) */
	  if( r2 != 0 ) {
	    FLOAT tmp2 = (numim+numre)*(a2+b2)*(FLOAT)0.5;
	    FLOAT tmp1 = -a2*numre+tmp2;
	    numre =       -b2*numim+tmp2;
	    numim = tmp1;
	    den *= r2;
	  } else {
	    /* do nothing */
	  }
	}
	
	{ /* r-prime factor */
	  FLOAT tmp = (2*r1-r2)/den;
	  numre *= tmp;
	  numim *= tmp;
	}
	den=rn+fabs(2*r1-r2);
	if( den != 0 ) {
	  numre = (an+bn)*(FLOAT)0.5-numre;
	  numim = (an-bn)*(FLOAT)0.5-numim;
	  den = sqrt(numre*numre+numim*numim)/den;
	}
	gfc->cw[j] = den;
      }



    /**********************************************************************
     *     compute unpredicatibility of next 200 spectral lines            *
     **********************************************************************/ 
    for ( j = gfc->cw_lower_index; j < gfc->cw_upper_index; j += 4 )
      {/* calculate unpredictability measure cw */
	FLOAT rn, r1, r2;
	FLOAT numre, numim, den;
	
	k = (j+2) / 4; 
	
	{ /* square (x1,y1) */
	  r1 = gfc->energy_s[0][k];
	  if( r1 != 0 ) {
	    FLOAT a1 = (*wsamp_s)[0][k]; 
	    FLOAT b1 = (*wsamp_s)[0][BLKSIZE_s-k]; /* k is never 0 */
	    numre = (a1*b1);
	    numim = (a1*a1-b1*b1)*(FLOAT)0.5;
	    den = r1;
	    r1 = sqrt(r1);
	  } else {
	    numre = 1;
	    numim = 0;
	    den = 1;
	  }
	}
	
	
	{ /* multiply by (x2,-y2) */
	  r2 = gfc->energy_s[2][k];
	  if( r2 != 0 ) {
	    FLOAT a2 = (*wsamp_s)[2][k]; 
	    FLOAT b2 = (*wsamp_s)[2][BLKSIZE_s-k];
	    
	    
	    FLOAT tmp2 = (numim+numre)*(a2+b2)*(FLOAT)0.5;
	    FLOAT tmp1 = -a2*numre+tmp2;
	    numre =       -b2*numim+tmp2;
	    numim = tmp1;
	    
	    r2 = sqrt(r2);
	    den *= r2;
	  } else {
	    /* do nothing */
	  }
	}
	
	{ /* r-prime factor */
	  FLOAT tmp = (2*r1-r2)/den;
	  numre *= tmp;
	  numim *= tmp;
	}
	
	rn = sqrt(gfc->energy_s[1][k]);
	den=rn+fabs(2*r1-r2);
	if( den != 0 ) {
	  FLOAT an = (*wsamp_s)[1][k]; 
	  FLOAT bn = (*wsamp_s)[1][BLKSIZE_s-k];
	  numre = (an+bn)*(FLOAT)0.5-numre;
	  numim = (an-bn)*(FLOAT)0.5-numim;
	  den = sqrt(numre*numre+numim*numim)/den;
	}
	
	gfc->cw[j+1] = gfc->cw[j+2] = gfc->cw[j+3] = gfc->cw[j] = den;
      }
    
#if 0
    for ( j = 14; j < HBLKSIZE-4; j += 4 )
      {/* calculate energy from short ffts */
	FLOAT8 tot,ave;
	k = (j+2) / 4; 
	for (tot=0, sblock=0; sblock < 3; sblock++)
	  tot+=gfc->energy_s[sblock][k];
	ave = gfc->energy[j+1]+ gfc->energy[j+2]+ gfc->energy[j+3]+ gfc->energy[j];
	ave /= 4.;
	/*
	  printf("energy / tot %i %5.2f   %e  %e\n",j,ave/(tot*16./3.),
	  ave,tot*16./3.);
	*/
	gfc->energy[j+1] = gfc->energy[j+2] = gfc->energy[j+3] =  gfc->energy[j]=tot;
      }
#endif
    
    
    
    
    
    
    
    
    /**********************************************************************
     *    Calculate the energy and the unpredictability in the threshold   *
     *    calculation partitions                                           *
     **********************************************************************/
    b = 0;
    for (j = 0; j < gfc->cw_upper_index && gfc->numlines_l[b] && b<gfc->npart_l_orig; )
      {
	FLOAT8 ebb, cbb;

	ebb = gfc->energy[j];
	cbb = gfc->energy[j] * gfc->cw[j];
	j++;


	for (i = gfc->numlines_l[b] - 1; i > 0; i--)
	  {
	    ebb += gfc->energy[j];
	    cbb += gfc->energy[j] * gfc->cw[j];
	    j++;
	  }
	eb[b] = ebb;
	cb[b] = cbb;
	b++;
      }

    for (; b < gfc->npart_l_orig; b++ )
      {
	FLOAT8 ebb = gfc->energy[j++];
	assert(gfc->numlines_l[b]);
	for (i = gfc->numlines_l[b] - 1; i > 0; i--)
	  {
	    ebb += gfc->energy[j++];
	  }
	eb[b] = ebb;
	cb[b] = ebb * 0.4;
      }


    /**********************************************************************
     *      convolve the partitioned energy and unpredictability           *
     *      with the spreading function, s3_l[b][k]                        *
     ******************************************************************** */
    gfc->pe[chn] = 0;		/*  calculate percetual entropy */
    for ( b = 0;b < gfc->npart_l; b++ )
      {
	FLOAT8 tbb,ecb,ctb;

	ecb = 0;
	ctb = 0;
	for ( k = gfc->s3ind[b][0]; k <= gfc->s3ind[b][1]; k++ )
	  {
	    ecb += gfc->s3_l[b][k] * eb[k];	/* sprdngf for Layer III */
	    ctb += gfc->s3_l[b][k] * cb[k];
	  }

	/* calculate the tonality of each threshold calculation partition */
	/* calculate the SNR in each threshhold calculation partition */

	tbb = ecb;
	if (tbb != 0)
	  {
	    tbb = ctb / tbb;
	    if (tbb <= 0.04875584301)
	      {
		tbb = exp(-LN_TO_LOG10 * (TMN - NMT));
	      }
	    else if (tbb > 0.4989003827)
	      {
		tbb = 1;
	      }
	    else
	      {
		tbb = log(tbb);
		tbb = exp(((TMN - NMT)*(LN_TO_LOG10*0.299))
			+ ((TMN - NMT)*(LN_TO_LOG10*0.43 ))*tbb);  /* conv1=-0.299, conv2=-0.43 */
	      }
	  }

	tbb = Min(gfc->minval[b], tbb);
	ecb *= tbb;

	/* long block pre-echo control.   */
	/* rpelev=2.0, rpelev2=16.0 */
	/* note: all surges in PE are because of this pre-echo formula
	 * for thr[b].  If it this is not used, PE is always around 600
	 */
	/* dont use long block pre-echo control if previous granule was 
	 * a short block.  This is to avoid the situation:   
	 * frame0:  quiet (very low masking)  
	 * frame1:  surge  (triggers short blocks)
	 * frame2:  regular frame.  looks like pre-echo when compared to 
	 *          frame0, but all pre-echo was in frame1.
	 */
	if (gfc->blocktype_old[chn] == SHORT_TYPE )
	  thr[b] = Min(ecb, rpelev*gfc->nb_1[chn][b]);
	else
	  thr[b] = Min(ecb, Min(rpelev*gfc->nb_1[chn][b],rpelev2*gfc->nb_2[chn][b]) );

	
	
	gfc->nb_2[chn][b] = gfc->nb_1[chn][b];
	gfc->nb_1[chn][b] = ecb;

	if (thr[b] < eb[b])
	  {
	    if (thr[b]<1e-6) thr[b]=1e-6;
	    gfc->pe[chn] -= gfc->numlines_l[b] * log(thr[b] / eb[b]);
	  }
      }


    
    /*************************************************************** 
     * determine the block type (window type) based on L & R channels
     * 
     ***************************************************************/
    {  /* compute PE for all 4 channels */
      FLOAT mn,mx,ma=0,mb=0,mc=0;
      
      for ( j = HBLKSIZE_s/2; j < HBLKSIZE_s; j ++)
	{
	  ma += gfc->energy_s[0][j];
	  mb += gfc->energy_s[1][j];
	  mc += gfc->energy_s[2][j];
	}
      mn = Min(ma,mb);
      mn = Min(mn,mc);
      mx = Max(ma,mb);
      mx = Max(mx,mc);
      
      /* bit allocation is based on pe.  */
      if (mx>mn) {
	FLOAT8 tmp = 400*log(mx/(1e-12+mn));
	if (tmp>gfc->pe[chn]) gfc->pe[chn]=tmp;
      }

      /* block type is based just on L or R channel */      
      if (chn<2) {
	uselongblock[chn] = 1;
	
	/* tuned for t1.wav.  doesnt effect most other samples */
	if (gfc->pe[chn] > 3000) 
	  uselongblock[chn]=0;
	
	if ( mx > 30*mn ) 
	  {/* big surge of energy - always use short blocks */
	    uselongblock[chn] = 0;
	  } 
	else if ((mx > 10*mn) && (gfc->pe[chn] > 1000))
	  {/* medium surge, medium pe - use short blocks */
	    uselongblock[chn] = 0;
	  }
	
	/* disable short blocks */
	if (gfp->no_short_blocks)
	  uselongblock[chn]=1;
      }
    }


    if (gfp->gtkflag) {
      FLOAT mn,mx,ma=0,mb=0,mc=0;

      for ( j = HBLKSIZE_s/2; j < HBLKSIZE_s; j ++)
      {
        ma += gfc->energy_s[0][j];
        mb += gfc->energy_s[1][j];
        mc += gfc->energy_s[2][j];
      }
      mn = Min(ma,mb);
      mn = Min(mn,mc);
      mx = Max(ma,mb);
      mx = Max(mx,mc);

      gfc->pinfo->ers[gr_out][chn]=gfc->ers_save[chn];
      gfc->ers_save[chn]=(mx/(1e-12+mn));
      gfc->pinfo->pe[gr_out][chn]=gfc->pe_save[chn];
      gfc->pe_save[chn]=gfc->pe[chn];
    }


    /*************************************************************** 
     * compute masking thresholds for both short and long blocks
     ***************************************************************/
    /* longblock threshold calculation (part 2) */
    for ( sb = 0; sb < SBPSY_l; sb++ )
      {
	FLOAT8 enn = gfc->w1_l[sb] * eb[gfc->bu_l[sb]] + gfc->w2_l[sb] * eb[gfc->bo_l[sb]];
	FLOAT8 thmm = gfc->w1_l[sb] *thr[gfc->bu_l[sb]] + gfc->w2_l[sb] * thr[gfc->bo_l[sb]];
	for ( b = gfc->bu_l[sb]+1; b < gfc->bo_l[sb]; b++ )
	  {
	    enn  += eb[b];
	    thmm += thr[b];
	  }
	gfc->en[chn].l[sb] = enn;
	gfc->thm[chn].l[sb] = thmm;
      }
    

    /* threshold calculation for short blocks */
    for ( sblock = 0; sblock < 3; sblock++ )
      {
	j = 0;
	for ( b = 0; b < gfc->npart_s_orig; b++ )
	  {
	    FLOAT ecb = gfc->energy_s[sblock][j++];
	    for (i = gfc->numlines_s[b]; i > 0; i--)
	      {
		ecb += gfc->energy_s[sblock][j++];
	      }
	    eb[b] = ecb;
	  }

	for ( b = 0; b < gfc->npart_s; b++ )
	  {
	    FLOAT8 ecb = 0;
	    for ( k = gfc->s3ind_s[b][0]; k <= gfc->s3ind_s[b][1]; k++ )
	      {
		ecb += gfc->s3_s[b][k] * eb[k];
	      }
	    thr[b] = Max (1e-6, ecb);
	  }

	for ( sb = 0; sb < SBPSY_s; sb++ )
	  {
	    FLOAT8 enn  = gfc->w1_s[sb] * eb[gfc->bu_s[sb]] + gfc->w2_s[sb] * eb[gfc->bo_s[sb]];
	    FLOAT8 thmm = gfc->w1_s[sb] *thr[gfc->bu_s[sb]] + gfc->w2_s[sb] * thr[gfc->bo_s[sb]];
	    for ( b = gfc->bu_s[sb]+1; b < gfc->bo_s[sb]; b++ )
	      {
		enn  += eb[b];
		thmm += thr[b];
	      }
	    gfc->en[chn].s[sb][sblock] = enn;
	    gfc->thm[chn].s[sb][sblock] = thmm;
	  }
      }

  } /* end loop over chn */


  /* compute M/S thresholds from Johnston & Ferreira 1992 ICASSP paper */
  if ( numchn==4 /* mid/side and r/l */) {
    FLOAT8 rside,rmid,mld;
    int chmid=2,chside=3; 
    
    for ( sb = 0; sb < SBPSY_l; sb++ ) {
      /* use this fix if L & R masking differs by 2db or less */
      /* if db = 10*log10(x2/x1) < 2 */
      /* if (x2 < 1.58*x1) { */
      if (gfc->thm[0].l[sb] <= 1.58*gfc->thm[1].l[sb]
	  && gfc->thm[1].l[sb] <= 1.58*gfc->thm[0].l[sb]) {

	mld = gfc->mld_l[sb]*gfc->en[chside].l[sb];
	rmid = Max(gfc->thm[chmid].l[sb], Min(gfc->thm[chside].l[sb],mld));

	mld = gfc->mld_l[sb]*gfc->en[chmid].l[sb];
	rside = Max(gfc->thm[chside].l[sb],Min(gfc->thm[chmid].l[sb],mld));

	gfc->thm[chmid].l[sb]=rmid;
	gfc->thm[chside].l[sb]=rside;
      }
    }
    for ( sb = 0; sb < SBPSY_s; sb++ ) {
      for ( sblock = 0; sblock < 3; sblock++ ) {
	if (gfc->thm[0].s[sb][sblock] <= 1.58*gfc->thm[1].s[sb][sblock]
	    && gfc->thm[1].s[sb][sblock] <= 1.58*gfc->thm[0].s[sb][sblock]) {

	  mld = gfc->mld_s[sb]*gfc->en[chside].s[sb][sblock];
	  rmid = Max(gfc->thm[chmid].s[sb][sblock],Min(gfc->thm[chside].s[sb][sblock],mld));

	  mld = gfc->mld_s[sb]*gfc->en[chmid].s[sb][sblock];
	  rside = Max(gfc->thm[chside].s[sb][sblock],Min(gfc->thm[chmid].s[sb][sblock],mld));

	  gfc->thm[chmid].s[sb][sblock]=rmid;
	  gfc->thm[chside].s[sb][sblock]=rside;
	}
      }
    }
  }


  

  
  
  if (gfp->mode == MPG_MD_JOINT_STEREO)  {
    /* determin ms_ratio from masking thresholds*/
    /* use ms_stereo (ms_ratio < .35) if average thresh. diff < 5 db */
    FLOAT8 db,x1,x2,sidetot=0,tot=0;
    for (sb= SBPSY_l/4 ; sb< SBPSY_l; sb ++ ) {
      x1 = Min(gfc->thm[0].l[sb],gfc->thm[1].l[sb]);
      x2 = Max(gfc->thm[0].l[sb],gfc->thm[1].l[sb]);
      /* thresholds difference in db */
      if (x2 >= 1000*x1)  db=3;
      else db = log10(x2/x1);  
      /*  printf("db = %f %e %e  \n",db,gfc->thm[0].l[sb],gfc->thm[1].l[sb]);*/
      sidetot += db;
      tot++;
    }
    ms_ratio_l= (sidetot/tot)*0.7; /* was .35*(sidetot/tot)/5.0*10 */
    ms_ratio_l = Min(ms_ratio_l,0.5);
    
    sidetot=0; tot=0;
    for ( sblock = 0; sblock < 3; sblock++ )
      for ( sb = SBPSY_s/4; sb < SBPSY_s; sb++ ) {
	x1 = Min(gfc->thm[0].s[sb][sblock],gfc->thm[1].s[sb][sblock]);
	x2 = Max(gfc->thm[0].s[sb][sblock],gfc->thm[1].s[sb][sblock]);
	/* thresholds difference in db */
	if (x2 >= 1000*x1)  db=3;
	else db = log10(x2/x1);  
	sidetot += db;
	tot++;
      }
    ms_ratio_s = (sidetot/tot)*0.7; /* was .35*(sidetot/tot)/5.0*10 */
    ms_ratio_s = Min(ms_ratio_s,.5);
  }

  /*************************************************************** 
   * determin final block type
   ***************************************************************/

  for (chn=0; chn<gfc->stereo; chn++) {
    blocktype[chn] = NORM_TYPE;
  }


  if (gfc->stereo==2) {
    if (!gfp->allow_diff_short || gfp->mode==MPG_MD_JOINT_STEREO) {
      /* force both channels to use the same block type */
      /* this is necessary if the frame is to be encoded in ms_stereo.  */
      /* But even without ms_stereo, FhG  does this */
      int bothlong= (uselongblock[0] && uselongblock[1]);
      if (!bothlong) {
	uselongblock[0]=0;
	uselongblock[1]=0;
      }
    }
  }

  
  
  /* update the blocktype of the previous granule, since it depends on what
   * happend in this granule */
  for (chn=0; chn<gfc->stereo; chn++) {
    if ( uselongblock[chn])
      {				/* no attack : use long blocks */
	switch( gfc->blocktype_old[chn] ) 
	  {
	  case NORM_TYPE:
	  case STOP_TYPE:
	    blocktype[chn] = NORM_TYPE;
	    break;
	  case SHORT_TYPE:
	    blocktype[chn] = STOP_TYPE; 
	    break;
	  case START_TYPE:
	    fprintf( stderr, "Error in block selecting\n" );
	    abort();
	    break; /* problem */
	  }
      } else   {
	/* attack : use short blocks */
	blocktype[chn] = SHORT_TYPE;
	if ( gfc->blocktype_old[chn] == NORM_TYPE ) {
	  gfc->blocktype_old[chn] = START_TYPE;
	}
	if ( gfc->blocktype_old[chn] == STOP_TYPE ) {
	  gfc->blocktype_old[chn] = SHORT_TYPE ;
	}
      }
    
    blocktype_d[chn] = gfc->blocktype_old[chn];  /* value returned to calling program */
    gfc->blocktype_old[chn] = blocktype[chn];    /* save for next call to l3psy_anal */
  }
  
  if (blocktype_d[0]==2) 
    *ms_ratio = gfc->ms_ratio_s_old;
  else
    *ms_ratio = gfc->ms_ratio_l_old;

  gfc->ms_ratio_s_old = ms_ratio_s;
  gfc->ms_ratio_l_old = ms_ratio_l;

  /* we dont know the block type of this frame yet - assume long */
  *ms_ratio_next = ms_ratio_l;



  /*********************************************************************/
  /* compute side_energy / (side+mid)_energy */
  /* 0 = no energy in side channel */
  /* .5 = half of total energy in side channel */
  /*********************************************************************/
  if (numchn==4)  {
    FLOAT tmp = tot_ener[3]+tot_ener[2];
    *ms_ener_ratio = gfc->ms_ener_ratio_old;
    gfc->ms_ener_ratio_old=0;
    if (tmp>0) gfc->ms_ener_ratio_old=tot_ener[3]/tmp;
  } else
    /* we didn't compute ms_ener_ratios */
    *ms_ener_ratio = 0;
 
}






void L3para_read(FLOAT8 sfreq, int *numlines_l,int *numlines_s, int *partition_l, FLOAT8 *minval,
FLOAT8 s3_l[64][64], FLOAT8 s3_s[CBANDS + 1][CBANDS + 1],
FLOAT8 *SNR, 
int *bu_l, int *bo_l, FLOAT8 *w1_l, FLOAT8 *w2_l, 
int *bu_s, int *bo_s, FLOAT8 *w1_s, FLOAT8 *w2_s)
{
  FLOAT8 freq_tp;
  FLOAT8 bval_l[CBANDS], bval_s[CBANDS];
  int   cbmax=0, cbmax_tp;
  FLOAT8 *p = psy_data;

  int  sbmax ;
  int  i,j,k,k2,loop, part_max ;
  int freq_scale=1;


  /* use MPEG1 tables.  The MPEG2 tables in tables.c appear to be 
   * junk.  MPEG2 doc claims data for these tables is the same as the
   * MPEG1 data for 2x sampling frequency */
  /*  if (sfreq<32000) freq_scale=2; */
  


  /* Read long block data */

  for(loop=0;loop<6;loop++)
    {
      freq_tp = *p++;
      cbmax_tp = (int) *p++;
      cbmax_tp++;

      if (sfreq == freq_tp/freq_scale )
	{
	  cbmax = cbmax_tp;
	  for(i=0,k2=0;i<cbmax_tp;i++)
	    {
	      j = (int) *p++;
	      numlines_l[i] = (int) *p++;
	      minval[i] = exp(-((*p++) - NMT) * LN_TO_LOG10);
	      /* qthr_l[i] = *p++ */ p++;
	      /* norm_l[i] = *p++*/ p++;
	      bval_l[i] = *p++;
	      if (j!=i)
		{
		  fprintf(stderr,"1. please check \"psy_data\"");
		  exit(-1);
		}
	      for(k=0;k<numlines_l[i];k++)
		partition_l[k2++] = i ;
	    }
	}
      else
	p += cbmax_tp * 6;
    }

#define NEWBARKXXX
#ifdef NEWBARK
  /* compute bark values of each critical band */
  j = 0;
  for(i=0;i<cbmax;i++)
    {
      FLOAT8 ji, freq, bark;

      ji = j + (numlines_l[i]-1)/2.0;
      freq = sfreq*ji/1024000.0;
      bark = 13*atan(.76*freq) + 3.5*atan(freq*freq/(7.5*7.5));

      printf("%i %i bval_l table=%f  f=%f  formaula=%f \n",i,j,bval_l[i],freq,bark);
      bval_l[i]=bark;
      j += numlines_l[i];
    }
#endif

  /************************************************************************
   * Now compute the spreading function, s[j][i], the value of the spread-*
   * ing function, centered at band j, for band i, store for later use    *
   ************************************************************************/
  /* i.e.: sum over j to spread into signal barkval=i  
     NOTE: i and j are used opposite as in the ISO docs */
  part_max = cbmax ;
  for(i=0;i<part_max;i++)
    {
      FLOAT8 tempx,x,tempy,temp;
      for(j=0;j<part_max;j++)
	{
          if (i>=j) tempx = (bval_l[i] - bval_l[j])*3.0;
	  else    tempx = (bval_l[i] - bval_l[j])*1.5; 

	  if(tempx>=0.5 && tempx<=2.5)
	    {
	      temp = tempx - 0.5;
	      x = 8.0 * (temp*temp - 2.0 * temp);
	    }
	  else x = 0.0;
	  tempx += 0.474;
	  tempy = 15.811389 + 7.5*tempx - 17.5*sqrt(1.0+tempx*tempx);

#ifdef NEWS3
	  if (j>=i) tempy = (bval_l[j] - bval_l[i])*(-15);
	  else    tempy = (bval_l[j] - bval_l[i])*25;
	  x=0; 
#endif
	  /*
	  if ((i==part_max/2)  && (fabs(bval_l[j] - bval_l[i])) < 3) {
	    printf("bark=%f   x+tempy = %f  \n",bval_l[j] - bval_l[i],x+tempy);
	  }
	  */

	  if (tempy <= -60.0) s3_l[i][j] = 0.0;
	  else                s3_l[i][j] = exp( (x + tempy)*LN_TO_LOG10 ); 
	}
    }

  /* Read short block data */
  for(loop=0;loop<6;loop++)
    {
      freq_tp = *p++;
      cbmax_tp = (int) *p++;
      cbmax_tp++;

      if (sfreq == freq_tp/freq_scale )
	{
	  cbmax = cbmax_tp;
	  for(i=0,k2=0;i<cbmax_tp;i++)
	    {
	      j = (int) *p++;
	      numlines_s[i] = (int) *p++;
	      /* qthr_s[i] = *p++*/  p++;         
	      /* norm_s[i] =*p++ */ p++;         
	      SNR[i] = *p++;            
	      bval_s[i] = *p++;
	      if (j!=i)
		{
		  fprintf(stderr,"3. please check \"psy_data\"");
		  exit(-1);
		}
	      numlines_s[i]--;
	    }
	  numlines_s[i] = -1;
	}
      else
	p += cbmax_tp * 6;
    }


#ifdef NEWBARK
  /* compute bark values of each critical band */
  j = 0;
  for(i=0;i<cbmax;i++)
    {
      FLOAT8 ji, freq, bark;
      ji = (j * 2 + numlines_s[i]) / 2.0;
      freq = sfreq*ji/256000.0;
      bark = 13*atan(.76*freq) + 3.5*atan(freq*freq/(7.5*7.5));
      printf("%i %i bval_s = %f  %f  %f \n",i,j,bval_s[i],freq,bark);
      bval_s[i]=bark;
      j += numlines_s[i] + 1;
    }
#endif



  /************************************************************************
   * Now compute the spreading function, s[j][i], the value of the spread-*
   * ing function, centered at band j, for band i, store for later use    *
   ************************************************************************/
  part_max = cbmax ;
  for(i=0;i<part_max;i++)
    {
      FLOAT8 tempx,x,tempy,temp;
      for(j=0;j<part_max;j++)
	{
          if (i>=j) tempx = (bval_s[i] - bval_s[j])*3.0;
	  else    tempx = (bval_s[i] - bval_s[j])*1.5; 

	  if(tempx>=0.5 && tempx<=2.5)
	    {
	      temp = tempx - 0.5;
	      x = 8.0 * (temp*temp - 2.0 * temp);
	    }
	  else x = 0.0;
	  tempx += 0.474;
	  tempy = 15.811389 + 7.5*tempx - 17.5*sqrt(1.0+tempx*tempx);
#ifdef NEWS3
	  if (j>=i) tempy = (bval_s[j] - bval_s[i])*(-15);
	  else    tempy = (bval_s[j] - bval_s[i])*25;
	  x=0; 
#endif
	  if (tempy <= -60.0) s3_s[i][j] = 0.0;
	  else                s3_s[i][j] = exp( (x + tempy)*LN_TO_LOG10 );
	}
    }
  /* Read long block data for converting threshold calculation 
     partitions to scale factor bands */

  for(loop=0;loop<6;loop++)
    {
      freq_tp = *p++;
      sbmax =  (int) *p++;
      sbmax++;

      if (sfreq == freq_tp/freq_scale)
	{
	  for(i=0;i<sbmax;i++)
	    {
	      j = (int) *p++;
	      p++;             
	      bu_l[i] = (int) *p++;
	      bo_l[i] = (int) *p++;
	      w1_l[i] = (FLOAT8) *p++;
	      w2_l[i] = (FLOAT8) *p++;
	      if (j!=i)
		{ fprintf(stderr,"30:please check \"psy_data\"\n");
		exit(-1);
		}

	      if (i!=0)
		if ( (fabs(1.0-w1_l[i]-w2_l[i-1]) > 0.01 ) )
		  {
		    fprintf(stderr,"31l: please check \"psy_data.\"\n");
                  fprintf(stderr,"w1,w2: %f %f \n",w1_l[i],w2_l[i-1]);
		    exit(-1);
		  }
	    }
	}
      else
	p += sbmax * 6;
    }

  /* Read short block data for converting threshold calculation 
     partitions to scale factor bands */

  for(loop=0;loop<6;loop++)
    {
      freq_tp = *p++;
      sbmax = (int) *p++;
      sbmax++;

      if (sfreq == freq_tp/freq_scale)
	{
	  for(i=0;i<sbmax;i++)
	    {
	      j = (int) *p++;
	      p++;
	      bu_s[i] = (int) *p++;
	      bo_s[i] = (int) *p++;
	      w1_s[i] = *p++;
	      w2_s[i] = *p++;
	      if (j!=i)
		{ fprintf(stderr,"30:please check \"psy_data\"\n");
		exit(-1);
		}

	      if (i!=0)
		if ( (fabs(1.0-w1_s[i]-w2_s[i-1]) > 0.01 ) )
		  { 
                  fprintf(stderr,"31s: please check \"psy_data.\"\n");
                  fprintf(stderr,"w1,w2: %f %f \n",w1_s[i],w2_s[i-1]);
		  exit(-1);
		  }
	    }
	}
      else
	p += sbmax * 6;
    }

}
