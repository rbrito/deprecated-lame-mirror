/*
 *	MPEG layer 3 tables source file
 *
 *	Copyright (c) 1999 Albert L Faber
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
# include <config.h>
#endif

#include "util.h"
#include "tables.h"

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

#include <assert.h>
#include <psymodel.h>

#ifdef M_LN10
#define		LN_TO_LOG10		(M_LN10/10)
#else
#define         LN_TO_LOG10             0.2302585093
#endif

/*
  The following table is used to implement the scalefactor
  partitioning for MPEG2 as described in section
  2.4.3.2 of the IS. The indexing corresponds to the
  way the tables are presented in the IS:

  [table_number][row_in_table][column of nr_of_sfb]
*/
const int  nr_of_sfb_block [6] [3] [4] =
{
  {
    {6, 5, 5, 5},
    {9, 9, 9, 9},
    {6, 9, 9, 9}
  },
  {
    {6, 5, 7, 3},
    {9, 9, 12, 6},
    {6, 9, 12, 6}
  },
  {
    {11, 10, 0, 0},
    {18, 18, 0, 0},
    {15,18,0,0}
  },
  {
    {7, 7, 7, 0},
    {12, 12, 12, 0},
    {6, 15, 12, 0}
  },
  {
    {6, 6, 6, 3},
    {12, 9, 9, 6},
    {6, 12, 9, 6}
  },
  {
    {8, 8, 5, 0},
    {15,12,9,0},
    {6,18,9,0}
  }
};


/* Table B.6: layer3 preemphasis */
const int  pretab [SBMAX_l] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 2, 2, 3, 3, 3, 2, 0
};

/*
  Here are MPEG1 Table B.8 and MPEG2 Table B.1
  -- Layer III scalefactor bands. 
  Index into this using a method such as:
    idx  = fr_ps->header->sampling_frequency
           + (fr_ps->header->version * 3)
*/


const scalefac_struct sfBandIndex[9] =
{
  { /* Table B.2.b: 22.05 kHz */
    {0,6,12,18,24,30,36,44,54,66,80,96,116,140,168,200,238,284,336,396,464,522,576},
    {0,4,8,12,18,24,32,42,56,74,100,132,174,192}
  },
  { /* Table B.2.c: 24 kHz */                 /* docs: 332. mpg123(broken): 330 */
    {0,6,12,18,24,30,36,44,54,66,80,96,114,136,162,194,232,278, 332, 394,464,540,576},
    {0,4,8,12,18,26,36,48,62,80,104,136,180,192}
  },
  { /* Table B.2.a: 16 kHz */
    {0,6,12,18,24,30,36,44,54,66,80,96,116,140,168,200,238,284,336,396,464,522,576},
    {0,4,8,12,18,26,36,48,62,80,104,134,174,192}
  },
  { /* Table B.8.b: 44.1 kHz */
    {0,4,8,12,16,20,24,30,36,44,52,62,74,90,110,134,162,196,238,288,342,418,576},
    {0,4,8,12,16,22,30,40,52,66,84,106,136,192}
  },
  { /* Table B.8.c: 48 kHz */
    {0,4,8,12,16,20,24,30,36,42,50,60,72,88,106,128,156,190,230,276,330,384,576},
    {0,4,8,12,16,22,28,38,50,64,80,100,126,192}
  },
  { /* Table B.8.a: 32 kHz */
    {0,4,8,12,16,20,24,30,36,44,54,66,82,102,126,156,194,240,296,364,448,550,576},
    {0,4,8,12,16,22,30,42,58,78,104,138,180,192}
  },
  { /* MPEG-2.5 11.025 kHz */
    {0,6,12,18,24,30,36,44,54,66,80,96,116,140,168,200,238,284,336,396,464,522,576},
    {0/3,12/3,24/3,36/3,54/3,78/3,108/3,144/3,186/3,240/3,312/3,402/3,522/3,576/3}
  },
  { /* MPEG-2.5 12 kHz */
    {0,6,12,18,24,30,36,44,54,66,80,96,116,140,168,200,238,284,336,396,464,522,576},
    {0/3,12/3,24/3,36/3,54/3,78/3,108/3,144/3,186/3,240/3,312/3,402/3,522/3,576/3}
  },
  { /* MPEG-2.5 8 kHz */
    {0,12,24,36,48,60,72,88,108,132,160,192,232,280,336,400,476,566,568,570,572,574,576},
    {0/3,24/3,48/3,72/3,108/3,156/3,216/3,288/3,372/3,480/3,486/3,492/3,498/3,576/3}
  }
};



static const short      t1HB[]   = {
  1, 1, 
  1, 0};

static const short      t2HB[]   = {
  1, 2, 1,
  3, 1, 1,
  3, 2, 0};

static const short      t3HB[]   = {
  3, 2, 1,
  1, 1, 1,
  3, 2, 0};

static const short      t5HB[]   = {
  1, 2, 6, 5,
  3, 1, 4, 4,
  7, 5, 7, 1,
  6, 1, 1, 0};

static const short      t6HB[]   = {
  7, 3, 5, 1,
  6, 2, 3, 2,
  5, 4, 4, 1,
  3, 3, 2, 0};

static const short      t7HB[]   = {
   1, 2,10,19,16,10,
   3, 3, 7,10, 5, 3,
  11, 4,13,17, 8, 4,
  12,11,18,15,11, 2,
   7, 6, 9,14, 3, 1,
   6, 4, 5, 3, 2, 0};

static const short      t8HB[]   = {
  3, 4, 6, 18,12, 5,
  5, 1, 2, 16, 9, 3,
  7, 3, 5, 14, 7, 3,
 19,17,15, 13,10, 4,
 13, 5, 8, 11, 5, 1,
 12, 4, 4,  1, 1, 0};

static const short      t9HB[]   = {
  7, 5, 9, 14, 15, 7,
  6, 4, 5,  5,  6, 7,
  7, 6, 8,  8,  8, 5,
 15, 6, 9, 10,  5, 1,
 11, 7, 9,  6,  4, 1,
 14, 4, 6,  2,  6, 0};

static const short      t10HB[]   = {
  1, 2, 10, 23, 35, 30, 12, 17,
  3, 3,  8, 12, 18, 21, 12,  7,
 11, 9, 15, 21, 32, 40, 19,  6,
 14,13, 22, 34, 46, 23, 18,  7,
 20,19, 33, 47, 27, 22,  9,  3,
 31,22, 41, 26, 21, 20,  5,  3,
 14,13, 10, 11, 16,  6,  5,  1,
  9, 8,  7,  8,  4,  4,  2,  0};

static const short      t11HB[]   = {
  3, 4, 10, 24, 34, 33, 21, 15,
  5, 3,  4, 10, 32, 17, 11, 10,
 11, 7, 13, 18, 30, 31, 20,  5,
 25,11, 19, 59, 27, 18, 12,  5,
 35,33, 31, 58, 30, 16,  7,  5,
 28,26, 32, 19, 17, 15,  8, 14,
 14,12,  9, 13, 14,  9,  4,  1,
 11, 4,  6,  6,  6,  3,  2,  0};

