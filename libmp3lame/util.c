/*
 *	lame utility library source file
 *
 *	Copyright (c) 1999 Albert L Faber
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

/* $Id$ */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "util.h"
#include <ctype.h>
#include <assert.h>
#include <stdarg.h>

#if defined(__FreeBSD__) && !defined(__alpha__)
# include <machine/floatingpoint.h>
#endif

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

/***********************************************************************
*
*  Global Function Definitions
*
***********************************************************************/
/*empty and close mallocs in gfc */

void  freegfc ( lame_internal_flags* const gfc )   /* bit stream structure */
{
    int  i;

    for ( i = 0 ; i <= 2*BPC; i++ )
        if ( gfc->blackfilt[i] != NULL ) {
            free ( gfc->blackfilt[i] );
	    gfc->blackfilt[i] = NULL;
	}
    if ( gfc->inbuf_old[0] ) { 
        free ( gfc->inbuf_old[0] );
	gfc->inbuf_old[0] = NULL;
    }
    if ( gfc->inbuf_old[1] ) { 
        free ( gfc->inbuf_old[1] );
	gfc->inbuf_old[1] = NULL;
    }

    if ( gfc->VBR_seek_table.bag ) {
        free ( gfc->VBR_seek_table.bag );
        gfc->VBR_seek_table.bag=NULL;
        gfc->VBR_seek_table.size=0;
    }
    if ( gfc->s3_ll ) {
        free ( gfc->s3_ll );
    }
    if ( gfc->s3_ss ) {
        free ( gfc->s3_ss );
    }
}



int
BitrateIndex(
    int bRate,        /* legal rates from 32 to 448 kbps */
    int version       /* MPEG-1 or MPEG-2/2.5 LSF */
    )
{
    int  i;

    for ( i = 0; i <= 14; i++)
        if ( bitrate_table [version] [i] == bRate )
            return i;
	    
    return -1;
}

/* resampling via FIR filter, blackman window */
inline static FLOAT blackman(FLOAT x,FLOAT fcn,int l)
{
  /* This algorithm from:
SIGNAL PROCESSING ALGORITHMS IN FORTRAN AND C
S.D. Stearns and R.A. David, Prentice-Hall, 1992
  */
  FLOAT bkwn,x2;
  FLOAT wcn = (PI * fcn);
  
  x /= l;
  if (x<0) x=0;
  if (x>1) x=1;
  x2 = x - .5;

  bkwn = 0.42 - 0.5*cos(2*x*PI)  + 0.08*cos(4*x*PI);
  if (fabs(x2)<1e-9) return wcn/PI;
  else 
    return  (  bkwn*sin(l*wcn*x2)  / (PI*l*x2)  );


}

/* gcd - greatest common divisor */
/* Joint work of Euclid and M. Hendry */

int gcd ( int i, int j )
{
/*    assert ( i > 0  &&  j > 0 ); */
    return j ? gcd(j, i % j) : i;
}



/* copy in new samples from in_buffer into mfbuf, with resampling
   if necessary.  n_in = number of samples from the input buffer that
   were used.  n_out = number of samples copied into mfbuf  */

void fill_buffer(lame_global_flags *gfp,
		 sample_t *mfbuf[2],
		 sample_t *in_buffer[2],
		 int nsamples, int *n_in, int *n_out)
{
    lame_internal_flags *gfc = gfp->internal_flags;
    int ch,i;

    /* copy in new samples into mfbuf, with resampling if necessary */
    if (gfc->resample_ratio != 1.0) {
	for (ch = 0; ch < gfc->channels_out; ch++) {
	    *n_out =
		fill_buffer_resample(gfp, &mfbuf[ch][gfc->mf_size],
				     gfp->framesize, in_buffer[ch],
				     nsamples, n_in, ch);
	}
    }
    else {
	*n_out = Min(gfp->framesize, nsamples);
	*n_in = *n_out;
	for (i = 0; i < *n_out; ++i) {
	    mfbuf[0][gfc->mf_size + i] = in_buffer[0][i];
	    if (gfc->channels_out == 2)
		mfbuf[1][gfc->mf_size + i] = in_buffer[1][i];
	}
    }
}



