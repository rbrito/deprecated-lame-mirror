/*
 * set_get.h -- Internal set/get definitions
 *
 * Copyright (C) 2003 Gabriel Bouvigne / Lame project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __SET_GET_H__
#define __SET_GET_H__

#include "lame.h"


#if defined(__cplusplus)
extern "C" {
#endif

#if defined(WIN32)
#undef CDECL
#define CDECL _cdecl
#else
#define CDECL
#endif


/* short blocks switching threshold */
int CDECL lame_set_short_threshold(lame_global_flags *, float s);

/* use mixed blocks */
int CDECL lame_set_use_mixed_blocks(lame_global_flags *, int);

int CDECL lame_set_vbr_smooth( lame_global_flags *, int);
int CDECL lame_get_vbr_smooth( const lame_global_flags *);

int CDECL lame_set_maskingadjust( lame_global_flags *, float);
float CDECL lame_get_maskingadjust( const lame_global_flags *);

int CDECL lame_set_maskingadjust_short( lame_global_flags *, float);
float CDECL lame_get_maskingadjust_short( const lame_global_flags *);

/* select ATH formula shape */
int CDECL lame_set_ATHcurve(lame_global_flags *, float);
int CDECL lame_get_ATHcurve(const lame_global_flags *);

/* substep shaping method */
int CDECL lame_set_substep(lame_global_flags *, int);
int CDECL lame_get_substep(const lame_global_flags *);

/* scalefactors scale */
int CDECL lame_set_sfscale(lame_global_flags *, int);
int CDECL lame_get_sfscale(const lame_global_flags *);

/* subblock gain */
int CDECL lame_set_subblock_gain(lame_global_flags *, int);
int CDECL lame_get_subblock_gain(const lame_global_flags *);

/* narrowen the stereo image */
int lame_set_narrowenStereo(lame_global_flags *, float);

/* reduce side channel PE */
int lame_set_reduceSide(lame_global_flags *, float);

#if defined(__cplusplus)
}
#endif

#endif
