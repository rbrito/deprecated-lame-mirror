#ifndef LAME_IEEEFLOAT_H
#define LAME_IEEEFLOAT_H
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
 * Machine-independent I/O routines for IEEE floating-point numbers.
 *
 * NaN's and infinities are converted to HUGE_VAL or HUGE, which
 * happens to be infinity on IEEE machines.  Unfortunately, it is
 * impossible to preserve NaN's in a machine-independent way.
 * Infinities are, however, preserved on IEEE machines.
 *
 * These routines have been tested on the following machines:
 *	Apple Macintosh, MPW 3.1 C compiler
 *	Apple Macintosh, THINK C compiler
 *	Silicon Graphics IRIS, MIPS compiler
 *	Cray X/MP and Y/MP
 *	Digital Equipment VAX
 *	Sequent Balance (Multiprocesor 386)
 *	NeXT
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
 *
 * $Log$
 * Revision 1.3  2000/09/17 04:19:09  cisc
 * conformed all this-is-included-defines to match 'project_file_name' style
 *
 * Revision 1.2  2000/06/07 22:56:02  sbellon
 * added support for FPA10 hardware (RISC OS only)
 *
 * Revision 1.1.1.1  1999/11/24 08:42:58  markt
 * initial checkin of LAME
 * Starting with LAME 3.57beta with some modifications
 *
 * Revision 1.1  1993/06/11  17:45:46  malcolm
 * Initial revision
 *
 */

#if defined(__riscos__) && defined(FPA10)
#include	"ymath.h"
#else
#include	<math.h>
#endif

typedef float Single;

#ifndef applec
 typedef double defdouble;
#else /* !applec */
 typedef long double defdouble;
#endif /* applec */

#ifndef THINK_C
 typedef double Double;
#else /* THINK_C */
 typedef short double Double;
#endif /* THINK_C */

#define	kFloatLength	4
#define	kDoubleLength	8
#define	kExtendedLength	10

extern defdouble ConvertFromIeeeSingle(char *bytes);
extern void ConvertToIeeeSingle(defdouble num, char *bytes);
extern defdouble ConvertFromIeeeDouble(char *bytes);
extern void ConvertToIeeeDouble(defdouble num, char *bytes);
extern defdouble ConvertFromIeeeExtended(char *bytes);
extern void ConvertToIeeeExtended(defdouble num, char *bytes);
#endif
