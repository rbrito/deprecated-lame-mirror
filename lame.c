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
#include <time.h>
#ifdef __unix__
   #include <sys/time.h>
   #include <unistd.h>
#endif


#include "gtkanal.h"
#include "lame.h"
#include "util.h"
#include "timestatus.h"
#include "psymodel.h"
#include "newmdct.h"
#include "quantize.h"
#include "quantize_pvt.h"
#include "bitstream.h"
#include "version.h"
#include "VbrTag.h"
#include "id3tag.h"
#include "tables.h"
#include "brhist.h"
#include "get_audio.h"

#ifdef __riscos__
#include "asmstuff.h"
#endif

/* lame_init_params_ppflt_lowpass */

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
		DEBUGF("lowpass band=%i  amp=%f \n",
		       band, gfc->amp_lowpass[band]);
		*/
	}
}

/* lame_init_params_ppflt */

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
			MSGF("Warning: highpass filter disabled. highpass frequency too small\n");
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

/********************************************************************
 *   initialize internal params based on data in gf
 *   (globalflags struct filled in by calling program)
 *
 ********************************************************************/
int lame_init_params(lame_global_flags *gfp)
{
    static unsigned char vbr_compression_ratio_guess [] = { 
        52, 57, 64, 71, 89, 98, 110, 125, 137, 150 
    };
    static const float  masking_lower [] = { 
        0.316, 0.422, 0.562, 0.750, 1.000, 1.096, 1.202, 1.318, 1.445, 1.585 
    };
    int                     i;
    const scalefac_struct*  sf;
    lame_internal_flags*    gfc = gfp -> internal_flags;
  
  gfc->lame_init_params_init=1;

  gfp->frameNum=0;
  if (gfp->num_channels==1) {
    gfp->mode = MPG_MD_MONO;
  }
  gfc->channels=2;
  if (gfp->mode == MPG_MD_MONO) gfc->channels=1;

  if (gfp->gtkflag) 
    gfp->silent=1;

  if (gfp->silent) {
   gfp->brhist_disp=0;  /* turn of VBR historgram */
  }
  if (gfp->VBR==vbr_off) {
    gfp->brhist_disp=0;  /* turn of VBR historgram */
  }
  if (gfp->VBR!=vbr_off) {
    gfp->free_format=0;  /* VBR cant mix with free format */
  }

    if ( gfp->VBR == vbr_off  &&  gfp->brate == 0  &&  gfp->compression_ratio == 0 ) {
        /* CBR/ABR, no bitrate or compression ratio specified, use 11.025 (CD => 128 kbps) */
        gfp->compression_ratio = 44100.0 * 16 * 2 / 128000;
    }


  /* find bitrate if user specify a compression ratio */
  if (gfp->VBR==vbr_off && gfp->compression_ratio > 0) {
    
    if (gfp->out_samplerate==0) 
      gfp->out_samplerate = RoundSampleRateUp (gfp->in_samplerate);   
    
    /* choose a bitrate for the output samplerate which achieves
     * specifed compression ratio 
     */
     
    gfp->brate = 
      gfp->out_samplerate*16*gfc->channels/(1000.0*gfp->compression_ratio);

    /* we need the version for the bitrate table look up */
    gfc->samplerate_index = SmpFrqIndex((long)gfp->out_samplerate, &gfp->version);
    /* find the nearest allowed bitrate */
    if (!gfp->free_format)
      gfp->brate = FindNearestBitrate(gfp->brate,gfp->version,gfp->out_samplerate);
  }
  if (gfp->brate >= 320) gfp->VBR=vbr_off;  /* dont bother with VBR at 320kbs */



  /* set the output sampling rate, and resample options if necessary
     samplerate = input sample rate
     resamplerate = ouput sample rate
  */
  if (gfp->out_samplerate==0) {
    /* user did not specify output sample rate */
    gfp->out_samplerate=gfp->in_samplerate;   /* default */


    /* if resamplerate is not valid, find a valid value */
    gfp->out_samplerate = RoundSampleRateToNearest (gfp -> out_samplerate);

    if (gfp->VBR==vbr_off && gfp->brate>0) {
      /* check if user specified bitrate requires downsampling */
      gfp->compression_ratio = gfp->out_samplerate*16*gfc->channels/(1000.0*gfp->brate);
      if (gfp->compression_ratio > 13 ) {
	/* automatic downsample, if possible */
	gfp->out_samplerate = RoundSampleRateToNearest ((10*1000L*gfp->brate)/(16*gfc->channels));
      }
    }
    if (gfp->VBR==vbr_abr) {
      /* check if user specified bitrate requires downsampling */
      gfp->compression_ratio = gfp->out_samplerate*16*gfc->channels/(1000.0*gfp->VBR_mean_bitrate_kbps);
      if (gfp->compression_ratio > 13 ) {
	/* automatic downsample, if possible */
	gfp->out_samplerate = RoundSampleRateToNearest ((10*1000L*gfp->VBR_mean_bitrate_kbps)/(16*gfc->channels));
      }
    }
  }

  gfc->mode_gr = (gfp->out_samplerate <= 24000) ? 1 : 2;  /* mode_gr = 2 */
  gfp->encoder_delay = ENCDELAY;
  gfp->framesize = gfc->mode_gr*576;
  if (gfp->ogg) gfp->framesize = 1024;


  gfc->resample_ratio=1.0;
  if (gfp->out_samplerate != gfp->in_samplerate) 
        gfc->resample_ratio = (long double)gfp->in_samplerate/gfp->out_samplerate;

  /* estimate total frames.  must be done after setting sampling rate so
   * we know the framesize.  */
  gfp->totalframes=0;
  if(gfp->input_format == sf_mp1 && gfp->decode_only)
    gfp->totalframes = (gfp->num_samples+383)/384;
  else
  if(gfp->input_format == sf_mp2 && gfp->decode_only)
    gfp->totalframes = (gfp->num_samples+1151)/1152;
  else
    gfp->totalframes = 2+ gfp->num_samples/(gfc->resample_ratio*gfp->framesize);


    /*
     * For the CBR and ABR mode the compression ratio is (nearly) correct,
     * for VBR it is only a guess. The real compression rate also depends on
     * the kind of music, so don't waste too much time for "correct" values
     * in the table vbr_compression_ratio_guess.
     *
     * Nevertheless at the end of the beta phase of a lame version, the
     * table should be calibrated. Take a normal piece of music (rock) and
     * compress the WAV file to different MP3s via:
     *
     *	in=/archive/Audio/Queen/We_Will_Rock_You
     *	out=We_Will_Rock_You
     *	for i in 0 1 2 3 4 5 6 7 8 9; do
     *	    lame -V $i $in.wav $out.$i.mp3
     *	done
     *	a=`cat $in.wav | wc -c`
     *	echo
     *	for i in 0 1 2 3 4 5 6 7 8 9; do
     *	    b=`cat $out.$i.mp3 | wc -c`
     *	    c=$[ 20*a/b + 1]
     *	    echo -n $[c/2], " "
     *	done
     *	echo
     *
     *  If you need lower bitrates than -V9 supports, you must reduce
     *  sampling frequency via --resample xx.xx
     */

    switch ( gfp -> VBR ) {
        
    case vbr_mt:
    case vbr_rh:
        assert ( (unsigned)(gfp -> VBR_q) < sizeof(vbr_compression_ratio_guess)/sizeof(*vbr_compression_ratio_guess) );
        gfp -> compression_ratio = 0.1 * vbr_compression_ratio_guess [gfp -> VBR_q];
	break;
    case vbr_abr:
        gfp -> compression_ratio = gfp->out_samplerate * 16.e-3 * gfc->channels / gfp->VBR_mean_bitrate_kbps;
	break;
    case vbr_off:
        gfp -> compression_ratio = gfp->out_samplerate * 16.e-3 * gfc->channels / gfp->brate;
	break;
    default:
        assert (0);
        return -1;
    }

    /* At higher quality (lower compression) use STEREO instead of JSTEREO.
     * (unless the user explicitly specified a mode ) 
     */
     
    if ( ! gfp -> mode_fixed  &&  gfp -> mode != MPG_MD_MONO ) {
        // 160 kbps and stereo instead of j-stereo is a bad idea
        if ( gfp -> compression_ratio < 7.35 ) {  // switch to stereo for > 192 kbps
            gfp -> mode = MPG_MD_STEREO;
        }
    }


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
    int band = 1+floor(.5 + 14-18*log(gfp->compression_ratio/16.0));
    if (gfc->resample_ratio != 1.0) {
      /* resampling.  if we are resampling, add lowpass at least 90% */
      band = Min(band,29);
    }

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
  gfc->channels = (gfp->mode == MPG_MD_MONO) ? 1 : 2;

  gfc->samplerate_index = SmpFrqIndex((long)gfp->out_samplerate, &gfp->version);

  if( gfc->samplerate_index < 0) {
    display_bitrates(stderr);
    return -1;
  }
  if (gfp->free_format || gfp->VBR!=vbr_off) {
    gfc->bitrate_index=0;
  }else{
    if( (gfc->bitrate_index = BitrateIndex(gfp->brate, gfp->version,gfp->out_samplerate)) < 0) {
      display_bitrates(stderr);
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
	display_bitrates(stderr);
	return -1;
      }
    }
    if (0==gfp->VBR_min_bitrate_kbps) {
      gfc->VBR_min_bitrate=1;  /* 32 kbps */
    }else{
      if( (gfc->VBR_min_bitrate  = BitrateIndex(gfp->VBR_min_bitrate_kbps, gfp->version,gfp->out_samplerate)) < 0) {
	display_bitrates(stderr);
	return -1;
      }
    }
    gfp->VBR_mean_bitrate_kbps = Min(index_to_bitrate[gfp->version][gfc->VBR_max_bitrate],gfp->VBR_mean_bitrate_kbps);
    gfp->VBR_mean_bitrate_kbps = Max(index_to_bitrate[gfp->version][gfc->VBR_min_bitrate],gfp->VBR_mean_bitrate_kbps);
    
    /* Note: ABR mode should normally be used without a -V n setting,
     * (or with the default value of 4)
     * but the code below allows us to test how adjusting the maskings
     * effects CBR encodings.  Lowering the maskings will make LAME
     * work harder to get over=0 and may give better noise shaping?
     */
     
        switch ( gfp -> VBR ) {
	case vbr_abr:
            assert ( (unsigned) (gfp -> VBR_q) < sizeof(masking_lower)/sizeof(*masking_lower) );
            gfc -> masking_lower = masking_lower [gfp -> VBR_q];
            gfc -> ATH_vbrlower  = 4. * ( 4 - gfp -> VBR_q ); 
	    break;
    
        case vbr_rh:
            gfc -> ATH_vbrlower  = 4. * ( 4 - gfp -> VBR_q );
	    break;
	    
        case vbr_off:
        case vbr_mt:
            break;
        }     



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
  if (gfp->gtkflag) gfp->bWriteVbrTag=0;

  /* some file options not allowed if output is: not specified or stdout */

  if (gfp->outPath!=NULL && gfp->outPath[0]=='-' ) {
    gfp->bWriteVbrTag=0; /* turn off VBR tag */
  }

  if (gfp->outPath==NULL || gfp->outPath[0]=='-' ) {
    gfp->id3v1_enabled=0;       /* turn off ID3 version 1 tagging */
  }



  if (gfc->pinfo != NULL) {
    gfp->bWriteVbrTag=0;  /* disable Xing VBR tag */
  }

    init_bit_stream_w (gfc);


    /* set internal feature flags.
     * User should not access these since some combinations 
     * will produce strange results 
     */

    switch ( gfp -> quality ) {
    case 9:  /* no psymodel, no noise shaping */
	gfc -> filter_type        = 0;
	gfc -> psymodel           = 0;
	gfc -> quantization       = 0;
	gfc -> noise_shaping      = 0;
	gfc -> noise_shaping_stop = 0;
	gfc -> use_best_huffman   = 0;
        break;
    case 8: gfp -> quality = 7;
    case 7:  /* use psymodel (for short block and m/s switching), but no noise shaping */
	gfc -> filter_type        = 0;
	gfc -> psymodel           = 1;
	gfc -> quantization       = 0;
	gfc -> noise_shaping      = 0;
	gfc -> noise_shaping_stop = 0;
	gfc -> use_best_huffman   = 0;
        break;
    case 6: gfp -> quality = 5;
    case 5:  /* the default */
	gfc -> filter_type        = 0;
	gfc -> psymodel           = 1;
	gfc -> quantization       = 0;
        gfc -> noise_shaping      = 1;
        gfc -> noise_shaping_stop = 0;
        gfc -> use_best_huffman   = 0;
        break;
    case 4: gfp -> quality = 3;
    case 3:  /* missing comment */
	gfc -> filter_type        = 0;
	gfc -> psymodel           = 1;
	gfc -> quantization       = 1;
	gfc -> noise_shaping      = 1;
	gfc -> noise_shaping_stop = 0;
	gfc -> use_best_huffman   = 1;
        break;
    case 2:
	gfc -> filter_type        = 0;
	gfc -> psymodel           = 1;
	gfc -> quantization       = 1;
	gfc -> noise_shaping      = 1;
	gfc -> noise_shaping_stop = 0;
	gfc -> use_best_huffman   = 1;
        break;
    case 1:
	gfc -> filter_type        = 0;
	gfc -> psymodel           = 1;
	gfc -> quantization       = 1;
	gfc -> noise_shaping      = 2;
	gfc -> noise_shaping_stop = 0;
	gfc -> use_best_huffman   = 1;
	break;
    case 0:
	gfc -> filter_type        = 1;  /* not yet coded */
	gfc -> psymodel           = 1;
	gfc -> quantization       = 1;
	gfc -> noise_shaping      = 3;  /* not yet coded */
	gfc -> noise_shaping_stop = 2;  /* not yet coded */
	gfc -> use_best_huffman   = 2;  /* not yet coded */
	return -1;
	break;
    default:
        assert (0);
	return -1;
    }
    
    sf = sfBandIndex + gfc->samplerate_index + 3 * (gfp->version + 2 * (gfp->out_samplerate < 16000) );

    for ( i = 0; i <= SBMAX_l; i++ )
        gfc -> scalefac_band.l [i] = sf -> l [i];
 
    for ( i = 0; i <= SBMAX_s; i++ )
        gfc -> scalefac_band.s [i] = sf -> s [i];


    /* determine the mean bitrate for main data */
    gfc -> sideinfo_len = 4;
  
    switch ( gfp -> version ) {
    case 1: /* MPEG 1 */
        gfc -> sideinfo_len += gfc->channels == 1  ?  17  :  32;
        break;
    case 0: /* MPEG 2 */
        gfc -> sideinfo_len += gfc->channels == 1  ?   9  :  17;
        break;
    default:
        assert (0);
        return -1;
    }
  
    if (gfp -> error_protection) 
        gfc -> sideinfo_len += 2;                 /* 16 bit CRC eats 2 bytes */

    if (gfp -> bWriteVbrTag)        /* Write initial VBR Header to bitstream */
        InitVbrTag (gfp);

    if (gfp -> brhist_disp)
        brhist_init ( gfp, 1, 14 );

#ifdef HAVEVORBIS
    if (gfp -> ogg) {
        lame_encode_ogg_init (gfp);
        gfc -> filter_type = -1;        /* vorbis claims not to need filters */
        gfp -> VBR         = vbr_off;   /* ignore lame's various VBR modes   */
    }
#endif

    return 0;
}









