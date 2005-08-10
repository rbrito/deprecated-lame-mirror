/*
 *	MP3 huffman table selecting and bit counting
 *
 *	Copyright 1999-2005 Takehiro TOMINAGA
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
#ifdef WITH_DMALLOC
# include <dmalloc.h>
#endif
#include <assert.h>

#include "encoder.h"
#include "tables.h"
#include "quantize_pvt.h"

#if HAVE_NASM
# define ixmax(a,b) gfc->ix_max(a,b)
#else
# define ixmax(a,b) ix_max(a,b)
#endif

/* log2(x). the last element is for the case when sfb is over valid range.*/
static const char log2tab[] = {
    0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5};

/*
 *  +------------+--------------+||
 *  |            |              |||
 *  |            |              |||
 *  |            |              |||
 *  |            |              |||
 *  |            |              |||
 *  |   table    |    table     |||
 *  |    7B89    |     AFBC     |||
 *  |   (6x6)    |    (8x8)     |||
 *  |            |              |||
 *  |            |              |||
 *  |            |              |||
 *  |            |              +--
 *  +-----+----++|              |
 *   table|table||              |
 *    5967| 2834||              |
 *   (4x4)|(3x3)|||||||||||||||||
 *        +-----+|||||||||||||||+--
 *  ------+
 */
/* ixmax=4,5 (select table from 7, 11, 8, 9) */
static const uint64_t table7B89[16*10+2] = {
    u64const(0x02000300020001), u64const(0x04000400040004), u64const(0x07000600060007), u64const(0x09000700080009),
    u64const(0x09000900090009), u64const(0x0a000a000a000a),
/* here starts tableAFBC, ixmax=6,7 (select table from 10, 15, 11, 12 */
    u64const(0x02000400030001), u64const(0x04000400050004), u64const(0x06000600060007), u64const(0x08000800080009),
    u64const(0x0900090008000a), u64const(0x0a000a0009000a), u64const(0x09000a000a000a), u64const(0x0a000a000a000b),
    0,0,

    u64const(0x04000400040004), u64const(0x04000500050006), u64const(0x06000600060008), u64const(0x0a000700080009),
    u64const(0x0a0008000a0009), u64const(0x0a000a000a000a),
    u64const(0x04000400050004), u64const(0x05000500050006), u64const(0x06000600070008), u64const(0x08000700080009),
    u64const(0x0a00090009000a), u64const(0x0a00090009000b), u64const(0x09000a000a000a), u64const(0x0a000a000a000a),
    0,0,

    u64const(0x07000500060007), u64const(0x06000600070007), u64const(0x08000700080009), u64const(0x0a00080009000a),
    u64const(0x0a0009000a000a), u64const(0x0b000a000b000b),
    u64const(0x06000600060007), u64const(0x07000600070008), u64const(0x08000700070009), u64const(0x0900080008000a),
    u64const(0x0a00090009000b), u64const(0x0b000a0009000c), u64const(0x0a0009000a000b), u64const(0x0a000a000a000b),
    0,0,

    u64const(0x09000700080008), u64const(0x0a000700080009), u64const(0x0a00080009000a), u64const(0x0b0009000b000b),
    u64const(0x0b0009000a000b), u64const(0x0c000a000c000b),
    u64const(0x08000700070008), u64const(0x08000700080009), u64const(0x0900080008000a), u64const(0x0b00080009000b),
    u64const(0x0a00090009000c), u64const(0x0c000a000a000c), u64const(0x0a000a000a000b), u64const(0x0b000a000b000c),
    0,0,

    u64const(0x09000800090008), u64const(0x090008000a0009), u64const(0x0a0009000a000a), u64const(0x0b0009000b000b),
    u64const(0x0c000a000b000b), u64const(0x0c000b000c000c),
    u64const(0x09000800080009), u64const(0x0a00080008000a), u64const(0x0a00090009000b), u64const(0x0b00090009000c),
    u64const(0x0b000a000a000c), u64const(0x0c000a000a000c), u64const(0x0b000a000b000c), u64const(0x0c000b000b000c),
    0,0,

    u64const(0x0a000900090009), u64const(0x0a0009000a000a), u64const(0x0b000a000b000b), u64const(0x0b000a000c000c),
    u64const(0x0d000b000c000c), u64const(0x0d000b000d000c),
    u64const(0x0900090009000a), u64const(0x0a00090009000b), u64const(0x0b000a0009000c), u64const(0x0c000a000a000c),
    u64const(0x0c000a000a000d), u64const(0x0d000b000a000d), u64const(0x0c000a000b000c), u64const(0x0d000b000b000d),

/* here starts table5967, ixmax=3 (select table from 5, 9, 6, 7) */
    u64const(0x03000100030001), u64const(0x04000400040004), u64const(0x06000700060007), u64const(0x08000900070008),
/* here starts table2834, ixmax=2 (select table from 2, 8, 3, 4=6) */
    u64const(0x02000300020001), u64const(0x03000400040004), u64const(0x07000600070007), u64const(0xff006300630063),
    u64const(0x090009000a0009), u64const(0x0900090009000a), u64const(0x090009000a000b), u64const(0x0a000a000a000c),
    u64const(0x0b000a000a000c), u64const(0x0c000b000b000c), u64const(0x0c000b000b000d), u64const(0x0c000c000b000d),

    u64const(0x04000400040004), u64const(0x04000600050005), u64const(0x06000800060008), u64const(0x07000900070009),
    u64const(0x04000400040004), u64const(0x04000400040005), u64const(0x07000600060007), u64const(0xff006300630063),
    u64const(0x09000a000a000a), u64const(0x09000a000a000a), u64const(0x0a000a000a000b), u64const(0x0b000b000b000c),
    u64const(0x0c000b000b000c), u64const(0x0c000b000b000d), u64const(0x0c000b000b000d), u64const(0x0c000c000c000d),

    u64const(0x05000700050007), u64const(0x06000700060008), u64const(0x07000900070009), u64const(0x08000a0008000a),
    u64const(0x06000500070006), u64const(0x07000600060007), u64const(0x08000700080008), u64const(0xff006300630063),
    u64const(0xff006300630063), u64const(0xff006300630063), u64const(0xff006300630063), u64const(0xff006300630063),
    u64const(0xff006300630063), u64const(0xff006300630063), u64const(0xff006300630063), u64const(0xff006300630063),

    u64const(0x07000800070008), u64const(0x07000900070008), u64const(0x08000a00080009), u64const(0x09000b0009000a),
};


