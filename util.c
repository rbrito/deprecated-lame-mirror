#include "util.h"
#include <assert.h>

/***********************************************************************
*
*  Global Variable Definitions
*
***********************************************************************/


/* 1: MPEG-1, 0: MPEG-2 LSF, 1995-07-11 shn */
FLOAT8  s_freq_table[3][4] = 
  {{22.05, 24, 16, 0}, {44.1, 48, 32, 0}, {11.025, 12,8,0}};

/* 1: MPEG-1, 0: MPEG-2 LSF, 1995-07-11 shn */
int     bitrate_table[2][15] = {
          {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160},
          {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320}};


enum byte_order NativeByteOrder = order_unknown;

/***********************************************************************
*
*  Global Function Definitions
*
***********************************************************************/


/***********************************************************************
 * compute bitsperframe and mean_bits for a layer III frame 
 **********************************************************************/
void getframebits(lame_global_flags *gfp,int *bitsPerFrame, int *mean_bits) {
  int whole_SpF;
  FLOAT8 bit_rate,samp;
  lame_internal_flags *gfc=gfp->internal_flags;

  
  samp =      gfp->out_samplerate/1000.0;
  bit_rate = bitrate_table[gfp->version][gfc->bitrate_index];

  /* -f fast-math option causes some strange rounding here, be carefull: */  
  whole_SpF = floor( (gfc->framesize /samp)*(bit_rate /  8.0) + 1e-9);
  *bitsPerFrame = 8 * whole_SpF + (gfc->padding * 8);
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
        fprintf(stderr,"Bitrate %dkbs not legal for %iHz output sampling.\n",
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
        fprintf(stderr, "SmpFrqIndex: %ldHz is not a legal sample rate\n", sRate);
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
  int i,j=0;
  if (bs->bstype) {
    int minimum = bs->buf_byte_idx + 1;
    if (minimum <= 0) return 0;
    if (size!=0 && minimum>size) return -1; /* buffer is too small */
    memcpy(buffer,bs->buf,minimum);
    bs->buf_byte_idx = -1;
    bs->buf_bit_idx = 0;
    return minimum;
  }else{
    if (size!=0 && (bs->buf_size-1 - bs->buf_byte_idx) > size ) return -1;
    for (i=bs->buf_size-1 ; i > bs->buf_byte_idx ; (i-- ))
      buffer[j++]=bs->buf[i];
    assert(j == (bs->buf_size-1 - bs->buf_byte_idx));
    empty_buffer(bs);  /* empty buffer, (changes bs->buf_size) */
    return j;
  }
}





void init_bit_stream_w(lame_internal_flags *gfc)
{
  Bit_stream_struc* bs = &gfc->bs;
   alloc_buffer(bs, BUFFER_SIZE);
   if (bs->bstype==1) {
     /* takehiro style */
     gfc->h_ptr = gfc->w_ptr = 0;
     gfc->header[gfc->h_ptr].write_timing = 0;
     gfc->bs.bstype = 1;
     gfc->bs.buf_byte_idx = -1;
     gfc->bs.buf_bit_idx = 0;
     gfc->bs.totbit = 0;
   }else{  
     /* ISO style */
     bs->buf_byte_idx = BUFFER_SIZE-1;
     bs->buf_bit_idx=8;
     bs->totbit=0;
   }
}


/*open and initialize the buffer; */
void alloc_buffer(
Bit_stream_struc *bs,   /* bit stream structure */
int size)
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


/*write N bits into the bit stream */
void putbits(
Bit_stream_struc *bs,   /* bit stream structure */
unsigned int val,       /* val to write into the buffer */
int N)                  /* number of bits of val */
{
 register int j = N;
 register int k, tmp;

 if (N > MAX_LENGTH)
    fprintf(stderr,"Cannot read or write more than %d bits at a time.\n", MAX_LENGTH);

 bs->totbit += N;
 while (j > 0) {
   k = Min(j, bs->buf_bit_idx);
   tmp = val >> (j-k);
   bs->buf[bs->buf_byte_idx] |= (tmp&putmask[k]) << (bs->buf_bit_idx-k);
   bs->buf_bit_idx -= k;
   if (!bs->buf_bit_idx) {
       bs->buf_bit_idx = 8;
       bs->buf_byte_idx--;
       assert(bs->buf_byte_idx >= 0);
       bs->buf[bs->buf_byte_idx] = 0;
   }
   j -= k;
 }
}



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
void reorder(int scalefac_band[],int ix[576],int ix_orig[576]) {
  int i,sfb, window, j=0;
  for (sfb = 0; sfb < SBMAX_s; sfb++) {
    int start = scalefac_band[sfb];
    int end   = scalefac_band[sfb + 1];
    for (window = 0; window < 3; window++) {
      for (i = start; i < end; ++i) {
	ix[j++] = ix_orig[3*i+window];
      }
    }
  }
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
  filter_l=19;  /* must be odd */
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
#define PRECOMPUTE
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


/* 4 point interpolation routine for upsampling */
int fill_buffer_upsample(lame_global_flags *gfp,short int *outbuf,int desired_len,
        short int *inbuf,int len,int *num_used,int ch) {

  int i,j=0,k,linear,value;
  lame_internal_flags *gfc=gfp->internal_flags;
  short int *inbuf_old;

  if (!gfc->fill_buffer_upsample_init) {
    gfc->fill_buffer_upsample_init=1;
    gfc->upsample_itime[0]=0;
    gfc->upsample_itime[1]=0;
    memset((char *) gfc->upsample_inbuf_old, 0, sizeof(short int)*2*OLDBUFSIZE);
  }


  inbuf_old=gfc->upsample_inbuf_old[ch];

  /* if downsampling by an integer multiple, use linear resampling,
   * otherwise use quadratic */
  linear = ( fabs(gfc->resample_ratio - floor(.5+gfc->resample_ratio)) < .0001 );

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