/************************************************************************
 *
 * print_config
 *
 * PURPOSE:  Prints the encoding parameters used
 *
 ************************************************************************/
void lame_print_config(lame_global_flags *gfp)
{
  lame_internal_flags *gfc=gfp->internal_flags;

  static const char *mode_names[4] = { "stereo", "j-stereo", "dual-ch", "single-ch" };
  FLOAT out_samplerate=gfp->out_samplerate/1000.0;
  FLOAT in_samplerate = gfc->resample_ratio*out_samplerate;

  lame_print_version(stderr);
  if (gfp->num_channels==2 && gfc->channels==1) {
    MSGF("Autoconverting from stereo to mono. Setting encoding to mono mode.\n");
  }
  if (gfc->resample_ratio!=1.0) {
    MSGF("Resampling:  input=%g kHz  output=%g kHz\n",
	    in_samplerate,out_samplerate);
  }
  if (gfc->filter_type==0) {
    if (gfc->highpass2>0.0)
      MSGF("Using polyphase highpass filter, transition band: %5.0f Hz - %5.0f Hz\n", 
           gfc->highpass1*out_samplerate*500, gfc->highpass2*out_samplerate*500);
    if (gfc->lowpass1>0.0)
      MSGF("Using polyphase lowpass  filter, transition band: %5.0f Hz - %5.0f Hz\n", 
           gfc->lowpass1*out_samplerate*500, gfc->lowpass2*out_samplerate*500);
    else
      MSGF("polyphase lowpass filter disabled\n");
  } else {
    MSGF("polyphase filters disabled\n");
  }
#ifdef RH_AMP
  if (gfp->experimentalY) {
    MSGF("careful noise shaping, only maximum distorted band at once\n");
  }
#endif
  if (gfp->gtkflag) {
    MSGF("Analyzing %s \n",gfp->inPath);
  }
  else {
    MSGF("Encoding %s%sto %s\n",
	    (strcmp(gfp->inPath, "-")? gfp->inPath : "<stdin>"),
	    strlen(gfp->inPath)+strlen(gfp->outPath) > 66 ? "\n      " : " ",
	    (strcmp(gfp->outPath, "-")? gfp->outPath : "<stdout>"));
    if (gfp->ogg) {
      MSGF("Encoding as %g kHz VBR Ogg Vorbis \n",
	      gfp->out_samplerate/1000.0);
    } else {
    
    	MSGF ( "Encoding as %g kHz ", gfp -> out_samplerate / 1000.0 );
    
    	switch ( gfp -> VBR ) {
    	case vbr_mt:
    	case vbr_rh:
      		MSGF ("VBR(q=%i)", gfp->VBR_q );
              	break;
    	case vbr_abr:
      		MSGF ("average %u kbps", gfp -> VBR_mean_bitrate_kbps );
         	break;
        default:
      		MSGF ("%u kbps", gfp -> brate );
          	break;
        }
    	MSGF (" %s MPEG-%u%s Layer III (%gx estim) qval=%i\n", 
	      mode_names [gfp -> mode],
              2 - gfp -> version, 
              gfp -> out_samplerate < 16000 ? ".5" : "",
              0.1 * (int) (gfp -> compression_ratio * 10 + 0.5),
              gfp -> quality );
    }
  }
  if (gfp->free_format) {
    MSGF("Warning: many decoders cannot handle free format bitstreams\n");
    if (gfp->brate>320) {
      MSGF("Warning: many decoders cannot handle free format bitrates > 320 kbps\n");
    }
  }

  fflush(stderr);
}



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
#ifdef HAVEVORBIS
  /* no ID3 version 2 tags in Ogg Vorbis output */
  if (!gfp->ogg) {
#endif
    id3tag_write_v2(&gfp->tag_spec,outf);
#ifdef HAVEVORBIS
  }
