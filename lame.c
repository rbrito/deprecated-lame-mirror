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

#ifdef HAVEGTK
#include "gtkanal.h"
#include <gtk/gtk.h>
#endif
#include "lame.h"
#include "util.h"
#include "globalflags.h"
#include "psymodel.h"
#include "newmdct.h"
#include "filters.h"
#include "quantize.h"
#include "quantize-pvt.h"
#include "l3bitstream.h"
#include "formatBitstream.h"
#include "version.h"
#include "VbrTag.h"
#include "id3tag.h"
#include "tables.h"
#include "brhist.h"
#include "get_audio.h"

#ifdef __riscos__
#include "asmstuff.h"
#endif

/* Global flags.  defined extern in globalflags.h */
/* default values set in lame_init() */
lame_global_flags gf;


/* Global variable definitions for lame.c */
Bit_stream_struc   bs;
III_side_info_t l3_side;
frame_params fr_ps;
int target_bitrate;
static layer info;





/********************************************************************
 *   initialize internal params based on data in gf
 *   (globalflags struct filled in by calling program)
 *
 ********************************************************************/
void lame_init_params(void)
{
  layer *info = fr_ps.header;
  int i,framesize;
  FLOAT compression_ratio;

  gf.frameNum=0;
  gf.force_ms=0;
  InitFormatBitStream();
  if (gf.num_channels==1) {
    gf.mode = MPG_MD_MONO;
  }      
  gf.stereo=2;
  if (gf.mode == MPG_MD_MONO) gf.stereo=1;

#ifdef BRHIST
  if (gf.silent) {
    disp_brhist=0;  /* turn of VBR historgram */
  }
  if (!gf.VBR) {
    disp_brhist=0;  /* turn of VBR historgram */
  }
#endif

  /* set the output sampling rate, and resample options if necessary 
     samplerate = input sample rate
     resamplerate = ouput sample rate
  */
  if (gf.resamplerate==0) {
    /* user did not specify output sample rate */
    gf.resamplerate=gf.samplerate;   /* default */


    /* if resamplerate is not valid, find a valid value */
    if (gf.resamplerate>=48000) gf.resamplerate=48000;
    else if (gf.resamplerate>=44100) gf.resamplerate=44100;
    else if (gf.resamplerate>=32000) gf.resamplerate=32000;
    else if (gf.resamplerate>=24000) gf.resamplerate=24000;
    else if (gf.resamplerate>=22050) gf.resamplerate=22050;
    else gf.resamplerate=16000;


    if (gf.brate>0) {
      /* check if user specified bitrate requires downsampling */
      compression_ratio = gf.resamplerate*16*gf.stereo/(1000.0*gf.brate);
      if (!gf.VBR && compression_ratio > 13 ) {
	/* automatic downsample, if possible */
	gf.resamplerate = (10*1000.0*gf.brate)/(16*gf.stereo);
	if (gf.resamplerate<=16000) gf.resamplerate=16000;
	else if (gf.resamplerate<=22050) gf.resamplerate=22050;
	else if (gf.resamplerate<=24000) gf.resamplerate=24000;
	else if (gf.resamplerate<=32000) gf.resamplerate=32000;
	else if (gf.resamplerate<=44100) gf.resamplerate=44100;
	else gf.resamplerate=48000;
      }
    }
  }

  gf.mode_gr = (gf.resamplerate <= 24000) ? 1 : 2;  /* mode_gr = 2 */
  gf.encoder_delay = ENCDELAY;
  gf.framesize = gf.mode_gr*576;

  if (gf.brate==0) { /* user didn't specify a bitrate, use default */
    gf.brate=128;
    if (gf.mode_gr==1) gf.brate=64;
  }
  

  gf.resample_ratio=1;
  if (gf.resamplerate != gf.samplerate) gf.resample_ratio = (FLOAT)gf.samplerate/(FLOAT)gf.resamplerate;

  /* estimate total frames.  must be done after setting sampling rate so
   * we know the framesize.  */
  gf.totalframes=0;
  framesize = gf.mode_gr*576;
  gf.totalframes = 2+ gf.num_samples/(gf.resample_ratio*framesize);



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
  if (gf.brate >= 320) gf.VBR=0;  /* dont bother with VBR at 320kbs */
  compression_ratio = gf.resamplerate*16*gf.stereo/(1000.0*gf.brate);

  /* if the user did not specify a large VBR_min_bitrate (=brate),
   * compression_ratio not well defined.  take a guess: */
  if (gf.VBR && compression_ratio>11) {
    compression_ratio = 11.025;               /* like 44kHz/64kbs per channel */
    if (gf.VBR_q <= 4) compression_ratio=8.82;   /* like 44kHz/80kbs per channel */
    if (gf.VBR_q <= 3) compression_ratio=7.35;   /* like 44kHz/96kbs per channel */
  }


  /* At higher quality (lower compression) use STEREO instead of JSTEREO.
   * (unless the user explicitly specified a mode ) */
  if ( (!gf.mode_fixed) && (gf.mode !=MPG_MD_MONO)) {
    if (compression_ratio < 9 ) {
      gf.mode = MPG_MD_STEREO; 
    }
  }

  /****************************************************************/
  /* if a filter has not been enabled, see if we should add one: */
  /****************************************************************/
  if (gf.lowpassfreq == 0) {
    /* Should we disable the scalefactor band 21 cutoff? 
     * FhG does this for bps <= 128kbs, so we will too.  
     * This amounds to a 16kHz low-pass filter.  If that offends you, you
     * probably should not be encoding at 128kbs!
     * There is no ratio[21] or xfsf[21], so when these coefficients are
     * included they are just quantized as is.  mt 5/99
     */
    /* disable sfb21 cutoff for low amounts of compression */
    if (compression_ratio<9.0) gf.sfb21=0;


    /* Should we use some lowpass filters? */    
    if (!gf.VBR) {
    if (gf.brate/gf.stereo <= 32  ) {
      /* high compression, low bitrates, lets use a filter */
      if (compression_ratio > 15.5) {
	gf.lowpass1=.35;  /* good for 16kHz 16kbs compression = 16*/
	gf.lowpass2=.50;
      }else if (compression_ratio > 14) {
	gf.lowpass1=.40;  /* good for 22kHz 24kbs compression = 14.7*/
	gf.lowpass2=.55;
      }else if (compression_ratio > 10) {
	gf.lowpass1=.55;  /* good for 16kHz 24kbs compression = 10.7*/
	gf.lowpass2=.70;
      }else if (compression_ratio > 8) {
	gf.lowpass1=.65; 
	gf.lowpass2=.80;
      }else {
	gf.lowpass1=.85;
	gf.lowpass2=.99;
      }
    }
    }
    /* 14.5khz = .66  16kHz = .73 */
    /*
      lowpass1 = .73-.10;
      lowpass2 = .73+.05;
    */
  }
    


  /* apply user driven filters*/
  if ( gf.highpassfreq > 0 ) {
    gf.highpass1 = 2.0*gf.highpassfreq/gf.resamplerate; /* will always be >=0 */
    if ( gf.highpasswidth >= 0 ) {
      gf.highpass2 = 2.0*(gf.highpassfreq+gf.highpasswidth)/gf.resamplerate;
    } else {
      gf.highpass2 = 1.15*2.0*gf.highpassfreq/gf.resamplerate; /* 15% above on default */
    }
    if ( gf.highpass1 == gf.highpass2 ) { /* ensure highpass1 < highpass2 */
      gf.highpass2 += 1E-6;
    }
    gf.highpass1 = Min( 1, gf.highpass1 );
    gf.highpass2 = Min( 1, gf.highpass2 );
  }
  if ( gf.lowpassfreq > 0 ) {
    gf.lowpass2 = 2.0*gf.lowpassfreq/gf.resamplerate; /* will always be >=0 */
    if ( gf.lowpasswidth >= 0 ) { 
      gf.lowpass1 = 2.0*(gf.lowpassfreq-gf.lowpasswidth)/gf.resamplerate;
      if ( gf.lowpass1 < 0 ) { /* has to be >= 0 */
	gf.lowpass1 = 0;
      }
    } else {
      gf.lowpass1 = 0.85*2.0*gf.lowpassfreq/gf.resamplerate; /* 15% below on default */
    }
    if ( gf.lowpass1 == gf.lowpass2 ) { /* ensure lowpass1 < lowpass2 */
      gf.lowpass2 += 1E-6;
    }
    gf.lowpass1 = Min( 1, gf.lowpass1 );
    gf.lowpass2 = Min( 1, gf.lowpass2 );
  }
  
  /* dont use cutoff filter and lowpass filter */
  if ( gf.lowpass1>0 ) gf.sfb21 = 0;



  info->emphasis = gf.emphasis;
  info->copyright = gf.copyright;
  info->original = gf.original;
  info->error_protection = gf.error_protection;
  info->lay = 3;
  info->mode = gf.mode;
  info->mode_ext=MPG_MD_LR_LR;
  fr_ps.actual_mode = info->mode;
  gf.stereo = (info->mode == MPG_MD_MONO) ? 1 : 2;
  

  info->sampling_frequency = SmpFrqIndex((long)gf.resamplerate, &info->version);
  if( info->sampling_frequency < 0) {
    display_bitrates(stderr);
    exit(1);
  }
  if( (info->bitrate_index = BitrateIndex(3, gf.brate, info->version,gf.resamplerate)) < 0) {
    display_bitrates(stderr);
    exit(1);
  }
  

  /* choose a min/max bitrate for VBR */
  if (gf.VBR) {
    /* if the user didn't specify VBR_max_bitrate: */
    if (0==gf.VBR_max_bitrate_kbps) {
      /* default max bitrate is 256kbs */
      /* we do not normally allow 320bps frams with VBR, unless: */
      gf.VBR_max_bitrate=13;   /* default: allow 256kbs */
      if (gf.VBR_min_bitrate_kbps>=256) gf.VBR_max_bitrate=14;  
      if (gf.VBR_q == 0) gf.VBR_max_bitrate=14;   /* allow 320kbs */
      if (gf.VBR_q >= 4) gf.VBR_max_bitrate=12;   /* max = 224kbs */
      if (gf.VBR_q >= 8) gf.VBR_max_bitrate=9;    /* low quality, max = 128kbs */
    }else{
      if( (gf.VBR_max_bitrate  = BitrateIndex(3, gf.VBR_max_bitrate_kbps, info->version,gf.resamplerate)) < 0) {
	display_bitrates(stderr);
	exit(1);
      }
    }
    if (0==gf.VBR_min_bitrate_kbps) {
      gf.VBR_min_bitrate=1;  /* 32 kbps */
    }else{
      if( (gf.VBR_min_bitrate  = BitrateIndex(3, gf.VBR_min_bitrate_kbps, info->version,gf.resamplerate)) < 0) {
	display_bitrates(stderr);
	exit(1);
      }
    }

  }


  if (gf.VBR) gf.quality=Min(gf.quality,2);    /* always use quality <=2  with VBR */
  /* dont allow forced mid/side stereo for mono output */
  if (gf.mode == MPG_MD_MONO) gf.force_ms=0;  


  /* Do not write VBR tag if VBR flag is not specified */
  if (gf.VBR==0) gf.bWriteVbrTag=0;

  /* some file options not allowed if output is: not specified or stdout */
  if (gf.outPath==NULL || gf.outPath[0]=='-' ) {
    gf.bWriteVbrTag=0; /* turn off VBR tag */
    id3tag.used=0;         /* turn of id3 tagging */
  }



  if (gf.gtkflag) {
    gf.bWriteVbrTag=0;  /* disable Xing VBR tag */
  }

  if (gf.mode_gr==1) {
    gf.bWriteVbrTag=0;      /* no MPEG2 Xing VBR tags yet */
  }

  init_bit_stream_w(&bs);


#ifdef BRHIST
  if (gf.VBR) {
    if (disp_brhist)
      brhist_init(1, 14, info);
  } else
    disp_brhist = 0;
#endif

  /* set internal feature flags.  USER should not access these since
   * some combinations will produce strange results */
  if (gf.quality>=9) {
    /* 9 = worst quality */
    gf.filter_type=0;
    gf.quantization=0;
    gf.psymodel=0;
    gf.noise_shaping=0;
    gf.noise_shaping_stop=0;
    gf.ms_masking=0;
    gf.use_best_huffman=0;
  } else if (gf.quality>=5) {
    /* 5..8 quality, the default  */
    gf.filter_type=0;
    gf.quantization=0;
    gf.psymodel=1;
    gf.noise_shaping=1;
    gf.noise_shaping_stop=0;
    gf.ms_masking=0;
    gf.use_best_huffman=0;
  } else if (gf.quality>=2) {
    /* 2..4 quality */
    gf.filter_type=0;
    gf.quantization=1;
    gf.psymodel=1;
    gf.noise_shaping=1;
    gf.noise_shaping_stop=0;
    gf.ms_masking=1;
    gf.use_best_huffman=1;
  } else {
    /* 0..1 quality */
    gf.filter_type=1;         /* not yet coded */
    gf.quantization=1;
    gf.psymodel=1;
    gf.noise_shaping=3;       /* not yet coded */
    gf.noise_shaping_stop=2;  /* not yet coded */
    gf.ms_masking=1;
    gf.use_best_huffman=2;   /* not yet coded */
    exit(-99);  
  }

  /* best_quant algorithms not based on over=0 require this: */
  if (gf.experimentalX) gf.noise_shaping_stop=1;


  for (i = 0; i < SBMAX_l + 1; i++) {
    scalefac_band.l[i] =
      sfBandIndex[info->sampling_frequency + (info->version * 3)].l[i];
  }
  for (i = 0; i < SBMAX_s + 1; i++) {
    scalefac_band.s[i] =
      sfBandIndex[info->sampling_frequency + (info->version * 3)].s[i];
  }



  if (gf.bWriteVbrTag)
    {
      /* Write initial VBR Header to bitstream */
      InitVbrTag(&bs,info->version-1,gf.mode,info->sampling_frequency);
    }

  return;
}









