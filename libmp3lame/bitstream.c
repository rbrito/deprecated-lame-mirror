/*
 *	MP3 bitstream Output interface for LAME
 *
 *	Copyright 1999-2003 Takehiro TOMINAGA
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
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include "bitstream.h"
#include "version.h"
#include "VbrTag.h"
#include "tables.h"

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

/* unsigned int is at least this large:  */
/* we work with ints, so when doing bit manipulation, we limit
 * ourselves to MAX_LENGTH-2 just to be on the safe side */
#define MAX_LENGTH      32  

/***********************************************************************
 * compute bits-per-frame
 **********************************************************************/
int getframebytes(const lame_global_flags * gfp)
{
    lame_internal_flags *gfc=gfp->internal_flags;
    int  bit_rate;

    /* get bitrate in kbps */
    if (gfc->bitrate_index) 
	bit_rate = bitrate_table[gfp->version][gfc->bitrate_index];
    else
	bit_rate = gfp->mean_bitrate_kbps;
    assert ( bit_rate <= 550 );

    /* main encoding routine toggles padding on and off */
    /* one Layer3 Slot consists of 8 bits */
    return gfc->mode_gr*bit_rate*(1000*576/8) / gfp->out_samplerate
	+ gfc->padding - gfc->l3_side.sideinfo_len;
}

/*write j bits into the bit stream */
inline static void
putbits24(Bit_stream_struc *bs, int val, int j)
{
    char *p = &bs->buf[bs->bitidx >> 3];

    val <<= (32 - j - (bs->bitidx & 7));
    bs->bitidx += j;

    p[0] |= val >> 24;
    p[1]  = val >> 16;
    p[2]  = val >> 8;
    p[3]  = val;
}

inline static void
putbits(Bit_stream_struc *bs, int val, int j)
{
    if (j > 25) {
	putbits24(bs, val>>16, j-16);
	val &= 0xffff;
	j = 16;
    }
    putbits24(bs, val, j);
}

/*
  Some combinations of bitrate, Fs, and stereo make it impossible to stuff
  out a frame using just main_data, due to the limited number of bits to
  indicate main_data_length. In these situations, we put stuffing bits into
  the ancillary data...
*/

inline static void
drain_into_ancillary(lame_internal_flags *gfc, int remainingBits)
{
    Bit_stream_struc *bs = &gfc->bs;
    int i, pad;
    assert(remainingBits >= 0);

    if (remainingBits >= 8) {
	putbits24(bs,0x4c,8);
	remainingBits -= 8;
    }
    if (remainingBits >= 8) {
	putbits24(bs,0x41,8);
	remainingBits -= 8;
    }
    if (remainingBits >= 8) {
	putbits24(bs,0x4d,8);
	remainingBits -= 8;
    }
    if (remainingBits >= 8) {
	putbits24(bs,0x45,8);
	remainingBits -= 8;
    }

    pad = 0xaa;
    if (remainingBits >= 8) {
	const char *version = get_lame_short_version ();
	for (i=0; remainingBits >=8 ; ++i) {
	    if (i < strlen(version))
		putbits24(bs,version[i],8);
	    else
		putbits24(bs,pad,8);
	    remainingBits -= 8;
	}
    }

    if (remainingBits)
	putbits24(bs, pad >> (8 - remainingBits), remainingBits);
}

/*write N bits into the header */
inline static int
writeheader(char *p, int val, int j, int ptr)
{
    p += ptr >> 3;
    assert(0 < j && j <= 16);
    val <<= (24 - j - (ptr&7));
    p[0] |= val >> 16;
    p[1]  = val >> 8;
    p[2]  = val;
    return ptr + j;
}

static int
CRC_update(int value, int crc)
{
    int i;
    value <<= 8;
    for (i = 0; i < 8; i++) {
	value <<= 1;
	crc <<= 1;

	if (((crc ^ value) & 0x10000))
	    crc ^= CRC16_POLYNOMIAL;
    }
    return crc;
}

