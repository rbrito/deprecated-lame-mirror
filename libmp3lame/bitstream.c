/*
 *	MP3 bitstream Output interface for LAME
 *
 *	Copyright (c) 1999 Takehiro TOMINAGA
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
#include <config.h>
#endif

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include "tables.h"
#include "bitstream.h"
#include "quantize.h"
#include "quantize_pvt.h"
#include "version.h"

/* This is the scfsi_band table from 2.4.2.7 of the IS */
const int scfsi_band[5] = { 0, 6, 11, 16, 21 };


/* unsigned int is at least this large:  */
/* we work with ints, so when doing bit manipulation, we limit
 * ourselves to MAX_LENGTH-2 just to be on the safe side */
#define MAX_LENGTH      32  



#ifdef DEBUG
static int hoge, hogege;
#endif





void putheader_bits(context * const gfc, int w_ptr)
{
    Bit_stream_struc *bs;
    bs = &gfc->bs;
#ifdef DEBUG
    hoge += gfc->sideinfo_len * 8;
    hogege += gfc->sideinfo_len * 8;
#endif
    memcpy(&bs->buf[bs->buf_byte_idx], gfc->header[gfc->w_ptr].buf,
	   gfc->sideinfo_len);
    bs->buf_byte_idx += gfc->sideinfo_len;
    bs->totbit += gfc->sideinfo_len * 8;
    gfc->w_ptr = (gfc->w_ptr + 1) & (MAX_HEADER_BUF - 1);
}




/*write j bits into the bit stream */
static INLINE void
putbits(context * const gfc, int val, int j)
{
    Bit_stream_struc *bs;
    bs = &gfc->bs;

    assert(j < MAX_LENGTH-2);

    while (j > 0) {
	int k;
	if (bs->buf_bit_idx == 0) {
	    bs->buf_bit_idx = 8;
	    bs->buf_byte_idx++;
	    assert(bs->buf_byte_idx < BUFFER_SIZE);
	    assert(gfc->header[gfc->w_ptr].write_timing >= bs->totbit);
	    if (gfc->header[gfc->w_ptr].write_timing == bs->totbit) {
	      putheader_bits(gfc,gfc->w_ptr);
	    }
	    bs->buf[bs->buf_byte_idx] = 0;
	}

	k = Min(j, bs->buf_bit_idx);
	j -= k;
        
	bs->buf_bit_idx -= k;
        
	assert (j < MAX_LENGTH); /* 32 too large on 32 bit machines */
        assert (bs->buf_bit_idx < MAX_LENGTH); 
	
        bs->buf[bs->buf_byte_idx] |= ((val >> j) << bs->buf_bit_idx);
	bs->totbit += k;
    }
}

/*write j bits into the bit stream, ignoreing frame headers */
static INLINE void
putbits_noheaders(context * const gfc, int val, int j)
{
    Bit_stream_struc *bs;
    bs = &gfc->bs;

    assert(j < MAX_LENGTH-2);

    while (j > 0) {
	int k;
	if (bs->buf_bit_idx == 0) {
	    bs->buf_bit_idx = 8;
	    bs->buf_byte_idx++;
	    assert(bs->buf_byte_idx < BUFFER_SIZE);
	    bs->buf[bs->buf_byte_idx] = 0;
	}

	k = Min(j, bs->buf_bit_idx);
	j -= k;
        
	bs->buf_bit_idx -= k;
        
	assert (j < MAX_LENGTH); /* 32 too large on 32 bit machines */
        assert (bs->buf_bit_idx < MAX_LENGTH); 
	
        bs->buf[bs->buf_byte_idx] |= ((val >> j) << bs->buf_bit_idx);
	bs->totbit += k;
    }
}

/*
  Some combinations of bitrate, Fs, and stereo make it impossible to stuff
  out a frame using just main_data, due to the limited number of bits to
  indicate main_data_length. In these situations, we put stuffing bits into
  the ancillary data...
*/