/************************************************************************
 *
 * print_config
 *
 * PURPOSE:  Prints the encoding parameters used
 *
 ************************************************************************/
void lame_print_config(void)
{
  layer *info = fr_ps.header;
  char *mode_names[4] = { "stereo", "j-stereo", "dual-ch", "single-ch" };
  FLOAT resamplerate=s_freq[info->version][info->sampling_frequency];    
  FLOAT samplerate = gf.resample_ratio*resamplerate;
  FLOAT compression=
    (FLOAT)(gf.stereo*16*resamplerate)/
    (FLOAT)(bitrate[info->version][info->lay-1][info->bitrate_index]);

  lame_print_version(stderr);
  if (gf.num_channels==2 && gf.stereo==1) {
    fprintf(stderr, "Autoconverting from stereo to mono. Setting encoding to mono mode.\n");
  }
  if (gf.resample_ratio!=1) {
    fprintf(stderr,"Resampling:  input=%ikHz  output=%ikHz\n",
	    (int)samplerate,(int)resamplerate);
  }
  if (gf.highpass2>0.0)
    fprintf(stderr, "Highpass filter: cutoff below %g Hz, increasing upto %g Hz\n",
	    gf.highpass1*resamplerate*500, 
	    gf.highpass2*resamplerate*500);
  if (gf.lowpass1>0.0)
    fprintf(stderr, "Lowpass filter: cutoff above %g Hz, decreasing from %g Hz\n",
	    gf.lowpass2*resamplerate*500, 
	    gf.lowpass1*resamplerate*500);
  if (gf.sfb21) {
    int *scalefac_band_long = &sfBandIndex[info->sampling_frequency + (info->version * 3)].l[0];
    fprintf(stderr, "sfb21 filter: sharp cutoff above %g Hz\n",
            scalefac_band_long[SBPSY_l]/576.0*resamplerate*500);
  }

  if (gf.gtkflag) {
    fprintf(stderr, "Analyzing %s \n",gf.inPath);
  }
  else {
    fprintf(stderr, "Encoding %s to %s\n",
	    (strcmp(gf.inPath, "-")? gf.inPath : "stdin"),
	    (strcmp(gf.outPath, "-")? gf.outPath : "stdout"));
    if (gf.VBR)
      fprintf(stderr, "Encoding as %.1fkHz VBR(q=%i) %s MPEG%i LayerIII  qual=%i\n",
	      s_freq[info->version][info->sampling_frequency],
	      gf.VBR_q,mode_names[info->mode],2-info->version,gf.quality);
    else
      fprintf(stderr, "Encoding as %.1f kHz %d kbps %s MPEG%i LayerIII (%4.1fx)  qual=%i\n",
	      s_freq[info->version][info->sampling_frequency],
	      bitrate[info->version][info->lay-1][info->bitrate_index],
	      mode_names[info->mode],2-info->version,compression,gf.quality);
  }
  fflush(stderr);
}



 








