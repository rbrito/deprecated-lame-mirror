/* ========================================================================== 
 * @(#)fft_apple_veclib.h
 * --------------------------------------------------------------------------   
 *
 *  (c)1982-2003 Tangerine-Software
 * 
 *       Hans-Peter Dusel
 *       An der Dreifaltigkeit 9
 *       89331 Burgau
 *       GERMANY
 * 
 *       mailto:hdusel@bnv-gz.de
 *       http://www.bnv-gz.de/~hdusel
 * --------------------------------------------------------------------------   
 *
 *	Substitution of the fft functions by the vecLib vDSP Library from Apple.
 *  This library automatically detects if the code is running on a G4 or G5
 *  PowerPC it benefits from their AltiVec Vector processing unit in order 
 *  to gain speed.
 *
 *
 *  Here is an excerpt of the description from Apple:
 *
 *  vDSP 
 *  =====
 *  The vDSP Library provides mathematical functions for applications such as 
 *  speech, sound, audio, and video processing, diagnostic medical imaging, 
 *  radar signal processing, seismic analysis, and scientific data processing. 
 *
 *  The vDSP functions operate on real and complex data types. The functions
 *  include data type conversions, fast Fourier transforms (FFTs), and 
 *  vector-to-vector and vector-to-scalar operations. 
 *
 *  The vDSP functions have been implemented in two ways: as vectorized code 
 *  (for single precision only), which uses the vector unit on the PowerPC G4 
 *  and G5 microprocessors, and as scalar code, which runs on Macintosh models
 *  that have a G3 microprocessor. 
 *
 *  It is noteworthy that vDSP's FFTs are one of the fastest implementations of
 *  the Discrete Fourier Transforms available anywhere. 
 *
 *  The vDSP Library itself is included as part of vecLib in Mac OS X. 
 *  The header file, vDSP.h, defines data types used by the vDSP functions and 
 *  symbols accepted as flag arguments to vDSP functions. 
 *  vDSP functions are available in single and double precision. 
 *
 *  Note that only the single precision is vectorized due to the underlying 
 *  instruction set architecture of the vector engine on board G4 and G5 processors. 
 *
 *  For more information about vDSP download the manual at 
 *  <http://developer.apple.com/hardware/ve/downloads/vDSP.sit.hqx> 
 *
 * --------------------------------------------------------------------------   
 *
 * This library (except the vecLib and vDSP headers from Apple) is free 
 * software; you can redistribute it and/or modify it under the terms of 
 * the GNU Library General Public License as published by the Free Software 
 * Foundation; either version 2 of the License, or (at your option) any 
 * later version.
 *
 * --------------------------------------------------------------------------   
 * The vecLib and all respective headers are
 * Copyright:  © 2000-2001 by Apple Computer, Inc., all rights reserved.
 * --------------------------------------------------------------------------   
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
 * --------------------------------------------------------------------------   
 *
 * $Id$
 * ========================================================================== */

#ifndef _FFT_APPLE_VECLIB_H_
#define _FFT_APPLE_VECLIB_H_
/* ==========================================================================
 * I n c l u d e s
 * ========================================================================== */

/* ==========================================================================
 * P r o t o t y p e s
 * ========================================================================== */

void init_APPLE_VDSP();
void free_APPLE_VDSP();

void fht_APPLE_VDSP(FLOAT *fz, int n);

#endif /* #ifndef _FFT_APPLE_VECLIB_H_ */
