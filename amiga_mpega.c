/* MPGLIB replacement using mpega.library (AmigaOS)
 * Written by Thomas Wenzel and Sigbjørn (CISC) Skjæret.
 *
 * Big thanks to Stéphane Tavernard for mpega.library.
 *
 *
 * $Id$
 *
 * $Log$
 * Revision 1.4  2000/02/22 00:54:40  cisc
 * Oh, nevermind, wasn't very efficient here, proper place would be in sndfile.lib
 *
 * Revision 1.3  2000/02/21 23:58:07  cisc
 * Make sure input-file is closed when CTRL-C is received.
 *
 * Revision 1.2  2000/01/11 20:23:41  cisc
 * Fixed timestatus bug (forgot nsamp variable).
 *
 * Revision 1.1  2000/01/10 03:40:20  cisc
 * MPGLIB replacement using mpega.library (AmigaOS)
 *
 */

#ifdef AMIGA_MPEGA

#include "lame.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <proto/exec.h>
#include <dos.h>
#include <proto/mpega.h>

struct Library  *MPEGABase;
MPEGA_STREAM    *mstream=NULL;
MPEGA_CTRL      mctrl;


static int break_cleanup(void)
{
	if(mstream) {
		MPEGA_close(mstream);
		mstream = NULL;
	}
	if(MPEGABase) {
		CloseLibrary(MPEGABase);
		MPEGABase = NULL;
	}
	return 1;
}

static void exit_cleanup(void)
{
	(void)break_cleanup();
}


int lame_decode_initfile(const char *fullname, int *stereo, int *samp, int *bitrate, unsigned long *nsamp)
{
	mctrl.bs_access = NULL;

	mctrl.layer_1_2.mono.quality    = 2;
	mctrl.layer_1_2.stereo.quality  = 2;
	mctrl.layer_1_2.mono.freq_div   = 1;
	mctrl.layer_1_2.stereo.freq_div = 1;
	mctrl.layer_1_2.mono.freq_max   = 48000;
	mctrl.layer_1_2.stereo.freq_max = 48000;
	mctrl.layer_3.mono.quality      = 2;
	mctrl.layer_3.stereo.quality    = 2;
	mctrl.layer_3.mono.freq_div     = 1;
	mctrl.layer_3.stereo.freq_div   = 1;
	mctrl.layer_3.mono.freq_max     = 48000;
	mctrl.layer_3.stereo.freq_max   = 48000;
	mctrl.layer_1_2.force_mono      = 0;
	mctrl.layer_3.force_mono        = 0;

	onbreak(break_cleanup);
	atexit(exit_cleanup);

	MPEGABase = OpenLibrary("mpega.library", 2);
	if(!MPEGABase) {
		fprintf(stderr, "Unable to open mpega.library v2\n");
		exit(1);
	}

	mstream=MPEGA_open(fullname, &mctrl);
	if(!mstream) { return (-1); }

	*stereo  = mstream->dec_channels;
	*samp    = mstream->dec_frequency;
	*bitrate = mstream->bitrate;
//	*nsamp   = MAX_U_32_NUM;
	*nsamp   = (mstream->ms_duration * mstream->dec_frequency) / 1000;

	return 0;
}

int lame_decode_fromfile(FILE *fd, short pcm[][1152])
{
	int outsize=0;
	WORD *b[MPEGA_MAX_CHANNELS];

	b[0]=&pcm[0][0];
	b[1]=&pcm[1][0];

	outsize = MPEGA_decode_frame(mstream, b);

	if (outsize < 0) { return (-1); }
	else { return outsize; }
}

#endif /* AMIGA_MPEGA */
