/**********************************************************************
 * ISO MPEG Audio Subgroup Software Simulation Group (1996)
 * ISO 13818-3 MPEG-2 Audio Encoder - Lower Sampling Frequency Extension
 *
 * $Id$
 *
 * $Log$
 * Revision 1.5  2000/01/09 10:54:56  takehiro
 * All Huffman code search algorithm is implemented.
 * (-h option to enable this)
 * more slower, but better quality.
 *
 * Revision 1.4  2000/01/05 10:04:20  markt
 * Check for Xing header in mp3 input files
 *
 * Revision 1.3  1999/12/21 08:18:51  markt
 * Bug fix in Mid/Side masking thresholds
 *
 * Revision 1.2  1999/11/29 02:45:59  markt
 * MS stereo switch slightly improved:  old formula was based on the average
 * of ms_ratio of both granules.  New formula uses ms_ratio from both
 * granules and the previous and next granule.  This will help avoid toggleing
 * MS stereo off for a single frame.  Long runs of MS stereo or regular
 * stereo will not be affected.
 *
 * also fixed a bug in frame analyzer - it was accessing l3_xmin in the last
 * scalefactor (l3_xmin and maskings are not computed for last scalefactor)
 *
 * Revision 1.1.1.1  1999/11/24 08:43:10  markt
 * initial checkin of LAME
 * Starting with LAME 3.57beta with some modifications
 *
 * Revision 1.1  1996/02/14 04:04:23  rowlands
 * Initial revision
 *
 * Received from Mike Coleman
 **********************************************************************/
/**********************************************************************
 *   date   programmers                comment                        *
 * 25. 6.92  Toshiyuki Ishino          Ver 1.0                        *
 * 29.10.92  Masahiro Iwadare          Ver 2.0                        *
 * 17. 4.93  Masahiro Iwadare          Updated for IS Modification    *
 *                                                                    *
 *********************************************************************/

#ifndef L3_SIDE_H
#define L3_SIDE_H
#include "encoder.h"
#include "machine.h"

/* Layer III side information. */

typedef FLOAT8	D576[576];
typedef int	I576[576];
typedef FLOAT8	D192_3[192][3];
typedef int	I192_3[192][3];


typedef struct {
	FLOAT8	thm_l[2][2][SBPSY_l];
	FLOAT8	thm_s[2][2][SBPSY_s][3];
	FLOAT8	en_l[2][2][SBPSY_l];
	FLOAT8	en_s[2][2][SBPSY_s][3];
} III_psy_ratio;

typedef struct {
	FLOAT8	l[2][2][SBPSY_l];
	FLOAT8	s[2][2][SBPSY_s][3];
} III_psy_xmin;

typedef struct {
	unsigned part2_3_length;
	unsigned big_values;
	unsigned count1;
 	unsigned global_gain;
	unsigned scalefac_compress;
	unsigned window_switching_flag;
	unsigned block_type;
	unsigned mixed_block_flag;
	unsigned table_select[3];
int /*	unsigned */ subblock_gain[3];
	unsigned region0_count;
	unsigned region1_count;
	unsigned preflag;
	unsigned scalefac_scale;
	unsigned count1table_select;

	unsigned part2_length;
	unsigned sfb_lmax;
	unsigned sfb_smax;
	unsigned count1bits;
	FLOAT8 quantizerStepSize;
	/* added for LSF */
	unsigned *sfb_partition_table;
	unsigned slen[4];
} gr_info;

typedef struct {
	int main_data_begin; /* unsigned -> int */
	unsigned private_bits;
	int resvDrain;
	unsigned scfsi[2][4];
	struct {
		struct gr_info_ss {
			gr_info tt;
			} ch[2];
		} gr[2];
	} III_side_info_t;

/* Layer III scale factors. */
/* note: there are only SBPSY_l=(SBMAX_l-1) and SBPSY_s=(SBMAX_s-1) scalefactors.
 * Dont know why these would be dimensioned SBMAX_l and SBMAX-s */
typedef struct {
	int l[2][2][SBMAX_l];            /* [cb] */
	int s[2][2][SBMAX_s][3];         /* [window][cb] */
	} III_scalefac_t;  /* [gr][ch] */

#endif