/* ixmax=8,9,...,15 (select table from 13, 14=16, 15) */
static const uint64_t tableDxEF[] = {
    u64const(0x01000300630001), u64const(0x05000500630005), u64const(0x07000600630007), u64const(0x09000800630008),
    u64const(0x0a000800630009), u64const(0x0a00090063000a), u64const(0x0b000a0063000a), u64const(0x0b000a0063000b),
    u64const(0x0c000a0063000a), u64const(0x0c000b0063000b), u64const(0x0c000b0063000c), u64const(0x0d000c0063000c),
    u64const(0x0d000c0063000d), u64const(0x0d000c0063000d), u64const(0x0e000d0063000e), u64const(0x0b000e0063000e),
    u64const(0x04000500630004), u64const(0x06000500630006), u64const(0x08000700630008), u64const(0x09000800630009),
    u64const(0x0a00090063000a), u64const(0x0b00090063000a), u64const(0x0b000a0063000b), u64const(0x0b000a0063000b),
    u64const(0x0c000a0063000b), u64const(0x0c000b0063000b), u64const(0x0c000b0063000c), u64const(0x0d000c0063000c),
    u64const(0x0e000c0063000d), u64const(0x0d000c0063000e), u64const(0x0e000d0063000e), u64const(0x0b000d0063000e),
    u64const(0x07000600630007), u64const(0x08000700630008), u64const(0x09000700630009), u64const(0x0a00080063000a),
    u64const(0x0b00090063000b), u64const(0x0b00090063000b), u64const(0x0c000a0063000c), u64const(0x0c000a0063000c),
    u64const(0x0d000a0063000b), u64const(0x0c000b0063000c), u64const(0x0d000b0063000c), u64const(0x0d000c0063000d),
    u64const(0x0d000c0063000d), u64const(0x0e000d0063000e), u64const(0x0e000d0063000f), u64const(0x0c000d0063000f),
    u64const(0x09000700630008), u64const(0x09000800630009), u64const(0x0a00080063000a), u64const(0x0b00090063000b),
    u64const(0x0b00090063000b), u64const(0x0c000a0063000c), u64const(0x0c000a0063000c), u64const(0x0c000b0063000c),
    u64const(0x0d000b0063000c), u64const(0x0d000b0063000d), u64const(0x0e000c0063000d), u64const(0x0e000c0063000d),
    u64const(0x0e000c0063000d), u64const(0x0f000d0063000e), u64const(0x0f000d0063000f), u64const(0x0d000d0063000f),
    u64const(0x0a000800630009), u64const(0x0a000800630009), u64const(0x0b00090063000b), u64const(0x0b00090063000b),
    u64const(0x0c000a0063000c), u64const(0x0c000a0063000c), u64const(0x0d000b0063000d), u64const(0x0d000b0063000d),
    u64const(0x0d000b0063000c), u64const(0x0e000b0063000d), u64const(0x0e000c0063000d), u64const(0x0e000c0063000e),
    u64const(0x0f000c0063000e), u64const(0x0f000d0063000f), u64const(0x0f000d0063000f), u64const(0x0c000d00630010),
    u64const(0x0a00090063000a), u64const(0x0a00090063000a), u64const(0x0b00090063000b), u64const(0x0b000a0063000c),
    u64const(0x0c000a0063000c), u64const(0x0d000a0063000c), u64const(0x0d000b0063000d), u64const(0x0e000b0063000d),
    u64const(0x0d000b0063000d), u64const(0x0e000b0063000d), u64const(0x0e000c0063000e), u64const(0x0f000c0063000d),
    u64const(0x0f000d0063000f), u64const(0x0f000d0063000f), u64const(0x10000d00630010), u64const(0x0d000e00630010),
    u64const(0x0b000a0063000a), u64const(0x0b00090063000b), u64const(0x0b000a0063000c), u64const(0x0c000a0063000c),
    u64const(0x0d000a0063000d), u64const(0x0d000b0063000d), u64const(0x0d000b0063000d), u64const(0x0d000b0063000d),
    u64const(0x0e000b0063000d), u64const(0x0e000c0063000e), u64const(0x0e000c0063000e), u64const(0x0e000c0063000e),
    u64const(0x0f000d0063000f), u64const(0x0f000d0063000f), u64const(0x10000e00630010), u64const(0x0d000e00630010),
    u64const(0x0b000a0063000b), u64const(0x0b000a0063000b), u64const(0x0c000a0063000c), u64const(0x0c000b0063000d),
    u64const(0x0d000b0063000d), u64const(0x0d000b0063000d), u64const(0x0d000b0063000e), u64const(0x0e000c0063000e),
    u64const(0x0e000c0063000e), u64const(0x0f000c0063000e), u64const(0x0f000c0063000f), u64const(0x0f000c0063000f),
    u64const(0x0f000d0063000f), u64const(0x11000d00630010), u64const(0x11000d00630012), u64const(0x0d000e00630012),
    u64const(0x0b000a0063000a), u64const(0x0c000a0063000a), u64const(0x0c000a0063000b), u64const(0x0d000b0063000c),
    u64const(0x0d000b0063000c), u64const(0x0d000b0063000d), u64const(0x0e000b0063000d), u64const(0x0e000c0063000e),
    u64const(0x0f000c0063000e), u64const(0x0f000c0063000e), u64const(0x0f000c0063000e), u64const(0x0f000d0063000f),
    u64const(0x10000d0063000f), u64const(0x10000e00630010), u64const(0x10000e00630011), u64const(0x0d000e00630011),
    u64const(0x0c000a0063000b), u64const(0x0c000a0063000b), u64const(0x0c000b0063000c), u64const(0x0d000b0063000c),
    u64const(0x0d000b0063000d), u64const(0x0e000b0063000d), u64const(0x0e000c0063000d), u64const(0x0f000c0063000f),
    u64const(0x0f000c0063000e), u64const(0x0f000d0063000f), u64const(0x0f000d0063000f), u64const(0x10000d00630010),
    u64const(0x0f000d00630010), u64const(0x10000e00630010), u64const(0x0f000e00630012), u64const(0x0e000e00630011),
    u64const(0x0c000b0063000b), u64const(0x0d000b0063000c), u64const(0x0c000b0063000c), u64const(0x0d000b0063000d),
    u64const(0x0e000c0063000d), u64const(0x0e000c0063000e), u64const(0x0e000c0063000e), u64const(0x0e000c0063000f),
    u64const(0x0f000c0063000e), u64const(0x10000d0063000f), u64const(0x10000d00630010), u64const(0x10000d0063000f),
    u64const(0x11000d00630010), u64const(0x11000e00630011), u64const(0x10000f00630012), u64const(0x0d000e00630013),
    u64const(0x0d000b0063000c), u64const(0x0d000b0063000c), u64const(0x0d000b0063000c), u64const(0x0d000b0063000d),
    u64const(0x0e000c0063000e), u64const(0x0e000c0063000e), u64const(0x0f000c0063000e), u64const(0x10000c0063000e),
    u64const(0x10000d0063000f), u64const(0x10000d0063000f), u64const(0x10000d0063000f), u64const(0x10000d00630010),
    u64const(0x10000e00630011), u64const(0x0f000e00630011), u64const(0x10000e00630011), u64const(0x0e000f00630012),
    u64const(0x0d000c0063000c), u64const(0x0e000c0063000d), u64const(0x0e000b0063000d), u64const(0x0e000c0063000e),
    u64const(0x0e000c0063000e), u64const(0x0f000c0063000f), u64const(0x0f000d0063000e), u64const(0x0f000d0063000f),
    u64const(0x0f000d00630010), u64const(0x11000d00630010), u64const(0x10000d00630011), u64const(0x10000d00630011),
    u64const(0x10000e00630011), u64const(0x10000e00630012), u64const(0x12000f00630012), u64const(0x0e000f00630012),
    u64const(0x0f000c0063000d), u64const(0x0e000c0063000d), u64const(0x0e000c0063000e), u64const(0x0e000c0063000f),
    u64const(0x0f000c0063000f), u64const(0x0f000d0063000f), u64const(0x10000d00630010), u64const(0x10000d00630010),
    u64const(0x10000d00630010), u64const(0x12000e00630010), u64const(0x11000e00630010), u64const(0x11000e00630011),
    u64const(0x11000e00630012), u64const(0x13000e00630011), u64const(0x11000f00630012), u64const(0x0e000f00630012),
    u64const(0x0e000d0063000e), u64const(0x0f000d0063000e), u64const(0x0d000d0063000e), u64const(0x0e000d0063000f),
    u64const(0x10000d0063000f), u64const(0x10000d0063000f), u64const(0x0f000d00630011), u64const(0x10000d00630010),
    u64const(0x10000e00630010), u64const(0x11000e00630013), u64const(0x12000e00630011), u64const(0x11000e00630011),
    u64const(0x13000f00630011), u64const(0x11000f00630013), u64const(0x10000e00630012), u64const(0x0e000f00630012),
    u64const(0x0b000d0063000d), u64const(0x0b000d0063000e), u64const(0x0b000d0063000f), u64const(0x0c000d00630010),
    u64const(0x0c000d00630010), u64const(0x0d000d00630010), u64const(0x0d000d00630011), u64const(0x0d000e00630010),
    u64const(0x0e000e00630011), u64const(0x0e000e00630011), u64const(0x0e000e00630012), u64const(0x0e000e00630012),
    u64const(0x0e000f00630015), u64const(0x0e000f00630014), u64const(0x0e000f00630015), u64const(0x0c000f00630012),
};

