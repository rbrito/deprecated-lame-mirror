/*
 *	LAME MP3 encoding engine
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

/* $Id$ */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include "lame.h"
#include "set_get.h"
#include "util.h"
#include "bitstream.h"
#include "version.h"
#include "tables.h"
#include "quantize.h"
#include "psymodel.h"
#include "VbrTag.h"
#include "id3tag.h"
#include "quantize_pvt.h"

#if defined(__FreeBSD__) && !defined(__alpha__)
#include <floatingpoint.h>
#endif
#ifdef __riscos__
#include "asmstuff.h"
#endif

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

#ifdef __sun__
/* woraround for SunOS 4.x, it has SEEK_* defined here */
#include <unistd.h>
#endif


#define LAME_DEFAULT_QUALITY 5

/* set internal feature flags.  USER should not access these since
 * some combinations will produce strange results */
void
lame_init_qval(lame_global_flags * gfp)
{
    lame_internal_flags *gfc = gfp->internal_flags;

    switch (gfp->quality) {
    case 9:            /* no psymodel, no noise shaping */
        gfc->filter_type = 0;
        gfc->psymodel = 0;
        gfc->noise_shaping_amp = 0;
        gfc->use_best_huffman = 0;
        break;

    case 8:            /* use psymodel (for short block and m/s switching), but no noise shapping */
        gfc->filter_type = 0;
        gfc->psymodel = 1;
        gfc->noise_shaping_amp = 0;
        gfc->use_best_huffman = 0;
        break;

    case 7:	/* same as the default setting(-q 5) before LAME 3.70 */
        gfc->filter_type = 0;
        gfc->psymodel = 2;
	gfc->noise_shaping_amp = 0;
        gfc->use_best_huffman = 0;
        break;

    case 6:
        gfc->filter_type = 0;
        gfc->psymodel = 2;
	gfc->noise_shaping_amp = 1;
        gfc->use_best_huffman = 0;
        break;

    case 5: /* same as the default setting(-q 2) before LAME 3.83 */
	/* LAME4's new default */
        gfc->filter_type = 0;
        gfc->psymodel = 2;
        gfc->noise_shaping_amp = 1;
        gfc->use_best_huffman = 1;
        break;

    case 4:
        gfc->filter_type = 0;
        gfc->psymodel = 2;
        gfc->noise_shaping_amp = 2;
        gfc->use_best_huffman = 1;
        break;

    case 3:
        gfc->filter_type = 0;
        gfc->psymodel = 2;
        gfc->noise_shaping_amp = 3;
        gfc->use_best_huffman = 1;
        break;

    case 2:	/* aliased -h */
        gfc->filter_type = 0;
        gfc->psymodel = 2;
        gfc->noise_shaping_amp = 3;
        gfc->use_best_huffman = 2; /* inner loop, PAINFULLY SLOW */
        break;

    case 1:	/* same with -q 2 */
        gfc->filter_type = 0;
        gfc->psymodel = 2;
        gfc->noise_shaping_amp = 3; /* 4 not yet coded */
        gfc->use_best_huffman = 2;
        break;

    case 0:	/* same with -q 2 */
        gfc->filter_type = 0; /* 1 not yet coded */
        gfc->psymodel = 2;
        gfc->noise_shaping_amp = 3;
        gfc->use_best_huffman = 2;
        break;
    }
}






#define ABS(A) (((A)>0) ? (A) : -(A))

static int
FindNearestBitrate(
    int bRate,       /* legal rates from 32 to 448 */
    int version      /* MPEG-1 or MPEG-2 LSF */
    )
{
    int  bitrate = 0;
    int  i;
  
    for ( i = 1; i <= 14; i++ )
        if ( ABS (bitrate_table[version][i] - bRate) < ABS (bitrate - bRate) )
            bitrate = bitrate_table [version] [i];

    return bitrate;
}


/* convert samp freq in Hz to index */

static int
SmpFrqIndex (int sample_freq)
{
    switch ( sample_freq ) {
    case 44100: case 22050: case 11025:
	return  0;
    case 48000: case 24000: case 12000:
	return  1;
    case 32000: case 16000: case  8000:
	return  2;
    }
    return -1;
}


static int
get_bitrate(lame_global_flags * gfp)
{
    /* assume 16bit linear PCM input */
    return gfp->in_samplerate * 16 * gfp->internal_flags->channels_in
	/ (1.e3 * gfp->compression_ratio);
}

static void
set_compression_ratio(lame_global_flags * gfp)
{
    if (gfp->compression_ratio != 0)
	return;

    if (gfp->VBR == vbr) {
	FLOAT  cmp[] = { 5.7, 6.5, 7.3, 8.2, 10, 11.9, 13, 14, 15, 16.5 };
	if (gfp->VBR_q < 10)
	    gfp->compression_ratio = cmp[gfp->VBR_q];
	else
	    gfp->compression_ratio = cmp[9] + gfp->VBR_q-9;
    } else {
	gfp->compression_ratio
	    = gfp->in_samplerate * 16 * gfp->internal_flags->channels_in
	    / (1.e3 * gfp->mean_bitrate_kbps);
    }
}

/* Switch mappings for target bitrate */
struct {
    int    abr_kbps;
    int    large_scalefac;
    int    lowpass;
    double reduce_side;
    double scale;
    int ath_curve;
    int ath_lower;
    double interch;
    double shortthreshold;
    double istereo_ratio;
    double narrow_stereo;
} switch_map [] = {
    /*  scalefac_s lowpass       scale     athlower  short-th   narrow-st */
    /* kbps              reduceside   athcurve  inter-ch     is-ratio */
    {   8,  1,      2000,  0.7,   0.90, 11,  -4, 0.0012, 1e3, 0.5, 0.6},
    {  16,  1,      3700,  0.7,   0.90, 11,  -4, 0.0010, 1e3, 0.5, 0.6},
    {  24,  1,      3900,  0.7,   0.90, 11,  -4, 0.0010, 20., 0.5, 0.6},
    {  32,  1,      5500,  0.7,   0.90, 11,  -4, 0.0010, 20., 0.5, 0.6},
    {  40,  1,      7000,  0.7,   0.90, 11,  -3, 0.0009, 20., 0.5, 0.6},
    {  48,  1,      7500,  0.7,   0.90, 11,  -3, 0.0009, 20., 0.5, 0.6},
    {  56,  1,     10000,  0.7,   0.90, 11,  -3, 0.0008, 5.0, 0.5, 0.5},
    {  64,  1,     12000,  0.4,   0.90, 10,  -2, 0.0008, 3.0, 0.5, 0.4},
    {  80,  1,     14500,  0.4,   0.93,  8,  -2, 0.0007, 3.0, 0.4, 0.3},
    {  96,  1,     15300,  0.2,   0.93,  8,  -2, 0.0006, 2.5, 0.2, 0.2},
    { 112,  1,     16000,  0.2,   0.93,  7,  -2, 0.0005, 2.5, 0.1, 0.1},
    { 128,  1,     17500,  0.1,   0.93,  5,  -1, 0.0002, 2.5, 0.0, 0.0},
    { 160,  1,     18000,  0.0,   0.95,  4,   0, 0.0000, 1.8, 0.0, 0.0},
    { 192,  1,     19500,  0.0,   0.97,  3,   0, 0.0000, 1.8, 0.0, 0.0},
    { 224,  1,     20000,  0.0,   0.98,  2,   1, 0.0000, 1.8, 0.0, 0.0},
    { 256,  0,     20500,  0.0,   1.00,  1,   1, 0.0000, 1.8, 0.0, 0.0},
    { 320,  0,     21000,  0.0,   1.00,  0,   1, 0.0000, 1.8, 0.0, 0.0}
};

