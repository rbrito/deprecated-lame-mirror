/**********************************************************************
 * ISO MPEG Audio Subgroup Software Simulation Group (1996)
 * ISO 13818-3 MPEG-2 Audio Encoder - Lower Sampling Frequency Extension
 *
 * $Id$
 *
 * $Log$
 * Revision 1.1  1999/11/24 08:43:40  markt
 * Initial revision
 *
 * Revision 1.1  1996/02/14 04:04:23  rowlands
 * Initial revision
 *
 * Received from Mike Coleman
 **********************************************************************/
/*
  Revision History:

  Date        Programmer                Comment
  ==========  ========================= ===============================
  1995/09/06  mc@fivebats.com           created

*/

#ifndef RESERVOIR_H
#define RESERVOIR_H

int ResvFrameBegin( frame_params *fr_ps, III_side_info_t *l3_side, int mean_bits, int frameLength );
void ResvMaxBits2( int mean_bits, int *targ_bits, int *max_bits, int gr);
void ResvAdjust( frame_params *fr_ps, gr_info *gi, III_side_info_t *l3_side, int mean_bits );
void ResvFrameEnd( frame_params *fr_ps, III_side_info_t *l3_side, int mean_bits );

#endif