static INLINE void 
drain_into_ancillary ( context * const gfc, int remainingBits )
{
    char                  buffer [80];
    char*                 p;
    
    assert (remainingBits >= 0);
    
    sprintf ( p = buffer, "LAME%s", get_lame_short_version () );

    for (; *p != '\0'  &&  remainingBits >= 8; p++, remainingBits -= 8 )
        putbits ( gfc, *p, 8 );

    for (; remainingBits >= 1; remainingBits -= 1 ) {
        putbits ( gfc, gfc->ancillary_flag, 1 );
        gfc->ancillary_flag ^= 1;
    }

    assert (remainingBits == 0);
}


static void  
writeheader ( context * const gfc, const unsigned value, size_t bitcount )
{
    static unsigned  tab [2] = { 0, CRC16_POLYNOMIAL };
    int              i;
    int              k;

    assert (bitcount < MAX_LENGTH-2  &&  value < (1lu << bitcount) );

    // Update CRC value
    for ( i = bitcount; i-- > 0; )
        gfc->crcvalue = (gfc->crcvalue << 1) ^ tab [ ((gfc->crcvalue >> 15) ^ (value >> i)) & 1 ];
      
    // Poke value into bitbuffer
    i = gfc->header[gfc->h_ptr].ptr;
    while ( 1 ) {
        if (bitcount < 8 - (i & 7)) {
	    gfc->header [gfc->h_ptr].buf [i >> 3] |= value << (8 - (i & 7) - bitcount);
	    i += bitcount;
	    break;
	} else {
	    bitcount -= (k = 8 - (i & 7));
	    gfc->header [gfc->h_ptr].buf [i >> 3] |= (value >> bitcount) << (8 - (i & 7) - k);
	    i += k;
	}
    }
    gfc->header[gfc->h_ptr].ptr = i;
}

static void  writedummy ( context * const gfc, size_t bitcount )
{
	gfc->header[gfc->h_ptr].ptr += bitcount;   /* dummy bits without updating CRC */
}