int fill_buffer_resample(
       lame_global_flags *gfp,
       sample_t *outbuf,
       int desired_len,
       sample_t *inbuf,
       int len,
       int *num_used,
       int ch) 
{
  lame_internal_flags *gfc=gfp->internal_flags;
  int BLACKSIZE;
  FLOAT offset,xvalue;
  int i,j=0,k;
  int filter_l;
  FLOAT fcn,intratio;
  FLOAT *inbuf_old;
  int bpc;   /* number of convolution functions to pre-compute */
  bpc = gfp->out_samplerate/gcd(gfp->out_samplerate,gfp->in_samplerate);
  if (bpc>BPC) bpc = BPC;

  intratio=( fabs(gfc->resample_ratio - floor(.5+gfc->resample_ratio)) < .0001 );
  fcn = 1.00/gfc->resample_ratio;
  if (fcn>1.00) fcn=1.00;
  filter_l = gfp->quality < 7 ? 31 : 7;
  filter_l = 31;
  if (0==filter_l % 2 ) --filter_l;/* must be odd */
  filter_l += intratio;            /* unless resample_ratio=int, it must be even */


  BLACKSIZE = filter_l+1;  /* size of data needed for FIR */
  
  if ( gfc->fill_buffer_resample_init == 0 ) {
    gfc->inbuf_old[0]=calloc(BLACKSIZE,sizeof(gfc->inbuf_old[0][0]));
    gfc->inbuf_old[1]=calloc(BLACKSIZE,sizeof(gfc->inbuf_old[0][0]));
    for (i=0; i<=2*bpc; ++i)
      gfc->blackfilt[i]=calloc(BLACKSIZE,sizeof(gfc->blackfilt[0][0]));

    gfc->itime[0]=0;
    gfc->itime[1]=0;

    /* precompute blackman filter coefficients */
    for ( j = 0; j <= 2*bpc; j++ ) {
        FLOAT sum = 0.; 
        offset = (j-bpc) / (2.*bpc);
        for ( i = 0; i <= filter_l; i++ ) 
            sum += 
	    gfc->blackfilt[j][i]  = blackman(i-offset,fcn,filter_l);
	for ( i = 0; i <= filter_l; i++ ) 
	  gfc->blackfilt[j][i] /= sum;
    }
    gfc->fill_buffer_resample_init = 1;
  }
  
  inbuf_old=gfc->inbuf_old[ch];

  /* time of j'th element in inbuf = itime + j/ifreq; */
  /* time of k'th element in outbuf   =  j/ofreq */
  for (k=0;k<desired_len;k++) {
    FLOAT time0;
    int joff;

    time0 = k*gfc->resample_ratio;       /* time of k'th output sample */
    j = floor( time0 -gfc->itime[ch]  );

    /* check if we need more input data */
    if ((filter_l + j - filter_l/2) >= len) break;

    /* blackman filter.  by default, window centered at j+.5(filter_l%2) */
    /* but we want a window centered at time0.   */
    offset = ( time0 -gfc->itime[ch] - (j + .5*(filter_l%2)));
    assert(fabs(offset)<=.501);

    /* find the closest precomputed window for this offset: */
    joff = floor((offset*2*bpc) + bpc +.5);

    xvalue = 0.;
    for (i=0 ; i<=filter_l ; ++i) {
      int j2 = i+j-filter_l/2;
      int y;
      assert(j2<len);
      assert(j2+BLACKSIZE >= 0);
      y = (j2<0) ? inbuf_old[BLACKSIZE+j2] : inbuf[j2];
      xvalue += y*gfc->blackfilt[joff][i];
    }
    outbuf[k]=xvalue;
  }

  /* k = number of samples added to outbuf */
  /* last k sample used data from [j-filter_l/2,j+filter_l-filter_l/2]  */

  /* how many samples of input data were used:  */
  *num_used = Min(len,filter_l+j-filter_l/2);

  /* adjust our input time counter.  Incriment by the number of samples used,
   * then normalize so that next output sample is at time 0, next
   * input buffer is at time itime[ch] */
  gfc->itime[ch] += *num_used - k*gfc->resample_ratio;

  /* save the last BLACKSIZE samples into the inbuf_old buffer */
  if (*num_used >= BLACKSIZE) {
      for (i=0;i<BLACKSIZE;i++)
	  inbuf_old[i]=inbuf[*num_used + i -BLACKSIZE];
  }else{
      /* shift in *num_used samples into inbuf_old  */
       int n_shift = BLACKSIZE-*num_used;  /* number of samples to shift */

       /* shift n_shift samples by *num_used, to make room for the
	* num_used new samples */
       for (i=0; i<n_shift; ++i ) 
	   inbuf_old[i] = inbuf_old[i+ *num_used];

       /* shift in the *num_used samples */
       for (j=0; i<BLACKSIZE; ++i, ++j ) 
	   inbuf_old[i] = inbuf[j];

       assert(j==*num_used);
  }
  return k;  /* return the number samples created at the new samplerate */
}