#endif
}



#ifdef RH_VALIDATE_MS
/* check if this frame is with a high probability 
 * a candidate for mid sum coding
 * (adapted from korr.c by Frank Klemm, RH 2000-08-23)
 */


/*
  Remark from pfk@uni-jena.de: 
  
  this makes not so much sense, because of a handicap of MP3: MP3 can only
  handle MS (dp=45°) and LR (dp=0°), no things between this. So highly
  correlated signals of L and R can't be used by MP3, MP3 needs highly
  identically signals.
  
  A much better approach would be (but I can't change the MP3 file format,
  who invents time travels ???) to allow a generic rotation of the
  coordinate axis':
  
  n   = 8 bit rotation angle = 0...255
  phi = n/256 * pi/2
  
   / CH1 \     /  cos phi  +sin phi \     / L \
  (       ) = (                      ) * (     )
   \ CH2 /     \ -sin phi   cos phi /     \ R /
  
   LR: n =   0, phi =  0
   MS: n = 128, phi = 45°  
  
   Advantages:
     No LR <=> MS switching artefacts
     Much better compression of unbalanced quasi mono signals 
          (for instance: R = 0.76*L => n = 106 => phi = 0.6504 = 37°)

   / M \     /  0.7958  0.6055 \     /   L    \
  (     ) = (                   ) * (          )
   \ S /     \ -0.6055  0.7958 /     \ 0.76*L /


     Which gives a:

     M =  1.256  L
     S = -0.0007 L
     
     So S is 65 dB below the M signal. Simple MS stereo gives only 17 dB,
     so a lot of bandwidth must be waste for the L-R signal.
  
 */  
  
