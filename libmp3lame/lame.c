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
#ifdef WITH_DMALLOC
# include <dmalloc.h>
#endif
#include <assert.h>

#include "encoder.h"
#include "newmdct.h"
#include "util.h"
#include "set_get.h"
#include "bitstream.h"
#include "tables.h"
#include "quantize.h"
#include "psymodel.h"
#include "tags.h"

#define LAME_DEFAULT_QUALITY 5

static int encode_buffer_sample(
    lame_t gfc,
    int nsamples,
    unsigned char *mp3buf,
    const int mp3buf_size
    );

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
	gfc->filter_type = 1;
	gfc->psymodel = 2;
	gfc->noise_shaping_amp = 1;
	gfc->use_best_huffman = 1;
	break;

    case 4:
	gfc->filter_type = 1;
	gfc->psymodel = 2;
	gfc->noise_shaping_amp = 2;
	gfc->use_best_huffman = 1;
	break;

    case 3:
	gfc->filter_type = 1;
	gfc->psymodel = 2;
	gfc->noise_shaping_amp = 3;
	gfc->use_best_huffman = 1;
	break;

    case 2:	/* aliased -h */
	gfc->filter_type = 1;
	gfc->psymodel = 2;
	gfc->noise_shaping_amp = 4;
	gfc->use_best_huffman = 1;
	break;

    case 1:	/* LAME 3.94's -h + alpha */
        gfc->filter_type = 1;
        gfc->psymodel = 2;
        gfc->noise_shaping_amp = 4;
        gfc->use_best_huffman = 2; /* inner loop, PAINFULLY SLOW */
        break;

    case 0:	/* same with -q 1 */
        gfc->filter_type = 1; /* 2 not yet coded */
        gfc->psymodel = 2;
        gfc->noise_shaping_amp = 4; /* 5 not yet coded */
        gfc->use_best_huffman = 2;
        break;
    }
}

#define ABS(A) (((A)>0) ? (A) : -(A))

static int
FindNearestBitrate(int bRate, int version)
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
    switch (sample_freq) {
    case 44100: case 22050: case 11025:	return  0;
    case 48000: case 24000: case 12000:	return  1;
    case 32000: case 16000: case  8000:	return  2;
    }
    return -1;
}


static int
BitrateIndex(int bRate, int version)
{
    int  i;
    for (i = 0; i <= 14; i++)
	if (bitrate_table[version][i] == bRate)
	    return i;

    return -1;
}

static int
get_bitrate(lame_t gfc)
{
    /* assume 16bit linear PCM input, and in_sample = out_sample */
    return (int)(gfc->in_samplerate * 16 * gfc->channels_out
		 / (FLOAT)((FLOAT)1e3 * gfc->compression_ratio) + (FLOAT)0.5);
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
	    / (FLOAT)(1000 * gfc->mean_bitrate_kbps);
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

static int
apply_preset(lame_t gfc, int bitrate, vbr_mode mode)
{
    int lower_range, lower_range_kbps, upper_range, upper_range_kbps;
    int r, b;

    if (gfc->out_samplerate != 0 && gfc->out_samplerate <= 24000
	&& gfc->VBR == vbr) {
	bitrate /= 2;
    }

    for (b = 1; b < (int) (sizeof(switch_map)/sizeof(switch_map[0])-1)
	     && bitrate > switch_map[b].kbps; b++)
	;

    upper_range_kbps = switch_map[b].kbps;    upper_range = b;
    lower_range_kbps = switch_map[b-1].kbps;  lower_range = b-1;

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
	    lame_set_scale( gfc, switch_map[r].scale );
    }

    if (gfc->ATHcurve < 0)
	lame_set_ATHcurve(gfc, switch_map[r].ath_curve);
    if (gfc->ATHlower < 0)
	lame_set_ATHlower(gfc, switch_map[r].ath_lower);

    if (gfc->lowpassfreq == 0) {
	int lowpass = switch_map[r].lowpass;
	if (gfc->quality > 7)
	    lowpass *= (FLOAT)0.9;
	if (gfc->mode == MONO)
	    lowpass *= (FLOAT)1.5;
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
 */
    int suggested_samplefreq = 48000;
    if (lowpassfreq <= 15960) suggested_samplefreq = 44100;
    if (lowpassfreq <= 15250) suggested_samplefreq = 32000;
    if (lowpassfreq <= 11220) suggested_samplefreq = 24000;
    if (lowpassfreq <=  9970) suggested_samplefreq = 22050;
    if (lowpassfreq <=  7230) suggested_samplefreq = 16000;
    if (lowpassfreq <=  5420) suggested_samplefreq = 12000;
    if (lowpassfreq <=  4510) suggested_samplefreq = 11025;
    if (lowpassfreq <=  3970) suggested_samplefreq = 8000;

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
 *   initialize internal params based on data in gfc
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
 *  3. compute bitrate and output samplerate:
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
    int gr, ch, sfb;

#ifdef HAVE_NASM
    extern int  haveUNITa ( void );
    int unit = haveUNITa ();

#define MU_tFPU		(1<<0)
#define MU_tMMX		(1<<1)
#define MU_t3DN		(1<<2)
#define MU_tSSE		(1<<3)
#define MU_tCMOV	(1<<4)
#define MU_tE3DN	(1<<5)
#define MU_tEMMX	(1<<6)
#define MU_tSSE2	(1<<7)
#define MU_tINTEL	(1<<8)
#define MU_tAMD		(1<<9)
#define MU_tCYRIX	(1<<10)
#define MU_tIDT		(1<<11)
#define MU_tUNKNOWN	(1<<15)
#define MU_tSPC1	(1<<16)
#define MU_tSPC2	(1<<17)
#define MU_tCLFLUSH	(1<<18)

    gfc->CPU_features.MMX
	= gfc->CPU_features.SSE = gfc->CPU_features.SSE2
	= gfc->CPU_features.AMD_3DNow = gfc->CPU_features.AMD_E3DNow = 0;

    if (gfc->asm_optimizations.mmx) {
        gfc->CPU_features.MMX  = !!(unit & MU_tMMX);
        gfc->CPU_features.MMX2 = !!(unit & MU_tEMMX);
    }

    if (gfc->asm_optimizations.amd3dnow ) {
	gfc->CPU_features.AMD_3DNow  = !!(unit & MU_t3DN);
	gfc->CPU_features.AMD_E3DNow = !!(unit & MU_tE3DN);
    }

    if (gfc->asm_optimizations.sse) {
	gfc->CPU_features.SSE  = !!(unit & MU_tSSE);
	gfc->CPU_features.SSE2 = !!(unit & MU_tSSE2);
    }
#endif

    if (gfc->in_samplerate < 0)
	gfc->in_samplerate = 44100;

    gfc->Class_ID = 0;
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
	int cutoff = gfc->lowpassfreq;
	if (cutoff < 0)
	    cutoff = gfc->in_samplerate/2;
	gfc->out_samplerate = optimum_samplefreq(cutoff, gfc->in_samplerate);
    }
    gfc->samplerate_index = SmpFrqIndex(gfc->out_samplerate);
    if (gfc->samplerate_index < 0)
        return LAME_BADSAMPFREQ;

    /* apply user driven high pass filter */
    if (gfc->highpassfreq > 0) {
	gfc->highpass1 = (FLOAT)2. * gfc->highpassfreq;

	if (gfc->highpasswidth >= 0)
	    gfc->highpass2 = (FLOAT)2. * (gfc->highpassfreq + gfc->highpasswidth);
	else            /* 0% above on default */
	    gfc->highpass2 = (FLOAT)2. * gfc->highpassfreq;

	gfc->highpass1 /= gfc->out_samplerate;
	gfc->highpass2 /= gfc->out_samplerate;
    }

    /* apply user driven low pass filter */
    if (gfc->lowpassfreq > 0) {
	gfc->lowpass2 = (FLOAT)2. * gfc->lowpassfreq;
	if (gfc->lowpasswidth >= 0) {
	    gfc->lowpass1 = (FLOAT)2. * (gfc->lowpassfreq - gfc->lowpasswidth);
	    if (gfc->lowpass1 < 0)
		gfc->lowpass1 = 0;
	}
	else          /* 0% below on default */
	    gfc->lowpass1 = (FLOAT)2. * gfc->lowpassfreq;

	gfc->lowpass1 /= gfc->out_samplerate;
	gfc->lowpass2 /= gfc->out_samplerate;
    }

    gfc->version = 1;
    if (gfc->out_samplerate <= 24000)
	gfc->version = 0;

    gfc->mode_gr = gfc->version+1;
    gfc->framesize = 576 * gfc->mode_gr;
    gfc->mf_needed = ENCDELAY + FFTOFFSET + gfc->framesize + 480;
    gfc->mf_size = gfc->mf_needed - 576 + MDCTDELAY;
    gfc->resample.ratio = (FLOAT) gfc->in_samplerate / gfc->out_samplerate;
    assert(gfc->mf_needed <= MFSIZE);
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
	    return LAME_BADBITRATE;
	gfc->VBR_max_bitrate_kbps = gfc->VBR_min_bitrate_kbps
	    = gfc->mean_bitrate_kbps;
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
	    gfc->report.errorf("Analyzer needs psymodel (-q 8 or lesser).\n");
	}
    }
