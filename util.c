
#include "util.h"
#include <ctype.h>
#include <assert.h>

#ifdef LAME_STD_PRINT
#include <stdarg.h>
#endif

/***********************************************************************
*
*  Global Variable Definitions
*
***********************************************************************/


/* 1995-07-11 shn */
int  index_to_bitrate [2] [16] = {
    // 0   1   2   3   4   5   6   7   8   9  10  11  12  13  14
    {  0,  8, 16, 24, 32, 40, 48, 56, 64, 80, 96,112,128,144,160 }, // MPEG-2/2.5, LSF < 32 kHz
    {  0, 32, 40, 48, 56, 64, 80, 96,112,128,160,192,224,256,320 }  // MPEG-1,        >= 32 kHz
};

unsigned char bitrate_to_index [2] [81] = {
    //0     8    16    24    32    40    48    56    64    72    80    88    96   104   112   120   128   136   144   152   160   168   176   184   192   200   208   216   224   232   240   248   256   264   272   280   288   296   304   312   320   
    //   4    12    20    28    36    44    52    60    68    76    84    92   100   108   116   124   132   140   148   156   164   172   180   188   196   204   212   220   228   236   244   252   260   268   276   284   292   300   308   316   
    { 1, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 8, 9, 9, 9, 9,10,10,10,10,11,11,11,11,12,12,12,12,13,13,13,13,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14 },
    { 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9,10,10,10,10,10,10,10,10,11,11,11,11,11,11,11,11,12,12,12,12,12,12,12,12,13,13,13,13,13,13,13,13,13,13,13,13,14,14,14,14,14,14,14,14,14 }
};

enum byte_order NativeByteOrder = order_unknown;

/***********************************************************************
*
*  Global Function Definitions
*
***********************************************************************/


/* Replacement for forward fseek(,,SEEK_CUR), because fseek() fails on pipes */
int fskip ( FILE* fp, off_t bytes, int whence )
{
    char    data [4096];
    size_t  skip;
  
    assert ( whence == SEEK_CUR );
    assert ( bytes >= 0 );
  
    while ( bytes > 0 ) {
        skip   = bytes > sizeof(data)  ?  sizeof(data)  :  bytes;
        skip   = fread ( data, 1, (size_t)skip, fp );
	if ( skip == 0 ) {
	    fprintf ( stderr, "Out of data in fskip().\n" );
	    return -1;
	}
	bytes -= skip;
    }
    return 0;
}


FLOAT8 ATHformula(FLOAT8 f)
{
  FLOAT8 ath;
  f  = Max(0.01, f);
  f  = Min(18.0,f);

  /* from Painter & Spanias, 1997 */
  /* minimum: (i=77) 3.3kHz = -5db */
  ath =    3.640 * pow(f,-0.8)
         - 6.500 * exp(-0.6*pow(f-3.3,2.0))
         + 0.001 * pow(f,4.0);
  return ath;
}


/* see for example "Zwicker: Psychoakustik, 1982; ISBN 3-540-11401-7 */
FLOAT8 freq2bark(FLOAT8 freq)
{
  /* input: freq in hz  output: barks */
    freq = freq * 0.001;
    return 13.0*atan(.76*freq) + 3.5*atan(freq*freq/(7.5*7.5));
}

/* see for example "Zwicker: Psychoakustik, 1982; ISBN 3-540-11401-7 */
FLOAT8 freq2cbw(FLOAT8 freq)
{
  /* input: freq in hz  output: critical band width */
    freq = freq * 0.001;
    return 25+75*pow(1+1.4*(freq*freq),0.69);
}






/***********************************************************************
 * compute bitsperframe and mean_bits for a layer III frame 
 **********************************************************************/
void getframebits(lame_global_flags *gfp,int *bitsPerFrame, int *mean_bits) {
  int whole_SpF;
  lame_internal_flags *gfc=gfp->internal_flags;

  long bit_rate;
  
  if (gfc->bitrate_index) 
    bit_rate = index_to_bitrate[gfp->version][gfc->bitrate_index];
  else
    bit_rate = gfp->brate;
  
  whole_SpF=((gfp->version+1)*72000L*bit_rate) / gfp->out_samplerate;
  *bitsPerFrame = 8 * (whole_SpF + gfc->padding);
  *mean_bits = (*bitsPerFrame - 8*gfc->sideinfo_len) / gfc->mode_gr;

}




