/**********************************************************************
 * ISO MPEG Audio Subgroup Software Simulation Group (1996)
 * ISO 13818-3 MPEG-2 Audio Encoder - Lower Sampling Frequency Extension
 *
 * $Id$
 *
 * $Log$
 * Revision 1.8  1999/12/14 04:38:08  markt
 * Takehiro's fft's back in.  fft_short2(), fft_long2() will call original
 * fft's  (with one minor change:  0 protection for ax[] and bx[] was not
 * needed and has been removed).  Takehiro's routines are fft_short() and
 * fft_long().  They dont give bit-for-bit identical answers, and I still
 * want to track this down before making them the defaults.  .
 *
 * Revision 1.7  1999/12/08 05:46:52  markt
 * avoid bounds check on xr[] for non-hq mode, from Mat.
 * spelling error in psymodel.c fixed :-)
 *
 * Revision 1.6  1999/12/08 03:49:15  takehiro
 * debugged possible buffer overrun.
 *
 * Revision 1.5  1999/12/07 02:04:41  markt
 * backed out takehiro's fft changes for now
 * added latest quantize_xrpow asm from Acy and Mat
 *
 * Revision 1.2  1999/11/29 02:45:59  markt
 * MS stereo switch slightly improved:  old formula was based on the average
 * of ms_ratio of both granules.  New formula uses ms_ratio from both
 * granules and the previous and next granule.  This will help avoid toggleing
 * MS stereo off for a single frame.  Long runs of MS stereo or regular
 * stereo will not be affected.
 *
 * also fixed a bug in frame analyzer - it was accessing l3_xmin in the last
 * scalefactor (l3_xmin and maskings are not computed for last scalefactor)
 *
 * Revision 1.1.1.1  1999/11/24 08:41:25  markt
 * initial checkin of LAME
 * Starting with LAME 3.57beta with some modifications
 *
 * Revision 1.2  1998/10/05 17:06:48  larsi
 * *** empty log message ***
 *
 * Revision 1.1.1.1  1998/10/05 14:47:18  larsi
 *
 * Revision 1.2  1997/01/19 22:28:29  rowlands
 * Layer 3 bug fixes from Seymour Shlien
 *
 * Revision 1.1  1996/02/14 04:04:23  rowlands
 * Initial revision
 *
 * Received from Mike Coleman
 **********************************************************************/
/**********************************************************************
 *   date   programmers         comment                               *
 * 2/25/91  Davis Pan           start of version 1.0 records          *
 * 5/10/91  W. Joseph Carter    Ported to Macintosh and Unix.         *
 * 7/10/91  Earle Jennings      Ported to MsDos.                      *
 *                              replace of floats with FLOAT          *
 * 2/11/92  W. Joseph Carter    Fixed mem_alloc() arg for "absthr".   *
 * 3/16/92  Masahiro Iwadare	Modification for Layer III            *
 * 17/4/93  Masahiro Iwadare    Updated for IS Modification           *
 **********************************************************************/

#include "util.h"
#include "globalflags.h"
#include "encoder.h"
#include "psymodel.h"
#include "l3side.h"
#include <assert.h>
#ifdef HAVEGTK
#include "gtkanal.h"
#endif
#include "tables.h"
#include "fft.h"

#ifdef M_LN10
#define		LN_TO_LOG10		(M_LN10/10)
#else
#define         LN_TO_LOG10             0.2302585093
#endif

#define maximum(x,y) ( (x>y) ? x : y )
#define minimum(x,y) ( (x<y) ? x : y )


static FLOAT8 s3_l[CBANDS][CBANDS]; /* needed global static by sprdngfs */

void L3para_read( FLOAT8 sfreq, int numlines[CBANDS],int numlines_s[CBANDS], int partition_l[HBLKSIZE],
		  FLOAT8 minval[CBANDS], FLOAT8 qthr_l[CBANDS], FLOAT8 norm_l[CBANDS],
		  FLOAT8 s3_l[CBANDS][CBANDS],  FLOAT8 s3_s[CBANDS][CBANDS], 
                  int partition_s[HBLKSIZE_s], FLOAT8 qthr_s[CBANDS],
		  FLOAT8 norm_s[CBANDS], FLOAT8 SNR_s[CBANDS],
		  int bu_l[SBPSY_l], int bo_l[SBPSY_l],
		  FLOAT8 w1_l[SBPSY_l], FLOAT8 w2_l[SBPSY_l],
		  int bu_s[SBPSY_s], int bo_s[SBPSY_s],
		  FLOAT8 w1_s[SBPSY_s], FLOAT8 w2_s[SBPSY_s] );
									







 

