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


#ifdef AMIGA_ASYNCIO
 #include <proto/dos.h>
 #include <proto/exec.h>
 #include <proto/asyncio.h>

 #undef SEEK_SET
 #define SEEK_SET MODE_START
 #undef SEEK_CUR
 #define SEEK_CUR MODE_CURRENT
 #undef SEEK_END
 #define SEEK_END MODE_END

 #define fclose(file)				((int) CloseAsync(file))
 #define fread(buf,size,num,file)	((size_t) (ReadAsync(file,buf,size*num)/size))
 #define fwrite(buf,size,num,file)	((size_t) (WriteAsync(file,buf,size*num)/size))
 #define fseek(file,pos,mode)		((int) SeekAsync(file,pos,mode))
 #define ftell(file)				((long) SeekAsync(file,0L,MODE_CURRENT))
 #undef feof
 #define feof(file)					((int) (IoErr() == EOF))
 #undef ferror
 #define ferror(file)				((int) (IoErr() != 0))
 #undef clearerr
 #define clearerr(file)				((void) SetIoErr(0L))

 typedef AsyncFile FILE;

 #ifdef __PPC__
  #define AmigaReadBuffer 400000
  #define AmigaWriteBuffer 65536
 #else
  #define AmigaReadBuffer 200000
  #define AmigaWriteBuffer 32768
 #endif
#endif


#if ( defined(_MSC_VER) && !defined(INLINE))
	#define INLINE _inline
#else
	#define INLINE inline
#endif


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
