/*
 *	psymodel.h
 *
 *	Copyright (c) 1999 Mark Taylor
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

#ifndef LAME_PSYMODEL_H
#define LAME_PSYMODEL_H

/* size of each partition band, in barks */
#define DELBARK ((FLOAT)(1.0/3.0))
#define FREQ_BOUND ((FLOAT)1.35)

#define ATHAdjustLimit ((FLOAT)0.001)

#define TEMPORALMASK_SUSTAIN_SEC ((FLOAT)0.01)

/* FFT -> MDCT conversion factor */
#define FFT2MDCT ((FLOAT)(32768.0*32768*1024))

#define NS_MSFIX ((FLOAT)4.0)

#define LONG_2_SHORT_FACTOR (FLOAT)(((double)BLKSIZE_s*BLKSIZE_s) / (BLKSIZE*BLKSIZE));

void psycho_analysis(lame_t gfc,
		     III_psy_ratio masking[MAX_GRANULES][MAX_CHANNELS],
		     FLOAT sbsmpl[MAX_CHANNELS][1152] );

void init_mask_add_max_values(lame_t gfc);

#endif /* LAME_PSYMODEL_H */