void   display_bitrates ( FILE* fp )
{
    int   i;

    fprintf ( fp, "\nMPEG-1   layer III sample rates (kHz): 32  44.1   48\n");
    fprintf ( fp, "bit rates (kbps):");
    for ( i = 1; i <= 14; i++ )
        fprintf ( fp, "%4u", index_to_bitrate [MPEG_1  ] [i] );
    fprintf ( fp, "\n" );

    fprintf ( fp, "\nMPEG-2   layer III sample rates (kHz): 16  22.05  24\n");
    fprintf ( fp, "bit rates (kbps):");
    for ( i = 1; i <= 14; i++ )
        fprintf ( fp, "%4u", index_to_bitrate [MPEG_2  ] [i] );
    fprintf ( fp, "\n" );
  
    fprintf ( fp, "\nMPEG-2.5 layer III sample rates (kHz):  8  11.025 16\n");
    fprintf ( fp, "bit rates (kbps):");
    for ( i = 1; i <= 14; i++ )
        fprintf ( fp, "%4u", index_to_bitrate [MPEG_2_5] [i] );
    fprintf ( fp, "\n" );
}

/* convert bitrate in kbps to index */

int FindNearestBitrateIndex ( 
		unsigned       bRate,        /* legal rates from 32 to 448 */
		unsigned       version,      /* MPEG-1 or MPEG-2 LSF */
                unsigned long  samplerate )  /* unused */  
{
    assert (version < 2);
    return bitrate_to_index [version] [ bRate <= 320 ? bRate >> 2 : 80 ];
}

/* round bitrate in kbps to nearest valid value for non freeformat */

unsigned FindNearestBitrate ( 
		unsigned       bRate,        /* legal rates from 32 to 448 */
		unsigned       version,      /* MPEG-1 or MPEG-2 LSF */
                unsigned long  samplerate )  /* unused */  
{
    assert (version < 2);
    return index_to_bitrate [version] 
                            [bitrate_to_index [version] [ bRate <= 320 ? bRate >> 2 : 80 ]];
}


/* map samplerate to a valid samplerate
 *
 * Robert.Hegemann@gmx.de 2000-07-01
 */

long RoundSampleRateToNearest ( long samplerate )         /* This functions rounds to nearest sample frequency */
{
    if (samplerate <=  9500) return  8000;
    if (samplerate <= 11500) return 11025;
    if (samplerate <= 14000) return 12000;
    if (samplerate <= 19000) return 16000;
    if (samplerate <= 23000) return 22050;
    if (samplerate <= 28000) return 24000;
    if (samplerate <= 38000) return 32000;
    if (samplerate <= 46000) return 44100;
    return 48000;
}

long RoundSampleRateUp ( long samplerate )         /* This functions rounds up sample frequency with a margin of 3% */
{
    if (samplerate <=  8240) return  8000;
    if (samplerate <= 11355) return 11025;
    if (samplerate <= 12360) return 12000;
    if (samplerate <= 16480) return 16000;
    if (samplerate <= 22710) return 22050;
    if (samplerate <= 24720) return 24000;
    if (samplerate <= 32960) return 32000;
    if (samplerate <= 45420) return 44100;
    return 48000;
}

int BitrateIndex (
		unsigned       bRate,        /* legal rates from 32 to 448 */
		unsigned       version,      /* MPEG-1 or MPEG-2 LSF */
		unsigned long  samplerate)   /* convert bitrate in kbps to index */
{
    int* t = index_to_bitrate [version];

    if ( bRate == t [ 0] ) { ERRORF ("Bitrate %d kbps not legal for %i Hz output sampling.\n", bRate, samplerate );
                             return 0;
			   }
    if ( bRate == t [ 1] ) return  1;
    if ( bRate == t [ 2] ) return  2;
    if ( bRate == t [ 3] ) return  3;
    if ( bRate == t [ 4] ) return  4;
    if ( bRate == t [ 5] ) return  5;
    if ( bRate == t [ 6] ) return  6;
    if ( bRate == t [ 7] ) return  7;
    if ( bRate == t [ 8] ) return  8;
    if ( bRate == t [ 9] ) return  9;
    if ( bRate == t [10] ) return 10;
    if ( bRate == t [11] ) return 11;
    if ( bRate == t [12] ) return 12;
    if ( bRate == t [13] ) return 13;
    if ( bRate == t [14] ) return 14;

    ERRORF ("Bitrate %d kbps not legal for %i Hz output sampling.\n", bRate, samplerate );
    return -1;
}

