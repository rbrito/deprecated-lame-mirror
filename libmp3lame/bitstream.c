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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
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
#include "bitstream.h"
#include "util.h"
#include "quantize_pvt.h"
#include "tables.h"

typedef struct {
    unsigned char code;
    unsigned char len;
} codetab_t;

#define PADDING_PATTERN 0xaa
#define PADM_PATTERN 0x4c
#define PADB_PATTERN 0x41
#define PADN_PATTERN 0x4d
#define PADF_PATTERN 0x45

static const codetab_t ht1[] = {    {0, 2},
    {1, 1}, {1, 4},
    {1, 3}, {0, 5},
};

static const codetab_t ht2[] = {    {0, 3},
    {1, 1}, {2, 4}, {1, 7},
    {3, 4}, {1, 5}, {1, 7},
    {3, 6}, {2, 7}, {0, 8},
};
static const codetab_t ht3[] = {    {0, 3},
    {3, 2}, {2, 3}, {1, 7},
    {1, 4}, {1, 4}, {1, 7},
    {3, 6}, {2, 7}, {0, 8},
};

static const codetab_t ht5[] = {    {0, 4},
    {1,  1}, {2,  4}, {6,  7}, {5,  8},
    {3,  4}, {1,  5}, {4,  8}, {4,  9},
    {7,  7}, {5,  8}, {7,  9}, {1, 10},
    {6,  8}, {1,  8}, {1,  9}, {0, 10},
};
static const codetab_t ht6[] = {    {0, 4},
    {7, 3}, {3, 4}, {5, 6}, {1, 8},
    {6, 4}, {2, 4}, {3, 6}, {2, 7},
    {5, 5}, {4, 6}, {4, 7}, {1, 8},
    {3, 7}, {3, 7}, {2, 8}, {0, 9},
};

static const codetab_t ht7[] = {    {0, 6},
    { 1, 1}, { 2, 4}, {10, 7}, {19, 9}, {16, 9}, {10,10},
    { 3, 4}, { 3, 6}, { 7, 8}, {10, 9}, { 5, 9}, { 3,10},
    {11, 7}, { 4, 7}, {13, 9}, {17,10}, { 8,10}, { 4,11},
    {12, 8}, {11, 9}, {18,10}, {15,11}, {11,11}, { 2,11},
    { 7, 8}, { 6, 9}, { 9,10}, {14,11}, { 3,11}, { 1,12},
    { 6, 9}, { 4,10}, { 5,11}, { 3,12}, { 2,12}, { 0,12},
};
static const codetab_t ht8[] = {    {0, 6},
    { 3, 2}, { 4, 4}, { 6, 7}, {18, 9}, {12, 9}, { 5,10},
    { 5, 4}, { 1, 4}, { 2, 6}, {16,10}, { 9,10}, { 3,10},
    { 7, 7}, { 3, 6}, { 5, 8}, {14,10}, { 7,10}, { 3,11},
    {19, 9}, {17,10}, {15,10}, {13,11}, {10,11}, { 4,12},
    {13, 9}, { 5, 9}, { 8,10}, {11,11}, { 5,12}, { 1,12},
    {12,10}, { 4,10}, { 4,11}, { 1,11}, { 1,13}, { 0,13}
};
static const codetab_t ht9[] = {    {0, 6},
    { 7, 3}, { 5, 4}, { 9, 6}, {14, 7}, {15, 9}, { 7,10},
    { 6, 4}, { 4, 5}, { 5, 6}, { 5, 7}, { 6, 8}, { 7,10},
    { 7, 5}, { 6, 6}, { 8, 7}, { 8, 8}, { 8, 9}, { 5,10},
    {15, 7}, { 6, 7}, { 9, 8}, {10, 9}, { 5, 9}, { 1,10},
    {11, 8}, { 7, 8}, { 9, 9}, { 6, 9}, { 4,10}, { 1,11},
    {14, 9}, { 4, 9}, { 6,10}, { 2,10}, { 6,11}, { 0,11},
};

static const codetab_t ht10[] = {    {0, 8},
    { 1, 1}, { 2, 4}, {10, 7}, {23, 9}, {35,10}, {30,10}, {12,10}, {17,11},
    { 3, 4}, { 3, 6}, { 8, 8}, {12, 9}, {18,10}, {21,11}, {12,10}, { 7,10},
    {11, 7}, { 9, 8}, {15, 9}, {21,10}, {32,11}, {40,12}, {19,11}, { 6,11},
    {14, 8}, {13, 9}, {22,10}, {34,11}, {46,12}, {23,12}, {18,11}, { 7,12},
    {20, 9}, {19,10}, {33,11}, {47,12}, {27,12}, {22,12}, { 9,12}, { 3,12},
    {31,10}, {22,11}, {41,12}, {26,12}, {21,13}, {20,13}, { 5,12}, { 3,13},
    {14, 9}, {13,10}, {10,11}, {11,12}, {16,12}, { 6,12}, { 5,13}, { 1,13},
    { 9,10}, { 8,10}, { 7,11}, { 8,12}, { 4,12}, { 4,13}, { 2,13}, { 0,13},
};
static const codetab_t ht11[] = {    {0, 8},
    { 3, 2}, { 4, 4}, {10, 6}, {24, 8}, {34, 9}, {33,10}, {21, 9}, {15,10},
    { 5, 4}, { 3, 5}, { 4, 6}, {10, 8}, {32,10}, {17,10}, {11, 9}, {10,10},
    {11, 6}, { 7, 7}, {13, 8}, {18, 9}, {30,10}, {31,11}, {20,10}, { 5,10},
    {25, 8}, {11, 8}, {19, 9}, {59,11}, {27,10}, {18,12}, {12,10}, { 5,11},
    {35, 9}, {33,10}, {31,10}, {58,11}, {30,11}, {16,12}, { 7,11}, { 5,12},
    {28, 9}, {26,10}, {32,11}, {19,12}, {17,12}, {15,13}, { 8,12}, {14,13},
    {14, 9}, {12, 9}, { 9, 9}, {13,10}, {14,11}, { 9,12}, { 4,12}, { 1,12},
    {11, 9}, { 4, 9}, { 6,10}, { 6,11}, { 6,12}, { 3,12}, { 2,12}, { 0,12},
};
static const codetab_t ht12[] = {    {0, 8},
    { 9, 4}, { 6, 4}, {16, 6}, {33, 8}, {41, 9}, {39,10}, {38,10}, {26,10},
    { 7, 4}, { 5, 5}, { 6, 6}, { 9, 7}, {23, 9}, {16, 9}, {26,10}, {11,10},
    {17, 6}, { 7, 6}, {11, 7}, {14, 8}, {21, 9}, {30,10}, {10, 9}, { 7,10},
    {17, 7}, {10, 7}, {15, 8}, {12, 8}, {18, 9}, {28,10}, {14,10}, { 5,10},
    {32, 8}, {13, 8}, {22, 9}, {19, 9}, {18,10}, {16,10}, { 9,10}, { 5,11},
    {40, 9}, {17, 9}, {31,10}, {29,10}, {17,10}, {13,11}, { 4,10}, { 2,11},
    {27, 9}, {12, 9}, {11, 9}, {15,10}, {10,10}, { 7,11}, { 4,11}, { 1,12},
    {27,10}, {12,10}, { 8,10}, {12,11}, { 6,11}, { 3,11}, { 1,11}, { 0,12},
};