static INLINE void
encodeSideInfo2(context * const gfc, int bitsPerFrame)
{
    III_side_info_t *l3_side;
    int gr, ch, band, old;
    
    l3_side = &gfc->l3_side;
    gfc->header[gfc->h_ptr].ptr = 0;
    memset(gfc->header[gfc->h_ptr].buf, 0, gfc->sideinfo_len);
    
    writeheader ( gfc, gfc->gfp->out_samplerate >= 16000 ? 0xfff : 0xffe, 12 );
    writeheader ( gfc, gfc->gfp->version          , 1 );
    writeheader ( gfc, 4 - 3                 , 2 );
    writeheader ( gfc, !gfc->gfp->error_protection, 1 );

    /* Reset CRC generator */
    gfc->crcvalue = 0xFFFF;
    writeheader ( gfc, gfc->bitrate_index    , 4 );
    writeheader ( gfc, gfc->samplerate_index , 2 );
    writeheader ( gfc, gfc->padding          , 1 );
    writeheader ( gfc, gfc->gfp->extension        , 1 );
    writeheader ( gfc, gfc->gfp->mode             , 2 );
    writeheader ( gfc, gfc->mode_ext         , 2 );
    writeheader ( gfc, gfc->gfp->copyright        , 1 );
    writeheader ( gfc, gfc->gfp->original         , 1 );
    writeheader ( gfc, gfc->gfp->emphasis         , 2 );
    if (gfc->gfp->error_protection)
        writedummy ( gfc, 16 );

    if ( gfc->gfp->version == 1 ) { /* MPEG1 */
    
	assert(l3_side->main_data_begin >= 0);
	writeheader ( gfc, l3_side->main_data_begin         , 9 );
	writeheader ( gfc, l3_side->private_bits            , gfc->stereo == 2  ?  3  :  5 );

	for (ch = 0; ch < gfc->stereo; ch++) {
	    for (band = 0; band < 4; band++) {
		writeheader ( gfc, l3_side->scfsi[ch][band] , 1 );
	    }
	}

	for (gr = 0; gr < 2; gr++) {
	    for (ch = 0; ch < gfc->stereo; ch++) {
		gr_info *gi = &l3_side->gr[gr].ch[ch].tt;
		writeheader ( gfc, gi->part2_3_length       ,12 );
		writeheader ( gfc, gi->big_values / 2       , 9 );
		writeheader ( gfc, gi->global_gain          , 8 );
		writeheader ( gfc, gi->scalefac_compress    , 4 );
		writeheader ( gfc, gi->window_switching_flag, 1 );

		if (gi->window_switching_flag) {
		    writeheader ( gfc, gi->block_type       , 2 );
		    writeheader ( gfc, gi->mixed_block_flag , 1 );

		    if (gi->table_select[0] == 14)
			gi->table_select[0] = 16;
		    writeheader ( gfc, gi->table_select[0]  , 5 );
		    if (gi->table_select[1] == 14)
			gi->table_select[1] = 16;
		    writeheader ( gfc, gi->table_select[1]  , 5 );

		    writeheader ( gfc, gi->subblock_gain[0] , 3 );
		    writeheader ( gfc, gi->subblock_gain[1] , 3 );
		    writeheader ( gfc, gi->subblock_gain[2] , 3 );
		} else {
		    assert(gi->block_type == NORM_TYPE);
		    if (gi->table_select[0] == 14)
			gi->table_select[0] = 16;
		    writeheader ( gfc, gi->table_select[0]  , 5 );
		    if (gi->table_select[1] == 14)
			gi->table_select[1] = 16;
		    writeheader ( gfc, gi->table_select[1]  , 5 );
		    if (gi->table_select[2] == 14)
			gi->table_select[2] = 16;
		    writeheader ( gfc, gi->table_select[2]  , 5 );

		    assert(gi->region0_count < 16U);
		    assert(gi->region1_count < 8U);
		    writeheader ( gfc, gi->region0_count    , 4 );
		    writeheader ( gfc, gi->region1_count    , 3 );
		}
		writeheader ( gfc, gi->preflag              , 1 );
		writeheader ( gfc, gi->scalefac_scale       , 1 );
		writeheader ( gfc, gi->count1table_select   , 1 );
	    }
	}
    }
    else { /* MPEG2 */
	
	assert(l3_side->main_data_begin >= 0);
	writeheader ( gfc, l3_side->main_data_begin         , 8 );
	writeheader ( gfc, l3_side->private_bits            , gfc->stereo );

	gr = 0;
	for (ch = 0; ch < gfc->stereo; ch++) {
	    gr_info *gi = &l3_side->gr[gr].ch[ch].tt;
	    writeheader ( gfc, gi->part2_3_length           ,12 );
	    writeheader ( gfc, gi->big_values / 2           , 9 );
	    writeheader ( gfc, gi->global_gain              , 8 );
	    writeheader ( gfc, gi->scalefac_compress        , 9 );
	    writeheader ( gfc, gi->window_switching_flag    , 1 );

	    if (gi->window_switching_flag) {
		writeheader ( gfc, gi->block_type           , 2 );
		writeheader ( gfc, gi->mixed_block_flag     , 1 );

		if (gi->table_select[0] == 14)
		    gi->table_select[0] = 16;
		writeheader ( gfc, gi->table_select[0]      , 5 );
		if (gi->table_select[1] == 14)
		    gi->table_select[1] = 16;
		writeheader ( gfc, gi->table_select [1]     , 5 );

		writeheader ( gfc, gi->subblock_gain [0]    , 3 );
		writeheader ( gfc, gi->subblock_gain [1]    , 3 );
		writeheader ( gfc, gi->subblock_gain [2]    , 3 );
	    } else {
		if (gi->table_select[0] == 14)
		    gi->table_select[0] = 16;
		writeheader ( gfc, gi->table_select [0]     , 5 );
		if (gi->table_select[1] == 14)
		    gi->table_select[1] = 16;
		writeheader ( gfc, gi->table_select [1]     , 5 );
		if (gi->table_select[2] == 14)
		    gi->table_select[2] = 16;
		writeheader ( gfc, gi->table_select [2]     , 5 );

		assert(gi->region0_count < 16U);
		assert(gi->region1_count < 8U);
		writeheader ( gfc, gi->region0_count        , 4 );
		writeheader ( gfc, gi->region1_count        , 3 );
	    }

	    writeheader ( gfc, gi->scalefac_scale           , 1 );
	    writeheader ( gfc, gi->count1table_select       , 1 );
	}
    }

    if ( gfc->gfp->error_protection ) {
	/* (jo) error_protection: add crc16 information to header */
	gfc->header[gfc->h_ptr].buf [4] = (gfc->crcvalue >> 8) & 0xFF;
	gfc->header[gfc->h_ptr].buf [5] = (gfc->crcvalue >> 0) & 0xFF;
    }


    old = gfc->h_ptr;
    assert (gfc->header[old].ptr == gfc->sideinfo_len * 8);

    gfc->h_ptr                           = (old + 1) & (MAX_HEADER_BUF - 1);
    gfc->header[gfc->h_ptr].write_timing = gfc->header[old].write_timing + bitsPerFrame;

    if ( gfc->h_ptr == gfc->w_ptr ) { /* yikes! we are out of header buffer space */
        ERRORF("Error: MAX_HEADER_BUF too small in bitstream.c \n");
    }
}