/* convert Sample Frequency in Hz to MPEG version and index */

int SmpFrqIndex ( long SampleFreq, MPEG_version_t* MPEGversion )
{
    switch ( SampleFreq ) {
    case 44100: *MPEGversion = 1; return  0;
    case 48000: *MPEGversion = 1; return  1;
    case 32000: *MPEGversion = 1; return  2;
    case 11025:
    case 22050: *MPEGversion = 0; return  0;
    case 12000:
    case 24000: *MPEGversion = 0; return  1;
    case  8000:
    case 16000: *MPEGversion = 0; return  2;
    default   : ERRORF ("SmpFrqIndex: %ld Hz is not a legal sample frequency\n", SampleFreq );
	        *MPEGversion = 1; return -1;
    }
}




/*****************************************************************************
*
*  Routines to determine byte order and swap bytes
*
*****************************************************************************/

enum byte_order DetermineByteOrder(void)
{
    char s[ sizeof(long) + 1 ];
    union
    {
        long longval;
        char charval[ sizeof(long) ];
    } probe;
    probe.longval = 0x41424344L;  /* ABCD in ASCII */
    strncpy( s, probe.charval, sizeof(long) );
    s[ sizeof(long) ] = '\0';
    /* fprintf( stderr, "byte order is %s\n", s ); */
    if ( strcmp(s, "ABCD") == 0 )
        return order_bigEndian;
    else
        if ( strcmp(s, "DCBA") == 0 )
            return order_littleEndian;
        else
            return order_unknown;
}


#if 0
void SwapBytesInWords( short *loc, size_t words )
{
    int i;
    short thisval;
    char *dst, *src;
    src = (char *) &thisval;
    for ( i = 0; i < words; i++ )
    {
        thisval = *loc;
        dst = (char *) loc++;
        dst[0] = src[1];
        dst[1] = src[0];
    }
}
#else
void SwapBytesInWords ( short* loc, size_t words )
{
    unsigned long*  p = (unsigned long*) loc;
    
    for ( ; words >= 2; words -= 2, p++ )
        *p = ((*p << 8) & 0xFF00FF00) |
             ((*p >> 8) & 0x00FF00FF);
    
    if ( words ) {
        loc  = (short*) p;    
        *loc = ((*loc << 8) & 0xFF00) |
               ((*loc >> 8) & 0x00FF);
    }
}
#endif


/*****************************************************************************
*
*  bit_stream.c package
*  Author:  Jean-Georges Fritsch, C-Cube Microsystems
*
*****************************************************************************/

/********************************************************************
  This package provides functions to write (exclusive or read)
  information from (exclusive or to) the bit stream.

  If the bit stream is opened in read mode only the get functions are
  available. If the bit stream is opened in write mode only the put
  functions are available.
********************************************************************/

/*alloc_buffer();      open and initialize the buffer;                    */
/*desalloc_buffer();   empty and close the buffer                         */
/*back_track_buffer();     goes back N bits in the buffer                 */
/*unsigned int get1bit();  read 1 bit from the bit stream                 */
/*unsigned long look_ahead(); grep the next N bits in the bit stream without*/
/*                            changing the buffer pointer                   */
/*putbits(); write N bits from the bit stream */
/*int seek_sync(); return 1 if a sync word was found in the bit stream      */
/*                 otherwise returns 0                                      */



void empty_buffer(Bit_stream_struc *bs)
{
  /* brain damaged ISO buffer type - counts down */
  int minimum=1+bs->buf_byte_idx;    /* end of the buffer to empty */
  if (bs->buf_size-minimum <= 0) return;
  bs->buf_byte_idx = bs->buf_size -1;
  bs->buf_bit_idx = 8;
  
  bs->buf[bs->buf_byte_idx] = 0;  /* what does this do? */
}