#endif
    gfc->frameNum=0;
    if (InitGainAnalysis(&gfc->rgdata, gfc->out_samplerate) == INIT_GAIN_ANALYSIS_ERROR)
        return -6;

#ifdef HAVE_MPGLIB
    if (gfc->decode_on_the_fly && !gfc->pmp_replaygain) {
	int ret = decode_init_for_replaygain(gfc);
	if (ret < 0)
	    return ret;
    }
#endif
    init_bitstream_w(gfc);

    gfc->Class_ID = LAME_ID;

    if (gfc->VBR != cbr && gfc->quality > 7) {
	gfc->quality = 7;
	gfc->report.errorf("currently, VBR/ABR mode with -q > 7 setting is meaningless.\n");
    }

    if (gfc->noise_shaping_amp > 4)
	gfc->noise_shaping_amp = 4;

    /* initialize internal qval settings */
    init_qval(gfc);

    iteration_init(gfc);
    psymodel_init(gfc);

    if (gfc->VBR == abr) {
	gfc->masking_lower *= (FLOAT)2.0;
	gfc->masking_lower_short *= (FLOAT)2.0;
    }

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
	    gr_info *gi = &gfc->tt[gr][ch];
	    gi->block_type = NORM_TYPE;
	    gi->mixed_block_flag = gi->subblock_gain[0] = 0;
	    gi->global_gain = 210;
	}
    }

    /* calculate width information */
    /* long */
    for (sfb = 0; sfb < SBMAX_l; sfb++) {
	gfc->w_long[sfb].width
	    = gfc->scalefac_band.l[sfb] - gfc->scalefac_band.l[sfb+1];
	gfc->w_long[sfb].window = 0;
    }
    gfc->w_long[sfb-1].width
	= gfc->scalefac_band.l[sfb-1] - gfc->xrNumMax_longblock;
    /* short */
    for (sfb = 0; sfb < SBMAX_s; sfb++) {
	int start = gfc->scalefac_band.s[sfb];
	int end   = gfc->scalefac_band.s[sfb + 1];
	int subwin;
	for (subwin = 0; subwin < 3; subwin++) {
	    gfc->w_short[sfb*3+subwin].width  = start - end;
	    gfc->w_short[sfb*3+subwin].window = subwin+1;
	}
    }
    /* mixed */
    memcpy(gfc->w_mixed, gfc->w_long, sizeof(winfo_t)*8);
    for (sfb = 3; sfb < SBMAX_s; sfb++) {
	int start = gfc->scalefac_band.s[sfb];
	int end   = gfc->scalefac_band.s[sfb + 1];
	int subwin;
	for (subwin = 0; subwin < 3; subwin++) {
	    gfc->w_mixed[sfb*3+subwin-1].width  = start - end;
	    gfc->w_mixed[sfb*3+subwin-1].window = subwin+1;
	}
    }
    return 0;
}

