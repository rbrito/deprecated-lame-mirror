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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#ifdef WITH_DMALLOC
# include <dmalloc.h>
#endif
#include <assert.h>

#include "encoder.h"
#include "newmdct.h"
#include "psymodel.h"
#include "quantize.h"
#include "tables.h"
#include "bitstream.h"
#include "VbrTag.h"

static void
conv_istereo(lame_t gfc, gr_info *gi, int sfb, int i)
{
    for (; sfb < gi[0].psymax && i < gi->xrNumMax; sfb++) {
	FLOAT lsum = 1e-30, rsum = 1e-30;
	int j = i + gi->width[sfb];
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
	if (lsum < 0.5) {
	    if (lsum < 0.211324865 * 0.5)
		j = 0;
	    else if (lsum < (0.366025404 + 0.211324865) * 0.5)
		j = 1;
	    else if (lsum < (0.5 + 0.366025404) * 0.5)
		j = 2;
	} else {
	    if (lsum > 1.0-0.211324865 * 0.5)
		j = 6;
	    else if (lsum > 1.0-(0.366025404 + 0.211324865) * 0.5)
		j = 5;
	    else if (lsum > 1.0-(0.5 + 0.366025404) * 0.5)
		j = 4;
	}
	gi[1].scalefac[sfb] = j;
    }
    gfc->scale_bitcounter(&gi[1]);
}


/***********************************************************************
 *
 *  some simple statistics
 *
 *  bitrate index 0: free bitrate -> not allowed in VBR mode
 *  : bitrates, kbps depending on MPEG version
 *  bitrate index 15: forbidden
 *
 *  mode_ext:
 *  0:  LR
 *  1:  LR-i
 *  2:  MS
 *  3:  MS-i
 *
 ***********************************************************************/

#ifdef BRHIST
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
	gfc->bitrate_stereoMode_Hist [gfc->bitrate_index] [gfc->mode_ext]++;

    for (gr = 0; gr < gfc->mode_gr; gr++) {
	for (ch = 0; ch < gfc->channels_out; ch++) {
	    int bt = gfc->l3_side.tt[gr][ch].block_type;
	    if (bt == SHORT_TYPE && gfc->l3_side.tt[gr][ch].mixed_block_flag)
		bt = 4;
	    gfc->bitrate_blockType_Hist [gfc->bitrate_index] [bt] ++;
	    gfc->bitrate_blockType_Hist [15] [bt] ++;
	}
    }
}
#endif