static int
writeTableHeader(lame_internal_flags *gfc, gr_info *gi, int ptr)
{
    static const int blockConv[] = {1, 3, 2};
    char *p = gfc->bs.header[gfc->bs.h_ptr].buf;
    if (gi->block_type != NORM_TYPE) {
	ptr = writeheader(p, 1, 1, ptr); /* window_switching_flag */
	ptr = writeheader(p, blockConv[gi->block_type-1], 2, ptr);
	ptr = writeheader(p, gi->mixed_block_flag, 1, ptr);

	if (gi->table_select[0] == 14)
	    gi->table_select[0] = 16;
	ptr = writeheader(p, gi->table_select[0],  5, ptr);
	if (gi->table_select[1] == 14)
	    gi->table_select[1] = 16;
	ptr = writeheader(p, gi->table_select[1],  5, ptr);

	ptr = writeheader(p, gi->subblock_gain[0], 3, ptr);
	ptr = writeheader(p, gi->subblock_gain[1], 3, ptr);
	ptr = writeheader(p, gi->subblock_gain[2], 3, ptr);
    } else {
	ptr++; /* window_switching_flag */
	if (gi->table_select[0] == 14)
	    gi->table_select[0] = 16;
	ptr = writeheader(p, gi->table_select[0], 5, ptr);
	if (gi->table_select[1] == 14)
	    gi->table_select[1] = 16;
	ptr = writeheader(p, gi->table_select[1], 5, ptr);
	if (gi->table_select[2] == 14)
	    gi->table_select[2] = 16;
	ptr = writeheader(p, gi->table_select[2], 5, ptr);

	assert(gi->region0_count < 16U);
	assert(gi->region1_count < 8U);
	ptr = writeheader(p, gi->region0_count, 4, ptr);
	ptr = writeheader(p, gi->region1_count, 3, ptr);
    }
    return ptr;
}

void
CRC_writeheader(lame_internal_flags *gfc, char *header)
{
    int crc = 0xffff; /* (jo) init crc16 for error_protection */
    int i;

    crc = CRC_update(((unsigned char*)header)[2], crc);
    crc = CRC_update(((unsigned char*)header)[3], crc);
    for (i = 6; i < gfc->l3_side.sideinfo_len; i++) {
	crc = CRC_update(((unsigned char*)header)[i], crc);
    }

    header[4] = crc >> 8;
    header[5] = crc & 255;
}

inline static void
Huf_count1(Bit_stream_struc *bs, gr_info *gi)
{
    int index;
    const unsigned char * const hcode = quadcode[gi->count1table_select];

    assert(gi->count1table_select < 2u);
    for (index = gi->big_values; index < gi->count1; index += 4) {
	int huffbits = 0, p = 0;

	if (gi->l3_enc[index  ]) {
	    p = 8;
	    huffbits = huffbits + (gi->xr[index  ] < 0);
	}

	if (gi->l3_enc[index+1]) {
	    p += 4;
	    huffbits = huffbits*2 + (gi->xr[index+1] < 0);
	}

	if (gi->l3_enc[index+2]) {
	    p += 2;
	    huffbits = huffbits*2 + (gi->xr[index+2] < 0);
	}

	if (gi->l3_enc[index+3]) {
	    p++;
	    huffbits = huffbits*2 + (gi->xr[index+3] < 0);
	}
	putbits24(bs, huffbits + hcode[p+16], hcode[p]);
    }
}



/*
  Implements the pseudocode of page 98 of the IS
  */
static void
Huffmancode_esc(Bit_stream_struc *bs, const struct huffcodetab* h,
		int index, int end, gr_info *gi)
{
    do {
	int cbits   = 0;
	int xbits   = 0;
	int ext = 0;
	int x1 = gi->l3_enc[index];
	int x2 = gi->l3_enc[index+1];

	if (x1 != 0) {
	    /* use ESC-words */
	    if (x1 > 14) {
		assert ( x1 <= h->linmax+15 );
		ext    = (x1-15) << 1;
		xbits  = h->xlen;
		x1     = 15;
	    }
	    ext += (gi->xr[index] < 0);
	    cbits--;
	    x1 *= 16;
	}

	if (x2 != 0) {
	    if (x2 > 14) {
		assert ( x2 <= h->linmax+15 );
		ext  <<= h->xlen;
		ext   |= x2-15;
		xbits += h->xlen;
		x2     = 15;
	    }
	    ext = ext*2 + (gi->xr[index+1] < 0);
	    cbits--;
	    x1 += x2;
	}

	xbits -= cbits;
	cbits += h->hlen  [x1];

	putbits24(bs, h->table [x1], cbits);
	if (xbits)
	    putbits(bs, ext, xbits);
    } while ((index += 2) < end);
}