/*
 * print_config
 *  Prints some selected information about the coding parameters via 
 *  gfc->report.msgf().
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
	gfc->report.msgf("CPU features:");

        if (gfc->CPU_features.MMX) {
            gfc->report.msgf("MMX");
	    if (!gfc->asm_optimizations.sse || !gfc->CPU_features.SSE2)
		gfc->report.msgf(" (ASM used)");
	}
        if (gfc->CPU_features.MMX2)
            gfc->report.msgf(", MMX2");
        if (gfc->CPU_features.AMD_3DNow)
            gfc->report.msgf(", 3DNow! (ASM used)");
        if (gfc->CPU_features.AMD_E3DNow)
            gfc->report.msgf(", E3DNow! (ASM used)");
        if (gfc->CPU_features.SSE)
            gfc->report.msgf(", SSE (ASM used)");
        if (gfc->CPU_features.SSE2)
            gfc->report.msgf(", SSE2 (ASM used)");
        gfc->report.msgf("\n");
    }
#endif

    if (gfc->channels_in == 2 && gfc->channels_out == 1 /* mono */ ) {
        gfc->report.msgf("Autoconverting from stereo to mono. Setting encoding to mono mode.\n");
    }

    if (gfc->resample.ratio != 1.) {
        gfc->report.msgf("Resampling:  input %g kHz  output %g kHz\n",
			 1.e-3 * in_samplerate, 1.e-3 * out_samplerate);
    }


    if (gfc->filter_type <= 1) {
        if (((FLOAT)0. < gfc->highpass2 && gfc->highpass2 < (FLOAT)1.0)
	    || ((FLOAT)0. < gfc->lowpass1 && gfc->lowpass1 < (FLOAT)1.0)) {
	    if (gfc->filter_type == 0) {
		gfc->report.msgf("Using polyphase filter\n");
	    }
	    else if (gfc->filter_type == 1) {
		gfc->report.msgf("Using MDCT filter\n");
	    }
	}
	if (gfc->lowpass1 >= (FLOAT)1.0 && gfc->psymodel) {
	    gfc->report.msgf("Adaptive lowpass filtering\n");
	}
        if ((FLOAT)0. < gfc->highpass2 && gfc->highpass2 < (FLOAT)1.0)
	    gfc->report.msgf(
		"  Highpass filter, transition band: %5.0f Hz - %5.0f Hz\n",
		(FLOAT)0.5 * gfc->highpass1 * out_samplerate,
		(FLOAT)0.5 * gfc->highpass2 * out_samplerate);
	if ((FLOAT)0. < gfc->lowpass1 && gfc->lowpass1 < (FLOAT)1.0) {
	    gfc->report.msgf(
		"  Lowpass  filter, transition band: %5.0f Hz - %5.0f Hz\n",
		(FLOAT)0.5 * gfc->lowpass1 * out_samplerate,
		(FLOAT)0.5 * gfc->lowpass2 * out_samplerate);
	}
    } else {
	gfc->report.msgf("lowpass/highpass filters are disabled\n");
    }

    if (gfc->free_format) {
        gfc->report.msgf(
	    "Warning: many decoders cannot handle free format bitstreams\n");
	if (gfc->mean_bitrate_kbps > 320) {
	    gfc->report.msgf(
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
    const char * pc;

    /*  compiler/processor optimizations, operational, etc.
     */
    gfc->report.msgf("\nmisc:\n\n" );
    gfc->report.msgf("\tscaling: \n");
    gfc->report.msgf("\t ^ ch0 (left) : %f\n", gfc->scale_left );
    gfc->report.msgf("\t ^ ch1 (right): %f\n", gfc->scale_right );
    switch (gfc->filter_type) {
    case 0:  pc = "Polyphase"; break;
    case 1:  pc = "MDCT";      break;
    default: pc = "none";      break;
    }
    gfc->report.msgf("\tfilter type: %s\n", pc);
    gfc->report.msgf("\t...\n" );

    /*  everything controlling the stream format 
     */
    gfc->report.msgf("\nstream format:\n\n" );
    switch ( gfc->version ) {
    case 0:  pc = "2.5"; break;
    case 1:  pc = "1";   break;
    case 2:  pc = "2";   break;
    default: pc = "?";   break;
    }
    gfc->report.msgf("\tMPEG-%s Layer 3\n", pc );
    switch ( gfc->mode ) {
    case JOINT_STEREO: pc = "joint stereo";   break;
    case STEREO      : pc = "stereo";         break;
    case DUAL_CHANNEL: pc = "dual channel";   break;
    case MONO        : pc = "mono";           break;
    case NOT_SET     : pc = "not set (error)"; break;
    default          : pc = "unknown (error)"; break;
    }
    gfc->report.msgf("\t%d channel - %s\n", gfc->channels_out, pc );
    if (gfc->use_istereo)
	gfc->report.msgf("\t ^ using intensity stereo, the ratio is %f\n",
			 1.0-gfc->istereo_ratio);

    pc = "";
    if (gfc->free_format)    pc = "(free format)";
    switch ( gfc->VBR ) {
    case cbr : gfc->report.msgf("\tconstant bitrate - CBR %s\n", pc ); break;
    case abr : gfc->report.msgf("\tvariable bitrate - ABR\n"        ); break;
    case vbr : gfc->report.msgf("\tvariable bitrate - VBR\n"        ); break;
    default  : gfc->report.msgf("\t ?? oops, some new one ?? \n"    ); break;
    }
    if (gfc->bWriteVbrTag) 
	gfc->report.msgf("\tusing LAME Tag\n" );
    gfc->report.msgf("\t...\n" );

    /*  everything controlling psychoacoustic settings, like ATH, etc.
     */
    gfc->report.msgf("\npsychoacoustic:\n\n" );
    gfc->report.msgf("\tshort block switching threshold: %f\n",
		     gfc->nsPsy.attackthre);
    gfc->report.msgf("\tpsymodel: %s\n", gfc->psymodel ? "used" : "not used");
    
    pc = "using";
    if ( gfc->ATHshort ) pc = "the only masking for short blocks";
    if ( gfc->ATHonly  ) pc = "the only masking";
    if ( gfc->noATH    ) pc = "not used";
    gfc->report.msgf("\tATH: %s\n", pc );
    gfc->report.msgf("\t ^ shape factor: %g\n", gfc->ATHcurve);
    gfc->report.msgf("\t ^ level adjustement: %f (dB)\n", gfc->ATHlower);
    gfc->report.msgf("\t ^ adaptive adjustment decay (dB): %f\n",
		     FAST_LOG10(gfc->ATH.aa_decay) * 10.0);

    gfc->report.msgf("\tadjust masking: %f dB\n", gfc->VBR_q-4.0 );
    gfc->report.msgf("\t ^ bass=%g dB, alto=%g dB, treble=%g dB, sfb21=%g dB\n",
		     gfc->nsPsy.tuneBass*0.25,
		     gfc->nsPsy.tuneAlto*0.25,
		     gfc->nsPsy.tuneTreble*0.25,
		     gfc->nsPsy.tuneSFB21*0.25);

/*    gfc->report.msgf("\ttemporal masking sustain factor: %f\n", gfc->nsPsy.decay);*/
    gfc->report.msgf("\tinterchannel masking ratio: %f\n", gfc->interChRatio );
    gfc->report.msgf("\treduce side channel PE factor: %f\n", 1.0-gfc->reduce_side);
    gfc->report.msgf("\tnarrowen stereo factor: %f\n", gfc->narrowStereo*2.0);
    gfc->report.msgf("\t...\n" );

    gfc->report.msgf("\nnoisechaping & quantization:\n\n" );
    switch( gfc->use_best_huffman ) {
    default: pc = "normal"; break;
    case  1: pc = "best (outside loop)"; break;
    case  2: pc = "best (inside loop, slow)"; break;
    }
    gfc->report.msgf("\thuffman search: %s\n", pc );
    if (gfc->VBR != vbr) {
	gfc->report.msgf("\tnoise shaping: %s\n", gfc->psymodel > 1 ? "used" : "not used");
	gfc->report.msgf("\t ^ amplification: %d\n", gfc->noise_shaping_amp );
	gfc->report.msgf("\tallow large scalefactor range=%s\n",
	      gfc->use_scalefac_scale ? "yes" : "no");
	gfc->report.msgf("\tuse subblock gain=%s\n",
	      gfc->use_subblock_gain ? "yes" : "no");
    }
    gfc->report.msgf("\tsubstep shaping=short blocks: %s, long blocks: %s\n",
	  gfc->substep_shaping&2 ? "yes" : "no",
	  gfc->substep_shaping&1 ? "yes" : "no" );
    gfc->report.msgf("\t...\n" );

    /*  that's all ?
     */
    gfc->report.msgf("\n" );
    return;
}


static int
update_buffer(lame_t gfc, int nsamples)
{
    if (gfc->Class_ID != LAME_ID)
        return LAME_NOTINIT;

    if (nsamples == 0)
	return 0;

    nsamples *= sizeof(sample_t) * gfc->channels_in;
    if (gfc->nsamples < nsamples) {
	gfc->in_buffer = realloc(gfc->in_buffer, nsamples);
	if (!gfc->in_buffer) {
	    return LAME_NOMEM;
	}
	gfc->nsamples = nsamples;
    }
    return 1;
}



int
lame_encode_buffer(lame_t gfc,
                   const short int buffer_l[], const short int buffer_r[],
                   const int nsamples, unsigned char *mp3buf,
		   const int mp3buf_size)
{
    int     ret, i;

    ret = update_buffer(gfc, nsamples);
    if (ret <= 0)
        return ret;

    /* make a copy of input buffer, changing type to sample_t */
    for (i = 0; i < nsamples; i++)
        gfc->in_buffer[i] = buffer_l[i] * gfc->scale_left;

    if (gfc->channels_in>1) {
	for (i = 0; i < nsamples; i++)
	    gfc->in_buffer[i+nsamples] = buffer_r[i] * gfc->scale_right;
    }

    return encode_buffer_sample(gfc, nsamples, mp3buf, mp3buf_size);
}

int
lame_encode_buffer_float(lame_t gfc,
			 const float buffer_l[], const float buffer_r[],
			 const int nsamples, unsigned char *mp3buf,
			 const int mp3buf_size)
{
    int     ret, i;

    ret = update_buffer(gfc, nsamples);
    if (ret <= 0)
        return ret;

    /* make a copy of input buffer, changing type to sample_t */
    for (i = 0; i < nsamples; i++) {
        gfc->in_buffer[i] = buffer_l[i] * gfc->scale_left;
    }
    if (gfc->channels_in>1)
	for (i = 0; i < nsamples; i++)
	    gfc->in_buffer[i+nsamples] = buffer_r[i] * gfc->scale_right;

    return encode_buffer_sample(gfc, nsamples, mp3buf, mp3buf_size);
}

int
lame_encode_buffer_float2(lame_t gfc,
			  const float buffer_l[], const float buffer_r[],
			  const int nsamples, unsigned char *mp3buf,
			  const int mp3buf_size)
{
    int     ret, i;
    FLOAT scale;

    ret = update_buffer(gfc, nsamples);
    if (ret <= 0)
        return ret;

    /* make a copy of input buffer, changing type to sample_t */
    scale = gfc->scale_left * (FLOAT)(1.0 / 65536.0);
    for (i = 0; i < nsamples; i++) {
	gfc->in_buffer[i] = buffer_l[i] * scale;
    }
    if (gfc->channels_in>1) {
	scale = gfc->scale_right * (FLOAT)(1.0 / 65536.0);
	for (i = 0; i < nsamples; i++)
	    gfc->in_buffer[i+nsamples] = buffer_r[i] * scale;
    }

    return encode_buffer_sample(gfc, nsamples, mp3buf, mp3buf_size);
}

int
lame_encode_buffer_int(lame_t gfc,
		       const int buffer_l[], const int buffer_r[],
		       const int nsamples, unsigned char *mp3buf,
		       const int mp3buf_size)
{
    int     ret, i;
    FLOAT scale;

    ret = update_buffer(gfc, nsamples);
    if (ret <= 0)
        return ret;

    /* make a copy of input buffer, changing type to sample_t */
    scale = gfc->scale_left * (FLOAT)(1.0 / ( 1L << (8 * sizeof(int) - 16)));
    for (i = 0; i < nsamples; i++)
	gfc->in_buffer[i] = buffer_l[i] * scale;

    if (gfc->channels_in>1) {
	scale = gfc->scale_right * (FLOAT)(1.0 / ( 1L << (8 * sizeof(int) - 16)));
	for (i = 0; i < nsamples; i++)
	    gfc->in_buffer[i+nsamples] = buffer_r[i] * scale;
    }

    return encode_buffer_sample(gfc, nsamples, mp3buf, mp3buf_size);
}

int
lame_encode_buffer_long2(lame_t gfc,
			 const long buffer_l[], const long buffer_r[],
			 const int nsamples, unsigned char *mp3buf,
			 const int mp3buf_size)
{
    int     ret, i;
    FLOAT scale;

    ret = update_buffer(gfc, nsamples);
    if (ret <= 0)
        return ret;

    /* make a copy of input buffer, changing type to sample_t */
    scale = gfc->scale_left * (FLOAT)(1.0 / (1L << (8 * sizeof(long) - 16)));
    for (i = 0; i < nsamples; i++)
	gfc->in_buffer[i] = buffer_l[i] * scale;

    if (gfc->channels_in>1) {
	scale = gfc->scale_right * (FLOAT)(1.0 / (1L << (8 * sizeof(long) - 16)));
	for (i = 0; i < nsamples; i++)
	    gfc->in_buffer[i+nsamples] = buffer_r[i] * scale;
    }

    return encode_buffer_sample(gfc, nsamples, mp3buf, mp3buf_size);
}

int
lame_encode_buffer_long(lame_t gfc,
			const long buffer_l[], const long buffer_r[],
			const int nsamples, unsigned char *mp3buf,
			const int mp3buf_size)
{
    int     ret, i;

    ret = update_buffer(gfc, nsamples);
    if (ret <= 0)
        return ret;

    /* make a copy of input buffer, changing type to sample_t */
    for (i = 0; i < nsamples; i++) {
	gfc->in_buffer[i] = buffer_l[i] * gfc->scale_left;
	if (gfc->channels_in>1)
	    gfc->in_buffer[i+nsamples] = gfc->scale_right * buffer_r[i];
    }

    return encode_buffer_sample(gfc, nsamples, mp3buf, mp3buf_size);
}

int
lame_encode_buffer_interleaved(lame_t gfc,
                               short int buffer[], int nsamples,
                               unsigned char *mp3buf, int mp3buf_size)
{
    int     ret, i;

    ret = update_buffer(gfc, nsamples);
    if (ret <= 0)
        return ret;

    for (i = 0; i < nsamples; i++) {
        gfc->in_buffer[i] = buffer[2 * i]    * gfc->scale_left;
        gfc->in_buffer[i+nsamples] = buffer[2 * i + 1]* gfc->scale_right;
    }
    return encode_buffer_sample(gfc, nsamples, mp3buf, mp3buf_size);
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
    return flush_bitstream(gfc, mp3buffer, mp3buffer_size);
}


/*****************************************************************/
/* flush internal PCM sample buffers, then mp3 buffers           */
/* then write id3 v1 tags into bitstream.                        */
/*****************************************************************/
int
lame_encode_flush(lame_t gfc, unsigned char *mp3buf, int mp3buffer_size)
{
/* current state
 *     |------------------LAST|.....|
 *     <----------------------------> mf_needed
 *     <--------mf_size-------X----->  mf_needed - mf_size
 *
 * after flush,
 * LAST|.....00000000000000000|00000|
 *           <---------------------->mf_size
 *     <---------------------------->mf_needed
 *     <-----> mf_needed - mf_size
 */
    int imp3, mp3count = 0, mp3buffer_size_remaining = mp3buffer_size;
    int samples_to_encode = gfc->mf_size + gfc->framesize;
    int i = (samples_to_encode + POSTDELAY - 1) / gfc->framesize;

    /* send in a frame of 0 padding until all internal sample buffers
     * are flushed
     */
    gfc->encoder_padding = i * gfc->framesize - samples_to_encode;

    imp3 = update_buffer(gfc, i * gfc->framesize);
    if (imp3 < 0)
	return imp3;

    if (imp3 > 0) {
	memset(gfc->in_buffer, 0,
	       gfc->channels_in*i*gfc->framesize*sizeof(sample_t));

	imp3 = encode_buffer_sample(gfc, i * gfc->framesize,
				    mp3buf, mp3buffer_size_remaining);
	if (imp3 < 0) return imp3;

	if (mp3buffer_size)
	    mp3buffer_size_remaining -= imp3;
	mp3buf += imp3;
	mp3count += imp3;
    }

    /* bit buffer might still contain some mp3 data */
    imp3 = flush_bitstream(gfc, mp3buf, mp3buffer_size_remaining);
    if (imp3 < 0) return imp3;

    if (mp3buffer_size)
	mp3buffer_size_remaining -= imp3;
    mp3buf += imp3;
    mp3count += imp3;

    /* write a id3 tag to the bitstream */
    imp3 = id3tag_write_v1(gfc, mp3buf, mp3buffer_size_remaining);
    if (imp3 < 0) return imp3;

    return mp3count + imp3;
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
        return LAME_NOTINIT;

    gfc->Class_ID = 0;

    /* free all malloc'd data in gfc, and then free gfc: */
    if (gfc->resample.blackfilt) {
	free(gfc->resample.blackfilt);
	gfc->resample.blackfilt = NULL;
    }
    for (i = 0; i < MAX_CHANNELS; i++) {
	if (gfc->resample.inbuf_old[i]) { 
	    free(gfc->resample.inbuf_old[i]);
	    gfc->resample.inbuf_old[i] = NULL;
	}
    }

    if (gfc->seekTable.bag) {
	free(gfc->seekTable.bag);
	gfc->seekTable.bag=NULL;
	gfc->seekTable.size=0;
    }

    if (gfc->s3_ll) free(gfc->s3_ll);
    if (gfc->s3_ss) free(gfc->s3_ss);

    if (gfc->in_buffer) free(gfc->in_buffer);

#ifdef HAVE_MPGLIB
    lame_decode_exit(gfc);
#endif

    free((unsigned char*)gfc - gfc->alignment);

    return 0;
}

/****************************************************************/
/* initialize mp3 encoder					*/
/****************************************************************/
static void
msgf(const char* format, ... )
{
    va_list  args;
    va_start ( args, format );

    vfprintf ( stderr, format, args );

    fflush ( stderr );      /* an debug function should flush immediately */
    va_end   ( args );
}

lame_t
lame_init(void)
{
    lame_t gfc;
    void *work;
#ifndef NDEBUG
    if (sizeof(gr_info) & 63) {
	printf("alignment error. gr_info size = %d\n", (int)sizeof(gr_info));
	return NULL;
    }
#endif
    disable_FPE();      /* disable floating point exceptions */

    work = calloc(1, sizeof(struct lame_internal_flags) + 64);
    if (!work)
	return NULL;

    gfc = (lame_t)(((unsigned long)work + 63) & ~63);
    gfc->alignment = (unsigned char*)gfc - (unsigned char*)work;

    gfc->report.debugf = gfc->report.msgf = gfc->report.errorf = msgf;

    /* Global flags.  set defaults here for non-zero values */
    /* set integer values to -1 to mean that LAME will compute the
     * best value, UNLESS the calling program as set it
     * (and the value is no longer -1)
     */
    gfc->mode = NOT_SET;
    gfc->original = 1;
    gfc->in_samplerate = -1;
    gfc->channels_in = 2;
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

    gfc->OldValue[0] = gfc->OldValue[1] = 210;
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

    gfc->encoder_padding = 0;

#ifdef HAVE_NASM
    gfc->asm_optimizations.mmx = 1;
    gfc->asm_optimizations.amd3dnow = 1;
    gfc->asm_optimizations.sse = 1;
#endif

    id3tag_init (gfc);

    return gfc;
}

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
    return 0;
}

#ifdef BRHIST
/***********************************************************************
 *
 *  some simple statistics
 *
 *  Robert Hegemann 2000-10-11
 *
 ***********************************************************************/
void
lame_bitrate_hist(lame_t gfc, int bitrate_count[14])
{
    int     i, j;

    if (!bitrate_count || !gfc)
	return;

    for (i = 0; i < 14; i++) {
	int total = 0;
	for (j = 0; j < 5; j++)
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
	for (i = 0; i < 5; i++)
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

/*2DO rh 20021015
  I thought BRHIST was only for the frontend, so that clients
  may use these stats, even if it's only a Windows DLL
  I'll extend the stats for block types used
*/
static void
updateStats(lame_t gfc)
{
    int gr, ch;
    assert((unsigned int)gfc->bitrate_index < 16u);
    assert((unsigned int)gfc->mode_ext      <  4u);

    /* count 'em for every mode extension in case of 2 channel encoding */
    if (gfc->channels_out == 2)
	gfc->bitrate_stereoMode_Hist[gfc->bitrate_index] [gfc->mode_ext]++;
    else
	gfc->bitrate_stereoMode_Hist[gfc->bitrate_index][4]++;

    for (gr = 0; gr < gfc->mode_gr; gr++) {
	for (ch = 0; ch < gfc->channels_out; ch++) {
	    int bt = gfc->tt[gr][ch].block_type;
	    if (bt == SHORT_TYPE && gfc->tt[gr][ch].mixed_block_flag)
		bt = 4;
	    gfc->bitrate_blockType_Hist [gfc->bitrate_index] [bt] ++;
	    gfc->bitrate_blockType_Hist [15] [bt] ++;
	}
    }
}
#endif

static void
conv_istereo(lame_t gfc, gr_info *gi, int sfb, int i)
{
    for (; sfb < gi[0].psymax && i < gi->xrNumMax; sfb++) {
	FLOAT lsum = 1e-30, rsum = 1e-30;
	int j = i - gi->wi[sfb].width;
	do {
	    FLOAT l = gi[0].xr[i];
	    FLOAT r = gi[1].xr[i];
	    gi[0].xr[i] = l+r;
	    gi[1].xr[i] = 0.0;
	    lsum += fabs(l);
	    rsum += fabs(r);
	} while (++i < j);

	lsum = lsum / (lsum+rsum);
	j = 3;
	if (lsum < (FLOAT)0.5) {
	    if (lsum < (FLOAT)(0.211324865 * 0.5))
		j = 0;
	    else if (lsum < (FLOAT)((0.366025404 + 0.211324865) * 0.5))
		j = 1;
	    else if (lsum < (FLOAT)((0.5 + 0.366025404) * 0.5))
		j = 2;
	} else {
	    if (lsum > (FLOAT)(1.0-0.211324865 * 0.5))
		j = 6;
	    else if (lsum > (FLOAT)(1.0-(0.366025404 + 0.211324865) * 0.5))
		j = 5;
	    else if (lsum > (FLOAT)(1.0-(0.5 + 0.366025404) * 0.5))
		j = 4;
	}
	gi[1].scalefac[sfb] = j;
    }
    gfc->scale_bitcounter(&gi[1]);
}


static void
init_gr_info(lame_t gfc, int gr, int ch)
{
    gr_info *gi = &gfc->tt[gr][ch];
    int sfb, j;

    /* mixed_block_flag, block_type was set in psymodel.c */
    gi->sfbdivide          = 11;
    gi->xrNumMax           = gfc->xrNumMax_longblock;

    j = gfc->max_sfb_l[ch][gr];
    gi->psymax = gi->psy_lmax = j;
    gi->sfbmax = gi->sfb_lmax = SBPSY_l;
    gi->sfb_smin              = SBPSY_s;
    gi->wi = gfc->w_long;

    if (gi->block_type != NORM_TYPE) {
	gi->region0_count = 7;
	if (gi->block_type == SHORT_TYPE) {
	    FLOAT ixwork[576];
	    FLOAT *ix;

	    if (gfc->mode_gr == 1)
		gi->region0_count = 5;
	    gi->sfb_smin        = 0;
	    gi->sfb_lmax        = 0;
	    gi->wi = gfc->w_short;
	    if (gi->mixed_block_flag) {
		/*
		 *  MPEG-1:      sfbs 0-7 long block, 3-12 short blocks
		 *  MPEG-2(.5):  sfbs 0-5 long block, 3-12 short blocks
		 */ 
		gi->sfb_smin    = 3;
		gi->sfb_lmax    = gfc->mode_gr*2 + 4;
		gi->wi = gfc->w_mixed;
	    }
	    j = gfc->max_sfb_s[ch][gr];
	    gi->psymax = gi->sfb_lmax + 3*(j - gi->sfb_smin);
	    gi->sfbmax = gi->sfb_lmax + 3*(SBPSY_s - gi->sfb_smin);
	    gi->sfbdivide   = gi->sfbmax - 18;
	    gi->psy_lmax    = gi->sfb_lmax;
	    gi->xrNumMax = gfc->scalefac_band.s[gi->psymax/3]*3;
	    /*
	     * Re-order the short blocks, for more efficient encoding.
	     * Within each scalefactor band, data is given for successive
	     * time windows, beginning with window 0 and ending with window 2.
	     * Within each window, the quantized values are then arranged in
	     * order of increasing frequency.
	     */
	    j = gi->sfb_lmax;
	    ix = &gi->xr[gfc->scalefac_band.l[j]];
	    memcpy(ixwork, gi->xr, sizeof(ixwork));
	    for (sfb = gi->sfb_smin; sfb < SBMAX_s; sfb++) {
		int start = gfc->scalefac_band.s[sfb];
		int end   = gfc->scalefac_band.s[sfb + 1];
		int subwin, l;
		for (subwin = 0; subwin < 3; subwin++) {
		    for (l = start; l < end; l++)
			*ix++ = ixwork[3*l+subwin];
		    j++;
		}
	    }
	}
	gi->region1_count = SBMAX_l - 2 - gi->region0_count;
    } else {
	/* analog silence detection in pseudo sfb 22 */
	if (gfc->scalefac_band.l[SBMAX_l-1] < 576-100) {
	    int k = (576+gfc->scalefac_band.l[SBMAX_l-1])/2;
	    FLOAT power = 0.0;
	    for (j = k; j < 576; j++)
		power += gi->xr[j] * gi->xr[j];
	    if (power < gi->ATHadjust * gfc->ATH.l[SBMAX_l-1]) {
		for (j = k; j < 576; j++)
		    gi->xr[j] = 0;
	    }
	}
    }
}

/*************************************************************************
 encode_mp3_frame()
   encode a single frame for Layer3

gfc->mfbuf: |---------------------- mf_needed --------------------------|

FFT's       |--------------1024--------------|
                               |--------------1024--------------|

            <--------> FFTOFFSET = 272
  |-------(pre.b)576+480----------|
subband in           |---------(a)576+480------------|
                     <------- 576 ----->|-----------(b)576+480----------|

                     <-------> PQF delay = 240

          |---(prev.b)576----|
subband out                  |------(a)576------|------(b)576------|

MDCT in   |----------------1152-----------------|
                             |----------------1152-----------------|
MDCT out:          |--------576-------|--------576-------|

                   <-> 576/2-240 = 48 = MDCTDELAY
            <------> (1024-576)/2 = 224

    MPEG1: subband ends at:  FFTOFFSET+576*2+480 = 1904 = mf_needed
    MPEG2: subband ends at:  FFTOFFSET+576  +480 = 1328 = mf_needed
*************************************************************************/

static int
encode_mp3_frame(lame_t gfc, unsigned char* mp3buf, int mp3buf_size)
{
    int mp3count, ch, gr;
    III_psy_ratio masking[2][MAX_CHANNELS];
    FLOAT sbsmpl[MAX_CHANNELS][1152];

    /* subband filtering in the next frame */
    /* to determine long/short swithcing in psymodel */
    for (ch = 0; ch < gfc->channels_out; ch++)
	subband(gfc, gfc->mfbuf[ch], sbsmpl[ch]);

    if (gfc->psymodel)
	psycho_analysis(gfc, masking, sbsmpl);
    else
	memset(masking, 0, sizeof(masking));

    if (gfc->frameNum++ == 0) {
	mp3count = 0;
	goto exit_encode_frame;
    }

    /* padding (by Robert.Hegemann at gmx.de 2000-06-22) */
    gfc->padding = FALSE;
    if ((gfc->slot_lag -= gfc->frac_SpF) < 0) {
	gfc->slot_lag += gfc->out_samplerate;
	gfc->padding = TRUE;
    }

    /* mdct */
    for (ch = 0; ch < gfc->channels_out; ch++) {
	mdct_sub48(gfc, ch);
	for (gr = 0; gr < gfc->mode_gr; gr++)
	    init_gr_info(gfc, gr, ch);
    }

    /* channel conversion */
    if (gfc->narrowStereo != (FLOAT)0.0) {
	/* narrown_stereo */
	for (gr = 0; gr < gfc->mode_gr; gr++) {
	    gr_info *gi = &gfc->tt[gr][0];
	    int i;
	    for (i = 0; i < gi->xrNumMax; i++) {
		FLOAT d = (gi[0].xr[i]-gi[1].xr[i]) * gfc->narrowStereo;
		gi[0].xr[i] -= d;
		gi[1].xr[i] += d;
	    }
	}
    }

    if (gfc->mode_ext == MPG_MD_MS_LR) {
	/* convert from L/R -> Mid/Side */
	if (1
	    && (gfc->tt[0][0].block_type != SHORT_TYPE
		|| gfc->max_sfb_s[1][0] == 0)
	    && (gfc->tt[1][0].block_type != SHORT_TYPE
		|| gfc->max_sfb_s[1][1] == 0)
	    && gfc->use_istereo) {
	    for (gr = 0; gr < gfc->mode_gr; gr++) {
		int sfb = gfc->max_sfb_l[1][gr];
		if (gfc->tt[gr][0].block_type == SHORT_TYPE)
		    sfb = gfc->max_sfb_s[1][gr];
		if (sfb && sfb < gfc->tt[gr][0].psymax) {
		    gfc->mode_ext = MPG_MD_MS_I;
		    break;
		}
	    }
	}

	for (gr = 0; gr < gfc->mode_gr; gr++) {
	    gr_info *gi = &gfc->tt[gr][0];
	    int sfb = gfc->max_sfb_l[1][gr];
	    int end = gfc->scalefac_band.l[sfb];
	    if (gi->block_type == SHORT_TYPE) {
		sfb = gfc->max_sfb_s[1][gr];
		end = gfc->scalefac_band.s[sfb]*3;
		sfb *= 3;
	    }
	    if (end) lr2ms(gfc, gi[0].xr, gi[1].xr, end);
	    if (gfc->mode_ext == MPG_MD_MS_I)
		conv_istereo(gfc, gi, sfb, end);
	}
    } else if (gfc->use_istereo) {
	gfc->mode_ext = MPG_MD_LR_I;
	for (gr = 0; gr < gfc->mode_gr; gr++) {
	    gr_info *gi = &gfc->tt[gr][0];
	    int sfb = gfc->max_sfb_l[1][gr];
	    int end = gfc->scalefac_band.l[sfb];
	    if (gi->block_type == SHORT_TYPE) {
		sfb = gfc->max_sfb_s[1][gr];
		end = gfc->scalefac_band.s[sfb]*3;
		sfb *= 3;
	    }
	    conv_istereo(gfc, gi, sfb, end);
	}
    }

    /* bit and noise allocation */
    switch (gfc->VBR){ 
    default:
    case cbr:	CBR_iteration_loop(gfc, masking); break;
    case vbr:	VBR_iteration_loop(gfc, masking); break;
    case abr:	ABR_iteration_loop(gfc, masking); break;
    }

    /*  write the frame to the bitstream  */
    mp3count = format_bitstream(gfc, mp3buf, mp3buf_size);

    if (gfc->bWriteVbrTag && gfc->VBR != cbr)
	AddVbrFrame(gfc);

#ifndef NOANALYSIS
    if (gfc->pinfo)
	set_frame_pinfo(gfc, masking);
#endif

#ifdef BRHIST
    updateStats( gfc );
#endif

 exit_encode_frame:
    /* shift out old samples/subband filter output */
    gfc->mf_size -= gfc->framesize;
    ch = 0;
    do {
	memcpy(gfc->w.sb_smpl[ch][0], gfc->w.sb_smpl[ch][gfc->mode_gr],
	       sizeof(gfc->w.sb_smpl[0][0]));
	memcpy(gfc->w.sb_smpl[ch][1], sbsmpl[ch],
	       sizeof(gfc->w.sb_smpl[0][0])*gfc->mode_gr);

	memmove(gfc->mfbuf[ch], &gfc->mfbuf[ch][gfc->framesize],
		sizeof(sample_t) * gfc->mf_size);
    } while (++ch < gfc->channels_out);
    return mp3count;
}

/* copy in new samples from in_buffer into mfbuf, with resampling
   if necessary.  n_in = number of samples from the input buffer that
   were used.  n_out = number of samples copied into mfbuf  */

static int
fill_buffer_resample(lame_t gfc, sample_t *outbuf, sample_t *inbuf, int len,
		     int *num_used, int ch)
{
    int i, j, k, filter_l = gfc->resample.filter_l, bpc = gfc->resample.bpc;
    sample_t *inbuf_old = gfc->resample.inbuf_old[ch];

    /* time of j'th element in inbuf = itime + j/ifreq; */
    /* time of k'th element in outbuf   =  j/ofreq */
    k = 0;
    do {
	sample_t xvalue;
	FLOAT time0 = k*gfc->resample.ratio - gfc->resample.itime[ch], offset;
	FLOAT *filter_coef;
	j = floor(time0);

	/* check if we need more input data */
	if ((filter_l + j - filter_l/2) >= len)
	    break;

	/* blackman filter.  by default, window centered at j+.5(filter_l%2) */
	/* but we want a window centered at time0.   */
	offset = time0 - (j + (FLOAT).5*(filter_l & 1));
	assert(fabs(offset)<=(FLOAT).5);

	/* find the closest precomputed window for this offset: */
	filter_coef
	    = gfc->resample.blackfilt
	    + ((int)((offset*2*bpc)+bpc+(FLOAT).5)) * (filter_l+1);

	xvalue = 0.;
	for (i = 0; i <= filter_l; i++) {
	    int j2 = i + j - filter_l/2;
	    sample_t y;
	    assert(j2<len);
	    assert(j2+filter_l+1 >= 0);
	    y = (j2<0) ? inbuf_old[filter_l+1+j2] : inbuf[j2];
	    xvalue += y * filter_coef[i];
	}
	*outbuf++ = xvalue;
	k++;
    } while (outbuf < &gfc->mfbuf[ch][gfc->mf_needed]);
    /* k = number of samples added to outbuf */
    /* last k sample used data from [j-filter_l/2,j+filter_l-filter_l/2]  */

    /* how many samples of input data were used:  */
    *num_used = Min(len, (filter_l+1)/2 + j);

    /* adjust our input time counter.  Incriment by the number of samples used,
     * then normalize so that next output sample is at time 0, next
     * input buffer is at time itime[ch] */
    gfc->resample.itime[ch] += *num_used - k*gfc->resample.ratio;

    /* save the last BLACKSIZE samples into the inbuf_old buffer */
    j = *num_used-filter_l-1;
    if (j >= 0) {
	inbuf += j;
	j = 0;
    } else {
	/* shift in *num_used samples into inbuf_old  */
	j = -j;

	/* shift j samples by *num_used, to make room for the
	 * num_used new samples */
	memcpy(inbuf_old, &inbuf_old[*num_used], sizeof(sample_t) * j);
	inbuf_old += j;
    }
    memcpy(inbuf_old, inbuf, sizeof(sample_t) * (filter_l+1 - j));

    return k;  /* return the number samples created at the new samplerate */
}

static int
fill_buffer(lame_t gfc, sample_t *in_buffer, int nsamples, int *n_in, int ch)
{
    /* copy in new samples into mfbuf, with resampling if necessary */
    if (gfc->resample.ratio != (FLOAT)1.0)
	return fill_buffer_resample(gfc, &gfc->mfbuf[ch][gfc->mf_size],
				    in_buffer, nsamples, n_in, ch);

    *n_in = Min(gfc->mf_needed - gfc->mf_size, nsamples);
    memcpy(&gfc->mfbuf[ch][gfc->mf_size], in_buffer, sizeof(sample_t) * *n_in);
    return *n_in;
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
static int
encode_buffer_sample(
    lame_t gfc, int nsamples, unsigned char *mp3buf, const int mp3buf_size)
{
    int buf_remain = mp3buf_size, i;
    sample_t *lr_buffer[2];
    unsigned char *p = mp3buf;

    if (gfc->frameNum == 0) {
	/* Write ID3v2 TAG at very the beggining */
	i = id3tag_write_v2(gfc, p, buf_remain);
	if (i < 0)
	    return i;  /* not enough buffer space */
	p += i;
	if (mp3buf_size)
	    buf_remain -= i;

	/* Write initial VBR Header to bitstream and init VBR data */
	i = InitVbrTag(gfc);
	if ((mp3buf_size && i >= buf_remain) || i < 0)
	    return LAME_INSUFFICIENTBUF;  /* not enough buffer space */
	memset(p, 0, i);
	p += i;
	if (mp3buf_size)
	    buf_remain -= i;
    }

    /* Downsample to Mono if 2 channels in and 1 channel out */
    lr_buffer[0] = gfc->in_buffer;
    lr_buffer[1] = gfc->in_buffer + nsamples;

    if (gfc->channels_in == 2 && gfc->channels_out == 1) {
	for (i = 0; i < nsamples; i++)
	    lr_buffer[0][i]
		= (FLOAT)0.5 * (lr_buffer[0][i] + lr_buffer[1][i]);
    }

    assert(nsamples > 0);

    do {
	int n_in, n_out, ch;
	/* copy the new samples into gfc->mfbuf (with resampling if needed)
	 * it consumes (n_in) samples from in_buffer,
	 * and output (n_out) samples in gfc->mfbuf. */
	ch = 0;
	do {
	    n_out = fill_buffer(gfc, lr_buffer[ch], nsamples, &n_in, ch);
	    lr_buffer[ch] += n_in;
	} while (++ch < gfc->channels_out);

        /* compute ReplayGain of resampled input if requested */
        if (gfc->findReplayGain && !gfc->decode_on_the_fly
	    && AnalyzeSamples(&gfc->rgdata, &gfc->mfbuf[0][gfc->mf_size],
			      &gfc->mfbuf[1][gfc->mf_size],
			      n_out, gfc->channels_out) == GAIN_ANALYSIS_ERROR)
	    return -6;

	/* update lr_buffer counters */
	nsamples -= n_in;

	/* update mfbuf[] counters */
	gfc->mf_size += n_out;
	if (gfc->mf_size < gfc->mf_needed)
	    break;

	assert(gfc->mf_size == gfc->mf_needed);
	/* encode the frame.
	 *  p           = pointer to current location in buffer
	 *  mp3buf_size = size of original mp3 output buffer
	 *		  if 0, we should not worry about the buffer size
	 *		  because calling program is to lazy to compute it
	 *  buf_remain  = amount of space avalable
	 */
	i = encode_mp3_frame(gfc, p, buf_remain);
	if (i < 0)
	    return i;

	if (mp3buf_size)
	    buf_remain -= i;
	p += i;
    } while (nsamples > 0);
    assert(nsamples == 0);
    return p - mp3buf;
}
