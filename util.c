#include "util.h"
#include "globalflags.h"
#include <assert.h>

/***********************************************************************
*
*  Global Variable Definitions
*
***********************************************************************/


/* 1: MPEG-1, 0: MPEG-2 LSF, 1995-07-11 shn */
FLOAT8  s_freq[2][4] = {{22.05, 24, 16, 0}, {44.1, 48, 32, 0}};

/* 1: MPEG-1, 0: MPEG-2 LSF, 1995-07-11 shn */
int     bitrate[2][3][15] = {
          {{0,32,48,56,64,80,96,112,128,144,160,176,192,224,256},
           {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160},
           {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160}},
	  {{0,32,64,96,128,160,192,224,256,288,320,352,384,416,448},
           {0,32,48,56,64,80,96,112,128,160,192,224,256,320,384},
           {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320}}
        };


enum byte_order NativeByteOrder = order_unknown;

/***********************************************************************
*
*  Global Function Definitions
*
***********************************************************************/


/***********************************************************************
 * compute bitsperframe and mean_bits for a layer III frame 
 **********************************************************************/
void getframebits(layer *info, int *bitsPerFrame, int *mean_bits) {
  int whole_SpF;
  FLOAT8 bit_rate,samp;
  int samplesPerFrame,bitsPerSlot;
  int sideinfo_len;
  
  samp =      s_freq[info->version][info->sampling_frequency];
  bit_rate = bitrate[info->version][info->lay-1][info->bitrate_index];
  samplesPerFrame = 576 * gf.mode_gr;
  bitsPerSlot = 8;

  /* determine the mean bitrate for main data */
  sideinfo_len = 32;
  if ( info->version == 1 )
    {   /* MPEG 1 */
      if ( gf.stereo == 1 )
	sideinfo_len += 136;
      else
	sideinfo_len += 256;
    }
  else
    {   /* MPEG 2 */
      if ( gf.stereo == 1 )
	sideinfo_len += 72;
      else
	sideinfo_len += 136;
    }
  
  if (info->error_protection) sideinfo_len += 16;
  
  
  whole_SpF =  (samplesPerFrame /samp)*(bit_rate /  (FLOAT8)bitsPerSlot);
  *bitsPerFrame = 8 * whole_SpF + (info->padding * 8);
  *mean_bits = (*bitsPerFrame - sideinfo_len) / gf.mode_gr;
}




void display_bitrates(int layr)
{
  int index,version;

  version = MPEG_AUDIO_ID;
  fprintf(stderr,"\n");
  fprintf(stderr,"MPEG1 samplerates(kHz): 32 44.1 48 \n");

  fprintf(stderr,"bitrates(kbs): ");
  for (index=1;index<15;index++) {
    fprintf(stderr,"%i ",bitrate[version][2][index]);
  }
  fprintf(stderr,"\n");
  
  
  version = MPEG_PHASE2_LSF; 
  fprintf(stderr,"\n");
  fprintf(stderr,"MPEG2 samplerates(kHz): 16 22.05 24 \n");
  fprintf(stderr,"bitrates(kbs): ");
  for (index=1;index<15;index++) {
    fprintf(stderr,"%i ",bitrate[version][2][index]);
  }
  fprintf(stderr,"\n");
}


int BitrateIndex(
int layr,         /* 1 or 2 */
int bRate,        /* legal rates from 32 to 448 */
int version,      /* MPEG-1 or MPEG-2 LSF */
int samplerate)   /* convert bitrate in kbps to index */
{
int     index = 0;
int     found = 0;

    while(!found && index<15)   {
        if(bitrate[version][layr-1][index] == bRate)
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
        *version = MPEG_AUDIO_ID; return(0);
    }
    else if (sRate == 48000L) {
        *version = MPEG_AUDIO_ID; return(1);
    }
    else if (sRate == 32000L) {
        *version = MPEG_AUDIO_ID; return(2);
    }
    else if (sRate == 24000L) {
        *version = MPEG_PHASE2_LSF; return(1);
    }
    else if (sRate == 22050L) {
        *version = MPEG_PHASE2_LSF; return(0);
    }
    else if (sRate == 16000L) {
        *version = MPEG_PHASE2_LSF; return(2);
    }
    else {
        fprintf(stderr, "SmpFrqIndex: %ldHz is not a legal sample rate\n", sRate);
        return(-1);     /* Error! */
    }
}

/*******************************************************************************
*
*  Allocate number of bytes of memory equal to "block".
*
*******************************************************************************/
/* exit(0) changed to exit(1) on memory allocation
 * error -- 1999/06 Alvaro Martinez Echevarria */