/************************************************************************
* 
* encodeframe()
*
************************************************************************/
int lame_encode(short int Buffer[2][1152],char *mpg123bs)
{
  static unsigned long frameBits;
  static unsigned long bitsPerSlot;
  static FLOAT8 frac_SpF;
  static FLOAT8 slot_lag;
  static unsigned long sentBits = 0;
#if (ENCDELAY < 800) 
#define EXTRADELAY (1152+ENCDELAY-MDCTDELAY)
#else
#define EXTRADELAY (ENCDELAY-MDCTDELAY)
#endif
#define MFSIZE (EXTRADELAY+1152)
  static short int mfbuf[2][MFSIZE];
  FLOAT8 xr[2][2][576];
  int l3_enc[2][2][576];
  int mpg123count;
  int samplesPerFrame;
  III_psy_ratio masking_ratio[2][2];    /*LR ratios */
  III_psy_ratio masking_MS_ratio[2][2]; /*MS ratios */
  III_psy_ratio (*masking)[2][2];  /*LR ratios and MS ratios*/
  III_scalefac_t scalefac[2][2];

  typedef FLOAT8 pedata[2][2];
  pedata pe,pe_MS;
  pedata *pe_use;

  int ch,gr,mean_bits;
  int bitsPerFrame;
#if (ENCDELAY < 800) 
  static int frame_buffered=0;
#endif
  layer *info;
  int i;
  int check_ms_stereo;
  static FLOAT8 ms_ratio[2]={0,0};
  FLOAT8 ms_ratio_next=0;
  FLOAT8 ms_ratio_prev=0;
  FLOAT8 ms_ener_ratio[2]={0,0};
  static int init=0;

  memset((char *) masking_ratio, 0, sizeof(masking_ratio));
  memset((char *) masking_MS_ratio, 0, sizeof(masking_MS_ratio));
  memset((char *) scalefac, 0, sizeof(scalefac));

  
  if (init==0) {
    memset((char *) mfbuf, 0, sizeof(mfbuf));
    init++;
  }
  samplesPerFrame=gf.mode_gr*576;
  info = fr_ps.header;
  info->mode_ext = MPG_MD_LR_LR; 


  if (gf.frameNum==0 )  {    
    /* Figure average number of 'slots' per frame. */
    FLOAT8 avg_slots_per_frame;
    FLOAT8 sampfreq =   s_freq[info->version][info->sampling_frequency];
    int bit_rate = bitrate[info->version][info->lay-1][info->bitrate_index];
    sentBits = 0;
    bitsPerSlot = 8;
    avg_slots_per_frame = (bit_rate*samplesPerFrame) /
           (sampfreq* bitsPerSlot);
    frac_SpF  = avg_slots_per_frame - (int) avg_slots_per_frame;
    slot_lag  = -frac_SpF;
    info->padding = 1;
    if (fabs(frac_SpF) < 1e-9) info->padding = 0;

    assert(MFSIZE>=samplesPerFrame+EXTRADELAY);  
    /* check enougy space to store data needed by FFT */
    assert(MFSIZE>=BLKSIZE+samplesPerFrame-FFTOFFSET);     
    /* check FFT will not use data behond what we read in */
    assert(samplesPerFrame+EXTRADELAY>=BLKSIZE+samplesPerFrame-FFTOFFSET); 
    /* check FFT will not use a negative starting offset */
    assert(576>=FFTOFFSET);
    /* total delay is at least as large is MDCTDELAY */
    assert(ENCDELAY>MDCTDELAY);
  }


  /* shift in new samples, delayed by EXTRADELAY */
   for (ch=0; ch<gf.stereo; ch++)
     for (i=0; i<EXTRADELAY; i++)
       mfbuf[ch][i]=mfbuf[ch][i+samplesPerFrame];
   for (ch=0; ch<gf.stereo; ch++)
     for (i=0; i<samplesPerFrame; i++)
       mfbuf[ch][i+EXTRADELAY]=Buffer[ch][i];

#if (ENCDELAY < 800) 
   /* just buffer the first frame, and return */
   if (gf.frameNum==0 && !frame_buffered) {
     frame_buffered=1;
     return 0;
   }else {
     /* reset, for the next time frameNum==0 */
     frame_buffered=0;  
   }
#endif


  /* use m/s gf.stereo? */
  check_ms_stereo =   ((info->mode == MPG_MD_JOINT_STEREO) && 
		       (info->version == 1) &&
		       (gf.stereo==2) );



  /********************** padding *****************************/
  if (gf.VBR) {
    info->padding=0;
  } else {
    if (gf.disable_reservoir) {
      info->padding = 0;
      /* if the user specified --nores, dont very info->padding either */
      /* tiny changes in frac_SpF rounding will cause file differences */
    }else{
      if (frac_SpF != 0) {
	if (slot_lag > (frac_SpF-1.0) ) {
	  slot_lag -= frac_SpF;
	  info->padding = 0;
	}
	else {
	  info->padding = 1;
	  slot_lag += (1-frac_SpF);
	}
      }
    }
  }


  /********************** status display  *****************************/
  if (!gf.gtkflag && !gf.silent) {
    int mod = info->version == 0 ? 100 : 20;
    if (gf.frameNum%mod==0) {
      timestatus(info,gf.frameNum,gf.totalframes);
#ifdef BRHIST
      if (disp_brhist)
	{
	  brhist_add_count();
	  brhist_disp();
	}
#endif
    }
  }



  /***************************** Layer 3 **********************************

                       gr 0            gr 1
mfbuf:           |--------------|---------------|-------------|
MDCT output:  |--------------|---------------|-------------|

FFT's                    <---------1024---------->
                                         <---------1024-------->
    


    mfbuf = large buffer of PCM data size=frame_size+EXTRADELAY
    encoder acts on mfbuf[ch][0], but output is delayed by MDCTDELAY
    so the MDCT coefficints are from mfbuf[ch][-MDCTDELAY]

    psy-model FFT has a 1 granule day, so we feed it data for the next granule.
    FFT is centered over granule:  224+576+224  
    So FFT starts at:   576-224-MDCTDELAY
   
    MPEG2:  FFT ends at:  BLKSIZE+576-224-MDCTDELAY
    MPEG1:  FFT ends at:  BLKSIZE+2*576-224-MDCTDELAY    (1904)

    FFT starts at 576-224-MDCTDELAY (304)

   */
  if (gf.psymodel) {  
    /* psychoacoustic model 
     * psy model has a 1 granule (576) delay that we must compensate for 
     * (mt 6/99). 
     */
    short int *bufp[2];  /* address of beginning of left & right granule */
    int blocktype[2];

    ms_ratio_prev=ms_ratio[gf.mode_gr-1];
    for (gr=0; gr < gf.mode_gr ; gr++) {

      for ( ch = 0; ch < gf.stereo; ch++ )
	bufp[ch] = &mfbuf[ch][576 + gr*576-FFTOFFSET];

      L3psycho_anal( bufp, gr, info,
		     s_freq[info->version][info->sampling_frequency] * 1000.0,
		     check_ms_stereo,
		     &ms_ratio[gr],&ms_ratio_next,&ms_ener_ratio[gr],
		     masking_ratio, masking_MS_ratio,
		     pe[gr],pe_MS[gr],blocktype);

      for ( ch = 0; ch < gf.stereo; ch++ ) 
	l3_side.gr[gr].ch[ch].tt.block_type=blocktype[ch];

    }
  }else{
    for (gr=0; gr < gf.mode_gr ; gr++) 
      for ( ch = 0; ch < gf.stereo; ch++ ) {
	l3_side.gr[gr].ch[ch].tt.block_type=NORM_TYPE;
	pe[gr][ch]=700;
      }
  }


  /* block type flags */
  for( gr = 0; gr < gf.mode_gr; gr++ ) {
    for ( ch = 0; ch < gf.stereo; ch++ ) {
      gr_info *cod_info = &l3_side.gr[gr].ch[ch].tt;
      cod_info->mixed_block_flag = 0;     /* never used by this model */
      if (cod_info->block_type == NORM_TYPE )
	cod_info->window_switching_flag = 0;
      else
	cod_info->window_switching_flag = 1;
    }
  }

  /* polyphase filtering / mdct */
  mdct_sub48(mfbuf[0], mfbuf[1], xr, &l3_side);

  /* lowpass MDCT filtering */
  if (gf.sfb21 || gf.lowpass1>0 || gf.highpass2>0) 
    filterMDCT(xr, &l3_side);

  if (check_ms_stereo) {
    /* make sure block type is the same in each channel */
    check_ms_stereo = 
      (l3_side.gr[0].ch[0].tt.block_type==l3_side.gr[0].ch[1].tt.block_type) &&
      (l3_side.gr[1].ch[0].tt.block_type==l3_side.gr[1].ch[1].tt.block_type);
  }
  if (check_ms_stereo) {
    /* ms_ratio = is like the ratio of side_energy/total_energy */
    FLOAT8 ms_ratio_ave;
    /*     ms_ratio_ave = .5*(ms_ratio[0] + ms_ratio[1]);*/
    ms_ratio_ave = .25*(ms_ratio[0] + ms_ratio[1]+ 
			 ms_ratio_prev + ms_ratio_next);
    if ( ms_ratio_ave <.35) info->mode_ext = MPG_MD_MS_LR;
  }
  if (gf.force_ms) info->mode_ext = MPG_MD_MS_LR;


#ifdef HAVEGTK
  if (gf.gtkflag) { 
    int j;
    for ( gr = 0; gr < gf.mode_gr; gr++ ) {
      for ( ch = 0; ch < gf.stereo; ch++ ) {
	pinfo->ms_ratio[gr]=ms_ratio[gr];
	pinfo->ms_ener_ratio[gr]=ms_ener_ratio[gr];
	pinfo->blocktype[gr][ch]=
	  l3_side.gr[gr].ch[ch].tt.block_type;
	for ( j = 0; j < 576; j++ ) pinfo->xr[gr][ch][j]=xr[gr][ch][j];
	/* if MS stereo, switch to MS psy data */
	if (gf.ms_masking && (info->mode_ext==MPG_MD_MS_LR)) {
	  pinfo->pe[gr][ch]=pinfo->pe[gr][ch+2];
	  pinfo->ers[gr][ch]=pinfo->ers[gr][ch+2];
	  memcpy(pinfo->energy[gr][ch],pinfo->energy[gr][ch+2],
		 sizeof(pinfo->energy[gr][ch]));
	}
      }
    }
  }
#endif




  /* bit and noise allocation */
  if ((MPG_MD_MS_LR == info->mode_ext) && gf.ms_masking) {
    masking = &masking_MS_ratio;    /* use MS masking */
    pe_use=&pe_MS;
  } else {
    masking = &masking_ratio;    /* use LR masking */
    pe_use=&pe;
  }




  if (gf.VBR) {
    VBR_iteration_loop( *pe_use, ms_ratio, xr, *masking, &l3_side, l3_enc, 
			scalefac, &fr_ps);
  }else{
    iteration_loop( *pe_use, ms_ratio, xr, *masking, &l3_side, l3_enc, 
		    scalefac, &fr_ps);
  }
  /*
  VBR_iteration_loop_new( *pe_use, ms_ratio, xr, masking, &l3_side, l3_enc, 
			  &scalefac, &fr_ps);
  */



#ifdef BRHIST
  brhist_temp[info->bitrate_index]++;
#endif


  /*  write the frame to the bitstream  */
  getframebits(info,&bitsPerFrame,&mean_bits);
  III_format_bitstream( bitsPerFrame, &fr_ps, l3_enc, &l3_side, 
			scalefac, &bs);


  frameBits = bs.totbit - sentBits;

  
  if ( frameBits % bitsPerSlot )   /* a program failure */
    fprintf( stderr, "Sent %ld bits = %ld slots plus %ld\n",
	     frameBits, frameBits/bitsPerSlot,
	     frameBits%bitsPerSlot );
  sentBits += frameBits;

  /* copy mp3 bit buffer into array */
  mpg123count = copy_buffer(mpg123bs,&bs);
  empty_buffer(&bs);  /* empty buffer */

  if (gf.bWriteVbrTag) AddVbrFrame((int)(sentBits/8));

#ifdef HAVEGTK 
  if (gf.gtkflag) { 
    int j;
    int framesize = (info->version==0) ? 576 : 1152;
    for ( ch = 0; ch < gf.stereo; ch++ ) {
      for ( j = 0; j < FFTOFFSET; j++ )
	pinfo->pcmdata[ch][j] = pinfo->pcmdata[ch][j+framesize];
      for ( j = FFTOFFSET; j < 1600; j++ ) {
	pinfo->pcmdata[ch][j] = mfbuf[ch][j-FFTOFFSET];
      }
    }
  }
#endif
  gf.frameNum++;

  return mpg123count;
}