static int apply_preset(lame_global_flags*  gfp, int bitrate, vbr_mode mode)
{
    int lower_range, lower_range_kbps, upper_range, upper_range_kbps;
    int r, b;

    for (b = 1; b < sizeof(switch_map)/sizeof(switch_map[0])-1
	     && bitrate > switch_map[b].abr_kbps; b++)
	;

    upper_range_kbps = switch_map[b].abr_kbps;
    upper_range = b;
    lower_range_kbps = switch_map[b-1].abr_kbps;
    lower_range = (b-1);

    /* Determine which range the value specified is closer to */
    r = upper_range;
    if (upper_range_kbps - bitrate > bitrate - lower_range_kbps)
        r = lower_range;

    lame_set_VBR(gfp, mode);
    lame_set_VBR_mean_bitrate_kbps(gfp, bitrate);
    lame_set_brate(gfp, lame_get_VBR_mean_bitrate_kbps(gfp));

    if (mode != vbr) {
	lame_set_sfscale(gfp, switch_map[r].large_scalefac);
	lame_set_subblock_gain(gfp, switch_map[r].large_scalefac);
	/*
	 * ABR seems to have big problems with clipping, especially at
	 * low bitrates. so we compensate for that here by using a scale
	 * value depending on bitrate
	 */
	if (gfp->scale != 0.0)
	    (void) lame_set_scale( gfp, switch_map[r].scale );
    }

    lame_set_ATHcurve(gfp, switch_map[r].ath_curve);
    lame_set_ATHlower(gfp, (double)switch_map[r].ath_lower);

    if (gfp->lowpassfreq == 0)
	lame_set_lowpassfreq(gfp, switch_map[r].lowpass);

    if (gfp->mode == NOT_SET)
	gfp->mode = JOINT_STEREO;

    if (gfp->internal_flags->reduce_side < 0.0)
	lame_set_reduceSide( gfp, switch_map[r].reduce_side);

    if (gfp->interChRatio < 0)
	lame_set_interChRatio(gfp, switch_map[r].interch);

    if (gfp->internal_flags->nsPsy.attackthre < 0.0)
	lame_set_short_threshold(gfp, switch_map[r].shortthreshold,
				 switch_map[r].shortthreshold);

    if (gfp->internal_flags->istereo_ratio < 0.0)
	lame_set_istereoRatio(gfp, switch_map[r].istereo_ratio);

    if (gfp->internal_flags->narrowStereo < 0.0)
	lame_set_narrowenStereo(gfp, switch_map[r].narrow_stereo);

    return bitrate;
}


static int
optimum_samplefreq(int lowpassfreq, int input_samplefreq)
{
/*
 * Rules:
 *  - if possible, sfb21 should NOT be used
 *
 */
    int suggested_samplefreq = 48000;
    if (lowpassfreq <= 15960)
        suggested_samplefreq = 44100;
    if (lowpassfreq <= 15250)
        suggested_samplefreq = 32000;
    if (lowpassfreq <= 11220)
        suggested_samplefreq = 24000;
    if (lowpassfreq <= 9970)
        suggested_samplefreq = 22050;
    if (lowpassfreq <= 7230)
        suggested_samplefreq = 16000;
    if (lowpassfreq <= 5420)
        suggested_samplefreq = 12000;
    if (lowpassfreq <= 4510)
        suggested_samplefreq = 11025;
    if (lowpassfreq <= 3970)
        suggested_samplefreq = 8000;

    if (suggested_samplefreq > input_samplefreq)
	suggested_samplefreq = input_samplefreq;

    if (suggested_samplefreq <=  8000) return  8000;
    if (suggested_samplefreq <= 11025) return 11025;
    if (suggested_samplefreq <= 12000) return 12000;
    if (suggested_samplefreq <= 16000) return 16000;
    if (suggested_samplefreq <= 22050) return 22050;
    if (suggested_samplefreq <= 24000) return 24000;
    if (suggested_samplefreq <= 32000) return 32000;
    if (suggested_samplefreq <= 44100) return 44100;
    return 48000;
}








/********************************************************************
 *   initialize internal params based on data in gf
 *   (globalflags struct filled in by calling program)
 *
 *  OUTLINE:
 *
 * We first have some complex code to determine bitrate, 
 * output samplerate and mode.  It is complicated by the fact
 * that we allow the user to set some or all of these parameters,
 * and need to determine best possible values for the rest of them:
 *
 *  1. set some CPU related flags
 *  2. check if we are mono->mono, stereo->mono or stereo->stereo
 *  3.  compute bitrate and output samplerate:
 *          user may have set compression ratio
 *          user may have set a bitrate  
 *          user may have set a output samplerate
 *  4. set some options which depend on output samplerate
 *  5. compute the actual compression ratio
 *  6. set mode based on compression ratio
 *
 *  The remaining code is much simpler - it just sets options
 *  based on the mode & compression ratio: 
 *   
 *   set allow_diff_short based on mode
 *   select lowpass filter based on compression ratio & mode
 *   set the bitrate index, and min/max bitrates for VBR modes
 *   disable VBR tag if it is not appropriate
 *   initialize the bitstream
 *   initialize scalefac_band data
 *   set sideinfo_len (based on channels, CRC, out_samplerate)
 *   write an id3v2 tag into the bitstream
 *   write VBR tag into the bitstream
 *   set mpeg1/2 flag
 *   estimate the number of frames (based on a lot of data)
 *         
 *   now we set more flags:
 *   nspsytune:
 *      see code
 *   VBR modes
 *      see code      
 *   CBR/ABR
 *      see code   
 *
 *  Finally, we set the algorithm flags based on the gfp->quality value
 *  lame_init_qval(gfp);
 *
 ********************************************************************/
