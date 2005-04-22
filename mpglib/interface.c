/* $Id$ */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "interface.h"
#include "tabinit.h"
#include "encoder.h"
#include "decode_i386.h"

#ifdef USE_LAYER_1
# include "layer1.h"
#endif

#ifdef USE_LAYER_2
# include "layer2.h"
#endif

# include "layer3.h"

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif


PMPSTR InitMP3(void)
{
    PMPSTR mp = calloc(1, sizeof(MPSTR));
    if (!mp)
	return mp;

    mp->framesize = 0;
    mp->num_frames = 0;
    mp->enc_delay = -1;
    mp->enc_padding = -1;
    mp->vbr_header=0;
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
    mp->wordpointer = mp->bsspace[mp->bsnum] + 512;
    mp->synth_bo = 1;
    mp->sync_bitstream = 1;

    make_decode_tables(32767);

    init_layer3(SBLIMIT);

#ifdef USE_LAYER_2
    init_layer2();
#endif

    return mp;
}

void ExitMP3(PMPSTR mp)
{
    struct buf *b,*bn;

    if (!mp)
	return;

    b = mp->tail;
    while (b) {
	bn = b->next;
	free(b->pnt);
	free(b);
	b = bn;
    }
    free(mp);
}

static struct buf *
addbuf( PMPSTR mp, unsigned char *buf,int size)
{
    struct buf *nbuf;

    nbuf = (struct buf*) malloc( sizeof(struct buf) );
    if(!nbuf) {
	fprintf(stderr,"Out of memory!\n");
	return NULL;
    }
    nbuf->pnt = (unsigned char*) malloc((size_t)size);
    if(!nbuf->pnt) {
	free(nbuf);
	return NULL;
    }
    nbuf->size = size;
    memcpy(nbuf->pnt,buf,(size_t)size);
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

static void
remove_buf(PMPSTR mp)
{
    struct buf *buf = mp->tail;

    mp->tail = buf->next;
    if (mp->tail)
	mp->tail->prev = NULL;
    else {
	mp->tail = mp->head = NULL;
    }

    free(buf->pnt);
    free(buf);
}

static int read_buf_byte(PMPSTR mp)
{
    unsigned int b;
    int pos;

    pos = mp->tail->pos;
    while(pos >= mp->tail->size) {
	remove_buf(mp);
	if(!mp->tail) {
	    fprintf(stderr,"Fatal error! tried to read past mp buffer\n");
	    exit(1);
	}
	pos = mp->tail->pos;
    }

    b = mp->tail->pnt[pos];
    mp->bsize--;
    mp->tail->pos++;

    return b;
}



static void read_head(PMPSTR mp)
{
    unsigned long head;

    head  = read_buf_byte(mp);    head <<= 8;
    head |= read_buf_byte(mp);    head <<= 8;
    head |= read_buf_byte(mp);    head <<= 8;
    head |= read_buf_byte(mp);

    mp->header = head;
}






static void
copy_mp(PMPSTR mp,int size,unsigned char *ptr) 
{
    int len = 0;
    while(len < size && mp->tail) {
	int nlen;
	int blen = mp->tail->size - mp->tail->pos;
	if( (size - len) <= blen) {
	    nlen = size-len;
	}
	else {
	    nlen = blen;
	}
	memcpy(ptr+len, mp->tail->pnt + mp->tail->pos, (size_t)nlen);
	len += nlen;
	mp->tail->pos += nlen;
	mp->bsize -= nlen;
	if(mp->tail->pos == mp->tail->size) {
	    remove_buf(mp);
	}
    }
}

/* number of bytes needed by GetVbrTag to parse header */
#define XING_HEADER_SIZE 194

static const char	VBRTag[]={"Xing"};
static const char	VBRTag2[]={"Info"};

/*-------------------------------------------------------------*/
static int ExtractI4(unsigned char *buf)
{
    int x;
    /* big endian extract */
    x  = buf[0];    x <<= 8;
    x |= buf[1];    x <<= 8;
    x |= buf[2];    x <<= 8;
    x |= buf[3];
    return x;
}

static int
GetVbrTag(PMPSTR mp,unsigned char *buf)
{
    VBRTAGDATA TagData;
    int	i, head_flags, h_bitrate,h_id, h_mode, h_sr_index;
    int enc_delay, enc_padding, frames = 0;

    /* Xing VBR Header is only suuported Layer 3 */
    if ((buf[1] & 0x06) != 0x01*2)
	return 0;

    /* get selected MPEG header data */
    h_id       = (buf[1] >> 3) & 1;
    h_sr_index = (buf[2] >> 2) & 3;
    h_mode     = (buf[3] >> 6) & 3;
    h_bitrate  = ((buf[2]>>4)&0xf);
    h_bitrate = bitrate_table[h_id][h_bitrate];

    /* get Vbr header data */
    TagData.flags = 0;

    /* check for FFE syncword */
    if ((buf[1]>>4)==0xE) 
	TagData.samprate = samplerate_table[2][h_sr_index];
    else
	TagData.samprate = samplerate_table[h_id][h_sr_index];

    /*  determine offset of header */
    if (h_id) {
	/* mpeg1 */
	if (h_mode != 3)	buf+=(32+4);
	else				buf+=(17+4);
    }
    else
    {
	/* mpeg2 */
	if (h_mode != 3) buf+=(17+4);
	else             buf+=(9+4);
    }

    if (strncmp(buf, VBRTag, 4) && strncmp(buf, VBRTag2, 4))
	return 0;
    buf+=4;

    TagData.h_id = h_id;
    head_flags = TagData.flags = ExtractI4(buf); buf+=4;      /* get flags */

    if (head_flags & FRAMES_FLAG)
	frames   = ExtractI4(buf); buf+=4;

    if (head_flags & BYTES_FLAG)
	TagData.bytes = ExtractI4(buf); buf+=4;

    if (head_flags & TOC_FLAG){
	if(TagData.toc)
	{
	    for(i=0;i<NUMTOCENTRIES;i++)
		TagData.toc[i] = buf[i];
	}
	buf+=NUMTOCENTRIES;
    }

    TagData.vbr_scale = -1;

    if (head_flags & VBR_SCALE_FLAG)
	TagData.vbr_scale = ExtractI4(buf); buf+=4;

    TagData.headersize = ((h_id+1)*72000*h_bitrate) / TagData.samprate;

    buf+=21;
    enc_delay = buf[0] << 4;
    enc_delay += buf[1] >> 4;
    enc_padding= (buf[1] & 0x0F)<<8;
    enc_padding += buf[2];
    /* check for reasonable values (this may be an old Xing header, */
    /* not a INFO tag) */
    if (enc_delay<0 || enc_delay > 3000) enc_delay=-1;
    if (enc_padding<0 || enc_padding > 3000) enc_padding=-1;

    mp->num_frames  = frames;
    mp->enc_delay   = enc_delay;
    mp->enc_padding = enc_padding;

    if (TagData.headersize < 1) return 1;
    return TagData.headersize;
}


/* traverse mp data structure without changing it */
/* (just like sync_buffer) */
/* pull out Xing bytes */
/* call vbr header check code from LAME */
/* if we find a header, parse it and also compute the VBR header size */
/* if no header, do nothing. */
/* */
/* bytes = number of bytes before MPEG header.  skip this many bytes */
/* before starting to read */
/* return value: number of bytes in VBR header, including syncword */
static int
check_vbr_header(PMPSTR mp,int bytes)
{
    int i,pos;
    struct buf *buf=mp->tail;
    unsigned char xing[XING_HEADER_SIZE];

    pos = buf->pos;
    /* skip to valid header */
    for (i=0; i<bytes; ++i) {
	while(pos >= buf->size) {
	    buf  = buf->next;
	    pos = buf->pos;
	    if(!buf) 	return -1; /* fatal error */
	}
	++pos;
    }
    /* now read header */
    for (i=0; i<XING_HEADER_SIZE; ++i) {
	while(pos >= buf->size) {
	    buf  = buf->next;
	    if(!buf) 	return -1; /* fatal error */
	    pos = buf->pos;
	}
	xing[i] = buf->pnt[pos];
	++pos;
    }

    /* check first bytes for Xing header */
    return GetVbrTag(mp, xing);
}








static int
sync_buffer(PMPSTR mp,int free_match) 
{
    /* traverse mp structure without modifing pointers, looking
     * for a frame valid header.
     * if free_format, valid header must also have the same samplerate.   
     * return number of bytes in mp, before the header
     * return -1 if header is not found
     */
    unsigned int b[4]={0,0,0,0};
    int i,h,pos;
    struct buf *buf=mp->tail;
    if (!buf) return -1;

    pos = buf->pos;
    for (i=0; i<mp->bsize; i++) {
	/* get 4 bytes */
        b[0]=b[1]; b[1]=b[2]; b[2]=b[3];
	while(pos >= buf->size) {
	    buf  = buf->next;
	    pos = buf->pos;
	    if(!buf) {
		return -1;
		/* not enough data to read 4 bytes */
	    }
	}
	b[3] = buf->pnt[pos];
	++pos;

	if (i>=3) {
	    struct frame *fr = &mp->fr;
	    unsigned long head;

	    head  = b[0];	head <<= 8;
	    head |= b[1];	head <<= 8;
	    head |= b[2];	head <<= 8;
	    head |= b[3];
	    h = head_check(head,fr->lay);

	    if (h && free_match) {
		/* just to be even more thorough, match the sample rate */
		int mode,channels,sampling_frequency,mpeg25,lsf;
		if( head & (1<<20) ) {
		    lsf = (head & (1<<19)) ? 0x0 : 0x1;
		    mpeg25 = 0;
		}
		else {
		    lsf = 1;
		    mpeg25 = 1;
		}

		mode      = ((head>>6)&0x3);
		channels    = (mode == MPG_MD_MONO) ? 1 : 2;

		if(mpeg25) 
		    sampling_frequency = 6 + ((head>>10)&0x3);
		else
		    sampling_frequency = ((head>>10)&0x3) + (lsf*3);
		h = ((channels==fr->channels) && (lsf==fr->lsf) && (mpeg25==fr->mpeg25) && 
		     (sampling_frequency == fr->sampling_frequency));
	    }
	    if (h) {
		return i-3;
	    }
	}
    }
    return -1;
}





static int
decodeMP3_clipchoice(
    PMPSTR mp, unsigned char *in, int isize, char *out, int *done,
    int (*synth_1to1_mono_ptr)(PMPSTR,real *,unsigned char *,int *),
    int (*synth_1to1_ptr)(PMPSTR,real *,int,unsigned char *, int *) )
{
    int i,iret,bits,bytes;

    if (in && isize && !addbuf(mp,in,isize))
	return MP3_ERR;

    /* First decode header */
    if (!mp->header_parsed) {
	if (mp->fsizeold==-1 || mp->sync_bitstream) {
	    int vbrbytes;
	    mp->sync_bitstream=0;

	    /* This is the very first call.   sync with anything */
	    /* bytes= number of bytes before header */
	    bytes=sync_buffer(mp,0); 

	    /* now look for Xing VBR header */
	    if (mp->bsize >= bytes+XING_HEADER_SIZE) {
		/* vbrbytes = number of bytes in entire vbr header */
		vbrbytes=check_vbr_header(mp,bytes);
	    } else {
		/* not enough data to look for Xing header */
		return MP3_NEED_MORE;
	    }

	    if (mp->vbr_header) {
		/* do we have enough data to parse entire Xing header? */
		if (bytes+vbrbytes > mp->bsize) return MP3_NEED_MORE;

		/* read in Xing header.  Buffer data in case it
		 * is used by a non zero main_data_begin for the next
		 * frame, but otherwise dont decode Xing header */
/*fprintf(stderr,"found xing header, skipping %i bytes\n",vbrbytes+bytes);*/
		for (i=0; i<vbrbytes+bytes; ++i) read_buf_byte(mp);
		/* now we need to find another syncword */
		/* just return and make user send in more data */
		return MP3_NEED_MORE;
	    }
	} else {
	    /* match channels, samplerate, etc, when syncing */
	    bytes=sync_buffer(mp,1);
	}

	if (bytes<0) return MP3_NEED_MORE;
	if (bytes>0) {
	    /* there were some extra bytes in front of header.
	     * bitstream problem, but we are now resynced 
	     * should try to buffer previous data in case new
	     * frame has nonzero main_data_begin, but we need
	     * to make sure we do not overflow buffer
	     */
	    int size;
	    fprintf(stderr,"bitstream problem: resyncing...\n");
	    mp->old_free_format=0;
/*          mp->sync_bitstream=1;*/

	    /* skip some bytes, buffer the rest */
	    size = (int) (mp->wordpointer - (mp->bsspace[mp->bsnum]+512));

	    if (size > MAXFRAMESIZE) {
		/* wordpointer buffer is trashed.
		   probably cant recover, but try anyway */
		fprintf(stderr,"mpglib: wordpointer trashed.  size=%i (%i)  bytes=%i \n",
			size,MAXFRAMESIZE,bytes);
		size=0;
		mp->wordpointer = mp->bsspace[mp->bsnum]+512;
	    }

	    /* buffer contains 'size' data right now 
	       we want to add 'bytes' worth of data, but do not 
	       exceed MAXFRAMESIZE, so we through away 'i' bytes */
	    i = (size+bytes)-MAXFRAMESIZE;
	    for (; i>0; --i) {
		--bytes;
		read_buf_byte(mp);
	    }
	    copy_mp(mp, bytes, mp->wordpointer);
	    mp->fsizeold += bytes;
	}

	read_head(mp);
	decode_header(&mp->fr,mp->header);
	mp->header_parsed=1;
	mp->framesize = mp->fr.framesize;
	mp->free_format = (mp->framesize==0);

	if(mp->fr.lsf)
	    mp->ssize = (mp->fr.channels == 1) ? 9 : 17;
	else
	    mp->ssize = (mp->fr.channels == 1) ? 17 : 32;
	if (mp->fr.error_protection) 
	    mp->ssize += 2;

	mp->bsnum = 1-mp->bsnum; /* toggle buffer */
	mp->wordpointer = mp->bsspace[mp->bsnum] + 512;
	mp->bitindex = 0;

	/* for very first header, never parse rest of data */
	if (mp->fsizeold==-1)
	    return MP3_NEED_MORE;
    }

    /* now decode side information */
    if (!mp->side_parsed) {
	/* Layer 3 only */
	if (mp->fr.lay==3) {
	    if (mp->bsize < mp->ssize) 
		return MP3_NEED_MORE;

	    copy_mp(mp, mp->ssize, mp->wordpointer);

	    if(mp->fr.error_protection)
		getbits(mp, 16);
	    bits=do_layer3_sideinfo(mp);
	    /* bits = actual number of bits needed to parse this frame */
	    /* can be negative, if all bits needed are in the reservoir */
	    if (bits<0) bits=0;

	    /* read just as many bytes as necessary before decoding */
	    mp->dsize = (bits+7)/8;

	    /* this will force mpglib to read entire frame before decoding */
	    /* mp->dsize= mp->framesize - mp->ssize;*/
	} else {
	    /* Layers 1 and 2 */
	    /* check if there is enough input data */
	    if(mp->fr.framesize > mp->bsize)
		return MP3_NEED_MORE;

	    /* takes care that the right amount of data is copied
	       into wordpointer */
	    mp->dsize=mp->fr.framesize;
	    mp->ssize=0;
	}
	mp->side_parsed=1;
    }

    /* now decode main data */
    iret=MP3_NEED_MORE;
    if (!mp->data_parsed ) {
	if(mp->dsize > mp->bsize) {
	    return MP3_NEED_MORE;
	}
	copy_mp(mp, mp->dsize, mp->wordpointer);
	*done = 0;

	/*do_layer3(&mp->fr,(unsigned char *) out,done); */
	switch (mp->fr.lay) {
#ifdef USE_LAYER_1
	case 1:
	    if(mp->fr.error_protection)
		getbits(mp, 16);
	    do_layer1(mp,(unsigned char *) out,done);
	    break;
#endif
#ifdef USE_LAYER_2
	case 2:
	    if(mp->fr.error_protection)
		getbits(mp, 16);
	    do_layer2(mp,(unsigned char *) out,done);
	    break;
#endif
	case 3:
	    do_layer3(mp,(unsigned char *) out,done, synth_1to1_mono_ptr, synth_1to1_ptr);
	    break;
	default:
	    fprintf(stderr,"invalid layer %d\n",mp->fr.lay);
	}

	mp->wordpointer = mp->bsspace[mp->bsnum] + 512 + mp->ssize + mp->dsize;

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
	} else {
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
	int size;
	while (bytes > 512) {
	    read_buf_byte(mp);
	    bytes--;
	    mp->framesize--;
	}
	copy_mp(mp,bytes,mp->wordpointer);
	mp->wordpointer += bytes;

	size = (int) (mp->wordpointer - (mp->bsspace[mp->bsnum]+512));
	if (size > MAXFRAMESIZE) {
	    fprintf(stderr,"fatal error.  MAXFRAMESIZE not large enough.\n");
	}
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

int decodeMP3(PMPSTR mp,unsigned char *in,int isize,char *out,
	      int osize,int *done)
{
    if(osize < 4608) {
	fprintf(stderr,"To less out space\n");
	return MP3_ERR;
    }

    /* passing pointers to the functions which clip the samples */
    return decodeMP3_clipchoice(mp, in, isize, out, done, synth_1to1_mono, synth_1to1);
}

int decodeMP3_unclipped(PMPSTR mp,unsigned char *in,int isize,char *out,
			int osize,int *done)
{
    /* we forbid input with more than 1152 samples per channel for output in unclipped mode */
    if (osize < (int)(1152 * 2 * sizeof(real))) { 
	fprintf(stderr,"To less out space\n");
	return MP3_ERR;
    }

    /* passing pointers to the functions which don't clip the samples */
    return decodeMP3_clipchoice(mp, in, isize, out, done, synth_1to1_mono_unclipped, synth_1to1_unclipped);
}