/* initialize mp3 encoder */
lame_global_flags * lame_init(void)
{

#ifdef __FreeBSD__
#include <floatingpoint.h>
  {
  /* seet floating point mask to the Linux default */
  fp_except_t mask;
  mask=fpgetmask();
  /* if bit is set, we get SIGFPE on that error! */
  fpsetmask(mask & ~(FP_X_INV|FP_X_DZ));
  /*  fprintf(stderr,"FreeBSD mask is 0x%x\n",mask); */
  }
#endif
#ifdef ABORTFP
  {
#include <fpu_control.h>
  unsigned int mask;
  _FPU_GETCW(mask);
  /* Set the Linux mask to abort on most FPE's */
  /* if bit is set, we _mask_ SIGFPE on that error! */
  /*  mask &= ~( _FPU_MASK_IM | _FPU_MASK_ZM | _FPU_MASK_OM | _FPU_MASK_UM );*/
   mask &= ~( _FPU_MASK_IM | _FPU_MASK_ZM | _FPU_MASK_OM );
  _FPU_SETCW(mask);
  }
#endif
#ifdef __riscos__
  /* Disable FPE's under RISC OS */
  /* if bit is set, we disable trapping that error! */
  /*   _FPE_IVO : invalid operation */
  /*   _FPE_DVZ : divide by zero */
  /*   _FPE_OFL : overflow */
  /*   _FPE_UFL : underflow */
  /*   _FPE_INX : inexact */
  DisableFPETraps( _FPE_IVO | _FPE_DVZ | _FPE_OFL );
#endif


#if defined(_MSC_VER)
  {
	#include <float.h>
	unsigned int mask;
	mask=_controlfp( 0, 0 );
	mask&=~(_EM_OVERFLOW|_EM_UNDERFLOW|_EM_ZERODIVIDE|_EM_INVALID);
	mask=_controlfp( mask, _MCW_EM );
	}
#endif


  /* Global flags.  set defaults here */
  gf.allow_diff_short=0;
  gf.ATHonly=0;
  gf.noATH=0;
  gf.bWriteVbrTag=1;
  gf.cwlimit=0;
  gf.disable_reservoir=0;
  gf.experimentalX = 0;
  gf.experimentalY = 0;
  gf.experimentalZ = 0;
  gf.frameNum=0;
  gf.gtkflag=0;
  gf.quality=5;
  gf.input_format=sf_unknown;
  gf.lowpassfreq=0;
  gf.highpassfreq=0;
  gf.lowpasswidth=-1;
  gf.highpasswidth=-1;
  gf.lowpass1=0;
  gf.lowpass2=0;
  gf.highpass1=0;
  gf.highpass2=0;
  gf.no_short_blocks=0;
  gf.resample_ratio=1;
  gf.swapbytes=0;
  gf.sfb21=1;
  gf.silent=0;
  gf.totalframes=0;
  gf.VBR=0;
  gf.VBR_q=4;
  gf.VBR_min_bitrate_kbps=0;
  gf.VBR_max_bitrate_kbps=0;
  gf.VBR_min_bitrate=1;
  gf.VBR_max_bitrate=13;


  gf.mode = MPG_MD_JOINT_STEREO; 
  gf.force_ms=0;
  gf.brate=0;
  gf.copyright=0;
  gf.original=1;
  gf.error_protection=0;
  gf.emphasis=0;
  gf.samplerate=1000*44.1;
  gf.resamplerate=0;
  gf.num_channels=2;
  gf.num_samples=MAX_U_32_NUM;

  gf.inPath=NULL;
  gf.outPath=NULL;
  id3tag.used=0;

  /* Clear info structure */
  memset(&info,0,sizeof(info));
  memset(&bs, 0, sizeof(Bit_stream_struc));
  memset(&fr_ps, 0, sizeof(frame_params));
  memset(&l3_side,0x00,sizeof(III_side_info_t));


  
  fr_ps.header = &info;
  fr_ps.tab_num = -1;             /* no table loaded */
  fr_ps.alloc = NULL;
  info.version = MPEG_AUDIO_ID;   /* =1   Default: MPEG-1 */
  info.extension = 0;       


  return &gf;
}



