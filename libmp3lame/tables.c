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
#include "psymodel.h"
#include "quantize_pvt.h"

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

#include <assert.h>

/*
  The following table is used to implement the scalefactor
  partitioning for MPEG2 as described in section
  2.4.3.2 of the IS. The indexing corresponds to the
  way the tables are presented in the IS:

  [table_number*3+row_in_table][column of nr_of_sfb]
*/
const int  nr_of_sfb_block [6*3][4] =
{
    {6, 5, 5, 5}, /* long/start/stop */
    {9, 9, 9, 9}, /* short */
    {6, 9, 9, 9}  /* mixed */
    ,
    {6, 5,  7, 3}, /* long/start/stop */
    {9, 9, 12, 6}, /* short */
    {6, 9, 12, 6}  /* mixed */
    ,
    {11, 10, 0, 0}, /* long/start/stop, preflag */
    {18, 18, 0, 0}, /* short, preflag ? */
    {15, 18, 0, 0}  /* mixed, preflag ? */
    ,
    { 7,  7,  7, 0}, /* long/start/stop, IS */
    {12, 12, 12, 0}, /* short, IS */
    { 6, 15, 12, 0}  /* mixed, IS */
    ,
    { 6,  6, 6, 3}, /* long/start/stop, IS */
    {12,  9, 9, 6}, /* short, IS */
    { 6, 12, 9, 6}  /* mixed, IS */
    ,
    { 8, 8, 5, 0}, /* long/start/stop, IS  */
    {15,12, 9, 0}, /* short, IS */
    { 6,18, 9, 0}  /* mixed, IS */
};


/* MPEG1 scalefactor bits */
const int s1bits[] = { 0, 0, 0, 0, 3, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4 };
const int s2bits[] = { 0, 1, 2, 3, 0, 1, 2, 3, 1, 2, 3, 1, 2, 3, 2, 3 };

/* Table B.6: layer3 preemphasis */
const int  pretab[SBMAX_l] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    1, 1, 1, 1, 2, 2, 3, 3, 3, 2, 0
};

/*
  Here are MPEG1 Table B.8 and MPEG2 Table B.1
  -- Layer III scalefactor bands. 
  Index into this using a method such as:
    idx  = fr_ps->header->sampling_frequency + (fr_ps->header->version * 3)
*/

