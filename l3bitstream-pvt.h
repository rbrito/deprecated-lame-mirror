/**********************************************************************
 * ISO MPEG Audio Subgroup Software Simulation Group (1996)
 * ISO 13818-3 MPEG-2 Audio Encoder - Lower Sampling Frequency Extension
 *
 * $Id$
 *
 * $Log$
 * Revision 1.1  1999/11/24 08:42:59  markt
 * Initial revision
 *
 * Revision 1.1  1996/02/14 04:04:23  rowlands
 * Initial revision
 *
 * Received from Mike Coleman
 **********************************************************************/

#ifndef L3BITSTREAM_PVT_H
#define L3BITSTREAM_PVT_H

static int encodeSideInfo( III_side_info_t  *si );

static void encodeMainData( int              l3_enc[2][2][576],
			    III_side_info_t  *si,
			    III_scalefac_t   *scalefac );

static void write_ancillary_data( char *theData, int lengthInBits );

static void drain_into_ancillary_data( int lengthInBits );

static void Huffmancodebits( BF_PartHolder **pph, int *ix, gr_info *gi );


#endif