static const short      t12HB[]   = {
  9,  6, 16, 33, 41, 39, 38,26,
  7,  5,  6,  9, 23, 16, 26,11,
 17,  7, 11, 14, 21, 30, 10, 7,
 17, 10, 15, 12, 18, 28, 14, 5,
 32, 13, 22, 19, 18, 16,  9, 5,
 40, 17, 31, 29, 17, 13,  4, 2,
 27, 12, 11, 15, 10,  7,  4, 1,
 27, 12,  8, 12,  6,  3,  1, 0};

static const short      t13HB[]   = {
  1,  5, 14, 21, 34, 51, 46, 71, 42, 52, 68, 52, 67, 44, 43, 19,
  3,  4, 12, 19, 31, 26, 44, 33, 31, 24, 32, 24, 31, 35, 22, 14,
 15, 13, 23, 36, 59, 49, 77, 65, 29, 40, 30, 40, 27, 33, 42, 16,
 22, 20, 37, 61, 56, 79, 73, 64, 43, 76, 56, 37, 26, 31, 25, 14,
 35, 16, 60, 57, 97, 75,114, 91, 54, 73, 55, 41, 48, 53, 23, 24,
 58, 27, 50, 96, 76, 70, 93, 84, 77, 58, 79, 29, 74, 49, 41, 17,
 47, 45, 78, 74,115, 94, 90, 79, 69, 83, 71, 50, 59, 38, 36, 15,
 72, 34, 56, 95, 92, 85, 91, 90, 86, 73, 77, 65, 51, 44, 43, 42,
 43, 20, 30, 44, 55, 78, 72, 87, 78, 61, 46, 54, 37, 30, 20, 16,
 53, 25, 41, 37, 44, 59, 54, 81, 66, 76, 57, 54, 37, 18, 39, 11,
 35, 33, 31, 57, 42, 82, 72, 80, 47, 58, 55, 21, 22, 26, 38, 22,
 53, 25, 23, 38, 70, 60, 51, 36, 55, 26, 34, 23, 27, 14,  9,  7,
 34, 32, 28, 39, 49, 75, 30, 52, 48, 40, 52, 28, 18, 17,  9,  5,
 45, 21, 34, 64, 56, 50, 49, 45, 31, 19, 12, 15, 10,  7,  6,  3,
 48, 23, 20, 39, 36, 35, 53, 21, 16, 23, 13, 10,  6,  1,  4,  2,
 16, 15, 17, 27, 25, 20, 29, 11, 17, 12, 16,  8,  1,  1,  0,  1};

static const short      t15HB[]   = {
   7, 12, 18, 53, 47, 76,124,108, 89,123,108,119,107, 81,122, 63,
  13,  5, 16, 27, 46, 36, 61, 51, 42, 70, 52, 83, 65, 41, 59, 36,
  19, 17, 15, 24, 41, 34, 59, 48, 40, 64, 50, 78, 62, 80, 56, 33,
  29, 28, 25, 43, 39, 63, 55, 93, 76, 59, 93, 72, 54, 75, 50, 29,
  52, 22, 42, 40, 67, 57, 95, 79, 72, 57, 89, 69, 49, 66, 46, 27,
  77, 37, 35, 66, 58, 52, 91, 74, 62, 48, 79, 63, 90, 62, 40, 38,
 125, 32, 60, 56, 50, 92, 78, 65, 55, 87, 71, 51, 73, 51, 70, 30,
 109, 53, 49, 94, 88, 75, 66,122, 91, 73, 56, 42, 64, 44, 21, 25,
  90, 43, 41, 77, 73, 63, 56, 92, 77, 66, 47, 67, 48, 53, 36, 20,
  71, 34, 67, 60, 58, 49, 88, 76, 67,106, 71, 54, 38, 39, 23, 15,
 109, 53, 51, 47, 90, 82, 58, 57, 48, 72, 57, 41, 23, 27, 62,  9,
  86, 42, 40, 37, 70, 64, 52, 43, 70, 55, 42, 25, 29, 18, 11, 11, 
 118, 68, 30, 55, 50, 46, 74, 65, 49, 39, 24, 16, 22, 13, 14,  7,
  91, 44, 39, 38, 34, 63, 52, 45, 31, 52, 28, 19, 14,  8,  9,  3,
 123, 60, 58, 53, 47, 43, 32, 22, 37, 24, 17, 12, 15, 10,  2,  1,
  71, 37, 34, 30, 28, 20, 17, 26, 21, 16, 10,  6,  8,  6,  2,  0};

static const short      t16HB[]   = {
   1,   5, 14, 44, 74, 63, 110, 93, 172, 149, 138, 242, 225, 195, 376, 17,
   3,   4, 12, 20, 35, 62,  53, 47,  83,  75,  68, 119, 201, 107, 207,  9,
  15,  13, 23, 38, 67, 58, 103, 90, 161,  72, 127, 117, 110, 209, 206, 16,
  45,  21, 39, 69, 64,114,  99, 87, 158, 140, 252, 212, 199, 387, 365, 26,
  75,  36, 68, 65,115,101, 179,164, 155, 264, 246, 226, 395, 382, 362,  9,
  66,  30, 59, 56,102,185, 173,265, 142, 253, 232, 400, 388, 378, 445, 16,
 111,  54, 52,100,184,178, 160,133, 257, 244, 228, 217, 385, 366, 715, 10,
  98,  48, 91, 88,165,157, 148,261, 248, 407, 397, 372, 380, 889, 884,  8,
  85,  84, 81,159,156,143, 260,249, 427, 401, 392, 383, 727, 713, 708,  7,
 154,  76, 73,141,131,256, 245,426, 406, 394, 384, 735, 359, 710, 352, 11,
 139, 129, 67,125,247,233, 229,219, 393, 743, 737, 720, 885, 882, 439,  4,
 243, 120,118,115,227,223, 396,746, 742, 736, 721, 712, 706, 223, 436,  6,
 202, 224,222,218,216,389, 386,381, 364, 888, 443, 707, 440, 437,1728,  4,
 747, 211,210,208,370,379, 734,723, 714,1735, 883, 877, 876,3459, 865,  2,
 377, 369,102,187,726,722, 358,711, 709, 866,1734, 871,3458, 870, 434,  0,
  12,  10,  7, 11, 10, 17,  11,  9,  13,  12,  10,   7,   5,   3,   1,  3};

static const short      t24HB[]   = {
   15, 13, 46, 80, 146, 262, 248, 434, 426, 669, 653, 649, 621, 517, 1032, 88,
   14, 12, 21, 38,  71, 130, 122, 216, 209, 198, 327, 345, 319, 297,  279, 42,
   47, 22, 41, 74,  68, 128, 120, 221, 207, 194, 182, 340, 315, 295,  541, 18,
   81, 39, 75, 70, 134, 125, 116, 220, 204, 190, 178, 325, 311, 293,  271, 16,
  147, 72, 69,135, 127, 118, 112, 210, 200, 188, 352, 323, 306, 285,  540, 14,
  263, 66,129,126, 119, 114, 214, 202, 192, 180, 341, 317, 301, 281,  262, 12,
  249,123,121,117, 113, 215, 206, 195, 185, 347, 330, 308, 291, 272,  520, 10,
  435,115,111,109, 211, 203, 196, 187, 353, 332, 313, 298, 283, 531,  381, 17,
  427,212,208,205, 201, 193, 186, 177, 169, 320, 303, 286, 268, 514,  377, 16,
  335,199,197,191, 189, 181, 174, 333, 321, 305, 289, 275, 521, 379,  371, 11,
  668,184,183,179, 175, 344, 331, 314, 304, 290, 277, 530, 383, 373,  366, 10,
  652,346,171,168, 164, 318, 309, 299, 287, 276, 263, 513, 375, 368,  362,  6,
  648,322,316,312, 307, 302, 292, 284, 269, 261, 512, 376, 370, 364,  359,  4,
  620,300,296,294, 288, 282, 273, 266, 515, 380, 374, 369, 365, 361,  357,  2,
 1033,280,278,274, 267, 264, 259, 382, 378, 372, 367, 363, 360, 358,  356,  0,
   43, 20, 19, 17,  15,  13,  11,   9,   7,   6,   4,   7,   5,   3,    1,  3};