int is_probably_MS ( const sample_t *L, const sample_t *R, size_t len )
{
  double  x = 0, x2 = 0, y = 0, y2 = 0, xy = 0;
  double  t1;
  double  t2;
  double  r;
  int i = len;

  for ( ; i--; ) {
    x  += (t1 = (double)(*L++));
    x2 += t1*t1;
    y  += (t2 = (double)(*R++));
    y2 += t2*t2;
    xy += t1*t2;
  }
  r = (x2*len - x*x) * (y2*len - y*y);
  r = r > 0 ? (xy*len - x*y) / sqrt (r) :  1.l;

  return r > 0.33;
}
#endif






/************************************************************************
*
* encodeframe()           Layer 3
*
* encode a single frame
*
************************************************************************
lame_encode_frame()


                       gr 0            gr 1
inbuf:           |--------------|---------------|-------------|
MDCT output:  |--------------|---------------|-------------|

FFT's                    <---------1024---------->
                                         <---------1024-------->



    inbuf = buffer of PCM data size=MP3 framesize
    encoder acts on inbuf[ch][0], but output is delayed by MDCTDELAY
    so the MDCT coefficints are from inbuf[ch][-MDCTDELAY]

    psy-model FFT has a 1 granule day, so we feed it data for the next granule.
    FFT is centered over granule:  224+576+224
    So FFT starts at:   576-224-MDCTDELAY

    MPEG2:  FFT ends at:  BLKSIZE+576-224-MDCTDELAY
    MPEG1:  FFT ends at:  BLKSIZE+2*576-224-MDCTDELAY    (1904)

    FFT starts at 576-224-MDCTDELAY (304)  = 576-FFTOFFSET

*/

