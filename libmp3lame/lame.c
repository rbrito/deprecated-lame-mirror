/* -*- mode: C; mode: fold -*- */
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

#include <assert.h>
#include "lame-analysis.h"
#include "lame.h"
#include "util.h"
#include "bitstream.h"
#include "version.h"
#include "tables.h"
#include "quantize_pvt.h"
#include "VbrTag.h"

#ifdef __FreeBSD__
#include <floatingpoint.h>
#endif
#ifdef __riscos__
#include "asmstuff.h"
#endif

/* static void   lame_init_params_ppflt_lowpass (FLOAT8 amp_lowpass[32], float lowpass1, float lowpass2, int *lowpass_band, int *minband, int *maxband)           *//*{{{*/

static void
lame_init_params_ppflt_lowpass(FLOAT8 amp_lowpass[32], float lowpass1,
			       float lowpass2, int *lowpass_band,
			       int *minband, int *maxband)
{
	int band;
	FLOAT8 freq;

	for (band = 0; band <= 31; band++) {
		freq = band / 31.0;
		amp_lowpass[band] = 1;
		/* this band and above will be zeroed: */
		if (freq >= lowpass2) {
			*lowpass_band = Min(*lowpass_band, band);
			amp_lowpass[band]=0;
		}
		if (lowpass1 < freq && freq < lowpass2) {
			*minband = Min(*minband, band);
			*maxband = Max(*maxband, band);
			amp_lowpass[band] = cos((PI / 2) *
						(lowpass1 - freq) /
						(lowpass2 - lowpass1));
		}
		/*
		 * DEBUGF("lowpass band=%i  amp=%f \n",
		 *      band, gfc->amp_lowpass[band]);
		 */
	}
}

/* lame_init_params_ppflt */

/*}}}*/
/* static void   lame_init_params_ppflt         (lame_internal_flags *gfc)                                                                                        *//*{{{*/

static void
lame_init_params_ppflt(lame_internal_flags *gfc)
{
  /***************************************************************/
  /* compute info needed for polyphase filter (filter type==0, default) */
  /***************************************************************/

	int band, maxband, minband;
	FLOAT8 freq;

	if (gfc->lowpass1 > 0) {
		minband = 999;
		maxband = -1;
		lame_init_params_ppflt_lowpass(gfc->amp_lowpass,
					       gfc->lowpass1, gfc->lowpass2,
					       &gfc->lowpass_band, &minband,
					       &maxband);
		/* compute the *actual* transition band implemented by
		 * the polyphase filter */
		if (minband == 999) {
			gfc->lowpass1 = (gfc->lowpass_band - .75) / 31.0;
		} else {
			gfc->lowpass1 = (minband - .75) / 31.0;
		}
		gfc->lowpass2 = gfc->lowpass_band / 31.0;

		gfc->lowpass_start_band = minband;
		gfc->lowpass_end_band = maxband;

		/* as the lowpass may have changed above
		 * calculate the amplification here again
		 */
		for (band = minband; band <= maxband; band++) {
			freq = band / 31.0;
			gfc->amp_lowpass[band] =
				cos((PI / 2) * (gfc->lowpass1 - freq) /
				    (gfc->lowpass2 - gfc->lowpass1));
		}
	} else {
		gfc->lowpass_start_band = 0;
		gfc->lowpass_end_band = -1;/* do not to run into for-loops */
	}

	/* make sure highpass filter is within 90% of what the effective
	 * highpass frequency will be */
	if (gfc->highpass2 > 0) {
		if (gfc->highpass2 < .9 * (.75 / 31.0) ) {
			gfc->highpass1 = 0;
			gfc->highpass2 = 0;
			MSGF("Warning: highpass filter disabled.  "
			     "highpass frequency too small\n");
		}
	}

	if (gfc->highpass2 > 0) {
		minband = 999;
		maxband = -1;
		for (band = 0; band <= 31; band++) {
			freq = band / 31.0;
			gfc->amp_highpass[band] = 1;
			/* this band and below will be zereod */
			if (freq <= gfc->highpass1) {
				gfc->highpass_band = Max(gfc->highpass_band,
							 band);
				gfc->amp_highpass[band] = 0;
			}
			if (gfc->highpass1 < freq && freq < gfc->highpass2) {
				minband = Min(minband, band);
				maxband = Max(maxband, band);
				gfc->amp_highpass[band] =
					cos((PI / 2) *
					    (gfc->highpass2 - freq) / 
					    (gfc->highpass2 - gfc->highpass1));
			}
			/*
			DEBUGF("highpass band=%i  amp=%f \n",
			       band, gfc->amp_highpass[band]);
			*/
		}
		/* compute the *actual* transition band implemented by
		 * the polyphase filter */
		gfc->highpass1 = gfc->highpass_band / 31.0;
		if (maxband == -1) {
			gfc->highpass2 = (gfc->highpass_band + .75) / 31.0;
		} else {
			gfc->highpass2 = (maxband + .75) / 31.0;
		}

		gfc->highpass_start_band = minband;
		gfc->highpass_end_band = maxband;

		/* as the highpass may have changed above
		 * calculate the amplification here again
		 */
		for (band = minband; band <= maxband; band++) {
			freq = band / 31.0;
			gfc->amp_highpass[band] =
				cos((PI / 2) * (gfc->highpass2 - freq) /
				    (gfc->highpass2 - gfc->highpass1));
		}
	} else {
		gfc->highpass_start_band = 0;
		gfc->highpass_end_band = -1;/* do not to run into for-loops */
	}
	/*
	DEBUGF("lowpass band with amp=0:  %i \n",gfc->lowpass_band);
	DEBUGF("highpass band with amp=0:  %i \n",gfc->highpass_band);
	DEBUGF("lowpass band start:  %i \n",gfc->lowpass_start_band);
	DEBUGF("lowpass band end:    %i \n",gfc->lowpass_end_band);
	DEBUGF("highpass band start:  %i \n",gfc->highpass_start_band);
	DEBUGF("highpass band end:    %i \n",gfc->highpass_end_band);
	*/
}

/*}}}*/
/* int           lame_init_params               (lame_global_flags *gfp)                                                                                          *//*{{{*/

/********************************************************************
 *   initialize internal params based on data in gf
 *   (globalflags struct filled in by calling program)
 *
 ********************************************************************/
