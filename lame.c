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


#include "gtkanal.h"
#include "lame.h"
#include "util.h"
#include "timestatus.h"
#include "psymodel.h"
#include "newmdct.h"
#include "quantize.h"
#include "quantize-pvt.h"
#include "l3bitstream.h"
#include "formatBitstream.h"
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




/********************************************************************
 *   initialize internal params based on data in gf
 *   (globalflags struct filled in by calling program)
 *
 ********************************************************************/
void lame_init_params(lame_global_flags *gfp)
{
  int i;
  FLOAT compression_ratio;
  lame_internal_flags *gfc=gfp->internal_flags;

  gfc->lame_init_params_init=1;

  memset(&gfc->bs, 0, sizeof(Bit_stream_struc));
  memset(&gfc->l3_side,0x00,sizeof(III_side_info_t));

  memset((char *) gfc->mfbuf, 0, sizeof(short)*2*MFSIZE);

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


  gfc->frameNum=0;
  InitFormatBitStream();
  if (gfp->num_channels==1) {
    gfp->mode = MPG_MD_MONO;
  }
  gfc->stereo=2;
  if (gfp->mode == MPG_MD_MONO) gfc->stereo=1;

#ifdef BRHIST
  if (gfp->silent) {
    disp_brhist=0;  /* turn of VBR historgram */
  }
  if (!gfp->VBR) {
    disp_brhist=0;  /* turn of VBR historgram */
  }
#endif


  /* set the output sampling rate, and resample options if necessary
     samplerate = input sample rate
     resamplerate = ouput sample rate
  */
  if (gfp->out_samplerate==0) {
    /* user did not specify output sample rate */
    gfp->out_samplerate=gfp->in_samplerate;   /* default */


    /* if resamplerate is not valid, find a valid value */
    if (gfp->out_samplerate>=48000) gfp->out_samplerate=48000;
    else if (gfp->out_samplerate>=44100) gfp->out_samplerate=44100;
    else if (gfp->out_samplerate>=32000) gfp->out_samplerate=32000;
    else if (gfp->out_samplerate>=24000) gfp->out_samplerate=24000;
    else if (gfp->out_samplerate>=22050) gfp->out_samplerate=22050;
    else gfp->out_samplerate=16000;


    if (gfp->brate>0) {
      /* check if user specified bitrate requires downsampling */
      compression_ratio = gfp->out_samplerate*16*gfc->stereo/(1000.0*gfp->brate);
      if (!gfp->VBR && compression_ratio > 13 ) {
	/* automatic downsample, if possible */
	gfp->out_samplerate = (10*1000.0*gfp->brate)/(16*gfc->stereo);
	if (gfp->out_samplerate<=16000) gfp->out_samplerate=16000;
	else if (gfp->out_samplerate<=22050) gfp->out_samplerate=22050;
	else if (gfp->out_samplerate<=24000) gfp->out_samplerate=24000;
	else if (gfp->out_samplerate<=32000) gfp->out_samplerate=32000;
	else if (gfp->out_samplerate<=44100) gfp->out_samplerate=44100;
	else gfp->out_samplerate=48000;
      }
    }
  }

  gfc->mode_gr = (gfp->out_samplerate <= 24000) ? 1 : 2;  /* mode_gr = 2 */
  gfc->encoder_delay = ENCDELAY;
  gfc->framesize = gfc->mode_gr*576;

  if (gfp->brate==0) { /* user didn't specify a bitrate, use default */
    gfp->brate=128;
    if (gfc->mode_gr==1) gfp->brate=64;
  }


  gfc->resample_ratio=1;
  if (gfp->out_samplerate != gfp->in_samplerate) 
        gfc->resample_ratio = (FLOAT)gfp->in_samplerate/(FLOAT)gfp->out_samplerate;

  /* estimate total frames.  must be done after setting sampling rate so
   * we know the framesize.  */
  gfc->totalframes=0;
  gfc->totalframes = 2+ gfp->num_samples/(gfc->resample_ratio*gfc->framesize);



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
  if (gfp->brate >= 320) gfp->VBR=0;  /* dont bother with VBR at 320kbs */
  compression_ratio = gfp->out_samplerate*16*gfc->stereo/(1000.0*gfp->brate);


  /* for VBR, take a guess at the compression_ratio. for example: */
  /* VBR_q           compression       like
     0                4.4             320kbs/41khz
     1                5.4             256kbs/41khz
     3                7.4             192kbs/41khz
     4                8.8             160kbs/41khz
     6                10.4            128kbs/41khz
  */
  if (gfp->VBR) {
    compression_ratio = 4.4 + gfp->VBR_q;
  }


  /* At higher quality (lower compression) use STEREO instead of JSTEREO.
   * (unless the user explicitly specified a mode ) */
  if ( (!gfp->mode_fixed) && (gfp->mode !=MPG_MD_MONO)) {
    if (compression_ratio < 9 ) {
      gfp->mode = MPG_MD_STEREO;
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
    int band = 1+floor(.5 + 14-18*log(compression_ratio/16.0));
    if (gfc->resample_ratio != 1) {
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
  /* compute info needed for polyphase filter                    */
  /***************************************************************/
  if (gfc->filter_type==0) {
    int band,maxband,minband;
    FLOAT8 amp,freq;
    if (gfc->lowpass1 > 0) {
      minband=999;
      maxband=-1;
      for (band=0;  band <=31 ; ++band) { 
	freq = band/31.0;
	amp = 1;
	/* this band and above will be zeroed: */
	if (freq >= gfc->lowpass2) {
	  gfc->lowpass_band= Min(gfc->lowpass_band,band);
	  amp=0;
	}
	if (gfc->lowpass1 < freq && freq < gfc->lowpass2) {
          minband = Min(minband,band);
          maxband = Max(maxband,band);
	  amp = cos((PI/2)*(gfc->lowpass1-freq)/(gfc->lowpass2-gfc->lowpass1));
	}
	/* printf("lowpass band=%i  amp=%f \n",band,amp);*/
      }
      /* compute the *actual* transition band implemented by the polyphase filter */
      if (minband==999) gfc->lowpass1 = (gfc->lowpass_band-.75)/31.0;
      else gfc->lowpass1 = (minband-.75)/31.0;
      gfc->lowpass2 = gfc->lowpass_band/31.0;
    }

    /* make sure highpass filter is within 90% of whan the effective highpass
     * frequency will be */
    if (gfc->highpass2 > 0) 
      if (gfc->highpass2 <  .9*(.75/31.0) ) {
	gfc->highpass1=0; gfc->highpass2=0;
	fprintf(stderr,"Warning: highpass filter disabled.  highpass frequency to small\n");
      }
    

    if (gfc->highpass2 > 0) {
      minband=999;
      maxband=-1;
      for (band=0;  band <=31; ++band) { 
	freq = band/31.0;
	amp = 1;
	/* this band and below will be zereod */
	if (freq <= gfc->highpass1) {
	  gfc->highpass_band = Max(gfc->highpass_band,band);
	  amp=0;
	}
	if (gfc->highpass1 < freq && freq < gfc->highpass2) {
          minband = Min(minband,band);
          maxband = Max(maxband,band);
	  amp = cos((PI/2)*(gfc->highpass2-freq)/(gfc->highpass2-gfc->highpass1));
	}
	/*	printf("highpass band=%i  amp=%f \n",band,amp);*/
      }
      /* compute the *actual* transition band implemented by the polyphase filter */
      gfc->highpass1 = gfc->highpass_band/31.0;
      if (maxband==-1) gfc->highpass2 = (gfc->highpass_band+.75)/31.0;
      else gfc->highpass2 = (maxband+.75)/31.0;
    }
    /*
    printf("lowpass band with amp=0:  %i \n",gfc->lowpass_band);
    printf("highpass band with amp=0:  %i \n",gfc->highpass_band);
    */
  }



  /***************************************************************/
  /* compute info needed for FIR filter */
  /***************************************************************/
  if (gfc->filter_type==1) {
  }




  gfc->mode_ext=MPG_MD_LR_LR;
  gfc->stereo = (gfp->mode == MPG_MD_MONO) ? 1 : 2;


  gfc->samplerate_index = SmpFrqIndex((long)gfp->out_samplerate, &gfp->version);
  if( gfc->samplerate_index < 0) {
    display_bitrates(stderr);
    exit(1);
  }
  if( (gfc->bitrate_index = BitrateIndex(gfp->brate, gfp->version,gfp->out_samplerate)) < 0) {
    display_bitrates(stderr);
    exit(1);
  }


  /* choose a min/max bitrate for VBR */
  if (gfp->VBR) {
    /* if the user didn't specify VBR_max_bitrate: */
    if (0==gfp->VBR_max_bitrate_kbps) {
      /* default max bitrate is 256kbs */
      /* we do not normally allow 320bps frams with VBR, unless: */
      gfc->VBR_max_bitrate=13;   /* default: allow 256kbs */
      if (gfp->VBR_min_bitrate_kbps>=256) gfc->VBR_max_bitrate=14;
      if (gfp->VBR_q == 0) gfc->VBR_max_bitrate=14;   /* allow 320kbs */
#if 0
      if (gfp->VBR_q >= 4) gfc->VBR_max_bitrate=12;   /* max = 224kbs */
      if (gfp->VBR_q >= 8) gfc->VBR_max_bitrate=9;    /* low quality, max = 128kbs */
#endif
    }else{
      if( (gfc->VBR_max_bitrate  = BitrateIndex(gfp->VBR_max_bitrate_kbps, gfp->version,gfp->out_samplerate)) < 0) {
	display_bitrates(stderr);
	exit(1);
      }
    }
    if (0==gfp->VBR_min_bitrate_kbps) {
      gfc->VBR_min_bitrate=1;  /* 32 kbps */
    }else{
      if( (gfc->VBR_min_bitrate  = BitrateIndex(gfp->VBR_min_bitrate_kbps, gfp->version,gfp->out_samplerate)) < 0) {
	display_bitrates(stderr);
	exit(1);
      }
    }

  }

  /* VBR needs at least the output of GPSYCHO,
   * so we have to garantee that by setting a minimum 
   * quality level, actually level 7 does it.
   * the -v and -V x settings switch the quality to level 2
   * you would have to add a -f or -q 5 to reduce the quality
   * down to level 7 or 5
   */
  if (gfp->VBR) gfp->quality=Min(gfp->quality,7);
  /* dont allow forced mid/side stereo for mono output */
  if (gfp->mode == MPG_MD_MONO) gfp->force_ms=0;


  /* Do not write VBR tag if VBR flag is not specified */
  if (gfp->VBR==0) gfp->bWriteVbrTag=0;
  if (gfp->gtkflag) gfp->bWriteVbrTag=0;

  /* some file options not allowed if output is: not specified or stdout */

  if (gfp->outPath!=NULL && gfp->outPath[0]=='-' ) {
    gfp->bWriteVbrTag=0; /* turn off VBR tag */
  }

  if (gfp->outPath==NULL || gfp->outPath[0]=='-' ) {
    gfp->id3tag_used=0;         /* turn of id3 tagging */
  }



  if (gfc->pinfo != NULL) {
    gfp->bWriteVbrTag=0;  /* disable Xing VBR tag */
  }

#define NEWFMT
#ifdef NEWFMT  
  gfc->bs.bstype=1;
#else
  gfc->bs.bstype=0;
#endif
  init_bit_stream_w(gfc);




  /* set internal feature flags.  USER should not access these since
   * some combinations will produce strange results */

  /* no psymodel, no noise shaping */
  if (gfp->quality==9) {
    gfc->filter_type=0;
    gfc->psymodel=0;
    gfc->quantization=0;
    gfc->noise_shaping=0;
    gfc->noise_shaping_stop=0;
    gfc->use_best_huffman=0;
  }

  if (gfp->quality==8) gfp->quality=7;

  /* use psymodel (for short block and m/s switching), but no noise shapping */
  if (gfp->quality==7) {
    gfc->filter_type=0;
    gfc->psymodel=1;
    gfc->quantization=0;
    gfc->noise_shaping=0;
    gfc->noise_shaping_stop=0;
    gfc->use_best_huffman=0;
  }

  if (gfp->quality==6) gfp->quality=5;

  if (gfp->quality==5) {
    /* the default */
    gfc->filter_type=0;
    gfc->psymodel=1;
    gfc->quantization=0;
    gfc->noise_shaping=1;
    gfc->noise_shaping_stop=0;
    gfc->use_best_huffman=0;
  }

  if (gfp->quality==4) gfp->quality=2;
  if (gfp->quality==3) gfp->quality=2;

  if (gfp->quality==2) {
    gfc->filter_type=0;
    gfc->psymodel=1;
    gfc->quantization=1;
    gfc->noise_shaping=1;
    gfc->noise_shaping_stop=0;
    gfc->use_best_huffman=1;
  }

  if (gfp->quality==1) {
    gfc->filter_type=0;
    gfc->psymodel=1;
    gfc->quantization=1;
    gfc->noise_shaping=1;
    gfc->noise_shaping_stop=1;
    gfc->use_best_huffman=1;
  }

  if (gfp->quality==0) {
    /* 0..1 quality */
    gfc->filter_type=1;         /* not yet coded */
    gfc->psymodel=1;
    gfc->quantization=1;
    gfc->noise_shaping=3;       /* not yet coded */
    gfc->noise_shaping_stop=2;  /* not yet coded */
    gfc->use_best_huffman=2;   /* not yet coded */
    exit(-99);
  }


  for (i = 0; i < SBMAX_l + 1; i++) {
    gfc->scalefac_band.l[i] =
      sfBandIndex[gfc->samplerate_index + (gfp->version * 3) + 
             6*(gfp->out_samplerate<16000)].l[i];
  }
  for (i = 0; i < SBMAX_s + 1; i++) {
    gfc->scalefac_band.s[i] =
      sfBandIndex[gfc->samplerate_index + (gfp->version * 3) + 
             6*(gfp->out_samplerate<16000)].s[i];
  }


  /* determine the mean bitrate for main data */
  gfc->sideinfo_len = 4;
  if ( gfp->version == 1 )
    {   /* MPEG 1 */
      if ( gfc->stereo == 1 )
	gfc->sideinfo_len += 17;
      else
	gfc->sideinfo_len += 32;
    }
  else
    {   /* MPEG 2 */
      if ( gfc->stereo == 1 )
	gfc->sideinfo_len += 9;
      else
	gfc->sideinfo_len += 17;
    }
  
  if (gfp->error_protection) gfc->sideinfo_len += 2;
  

  if (gfp->bWriteVbrTag)
    {
      /* Write initial VBR Header to bitstream */
      InitVbrTag(gfp);
    }


#ifdef BRHIST
  if (gfp->VBR) {
    if (disp_brhist)
      brhist_init(gfp,1, 14);
  } else
    disp_brhist = 0;
#endif
  return;
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
  FLOAT compression=
    (FLOAT)(gfc->stereo*16*out_samplerate)/(FLOAT)(gfp->brate);

  lame_print_version(stderr);
  if (gfp->num_channels==2 && gfc->stereo==1) {
    fprintf(stderr, "Autoconverting from stereo to mono. Setting encoding to mono mode.\n");
  }
  if (gfc->resample_ratio!=1) {
    fprintf(stderr,"Resampling:  input=%.1fkHz  output=%.1fkHz\n",
	    in_samplerate,out_samplerate);
  }
  if (gfc->highpass2>0.0)
    fprintf(stderr, "Using polyphase highpass filter, transition band: %.0f Hz -  %.0f Hz\n",
	    gfc->highpass1*out_samplerate*500,
	    gfc->highpass2*out_samplerate*500);
  if (gfc->lowpass1>0.0)
    fprintf(stderr, "Using polyphase lowpass filter,  transition band:  %.0f Hz - %.0f Hz\n",
	    gfc->lowpass1*out_samplerate*500,
	    gfc->lowpass2*out_samplerate*500);

  if (gfc->pinfo != NULL) {
    fprintf(stderr, "Analyzing %s \n",gfp->inPath);
  }
  else {
    fprintf(stderr, "Encoding %s to %s\n",
	    (strcmp(gfp->inPath, "-")? gfp->inPath : "stdin"),
	    (strcmp(gfp->outPath, "-")? gfp->outPath : "stdout"));
    if (gfp->VBR)
      fprintf(stderr, "Encoding as %.1fkHz VBR(q=%i) %s MPEG%i LayerIII  qval=%i\n",
	      gfp->out_samplerate/1000.0,
	      gfp->VBR_q,mode_names[gfp->mode],2-gfp->version,gfp->quality);
    else
      fprintf(stderr, "Encoding as %.1f kHz %d kbps %s MPEG%i LayerIII (%4.1fx)  qval=%i\n",
	      gfp->out_samplerate/1000.0,gfp->brate,
	      mode_names[gfp->mode],2-gfp->version,compression,gfp->quality);
  }
  fflush(stderr);
}












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
int lame_encode_frame(lame_global_flags *gfp,
short int inbuf_l[],short int inbuf_r[],
char *mp3buf, int mp3buf_size)
{
  FLOAT8 xr[2][2][576];
  int l3_enc[2][2][576];
  int mp3count;
  III_psy_ratio masking_ratio[2][2];    /*LR ratios */
  III_psy_ratio masking_MS_ratio[2][2]; /*MS ratios */
  III_psy_ratio (*masking)[2][2];  /*LR ratios and MS ratios*/
  III_scalefac_t scalefac[2][2];
  short int *inbuf[2];
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
    /* Figure average number of 'slots' per frame. */
    FLOAT8 avg_slots_per_frame;
    FLOAT8 sampfreq =   gfp->out_samplerate/1000.0;
    int bit_rate = gfp->brate;
    gfc->lame_encode_frame_init=1;

    avg_slots_per_frame = (bit_rate*gfc->framesize) /
           (sampfreq* 8);
    /* -f fast-math option causes some strange rounding here, be carefull: */
    gfc->frac_SpF  = avg_slots_per_frame - floor(avg_slots_per_frame + 1e-9);
    if (fabs(gfc->frac_SpF) < 1e-9) gfc->frac_SpF = 0;

    gfc->slot_lag  = -gfc->frac_SpF;
    gfc->padding = 1;
    if (gfc->frac_SpF==0) gfc->padding = 0;
    /* check FFT will not use a negative starting offset */
    assert(576>=FFTOFFSET);
    /* check if we have enough data for FFT */
    assert(gfc->mf_size>=(BLKSIZE+gfc->framesize-FFTOFFSET));
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
    if (gfp->VBR) {
      gfc->padding=0;
    } else {
      if (gfp->disable_reservoir) {
	gfc->padding = 0;
	/* if the user specified --nores, dont very gfc->padding either */
	/* tiny changes in frac_SpF rounding will cause file differences */
      }else{
	if (gfc->frac_SpF != 0) {
	  if (gfc->slot_lag > (gfc->frac_SpF-1.0) ) {
	    gfc->slot_lag -= gfc->frac_SpF;
	    gfc->padding = 0;
	  }
	  else {
	    gfc->padding = 1;
	    gfc->slot_lag += (1-gfc->frac_SpF);
	  }
	}
      }
    }
  }


  /********************** status display  *****************************/
  if (gfc->pinfo == NULL && !gfp->silent) {
    int mod = gfp->version == 0 ? 100 : 50;
    if (gfc->frameNum%mod==0) {
      timestatus(gfp->out_samplerate,gfc->frameNum,gfc->totalframes,gfc->framesize);
#ifdef BRHIST
      if (disp_brhist)
	{
	  brhist_add_count();
	  brhist_disp();
	}
#endif
    }
  }


  if (gfc->psymodel) {
    /* psychoacoustic model
     * psy model has a 1 granule (576) delay that we must compensate for
     * (mt 6/99).
     */
    short int *bufp[2];  /* address of beginning of left & right granule */
    int blocktype[2];

    ms_ratio_prev=gfc->ms_ratio[gfc->mode_gr-1];
    for (gr=0; gr < gfc->mode_gr ; gr++) {

      for ( ch = 0; ch < gfc->stereo; ch++ )
	bufp[ch] = &inbuf[ch][576 + gr*576-FFTOFFSET];

      L3psycho_anal( gfp,bufp, gr, 
		     &gfc->ms_ratio[gr],&ms_ratio_next,&gfc->ms_ener_ratio[gr],
		     masking_ratio, masking_MS_ratio,
		     pe[gr],pe_MS[gr],blocktype);

      for ( ch = 0; ch < gfc->stereo; ch++ )
	gfc->l3_side.gr[gr].ch[ch].tt.block_type=blocktype[ch];

    }
  }else{
    for (gr=0; gr < gfc->mode_gr ; gr++)
      for ( ch = 0; ch < gfc->stereo; ch++ ) {
	gfc->l3_side.gr[gr].ch[ch].tt.block_type=NORM_TYPE;
	pe[gr][ch]=700;
      }
  }


  /* block type flags */
  for( gr = 0; gr < gfc->mode_gr; gr++ ) {
    for ( ch = 0; ch < gfc->stereo; ch++ ) {
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



  /* use m/s gfc->stereo? */
  check_ms_stereo =  (gfp->mode == MPG_MD_JOINT_STEREO);
  if (check_ms_stereo) {
    /* make sure block type is the same in each channel */
    check_ms_stereo =
      (gfc->l3_side.gr[0].ch[0].tt.block_type==gfc->l3_side.gr[0].ch[1].tt.block_type) &&
      (gfc->l3_side.gr[1].ch[0].tt.block_type==gfc->l3_side.gr[1].ch[1].tt.block_type);
  }
  if (check_ms_stereo) {
    /* ms_ratio = is like the ratio of side_energy/total_energy */
    FLOAT8 ms_ratio_ave;
    /*     ms_ratio_ave = .5*(ms_ratio[0] + ms_ratio[1]);*/
    ms_ratio_ave = .25*(gfc->ms_ratio[0] + gfc->ms_ratio[1]+
			 ms_ratio_prev + ms_ratio_next);
    if ( (ms_ratio_ave <.35) &&  
         (gfc->ms_ratio[0]<.45) &&
         (gfc->ms_ratio[1]<.45) )  gfc->mode_ext = MPG_MD_MS_LR;
  }
  if (gfp->force_ms) gfc->mode_ext = MPG_MD_MS_LR;



  if (gfc->pinfo != NULL) {
    int j;
    plotting_data *pinfo=gfc->pinfo;
    for ( gr = 0; gr < gfc->mode_gr; gr++ ) {
      for ( ch = 0; ch < gfc->stereo; ch++ ) {
	pinfo->ms_ratio[gr]=gfc->ms_ratio[gr];
	pinfo->ms_ener_ratio[gr]=gfc->ms_ener_ratio[gr];
	pinfo->blocktype[gr][ch]=
	  gfc->l3_side.gr[gr].ch[ch].tt.block_type;
	for ( j = 0; j < 576; j++ ) pinfo->xr[gr][ch][j]=xr[gr][ch][j];
	/* if MS stereo, switch to MS psy data */
	if (gfc->mode_ext==MPG_MD_MS_LR) {
	  pinfo->pe[gr][ch]=pinfo->pe[gr][ch+2];
	  pinfo->ers[gr][ch]=pinfo->ers[gr][ch+2];
	  memcpy(pinfo->energy[gr][ch],pinfo->energy[gr][ch+2],
		 sizeof(pinfo->energy[gr][ch]));
	}
      }
    }
  }




  /* bit and noise allocation */
  if (MPG_MD_MS_LR == gfc->mode_ext) {
    masking = &masking_MS_ratio;    /* use MS masking */
    pe_use=&pe_MS;
  } else {
    masking = &masking_ratio;    /* use LR masking */
    pe_use=&pe;
  }


  if (gfp->VBR) {
  if (gfp->experimentalY)
    VBR_quantize( gfp,*pe_use, gfc->ms_ener_ratio, xr, *masking, l3_enc,
		  scalefac);
  else
    VBR_iteration_loop( gfp,*pe_use, gfc->ms_ener_ratio, xr, *masking, l3_enc,
			scalefac);
  }else{
    iteration_loop( gfp,*pe_use, gfc->ms_ener_ratio, xr, *masking, l3_enc,
		    scalefac);
  }


#ifdef BRHIST
  brhist_temp[gfc->bitrate_index]++;
#endif


  /*  write the frame to the bitstream  */
  getframebits(gfp,&bitsPerFrame,&mean_bits);
  if (1==gfc->bs.bstype) 
    format_bitstream( gfp, bitsPerFrame, l3_enc, scalefac);
  else
    III_format_bitstream( gfp,bitsPerFrame, l3_enc, scalefac);


  /* copy mp3 bit buffer into array */
  mp3count = copy_buffer(mp3buf,mp3buf_size,&gfc->bs);

  if (gfp->bWriteVbrTag) AddVbrFrame(gfp);


  if (gfc->pinfo != NULL) {
    int j;
    plotting_data *pinfo=gfc->pinfo;
    for ( ch = 0; ch < gfc->stereo; ch++ ) {
      for ( j = 0; j < FFTOFFSET; j++ )
	pinfo->pcmdata[ch][j] = pinfo->pcmdata[ch][j+gfc->framesize];
      for ( j = FFTOFFSET; j < 1600; j++ ) {
	pinfo->pcmdata[ch][j] = inbuf[ch][j-FFTOFFSET];
      }
    }
  }

  gfc->frameNum++;
  return mp3count;
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
   short int buffer_l[], short int buffer_r[],int nsamples,
   char *mp3buf, int mp3buf_size)
{
  int mp3size=0,ret,i,ch,mf_needed;
  lame_internal_flags *gfc=gfp->internal_flags;
  short int *mfbuf[2];
  short int *in_buffer[2];
  in_buffer[0] = buffer_l;
  in_buffer[1] = buffer_r;

  if (!gfc->lame_init_params_init) return -3;

  /* some sanity checks */
  assert(ENCDELAY>=MDCTDELAY);
  assert(BLKSIZE-FFTOFFSET >= 0);
  mf_needed = BLKSIZE+gfc->framesize-FFTOFFSET;
  assert(MFSIZE>=mf_needed);

  mfbuf[0]=gfc->mfbuf[0];
  mfbuf[1]=gfc->mfbuf[1];

  if (gfp->num_channels==2  && gfc->stereo==1) {
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

    for (ch=0; ch<gfc->stereo; ch++) {
      if (gfc->resample_ratio>1)  {
	n_out=fill_buffer_downsample(gfp,&mfbuf[ch][gfc->mf_size],gfc->framesize,
					  in_buffer[ch],nsamples,&n_in,ch);
      } else if (gfc->resample_ratio<1) {
	n_out=fill_buffer_upsample(gfp,&mfbuf[ch][gfc->mf_size],gfc->framesize,
					  in_buffer[ch],nsamples,&n_in,ch);
      } else {
	n_out=Min(gfc->framesize,nsamples);
	n_in = n_out;
	memcpy( (char *) &mfbuf[ch][gfc->mf_size],(char *)in_buffer[ch],sizeof(short int)*n_out);
      }
      in_buffer[ch] += n_in;
    }

    nsamples -= n_in;
    gfc->mf_size += n_out;
    assert(gfc->mf_size<=MFSIZE);
    gfc->mf_samples_to_encode += n_out;


    if (gfc->mf_size >= mf_needed) {
      /* encode the frame */
      ret = lame_encode_frame(gfp,mfbuf[0],mfbuf[1],mp3buf,mp3buf_size);
      if (ret < 0) return ret;
      mp3buf += ret;
      mp3size += ret;

      /* shift out old samples */
      gfc->mf_size -= gfc->framesize;
      gfc->mf_samples_to_encode -= gfc->framesize;
      for (ch=0; ch<gfc->stereo; ch++)
	for (i=0; i<gfc->mf_size; i++)
	  mfbuf[ch][i]=mfbuf[ch][i+gfc->framesize];
    }
  }
  assert(nsamples==0);

  return mp3size;
}




int lame_encode_buffer_interleaved(lame_global_flags *gfp,
   short int buffer[], int nsamples, char *mp3buf, int mp3buf_size)
{
  int mp3size=0,ret,i,ch,mf_needed;
  lame_internal_flags *gfc=gfp->internal_flags;
  short int *mfbuf[2];

  if (!gfc->lame_init_params_init) return -3;

  mfbuf[0]=gfc->mfbuf[0];
  mfbuf[1]=gfc->mfbuf[1];

  /* some sanity checks */
  assert(ENCDELAY>=MDCTDELAY);
  assert(BLKSIZE-FFTOFFSET >= 0);
  mf_needed = BLKSIZE+gfc->framesize-FFTOFFSET;
  assert(MFSIZE>=mf_needed);

  if (gfp->num_channels == 1) {
    return lame_encode_buffer(gfp,buffer, NULL ,nsamples,mp3buf,mp3buf_size);
  }

  if (gfc->resample_ratio!=1)  {
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


  if (gfp->num_channels==2  && gfc->stereo==1) {
    /* downsample to mono */
    for (i=0; i<nsamples; ++i) {
      buffer[2*i]=((int)buffer[2*i]+(int)buffer[2*i+1])/2;
      buffer[2*i+1]=0;
    }
  }


  while (nsamples > 0) {
    int n_out;
    /* copy in new samples */
    n_out = Min(gfc->framesize,nsamples);
    for (i=0; i<n_out; ++i) {
      mfbuf[0][gfc->mf_size+i]=buffer[2*i];
      mfbuf[1][gfc->mf_size+i]=buffer[2*i+1];
    }
    buffer += 2*n_out;

    nsamples -= n_out;
    gfc->mf_size += n_out;
    assert(gfc->mf_size<=MFSIZE);
    gfc->mf_samples_to_encode += n_out;

    if (gfc->mf_size >= mf_needed) {
      /* encode the frame */
      ret = lame_encode_frame(gfp,mfbuf[0],mfbuf[1],mp3buf,mp3buf_size);
      if (ret == -1) {
	/* fatel error: mp3buffer was too small */
	return -1;
      }
      mp3buf += ret;
      mp3size += ret;

      /* shift out old samples */
      gfc->mf_size -= gfc->framesize;
      gfc->mf_samples_to_encode -= gfc->framesize;
      for (ch=0; ch<gfc->stereo; ch++)
	for (i=0; i<gfc->mf_size; i++)
	  mfbuf[ch][i]=mfbuf[ch][i+gfc->framesize];
    }
  }
  assert(nsamples==0);
  return mp3size;
}




/* old LAME interface.  use lame_encode_buffer instead */
int lame_encode(lame_global_flags *gfp, short int in_buffer[2][1152],char *mp3buf,int size){
  int imp3;
  lame_internal_flags *gfc=gfp->internal_flags;
  if (!gfc->lame_init_params_init) return -3;
  imp3= lame_encode_buffer(gfp,in_buffer[0],in_buffer[1],576*gfc->mode_gr,mp3buf,size);
  return imp3;
}


/*****************************************************************/
/* flush internal mp3 buffers,                                   */
/*****************************************************************/
int lame_encode_finish(lame_global_flags *gfp,char *mp3buffer, int mp3buffer_size)
{
  int imp3,mp3count,mp3buffer_size_remaining;
  short int buffer[2][1152];
  lame_internal_flags *gfc=gfp->internal_flags;

  memset((char *)buffer,0,sizeof(buffer));
  mp3count = 0;

  while (gfc->mf_samples_to_encode > 0) {

    mp3buffer_size_remaining = mp3buffer_size - mp3count;

    /* if user specifed buffer size = 0, dont check size */
    if (mp3buffer_size == 0) mp3buffer_size_remaining=0;  

    /* send in a frame of 0 padding until all internal sample buffers flushed */
    imp3=lame_encode_buffer(gfp,buffer[0],buffer[1],gfc->framesize,mp3buffer,mp3buffer_size_remaining);
    /* dont count the above padding: */
    gfc->mf_samples_to_encode -= gfc->framesize;

    if (imp3 < 0) {
      /* some type of fatel error */
      freegfc(gfc);    
      return imp3;
    }
    mp3buffer += imp3;
    mp3count += imp3;
  }


  gfc->frameNum--;
  if (gfc->pinfo == NULL && !gfp->silent) {
      timestatus(gfp->out_samplerate,gfc->frameNum,gfc->totalframes,gfc->framesize);
#ifdef BRHIST
      if (disp_brhist)
	{
	  brhist_add_count();
	  brhist_disp();
	  brhist_disp_total(gfp);
	}
#endif
      fprintf(stderr,"\n");
      fflush(stderr);
  }


  if (1==gfc->bs.bstype) 
    flush_bitstream(gfp);
  else
    III_FlushBitstream();


  mp3buffer_size_remaining = mp3buffer_size - mp3count;
  /* if user specifed buffer size = 0, dont check size */
  if (mp3buffer_size == 0) mp3buffer_size_remaining=0;  

  imp3= copy_buffer(mp3buffer,mp3buffer_size_remaining,&gfc->bs);
  if (imp3 < 0) {
    freegfc(gfc);    
    return imp3;
  }

  mp3count += imp3;
  freegfc(gfc);    
  return mp3count;
}


/*****************************************************************/
/* write VBR Xing header, and ID3 tag, if asked for               */
/*****************************************************************/
void lame_mp3_tags(lame_global_flags *gfp)
{
  if (gfp->bWriteVbrTag)
    {
      /* Calculate relative quality of VBR stream
       * 0=best, 100=worst */
      int nQuality=gfp->VBR_q*100/9;
      /* Write Xing header again */
      PutVbrTag(gfp,gfp->outPath,nQuality);
    }


  /* write an ID3 tag  */
  if(gfp->id3tag_used) {
    id3_buildtag(&gfp->id3tag);
    id3_writetag(gfp->outPath, &gfp->id3tag);
  }
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
  /*  fprintf(stderr,"FreeBSD mask is 0x%x\n",mask); */
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
  gfp->gtkflag=0;
  gfp->quality=5;
  gfp->input_format=sf_unknown;

  gfp->lowpassfreq=0;
  gfp->highpassfreq=0;
  gfp->lowpasswidth=-1;
  gfp->highpasswidth=-1;

  gfp->no_short_blocks=0;
  gfp->padding_type=2;
  gfp->swapbytes=0;
  gfp->silent=0;
  gfp->VBR=0;
  gfp->VBR_q=4;
  gfp->VBR_min_bitrate_kbps=0;
  gfp->VBR_max_bitrate_kbps=0;

  gfc->pcmbitwidth = 16;
  gfc->resample_ratio=1;
  gfc->lowpass_band=32;
  gfc->highpass_band=-1;
  gfc->VBR_min_bitrate=1;
  gfc->VBR_max_bitrate=13;

  gfc->OldValue[0]=180;
  gfc->OldValue[1]=180;
  gfc->CurrentStep=4;
  gfc->masking_lower=1;

  gfc->ms_ener_ratio_old=.25;
  gfc->blocktype_old[0]=STOP_TYPE;
  gfc->blocktype_old[1]=STOP_TYPE;

  return 0;
}