const scalefac_struct sfBandIndex[9] = {
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

const unsigned char quadcode[2][16*2]  = {
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

const struct huffcodetab ht[] = {
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

  { 1,   1+15,t16HB, t16l},
  { 2,   3+15,t16HB, t16l},
  { 3,   7+15,t16HB, t16l},
  { 4,  15+15,t16HB, t16l},
  { 6,  63+15,t16HB, t16l},
  { 8, 255+15,t16HB, t16l},
  {10,1023+15,t16HB, t16l},
  {13,8191+15,t16HB, t16l},

  { 4,  15+15,t24HB, t24l},
  { 5,  31+15,t24HB, t24l},
  { 6,  63+15,t24HB, t24l},
  { 7, 127+15,t24HB, t24l},
  { 8, 255+15,t24HB, t24l},
  { 9, 511+15,t24HB, t24l},
  {11,2047+15,t24HB, t24l},
  {13,8191+15,t24HB, t24l}
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


/* for subband filtering */
const int mdctorder[] = {
    0,30,15,17, 8,22, 7,25, 4,26,11,21,12,18, 3,29,
    2,28,13,19,10,20, 5,27, 6,24, 9,23,14,16, 1,31
};

const char* version_string  [3] = { "2", "1", "2.5" };

const int  samplerate_table [3]  [4] = { 
    { 22050, 24000, 16000, -1 },      /* MPEG 2 */
    { 44100, 48000, 32000, -1 },      /* MPEG 1 */  
    { 11025, 12000,  8000, -1 },      /* MPEG 2.5 */
};

const int  bitrate_table    [3] [16] = {
    { 0,  8, 16, 24, 32, 40, 48, 56,  64,  80,  96, 112, 128, 144, 160, -1 },
    { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, -1 },
    { 0,  8, 16, 24, 32, 40, 48, 56,  64,  80,  96, 112, 128, 144, 160, -1 },
};

/* This is the scfsi_band table from 2.4.2.7 of the IS */
const int scfsi_band[5] = { 0, 6, 11, 16, 21 };

/* for fast quantization */
FLOAT pow20[Q_MAX+Q_MAX2];
FLOAT ipow20[Q_MAX+Q_MAX2];

/* initialized in first call to iteration_init */
#ifdef TAKEHIRO_IEEE754_HACK
FLOAT pow43[PRECALC_SIZE*3];
#else
FLOAT pow43[PRECALC_SIZE*2];
#endif

/* psymodel window */
FLOAT window[BLKSIZE], window_s[BLKSIZE_s];

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
	double a, b, x;
	x = log(1.0 + (j+0.5)/(double)LOG2_SIZE) / log(2.0);
	a = log(1.0 +  j     /(double)LOG2_SIZE) / log(2.0);
	b = log(1.0 + (j+1.0)/(double)LOG2_SIZE) / log(2.0);

	x = (a + x*2 + b) / 4.0;
	a = (b-a) * (1.0 / (1 << (23 - LOG2_SIZE_L2)));
	log_table[j*2+1] = a;
	log_table[j*2  ] = x - a*(1<<(23-LOG2_SIZE_L2-1))*(j*2+1) - 0x7f;
    }
}
#endif /* define FAST_LOG */


static FLOAT ATHformula(FLOAT f,lame_global_flags *gfp)
{
    /* from Painter & Spanias
       modified by Gabriel Bouvigne to better fit the reality

       ath =    3.640 * pow(f,-0.8)
         - 6.800 * exp(-0.6*pow(f-3.4,2.0))
         + 6.000 * exp(-0.15*pow(f-8.7,2.0))
         + 0.6* 0.001 * pow(f,4.0);


	In the past LAME was using the Painter &Spanias formula.
	But we had some recurrent problems with HF content.
	We measured real ATH values, and found the older formula
	to be inacurate in the higher part. So we made this new
	formula and this solved most of HF problematic testcases.
	The tradeoff is that in VBR mode it increases a lot the
	bitrate.*/

    f /= 1000;  /* convert to khz */
    f  = Max(0.01, f);
    f  = Min(18.0, f);

    return 3.640 * pow(f,-0.8)
	- 6.800 * exp(-0.6*pow(f-3.4,2.0))
	+ 6.000 * exp(-0.15*pow(f-8.7,2.0))
	+ (0.6+0.04*gfp->ATHcurve)* 0.001 * pow(f,4.0)
	- gfp->ATHlower;
}

static FLOAT ATHmdct( lame_global_flags *gfp, FLOAT f )
{
    /* modify the MDCT scaling for the ATH and convert to energy */
    return db2pow(ATHformula( f , gfp ) - NSATHSCALE);
}

static void compute_ath( lame_global_flags *gfp )
{
    lame_internal_flags *gfc = gfp->internal_flags;
    FLOAT ATH_f, samp_freq = gfp->out_samplerate;
    int sfb, i, start, end;

    /*  no-ATH mode: reduce ATH to -370 dB */
    if (gfp->noATH) {
        for (sfb = 0; sfb < SBMAX_l; sfb++) {
            gfc->ATH.l[sfb] = 1E-37;
        }
        for (sfb = 0; sfb < SBMAX_s; sfb++) {
            gfc->ATH.s[sfb] = 1E-37;
        }
	return;
    }

    for (sfb = 0; sfb < SBMAX_l; sfb++) {
	start = gfc->scalefac_band.l[ sfb ];
	end   = gfc->scalefac_band.l[ sfb+1 ];
	gfc->ATH.l[sfb] = FLOAT_MAX;
	for (i = start ; i < end; i++) {
	    FLOAT freq = i*samp_freq/(2.0*576);
	    ATH_f = ATHmdct( gfp, freq );  /* freq in kHz */
	    if (gfc->ATH.l[sfb] > ATH_f)
		gfc->ATH.l[sfb] = ATH_f;
	}
    }

    for (sfb = 0; sfb < SBMAX_s; sfb++){
	start = gfc->scalefac_band.s[ sfb ];
	end   = gfc->scalefac_band.s[ sfb+1 ];
	gfc->ATH.s[sfb] = FLOAT_MAX;
	for (i = start ; i < end; i++) {
	    FLOAT freq = i*samp_freq/(2*192);
	    ATH_f = ATHmdct( gfp, freq );    /* freq in kHz */
	    if (gfc->ATH.s[sfb] > ATH_f)
		gfc->ATH.s[sfb] = ATH_f;
	}
    }
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

#if HAVE_NASM
    gfc->choose_table = choose_table_nonMMX;
    if (gfc->CPU_features.MMX) {
        extern int choose_table_MMX(const int *ix, const int *end, int *s);
        gfc->choose_table = choose_table_MMX;
    }
#endif

    gfc->scale_bitcounter = scale_bitcount;
    if (gfc->mode_gr < 2)
	gfc->scale_bitcounter = scale_bitcount_lsf;

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

static FLOAT
filter_coef(FLOAT x)
{
    if (x > 1.0) return 0.0;
    if (x <= 0.0) return 1.0;

    return cos(PI/2 * x);
}

static void
lame_init_params_ppflt(lame_global_flags * gfp)
{
    lame_internal_flags *gfc = gfp->internal_flags;
    /***************************************************************/
    /* compute info needed for polyphase filter (filter type==0, default) */
    /***************************************************************/

    int band, maxband, minband;
    int lowpass_band = 32;
    int highpass_band = -1;
    FLOAT   freq;

    if (gfc->lowpass1 > 0) {
	minband = 999;
	for (band = 0; band <= 31; band++) {
	    freq = band / 31.0;
	    /* this band and above will be zeroed: */
	    if (freq >= gfc->lowpass2) {
		lowpass_band = Min(lowpass_band, band);
	    }
	    if (gfc->lowpass1 < freq && freq < gfc->lowpass2) {
		minband = Min(minband, band);
	    }
	}

        /* compute the *actual* transition band implemented by
         * the polyphase filter */
        if (minband == 999) {
            gfc->lowpass1 = (lowpass_band - .75) / 31.0;
        }
        else {
            gfc->lowpass1 = (minband - .75) / 31.0;
        }
        gfc->lowpass2 = lowpass_band / 31.0;
    }

    /* make sure highpass filter is within 90% of what the effective
     * highpass frequency will be */
    if (gfc->highpass2 > 0 && gfc->highpass2 < .9 * (.75 / 31.0)) {
	gfc->highpass1 = gfc->highpass2 = 0;
	MSGF(gfc,
	     "Warning: highpass filter disabled (the frequency too small)\n");
    }

    if (gfc->highpass2 > 0) {
        maxband = -1;
        for (band = 0; band <= 31; band++) {
            freq = band / 31.0;
            /* this band and below will be zereod */
            if (freq <= gfc->highpass1) {
                highpass_band = Max(highpass_band, band);
            }
            if (gfc->highpass1 < freq && freq < gfc->highpass2) {
                maxband = Max(maxband, band);
            }
        }
        /* compute the *actual* transition band implemented by
         * the polyphase filter */
        gfc->highpass1 = highpass_band / 31.0;
        if (maxband == -1) {
            gfc->highpass2 = (highpass_band + .75) / 31.0;
        }
        else {
            gfc->highpass2 = (maxband + .75) / 31.0;
        }
    }

    for (band = 0; band < 32; band++) {
	freq = band / 31.0;
	gfc->amp_filter[band]
	    = filter_coef((gfc->highpass2 - freq)
			  / (gfc->highpass2 - gfc->highpass1 + 1e-37))
	    * filter_coef((freq - gfc->lowpass1)
			  / (gfc->lowpass2 - gfc->lowpass1 - 1e-37));
    }
    while (--band >= 0) {
	if (gfc->amp_filter[band] > 1e-20)
	    break;
    }
    gfc->xrNumMax_longblock = (band+1) * 18;
}


void
iteration_init( lame_global_flags *gfp)
{
    lame_internal_flags *gfc=gfp->internal_flags;
    III_side_info_t * const l3_side = &gfc->l3_side;
    int i, j;

    if (gfc->iteration_init_init)
	return;
    gfc->iteration_init_init=1;

    /**********************************************************************/
    /* compute info needed for polyphase filter (filter type==0, default) */
    /**********************************************************************/
    lame_init_params_ppflt(gfp);

    /*******************************************************/
    /* compute info needed for FIR filter (filter_type==1) */
    /*******************************************************/
    /* not yet coded */

    /* scalefactor band start/end position */
    j = gfc->samplerate_index
	+ (3 * gfp->version) + 6 * (gfp->out_samplerate < 16000);
    for (i = 0; i < SBMAX_l + 1; i++)
        gfc->scalefac_band.l[i] = sfBandIndex[j].l[i];
    for (i = 0; i < SBMAX_s + 1; i++)
        gfc->scalefac_band.s[i] = sfBandIndex[j].s[i];

    /* cutoff and intensity stereo */
    for (i = 0; i < SBMAX_l; i++) {
	if (gfc->scalefac_band.l[i] > gfc->lowpass2*576)
	    break;
    }
    gfc->cutoff_sfb_l = i;
    for (i = 0; i < SBMAX_s; i++) {
	if (gfc->scalefac_band.s[i] > gfc->lowpass2*192)
	    break;
    }
    gfc->cutoff_sfb_s = i;

    gfp->use_istereo = 0;
    gfc->is_start_sfb_l_next[0] = gfc->is_start_sfb_l_next[1]
	= gfc->is_start_sfb_l[0] = gfc->is_start_sfb_l[1]
	= gfc->cutoff_sfb_l;
    gfc->is_start_sfb_s_next[0] = gfc->is_start_sfb_s_next[1]
	= gfc->is_start_sfb_s[0] = gfc->is_start_sfb_s[1]
	= gfc->cutoff_sfb_s;
    if (gfp->mode != MONO && gfp->compression_ratio > 12.0
	&& gfc->mode_gr == 2 /* currently only MPEG1 */)
	gfp->use_istereo = 1;

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
	adj43asm[i] = i - 0.5 - pow(0.5 * (pow(i-1.0,4.0/3.0) + pow(i,4.0/3.0)), 0.75);
#endif
    for (i = 0; i < PRECALC_SIZE-1; i++)
	adj43[i] = (i + 1) - pow(0.5 * (pow43[i] + pow43[i + 1]), 0.75);
    adj43[i] = 0.5;

    for (i = 0; i < Q_MAX+Q_MAX2; i++) {
	ipow20[i] = pow(2.0, (double)(i - 210 - Q_MAX2) * -0.1875);
	pow20[i] = pow(2.0, (double)(i - 210 - Q_MAX2) * 0.25);
    }

    huffman_init(gfc);
}





/* Mapping from frequency to barks */
/* see for example "Zwicker: Psychoakustik, 1982; ISBN 3-540-11401-7 */
static FLOAT freq2bark(FLOAT freq)
{
    /* input: freq in hz  output: barks */
    if (freq<0) freq=0;
    freq = freq * 0.001;
    return 13.0*atan(.76*freq) + 3.5*atan(freq*freq/(7.5*7.5));
}

/* 
 *   The spreading function.  Values returned in units of energy
 */
static FLOAT
s3_func(FLOAT bark)
{
    FLOAT x, tempy;
    bark *= 1.5;
    if (bark >= 0.0)
	bark += bark;

    x = 0.0;
    if (0.5 <= bark && bark <= 2.5)
	x = 8.0 * ((bark-1.5)*(bark-1.5) - 1.0);

    bark += 0.474;
    tempy = 15.811389 + 7.5*bark - 17.5*sqrt(1.0 + bark*bark) + x;

    if (tempy <= -60.0) return 0.0;

    return db2pow(tempy);
}

static int
init_numline(
    int *numlines, int *bo, int *bm,
    FLOAT *bval, FLOAT *mld,

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

	bark1 = freq2bark (sfreq*(j  ));
	bark2 = freq2bark (sfreq*(j+w));
	bval[k] = .5*(bark1+bark2);
	j += w;
    }
    return i+1;
}

static void
init_numline_l2s(
    int *bo,

    FLOAT sfreq, int blksize, int *scalepos,
    FLOAT deltafreq, int sbmax
    )
{
    int partition[HBLKSIZE];
    int i, j;
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

	while (j<j2)
	    partition[j++]=i;
	if (j > blksize/2) break;
    }

    for ( sfb = 0; sfb < sbmax; sfb++ ) {
	int i2, end;
	end   = scalepos[sfb+1];

	i2 = floor(.5 + deltafreq*(end-.5));
	if (i2>blksize/2) i2=blksize/2;
	bo[sfb] = partition[i2];
    }
}