static INLINE int
huffman_coder_count1(context * const gfc, int *ix, gr_info *gi)
{
    /* Write count1 area */
    const struct huffcodetab *h = &ht[gi->count1table_select + 32];
    int i,bits=0;
#ifdef DEBUG
    int gegebo = gfc->bs.totbit;
#endif

    ix += gi->big_values;
    assert(gi->count1table_select < 2);


    for (i = (gi->count1 - gi->big_values) / 4; i > 0; --i) {
	int huffbits = 0;
	int p = 0, v;

	v = ix[0];
	if (v) {
	    p += 8;
	    if (v < 0)
		huffbits++;
	    assert(-1 <= v && v <= 1);
	}

	v = ix[1];
	if (v) {
	    p += 4;
	    huffbits *= 2;
	    if (v < 0)
		huffbits++;
	    assert(-1 <= v && v <= 1);
	}

	v = ix[2];
	if (v) {
	    p += 2;
	    huffbits *= 2;
	    if (v < 0)
		huffbits++;
	    assert(-1 <= v && v <= 1);
	}

	v = ix[3];
	if (v) {
	    p++;
	    huffbits *= 2;
	    if (v < 0)
		huffbits++;
	    assert(-1 <= v && v <= 1);
	}

	ix += 4;
	putbits(gfc, huffbits + h->table[p], h->hlen[p]);
	bits += h->hlen[p];
    }
#ifdef DEBUG
    DEBUGF("%ld %d %d %d\n",gfc->bs.totbit -gegebo, gi->count1bits, gi->big_values, gi->count1);
#endif
    return bits;
}

/*
  Implements the pseudocode of page 98 of the IS
  */

static INLINE int
HuffmanCode(context * const gfc, int table_select, int x, int y)
{
    int code, ext, xlen;
    int cbits, xbits;
    int signx, signy, linbits;
    const struct huffcodetab *h;

    cbits = 0;
    xbits = 0;
    code  = 0;
    signx = signy = 0;

    if (x < 0) {
	signx++;
	x = -x;
    }

    if (y < 0) {
	signy++;
	y = -y;
    }

    assert(table_select>0);
    h = &(ht[table_select]);
    linbits = h->xlen;
    ext = signx;
    xlen = h->xlen;

    if (table_select > 15) {
	/* use ESC-words */
	if (x > 14) {
	    int linbitsx = x - 15;
	    assert(linbitsx <= h->linmax);
	    ext |= linbitsx << 1;
	    xbits = linbits;
	    x = 15;
	}

	if (y > 14) {
	    int linbitsy = y - 15;
	    assert(linbitsy <= h->linmax);
	    ext <<= linbits;
	    ext |= linbitsy;
	    xbits += linbits;
	    y = 15;
	}
	xlen = 16;
    }

    if (x != 0) {
	cbits--;
    }

    if (y != 0) {
	ext <<= 1;
	ext |= signy;
	cbits--;
    }

    xbits -= cbits;

    assert(x <= 15);
    assert(y <= 15);
    assert(x >= 0);
    assert(y >= 0);

    x = x * xlen + y;

    code = h->table[x];
    cbits += h->hlen[x];

    assert(cbits <= MAX_LENGTH);
    assert(xbits <= MAX_LENGTH);

    putbits(gfc,code, cbits);
    putbits(gfc, ext, xbits);
    return cbits+xbits;
}