static const codetab_t ht13[] = {    {0, 16},
    { 1, 1}, { 5, 5}, {14, 7}, {21, 8}, { 34, 9}, {51,10}, { 46,10}, {71,11},
    {42,10}, {52,11}, {68,12}, {52,12}, { 67,13}, {44,13}, { 43,14}, {19,14},
    { 3, 4}, { 4, 6}, {12, 8}, {19, 9}, { 31,10}, {26,10}, { 44,11}, {33,11},
    {31,11}, {24,11}, {32,12}, {24,12}, { 31,13}, {35,14}, { 22,14}, {14,14},
    {15, 7}, {13, 8}, {23, 9}, {36,10}, { 59,11}, {49,11}, { 77,12}, {65,12},
    {29,11}, {40,12}, {30,12}, {40,13}, { 27,13}, {33,14}, { 42,15}, {16,15},
    {22, 8}, {20, 9}, {37,10}, {61,11}, { 56,11}, {79,12}, { 73,12}, {64,12},
    {43,12}, {76,13}, {56,13}, {37,13}, { 26,13}, {31,14}, { 25,15}, {14,15},
    {35, 9}, {16, 9}, {60,11}, {57,11}, { 97,12}, {75,12}, {114,13}, {91,13},
    {54,12}, {73,13}, {55,13}, {41,14}, { 48,14}, {53,15}, { 23,15}, {24,16},
    {58,10}, {27,10}, {50,11}, {96,12}, { 76,12}, {70,12}, { 93,13}, {84,13},
    {77,13}, {58,13}, {79,14}, {29,13}, { 74,15}, {49,15}, { 41,16}, {17,16},
    {47,10}, {45,11}, {78,12}, {74,12}, {115,13}, {94,13}, { 90,13}, {79,13},
    {69,13}, {83,14}, {71,14}, {50,14}, { 59,15}, {38,15}, { 36,16}, {15,16},
    {72,11}, {34,11}, {56,12}, {95,13}, { 92,13}, {85,13}, { 91,14}, {90,14},
    {86,14}, {73,14}, {77,15}, {65,15}, { 51,15}, {44,16}, { 43,18}, {42,18},
    {43,10}, {20,10}, {30,11}, {44,12}, { 55,12}, {78,13}, { 72,13}, {87,14},
    {78,14}, {61,14}, {46,14}, {54,15}, { 37,15}, {30,16}, { 20,17}, {16,17},
    {53,11}, {25,11}, {41,12}, {37,12}, { 44,13}, {59,13}, { 54,13}, {81,15},
    {66,14}, {76,15}, {57,15}, {54,16}, { 37,16}, {18,16}, { 39,18}, {11,17},
    {35,11}, {33,12}, {31,12}, {57,13}, { 42,13}, {82,14}, { 72,14}, {80,15},
    {47,14}, {58,15}, {55,16}, {21,15}, { 22,16}, {26,17}, { 38,18}, {22,19},
    {53,12}, {25,12}, {23,12}, {38,13}, { 70,14}, {60,14}, { 51,14}, {36,14},
    {55,15}, {26,15}, {34,15}, {23,16}, { 27,17}, {14,17}, {  9,17}, { 7,18},
    {34,12}, {32,13}, {28,13}, {39,14}, { 49,14}, {75,15}, { 30,14}, {52,15},
    {48,16}, {40,16}, {52,17}, {28,17}, { 18,17}, {17,18}, {  9,18}, { 5,18},
    {45,13}, {21,13}, {34,14}, {64,15}, { 56,15}, {50,15}, { 49,16}, {45,16},
    {31,16}, {19,16}, {12,16}, {15,17}, { 10,18}, { 7,17}, {  6,18}, { 3,18},
    {48,14}, {23,14}, {20,14}, {39,15}, { 36,15}, {35,15}, { 53,17}, {21,16},
    {16,16}, {23,19}, {13,17}, {10,17}, {  6,17}, { 1,19}, {  4,18}, { 2,18},
    {16,13}, {15,14}, {17,15}, {27,16}, { 25,16}, {20,16}, { 29,17}, {11,16},
    {17,17}, {12,17}, {16,18}, { 8,18}, {  1,21}, { 1,20}, {  0,21}, { 1,18},
};
static const codetab_t ht15[] = {    {0, 16},
    {  7, 3},{ 12, 5},{ 18, 6},{ 53, 8},{ 47, 8},{76, 9},{124,10},{108,10},
    { 89,10},{123,11},{108,11},{119,12},{107,12},{81,12},{122,13},{ 63,14},
    { 13, 5},{  5, 5},{ 16, 7},{ 27, 8},{ 46, 9},{36, 9},{ 61,10},{ 51,10},
    { 42,10},{ 70,11},{ 52,11},{ 83,12},{ 65,12},{41,12},{ 59,13},{ 36,13},
    { 19, 6},{ 17, 7},{ 15, 7},{ 24, 8},{ 41, 9},{34, 9},{ 59,10},{ 48,10},
    { 40,10},{ 64,11},{ 50,11},{ 78,12},{ 62,12},{80,13},{ 56,13},{ 33,13},
    { 29, 7},{ 28, 8},{ 25, 8},{ 43, 9},{ 39, 9},{63,10},{ 55,10},{ 93,11},
    { 76,11},{ 59,11},{ 93,12},{ 72,12},{ 54,12},{75,13},{ 50,13},{ 29,13},
    { 52, 8},{ 22, 8},{ 42, 9},{ 40, 9},{ 67,10},{57,10},{ 95,11},{ 79,11},
    { 72,11},{ 57,11},{ 89,12},{ 69,12},{ 49,12},{66,13},{ 46,13},{ 27,13},
    { 77, 9},{ 37, 9},{ 35, 9},{ 66,10},{ 58,10},{52,10},{ 91,11},{ 74,11},
    { 62,11},{ 48,11},{ 79,12},{ 63,12},{ 90,13},{62,13},{ 40,13},{ 38,14},
    {125,10},{ 32, 9},{ 60,10},{ 56,10},{ 50,10},{92,11},{ 78,11},{ 65,11},
    { 55,11},{ 87,12},{ 71,12},{ 51,12},{ 73,13},{51,13},{ 70,14},{ 30,14},
    {109,10},{ 53,10},{ 49,10},{ 94,11},{ 88,11},{75,11},{ 66,11},{122,12},
    { 91,12},{ 73,12},{ 56,12},{ 42,12},{ 64,13},{44,13},{ 21,13},{ 25,14},
    { 90,10},{ 43,10},{ 41,10},{ 77,11},{ 73,11},{63,11},{ 56,11},{ 92,12},
    { 77,12},{ 66,12},{ 47,12},{ 67,13},{ 48,13},{53,14},{ 36,14},{ 20,14},
    { 71,10},{ 34,10},{ 67,11},{ 60,11},{ 58,11},{49,11},{ 88,12},{ 76,12},
    { 67,12},{106,13},{ 71,13},{ 54,13},{ 38,13},{39,14},{ 23,14},{ 15,14},
    {109,11},{ 53,11},{ 51,11},{ 47,11},{ 90,12},{82,12},{ 58,12},{ 57,12},
    { 48,12},{ 72,13},{ 57,13},{ 41,13},{ 23,13},{27,14},{ 62,15},{  9,14},
    { 86,11},{ 42,11},{ 40,11},{ 37,11},{ 70,12},{64,12},{ 52,12},{ 43,12},
    { 70,13},{ 55,13},{ 42,13},{ 25,13},{ 29,14},{18,14},{ 11,14},{ 11,15},
    {118,12},{ 68,12},{ 30,11},{ 55,12},{ 50,12},{46,12},{ 74,13},{ 65,13},
    { 49,13},{ 39,13},{ 24,13},{ 16,13},{ 22,14},{13,14},{ 14,15},{  7,15},
    { 91,12},{ 44,12},{ 39,12},{ 38,12},{ 34,12},{63,13},{ 52,13},{ 45,13},
    { 31,13},{ 52,14},{ 28,14},{ 19,14},{ 14,14},{ 8,14},{  9,15},{  3,15},
    {123,13},{ 60,13},{ 58,13},{ 53,13},{ 47,13},{43,13},{ 32,13},{ 22,13},
    { 37,14},{ 24,14},{ 17,14},{ 12,14},{ 15,15},{10,15},{  2,14},{  1,15},
    { 71,13},{ 37,13},{ 34,13},{ 30,13},{ 28,13},{20,13},{ 17,13},{ 26,14},
    { 21,14},{ 16,14},{ 10,14},{  6,14},{  8,15},{ 6,15},{  2,15},{  0,15},
};