int copy_buffer(char *buffer,int size,Bit_stream_struc *bs)
{
  int minimum = bs->buf_byte_idx + 1;
  if (minimum <= 0) return 0;
  if (size!=0 && minimum>size) return -1; /* buffer is too small */
  memcpy(buffer,bs->buf,(unsigned int)minimum);
  bs->buf_byte_idx = -1;
  bs->buf_bit_idx = 0;
  return minimum;
}





void init_bit_stream_w(lame_internal_flags *gfc)
{
  Bit_stream_struc* bs = &gfc->bs;
   alloc_buffer(bs, BUFFER_SIZE);
   gfc->h_ptr = gfc->w_ptr = 0;
   gfc->header[gfc->h_ptr].write_timing = 0;
   gfc->bs.buf_byte_idx = -1;
   gfc->bs.buf_bit_idx = 0;
   gfc->bs.totbit = 0;
}


/* open and initialize the buffer */
void  alloc_buffer (
		Bit_stream_struc*  bs,   /* bit stream structure */
		size_t             size)
{
    bs -> buf_size = size;
    bs -> buf      = (unsigned char*) malloc (size);
}

/*empty and close mallocs in gfc */
void freegfc(lame_internal_flags *gfc)   /* bit stream structure */
{
   free(gfc->bs.buf);
   free(gfc);
}

int putmask[9]={0x0, 0x1, 0x3, 0x7, 0xf, 0x1f, 0x3f, 0x7f, 0xff};





/*****************************************************************************
*
*  End of bit_stream.c package
*
*****************************************************************************/

/* reorder the three short blocks By Takehiro TOMINAGA */
/*
  Within each scalefactor band, data is given for successive
  time windows, beginning with window 0 and ending with window 2.
  Within each window, the quantized values are then arranged in
  order of increasing frequency...
*/
void ireorder(int scalefac_band[],int ix_orig[576]) {
  int i,sfb, window, j=0;
  int ix[576];
  for (sfb = 0; sfb < SBMAX_s; sfb++) {
    int start = scalefac_band[sfb];
    int end   = scalefac_band[sfb + 1];
    for (window = 0; window < 3; window++) {
      for (i = start; i < end; ++i) {
	ix[j++] = ix_orig[3*i+window];
      }
    }
  }
  memcpy(ix_orig,ix,576*sizeof(int));
}
void iun_reorder(int scalefac_band[],int ix_orig[576]) {
  int i,sfb, window, j=0;
  int ix[576];
  for (sfb = 0; sfb < SBMAX_s; sfb++) {
    int start = scalefac_band[sfb];
    int end   = scalefac_band[sfb + 1];
    for (window = 0; window < 3; window++) {
      for (i = start; i < end; ++i) {
	ix[3*i+window] = ix_orig[j++];
      }
    }
  }
  memcpy(ix_orig,ix,576*sizeof(int));
}
void freorder(int scalefac_band[],FLOAT8 ix_orig[576]) {
  int i,sfb, window, j=0;
  FLOAT8 ix[576];
  for (sfb = 0; sfb < SBMAX_s; sfb++) {
    int start = scalefac_band[sfb];
    int end   = scalefac_band[sfb + 1];
    for (window = 0; window < 3; window++) {
      for (i = start; i < end; ++i) {
	ix[j++] = ix_orig[3*i+window];
      }
    }
  }
  memcpy(ix_orig,ix,576*sizeof(FLOAT8));
}
void fun_reorder(int scalefac_band[],FLOAT8 ix_orig[576]) {
  int i,sfb, window, j=0;
  FLOAT8 ix[576];
  for (sfb = 0; sfb < SBMAX_s; sfb++) {
    int start = scalefac_band[sfb];
    int end   = scalefac_band[sfb + 1];
    for (window = 0; window < 3; window++) {
      for (i = start; i < end; ++i) {
	ix[3*i+window] = ix_orig[j++];
      }
    }
  }
  memcpy(ix_orig,ix,576*sizeof(FLOAT8));
}










/* resampling via FIR filter, blackman window */


/* calculates window function with an carrier of (-1,+1) */

INLINE double blackman_slow (double x)
{
   if ( fabs (x) >= 1. )
       return 0.;

   x *= PI;
   return 0.42 + 0.5 * cos(x) + 0.08 * cos(2*x); // original formula
}

INLINE double blackman ( double x )
{
    if ( fabs (x) >= 1)
        return 0.;

    x = cos (x * PI);

    // using addition theorem of arc functions
    return (0.16 * x + 0.5) * x + 0.34;
}