int
lame_init_params(lame_global_flags * const gfp)
{
    lame_internal_flags *gfc = gfp->internal_flags;
    int gr, ch;

    gfc->gfp = gfp;
    gfc->Class_ID = 0;

    /* report functions */
    gfc->report.msgf   = gfp->report.msgf;
    gfc->report.debugf = gfp->report.debugf;
    gfc->report.errorf = gfp->report.errorf;

#ifdef HAVE_NASM
    gfc->CPU_features.i387 = has_i387();
    if (gfp->asm_optimizations.amd3dnow ) {
        gfc->CPU_features.AMD_3DNow = has_3DNow();
        gfc->CPU_features.AMD_E3DNow = has_E3DNow();
    } else
        gfc->CPU_features.AMD_3DNow
	    = gfc->CPU_features.AMD_E3DNow = 0;

    if (gfp->asm_optimizations.mmx )
        gfc->CPU_features.MMX = has_MMX();
    else 
        gfc->CPU_features.MMX = 0;

    if (gfp->asm_optimizations.sse ) {
        gfc->CPU_features.SSE = has_SSE();
        gfc->CPU_features.SSE2 = has_SSE2();
    } else {
        gfc->CPU_features.SSE = 0;
        gfc->CPU_features.SSE2 = 0;
    }
#endif

    gfc->channels_in = gfp->num_channels;
    if (gfc->channels_in == 1)
        gfp->mode = MONO;
    gfc->channels_out = (gfp->mode == MONO) ? 1 : 2;
    gfc->mode_ext_next = gfc->mode_ext = MPG_MD_LR_LR;
    if (gfp->mode == MONO)
        gfp->force_ms = 0; // don't allow forced mid/side stereo for mono output
    if (gfp->VBR != cbr) {
	gfp->free_format = 0; /* VBR can't be mixed with free format */
	/* at 160 kbps (MPEG-2/2.5)/ 320 kbps (MPEG-1) only
	   Free format or CBR are possible, no VBR */
	if (gfp->mean_bitrate_kbps > 160 * gfc->channels_out) {
	    gfp->VBR = cbr;
	    gfp->free_format = 1;
	}
    }
    if (gfp->VBR != vbr && gfp->mean_bitrate_kbps == 0) {
	/* no bitrate or compression ratio specified,
	   use default compression ratio of 11.025,
	   which is the rate to compress a CD down to exactly 128000 bps */
	if (gfp->compression_ratio <= 0)
	    gfp->compression_ratio = 11.025;

	/* choose a bitrate which achieves the specified compression ratio
	 */
	gfp->mean_bitrate_kbps = get_bitrate(gfp);
    }
    set_compression_ratio(gfp);

    /* if a filter has not been enabled, see if we should add one */
    apply_preset(gfp, get_bitrate(gfp), gfp->VBR);

    /* output sampling rate is determined by the lowpass value */
    if (gfp->out_samplerate == 0) {
	if (gfp->lowpassfreq < 0)
	    gfp->out_samplerate
		= optimum_samplefreq(gfp->in_samplerate/2.0,
				     gfp->in_samplerate);
	else
	    gfp->out_samplerate
		= optimum_samplefreq(gfp->lowpassfreq, gfp->in_samplerate);
    }
    gfc->samplerate_index = SmpFrqIndex(gfp->out_samplerate);
    if (gfc->samplerate_index < 0)
        return -1;

    /* apply user driven high pass filter */
    if (gfp->highpassfreq > 0) {
        gfc->highpass1 = 2. * gfp->highpassfreq;

        if (gfp->highpasswidth >= 0)
            gfc->highpass2 = 2. * (gfp->highpassfreq + gfp->highpasswidth);
        else            /* 0% above on default */
            gfc->highpass2 = (1 + 0.00) * 2. * gfp->highpassfreq;

	gfc->highpass1 /= gfp->out_samplerate;
	gfc->highpass2 /= gfp->out_samplerate;
    }

    /* apply user driven low pass filter */
    if (gfp->lowpassfreq > 0) {
	gfc->lowpass2 = 2. * gfp->lowpassfreq;
        if (gfp->lowpasswidth >= 0) {
            gfc->lowpass1 = 2. * (gfp->lowpassfreq - gfp->lowpasswidth);
            if (gfc->lowpass1 < 0)
                gfc->lowpass1 = 0;
        }
        else          /* 0% below on default */
	    gfc->lowpass1 = (1 - 0.00) * 2. * gfp->lowpassfreq;

	gfc->lowpass1 /= gfp->out_samplerate;
	gfc->lowpass2 /= gfp->out_samplerate;
    }

    gfc->scale_bitcounter = scale_bitcount;
    gfp->version = 1;
    gfc->mode_gr = 2;
    if (gfp->out_samplerate <= 24000) {
	gfp->version = 0;
	gfc->mode_gr = 1;
	gfc->scale_bitcounter = scale_bitcount_lsf;
    }
    gfp->framesize = 576 * gfc->mode_gr;
    gfp->encoder_delay = ENCDELAY;
    gfc->resample_ratio = (double) gfp->in_samplerate / gfp->out_samplerate;
    if (.9999 < gfc->resample_ratio && gfc->resample_ratio < 1.0001)
	gfc->resample_ratio = 1.0;

    /*******************************************************
     * bitrate index
     *******************************************************/
    /* for non Free Format find the nearest allowed bitrate */
    gfc->bitrate_index = 0;
    if (gfp->VBR == cbr && !gfp->free_format) {
	gfp->mean_bitrate_kbps
	    = FindNearestBitrate(gfp->mean_bitrate_kbps, gfp->version);
	gfc->bitrate_index
	    = BitrateIndex(gfp->mean_bitrate_kbps, gfp->version);
	if (gfc->bitrate_index < 0)
	    return -1;
    }
    if (gfp->VBR != cbr) {
	/* choose a min/max bitrate for VBR */
        /* if the user didn't specify VBR_max_bitrate: */
        gfc->VBR_min_bitrate = 1;
        gfc->VBR_max_bitrate = 14;
	if (gfp->mode == MONO)
	    gfc->VBR_max_bitrate--;

        if (gfp->VBR_min_bitrate_kbps
	    && (gfc->VBR_min_bitrate
		= BitrateIndex(gfp->VBR_min_bitrate_kbps, gfp->version)) < 0)
	    return -1;

	gfp->VBR_min_bitrate_kbps
	    = bitrate_table[gfp->version][gfc->VBR_min_bitrate];
        if (gfp->mean_bitrate_kbps < gfp->VBR_min_bitrate_kbps)
	    gfp->mean_bitrate_kbps = gfp->VBR_min_bitrate_kbps;

        if (gfp->VBR_max_bitrate_kbps
	    && (gfc->VBR_max_bitrate
		= BitrateIndex(gfp->VBR_max_bitrate_kbps, gfp->version)) < 0)
	    return -1;

	gfp->VBR_max_bitrate_kbps
	    = bitrate_table[gfp->version][gfc->VBR_max_bitrate];
        if (gfp->mean_bitrate_kbps > gfp->VBR_max_bitrate_kbps)
	    gfp->mean_bitrate_kbps = gfp->VBR_max_bitrate_kbps;
    }

#ifndef NOANALYSIS
    /* some file options not allowed if output is: not specified or stdout */
    if (!gfc->pinfo)
	gfp->bWriteVbrTag = 0; /* disable Xing VBR tag */
#endif
    init_bit_stream_w(gfp);

    gfc->Class_ID = LAME_ID;

    if (gfp->quality < 0)
	gfp->quality = LAME_DEFAULT_QUALITY;

    if (gfp->VBR == vbr && gfp->quality != 5) {
	gfp->quality = 5;
	ERRORF(gfc, "VBR mode with -q setting is meaningless.\n");
    }

    if (gfc->noise_shaping_amp > 2)
	gfc->noise_shaping_amp = 2;

    /* initialize internal qval settings */
    lame_init_qval(gfp);

    /* initialize internal adaptive ATH settings  -jd */
    gfc->ATH.aa_sensitivity_p = db2pow(-gfp->athaa_sensitivity);

//    if (gfp->useTemporal < 0 ) gfp->useTemporal = 1;  // on by default

    lame_init_bitstream(gfp);
    iteration_init(gfp);
    psymodel_init(gfp);

    /* scaling */
    if (gfp->scale == 0.0)
	gfp->scale = 1.0;

    if (gfp->scale_left == 0.0)
	gfp->scale_left = 1.0;

    if (gfp->scale_right == 0.0)
	gfp->scale_right = 1.0;

    gfp->scale_left  *= gfp->scale;
    gfp->scale_right *= gfp->scale;

    if (!gfc->psymodel) {
	gfc->ATH.adjust = 1.0;	/* no adjustment */
	gfc->mode_ext = MPG_MD_LR_LR;
	if (gfp->mode == JOINT_STEREO)
	    gfc->mode_ext = MPG_MD_MS_LR;
	gfp->disable_reservoir = 1;
	gfc->substep_shaping = 0;
    }

    for (gr=0; gr < gfc->mode_gr ; gr++) {
	for ( ch = 0; ch < gfc->channels_out; ch++ ) {
	    gfc->l3_side.tt[gr][ch].block_type=NORM_TYPE;
	    gfc->l3_side.tt[gr][ch].mixed_block_flag=0;
	}
    }

    return 0;
}

