/* ========================================================================== 
 * @(#)fft_apple_veclib.c
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
 *	Substitution of the fft functions by the vDSP Library from Apple.
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

/* ==========================================================================
 * I n c l u d e s
 * ========================================================================== */
#ifdef USE_APPLE_VECLIB

#ifdef HAVE_CONFIG_H
# include <config.h>
#else
# include "configMacOS.h"
#endif


#include <math.h>

#include "util.h"
#include "fft.h"

#include <vecLib/vDSP.h>

/* ==========================================================================
 * Define this if you want some additional checks. The vDSP library does use
 * the AltiVec unit only if some requirements are met (see the comment of 
 * fht_APPLE_VDSP() below.
 * ========================================================================== */

static FFTSetup sFFTSetupLong;
static FFTSetup sFFTSetupShort;

static DSPSplitComplex sOpData;
static DSPSplitComplex sTmpData;

/* asume the biggest array size used in this lib for the allocation of 
 * temp. Buffers */
const static int kMaxArraySize = (BLKSIZE/2) + 16;

//#define CHECK_ALTIVEC 1
/* ==========================================================================
 * P r o t o t y p e s
 * ========================================================================== */
static void checkAltiVecAlign(const char* inMessage, const float *inDataPtr);
inline static void setArrayToNul(float* data, size_t inSize);

/* ==========================================================================
 * S t a t i c   ( i n t e r n a l )   M e t h o d s
 * ========================================================================== */
/* ==========================================================================
 *
 * ========================================================================== */
inline static void checkAltiVecAlign(const char* inMessage, const float *inDataPtr)
{
#ifdef CHECK_ALTIVEC
    if (((unsigned long)inDataPtr & 0x0f))
    {
        fprintf(stderr, "WARNING - the pointer '%s' is not aligned by 16 bytes! "
               "AltiVec Code will not be used! (%p)\n", 
               inMessage, inDataPtr);
    }
#endif // #ifdef CHECK_ALTIVEC
}

/* ==========================================================================
 *
 * ========================================================================== */
inline static void setArrayToNul(float* data, size_t inSize)
{
#if 0 // Should we use the vDSP lib to set the data to 0 (altivec) ?

    vsub(data, 1, data, 1, data, 1, inSize);

#else // use a conventional loop...
    int i;

     /* despite the membloc carries float values,
      * fill it with longs because a 0L is the same 
      * as 0.0f, but probably a little bit faster */
    for (i=0; i<inSize; ++i)
        ((long*)data)[i] = 0L;
#endif
}

/* ==========================================================================
 * G l o b a l   M e t h o d s
 * ========================================================================== */
 
/* ==========================================================================
 * Prepare the vDSP Lib fft routines for work and allocate some buffers which 
 * will speed up some repetive tasks. 
 *
 * Note, that a call of this function must be balanced by a free_APPLE_VDSP()!
 *
 * A good place to call this is very soon before the lib first performs a fft.
 * (e.g. 
 * 
 * You must call before using any fht_APPLE_VDSP()!
 * 
 * ========================================================================== */
void init_APPLE_VDSP()
{
    if (sOpData.imagp==NULL)
    {
        int i;
            
        sOpData.imagp = malloc(sizeof(float) * kMaxArraySize);
        for (i=0; i<kMaxArraySize; ++i)
           sOpData.imagp[i]=0.0; 
    }
    
    if (sTmpData.realp == NULL)
        sTmpData.realp=malloc(sizeof(float) * kMaxArraySize);

    if (sTmpData.imagp == NULL)
        sTmpData.imagp=malloc(sizeof(float) * kMaxArraySize);
    
    if (sFFTSetupShort == NULL)
        sFFTSetupShort = create_fftsetup(8-1, kFFTRadix2);

    if (sFFTSetupLong == NULL)
        sFFTSetupLong = create_fftsetup(10-1, kFFTRadix2);
}

/* ==========================================================================
 *
 * ========================================================================== */