/*  for (i = 0; i < 16*16; i++)
 *      largetbl[i] = ((ht[16].hlen[i]) << 16) + ht[24].hlen[i];
 */
static const unsigned int largetbl[16*16] = {
0x010004, 0x050005, 0x070007, 0x090008, 0x0a0009, 0x0a000a, 0x0b000a, 0x0b000b,
0x0c000b, 0x0c000c, 0x0c000c, 0x0d000c, 0x0d000c, 0x0d000c, 0x0e000d, 0x0a000a,
0x040005, 0x060006, 0x080007, 0x090008, 0x0a0009, 0x0b000a, 0x0b000a, 0x0b000b,
0x0c000b, 0x0c000b, 0x0c000c, 0x0d000c, 0x0e000c, 0x0d000c, 0x0e000c, 0x0a000a,
0x070007, 0x080007, 0x090008, 0x0a0009, 0x0b0009, 0x0b000a, 0x0c000a, 0x0c000b,
0x0d000b, 0x0c000b, 0x0d000b, 0x0d000c, 0x0d000c, 0x0e000c, 0x0e000d, 0x0b0009,
0x090008, 0x090008, 0x0a0009, 0x0b0009, 0x0b000a, 0x0c000a, 0x0c000a, 0x0c000b,
0x0d000b, 0x0d000b, 0x0e000b, 0x0e000c, 0x0e000c, 0x0f000c, 0x0f000c, 0x0c0009,
0x0a0009, 0x0a0009, 0x0b0009, 0x0b000a, 0x0c000a, 0x0c000a, 0x0d000a, 0x0d000b,
0x0d000b, 0x0e000b, 0x0e000c, 0x0e000c, 0x0f000c, 0x0f000c, 0x0f000d, 0x0b0009,
0x0a000a, 0x0a0009, 0x0b000a, 0x0b000a, 0x0c000a, 0x0d000a, 0x0d000b, 0x0e000b,
0x0d000b, 0x0e000b, 0x0e000c, 0x0f000c, 0x0f000c, 0x0f000c, 0x10000c, 0x0c0009,
0x0b000a, 0x0b000a, 0x0b000a, 0x0c000a, 0x0d000a, 0x0d000b, 0x0d000b, 0x0d000b,
0x0e000b, 0x0e000c, 0x0e000c, 0x0e000c, 0x0f000c, 0x0f000c, 0x10000d, 0x0c0009,
0x0b000b, 0x0b000a, 0x0c000a, 0x0c000a, 0x0d000b, 0x0d000b, 0x0d000b, 0x0e000b,
0x0e000c, 0x0f000c, 0x0f000c, 0x0f000c, 0x0f000c, 0x11000d, 0x11000d, 0x0c000a,
0x0b000b, 0x0c000b, 0x0c000b, 0x0d000b, 0x0d000b, 0x0d000b, 0x0e000b, 0x0e000b,
0x0f000b, 0x0f000c, 0x0f000c, 0x0f000c, 0x10000c, 0x10000d, 0x10000d, 0x0c000a,
0x0c000b, 0x0c000b, 0x0c000b, 0x0d000b, 0x0d000b, 0x0e000b, 0x0e000b, 0x0f000c,
0x0f000c, 0x0f000c, 0x0f000c, 0x10000c, 0x0f000d, 0x10000d, 0x0f000d, 0x0d000a,
0x0c000c, 0x0d000b, 0x0c000b, 0x0d000b, 0x0e000b, 0x0e000c, 0x0e000c, 0x0e000c,
0x0f000c, 0x10000c, 0x10000c, 0x10000d, 0x11000d, 0x11000d, 0x10000d, 0x0c000a,
0x0d000c, 0x0d000c, 0x0d000b, 0x0d000b, 0x0e000b, 0x0e000c, 0x0f000c, 0x10000c,
0x10000c, 0x10000c, 0x10000c, 0x10000d, 0x10000d, 0x0f000d, 0x10000d, 0x0d000a,
0x0d000c, 0x0e000c, 0x0e000c, 0x0e000c, 0x0e000c, 0x0f000c, 0x0f000c, 0x0f000c,
0x0f000c, 0x11000c, 0x10000d, 0x10000d, 0x10000d, 0x10000d, 0x12000d, 0x0d000a,
0x0f000c, 0x0e000c, 0x0e000c, 0x0e000c, 0x0f000c, 0x0f000c, 0x10000c, 0x10000c,
0x10000d, 0x12000d, 0x11000d, 0x11000d, 0x11000d, 0x13000d, 0x11000d, 0x0d000a,
0x0e000d, 0x0f000c, 0x0d000c, 0x0e000c, 0x10000c, 0x10000c, 0x0f000c, 0x10000d,
0x10000d, 0x11000d, 0x12000d, 0x11000d, 0x13000d, 0x11000d, 0x10000d, 0x0d000a,
0x0a0009, 0x0a0009, 0x0a0009, 0x0b0009, 0x0b0009, 0x0c0009, 0x0c0009, 0x0c0009,
0x0d0009, 0x0d0009, 0x0d0009, 0x0d000a, 0x0d000a, 0x0d000a, 0x0d000a, 0x0a0006
};

