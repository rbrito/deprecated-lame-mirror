/*
 *	lame utility library source file
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
#ifdef WITH_DMALLOC
# include <dmalloc.h>
#endif
#include <assert.h>
#include <stdarg.h>

#ifdef HAVE_ERRNO_H
# include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif

#if defined(__FreeBSD__) && !defined(__alpha__)
#include <floatingpoint.h>
#endif
#ifdef __riscos__
#include "asmstuff.h"
#endif

#include "util.h"
#include "encoder.h"

/***********************************************************************
*
*  Global Function Definitions
*
***********************************************************************/
int
BitrateIndex(
    int bRate,        /* legal rates from 32 to 448 kbps */
    int version       /* MPEG-1 or MPEG-2/2.5 LSF */
    )
{
    int  i;
    for (i = 0; i <= 14; i++)
	if (bitrate_table[version][i] == bRate)
	    return i;

    return -1;
}

/*
 *  Disable floating point exceptions
 *  extremly system dependent stuff, move to a lib to make the code readable
 */
void disable_FPE(void) {
#if defined(__FreeBSD__) && !defined(__alpha__)
    {
        /* seet floating point mask to the Linux default */
        fp_except_t mask;
        mask = fpgetmask();
        /* if bit is set, we get SIGFCE on that error! */
        fpsetmask(mask & ~(FP_X_INV | FP_X_DZ));
        /*  DEBUGF("FreeBSD mask is 0x%x\n",mask); */
    }
#endif

#if defined(__riscos__) && !defined(ABORTFP)
    /* Disable FPE's under RISC OS */
    /* if bit is set, we disable trapping that error! */
    /*   _FPE_IVO : invalid operation */
    /*   _FPE_DVZ : divide by zero */
    /*   _FPE_OFL : overflow */
    /*   _FPE_UFL : underflow */
    /*   _FPE_INX : inexact */
    DisableFPETraps(_FPE_IVO | _FPE_DVZ | _FPE_OFL);
#endif

    /*
     *  Debugging stuff
     *  The default is to ignore FPE's, unless compiled with -DABORTFP
     *  so add code below to ENABLE FPE's.
     */

#if defined(ABORTFP)
#if defined(_MSC_VER)
    {

   /* set affinity to a single CPU.  Fix for EAC/lame on SMP systems from
     "Todd Richmond" <todd.richmond@openwave.com> */
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    SetProcessAffinityMask(GetCurrentProcess(), si.dwActiveProcessorMask);

#include <float.h>
        unsigned int mask;
        mask = _controlfp(0, 0);
        mask &= ~(_EM_OVERFLOW | _EM_UNDERFLOW | _EM_ZERODIVIDE | _EM_INVALID);
        mask = _controlfp(mask, _MCW_EM);
    }
#elif defined(__CYGWIN__)
#  define _FPU_GETCW(cw) __asm__ ("fnstcw %0" : "=m" (*&cw))
#  define _FPU_SETCW(cw) __asm__ ("fldcw %0" : : "m" (*&cw))

#  define _EM_INEXACT     0x00000020 /* inexact (precision) */
#  define _EM_UNDERFLOW   0x00000010 /* underflow */
#  define _EM_OVERFLOW    0x00000008 /* overflow */
#  define _EM_ZERODIVIDE  0x00000004 /* zero divide */
#  define _EM_INVALID     0x00000001 /* invalid */
    {
        unsigned int mask;
        _FPU_GETCW(mask);
        /* Set the FPU control word to abort on most FPEs */
        mask &= ~(_EM_OVERFLOW | _EM_ZERODIVIDE | _EM_INVALID);
        _FPU_SETCW(mask);
    }
# elif defined(__linux__)
    {

#  include <fpu_control.h>
#  ifndef _FPU_GETCW
#  define _FPU_GETCW(cw) __asm__ ("fnstcw %0" : "=m" (*&cw))
#  endif
#  ifndef _FPU_SETCW
#  define _FPU_SETCW(cw) __asm__ ("fldcw %0" : : "m" (*&cw))
#  endif

        /* 
         * Set the Linux mask to abort on most FPE's
         * if bit is set, we _mask_ SIGFPE on that error!
         *  mask &= ~( _FPU_MASK_IM | _FPU_MASK_ZM | _FPU_MASK_OM | _FPU_MASK_UM );
         */

        unsigned int mask;
        _FPU_GETCW(mask);
        mask &= ~(_FPU_MASK_IM | _FPU_MASK_ZM | _FPU_MASK_OM);
        _FPU_SETCW(mask);
    }
#endif
#endif /* ABORTFP */
}


/* Lookup table for fast CRC computation
 * See 'CRC_update_lookup'
 * Uses the polynomial x^16+x^15+x^2+1 */
static uint16_t crc16_lookup[256] =
{
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
    0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
    0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40, 
    0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
    0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40, 
    0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41, 
    0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
    0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
    0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
    0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441, 
    0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41, 
    0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
    0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41, 
    0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
    0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640, 
    0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
    0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240, 
    0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441, 
    0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
    0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840, 
    0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
    0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
    0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640, 
    0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041, 
    0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241, 
    0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
    0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40, 
    0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
    0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
    0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
    0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
    0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};

/* fast CRC-16 computation - uses table crc16_lookup 8*/
static int
CRC_update_lookup(int value, int crc)
{
    int tmp = crc ^ value;
    return (crc>>8) ^ crc16_lookup[tmp & 0xff];
}

uint16_t
calculateCRC(unsigned char *buffer, int size, uint16_t crc)
{
    while (--size >= 0)
	crc = CRC_update_lookup(*buffer++, crc);
    return crc;
}
