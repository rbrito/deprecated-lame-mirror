
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


/* 1: MPEG-1, 0: MPEG-2 LSF, 1995-07-11 shn */
const int  bitrate_table [2] [16] = {
          {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160},
          {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320}};


enum byte_order NativeByteOrder = order_unknown;

/***********************************************************************
*
*  Global Function Definitions
*
***********************************************************************/
/* Replacement for forward fseek(,,SEEK_CUR), because fseek() fails on pipes */
int fskip(FILE *sf,long num_bytes,int dummy)
{
  char data[1024];
  int nskip = 0;
  while (num_bytes > 0) {
    nskip = (num_bytes>1024) ? 1024 : num_bytes;
    num_bytes -= fread(data,(size_t)1,(size_t)nskip,sf);
  }
  /* return 0 if last read was successful */
  return num_bytes;
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
    bit_rate = bitrate_table[gfp->version][gfc->bitrate_index];
  else
    bit_rate = gfp->brate;
  
  whole_SpF=((gfp->version+1)*72000L*bit_rate) / gfp->out_samplerate;
  *bitsPerFrame = 8 * (whole_SpF + gfc->padding);
  *mean_bits = (*bitsPerFrame - 8*gfc->sideinfo_len) / gfc->mode_gr;

}




void display_bitrates(FILE *out_fh)
{
  int index,version;

  version = 1;
  fprintf(out_fh,"\n");
  fprintf(out_fh,"MPEG1 layer III samplerates(kHz): 32 44.1 48 \n");

  fprintf(out_fh,"bitrates(kbs): ");
  for (index=1;index<15;index++) {
    fprintf(out_fh,"%i ",bitrate_table[version][index]);
  }
  fprintf(out_fh,"\n");
  
  
  version = 0;
  fprintf(out_fh,"\n");
  fprintf(out_fh,"MPEG2 layer III samplerates(kHz): 16 22.05 24 \n");
  fprintf(out_fh,"bitrates(kbs): ");
  for (index=1;index<15;index++) {
    fprintf(out_fh,"%i ",bitrate_table[version][index]);
  }
  fprintf(out_fh,"\n");

  version = 0;
  fprintf(out_fh,"\n");
  fprintf(out_fh,"MPEG2.5 layer III samplerates(kHz): 8 11.025 12 \n");
  fprintf(out_fh,"bitrates(kbs): ");
  for (index=1;index<15;index++) {
    fprintf(out_fh,"%i ",bitrate_table[version][index]);
  }
  fprintf(out_fh,"\n");
}


#define ABS(A) (((A)>0) ? (A) : -(A))

int FindNearestBitrate(
int bRate,        /* legal rates from 32 to 448 */
int version,      /* MPEG-1 or MPEG-2 LSF */
int samplerate)   /* convert bitrate in kbps to index */
{
  int     index = 0;
  int     bitrate = 10000;
  
  while( index<15)   {
    if( ABS( bitrate_table[version][index] - bRate) < ABS(bitrate-bRate) ) {
      bitrate = bitrate_table[version][index];
    }
    ++index;
  }
  return bitrate;
}


/* map samplerate to a valid samplerate
 *
 * Robert.Hegemann@gmx.de 2000-07-01
 */
long validSamplerate(long samplerate)
{
    if (samplerate<= 8000) return  8000;
    if (samplerate<=11025) return 11025;
    if (samplerate<=12000) return 12000;
    if (samplerate<=16000) return 16000;
    if (samplerate<=22050) return 22050;
    if (samplerate<=24000) return 24000;
    if (samplerate<=32000) return 32000;
    if (samplerate<=44100) return 44100;
    
    return 48000;
}

int BitrateIndex(
int bRate,        /* legal rates from 32 to 448 */
int version,      /* MPEG-1 or MPEG-2 LSF */
int samplerate)   /* convert bitrate in kbps to index */
{
int     index = 0;
int     found = 0;

    while(!found && index<15)   {
        if(bitrate_table[version][index] == bRate)
            found = 1;
        else
            ++index;
    }
    if(found)
        return(index);
    else {
        ERRORF("Bitrate %dkbs not legal for %iHz output sampling.\n",
                bRate, samplerate);
        return(-1);     /* Error! */
    }
}