int lame_encode_mp3_frame(lame_global_flags *gfp,
sample_t inbuf_l[],sample_t inbuf_r[],
char *mp3buf, size_t mp3buf_size)
{
#ifdef macintosh /* PLL 14/04/2000 */
  static FLOAT8 xr[2][2][576];
  static int l3_enc[2][2][576];
#else
  FLOAT8 xr[2][2][576];
  int l3_enc[2][2][576];
#endif
  int mp3count;
  III_psy_ratio masking_ratio[2][2];    /*LR ratios */
  III_psy_ratio masking_MS_ratio[2][2]; /*MS ratios */
  III_psy_ratio (*masking)[2][2];  /*LR ratios and MS ratios*/
  III_scalefac_t scalefac[2][2];
  sample_t *inbuf[2];
  lame_internal_flags *gfc=gfp->internal_flags;


  typedef FLOAT8 pedata[2][2];
  pedata pe,pe_MS;
  pedata *pe_use;

  int ch,gr,mean_bits;
  int bitsPerFrame;

  int check_ms_stereo;
  FLOAT8 ms_ratio_next=0;
  FLOAT8 ms_ratio_prev=0;

  memset((char *) masking_ratio, 0, sizeof(masking_ratio));
  memset((char *) masking_MS_ratio, 0, sizeof(masking_MS_ratio));
  memset((char *) scalefac, 0, sizeof(scalefac));
  inbuf[0]=inbuf_l;
  inbuf[1]=inbuf_r;
    
  gfc->mode_ext = MPG_MD_LR_LR;

  if (gfc->lame_encode_frame_init==0 )  {
    /* padding method as described in 
     * "MPEG-Layer3 / Bitstream Syntax and Decoding"
     * by Martin Sieler, Ralph Sperschneider
     *
     * note: there is no padding for the very first frame
     *
     * Robert.Hegemann@gmx.de 2000-06-22
     */
        
    gfc->frac_SpF = ((gfp->version+1)*72000L*gfp->brate) % gfp->out_samplerate;
    gfc->slot_lag  = gfc->frac_SpF;
    
    gfc->lame_encode_frame_init=1;
    
    /* check FFT will not use a negative starting offset */
    assert(576>=FFTOFFSET);
    /* check if we have enough data for FFT */
    assert(gfc->mf_size>=(BLKSIZE+gfp->framesize-FFTOFFSET));
    /* check if we have enough data for polyphase filterbank */
    /* it needs 1152 samples + 286 samples ignored for one granule */
    /*          1152+576+286 samples for two granules */
    assert(gfc->mf_size>=(286+576*(1+gfc->mode_gr)));

    /* prime the MDCT/polyphase filterbank with a short block */
    { 
      int i,j;
      sample_t primebuff0 [286+1152+576];
      sample_t primebuff1 [286+1152+576];
      for (i=0, j=0; i<286+576*(1+gfc->mode_gr); ++i) {
	if (i<576*gfc->mode_gr) {
	  primebuff0[i]=0;
	  fprintf (stderr, "Check for bug here! %u pfk@uni-jena.de", gfc->channels);
	  if (gfc->channels) 
	    primebuff1[i]=0;
	}else{
	  primebuff0[i]=inbuf[0][j];
	  fprintf (stderr, "Check for bug here! %u pfk@uni-jena.de", gfc->channels);
	  if (gfc->channels) 
	    primebuff1[i]=inbuf[1][j];
	  ++j;
	}
      }
      /* polyphase filtering / mdct */
      for ( gr = 0; gr < gfc->mode_gr; gr++ ) {
	for ( ch = 0; ch < gfc->channels; ch++ ) {
	  gfc->l3_side.gr[gr].ch[ch].tt.block_type=SHORT_TYPE;
	}
      }
      mdct_sub48(gfp,primebuff0, primebuff1, xr, &gfc->l3_side);
    }
  }


  /********************** padding *****************************/
  switch (gfp->padding_type) {
  case 0:
    gfc->padding=0;
    break;
  case 1:
    gfc->padding=1;
    break;
  case 2:
  default:
    if (gfp->VBR!=vbr_off) {
      gfc->padding=0;
    } else {
      if (gfp->disable_reservoir) {
	gfc->padding = 0;
	/* if the user specified --nores, dont very gfc->padding either */
	/* tiny changes in frac_SpF rounding will cause file differences */
      }else{
        /* padding method as described in 
         * "MPEG-Layer3 / Bitstream Syntax and Decoding"
         * by Martin Sieler, Ralph Sperschneider
         *
         * note: there is no padding for the very first frame
         *
         * Robert.Hegemann@gmx.de 2000-06-22
         */

        gfc->slot_lag -= gfc->frac_SpF;
        if (gfc->slot_lag < 0) {
          gfc->slot_lag += gfp->out_samplerate;
          gfc->padding = 1;
        } else {
          gfc->padding = 0;
        }
      } /* reservoir enabled */
    }
  }


  /********************** status display  *****************************/
  if (!gfp->silent) {
  
  
#ifdef __unix  
      long double curr_time;
      struct timeval      t;

      gettimeofday (&t, NULL);
      curr_time = t.tv_sec + 1.e-6l * t.tv_usec;


      /* Update display once per 2 seconds */
      if ( curr_time - gfc->last_time >= gfp->display_update_interval  ||  gfp->frameNum == 0  ||  gfp->frameNum == 9 ) {
	timestatus(gfp->out_samplerate,gfp->frameNum,gfp->totalframes,gfp->framesize);
	
	if (gfp->brhist_disp)
	  brhist_disp(gfp->totalframes);

        gettimeofday (&t, NULL);
        gfc->last_time = t.tv_sec + 1.e-6l * t.tv_usec;
      }
  
#else  
      time_t         curr_time = time (NULL);
      
      /* Update display once per 2 seconds */
      if ( difftime ( curr_time, (time_t)gfc->last_time ) >= gfp->display_update_interval  ||  gfp->frameNum == 0  ||  gfp->frameNum == 9 ) {
	timestatus(gfp->out_samplerate,gfp->frameNum,gfp->totalframes,gfp->framesize);
	
	if (gfp->brhist_disp)
	  brhist_disp(gfp->totalframes);
	gfc->last_time = time (NULL);
      }
#endif
  }

  if (gfc->psymodel) {
    /* psychoacoustic model
     * psy model has a 1 granule (576) delay that we must compensate for
     * (mt 6/99).
     */
    int ret;
    sample_t *bufp[2];  /* address of beginning of left & right granule */
    int blocktype[2];

    ms_ratio_prev=gfc->ms_ratio[gfc->mode_gr-1];
    for (gr=0; gr < gfc->mode_gr ; gr++) {

      for ( ch = 0; ch < gfc->channels; ch++ )
	bufp[ch] = &inbuf[ch][576 + gr*576-FFTOFFSET];

      ret=L3psycho_anal( gfp,bufp, gr, 
		     &gfc->ms_ratio[gr],&ms_ratio_next,&gfc->ms_ener_ratio[gr],
		     masking_ratio, masking_MS_ratio,
		     pe[gr],pe_MS[gr],blocktype);
      if (ret!=0) return -4;

      for ( ch = 0; ch < gfc->channels; ch++ )
	gfc->l3_side.gr[gr].ch[ch].tt.block_type=blocktype[ch];

    }
    /* at LSF there is no 2nd granule */
    gfc->ms_ratio[1]=gfc->ms_ratio[gfc->mode_gr-1];
  }else{
    for (gr=0; gr < gfc->mode_gr ; gr++)
      for ( ch = 0; ch < gfc->channels; ch++ ) {
	gfc->l3_side.gr[gr].ch[ch].tt.block_type=NORM_TYPE;
	pe_MS[gr][ch]=pe[gr][ch]=700;
      }
  }


  /* block type flags */
  for( gr = 0; gr < gfc->mode_gr; gr++ ) {
    for ( ch = 0; ch < gfc->channels; ch++ ) {
      gr_info *cod_info = &gfc->l3_side.gr[gr].ch[ch].tt;
      cod_info->mixed_block_flag = 0;     /* never used by this model */
      if (cod_info->block_type == NORM_TYPE )
	cod_info->window_switching_flag = 0;
      else
	cod_info->window_switching_flag = 1;
    }
  }


  /* polyphase filtering / mdct */
  mdct_sub48(gfp,inbuf[0], inbuf[1], xr, &gfc->l3_side);
  /* re-order the short blocks, for more efficient encoding below */
  for (gr = 0; gr < gfc->mode_gr; gr++) {
    for (ch = 0; ch < gfc->channels; ch++) {
      gr_info *cod_info = &gfc->l3_side.gr[gr].ch[ch].tt;
      if (cod_info->block_type==SHORT_TYPE) {
	freorder(gfc->scalefac_band.s,xr[gr][ch]);
      }
    }
  }
  



  /* use m/s gfc->stereo? */
  check_ms_stereo =  (gfp->mode == MPG_MD_JOINT_STEREO);
  if (check_ms_stereo) {
    int gr0 = 0, gr1 = gfc->mode_gr-1;
    /* make sure block type is the same in each channel */
    check_ms_stereo =
      (gfc->l3_side.gr[gr0].ch[0].tt.block_type==gfc->l3_side.gr[gr0].ch[1].tt.block_type) &&
      (gfc->l3_side.gr[gr1].ch[0].tt.block_type==gfc->l3_side.gr[gr1].ch[1].tt.block_type);
  }
  if (gfp->force_ms) 
    gfc->mode_ext = MPG_MD_MS_LR;
  else if (check_ms_stereo) {
    /* ms_ratio = is like the ratio of side_energy/total_energy */
    FLOAT8 ms_ratio_ave;
    /*     ms_ratio_ave = .5*(ms_ratio[0] + ms_ratio[1]);*/

    ms_ratio_ave = .25*(gfc->ms_ratio[0] + gfc->ms_ratio[1]+
			 ms_ratio_prev + ms_ratio_next);
    if ( (ms_ratio_ave <.35) && (.5*(gfc->ms_ratio[0]+gfc->ms_ratio[1])<.45) )
#ifdef RH_VALIDATE_MS
      if (       (pe_MS[0][0]+pe_MS[0][1]+pe_MS[1][0]+pe_MS[1][1])
          < 1.07*(pe   [0][0]+pe   [0][1]+pe   [1][0]+pe   [1][1])
        && is_probably_MS(inbuf_l, inbuf_r, gfp->framesize)       )
#endif
         gfc->mode_ext = MPG_MD_MS_LR;
  }



  if (gfp->gtkflag && gfc->pinfo != NULL) {
    for ( gr = 0; gr < gfc->mode_gr; gr++ ) {
      for ( ch = 0; ch < gfc->channels; ch++ ) {
	gfc->pinfo->ms_ratio[gr]=gfc->ms_ratio[gr];
	gfc->pinfo->ms_ener_ratio[gr]=gfc->ms_ener_ratio[gr];
	gfc->pinfo->blocktype[gr][ch]=
	  gfc->l3_side.gr[gr].ch[ch].tt.block_type;
	memcpy(gfc->pinfo->xr[gr][ch],xr[gr][ch],sizeof(xr[gr][ch]));
	/* if MS stereo, switch to MS psy data */
	if (gfc->mode_ext==MPG_MD_MS_LR) {
	  gfc->pinfo->pe[gr][ch]=gfc->pinfo->pe[gr][ch+2];
	  gfc->pinfo->ers[gr][ch]=gfc->pinfo->ers[gr][ch+2];
	  memcpy(gfc->pinfo->energy[gr][ch],gfc->pinfo->energy[gr][ch+2],
		 sizeof(gfc->pinfo->energy[gr][ch]));
	}
      }
    }
  }




  /* bit and noise allocation */
  if (MPG_MD_MS_LR == gfc->mode_ext) {
    masking = &masking_MS_ratio;    /* use MS masking */
    pe_use = &pe_MS;
  } else {
    masking = &masking_ratio;    /* use LR masking */
    pe_use = &pe;
  }


  switch (gfp->VBR){ 
  default:
  case vbr_off:
    iteration_loop( gfp,*pe_use, gfc->ms_ener_ratio, xr, *masking, l3_enc, scalefac);
    break;
  case vbr_mt:
    VBR_quantize( gfp,*pe_use, gfc->ms_ener_ratio, xr, *masking, l3_enc, scalefac);
    break;
  case vbr_rh:
    VBR_iteration_loop( gfp,*pe_use, gfc->ms_ener_ratio, xr, *masking, l3_enc, scalefac);
    break;
  case vbr_abr:
    ABR_iteration_loop( gfp,*pe_use, gfc->ms_ener_ratio, xr, *masking, l3_enc, scalefac);
    break;
  }

  /* update VBR histogram data */
  brhist_add_count(gfc->bitrate_index);

  /*  write the frame to the bitstream  */
  getframebits(gfp,&bitsPerFrame,&mean_bits);

  format_bitstream( gfp, bitsPerFrame, l3_enc, scalefac);

  /* copy mp3 bit buffer into array */
  mp3count = copy_buffer(mp3buf,mp3buf_size,&gfc->bs);

  if (gfp->bWriteVbrTag) AddVbrFrame(gfp);


  if (gfp->gtkflag && gfc->pinfo != NULL) {
    int j;
    for ( ch = 0; ch < gfc->channels; ch++ ) {
      for ( j = 0; j < FFTOFFSET; j++ )
	gfc->pinfo->pcmdata[ch][j] = gfc->pinfo->pcmdata[ch][j+gfp->framesize];
      for ( j = FFTOFFSET; j < 1600; j++ ) {
	gfc->pinfo->pcmdata[ch][j] = inbuf[ch][j-FFTOFFSET];
      }
    }
  }

  gfp->frameNum++;
  return mp3count;
}


