#ifdef HAVEMPGLIB
#include <stdlib.h>
#include <stdio.h>

#include "mpg123.h"
#include "mpglib.h"


/* Global mp .. it's a hack */
struct mpstr *gmp;


BOOL InitMP3(struct mpstr *mp) 
{
	memset(mp,0,sizeof(struct mpstr));

	mp->framesize = 0;
	mp->header_parsed=0;
	mp->side_parsed=0;
	mp->data_parsed=0;
	mp->free_format=0;
	mp->old_free_format=0;
	mp->ssize = 0;
	mp->dsize=0;
	mp->fsizeold = -1;
	mp->bsize = 0;
	mp->head = mp->tail = NULL;
	mp->fr.single = -1;
	mp->bsnum = 0;
	wordpointer = mp->bsspace[mp->bsnum] + 512;
	mp->synth_bo = 1;

	make_decode_tables(32767);
	init_layer3(SBLIMIT);

	return !0;
}

void ExitMP3(struct mpstr *mp)
{
	struct buf *b,*bn;
	
	b = mp->tail;
	while(b) {
		free(b->pnt);
		bn = b->next;
		free(b);
		b = bn;
	}
}

static struct buf *addbuf(struct mpstr *mp,char *buf,int size)
{
	struct buf *nbuf;

	nbuf = (struct buf*) malloc( sizeof(struct buf) );
	if(!nbuf) {
		fprintf(stderr,"Out of memory!\n");
		return NULL;
	}
	nbuf->pnt = (unsigned char*) malloc(size);
	if(!nbuf->pnt) {
		free(nbuf);
		return NULL;
	}
	nbuf->size = size;
	memcpy(nbuf->pnt,buf,size);
	nbuf->next = NULL;
	nbuf->prev = mp->head;
	nbuf->pos = 0;

	if(!mp->tail) {
		mp->tail = nbuf;
	}
	else {
	  mp->head->next = nbuf;
	}

	mp->head = nbuf;
	mp->bsize += size;

	return nbuf;
}

static void remove_buf(struct mpstr *mp)
{
  struct buf *buf = mp->tail;
  
  mp->tail = buf->next;
  if(mp->tail)
    mp->tail->prev = NULL;
  else {
    mp->tail = mp->head = NULL;
  }
  
  free(buf->pnt);
  free(buf);

}

static int read_buf_byte(struct mpstr *mp)
{
	unsigned int b;

	int pos;

	pos = mp->tail->pos;
	while(pos >= mp->tail->size) {
	        remove_buf(mp);
		pos = mp->tail->pos;
		if(!mp->tail) {
			fprintf(stderr,"Fatal error!\n");
			exit(1);
		}
	}

	b = mp->tail->pnt[pos];
	mp->bsize--;
	mp->tail->pos++;
	

	return b;
}



static void read_head(struct mpstr *mp)
{
	unsigned long head;

	head = read_buf_byte(mp);
	head <<= 8;
	head |= read_buf_byte(mp);
	head <<= 8;
	head |= read_buf_byte(mp);
	head <<= 8;
	head |= read_buf_byte(mp);

	mp->header = head;
}






void copy_mp(struct mpstr *mp,int size,char *ptr) 
{
  int len = 0;
  while(len < size) {
    int nlen;
    int blen = mp->tail->size - mp->tail->pos;
    if( (size - len) <= blen) {
      nlen = size-len;
    }
    else {
      nlen = blen;
    }
    memcpy(ptr+len,mp->tail->pnt+mp->tail->pos,nlen);
    len += nlen;
    mp->tail->pos += nlen;
    mp->bsize -= nlen;
    if(mp->tail->pos == mp->tail->size) {
      remove_buf(mp);
    }
  }
}



int sync_buffer(struct mpstr *mp,int free_match) 
{
  /* traverse mp structure without modifing pointers, looking
   * for a frame valid header.
   * if free_format, valid header must also have the same
   * samplerate.   
   * return number of bytes in mp, before the header
   * return -1 if header is not found
   */
  unsigned int b[4]={0,0,0,0};
  int i,h,pos;
  struct buf *buf=mp->tail;

  pos = buf->pos;
  for (i=0; i<mp->bsize; i++) {
    /* get 4 bytes */
    
    b[0]=b[1]; b[1]=b[2]; b[2]=b[3];
    while(pos >= buf->size) {
      buf  = buf->next;
      pos = buf->pos;
      if(!buf) {
	fprintf(stderr,"Fatal error!\n");
	exit(1);
      }
    }
    b[3] = buf->pnt[pos];
    ++pos;

    if (i>=3) {
	unsigned long head;

	head = b[0];
	head <<= 8;
	head |= b[1];
	head <<= 8;
	head |= b[2];
	head <<= 8;
	head |= b[3];
	h = head_check(head);

	if (h && free_match) {
	  /* just to be even more thorough, match the sample rate */
	  struct frame *fr = &mp->fr;
	  int sampling_frequency,mpeg25,lsf;

	  if( head & (1<<20) ) {
	    lsf = (head & (1<<19)) ? 0x0 : 0x1;
	    mpeg25 = 0;
	  }
	  else {
	    lsf = 1;
	    mpeg25 = 1;
	  }

	  if(mpeg25) 
	    sampling_frequency = 6 + ((head>>10)&0x3);
	  else
	    sampling_frequency = ((head>>10)&0x3) + (lsf*3);
	  h = ((lsf==fr->lsf) && (mpeg25==fr->mpeg25) && 
                 (sampling_frequency == fr->sampling_frequency));
	}

	if (h) {
	  return i-3;
	}
    }
  }
  return -1;
}