void  *mem_alloc(unsigned long block, char *item)
{

    void    *ptr;

    /* what kind of shit does ISO put out?  */
    ptr = (void *) malloc((size_t) block/*<<1*/); /* allocate twice as much memory as needed. fixes dodgy
					    memory problem on most systems */


    if (ptr != NULL) {
        memset(ptr, 0, (size_t) block);
    } else {
        fprintf(stderr,"Unable to allocate %s\n", item);
        exit(1);
    }
    return(ptr);
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

/*open_bit_stream_w(); open the device to write the bit stream into it    */
/*open_bit_stream_r(); open the device to read the bit stream from it     */
/*alloc_buffer();      open and initialize the buffer;                    */
/*desalloc_buffer();   empty and close the buffer                         */
/*back_track_buffer();     goes back N bits in the buffer                 */
/*unsigned int get1bit();  read 1 bit from the bit stream                 */
/*unsigned long look_ahead(); grep the next N bits in the bit stream without*/
/*                            changing the buffer pointer                   */
/*putbits(); write N bits from the bit stream */
/*int seek_sync(); return 1 if a sync word was found in the bit stream      */
/*                 otherwise returns 0                                      */



/* empty the buffer to the output device when the buffer becomes full */
/* mt 3/99: modified so we can call empty_buffer even if buffer is not full */
/* add error checking -- 1999/06 Alvaro Martinez Echevarria */
/* someone gimme a good reason for writing bytes one by
 * one; changed -- 1999/06 Alvaro Martinez Echevarria */
void write_buffer(bs)
Bit_stream_struc *bs;   /* bit stream structure */
{
   register int i,j;
   int minimum=1+bs->buf_byte_idx;    /* end of the buffer to empty */
   unsigned char *tmpbuf;
   if (bs->buf_size-minimum <= 0) return;

   tmpbuf=(unsigned char *)malloc(bs->buf_size-minimum);
   if (tmpbuf==NULL) {
     perror("write_buffer(): error in malloc()\n");
     exit(1);
   }
   for (i=bs->buf_size-1,j=0;i>=minimum;i--,j++) {
     tmpbuf[j]=bs->buf[i];
   }
   fwrite(tmpbuf,1,bs->buf_size-minimum,bs->pt);
   if (ferror(bs->pt)) {
     fprintf(stderr,"error en fwrite()\n");
     exit(1);
   }
   free(tmpbuf);
}

int copy_buffer(char *buffer,Bit_stream_struc *bs)
{
  int i,j=0;
  for (i=bs->buf_size-1 ; i > bs->buf_byte_idx ; (i-- ))
    buffer[j++]=bs->buf[i];
  return j;
}


void empty_buffer(Bit_stream_struc *bs)
{
   int minimum=1+bs->buf_byte_idx;    /* end of the buffer to empty */
   if (bs->buf_size-minimum <= 0) return;
   bs->buf_byte_idx = bs->buf_size -1;
   bs->buf_bit_idx = 8;

   bs->buf[bs->buf_byte_idx] = 0;  /* what does this do? */

}



/* open the device to write the bit stream into it */
void open_bit_stream_w(	
Bit_stream_struc* bs,	/* bit stream structure */
char* bs_filenam,	/* name of the bit stream file */
int size,              /* size of the buffer */
int nowrite             /* =1 to disable output */)
{
  if (!nowrite) {
    if (!strcmp(bs_filenam, "-")) {
      /* Write to standard output. */
#ifdef __EMX__
      _fsetmode(stdout,"b");
#elif (defined  __BORLANDC__)
      setmode(_fileno(stdout), O_BINARY);
#elif (defined  __CYGWIN__)
      setmode(fileno(stdout), _O_BINARY);
#elif (defined _WIN32)
      _setmode(_fileno(stdout), _O_BINARY);
#endif
      bs->pt = stdout;
    } else {
      if ((bs->pt = fopen(bs_filenam, "wb")) == NULL) {
	fprintf(stderr,"Could not create \"%s\".\n", bs_filenam);
	exit(1);
      }
    }
  }

   alloc_buffer(bs, size);
   bs->buf_byte_idx = size-1;
   bs->buf_bit_idx=8;
   bs->totbit=0;
}


/*open and initialize the buffer; */
void alloc_buffer(
Bit_stream_struc *bs,   /* bit stream structure */
int size)
{
   bs->buf = (unsigned char *)
	mem_alloc((unsigned long) (size * sizeof(unsigned char)), "buffer");
   bs->buf_size = size;
}

/*empty and close the buffer */
void desalloc_buffer(Bit_stream_struc *bs)   /* bit stream structure */
{
   free(bs->buf);
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

