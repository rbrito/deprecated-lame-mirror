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
#else
#include	<math.h>
#endif
#include	"portableio.h"

#ifdef WITH_DMALLOC
#include <dmalloc.h>
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

static signed int
ReadByte ( FILE* fp )
{
    int  result = getc (fp);
    return result == EOF  ?  0  :  (signed char) (result & 0xFF);
}

static unsigned int
ReadByteUnsigned ( FILE* fp )
{
    int  result = getc (fp);
    return result == EOF  ?  0  :  (unsigned char) (result & 0xFF);
}

int  Read16BitsLowHigh ( FILE* fp )
{
    int  low  = ReadByteUnsigned (fp);
    int  high = ReadByte         (fp);
    
    return (high << 8) | low;
}

int  Read16BitsHighLow ( FILE* fp )
{
    int  high = ReadByte         (fp);
    int  low  = ReadByteUnsigned (fp);
    
    return (high << 8) | low;
}

void
Write16BitsLowHigh(FILE *fp, int i)
{
	putc(i&0xff,fp);
	putc((i>>8)&0xff,fp);
}

#define	Read32BitsLowHigh(f)	Read32Bits(f)

int  Read32Bits ( FILE* fp )
{
    int  low  = ReadByteUnsigned (fp);
    int  medl = ReadByteUnsigned (fp);
    int  medh = ReadByteUnsigned (fp);
    int  high = ReadByte         (fp);

    return (high << 24) | (medh << 16) | (medl << 8) | low;
}

int  Read32BitsHighLow ( FILE* fp )
{
    int  high = ReadByte         (fp);
    int  medh = ReadByteUnsigned (fp);
    int  medl = ReadByteUnsigned (fp);
    int  low  = ReadByteUnsigned (fp);
    
    return (high << 24) | (medh << 16) | (medl << 8) | low;
}

void
Write32BitsLowHigh(FILE *fp, int i)
{
	Write16BitsLowHigh(fp,(int)(i&0xffffL));
	Write16BitsLowHigh(fp,(int)((i>>16)&0xffffL));
}


void ReadBytes (FILE     *fp, char *p, int n) 
{
    memset ( p, 0, n );
    fread  ( p, 1, n, fp );
}

void WriteBytes(FILE *fp, char *p, int n)
{
    /* return n == */
    fwrite ( p, 1, n, fp );
}

void WriteBytesSwapped(FILE *fp, char *p, int n)
{
    p += n;
    while ( n-- > 0 )
	putc ( *--p, fp );
}

/****************************************************************
 * The following two routines make up for deficiencies in many
 * compilers to convert properly between unsigned integers and
 * floating-point.  Some compilers which have this bug are the
 * THINK_C compiler for the Macintosh and the C compiler for the
 * Silicon Graphics MIPS-based Iris.
 ****************************************************************/

#ifdef applec	/* The Apple C compiler works */
# define FloatToUnsigned(f)	((unsigned long)(f))
# define UnsignedToFloat(u)	((double)(u))
#else /* applec */
# define FloatToUnsigned(f)	((unsigned long)(((long)((f) - 2147483648.0)) + 2147483647L + 1))
# define UnsignedToFloat(u)	(((double)((long)((u) - 2147483647L - 1))) + 2147483648.0)
#endif /* applec */
/****************************************************************
 * Extended precision IEEE floating-point conversion routines
 ****************************************************************/

static double
ConvertFromIeeeExtended(char* bytes)
{
	double	f;
	long	expon;
	unsigned long hiMant, loMant;

#ifdef	TEST
printf("ConvertFromIEEEExtended(%lx,%lx,%lx,%lx,%lx,%lx,%lx,%lx,%lx,%lx\r",
	(long)bytes[0], (long)bytes[1], (long)bytes[2], (long)bytes[3],
	(long)bytes[4], (long)bytes[5], (long)bytes[6],
	(long)bytes[7], (long)bytes[8], (long)bytes[9]);
#endif

	expon = ((bytes[0] & 0x7F) << 8) | (bytes[1] & 0xFF);
	hiMant	=	((unsigned long)(bytes[2] & 0xFF) << 24)
			|	((unsigned long)(bytes[3] & 0xFF) << 16)
			|	((unsigned long)(bytes[4] & 0xFF) << 8)
			|	((unsigned long)(bytes[5] & 0xFF));
	loMant	=	((unsigned long)(bytes[6] & 0xFF) << 24)
			|	((unsigned long)(bytes[7] & 0xFF) << 16)
			|	((unsigned long)(bytes[8] & 0xFF) << 8)
			|	((unsigned long)(bytes[9] & 0xFF));

        /* This case should also be called if the number is below the smallest
	 * positive double variable */
	if (expon == 0 && hiMant == 0 && loMant == 0) {
		f = 0;
	}
	else {
	        /* This case should also be called if the number is too large to fit into 
		 * a double variable */
	    
		if (expon == 0x7FFF) {	/* Infinity or NaN */
			f = HUGE_VAL;
		}
		else {
			expon -= 16383;
			f  = ldexp(UnsignedToFloat(hiMant), (int) (expon -= 31));
			f += ldexp(UnsignedToFloat(loMant), (int) (expon -= 32));
		}
	}

	if (bytes[0] & 0x80)
		return -f;
	else
		return f;
}

double
ReadIeeeExtendedHighLow(FILE *fp)
{
	char	bytes [10];

	ReadBytes ( fp, bytes, 10 );
	return ConvertFromIeeeExtended ( bytes );
}