/*
 *  print_config
 *
 *  Prints some selected information about the coding parameters via 
 *  the macro command MSGF(), which is currently mapped to lame_errorf 
 *  (reports via a error function?), which is a printf-like function 
 *  for <stderr>.
 */

void
lame_print_config(const lame_global_flags * gfp)
{
    lame_internal_flags *gfc = gfp->internal_flags;
    double  out_samplerate = gfp->out_samplerate;
    double  in_samplerate = gfp->in_samplerate;

#ifdef HAVE_NASM
    if (gfc->CPU_features.MMX
	|| gfc->CPU_features.AMD_3DNow || gfc->CPU_features.AMD_E3DNow
        || gfc->CPU_features.SSE || gfc->CPU_features.SSE2) {
        MSGF(gfc, "CPU features:");

        if (gfc->CPU_features.i387)
            MSGF(gfc, " i387");
        if (gfc->CPU_features.MMX)
            MSGF(gfc, ", MMX (ASM used)");
        if (gfc->CPU_features.AMD_3DNow)
            MSGF(gfc, ", 3DNow! (ASM used)");
        if (gfc->CPU_features.AMD_E3DNow)
            MSGF(gfc, ", 3DNow!");
        if (gfc->CPU_features.SSE)
            MSGF(gfc, ", SSE (ASM used)");
        if (gfc->CPU_features.SSE2)
            MSGF(gfc, ", SSE2");
        MSGF(gfc, "\n");
    }
#endif

    if (gfp->num_channels == 2 && gfc->channels_out == 1 /* mono */ ) {
        MSGF(gfc,
             "Autoconverting from stereo to mono. Setting encoding to mono mode.\n");
    }

    if (gfc->resample_ratio != 1.) {
        MSGF(gfc, "Resampling:  input %g kHz  output %g kHz\n",
             1.e-3 * in_samplerate, 1.e-3 * out_samplerate);
    }

    if (gfc->filter_type == 0) {
        if (gfc->highpass2 > 0.)
            MSGF
                (gfc,
                 "Using polyphase highpass filter, transition band: %5.0f Hz - %5.0f Hz\n",
                 0.5 * gfc->highpass1 * out_samplerate,
                 0.5 * gfc->highpass2 * out_samplerate);
        if (gfc->lowpass1 > 0.) {
            MSGF
                (gfc,
                 "Using polyphase lowpass  filter, transition band: %5.0f Hz - %5.0f Hz\n",
                 0.5 * gfc->lowpass1 * out_samplerate,
                 0.5 * gfc->lowpass2 * out_samplerate);
        }
        else {
            MSGF(gfc, "polyphase lowpass filter disabled\n");
        }
    }
    else {
        MSGF(gfc, "polyphase filters disabled\n");
    }

    if (gfp->free_format) {
        MSGF(gfc,
             "Warning: many decoders cannot handle free format bitstreams\n");
        if (gfp->mean_bitrate_kbps > 320) {
            MSGF(gfc,
                 "Warning: many decoders cannot handle free format bitrates >320 kbps (see documentation)\n");
        }
    }
}


/**     rh:
 *      some pretty printing is very welcome at this point!
 *      so, if someone is willing to do so, please do it!
 *      add more, if you see more...
 */
void 
lame_print_internals( const lame_global_flags * gfp )
{
    lame_internal_flags *gfc = gfp->internal_flags;
    const char * pc = "";
    FLOAT bass, alto, treble, sfb21;
    int i;

    /*  compiler/processor optimizations, operational, etc.
     */
    MSGF( gfc, "\nmisc:\n\n" );
    
    MSGF( gfc, "\tscaling: \n");
    MSGF( gfc, "\t ^ ch0 (left) : %f\n", gfp->scale_left );
    MSGF( gfc, "\t ^ ch1 (right): %f\n", gfp->scale_right );
    MSGF( gfc, "\tfilter type: %d\n", gfc->filter_type );
    MSGF( gfc, "\texperimental X=%d Y=%d Z=%d\n",
	  gfp->experimentalX, gfp->experimentalY, gfp->experimentalZ );
    MSGF( gfc, "\t...\n" );

    /*  everything controlling the stream format 
     */
    MSGF( gfc, "\nstream format:\n\n" );
    switch ( gfp->version ) {
    case 0:  pc = "2.5"; break;
    case 1:  pc = "1";   break;
    case 2:  pc = "2";   break;
    default: pc = "?";   break;
    }
    MSGF( gfc, "\tMPEG-%s Layer 3\n", pc );
    switch ( gfp->mode ) {
    case JOINT_STEREO: pc = "joint stereo";   break;
    case STEREO      : pc = "stereo";         break;
    case DUAL_CHANNEL: pc = "dual channel";   break;
    case MONO        : pc = "mono";           break;
    case NOT_SET     : pc = "not set (error)"; break;
    default          : pc = "unknown (error)"; break;
    }
    MSGF( gfc, "\t%d channel - %s\n", gfc->channels_out, pc );
    if (gfp->use_istereo)
	MSGF(gfc, "\t ^ using intensity stereo, the ratio is %f\n",
	     1.0-gfc->istereo_ratio);

    if (gfp->free_format)    pc = "(free format)";
    else pc = "";
    switch ( gfp->VBR ) {
    case cbr : MSGF( gfc, "\tconstant bitrate - CBR %s\n", pc ); break;
    case abr : MSGF( gfc, "\tvariable bitrate - ABR %s\n", pc ); break;
    case vbr : MSGF( gfc, "\tvariable bitrate - VBR\n"        ); break;
    default  : MSGF( gfc, "\t ?? oops, some new one ?? \n"    ); break;
    }
    if (gfp->bWriteVbrTag) 
    MSGF( gfc, "\tusing LAME Tag\n" );
    MSGF( gfc, "\t...\n" );

    /*  everything controlling psychoacoustic settings, like ATH, etc.
     */
    MSGF( gfc, "\npsychoacoustic:\n\n" );
    MSGF( gfc, "\tshort block switching threshold: %f %f\n",
	  gfc->nsPsy.attackthre, gfc->nsPsy.attackthre_s );
    MSGF( gfc, "\tpsymodel: %s\n", gfc->psymodel ? "used" : "not used");
    
    pc = "using";
    if ( gfp->ATHshort ) pc = "the only masking for short blocks";
    if ( gfp->ATHonly  ) pc = "the only masking";
    if ( gfp->noATH    ) pc = "not used";
    MSGF( gfc, "\tATH: %s\n", pc );
    MSGF( gfc, "\t ^ shape: %g\n", gfp->ATHcurve);
    MSGF( gfc, "\t ^ level adjustement: %f (dB)\n", gfp->ATHlower );
    MSGF( gfc, "\t ^ adjust sensitivity power (dB): %f\n", gfp->athaa_sensitivity);

    i = (gfp->exp_nspsytune >> 2) & 63;
    if (i >= 32)
	i -= 64;
    bass = i*0.25;

    i = (gfp->exp_nspsytune >> 8) & 63;
    if (i >= 32)
	i -= 64;
    alto = i*0.25;

    i = (gfp->exp_nspsytune >> 14) & 63;
    if (i >= 32)
	i -= 64;
    treble = i*0.25;

    /*  to be compatible with Naoki's original code, the next 6 bits
     *  define only the amount of changing treble for sfb21 */
    i = (gfp->exp_nspsytune >> 20) & 63;
    if (i >= 32)
	i -= 64;
    sfb21 = treble + i*0.25;

    MSGF(gfc, "\tadjust masking: %f dB\n", gfp->VBR_q-4.0 );
    MSGF(gfc, "\t ^ bass=%g dB, alto=%g dB, treble=%g dB, sfb21=%g dB\n",
	 bass, alto, treble, sfb21);

    pc = gfp->useTemporal ? "yes" : "no";
    MSGF( gfc, "\tusing temporal masking effect: %s\n", pc );
    MSGF( gfc, "\tinterchannel masking ratio: %f\n", gfp->interChRatio );
    MSGF( gfc, "\treduce side channel PE factor: %f\n", 1.0-gfc->reduce_side);
    MSGF( gfc, "\tnarrowen stereo factor: %f\n", gfc->narrowStereo*2.0);
    MSGF( gfc, "\t...\n" );

    MSGF( gfc, "\nnoisechaping & quantization:\n\n" );
    switch( gfc->use_best_huffman ) {
    default: pc = "normal"; break;
    case  1: pc = "best (outside loop)"; break;
    case  2: pc = "best (inside loop, slow)"; break;
    }
    MSGF( gfc, "\thuffman search: %s\n", pc );
    if (gfp->VBR != vbr) {
	MSGF( gfc, "\tnoise shaping: %s\n", gfc->psymodel > 1 ? "used" : "not used");
	MSGF( gfc, "\t ^ amplification: %d\n", gfc->noise_shaping_amp );
	MSGF( gfc, "\tallow large scalefactor range=%s\n",
	      gfc->use_scalefac_scale ? "yes" : "no");
	MSGF( gfc, "\tuse subblock gain=%s\n",
	      gfc->use_subblock_gain ? "yes" : "no");
    }
    MSGF( gfc, "\tsubstep shaping=short blocks: %s, long blocks: %s\n",
	  gfc->substep_shaping&2 ? "yes" : "no",
	  gfc->substep_shaping&1 ? "yes" : "no" );
    MSGF( gfc, "\t...\n" );

    /*  that's all ?
     */
    MSGF( gfc, "\n" );
    return;
}