static void
Huffmancode(Bit_stream_struc *bs, const struct huffcodetab* h,
	    int index, int end, gr_info *gi)
{
    do {
	int code, clen;
	int x1 = gi->l3_enc[index];
	int x2 = gi->l3_enc[index+1];
	assert ( (x1|x2) < 16u );

	code = x1*h->xlen + x2;
	clen = h->hlen[code];
	code = h->table[code];

	if (x1) code = code*2 + (gi->xr[index  ] < 0);
	if (x2) code = code*2 + (gi->xr[index+1] < 0);

	putbits24(bs, code, clen);
    } while ((index += 2) < end);
}

inline static void
Huf_bigvalue(Bit_stream_struc *bs, int tablesel,
	     int start, int end, gr_info *gi)
{
    if (tablesel == 0 || start >= end)
	return;

    if (tablesel > 15)
	Huffmancode_esc(bs, &ht[tablesel], start, end, gi);
    else
	Huffmancode(bs, &ht[tablesel], start, end, gi);
}

/*
  Note the discussion of huffmancodebits() on pages 28
  and 29 of the IS, as well as the definitions of the side
  information on pages 26 and 27.
  */
static void
Huffmancodebits(lame_internal_flags *gfc, gr_info *gi)
{
#ifndef NDEBUG
    int data_bits = gfc->bs.bitidx + gi->part2_3_length - gi->count1bits;
#endif
    int r1, r2;

    r1 = gfc->scalefac_band.l[gi->region0_count + 1];
    if (r1 > gi->big_values)
	r1 = gi->big_values;
    Huf_bigvalue(&gfc->bs, gi->table_select[0], 0, r1, gi);

    r2 = gfc->scalefac_band.l[gi->region0_count + gi->region1_count + 2];
    if (r2 > gi->big_values)
	r2 = gi->big_values;
    Huf_bigvalue(&gfc->bs, gi->table_select[1], r1, r2, gi);

    Huf_bigvalue(&gfc->bs, gi->table_select[2], r2, gi->big_values, gi);
    assert(gfc->bs.bitidx == data_bits);

    Huf_count1(&gfc->bs, gi);
    assert(gfc->bs.bitidx == data_bits + gi->count1bits);
}