/* routine to feed exactly one frame (gfp->framesize) worth of data to the 
encoding engine.  All buffering, resampling, etc, handled by calling
program.  
*/
int lame_encode_frame(lame_global_flags *gfp,
sample_t inbuf_l[],sample_t inbuf_r[],
char *mp3buf, size_t mp3buf_size)
{
  if (gfp->ogg) {
#ifdef HAVEVORBIS
    return lame_encode_ogg_frame(gfp,inbuf_l,inbuf_r,mp3buf,mp3buf_size);
#else
    return -5; /* wanna encode ogg without vorbis */
#endif
  } else {
    return lame_encode_mp3_frame(gfp,inbuf_l,inbuf_r,mp3buf,mp3buf_size);
  }
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
*/
int lame_encode_buffer(lame_global_flags *gfp,
   sample_t* buffer_l, sample_t* buffer_r,size_t nsamples,
   char *mp3buf, size_t mp3buf_size)
{
  int mp3size = 0, ret, i, ch, mf_needed;
  lame_internal_flags *gfc=gfp->internal_flags;
  sample_t *mfbuf[2];
  sample_t *in_buffer[2];
  
  assert (nsamples > 0);
  
  in_buffer[0] = buffer_l;
  in_buffer[1] = buffer_r;

  if (!gfc->lame_init_params_init) return -3;

  /* some sanity checks */
  assert(ENCDELAY>=MDCTDELAY);
  assert(BLKSIZE-FFTOFFSET >= 0);
  mf_needed = BLKSIZE+gfp->framesize-FFTOFFSET;  /* amount needed for FFT */
  mf_needed = Max(mf_needed,286+576*(1+gfc->mode_gr)); /* amount needed for MDCT/filterbank */
  assert(MFSIZE>=mf_needed);

  mfbuf[0]=gfc->mfbuf[0];
  mfbuf[1]=gfc->mfbuf[1];

  if (gfp->num_channels==2  && gfc->channels==1) {
    /* downsample to mono */
    for (i=0; i<nsamples; ++i) {
      in_buffer[0][i]=((int)in_buffer[0][i]+(int)in_buffer[1][i])/2;
      in_buffer[1][i]=0;
    }
  }


  while (nsamples > 0) {
    int n_in=0;
    int n_out=0;
    /* copy in new samples into mfbuf, with filtering */

    for (ch=0; ch<gfc->channels; ch++) {
      if (gfc->resample_ratio>1.0)  {
	n_out=fill_buffer_downsample(gfp,&mfbuf[ch][gfc->mf_size],gfp->framesize,
					  in_buffer[ch],nsamples,&n_in,ch);
      } else if (gfc->resample_ratio<1.0) {
	n_out=fill_buffer_upsample(gfp,&mfbuf[ch][gfc->mf_size],gfp->framesize,
					  in_buffer[ch],nsamples,&n_in,ch);
      } else {
	n_out=Min(gfp->framesize,nsamples);
	n_in = n_out;
	memcpy( (char *) &mfbuf[ch][gfc->mf_size],(char *)in_buffer[ch],sizeof(sample_t)*n_out);
      }
      in_buffer[ch] += n_in;
    }

    nsamples -= n_in;
    gfc->mf_size += n_out;
    assert(gfc->mf_size<=MFSIZE);
    gfc->mf_samples_to_encode += n_out;


    if (gfc->mf_size >= mf_needed) {
      /* encode the frame.  */
      ret = lame_encode_frame(gfp,mfbuf[0],mfbuf[1],mp3buf,mp3buf_size);

      if (ret < 0) return ret;
      mp3buf += ret;
      mp3size += ret;

      /* shift out old samples */
      gfc->mf_size -= gfp->framesize;
      gfc->mf_samples_to_encode -= gfp->framesize;
      for (ch=0; ch<gfc->channels; ch++)
	for (i=0; i<gfc->mf_size; i++)
	  mfbuf[ch][i]=mfbuf[ch][i+gfp->framesize];
    }
  }
  assert(nsamples==0);

  return mp3size;
}




int   lame_encode_buffer_interleaved (
		lame_global_flags* gfp,
   		stereo_t           buffer [], 
   		size_t             nsamples, 
   		char               mp3buf [], 
   		size_t             mp3buf_size )
{
    int        mp3size = 0;
    int        ret;
    int        i; 
    int        ch;
    int        mf_needed;
    sample_t*  mfbuf [2];
    lame_internal_flags* gfc = gfp -> internal_flags;

  assert (nsamples > 0);

  if (!gfc->lame_init_params_init) return -3;

  mfbuf[0]=gfc->mfbuf[0];
  mfbuf[1]=gfc->mfbuf[1];

  /* some sanity checks */
  assert(ENCDELAY>=MDCTDELAY);
  assert(BLKSIZE-FFTOFFSET >= 0);
  mf_needed = BLKSIZE+gfp->framesize-FFTOFFSET;
  assert(MFSIZE>=mf_needed);

  if (gfp->num_channels == 1) {
    return lame_encode_buffer (gfp,(sample_t*)buffer, NULL ,nsamples,mp3buf,mp3buf_size);
  }

  if (gfc->resample_ratio != 1.0)  {
    sample_t *buffer_l;
    sample_t *buffer_r;
    buffer_l=malloc(sizeof(sample_t)*nsamples);
    buffer_r=malloc(sizeof(sample_t)*nsamples);
    if (buffer_l == NULL || buffer_r == NULL) {
      return -2;
    }
    for (i=0; i<nsamples; i++) {
      buffer_l [i] = buffer [i] [0];
      buffer_r [i] = buffer [i] [1];
    }
    ret = lame_encode_buffer(gfp,buffer_l,buffer_r,nsamples,mp3buf,mp3buf_size);
    free(buffer_l);
    free(buffer_r);
    return ret;
  }


  if (gfp->num_channels==2  && gfc->channels==1) {
    /* downsample to mono */
    for (i=0; i<nsamples; ++i) {
      buffer[i][0]=((int)buffer[i][0]+(int)buffer[i][1])/2;
      buffer[i][1]=0;
    }
  }


  while (nsamples > 0) {
    int n_out;
    /* copy in new samples */
    n_out = Min(gfp->framesize,nsamples);
    for (i=0; i<n_out; ++i) {
      mfbuf[0][gfc->mf_size+i]=buffer[i][0];
      mfbuf[1][gfc->mf_size+i]=buffer[i][1];
    }
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
      for (ch=0; ch<gfc->channels; ch++)
	for (i=0; i<gfc->mf_size; i++)
	  mfbuf[ch][i]=mfbuf[ch][i+gfp->framesize];
    }
  }
  assert(nsamples==0);
  return mp3size;
}