int decodeMP3(struct mpstr *mp,char *in,int isize,char *out,
		int osize,int *done)
{
	int i,iret,bits,bytes;
	gmp = mp;

	if(osize < 4608) {
		fprintf(stderr,"To less out space\n");
		return MP3_ERR;
	}

	if(in) {
		if(addbuf(mp,in,isize) == NULL) {
			return MP3_ERR;
		}
	}


	/* First decode header */
	if(!mp->header_parsed) {

	        bytes=sync_buffer(mp,0);
		if (bytes<0) return MP3_NEED_MORE;
		if (bytes>0) {
		  /* bitstream problem, but we are now resynced 
		   * should try to buffer previous data in case new
		   * frame has nonzero main_data_begin, but we need
		   * to make sure we do not overflow buffer
		  */
		  int size;
		  fprintf(stderr,"bitstream problem: resyncing...\n");
		  mp->old_free_format=0;

		  /* skip some bytes, buffer the rest */
		  size = (int) (wordpointer - mp->bsspace[mp->bsnum]+512);
		  i = (size+bytes)-MAXFRAMESIZE;
		  for (; i>0; --i) {
		    --bytes;
		    read_buf_byte(mp);
		  }
		  copy_mp(mp,bytes,wordpointer);
		  mp->fsizeold += bytes;

		}

		read_head(mp);
		decode_header(&mp->fr,mp->header);
		mp->header_parsed=1;
		mp->framesize = mp->fr.framesize;
		mp->free_format = (mp->framesize==0);

		if(mp->fr.lsf)
		  mp->ssize = (mp->fr.stereo == 1) ? 9 : 17;
		else
		  mp->ssize = (mp->fr.stereo == 1) ? 17 : 32;
		if (mp->fr.error_protection) 
		  mp->ssize += 2;

		mp->bsnum = 1-mp->bsnum; /* toggle buffer */
		wordpointer = mp->bsspace[mp->bsnum] + 512;
		//		mp->bsnum = (mp->bsnum + 1) & 0x1;
		bitindex = 0;
	}

	/* now decode side information */
	if (!mp->side_parsed ) {
                if (mp->bsize < mp->ssize) 
		  return MP3_NEED_MORE;

		copy_mp(mp,mp->ssize,wordpointer);

		if(mp->fr.error_protection)
		  getbits(16);
		bits=do_layer3_sideinfo(&mp->fr);
		/* bits = actual number of bits needed to parse this frame */
		/* can be negative, if all bits needed are in the reservoir */
		if (bits<0) bits=0;

		/* read just as many bytes as necessary before decoding */
		mp->dsize = (bits+7)/8;

		/* this will force mpglib to read entire frame before decoding */
		/* mp->dsize= mp->framesize - mp->ssize;*/

		mp->side_parsed=1;
	}

	/* now decode main data */
	iret=MP3_NEED_MORE;
	if (!mp->data_parsed) {
	        if(mp->dsize > mp->bsize) {
		  return MP3_NEED_MORE;
		}
		copy_mp(mp,mp->dsize,wordpointer);
		*done = 0;
		do_layer3(&mp->fr,(unsigned char *) out,done);
		wordpointer = mp->bsspace[mp->bsnum] + 512 + mp->ssize + mp->dsize;
		mp->data_parsed=1;
		iret=MP3_OK;
	}


	/* remaining bits are ancillary data, or reservoir for next frame 
	 * If free format, scan stream looking for next frame to determine
	 * mp->framesize */
	if (mp->free_format) {
	  if (mp->old_free_format) {
	    /* free format.  bitrate must not vary */
	    mp->framesize=mp->fsizeold_nopadding + (mp->fr.padding);
	  }else{
	    bytes=sync_buffer(mp,1);
	    if (bytes<0) return iret;
	    mp->framesize = bytes + mp->ssize+mp->dsize;
	    mp->fsizeold_nopadding= mp->framesize - mp->fr.padding;
	    /*
	    fprintf(stderr,"freeformat bitstream:  estimated bitrate=%ikbs  \n",
	        8*(4+mp->framesize)*freqs[mp->fr.sampling_frequency]/
		    (1000*576*(2-mp->fr.lsf)));
	    */
	  }
	}

	/* buffer the ancillary data and reservoir for next frame */
	bytes = mp->framesize-(mp->ssize+mp->dsize);
	if (bytes > mp->bsize) {
	  return iret;
	}
	if (bytes>0) {
	  copy_mp(mp,bytes,wordpointer);
	  wordpointer += bytes;
	}

	/* the above frame is completey parsed.  start looking for next frame */
	mp->fsizeold = mp->framesize;
	mp->old_free_format = mp->free_format;
	mp->framesize =0;
	mp->header_parsed=0;
	mp->side_parsed=0;
	mp->data_parsed=0;
	return iret;
}

	
int set_pointer(long backstep)
{
  unsigned char *bsbufold;

  if(gmp->fsizeold < 0 && backstep > 0) {
    fprintf(stderr,"Can't step back %ld!\n",backstep);
    return MP3_ERR; 
  }
  bsbufold = gmp->bsspace[1-gmp->bsnum] + 512;
  wordpointer -= backstep;
  if (backstep)
    memcpy(wordpointer,bsbufold+gmp->fsizeold-backstep,backstep);
  bitindex = 0;
  return MP3_OK;
}





#endif