inline static void
encodeBitStream(lame_global_flags *gfp)
{
    lame_internal_flags *gfc = gfp->internal_flags;
    III_side_info_t *l3_side = &gfc->l3_side;
    char *p = gfc->bs.header[gfc->bs.h_ptr].buf;
    int gr, ch, ptr;
    assert(l3_side->main_data_begin >= 0);

    memset(p, 0, l3_side->sideinfo_len);
    ptr = writeheader(p, 0xfff - (gfp->out_samplerate < 16000), 12, 0);
    ptr = writeheader(p, gfp->version,            1, ptr);
    ptr = writeheader(p, 4 - 3,                   2, ptr);
    ptr = writeheader(p, !gfp->error_protection,  1, ptr);
    ptr = writeheader(p, gfc->bitrate_index,      4, ptr);
    ptr = writeheader(p, gfc->samplerate_index,   2, ptr);
    ptr = writeheader(p, gfc->padding,            1, ptr);
    ptr = writeheader(p, gfp->extension,          1, ptr);
    ptr = writeheader(p, gfp->mode,               2, ptr);
    ptr = writeheader(p, gfc->mode_ext,           2, ptr);
    ptr = writeheader(p, gfp->copyright,          1, ptr);
    ptr = writeheader(p, gfp->original,           1, ptr);
    ptr = writeheader(p, gfp->emphasis,           2, ptr);
    if (gfp->error_protection)
	ptr += 16;

    ptr = writeheader(p, l3_side->main_data_begin, 7+gfc->mode_gr, ptr);
    if (gfp->version == 1) {
	/* MPEG1 */
	ptr += 7 - gfc->channels_out*2; /* private_bits */
	for (ch = 0; ch < gfc->channels_out; ch++)
	    ptr = writeheader(p,
			      l3_side->scfsi[ch][0]*8
			      + l3_side->scfsi[ch][1]*4
			      + l3_side->scfsi[ch][2]*2
			      + l3_side->scfsi[ch][3], 4, ptr);

	for (gr = 0; gr < 2; gr++) {
	    for (ch = 0; ch < gfc->channels_out; ch++) {
		gr_info *gi = &l3_side->tt[gr][ch];
#ifndef NDEBUG
		int data_bits = gfc->bs.bitidx + gi->part2_length;
#endif
		int slen, sfb;
		ptr = writeheader(p, gi->part2_3_length+gi->part2_length,
				  12, ptr);
		ptr = writeheader(p, gi->big_values / 2,        9, ptr);
		ptr = writeheader(p, gi->global_gain,           8, ptr);
		ptr = writeheader(p, gi->scalefac_compress,     4, ptr);
		ptr = writeTableHeader(gfc, gi, ptr);
		ptr = writeheader(p,
				  (gi->preflag > 0)*4 + gi->scalefac_scale*2
				  + gi->count1table_select, 3, ptr);

		slen = s1bits[gi->scalefac_compress];
		if (slen)
		    for (sfb = 0; sfb < gi->sfbdivide; sfb++) {
			if (gi->scalefac[sfb] == -1)
			    continue; /* scfsi is used */
			if (gi->scalefac[sfb] == -2)
			    gi->scalefac[sfb] = 0;
			putbits24(&gfc->bs, gi->scalefac[sfb], slen);
		    }
		slen = s2bits[gi->scalefac_compress];
		if (slen)
		    for (sfb = gi->sfbdivide; sfb < gi->sfbmax; sfb++) {
			if (gi->scalefac[sfb] == -1)
			    continue; /* scfsi is used */
			if (gi->scalefac[sfb] == -2)
			    gi->scalefac[sfb] = 0;
			putbits24(&gfc->bs, gi->scalefac[sfb], slen);
		    }
		assert(data_bits == gfc->bs.bitidx);
		Huffmancodebits(gfc, gi);
	    } /* for ch */
	} /* for gr */
    } else {
	/* MPEG2 */
	ptr += gfc->channels_out; /* private_bits */
	for (ch = 0; ch < gfc->channels_out; ch++) {
	    gr_info *gi = &l3_side->tt[0][ch];
	    int partition, sfb = 0, part;
#ifndef NDEBUG
	    int data_bits = gfc->bs.bitidx + gi->part2_length;
#endif
	    ptr = writeheader(p, gi->part2_3_length+gi->part2_length, 12,
			      ptr);
	    ptr = writeheader(p, gi->big_values / 2,        9, ptr);
	    ptr = writeheader(p, gi->global_gain,           8, ptr);

	    /* set scalefac_compress */
	    switch (gi->scalefac_compress) {
	    case 0: case 1: case 2:
		part = gi->slen[0]*80 + gi->slen[1]*16
		    + gi->slen[2]*4 + gi->slen[3];
		assert(part < 400);
		break;
	    case 3: case 4: case 5:
		part = 400 + gi->slen[0]*20 + gi->slen[1]*4 + gi->slen[2];
		assert(400 <= part && part < 500);
		assert(gi->slen[3] == 0);
		break;
	    case 6: case 7: case 8:
		part = 500 + gi->slen[0]*3 + gi->slen[1];
		{int i;
		for (i = 0; i < SBMAX_l; i++)
		    gi->scalefac[i] -= pretab[i];
		}
		assert(500 <= part && part < 512);
		assert(gi->slen[2] == 0);
		assert(gi->slen[3] == 0);
		break;
	    case 9: case 10: case 11:
		part = (gi->slen[0]*36 + gi->slen[1]*6 + gi->slen[2])*2
		    + gi->preflag+2;
		assert(0 <= part && part < 180*2);
		assert(gi->slen[3] == 0);
		break;
	    case 12: case 13: case 14:
		part = (180 + gi->slen[0]*16 + gi->slen[1]*4 + gi->slen[2])*2
		    + gi->preflag+2;
		assert(180*2 <= part && part < 244*2);
		assert(gi->slen[3] == 0);
		break;
	    case 15: case 16: case 17:
		part = (244 + gi->slen[0]*3 + gi->slen[1])*2 + gi->preflag+2;
		assert(244*2 <= part && part < 512);
		assert(gi->slen[2] == 0);
		assert(gi->slen[3] == 0);
		break;
	    default:
		part = 0;
		assert(0);
	    }
	    ptr = writeheader(p, part, 9, ptr);
	    ptr = writeTableHeader(gfc, gi, ptr);
	    ptr = writeheader(p, gi->scalefac_scale,     1, ptr);
	    ptr = writeheader(p, gi->count1table_select, 1, ptr);

	    for (partition = 0; partition < 4; partition++) {
		int sfbend
		    = sfb + nr_of_sfb_block[gi->scalefac_compress][partition];
		int slen = gi->slen[partition];
		if (slen)
		    for (; sfb < sfbend; sfb++)
			putbits24(&gfc->bs, Max(gi->scalefac[sfb], 0U), slen);
		sfb = sfbend;
	    }
	    assert(data_bits == gfc->bs.bitidx);
	    Huffmancodebits(gfc, gi);
	} /* for ch */
    } /* for MPEG version */
    assert(ptr == l3_side->sideinfo_len * 8);

    if (gfp->error_protection) {
	/* (jo) error_protection: add crc16 information to header */
	CRC_writeheader(gfc, gfc->bs.header[gfc->bs.h_ptr].buf);
    }

    ptr = gfc->bs.h_ptr;
    gfc->bs.h_ptr = (ptr + 1) & (MAX_HEADER_BUF-1);
    assert(gfc->bs.h_ptr != gfc->bs.w_ptr);

    gfc->bs.header[gfc->bs.h_ptr].write_timing
	= gfc->bs.header[ptr].write_timing + getframebytes(gfp);
}