#define PL(pfx, len) (((pfx & 2047)<<5) + (len))
static const unsigned short escHBL[] = {
    PL(   1, 1),PL(   5, 4),PL(  14, 6),PL(  44, 8),
    PL(  74, 9),PL(  63, 9),PL( 110,10),PL(  93,10),
    PL( 172,11),PL( 149,11),PL( 138,11),PL( 242,12),
    PL( 225,12),PL( 195,12),PL( 376,13),PL(  17, 9),
    PL(   3, 3),PL(   4, 4),PL(  12, 6),PL(  20, 7),
    PL(  35, 8),PL(  62, 9),PL(  53, 9),PL(  47, 9),
    PL(  83,10),PL(  75,10),PL(  68,10),PL( 119,11),
    PL( 201,12),PL( 107,11),PL( 207,12),PL(   9, 8),
    PL(  15, 6),PL(  13, 6),PL(  23, 7),PL(  38, 8),
    PL(  67, 9),PL(  58, 9),PL( 103,10),PL(  90,10),
    PL( 161,11),PL(  72,10),PL( 127,11),PL( 117,11),
    PL( 110,11),PL( 209,12),PL( 206,12),PL(  16, 9),
    PL(  45, 8),PL(  21, 7),PL(  39, 8),PL(  69, 9),
    PL(  64, 9),PL( 114,10),PL(  99,10),PL(  87,10),
    PL( 158,11),PL( 140,11),PL( 252,12),PL( 212,12),
    PL( 199,12),PL( 387,13),PL( 365,13),PL(  26,10),
    PL(  75, 9),PL(  36, 8),PL(  68, 9),PL(  65, 9),
    PL( 115,10),PL( 101,10),PL( 179,11),PL( 164,11),
    PL( 155,11),PL( 264,12),PL( 246,12),PL( 226,12),
    PL( 395,13),PL( 382,13),PL( 362,13),PL(   9, 9),
    PL(  66, 9),PL(  30, 8),PL(  59, 9),PL(  56, 9),
    PL( 102,10),PL( 185,11),PL( 173,11),PL( 265,12),
    PL( 142,11),PL( 253,12),PL( 232,12),PL( 400,13),
    PL( 388,13),PL( 378,13),PL( 445,14),PL(  16,10),
    PL( 111,10),PL(  54, 9),PL(  52, 9),PL( 100,10),
    PL( 184,11),PL( 178,11),PL( 160,11),PL( 133,11),
    PL( 257,12),PL( 244,12),PL( 228,12),PL( 217,12),
    PL( 385,13),PL( 366,13),PL( 715,14),PL(  10,10),
    PL(  98,10),PL(  48, 9),PL(  91,10),PL(  88,10),
    PL( 165,11),PL( 157,11),PL( 148,11),PL( 261,12),
    PL( 248,12),PL( 407,13),PL( 397,13),PL( 372,13),
    PL( 380,13),PL( 889,15),PL( 884,15),PL(   8,10),
    PL(  85,10),PL(  84,10),PL(  81,10),PL( 159,11),
    PL( 156,11),PL( 143,11),PL( 260,12),PL( 249,12),
    PL( 427,13),PL( 401,13),PL( 392,13),PL( 383,13),
    PL( 727,14),PL( 713,14),PL( 708,14),PL(   7,10),
    PL( 154,11),PL(  76,10),PL(  73,10),PL( 141,11),
    PL( 131,11),PL( 256,12),PL( 245,12),PL( 426,13),
    PL( 406,13),PL( 394,13),PL( 384,13),PL( 735,14),
    PL( 359,13),PL( 710,14),PL( 352,13),PL(  11,11),
    PL( 139,11),PL( 129,11),PL(  67,10),PL( 125,11),
    PL( 247,12),PL( 233,12),PL( 229,12),PL( 219,12),
    PL( 393,13),PL( 743,14),PL( 737,14),PL( 720,14),
    PL( 885,15),PL( 882,15),PL( 439,14),PL(   4,10),
    PL( 243,12),PL( 120,11),PL( 118,11),PL( 115,11),
    PL( 227,12),PL( 223,12),PL( 396,13),PL( 746,14),
    PL( 742,14),PL( 736,14),PL( 721,14),PL( 712,14),
    PL( 706,14),PL( 223,13),PL( 436,14),PL(   6,11),
    PL( 202,12),PL( 224,12),PL( 222,12),PL( 218,12),
    PL( 216,12),PL( 389,13),PL( 386,13),PL( 381,13),
    PL( 364,13),PL( 888,15),PL( 443,14),PL( 707,14),
    PL( 440,14),PL( 437,14),PL(1728,16),PL(   4,11),
    PL( 747,14),PL( 211,12),PL( 210,12),PL( 208,12),
    PL( 370,13),PL( 379,13),PL( 734,14),PL( 723,14),
    PL( 714,14),PL(1735,16),PL( 883,15),PL( 877,15),
    PL( 876,15),PL(3459,17),PL( 865,15),PL(   2,11),
    PL( 377,13),PL( 369,13),PL( 102,11),PL( 187,12),
    PL( 726,14),PL( 722,14),PL( 358,13),PL( 711,14),
    PL( 709,14),PL( 866,15),PL(1734,16),PL( 871,15),
    PL(3458,17),PL( 870,15),PL( 434,14),PL(   0,11),
    PL(  12, 9),PL(  10, 8),PL(   7, 8),PL(  11, 9),
    PL(  10, 9),PL(  17,10),PL(  11,10),PL(   9,10),
    PL(  13,11),PL(  12,11),PL(  10,11),PL(   7,11),
    PL(   5,11),PL(   3,11),PL(   1,11),PL(   3, 8),
    PL(  15, 4),PL(  13, 4),PL(  46, 6),PL(  80, 7),
    PL( 146, 8),PL( 262, 9),PL( 248, 9),PL( 434,10),
    PL( 426,10),PL( 669,11),PL( 653,11),PL( 649,11),
    PL( 621,11),PL( 517,11),PL(1032,12),PL(  88, 9),
    PL(  14, 4),PL(  12, 4),PL(  21, 5),PL(  38, 6),
    PL(  71, 7),PL( 130, 8),PL( 122, 8),PL( 216, 9),
    PL( 209, 9),PL( 198, 9),PL( 327,10),PL( 345,10),
    PL( 319,10),PL( 297,10),PL( 279,10),PL(  42, 8),
    PL(  47, 6),PL(  22, 5),PL(  41, 6),PL(  74, 7),
    PL(  68, 7),PL( 128, 8),PL( 120, 8),PL( 221, 9),
    PL( 207, 9),PL( 194, 9),PL( 182, 9),PL( 340,10),
    PL( 315,10),PL( 295,10),PL( 541,11),PL(  18, 7),
    PL(  81, 7),PL(  39, 6),PL(  75, 7),PL(  70, 7),
    PL( 134, 8),PL( 125, 8),PL( 116, 8),PL( 220, 9),
    PL( 204, 9),PL( 190, 9),PL( 178, 9),PL( 325,10),
    PL( 311,10),PL( 293,10),PL( 271,10),PL(  16, 7),
    PL( 147, 8),PL(  72, 7),PL(  69, 7),PL( 135, 8),
    PL( 127, 8),PL( 118, 8),PL( 112, 8),PL( 210, 9),
    PL( 200, 9),PL( 188, 9),PL( 352,10),PL( 323,10),
    PL( 306,10),PL( 285,10),PL( 540,11),PL(  14, 7),
    PL( 263, 9),PL(  66, 7),PL( 129, 8),PL( 126, 8),
    PL( 119, 8),PL( 114, 8),PL( 214, 9),PL( 202, 9),
    PL( 192, 9),PL( 180, 9),PL( 341,10),PL( 317,10),
    PL( 301,10),PL( 281,10),PL( 262,10),PL(  12, 7),
    PL( 249, 9),PL( 123, 8),PL( 121, 8),PL( 117, 8),
    PL( 113, 8),PL( 215, 9),PL( 206, 9),PL( 195, 9),
    PL( 185, 9),PL( 347,10),PL( 330,10),PL( 308,10),
    PL( 291,10),PL( 272,10),PL( 520,11),PL(  10, 7),
    PL( 435,10),PL( 115, 8),PL( 111, 8),PL( 109, 8),
    PL( 211, 9),PL( 203, 9),PL( 196, 9),PL( 187, 9),
    PL( 353,10),PL( 332,10),PL( 313,10),PL( 298,10),
    PL( 283,10),PL( 531,11),PL( 381,11),PL(  17, 8),
    PL( 427,10),PL( 212, 9),PL( 208, 9),PL( 205, 9),
    PL( 201, 9),PL( 193, 9),PL( 186, 9),PL( 177, 9),
    PL( 169, 9),PL( 320,10),PL( 303,10),PL( 286,10),
    PL( 268,10),PL( 514,11),PL( 377,11),PL(  16, 8),
    PL( 335,10),PL( 199, 9),PL( 197, 9),PL( 191, 9),
    PL( 189, 9),PL( 181, 9),PL( 174, 9),PL( 333,10),
    PL( 321,10),PL( 305,10),PL( 289,10),PL( 275,10),
    PL( 521,11),PL( 379,11),PL( 371,11),PL(  11, 8),
    PL( 668,11),PL( 184, 9),PL( 183, 9),PL( 179, 9),
    PL( 175, 9),PL( 344,10),PL( 331,10),PL( 314,10),
    PL( 304,10),PL( 290,10),PL( 277,10),PL( 530,11),
    PL( 383,11),PL( 373,11),PL( 366,11),PL(  10, 8),
    PL( 652,11),PL( 346,10),PL( 171, 9),PL( 168, 9),
    PL( 164, 9),PL( 318,10),PL( 309,10),PL( 299,10),
    PL( 287,10),PL( 276,10),PL( 263,10),PL( 513,11),
    PL( 375,11),PL( 368,11),PL( 362,11),PL(   6, 8),
    PL( 648,11),PL( 322,10),PL( 316,10),PL( 312,10),
    PL( 307,10),PL( 302,10),PL( 292,10),PL( 284,10),
    PL( 269,10),PL( 261,10),PL( 512,11),PL( 376,11),
    PL( 370,11),PL( 364,11),PL( 359,11),PL(   4, 8),
    PL( 620,11),PL( 300,10),PL( 296,10),PL( 294,10),
    PL( 288,10),PL( 282,10),PL( 273,10),PL( 266,10),
    PL( 515,11),PL( 380,11),PL( 374,11),PL( 369,11),
    PL( 365,11),PL( 361,11),PL( 357,11),PL(   2, 8),
    PL(1033,12),PL( 280,10),PL( 278,10),PL( 274,10),
    PL( 267,10),PL( 264,10),PL( 259,10),PL( 382,11),
    PL( 378,11),PL( 372,11),PL( 367,11),PL( 363,11),
    PL( 360,11),PL( 358,11),PL( 356,11),PL(   0, 8),
    PL(  43, 8),PL(  20, 7),PL(  19, 7),PL(  17, 7),
    PL(  15, 7),PL(  13, 7),PL(  11, 7),PL(   9, 7),
    PL(   7, 7),PL(   6, 7),PL(   4, 7),PL(   7, 8),
    PL(   5, 8),PL(   3, 8),PL(   1, 8),PL(   3, 4),
};

