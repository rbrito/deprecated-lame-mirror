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



#if ( defined(_MSC_VER) || defined(__BORLANDC__) )
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
#else
	typedef float FLOAT;
#endif
typedef double FLOAT8;



#ifdef _WIN32
	# define M_PI       3.14159265358979323846
	# define M_SQRT2	1.41421356237309504880
	typedef unsigned long	u_long;
	typedef unsigned int	u_int;
	typedef unsigned short	u_short;
	typedef unsigned char	u_char;

	#include <fcntl.h>
	#include <io.h>
#endif


#endif