/* compute the number of bits required to flush all mp3 frames
   currently in the buffer.  This should be the same as the
   reservoir size.  Only call this routine between frames - i.e.
   only after all headers and data have been added to the buffer
   by format_bitstream().

   Also compute total_bits_output = 
       size of mp3 buffer (including frame headers which may not
       have yet been send to the mp3 buffer) + 
       number of bits needed to flush all mp3 frames.

   total_bytes_output is the size of the mp3 output buffer if 
   lame_encode_flush_nogap() was called right now. 

 */
int
compute_flushbits( const lame_global_flags * gfp, int *total_bytes_output )
{
    lame_internal_flags *gfc=gfp->internal_flags;
    Bit_stream_struc *bs = &gfc->bs;
    int flushbits;
    int bitsPerFrame;
    int last_ptr = (bs->h_ptr - 1) & (MAX_HEADER_BUF-1);

    /* add this many bits to bitstream so we can flush all headers */
    flushbits = (bs->header[last_ptr].write_timing - bs->totbyte)*8;
    *total_bytes_output = flushbits;

    if (flushbits >= 0) {
	/* if flushbits >= 0, some headers have not yet been written */
	/* add the size of the headers to the total byte output */
	*total_bytes_output += 8 * gfc->l3_side.sideinfo_len
	    * ((bs->h_ptr - bs->w_ptr) & (MAX_HEADER_BUF-1));
    }

    /* finally, add some bits so that the last frame is complete
     * these bits are not necessary to decode the last frame, but
     * some decoders will ignore last frame if these bits are missing 
     */
    bitsPerFrame = getframebytes(gfp)*8;
    flushbits += bitsPerFrame;
    assert(flushbits >= 0);
    *total_bytes_output
	= (*total_bytes_output + bitsPerFrame + bs->bitidx + 7) / 8;
    return flushbits;
}

int
flush_bitstream(
    lame_global_flags *gfp,unsigned char *buffer,int size,int mp3data)
{
    III_side_info_t *l3_side = &gfp->internal_flags->l3_side;
    int dummy, flushbits;

    if ((flushbits = compute_flushbits(gfp,&dummy)) >= 0) {
	drain_into_ancillary(gfp->internal_flags, flushbits);
	/* we have padded out all frames with ancillary data, which is the
	   same as filling the bitreservoir with ancillary data, so : */
	l3_side->ResvSize=l3_side->main_data_begin = 0;
    }

    return copy_buffer(gfp->internal_flags, buffer, size, mp3data);
}

void  add_dummy_byte (lame_internal_flags* gfc, unsigned char val )
{
    Bit_stream_struc *bs = &gfc->bs;
    int i;
    assert((bs->bitidx & 7) == 0);
    bs->buf[bs->bitidx>>3] = val;
    bs->bitidx += 8;
    for (i = 0; i < MAX_HEADER_BUF; i++)
	bs->header[i].write_timing++;
}