int lame_init_params ( lame_global_flags* const gfp )
{
    /* A third dbQ table */
    /* Can all dbQ setup can be done here using a switch statement? */
    static const FLOAT8  dbQ [] = { -5.0, -3.75, -2.5, -1.25, 0, +0.4, +0.8, +1.2, +1.6, +2.0 };
    static const int     atQ [] = { +16,  +12,   +8,   +4,    0, -4,   -8,   -12,  -16,  -20  };
    static const FLOAT8  cmp [] = {   5,    6,    7,    8,    9, 10,   11,    12,   13,   14  };
    
    int                  i;
    int                  j;
    lame_internal_flags* gfc = gfp -> internal_flags;

    gfc -> lame_init_params_init = 0;
  
    gfc -> CPU_features_3DNow = has_3DNow ();
    gfc -> CPU_features_MMX   = has_MMX   ();
    gfc -> CPU_features_SIMD  = has_SIMD  ();
    gfc -> CPU_features_SIMD2 = 0;
    init_scalar_functions ( gfc );      /* Select the fastest functions for this CPU */

  if (gfp->num_channels == 1)
      gfp->mode = MPG_MD_MONO;
  
  if (gfp->mode == MPG_MD_MONO) 
      gfc->stereo = 1;  /* malicious variable name, really */
  else
      gfc->stereo = 2;

  if (gfp->VBR!=vbr_off) {
    gfp->free_format=0;  /* VBR can't be mixed with free format */
  }

  if (gfp->VBR==vbr_off && gfp->brate==0) {
    /* no bitrate or compression ratio specified, use 11.025 */
    if (gfp->compression_ratio==0) gfp->compression_ratio=11.025;   /* rate to compress a CD down to exactly 128000 bps */
  }


  /* find bitrate if user specify a compression ratio */
  if (gfp->VBR==vbr_off && gfp->compression_ratio > 0) {
    
    if (gfp->out_samplerate==0) 
      gfp->out_samplerate = map2MP3Frequency (0.97 * gfp->in_samplerate); /* round up with a margin of 3% */
    
    /* choose a bitrate for the output samplerate which achieves
     * specifed compression ratio 
     */
    gfp->brate = 
      gfp->out_samplerate*16*gfc->stereo/(1000.0*gfp->compression_ratio);

    /* we need the version for the bitrate table look up */
    gfc->samplerate_index = SmpFrqIndex(gfp->out_samplerate, &gfp->version);
    /* find the nearest allowed bitrate */
    if (!gfp->free_format)
      gfp->brate = FindNearestBitrate(gfp->brate,gfp->version,gfp->out_samplerate);
  }
  if (gfp->brate >= 320) gfp->VBR=vbr_off;  /* don't bother with VBR at 320kbs */



  /* set the output sampling rate, and resample options if necessary
     samplerate = input sample rate
     resamplerate = ouput sample rate
  */
    
  if (gfp->out_samplerate==0) {
    /* if resamplerate is not valid, find a valid value */
    gfp->out_samplerate = map2MP3Frequency (0.97 * gfp->in_samplerate);


    /* check if user specified bitrate requires downsampling */
    /* if compression ratio is > 13, choose a new samplerate to get
     * the compression ratio down to about 10 */
    if (gfp->VBR==vbr_off && gfp->brate>0) {
      gfp->compression_ratio = gfp->out_samplerate*16*gfc->stereo/(1000.0*gfp->brate);
      if (gfp->compression_ratio > 13 ) {
	gfp->out_samplerate = map2MP3Frequency((10*1000*gfp->brate)/(16*gfc->stereo));
      }
    }
    if (gfp->VBR==vbr_abr) {
      gfp->compression_ratio = 
	gfp->out_samplerate*16*gfc->stereo/(1000.0*gfp->VBR_mean_bitrate_kbps);
      if (gfp->compression_ratio > 13 ) {
	gfp->out_samplerate = 
	  map2MP3Frequency((10*1000*gfp->VBR_mean_bitrate_kbps)/(16*gfc->stereo));
      }
    }
  }

  gfc->mode_gr = (gfp->out_samplerate <= 24000) ? 1 : 2;  /* mode_gr = 2 */
  gfp->framesize = gfc->mode_gr*576;
  if (gfp->ogg) gfp->framesize = 1024;

  gfp->encoder_delay = ENCDELAY;

    if ( gfp->out_samplerate != gfp->in_samplerate ) 
        gfc->resample_ratio = (double)gfp->in_samplerate / gfp->out_samplerate;
    else
        gfc->resample_ratio = 1.;

  /* 44.1kHz at 56kbs/channel: compression factor of 12.6
     44.1kHz at 64kbs/channel: compression factor of 11.025
     44.1kHz at 80kbs/channel: compression factor of 8.82
     22.05kHz at 24kbs:  14.7
     22.05kHz at 32kbs:  11.025
     22.05kHz at 40kbs:  8.82
     16kHz at 16kbs:  16.0
     16kHz at 24kbs:  10.7

     compression_ratio
        11                                .70?
        12                   sox resample .66
        14.7                 sox resample .45

  */

  /* for VBR, take a guess at the compression_ratio. for example: */
  /* VBR_q           compression       like
                      4.4             320kbs/41khz
     0-1              5.5             256kbs/41khz
     2                7.3             192kbs/41khz
     4                8.8             160kbs/41khz
     6                11              128kbs/41khz
     9                14.7             96kbs
     for lower bitrates, downsample with --resample
  */
    switch ( gfp->VBR ) {
    case vbr_mt:
    case vbr_rh:
    case vbr_mtrh:
        gfp->compression_ratio = cmp [ gfp->VBR_q ];
	break;
    case vbr_abr:
        gfp->compression_ratio = gfp->out_samplerate * 16 * gfc->stereo / (1.e3 * gfp->VBR_mean_bitrate_kbps);
	break;
    default:  
        gfp->compression_ratio = gfp->out_samplerate * 16 * gfc->stereo / (1.e3 * gfp->brate);
	break;
    }

#ifdef KLEMM_12

  /* At higher quality (lower compression) use STEREO instead of J-STEREO.
   * (unless the user explicitly specified a mode)
   *
   * The threshold to completely switch to STEREO is:
   *    48 kHz:   244 kbps (used at 256+)
   *    44.1 kHz: 224 kbps (used at 224+)
   *    32 kHz:   162 kbps (used at 192+)
   *
   * Note, that there is a second mechanism to reduce MS usage at high
   * compression rates. This code is only to speed up compression for high
   * data rates, where MS is really never necessary.
   */
   
    if ( !gfp->mode_fixed  &&  gfp->mode != MPG_MD_MONO )
        if ( gfp->compression_ratio <= 6.3001 )
            gfp->mode = MPG_MD_STEREO;

#else

  /* At higher quality (lower compression) use STEREO instead of J-STEREO.
   * (unless the user explicitly specified a mode)
   *
   * The threshold to switch to STEREO is:
   *    48 kHz:   171 kbps (used at 192+)
   *    44.1 kHz: 160 kbps (used at 160+)
   *    32 kHz:   119 kbps (used at 128+)
   *
   *   Note, that for 32 kHz/128 kbps J-STEREO FM recordings sound much
   *   better than STEREO, so I'm not so very happy with that. 
   *   fs < 32 kHz I have not tested.
   */
   
  if ( (!gfp->mode_fixed) && (gfp->mode !=MPG_MD_MONO)) {
    if (gfp->compression_ratio /* <= 8.82 */ < 9 ) {
      gfp->mode = MPG_MD_STEREO;
    }
  }

#endif    


  /****************************************************************/
  /* if a filter has not been enabled, see if we should add one: */
  /****************************************************************/

  if (gfp->lowpassfreq == 0) {
    /* If the user has not selected their own filter, add a lowpass
     * filter based on the compression ratio.  Formula based on
          44.1   /160    4.4x
          44.1   /128    5.5x      keep all bands
          44.1   /96kbs  7.3x      keep band 28
          44.1   /80kbs  8.8x      keep band 25
          44.1khz/64kbs  11x       keep band 21  22?

	  16khz/24kbs  10.7x       keep band 21
	  22kHz/32kbs  11x         keep band ?
	  22kHz/24kbs  14.7x       keep band 16
          16    16     16x         keep band 14
    */


    /* Should we use some lowpass filters? */
    double band = floor (15.5 - 18*log (gfp->compression_ratio/16.) );
    if (gfc->resample_ratio != 1) {
      /* resampling.  if we are resampling, add lowpass at least 90.6% (29/32) */
      if (band > 29.)
          band = 29.;
    }

#ifdef KLEMM_01
        if ( gfp->out_samplerate >= 32000 ) {
            static unsigned char bandwidth [3] [11] = {
	                 /*  0  32  64  96 128 160 192 224 256 288 320 */
	        /* 32 */ {   1, 21, 31,  54,58, 60, 62, 62, 62, 62, 62 },
		/* 44 */ {   1, 17, 25,  39,42, 44, 45, 46, 47, 48, 49 },
		/* 48 */ {   1, 15, 23,  36,39, 40, 41, 42, 43, 44, 45 },
            };
            int  index;
            int  bitrate;

            switch ( gfp->out_samplerate ) {
	    case 32000: index = 0; break;
	    default:    assert (0);
	    case 44100: index = 1; break;
	    case 48000: index = 2; break;
            }

            bitrate = gfp->out_samplerate * 16 * gfc->stereo / gfp->compression_ratio / 32000;
	    if ( bitrate > 10 )
	        bitrate = 10;
            band  = 0.5 * bandwidth [index] [bitrate];
        }
#endif  
      
    if (band < 31) {
      gfc->lowpass1 = band/31.0;
      gfc->lowpass2 = band/31.0;
    }
  }
  

  /****************************************************************/
  /* apply user driven filters*/
  /****************************************************************/
  if ( gfp->highpassfreq > 0 ) {
    gfc->highpass1 = 2.0*gfp->highpassfreq/gfp->out_samplerate; /* will always be >=0 */
    if ( gfp->highpasswidth >= 0 ) {
      gfc->highpass2 = 2.0*(gfp->highpassfreq+gfp->highpasswidth)/gfp->out_samplerate;
    } else {
      /* 15% above on default */
      /* gfc->highpass2 = 1.15*2.0*gfp->highpassfreq/gfp->out_samplerate;  */
      gfc->highpass2 = 1.00*2.0*gfp->highpassfreq/gfp->out_samplerate; 
    }
  }

  if ( gfp->lowpassfreq > 0 ) {
    gfc->lowpass2 = 2.0*gfp->lowpassfreq/gfp->out_samplerate; /* will always be >=0 */
    if ( gfp->lowpasswidth >= 0 ) {
      gfc->lowpass1 = 2.0*(gfp->lowpassfreq-gfp->lowpasswidth)/gfp->out_samplerate;
      if ( gfc->lowpass1 < 0 ) { /* has to be >= 0 */
	gfc->lowpass1 = 0;
      }
    } else {
      /* 15% below on default */
      /* gfc->lowpass1 = 0.85*2.0*gfp->lowpassfreq/gfp->out_samplerate;  */
      gfc->lowpass1 = 1.00*2.0*gfp->lowpassfreq/gfp->out_samplerate;
    }
  }


  /***************************************************************/
  /* compute info needed for polyphase filter (filter type==0, default)   */
  /***************************************************************/
  lame_init_params_ppflt(gfc);


  /***************************************************************/
  /* compute info needed for FIR filter (filter_type==1) */
  /***************************************************************/
  /* not yet coded */

  gfc->mode_ext=MPG_MD_LR_LR;
  gfc->stereo = (gfp->mode == MPG_MD_MONO) ? 1 : 2;

  gfc->samplerate_index = SmpFrqIndex(gfp->out_samplerate, &gfp->version);
  if( gfc->samplerate_index < 0) {
    return -1;
  }
  if (gfp->free_format) {
    gfc->bitrate_index=0;
  }else{
    if( (gfc->bitrate_index = BitrateIndex(gfp->brate, gfp->version,gfp->out_samplerate)) < 0) {
      return -1;
    }
  }


  /* choose a min/max bitrate for VBR */
  if (gfp->VBR!=vbr_off) {
    /* if the user didn't specify VBR_max_bitrate: */
    if (0==gfp->VBR_max_bitrate_kbps) {
      gfc->VBR_max_bitrate=14;   /* default: allow 320kbs */
    }else{
      if( (gfc->VBR_max_bitrate  = BitrateIndex(gfp->VBR_max_bitrate_kbps, gfp->version,gfp->out_samplerate)) < 0) {
	return -1;
      }
    }
    if (0==gfp->VBR_min_bitrate_kbps) {
      gfc->VBR_min_bitrate=1;  	/* 8 or 32 kbps */
    }else{
      if( (gfc->VBR_min_bitrate  = BitrateIndex(gfp->VBR_min_bitrate_kbps, gfp->version,gfp->out_samplerate)) < 0) {
	return -1;
      }
    }

    gfp->VBR_min_bitrate_kbps = bitrate_table[gfp->version][gfc->VBR_min_bitrate];
    gfp->VBR_max_bitrate_kbps = bitrate_table[gfp->version][gfc->VBR_max_bitrate];

    gfp->VBR_mean_bitrate_kbps = Min(bitrate_table[gfp->version][gfc->VBR_max_bitrate],gfp->VBR_mean_bitrate_kbps);
    gfp->VBR_mean_bitrate_kbps = Max(bitrate_table[gfp->version][gfc->VBR_min_bitrate],gfp->VBR_mean_bitrate_kbps);
    
    /* Note: ABR mode should normally be used without a -V n setting,
     * (or with the default value of 4)
     * but the code below allows us to test how adjusting the maskings
     * effects CBR encodings.  Lowering the maskings will make LAME
     * work harder to get over=0 and may give better noise shaping?
     */

      /* A third dbQ table */
      /* Can all dbQ setup can be done here using a switch statement? */

     
        switch ( gfp->VBR ) {
	case vbr_abr:
	    assert( (unsigned)gfp->VBR_q < sizeof(dbQ)/sizeof(*dbQ) );
	    assert( (unsigned)gfp->VBR_q < sizeof(atQ)/sizeof(*atQ) );
            gfc->masking_lower = pow (10., 0.1 * dbQ [gfp->VBR_q] );
            gfc->ATH_vbrlower  = atQ [ gfp->VBR_q ];
            break;
        case vbr_rh:
        case vbr_mtrh:
	    assert( (unsigned)gfp->VBR_q < sizeof(atQ)/sizeof(*atQ) );
	    gfc->ATH_vbrlower  = atQ [ gfp->VBR_q ];
            break;
	default:
	    break;
	}
	
    
    // At low levels VBR currently switches to 32 kbps.
    // instead of filling this minimum data rate with useful data, it often
    // happens that LAME thinks all is below ATH and don't use this minimum
    // data rate to have at least a bad quality at this level instead of
    // heavy switch muting. Such switching is very conspicuous.
    
    // The absolute threshold of hearing is so a sharp border so it is
    // senseful to use a SWITCH ON/OFF muting. See delta script, they have a width
    // of 5...7 dB
    // Delta scripts are SPL/freq plots. Frequency slowly increases (1 Terz/minute).
    // SPL raises slowly. The listener have to report the arise of the tone.
    // 0...10 seconds after this report the SPL falls. The listener have the disapperance
    // to report. The SPL still falls for a random 0...10 seconds, then the SPL raises ...

    // this is not the right way to fix this problem.    
    //if ( gfp -> VBR_q < 2 )
    //       gfp -> noATH = 1;

  }

  /* VBR needs at least the output of GPSYCHO,
   * so we have to garantee that by setting a minimum 
   * quality level, actually level 5 does it.
   * the -v and -V x settings switch the quality to level 2
   * you would have to add a -q 5 to reduce the quality
   * down to level 5
   */
  if (gfp->VBR!=vbr_off) gfp->quality=Min(gfp->quality,5);
  /* dont allow forced mid/side stereo for mono output */
  if (gfp->mode == MPG_MD_MONO) gfp->force_ms=0;


  /* Do not write VBR tag if VBR flag is not specified */
  if (gfp->VBR==vbr_off) gfp->bWriteVbrTag=0;
  if (gfp->ogg) gfp->bWriteVbrTag=0;
  if (gfp->analysis) gfp->bWriteVbrTag=0;

  /* some file options not allowed if output is: not specified or stdout */

  if (gfc->pinfo != NULL) {
    gfp->bWriteVbrTag=0;  /* disable Xing VBR tag */
  }

  init_bit_stream_w(gfc);


  /* set internal feature flags.  USER should not access these since
   * some combinations will produce strange results */


    switch ( gfp->quality ) {
    case 9: /* no psymodel, no noise shaping */
	gfc->filter_type        = 0;
	gfc->psymodel           = 0;
	gfc->quantization       = 0;
	gfc->noise_shaping      = 0;
	gfc->noise_shaping_stop = 0;
	gfc->use_best_huffman   = 0;
        break;
	
    case 8:
	gfp->quality = 7;
    case 7: /* use psymodel (for short block and m/s switching), but no noise shapping */
	gfc->filter_type        = 0;
	gfc->psymodel           = 1; /**/
	gfc->quantization       = 0;
	gfc->noise_shaping      = 0;
	gfc->noise_shaping_stop = 0;
	gfc->use_best_huffman   = 0;
	break;

    case 6:
	gfp->quality = 5;
    case 5: /* the default */
	gfc->filter_type        = 0;
	gfc->psymodel           = 1;
	gfc->quantization       = 0;
	gfc->noise_shaping      = 1; /**/
	gfc->noise_shaping_stop = 0;
	gfc->use_best_huffman   = 0;
	break;

    case 4:
	gfp->quality = 3;
    case 3:
	gfc->filter_type        = 0;
	gfc->psymodel           = 1;
	gfc->quantization       = 1; /**/
	gfc->noise_shaping      = 1;
	gfc->noise_shaping_stop = 0;
	gfc->use_best_huffman   = 1; /**/
	break;

    case 2:
	gfc->filter_type        = 0;
	gfc->psymodel           = 1;
	gfc->quantization       = 1;
	gfc->noise_shaping      = 1;
	gfc->noise_shaping_stop = 0;
	gfc->use_best_huffman   = 1;
	break;

    case 1:
	gfc->filter_type        = 0;
	gfc->psymodel           = 1;
	gfc->quantization       = 1;
	gfc->noise_shaping      = 2; /**/
	gfc->noise_shaping_stop = 0;
	gfc->use_best_huffman   = 1;
	break;

    case 0: /* 0..1 quality */
	gfc->filter_type        = 1; /**/ /* not yet coded */
	gfc->psymodel           = 1;
	gfc->quantization       = 1;
	gfc->noise_shaping      = 3; /**/ /* not yet coded */
	gfc->noise_shaping_stop = 2; /**/ /* not yet coded */
	gfc->use_best_huffman   = 2; /**/ /* not yet coded */
	return -2;
    }

    j = gfc->samplerate_index + (3 * gfp->version) + 6 * (gfp->out_samplerate < 16000);
    for (i = 0; i < SBMAX_l + 1; i++)
        gfc->scalefac_band.l[i] = sfBandIndex [j].l[i];
    for (i = 0; i < SBMAX_s + 1; i++)
        gfc->scalefac_band.s[i] = sfBandIndex [j].s[i];

    /* determine the mean bitrate for main data */
    if ( gfp->version == 1 ) /* MPEG 1 */
	gfc->sideinfo_len = (gfc->stereo == 1)  ?  4+17  :  4+32;
    else                     /* MPEG 2 */
	gfc->sideinfo_len = (gfc->stereo == 1)  ?  4+ 9  :  4+17;
  
    if ( gfp->error_protection ) 
        gfc->sideinfo_len += 2;
  

  /* Write id3v2 tag into the bitstream */
  /* this tag must be before the Xing VBR header */
  /* does id3v2 and Xing header really work??? */
  if (!gfp->ogg) {
    id3tag_write_v2(gfp);
  }

  /* Write initial VBR Header to bitstream */
  if (gfp->bWriteVbrTag)
      InitVbrTag(gfp);

  gfc->sfb21_extra = (gfp->VBR==vbr_rh || gfp->VBR==vbr_mtrh || gfp->VBR==vbr_mt)
                   &&(gfp->out_samplerate >= 32000);
  
    gfc->exp_nspsytune = gfp->exp_nspsytune;
    //^                    ^
    // like swimmer and swimner

    /* estimate total frames.  */
    gfp->totalframes           = 2 + gfp->num_samples/(gfc->resample_ratio * gfp->framesize);
    gfc->lame_init_params_init = 1;
    
    return 0;
}