/* routine to feed exactly one frame (gfp->framesize) worth of data to the 
encoding engine.  All buffering, resampling, etc, handled by calling
program.  
*/
int
lame_encode_frame(lame_global_flags * gfp,
                  sample_t inbuf_l[], sample_t inbuf_r[],
                  unsigned char *mp3buf, int mp3buf_size)
{
    int     ret;
    ret = lame_encode_mp3_frame(gfp, inbuf_l, inbuf_r, mp3buf, mp3buf_size);
    gfp->frameNum++;
    return ret;
}

/*
 * THE MAIN LAME ENCODING INTERFACE
 * mt 3/00
 *
 * input pcm data, output (maybe) mp3 frames.
 * This routine handles all buffering, resampling and filtering for you.
 * The required mp3buffer_size can be computed from num_samples,
 * samplerate and encoding rate, but here is a worst case estimate:
 *
 * mp3buffer_size in bytes = 1.25*num_samples + 7200
 *
 * return code = number of bytes output in mp3buffer.  can be 0
 *
 * NOTE: this routine uses LAME's internal PCM data representation,
 * 'sample_t'.  It should not be used by any application.  
 * applications should use lame_encode_buffer(), 
 *                         lame_encode_buffer_float()
 *                         lame_encode_buffer_int()
 * etc... depending on what type of data they are working with.  
 */
int
lame_encode_buffer_sample_t(
    lame_global_flags * gfp,
    sample_t buffer_l[],
    sample_t buffer_r[],
    int nsamples,
    unsigned char *mp3buf,
    const int mp3buf_size
    )
{
    lame_internal_flags *gfc = gfp->internal_flags;
    int mp3size = 0, ret, i, ch, mf_needed;
    int mp3out;
    sample_t *mfbuf[2];
    sample_t *in_buffer[2];

    if (gfc->Class_ID != LAME_ID)
        return -3;

    if (nsamples == 0)
        return 0;

    /* copy out any tags that may have been written into bitstream */
    mp3out = copy_buffer(gfc,mp3buf,mp3buf_size,0);
    if (mp3out<0) return mp3out;  // not enough buffer space
    mp3buf += mp3out;
    mp3size += mp3out;


    in_buffer[0]=buffer_l;
    in_buffer[1]=buffer_r;  


    /* Apply user defined re-scaling */
    /* user selected scaling of the channel 0 (left) samples */
    if (gfp->scale_left != 1.0)
	for (i=0 ; i<nsamples; ++i)
	    in_buffer[0][i] *= gfp->scale_left;

    /* user selected scaling of the channel 1 (right) samples */
    if (gfp->scale_right != 1.0)
	for (i=0 ; i<nsamples; ++i)
	    in_buffer[1][i] *= gfp->scale_right;

    /* Downsample to Mono if 2 channels in and 1 channel out */
    if (gfp->num_channels == 2 && gfc->channels_out == 1) {
	for (i=0; i<nsamples; ++i) {
	    in_buffer[0][i] =
		0.5f * ((FLOAT) in_buffer[0][i] + in_buffer[1][i]);
		in_buffer[1][i] = 0.0;
	}
    }

    /* some sanity checks */
#if ENCDELAY < MDCTDELAY
# error ENCDELAY is less than MDCTDELAY, see encoder.h
#endif
#if FFTOFFSET > BLKSIZE
# error FFTOFFSET is greater than BLKSIZE, see encoder.h
#endif

    mf_needed = BLKSIZE + gfp->framesize - FFTOFFSET + 1152; /* amount needed for FFT */
    mf_needed = Max(mf_needed, 480 + gfp->framesize + 1152); /* amount needed for MDCT/filterbank */
    assert(MFSIZE >= mf_needed);

    mfbuf[0] = gfc->mfbuf[0];
    mfbuf[1] = gfc->mfbuf[1];

    while (nsamples > 0) {
        int     n_in = 0;    /* number of input samples processed with fill_buffer */
        int     n_out = 0;   /* number of samples output with fill_buffer */
        /* n_in <> n_out if we are resampling */

        /* copy in new samples into mfbuf, with resampling */
        fill_buffer(gfp, mfbuf, in_buffer, nsamples, &n_in, &n_out);

        /* update in_buffer counters */
        nsamples -= n_in;
        in_buffer[0] += n_in;
        if (gfc->channels_out == 2)
            in_buffer[1] += n_in;

        /* update mfbuf[] counters */
        gfc->mf_size += n_out;
        assert(gfc->mf_size <= MFSIZE);
        gfc->mf_samples_to_encode += n_out;


        if (gfc->mf_size >= mf_needed) {
            /* encode the frame.  */
            // mp3buf              = pointer to current location in buffer
            // mp3buf_size         = size of original mp3 output buffer
            //                     = 0 if we should not worry about the
            //                       buffer size because calling program is 
            //                       to lazy to compute it
            // mp3size             = size of data written to buffer so far
            // mp3buf_size-mp3size = amount of space avalable 

            int buf_size=mp3buf_size - mp3size;
            if (mp3buf_size==0) buf_size=0;

            ret =
                lame_encode_frame(gfp, mfbuf[0], mfbuf[1], mp3buf,buf_size);

            if (ret < 0) goto retr;
            mp3buf += ret;
            mp3size += ret;

            /* shift out old samples */
            gfc->mf_size -= gfp->framesize;
            gfc->mf_samples_to_encode -= gfp->framesize;
            for (ch = 0; ch < gfc->channels_out; ch++)
                for (i = 0; i < gfc->mf_size; i++)
                    mfbuf[ch][i] = mfbuf[ch][i + gfp->framesize];
        }
    }
    assert(nsamples == 0);
    ret = mp3size;

  retr:
    return ret;
}


