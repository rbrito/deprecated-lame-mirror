/*
 *	psymodel.h
 *
 *	Copyright (c) 1999 Mark Taylor
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

#ifndef LAME_PSYMODEL_H
#define LAME_PSYMODEL_H

#define rpelev 2
#define rpelev2 16

/* size of each partition band, in barks: */
#define DELBARK .34

#define vo_scale (1./( 14752 ))

#define TEMPORALMASK_SUSTAIN_SEC 0.01

#define NS_PREECHO_ATT0 1.0 /* 0.8 */
#define NS_PREECHO_ATT1 0.6
#define NS_PREECHO_ATT2 0.3

#define NS_MSFIX 4.0
#define NSATTACKTHRE 1.8
#define NSATTACKTHRE_S 10

void psycho_analysis( lame_global_flags *gfp,
		      const sample_t *buffer[2],
		      FLOAT ms_ener_ratio_d[2],
		      III_psy_ratio masking[2][2],
		      FLOAT sbsmpl[2][2*1152] );

int psymodel_init(lame_global_flags *gfp);
void init_mask_add_max_values(lame_internal_flags *gfc);

#endif /* LAME_PSYMODEL_H */