/*}}}*/
/* void          lame_print_config              (lame_global_flags *gfp)                                                                                          *//*{{{*/

/*
 *  print_config
 *
 *  Prints some selected information about the coding parameters via 
 *  the macro command MSGF(), which is currently mapped to lame_errorf 
 *  (reports via a error function?), which is a printf-like function 
 *  for <stderr>.
 */

void lame_print_config ( const lame_global_flags* gfp )
{
    lame_internal_flags* gfc = gfp->internal_flags;
    double    out_samplerate = gfp->out_samplerate;
    double    in_samplerate  = gfp->out_samplerate * gfc->resample_ratio;

    MSGF ( "LAME version %s    (%s)\n", get_lame_version (), LAME_URL );

    if ( gfc->CPU_features_MMX  ||  gfc->CPU_features_3DNow  ||  gfc->CPU_features_SIMD  ||  gfc->CPU_features_SIMD2 ) {
        MSGF ("CPU features:"); 

        if ( gfc->CPU_features_MMX )
#ifdef MMX_choose_table
            MSGF (" MMX (used)" );
#else
	    MSGF (" MMX (unused)" );
#endif            
        if ( gfc->CPU_features_3DNow )
            MSGF (", 3DNow! (unused)");
        if ( gfc->CPU_features_SIMD )
            MSGF (", SIMD (unused)");
        if ( gfc->CPU_features_SIMD2 )
            MSGF (", SIMD2 (unused)");
        MSGF ("\n");  
    }
  
    if ( gfp->num_channels==2  &&  gfc->stereo==1 /* mono */ ) {
	MSGF ("Autoconverting from stereo to mono. Setting encoding to mono mode.\n");
    }
    
    if (gfc->resample_ratio != 1.) {
	MSGF ("Resampling:  input=%g kHz  output=%g kHz\n", 1.e-3 * in_samplerate, 1.e-3 * out_samplerate );
    }
    
    if (gfc->filter_type == 0) {
	if (gfc->highpass2 > 0.)
	    MSGF ("Using polyphase highpass filter, transition band: %5.0f Hz - %5.0f Hz\n",  
		 0.5 * gfc->highpass1 * out_samplerate, 0.5 * gfc->highpass2 * out_samplerate );
	if (gfc->lowpass1 > 0.) {
	    MSGF ("Using polyphase lowpass  filter, transition band: %5.0f Hz - %5.0f Hz\n", 
		 0.5 * gfc->lowpass1 * out_samplerate, 0.5 * gfc->lowpass2 * out_samplerate );
	} else {
	    MSGF ("polyphase lowpass filter disabled\n");
	}
    } else {
	MSGF ("polyphase filters disabled\n");
    }
    
#ifdef RH_AMP
    if (gfp->experimentalY) {
	MSGF ("careful noise shaping, only maximum distorted band at once\n");
    }
#endif
    
    if ( gfp->free_format ) {
	MSGF ("Warning: many decoders cannot handle free format bitstreams\n");
	if ( gfp->brate > 320 ) {
	    MSGF ("Warning: many decoders cannot handle free format bitrates >320 kbps (see documentation)\n");
	}
    }
}

