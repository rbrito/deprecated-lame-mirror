#include "util.h"
#include "quantize-pvt.h"
#include "globalflags.h"



/************************************************************************
 *  lowpass filter on MDCT coefficients
 ************************************************************************/
void filterMDCT( FLOAT8 xr_org[2][2][576], III_side_info_t *l3_side)
{
  int gr,ch,i;

  /* will will soon replace with polyphase fitler */
  if (gf.sfb21) {


    for ( gr = 0; gr < gf.mode_gr; gr++ ) {
      for (ch =0 ; ch < gf.stereo ; ch++) {
	int shortblock = (l3_side->gr[gr].ch[ch].tt.block_type==SHORT_TYPE);
	if (shortblock) {
	  int j;
	  for (j=0; j<3; j++) {
	    int start = scalefac_band.s[ SBPSY_s ];
	    for ( i = start; i < 192; i++ ) {
	      int i0 = 3*i+j; 
	      xr_org[gr][ch][i0]=0;
	    }
	  }
	}else{
	  int start = scalefac_band.l[ SBPSY_l ];
	  for ( i = start; i < 576; i++ ) xr_org[gr][ch][i]=0;
	}
      }
    }
  }
}


