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
#include <config.h>
#endif

#include <assert.h>

#include "util.h"
#include "newmdct.h"
#include "psymodel.h"
#include "quantize.h"
#include "tables.h"
#include "bitstream.h"
#include "VbrTag.h"

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif


static void
conv_istereo(lame_t gfc, gr_info *gi, int sfb, int i)
{
    for (; i < gi->xrNumMax; sfb++) {
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

	if (sfb > gi[0].psymax)
	    continue;
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
    assert ( gfc->bitrate_index < 16u );
    assert ( gfc->mode_ext      <  4u );
    
    /* count 'em for every mode extension in case of 2 channel encoding */
    if (gfc->channels_out == 2)
	gfc->bitrate_stereoMode_Hist [gfc->bitrate_index] [gfc->mode_ext]++;

    for (gr = 0; gr < gfc->mode_gr; gr++) {
	for (ch = 0; ch < gfc->channels_out; ch++) {
	    int bt = gfc->l3_side.tt[gr][ch].block_type;
	    int mf = gfc->l3_side.tt[gr][ch].mixed_block_flag;
	    if (mf) bt = 4;
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

    gi->part2_3_length      = 0;
    gi->big_values          = 0;
    gi->count1              = 0;
    gi->global_gain         = 210;
    gi->scalefac_compress   = 0;
    /* mixed_block_flag, block_type was set in psymodel.c */
    gi->table_select [0]    = 0;
    gi->table_select [1]    = 0;
    gi->table_select [2]    = 0;
    gi->subblock_gain[0]    = 0;
    gi->subblock_gain[1]    = 0;
    gi->subblock_gain[2]    = 0;
    gi->subblock_gain[3]    = 0;    /* this one is always 0 */
    gi->region0_count       = 0;
    gi->region1_count       = 0;
    gi->preflag             = 0;
    gi->scalefac_scale      = 0;
    gi->count1table_select  = 0;
    gi->part2_length        = 0;
    gi->sfbdivide           = 11;
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
		int window, l;
		for (window = 0; window < 3; window++) {
		    for (l = start; l < end; l++)
			*ix++ = ixwork[3*l+window];
		    gi->width [j] = end - start;
		    gi->window[j] = window;
		    j++;
		}
	    }
	}
	gi->region1_count = SBMAX_l - 2 - gi->region0_count;
    } else {
	/* analog silence detection in pseudo sfb 22 */
	if (gfc->scalefac_band.l[SBMAX_l-1] < 576-100) {
	    int j0 = (576+gfc->scalefac_band.l[SBMAX_l-1])/2, j;
	    FLOAT power = 0.0;
	    for (j = j0; j < 576; j++)
		power += gi->xr[j] * gi->xr[j];
	    if (power < gi->ATHadjust * gfc->ATH.l[SBMAX_l-1]) {
		for (j = j0; j < 576; j++)
		    gi->xr[j] = 0;
	    }
	}
    }
    gi->count1bits          = 0;
    gi->slen[0]             = 0;
    gi->slen[1]             = 0;
    gi->slen[2]             = 0;
    gi->slen[3]             = 0;
    if (gi->block_type == SHORT_TYPE)
	gi->xrNumMax = gfc->scalefac_band.s[gi->psymax/3]*3;
    else
	gi->xrNumMax = gfc->xrNumMax_longblock;

    memset(gi->scalefac, 0, sizeof(gi->scalefac));
}



/*************************************************************************
 encode_mp3_frame()
   encode a single frame for Layer3

                       gr 0            gr 1
inbuf:           |--------------|---------------|-------------|
MDCT output:  |--------------|---------------|-------------|

FFT's                    <---------1024---------->
                                         <---------1024-------->


    inbuf = buffer of PCM data size=MP3 framesize
    encoder acts on inbuf[ch][0], but output is delayed by MDCTDELAY
    so the MDCT coefficints are from inbuf[ch][-MDCTDELAY]

    psy-model FFT has a 1 granule delay, so we feed it data for the 
    next granule.
    FFT is centered over granule:  224+576+224
    So FFT starts at:   576-224-MDCTDELAY

    MPEG2:  FFT ends at:  BLKSIZE+576-224-MDCTDELAY
    MPEG1:  FFT ends at:  BLKSIZE+2*576-224-MDCTDELAY    (1904)

    FFT starts at 576-224-MDCTDELAY (304)  = 576-FFTOFFSET
*************************************************************************/

