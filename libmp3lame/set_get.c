/*
 * set/get functions for lame internal flags
 *
 * Copyright (c) 2001 Alexander Leidinger
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* $Id$ */

/* see include/lame.h file for the function details */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#ifdef WITH_DMALLOC
# include <dmalloc.h>
#endif
#include <assert.h>
#include <stdio.h>

#include "lame.h"
#include "version.h"

#include "encoder.h"
#include "bitstream.h"  /* because of compute_flushbits */
#include "set_get.h"

/*
 * input stream description
 */

/* number of samples */
/* it's unlikely for this function to return an error */
int
lame_set_num_samples(lame_t gfc, unsigned long num_samples)
{
    /* default = 2^32-1 */
    gfc->num_samples = num_samples;
    return 0;
}

unsigned long
lame_get_num_samples(lame_t gfc)
{
    return gfc->num_samples;
}


/* input samplerate */
int
lame_set_in_samplerate(lame_t gfc, int in_samplerate)
{
    /* input sample rate in Hz,  default = 44100 Hz */
    gfc->in_samplerate = in_samplerate;
    return 0;
}

int
lame_get_in_samplerate(lame_t gfc)
{
    return gfc->in_samplerate;
}


/* number of channels in input stream */
int
lame_set_num_channels(lame_t gfc, int num_channels)
{
    /* default = 2 */
    if (!(0 < num_channels && num_channels <= 2))
        return -1;    /* we don't support more than 2 channels */

    gfc->channels_in = num_channels;
    return 0;
}

int
lame_get_num_channels(lame_t gfc)
{
    return gfc->channels_in;
}


/* scale the input by this amount before encoding (not used for decoding) */
int
lame_set_scale(lame_t gfc, float scale)
{
    /* default = 0 */
    gfc->scale = scale;
    return 0;
}

float
lame_get_scale(lame_t gfc)
{
    return gfc->scale;
}


/* scale the channel 0 (left) input by this amount before 
   encoding (not used for decoding) */
int
lame_set_scale_left(lame_t gfc, float scale)
{
    /* default = 0 */
    gfc->scale_left = scale;
    return 0;
}

float
lame_get_scale_left(lame_t gfc)
{
    return gfc->scale_left;
}


/* scale the channel 1 (right) input by this amount before 
   encoding (not used for decoding) */
int
lame_set_scale_right(lame_t gfc, float scale)
{
    /* default = 0 */
    gfc->scale_right = scale;
    return 0;
}

float
lame_get_scale_right(lame_t gfc)
{
    return gfc->scale_right;
}


/* output sample rate in Hz */
int
lame_set_out_samplerate(lame_t gfc, int out_samplerate)
{
    gfc->out_samplerate = out_samplerate;
    return 0;
}

int
lame_get_out_samplerate(lame_t gfc)
{
    return gfc->out_samplerate;
}




/*
 * general control parameters
 */

/* write a Xing VBR header frame */
int
lame_set_bWriteVbrTag(lame_t gfc, int bWriteVbrTag)
{
    /* default = 1 (on) for VBR/ABR modes, 0 (off) for CBR mode */

    /* enforce disable/enable meaning, if we need more than two values
       we need to switch to an enum to have an apropriate representation
       of the possible meanings of the value */
    if ( 0 > bWriteVbrTag || 1 < bWriteVbrTag)
        return -1;

    gfc->bWriteVbrTag = bWriteVbrTag;

    return 0;
}

int
lame_get_bWriteVbrTag(lame_t gfc)
{
    assert( 0 <= gfc->bWriteVbrTag && 1 >= gfc->bWriteVbrTag);

    return gfc->bWriteVbrTag;
}



int
lame_set_quality(lame_t gfc, int quality)
{
    if (quality < 0)
	quality = 0;
    if (quality > 9)
	quality = 9;

    gfc->quality = quality;
    return 0;
}