static int
Huffmancodebits(context * const gfc, int tableindex, int start, int end, int *ix)
{
    int i,bits;

    assert(tableindex < 32);
    if (!tableindex) return 0;

    bits=0;
    for (i = start; i < end; i += 2) {
      bits += HuffmanCode(gfc, tableindex, ix[i], ix[i + 1]);
    }
    return bits;
}



/*
  Note the discussion of huffmancodebits() on pages 28
  and 29 of the IS, as well as the definitions of the side
  information on pages 26 and 27.
  */
static int
ShortHuffmancodebits(context * const gfc, int *ix, gr_info *gi)
{
    int bits;
    int region1Start;
    
    region1Start=3*gfc->scalefac_band.s[3];
    if (region1Start > gi->big_values) 	region1Start = gi->big_values;

    /* short blocks do not have a region2 */
    bits  = Huffmancodebits(gfc,gi->table_select[0], 0, region1Start, ix);
    bits += Huffmancodebits(gfc,gi->table_select[1], region1Start, gi->big_values, ix);
    return bits;
}

static int
LongHuffmancodebits(context * const gfc,int *ix, gr_info *gi)
{
    int i, bigvalues,bits=0;
    int region1Start, region2Start;

    bigvalues = gi->big_values;
    assert(0 <= bigvalues && bigvalues <= 576);

    i = gi->region0_count + 1;
    assert(i < 23);
    region1Start = gfc->scalefac_band.l[i];
    i += gi->region1_count + 1;
    assert(i < 23);
    region2Start = gfc->scalefac_band.l[i];

    if (region1Start > bigvalues)
	region1Start = bigvalues;

    if (region2Start > bigvalues)
	region2Start = bigvalues;

    bits +=Huffmancodebits(gfc,gi->table_select[0], 0, region1Start, ix);
    bits +=Huffmancodebits(gfc,gi->table_select[1], region1Start, region2Start, ix);
    bits +=Huffmancodebits(gfc,gi->table_select[2], region2Start, bigvalues, ix);
    return bits;
}