/* calculates sinc function, zero crosses are at ..., -2, -1, +1, +2, ... */

INLINE double sinc ( double x )
{
    if ( x == 0. )
        return 1.;

    if ( x <  0. )
        x = -x;

    // mirroring argument of sin to [-pi/2,+pi/2), so zero crosses are really exact zero
    return sin ( (x - (int) ( x + 0.5 )) * PI ) / ( x * PI );
}

/* Can someone rewrite this function using blackman() and sinc() and a short comment? */

INLINE double snafucate (int i,double offset,double fcn,int l)
{
  double bkwn;
  double wcn = (PI * fcn);
  double dly = l / 2.0;
  double x = i-offset;
  if (x<0) x=0;
  if (x>l) x=l;
  bkwn = 0.42 - 0.5 * cos((x * 2) * PI / l) + 0.08 * cos((x * 4) * PI / l);
  if (fabs(x-dly)<1.045124723174582173568712e-9) return wcn/PI;
  else 
    return  (sin( (wcn *  ( x - dly))) / (PI * ( x - dly)) * bkwn );
}

int fill_buffer_downsample(lame_global_flags *gfp,sample_t *outbuf,int desired_len,
			 sample_t *inbuf,int len,int *num_used,int ch) {
  
  lame_internal_flags *gfc=gfp->internal_flags;
  FLOAT8 offset,xvalue;
  int i,j=0,k,value;
  int filter_l;
  FLOAT8 fcn,intratio;
  sample_t *inbuf_old;

  intratio=( fabs(gfc->resample_ratio - floor(.5+gfc->resample_ratio)) < 1.e-4 );
  fcn = .90/gfc->resample_ratio;
  if (fcn>.90) fcn=.90;
  filter_l=19;  /* must be odd */
  /* if resample_ratio = int, filter_l should be even */
  filter_l += intratio;
  assert(filter_l +5 < BLACKSIZE);
  
  if (!gfc->fill_buffer_downsample_init) {
    gfc->fill_buffer_downsample_init=1;
    gfc->itime[0]=0;
    gfc->itime[1]=0;
    memset((char *) gfc->inbuf_old, 0, sizeof(sample_t)*2*BLACKSIZE);
    /* precompute blackman filter coefficients */
    for (j= 0; j<= 2*BPC; ++j) {
      offset=(double)(j-BPC)/(double)(2*BPC);
      for (i=0; i<=filter_l; ++i) {
	gfc->blackfilt[j][i]=snafucate(i,offset,fcn,filter_l);
      }
    }

  }
  inbuf_old=gfc->inbuf_old[ch];

  /* time of j'th element in inbuf = itime + j/ifreq; */
  /* time of k'th element in outbuf   =  j/ofreq */
  for (k=0;k<desired_len;k++) {
    FLOAT8 time0;
    int joff;

    time0 = k*gfc->resample_ratio;       /* time of k'th output sample */
    j = floor( time0 -gfc->itime[ch]  );
    if ((j+filter_l/2) >= len) break;

    /* blackmon filter.  by default, window centered at j+.5(filter_l%2) */
    /* but we want a window centered at time0.   */
    offset = ( time0 -gfc->itime[ch] - (j + .5*(filter_l%2)));
    assert(fabs(offset)<=.500001);
    joff = floor((offset*2*BPC) + BPC +.5);

    xvalue=0;
    for (i=0 ; i<=filter_l ; ++i) {
      int j2 = i+j-filter_l/2;
      int y;
      y = (j2<0) ? inbuf_old[BLACKSIZE+j2] : inbuf[j2];
#define PRECOMPUTE
#ifdef PRECOMPUTE
      xvalue += y*gfc->blackfilt[joff][i];
#else
      if (intratio) 
	xvalue += y*gfc->blackfilt[joff][i];
      else
	xvalue += y*snafucate(i,offset,fcn,filter_l);  /* very slow! */
#endif
    }
    value = floor(.5+xvalue);
    if (value > 32767) outbuf[k]=32767;
    else if (value < -32767) outbuf[k]=-32767;
    else outbuf[k]=value;
  }

  
  /* k = number of samples added to outbuf */
  /* last k sample used data from j,j+1, or j+1 overflowed buffer */
  /* remove num_used samples from inbuf: */
  *num_used = Min(len,j+filter_l/2);
  gfc->itime[ch] += *num_used - k*gfc->resample_ratio;
  for (i=0;i<BLACKSIZE;i++)
    inbuf_old[i]=inbuf[*num_used + i -BLACKSIZE];
  return k;
}


