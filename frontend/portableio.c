/* Copyright (C) 1988-1991 Apple Computer, Inc.
 * All Rights Reserved.
 *
 * Warranty Information
 * Even though Apple has reviewed this software, Apple makes no warranty
 * or representation, either express or implied, with respect to this
 * software, its quality, accuracy, merchantability, or fitness for a
 * particular purpose.  As a result, this software is provided "as is,"
 * and you, its user, are assuming the entire risk as to its quality
 * and accuracy.
 *
 * This code may be used and freely distributed as long as it includes
 * this copyright notice and the warranty information.
 *
 *
 * Motorola processors (Macintosh, Sun, Sparc, MIPS, etc)
 * pack bytes from high to low (they are big-endian).
 * Use the HighLow routines to match the native format
 * of these machines.
 *
 * Intel-like machines (PCs, Sequent)
 * pack bytes from low to high (the are little-endian).
 * Use the LowHigh routines to match the native format
 * of these machines.
 *
 * These routines have been tested on the following machines:
 *	Apple Macintosh, MPW 3.1 C compiler
 *	Apple Macintosh, THINK C compiler
 *	Silicon Graphics IRIS, MIPS compiler
 *	Cray X/MP and Y/MP
 *	Digital Equipment VAX
 *
 *
 * Implemented by Malcolm Slaney and Ken Turkowski.
 *
 * Malcolm Slaney contributions during 1988-1990 include big- and little-
 * endian file I/O, conversion to and from Motorola's extended 80-bit
 * floating-point format, and conversions to and from IEEE single-
 * precision floating-point format.
 *
 * In 1991, Ken Turkowski implemented the conversions to and from
 * IEEE double-precision format, added more precision to the extended
 * conversions, and accommodated conversions involving +/- infinity,
 * NaN's, and denormalized numbers.
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include	<stdio.h>
#if defined(__riscos__) && defined(FPA10)
#include	"ymath.h"
#endif
#include	"portableio.h"

#ifdef WITH_DMALLOC
# include <dmalloc.h>
#endif

/****************************************************************
 * Big/little-endian independent I/O routines.
 ****************************************************************/

/*
 * It is a hoax to call this code portable-IO:
 * 
 *   - It doesn't work on machines with CHAR_BIT != 8
 *   - it also don't test this error condition
 *   - otherwise it tries to handle CHAR_BIT != 8 by things like 
 *     masking 'putc(i&0xff,fp)'
 *   - It doesn't handle EOF in any way
 *   - it only works with ints with 32 or more bits
 *   - It is a collection of initial buggy code with patching the known errors
 *     instead of CORRECTING them! 
 *     For that see comments on the old Read16BitsHighLow()
 */

static unsigned int
ReadByteUnsigned ( FILE* fp )
{
    int  result = getc (fp);
    return result == EOF  ?  0  : result;
}

int  Read16BitsLowHigh ( FILE* fp )
{
    int  low  = ReadByteUnsigned (fp);
    int  high = ReadByteUnsigned (fp);

    return (int) ((short)((high << 8) | low));
}

void
Write16BitsLowHigh(FILE *fp, int i)
{
    putc(i & 0xff, fp);
    putc((i >> 8) & 0xff, fp);
}

int  Read32Bits ( FILE* fp )
{
    int  low  = ReadByteUnsigned (fp);
    int  medl = ReadByteUnsigned (fp);
    int  medh = ReadByteUnsigned (fp);
    int  high = ReadByteUnsigned (fp);

    return (high << 24) | (medh << 16) | (medl << 8) | low;
}

int  Read32BitsHighLow ( FILE* fp )
{
    int  high = ReadByteUnsigned (fp);
    int  medh = ReadByteUnsigned (fp);
    int  medl = ReadByteUnsigned (fp);
    int  low  = ReadByteUnsigned (fp);
    
    return (high << 24) | (medh << 16) | (medl << 8) | low;
}

void
Write32BitsLowHigh(FILE *fp, int i)
{
    Write16BitsLowHigh(fp, i);
    Write16BitsLowHigh(fp, i >> 16);
}


void ReadBytes (FILE     *fp, char *p, int n) 
{
    memset ( p, 0, n );
    fread  ( p, 1, n, fp );
}
