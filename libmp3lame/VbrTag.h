/*
 *	Xing VBR tagging for LAME.
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

#ifndef LAME_VRBTAG_H
#define LAME_VRBTAG_H

/* -----------------------------------------------------------
 * A Vbr header may be present in the ancillary
 * data field of the first frame of an mp3 bitstream
 * The Vbr header (optionally) contains
 *      frames      total number of audio frames in the bitstream
 *      bytes       total number of bytes in the bitstream
 *      toc         table of contents

 * toc (table of contents) gives seek points
 * for random access
 * the ith entry determines the seek point for
 * i-percent duration
 * seek point in bytes = (toc[i]/256.0) * total_bitstream_bytes
 * e.g. half duration seek point = (toc[50]/256.0) * total_bitstream_bytes
 */

int InitVbrTag(lame_t gfc);
int PutVbrTag(lame_t gfc, FILE *fid);
void AddVbrFrame(lame_t gfc);
void UpdateMusicCRC(uint16_t *crc,unsigned char *buffer, int size);

#endif