/*
 * copy data out of the internal MP3 bit buffer into a user supplied buffer.
 */
static int
copy_buffer(lame_t gfc, unsigned char *buffer, int size)
{
    bit_stream_t *bs=&gfc->bs;
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
	int copy_len;
	if (bs->totbyte + spec_idx == bs->header[bs->w_ptr].write_timing) {
	    int i = gfc->sideinfo_len;
	    if (pbuf+i >= pend) return -1; /* buffer is too small */
	    memcpy(pbuf, bs->header[bs->w_ptr].buf, i);
	    pbuf += i;
	    bs->w_ptr = (bs->w_ptr + 1) & (MAX_HEADER_BUF-1);
	}
	copy_len = bs->header[bs->w_ptr].write_timing - bs->totbyte - spec_idx;
	if (copy_len > bs->bitidx - spec_idx)
	    copy_len = bs->bitidx - spec_idx;
	if (pbuf + copy_len >= pend) return -1; /* buffer is too small */
	memcpy(pbuf, &bs->buf[spec_idx], copy_len);
	spec_idx += copy_len;
	pbuf += copy_len;
    } while (spec_idx < bs->bitidx);
    bs->totbyte += spec_idx;

    memset(bs->buf, 0, spec_idx);
    bs->bitidx = 0;
    minimum = pbuf - buffer;

    gfc->nMusicCRC = calculateCRC((char*)buffer, minimum, gfc->nMusicCRC);
