/*
 *	Fast Fourier Transform include file
 *
 *	Copyright (c) 2000 Mark Taylor
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

#ifndef __FFT_H__
# define __FFT_H__

# include "encoder.h"

void fft_long  ( lame_global_flags* gfp, FLOAT x_real     [BLKSIZE  ], int, sample_t ** );
void fft_short ( lame_global_flags* gfp, FLOAT x_real [3] [BLKSIZE_s], int, sample_t ** );
void init_fft  ( lame_global_flags* gfp );

#endif

/* End of fft.h */