static const short      t32HB[]   = {
  1 << 0,  5 << 1,  4 << 1,  5 << 2,  6 << 1,  5 << 2,  4 << 2,  4 << 3,
  7 << 1,  3 << 2,  6 << 2,  0 << 3,  7 << 2,  2 << 3,  3 << 3,  1 << 4};

static const short      t33HB[]   = {
 15 << 0, 14 << 1, 13 << 1, 12 << 2, 11 << 1, 10 << 2,  9 << 2,  8 << 3,
  7 << 1,  6 << 2,  5 << 2,  4 << 3,  3 << 2,  2 << 3,  1 << 3,  0 << 4};


const  char t1l[] = {
 1,  4, 
 3,  5};

const  char t2l[] = {
 1,  4,  7, 
 4,  5,  7, 
 6,  7,  8};

const  char t3l[] = {
 2,  3,  7, 
 4,  4,  7, 
 6,  7,  8};

const  char t5l[] = {
 1,  4,  7,  8, 
 4,  5,  8,  9, 
 7,  8,  9, 10, 
 8,  8,  9, 10};

const  char t6l[] = {
 3,  4,  6,  8, 
 4,  4,  6,  7, 
 5,  6,  7,  8, 
 7,  7,  8,  9};

const  char t7l[] = {
 1,  4,  7,  9,  9, 10, 
 4,  6,  8,  9,  9, 10, 
 7,  7,  9, 10, 10, 11, 
 8,  9, 10, 11, 11, 11, 
 8,  9, 10, 11, 11, 12, 
 9, 10, 11, 12, 12, 12};

const  char t8l[] = {
 2,  4,  7,  9,  9, 10, 
 4,  4,  6, 10, 10, 10, 
 7,  6,  8, 10, 10, 11, 
 9, 10, 10, 11, 11, 12, 
 9,  9, 10, 11, 12, 12, 
10, 10, 11, 11, 13, 13};

const  char t9l[] = {
 3,  4,  6,  7,  9, 10, 
 4,  5,  6,  7,  8, 10, 
 5,  6,  7,  8,  9, 10, 
 7,  7,  8,  9,  9, 10, 
 8,  8,  9,  9, 10, 11, 
 9,  9, 10, 10, 11, 11};

const  char t10l[] = {
 1,  4,  7,  9, 10, 10, 10, 11, 
 4,  6,  8,  9, 10, 11, 10, 10, 
 7,  8,  9, 10, 11, 12, 11, 11, 
 8,  9, 10, 11, 12, 12, 11, 12, 
 9, 10, 11, 12, 12, 12, 12, 12, 
10, 11, 12, 12, 13, 13, 12, 13, 
 9, 10, 11, 12, 12, 12, 13, 13, 
10, 10, 11, 12, 12, 13, 13, 13};

const  char t11l[] = {
 2,  4,  6,  8,  9, 10,  9, 10, 
 4,  5,  6,  8, 10, 10,  9, 10, 
 6,  7,  8,  9, 10, 11, 10, 10, 
 8,  8,  9, 11, 10, 12, 10, 11, 
 9, 10, 10, 11, 11, 12, 11, 12, 
 9, 10, 11, 12, 12, 13, 12, 13, 
 9,  9,  9, 10, 11, 12, 12, 12, 
 9,  9, 10, 11, 12, 12, 12, 12};

const  char t12l[] = {
 4,  4,  6,  8,  9, 10, 10, 10, 
 4,  5,  6,  7,  9,  9, 10, 10, 
 6,  6,  7,  8,  9, 10,  9, 10, 
 7,  7,  8,  8,  9, 10, 10, 10, 
 8,  8,  9,  9, 10, 10, 10, 11, 
 9,  9, 10, 10, 10, 11, 10, 11, 
 9,  9,  9, 10, 10, 11, 11, 12, 
10, 10, 10, 11, 11, 11, 11, 12};

const  char t13l[] = {
 1,  5,  7,  8,  9, 10, 10, 11, 10, 11, 12, 12, 13, 13, 14, 14, 
 4,  6,  8,  9, 10, 10, 11, 11, 11, 11, 12, 12, 13, 14, 14, 14, 
 7,  8,  9, 10, 11, 11, 12, 12, 11, 12, 12, 13, 13, 14, 15, 15, 
 8,  9, 10, 11, 11, 12, 12, 12, 12, 13, 13, 13, 13, 14, 15, 15, 
 9,  9, 11, 11, 12, 12, 13, 13, 12, 13, 13, 14, 14, 15, 15, 16, 
10, 10, 11, 12, 12, 12, 13, 13, 13, 13, 14, 13, 15, 15, 16, 16, 
10, 11, 12, 12, 13, 13, 13, 13, 13, 14, 14, 14, 15, 15, 16, 16, 
11, 11, 12, 13, 13, 13, 14, 14, 14, 14, 15, 15, 15, 16, 18, 18, 
10, 10, 11, 12, 12, 13, 13, 14, 14, 14, 14, 15, 15, 16, 17, 17, 
11, 11, 12, 12, 13, 13, 13, 15, 14, 15, 15, 16, 16, 16, 18, 17, 
11, 12, 12, 13, 13, 14, 14, 15, 14, 15, 16, 15, 16, 17, 18, 19, 
12, 12, 12, 13, 14, 14, 14, 14, 15, 15, 15, 16, 17, 17, 17, 18, 
12, 13, 13, 14, 14, 15, 14, 15, 16, 16, 17, 17, 17, 18, 18, 18, 
13, 13, 14, 15, 15, 15, 16, 16, 16, 16, 16, 17, 18, 17, 18, 18, 
14, 14, 14, 15, 15, 15, 17, 16, 16, 19, 17, 17, 17, 19, 18, 18, 
13, 14, 15, 16, 16, 16, 17, 16, 17, 17, 18, 18, 21, 20, 21, 18};

const  char t15l[] = {
 3,  5,  6,  8,  8,  9, 10, 10, 10, 11, 11, 12, 12, 12, 13, 14, 
 5,  5,  7,  8,  9,  9, 10, 10, 10, 11, 11, 12, 12, 12, 13, 13, 
 6,  7,  7,  8,  9,  9, 10, 10, 10, 11, 11, 12, 12, 13, 13, 13, 
 7,  8,  8,  9,  9, 10, 10, 11, 11, 11, 12, 12, 12, 13, 13, 13, 
 8,  8,  9,  9, 10, 10, 11, 11, 11, 11, 12, 12, 12, 13, 13, 13, 
 9,  9,  9, 10, 10, 10, 11, 11, 11, 11, 12, 12, 13, 13, 13, 14, 
10,  9, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 13, 13, 14, 14, 
10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 12, 13, 13, 13, 14, 
10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 14, 14, 14, 
10, 10, 11, 11, 11, 11, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 
11, 11, 11, 11, 12, 12, 12, 12, 12, 13, 13, 13, 13, 14, 15, 14, 
11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 15, 
12, 12, 11, 12, 12, 12, 13, 13, 13, 13, 13, 13, 14, 14, 15, 15, 
12, 12, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 14, 14, 15, 15, 
13, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 15, 15, 14, 15, 
13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 15, 15, 15, 15};