int
lame_encode_buffer(lame_global_flags * gfp,
                   const short int buffer_l[],
                   const short int buffer_r[],
                   const int nsamples, unsigned char *mp3buf, const int mp3buf_size)
{
    lame_internal_flags *gfc = gfp->internal_flags;
    int     ret, i;
    sample_t *in_buffer[2];

    if (gfc->Class_ID != LAME_ID)
        return -3;

    if (nsamples == 0)
        return 0;

    in_buffer[0] = calloc(sizeof(sample_t), nsamples);
    in_buffer[1] = calloc(sizeof(sample_t), nsamples);

    if (!in_buffer[0] || !in_buffer[1]) {
        ERRORF(gfc, "Error: can't allocate in_buffer buffer\n");
        return -2;
    }

    /* make a copy of input buffer, changing type to sample_t */
    for (i = 0; i < nsamples; i++) {
        in_buffer[0][i] = buffer_l[i];
	if (gfc->channels_in>1) in_buffer[1][i] = buffer_r[i];
    }

    ret = lame_encode_buffer_sample_t(gfp,in_buffer[0],in_buffer[1],
				      nsamples, mp3buf, mp3buf_size);
    
    free(in_buffer[0]);
    free(in_buffer[1]);
    return ret;
}


int
lame_encode_buffer_float(lame_global_flags * gfp,
                   const float buffer_l[],
                   const float buffer_r[],
                   const int nsamples, unsigned char *mp3buf, const int mp3buf_size)
{
    lame_internal_flags *gfc = gfp->internal_flags;
    int     ret, i;
    sample_t *in_buffer[2];

    if (gfc->Class_ID != LAME_ID)
        return -3;

    if (nsamples == 0)
        return 0;

    in_buffer[0] = calloc(sizeof(sample_t), nsamples);
    in_buffer[1] = calloc(sizeof(sample_t), nsamples);

    if (!in_buffer[0] || !in_buffer[1]) {
        ERRORF(gfc, "Error: can't allocate in_buffer buffer\n");
        return -2;
    }

    /* make a copy of input buffer, changing type to sample_t */
    for (i = 0; i < nsamples; i++) {
        in_buffer[0][i] = buffer_l[i];
        if (gfc->channels_in>1) in_buffer[1][i] = buffer_r[i];
    }

    ret = lame_encode_buffer_sample_t(gfp,in_buffer[0],in_buffer[1],
				      nsamples, mp3buf, mp3buf_size);
    
    free(in_buffer[0]);
    free(in_buffer[1]);
    return ret;
}



int
lame_encode_buffer_int(lame_global_flags * gfp,
                   const int buffer_l[],
                   const int buffer_r[],
                   const int nsamples, unsigned char *mp3buf, const int mp3buf_size)
{
    lame_internal_flags *gfc = gfp->internal_flags;
    int     ret, i;
    sample_t *in_buffer[2];

    if (gfc->Class_ID != LAME_ID)
        return -3;

    if (nsamples == 0)
        return 0;

    in_buffer[0] = calloc(sizeof(sample_t), nsamples);
    in_buffer[1] = calloc(sizeof(sample_t), nsamples);

    if (!in_buffer[0] || !in_buffer[1]) {
        ERRORF(gfc, "Error: can't allocate in_buffer buffer\n");
        return -2;
    }

    /* make a copy of input buffer, changing type to sample_t */
    for (i = 0; i < nsamples; i++) {
                                /* internal code expects +/- 32768.0 */
      in_buffer[0][i] = buffer_l[i] * (1.0 / ( 1L << (8 * sizeof(int) - 16)));
      if (gfc->channels_in>1)
	  in_buffer[1][i] = buffer_r[i] * (1.0 / ( 1L << (8 * sizeof(int) - 16)));
    }

    ret = lame_encode_buffer_sample_t(gfp,in_buffer[0],in_buffer[1],
				      nsamples, mp3buf, mp3buf_size);
    
    free(in_buffer[0]);
    free(in_buffer[1]);
    return ret;

}




int
lame_encode_buffer_long2(lame_global_flags * gfp,
                   const long buffer_l[],
                   const long buffer_r[],
                   const int nsamples, unsigned char *mp3buf, const int mp3buf_size)
{
    lame_internal_flags *gfc = gfp->internal_flags;
    int     ret, i;
    sample_t *in_buffer[2];

    if (gfc->Class_ID != LAME_ID)
        return -3;

    if (nsamples == 0)
        return 0;

    in_buffer[0] = calloc(sizeof(sample_t), nsamples);
    in_buffer[1] = calloc(sizeof(sample_t), nsamples);

    if (!in_buffer[0] || !in_buffer[1]) {
        ERRORF(gfc, "Error: can't allocate in_buffer buffer\n");
        return -2;
    }

    /* make a copy of input buffer, changing type to sample_t */
    for (i = 0; i < nsamples; i++) {
                                /* internal code expects +/- 32768.0 */
      in_buffer[0][i] = buffer_l[i] * (1.0 / (1L << (8 * sizeof(long) - 16)));
      if (gfc->channels_in>1)
	  in_buffer[1][i] = buffer_r[i] * (1.0 / (1L << (8 * sizeof(long) - 16)));
    }

    ret = lame_encode_buffer_sample_t(gfp,in_buffer[0],in_buffer[1],
				      nsamples, mp3buf, mp3buf_size);
    
    free(in_buffer[0]);
    free(in_buffer[1]);
    return ret;

}



int
lame_encode_buffer_long(lame_global_flags * gfp,
                   const long buffer_l[],
                   const long buffer_r[],
                   const int nsamples, unsigned char *mp3buf, const int mp3buf_size)
{
    lame_internal_flags *gfc = gfp->internal_flags;
    int     ret, i;
    sample_t *in_buffer[2];

    if (gfc->Class_ID != LAME_ID)
        return -3;

    if (nsamples == 0)
        return 0;

    in_buffer[0] = calloc(sizeof(sample_t), nsamples);
    in_buffer[1] = calloc(sizeof(sample_t), nsamples);

    if (!in_buffer[0] || !in_buffer[1]) {
        ERRORF(gfc, "Error: can't allocate in_buffer buffer\n");
        return -2;
    }

    /* make a copy of input buffer, changing type to sample_t */
    for (i = 0; i < nsamples; i++) {
        in_buffer[0][i] = buffer_l[i];
        if (gfc->channels_in>1)
	    in_buffer[1][i] = buffer_r[i];
    }

    ret = lame_encode_buffer_sample_t(gfp,in_buffer[0],in_buffer[1],
				      nsamples, mp3buf, mp3buf_size);
    
    free(in_buffer[0]);
    free(in_buffer[1]);
    return ret;
}