#ifdef HAVE_MPGLIB
    if (gfc->decode_on_the_fly) {  /* decode the frame */
	/* re-synthesis to pcm.  Repeat until we get a samples_out=0 */
	sample_t pcm_buf[2][1152];
	int mp3_in = minimum, i, samples_out;
	for (;;) {
	    FLOAT peak;
	    samples_out = decode1_unclipped(gfc->pmp_replaygain, buffer, mp3_in, pcm_buf[0], pcm_buf[1]);
	    /* samples_out = 0:  need more data to decode 
             * samples_out = -1:  error.  Lets assume 0 pcm output 
             * samples_out = number of samples output */
	    if (samples_out <= 0) {
                /* error decoding. Not fatal, but might screw up 
                 * the ReplayGain tag. What should we do? Ignore for now */
                break;
            }

	    /* set the lenght of the mp3 input buffer to zero, so that in the 
	     * next iteration of the loop we will be querying mpglib about 
	     * buffered data */
	    mp3_in = 0;

	    /* process the PCM data */
	    /* this should not be possible, and indicates we have 
	     * overflown the pcm_buf buffer */
	    assert(samples_out <= 1152);

	    peak = gfc->PeakSample;
	    for (i=0; i<samples_out; i++) {
		if (peak <  pcm_buf[0][i])
		    peak =  pcm_buf[0][i];
		if (peak < -pcm_buf[0][i])
		    peak = -pcm_buf[0][i];
	    }
	    if (gfc->channels_out > 1) {
		for (i=0; i<samples_out; i++) {
		    if (peak <  pcm_buf[1][i])
			peak =  pcm_buf[1][i];
		    if (peak < -pcm_buf[1][i])
			peak = -pcm_buf[1][i];
		}
	    }
	    gfc->PeakSample = peak;

	    if (gfc->findReplayGain
		&& AnalyzeSamples(&gfc->rgdata, pcm_buf[0], pcm_buf[1], samples_out, gfc->channels_out) == GAIN_ANALYSIS_ERROR)
		return -6;
	}
    } /* if (gfc->decode_on_the_fly) */ 
#endif

    return minimum;
}
/***********************************************************************
 * compute bits-per-frame
 **********************************************************************/
int
getframebytes(const lame_t gfc)
{
    /* get bitrate in kbps */
    int bit_rate = bitrate_table[gfc->version][gfc->bitrate_index];
    if (!bit_rate)
	bit_rate = gfc->mean_bitrate_kbps;
    assert ( bit_rate <= 550 );

    /* main encoding routine toggles padding on and off */
    /* one Layer3 Slot consists of 8 bits */
    return gfc->mode_gr*bit_rate*(1000*576/8) / gfc->out_samplerate
	+ gfc->padding - gfc->sideinfo_len;
}

#ifndef NDEBUG
# define putbits24(a,b,c) assert(((unsigned int)c) <= 25u), assert(((unsigned int)b) < (1ul << (c))), putbits24main(a,b,c)
#else
# define putbits24(a,b,c) putbits24main(a,b,c)
#endif

/*write j bits into the bit stream */
inline static void
putbits24main(bit_stream_t *bs, unsigned int val, int j)
{
    unsigned char *p = &bs->buf[bs->bitidx >> 3];

    val <<= (32 - j - (bs->bitidx & 7));
    bs->bitidx += j;

    p[0] |= val >> 24;
    p[1]  = val >> 16;
    p[2]  = val >> 8;
    p[3]  = val;
}

inline static void
putbits8(bit_stream_t *bs, unsigned int val, int j)
{
    unsigned char *p = &bs->buf[bs->bitidx >> 3];

    val <<= (16 - j - (bs->bitidx & 7));
    bs->bitidx += j;

    p[0] |= val >> 8;
    p[1]  = val;
}