const  char t16_5l[] = {
 1,  5,  7,  9, 10, 10, 11, 11, 12, 12, 12, 13, 13, 13, 14, 11, 
 4,  6,  8,  9, 10, 11, 11, 11, 12, 12, 12, 13, 14, 13, 14, 11, 
 7,  8,  9, 10, 11, 11, 12, 12, 13, 12, 13, 13, 13, 14, 14, 12, 
 9,  9, 10, 11, 11, 12, 12, 12, 13, 13, 14, 14, 14, 15, 15, 13, 
10, 10, 11, 11, 12, 12, 13, 13, 13, 14, 14, 14, 15, 15, 15, 12, 
10, 10, 11, 11, 12, 13, 13, 14, 13, 14, 14, 15, 15, 15, 16, 13, 
11, 11, 11, 12, 13, 13, 13, 13, 14, 14, 14, 14, 15, 15, 16, 13, 
11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 15, 15, 17, 17, 13, 
11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 15, 15, 16, 16, 16, 13, 
12, 12, 12, 13, 13, 14, 14, 15, 15, 15, 15, 16, 15, 16, 15, 14, 
12, 13, 12, 13, 14, 14, 14, 14, 15, 16, 16, 16, 17, 17, 16, 13, 
13, 13, 13, 13, 14, 14, 15, 16, 16, 16, 16, 16, 16, 15, 16, 14, 
13, 14, 14, 14, 14, 15, 15, 15, 15, 17, 16, 16, 16, 16, 18, 14, 
15, 14, 14, 14, 15, 15, 16, 16, 16, 18, 17, 17, 17, 19, 17, 14, 
14, 15, 13, 14, 16, 16, 15, 16, 16, 17, 18, 17, 19, 17, 16, 14, 
11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14, 12};

const  char t16l[] = {
 1,  5,  7,  9, 10, 10, 11, 11, 12, 12, 12, 13, 13, 13, 14, 10, 
 4,  6,  8,  9, 10, 11, 11, 11, 12, 12, 12, 13, 14, 13, 14, 10, 
 7,  8,  9, 10, 11, 11, 12, 12, 13, 12, 13, 13, 13, 14, 14, 11, 
 9,  9, 10, 11, 11, 12, 12, 12, 13, 13, 14, 14, 14, 15, 15, 12, 
10, 10, 11, 11, 12, 12, 13, 13, 13, 14, 14, 14, 15, 15, 15, 11, 
10, 10, 11, 11, 12, 13, 13, 14, 13, 14, 14, 15, 15, 15, 16, 12, 
11, 11, 11, 12, 13, 13, 13, 13, 14, 14, 14, 14, 15, 15, 16, 12, 
11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 15, 15, 17, 17, 12, 
11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 15, 15, 16, 16, 16, 12, 
12, 12, 12, 13, 13, 14, 14, 15, 15, 15, 15, 16, 15, 16, 15, 13, 
12, 13, 12, 13, 14, 14, 14, 14, 15, 16, 16, 16, 17, 17, 16, 12, 
13, 13, 13, 13, 14, 14, 15, 16, 16, 16, 16, 16, 16, 15, 16, 13, 
13, 14, 14, 14, 14, 15, 15, 15, 15, 17, 16, 16, 16, 16, 18, 13, 
15, 14, 14, 14, 15, 15, 16, 16, 16, 18, 17, 17, 17, 19, 17, 13, 
14, 15, 13, 14, 16, 16, 15, 16, 16, 17, 18, 17, 19, 17, 16, 13, 
10, 10, 10, 11, 11, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 10};

const  char t24l[] = {
 4,  5,  7,  8,  9, 10, 10, 11, 11, 12, 12, 12, 12, 12, 13, 10, 
 5,  6,  7,  8,  9, 10, 10, 11, 11, 11, 12, 12, 12, 12, 12, 10, 
 7,  7,  8,  9,  9, 10, 10, 11, 11, 11, 11, 12, 12, 12, 13,  9, 
 8,  8,  9,  9, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12,  9, 
 9,  9,  9, 10, 10, 10, 10, 11, 11, 11, 12, 12, 12, 12, 13,  9, 
10,  9, 10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 12,  9, 
10, 10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 12, 13,  9, 
11, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 12, 13, 13, 10, 
11, 11, 11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 10, 
11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 13, 13, 13, 10, 
12, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13, 10, 
12, 12, 11, 11, 11, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13, 10, 
12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 10, 
12, 12, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 10, 
13, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 13, 10, 
 9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9, 10, 10, 10, 10,  6};

const  char t32l[]  = {
  1+0,  4+1,  4+1,  5+2,  4+1,  6+2,  5+2,  6+3,
  4+1,  5+2,  5+2,  6+3,  5+2,  6+3,  6+3,  6+4};

const  char t33l[]  = {
  4+0,  4+1,  4+1,  4+2,  4+1,  4+2,  4+2,  4+3,
  4+1,  4+2,  4+2,  4+3,  4+2,  4+3,  4+3,  4+4};


const struct huffcodetab ht[HTN] =
{
  /* xlen, linmax, table, hlen */
  { 0,   0,NULL,NULL},
  { 2,   0,t1HB, t1l},
  { 3,   0,t2HB, t2l},
  { 3,   0,t3HB, t3l},
  { 0,   0,NULL,NULL},/* Apparently not used */
  { 4,   0,t5HB, t5l},
  { 4,   0,t6HB, t6l},
  { 6,   0,t7HB, t7l},
  { 6,   0,t8HB, t8l},
  { 6,   0,t9HB, t9l},
  { 8,   0,t10HB, t10l},
  { 8,   0,t11HB, t11l},
  { 8,   0,t12HB, t12l},
  {16,   0,t13HB, t13l},
  { 0,   0,NULL,  t16_5l},/* Apparently not used */
  {16,   0,t15HB, t15l},

  { 1,   1,t16HB, t16l},
  { 2,   3,t16HB, t16l},
  { 3,   7,t16HB, t16l},
  { 4,  15,t16HB, t16l},
  { 6,  63,t16HB, t16l},
  { 8, 255,t16HB, t16l},
  {10,1023,t16HB, t16l},
  {13,8191,t16HB, t16l},

  { 4,  15,t24HB, t24l},
  { 5,  31,t24HB, t24l},
  { 6,  63,t24HB, t24l},
  { 7, 127,t24HB, t24l},
  { 8, 255,t24HB, t24l},
  { 9, 511,t24HB, t24l},
  {11,2047,t24HB, t24l},
  {13,8191,t24HB, t24l},

  { 0,   0,t32HB, t32l},
  { 0,   0,t33HB, t33l},
};





/*  for (i = 0; i < 16*16; i++) {
 *      largetbl[i] = ((ht[16].hlen[i]) << 16) + ht[24].hlen[i];
 *  }
 */