/*********************************************************************
 * nonlinear quantization of xr 
 * More accurate formula than the ISO formula.  Takes into account
 * the fact that we are quantizing xr -> ix, but we want ix^4/3 to be 
 * as close as possible to x^4/3.  (taking the nearest int would mean
 * ix is as close as possible to xr, which is different.)
 * From Segher Boessenkool <segher@eastsite.nl>  11/1999
 *********************************************************************/
static void
quantize_01(const FLOAT *xp, gr_info *gi, fi_union *fi, int sfb,
	    const FLOAT *xend)
{
    do {
	const FLOAT *xe = xp - gi->wi[sfb].width;
	FLOAT istep;
	if (xe > xend)
	    xe = xend;
	if (gi->scalefac[sfb] < 0) {
	    fi += (xe-xp);
	    xp = xe;
	    continue;
	}
	istep = (FLOAT)(1.0 - ROUNDFAC) / IPOW20(scalefactor(gi, sfb));
#ifdef USE_IEEE754_HACK
	{
	    fi_union thre;
	    thre.f = istep;
	    do {
		(fi++)->i = ((int*)xp)[0] > thre.i ? 1:0;
		(fi++)->i = ((int*)xp)[1] > thre.i ? 1:0;
		xp += 2;
	    } while (xp < xend);
	}
#else
	do {
	    (fi++)->i = *xp++ > istep ? 1:0;
	    (fi++)->i = *xp++ > istep ? 1:0;
	} while (xp < xe);
#endif
    } while (sfb++, xp < xend);
}

/*
 * count how many bits we need to encode the quantized spectrums
 */
static int
count_bit_ESC(
          int * const s,
    const int *       ix, 
    const int * const end, 
          int         t1,
    const int         t2
    )
{
    /* ESC-table is used */
    int linbits = htESC_xlen[t1-16] * 65536 + htESC_xlen[t2-16];
    int sum = 0, sum2;

    do {
	int x = ix[0];
	int y = ix[1];

	if (x > 14) {
	    x = 15;
	    sum += linbits;
	}
	if (y > 14) {
	    y = 15;
	    sum += linbits;
	}
	sum += largetbl[x*16 + y];
    } while ((ix += 2) < end);

    sum2 = sum & 0xffff;
    sum >>= 16;

    if (sum > sum2) {
	sum = sum2;
	t1 = t2;
    }
    *s = t1;
    return sum;
}


inline static int
count_bit_noESC_from2(int * const s, const int *ix, const int * const end)
{
    int t1, diff = 0, sum = 0;
    do {
	sum += ix[1] + ix[0]*2 - (ix[0]&ix[1]) + 2;
	diff += ix[1]*2 - 1;
    } while ((ix += 2) < end);

    t1 = 3;
    if (diff < 0) {
	sum += diff;
	t1 = 1;
    }
    *s = t1;
    return sum;
}


inline static int
count_bit_noESC_from4(
          int * const s,
    const int *       ix,
    const int * const end,
          int         t1,
    const uint64_t * const table
    )
{
    /* No ESC-words */
    uint64_t sum = 0;
    int	sum1, sum2, sum3;
    int t;
    do {
	sum += table[ix[0]*16 + ix[1]];
    } while ((ix += 2) < end);

    t = t1;
    sum1 = sum & 0xffff;
    sum2 = (sum>>16) & 0xffff;
    if (sum1 > sum2) {
	sum1 = sum2;
	t += 4;
	if (t1 == 10)
	    t = 15;
	if (t1 == 2)
	    t = 8;
    }
    sum3 = (sum>>32);
    sum2 = sum3 & 0xffff;
    if (sum1 > sum2) {
	sum1 = sum2;
	t = t1+2;
    }
    sum2 = sum3>>16;
    if (sum1 > sum2) {
	sum1 = sum2;
	t = t1+1;
    }

    *s = t;
    return sum1;
}


#if !HAVE_NASM
static
#endif
int
ix_max(const int *ix, const int *end)
{
    int max1 = 0, max2 = 0;

    do {
	int x1 = *ix++;
	int x2 = *ix++;
	if (max1 < x1)
	    max1 = x1;

	if (max2 < x2)
	    max2 = x2;
    } while (ix < end);
    if (max1 < max2)
	max1 = max2;
    return max1;
}

/*************************************************************************
 *	choose table()
 *  Choose the Huffman table that will encode ix[begin..end] with
 *  the fewest bits.
 *  Note: This code contains knowledge about the sizes and characteristics
 *  of the Huffman tables as defined in the IS (Table B.7), and will not work
 *  with any arbitrary tables.
 *************************************************************************/
static int
choose_table(const int *ix, const int * const end, int * const s)
{
    int choice, choice2;
    unsigned int max;
    const uint64_t *ptable;

    max = *s;
    switch (max) {
    case 0:
	return max;

    case 1:
	return count_bit_noESC_from2(s, ix, end);

    case 2:
	          ptable = table7B89+98; break;

    case 3:
	max =  5; ptable = table7B89+94; break;

    case 4: case 5:
	max =  7; ptable = table7B89;    break;

    case 6: case 7:
	max = 10; ptable = table7B89+6;  break;

    case 8: case 9: case 10: case 11: case 12: case 13: case 14: case 15:
	max = 13; ptable = tableDxEF;    break;

    default:
	/* try tables with linbits */
	for (choice2 = 24; choice2 < 32; choice2++) {
	    if (((max - 15) >> htESC_xlen[choice2-16]) <= 0)
		break;
	}
	if (choice2 == 32)
	    return max;
	choice = choice2 - 8;
	do {
	    if (((max - 15) >> htESC_xlen[choice-16]) <= 0)
		break;
	} while (++choice < 24);
	return count_bit_ESC(s, ix, end, choice, choice2);
    }
    return count_bit_noESC_from4(s, ix, end, max, ptable);
}



/***********************************************************************
  best huffman table selection for scalefactor band
  the saved bits are kept in the bit reservoir.
 **********************************************************************/