void L3psycho_anal( short int *buffer[2], int stereo,
		    int gr_out , layer * info,
		    FLOAT8 sfreq, int check_ms_stereo, 
                    FLOAT8 *ms_ratio,
                    FLOAT8 *ms_ratio_next,
		    FLOAT8 masking_ratio_d[2][SBPSY_l], FLOAT8 masking_ratio_ds[2][SBPSY_s][3],
		    FLOAT8 masking_MS_ratio_d[2][SBPSY_l], FLOAT8 masking_MS_ratio_ds[2][SBPSY_s][3],
		    FLOAT8 percep_entropy[2],FLOAT8 percep_MS_entropy[2], 
                    int blocktype_d[2])
{
  static FLOAT8 pe[4]={0,0,0,0};
  static FLOAT8 ms_ratio_s_old=0,ms_ratio_l_old=0;
  
  static FLOAT8 ratio[4][SBPSY_l];
  static FLOAT8 ratio_s[4][SBPSY_s][3];
#ifdef HAVEGTK
  static FLOAT energy_save[4][HBLKSIZE];
  static FLOAT8 pe_save[4];
  static FLOAT8 ers_save[4];
  static FLOAT8 en_save[4][SBPSY_l];
  static FLOAT8 en_s_save[4][SBPSY_s][3];
#endif
  static FLOAT8 thm_save[4][SBPSY_l];
  static FLOAT8 thm_s_save[4][SBPSY_s][3];
  
  
  int blocktype[2],uselongblock[2],chn;
  int numchn;
  int   b, i, j, k;
  FLOAT8 ms_ratio_l=0,ms_ratio_s=0;
  FLOAT8 estot[4][3];
  FLOAT wsamp[BLKSIZE];
  FLOAT wsamp_s[3][BLKSIZE_s];


  FLOAT/*FLOAT8*/   thr[CBANDS];
  FLOAT energy[HBLKSIZE];
  FLOAT energy_s[3][HBLKSIZE_s];

  static float mld_l[SBPSY_l],mld_s[SBPSY_s];
  

  

/* The static variables "r", "phi_sav", "new", "old" and "oldest" have    */
/* to be remembered for the unpredictability measure.  For "r" and        */
/* "phi_sav", the first index from the left is the channel select and     */
/* the second index is the "age" of the data.                             */
  static FLOAT8 	cw[HBLKSIZE], eb[CBANDS];
  static FLOAT8 	ctb[CBANDS];
  static FLOAT8	SNR_l[CBANDS], SNR_s[CBANDS];
  static FLOAT8	minval[CBANDS],qthr_l[CBANDS],norm_l[CBANDS];
  static FLOAT8	qthr_s[CBANDS],norm_s[CBANDS];
  static FLOAT8	nb_1[4][CBANDS], nb_2[4][CBANDS];
  static FLOAT8  s3_s[CBANDS][CBANDS];
  
/* Scale Factor Bands */
  static int	bu_l[SBPSY_l],bo_l[SBPSY_l] ;
  static int	bu_s[SBPSY_s],bo_s[SBPSY_s] ;
  static FLOAT8	w1_l[SBPSY_l], w2_l[SBPSY_l];
  static FLOAT8	w1_s[SBPSY_s], w2_s[SBPSY_s];
  static FLOAT8	en[SBPSY_l],   thm[SBPSY_l] ;
  static int	blocktype_old[2];
  int	sb,sblock;
  static int	partition_l[HBLKSIZE],partition_s[HBLKSIZE_s];
  static int npart_l,npart_s;
  static int npart_l_orig,npart_s_orig;
  
  static int      s3ind[CBANDS][2];
  static int      s3ind_s[CBANDS][2];
  
  
  static FLOAT   nb[CBANDS], cb[CBANDS], ecb[CBANDS];
  static	int	numlines_s[CBANDS] ;
  static	int	numlines_l[CBANDS];
  static FLOAT   ax_sav[4][2][HBLKSIZE], bx_sav[4][2][HBLKSIZE],rx_sav[4][2][HBLKSIZE];

  if((frameNum==0) && (gr_out==0)){
    
    blocktype_old[0]=STOP_TYPE;
    blocktype_old[1]=STOP_TYPE;
    i = sfreq + 0.5;
    switch(i){
    case 32000: break;
    case 44100: break;
    case 48000: break;
    case 16000: break;
    case 22050: break;
    case 24000: break;
    default:    fprintf(stderr,"error, invalid sampling frequency: %d Hz\n",i);
      exit(-1);
    }
    
    /* reset states used in unpredictability measure */
    memset (rx_sav,0, sizeof(rx_sav));
    memset (ax_sav,0, sizeof(ax_sav));
    memset (bx_sav,0, sizeof(bx_sav));
    
    
    
    /* setup stereo demasking thresholds */
    /* formula reverse enginerred from plot in paper */
    for ( sb = 0; sb < SBPSY_s; sb++ ) {
      FLOAT8 mld = 1.25*(1-cos(PI*sb/SBPSY_s))-2.5;
      mld_s[sb] = pow(10.0,mld);
    }
    for ( sb = 0; sb < SBPSY_l; sb++ ) {
      FLOAT8 mld = 1.25*(1-cos(PI*sb/SBPSY_l))-2.5;
      mld_l[sb] = pow(10.0,mld);
    }
    
    for (i=0;i<HBLKSIZE;i++) partition_l[i]=-1;
    for (i=0;i<HBLKSIZE_s;i++) partition_s[i]=-1;
    memset (norm_l,0, sizeof(norm_l));
    memset (norm_s,0, sizeof(norm_s));
    
    L3para_read( sfreq,numlines_l,numlines_s,partition_l,minval,qthr_l,norm_l,s3_l,s3_s,
		 partition_s,qthr_s,norm_s,SNR_s,
		 bu_l,bo_l,w1_l,w2_l, bu_s,bo_s,w1_s,w2_s );
    
    
    /* npart_l_orig   = number of partition bands before convolution */
    /* npart_l  = number of partition bands after convolution */
    npart_l_orig=0; npart_s_orig=0;
    for (i=0;i<HBLKSIZE;i++) 
      if (partition_l[i]>npart_l_orig) npart_l_orig=partition_l[i];
    for (i=0;i<HBLKSIZE_s;i++) 
      if (partition_s[i]>npart_s_orig) npart_s_orig=partition_s[i];
    npart_l_orig++;
    npart_s_orig++;
    
    npart_l=bo_l[SBPSY_l-1]+1;
    npart_s=bo_s[SBPSY_s-1]+1;
    
    
    /* MPEG2 tables are screwed up 
     * the mapping from paritition bands to scalefactor bands will use
     * more paritition bands than we have.  
     * So we will not compute these fictitious partition bands by reducing
     * npart_l below.  */
    if (npart_l > npart_l_orig) {
      npart_l=npart_l_orig;
      bo_l[SBPSY_l-1]=npart_l-1;
      w2_l[SBPSY_l-1]=1.0;
    }
    if (npart_s > npart_s_orig) {
      npart_s=npart_s_orig;
      bo_s[SBPSY_s-1]=npart_s-1;
      w2_s[SBPSY_s-1]=1.0;
    }
    
    
    
    for (i=0; i<npart_l; i++) {
      for (j = 0; j < npart_l_orig; j++) {
	if (s3_l[i][j] != 0.0)
	  break;
      }
      s3ind[i][0] = j;
      
      for (j = npart_l_orig - 1; j > 0; j--) {
	if (s3_l[i][j] != 0.0)
	  break;
      }
      s3ind[i][1] = j;
    }
    
    
    for (i=0; i<npart_s; i++) {
      for (j = 0; j < npart_s_orig; j++) {
	if (s3_s[i][j] != 0.0)
	  break;
      }
      s3ind_s[i][0] = j;
      
      for (j = npart_s_orig - 1; j > 0; j--) {
	if (s3_s[i][j] != 0.0)
	  break;
      }
      s3ind_s[i][1] = j;
    }
    
    
    /*  
      #include "debugscalefac.c"
    */
    
    
#define NEWS3XX
#ifdef NEWS3     
    
    /* compute norm_l, norm_s instead of relying on table data */
    for ( b = 0;b < npart_l; b++ ) {
      FLOAT8 norm=0;
      for ( k = s3ind[b][0]; k <= s3ind[b][1]; k++ ) {
	norm += s3_l[b][k];
      }
      norm_l[b] = 1/norm;
      /*printf("%i  norm=%f  norm_l=%f \n",b,1/norm,norm_l[b]);*/
    }
    for ( b = 0;b < npart_s; b++ ) {
      FLOAT8 norm=0;
      for ( k = s3ind_s[b][0]; k <= s3ind_s[b][1]; k++ ) {
	norm += s3_s[b][k];
	/*	 printf("%i %i  k=%i norm = %f %f \n",npart_s,npart_s_orig,k,norm,s3_s[b][k]);
	 */
      }
      norm_s[b] = 1/norm;
      /*printf("%i  norm=%f  norm_s=%f \n",b,1/norm,norm_l[b]);*/
    }
#endif
    
    /* MPEG1 SNR_s data is given in db, convert to energy */
    if (info->version == MPEG_AUDIO_ID) {
      for ( b = 0;b < npart_s; b++ ) {
	SNR_s[b]=exp( (FLOAT8) SNR_s[b] * LN_TO_LOG10 );
      }
    }
    init_fft();
  }
  /************************* End of Initialization *****************************/
  
  
  
  
  
  numchn=stereo;
  if (highq && (info->mode == MPG_MD_JOINT_STEREO)) numchn=4;
  for (chn=0; chn<numchn; chn++) {

    if (chn<2) {    
      /* LR maskings  */
      percep_entropy[chn] = pe[chn]; 
      for ( j = 0; j < SBPSY_l; j++ )
	masking_ratio_d[chn][j] = ratio[chn][j];
      for ( j = 0; j < SBPSY_s; j++ )
	for ( i = 0; i < 3; i++ )
	  masking_ratio_ds[chn][j][i] = ratio_s[chn][j][i];
    }else{
      /* MS maskings  */
      percep_MS_entropy[chn-2] = pe[chn]; 
      for ( j = 0; j < SBPSY_l; j++ )
	masking_MS_ratio_d[chn-2][j] = ratio[chn][j];
      for ( j = 0; j < SBPSY_s; j++ )
	for ( i = 0; i < 3; i++ )
	  masking_MS_ratio_ds[chn-2][j][i] = ratio_s[chn][j][i];
    }
    

    
    /**********************************************************************
     *  compute FFTs
     **********************************************************************/

    fft_long2( wsamp, energy, chn, buffer);
  
#ifdef HAVEGTK
  if(gtkflag) {
    for (j=0; j<HBLKSIZE ; j++) {
      pinfo->energy[gr_out][chn][j]=energy_save[chn][j];
      energy_save[chn][j]=energy[j];
    }
  }
#endif
    
    /**********************************************************************
     *    compute unpredicatability of first six spectral lines            * 
     **********************************************************************/
    for ( j = 0; j < 6; j++ )
      {	 /* calculate unpredictability measure cw */
	FLOAT8 an, a1, a2;
	FLOAT8 bn, b1, b2;
	FLOAT8 rn, r1, r2;
	FLOAT8 numre, numim, den;
	
	a2 = ax_sav[chn][1][j];
	b2 = bx_sav[chn][1][j];
	r2 = rx_sav[chn][1][j];
	a1 = ax_sav[chn][1][j] = ax_sav[chn][0][j];
	b1 = bx_sav[chn][1][j] = bx_sav[chn][0][j];
	r1 = rx_sav[chn][1][j] = rx_sav[chn][0][j];
	an = ax_sav[chn][0][j] = wsamp[j];
	bn = bx_sav[chn][0][j] = j==0 ? wsamp[0] : wsamp[BLKSIZE-j];  // bx[j]; 
	rn = rx_sav[chn][0][j] = sqrt(energy[j]);
	
	{ /* square (x1,y1) */
	  if( r1 != 0.0 ) {
	    numre = (a1*b1);
	    numim = (a1*a1-b1*b1)*0.5;
	    den = r1*r1;
	  } else {
	    numre = 1.0;
	    numim = 0.0;
	    den = 1.0;
	  }
	}
	
	{ /* multiply by (x2,-y2) */
	  if( r2 != 0.0 ) {
	    FLOAT8 tmp2 = (numim+numre)*(a2+b2)*0.5;
	    FLOAT8 tmp1 = -a2*numre+tmp2;
	    numre =       -b2*numim+tmp2;
	    numim = tmp1;
	    den *= r2;
	  } else {
	    /* do nothing */
	  }
	}
	
	{ /* r-prime factor */
	  FLOAT8 tmp = (2.0*r1-r2)/den;
	  numre *= tmp;
	  numim *= tmp;
	}
	
	if( (den=rn+fabs(2.0*r1-r2)) != 0.0 ) {
	  numre = (an+bn)/2.0-numre;
	  numim = (an-bn)/2.0-numim;
	  cw[j] = sqrt(numre*numre+numim*numim)/den;
	} else {
	  cw[j] = 0.0;
	}
	
      }


    fft_short2( wsamp_s, energy_s, chn, buffer);
    /**********************************************************************
     *     compute unpredicatibility of next 200 spectral lines            *
     **********************************************************************/ 
    for ( j = 6; j < 206; j += 4 )
      {/* calculate unpredictability measure cw */
	FLOAT8 rn, r1, r2;
	FLOAT8 numre, numim, den;
	
	k = (j+2) / 4; 
	
	{ /* square (x1,y1) */
	  r1 = sqrt((FLOAT8)energy_s[0][k]);
	  if( r1 != 0.0 ) {
	    FLOAT8 a1 = wsamp_s[0][k]; 
	    FLOAT8 b1 = wsamp_s[0][BLKSIZE_s-k]; /* k is never 0 */
	    numre = (a1*b1);
	    numim = (a1*a1-b1*b1)*0.5;
	    den = r1*r1;
	  } else {
	    numre = 1.0;
	    numim = 0.0;
	    den = 1.0;
	  }
	}
	
	
	{ /* multiply by (x2,-y2) */
	  r2 = sqrt((FLOAT8)energy_s[2][k]);
	  if( r2 != 0.0 ) {
	    FLOAT8 a2 = wsamp_s[2][k]; 
	    FLOAT8 b2 = wsamp_s[2][BLKSIZE_s-k];
	    
	    
	    FLOAT8 tmp2 = (numim+numre)*(a2+b2)*0.5;
	    FLOAT8 tmp1 = -a2*numre+tmp2;
	    numre =       -b2*numim+tmp2;
	    numim = tmp1;
	    
	    den *= r2;
	  } else {
	    /* do nothing */
	  }
	}
	
	{ /* r-prime factor */
	  FLOAT8 tmp = (2.0*r1-r2)/den;
	  numre *= tmp;
	  numim *= tmp;
	}
	
	rn = sqrt((FLOAT8)energy_s[1][k]);
	if( (den=rn+fabs(2.0*r1-r2)) != 0.0 ) {
	  FLOAT8 an = wsamp_s[1][k]; 
	  FLOAT8 bn = wsamp_s[1][BLKSIZE_s-k];
	  numre = (an+bn)/2.0-numre;
	  numim = (an-bn)/2.0-numim;
	  cw[j] = sqrt(numre*numre+numim*numim)/den;
	} else {
	  cw[j] = 0.0;
	}
	
	cw[j+1] = cw[j+2] = cw[j+3] = cw[j];
      }
    
    
    
    
    
    /**********************************************************************
     *    Set unpredicatiblility of remaining spectral lines to 0.4  206..513 *
     **********************************************************************/
    for ( j = 206; j < HBLKSIZE; j++ )
      cw[j] = 0.4;
    
    
    
#if 0
    for ( j = 14; j < HBLKSIZE-4; j += 4 )
      {/* calculate energy from short ffts */
	FLOAT8 tot,ave;
	k = (j+2) / 4; 
	for (tot=0, sblock=0; sblock < 3; sblock++)
	  tot+=energy_s[sblock][k];
	ave = energy[j+1]+ energy[j+2]+ energy[j+3]+ energy[j];
	ave /= 4.;
	/*
	  printf("energy / tot %i %5.2f   %e  %e\n",j,ave/(tot*16./3.),
	  ave,tot*16./3.);
	*/
	energy[j+1] = energy[j+2] = energy[j+3] =  energy[j]=tot;
      }
#endif
    
    
    
    
    
    
    
    
    /**********************************************************************
     *    Calculate the energy and the unpredictability in the threshold   *
     *    calculation partitions                                           *
     **********************************************************************/
    for ( b = 0; b < CBANDS; b++ )
      {
	eb[b] = 0.0;
	cb[b] = 0.0;
      }
    for ( j = 0; j < HBLKSIZE; j++ )
      {
	int tp = partition_l[j];
	if ( tp >= 0 )
	  {
	    eb[tp] += energy[j];
	    cb[tp] += cw[j] * energy[j];
	  }
	assert(tp<npart_l_orig);
      }
    
    
    
    /**********************************************************************
     *      convolve the partitioned energy and unpredictability           *
     *      with the spreading function, s3_l[b][k]                        *
     ******************************************************************** */
    for ( b = 0; b < CBANDS; b++ )
      {
	ecb[b] = 0.0;
	ctb[b] = 0.0;
      }
    for ( b = 0;b < npart_l; b++ )
      for ( k = s3ind[b][0]; k <= s3ind[b][1]; k++ )
	{
	  ecb[b] += s3_l[b][k] * eb[k];	/* sprdngf for Layer III */
	  ctb[b] += s3_l[b][k] * cb[k];
	}
    
    /* calculate the tonality of each threshold calculation partition */
    /* calculate the SNR in each threshhold calculation partition */
    
    for ( b = 0; b < npart_l; b++ )
      {
	FLOAT8 cbb,tbb;
	if (ecb[b] != 0.0 )
	  {
	    cbb = ctb[b]/ecb[b];
	    if (cbb <0.01) cbb = 0.01;
	    cbb = log( cbb);
	  }
	else
	  cbb = 0.0 ;
	tbb = -0.299 - 0.43*cbb;  /* conv1=-0.299, conv2=-0.43 */
	tbb = minimum( 1.0, maximum( 0.0, tbb) ) ;  /* 0<tbb<1 */
	SNR_l[b] = maximum( minval[b], 29.0*tbb+6.0*(1.0-tbb) );
	
	
      }	/* TMN=29.0,NMT=6.0 for all calculation partitions */
    
    for ( b = 0; b < npart_l; b++ ) /* calculate the threshold for each partition */
      nb[b] = ecb[b] * norm_l[b] * exp( -SNR_l[b] * LN_TO_LOG10 );
    
    
    
    for ( b = 0; b < npart_l; b++ )
      { /* pre-echo control */
	FLOAT8 temp_1; /* BUG of IS */
	int rpelev=2; int rpelev2=16; 
	/* rpelev=2.0, rpelev2=16.0 */
	temp_1 = minimum( nb[b], minimum(rpelev*nb_1[chn][b],rpelev2*nb_2[chn][b]) );
	thr[b] = maximum( qthr_l[b], temp_1 ); 
	nb_2[chn][b] = nb_1[chn][b];
	nb_1[chn][b] = nb[b];
      }
    
    /* note: all surges in PE are because of the above pre-echo formula
     * for temp_1.  it this is not used, PE is always around 600
     */
    
    pe[chn] = 0.0;		/*  calculate percetual entropy */
    for ( b = 0; b < npart_l; b++ ) {
      FLOAT8 tp = log((thr[b]+1.0) / (eb[b]+1.0) );
      tp = minimum( 0.0, tp ) ;  /*not log*/
      pe[chn] -= numlines_l[b] * tp ;
    }	/* thr[b] -> thr[b]+1.0 : for non sound portition */
    
    
    
#ifdef HAVEGTK
    if (gtkflag) {
      FLOAT8 mn,mx;
      
      for (sblock=0; sblock < 3; sblock++)
	estot[chn][sblock]=0;
      for ( j = HBLKSIZE_s/2; j < HBLKSIZE_s; j ++)
	for (sblock=0; sblock < 3; sblock++)
	  estot[chn][sblock]+=energy_s[sblock][j];
      mn = minimum(estot[chn][0],estot[chn][1]);
      mn = minimum(mn,estot[chn][2]);
      mx = maximum(estot[chn][0],estot[chn][1]);
      mx = maximum(mx,estot[chn][2]);
      
      pinfo->ers[gr_out][chn]=ers_save[chn];
      ers_save[chn]=mx/(1e-12+mn);
      pinfo->pe[gr_out][chn]=pe_save[chn];
      pe_save[chn]=pe[chn];
    }
#endif
    
    
    /*************************************************************** 
     * determine the block type (window type) based on L & R channels
     * 
     ***************************************************************/
    if (chn<2) {
      if (no_short_blocks){
	uselongblock[chn]=1;
      } else {
	FLOAT8 mn,mx;
	
	for (sblock=0; sblock < 3; sblock++)
	  estot[chn][sblock]=0;
	for ( j = HBLKSIZE_s/2; j < HBLKSIZE_s; j ++)
	  for (sblock=0; sblock < 3; sblock++)
	    estot[chn][sblock]+=energy_s[sblock][j];
	mn = minimum(estot[chn][0],estot[chn][1]);
	mn = minimum(mn,estot[chn][2]);
	mx = maximum(estot[chn][0],estot[chn][1]);
	mx = maximum(mx,estot[chn][2]);
	
	uselongblock[chn] = 1;
	
	/* tuned for t1.wav.  doesnt effect most other samples */
	if (pe[chn] > 3000) uselongblock[chn]=0; 
	
	/* big surge of energy - always use short blocks */
	if (  mx > 30*mn) uselongblock[chn] = 0;
	
	/* medium surge, medium pe - use short blocks */
	if ((mx > 10*mn) && (pe[chn] > 1000))  uselongblock[chn] = 0; 
      }
    }
    
    
    
    /*************************************************************** 
     * compute masking thresholds for both short and long blocks
     ***************************************************************/
    /* threshold calculation (part 2) */
    for ( sb = 0; sb < SBPSY_l; sb++ )
      {
	en[sb] = w1_l[sb] * eb[bu_l[sb]] + w2_l[sb] * eb[bo_l[sb]];
	thm[sb] = w1_l[sb] *thr[bu_l[sb]] + w2_l[sb] * thr[bo_l[sb]];
	for ( b = bu_l[sb]+1; b < bo_l[sb]; b++ )
	  {
	    en[sb]  += eb[b];
	    thm[sb] += thr[b];
	  }
	if ( en[sb] != 0.0 )
	  ratio[chn][sb] = thm[sb]/en[sb];
	else
	  ratio[chn][sb] = 0.0;
      }
#ifdef HAVEGTK
    if (gtkflag) {
      for (sb=0; sb< SBPSY_l; sb ++ ) {
	pinfo->thr[gr_out][chn][sb]=thm_save[chn][sb];
	pinfo->en[gr_out][chn][sb]=en_save[chn][sb];
	en_save[chn][sb]=en[sb];
      }
    }
#endif
    for (sb=0; sb< SBPSY_l; sb ++ ) {
      thm_save[chn][sb]=thm[sb];
    }
    
    /* threshold calculation for short blocks */
    for ( sblock = 0; sblock < 3; sblock++ )    {
      for ( b = 0; b < CBANDS; b++ )
	{
	  eb[b] = 0.0;
	  ecb[b] = 0.0;
	}
      for ( j = 0; j < HBLKSIZE_s; j++ ) {
	assert(partition_s[j] < npart_s_orig);
	if (partition_s[j]>=0) eb[partition_s[j]] += energy_s[sblock][j];
      }
      for ( b = 0; b < npart_s; b++ )
	for ( k = s3ind_s[b][0]; k <= s3ind_s[b][1]; k++ ) 
          ecb[b] += s3_s[b][k] * eb[k];
      
      
      for ( b = 0; b < npart_s; b++ )
	{
	  nb[b] = ecb[b] * norm_s[b] * SNR_s[b];
	  thr[b] = maximum (qthr_s[b],nb[b]);
	}
      
      for ( sb = 0; sb < SBPSY_s; sb++ )
	{
	  en[sb] = w1_s[sb] * eb[bu_s[sb]] + w2_s[sb] * eb[bo_s[sb]];
	  thm[sb] = w1_s[sb] *thr[bu_s[sb]] + w2_s[sb] * thr[bo_s[sb]];
	  for ( b = bu_s[sb]+1; b < bo_s[sb]; b++ )
	    {
	      en[sb] += eb[b];
	      thm[sb] += thr[b];
	    }
	  if ( en[sb] != 0.0 ) 
	    ratio_s[chn][sb][sblock] = thm[sb]/en[sb];
	  else
	    ratio_s[chn][sb][sblock] = 0.0;
#ifdef HAVEGTK
	  if (gtkflag) {
	    pinfo->thr_s[gr_out][chn][3*sb+sblock]=thm_s_save[chn][sb][sblock];
	    pinfo->en_s[gr_out][chn][3*sb+sblock]=en_s_save[chn][sb][sblock];
	    en_s_save[chn][sb][sblock]=en[sb];
	  }
#endif
	  thm_s_save[chn][sb][sblock]=thm[sb];
	}
      
    } 
    
    
    /* compute M/S thresholds from Johnston & Ferreira 1992 ICASSP paper */
#define JOHNSTON
#ifdef JOHNSTON
    if ( chn==3 /* the side channel */) {
      FLOAT8 rside,rmid,mld;
      int ch0=2,ch1=3; 
      
      for ( sb = 0; sb < SBPSY_l; sb++ ) {
	/* use this fix if L & R masking differs by 2db or less */
	/* if db = 10*log10(x2/x1) < 2 */
	/* if (x2 < 1.58*x1) { */
 	if (ratio[0][sb] <= 1.58*ratio[1][sb]
 	 && ratio[1][sb] <= 1.58*ratio[0][sb]) {
	  mld = mld_l[sb];
	  rmid = Max(ratio[ch0][sb],Min(ratio[ch1][sb],mld));
	  rside = Max(ratio[ch1][sb],Min(ratio[ch0][sb],mld));
	  ratio[ch0][sb]=rmid;
	  ratio[ch1][sb]=rside;
	}
      }
      for ( sblock = 0; sblock < 3; sblock++ ){
	for ( sb = 0; sb < SBPSY_s; sb++ ) {
 	  if (ratio_s[0][sb][sblock] <= 1.58*ratio_s[1][sb][sblock]
	      && ratio_s[1][sb][sblock] <= 1.58*ratio_s[0][sb][sblock]) {
	    mld = mld_s[sb];
	    rmid = Max(ratio_s[ch0][sb][sblock],Min(ratio_s[ch1][sb][sblock],mld));
	    rside = Max(ratio_s[ch1][sb][sblock],Min(ratio_s[ch0][sb][sblock],mld));
	    ratio_s[ch0][sb][sblock]=rmid;
	    ratio_s[ch1][sb][sblock]=rside;
	  }
	}
      }
    }
#endif
  } /* end loop over chn */
  
  
  
  if (check_ms_stereo)  {
    /* determin ms_ratio from masking thresholds*/
    /* use ms_stereo (ms_ratio < .35) if average thresh. diff < 5 db */
    { FLOAT8 db,x1,x2,sidetot=0,tot=0;
    for (sb= SBPSY_l/4 ; sb< SBPSY_l; sb ++ ) {
      x1 = minimum(thm_save[0][sb],thm_save[1][sb]);
      x2 = maximum(thm_save[0][sb],thm_save[1][sb]);
      /* thresholds difference in db */
      if (x2 >= 1000*x1)  db=30;
      else db = 10*log10(x2/x1);  
      /*  printf("db = %f %e %e  \n",db,thm_save[0][sb],thm_save[1][sb]);*/
      sidetot += db;
      tot++;
    }
    ms_ratio_l= .35*(sidetot/tot)/5.0;
    ms_ratio_l = Min(ms_ratio_l,.5);
    
    sidetot=0; tot=0;
    for ( sblock = 0; sblock < 3; sblock++ )
      for ( sb = SBPSY_s/4; sb < SBPSY_s; sb++ ) {
	x1 = minimum(thm_s_save[0][sb][sblock],thm_s_save[1][sb][sblock]);
	x2 = maximum(thm_s_save[0][sb][sblock],thm_s_save[1][sb][sblock]);
	/* thresholds difference in db */
	if (x2 >= 1000*x1)  db=30;
	else db = 10*log10(x2/x1);  
	sidetot += db;
	tot++;
      }
    ms_ratio_s = .35*(sidetot/tot)/5.0;
    ms_ratio_s = Min(ms_ratio_s,.5);
    }
  }
  
  
  /*************************************************************** 
   * determin final block type
   ***************************************************************/
  
  for (chn=0; chn<stereo; chn++) {
    blocktype[chn] = NORM_TYPE;
  }
  
  if (!allow_diff_short)
    if (info->mode==MPG_MD_JOINT_STEREO) {
      /* force both channels to use the same block type */
      /* this is necessary if the frame is to be encoded in ms_stereo.  */
      /* But even without ms_stereo, FhG  does this */
      int bothlong= (uselongblock[0] && uselongblock[1]);
      if (!bothlong) {
	uselongblock[0]=0;
	uselongblock[1]=0;
      }
    }
  
  
  
  
  /* update the blocktype of the previous granule, since it depends on what
   * happend in this granule */
  for (chn=0; chn<stereo; chn++) {
    if ( uselongblock[chn])
      {				/* no attack : use long blocks */
	switch( blocktype_old[chn] ) 
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
	if ( blocktype_old[chn] == NORM_TYPE ) {
	  blocktype_old[chn] = START_TYPE;
	}
	if ( blocktype_old[chn] == STOP_TYPE ) {
	  blocktype_old[chn] = SHORT_TYPE ;
	}
      }
    
    blocktype_d[chn] = blocktype_old[chn];  /* value returned to calling program */
    blocktype_old[chn] = blocktype[chn];    /* save for next call to l3psy_anal */
  }
  
  if (blocktype_d[0]==2) 
    *ms_ratio = ms_ratio_s_old;
  else
    *ms_ratio = ms_ratio_l_old;

  ms_ratio_s_old = ms_ratio_s;
  ms_ratio_l_old = ms_ratio_l;

  /* we dont know the block type of this frame yet - assume long */
  *ms_ratio_next = ms_ratio_l;

  
}






