#ifndef L3_SIDE_H
#define L3_SIDE_H
#include "encoder.h"
#include "machine.h"

/* Layer III side information. */

typedef FLOAT8	D576[576];
typedef int	I576[576];
typedef FLOAT8	D192_3[192][3];
typedef int	I192_3[192][3];


typedef struct 
{
   int l[1+SBMAX_l];
   int s[1+SBMAX_s];
} scalefac_struct;


typedef struct {
	FLOAT8	l[SBMAX_l];
	FLOAT8	s[SBMAX_s][3];
} III_psy_xmin;

typedef struct {
    III_psy_xmin thm;
    III_psy_xmin en;
} III_psy_ratio;

typedef struct {
	unsigned int part2_3_length;
	unsigned int big_values;
	unsigned int count1;
 	unsigned int global_gain;
	unsigned int scalefac_compress;
	unsigned int window_switching_flag;
	unsigned int block_type;
	unsigned int mixed_block_flag;
	unsigned int table_select[3];
	unsigned int subblock_gain[3];
	unsigned int region0_count;
	unsigned int region1_count;
	unsigned int preflag;
	unsigned int scalefac_scale;
	unsigned int count1table_select;

	unsigned int part2_length;
	unsigned int sfb_lmax;
	unsigned int sfb_smax;
	unsigned int count1bits;
	/* added for LSF */
	unsigned int *sfb_partition_table;
	unsigned int slen[4];
} gr_info;

typedef struct {
	int main_data_begin; /* unsigned -> int */
	unsigned int private_bits;
	int resvDrain_pre;
	int resvDrain_post;
	unsigned int scfsi[2][4];
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
	int l[SBMAX_l];            /* [cb] */
	int s[SBMAX_s][3];         /* [window][cb] */
} III_scalefac_t;  /* [gr][ch] */

#endif