/* old LAME interface.  use lame_encode_buffer instead */
int lame_encode(lame_global_flags *gfp, sample_t in_buffer[2][1152],char *mp3buf,size_t size){
  int imp3;
  lame_internal_flags *gfc=gfp->internal_flags;
  if (!gfc->lame_init_params_init) return -3;
  imp3= lame_encode_buffer(gfp,in_buffer[0],in_buffer[1],gfp->framesize,mp3buf,size);
  return imp3;
}


/*****************************************************************/
/* flush internal mp3 buffers,                                   */
/*****************************************************************/
int lame_encode_finish(lame_global_flags *gfp,char *mp3buffer, size_t mp3buffer_size)
{
  int imp3=0,mp3count,mp3buffer_size_remaining;
  sample_t buffer[2][1152];
  lame_internal_flags *gfc=gfp->internal_flags;

  memset((char *)buffer,0,sizeof(buffer));
  mp3count = 0;

  while (gfc->mf_samples_to_encode > 0) {

    mp3buffer_size_remaining = mp3buffer_size - mp3count;

    /* if user specifed buffer size = 0, dont check size */
    if (mp3buffer_size == 0) mp3buffer_size_remaining=0;  

    /* send in a frame of 0 padding until all internal sample buffers flushed */
    imp3=lame_encode_buffer(gfp,buffer[0],buffer[1],gfp->framesize,mp3buffer,mp3buffer_size_remaining);
    /* dont count the above padding: */
    gfc->mf_samples_to_encode -= gfp->framesize;

    if (imp3 < 0) {
      /* some type of fatel error */
      freegfc(gfc);    
      return imp3;
    }
    mp3buffer += imp3;
    mp3count += imp3;
  }


  gfp->frameNum--;
  if (!gfp->silent) {
      timestatus(gfp->out_samplerate,gfp->frameNum,gfp->totalframes,gfp->framesize);

      if (gfp->brhist_disp)
	{
	  brhist_disp(gfp->totalframes);
	  brhist_disp_total(gfp);
	}

      timestatus_finish();
  }

  mp3buffer_size_remaining = mp3buffer_size - mp3count;
  /* if user specifed buffer size = 0, dont check size */
  if (mp3buffer_size == 0) mp3buffer_size_remaining=0;  

  if (gfp->ogg) {
#ifdef HAVEVORBIS    
    /* ogg related stuff */
    imp3 = lame_encode_ogg_finish(gfp,mp3buffer,mp3buffer_size_remaining);
#endif
  }else{
    /* mp3 related stuff.  bit buffer might still contain some data */
    flush_bitstream(gfp);
    imp3= copy_buffer(mp3buffer,mp3buffer_size_remaining,&gfc->bs);
  }
  if (imp3 < 0) {
    freegfc(gfc);    
    return imp3;
  }
  mp3count += imp3;

  freegfc(gfc);    
  return mp3count;
}