int
lame_get_quality(lame_t gfc)
{
    return gfc->quality;
}


/* mode = STEREO, JOINT_STEREO, DUAL_CHANNEL (not supported), MONO */
int
lame_set_mode(lame_t gfc, MPEG_mode mode)
{
    /* default: lame chooses based on compression ratio and input channels */

    if ((unsigned int)mode >= MAX_INDICATOR)
        return -1;  /* Unknown MPEG mode! */

    gfc->mode = mode;

    return 0;
}

MPEG_mode
lame_get_mode(lame_t gfc)
{
    assert((unsigned int)gfc->mode < MAX_INDICATOR);

    return gfc->mode;
}


/*
 * Force M/S for all frames.  For testing only.
 * Requires mode = 1.
 */
int
lame_set_force_ms(lame_t gfc, int force_ms)
{
    /* default = 0 (disabled) */

    /* enforce disable/enable meaning, if we need more than two values
       we need to switch to an enum to have an apropriate representation
       of the possible meanings of the value */
    if ( 0 > force_ms || 1 < force_ms)
        return -1;

    gfc->force_ms = force_ms;

    return 0;
}

int
lame_get_force_ms(lame_t gfc)
{
    assert( 0 <= gfc->force_ms && 1 >= gfc->force_ms);

    return gfc->force_ms;
}


/* Use free_format. */
int
lame_set_free_format(lame_t gfc, int free_format)
{
    /* default = 0 (disabled) */

    /* enforce disable/enable meaning, if we need more than two values
       we need to switch to an enum to have an apropriate representation
       of the possible meanings of the value */
    if ( 0 > free_format || 1 < free_format)
        return -1;

    gfc->free_format = free_format;

    return 0;
}

int
lame_get_free_format(lame_t gfc)
{
    assert( 0 <= gfc->free_format && 1 >= gfc->free_format);

    return gfc->free_format;
}


/* Perform ReplayGain analysis */
int
lame_set_findReplayGain( lame_t  gfc,
                         int                 findReplayGain )
{
    /* default = 0 (disabled) */

    /* enforce disable/enable meaning, if we need more than two values
       we need to switch to an enum to have an apropriate representation
       of the possible meanings of the value */
    if ( 0 > findReplayGain || 1 < findReplayGain )
        return -1;

    gfc->findReplayGain = findReplayGain;

    return 0;
}

int
lame_get_findReplayGain( lame_t  gfc )
{
    assert( 0 <= gfc->findReplayGain && 1 >= gfc->findReplayGain);

    return gfc->findReplayGain;
}


/* Decode on the fly. Find the peak sample. If ReplayGain analysis is 
   enabled then perform it on the decoded data. */
int
lame_set_decode_on_the_fly( lame_t  gfc,
                            int                 decode_on_the_fly )
{
#ifndef HAVE_MPGLIB
    (void) gfc; (void) decode_on_the_fly;
    return -1;
#else
    /* default = 0 (disabled) */

    /* enforce disable/enable meaning, if we need more than two values
       we need to switch to an enum to have an apropriate representation
       of the possible meanings of the value */
    if ( 0 > decode_on_the_fly || 1 < decode_on_the_fly )
        return -1;

    gfc->decode_on_the_fly = decode_on_the_fly;

    return 0;
#endif
}

int
lame_get_decode_on_the_fly( lame_t  gfc )
{
    assert( 0 <= gfc->decode_on_the_fly && 1 >= gfc->decode_on_the_fly );

    return gfc->decode_on_the_fly;
}

/* set and get some gapless encoding flags */

int
lame_set_nogap_total( lame_t gfc, int the_nogap_total )
{
    gfc->nogap_total = the_nogap_total;
    return 0;
}

int
lame_get_nogap_total( lame_t gfc )
{
    return gfc->nogap_total;
}

int
lame_set_nogap_currentindex( lame_t gfc,
                             int the_nogap_index )
{
    gfc->nogap_current = the_nogap_index;
    return 0;
}