/*}}}*/
/* void          lame_id3v2_tag                 (lame_global_flags *gfp,FILE *outf)                                                                               *//*{{{*/

/*****************************************************************/
/* write ID3 version 2 tag to output file, if asked for          */
/*****************************************************************/
void lame_id3v2_tag(lame_global_flags *gfp,FILE *outf)
{
  /*
   * NOTE: "lame_id3v2_tag" is obviously just a wrapper to call the function
   * below and have a nice "lame_"-prefixed function name in "lame.h".
   * -- gramps
   */
  /* no ID3 version 2 tags in Ogg Vorbis output */

}

/*}}}*/
/* int           lame_encode_frame              (lame_global_flags *gfp, sample_t inbuf_l[],sample_t inbuf_r[], char *mp3buf, int mp3buf_size)                    *//*{{{*/

/* routine to feed exactly one frame (gfp->framesize) worth of data to the 
encoding engine.  All buffering, resampling, etc, handled by calling
program.  
*/
int lame_encode_frame(lame_global_flags *gfp,
sample_t inbuf_l[],sample_t inbuf_r[],
char *mp3buf, int mp3buf_size)
{
  int ret;
  if (gfp->ogg) {
#ifdef HAVEVORBIS
    ret = lame_encode_ogg_frame(gfp,inbuf_l,inbuf_r,mp3buf,mp3buf_size);
#else
    return -5; /* wanna encode ogg without vorbis */
#endif
  } else {
    ret = lame_encode_mp3_frame(gfp,inbuf_l,inbuf_r,mp3buf,mp3buf_size);
  }

    /* check to see if we underestimated totalframes */
    gfp->frameNum++;
    if ( gfp->totalframes < gfp->frameNum ) 
        gfp->totalframes = gfp->frameNum;
  return ret;
}