int
lame_encode_buffer_interleaved(lame_global_flags * gfp,
                               short int buffer[],
                               int nsamples,
                               unsigned char *mp3buf, int mp3buf_size)
{
    int     ret, i;
    sample_t *buffer_l;
    sample_t *buffer_r;

    buffer_l = calloc(sizeof(sample_t), nsamples);
    buffer_r = calloc(sizeof(sample_t), nsamples);
    if (!buffer_l || !buffer_r) {
        return -2;
    }
    for (i = 0; i < nsamples; i++) {
        buffer_l[i] = buffer[2 * i];
        buffer_r[i] = buffer[2 * i + 1];
    }
    ret =
        lame_encode_buffer_sample_t(gfp, buffer_l, buffer_r, nsamples, mp3buf,
                           mp3buf_size);
    free(buffer_l);
    free(buffer_r);
    return ret;

}


int
lame_encode(lame_global_flags * const gfp,
            const short int in_buffer[2][1152],
            unsigned char *const mp3buf, const int size)
{
    lame_internal_flags *gfc = gfp->internal_flags;

    if (gfc->Class_ID != LAME_ID)
        return -3;

    return lame_encode_buffer(gfp, in_buffer[0], in_buffer[1], gfp->framesize,
                              mp3buf, size);
}

/*****************************************************************
 Flush mp3 buffer, pad with ancillary data so last frame is complete.
 Reset reservoir size to 0                  
 but keep all PCM samples and MDCT data in memory             
 This option is used to break a large file into several mp3 files 
 that when concatenated together will decode with no gaps         
 Because we set the reservoir=0, they will also decode seperately 
 with no errors. 
*********************************************************************/
int
lame_encode_flush_nogap(lame_global_flags * gfp,
                  unsigned char *mp3buffer, int mp3buffer_size)
{
    return flush_bitstream(gfp,  mp3buffer, mp3buffer_size, 1);
}


/* called by lame_init_params.  You can also call this after flush_nogap 
   if you want to write new id3v2 and Xing VBR tags into the bitstream */
int
lame_init_bitstream(lame_global_flags * gfp)
{
#ifdef BRHIST
    lame_internal_flags *gfc = gfp->internal_flags;
    /* initialize histogram data optionally used by frontend */
    memset(gfc->bitrate_stereoMode_Hist, 0,
	   sizeof(gfc->bitrate_stereoMode_Hist));
    memset(gfc->bitrate_blockType_Hist, 0,
	   sizeof(gfc->bitrate_blockType_Hist));
#endif
    /* Write initial VBR Header to bitstream and init VBR data */
    if (gfp->bWriteVbrTag)
        InitVbrTag(gfp);

    gfp->frameNum=0;

    return 0;
}


/*****************************************************************/
/* flush internal PCM sample buffers, then mp3 buffers           */
/* then write id3 v1 tags into bitstream.                        */
/*****************************************************************/

int
lame_encode_flush(lame_global_flags * gfp,
                  unsigned char *mp3buffer, int mp3buffer_size)
{
    lame_internal_flags *gfc = gfp->internal_flags;
    short int buffer[2][1152];
    int     imp3 = 0, mp3count, mp3buffer_size_remaining;

    /* we always add POSTDELAY=288 padding to make sure granule with real
     * data can be complety decoded (because of 50% overlap with next granule */
    int end_padding=POSTDELAY;  

    memset(buffer, 0, sizeof(buffer));
    mp3count = 0;


    while (gfc->mf_samples_to_encode > 0) {

        mp3buffer_size_remaining = mp3buffer_size - mp3count;

        /* if user specifed buffer size = 0, dont check size */
        if (mp3buffer_size == 0)
            mp3buffer_size_remaining = 0;

        /* send in a frame of 0 padding until all internal sample buffers
         * are flushed 
         */
        imp3 = lame_encode_buffer(gfp, buffer[0], buffer[1], gfp->framesize,
                                  mp3buffer, mp3buffer_size_remaining);

        /* don't count the above padding: */
        gfc->mf_samples_to_encode -= gfp->framesize;
        if (gfc->mf_samples_to_encode < 0) {
            /* we added extra padding to the end */
            end_padding += -gfc->mf_samples_to_encode;  
        }


        if (imp3 < 0) {
            /* some type of fatal error */
            return imp3;
        }
        mp3buffer += imp3;
        mp3count += imp3;
    }

    mp3buffer_size_remaining = mp3buffer_size - mp3count;
    /* if user specifed buffer size = 0, dont check size */
    if (mp3buffer_size == 0)
        mp3buffer_size_remaining = 0;

    /* mp3 related stuff.  bit buffer might still contain some mp3 data */
    imp3 = flush_bitstream(gfp, mp3buffer, mp3buffer_size_remaining, 1);
    if (imp3 < 0) {
	/* some type of fatal error */
	return imp3;
    }
    mp3buffer += imp3;
    mp3count += imp3;
    mp3buffer_size_remaining = mp3buffer_size - mp3count;
    /* if user specifed buffer size = 0, dont check size */
    if (mp3buffer_size == 0)
	mp3buffer_size_remaining = 0;

    /* write a id3 tag to the bitstream */
    id3tag_write_v1(gfp);
    imp3 = copy_buffer(gfc,mp3buffer, mp3buffer_size_remaining, 0);

    if (imp3 < 0) {
        return imp3;
    }
    mp3count += imp3;
    gfp->encoder_padding=end_padding;
    return mp3count;
}

/***********************************************************************
 *
 *      lame_close ()
 *
 *  frees internal buffers
 *
 ***********************************************************************/

int
lame_close(lame_global_flags * gfp)
{
    lame_internal_flags *gfc = gfp->internal_flags;

    if (gfc->Class_ID != LAME_ID)
        return -3;

    gfc->Class_ID = 0;

    // this routien will free all malloc'd data in gfc, and then free gfc:
    freegfc(gfc);
    free((lame_internal_flags*)((int)gfc - gfc->alignment));

    gfp->internal_flags = NULL;

    if (gfp->lame_allocated_gfp) {
        gfp->lame_allocated_gfp = 0;
        free(gfp);
    }

    return 0;
}

/*****************************************************************/
/* flush internal mp3 buffers, and free internal buffers         */
/*****************************************************************/

int
lame_encode_finish(lame_global_flags * gfp,
                   unsigned char *mp3buffer, int mp3buffer_size)
{
    int     ret = lame_encode_flush(gfp, mp3buffer, mp3buffer_size);

    lame_close(gfp);

    return ret;
}

/*****************************************************************/
/* write VBR Xing header, and ID3 version 1 tag, if asked for    */
/*****************************************************************/
void
lame_mp3_tags_fid(lame_global_flags * gfp, FILE * fpStream)
{
    /* Write Xing header again */
    if (gfp->bWriteVbrTag && fpStream && !fseek(fpStream, 0, SEEK_SET))
	PutVbrTag(gfp, fpStream);
}

