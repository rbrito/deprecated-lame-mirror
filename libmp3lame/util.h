/*
 *	lame utility library include file
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

#ifndef LAME_UTIL_H
#define LAME_UTIL_H

#include "lame.h"

#ifdef __cplusplus
extern "C" {
#endif

/***********************************************************************
*
*  Global Function Prototype Declarations
*
***********************************************************************/
int	BitrateIndex(int, int);
void	disable_FPE(void);

#ifdef HAVE_NASM
extern int  has_MMX   ( void );
extern int  has_3DNow ( void );
extern int  has_E3DNow( void );
extern int  has_SSE   ( void );
extern int  has_SSE2  ( void );
#else
# define has_MMX()	0
# define has_3DNow()	0
# define has_E3DNow()	0
# define has_SSE()	0
# define has_SSE2()	0
#endif

/***********************************************************************
*
*  Macros about Message Printing and Exit
*
***********************************************************************/
extern void lame_errorf(lame_t gfc, const char *, ...);
extern void lame_debugf(lame_t gfc, const char *, ...);
extern void lame_msgf  (lame_t gfc, const char *, ...);
#define DEBUGF  lame_debugf
#define ERRORF	lame_errorf
#define MSGF	lame_msgf


#ifdef __cplusplus
}
#endif

#endif /* LAME_UTIL_H */