int SmpFrqIndex(  /* convert samp frq in Hz to index */
long sRate,             /* legal rates 16000, 22050, 24000, 32000, 44100, 48000 */
int  *version)
{
	/* Assign default value */
	*version=0;

    if (sRate == 44100L) {
        *version = 1; return(0);
    }
    else if (sRate == 48000L) {
        *version = 1; return(1);
    }
    else if (sRate == 32000L) {
        *version = 1; return(2);
    }
    else if (sRate == 24000L) {
        *version = 0; return(1);
    }
    else if (sRate == 22050L) {
        *version = 0; return(0);
    }
    else if (sRate == 16000L) {
        *version = 0; return(2);
    }
    else if (sRate == 12000L) {
        *version = 0; return(1);
    }
    else if (sRate == 11025L) {
        *version = 0; return(0);
    }
    else if (sRate ==  8000L) {
        *version = 0; return(2);
    }
    else {
        ERRORF("SmpFrqIndex: %ldHz is not a legal sample rate\n", sRate);
        return(-1);     /* Error! */
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

void SwapBytesInWords( short *loc, int words )
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


/*open and initialize the buffer; */
void alloc_buffer(
Bit_stream_struc *bs,   /* bit stream structure */
unsigned int size)
{
   bs->buf = (unsigned char *)       malloc(size);
   bs->buf_size = size;
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
INLINE double blackman(int i,double offset,double fcn,int l)
{
  double bkwn;
  double wcn = (PI * fcn);
  double dly = l / 2.0;
  double x = i-offset;
  if (x<0) x=0;
  if (x>l) x=l;
  bkwn = 0.42 - 0.5 * cos((x * 2) * PI /l)
    + 0.08 * cos((x * 4) * PI /l);
  if (fabs(x-dly)<1e-9) return wcn/PI;
  else 
    return  (sin( (wcn *  ( x - dly))) / (PI * ( x - dly)) * bkwn );
}

int fill_buffer_downsample(lame_global_flags *gfp,short int *outbuf,int desired_len,
			 short int *inbuf,int len,int *num_used,int ch) {
  
  lame_internal_flags *gfc=gfp->internal_flags;
  FLOAT8 offset,xvalue;
  int i,j=0,k,value;
  int filter_l;
  FLOAT8 fcn,intratio;
  short int *inbuf_old;

  intratio=( fabs(gfc->resample_ratio - floor(.5+gfc->resample_ratio)) < .0001 );
  fcn = .90/gfc->resample_ratio;
  if (fcn>.90) fcn=.90;
  filter_l=BLACKSIZE-6;  
  if (0==filter_l % 2 ) --filter_l;  /* must be odd */


  /* if resample_ratio = int, filter_l should be even */
  filter_l += intratio;
  assert(filter_l +5 < BLACKSIZE);
  
  if (!gfc->fill_buffer_downsample_init) {
    gfc->fill_buffer_downsample_init=1;
    gfc->itime[0]=0;
    gfc->itime[1]=0;
    memset((char *) gfc->inbuf_old, 0, sizeof(short int)*2*BLACKSIZE);
    /* precompute blackman filter coefficients */
    for (j= 0; j<= 2*BPC; ++j) {
      offset=(double)(j-BPC)/(double)(2*BPC);
      for (i=0; i<=filter_l; ++i) {
	gfc->blackfilt[j][i]=blackman(i,offset,fcn,filter_l);
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
#undef PRECOMPUTE
#ifdef PRECOMPUTE
      xvalue += y*gfc->blackfilt[joff][i];
#else
      if (intratio) 
	xvalue += y*gfc->blackfilt[joff][i];
      else
	xvalue += y*blackman(i,offset,fcn,filter_l);  /* very slow! */
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