/* 4 point interpolation routine for upsampling */
int fill_buffer_upsample(lame_global_flags *gfp,sample_t *outbuf,int desired_len,
        sample_t *inbuf,int len,int *num_used,int ch) {

  int i,j=0,k,linear,value;
  lame_internal_flags *gfc=gfp->internal_flags;
  sample_t* inbuf_old;

  if (!gfc->fill_buffer_upsample_init) {
    gfc->fill_buffer_upsample_init=1;
    gfc->upsample_itime[0]=0;
    gfc->upsample_itime[1]=0;
    memset((char *) gfc->upsample_inbuf_old, 0, sizeof(sample_t)*2*OLDBUFSIZE);
  }


  inbuf_old=gfc->upsample_inbuf_old[ch];

  /* if downsampling by an integer multiple, use linear resampling,
   * otherwise use quadratic */
  linear = ( fabs(gfc->resample_ratio - floor(.5+gfc->resample_ratio)) < 1.e-4 );

  /* time of j'th element in inbuf = itime + j/ifreq; */
  /* time of k'th element in outbuf   =  j/ofreq */
  for (k=0;k<desired_len;k++) {
    int y0,y1,y2,y3;
    FLOAT8 x0,x1,x2,x3;
    FLOAT8 time0;

    time0 = k*gfc->resample_ratio;       /* time of k'th output sample */
    j = floor( time0 -gfc->upsample_itime[ch]  );
    /* itime[ch] + j;    */            /* time of j'th input sample */
    if (j+2 >= len) break;             /* not enough data in input buffer */

    x1 = time0-(gfc->upsample_itime[ch]+j);
    x2 = x1-1;
    y1 = (j<0) ? inbuf_old[OLDBUFSIZE+j] : inbuf[j];
    y2 = ((1+j)<0) ? inbuf_old[OLDBUFSIZE+1+j] : inbuf[1+j];

    /* linear resample */
    if (linear) {
      outbuf[k] = floor(.5 +  (y2*x1-y1*x2) );
    } else {
      /* quadratic */
      x0 = x1+1;
      x3 = x1-2;
      y0 = ((j-1)<0) ? inbuf_old[OLDBUFSIZE+(j-1)] : inbuf[j-1];
      y3 = ((j+2)<0) ? inbuf_old[OLDBUFSIZE+(j+2)] : inbuf[j+2];
      value = floor(.5 +
			-y0*x1*x2*x3/6 + y1*x0*x2*x3/2 - y2*x0*x1*x3/2 +y3*x0*x1*x2/6
			);
      if (value > 32767) outbuf[k]=32767;
      else if (value < -32767) outbuf[k]=-32767;
      else outbuf[k]=value;

      /*
      printf("k=%i  new=%i   [ %i %i %i %i ]\n",k,outbuf[k],
	     y0,y1,y2,y3);
      */
    }
  }


  /* k = number of samples added to outbuf */
  /* last k sample used data from j,j+1, or j+1 overflowed buffer */
  /* remove num_used samples from inbuf: */
  *num_used = Min(len,j+2);
  gfc->upsample_itime[ch] += *num_used - k*gfc->resample_ratio;
  for (i=0;i<OLDBUFSIZE;i++)
    inbuf_old[i]=inbuf[*num_used + i -OLDBUFSIZE];
  return k;
}


/***********************************************************************
*
*  Message Output
*
***********************************************************************/

#ifdef LAME_STD_PRINT

void
lame_errorf(const char * s, ...)
{
  va_list args;
  va_start(args, s);
  vfprintf(stderr, s, args);
  va_end(args);
}

#endif

/* would use real "strcasecmp" but it isn't portable */
int local_strcasecmp ( const char* s1, const char* s2 )
{
    unsigned char c1;
    unsigned char c2;
    
    do {
        c1 = tolower(*s1);
        c2 = tolower(*s2);
        if (!c1) {
            break;
        }
        ++s1;
        ++s2;
    } while (c1 == c2);
    return c1 - c2;
}