static int
init_s3_values(
    lame_internal_flags *gfc,
    FLOAT **p,
    int (*s3ind)[2],
    int npart,
    FLOAT *bval,
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
	    s3[i][j] = s3_func(bval[i] - bval[j]) * norm[i] * 0.3;

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
    for (i = 0; i < npart; i++) {
	FLOAT fact;
	int f = gfc->nsPsy.tune;
	if (bval[i] < 6.0) {
	    f = (f >> 2) & 63;
	    if (f >= 32)
		f -= 64;
	}
	else if (bval[i] < 12.0) {
	    f = (f >> 8) & 63;
	    if (f >= 32)
		f -= 64;
	}
	else if (bval[i] < 18.0) {
	    f = (f >> 14) & 63;
	    if (f >= 32)
		f -= 64;
	}
	else {
	    int g = (f >> 14) & 63;
	    f = (f >> 20) & 63;
	    if (f >= 32)
		f -= 64;
	    if (g >= 32)
		g -= 64;
	    f += g;
	}
	fact = db2pow(f*0.25);
	for (j = s3ind[i][0]; j <= s3ind[i][1]; j++)
	    (*p)[k++] = s3[i][j] * fact;
    }
    return 0;
}

int psymodel_init(lame_global_flags *gfp)
{
    lame_internal_flags *gfc=gfp->internal_flags;
    int i,j,sb,k;
    int bm[SBMAX_l];

    FLOAT bval[CBANDS];
    FLOAT norm[CBANDS];
    FLOAT sfreq = gfp->out_samplerate;
    int numlines_s[CBANDS];
    FLOAT eql_balance;

    for (i=0; i<MAX_CHANNELS*2; ++i) {
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
	}
	for (j=0;j<6;j++)
	    gfc->nsPsy.subbk_ene[i][j] = 1.0;
	gfc->blocktype_next[0][i] = gfc->blocktype_next[1][i] = NORM_TYPE;
    }

    gfc->masking_lower = db2pow(gfp->VBR_q - 8 - 4);

    /*************************************************************************
     * now compute the psychoacoustic model specific constants
     ************************************************************************/
    /* compute numlines, bo, bm, bval, mld */
    gfc->npart_l
	= init_numline(gfc->numlines_l, gfc->bo_l, bm,
		       bval, gfc->mld_l,
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

	norm[i] = 0.11749/5.0;
	if (i < 8)
	    norm[i] = 0.001/5;
	if (i > 50)
	    norm[i] *= 2;
	gfc->rnumlines_ls[i] = 20.0/(l-1);
	gfc->rnumlines_l[i] = 1.0 / (gfc->numlines_l[i] * 3);
	if (gfp->ATHonly)
	    norm[i] = 1e-37;
    }
    i = init_s3_values(gfc, &gfc->s3_ll, gfc->s3ind,
		       gfc->npart_l, bval, norm);
    if (i)
	return i;

    /* compute long block specific values, ATH */
    j = 0;
    for ( i = 0; i < gfc->npart_l; i++ ) {
	/* ATH */
	FLOAT x = FLOAT_MAX;
	for (k=0; k < gfc->numlines_l[i]; k++, j++) {
	    FLOAT level = db2pow(ATHformula(sfreq*j/BLKSIZE, gfp) - 20)
		* gfc->numlines_l[i];
	    if (x > level)
		x = level;
	}
	gfc->ATH.cb[i] = x;
    }
    for (i = 0; i < SBMAX_l; i++)
	gfc->ATH.l_avg[i] = gfc->ATH.cb[bm[i]];

    /* table for long block threshold -> short block threshold conversion */
    init_numline_l2s(gfc->bo_l2s,
		     sfreq, BLKSIZE, 
		     gfc->scalefac_band.s, BLKSIZE/(2.0*192), SBMAX_s);

    /************************************************************************
     * do the same things for short blocks
     ************************************************************************/
    gfc->npart_s
	= init_numline(numlines_s, gfc->bo_s, bm,
		       bval, gfc->mld_s,
		       sfreq, BLKSIZE_s,
		       gfc->scalefac_band.s, BLKSIZE_s/(2.0*192), SBMAX_s);
    assert(gfc->npart_s <= CBANDS);

    /* SNR formula is removed. but we should tune s3_s more */
    for (i=0;i<gfc->npart_s;i++) {
	norm[i] = db2pow(-0.25) * NS_PREECHO_ATT0 * 0.8*0.1;
	if (i < 4)
	    norm[i] *= 0.01;
	else if (i < 8)
	    norm[i] *= 0.1;
	else
	    norm[i] *= db2pow(-1);
	gfc->endlines_s[i] = numlines_s[i];
	if (i != 0)
	    gfc->endlines_s[i] += gfc->endlines_s[i-1];
	if (gfp->ATHonly || gfp->ATHshort)
	    norm[i] = 1e-37;
    }
    for (i = 0; i < SBMAX_s; i++)
	gfc->ATH.s_avg[i] = gfc->ATH.cb[bm[i]] * BLKSIZE_s / BLKSIZE;

    i = init_s3_values(gfc, &gfc->s3_ss, gfc->s3ind_s,
		       gfc->npart_s, bval, norm);
    if (i)
	return i;

    assert(gfc->bo_l[SBMAX_l-1] <= gfc->npart_l);
    assert(gfc->bo_l2s[SBMAX_s-1] <= gfc->npart_l);
    assert(gfc->bo_s[SBMAX_s-1] <= gfc->npart_s);

    init_mask_add_max_values(gfc);

    /* setup temporal masking */
    gfc->decay = db2pow(-(576.0/3)/(TEMPORALMASK_SUSTAIN_SEC*sfreq));

    /* long/short switching, use subbandded sample in f > 2kHz */
    i = (int) (2000.0 / (sfreq / 2.0 / 32.0) + 0.5);
    gfc->nsPsy.switching_band = Min(i, 30);

    /*  prepare for ATH auto adjustment:
     *  we want to decrease the ATH by 12 dB per second
     */
    gfc->ATH.adjust = 0.01; /* minimum, for leading low loudness */
    gfc->ATH.adjust_limit = 1.0; /* on lead, allow adjust up to maximum */

    /* compute equal loudness weights jd - 2001 mar 12
       notes:
         ATHformula is used to approximate an equal loudness curve.
       future:
         Data indicates that the shape of the equal loudness curve varies
	 with intensity.  This function might be improved by using an equal
	 loudness curve shaped for typical playback levels (instead of the
	 ATH, that is shaped for the threshold).  A flexible realization might
	 simply bend the existing ATH curve to achieve the desired shape.
	 However, the potential gain may not be enough to justify an effort.
    */
    gfc->loudness_next[0] = gfc->loudness_next[1] = 0.0;
    eql_balance = 0.0;
    for( i = 0; i < BLKSIZE/2; ++i ) {
	FLOAT freq = gfp->out_samplerate * i / BLKSIZE;
	/* we cannot get precise analysis at lower frequency */
	if (i < 20)
	    freq = gfp->out_samplerate * 0.0 / BLKSIZE;
	gfc->ATH.eql_w[i] = db2pow(-ATHformula( freq, gfp ));
	eql_balance += gfc->ATH.eql_w[i];
    }
    eql_balance =  (vo_scale * vo_scale / (BLKSIZE/2)) / eql_balance * 0.5;
    for( i = BLKSIZE/2; --i >= 0; )
	gfc->ATH.eql_w[i] *= eql_balance;

    /* The type of window used here will make no real difference, but
     * use blackman window for long block. */
    for (i = 0; i < BLKSIZE ; i++)
      window[i] =
	  0.42-0.5*cos(2*PI*(i+.5)/BLKSIZE) + 0.08*cos(4*PI*(i+.5)/BLKSIZE);

    for (i = 0; i < BLKSIZE_s; i++)
	window_s[i] = 0.5 * (1.0 - cos(2.0 * PI * (i + 0.5) / BLKSIZE_s));

