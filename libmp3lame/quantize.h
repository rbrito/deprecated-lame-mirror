/*
 * MP3 quantization
 *
 * Copyright (c) 1999 Mark Taylor
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

#ifndef LAME_QUANTIZE_H
#define LAME_QUANTIZE_H

#include "util.h"

void iteration_loop( lame_global_flags *gfp,
		     III_psy_ratio ratio[2][2]);

void VBR_iteration_loop( lame_global_flags *gfp,
			 III_psy_ratio ratio[2][2]);

void ABR_iteration_loop( lame_global_flags *gfp,
			 III_psy_ratio ratio[2][2]);

void    iteration_init (lame_global_flags *gfp);



#ifndef NOANALYSIS
void    set_frame_pinfo (lame_global_flags *gfp, III_psy_ratio ratio[2][2],
			 const sample_t *inbuf[]);
#endif

#endif /* LAME_QUANTIZE_H */