inline static void
recalc_divide_init(
    lame_t   gfc,
    gr_info *gi,
    int      r01_bits[],
    int      r01_info[],
    short    max_info[]
    )
{
    int r0, r0max;
    for (r0 = 0; r0 < SBMAX_l; r0++) {
	int m = 0;
	if (gfc->scalefac_band.l[r0] < gi->big_values && gi->scalefac[r0] >= 0)
	    m = ixmax(&gi->l3_enc[gfc->scalefac_band.l[r0]],
		      &gi->l3_enc[gfc->scalefac_band.l[r0+1]]);
	assert((unsigned int)m <= IXMAX_VAL);
	max_info[r0] = m;
	r01_bits[r0] = LARGE_BITS;
    }

    r0 = 0;
    r0max = max_info[0];
    if (!r0max)
	for (; r0 < 16-1; r0++)
	    if (max_info[r0+1])
		break;

    for (; r0 < 16; r0++) {
	int a1, r0bits, r1, r0t, r1t, bits, r1max, prev;
	if (gfc->scalefac_band.l[r0 + 2] >= gi->big_values)
	    break;
	a1 = gfc->scalefac_band.l[r0 + 1];
	if (r0max < max_info[r0])
	    r0max = max_info[r0];
	r0t = r0max;
	r0bits = choose_table(gi->l3_enc, &gi->l3_enc[a1], &r0t);
	if (r0bits >= gi->part2_3_length - gi->count1bits - 6)
	    break;

	r0t = (r0 << 16) + (r0t << 8);
	r1max = max_info[r0+1];
	prev = 0;
	for (r1 = 0; r1 < 8; r1++) {
	    int a2 = gfc->scalefac_band.l[r0 + r1 + 2];
	    if (a2 >= gi->big_values)
		break;
	    bits = r0bits;
	    if (r1max < max_info[r0+r1+1])
		r1max = max_info[r0+r1+1];
	    r1t = r1max;
	    if (r1max) {
		if (prev == 0)
		    prev = ((a2-a1)>>1) + 1;
		prev += bits;
		if (prev >= r01_bits[r0 + r1]
		 || prev >= gi->part2_3_length - gi->count1bits - ((gi->big_values-a2)>>1) - 1)
		    continue;
		prev = choose_table(&gi->l3_enc[a1], &gi->l3_enc[a2], &r1t);
		bits += prev;
	    }
	    if (r01_bits[r0 + r1] > bits) {
		r01_bits[r0 + r1] = bits;
		r01_info[r0 + r1] = r0t + r1t;
	    }
	}
    }
    for (r0 = 0; r0 < SBMAX_l - 1; r0++) {
	int r1 = r01_info[r0];
	if (gfc->scalefac_band.l[r0+2+1+1] <= gi->big_values && r0 < 16 - 1
	    && ((r1 >> 8) & 0xff) == (r1 & 0xff)) {
	    r01_bits[r0] = LARGE_BITS;
	    continue;
	}

	for (r1 = r0 + 1; r1 < SBMAX_l; r1++) {
	    if (r01_bits[r0] >= r01_bits[r1]) {
		r01_bits[r0] = LARGE_BITS;
		break;
	    }
	}
    }
}

inline static int
recalc_divide_sub(
    lame_t  gfc,
    gr_info *gi,
    const int     r01_bits[],
    const int     r01_info[],
    const short   max_info[]
    )
{
    int bits, r2, r2t, old = gi->part2_3_length, prev = 0, r2max = 0;
    for (r2 = SBMAX_l - 3; r2 >= 0; --r2) {
	int a2 = gfc->scalefac_band.l[r2+2];
	if (a2 >= gi->big_values)
	    continue;
	if (r2max < max_info[r2+2])
	    r2max = max_info[r2+2];

	/* in the region2, there must be the ix larger than 1
	 * (if not, the region will be count1 region).
	 * So the smallest number of bits to encode the region2 is
	 *   (length of region2 - 2)/2 + 6 = (length of region2)/2 + 5
	 * with huffman table 2 or 3.
	 */
	bits = r01_bits[r2] + gi->count1bits;
	if (gi->part2_3_length <= bits + ((gi->big_values - a2) >> 1) + 5)
	    continue;
	if (gi->part2_3_length <= bits + prev + (a2-gfc->scalefac_band.l[r2+1])/2)
	    continue;

	r2t = r2max;
	prev = choose_table(&gi->l3_enc[a2], &gi->l3_enc[gi->big_values], &r2t);
	bits += prev;
	if (gi->part2_3_length <= bits)
	    continue;

	gi->part2_3_length = bits;
	gi->region0_count = r01_info[r2] >> 16;
	gi->region1_count = r2 - (r01_info[r2] >> 16);
	gi->table_select[0] = (r01_info[r2] >> 8) & 0xff;
	gi->table_select[1] = r01_info[r2] & 0xff;
	gi->table_select[2] = r2t;
    }
    return gi->part2_3_length - old;
}

static void
best_huffman_divide(lame_t gfc, gr_info * const gi)
{
    int i, a1, a2;
    gr_info gi_w;
    int * const ix = gi->l3_enc;

    int r01_bits[SBMAX_l];
    int r01_info[SBMAX_l];
    short max_info[SBMAX_l];

    if ((gi->big_values == 0
	 || gi->big_values == gfc->scalefac_band.l[1]
	 || gi->big_values == gfc->scalefac_band.l[2])
	&& gi->block_type == NORM_TYPE && gi->count1 != gi->big_values) {
	for (i = gi->big_values; i < gi->count1 - 4; i += 4)
	    if (gi->l3_enc[i] + gi->l3_enc[i+1]
		+ gi->l3_enc[i+2] + gi->l3_enc[i+3])
		break;
	if (i == gi->big_values)
	    return;

	if (gi->big_values == 0)
	    gi->table_select[0] = 0;
	if (gi->big_values <= gfc->scalefac_band.l[1])
	    gi->table_select[1] = 0;
	gi->table_select[2] = gi->table_select[3] = 0;

	gi->big_values = i;
	a1 = a2 = 0;
	for (; i < gi->count1; i += 4) {
	    int p = ((ix[i] * 2 + ix[i+1]) * 2 + ix[i+2]) * 2 + ix[i+3];
	    a1 += quadcode[0][p];
	    a2 += quadcode[1][p];
	}

	if (a1 > a2) {
	    a1 = a2;
	    gi->table_select[3] = 1;
	}
	a2 = gi->count1bits - a1;
	gi->part2_3_length -= a2;
	gi->count1bits = a1;
	return;
    }

    if (gi->big_values == 0)
	return;

    if (gi->block_type == NORM_TYPE) {
	recalc_divide_init(gfc, gi, r01_bits, r01_info, max_info);
	recalc_divide_sub(gfc, gi, r01_bits, r01_info, max_info);
    }
#ifndef EXPERIMENTAL
    if ((unsigned int)(ix[gi->big_values-2] | ix[gi->big_values-1]) > 1)
	return;
#endif
    i = gi->count1 + 2;
    if (i > 576)
	return;

    ix[i-2] = ix[i-1] = 0; /* this may not satisfied in some case */
    gi_work_copy(&gi_w, gi);
    gi->count1 = i;
    gi->big_values -= 2;
#ifdef EXPERIMENTAL
    if ((unsigned int)(ix[gi->big_values-2] | ix[gi->big_values-1]) > 1)
	gi->big_values += 2;
#endif
    a1 = a2 = 0;
    for (; i > gi->big_values; i -= 4) {
	int p = ((ix[i-4] * 2 + ix[i-3]) * 2 + ix[i-2]) * 2 + ix[i-1];
	a1 += quadcode[0][p];
	a2 += quadcode[1][p];
    }

    gi->table_select[3] = 0;
    if (a1 > a2) {
	a1 = a2;
	gi->table_select[3] = 1;
    }

    gi->count1bits = a1;

    if (gi->block_type == NORM_TYPE
	&& gi->big_values > gfc->scalefac_band.l[2]) {
	if (!recalc_divide_sub(gfc, gi, r01_bits, r01_info, max_info))
	    gi_work_copy(gi, &gi_w);
    } else {
	/* Count the number of bits necessary to code the bigvalues region. */
	gi->part2_3_length = a1;
	if (i > 0) {
	    a1 = gfc->scalefac_band.l[gi->region0_count+1];

	    if (a1 > i)
		a1 = i;

	    gi->table_select[0] = ixmax(ix, &ix[a1]);
	    gi->part2_3_length
		+= choose_table(ix, &ix[a1], &gi->table_select[0]);

	    if (i > a1) {
		gi->table_select[1] = ixmax(&ix[a1], &ix[i]);
		gi->part2_3_length
		    += choose_table(&ix[a1], &ix[i], &gi->table_select[1]);
	    }
	}
	if (gi->part2_3_length < gi_w.part2_3_length)
	    gi_work_copy(gi, &gi_w);
    }
}