int
lame_get_nogap_currentindex( lame_t gfc )
{
    return gfc->nogap_current;
}
  

/* message handlers */
int
lame_set_errorf(lame_t gfc, void (*func)(const char*, ...))
{
    gfc->report.errorf = func;

    return 0;
}

int
lame_set_debugf(lame_t gfc, void (*func)(const char*, ...))
{
    gfc->report.debugf = func;

    return 0;
}

int
lame_set_msgf(lame_t gfc, void (*func)( const char *, ...))
{
    gfc->report.msgf = func;

    return 0;
}


/*
 * Set one of
 *  - bitrate
 *  - compression ratio.
 *
 * Default is compression ratio of 11.
 */
int
lame_set_brate(lame_t gfc, int brate)
{
    gfc->mean_bitrate_kbps = brate;
    return 0;
}

int
lame_get_brate(lame_t gfc)
{
    return gfc->mean_bitrate_kbps;
}

int
lame_set_compression_ratio(lame_t gfc, float compression_ratio)
{
    gfc->compression_ratio = compression_ratio;
    return 0;
}

float
lame_get_compression_ratio(lame_t gfc)
{
    return gfc->compression_ratio;
}




/*
 * frame parameters
 */

/* Mark as copyright protected. */
int
lame_set_copyright(lame_t gfc, int copyright)
{
    /* default = 0 (disabled) */

    /* enforce disable/enable meaning, if we need more than two values
       we need to switch to an enum to have an apropriate representation
       of the possible meanings of the value */
    if ( 0 > copyright || 1 < copyright)
        return -1;

    gfc->copyright = copyright;

    return 0;
}

int
lame_get_copyright(lame_t gfc)
{
    assert( 0 <= gfc->copyright && 1 >= gfc->copyright);

    return gfc->copyright;
}


/* Mark as original. */
int
lame_set_original(lame_t gfc, int original)
{
    /* default = 1 (enabled) */

    /* enforce disable/enable meaning, if we need more than two values
       we need to switch to an enum to have an apropriate representation
       of the possible meanings of the value */
    if ( 0 > original || 1 < original)
        return -1;

    gfc->original = original;

    return 0;
}

int
lame_get_original(lame_t gfc)
{
    assert( 0 <= gfc->original && 1 >= gfc->original);

    return gfc->original;
}


/*
 * error_protection.
 * Use 2 bytes from each frame for CRC checksum.
 */
int
lame_set_error_protection(lame_t gfc, int error_protection)
{
    /* default = 0 (disabled) */

    /* enforce disable/enable meaning, if we need more than two values
       we need to switch to an enum to have an apropriate representation
       of the possible meanings of the value */
    if ( 0 > error_protection || 1 < error_protection)
        return -1;

    gfc->error_protection = error_protection;

    return 0;
}

int
lame_get_error_protection(lame_t gfc)
{
    assert( 0 <= gfc->error_protection && 1 >= gfc->error_protection);

    return gfc->error_protection;
}


/* MP3 'private extension' bit. Meaningless. */
int
lame_set_extension(lame_t gfc, int extension)
{
    /* default = 0 (disabled) */

    /* enforce disable/enable meaning, if we need more than two values
       we need to switch to an enum to have an apropriate representation
       of the possible meanings of the value */
    if ( 0 > extension || 1 < extension)
        return -1;

    gfc->extension = extension;

    return 0;
}

int
lame_get_extension(lame_t gfc)
{
    assert( 0 <= gfc->extension && 1 >= gfc->extension);

    return gfc->extension;
}


/* Enforce strict ISO compliance. */
int
lame_set_strict_ISO(lame_t gfc, int strict_ISO)
{
    /* default = 0 (disabled) */

    /* enforce disable/enable meaning, if we need more than two values
       we need to switch to an enum to have an apropriate representation
       of the possible meanings of the value */
    if ( 0 > strict_ISO || 1 < strict_ISO)
        return -1;

    gfc->strict_ISO = strict_ISO;

    return 0;
}