/*
  format_bitstream()

  This is called after a frame of audio has been quantized and coded.
  It will write the encoded audio to the bitstream. Note that
  from a layer3 encoder's perspective the bit stream is primarily
  a series of main_data() blocks, with header and side information
  inserted at the proper locations to maintain framing. (See Figure A.7
  in the IS).
  */
int
format_bitstream(lame_global_flags *gfp)
{
    lame_internal_flags *gfc=gfp->internal_flags;
    III_side_info_t *l3_side = &gfc->l3_side;
    int drainPre, drainbits = l3_side->ResvSize % 8;

    /* reservoir is overflowed ? */
    if (drainbits < l3_side->ResvSize - l3_side->ResvMax)
	drainbits = l3_side->ResvSize - l3_side->ResvMax;
    assert(drainbits >= 0);

    /* drain as many bits as possible into previous frame ancillary data
     * In particular, in VBR mode ResvMax may have changed, and we have
     * to make sure main_data_begin does not create a reservoir bigger
     * than ResvMax  mt 4/00*/
    drainPre = drainbits & (~7U);
    if (drainPre > l3_side->main_data_begin*8)
	drainPre = l3_side->main_data_begin*8;

    drain_into_ancillary(gfc, drainPre);
    encodeBitStream(gfp);
    gfc->bs.bitidx += drainbits - drainPre;

    l3_side->main_data_begin = (l3_side->ResvSize -= drainbits) / 8;

    assert(gfc->bs.bitidx % 8 == 0);
    assert(l3_side->ResvSize % 8 == 0);
    return 0;
}





/* copy data out of the internal MP3 bit buffer into a user supplied
   unsigned char buffer.

   mp3data=0      indicates data in buffer is an id3tags and VBR tags
   mp3data=1      data is real mp3 frame data. 
*/
int
copy_buffer(
    lame_internal_flags *gfc, unsigned char *buffer, int size, int mp3data)
{
    Bit_stream_struc *bs=&gfc->bs;
    int spec_idx, minimum;
    unsigned char *pbuf, *pend;
    if (bs->bitidx <= 0)
	return 0;

    /* interleave the sideinfo and spectrum data */
    pbuf = buffer;
    if (size == 0)
	size = 65535;
    pend = &pbuf[size];

    assert(bs->bitidx % 8 == 0);
    bs->bitidx >>= 3;
    spec_idx = 0;
    do {
	if (bs->totbyte + spec_idx == bs->header[bs->w_ptr].write_timing) {
	    int i = gfc->l3_side.sideinfo_len;
	    if (pbuf+i >= pend) return -1; /* buffer is too small */
	    memcpy(pbuf, bs->header[bs->w_ptr].buf, i);
	    pbuf += i;
	    bs->w_ptr = (bs->w_ptr + 1) & (MAX_HEADER_BUF-1);
	}
	if (pbuf >= pend) return -1; /* buffer is too small */
	*pbuf++ = bs->buf[spec_idx++];
    } while (spec_idx < bs->bitidx);
    bs->totbyte += spec_idx;

    memset(bs->buf, 0, spec_idx);
    bs->bitidx = 0;
    minimum = pbuf - buffer;

    if (mp3data) {
	UpdateMusicCRC(&gfc->nMusicCRC, buffer, minimum);
#ifdef DECODE_ON_THE_FLY
	/* this is untested code;
	   if, for somereason, we would like to decode the frame: */
	short int pcm_out[2][1152];
	int mp3out;
	do {
	    /* re-synthesis to pcm.  Repeat until we get a mp3out=0 */
	    mp3out=lame_decode1(buffer,minimum,pcm_out[0],pcm_out[1]); 
	    /* mp3out = 0:  need more data to decode */
	    /* mp3out = -1:  error.  Lets assume 0 pcm output */
	    /* mp3out = number of samples output */
	    if (mp3out==-1) {
		/* error decoding.  Not fatal, but might screw up are
		 * ReplayVolume Info tag.  
		 * what should we do?  ignore for now */
		mp3out=0;
	    }
	    if (mp3out>0 && mp3out>1152) {
		/* this should not be possible, and indicates we have
		 * overflowed the pcm_out buffer.  Fatal error. */
		return -6;
	    }
	} while (mp3out>0);
#endif
    }
    return minimum;
}
