#ifndef LAME_PORTABLEIO_H
#define LAME_PORTABLEIO_H
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
 * Machine-independent I/O routines for 8-, 16-, 24-, and 32-bit integers.
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

#include <stdio.h>

#if defined(__cplusplus)
extern "C" {
#endif

extern int ReadByte(FILE * fp);
extern int Read16BitsLowHigh(FILE * fp);
extern int Read16BitsHighLow(FILE * fp);
extern void Write8Bits(FILE * fp, int i);
extern void Write16BitsLowHigh(FILE * fp, int i);
extern void Write16BitsHighLow(FILE * fp, int i);
extern int Read24BitsHighLow(FILE * fp);
extern int Read32Bits(FILE * fp);
extern int Read32BitsHighLow(FILE * fp);
extern void Write32Bits(FILE * fp, int i);
extern void Write32BitsLowHigh(FILE * fp, int i);
extern void Write32BitsHighLow(FILE * fp, int i);
extern void ReadBytes(FILE * fp, char *p, int n);
extern void ReadBytesSwapped(FILE * fp, char *p, int n);
extern void WriteBytes(FILE * fp, char *p, int n);
extern void WriteBytesSwapped(FILE * fp, char *p, int n);
extern double ReadIeeeFloatHighLow(FILE * fp);
extern double ReadIeeeFloatLowHigh(FILE * fp);
extern double ReadIeeeDoubleHighLow(FILE * fp);
extern double ReadIeeeDoubleLowHigh(FILE * fp);
extern double ReadIeeeExtendedHighLow(FILE * fp);
extern double ReadIeeeExtendedLowHigh(FILE * fp);
extern void WriteIeeeFloatLowHigh(FILE * fp, double num);
extern void WriteIeeeFloatHighLow(FILE * fp, double num);
extern void WriteIeeeDoubleLowHigh(FILE * fp, double num);
extern void WriteIeeeDoubleHighLow(FILE * fp, double num);
extern void WriteIeeeExtendedLowHigh(FILE * fp, double num);
extern void WriteIeeeExtendedHighLow(FILE * fp, double num);

#define Read32BitsLowHigh(f) Read32Bits(f)
#define WriteString(f,s) fwrite(s,strlen(s),sizeof(char),f)

#if defined(__cplusplus)
}
#endif

#endif