/*}}}*/
/* int           lame_encode_buffer             (lame_global_flags* gfp, short int buffer_l[], short int buffer_r[], int nsamples, char* mp3buf, int mp3buf_size )*//*{{{*/



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
*/

/*
 *  lame_encode_buffer_interleaved() will disaapear in this form in the next 
 *  version. It is too similar to lame_encode_buffer() and 
 *  lame_encode_buffer_interleaved(). Especially if there are multiple versions
 *   (int16, int24, int32, float) I don't want to care several times nearly the same code
 */

int    lame_encode_buffer (
        lame_global_flags*  gfp,
        const short int     buffer_l [],
        const short int     buffer_r [],
        int                 nsamples,
        char*               mp3buf,
        const int           mp3buf_size )
{
  lame_internal_flags *gfc = gfp->internal_flags;
  int mp3size = 0, ret, i, ch, mf_needed;
  sample_t* mfbuf     [2];
  sample_t* in_buffer [2];
  sample_t* fn_buffer [2];

  if (!gfc->lame_init_params_init) return -3;
  
  fn_buffer [0] = in_buffer [0] = calloc ( sizeof(sample_t), nsamples );
  fn_buffer [1] = in_buffer [1] = calloc ( sizeof(sample_t), nsamples );
  
  if (in_buffer[0] == NULL || in_buffer[1] == NULL) {
    ERRORF ("Error: can't allocate in_buffer buffer\n");
    return -2;
  }
  
  for (i = 0; i < nsamples; i++) {
  
      in_buffer [0] [i] = buffer_l [i];
      in_buffer [1] [i] = buffer_r [i];
  }


  /* some sanity checks */
  assert(ENCDELAY>=MDCTDELAY);
  assert(BLKSIZE-FFTOFFSET >= 0);
  mf_needed = BLKSIZE+gfp->framesize-FFTOFFSET;  /* amount needed for FFT */
  mf_needed = Max(mf_needed,286+576*(1+gfc->mode_gr)); /* amount needed for MDCT/filterbank */
  assert(MFSIZE>=mf_needed);

  mfbuf[0]=gfc->mfbuf[0];
  mfbuf[1]=gfc->mfbuf[1];

  if (gfp->num_channels==2  && gfc->stereo==1) {
    /* downsample to mono */
    for (i=0; i<nsamples; ++i) {
      in_buffer[0][i] = 0.5 * ( (double)in_buffer[0][i] + in_buffer[1][i] );
      in_buffer[1][i] = 0.0;
    }
  }


  while (nsamples > 0) {
    int n_in=0;
    int n_out=0;

    /* copy in new samples into mfbuf, with resampling if necessary */
    if (gfc->resample_ratio != 1.0)  {
      for (ch=0; ch<gfc->stereo; ch++) {
	n_out = fill_buffer_resample(gfp,&mfbuf[ch][gfc->mf_size],gfp->framesize, in_buffer[ch],nsamples,&n_in,ch);
	in_buffer[ch] += n_in;
      }
    }else{
      n_out=Min(gfp->framesize,nsamples);
      n_in=n_out;
      for (i = 0 ; i< n_out; ++i) {
	mfbuf[0][gfc->mf_size+i]=in_buffer[0][i];
	if (gfc->stereo==2)
	  mfbuf[1][gfc->mf_size+i]=in_buffer[1][i];
      }
      in_buffer[0] += n_in;
      in_buffer[1] += n_in;
    }



    nsamples -= n_in;
    gfc->mf_size += n_out;
    assert(gfc->mf_size<=MFSIZE);
    gfc->mf_samples_to_encode += n_out;


    if (gfc->mf_size >= mf_needed) {
      /* encode the frame.  */
      ret = lame_encode_frame(gfp,mfbuf[0],mfbuf[1],mp3buf,mp3buf_size);

      if (ret < 0) 
          goto retr;
      mp3buf += ret;
      mp3size += ret;

      /* shift out old samples */
      gfc->mf_size -= gfp->framesize;
      gfc->mf_samples_to_encode -= gfp->framesize;
      for (ch=0; ch<gfc->stereo; ch++)
	for (i=0; i<gfc->mf_size; i++)
	  mfbuf[ch][i]=mfbuf[ch][i+gfp->framesize];
    }
  }
  assert(nsamples==0);
  ret = mp3size;

retr:
  free (fn_buffer [0]);
  free (fn_buffer [1]);

  return ret;
}