int
noquant_count_bits(lame_t gfc, gr_info * const gi)
{
    int i, a1, a2;
    int *const ix = gi->l3_enc;

    /* Determine count1 region */
    for (i = gi->count1; i > 1; i -= 2)
	if (ix[i - 1] | ix[i - 2])
	    break;
    gi->count1 = i;

    /* Determines the number of bits to encode the quadruples. */
    a1 = a2 = 0;
    for (; i > 3; i -= 4) {
	int p;
	/* hack to check if all values <= 1 */
	if ((unsigned int)(ix[i-1] | ix[i-2] | ix[i-3] | ix[i-4]) > 1u)
	    break;

	p = ((ix[i-4] * 2 + ix[i-3]) * 2 + ix[i-2]) * 2 + ix[i-1];
	a1 += quadcode[0][p];
	a2 += quadcode[1][p];
    }

    gi->table_select[3] = 0;
    if (a1 > a2) {
	a1 = a2;
	gi->table_select[3] = 1;
    }

    gi->part2_3_length = gi->count1bits = a1;
    gi->big_values = i;
    if (i == 0)
	return a1;

    if (gi->block_type == NORM_TYPE) {
	assert(i <= 576); /* bv_scf has 576 entries (0..575) */
	a1 = gi->region0_count = gfc->bv_scf[i-2];
	a2 = gi->region1_count = gfc->bv_scf[i-1];

	assert(a1+a2+2 < SBPSY_l);
        a2 = gfc->scalefac_band.l[a1 + a2 + 2];
	if (a2 < i) {
	    gi->table_select[2] = ixmax(&ix[a2], &ix[i]);
	    gi->part2_3_length
		+= choose_table(&ix[a2], &ix[i], &gi->table_select[2]);
	} else
	    a2 = i;
    } else {
	a1 = gi->region0_count;
	a2 = i;
    }
    a1 = gfc->scalefac_band.l[a1 + 1];

    /* have to allow for the case when bigvalues < region0 < region1 */
    /* (and region0, region1 are ignored) */
    assert( a1 > 0 );
    assert( a2 > 0 );

    if (a1 >= a2) {
	a1 = a2;
    } else {
	gi->table_select[1] = ixmax(&ix[a1], &ix[a2]);
	gi->part2_3_length
	    += choose_table(&ix[a1], &ix[a2], &gi->table_select[1]);
    }
    gi->table_select[0] = ixmax(ix, &ix[a1]);
    gi->part2_3_length += choose_table(ix, &ix[a1], &gi->table_select[0]);

    if (gfc->use_best_huffman == 2)
	best_huffman_divide (gfc, gi);

    return gi->part2_3_length;
}

int
count_bits(lame_t gfc, gr_info * const gi)
{
    /* quantize on xr^(3/4) instead of xr */
    int sfb = 0;
    fi_union *fi = (fi_union *)gi->l3_enc;
    const FLOAT *xp = xr34, *xend;
    const FLOAT *xe01 = &xp[gi->big_values];

    if (gi->count1 > gi->xrNumMax)
	gi->count1 = gi->xrNumMax;
    xend = &xp[gi->count1];

    do {
	const FLOAT *xe; FLOAT istep;
	if (xp > xe01) {
	    quantize_01(xp, gi, fi, sfb, xend);
	    break;
	}

	xe = xp - gi->wi[sfb].width;
	if (xe > xend)
	    xe = xend;
	if (gi->scalefac[sfb] < 0) {
	    fi += (xe-xp);
	    xp = xe;
	} else
#ifdef HAVE_NASM
	if (gfc->CPU_features.AMD_3DNow) {
	    fi -= xp-xe;
	    quantize_sfb_3DN(xe, xp-xe, scalefactor(gi, sfb), &fi[0].i);
	    xp = xe;
	} else
#endif
	{
	    istep = IPOW20(scalefactor(gi, sfb));
	    do {
#ifdef USE_IEEE754_HACK
		int i0, i1;
		fi[0].f = istep * xp[0] + MAGIC_FLOAT2;
		fi[1].f = istep * xp[1] + MAGIC_FLOAT2;
		xp += 2;
		i0 = fi[0].i;
		i1 = fi[1].i;
		if (i0 > MAGIC_INT2 + (IXMAX_VAL << FIXEDPOINT))
		    i0 = MAGIC_INT2 + (IXMAX_VAL << FIXEDPOINT);
		if (i1 > MAGIC_INT2 + (IXMAX_VAL << FIXEDPOINT))
		    i1 = MAGIC_INT2 + (IXMAX_VAL << FIXEDPOINT);
		fi[0].i = (i0 + (adj43asm - (MAGIC_INT2 >> FIXEDPOINT))[i0 >> FIXEDPOINT]) >> FIXEDPOINT;
		fi[1].i = (i1 + (adj43asm - (MAGIC_INT2 >> FIXEDPOINT))[i1 >> FIXEDPOINT]) >> FIXEDPOINT;
		fi += 2;
#else
		FLOAT x0 = *xp++ * istep;
		FLOAT x1 = *xp++ * istep;
		if (x0 > IXMAX_VAL)
		    x0 = IXMAX_VAL;
		if (x1 > IXMAX_VAL)
		    x1 = IXMAX_VAL;
		(fi++)->i = (int)(x0 + adj43[(int)x0]);
		(fi++)->i = (int)(x1 + adj43[(int)x1]);
#endif
	    } while (xp < xe);
	}
	sfb++;
    } while (xp < xend);

    if (gfc->noise_shaping_amp >= 3) {
	int j = 0;
	for (sfb = 0; sfb < gi->psymax && j < gi->count1; sfb++) {
	    FLOAT roundfac;
	    int l = gi->wi[sfb].width;
	    j -= l;
	    if (!gfc->pseudohalf[sfb] || gi->scalefac[sfb] < 0)
		continue;
	    roundfac = (FLOAT)0.634521682242439
		/ IPOW20(scalefactor(gi, sfb) + gi->scalefac_scale);
	    do {
		if (xr34[j+l] < roundfac)
		    gi->l3_enc[j+l] = 0;
	    } while (++l < 0);
	}
    }
    return noquant_count_bits(gfc, gi);
}

/***********************************************************************
  re-calculate the best scalefac_compress using scfsi
  the saved bits are kept in the bit reservoir.
 **********************************************************************/
/* This is the scfsi_band table from 2.4.2.7 of the IS */
static const char scfsi_band[5] = { 0, 6, 11, 16, 21 };