void L3para_read(FLOAT8 sfreq, int *numlines_l,int *numlines_s, int *partition_l, FLOAT8 *minval,
FLOAT8 *qthr_l, FLOAT8 *norm_l, FLOAT8 (*s3_l)[63], FLOAT8 s3_s[CBANDS][CBANDS],
int *partition_s, FLOAT8 *qthr_s, FLOAT8 *norm_s, FLOAT8 *SNR, 
int *bu_l, int *bo_l, FLOAT8 *w1_l, FLOAT8 *w2_l, 
int *bu_s, int *bo_s, FLOAT8 *w1_s, FLOAT8 *w2_s)
{
  FLOAT8 freq_tp;
  static FLOAT8 bval_l[CBANDS], bval_s[CBANDS];
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
	      minval[i] = *p++;
	      qthr_l[i] = *p++;
	      norm_l[i] = *p++;
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
  for(i=0;i<cbmax;i++) {
    for (j=0;(i != partition_l[j]);j++);
    { FLOAT8 ji = j + (numlines_l[i]-1)/2.0;
    FLOAT8 freq = sfreq*ji/1024000.0;
    FLOAT8 bark = 13*atan(.76*freq) + 3.5*atan(freq*freq/(7.5*7.5));
    printf("%i %i bval_l table=%f  f=%f  formaula=%f \n",i,j,bval_l[i],freq,bark);
    bval_l[i]=bark;
    }
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
	  /*tempx = (bval_l[i] - bval_l[j])*1.05;*/
	  if (j>=i) tempx = (bval_l[i] - bval_l[j])*3.0;
	  else    tempx = (bval_l[i] - bval_l[j])*1.5;
	  /*             if (j>=i) tempx = (bval_l[j] - bval_l[i])*3.0;
			 else    tempx = (bval_l[j] - bval_l[i])*1.5; */


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
	      qthr_s[i] = *p++;         
	      norm_s[i] = *p++;         
	      SNR[i] = *p++;            
	      bval_s[i] = *p++;
	      if (j!=i)
		{
		  fprintf(stderr,"3. please check \"psy_data\"");
		  exit(-1);
		}
	      for(k=0;k<numlines_s[i];k++) 
		partition_s[k2++] = i ;
	    }
	}
      else
	p += cbmax_tp * 6;
    }


#ifdef NEWBARK
  /* compute bark values of each critical band */
  for(i=0;i<cbmax;i++) {
    for (j=0;(i != partition_s[j]);j++);
    { FLOAT8 ji = j + (numlines_s[i]-1)/2.0;
    FLOAT8 freq = sfreq*ji/256000.0;
    FLOAT8 bark = 13*atan(.76*freq) + 3.5*atan(freq*freq/(7.5*7.5));
    printf("%i %i bval_s = %f  %f  %f \n",i,j,bval_s[i],freq,bark);
    bval_s[i]=bark;
    }
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
	  /* tempx = (bval_s[i] - bval_s[j])*1.05;*/
	  if (j>=i) tempx = (bval_s[i] - bval_s[j])*3.0;
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