int    lame_encode_buffer_interleaved (
                lame_global_flags* gfp,
                short int          buffer [],
                int                nsamples,
                char*              mp3buf,
                int                mp3buf_size )
{
  int mp3size = 0, ret, i, ch, mf_needed;
  lame_internal_flags *gfc=gfp->internal_flags;
  sample_t *mfbuf[2];
    signed short int* p;

  if (!gfc->lame_init_params_init) return -3;

  mfbuf[0]=gfc->mfbuf[0];
  mfbuf[1]=gfc->mfbuf[1];

  /* some sanity checks */
  assert(ENCDELAY>=MDCTDELAY);
  assert(BLKSIZE-FFTOFFSET >= 0);
  mf_needed = BLKSIZE+gfp->framesize-FFTOFFSET;
  assert(MFSIZE>=mf_needed);

  if (gfp->num_channels == 1) {
    return lame_encode_buffer(gfp,buffer,NULL,nsamples,mp3buf,mp3buf_size);
  }

  if (gfc->resample_ratio != 1.0)  {
    short int *buffer_l;
    short int *buffer_r;
    
    buffer_l=malloc(sizeof(short int)*nsamples);
    buffer_r=malloc(sizeof(short int)*nsamples);
    if (buffer_l == NULL || buffer_r == NULL) {
      return -2;
    }
    for (i=0; i<nsamples; i++) {
      buffer_l[i]=buffer[2*i];
      buffer_r[i]=buffer[2*i+1];
    }
    ret = lame_encode_buffer(gfp,buffer_l,buffer_r,nsamples,mp3buf,mp3buf_size);
    free(buffer_l);
    free(buffer_r);
    return ret;
  }

  assert (gfp->num_channels == 2);
  
  while (nsamples > 0) {         /* while copying in new samples */
    int n_out = Min (gfp->framesize,nsamples);
    
#ifdef KLEMM_09  /* only a try to understand the code */

    assert (gfp->num_channels == 2);
    p = buffer;
    if ( gfc->stereo == 1 /* mono, although true */ )
        for (i = 0; i < n_out; i++, p += 2 ) {
	    mfbuf [0] [gfc->mf_size + i] = 0.5 * ((sample_t)p[0] + (sample_t)p[1]);
	    mfbuf [1] [gfc->mf_size + i] = 0.;
        }
    else
        for (i = 0; i < n_out; i++, p += 2) {
	    mfbuf [0] [gfc->mf_size + i] = p[0];
	    mfbuf [1] [gfc->mf_size + i] = p[1];
        }
      
#else 
   
    for (i=0; i<n_out; ++i) {
      if (gfp->num_channels==2  && gfc->stereo==1) {
	mfbuf[0][gfc->mf_size+i]=((int)buffer[2*i]+(int)buffer[2*i+1])/2.0;
	mfbuf[1][gfc->mf_size+i]=0;
      }else{
	mfbuf[0][gfc->mf_size+i]=buffer[2*i];
	mfbuf[1][gfc->mf_size+i]=buffer[2*i+1];
      }
    }
      
#endif
      
    buffer += 2*n_out;

    nsamples -= n_out;
    gfc->mf_size += n_out;
    assert(gfc->mf_size<=MFSIZE);
    gfc->mf_samples_to_encode += n_out;

    if (gfc->mf_size >= mf_needed) {
      /* encode the frame */
      ret = lame_encode_frame(gfp,mfbuf[0],mfbuf[1],mp3buf,mp3buf_size);
      if (ret < 0) {
	/* fatel error: mp3buffer was too small */
	return ret;
      }
      mp3buf += ret;
      mp3size += ret;

      /* shift out old samples */
      gfc->mf_size -= gfp->framesize;
      gfc->mf_samples_to_encode -= gfp->framesize;
      for (ch=0; ch<gfc->stereo; ch++)
	for (i=0; i<gfc->mf_size; i++)
	  mfbuf[ch][i]=mfbuf[ch][i+gfp->framesize];
    }
  }
  assert(nsamples==0);
  return mp3size;
}


