#ifndef LOOP_PVT_H
#define LOOP_PVT_H
#include "l3side.h"
#define IXMAX_VAL 8206 /* ix always <= 8191+15.    see count_bits() */
#define PRECALC_SIZE (IXMAX_VAL+2)


extern int convert_mdct, convert_psy, reduce_sidechannel;
extern unsigned int nr_of_sfb_block[6][3][4];
extern int pretab[SBMAX_l];
extern const int slen1_tab[16];
extern const int slen2_tab[16];

extern const scalefac_struct sfBandIndex[9];

extern FLOAT8 pow43[PRECALC_SIZE];
extern FLOAT8 adj43[PRECALC_SIZE];
extern FLOAT8 adj43asm[PRECALC_SIZE];

#define Q_MAX 330

extern FLOAT8 pow20[Q_MAX];
extern FLOAT8 ipow20[Q_MAX];

typedef struct calc_noise_result_t {
	int over_count;		/* number of quantization noise > masking */
	int tot_count;		/* all */
	FLOAT8 over_noise;	/* sum of quantization noise > masking */
	FLOAT8 tot_noise;	/* sum of all quantization noise */
	FLOAT8 max_noise;	/* max quantization noise */
} calc_noise_result;

void compute_ath(lame_global_flags *gfp,FLOAT8 ATH_l[SBPSY_l],FLOAT8 ATH_s[SBPSY_l]);
void ms_convert(FLOAT8 xr[2][576],FLOAT8 xr_org[2][576]);

void outer_loop( 
    lame_global_flags *gfp,
    gr_info           *cod_info,
    int                ch, 
    int                bits,        /* maximum allowed bits */
    FLOAT8             xr[576],     /* magnitudes of spectral values */
    III_psy_xmin      *l3_xmin,     /* allowed distortion of the scalefactor */
    III_scalefac_t    *scalefac,    /* scalefactors */
    FLOAT8             xrpow[576],  /* coloured magnitudes of spectral values */
    int                l3_enc[576], /* vector of quantized values ix(0..575) */
    FLOAT8 noise[4]);



void iteration_init( lame_global_flags *gfp,III_side_info_t *l3_side, int l3_enc[2][2][576]);

void huffman_init( lame_global_flags *gfp );
int scale_bitcount( III_scalefac_t *scalefac, gr_info *cod_info);
int scale_bitcount_lsf( III_scalefac_t *scalefac, gr_info *cod_info);

int calc_xmin( lame_global_flags *gfp,FLOAT8 xr[576],
                III_psy_ratio *ratio,
	            gr_info *cod_info,
                III_psy_xmin *l3_xmin);

int calc_noise( lame_global_flags *gfp, FLOAT8 xr[576],
                 int ix[576],
                 gr_info *cod_info,
                 FLOAT8 xfsf[4][SBMAX_l], 
		 FLOAT8 distort[4][SBMAX_l],
                 III_psy_xmin *l3_xmin,
		 III_scalefac_t *,
                 calc_noise_result *);


#ifdef ASM_QUANTIZE
void quantize_xrpow_ASM( FLOAT8 xr[576],
               int  ix[576],
               int );

void quantize_xrpow_ISO_ASM( FLOAT8 xr[576],
               int  ix[576],
               int);
#else
void quantize_xrpow( FLOAT8 xr[576],
               int  ix[576],
               gr_info *cod_info );

void quantize_xrpow_ISO( FLOAT8 xr[576],
               int  ix[576],
               gr_info *cod_info );
#endif

int
new_choose_table( int ix[576],
		  unsigned int begin,
		  unsigned int end, int * s );

int count_bits(lame_global_flags *gfp,int  *ix, FLOAT8 xr[576], gr_info *cod_info);

void best_huffman_divide(lame_internal_flags *gfc, int gr, int ch, gr_info *cod_info, int *ix);

void best_scalefac_store(lame_global_flags *gfp,int gr, int ch,
			 int l3_enc[2][2][576],
			 III_side_info_t *l3_side,
			 III_scalefac_t scalefac[2][2]);

static const long LARGE_BITS = 100000;

#endif

