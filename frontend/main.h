/*
 *	Command line frontend program
 *
 *	Copyright (c) 1999 Mark Taylor
 *                    2000 Takehiro TOMIANGA
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

typedef enum sound_file_format_e {
  sf_unknown, 
  sf_raw, 
  sf_wave, 
  sf_aiff, 
  sf_mp1,  /* MPEG Layer 1, aka mpg */
  sf_mp2,  /* MPEG Layer 2 */
  sf_mp3,  /* MPEG Layer 3 */
  sf_ogg 
} sound_file_format;


#define         MAX_NAME_SIZE           1000
char inPath[MAX_NAME_SIZE];
FILE * musicin;             /* file pointer to input file */
sound_file_format input_format;   
int swapbytes;              /* force byte swapping   default=0*/
int totalframes;                /* frames: 0..totalframes-1 (estimate)*/
int frameNum;                   /* frame counter */

float update_interval;      /* to use Frank's time status display */

#ifndef FALSE
#define         FALSE                   0
#endif

#ifndef TRUE
#define         TRUE                    (!FALSE)
#endif


#define         Min(A, B)       ((A) < (B) ? (A) : (B))
#define         Max(A, B)       ((A) > (B) ? (A) : (B))

#define         MAX_U_32_NUM            0xFFFFFFFF