/*}}}*/
/* int           lame_encode                    (lame_global_flags* gfp, short int in_buffer[2][1152], char* mp3buf, int size )                                   *//*{{{*/

    
/* old LAME interface.  use lame_encode_buffer instead */

int lame_encode (
        lame_global_flags* const  gfp,
        const short int           in_buffer [2] [1152],
        char* const               mp3buf,
        const int                 size )
{
    lame_internal_flags*  gfc = gfp->internal_flags;
  
    if ( !gfc->lame_init_params_init ) 
        return -3;
  
    return lame_encode_buffer ( gfp, in_buffer[0], in_buffer[1], gfp->framesize, mp3buf, size );
}

/*}}}*/
/* int           lame_encode_flush              (lame_global_flags* gfp, char* mp3buffer, int mp3buffer_size )                                                    *//*{{{*/
    
/*****************************************************************/
/* flush internal mp3 buffers,                                   */
/*****************************************************************/

int    lame_encode_flush (
                lame_global_flags* gfp,
                char*              mp3buffer,
                int                mp3buffer_size )
{
    short int buffer[2][1152];
    int imp3 = 0, mp3count, mp3buffer_size_remaining;
    lame_internal_flags *gfc = gfp->internal_flags;

    memset ( buffer, 0, sizeof(buffer) );
    mp3count = 0;

    while (gfc->mf_samples_to_encode > 0) {

        mp3buffer_size_remaining = mp3buffer_size - mp3count;

        /* if user specifed buffer size = 0, dont check size */
        if (mp3buffer_size == 0)
            mp3buffer_size_remaining = 0;  

        /* send in a frame of 0 padding until all internal sample buffers
         * are flushed 
         */
        imp3 = lame_encode_buffer (gfp, buffer[0], buffer[1], gfp->framesize,
                                   mp3buffer, mp3buffer_size_remaining);
        /* dont count the above padding: */
        gfc->mf_samples_to_encode -= gfp->framesize;

        if (imp3 < 0) {
            /* some type of fatel error */
            return imp3;
        }
        mp3buffer += imp3;
        mp3count += imp3;
    }

    mp3buffer_size_remaining = mp3buffer_size - mp3count;
    /* if user specifed buffer size = 0, dont check size */
    if (mp3buffer_size == 0) mp3buffer_size_remaining = 0;  

    if (gfp->ogg) {
#ifdef HAVEVORBIS
        /* ogg related stuff */
        imp3 = lame_encode_ogg_finish(gfp, mp3buffer, mp3buffer_size_remaining);
#endif
    }else{
        /* mp3 related stuff.  bit buffer might still contain some mp3 data */
        flush_bitstream(gfp);
        /* write a id3 tag to the bitstream */
        id3tag_write_v1(gfp);
        imp3 = copy_buffer(mp3buffer, mp3buffer_size_remaining, &gfc->bs);
    }

    if (imp3 < 0) {
        return imp3;
    }
    mp3count += imp3;
    return mp3count;
}

/*}}}*/
/* void          lame_close                     (lame_global_flags *gfp)                                                                                          *//*{{{*/
    
/***********************************************************************
 *
 *      lame_close ()
 *
 *  frees internal buffers
 *
 ***********************************************************************/
 
void lame_close (lame_global_flags *gfp)
{
    freegfc(gfp->internal_flags);    
    if (gfp->lame_allocated_gfp) free(gfp);
}


/*}}}*/
/* int           lame_encode_finish             (lame_global_flags* gfp, char* mp3buffer, int mp3buffer_size )                                                    *//*{{{*/

    
/*****************************************************************/
/* flush internal mp3 buffers, and free internal buffers         */
/*****************************************************************/

int    lame_encode_finish (
                lame_global_flags* gfp,
                char*              mp3buffer,
                int                mp3buffer_size )
{
    int ret = lame_encode_flush( gfp, mp3buffer, mp3buffer_size );
    
    lame_close(gfp);
    
    return ret;
}

/*}}}*/
/* void          lame_mp3_tags_fid              (lame_global_flags *gfp,FILE *fpStream)                                                                           *//*{{{*/
    
/*****************************************************************/
/* write VBR Xing header, and ID3 version 1 tag, if asked for    */
/*****************************************************************/
void lame_mp3_tags_fid(lame_global_flags *gfp,FILE *fpStream)
{
  if (gfp->bWriteVbrTag && (gfp->VBR!=vbr_off))
    {
      /* Calculate relative quality of VBR stream
       * 0=best, 100=worst */
      int nQuality=gfp->VBR_q*100/9;

      /* Write Xing header again */
      if (fpStream && !fseek(fpStream, 0, SEEK_SET)) 
	PutVbrTag(gfp,fpStream,nQuality);
    }


}
/*}}}*/
/* lame_global_flags *lame_init                 (void)                                                                                                            *//*{{{*/

lame_global_flags *lame_init(void)
{
  lame_global_flags *gfp;
  int ret;
  gfp = malloc(sizeof(lame_global_flags));
  if (NULL==gfp) return NULL;

  ret = lame_init_old(gfp);
  gfp->lame_allocated_gfp=1;
  if (0!=ret) return NULL;
  return gfp;

}

/*}}}*/
/* int           lame_init_old                  (lame_global_flags *gfp)                                                                                          *//*{{{*/
    