static void
init_gr_info(lame_t gfc, int gr, int ch)
{
    int sfb, j;
    gr_info *gi = &gfc->l3_side.tt[gr][ch];

    gi->part2_3_length     = 0;
    gi->part2_length       = 0;
    gi->count1bits         = 0;
    gi->slen[0]            = 0;
    gi->slen[1]            = 0;
    gi->slen[2]            = 0;
    gi->slen[3]            = 0;
    gi->big_values         = 0;
    gi->count1             = 0;
    gi->scalefac_compress  = 0;
    /* mixed_block_flag, block_type was set in psymodel.c */
    gi->table_select [0]   = 0;
    gi->table_select [1]   = 0;
    gi->table_select [2]   = 0;
    gi->preflag            = 0;
    gi->scalefac_scale     = 0;
    gi->count1table_select = 0;
    gi->sfbdivide          = 11;
    gi->xrNumMax           = gfc->xrNumMax_longblock;

    j = gfc->cutoff_sfb_l;
    if (ch & 1)
	j = gfc->is_start_sfb_l[gr];
    gi->psymax = gi->psy_lmax = j;
    gi->sfbmax = gi->sfb_lmax = SBPSY_l;
    gi->sfb_smin              = SBPSY_s;
    for (sfb = 0; sfb < SBMAX_l; sfb++) {
	gi->width[sfb]
	    = gfc->scalefac_band.l[sfb+1] - gfc->scalefac_band.l[sfb];
	gi->window[sfb] = 3; /* subblockgain[3] is always 0. */
    }
    gi->width[sfb-1] = gfc->xrNumMax_longblock - gfc->scalefac_band.l[sfb-1];

    if (gi->block_type != NORM_TYPE) {
	gi->subblock_gain[0] = gi->subblock_gain[1] = gi->subblock_gain[2] = 0;
	gi->region0_count = 7;
	if (gi->block_type == SHORT_TYPE) {
	    FLOAT ixwork[576];
	    FLOAT *ix;

	    if (gfc->mode_gr == 1)
		gi->region0_count = 5;
	    gi->sfb_smin        = 0;
	    gi->sfb_lmax        = 0;
	    if (gi->mixed_block_flag) {
		/*
		 *  MPEG-1:      sfbs 0-7 long block, 3-12 short blocks 
		 *  MPEG-2(.5):  sfbs 0-5 long block, 3-12 short blocks
		 */ 
		gi->sfb_smin    = 3;
		gi->sfb_lmax    = gfc->mode_gr*2 + 4;
	    }
	    j = gfc->cutoff_sfb_s;
	    if (ch & 1)
		j = gfc->is_start_sfb_s[gr];
	    gi->psymax = gi->sfb_lmax + 3*(j - gi->sfb_smin);
	    gi->sfbmax = gi->sfb_lmax + 3*(SBPSY_s - gi->sfb_smin);
	    gi->sfbdivide   = gi->sfbmax - 18;
	    gi->psy_lmax    = gi->sfb_lmax;
	    gi->xrNumMax = gfc->scalefac_band.s[gi->psymax/3]*3;
	    /* re-order the short blocks, for more efficient encoding below */
	    /* By Takehiro TOMINAGA */
	    /*
	      Within each scalefactor band, data is given for successive
	      time windows, beginning with window 0 and ending with window 2.
	      Within each window, the quantized values are then arranged in
	      order of increasing frequency...
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
		    gi->width [j] = end - start;
		    gi->window[j] = subwin;
		    j++;
		}
	    }
	}
	gi->region1_count = SBMAX_l - 2 - gi->region0_count;
    } else {
	gi->region0_count = gi->region1_count = 0;
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

    memset(gi->scalefac, 0, sizeof(gi->scalefac));
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
	/* very the first frame */
	for (ch = 0; ch < gfc->channels_out; ch++) {
	    memcpy(gfc->sb_sample[ch][1], sbsmpl[ch],
		   sizeof(gfc->sb_sample[0][0])*gfc->mode_gr);
	}
	return 0;
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
	/* aging subband filetr output */
	memcpy(gfc->sb_sample[ch][0], gfc->sb_sample[ch][gfc->mode_gr],
	       sizeof(gfc->sb_sample[0][0]));
	memcpy(gfc->sb_sample[ch][1], sbsmpl[ch],
	       sizeof(gfc->sb_sample[0][0])*gfc->mode_gr);

	for (gr = 0; gr < gfc->mode_gr; gr++)
	    init_gr_info(gfc, gr, ch);
    }

    /* channel conversion */
    if (gfc->narrowStereo != 0.0) {
	/* narrown_stereo */
	for (gr = 0; gr < gfc->mode_gr; gr++) {
	    gr_info *gi = &gfc->l3_side.tt[gr][0];
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
#if 1
	    && (gfc->l3_side.tt[0][0].block_type != SHORT_TYPE
		|| gfc->is_start_sfb_s[0] == 0)
	    && (gfc->l3_side.tt[1][0].block_type != SHORT_TYPE
		|| gfc->is_start_sfb_s[1] == 0)
#endif
	    && gfc->use_istereo) {
	    for (gr = 0; gr < gfc->mode_gr; gr++) {
		int sfb = gfc->is_start_sfb_l[gr];
		if (gfc->l3_side.tt[gr][0].block_type == SHORT_TYPE)
		    sfb = gfc->is_start_sfb_s[gr];
		if (sfb && sfb < gfc->l3_side.tt[gr][0].psymax) {
		    gfc->mode_ext = MPG_MD_MS_I;
		    break;
		}
	    }
	}

	for (gr = 0; gr < gfc->mode_gr; gr++) {
	    gr_info *gi = &gfc->l3_side.tt[gr][0];
	    int sfb = gfc->is_start_sfb_l[gr];
	    int end = gfc->scalefac_band.l[sfb];
	    int i;
	    if (gi->block_type == SHORT_TYPE) {
		sfb = gfc->is_start_sfb_s[gr];
		end = gfc->scalefac_band.s[sfb]*3;
		sfb *= 3;
	    }
	    for (i = 0; i < end; i++) {
		FLOAT l = gi[0].xr[i];
		FLOAT r = gi[1].xr[i];
		gi[0].xr[i] = (l+r) * (FLOAT)(SQRT2*0.5);
		gi[1].xr[i] = (l-r) * (FLOAT)(SQRT2*0.5);
	    }
	    if (gfc->mode_ext == MPG_MD_MS_I)
		conv_istereo(gfc, gi, sfb, end);
	}
    } else if (gfc->use_istereo) {
	gfc->mode_ext = MPG_MD_LR_I;
	for (gr = 0; gr < gfc->mode_gr; gr++) {
	    gr_info *gi = &gfc->l3_side.tt[gr][0];
	    int sfb = gfc->is_start_sfb_l[gr];
	    int end = gfc->scalefac_band.l[sfb];
	    if (gi->block_type == SHORT_TYPE) {
		sfb = gfc->is_start_sfb_s[gr];
		end = gfc->scalefac_band.s[sfb]*3;
		sfb *= 3;
	    }
	    conv_istereo(gfc, gi, sfb, end);
	}
    }

    /* bit and noise allocation */
    switch (gfc->VBR){ 
    default:
    case cbr:	    iteration_loop(gfc, masking); break;
    case vbr:	VBR_iteration_loop(gfc, masking); break;
    case abr:	ABR_iteration_loop(gfc, masking); break;
    }

    /*  write the frame to the bitstream  */
    format_bitstream(gfc);

    /* copy mp3 bit buffer into array */
    mp3count = copy_buffer(gfc,mp3buf,mp3buf_size,1);

    if (gfc->bWriteVbrTag && gfc->VBR != cbr)
	AddVbrFrame(gfc);

#ifndef NOANALYSIS
    if (gfc->pinfo)
	set_frame_pinfo(gfc, masking);
#endif

#ifdef BRHIST
    updateStats( gfc );
#endif

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
	offset = time0 - (j + .5*(filter_l & 1));
	assert(fabs(offset)<=.500);

	/* find the closest precomputed window for this offset: */
	filter_coef = gfc->resample.blackfilt[(int)((offset*2*bpc)+bpc+.5)];

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
    int n_out;

    /* copy in new samples into mfbuf, with resampling if necessary */
    if (gfc->resample.ratio != 1.0) {
	n_out = fill_buffer_resample(gfc, &gfc->mfbuf[ch][gfc->mf_size],
				     in_buffer, nsamples, n_in, ch);
    }
    else {
	*n_in = n_out = Min(gfc->mf_needed - gfc->mf_size, nsamples);
	memcpy(&gfc->mfbuf[ch][gfc->mf_size], in_buffer,
	       sizeof(sample_t) * n_out);
    }
    return n_out;
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
encode_buffer_sample(
    lame_t gfc, sample_t buffer_lr[], int nsamples,
    unsigned char *mp3buf, const int mp3buf_size)
{
    int buf_remain = mp3buf_size, i;
    sample_t *in_buffer[2];
    unsigned char *p = mp3buf;

    /* copy out any tags that may have been written into bitstream */
    i = copy_buffer(gfc, p, mp3buf_size, 0);
    if (i < 0)
	return i;  /* not enough buffer space */
    p += i;
    if (mp3buf_size)
	buf_remain -= i;

    /* Downsample to Mono if 2 channels in and 1 channel out */
    if (gfc->channels_in == 2 && gfc->channels_out == 1) {
	for (i = 0; i < nsamples; i++)
	    buffer_lr[i] = 0.5f * (buffer_lr[i] + buffer_lr[i+nsamples]);
    }

    assert(nsamples > 0);

    in_buffer[0] = buffer_lr;
    in_buffer[1] = buffer_lr + nsamples;
    do {
	int n_in, n_out, ch;
	/* copy the new samples into gfc->mfbuf (with resampling if needed)
	 * it consumes (n_in) samples from in_buffer,
	 * and output (n_out) samples in gfc->mfbuf. */
	ch = 0;
	do {
	    n_out = fill_buffer(gfc, in_buffer[ch], nsamples, &n_in, ch);
	    in_buffer[ch] += n_in;
	} while (++ch < gfc->channels_out);

	/* update in_buffer counters */
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

	/* shift out old samples */
	gfc->mf_size -= gfc->framesize;
	ch = 0;
	do {
	    memmove(gfc->mfbuf[ch], &gfc->mfbuf[ch][gfc->framesize],
		    sizeof(sample_t) * gfc->mf_size);
	} while (++ch < gfc->channels_out);
    } while (nsamples > 0);
    assert(nsamples == 0);
    return p - mp3buf;
}