int
lame_get_strict_ISO(lame_t gfc)
{
    assert( 0 <= gfc->strict_ISO && 1 >= gfc->strict_ISO);

    return gfc->strict_ISO;
}
 



/********************************************************************
 * quantization/noise shaping 
 ***********************************************************************/

/* Disable the bit reservoir. For testing only. */
int
lame_set_disable_reservoir(lame_t gfc, int disable_reservoir)
{
    /* default = 0 (disabled) */

    /* enforce disable/enable meaning, if we need more than two values
       we need to switch to an enum to have an apropriate representation
       of the possible meanings of the value */
    if ( 0 > disable_reservoir || 1 < disable_reservoir)
        return -1;

    gfc->disable_reservoir = disable_reservoir;

    return 0;
}

int
lame_get_disable_reservoir(lame_t gfc)
{
    assert( 0 <= gfc->disable_reservoir && 1 >= gfc->disable_reservoir);

    return gfc->disable_reservoir;
}




int
lame_set_sfscale(lame_t gfc,	int method)
{
    gfc->use_scalefac_scale = method;
    return 0;
}


int
lame_set_subblock_gain(lame_t gfc, int method)
{
    gfc->use_subblock_gain = method;
    return 0;
}


int
lame_set_narrowenStereo(lame_t gfc, float factor)
{
    if (factor < 0.0 || factor > 1.0)
	return -1;
    gfc->narrowStereo = factor*.5;
    return 0;
}

int
lame_set_tune_bass(lame_t gfc, int factor)
{
    gfc->nsPsy.tuneBass = factor;
    return 0;
}

int
lame_set_tune_alto(lame_t gfc, int factor)
{
    gfc->nsPsy.tuneAlto = factor;
    return 0;
}

int
lame_set_tune_treble(lame_t gfc, int factor)
{
    gfc->nsPsy.tuneTreble = factor;
    return 0;
}

int
lame_set_tune_sfb21(lame_t gfc, int factor)
{
    gfc->nsPsy.tuneSFB21 = factor;
    return 0;
}

/********************************************************************
 * VBR control
 ***********************************************************************/

/* Types of VBR.  default = CBR */
int
lame_set_VBR(lame_t gfc, vbr_mode VBR)
{
    if ((unsigned int)VBR >= vbr_max_indicator)
        return -1;  /* Unknown VBR mode! */

    gfc->VBR = VBR;

    return 0;
}

vbr_mode
lame_get_VBR(lame_t gfc)
{
    assert((unsigned int)gfc->VBR < vbr_max_indicator);
    return gfc->VBR;
}


/*
 * VBR quality level.
 *  0 = highest
 *  9 = lowest 
 */
int
lame_set_VBR_q(lame_t gfc, int VBR_q)
{
    if (VBR_q < 0)
        return -1;  /* Unknown VBR quality level! */

    gfc->VBR_q = VBR_q;
    return 0;
}

int
lame_get_VBR_q(lame_t gfc)
{
    assert(0 <= gfc->VBR_q);

    return gfc->VBR_q;
}


/* Ignored except for VBR = abr (ABR mode) */
int
lame_set_VBR_mean_bitrate_kbps(lame_t gfc, int VBR_mean_bitrate_kbps)
{
    gfc->mean_bitrate_kbps = VBR_mean_bitrate_kbps;

    return 0;
}

int
lame_get_VBR_mean_bitrate_kbps(lame_t gfc)
{
    return gfc->mean_bitrate_kbps;
}

int
lame_set_VBR_min_bitrate_kbps(lame_t gfc, int VBR_min_bitrate_kbps)
{
    gfc->VBR_min_bitrate_kbps = VBR_min_bitrate_kbps;

    return 0;
}