/*****************************************************************/
/* write VBR Xing header, and ID3 version 1 tag, if asked for    */
/*****************************************************************/
void lame_mp3_tags_fid(lame_global_flags *gfp,FILE *fpStream)
{
  if (gfp->bWriteVbrTag)
    {
      /* Calculate relative quality of VBR stream
       * 0=best, 100=worst */
      int nQuality=gfp->VBR_q*100/9;

      /* Write Xing header again */
      if (fpStream && !fseek(fpStream, 0, SEEK_SET)) 
	PutVbrTag(gfp,fpStream,nQuality);
    }

  /* write an ID3 version 1 tag  */
  if(gfp->id3v1_enabled
    /* no ID3 version 1 tags in Ogg Vorbis output */
    && !gfp->ogg
    ) {
    /*
     * NOTE: The new tagging API only knows about streams and always writes at
     * the current position, so we have to open the file and seek to the end of
     * it here.  Perhaps we should just NOT close the file when the bitstream is
     * completed, nor when the final VBR tag is written.
     * -- gramps
     *
     * ideally this should not be done here: lame_encode_finish should 
     * write the tag directly into the mp3 output buffer.  
     */
    if (fpStream && !fseek(fpStream, 0, SEEK_END)) {
      id3tag_write_v1(&gfp->tag_spec, fpStream);
    }
  }

}

void lame_mp3_tags(lame_global_flags *gfp)
{
  FILE *fpStream;

  /* Open the bitstream again */
  fpStream=fopen(gfp->outPath,"rb+");
  /* Assert stream is valid */
  if (fpStream==NULL)
    return;
  lame_mp3_tags_fid(gfp,fpStream);
  fclose(fpStream);
}




void lame_version(lame_global_flags *gfp,char *ostring) {
  strncpy(ostring,get_lame_version(),20);
}







/* initialize mp3 encoder */
int lame_init(lame_global_flags *gfp)
{
  lame_internal_flags *gfc;

  /*
   *  Disable floating point exepctions
   */
#ifdef __FreeBSD__
# include <floatingpoint.h>
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
# elif (defined(__linux__) || defined(__FreeBSD__))
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

  /* Global flags.  set defaults here */
  gfp->mode = MPG_MD_JOINT_STEREO;
  gfp->mode_fixed=0;
  gfp->force_ms=0;
  gfp->brate=0;
  gfp->copyright=0;
  gfp->original=1;
  gfp->extension=0;
  gfp->error_protection=0;
  gfp->emphasis=0;
  gfp->in_samplerate=1000*44.1;
  gfp->out_samplerate=0;
  gfp->num_channels=2;
  gfp->num_samples=MAX_U_32_NUM;

  gfp->allow_diff_short=0;
  gfp->ATHonly=0;
  gfp->noATH=0;
  gfp->bWriteVbrTag=1;
  gfp->cwlimit=0;
  gfp->disable_reservoir=0;
  gfp->experimentalX = 0;
  gfp->experimentalY = 0;
  gfp->experimentalZ = 0;
  gfp->exp_nspsytune = 0;
  gfp->gtkflag=0;
  gfp->quality=5;
  gfp->input_format=sf_unknown;

  gfp->lowpassfreq=0;
  gfp->highpassfreq=0;
  gfp->lowpasswidth = -1;
  gfp->highpasswidth = -1;

  gfp->no_short_blocks=0;
  gfp->padding_type=2;
  gfp->swapbytes=0;
  gfp->silent=1;
  gfp->VBR=vbr_off;
  gfp->VBR_q=4;
  gfp->VBR_mean_bitrate_kbps=128;
  gfp->VBR_min_bitrate_kbps=0;
  gfp->VBR_max_bitrate_kbps=0;
  gfp->VBR_hard_min=0;

  gfc->pcmbitwidth = 16;
  gfc->resample_ratio=1.0;
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

  memset((char *) gfc->mfbuf, 0, sizeof(sample_t)*2*MFSIZE);

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

  return 0;
}


