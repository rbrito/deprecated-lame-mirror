/**********************************************************************
 * ISO MPEG Audio Subgroup Software Simulation Group (1996)
 * ISO 13818-3 MPEG-2 Audio Encoder - Lower Sampling Frequency Extension
 *
 **********************************************************************/
/*
  Revision History:

  Date        Programmer                Comment
  ==========  ========================= ===============================
  1995/09/06  mc@fivebats.com           created

*/
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include "util.h"
#include "gtkanal.h"
#include "lame.h"


/*
  ResvFrameBegin:
  Called (repeatedly) at the beginning of a frame. Updates the maximum
  size of the reservoir, and checks to make sure main_data_begin
  was set properly by the formatter
*/
int
ResvFrameBegin(lame_global_flags *gfp,III_side_info_t *l3_side, int mean_bits, int frameLength )
{
    lame_internal_flags *gfc=gfp->internal_flags;
    int fullFrameBits;
    int resvLimit;


    /* main_data_begin has 9 bits in MPEG 1, 8 bits MPEG2 */
    resvLimit = (gfp->version==1) ? 4088 : 2040 ;


    /*
      determine maximum size of reservoir:
      ResvMax + frameLength <= MAXMP3BUF;
    */
#define MAXMP3BUF 7680
    if ( frameLength > MAXMP3BUF )
	gfc->ResvMax = 0;
    else
	gfc->ResvMax = MAXMP3BUF - frameLength;
    if (gfp->disable_reservoir) gfc->ResvMax=0;
    if ( gfc->ResvMax > resvLimit ) gfc->ResvMax = resvLimit;

    l3_side->resvDrain_pre = 0;

    if (gfc->pinfo != NULL){
      plotting_data *pinfo=gfc->pinfo;
      pinfo->mean_bits=mean_bits/2;  /* expected bits per channel per granule */
      pinfo->resvsize=gfc->ResvSize;
    }
    
    fullFrameBits = mean_bits * gfc->mode_gr + Min(gfc->ResvSize,gfc->ResvMax);
    return fullFrameBits;
}


/*
  ResvMaxBits2:
  As above, but now it *really* is bits per granule (both channels).  
  Mark Taylor 4/99
*/
void ResvMaxBits(lame_global_flags *gfp, int mean_bits, int *targ_bits, int *extra_bits)
{
  lame_internal_flags *gfc=gfp->internal_flags;
  int add_bits,full_fac;
  *targ_bits = mean_bits ;


  /* extra bits if the reservoir is almost full */
  full_fac=9;
  if (gfc->ResvSize > ((gfc->ResvMax * full_fac) / 10)) {
    add_bits= gfc->ResvSize-((gfc->ResvMax * full_fac) / 10);
    *targ_bits += add_bits;
  }else {
    add_bits =0 ;
    /* build up reservoir.  this builds the reservoir a little slower
     * than FhG.  It could simple be mean_bits/15, but this was rigged
     * to always produce 100 (the old value) at 128kbs */
    /*    *targ_bits -= (int) (mean_bits/15.2);*/
    *targ_bits -= .1*mean_bits;
  }

  
  /* amount from the reservoir we are allowed to use. ISO says 6/10 */
  *extra_bits =    
    (gfc->ResvSize  < (gfc->ResvMax*6)/10  ? gfc->ResvSize : (gfc->ResvMax*6)/10);
  *extra_bits -= add_bits;
  
  if (*extra_bits < 0) *extra_bits=0;

  
}

/*
  ResvAdjust:
  Called after a granule's bit allocation. Readjusts the size of
  the reservoir to reflect the granule's usage.
*/
void
ResvAdjust(lame_global_flags *gfp,gr_info *gi, III_side_info_t *l3_side, int mean_bits )
{
  lame_internal_flags *gfc=gfp->internal_flags;
  gfc->ResvSize += (mean_bits / gfc->stereo) - gi->part2_3_length;
}


/*
  ResvFrameEnd:
  Called after all granules in a frame have been allocated. Makes sure
  that the reservoir size is within limits, possibly by adding stuffing
  bits. 
*/
void
ResvFrameEnd(lame_global_flags *gfp,III_side_info_t *l3_side, int mean_bits)
{
    int stuffingBits;
    int over_bits,mdb_bytes;
    lame_internal_flags *gfc=gfp->internal_flags;


    /* just in case mean_bits is odd, this is necessary... */
    if ( gfc->stereo == 2 && mean_bits & 1)
	gfc->ResvSize += 1;

    stuffingBits=0;
    l3_side->resvDrain_post = 0;
    l3_side->resvDrain_pre = 0;


#if 0
    if (gfc->frameNum==0 && gfc->ResvSize >= 32) {
      /* if it is the first frame, try to put 9 bytes into ancillary data */
      /* bitstream.c will use these for LAME version string */
      stuffingBits += Min(64,gfc->ResvSize);
      gfc->ResvSize -= stuffingBits;
      l3_side->resvDrain_post += stuffingBits;
    }
#endif



    over_bits = gfc->ResvSize - gfc->ResvMax;
    if ( over_bits < 0 )
	over_bits = 0;
    stuffingBits += over_bits;

    /* we must be byte aligned */
    if ( (over_bits = (gfc->ResvSize-over_bits) % 8) )
	stuffingBits += over_bits;

    /*#define NEW_DRAIN*/
#ifdef NEW_DRAIN
    /* drain as many bits as possible into previous frame ancillary data
     * In particular, in VBR mode ResvMax may have changed, and we have
     * to make sure main_data_begin does not create a reservoir bigger 
     * than ResvMax  mt 4/00*/
    mdb_bytes = Min(l3_side->main_data_begin*8,stuffingBits)/8;
    l3_side->resvDrain_pre += 8*mdb_bytes;
    stuffingBits -= 8*mdb_bytes;
    gfc->ResvSize -= 8*mdb_bytes;
    
    if (gfp->VBR && mdb_bytes && !gfp->disable_reservoir) {
      printf("**** informative message:  drain_pre: wasting bits=%i\n",8*mdb_bytes);
    }
#endif

#ifdef NEW_DRAIN
    /* drain enough to be byte aligned.  The remaining bits will
     * be added to the reservoir, and we will deal with them next frame
     * If the next frame
     * is at a lower bitrate, it may have a larger ResvMax, and
     * we will not have to waste these bits!  mt 4/00 */
    l3_side->resvDrain_post += (stuffingBits % 8);
    gfc->ResvSize -= stuffingBits % 8;
#else
    /* drain the rest into this frames ancillary data*/
    l3_side->resvDrain_post += stuffingBits;
    gfc->ResvSize -= stuffingBits;
#endif


    if (gfp->VBR && stuffingBits>7 && !gfp->disable_reservoir) {
      printf("**** informative message: drain_post: wasting bits=%i\n",stuffingBits);
    }
    return;
}