int
lame_get_VBR_min_bitrate_kbps(lame_t gfc)
{
    return gfc->VBR_min_bitrate_kbps;
}

int
lame_set_VBR_max_bitrate_kbps(lame_t gfc, int VBR_max_bitrate_kbps)
{
    gfc->VBR_max_bitrate_kbps = VBR_max_bitrate_kbps;

    return 0;
}

int
lame_get_VBR_max_bitrate_kbps(lame_t gfc)
{
    return gfc->VBR_max_bitrate_kbps;
}


/********************************************************************
 * Filtering control
 ***********************************************************************/
int
lame_set_lowpassfreq(lame_t gfc, int lowpassfreq)
{
    gfc->lowpassfreq = lowpassfreq;
    return 0;
}

int
lame_get_lowpassfreq(lame_t gfc)
{
    return gfc->lowpassfreq;
}


int
lame_set_lowpasswidth(lame_t gfc, int lowpasswidth)
{
    gfc->lowpasswidth = lowpasswidth;

    return 0;
}

int
lame_get_lowpasswidth(lame_t gfc)
{
    return gfc->lowpasswidth;
}


int
lame_set_highpassfreq(lame_t gfc, int highpassfreq)
{
    gfc->highpassfreq = highpassfreq;

    return 0;
}

int
lame_get_highpassfreq(lame_t gfc)
{
    return gfc->highpassfreq;
}


int
lame_set_highpasswidth(lame_t gfc, int highpasswidth)
{
    gfc->highpasswidth = highpasswidth;

    return 0;
}

int
lame_get_highpasswidth(lame_t gfc)
{
    return gfc->highpasswidth;
}




/*
 * psycho acoustics and other arguments which you should not change 
 * unless you know what you are doing
 */

/* Adjust masking values. */
int
lame_set_maskingadjust(lame_t gfc, float adjust)
{
    gfc->masklower_base = gfc->masking_lower = db2pow(adjust);
    return 0;
}

int
lame_set_maskingadjust_short(lame_t gfc, float adjust)
{
    gfc->masking_lower_short = db2pow(adjust);
    return 0;
}

/* Only use ATH for masking. */
int
lame_set_ATHonly(lame_t gfc, int ATHonly)
{
    gfc->ATHonly = ATHonly;

    return 0;
}

int
lame_get_ATHonly(lame_t gfc)
{
    return gfc->ATHonly;
}


/* Only use ATH for short blocks. */
int
lame_set_ATHshort(lame_t gfc, int ATHshort)
{
    gfc->ATHshort = ATHshort;
    return 0;
}

int
lame_get_ATHshort(lame_t gfc)
{
    return gfc->ATHshort;
}


/* Disable ATH. */
int
lame_set_noATH(lame_t gfc, int noATH)
{
    gfc->noATH = noATH;

    return 0;
}

int
lame_get_noATH(lame_t gfc)
{
    return gfc->noATH;
}


/* Select ATH formula 4 shape. */
int
lame_set_ATHcurve(lame_t gfc, float ATHcurve)
{
    if (ATHcurve < 0)
	ATHcurve = 0;
    gfc->ATHcurve = ATHcurve;

    return 0;
}

int
lame_get_ATHcurve(lame_t gfc)
{
    return gfc->ATHcurve;
}


/* Lower ATH by this many db. */
int
lame_set_ATHlower(lame_t gfc, float ATHlower)
{
    gfc->ATHlower = db2pow(-ATHlower);
    return 0;
}

float
lame_get_ATHlower(lame_t gfc)
{
    return gfc->ATHlower;
}


/* Adjust (in dB) the point below which adaptive ATH level adjustment occurs. */
int
lame_set_athaa_sensitivity(lame_t gfc, float athaa_sensitivity)
{
    gfc->ATH.aa_decay = db2pow(athaa_sensitivity);
    return 0;
}