/*****************************************************************/
/* flush internal mp3 buffers,                                   */
/*****************************************************************/
int lame_encode_finish(char *mpg123bs)
{
  int mpg123count;
  gf.frameNum--;
  if (!gf.gtkflag && !gf.silent) {
      timestatus(&info,gf.frameNum,gf.totalframes);
#ifdef BRHIST
      if (disp_brhist)
	{
	  brhist_add_count();
	  brhist_disp();
	  brhist_disp_total(fr_ps.header);
	}
#endif
      fprintf(stderr,"\n");
      fflush(stderr);
  }


  III_FlushBitstream();
  mpg123count = copy_buffer(mpg123bs,&bs);

  desalloc_buffer(&bs);    /* Deallocate all buffers */
  
  return mpg123count;
}



/*****************************************************************/
/* write VBR Xing header, and ID3 tag, if asked for               */
/*****************************************************************/
void lame_mp3_tags(void)
{
  if (gf.bWriteVbrTag)
    {
      /* Calculate relative quality of VBR stream  
       * 0=best, 100=worst */
      int nQuality=gf.VBR_q*100/9;
      /* Write Xing header again */
      PutVbrTag(gf.outPath,nQuality);
    }
  
  
  /* write an ID3 tag  */
  if(id3tag.used) {
    id3_buildtag(&id3tag);
    id3_writetag(gf.outPath, &id3tag);
  }
}