static INLINE int
writeMainData(context * const gfc,
      int              l3_enc[2][2][576],
  	III_scalefac_t   scalefac[2][2] )
{
    int gr, ch, sfb,data_bits,scale_bits,tot_bits=0;
    III_side_info_t *l3_side;

    l3_side = &gfc->l3_side;
    if (gfc->gfp->version == 1) {
	/* MPEG 1 */
	for (gr = 0; gr < 2; gr++) {
	    for (ch = 0; ch < gfc->stereo; ch++) {
		gr_info *gi = &l3_side->gr[gr].ch[ch].tt;
		int slen1 = slen1_tab[gi->scalefac_compress];
		int slen2 = slen2_tab[gi->scalefac_compress];
		data_bits=0;
		scale_bits=0;

#ifdef DEBUG
		hogege = gfc->bs.totbit;
#endif
		if (gi->block_type == SHORT_TYPE) {
		    for (sfb = 0; sfb < SBPSY_s; sfb++) {
			int slen = sfb < 6 ? slen1 : slen2;

			assert(scalefac[gr][ch].s[sfb][0]>=0);
			assert(scalefac[gr][ch].s[sfb][1]>=0);
			assert(scalefac[gr][ch].s[sfb][2]>=0);

			putbits(gfc,scalefac[gr][ch].s[sfb][0], slen);
			putbits(gfc,scalefac[gr][ch].s[sfb][1], slen);
			putbits(gfc,scalefac[gr][ch].s[sfb][2], slen);
			scale_bits += 3*slen;
		    }
		    data_bits += ShortHuffmancodebits(gfc,l3_enc[gr][ch], gi);
		} else {
		    int i;
		    for (i = 0; i < sizeof(scfsi_band) / sizeof(int) - 1;
			 i++) {
			if (gr != 0 && l3_side->scfsi[ch][i])
			    continue;

			for (sfb = scfsi_band[i]; sfb < scfsi_band[i + 1];
			     sfb++) {

			    assert(scalefac[gr][ch].l[sfb]>=0);
			    putbits(gfc,scalefac[gr][ch].l[sfb],
				    sfb < 11 ? slen1 : slen2);
			    scale_bits += sfb < 11 ? slen1 : slen2;
			}
		    }
		    data_bits +=LongHuffmancodebits(gfc,l3_enc[gr][ch], gi);
		}
		data_bits +=huffman_coder_count1(gfc,l3_enc[gr][ch], gi);
#ifdef DEBUG
		DEBUGF("<%ld> ", gfc->bs.totbit-hogege);
#endif
		/* does bitcount in quantize.c agree with actual bit count?*/
		assert(data_bits==gi->part2_3_length-gi->part2_length);
		assert(scale_bits==gi->part2_length);
		tot_bits += scale_bits + data_bits;

	    } /* for ch */
	} /* for gr */
    } else {
	/* MPEG 2 */
	gr = 0;
	for (ch = 0; ch < gfc->stereo; ch++) {
	    gr_info *gi = &l3_side->gr[gr].ch[ch].tt;
	    int i, sfb_partition;
	    assert(gi->sfb_partition_table);
	    data_bits = 0;
	    scale_bits=0;

	    sfb = 0;
	    sfb_partition = 0;
	    if (gi->block_type == SHORT_TYPE) {
		for (; sfb_partition < 4; sfb_partition++) {
		    int sfbs = gi->sfb_partition_table[sfb_partition] / 3;
		    int slen = gi->slen[sfb_partition];
		    for (i = 0; i < sfbs; i++, sfb++) {
			putbits(gfc,Max(scalefac[gr][ch].s[sfb][0], 0U), slen);
			putbits(gfc,Max(scalefac[gr][ch].s[sfb][1], 0U), slen);
			putbits(gfc,Max(scalefac[gr][ch].s[sfb][2], 0U), slen);
			scale_bits += 3*slen;
		    }
		}
		data_bits += ShortHuffmancodebits(gfc,l3_enc[gr][ch], gi);
	    } else {
		for (; sfb_partition < 4; sfb_partition++) {
		    int sfbs = gi->sfb_partition_table[sfb_partition];
		    int slen = gi->slen[sfb_partition];
		    for (i = 0; i < sfbs; i++, sfb++) {
			putbits(gfc,Max(scalefac[gr][ch].l[sfb], 0U), slen);
			scale_bits += slen;
		    }
		}
		data_bits +=LongHuffmancodebits(gfc,l3_enc[gr][ch], gi);
	    }
	    data_bits +=huffman_coder_count1(gfc,l3_enc[gr][ch], gi);

	    /* does bitcount in quantize.c agree with actual bit count?*/
	    assert(data_bits==gi->part2_3_length-gi->part2_length);
	    assert(scale_bits==gi->part2_length);
	    tot_bits += scale_bits + data_bits;
	} /* for ch */
    } /* for gf */
    return tot_bits;
} /* main_data */




void
flush_bitstream(context * const gfc)
{
  int flushbits,remaining_headers;
  int bitsPerFrame, mean_bits;
  int last_ptr,first_ptr;
  first_ptr=gfc->w_ptr;           /* first header to add to bitstream */
  last_ptr = gfc->h_ptr - 1;   /* last header to add to bitstream */
  if (last_ptr==-1) last_ptr=MAX_HEADER_BUF-1;   

  /* add this many bits to bitstream so we can flush all headers */
  flushbits = gfc->header[last_ptr].write_timing - gfc->bs.totbit;

  if (flushbits >= 0) {
    /* if flushbits >= 0, some headers have not yet been written */
    /* reduce flushbits by the size of the headers */
    remaining_headers= 1+last_ptr - first_ptr;
    if (last_ptr < first_ptr) 
      remaining_headers= 1+last_ptr - first_ptr + MAX_HEADER_BUF;
    flushbits -= remaining_headers*8*gfc->sideinfo_len;
  }


  /* finally, add some bits so that the last frame is complete
   * these bits are not necessary to decode the last frame, but
   * some decoders will ignore last frame if these bits are missing 
   */
  getframebits(gfc, &bitsPerFrame, &mean_bits);
  flushbits += bitsPerFrame;
  if (flushbits<0) {
#if 0
    /* if flushbits < 0, this would mean that the buffer looks like:
     * (data...)  last_header  (data...)  (extra data that should not be here...)
     */
    DEBUGF("last header write_timing = %i \n",gfc->header[last_ptr].write_timing);
    DEBUGF("first header write_timing = %i \n",gfc->header[first_ptr].write_timing);
    DEBUGF("bs.totbit:                 %i \n",gfc->bs.totbit);
    DEBUGF("first_ptr, last_ptr        %i %i \n",first_ptr,last_ptr);
    DEBUGF("remaining_headers =        %i \n",remaining_headers);
    DEBUGF("bitsperframe:              %i \n",bitsPerFrame);
    DEBUGF("sidelen:                   %i \n",gfc->sideinfo_len);
#endif
    ERRORF("strange error flushing buffer ... \n");
  } else {
    drain_into_ancillary(gfc,flushbits);
  }

  assert (gfc->header[last_ptr].write_timing + bitsPerFrame  == gfc->bs.totbit);
}