/***********************************************************************
*
*  Message Output
*
***********************************************************************/
void  lame_debugf (const lame_internal_flags *gfc, const char* format, ... )
{
    va_list  args;

    va_start ( args, format );

    if ( gfc->report.debugf ) {
        gfc->report.debugf( format, args );
    } else {
        (void) vfprintf ( stderr, format, args );
        fflush ( stderr );      /* an debug function should flush immediately */
    }

    va_end   ( args );
}


void  lame_msgf (const lame_internal_flags *gfc, const char* format, ... )
{
    va_list  args;

    va_start ( args, format );
   
    if ( gfc->report.msgf ) {
        gfc->report.msgf( format, args );
    } else {
        (void) vfprintf ( stderr, format, args );
        fflush ( stderr );     /* we print to stderr, so me may want to flush */
    }

    va_end   ( args );
}


void  lame_errorf (const lame_internal_flags *gfc, const char* format, ... )
{
    va_list  args;

    va_start ( args, format );
    
    if ( gfc->report.errorf ) {
        gfc->report.errorf( format, args );
    } else {
        (void) vfprintf ( stderr, format, args );
        fflush   ( stderr );    /* an error function should flush immediately */
    }

    va_end   ( args );
}



/***********************************************************************
 *
 *      routines to detect CPU specific features like 3DNow, MMX, SSE
 *
 *  donated by Frank Klemm
 *  added Robert Hegemann 2000-10-10
 *
 ***********************************************************************/

int  has_i387 ( void )
{
#ifdef HAVE_NASM 
    return 1;
#else
    return 0;   /* don't know, assume not */
#endif
}    

int  has_MMX ( void )
{
#ifdef HAVE_NASM 
    extern int has_MMX_nasm ( void );
    return has_MMX_nasm ();
#else
    return 0;   /* don't know, assume not */
#endif
}    

int  has_3DNow ( void )
{
#ifdef HAVE_NASM 
    extern int has_3DNow_nasm ( void );
    return has_3DNow_nasm ();
#else
    return 0;   /* don't know, assume not */
#endif
}    

int  has_E3DNow ( void )
{
#ifdef HAVE_NASM 
    extern int has_E3DNow_nasm ( void );
    return has_E3DNow_nasm ();
#else
    return 0;   /* don't know, assume not */
#endif
}    

int  has_SSE ( void )
{
#ifdef HAVE_NASM 
    extern int has_SSE_nasm ( void );
    return has_SSE_nasm ();
#else
    return 0;   /* don't know, assume not */
#endif
}    

