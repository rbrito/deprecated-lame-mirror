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

void	disable_FPE(void);
uint16_t calculateCRC(unsigned char *p, int size, uint16_t crc);
#ifdef __x86_64__
# define lr2ms(gfc, pl, pr, len) lr2ms_SSE(pl, pr, len)
void lr2ms_SSE(FLOAT *, FLOAT *, int);
#else
void lr2ms(lame_t gfc, FLOAT *, FLOAT *, int);
#endif

#endif /* LAME_UTIL_H */