static void
scfsi_calc(lame_t gfc, int ch)
{
    int i, s1 = 0, s2, c1, c2, sfb;
    gr_info *gi = &gfc->tt[1][ch];
    gr_info *g0 = &gfc->tt[0][ch];

    for (i = 0; i < (int)(sizeof(scfsi_band) / sizeof(int)) - 1; i++) {
	for (sfb = scfsi_band[i]; sfb < scfsi_band[i + 1]; sfb++) {
	    if (g0->scalefac[sfb] != gi->scalefac[sfb]
		&& gi->scalefac[sfb] >= 0)
		break;
	}
	if (sfb == scfsi_band[i + 1]) {
	    for (sfb = scfsi_band[i]; sfb < scfsi_band[i + 1]; sfb++) {
		gi->scalefac[sfb] = SCALEFAC_SCFSI_FLAG;
	    }
	    s1 |= 8 >> i;
	}
    }

    if (!s1) return;
    gfc->scfsi[ch] = s1;
    s1 = c1 = 0;
    for (sfb = 0; sfb < 11; sfb++) {
	if (gi->scalefac[sfb] == SCALEFAC_SCFSI_FLAG)
	    continue;
	c1++;
	if (s1 < gi->scalefac[sfb])
	    s1 = gi->scalefac[sfb];
    }
    s1 = log2tab[s1];

    s2 = c2 = 0;
    for (; sfb < SBPSY_l; sfb++) {
	if (gi->scalefac[sfb] == SCALEFAC_SCFSI_FLAG)
	    continue;
	c2++;
	if (s2 < gi->scalefac[sfb])
	    s2 = gi->scalefac[sfb];
    }
    s2 = log2tab[s2];

    for (i = 0; i < 16; i++) {
	if (((s1bits[i] - s1) | (s2bits[i] - s2)) >= 0) {
	    int c = s1bits[i] * c1 + s2bits[i] * c2;
	    if (gi->part2_length > c) {
		gi->part2_length = c;
		gi->scalefac_compress = i;
	    }
	}
    }
}

/*
  Find the optimal way to store the scalefactors.
  Only call this routine after final scalefactors have been
  chosen and the channel/granule will not be re-encoded.
 */
static void
check_preflag(gr_info * const gi)
{
    int sfb;
    if (gi->preflag)
	return;

    for (sfb = 11; sfb < gi->psymax; sfb++)
	if (gi->scalefac[sfb] < pretab[sfb]
	    && gi->scalefac[sfb] != SCALEFAC_ANYTHING_GOES)
	    return;

    gi->preflag = 1;
    for (sfb = 11; sfb < gi->psymax; sfb++)
	if (gi->scalefac[sfb] != SCALEFAC_ANYTHING_GOES)
	    gi->scalefac[sfb] -= pretab[sfb];
}

static void
best_scalefac_store(lame_t gfc, int gr, int ch)
{
    /* use scalefac_scale if we can */
    gr_info *gi = &gfc->tt[gr][ch];
    int sfb, recalc = 0, j = 0;

    gfc->scfsi[ch] = 0;
    for (sfb = 0; sfb < gi->psymax; sfb++) {
	int l = gi->wi[sfb].width;
	int even = 0;
	j -= l;
	if (gi->scalefac[sfb] < 0)
	    continue;
	do {
	    even |= gi->l3_enc[l+j  ];
	    even |= gi->l3_enc[l+j+1];
	} while ((l+=2) < 0 && (even & 7) == 0);
	/* remove scalefacs from bands with all ix=0.
	 * This idea comes from the AAC ISO docs.  added mt 3/00 */
	if (even == 0) {
	    gi->scalefac[sfb] = SCALEFAC_ANYTHING_GOES;
	    recalc |= 1;
	    continue;
	}
	/* if all the ix[] is multiple of 8, we can losslessly reduce the
	   scalefactor value and ix[]. */
	while (((even & 7) == 0)
	       && gi->scalefac[sfb] >= (8 >> gi->scalefac_scale)) {
	    l = gi->wi[sfb].width;
	    do {
		gi->l3_enc[l+j] >>= 3;
	    } while (++l < 0);
	    gi->scalefac[sfb] -= (8 >> gi->scalefac_scale);
	    even >>= 3;
	    recalc |= 2;
	}
    }
    if (recalc) {
	if (recalc & 2)
	    noquant_count_bits(gfc, gi);
	gfc->scale_bitcounter(gi);
	recalc = 0;
    }

    if (gi->psymax > gi->sfbmax && gi->block_type != SHORT_TYPE
	&& gi->scalefac[gi->sfbmax] == SCALEFAC_ANYTHING_GOES) {
	int minsfb0 = 999;
	int minsfbp = 999;
	for (sfb = 0; sfb < gi->psymax; sfb++) {
	    if (gi->scalefac[sfb] < 0)
		continue;
	    if (minsfbp > gi->scalefac[sfb] + (gi->preflag>0 ? pretab[sfb]:0))
		minsfbp = gi->scalefac[sfb] + (gi->preflag>0 ? pretab[sfb]:0);
	    if (minsfb0 > gi->scalefac[sfb])
		minsfb0 = gi->scalefac[sfb];
	}
	if (gi->global_gain - (minsfb0 << (gi->scalefac_scale+1)) < 0)
	    minsfb0 = gi->global_gain >> (gi->scalefac_scale+1);

	if (minsfb0 != 0) {
	    gi->global_gain -= minsfb0 << (gi->scalefac_scale+1);
	    for (sfb = 0; sfb < gi->psymax; sfb++)
		if (gi->scalefac[sfb] != SCALEFAC_ANYTHING_GOES)
		    gi->scalefac[sfb] -= minsfb0;

	    gfc->scale_bitcounter(gi);
	}

	minsfbp -= minsfb0;
	if (gi->global_gain - (minsfbp << (gi->scalefac_scale+1)) < 0)
	    minsfbp = gi->global_gain >> (gi->scalefac_scale+1);
	if (minsfbp != 0) {
	    gr_info gi_w;
	    gi_work_copy(&gi_w, gi);
	    gi->global_gain -= minsfbp << (gi->scalefac_scale+1);
	    for (sfb = 0; sfb < gi->psymax; sfb++)
		if (gi->scalefac[sfb] != SCALEFAC_ANYTHING_GOES)
		    gi->scalefac[sfb] -= minsfbp-(gi->preflag>0 ? pretab[sfb]:0);
	    gi->preflag = 0;
	    gfc->scale_bitcounter(gi);
	    if (gi->part2_length > gi_w.part2_length)
		gi_work_copy(gi, &gi_w);
	}
    }

    if (!gi->scalefac_scale && !gi->preflag) {
	int s = 0;
	for (sfb = 0; sfb < gi->psymax; sfb++)
	    if (gi->scalefac[sfb] > 0)
		s |= gi->scalefac[sfb];

	if (!(s & 1) && s != 0) {
	    for (sfb = 0; sfb < gi->psymax; sfb++)
		if (gi->scalefac[sfb] > 0)
		    gi->scalefac[sfb] >>= 1;

	    gi->scalefac_scale = recalc = 1;
	}
    }

    if (gfc->mode_gr == 2 && !gi->preflag && gi->block_type != SHORT_TYPE) {
	for (sfb = 11; sfb < gi->psymax; sfb++)
	    if (gi->scalefac[sfb] < pretab[sfb]
		&& gi->scalefac[sfb] != SCALEFAC_ANYTHING_GOES)
		break;
	if (sfb == gi->psymax) {
	    for (sfb = 11; sfb < gi->psymax; sfb++)
		if (gi->scalefac[sfb] > 0)
		    gi->scalefac[sfb] -= pretab[sfb];

	    gi->preflag = recalc = 1;
	}
    }

    if (gfc->mode_gr == 2 && gr == 1
	&& gi[-2].block_type != SHORT_TYPE && gi[0].block_type != SHORT_TYPE) {
	scfsi_calc(gfc, ch);
    } else if (recalc)
	gfc->scale_bitcounter(gi);
}