lame_global_flags *
lame_init(void)
{
    lame_global_flags *gfp;
    int     ret;

    gfp = calloc(1, sizeof(lame_global_flags));
    if (!gfp)
        return NULL;

    ret = lame_init_old(gfp);
    if (ret != 0) {
        free(gfp);
        return NULL;
    }

    gfp->lame_allocated_gfp = 1;
    return gfp;
}

/* initialize mp3 encoder */
int
lame_init_old(lame_global_flags * gfp)
{
    lame_internal_flags *gfc;
    int align;

    disable_FPE();      // disable floating point exceptions

    memset(gfp, 0, sizeof(lame_global_flags));

    if (!(gfc = calloc(1, sizeof(lame_internal_flags) + 16))) return -1;

    align = (((int)gfc + 15) & ~15) - (int)gfc;
    gfp->internal_flags = gfc = (lame_internal_flags*)((int)gfc + align);
    gfc->alignment = align;

    /* Global flags.  set defaults here for non-zero values */
    /* see lame.h for description */
    /* set integer values to -1 to mean that LAME will compute the
     * best value, UNLESS the calling program as set it
     * (and the value is no longer -1)
     */


    gfp->mode = NOT_SET;
    gfp->original = 1;
    gfp->in_samplerate = 1000 * 44.1;
    gfp->num_channels = 2;
    gfp->num_samples = MAX_U_32_NUM;

    gfp->bWriteVbrTag = 1;
    gfp->quality = -1;

    gfp->lowpassfreq = 0;
    gfp->highpassfreq = 0;
    gfp->lowpasswidth = -1;
    gfp->highpasswidth = -1;

    gfp->VBR = cbr;
    gfp->VBR_q = 4;
    gfp->mean_bitrate_kbps = 0;
    gfp->VBR_min_bitrate_kbps = 0;
    gfp->VBR_max_bitrate_kbps = 0;
    gfp->VBR_hard_min = 0;

    gfc->OldValue[0] = 180;
    gfc->OldValue[1] = 180;
    gfc->CurrentStep[0] = 4;
    gfc->CurrentStep[1] = 4;
    gfc->masking_lower = 1.0;

    gfc->nsPsy.attackthre = gfc->nsPsy.attackthre_s = -1.0;
    gfc->istereo_ratio = -1.0;
    gfc->narrowStereo = -1.0;
    gfc->reduce_side = -1.0;
    gfc->nsPsy.msfix = NS_MSFIX*SQRT2;

    gfp->ATHcurve = 4;
    gfp->athaa_sensitivity = 0.0; /* no offset */
    gfp->useTemporal = -1;
    gfp->interChRatio = -1.0;

    /* The reason for
     *       int mf_samples_to_encode = ENCDELAY + POSTDELAY;
     * ENCDELAY = internal encoder delay.  And then we have to add POSTDELAY=288
     * because of the 50% MDCT overlap.  A 576 MDCT granule decodes to
     * 1152 samples.  To synthesize the 576 samples centered under this granule
     * we need the previous granule for the first 288 samples (no problem), and
     * the next granule for the next 288 samples (not possible if this is last
     * granule).  So we need to pad with 288 samples to make sure we can
     * encode the 576 samples we are interested in.
     */
    gfc->mf_samples_to_encode = ENCDELAY + POSTDELAY;
    gfp->encoder_padding = 0;
    gfc->mf_size = ENCDELAY - MDCTDELAY; /* we pad input with this many 0's */

#ifdef DECODE_ON_THE_FLY
    lame_decode_init();  // initialize the decoder 
#endif
#ifdef HAVE_NASM
    gfp->asm_optimizations.mmx = 1;
    gfp->asm_optimizations.amd3dnow = 1;
    gfp->asm_optimizations.sse = 1;
#endif
    gfp->preset = 0;
    
    gfp->sparsing = 0;
    gfp->sparse_low = 9.0;
    gfp->sparse_high = 3.0;
    
    return 0;
}

/***********************************************************************
 *
 *  some simple statistics
 *
 *  Robert Hegemann 2000-10-11
 *
 ***********************************************************************/

/*  histogram of used bitrate indexes:
 *  One has to weight them to calculate the average bitrate in kbps
 *
 *  bitrate indices:
 *  there are 14 possible bitrate indices, 0 has the special meaning 
 *  "free format" which is not possible to mix with VBR and 15 is forbidden
 *  anyway.
 *
 *  stereo modes:
 *  0: LR   number of left-right encoded frames
 *  1: LR-I number of left-right and intensity encoded frames
 *  2: MS   number of mid-side encoded frames
 *  3: MS-I number of mid-side and intensity encoded frames
 *
 *  4: number of encoded frames
 *
 */

void
lame_bitrate_kbps(const lame_global_flags * const gfp, int bitrate_kbps[14])
{
    const lame_internal_flags *gfc;
    int     i;

    if (!bitrate_kbps)
        return;
    if (!gfp)
        return;
    gfc = gfp->internal_flags;
    if (!gfc)
        return;

    for (i = 0; i < 14; i++)
        bitrate_kbps[i] = bitrate_table[gfp->version][i + 1];
}


#ifdef BRHIST
void
lame_bitrate_hist(const lame_global_flags * const gfp, int bitrate_count[14])
{
    const lame_internal_flags *gfc;
    int     i;

    if (!bitrate_count || !gfp || !(gfc = gfp->internal_flags))
        return;

    for (i = 0; i < 14; i++)
        bitrate_count[i] = gfc->bitrate_stereoMode_Hist[i + 1][4];
}


void
lame_stereo_mode_hist(const lame_global_flags * const gfp, int stmode_count[4])
{
    const lame_internal_flags *gfc;
    int     i;

    if (!stmode_count || !gfp || !(gfc = gfp->internal_flags))
        return;

    for (i = 0; i < 4; i++)
        stmode_count[i] = gfc->bitrate_stereoMode_Hist[15][i];
}



void
lame_bitrate_stereo_mode_hist(const lame_global_flags * const gfp,
                              int bitrate_stmode_count[14][4])
{
    const lame_internal_flags *gfc;
    int     i, j;

    if (!bitrate_stmode_count ||!gfp || !(gfc = gfp->internal_flags))
        return;

    for (j = 0; j < 14; j++)
        for (i = 0; i < 4; i++)
            bitrate_stmode_count[j][i] = gfc->bitrate_stereoMode_Hist[j + 1][i];
}



void
lame_block_type_hist(const lame_global_flags * const gfp, int btype_count[6])
{
    const lame_internal_flags *gfc;
    int     i;

    if (!btype_count || !gfp || !(gfc = gfp->internal_flags))
	return;

    for (i = 0; i < 6; ++i)
        btype_count[i] = gfc->bitrate_blockType_Hist[15][i];
}



void 
lame_bitrate_block_type_hist(const lame_global_flags * const gfp, 
                             int bitrate_btype_count[14][6])
{
    const lame_internal_flags * gfc;
    int     i, j;
    
    if (!bitrate_btype_count || !gfp || !(gfc = gfp->internal_flags))
        return;
        
    for (j = 0; j < 14; ++j)
	for (i = 0; i <  6; ++i)
	    bitrate_btype_count[j][i] = gfc->bitrate_blockType_Hist[j+1][i];
}

#endif

/* end of lame.c */
