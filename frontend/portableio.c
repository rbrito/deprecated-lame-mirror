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
#elif defined(HAVE_CONFIG_MS_H)
# include <configMS.h>
#endif

#include	<stdio.h>
#if defined(__riscos__) && defined(FPA10)
#include	"ymath.h"
#else
#include	<math.h>
#endif
#include	"portableio.h"

/****************************************************************
 * Big/little-endian independent I/O routines.
 ****************************************************************/


int
ReadByte(FILE *fp)
{
	int	result;

	result = getc(fp) & 0xff;
	if (result & 0x80)
		result = result - 0x100;
	return result;
}


int
Read16BitsLowHigh(FILE *fp)
{
	int	first, second, result;

	first = 0xff & getc(fp);
	second = 0xff & getc(fp);

	result = (second << 8) + first;
#ifndef	THINK_C42
	if (result & 0x8000)
		result = result - 0x10000;
#endif	/* THINK_C */
	return(result);
}


int
Read16BitsHighLow(FILE *fp)
{
	int	first, second, result;

	first = 0xff & getc(fp);
	second = 0xff & getc(fp);

	result = (first << 8) + second;
#ifndef	THINK_C42
	if (result & 0x8000)
		result = result - 0x10000;
#endif	/* THINK_C */
	return(result);
}


void
Write8Bits(FILE *fp, int i)
{
	putc(i&0xff,fp);
}


void
Write16BitsLowHigh(FILE *fp, int i)
{
	putc(i&0xff,fp);
	putc((i>>8)&0xff,fp);
}


void
Write16BitsHighLow(FILE *fp, int i)
{
	putc((i>>8)&0xff,fp);
	putc(i&0xff,fp);
}


int
Read24BitsHighLow(FILE *fp)
{
	int	first, second, third;
	int	result;

	first = 0xff & getc(fp);
	second = 0xff & getc(fp);
	third = 0xff & getc(fp);

	result = (first << 16) + (second << 8) + third;
	if (result & 0x800000)
		result = result - 0x1000000;
	return(result);
}

#define	Read32BitsLowHigh(f)	Read32Bits(f)


int
Read32Bits(FILE *fp)
{
	int	first, second, result;

	first = 0xffff & Read16BitsLowHigh(fp);
	second = 0xffff & Read16BitsLowHigh(fp);

	result = (second << 16) + first;
#ifdef	CRAY
	if (result & 0x80000000)
		result = result - 0x100000000;
#endif	/* CRAY */
	return(result);
}


int
Read32BitsHighLow(FILE *fp)
{
	int	first, second, result;

	first = 0xffff & Read16BitsHighLow(fp);
	second = 0xffff & Read16BitsHighLow(fp);

	result = (first << 16) + second;
#ifdef	CRAY
	if (result & 0x80000000)
		result = result - 0x100000000;
#endif
	return(result);
}


void
Write32Bits(FILE *fp, int i)
{
	Write16BitsLowHigh(fp,(int)(i&0xffffL));
	Write16BitsLowHigh(fp,(int)((i>>16)&0xffffL));
}


void
Write32BitsLowHigh(FILE *fp, int i)
{
	Write16BitsLowHigh(fp,(int)(i&0xffffL));
	Write16BitsLowHigh(fp,(int)((i>>16)&0xffffL));
}


void
Write32BitsHighLow(FILE *fp, int i)
{
	Write16BitsHighLow(fp,(int)((i>>16)&0xffffL));
	Write16BitsHighLow(fp,(int)(i&0xffffL));
}

void ReadBytes(FILE	*fp, char *p, int n)
{
	while (!feof(fp) & (n-- > 0))
		*p++ = getc(fp);
}

void ReadBytesSwapped(FILE *fp, char *p, int n)
{
	register char	*q = p;

	while (!feof(fp) & (n-- > 0))
		*q++ = getc(fp);

	for (q--; p < q; p++, q--){
		n = *p;
		*p = *q;
		*q = n;
	}
}

void WriteBytes(FILE *fp, char *p, int n)
{
	while (n-- > 0)
		putc(*p++, fp);
}

void WriteBytesSwapped(FILE *fp, char *p, int n)
{
	p += n-1;
	while (n-- > 0)
		putc(*p--, fp);
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

double
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

	if (expon == 0 && hiMant == 0 && loMant == 0) {
		f = 0;
	}
	else {
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
	char	bits[10];

	ReadBytes(fp, bits, 10);
	return ConvertFromIeeeExtended(bits);
}

