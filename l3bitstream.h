/**********************************************************************
 * ISO MPEG Audio Subgroup Software Simulation Group (1996)
 * ISO 13818-3 MPEG-2 Audio Encoder - Lower Sampling Frequency Extension
 *
 * $Id$
 *
 * $Log$
 * Revision 1.2  1999/12/03 09:45:30  takehiro
 * little bit cleanup
 *
 * Revision 1.1.1.1  1999/11/24 08:43:09  markt
 * initial checkin of LAME
 * Starting with LAME 3.57beta with some modifications
 *
 * Revision 1.1  1996/02/14 04:04:23  rowlands
 * Initial revision
 *
 * Received from Mike Coleman
 **********************************************************************/

#ifndef L3_BITSTREAM_H
#define L3_BITSTREAM_H

#include "util.h"

void III_format_bitstream( int              bitsPerFrame,
			   frame_params     *in_fr_ps,
			   int              l3_enc[2][2][576],
                           III_side_info_t  *l3_side,
			   III_scalefac_t   *scalefac,
			   Bit_stream_struc *in_bs);

int HuffmanCode( int table_select, int x, int y, unsigned *code, unsigned int *extword, int *codebits, int *extbits );
void III_FlushBitstream(void);

int abs_and_sign( int *x ); /* returns signx and changes *x to abs(*x) */


#endif