inline static void
putbits16(bit_stream_t *bs, unsigned int val, int j)
{
    unsigned char *p = &bs->buf[bs->bitidx >> 3];

    val <<= (24 - j - (bs->bitidx & 7));
    bs->bitidx += j;

    p[0] |= val >> 16;
    p[1]  = val >> 8;
    p[2]  = val;
}

inline static void
putbits(bit_stream_t *bs, unsigned int val, int j)
{
    if (j > 25) {
	putbits8(bs, val>>24, j-24);
	j = 24;
    }
    putbits24main(bs, val, j);
}

static const unsigned char quadcode[2][16*2]  = {
    {
	1+0,  4+1,  4+1,  5+2,  4+1,  6+2,  5+2,  6+3,
	4+1,  5+2,  5+2,  6+3,  5+2,  6+3,  6+3,  6+4,

	1 << 0,  5 << 1,  4 << 1,  5 << 2,  6 << 1,  5 << 2,  4 << 2,  4 << 3,
	7 << 1,  3 << 2,  6 << 2,  0 << 3,  7 << 2,  2 << 3,  3 << 3,  1 << 4
    },
    {
	4+0,  4+1,  4+1,  4+2,  4+1,  4+2,  4+2,  4+3,
	4+1,  4+2,  4+2,  4+3,  4+2,  4+3,  4+3,  4+4,

	15 << 0, 14 << 1, 13 << 1, 12 << 2, 11 << 1, 10 << 2,  9 << 2,  8 << 3,
	 7 << 1,  6 << 2,  5 << 2,  4 << 3,  3 << 2,  2 << 3,  1 << 3,  0 << 4
    }
};

inline static void
Huf_count1(bit_stream_t *bs, gr_info *gi)
{
    int i, wcode, remain, bitidx;
    const unsigned char * const hcode = quadcode[gi->table_select[3]];
    unsigned char *ptr, *ptr0;
    assert((unsigned int)gi->table_select[3] < 2u);

    ptr0 = ptr = &bs->buf[bs->bitidx >> 3];
    remain = 32 - (bs->bitidx & 7);
    wcode = *ptr << 24;
    for (i = gi->big_values; i < gi->count1; i += 4) {
	int huffbits = 0, p = 0;
	assert((unsigned int)(gi->l3_enc[i] | gi->l3_enc[i+1]
			      | gi->l3_enc[i+2] | gi->l3_enc[i+3]) <= 1u);

	if (gi->l3_enc[i  ]) {
	    p = 8;
	    huffbits = signbits(gi->xr[i  ]);
	}
	if (gi->l3_enc[i+1]) {
	    p += 4;
	    huffbits = huffbits*2 + signbits(gi->xr[i+1]);
	}
	if (gi->l3_enc[i+2]) {
	    p += 2;
	    huffbits = huffbits*2 + signbits(gi->xr[i+2]);
	}
	if (gi->l3_enc[i+3]) {
	    p++;
	    huffbits = huffbits*2 + signbits(gi->xr[i+3]);
	}

	/* 0 < hcode[p] <= 10 */
	wcode |= (huffbits + hcode[p+16]) << (remain - hcode[p]);
	remain -= hcode[p];
	if (remain <= 16) {
	    *ptr++ = wcode >> 24;
	    *ptr++ = wcode >> 16;
	    remain += 16;
	    wcode <<= 16;
	}
    }
    assert(i == gi->count1);

    bitidx = (bs->bitidx & ~7u) + (ptr - ptr0) * 8;
    remain -= 32;
    if (remain) {
	*ptr++ = wcode >> 24;
	*ptr++ = wcode >> 16;
	bitidx -= remain;
    }
    bs->bitidx = bitidx;
}

/* Implements the pseudocode of page 98 of the IS */
static void
Huffmancode_esc(bit_stream_t *bs, int tablesel, int i, int end, gr_info *gi)
{
    int xlen = htESC_xlen[tablesel]; /* 1 <= xlen <= 13 */
#ifndef NDEBUG
    int linmax = (1 << xlen) + 15;
#endif
    tablesel = (tablesel & 8) * 32;
    do {
	int xbits = 0;
	int ext = 0;
	int x1 = gi->l3_enc[i];
	int x2 = gi->l3_enc[i+1];
	int hb, hlen;

	if (x1 != 0) {
	    if (x1 > 14) {
		/* use ESC-words */
		assert (x1 < linmax);
		ext    = (x1-15) << 1;
		xbits  = xlen;
		x1     = 15;
	    }
	    ext += signbits(gi->xr[i]);
	    xbits++;
	    x1 *= 16;
	}

	if (x2 != 0) {
	    x1 += x2;
	    x2 -= 15;
	    if (x2 >= 0) {
		assert (x2 < linmax);
		ext  <<= xlen;
		ext   |= x2;
		xbits += xlen;
		x1    -= x2;
	    }
	    ext = ext*2 + signbits(gi->xr[i+1]);
	    xbits++;
	}
	x1 += tablesel;
	/* 0 < escLen[] <= 17 */
	hb = escHBL[x1] >> 5;
	hlen = escHBL[x1] & 31;
	if (hlen == 17) hb += 2048;
	if (hlen + xbits < 25 && xbits) {
	    putbits24(bs, ext + (hb << xbits), hlen + xbits);
	} else {
	    putbits16(bs, hb, hlen);
	    if (xbits)      /* 0 <= xbits <= 28 */
		putbits(bs, ext, xbits);
	}
    } while ((i += 2) < end);
}

inline static void
Huf_bigvalue(bit_stream_t *bs, int tablesel, int start, int end, gr_info *gi)
{
    if (tablesel == 0 || start >= end)
	return;

    if (tablesel >= 16)
	Huffmancode_esc(bs, tablesel-16, start, end, gi);
    else {
	static const codetab_t * htTable[] = {
	    ht1, ht2, ht3, NULL, ht5, ht6, ht7, ht8, ht9, ht10,
	    ht11,ht12,ht13, NULL, ht15
	};
	const codetab_t *t = htTable[tablesel-1];
	int xlen = t[0].len;
	int wlen = 0, wcode = 0;
	do {
	    int code, clen;
	    int x1 = gi->l3_enc[start], x2 = gi->l3_enc[start+1];
	    assert(x1 < xlen && x2 < xlen);

	    code = x1*xlen + x2;
	    clen = t[code+1].len;
	    code = t[code+1].code;
	    wlen += clen;
	    if (wlen > 25) {
		putbits24(bs, wcode, wlen - clen);
		wlen = clen;
		wcode = 0;
	    }
	    if (x1) code = code*2 + signbits(gi->xr[start  ]);
	    if (x2) code = code*2 + signbits(gi->xr[start+1]);
	    wcode = (wcode << clen) + code;
	} while ((start += 2) < end);
	putbits24(bs, wcode, wlen);
    }
}

