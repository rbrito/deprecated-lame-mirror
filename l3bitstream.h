/**********************************************************************
 * ISO MPEG Audio Subgroup Software Simulation Group (1996)
 * ISO 13818-3 MPEG-2 Audio Encoder - Lower Sampling Frequency Extension
 *
 * $Id$
 *
 * $Log$
 * Revision 1.6  2000/04/03 18:24:25  markt
 * more massive internal changes in the goal of making
 * lame thread safe.
 *
 * gfp-> user controlled varialbes
 * gfc-> all other internal parameters.  eventually all global variables
 *       which very from encode to encode need to go in here.
 *
 * Revision 1.5  2000/03/21 23:02:17  markt
 * replaced all "gf." by gfp->
 *
 * Revision 1.4  2000/03/14 20:45:04  markt
 * removed "info" sturct.   Removing fr_ps struct
 *
 * Revision 1.3  2000/02/01 11:26:32  takehiro
 * scalefactor's structure changed
 *
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

void III_format_bitstream( lame_global_flags *gfp,
			   int              bitsPerFrame,
			   int              l3_enc[2][2][576],
			   III_scalefac_t   scalefac[2][2]);

int HuffmanCode( int table_select, int x, int y, unsigned *code, unsigned int *extword, int *codebits, int *extbits );
void III_FlushBitstream(void);

int abs_and_sign( int *x ); /* returns signx and changes *x to abs(*x) */


#endif
