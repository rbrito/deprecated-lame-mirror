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

#include "util.h"
#include "set_get.h"
#include "bitstream.h"
#include "version.h"
#include "tables.h"
#include "quantize.h"
#include "psymodel.h"
#include "VbrTag.h"
#include "id3tag.h"

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
static void
init_qval(lame_t gfc)
{
    switch (gfc->quality) {
    case 9:            /* no psymodel, no noise shaping, no reservoir */
	gfc->disable_reservoir = 1;
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
	gfc->noise_shaping_amp = 4;
	gfc->use_best_huffman = 1;
	break;

    case 1:	/* LAME 3.94's -h + alpha */
        gfc->filter_type = 0;
        gfc->psymodel = 2;
        gfc->noise_shaping_amp = 4;
        gfc->use_best_huffman = 2; /* inner loop, PAINFULLY SLOW */
        break;

    case 0:	/* same with -q 1 */
        gfc->filter_type = 0; /* 1 not yet coded */
        gfc->psymodel = 2;
        gfc->noise_shaping_amp = 4; /* 5 not yet coded */
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
    int bitrate = 0, i;
    for (i = 1; i <= 14; i++)
        if (ABS (bitrate_table[version][i] - bRate) < ABS (bitrate - bRate))
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
get_bitrate(lame_t gfc)
{
    /* assume 16bit linear PCM input, and in_sample = out_sample */
    return gfc->in_samplerate * 16 * gfc->channels_out
	/ (1.e3 * gfc->compression_ratio);
}

static void
set_compression_ratio(lame_t gfc)
{
    if (gfc->compression_ratio != 0)
	return;

    if (gfc->VBR == vbr) {
	FLOAT  cmp[] = { 5.7, 6.5, 7.3, 8.2, 10, 11.9, 14, 16, 18, 20 };
	if (gfc->VBR_q < 10)
	    gfc->compression_ratio = cmp[gfc->VBR_q];
	else
	    gfc->compression_ratio = cmp[9] + gfc->VBR_q-9;
    } else {
	gfc->compression_ratio
	    = gfc->in_samplerate * 16 * gfc->channels_in
	    / (1.e3 * gfc->mean_bitrate_kbps);
    }
}

/* Switch mappings for target bitrate */
static struct {
    int    kbps;
    int    large_scalefac;
    int    lowpass;
    double mask_lower;
    double reduce_side;
    double scale;
    int    ath_curve;
    int    ath_lower;
    double interch;
    double shortthreshold;
    double istereo_ratio;
    double narrow_stereo;
} switch_map [] = {
    /*  scalefac_s  masklower     scale    athlower   short-th   narrow-st */
    /* kbps  lowpass     reduceside   athcurve  inter-ch     is-ratio */
    {   8, 1,  2000,  -1.0, 0.7,   0.90, 11,  -4, 0.0012, 20., 0.99, 0.6},
    {  16, 1,  3700,  -1.0, 0.7,   0.90, 11,  -4, 0.0010, 20., 0.99, 0.6},
    {  24, 1,  3900,  -1.0, 0.7,   0.90, 11,  -4, 0.0010, 20., 0.99, 0.6},
    {  32, 1,  5500,  -1.0, 0.7,   0.90, 11,  -4, 0.0010, 20., 0.99, 0.6},
    {  40, 1,  7000,  -1.0, 0.7,   0.90, 11,  -3, 0.0009, 20., 0.99, 0.6},
    {  48, 1,  7500,  -1.0, 0.7,   0.90, 11,  -3, 0.0009, 20., 0.99, 0.6},
    {  56, 1, 10000,  -1.0, 0.7,   0.90, 11,  -3, 0.0008, 5.0, 0.99, 0.5},
    {  64, 1, 12000,  -3.0, 0.4,   0.90, 10,  -2, 0.0008, 3.0, 0.99, 0.4},
    {  80, 1, 14500,  -4.5, 0.4,   0.93,  8,  -2, 0.0007, 3.0, 0.9, 0.3},
    {  96, 1, 15300,  -6.0, 0.2,   0.93,  8,  -2, 0.0006, 2.5, 0.6, 0.2},
    { 112, 1, 16000,  -7.0, 0.2,   0.93,  7,  -2, 0.0005, 2.5, 0.3, 0.1},
    { 128, 1, 17500,  -8.0, 0.0,   0.93,  5,  -1, 0.0002, 2.5, 0.0, 0.0},
    { 160, 1, 18000,  -9.0, 0.0,   0.95,  4,   0, 0.0000, 1.8, 0.0, 0.0},
    { 192, 1, 19500, -10.0, 0.0,   0.97,  3,   0, 0.0000, 1.8, 0.0, 0.0},
    { 224, 1, 20000, -11.0, 0.0,   0.98,  2,   1, 0.0000, 1.8, 0.0, 0.0},
    { 256, 0, 20500, -12.0, 0.0,   1.00,  1,   1, 0.0000, 1.8, 0.0, 0.0},
    { 320, 0, 21000, -13.0, 0.0,   1.00,  0,   1, 0.0000, 1.8, 0.0, 0.0}
};

static int apply_preset(lame_t gfc, int bitrate, vbr_mode mode)
{
    int lower_range, lower_range_kbps, upper_range, upper_range_kbps;
    int r, b;

    for (b = 1; b < sizeof(switch_map)/sizeof(switch_map[0])-1
	     && bitrate > switch_map[b].kbps; b++)
	;

    upper_range_kbps = switch_map[b].kbps;
    upper_range = b;
    lower_range_kbps = switch_map[b-1].kbps;
    lower_range = (b-1);

    /* Determine which range the value specified is closer to */
    r = upper_range;
    if (upper_range_kbps - bitrate > bitrate - lower_range_kbps)
        r = lower_range;

    lame_set_VBR_mean_bitrate_kbps(gfc, bitrate);
    lame_set_brate(gfc, lame_get_VBR_mean_bitrate_kbps(gfc));

    if (mode != vbr) {
	lame_set_sfscale(gfc, switch_map[r].large_scalefac);
	lame_set_subblock_gain(gfc, switch_map[r].large_scalefac);
	/*
	 * ABR seems to have big problems with clipping, especially at
	 * low bitrates. so we compensate for that here by using a scale
	 * value depending on bitrate
	 */
	if (gfc->scale == 0.0)
	    (void) lame_set_scale( gfc, switch_map[r].scale );
    }

    if (gfc->ATHcurve < 0)
	lame_set_ATHcurve(gfc, switch_map[r].ath_curve);
    if (gfc->ATHlower < 0)
	lame_set_ATHlower(gfc, switch_map[r].ath_lower);

    if (gfc->lowpassfreq == 0) {
	int lowpass = switch_map[r].lowpass;
	if (gfc->quality > 7)
	    lowpass *= 0.9;
	if (gfc->mode == MONO)
	    lowpass *= 1.5;
	lame_set_lowpassfreq(gfc, lowpass);
    }

    if (gfc->mode == NOT_SET)
	gfc->mode = JOINT_STEREO;

    if (gfc->reduce_side < 0.0)
	lame_set_reduceSide(gfc, switch_map[r].reduce_side);

    if (gfc->interChRatio < 0)
	lame_set_interChRatio(gfc, switch_map[r].interch);

    if (gfc->nsPsy.attackthre < 0.0)
	lame_set_short_threshold(gfc, switch_map[r].shortthreshold);

    if (gfc->istereo_ratio < 0.0)
	lame_set_istereoRatio(gfc, switch_map[r].istereo_ratio);

    if (gfc->narrowStereo < 0.0)
	lame_set_narrowenStereo(gfc, switch_map[r].narrow_stereo);

    if (gfc->masking_lower < 0.0)
	lame_set_maskingadjust(gfc, switch_map[r].mask_lower);

    if (gfc->masking_lower_short < 0.0)
	lame_set_maskingadjust_short(gfc, switch_map[r].mask_lower);

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
 *  Finally, we set the algorithm flags based on the gfc->quality value
 *  init_qval(gfc);
 *
 ********************************************************************/
int
lame_init_params(lame_t gfc)
{
    int gr, ch;

    gfc->Class_ID = 0;

    /* report functions */
    gfc->report.msgf   = gfc->report.msgf;
    gfc->report.debugf = gfc->report.debugf;
    gfc->report.errorf = gfc->report.errorf;

#ifdef HAVE_NASM
    gfc->CPU_features.MMX
	= gfc->CPU_features.SSE = gfc->CPU_features.SSE2
	= gfc->CPU_features.AMD_3DNow = gfc->CPU_features.AMD_E3DNow = 0;

    if (gfc->asm_optimizations.mmx)
        gfc->CPU_features.MMX = has_MMX();

    if (gfc->asm_optimizations.amd3dnow ) {
	gfc->CPU_features.AMD_3DNow = has_3DNow();
	gfc->CPU_features.AMD_E3DNow = has_E3DNow();
    }
    if (gfc->asm_optimizations.sse) {
	gfc->CPU_features.SSE = has_SSE();
	gfc->CPU_features.SSE2 = has_SSE2();
    }
#endif

    gfc->channels_in = gfc->num_channels;
    if (gfc->channels_in == 1)
        gfc->mode = MONO;
    gfc->channels_out = (gfc->mode == MONO) ? 1 : 2;
    gfc->mode_ext_next = gfc->mode_ext = MPG_MD_LR_LR;
    if (gfc->mode == MONO)
	gfc->force_ms = 0; /* cannot use force MS stereo for mono output */
    if (gfc->VBR != cbr)
	gfc->free_format = 0; /* VBR can't be mixed with free format */

    if (gfc->VBR != vbr && gfc->mean_bitrate_kbps == 0) {
	/* no bitrate or compression ratio specified,
	   use default compression ratio of 11.025,
	   which is the rate to compress a CD down to exactly 128000 bps */
	if (gfc->compression_ratio <= 0)
	    gfc->compression_ratio = 11.025;

	/* choose a bitrate which achieves the specified compression ratio
	 */
	gfc->mean_bitrate_kbps = get_bitrate(gfc);
    }
    set_compression_ratio(gfc);
    apply_preset(gfc, get_bitrate(gfc), gfc->VBR);

    /* output sampling rate is determined by the lowpass value */
    if (gfc->out_samplerate == 0) {
	if (gfc->lowpassfreq < 0)
	    gfc->out_samplerate
		= optimum_samplefreq(gfc->in_samplerate/2.0,
				     gfc->in_samplerate);
	else
	    gfc->out_samplerate
		= optimum_samplefreq(gfc->lowpassfreq, gfc->in_samplerate);
    }
    gfc->samplerate_index = SmpFrqIndex(gfc->out_samplerate);
    if (gfc->samplerate_index < 0)
        return -1;

    /* apply user driven high pass filter */
    if (gfc->highpassfreq > 0) {
        gfc->highpass1 = 2. * gfc->highpassfreq;

        if (gfc->highpasswidth >= 0)
            gfc->highpass2 = 2. * (gfc->highpassfreq + gfc->highpasswidth);
        else            /* 0% above on default */
            gfc->highpass2 = (1 + 0.00) * 2. * gfc->highpassfreq;

	gfc->highpass1 /= gfc->out_samplerate;
	gfc->highpass2 /= gfc->out_samplerate;
    }

    /* apply user driven low pass filter */
    if (gfc->lowpassfreq > 0) {
	gfc->lowpass2 = 2. * gfc->lowpassfreq;
        if (gfc->lowpasswidth >= 0) {
            gfc->lowpass1 = 2. * (gfc->lowpassfreq - gfc->lowpasswidth);
            if (gfc->lowpass1 < 0)
                gfc->lowpass1 = 0;
        }
        else          /* 0% below on default */
	    gfc->lowpass1 = (1 - 0.00) * 2. * gfc->lowpassfreq;

	gfc->lowpass1 /= gfc->out_samplerate;
	gfc->lowpass2 /= gfc->out_samplerate;
    }

    gfc->version = 1;
    if (gfc->out_samplerate <= 24000)
	gfc->version = 0;

    gfc->mode_gr = gfc->version+1;
    gfc->framesize = 576 * gfc->mode_gr;
    gfc->encoder_delay = ENCDELAY;
    gfc->resample_ratio = (double) gfc->in_samplerate / gfc->out_samplerate;
    /* at 160 kbps (MPEG-2/2.5)/ 320 kbps (MPEG-1) only
       Free format or CBR are possible, no ABR */
    if (gfc->mean_bitrate_kbps > 160 * gfc->mode_gr) {
	gfc->VBR = cbr;
	gfc->free_format = 1;
    }

    /*******************************************************
     * bitrate index
     *******************************************************/
    /* for non Free Format find the nearest allowed bitrate */
    gfc->bitrate_index = 0;
    if (gfc->VBR == cbr && !gfc->free_format) {
	gfc->mean_bitrate_kbps
	    = FindNearestBitrate(gfc->mean_bitrate_kbps, gfc->version);
	gfc->bitrate_index
	    = BitrateIndex(gfc->mean_bitrate_kbps, gfc->version);
	if (gfc->bitrate_index < 0)
	    return -1;
    }
    if (gfc->VBR != cbr) {
	/* choose a min/max bitrate for VBR */
        /* if the user didn't specify VBR_max_bitrate: */
        gfc->VBR_min_bitrate = 1;
        gfc->VBR_max_bitrate = 14;
	if (gfc->mode == MONO)
	    gfc->VBR_max_bitrate--;

        if (gfc->VBR_min_bitrate_kbps
	    && (gfc->VBR_min_bitrate
		= BitrateIndex(gfc->VBR_min_bitrate_kbps, gfc->version)) < 0)
	    return -1;

	gfc->VBR_min_bitrate_kbps
	    = bitrate_table[gfc->version][gfc->VBR_min_bitrate];
        if (gfc->mean_bitrate_kbps < gfc->VBR_min_bitrate_kbps)
	    gfc->mean_bitrate_kbps = gfc->VBR_min_bitrate_kbps;

        if (gfc->VBR_max_bitrate_kbps
	    && (gfc->VBR_max_bitrate
		= BitrateIndex(gfc->VBR_max_bitrate_kbps, gfc->version)) < 0)
	    return -1;

	gfc->VBR_max_bitrate_kbps
	    = bitrate_table[gfc->version][gfc->VBR_max_bitrate];
        if (gfc->mean_bitrate_kbps > gfc->VBR_max_bitrate_kbps)
	    gfc->mean_bitrate_kbps = gfc->VBR_max_bitrate_kbps;
    }

#ifndef NOANALYSIS
    if (gfc->pinfo) {
	gfc->bWriteVbrTag = 0; /* disable Xing VBR tag */
	if (gfc->quality > 8) {
	    gfc->quality = 8;
	    ERRORF(gfc, "Analyzer needs psymodel (-q 8 or lesser).\n");
	}
    }
#endif
    init_bit_stream_w(gfc);

    gfc->Class_ID = LAME_ID;

    if (gfc->VBR == vbr && gfc->quality != 5) {
	gfc->quality = 5;
	ERRORF(gfc, "VBR mode with -q setting is meaningless.\n");
    }

    if (gfc->noise_shaping_amp > 4)
	gfc->noise_shaping_amp = 4;

    /* initialize internal qval settings */
    init_qval(gfc);

    lame_init_bitstream(gfc);
    iteration_init(gfc);
    psymodel_init(gfc);

    /* scaling */
    if (gfc->scale == 0.0)
	gfc->scale = 1.0;

    if (gfc->scale_left == 0.0)
	gfc->scale_left = 1.0;

    if (gfc->scale_right == 0.0)
	gfc->scale_right = 1.0;

    gfc->scale_left  *= gfc->scale;
    gfc->scale_right *= gfc->scale;

    if (!gfc->psymodel) {
	gfc->ATH.adjust[0] = gfc->ATH.adjust[1]
	    = gfc->ATH.adjust[2] = gfc->ATH.adjust[3] = 1.0;
	gfc->mode_ext = MPG_MD_LR_LR;
	if (gfc->mode == JOINT_STEREO)
	    gfc->mode_ext = MPG_MD_MS_LR;
	gfc->disable_reservoir = 1;
	gfc->substep_shaping = 0;
    }

    for (gr = 0; gr < gfc->mode_gr ; gr++) {
	for (ch = 0; ch < gfc->channels_out; ch++) {
	    gfc->l3_side.tt[gr][ch].block_type = NORM_TYPE;
	    gfc->l3_side.tt[gr][ch].mixed_block_flag = 0;
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
lame_print_config(lame_t gfc)
{
    double  out_samplerate = gfc->out_samplerate;
    double  in_samplerate = gfc->in_samplerate;

#ifdef HAVE_NASM
    if (gfc->CPU_features.MMX
	|| gfc->CPU_features.AMD_3DNow || gfc->CPU_features.AMD_E3DNow
        || gfc->CPU_features.SSE || gfc->CPU_features.SSE2) {
        MSGF(gfc, "CPU features:");

        if (gfc->CPU_features.MMX)
            MSGF(gfc, "MMX (ASM used)");
        if (gfc->CPU_features.AMD_3DNow)
            MSGF(gfc, ", 3DNow! (ASM used)");
        if (gfc->CPU_features.AMD_E3DNow)
            MSGF(gfc, ", E3DNow! (ASM used)");
        if (gfc->CPU_features.SSE)
            MSGF(gfc, ", SSE (ASM used)");
        if (gfc->CPU_features.SSE2)
            MSGF(gfc, ", SSE2");
        MSGF(gfc, "\n");
    }
#endif

    if (gfc->num_channels == 2 && gfc->channels_out == 1 /* mono */ ) {
        MSGF(gfc,
             "Autoconverting from stereo to mono. Setting encoding to mono mode.\n");
    }

    if (gfc->resample_ratio != 1.) {
        MSGF(gfc, "Resampling:  input %g kHz  output %g kHz\n",
             1.e-3 * in_samplerate, 1.e-3 * out_samplerate);
    }

    if (gfc->filter_type == 0) {
        if (0. < gfc->highpass2 && gfc->highpass2 < 1.0)
            MSGF(gfc,
                 "Using polyphase highpass filter, transition band: %5.0f Hz - %5.0f Hz\n",
                 0.5 * gfc->highpass1 * out_samplerate,
                 0.5 * gfc->highpass2 * out_samplerate);
        if (0. < gfc->lowpass1 && gfc->lowpass1 < 1.0) {
	    MSGF(gfc,
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

    if (gfc->free_format) {
        MSGF(gfc,
             "Warning: many decoders cannot handle free format bitstreams\n");
        if (gfc->mean_bitrate_kbps > 320) {
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
lame_print_internals(lame_t gfc)
{
    const char * pc = "";
    FLOAT bass, alto, treble, sfb21;
    int i;

    /*  compiler/processor optimizations, operational, etc.
     */
    MSGF( gfc, "\nmisc:\n\n" );
    
    MSGF( gfc, "\tscaling: \n");
    MSGF( gfc, "\t ^ ch0 (left) : %f\n", gfc->scale_left );
    MSGF( gfc, "\t ^ ch1 (right): %f\n", gfc->scale_right );
    MSGF( gfc, "\tfilter type: %d\n", gfc->filter_type );
    MSGF( gfc, "\t...\n" );

    /*  everything controlling the stream format 
     */
    MSGF( gfc, "\nstream format:\n\n" );
    switch ( gfc->version ) {
    case 0:  pc = "2.5"; break;
    case 1:  pc = "1";   break;
    case 2:  pc = "2";   break;
    default: pc = "?";   break;
    }
    MSGF( gfc, "\tMPEG-%s Layer 3\n", pc );
    switch ( gfc->mode ) {
    case JOINT_STEREO: pc = "joint stereo";   break;
    case STEREO      : pc = "stereo";         break;
    case DUAL_CHANNEL: pc = "dual channel";   break;
    case MONO        : pc = "mono";           break;
    case NOT_SET     : pc = "not set (error)"; break;
    default          : pc = "unknown (error)"; break;
    }
    MSGF( gfc, "\t%d channel - %s\n", gfc->channels_out, pc );
    if (gfc->use_istereo)
	MSGF(gfc, "\t ^ using intensity stereo, the ratio is %f\n",
	     1.0-gfc->istereo_ratio);

    if (gfc->free_format)    pc = "(free format)";
    else pc = "";
    switch ( gfc->VBR ) {
    case cbr : MSGF( gfc, "\tconstant bitrate - CBR %s\n", pc ); break;
    case abr : MSGF( gfc, "\tvariable bitrate - ABR %s\n", pc ); break;
    case vbr : MSGF( gfc, "\tvariable bitrate - VBR\n"        ); break;
    default  : MSGF( gfc, "\t ?? oops, some new one ?? \n"    ); break;
    }
    if (gfc->bWriteVbrTag) 
    MSGF( gfc, "\tusing LAME Tag\n" );
    MSGF( gfc, "\t...\n" );

    /*  everything controlling psychoacoustic settings, like ATH, etc.
     */
    MSGF( gfc, "\npsychoacoustic:\n\n" );
    MSGF( gfc, "\tshort block switching threshold: %f\n",
	  gfc->nsPsy.attackthre);
    MSGF( gfc, "\tpsymodel: %s\n", gfc->psymodel ? "used" : "not used");
    
    pc = "using";
    if ( gfc->ATHshort ) pc = "the only masking for short blocks";
    if ( gfc->ATHonly  ) pc = "the only masking";
    if ( gfc->noATH    ) pc = "not used";
    MSGF( gfc, "\tATH: %s\n", pc );
    MSGF( gfc, "\t ^ shape: %g\n", gfc->ATHcurve);
    MSGF( gfc, "\t ^ level adjustement: %f (dB)\n", gfc->ATHlower );
    MSGF( gfc, "\t ^ adaptive adjustment decay (dB): %f\n",
	  FAST_LOG10(gfc->ATH.aa_decay) * 10.0);

    i = (gfc->nsPsy.tune >> 2) & 63;
    if (i >= 32)
	i -= 64;
    bass = i*0.25;

    i = (gfc->nsPsy.tune >> 8) & 63;
    if (i >= 32)
	i -= 64;
    alto = i*0.25;

    i = (gfc->nsPsy.tune >> 14) & 63;
    if (i >= 32)
	i -= 64;
    treble = i*0.25;

    /*  to be compatible with Naoki's original code, the next 6 bits
     *  define only the amount of changing treble for sfb21 */
    i = (gfc->nsPsy.tune >> 20) & 63;
    if (i >= 32)
	i -= 64;
    sfb21 = treble + i*0.25;

    MSGF(gfc, "\tadjust masking: %f dB\n", gfc->VBR_q-4.0 );
    MSGF(gfc, "\t ^ bass=%g dB, alto=%g dB, treble=%g dB, sfb21=%g dB\n",
	 bass, alto, treble, sfb21);

    MSGF( gfc, "\ttemporal masking sustain factor: %f\n", gfc->nsPsy.decay);
    MSGF( gfc, "\tinterchannel masking ratio: %f\n", gfc->interChRatio );
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
    if (gfc->VBR != vbr) {
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



int
lame_encode_buffer(lame_t gfc,
                   const short int buffer_l[], const short int buffer_r[],
                   const int nsamples, unsigned char *mp3buf,
		   const int mp3buf_size)
{
    int     ret, i;
    sample_t *in_buffer;

    if (gfc->Class_ID != LAME_ID)
        return -3;

    if (nsamples == 0)
        return 0;

    in_buffer = malloc(sizeof(sample_t)*gfc->channels_in*nsamples);

    if (!in_buffer) {
	ERRORF(gfc, "Error: can't allocate in_buffer buffer\n");
	return -2;
    }

    /* make a copy of input buffer, changing type to sample_t */
    for (i = 0; i < nsamples; i++)
        in_buffer[i] = buffer_l[i] * gfc->scale_left;

    if (gfc->channels_in>1) {
	for (i = 0; i < nsamples; i++)
	    in_buffer[i+nsamples] = buffer_r[i] * gfc->scale_right;
    }

    ret = lame_encode_buffer_sample_t(gfc, in_buffer,
				      nsamples, mp3buf, mp3buf_size);
    free(in_buffer);
    return ret;
}


int
lame_encode_buffer_float(lame_t gfc,
                   const float buffer_l[],
                   const float buffer_r[],
                   const int nsamples, unsigned char *mp3buf, const int mp3buf_size)
{
    int     ret, i;
    sample_t *in_buffer;

    if (gfc->Class_ID != LAME_ID)
        return -3;

    if (nsamples == 0)
        return 0;

    in_buffer = malloc(sizeof(sample_t)*gfc->channels_in*nsamples);

    if (!in_buffer) {
	ERRORF(gfc, "Error: can't allocate in_buffer buffer\n");
	return -2;
    }

    /* make a copy of input buffer, changing type to sample_t */
    for (i = 0; i < nsamples; i++) {
        in_buffer[i] = buffer_l[i] * gfc->scale_left;
        if (gfc->channels_in>1)
	    in_buffer[i+nsamples] = buffer_r[i] * gfc->scale_right;
    }

    ret = lame_encode_buffer_sample_t(gfc, in_buffer,
				      nsamples, mp3buf, mp3buf_size);
    free(in_buffer);
    return ret;
}

int
lame_encode_buffer_int(lame_t gfc,
                   const int buffer_l[],
                   const int buffer_r[],
                   const int nsamples, unsigned char *mp3buf, const int mp3buf_size)
{
    int     ret, i;
    sample_t *in_buffer;
    FLOAT scale;

    if (gfc->Class_ID != LAME_ID)
        return -3;

    if (nsamples == 0)
        return 0;

    in_buffer = malloc(sizeof(sample_t)*gfc->channels_in*nsamples);

    if (!in_buffer) {
	ERRORF(gfc, "Error: can't allocate in_buffer buffer\n");
	return -2;
    }

    /* make a copy of input buffer, changing type to sample_t */
    scale = gfc->scale_left * (1.0 / ( 1L << (8 * sizeof(int) - 16)));
    for (i = 0; i < nsamples; i++)
	in_buffer[i] = buffer_l[i] * scale;

    if (gfc->channels_in>1) {
	scale = gfc->scale_right * (1.0 / ( 1L << (8 * sizeof(int) - 16)));
	for (i = 0; i < nsamples; i++)
	    in_buffer[i+nsamples] = buffer_r[i] * scale;
    }

    ret = lame_encode_buffer_sample_t(gfc, in_buffer,
				      nsamples, mp3buf, mp3buf_size);
    free(in_buffer);
    return ret;
}

int
lame_encode_buffer_long2(lame_t gfc,
                   const long buffer_l[],
                   const long buffer_r[],
                   const int nsamples, unsigned char *mp3buf, const int mp3buf_size)
{
    int     ret, i;
    sample_t *in_buffer;
    FLOAT scale;

    if (gfc->Class_ID != LAME_ID)
        return -3;

    if (nsamples == 0)
        return 0;

    in_buffer = malloc(sizeof(sample_t)*gfc->channels_in*nsamples);

    if (!in_buffer) {
	ERRORF(gfc, "Error: can't allocate in_buffer buffer\n");
	return -2;
    }

    /* make a copy of input buffer, changing type to sample_t */
    scale = gfc->scale_left * (1.0 / (1L << (8 * sizeof(long) - 16)));
    for (i = 0; i < nsamples; i++)
	in_buffer[i] = buffer_l[i] * scale;

    if (gfc->channels_in>1) {
	scale = gfc->scale_right * (1.0 / (1L << (8 * sizeof(long) - 16)));
	for (i = 0; i < nsamples; i++)
	    in_buffer[i+nsamples] = buffer_r[i] * scale;
    }

    ret = lame_encode_buffer_sample_t(gfc, in_buffer,
				      nsamples, mp3buf, mp3buf_size);

    free(in_buffer);
    return ret;
}

int
lame_encode_buffer_long(lame_t gfc,
			const long buffer_l[], const long buffer_r[],
			const int nsamples, unsigned char *mp3buf,
			const int mp3buf_size)
{
    int     ret, i;
    sample_t *in_buffer;

    if (gfc->Class_ID != LAME_ID)
        return -3;

    if (nsamples == 0)
        return 0;

    in_buffer = malloc(sizeof(sample_t)*gfc->channels_in*nsamples);

    if (!in_buffer) {
	ERRORF(gfc, "Error: can't allocate in_buffer buffer\n");
	return -2;
    }

    /* make a copy of input buffer, changing type to sample_t */
    for (i = 0; i < nsamples; i++) {
	in_buffer[i] = buffer_l[i] * gfc->scale_left;
	if (gfc->channels_in>1)
	    in_buffer[i+nsamples] = gfc->scale_right * buffer_r[i];
    }

    ret = lame_encode_buffer_sample_t(gfc, in_buffer,
				      nsamples, mp3buf, mp3buf_size);

    free(in_buffer);
    return ret;
}

int
lame_encode_buffer_interleaved(lame_t gfc,
                               short int buffer[], int nsamples,
                               unsigned char *mp3buf, int mp3buf_size)
{
    int     ret, i;
    sample_t *in_buffer;

    if (gfc->Class_ID != LAME_ID)
        return -3;

    if (nsamples == 0)
        return 0;

    in_buffer = malloc(sizeof(sample_t)*2*nsamples);
    if (!in_buffer) {
        return -2;
    }
    for (i = 0; i < nsamples; i++) {
        in_buffer[i] = buffer[2 * i]    * gfc->scale_left;
        in_buffer[i+nsamples] = buffer[2 * i + 1]* gfc->scale_right;
    }
    ret = lame_encode_buffer_sample_t(gfc, in_buffer,
				      nsamples, mp3buf, mp3buf_size);
    free(in_buffer);
    return ret;
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
lame_encode_flush_nogap(lame_t gfc,
			unsigned char *mp3buffer, int mp3buffer_size)
{
    return flush_bitstream(gfc,  mp3buffer, mp3buffer_size, 1);
}


/* called by lame_init_params.  You can also call this after flush_nogap 
   if you want to write new id3v2 and Xing VBR tags into the bitstream */
int
lame_init_bitstream(lame_t gfc)
{
#ifdef BRHIST
    /* initialize histogram data optionally used by frontend */
    memset(gfc->bitrate_stereoMode_Hist, 0,
	   sizeof(gfc->bitrate_stereoMode_Hist));
    memset(gfc->bitrate_blockType_Hist, 0,
	   sizeof(gfc->bitrate_blockType_Hist));
#endif
    gfc->frameNum=0;
    /* Write ID3v2 TAG at very the beggining */
    id3tag_write_v2(gfc);
    /* Write initial VBR Header to bitstream and init VBR data */
    InitVbrTag(gfc);

    return 0;
}

/*****************************************************************/
/* flush internal PCM sample buffers, then mp3 buffers           */
/* then write id3 v1 tags into bitstream.                        */
/*****************************************************************/
int
lame_encode_flush(lame_t gfc, unsigned char *mp3buffer, int mp3buffer_size)
{
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
        imp3 = lame_encode_buffer(gfc, buffer[0], buffer[1], gfc->framesize,
                                  mp3buffer, mp3buffer_size_remaining);

        /* don't count the above padding: */
        gfc->mf_samples_to_encode -= gfc->framesize;
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
    imp3 = flush_bitstream(gfc, mp3buffer, mp3buffer_size_remaining, 1);
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
    id3tag_write_v1(gfc);
    imp3 = copy_buffer(gfc,mp3buffer, mp3buffer_size_remaining, 0);

    if (imp3 < 0) {
        return imp3;
    }
    mp3count += imp3;
    gfc->encoder_padding=end_padding;
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
lame_close(lame_t gfc)
{
    int  i;
    if (gfc->Class_ID != LAME_ID)
        return -3;

    gfc->Class_ID = 0;

    /* free all malloc'd data in gfc, and then free gfc: */
    for ( i = 0 ; i <= 2*BPC; i++ )
        if ( gfc->blackfilt[i] != NULL ) {
            free ( gfc->blackfilt[i] );
	    gfc->blackfilt[i] = NULL;
	}
    if ( gfc->inbuf_old[0] ) { 
        free ( gfc->inbuf_old[0] );
	gfc->inbuf_old[0] = NULL;
    }
    if ( gfc->inbuf_old[1] ) { 
        free ( gfc->inbuf_old[1] );
	gfc->inbuf_old[1] = NULL;
    }

    if ( gfc->VBR_seek_table.bag ) {
        free ( gfc->VBR_seek_table.bag );
        gfc->VBR_seek_table.bag=NULL;
        gfc->VBR_seek_table.size=0;
    }
    if ( gfc->s3_ll ) {
        free ( gfc->s3_ll );
    }
    if ( gfc->s3_ss ) {
        free ( gfc->s3_ss );
    }

    free((unsigned char*)gfc - gfc->alignment);

    return 0;
}

/*****************************************************************/
/* flush internal mp3 buffers, and free internal buffers         */
/*****************************************************************/
int
lame_encode_finish(lame_t gfc,
		   unsigned char *mp3buffer, int mp3buffer_size)
{
    int     ret = lame_encode_flush(gfc, mp3buffer, mp3buffer_size);

    lame_close(gfc);

    return ret;
}

/*****************************************************************/
/* write VBR Xing header, and ID3 version 1 tag, if asked for    */
/*****************************************************************/
void
lame_mp3_tags_fid(lame_t gfc, FILE * fpStream)
{
    /* Write Xing header again */
    if (gfc->bWriteVbrTag && fpStream && !fseek(fpStream, 0, SEEK_SET))
	PutVbrTag(gfc, fpStream);
}

/****************************************************************/
/* initialize mp3 encoder					*/
/****************************************************************/
lame_t
lame_init(void)
{
    lame_t gfc;
    void *work = calloc(1, sizeof(struct lame_internal_flags) + 16);
    if (!work)
        return NULL;

    disable_FPE();      /* disable floating point exceptions */

    gfc = (lame_t)(((unsigned int)work + 15) & ~15);
    gfc->alignment = (unsigned char*)gfc - (unsigned char*)work;

    /* Global flags.  set defaults here for non-zero values */
    /* set integer values to -1 to mean that LAME will compute the
     * best value, UNLESS the calling program as set it
     * (and the value is no longer -1)
     */
    gfc->mode = NOT_SET;
    gfc->original = 1;
    gfc->in_samplerate = 44100;
    gfc->num_channels = 2;
    gfc->num_samples = MAX_U_32_NUM;

    gfc->bWriteVbrTag = 1;
    gfc->quality = LAME_DEFAULT_QUALITY;

    gfc->lowpassfreq = 0;
    gfc->highpassfreq = 0;
    gfc->lowpasswidth = -1;
    gfc->highpasswidth = -1;

    gfc->VBR = cbr;
    gfc->VBR_q = 4;
    gfc->mean_bitrate_kbps = 0;
    gfc->VBR_min_bitrate_kbps = 0;
    gfc->VBR_max_bitrate_kbps = 0;

    gfc->OldValue[0] = gfc->OldValue[1] = 180;
    gfc->CurrentStep[0] = gfc->CurrentStep[1] = 4;

    gfc->nsPsy.attackthre = -1.0;
    gfc->istereo_ratio = -1.0;
    gfc->narrowStereo = -1.0;
    gfc->reduce_side = -1.0;
    gfc->nsPsy.msfix = NS_MSFIX*SQRT2;
    gfc->interChRatio = -1.0;
    gfc->masking_lower = -1.0;
    gfc->masking_lower_short = -1.0;
    gfc->ATHlower = -1.0;
    gfc->ATHcurve = -1;

    /* The reason for
     *       int mf_samples_to_encode = ENCDELAY + POSTDELAY;
     * ENCDELAY = internal encoder delay. And then we have to add POSTDELAY=288
     * because of the 50% MDCT overlap.  A 576 MDCT granule decodes to
     * 1152 samples.  To synthesize the 576 samples centered under this granule
     * we need the previous granule for the first 288 samples (no problem), and
     * the next granule for the next 288 samples (not possible if this is last
     * granule).  So we need to pad with 288 samples to make sure we can
     * encode the 576 samples we are interested in.
     */
    gfc->mf_samples_to_encode = ENCDELAY + POSTDELAY;
    gfc->encoder_padding = 0;
    gfc->mf_size = ENCDELAY - MDCTDELAY; /* we pad input with this many 0's */

#ifdef DECODE_ON_THE_FLY
    lame_decode_init();  /* initialize the decoder */
#endif
#ifdef HAVE_NASM
    gfc->asm_optimizations.mmx = 1;
    gfc->asm_optimizations.amd3dnow = 1;
    gfc->asm_optimizations.sse = 1;
#endif

    id3tag_init (gfc);

    return gfc;
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
lame_bitrate_kbps(lame_t gfc, int bitrate_kbps[14])
{
    int     i;

    if (!bitrate_kbps || !gfc)
        return;

    for (i = 0; i < 14; i++)
        bitrate_kbps[i] = bitrate_table[gfc->version][i + 1];
}


#ifdef BRHIST
void
lame_bitrate_hist(lame_t gfc, int bitrate_count[14])
{
    int     i, j;

    if (!bitrate_count || !gfc)
	return;

    for (i = 0; i < 14; i++) {
	int total = 0;
	for (j = 0; j < 4; j++)
	    total += gfc->bitrate_stereoMode_Hist[i+1][j];
	bitrate_count[i] = total;
    }
}


void
lame_stereo_mode_hist(lame_t gfc, int stmode_count[4])
{
    int     i, j;

    if (!stmode_count || !gfc)
	return;

    for (i = 0; i < 4; i++) {
	int total = 0;
	for (j = 0; j < 14; j++)
	    total += gfc->bitrate_stereoMode_Hist[j+1][i];
	stmode_count[i] = total;
    }
}



void
lame_bitrate_stereo_mode_hist(lame_t gfc, int bitrate_stmode_count[14][4])
{
    int     i, j;

    if (!bitrate_stmode_count ||!gfc)
	return;

    for (j = 0; j < 14; j++)
	for (i = 0; i < 4; i++)
            bitrate_stmode_count[j][i] = gfc->bitrate_stereoMode_Hist[j+1][i];
}



void
lame_block_type_hist(lame_t gfc, int btype_count[6])
{
    int     i, total = 0;

    if (!btype_count || !gfc)
	return;

    for (i = 0; i < 5; ++i) {
	total += gfc->bitrate_blockType_Hist[15][i];
	btype_count[i] = gfc->bitrate_blockType_Hist[15][i];
    }
    btype_count[i] = total;
}



void 
lame_bitrate_block_type_hist(lame_t gfc, int bitrate_btype_count[14][6])
{
    int     i, j;
    if (!bitrate_btype_count || !gfc)
        return;

    for (j = 0; j < 14; ++j) {
	int total = 0;
	for (i = 0; i < 5; ++i) {
	    bitrate_btype_count[j][i] = gfc->bitrate_blockType_Hist[j+1][i];
	    total += gfc->bitrate_blockType_Hist[j+1][i];
	}
	bitrate_btype_count[j][i] = total;
    }
}

#endif

/* end of lame.c */