float
lame_get_athaa_sensitivity(lame_t gfc)
{
    return FAST_LOG10(gfc->ATH.aa_decay) * 10.0;
}


int
lame_set_short_threshold(lame_t gfc, float s)
{
    gfc->nsPsy.attackthre   = s;
    return 0;
}


/* Use intensity stereo effect */
int
lame_set_istereoRatio(lame_t gfc, float ratio)
{
    if (! (0 <= ratio && ratio <= 1.0))
        return -1;

    gfc->istereo_ratio = 1.0-ratio;

    return 0;
}

/* Use inter-channel masking effect */
int
lame_set_interChRatio(lame_t gfc, float ratio)
{
    if (! (0 <= ratio && ratio <= 1.0))
        return -1;

    gfc->interChRatio = ratio;

    return 0;
}

/* reduce side channel PE */
int
lame_set_reduceSide(lame_t gfc, float ratio)
{
    if (! (0 <= ratio && ratio <= 1.0))
        return -1;

    gfc->reduce_side = 1.0-ratio;

    return 0;
}

float
lame_get_interChRatio(lame_t gfc)
{
    assert( 0 <= gfc->interChRatio
	    && gfc->interChRatio <= 1.0);

    return gfc->interChRatio;
}


/* Use pseudo substep shaping method */
int
lame_set_substep(lame_t gfc, int method)
{
    /* default = 0.0 (no inter-cahnnel maskin) */
    if (! (0 <= method && method <= 7))
        return -1;

    gfc->substep_shaping = method;
    return 0;
}

int
lame_get_substep(lame_t gfc)
{
    assert(0 <= gfc->substep_shaping && gfc->substep_shaping <= 3);
    return gfc->substep_shaping;
}

/* Use mixed block. */
int
lame_set_use_mixed_blocks(lame_t gfc, int use_mixed_blocks)
{
    /* 0 ... not use the mixed block
       1 ... use all short block as mixed block
       2 ... mixed/not mixed is determined by PE (not implemented yet)
    */
    if (!(0 <= use_mixed_blocks && use_mixed_blocks <= 2))
        return -1;

    gfc->mixed_blocks = use_mixed_blocks;
    return 0;
}

/*
 * Input PCM is emphased PCM
 * (for instance from one of the rarely emphased CDs).
 *
 * It is STRONGLY not recommended to use this, because psycho does not
 * take it into account, and last but not least many decoders
 * ignore these bits
 */
int
lame_set_emphasis(lame_t gfc, int emphasis)
{
    /* XXX: emphasis should be converted to an enum */
    if ( 0 > emphasis || 4 <= emphasis)
        return -1;

    gfc->emphasis = emphasis;

    return 0;
}

int
lame_get_emphasis(lame_t gfc)
{
    assert( 0 <= gfc->emphasis && 4 > gfc->emphasis);

    return gfc->emphasis;
}




/***************************************************************/
/* internal variables, cannot be set...                        */
/* provided because they may be of use to calling application  */
/***************************************************************/

/* MPEG version.
 *  0 = MPEG-2
 *  1 = MPEG-1
 * (2 = MPEG-2.5)    
 */
int
lame_get_version(lame_t gfc)
{
    return gfc->version;
}


/* Encoder delay. */
int
lame_get_encoder_delay(lame_t gfc)
{
    return ENCDELAY + gfc->framesize;
}

/* padding added to the end of the input */
int
lame_get_encoder_padding(lame_t gfc)
{
    return gfc->encoder_padding;
}


/* Size of MPEG frame. */
int
lame_get_framesize(lame_t gfc)
{
    return gfc->framesize;
}


/* Number of frames encoded so far. */
int
lame_get_frameNum(lame_t gfc)
{
    return gfc->frameNum;
}

int
lame_get_mf_samples_to_encode(lame_t gfc)
{
    return gfc->mf_size + POSTDELAY;
}