/*
  Note the discussion of huffmancodebits() on pages 28
  and 29 of the IS, as well as the definitions of the side
  information on pages 26 and 27.
  */
static void
Huffmancodebits(lame_t gfc, gr_info *gi)
{
#ifndef NDEBUG
    int data_bits = gfc->bs.bitidx + gi->part2_3_length;
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
    assert(gfc->bs.bitidx == data_bits - gi->count1bits);

    Huf_count1(&gfc->bs, gi);
    assert(gfc->bs.bitidx == data_bits);
}

/*write N bits into the header */
inline static int
writeheader(char *p, int val, int j, int ptr)
{
    p += ptr >> 3;
    assert(0 < j && j <= 17);
    val <<= (24 - j - (ptr&7));
    p[0] |= val >> 16;
    p[1]  = val >> 8;
    p[2]  = val;
    return ptr + j;
}

void
CRC_writeheader(char *header, int len)
{
    uint16_t crc;
    crc = calculateCRC(&header[2], 2, 0xffff);
    crc = calculateCRC(&header[6], len - 6, crc);
    header[4] = crc >> 8;
    header[5] = crc & 255;
}

static int
writeTableHeader(gr_info *gi, int ptr, char *p)
{
    static const short blockConv[] = {1*2048+8192, 3*2048+8192, 2*2048+8192};
    int tsel;
    if (gi->table_select[0] == 14)
	gi->table_select[0] = 16;
    if (gi->table_select[0] == 4)
	gi->table_select[0] = 6;
    if (gi->table_select[1] == 14)
	gi->table_select[1] = 16;
    if (gi->table_select[1] == 4)
	gi->table_select[1] = 6;
    tsel = gi->table_select[0]*32 + gi->table_select[1];
    if (gi->block_type != NORM_TYPE) {
	writeheader(p,
		    blockConv[gi->block_type-1]
		    + gi->mixed_block_flag*1024 + tsel,
		    1+2+1+5*2, ptr);

	writeheader(p, gi->subblock_gain[1]*64
		    + gi->subblock_gain[2]*8
		    + gi->subblock_gain[3], 3*3, ptr+1+2+1+5*2);
    } else {
	assert((unsigned int)gi->region0_count < 16u);
	assert((unsigned int)gi->region1_count <  8u);

	if (gi->table_select[2] == 14)
	    gi->table_select[2] = 16;
	if (gi->table_select[2] == 4)
	    gi->table_select[2] = 6;
	writeheader(p, tsel*32 + gi->table_select[2], 1+5*3, ptr);
	writeheader(p, gi->region0_count*8 + gi->region1_count, 4+3,
		    ptr+1+5*3);
    }
    return ptr + 23;
}