const unsigned int largetbl[16*16] = {
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

/*  for (i = 0; i < 3*3; i++) {
 *      table23[i] = ((ht[2].hlen[i]) << 16) + ht[3].hlen[i];
 *  }
 */
const unsigned int table23[3*3] = {
0x010002, 0x040003, 0x070007,
0x040004, 0x050004, 0x070007, 
0x060006, 0x070007, 0x080008
};

/*   for (i = 0; i < 4*4; i++) {
 *       table56[i] = ((ht[5].hlen[i]) << 16) + ht[6].hlen[i];
 *   }
 */
const unsigned int table56[4*4] = {
0x010003, 0x040004, 0x070006, 0x080008, 0x040004, 0x050004, 0x080006, 0x090007,
0x070005, 0x080006, 0x090007, 0x0a0008, 0x080007, 0x080007, 0x090008, 0x0a0009
};



/* 
 * 0: MPEG-2 LSF
 * 1: MPEG-1
 * 2: MPEG-2.5 LSF FhG extention                  (1995-07-11 shn)
 */

typedef enum {
    MPEG_2  = 0,
    MPEG_1  = 1,
    MPEG_25 = 2
} MPEG_t;
 
const int  bitrate_table    [3] [16] = {
    { 0,  8, 16, 24, 32, 40, 48, 56,  64,  80,  96, 112, 128, 144, 160, -1 },
    { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, -1 },
    { 0,  8, 16, 24, 32, 40, 48, 56,  64,  80,  96, 112, 128, 144, 160, -1 },
};

const int  samplerate_table [3]  [4] = { 
    { 22050, 24000, 16000, -1 },      /* MPEG 2 */
    { 44100, 48000, 32000, -1 },      /* MPEG 1 */  
    { 11025, 12000,  8000, -1 },      /* MPEG 2.5 */
};

const char* version_string  [3] = { "2", "1", "2.5" };

const unsigned  header_word [3] = { 0xFFF00000, 0xFFF80000, 0xFFE00000 };

/* This is the scfsi_band table from 2.4.2.7 of the IS */
const int scfsi_band[5] = { 0, 6, 11, 16, 21 };

/* for fast quantization */
FLOAT pow20[Q_MAX+Q_MAX2];
FLOAT ipow20[Q_MAX];
FLOAT iipow20[Q_MAX2];
FLOAT pow43[PRECALC_SIZE];
/* initialized in first call to iteration_init */
#ifdef TAKEHIRO_IEEE754_HACK
FLOAT adj43asm[PRECALC_SIZE];
#else
FLOAT adj43[PRECALC_SIZE];
#endif

/* psymodel window */
FLOAT window[BLKSIZE], window_s[BLKSIZE_s];

/* 
compute the ATH for each scalefactor band 
cd range:  0..96db

Input:  3.3kHz signal  32767 amplitude  (3.3kHz is where ATH is smallest = -5db)
longblocks:  sfb=12   en0/bw=-11db    max_en0 = 1.3db
shortblocks: sfb=5           -9db              0db

Input:  1 1 1 1 1 1 1 -1 -1 -1 -1 -1 -1 -1 (repeated)
longblocks:  amp=1      sfb=12   en0/bw=-103 db      max_en0 = -92db
            amp=32767   sfb=12           -12 db                 -1.4db 

Input:  1 1 1 1 1 1 1 -1 -1 -1 -1 -1 -1 -1 (repeated)
shortblocks: amp=1      sfb=5   en0/bw= -99                    -86 
            amp=32767   sfb=5           -9  db                  4db 


MAX energy of largest wave at 3.3kHz = 1db
AVE energy of largest wave at 3.3kHz = -11db
Let's take AVE:  -11db = maximum signal in sfb=12.  
Dynamic range of CD: 96db.  Therefor energy of smallest audible wave 
in sfb=12  = -11  - 96 = -107db = ATH at 3.3kHz.  

ATH formula for this wave: -5db.  To adjust to LAME scaling, we need
ATH = ATH_formula  - 103  (db)
ATH = ATH * 2.5e-10      (ener)

*/

#ifdef USE_FAST_LOG
/***********************************************************************
 *
 * Fast Log Approximation for log2, used to approximate every other log
 * (log10 and log)
 * maximum absolute error for log10 is around 10-6
 * maximum *relative* error can be high when x is almost 1 because error/log10(x) tends toward x/e
 *
 * use it if typical RESULT values are > 1e-5 (for example if x>1.00001 or x<0.99999)
 * or if the relative precision in the domain around 1 is not important (result in 1 is exact and 0)
 *
 ***********************************************************************/


ieee754_float32_t log_table[LOG2_SIZE*2];



static void init_log_table(void)
{
    int j;
    /* Range for log2(x) over [1,2[ is [0,1[ */
    assert((1<<LOG2_SIZE_L2)==LOG2_SIZE);

    for (j = 0; j < LOG2_SIZE; j++) {
	FLOAT8 a, b, x;
	x = log(1.0 + (j+0.5)/(FLOAT8)LOG2_SIZE) / log(2.0);
	a = log(1.0 +  j     /(FLOAT8)LOG2_SIZE) / log(2.0);
	b = log(1.0 + (j+1.0)/(FLOAT8)LOG2_SIZE) / log(2.0);

	x = (a + x*2 + b) / 4.0;
	a = (b-a) * (1.0 / (1 << (23 - LOG2_SIZE_L2)));
	log_table[j*2+1] = a;
	log_table[j*2  ] = x - a*(1<<(23-LOG2_SIZE_L2-1))*(j*2+1) - 0x7f;
    }
#if 0
    for (j = 0; j < 1024; j++) {
	FLOAT x = j / 1024.0+1.0;
	printf("%f %f\n", log(x)/log(2.0), fast_log2(x));
    }
#endif
}
#endif /* define FAST_LOG */

#define NSATHSCALE 100 // Assuming dynamic range=96dB, this value should be 92

static FLOAT ATHmdct( lame_global_flags *gfp, FLOAT f )
{
    FLOAT ath = ATHformula( f , gfp ) - NSATHSCALE;

    /* modify the MDCT scaling for the ATH and convert to energy */
    return db2pow((ath+gfp->ATHlower*10.0));
}

static void compute_ath( lame_global_flags *gfp )
{
    FLOAT *ATH_l = gfp->internal_flags->ATH.l;
    FLOAT *ATH_s = gfp->internal_flags->ATH.s;
    lame_internal_flags *gfc = gfp->internal_flags;
    int sfb, i, start, end;
    FLOAT ATH_f;
    FLOAT samp_freq = gfp->out_samplerate;

    for (sfb = 0; sfb < SBMAX_l; sfb++) {
        start = gfc->scalefac_band.l[ sfb ];
        end   = gfc->scalefac_band.l[ sfb+1 ];
        ATH_l[sfb]=FLOAT_MAX;
        for (i = start ; i < end; i++) {
            FLOAT freq = i*samp_freq/(2*576);
            ATH_f = ATHmdct( gfp, freq );  /* freq in kHz */
            ATH_l[sfb] = Min( ATH_l[sfb], ATH_f );
        }
    }

    for (sfb = 0; sfb < SBMAX_s; sfb++){
        start = gfc->scalefac_band.s[ sfb ];
        end   = gfc->scalefac_band.s[ sfb+1 ];
        ATH_s[sfb] = FLOAT_MAX;
        for (i = start ; i < end; i++) {
            FLOAT freq = i*samp_freq/(2*192);
            ATH_f = ATHmdct( gfp, freq );    /* freq in kHz */
            ATH_s[sfb] = Min( ATH_s[sfb], ATH_f );
        }
	ATH_s[sfb] *=
	    (gfc->scalefac_band.s[sfb+1] - gfc->scalefac_band.s[sfb]);
    }

    /*  no-ATH mode:
     *  reduce ATH to -200 dB
     */
    
    if (gfp->noATH) {
        for (sfb = 0; sfb < SBMAX_l; sfb++) {
            ATH_l[sfb] = 1E-37;
        }
        for (sfb = 0; sfb < SBMAX_s; sfb++) {
            ATH_s[sfb] = 1E-37;
        }
    }
    
    /*  work in progress, don't rely on it too much
     */
    gfc->ATH.floor = 10. * log10( ATHmdct( gfp, -1. ) );
    
    /*
    {   FLOAT g=10000, t=1e30, x;
        for ( f = 100; f < 10000; f++ ) {
            x = ATHmdct( gfp, f );
            if ( t > x ) t = x, g = f;
        }
        printf("min=%g\n", g);
    }*/
}


/************************************************************************/
/*  initialization for iteration_loop */
/************************************************************************/
static const struct
{
    const int region0_count;
    const int region1_count;
} subdv_table[ 23 ] =
{
{0, 0}, /* 0 bands */
{0, 0}, /* 1 bands */
{0, 0}, /* 2 bands */
{0, 0}, /* 3 bands */
{0, 0}, /* 4 bands */
{0, 1}, /* 5 bands */
{1, 1}, /* 6 bands */
{1, 1}, /* 7 bands */
{1, 2}, /* 8 bands */
{2, 2}, /* 9 bands */
{2, 3}, /* 10 bands */
{2, 3}, /* 11 bands */
{3, 4}, /* 12 bands */
{3, 4}, /* 13 bands */
{3, 4}, /* 14 bands */
{4, 5}, /* 15 bands */
{4, 5}, /* 16 bands */
{4, 6}, /* 17 bands */
{5, 6}, /* 18 bands */
{5, 6}, /* 19 bands */
{5, 7}, /* 20 bands */
{6, 7}, /* 21 bands */
{6, 7}, /* 22 bands */
};



static void
huffman_init(lame_internal_flags * const gfc)
{
    int i;
    extern int choose_table_nonMMX(const int *ix, const int *end, int *s);

    gfc->choose_table = choose_table_nonMMX;

#ifdef MMX_choose_table
    if (gfc->CPU_features.MMX) {
        extern int choose_table_MMX(const int *ix, const int *end, int *s);
        gfc->choose_table = choose_table_MMX;
    }
#endif

    for (i = 2; i <= 576; i += 2) {
	int scfb_anz = 0, index;
	while (gfc->scalefac_band.l[++scfb_anz] < i)
	    ;

	index = subdv_table[scfb_anz].region0_count;
	while (gfc->scalefac_band.l[index + 1] > i)
	    index--;

	if (index < 0) {
	  /* this is an indication that everything is going to
	     be encoded as region0:  bigvalues < region0 < region1
	     so lets set region0, region1 to some value larger
	     than bigvalues */
	  index = subdv_table[scfb_anz].region0_count;
	}

	gfc->bv_scf[i-2] = index;

	index = subdv_table[scfb_anz].region1_count;
	while (gfc->scalefac_band.l[index + gfc->bv_scf[i-2] + 2] > i)
	    index--;

	if (index < 0) {
	    index = subdv_table[scfb_anz].region1_count;
	}

	gfc->bv_scf[i-1] = index;
    }
}

void
iteration_init( lame_global_flags *gfp)
{
    lame_internal_flags *gfc=gfp->internal_flags;
    III_side_info_t * const l3_side = &gfc->l3_side;
    int i;
    FLOAT bass, alto, treble, sfb21;

    if (gfc->iteration_init_init)
	return;

    gfc->iteration_init_init=1;

    l3_side->main_data_begin = 0;
    compute_ath(gfp);
#ifdef USE_FAST_LOG
    init_log_table();
#endif

    pow43[0] = 0.0;
    for(i=1;i<PRECALC_SIZE;i++)
        pow43[i] = pow((FLOAT)i, 4.0/3.0);

#ifdef TAKEHIRO_IEEE754_HACK
    adj43asm[0] = 0.0;
    for (i = 1; i < PRECALC_SIZE; i++)
      adj43asm[i] = i - 0.5 - pow(0.5 * (pow43[i - 1] + pow43[i]),0.75);
#else
    for (i = 0; i < PRECALC_SIZE-1; i++)
	adj43[i] = (i + 1) - pow(0.5 * (pow43[i] + pow43[i + 1]), 0.75);
    adj43[i] = 0.5;
#endif
    for (i = 0; i < Q_MAX; i++)
	ipow20[i] = pow(2.0, (double)(i - 210) * -0.1875);
    for (i = 0; i < Q_MAX+Q_MAX2; i++)
	pow20[i] = pow(2.0, (double)(i - 210 - Q_MAX2) * 0.25);
    for (i = 0; i < Q_MAX2; i++)
        iipow20[i] = pow(2.0, (double)i * 0.1875);

    huffman_init(gfc);

    i = (gfp->exp_nspsytune >> 2) & 63;
    if (i >= 32)
	i -= 64;
    bass = db2pow(i*0.25);

    i = (gfp->exp_nspsytune >> 8) & 63;
    if (i >= 32)
	i -= 64;
    alto = db2pow(i*0.25);

    i = (gfp->exp_nspsytune >> 14) & 63;
    if (i >= 32)
	i -= 64;
    treble = db2pow(i*0.25);

    /*  to be compatible with Naoki's original code, the next 6 bits
     *  define only the amount of changing treble for sfb21 */
    i = (gfp->exp_nspsytune >> 20) & 63;
    if (i >= 32)
	i -= 64;
    sfb21 = treble * db2pow(i*0.25);

    for (i = 0; i < SBMAX_l; i++) {
	FLOAT f;
	if      (i <=  6) f = bass;
	else if (i <= 13) f = alto;
	else if (i <= 20) f = treble;
	else              f = sfb21;
	if ((gfp->VBR == vbr_off || gfp->VBR == vbr_abr) && gfp->quality <= 1)
	    f *= 0.001;

	gfc->nsPsy.longfact[i] = f;
    }
    for (i = 0; i < SBMAX_s; i++) {
	FLOAT f;
	if      (i <=  5) f = bass;
	else if (i <= 10) f = alto;
	else              f = treble;
	if ((gfp->VBR == vbr_off || gfp->VBR == vbr_abr) && gfp->quality <= 1)
	    f *= 0.001;

	gfc->nsPsy.shortfact[i] = f;
    }
}





/* 
 *   The spreading function.  Values returned in units of energy
 */
static FLOAT s3_func(FLOAT bark) {
    FLOAT tempx,x,tempy,temp;
    tempx = bark;
    if (tempx>=0) tempx *= 3;
    else tempx *=1.5; 

    if (tempx>=0.5 && tempx<=2.5)
      {
	temp = tempx - 0.5;
	x = 8.0 * (temp*temp - 2.0 * temp);
      }
    else x = 0.0;
    tempx += 0.474;
    tempy = 15.811389 + 7.5*tempx - 17.5*sqrt(1.0+tempx*tempx);

    if (tempy <= -60.0) return  0.0;

    tempx = exp( (x + tempy)*LN_TO_LOG10 ); 

    /* Normalization.  The spreading function should be normalized so that:
         +inf
           /
           |  s3 [ bark ]  d(bark)   =  1
           /
         -inf
    */
    tempx /= .6609193;
    return tempx;
}

static int
init_numline(
    int *numlines, int *bo, int *bm,
    FLOAT *bval, FLOAT *bval_width, FLOAT *mld,

    FLOAT sfreq, int blksize, int *scalepos,
    FLOAT deltafreq, int sbmax
    )
{
    int partition[HBLKSIZE];
    int i, j, k;
    int sfb;

    sfreq /= blksize;
    j = 0;
    /* compute numlines, the number of spectral lines in each partition band */
    /* each partition band should be about DELBARK wide. */
    for (i=0;i<CBANDS;i++) {
	FLOAT bark1;
	int j2;
	bark1 = freq2bark(sfreq*j);
	for (j2 = j; freq2bark(sfreq*j2) - bark1 < DELBARK && j2 <= blksize/2;
	     j2++)
	    ;

	numlines[i] = j2 - j;
	while (j<j2)
	    partition[j++]=i;
	if (j > blksize/2) break;
    }

    for ( sfb = 0; sfb < sbmax; sfb++ ) {
	int i1,i2,start,end;
	FLOAT arg;
	start = scalepos[sfb];
	end   = scalepos[sfb+1];

	i1 = floor(.5 + deltafreq*(start-.5));
	if (i1<0) i1=0;
	i2 = floor(.5 + deltafreq*(end-.5));
	if (i2>blksize/2) i2=blksize/2;

	bm[sfb] = (partition[i1]+partition[i2])/2;
	bo[sfb] = partition[i2];

	/* setup stereo demasking thresholds */
	/* formula reverse enginerred from plot in paper */
	arg = freq2bark(sfreq*scalepos[sfb]*deltafreq);
	arg = (Min(arg, 15.5)/15.5);

	mld[sfb] = db2pow(-12.5*(1+cos(PI*arg)));
    }

    /* compute bark values of each critical band */
    j = 0;
    for (k = 0; k < i+1; k++) {
	int w = numlines[k];
	FLOAT  bark1,bark2;

	bark1 = freq2bark (sfreq*(j    ));
	bark2 = freq2bark (sfreq*(j+w-1));
	bval[k] = .5*(bark1+bark2);

	bark1 = freq2bark (sfreq*(j  -.5));
	bark2 = freq2bark (sfreq*(j+w-.5));
	bval_width[k] = bark2-bark1;
	j += w;
    }

    return i+1;
}

static int
init_s3_values(
    lame_internal_flags *gfc,
    FLOAT **p,
    int (*s3ind)[2],
    int npart,
    FLOAT *bval,
    FLOAT *bval_width,
    FLOAT *norm
    )
{
    FLOAT s3[CBANDS][CBANDS];
    int i, j, k;
    int numberOfNoneZero = 0;

    /* s[j][i], the value of the spreading function,
     * centered at band j, for band i
     *
     * i.e.: sum over j to spread into signal barkval=i
     * NOTE: i and j are used opposite as in the ISO docs
     */
    for (i = 0; i < npart; i++)
	for (j = 0; j < npart; j++)
	    s3[i][j] = s3_func(bval[i] - bval[j]) * bval_width[j] * norm[i];

    for (i = 0; i < npart; i++) {
	for (j = 0; j < npart; j++) {
	    if (s3[i][j] != 0.0)
		break;
	}
	s3ind[i][0] = j;

	for (j = npart - 1; j > 0; j--) {
	    if (s3[i][j] != 0.0)
		break;
	}
	s3ind[i][1] = j;
	numberOfNoneZero += (s3ind[i][1] - s3ind[i][0] + 1);
    }
    *p = malloc(sizeof(FLOAT)*numberOfNoneZero);
    if (!*p)
	return -1;

    k = 0;
    for (i = 0; i < npart; i++)
	for (j = s3ind[i][0]; j <= s3ind[i][1]; j++)
	    (*p)[k++] = s3[i][j];

    return 0;
}

int psymodel_init(lame_global_flags *gfp)
{
    lame_internal_flags *gfc=gfp->internal_flags;
    int i,j,b,sb,k;
    int bm[SBMAX_l];

    FLOAT bval[CBANDS];
    FLOAT bval_width[CBANDS];
    FLOAT norm[CBANDS];
    FLOAT sfreq = gfp->out_samplerate;
    int numlines_s[CBANDS];

    for (i=0; i<4; ++i) {
	for (j=0; j<CBANDS; ++j) {
	    gfc->nb_1[i][j]=1e20;
	    gfc->nb_2[i][j]=1e20;
	}
	for ( sb = 0; sb < SBMAX_l; sb++ ) {
	    gfc->masking_next[0][i].en.l[sb] = 1e20;
	    gfc->masking_next[1][i].en.l[sb] = 1e20;
	    gfc->masking_next[0][i].thm.l[sb] = 1e20;
	    gfc->masking_next[1][i].thm.l[sb] = 1e20;
	}
	for (j=0; j<3; ++j) {
	    for ( sb = 0; sb < SBMAX_s; sb++ ) {
		gfc->masking_next[0][i].en.s[sb][j] = 1e20;
		gfc->masking_next[1][i].en.s[sb][j] = 1e20;
		gfc->masking_next[0][i].thm.s[sb][j] = 1e20;
		gfc->masking_next[1][i].thm.s[sb][j] = 1e20;
	    }
	    gfc->nsPsy.last_attacks[i] = 0;
	}
	for (j=0;j<9;j++)
	    gfc->nsPsy.last_en_subshort[i][j] = 1.0;
	gfc->useshort_next[0][i] = NORM_TYPE;
	gfc->useshort_next[1][i] = NORM_TYPE;
    }

    /* init. for loudness approx. -jd 2001 mar 27*/
    gfc->loudness_next[0][0] = gfc->loudness_next[0][1]
	= gfc->loudness_next[1][0] = gfc->loudness_next[1][1]
	= 0.0;

    /*************************************************************************
     * now compute the psychoacoustic model specific constants
     ************************************************************************/
    /* compute numlines, bo, bm, bval, bval_width, mld */
    gfc->npart_l
	= init_numline(gfc->numlines_l, gfc->bo_l, bm,
		       bval, bval_width, gfc->mld_l,
		       sfreq, BLKSIZE, 
		       gfc->scalefac_band.l, BLKSIZE/(2.0*576), SBMAX_l);
    assert(gfc->npart_l <= CBANDS);
    /* compute the spreading function */
    for (i=0;i<gfc->npart_l;i++) {
	int l;
	l = gfc->numlines_l[i];
	if (i != 0)
	    l += gfc->numlines_l[i-1];
	if (i != gfc->npart_l-1)
	    l += gfc->numlines_l[i+1];

	gfc->rnumlines_ls[i] = 20.0/(l-1);
	norm[i] = 0.11749;
	gfc->rnumlines_l[i] = 1.0 / (gfc->numlines_l[i] * 3);
    }
    i = init_s3_values(gfc, &gfc->s3_ll, gfc->s3ind,
		       gfc->npart_l, bval, bval_width, norm);
    if (i)
	return i;

    /* compute long block specific values, ATH */
    j = 0;
    for ( i = 0; i < gfc->npart_l; i++ ) {
	/* ATH */
	FLOAT x = FLOAT_MAX;
	for (k=0; k < gfc->numlines_l[i]; k++, j++) {
	    FLOAT  freq = sfreq*j/(1000.0*BLKSIZE);
	    FLOAT  level;
	    assert( freq <= 24 );              // or only '<'
	    //	freq = Min(.1,freq);       // ATH below 100 Hz constant, not further climbing
	    level  = ATHformula (freq*1000, gfp) - 20;   // scale to FFT units; returned value is in dB
	    level  = pow ( 10., 0.1*level );   // convert from dB -> energy
	    level *= gfc->numlines_l [i];
	    if (x > level)
		x = level;
	}
	gfc->ATH.cb[i] = x;
    }
    for (i = 0; i < SBMAX_l; i++)
	gfc->ATH.l_avg[i] = gfc->ATH.cb[bm[i]] * db2pow(gfp->ATHlower*10.0);

    /************************************************************************
     * do the same things for short blocks
     ************************************************************************/
    gfc->npart_s
	= init_numline(numlines_s, gfc->bo_s, bm,
		       bval, bval_width, gfc->mld_s,
		       sfreq, BLKSIZE_s,
		       gfc->scalefac_band.s, BLKSIZE_s/(2.0*192), SBMAX_s);
    assert(gfc->npart_s <= CBANDS);

    /* SNR formula. short block is normalized by SNR. is it still right ? */
    for (i=0;i<gfc->npart_s;i++) {
	double snr=-8.25;
	if (bval[i]>=13)
	    snr = -4.5 * (bval[i]-13)/(24.0-13.0)
		-8.25*(bval[i]-24)/(13.0-24.0);

	norm[i] = db2pow(snr) * NS_PREECHO_ATT0;
	gfc->endlines_s[i] = numlines_s[i];
	if (i != 0)
	    gfc->endlines_s[i] += gfc->endlines_s[i-1];
    }
    for (i = 0; i < SBMAX_s; i++)
	gfc->ATH.s_avg[i] = gfc->ATH.cb[bm[i]] * db2pow(gfp->ATHlower*10.0);
    
    i = init_s3_values(gfc, &gfc->s3_ss, gfc->s3ind_s,
		       gfc->npart_s, bval, bval_width, norm);
    if (i)
	return i;


    if (gfp->ATHtype != -1) { 
	/* compute equal loudness weights */
	FLOAT eql_balance = 0.0;
	for( i = 0; i < BLKSIZE/2; ++i ) {
	    FLOAT freq = gfp->out_samplerate * i / BLKSIZE;
	    gfc->ATH.eql_w[i] = db2pow(-ATHformula( freq, gfp ));
	    eql_balance += gfc->ATH.eql_w[i];
	}
	eql_balance =  (vo_scale * vo_scale / (BLKSIZE/2)) / eql_balance;
	for( i = BLKSIZE/2; --i >= 0; ) { /* scale weights */
	    gfc->ATH.eql_w[i] *= eql_balance;
	}
    }


    init_mask_add_max_values();

    /* setup temporal masking */
#define temporalmask_sustain_sec 0.01
    gfc->decay = exp(-1.0*LOG10/(temporalmask_sustain_sec*sfreq/192.0));

    {
	FLOAT msfix;

#define NS_MSFIX 3.5
#define NSATTACKTHRE 3.5
#define NSATTACKTHRE_S 30

	msfix = NS_MSFIX;
	if (gfp->exp_nspsytune & 2) msfix = 1.0;
	if (gfp->msfix != 0.0) msfix = gfp->msfix;
	gfp->msfix = msfix;
	if (!gfc->presetTune.use || gfc->nsPsy.athadjust_msfix <= 0.0)
	    gfc->nsPsy.athadjust_msfix = gfp->msfix;

	if (gfc->presetTune.ms_maskadjust <= 0.0)
	    gfc->presetTune.ms_maskadjust = gfp->msfix;

	gfp->msfix *= 2.0;
	gfc->nsPsy.athadjust_msfix *= 2.0;
	gfc->presetTune.ms_maskadjust *= 2.0;

	/* spread only from npart_l bands.  Normally, we use the spreading
	 * function to convolve from npart_l down to npart_l bands 
	 */
	for (b=0;b<gfc->npart_l;b++)
	    if (gfc->s3ind[b][1] > gfc->npart_l-1)
		gfc->s3ind[b][1] = gfc->npart_l-1;
    }

    if (gfc->presetTune.quantcomp_alt_type < 0)
	gfc->presetTune.quantcomp_alt_type = gfp->experimentalX;

    if (!gfc->presetTune.use) {
	gfc->presetTune.quantcomp_type_s
	    = gfc->presetTune.quantcomp_alt_type = gfp->experimentalX;
	gfc->presetTune.athadjust_switch_level = 0.0;
	gfc->presetTune.ms_maskadjust = gfp->msfix;

	gfc->nsPsy.attackthre   = NSATTACKTHRE;
	gfc->nsPsy.attackthre_s = NSATTACKTHRE_S;
    }

    /*  prepare for ATH auto adjustment:
     *  we want to decrease the ATH by 12 dB per second
     */
    {
        FLOAT frame_duration = 576. * gfc->mode_gr / sfreq;
        gfc->ATH.decay = db2pow(-12.0 * frame_duration);
        gfc->ATH.adjust = 0.01; /* minimum, for leading low loudness */
        gfc->ATH.adjust_limit = 1.0; /* on lead, allow adjust up to maximum */
    }

    gfc->bo_s[SBMAX_s-1]--;
    assert(gfc->bo_l[SBMAX_l-1] <= gfc->npart_l);
    assert(gfc->bo_s[SBMAX_s-1] <= gfc->npart_s);

    // The type of window used here will make no real difference, but
    // in the interest of merging nspsytune stuff - switch to blackman window
    for (i = 0; i < BLKSIZE ; i++)
      /* blackman window */
      window[i] =
	  0.42-0.5*cos(2*PI*(i+.5)/BLKSIZE) + 0.08*cos(4*PI*(i+.5)/BLKSIZE);

    for (i = 0; i < BLKSIZE_s; i++)
	window_s[i] = 0.5 * (1.0 - cos(2.0 * PI * (i + 0.5) / BLKSIZE_s));

    {
        extern void fht(FLOAT *fz, int n);
        gfc->fft_fht = fht;
    }
#ifdef HAVE_NASM
    if (gfc->CPU_features.AMD_3DNow) {
        extern void fht_3DN(FLOAT *fz, int n);
        gfc->fft_fht = fht_3DN;
    } else 
#endif
#ifdef USE_FFTSSE
    if (gfc->CPU_features.SIMD) {
        extern void fht_SSE(FLOAT *fz, int n);
        gfc->fft_fht = fht_SSE;
    } else 
#endif
#ifdef USE_FFTFPU
    if (gfc->CPU_features.i387) {
        extern void fht_FPU(FLOAT *fz, int n);
        gfc->fft_fht = fht_FPU;
    }
#endif

    return 0;
}


/* end of tables.c */