int CDECL lame_get_size_mp3buffer(lame_t gfc)
{
    int size;
    compute_flushbits(gfc, &size);
    return size;
}



int
lame_get_RadioGain(lame_t gfc)
{
    return gfc->RadioGain;
}

int
lame_get_AudiophileGain(lame_t gfc)
{
    return gfc->AudiophileGain;
}

float
lame_get_PeakSample(lame_t gfc)
{
    return (float)gfc->PeakSample;
}

int
lame_get_noclipGainChange(lame_t gfc)
{
    return gfc->noclipGainChange; 
}

float
lame_get_noclipScale(lame_t gfc)
{
    return gfc->noclipScale; 
}


/*
 * LAME's estimate of the total number of frames to be encoded.
 * Only valid if calling program set num_samples.
 */
int
lame_get_totalframes(lame_t gfc)
{
    int totalframes;
    /* estimate based on user set num_samples: */
    totalframes =
        ((double)gfc->num_samples * gfc->out_samplerate)
	/ ((double)gfc->in_samplerate * gfc->framesize) + 2;

    /* check to see if we underestimated totalframes
       if (totalframes < gfc->frameNum)
           totalframes = gfc->frameNum;
    */
    return totalframes;
}


/* Custom msfix hack */
void
lame_set_msfix(lame_t gfc, float msfix)
{
    gfc->nsPsy.msfix = msfix;
}

/* Custom msfix hack */
void
lame_set_forbid_diff_blocktype(lame_t gfc, int val)
{
    gfc->forbid_diff_type = val;
}


void
lame_set_analysis(lame_t gfc, plotting_data *pinfo)
{
#ifndef NOANALYSIS
    gfc->pinfo = pinfo;
#endif
}

int 
lame_set_asm_optimizations(lame_t gfc, int optim, int mode)
{
#if HAVE_NASM
    mode = (mode == 1? 1 : 0);
    switch (optim){
        case MMX: {
            gfc->asm_optimizations.mmx = mode;
            return optim;
        }
        case AMD_3DNOW: {
            gfc->asm_optimizations.amd3dnow = mode;
            return optim;
        }
        case SSE: {
            gfc->asm_optimizations.sse = mode;
            return optim;
        }
        default:
	    return optim;
    }
#else
    (void) gfc; (void) optim; (void) mode;
    return -1;
#endif
}


/*! Stringify \a x. */
#define STR(x)   #x
/*! Stringify \a x, perform macro expansion. */
#define XSTR(x)  STR(x)

#ifdef HAVE_NASM
# define V  "MMX, [E]3DNow!, SSE"
#else
# define V  ""
#endif

/*! Get the LAME version string. */
/*!
  \param void
  \return a pointer to a string which describes the version of LAME.
*/
const char*
get_lame_version ( void )		/* primary to write screen reports */
{
    /* Here we can also add informations about compile time configurations */

#if   LAME_ALPHA_VERSION > 0
    static /*@observer@*/ const char *const str =
        XSTR(LAME_MAJOR_VERSION) "." XSTR(LAME_MINOR_VERSION) " " V
        "(alpha " XSTR(LAME_ALPHA_VERSION) ", " __DATE__ " " __TIME__ ")";
#elif LAME_BETA_VERSION > 0
    static /*@observer@*/ const char *const str =
        XSTR(LAME_MAJOR_VERSION) "." XSTR(LAME_MINOR_VERSION) " " V
        "(beta " XSTR(LAME_BETA_VERSION) ", " __DATE__ ")";
#else
    static /*@observer@*/ const char *const str =
        XSTR(LAME_MAJOR_VERSION) "." XSTR(LAME_MINOR_VERSION) " " V;
#endif

    return str;
}