inline static void
encodeBitStream(lame_t gfc)
{
    char *p = gfc->bs.header[gfc->bs.h_ptr].buf;
    int gr, ch, ptr;
    assert(gfc->main_data_begin >= 0);

    ptr = gfc->bs.h_ptr;
    gfc->bs.h_ptr = (ptr + 1) & (MAX_HEADER_BUF-1);
    assert(gfc->bs.h_ptr != gfc->bs.w_ptr);

    gfc->bs.header[gfc->bs.h_ptr].write_timing
	= gfc->bs.header[ptr].write_timing + getframebytes(gfc);

    p[0] = 0xff;
    p[1] = 0xf0 - (gfc->out_samplerate < 16000)*16
	+ gfc->version*8 + (4 - 3)*2 + (!gfc->error_protection);
    p[2] = gfc->bitrate_index*16
	+ gfc->samplerate_index*4 + gfc->padding*2 + gfc->extension;
    p[3] = gfc->mode*64 + gfc->mode_ext*16
	+ gfc->copyright*8 + gfc->original*4 + gfc->emphasis;

    memset(p+4, 0, gfc->sideinfo_len-4);
    ptr = 32;
    if (gfc->error_protection)
	ptr += 16;
    ptr = writeheader(p, gfc->main_data_begin, 7+gfc->mode_gr, ptr);
    if (gfc->mode_gr == 2) {
	/* MPEG1 */
	ptr += 7 - gfc->channels_out*2; /* private_bits */
	for (ch = 0; ch < gfc->channels_out; ch++)
	    ptr = writeheader(p, gfc->scfsi[ch], 4, ptr);

	for (gr = 0; gr < 2; gr++) {
	    for (ch = 0; ch < gfc->channels_out; ch++) {
		gr_info *gi = &gfc->tt[gr][ch];
#ifndef NDEBUG
		int headerbits = gfc->bs.bitidx + gi->part2_length;
#endif
		int slen, sfb;
		ptr = writeheader(p, gi->part2_3_length+gi->part2_length,
				  12, ptr);
		ptr = writeheader(p, gi->big_values / 2,        9, ptr);
		ptr = writeheader(p, gi->global_gain,           8, ptr);
		ptr = writeheader(p, gi->scalefac_compress,     4, ptr);
		ptr = writeTableHeader(gi, ptr, p);
		ptr = writeheader(p,
				  (gi->preflag > 0)*4 + gi->scalefac_scale*2
				  + gi->table_select[3], 3, ptr);
		assert((unsigned int)gi->scalefac_scale < 2u);

		slen = s1bits[gi->scalefac_compress];
		if (slen)
		    for (sfb = 0; sfb < gi->sfbdivide; sfb++)
			if (gi->scalefac[sfb] != SCALEFAC_SCFSI_FLAG)
			    putbits8(&gfc->bs, Max(gi->scalefac[sfb],0), slen);
		slen = s2bits[gi->scalefac_compress];
		if (slen)
		    for (sfb = gi->sfbdivide; sfb < gi->sfbmax; sfb++)
			if (gi->scalefac[sfb] != SCALEFAC_SCFSI_FLAG)
			    putbits8(&gfc->bs, Max(gi->scalefac[sfb],0), slen);
		assert(headerbits == gfc->bs.bitidx);
		Huffmancodebits(gfc, gi);
	    } /* for ch */
	} /* for gr */
    } else {
	/* MPEG2 */
	ptr += gfc->channels_out; /* private_bits */
	for (ch = 0; ch < gfc->channels_out; ch++) {
	    gr_info *gi = &gfc->tt[0][ch];
	    int partition, sfb = 0, part;
#ifndef NDEBUG
	    int headerbits = gfc->bs.bitidx + gi->part2_length;
#endif
	    ptr = writeheader(p, gi->part2_3_length+gi->part2_length, 12, ptr);
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
		for (part = 0; part < SBMAX_l; part++)
		    gi->scalefac[part] -= pretab[part];
		part = 500 + gi->slen[0]*3 + gi->slen[1];
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
	    ptr = writeTableHeader(gi, ptr, p);
	    ptr = writeheader(p, gi->scalefac_scale,  1, ptr);
	    ptr = writeheader(p, gi->table_select[3], 1, ptr);

	    for (partition = 0; partition < 4; partition++) {
		int sfbend
		    = sfb + nr_of_sfb_block[gi->scalefac_compress][partition];
		int slen = gi->slen[partition];
		if (slen)
		    for (; sfb < sfbend; sfb++)
			putbits8(&gfc->bs, Max(gi->scalefac[sfb], 0), slen);
		sfb = sfbend;
	    }
	    assert(headerbits == gfc->bs.bitidx);
	    Huffmancodebits(gfc, gi);
	} /* for ch */
    } /* for MPEG version */
    assert(ptr == gfc->sideinfo_len * 8);

    if (gfc->error_protection)
	CRC_writeheader(p, gfc->sideinfo_len);
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
compute_flushbits(const lame_t gfc, int *total_bytes_output)
{
    bit_stream_t *bs = &gfc->bs;
    int flushbits;
    int bitsPerFrame;
    int last_ptr = (bs->h_ptr - 1) & (MAX_HEADER_BUF-1);

    /* add this many bits to bitstream so we can flush all headers */
    flushbits = (bs->header[last_ptr].write_timing - bs->totbyte)*8;
    *total_bytes_output = flushbits;

    if (flushbits >= 0)
	/* if flushbits >= 0, some headers have not yet been written */
	/* add the size of the headers to the total byte output */
	*total_bytes_output += 8 * gfc->sideinfo_len
	    * ((bs->h_ptr - bs->w_ptr) & (MAX_HEADER_BUF-1));

    /* finally, add some bits so that the last frame is complete
     * these bits are not necessary to decode the last frame, but
     * some decoders will ignore last frame if these bits are missing 
     */
    bitsPerFrame = getframebytes(gfc)*8;
    flushbits += bitsPerFrame;
    assert(flushbits >= 0);
    *total_bytes_output
	= (*total_bytes_output + bitsPerFrame + bs->bitidx + 7) / 8;
    return flushbits;
}

int
flush_bitstream(lame_t gfc, unsigned char *buffer, int size)
{
    int dummy;

    /* save the ReplayGain value */
    if (gfc->findReplayGain) {
	FLOAT RadioGain = (FLOAT) GetTitleGain(&gfc->rgdata);
	assert(RadioGain != GAIN_NOT_ENOUGH_SAMPLES); 
	gfc->RadioGain = (int) floor( RadioGain * (FLOAT)10.0 + (FLOAT)0.5 ); /* round to nearest */
    }

    if (gfc->decode_on_the_fly) {
	/* find the gain and scale change required for no clipping */
	gfc->noclipGainChange
	    = (int) ceil(log10(gfc->PeakSample / (FLOAT)32767.0) * (FLOAT)20.0*(FLOAT)10.0);  /* round up */
	if (gfc->noclipGainChange > (FLOAT)0.0) { /* clipping occurs */
	    if (gfc->scale == (FLOAT)1.0)
		gfc->noclipScale = floor( ((FLOAT)32767.0 / gfc->PeakSample) * (FLOAT)100.0 ) / (FLOAT)100.0;  /* round down */
	    else
		/* the user specified his own scaling factor. We could suggest 
		 * the scaling factor of (32767.0/gfc->PeakSample)*(gfc->scale)
		 * but it's usually very inaccurate. So we'd rather not advice
		 * him/her on the scaling factor. */
		gfc->noclipScale = -1;
	}
	else  /* no clipping */
	    gfc->noclipScale = -1;
    }

    /* we have padded out all frames with ancillary data, which is the
       same as filling the bitreservoir with ancillary data, so : */
    gfc->bs.bitidx += compute_flushbits(gfc, &dummy);
    gfc->ResvSize = gfc->main_data_begin = 0;
    return copy_buffer(gfc, buffer, size);
}

/*
  Some combinations of bitrate, Fs, and stereo make it impossible to stuff
  out a frame using just main_data, due to the limited number of bits to
  indicate main_data_length. In these situations, we put stuffing bits into
  the ancillary data...
*/
inline static void
drain_into_ancillary(lame_t gfc, int remainingBits)
{
    bit_stream_t *bs = &gfc->bs;
    unsigned char *p, *end;

    p = &bs->buf[(bs->bitidx + 7) >> 3];
    bs->bitidx += remainingBits;
    end = &bs->buf[bs->bitidx >> 3];
    if (p == end) return;

    *p++ = PADM_PATTERN; if (p == end) return;
    *p++ = PADB_PATTERN; if (p == end) return;
    *p++ = PADN_PATTERN; if (p == end) return;
    *p++ = PADF_PATTERN; if (p == end) return;

    {
	const char *version = get_lame_short_version ();
	int len = strlen(version);
	if (len < end - p) {
	    memcpy(p, version, len);
	    p += len;
	}
    }
    while (p != end)
	*p++ = PADDING_PATTERN;
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
format_bitstream(lame_t gfc, unsigned char *mp3buf, int mp3buf_size)
{
    int drainPre, drainbits = gfc->ResvSize & 7;

    /* reservoir is overflowed ? */
    if (drainbits < gfc->ResvSize - gfc->ResvMax)
	drainbits = gfc->ResvSize - gfc->ResvMax;
    assert(drainbits >= 0);

    /* drain as many bits as possible into previous frame ancillary data
     * In particular, in VBR mode ResvMax may have changed, and we have
     * to make sure main_data_begin does not create a reservoir bigger
     * than ResvMax  mt 4/00*/
    drainPre = drainbits & (~7U);
    if (drainPre > gfc->main_data_begin*8)
	drainPre = gfc->main_data_begin*8;
    gfc->main_data_begin -= drainPre/8;
    drain_into_ancillary(gfc, drainPre);
    encodeBitStream(gfc);
    gfc->bs.bitidx += drainbits - drainPre;

    gfc->main_data_begin = (gfc->ResvSize -= drainbits) >> 3;

    assert(gfc->bs.bitidx % 8 == 0);
    assert(gfc->ResvSize % 8 == 0);
    assert(gfc->ResvSize >= 0);

    /* copy mp3 bit buffer into array */
    return copy_buffer(gfc, mp3buf, mp3buf_size);
}