#ifdef HAVE_NASM
    {
	extern void fht(FLOAT *fz, int n);
	gfc->fft_fht = fht;
    }
    if (gfc->CPU_features.AMD_E3DNow) {
	extern void fht_E3DN(FLOAT *fz, int n);
	gfc->fft_fht = fht_E3DN;
    } else
    if (gfc->CPU_features.AMD_3DNow) {
	extern void fht_3DN(FLOAT *fz, int n);
	gfc->fft_fht = fht_3DN;
    }
#endif

    return 0;
}


void init_bit_stream_w(lame_global_flags *gfp)
{
    lame_internal_flags *gfc = gfp->internal_flags;
    gfc->bs.h_ptr = gfc->bs.w_ptr = 0;
    gfc->bs.header[gfc->bs.h_ptr].write_timing = 0;
    gfc->bs.bitidx = gfc->bs.totbyte = 0;
    memset(gfc->bs.buf, 0, sizeof(gfc->bs.buf));

    /* determine the mean bitrate for main data */
    if (gfp->version == 1) /* MPEG 1 */
        gfc->l3_side.sideinfo_len = (gfc->channels_out == 1) ? 4 + 17 : 4 + 32;
    else                /* MPEG 2 */
        gfc->l3_side.sideinfo_len = (gfc->channels_out == 1) ? 4 + 9 : 4 + 17;

    if (gfp->error_protection)
        gfc->l3_side.sideinfo_len += 2;

    /* padding method as described in 
     * "MPEG-Layer3 / Bitstream Syntax and Decoding"
     * by Martin Sieler, Ralph Sperschneider
     *
     * note: there is no padding for the very first frame
     *
     * Robert.Hegemann@gmx.de 2000-06-22
     */
    gfc->slot_lag = gfc->frac_SpF = 0;
    if (gfp->VBR == cbr && !gfp->disable_reservoir)
	gfc->slot_lag = gfc->frac_SpF
	    = ((gfp->version+1)*72000L*gfp->mean_bitrate_kbps)
	    % gfp->out_samplerate;

    /* resv. size */
    if (gfp->mean_bitrate_kbps >= 320) {
        /* in freeformat the buffer is constant*/
	gfc->l3_side.maxmp3buf
	    = ((int)((gfp->mean_bitrate_kbps*1000 * 1152/8)
		     /gfp->out_samplerate +.5));
    } else {
        /* maximum allowed frame size.  dont use more than this number of
           bits, even if the frame has the space for them: */
        /* Bouvigne suggests this more lax interpretation of the ISO doc 
           instead of using 8*960. */

	/*all mp3 decoders should have enough buffer to handle this value: size of a 320kbps 32kHz frame*/
	gfc->l3_side.maxmp3buf = 1440;

        if (gfp->strict_ISO)
	    gfc->l3_side.maxmp3buf
		= (320000*1152 / gfp->out_samplerate + 7) >> 3;
	if (gfp->disable_reservoir)
	    gfc->l3_side.maxmp3buf = 0;
    }
    gfc->l3_side.maxmp3buf -= gfc->l3_side.sideinfo_len;
}