/* initialize mp3 encoder */
int lame_init_old(lame_global_flags *gfp)
{
  lame_internal_flags *gfc;

  /*
   *  Disable floating point exceptions
   */
#ifdef __FreeBSD__
  {
  /* seet floating point mask to the Linux default */
  fp_except_t mask;
  mask=fpgetmask();
  /* if bit is set, we get SIGFPE on that error! */
  fpsetmask(mask & ~(FP_X_INV|FP_X_DZ));
  /*  DEBUGF("FreeBSD mask is 0x%x\n",mask); */
  }
#endif
#if defined(__riscos__) && !defined(ABORTFP)
  /* Disable FPE's under RISC OS */
  /* if bit is set, we disable trapping that error! */
  /*   _FPE_IVO : invalid operation */
  /*   _FPE_DVZ : divide by zero */
  /*   _FPE_OFL : overflow */
  /*   _FPE_UFL : underflow */
  /*   _FPE_INX : inexact */
  DisableFPETraps( _FPE_IVO | _FPE_DVZ | _FPE_OFL );
#endif


  /*
   *  Debugging stuff
   *  The default is to ignore FPE's, unless compiled with -DABORTFP
   *  so add code below to ENABLE FPE's.
   */

#if defined(ABORTFP) 
#if defined(_MSC_VER)
  {
	#include <float.h>
	unsigned int mask;
	mask=_controlfp( 0, 0 );
	mask&=~(_EM_OVERFLOW|_EM_UNDERFLOW|_EM_ZERODIVIDE|_EM_INVALID);
	mask=_controlfp( mask, _MCW_EM );
	}
#elif defined(__CYGWIN__)
#  define _FPU_GETCW(cw) __asm__ ("fnstcw %0" : "=m" (*&cw))
#  define _FPU_SETCW(cw) __asm__ ("fldcw %0" : : "m" (*&cw))

#  define _EM_INEXACT     0x00000001 /* inexact (precision) */
#  define _EM_UNDERFLOW   0x00000002 /* underflow */
#  define _EM_OVERFLOW    0x00000004 /* overflow */
#  define _EM_ZERODIVIDE  0x00000008 /* zero divide */
#  define _EM_INVALID     0x00000010 /* invalid */
  {
    unsigned int mask;
    _FPU_GETCW(mask);
    /* Set the FPU control word to abort on most FPEs */
    mask &= ~(_EM_UNDERFLOW | _EM_OVERFLOW | _EM_ZERODIVIDE | _EM_INVALID);
    _FPU_SETCW(mask);
  }
# elif defined(__linux__)
  {
#  include <fpu_control.h>
#  ifndef _FPU_GETCW
#  define _FPU_GETCW(cw) __asm__ ("fnstcw %0" : "=m" (*&cw))
#  endif
#  ifndef _FPU_SETCW
#  define _FPU_SETCW(cw) __asm__ ("fldcw %0" : : "m" (*&cw))
#  endif
    unsigned int mask;
    _FPU_GETCW(mask);
    /* Set the Linux mask to abort on most FPE's */
    /* if bit is set, we _mask_ SIGFPE on that error! */
    /*  mask &= ~( _FPU_MASK_IM | _FPU_MASK_ZM | _FPU_MASK_OM | _FPU_MASK_UM );*/
    mask &= ~( _FPU_MASK_IM | _FPU_MASK_ZM | _FPU_MASK_OM );
    _FPU_SETCW(mask);
  }
#endif
#endif /* ABORTFP */



  memset(gfp,0,sizeof(lame_global_flags));
  if (NULL==(gfp->internal_flags = malloc(sizeof(lame_internal_flags))))
    return -1;
  gfc=(lame_internal_flags *) gfp->internal_flags;
  memset(gfc,0,sizeof(lame_internal_flags));

  /* Global flags.  set defaults here for non-zero values */
  /* see lame.h for description */
  gfp->mode = MPG_MD_JOINT_STEREO;
  gfp->original=1;
  gfp->in_samplerate=1000*44.1;
  gfp->num_channels=2;
  gfp->num_samples=MAX_U_32_NUM;

  gfp->bWriteVbrTag=1;
  gfp->quality=5;

  gfp->lowpassfreq=0;
  gfp->highpassfreq=0;
  gfp->lowpasswidth = -1;
  gfp->highpasswidth = -1;
  
  gfp->padding_type=2;
  gfp->VBR=vbr_off;
  gfp->VBR_q=4;
  gfp->VBR_mean_bitrate_kbps=128;
  gfp->VBR_min_bitrate_kbps=0;
  gfp->VBR_max_bitrate_kbps=0;
  gfp->VBR_hard_min=0;

  gfc->resample_ratio=1;
  gfc->lowpass_band=32;
  gfc->highpass_band = -1;
  gfc->VBR_min_bitrate=1;
  gfc->VBR_max_bitrate=13;

  gfc->OldValue[0]=180;
  gfc->OldValue[1]=180;
  gfc->CurrentStep=4;
  gfc->masking_lower=1;


  memset(&gfc->bs, 0, sizeof(Bit_stream_struc));
  memset(&gfc->l3_side,0x00,sizeof(III_side_info_t));

  memset((char *) gfc->mfbuf, 0, sizeof(gfc->mfbuf[0][0])*2*MFSIZE);

  /* The reason for
   *       int mf_samples_to_encode = ENCDELAY + 288;
   * ENCDELAY = internal encoder delay.  And then we have to add 288
   * because of the 50% MDCT overlap.  A 576 MDCT granule decodes to
   * 1152 samples.  To synthesize the 576 samples centered under this granule
   * we need the previous granule for the first 288 samples (no problem), and
   * the next granule for the next 288 samples (not possible if this is last
   * granule).  So we need to pad with 288 samples to make sure we can
   * encode the 576 samples we are interested in.
   */
  gfc->mf_samples_to_encode = ENCDELAY+288;
  gfc->mf_size=ENCDELAY-MDCTDELAY;  /* we pad input with this many 0's */

  main_CRC_init ();  /* ulgy, it's C, not Ada, C++ or Modula */

  return 0;
}

/*}}}*/

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
 
void lame_bitrate_hist( 
        const lame_global_flags * const gfp, 
              int                       bitrate_count[14] )
{
    const lame_internal_flags *gfc;
    int i;
    
    assert( NULL != gfp );
    assert( NULL != gfp->internal_flags );
    assert( NULL != bitrate_count );
    
    gfc = gfp->internal_flags;
    
    for (i = 0; i < 14; i++) 
        bitrate_count [i] = gfc->bitrate_stereoMode_Hist [i+1][4];
}


void lame_bitrate_kbps( 
        const lame_global_flags * const gfp, 
              int                       bitrate_kbps [14] )
{
    const lame_internal_flags *gfc;
    int i;
    
    assert( NULL != gfp );
    assert( NULL != gfp->internal_flags );
    assert( NULL != bitrate_kbps );
    
    gfc = gfp->internal_flags;
    
    for (i = 0; i < 14; i++) 
        bitrate_kbps [i] = bitrate_table [gfp->version] [i+1];
}


 
void lame_stereo_mode_hist( 
        const lame_global_flags * const gfp, 
              int                       stmode_count[4] )
{
    const lame_internal_flags * gfc;
    int i;

    assert( gfp != NULL );
    assert( gfp->internal_flags != NULL );
    assert( stmode_count != NULL );
    
    gfc = gfp->internal_flags;
    
    for (i = 0; i < 4; i++) {
        int j, sum = 0;
        for (j = 0; j < 14; j++)
            sum += gfc->bitrate_stereoMode_Hist [j+1][i];
        stmode_count[i] = sum; 
    }
}


    
void lame_bitrate_stereo_mode_hist ( 
        const lame_global_flags* const  gfp, 
        int  bitrate_stmode_count [14] [4] )
{
    const lame_internal_flags* gfc;
    int  i;
    int  j;

    assert ( NULL != gfp );
    assert ( NULL != gfp->internal_flags );
    assert ( NULL != bitrate_stmode_count );
    
    gfc = gfp->internal_flags;
    
    for ( j = 0; j < 14; j++ )
        for ( i = 0; i < 4; i++ )
            bitrate_stmode_count [j][i] = gfc->bitrate_stereoMode_Hist [j+1][i];
}

/* end of lame.c */