/*! Get the short LAME version string. */
/*!
  It's mainly for inclusion into the MP3 stream.

  \param void   
  \return a pointer to the short version of the LAME version string.
*/
const char*
get_lame_short_version ( void )
{
    /* adding date and time to version string makes it harder for output
       validation */

#if   LAME_ALPHA_VERSION > 0
    static /*@observer@*/ const char *const str =
        XSTR(LAME_MAJOR_VERSION) "." XSTR(LAME_MINOR_VERSION) " (alpha)";
#elif LAME_BETA_VERSION > 0
    static /*@observer@*/ const char *const str =
        XSTR(LAME_MAJOR_VERSION) "." XSTR(LAME_MINOR_VERSION) " (beta)";
#else
    static /*@observer@*/ const char *const str =
        XSTR(LAME_MAJOR_VERSION) "." XSTR(LAME_MINOR_VERSION);
#endif

    return str;
}

/*! Get the _very_ short LAME version string. */
/*!
  It's used in the LAME VBR tag only.

  \param void   
  \return a pointer to the short version of the LAME version string.
*/
const char*
get_lame_very_short_version ( void )
{
    /* adding date and time to version string makes it harder for output
       validation */

#if   LAME_ALPHA_VERSION > 0
    static /*@observer@*/ const char *const str =
       "LAME" XSTR(LAME_MAJOR_VERSION) "." XSTR(LAME_MINOR_VERSION) "a";
#elif LAME_BETA_VERSION > 0
    static /*@observer@*/ const char *const str =
       "LAME" XSTR(LAME_MAJOR_VERSION) "." XSTR(LAME_MINOR_VERSION) "b";
#else
    static /*@observer@*/ const char *const str =
       "LAME" XSTR(LAME_MAJOR_VERSION) "." XSTR(LAME_MINOR_VERSION) " ";
#endif

    return str;
}

/*! Get the version string for psycho-model */
/*!
  \param void
  \return a pointer to a string which describes the version of psycho-model.
*/
const char*
get_psy_version ( void )
{
#if   PSY_ALPHA_VERSION > 0
    static /*@observer@*/ const char *const str =
        XSTR(PSY_MAJOR_VERSION) "." XSTR(PSY_MINOR_VERSION)
        " (alpha " XSTR(PSY_ALPHA_VERSION) ", " __DATE__ " " __TIME__ ")";
#elif PSY_BETA_VERSION > 0
    static /*@observer@*/ const char *const str =
        XSTR(PSY_MAJOR_VERSION) "." XSTR(PSY_MINOR_VERSION)
        " (beta " XSTR(PSY_BETA_VERSION) ", " __DATE__ ")";
#else
    static /*@observer@*/ const char *const str =
        XSTR(PSY_MAJOR_VERSION) "." XSTR(PSY_MINOR_VERSION);
#endif

    return str;
}


/*! Get the URL for the LAME website. */
/*!
  \param void
  \return a pointer to a string which is a URL for the LAME website.
*/
const char*
get_lame_url ( void )
{
    static /*@observer@*/ const char *const str = LAME_URL;

    return str;
}    


/*! Get the numerical representation of the version. */
/*!
  Writes the numerical representation of the version of LAME and
  its psycho-model into lvp.

  \param lvp    
*/
void
get_lame_version_numerical ( lame_version_t *const lvp )
{
    static /*@observer@*/ const char *const features = V;

    /* generic version */
    lvp->major = LAME_MAJOR_VERSION;
    lvp->minor = LAME_MINOR_VERSION;
    lvp->alpha = LAME_ALPHA_VERSION;
    lvp->beta  = LAME_BETA_VERSION;

    /* psy version */
    lvp->psy_major = PSY_MAJOR_VERSION;
    lvp->psy_minor = PSY_MINOR_VERSION;
    lvp->psy_alpha = PSY_ALPHA_VERSION;
    lvp->psy_beta  = PSY_BETA_VERSION;

    /* compile time features */
    /*@-mustfree@*/
    lvp->features = features;
    /*@=mustfree@*/
}

/* end of version.c */