/************************************************************************
 *
 *      iteration_finish_one()
 *
 *  Robert Hegemann 2000-09-06
 *
 ************************************************************************/
int
iteration_finish_one(lame_t gfc, int gr, int ch)
{
    gr_info *gi = &gfc->tt[gr][ch];

    /*  try some better scalefac storage  */
    best_scalefac_store (gfc, gr, ch);

    /*  best huffman_divide may save some bits too */
    if (gfc->use_best_huffman == 1)
	best_huffman_divide (gfc, gi);

    return gi->part2_length + gi->part2_3_length;
}

/*************************************************************************
 *	scale_bitcount()
 * calculates the number of bits necessary to code the scalefactors.
 *************************************************************************/

/* 18*s1bits[i] + 18*s2bits[i] */
static const char scale_short[16] = {
    0, 18, 36, 54, 54, 36, 54, 72, 54, 72, 90, 72, 90, 108, 108, 126 };

/* 17*s1bits[i] + 18*s2bits[i] */
static const char scale_mixed[16] = {
    0, 18, 36, 54, 51, 35, 53, 71, 52, 70, 88, 69, 87, 105, 104, 122 };

/* 11*s1bits[i] + 10*s2bits[i] */
static const char scale_long[16] = {
    0, 10, 20, 30, 33, 21, 31, 41, 32, 42, 52, 43, 53, 63, 64, 74 };

int
scale_bitcount(gr_info * const gi)
{
    int k, sfb, s1, s2;
    const char *tab;

    /* maximum values */
    tab = scale_long;
    if (gi->block_type == SHORT_TYPE) {
	tab = scale_short;
	if (gi->mixed_block_flag)
	    tab = scale_mixed;
    } else
	check_preflag(gi);

    s1 = s2 = 0;
    for (sfb = 0; sfb < gi->sfbdivide; sfb++)
	if (s1 < gi->scalefac[sfb])
	    s1 = gi->scalefac[sfb];
    for (; sfb < gi->sfbmax; sfb++)
	if (s2 < gi->scalefac[sfb])
	    s2 = gi->scalefac[sfb];
    s1 = log2tab[s1];
    s2 = log2tab[s2];

    /* from Takehiro TOMINAGA <tominaga@isoternet.org> 10/99
     * loop over *all* posible values of scalefac_compress to find the
     * one which uses the smallest number of bits.  ISO would stop
     * at first valid index */
    gi->part2_length = LARGE_BITS;
    for (k = s2; k < 16; k++) {
	if (((gi->part2_length - tab[k]) | (s1bits[k] - s1) | (s2bits[k] - s2))
	    >= 0) {
	    gi->part2_length = tab[k];
	    gi->scalefac_compress = k;
	}
    }
    return gi->part2_length;
}

/*************************************************************************
 *	scale_bitcount_lsf()
 *
 * counts the number of bits to encode the scalefacs for MPEG 2 and 2.5
 * Lower sampling frequencies  (lower than 24kHz.)
 *
 * This is reverse-engineered from section 2.4.3.2 of the MPEG2 IS,
 * "Audio Decoding Layer III"
 ************************************************************************/
/* table of largest scalefactor values for MPEG2 */
static const char max_range_sfac_tab[6][4] = {
    { 4, 4, 3, 3}, { 4, 4, 3, 0}, { 3, 2, 0, 0},
    { 4, 5, 5, 0}, { 3, 3, 3, 0}, { 2, 2, 0, 0}
};

int
scale_bitcount_lsf(gr_info * const gi)
{
    int part, i, sfb, tableID, table_type = 0;
    if (gi->preflag < 0)
	table_type = 3; /* intensity stereo */

    tableID = table_type * 3;
    if (gi->block_type == SHORT_TYPE) {
	tableID++;
	if (gi->mixed_block_flag)
	    tableID++;
    }

    sfb = i = 0;
    for (part = 0; part < 4; part++) {
	int m = 0, sfbend = sfb + nr_of_sfb_block[tableID][part];
	for (; sfb < sfbend; sfb++)
	    if (m < gi->scalefac[sfb])
		m = gi->scalefac[sfb];
	gi->slen[part] = m = log2tab[m];

	if (m > max_range_sfac_tab[table_type][part])
	    return (gi->part2_length = LARGE_BITS); /* fail */
	i += m * nr_of_sfb_block[tableID][part];
    }
    gi->part2_length = i;

    if (table_type == 0) {
	/* try to use table type 1 and 2(preflag) */
	char slen2[4];
	sfb = i = 0;
	for (part = 0; part < 4; part++) {
	    int m = 0, sfbend = sfb + nr_of_sfb_block[3+tableID][part];
	    for (; sfb < sfbend; sfb++)
		if (m < gi->scalefac[sfb])
		    m = gi->scalefac[sfb];
	    slen2[part] = m = log2tab[m];

	    if (m > max_range_sfac_tab[1][part]) {
		i = LARGE_BITS;
		break;
	    }
	    i += m * nr_of_sfb_block[3+tableID][part];
	}
	if (gi->part2_length > i) {
	    gi->part2_length = i;
	    tableID += 3;
	    memcpy(gi->slen, slen2, sizeof(slen2));
	}

	if ((tableID == 3 || tableID == 0) && gi->preflag == 0) {
	    /* not SHORT block and not i-stereo of channel 1 */
	    for (part = 0; part < 4; part++) {
		int m = 0, sfbend = sfb + nr_of_sfb_block[6+tableID][part];
		for (; sfb < sfbend; sfb++) {
		    if (gi->scalefac[sfb] < pretab[sfb])
			m = 16;
		    if (m < gi->scalefac[sfb]-pretab[sfb])
			m = gi->scalefac[sfb]-pretab[sfb];
		}
		slen2[part] = m = log2tab[m];

		if (m > max_range_sfac_tab[2][part]) {
		    i = LARGE_BITS;
		    break;
		}
		i += m * nr_of_sfb_block[6+tableID][part];
	    }
	    if (gi->part2_length > i) {
		gi->part2_length = i;
		tableID = 6;
		memcpy(gi->slen, slen2, sizeof(slen2));
	    }
	}
    }
    gi->scalefac_compress = tableID;
    return gi->part2_length;
}
