#ifndef LOOP_PVT_H
#define LOOP_PVT_H
#include "l3side.h"
#define IXMAX_VAL 8206 /* ix always <= 8191+15.    see count_bits() */
#define PRECALC_SIZE (IXMAX_VAL+2)


extern int convert_mdct, convert_psy, reduce_sidechannel;
extern unsigned nr_of_sfb_block[6][3][4];
extern int pretab[SBMAX_l];
extern const int slen1_tab[16];
extern const int slen2_tab[16];

extern const scalefac_struct sfBandIndex[9];

extern FLOAT8 pow43[PRECALC_SIZE];
extern FLOAT8 adj43[PRECALC_SIZE];
extern FLOAT8 adj43asm[PRECALC_SIZE];

#define Q_MAX 256

extern FLOAT8 pow20[Q_MAX];
extern FLOAT8 ipow20[Q_MAX];

FLOAT8 ATHformula(lame_global_flags *gfp,FLOAT8 f);
void compute_ath(lame_global_flags *gfp,FLOAT8 ATH_l[SBPSY_l],FLOAT8 ATH_s[SBPSY_l]);
void ms_convert(FLOAT8 xr[2][576],FLOAT8 xr_org[2][576]);
int on_pe(lame_global_flags *gfp,FLOAT8 pe[2][2],III_side_info_t *l3_side,
int targ_bits[2],int mean_bits, int gr);
void reduce_side(int targ_bits[2],FLOAT8 ms_ener_ratio,int mean_bits,int max_bits);


void outer_loop( lame_global_flags *gfp,
                FLOAT8 xr[576],     /*vector of the magnitudees of the spectral values */
                int bits,
		FLOAT8 noise[4],
                III_psy_xmin *l3_xmin, /* the allowed distortion of the scalefactor */
                int l3_enc[576],    /* vector of quantized values ix(0..575) */
		 III_scalefac_t *scalefac, /* scalefactors */
		 gr_info *,
                FLOAT8 xfsf[4][SBMAX_l],
		int ch);



void iteration_init( lame_global_flags *gfp,III_side_info_t *l3_side, int l3_enc[2][2][576]);

int inner_loop( lame_global_flags *gfp,FLOAT8 xrpow[576],
                int l3_enc[576],
                int max_bits,
                gr_info *cod_info);

int calc_xmin( lame_global_flags *gfp,FLOAT8 xr[576],
               III_psy_ratio *ratio,
               gr_info *cod_info,
               III_psy_xmin *l3_xmin);


int scale_bitcount( III_scalefac_t *scalefac, gr_info *cod_info);
int scale_bitcount_lsf( III_scalefac_t *scalefac, gr_info *cod_info);
int calc_noise1( lame_global_flags *gfp, FLOAT8 xr[576],
                 int ix[576],
                 gr_info *cod_info,
                 FLOAT8 xfsf[4][SBMAX_l], 
		 FLOAT8 distort[4][SBMAX_l],
                 III_psy_xmin *l3_xmin,
		 III_scalefac_t *,
                 FLOAT8 *noise, FLOAT8 *tot_noise, FLOAT8 *max_noise);

int loop_break( III_scalefac_t *scalefac, gr_info *cod_info);

void amp_scalefac_bands(lame_global_flags *gfp, FLOAT8 xrpow[576],
			gr_info *cod_info,
			III_scalefac_t *scalefac,
			FLOAT8 distort[4][SBMAX_l]);

void quantize_xrpow( FLOAT8 xr[576],
               int  ix[576],
               gr_info *cod_info );
void quantize_xrpow_ISO( FLOAT8 xr[576],
               int  ix[576],
               gr_info *cod_info );

int
new_choose_table( int ix[576],
		  unsigned int begin,
		  unsigned int end, int * s );

int bin_search_StepSize2(lame_global_flags *gfp,int desired_rate, int start, int ix[576],
                         FLOAT8 xrspow[576], gr_info * cod_info);
int count_bits(lame_global_flags *gfp,int  *ix, FLOAT8 xr[576], gr_info *cod_info);


int quant_compare(int type,
int best_over,FLOAT8 best_tot_noise,FLOAT8 best_over_noise,FLOAT8 best_max_over,
int over,FLOAT8 tot_noise, FLOAT8 over_noise,FLOAT8 max_noise);

int VBR_compare(
int best_over,FLOAT8 best_tot_noise,FLOAT8 best_over_noise,FLOAT8 best_max_over,
int over,FLOAT8 tot_noise, FLOAT8 over_noise,FLOAT8 max_noise);

void best_huffman_divide(lame_internal_flags *gfc, int gr, int ch, gr_info *cod_info, int *ix);

void best_scalefac_store(lame_global_flags *gfp,int gr, int ch,
			 int l3_enc[2][2][576],
			 III_side_info_t *l3_side,
			 III_scalefac_t scalefac[2][2]);

void inc_scalefac_scale(lame_global_flags *gfp,
			III_scalefac_t *scalefac,
			gr_info *cod_info,
			FLOAT8 xrpow[576]);


int init_outer_loop(
    lame_global_flags *gfp,
    FLOAT8 xr[576],        /*  could be L/R OR MID/SIDE */
    gr_info *cod_info);

#define LARGE_BITS 100000

#endif

