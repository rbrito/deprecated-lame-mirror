/*
 *	Machine dependent defines/includes for LAME.
 *
 *	Copyright (c) 1999 A.L. Faber
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


#ifndef MACHINE_H_INCLUDED
#define MACHINE_H_INCLUDED

#if	defined hpux
#define	_POSIX_SOURCE
#endif

#include        <stdio.h>
#include        <string.h>
#include        <math.h>
#include        <stdlib.h>
#include		<ctype.h>
#include		<signal.h>
#include		<sys/types.h>
#include		<sys/stat.h>
#include		<fcntl.h>
#include		<errno.h>


/* three different types of pow() functions:  
   1. table lookup   
   2. pow()
   3. exp()   on some machines this is claimed to be faster than pow()
*/


#define POW20(x)  pow20[x]
/*
#define POW20(x)  pow(2.0,((double)(x)-210)*.25)
#define POW20(x)  exp( ((double)(x)-210)*(.25*LOG2) )
*/

#define IPOW20(x)  ipow20[x]
/*
#define IPOW20(x)  exp( -((double)(x)-210)*.1875*LOG2 ) 
#define IPOW20(x)  pow(2.0,-((double)(x)-210)*.1875)
*/



#if ( defined(_MSC_VER) && !defined(INLINE))
#	define INLINE _inline
#elif defined(__SASC) || defined(__GNUC__)
#	define INLINE __inline
#else
#	define INLINE
#endif

#if ( defined(_MSC_VER))
#	pragma warning( disable : 4244 )
#	pragma warning( disable : 4305 )
#endif

#if ( defined(_MSC_VER) || defined(__BORLANDC__) )
#	define WIN32_LEAN_AND_MEAN
#	include <windows.h>
#else
	typedef float FLOAT;
#endif
typedef double FLOAT8;
/* NOTE: the ASM versions of quantize_xrpow() assume FLOAT8=double. 
   so if you redefine this as float, disable the ASM routines for 
   quantize_xrpow() and quantize_xrpow_iso().   
*/


#if defined _WIN32 && !defined __CYGWIN__
#	define M_PI       3.14159265358979323846
#	define M_SQRT2	1.41421356237309504880
	typedef unsigned long	u_long;
	typedef unsigned int	u_int;
	typedef unsigned short	u_short;
	typedef unsigned char	u_char;

#	include <fcntl.h>
#	include <io.h>
#else
#if !defined __GNUC__ || defined __STRICT_ANSI__
	typedef unsigned long	u_long;
	typedef unsigned int	u_int;
	typedef unsigned short	u_short;
	typedef unsigned char	u_char;
#endif
#endif
#ifdef __CYGWIN__
#include <io.h>
#endif




#endif