void add_dummy_byte(context * const gfc, int val)
{
  int i;

  putbits_noheaders(gfc,val,8);   

  for (i=0 ; i< MAX_HEADER_BUF ; ++i) 
    gfc->header[i].write_timing += 8;
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
format_bitstream(context * const gfc, int bitsPerFrame,
      int              l3_enc[2][2][576],
  	III_scalefac_t   scalefac[2][2] )
{
    int bits;
    III_side_info_t *l3_side;
    l3_side = &gfc->l3_side;

    drain_into_ancillary(gfc,l3_side->resvDrain_pre);

    encodeSideInfo2(gfc,bitsPerFrame);
    bits = 8*gfc->sideinfo_len;
    bits+=writeMainData(gfc,l3_enc,scalefac);
    drain_into_ancillary(gfc,l3_side->resvDrain_post);
    bits += l3_side->resvDrain_post;

    l3_side->main_data_begin += (bitsPerFrame-bits)/8;
    if ((l3_side->main_data_begin * 8) != gfc->ResvSize ) {
      ERRORF("bit reservoir error: \n"
             "l3_side->main_data_begin: %i \n"
             "Resvoir size:             %i \n"
             "resv drain (post)         %i \n"
             "resv drain (pre)          %i \n"
             "header and sideinfo:      %i \n"
             "data bits:                %i \n"
             "total bits:               %i (remainder: %i) \n"
             "bitsperframe:             %i \n",

             8*l3_side->main_data_begin,
             gfc->ResvSize,
             l3_side->resvDrain_post,
             l3_side->resvDrain_pre,
             8*gfc->sideinfo_len,
             bits-l3_side->resvDrain_post-8*gfc->sideinfo_len,
             bits, bits % 8,
             bitsPerFrame
      );

      gfc->ResvSize = l3_side->main_data_begin*8;
    };
    assert(gfc->bs.totbit % 8 == 0);

    if (gfc->bs.totbit > 1000000000  ) {
      /* to avoid totbit overflow, (at 8h encoding at 128kbs) lets reset bit counter*/
      int i;
      for (i=0 ; i< MAX_HEADER_BUF ; ++i) 
	gfc->header[i].write_timing -= gfc->bs.totbit;      
      gfc->bs.totbit=0;
    }
    return 0;
}





int copy_buffer(char *buffer,int size,Bit_stream_struc *bs)
{
  int minimum = bs->buf_byte_idx + 1;
  if (minimum <= 0) return 0;
  if (size!=0 && minimum>size) return -1; /* buffer is too small */
  memcpy(buffer,bs->buf,minimum);
  bs->buf_byte_idx = -1;
  bs->buf_bit_idx = 0;
  return minimum;
}





void init_bit_stream_w(context * const gfc)
{
    gfc->bs.buf = (unsigned char *)       malloc(BUFFER_SIZE);
    gfc->bs.buf_size = BUFFER_SIZE;

    gfc->h_ptr = gfc->w_ptr = 0;
    gfc->header[gfc->h_ptr].write_timing = 0;
    gfc->bs.buf_byte_idx = -1;
    gfc->bs.buf_bit_idx = 0;
    gfc->bs.totbit = 0;
   
    if (gfc->bs.buf == NULL) {
        ERRORF ("PANIC: can't allocate bs.buf buffer\n");
        exit (-1);
    }
}







