#include "util.h"
#include "tables.h"
#include "globalflags.h"



/************************************************************************
 *  lowpass filter on MDCT coefficients
 ************************************************************************/
void filterMDCT( FLOAT8 xr_org[2][2][576], 
		III_side_info_t *l3_side, frame_params *fr_ps)
{
  int gr,ch,i;
  layer *info  = fr_ps->header;
  int stereo = fr_ps->stereo;
  int *scalefac_band_long;
  int *scalefac_band_short;

  scalefac_band_long  = &sfBandIndex[info->sampling_frequency + (info->version * 3)].l[0];
  scalefac_band_short = &sfBandIndex[info->sampling_frequency + (info->version * 3)].s[0];

  /* voice mode disables short blocks */  
  if (gf.voice_mode) {
    for ( gr = 0; gr < gf.mode_gr; gr++ ) {
      for (ch =0 ; ch < stereo ; ch++) {
		int start = scalefac_band_long[ SBMAX_l-3 ];
		  for ( i = start; i < 576; i++ )
			  xr_org[gr][ch][i]=0;
      }
      for (ch =0 ; ch < stereo ; ch++) {
		for ( i = 0; i < 2; i++ )
			  xr_org[gr][ch][i]=0;
      }
    }
  }

  if (gf.lowpass1>0) {
    FLOAT start,stop;
    for ( gr = 0; gr < gf.mode_gr; gr++ ) {
      for (ch =0 ; ch < stereo ; ch++) {
	int shortblock = (l3_side->gr[gr].ch[ch].tt.block_type==SHORT_TYPE);
	if (shortblock) {
	  int j;
	  start = gf.lowpass1*192;
	  stop  = gf.lowpass2*192;
	  for (j=0; j<3; j++) {
	    for ( i = ceil(start);  i < 192; i++ ) {
	      int i0 = 3*i+j; 
	      if (i<=stop) xr_org[gr][ch][i0]*=cos((PI/2)*(i-start)/(stop-start));
	      else xr_org[gr][ch][i0] = 0;
	    }
	  }
	}else{
	  start = gf.lowpass1*576;
	  stop  = gf.lowpass2*576;
	  for ( i = ceil(start) ; i < 576; i++ )
	    if (i<=stop) xr_org[gr][ch][i] *=  cos((PI/2)*(i-start)/(stop-start));
	    else xr_org[gr][ch][i]=0;
	}
      }
    }
  }
  if (gf.highpass2>0) {
    FLOAT start,stop;
    for ( gr = 0; gr < gf.mode_gr; gr++ ) {
      for (ch =0 ; ch < stereo ; ch++) {
	int shortblock = (l3_side->gr[gr].ch[ch].tt.block_type==SHORT_TYPE);
	if (shortblock) {
	  int j;
	  start = gf.highpass1*192;
	  stop  = gf.highpass2*192;
	  for (j=0; j<3; j++) {
	    for ( i = 0;  i < stop; i++ ) {
	      int i0 = 3*i+j; 
	      if (i>=start) xr_org[gr][ch][i0]*=cos((PI/2)*(stop-i)/(stop-start));
	      else xr_org[gr][ch][i0] = 0;
	    }
	  }
	}else{
	  start = gf.highpass1*576;
	  stop  = gf.highpass2*576;
	  for ( i = 0 ; i < stop; i++ )
	    if (i>=start) xr_org[gr][ch][i] *=  cos((PI/2)*(stop-i)/(stop-start));
	    else xr_org[gr][ch][i]=0;
	}
      }
    }
  }

  if (gf.sfb21) {
    for ( gr = 0; gr < gf.mode_gr; gr++ ) {
      for (ch =0 ; ch < stereo ; ch++) {
	int shortblock = (l3_side->gr[gr].ch[ch].tt.block_type==SHORT_TYPE);
	if (shortblock) {
	  int j;
	  for (j=0; j<3; j++) {
	    int start = scalefac_band_short[ SBPSY_s ];
	    for ( i = start; i < 192; i++ ) {
	      int i0 = 3*i+j; 
	      xr_org[gr][ch][i0]=0;
	    }
	  }
	}else{
	  int start = scalefac_band_long[ SBPSY_l ];
	  for ( i = start; i < 576; i++ ) xr_org[gr][ch][i]=0;
	}
      }
    }
  }
}