static int
encode_mp3_frame(lame_t gfc, unsigned char* mp3buf, int mp3buf_size)
{
    int mp3count, ch, gr;
    III_psy_ratio masking[2][MAX_CHANNELS];
    FLOAT sbsmpl[MAX_CHANNELS][1152];

    if (!gfc->lame_encode_frame_init) {
	/* prime the MDCT/polyphase filterbank with a short block */
	sample_t primebuff[1152+576];
	memset(gfc->sb_sample, 0, sizeof(gfc->sb_sample));
	gfc->lame_encode_frame_init = 1;

	/* polyphase filtering / mdct */
	for ( ch = 0; ch < gfc->channels_out; ch++ ) {
	    int i;
	    memset(primebuff, 0, sizeof(FLOAT)*gfc->framesize);
	    for (i = -48; i < 576; i++)
		primebuff[gfc->framesize + i] = gfc->mfbuf[ch][i+1152];

	    subband(gfc, primebuff-gfc->framesize, sbsmpl[ch]);
	    memset(gfc->sb_sample[ch][0], 0, sizeof(gfc->sb_sample[0][0]));
	    memcpy(gfc->sb_sample[ch][1], sbsmpl[ch],
		   sizeof(gfc->sb_sample[0][0])*gfc->mode_gr);
	}

	/* check FFT will not use a negative starting offset */
#if 576 < FFTOFFSET
# error FFTOFFSET greater than 576: FFT uses a negative offset
#endif
	/* check if we have enough data for FFT */
	assert(gfc->mf_size>=(BLKSIZE+gfc->framesize*2-FFTOFFSET));
	/* check if we have enough data for polyphase filterbank */
	/* it needs 1152 samples + 286 samples ignored for one granule */
	/*          1152+576+286 samples for two granules */
	assert(gfc->mf_size >= 286+576+gfc->framesize);

	if (gfc->psymodel)
	    psycho_analysis(gfc, masking, sbsmpl);
    }

    /********************** padding *****************************/
    /* padding method as described in 
     * "MPEG-Layer3 / Bitstream Syntax and Decoding"
     * by Martin Sieler, Ralph Sperschneider
     *
     * note: there is no padding for the very first frame
     *
     * Robert.Hegemann@gmx.de 2000-06-22
     */
    gfc->padding = FALSE;
    if ((gfc->slot_lag -= gfc->frac_SpF) < 0) {
	gfc->slot_lag += gfc->out_samplerate;
	gfc->padding = TRUE;
    }

    /* subband filtering in the next frame */
    /* to determine long/short swithcing in psymodel */
    for ( ch = 0; ch < gfc->channels_out; ch++ )
	subband(gfc, gfc->mfbuf[ch], sbsmpl[ch]);

    if (gfc->psymodel)
	psycho_analysis(gfc, masking, sbsmpl);
    else
	memset(masking, 0, sizeof(masking));

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

    gfc->mode_ext |= gfc->use_istereo;
    if (gfc->mode_ext & MPG_MD_MS_LR) {
	/* convert from L/R -> Mid/Side */
	if (gfc->mode_ext == MPG_MD_MS_I) {
	    int i = 0;
	    for (gr = 0; gr < gfc->mode_gr; gr++) {
		if (gfc->l3_side.tt[gr][0].block_type == SHORT_TYPE)
		    i += gfc->is_start_sfb_s[gr];
		else
		    i += gfc->is_start_sfb_l[gr];
	    }
	    if (i == 0)
		gfc->mode_ext = MPG_MD_MS_LR;
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
	    if (gfc->mode_ext & 1)
		conv_istereo(gfc, gi, sfb, end);
	}
    } else if (gfc->mode_ext & 1) {
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

    if (gfc->bWriteVbrTag) {
	gfc->nVbrNumFrames++;
	if (gfc->VBR == vbr)
	    AddVbrFrame(gfc);
    }

#ifndef NOANALYSIS
    if (gfc->pinfo) {
	const sample_t *inbuf[MAX_CHANNELS];
	inbuf[0]=gfc->mfbuf[0];
	inbuf[1]=gfc->mfbuf[1];
	set_frame_pinfo(gfc, masking, inbuf);
    }
#endif

#ifdef BRHIST
    updateStats( gfc );
#endif

  return mp3count;
}

/* resampling via FIR filter, blackman window */
inline static FLOAT blackman(FLOAT x,FLOAT fcn,int l)
{
    /* This algorithm from:
       SIGNAL PROCESSING ALGORITHMS IN FORTRAN AND C
       S.D. Stearns and R.A. David, Prentice-Hall, 1992
    */
    FLOAT bkwn,x2;
    FLOAT wcn = (PI * fcn);
  
    x /= l;
    if (x<0) x=0;
    if (x>1) x=1;
    x2 = x - .5;

    bkwn = 0.42 - 0.5*cos(2*x*PI)  + 0.08*cos(4*x*PI);
    if (fabs(x2)<1e-9) return wcn/PI;
    else 
	return  (  bkwn*sin(l*wcn*x2)  / (PI*l*x2)  );
}

/* gcd - greatest common divisor */
/* Joint work of Euclid and M. Hendry */

static int
gcd(int i, int j)
{
    return j ? gcd(j, i % j) : i;
}



/* copy in new samples from in_buffer into mfbuf, with resampling
   if necessary.  n_in = number of samples from the input buffer that
   were used.  n_out = number of samples copied into mfbuf  */

static int
fill_buffer_resample(
    lame_t gfc,
    sample_t *outbuf,
    sample_t *inbuf,
    int len,
    int *num_used,
    int ch) 
{
    int desired_len = gfc->framesize;
    int BLACKSIZE;
    FLOAT offset,xvalue;
    int i,j=0,k;
    FLOAT fcn;
    FLOAT *inbuf_old;
    int bpc = gcd(gfc->out_samplerate, gfc->in_samplerate);
    int filter_l = 31;

    if (bpc == gfc->out_samplerate || bpc == gfc->in_samplerate)
	filter_l++; /* if the resample ratio is int, it must be even */

    bpc = gfc->out_samplerate/bpc;
    if (bpc>BPC) bpc = BPC;

    fcn = 1.00;
    if (gfc->resample_ratio > 1.00) fcn /= gfc->resample_ratio;
    BLACKSIZE = filter_l+1;  /* size of data needed for FIR */

    if (gfc->fill_buffer_resample_init == 0) {
	gfc->inbuf_old[0]=calloc(BLACKSIZE,sizeof(gfc->inbuf_old[0][0]));
	gfc->inbuf_old[1]=calloc(BLACKSIZE,sizeof(gfc->inbuf_old[0][0]));
	for (i=0; i<=2*bpc; ++i)
	    gfc->blackfilt[i]=calloc(BLACKSIZE,sizeof(gfc->blackfilt[0][0]));

	gfc->itime[0]=0;
	gfc->itime[1]=0;

	/* precompute blackman filter coefficients */
	for (j = 0; j <= 2*bpc; j++) {
	    FLOAT sum = 0.; 
	    offset = (j-bpc) / (2.*bpc);
	    for (i = 0; i <= filter_l; i++)
		sum += gfc->blackfilt[j][i] = blackman(i-offset,fcn,filter_l);
	    for (i = 0; i <= filter_l; i++)
		gfc->blackfilt[j][i] /= sum;
	}
	gfc->fill_buffer_resample_init = 1;
    }

    inbuf_old=gfc->inbuf_old[ch];

    /* time of j'th element in inbuf = itime + j/ifreq; */
    /* time of k'th element in outbuf   =  j/ofreq */
    for (k=0;k<desired_len;k++) {
	FLOAT time0;
	int joff;

	time0 = k*gfc->resample_ratio;       /* time of k'th output sample */
	j = floor( time0 -gfc->itime[ch]  );

	/* check if we need more input data */
	if ((filter_l + j - filter_l/2) >= len) break;

	/* blackman filter.  by default, window centered at j+.5(filter_l%2) */
	/* but we want a window centered at time0.   */
	offset = ( time0 -gfc->itime[ch] - (j + .5*(filter_l%2)));
	assert(fabs(offset)<=.501);

	/* find the closest precomputed window for this offset: */
	joff = floor((offset*2*bpc) + bpc +.5);

	xvalue = 0.;
	for (i=0 ; i<=filter_l ; ++i) {
	    int j2 = i+j-filter_l/2;
	    int y;
	    assert(j2<len);
	    assert(j2+BLACKSIZE >= 0);
	    y = (j2<0) ? inbuf_old[BLACKSIZE+j2] : inbuf[j2];
	    xvalue += y*gfc->blackfilt[joff][i];
	}
	outbuf[k]=xvalue;
    }

    /* k = number of samples added to outbuf */
    /* last k sample used data from [j-filter_l/2,j+filter_l-filter_l/2]  */

    /* how many samples of input data were used:  */
    *num_used = Min(len,filter_l+j-filter_l/2);

    /* adjust our input time counter.  Incriment by the number of samples used,
     * then normalize so that next output sample is at time 0, next
     * input buffer is at time itime[ch] */
    gfc->itime[ch] += *num_used - k*gfc->resample_ratio;

    /* save the last BLACKSIZE samples into the inbuf_old buffer */
    if (*num_used >= BLACKSIZE) {
	for (i=0;i<BLACKSIZE;i++)
	    inbuf_old[i]=inbuf[*num_used + i -BLACKSIZE];
    }else{
	/* shift in *num_used samples into inbuf_old  */
	int n_shift = BLACKSIZE-*num_used;  /* number of samples to shift */

	/* shift n_shift samples by *num_used, to make room for the
	 * num_used new samples */
	for (i=0; i<n_shift; ++i ) 
	    inbuf_old[i] = inbuf_old[i+ *num_used];

	/* shift in the *num_used samples */
	for (j=0; i<BLACKSIZE; ++i, ++j ) 
	    inbuf_old[i] = inbuf[j];

	assert(j == *num_used);
    }
    return k;  /* return the number samples created at the new samplerate */
}





static int
fill_buffer(lame_t gfc, sample_t *in_buffer[2], int nsamples, int *n_in)
{
    int n_out;

    /* copy in new samples into mfbuf, with resampling if necessary */
    if (gfc->resample_ratio != 1.0) {
	int ch = 0;
	do {
	    n_out = fill_buffer_resample(gfc, &gfc->mfbuf[ch][gfc->mf_size],
					 in_buffer[ch], nsamples, n_in, ch);
	} while (ch < gfc->channels_out);
    }
    else {
	*n_in = n_out = Min(gfc->framesize, nsamples);
	memcpy(&gfc->mfbuf[0][gfc->mf_size], &in_buffer[0][0],
	       sizeof(sample_t) * n_out);
	if (gfc->channels_out == 2)
	    memcpy(&gfc->mfbuf[1][gfc->mf_size], &in_buffer[1][0],
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
lame_encode_buffer_sample_t(
    lame_t gfc,
    sample_t buffer_lr[],
    int nsamples,
    unsigned char *mp3buf,
    const int mp3buf_size
    )
{
    int mp3size = 0, ret, i, ch, mf_needed;
    int mp3out;
    sample_t *in_buffer[2];

    /* copy out any tags that may have been written into bitstream */
    mp3out = copy_buffer(gfc,mp3buf,mp3buf_size,0);
    if (mp3out<0) return mp3out;  /* not enough buffer space */
    mp3buf += mp3out;
    mp3size += mp3out;

    /* Downsample to Mono if 2 channels in and 1 channel out */
    if (gfc->num_channels == 2 && gfc->channels_out == 1) {
	for (i=0; i<nsamples; i++)
	    buffer_lr[i] = 0.5f * ((FLOAT) buffer_lr[i]
				   + buffer_lr[i+nsamples]);
    }

    in_buffer[0]=buffer_lr;
    in_buffer[1]=buffer_lr + nsamples;

    /* some sanity checks */
#if ENCDELAY < MDCTDELAY
# error ENCDELAY is less than MDCTDELAY, see encoder.h
#endif
#if FFTOFFSET > BLKSIZE
# error FFTOFFSET is greater than BLKSIZE, see encoder.h
#endif

    mf_needed = BLKSIZE + gfc->framesize - FFTOFFSET + 1152; /* amount needed for FFT */
    mf_needed = Max(mf_needed, 480 + gfc->framesize + 1152); /* amount needed for MDCT/filterbank */
    assert(MFSIZE >= mf_needed);

    assert(nsamples > 0);
    do {
        int     n_in;  /* number of input samples processed with fill_buffer */
        int     n_out; /* number of samples output with fill_buffer */
			/* n_in != n_out if we are resampling */
	int	buf_size;

        /* copy in new samples into mfbuf, with resampling */
        n_out = fill_buffer(gfc, in_buffer, nsamples, &n_in);

        /* update in_buffer counters */
        nsamples -= n_in;
        in_buffer[0] += n_in;
	in_buffer[1] += n_in;

        /* update mfbuf[] counters */
        gfc->mf_size += n_out;
        gfc->mf_samples_to_encode += n_out;

        if (gfc->mf_size < mf_needed)
	    break;

        assert(gfc->mf_size <= MFSIZE);
	/* encode the frame.
	 *  mp3buf              = pointer to current location in buffer
	 *  mp3buf_size         = size of original mp3 output buffer
	 *			= 0 if we should not worry about the
	 *			    buffer size because calling program is 
	 *			    to lazy to compute it
	 *  mp3size		= size of data written to buffer so far
	 *  mp3buf_size-mp3size = amount of space avalable
	 */
	buf_size = mp3buf_size - mp3size;
	if (mp3buf_size==0)
	    buf_size=0;

	ret = encode_mp3_frame(gfc, mp3buf, buf_size);
	gfc->frameNum++;

	if (ret < 0)
	    return ret;
	mp3buf += ret;
	mp3size += ret;

	/* shift out old samples */
	gfc->mf_size -= gfc->framesize;
	gfc->mf_samples_to_encode -= gfc->framesize;
	for (ch = 0; ch < gfc->channels_out; ch++)
	    for (i = 0; i < gfc->mf_size; i++)
		gfc->mfbuf[ch][i] = gfc->mfbuf[ch][i + gfc->framesize];
    } while (nsamples > 0);
    assert(nsamples == 0);
    return mp3size;
}