void free_APPLE_VDSP()
{
    if (sFFTSetupLong)
    {
        destroy_fftsetup(sFFTSetupLong);
        sFFTSetupLong = NULL;
    }

    if (sFFTSetupShort)
    {
        destroy_fftsetup(sFFTSetupShort);
        sFFTSetupShort = NULL;
    }

    if (sOpData.imagp!=NULL)
    {
        free(sOpData.imagp);
        sOpData.imagp = NULL;
    }

    if (sTmpData.imagp != NULL)
    {
        free(sTmpData.imagp);
        sTmpData.imagp=NULL;
    }

    if (sTmpData.realp != NULL)
    {
        free(sTmpData.realp);
        sTmpData.realp=NULL;
    }
}

/* ==========================================================================
 * Do a FFT by using special FFT functions which is covered by Apples vDSP-Lib
 * The vecLib is a basic part of all PowerMacs running on Mac-OS 9.x and
 * OS-X as well.
 *
 * The "big deal" by using the vecLib is, that it benefits from a AltiVec unit
 * if present (on every PowerMACs equipped with a G4 and G5 PowerPC Processor).
 * 
 * If the Machine is "just" running on a G3, then don't worry. In this case the vecLib 
 * automatically uses the CPU and FPU to perform the fft.
 *
 * However to vecLib needs some asumptions to use the AltiVec Unit (excerpt 
 * from vDSP.h):
 *
 *  ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
 * void fft_zript(FFTSetup           setup,
 *                DSPSplitComplex *  ioData,
 *                SInt32             stride,
 *                DSPSplitComplex *  bufferTemp,
 *                UInt32             log2n,
 *                FFTDirection       direction);
 *
 *  Functions fft_zrip and fft_zript
 *  
 *  In-Place Real Fourier Transform with or without temporary memory,
 *  split Complex Format
 *          
 *    Criteria to invoke PowerPC vector code:    
 *      1. ioData.realp and ioData.imagp must be 16-byte aligned.
 *      2. stride = 1
 *      3. 3 <= log2n <= 13
 *    
 *    If any of the above criteria are not satisfied, the PowerPC scalor code
 *    implementation will be used.  The size of temporary memory for each part
 *    is the lower value of 4*n and 16k.  Direction can be either
 *    kFFTDirection_Forward or kFFTDirection_Inverse.
 *  ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
 * 
 * If you need further informations, then please refer the very good developer 
 * documentation at 
 * /Developer/Documentation/CoreTechnologies/vDSP/vDSP_Library.pdf
 *
 * or in the wab at http://developer.apple.com/documentation/Performance/Conceptual/vDSP/vDSP_Library.pdf
 * ========================================================================== */
void fht_APPLE_VDSP(FLOAT *fz, int n)
{
    FFTSetup setup;
    int log2n;
    
    if (n==BLKSIZE_s/2)
    {
        log2n = 7;
        setup = sFFTSetupShort;
    }
    else
    {
        log2n = 9;
        setup = sFFTSetupLong;
    }
    
    sOpData.realp = fz;
    
    checkAltiVecAlign("sOpData.realp",  sOpData.realp);
    checkAltiVecAlign("sOpData.imagp",  sOpData.imagp);
    checkAltiVecAlign("sTmpData.realp", sTmpData.realp);
    checkAltiVecAlign("sTmpData.imagp", sTmpData.imagp);
    
//    fft_zip(setup, &sOpData, 1, log2n, kFFTDirection_Forward);
#if 0
    ctoz((DSPComplex*)fz, 2, &sOpData, 1, n);
    fft_zript(setup, &sOpData, 1, &sTmpData, log2n, kFFTDirection_Forward);
#else
    setArrayToNul(sOpData.imagp, 1<<log2n);
    fft_zipt(setup, &sOpData, 1, &sTmpData, log2n, kFFTDirection_Forward);
#endif    
}
#endif // USE_APPLE_VECLIB