int  has_SSE2 ( void )
{
#ifdef HAVE_NASM 
    extern int has_SSE2_nasm ( void );
    return has_SSE2_nasm ();
#else
    return 0;   /* don't know, assume not */
#endif
}    

void disable_FPE(void) {
/* extremly system dependent stuff, move to a lib to make the code readable */
/*==========================================================================*/



    /*
     *  Disable floating point exceptions
     */




#if defined(__FreeBSD__) && !defined(__alpha__)
    {
        /* seet floating point mask to the Linux default */
        fp_except_t mask;
        mask = fpgetmask();
        /* if bit is set, we get SIGFPE on that error! */
        fpsetmask(mask & ~(FP_X_INV | FP_X_DZ));
        /*  DEBUGF("FreeBSD mask is 0x%x\n",mask); */
    }
#endif

#if defined(__riscos__) && !defined(ABORTFP)
    /* Disable FPE's under RISC OS */
    /* if bit is set, we disable trapping that error! */
    /*   _FPE_IVO : invalid operation */
    /*   _FPE_DVZ : divide by zero */
    /*   _FPE_OFL : overflow */
    /*   _FPE_UFL : underflow */
    /*   _FPE_INX : inexact */
    DisableFPETraps(_FPE_IVO | _FPE_DVZ | _FPE_OFL);
#endif

    /*
     *  Debugging stuff
     *  The default is to ignore FPE's, unless compiled with -DABORTFP
     *  so add code below to ENABLE FPE's.
     */

#if defined(ABORTFP)
#if defined(_MSC_VER)
    {

   /* set affinity to a single CPU.  Fix for EAC/lame on SMP systems from
     "Todd Richmond" <todd.richmond@openwave.com> */
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    SetProcessAffinityMask(GetCurrentProcess(), si.dwActiveProcessorMask);

#include <float.h>
        unsigned int mask;
        mask = _controlfp(0, 0);
        mask &= ~(_EM_OVERFLOW | _EM_UNDERFLOW | _EM_ZERODIVIDE | _EM_INVALID);
        mask = _controlfp(mask, _MCW_EM);
    }
#elif defined(__CYGWIN__)
#  define _FPU_GETCW(cw) __asm__ ("fnstcw %0" : "=m" (*&cw))
#  define _FPU_SETCW(cw) __asm__ ("fldcw %0" : : "m" (*&cw))

#  define _EM_INEXACT     0x00000020 /* inexact (precision) */
#  define _EM_UNDERFLOW   0x00000010 /* underflow */
#  define _EM_OVERFLOW    0x00000008 /* overflow */
#  define _EM_ZERODIVIDE  0x00000004 /* zero divide */
#  define _EM_INVALID     0x00000001 /* invalid */
    {
        unsigned int mask;
        _FPU_GETCW(mask);
        /* Set the FPU control word to abort on most FPEs */
        mask &= ~(_EM_OVERFLOW | _EM_ZERODIVIDE | _EM_INVALID);
        _FPU_SETCW(mask);
    }
# elif defined(__linux__)
    {

#  include <fpu_control.h>
#  ifndef _FPU_GETCW
#  define _FPU_GETCW(cw) __asm__ ("fnstcw %0" : "=m" (*&cw))
#  endif
#  ifndef _FPU_SETCW
#  define _FPU_SETCW(cw) __asm__ ("fldcw %0" : : "m" (*&cw))
#  endif

        /* 
         * Set the Linux mask to abort on most FPE's
         * if bit is set, we _mask_ SIGFPE on that error!
         *  mask &= ~( _FPU_MASK_IM | _FPU_MASK_ZM | _FPU_MASK_OM | _FPU_MASK_UM );
         */

        unsigned int mask;
        _FPU_GETCW(mask);
        mask &= ~(_FPU_MASK_IM | _FPU_MASK_ZM | _FPU_MASK_OM);
        _FPU_SETCW(mask);
    }
#endif
#endif /* ABORTFP */
}





/* end of util.c */
