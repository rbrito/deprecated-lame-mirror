/* MPGLIB replacement using mpega.library (AmigaOS)
 * Written by Thomas Wenzel and Sigbjørn (CISC) Skjæret.
 *
 * Big thanks to Stéphane Tavernard for mpega.library.
 *
 */

#ifdef AMIGA_MPEGA

#define __USE_SYSBASE
#include "lame.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>

/* We need a small workaround here so GCC doesn't fail upon redefinition. :P */
#define FLOAT _FLOAT
#include <proto/exec.h>
#include <proto/mpega.h>
#undef _FLOAT

#ifndef __GNUC__
#include <dos.h>
#endif

struct Library  *MPEGABase=NULL;
MPEGA_STREAM    *mstream=NULL;
MPEGA_CTRL      mctrl;


#ifndef __GNUC__
static int break_cleanup(void)
{
	/* Dummy break function to make atexit() work. :P */
	return 1;
}
#endif

static void exit_cleanup(void)
{
	if(mstream) {
		MPEGA_close(mstream);
		mstream = NULL;
	}
	if(MPEGABase) {
		CloseLibrary(MPEGABase);
		MPEGABase = NULL;
	}
}


int lame_decode_initfile(const char *fullname, mp3data_struct *mp3data)
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

	MPEGABase = OpenLibrary("mpega.library", 2);
	if(!MPEGABase) {
		fprintf(stderr, "Unable to open mpega.library v2\n");
		exit(1);
	}
#ifndef __GNUC__
	onbreak(break_cleanup);
#endif
	atexit(exit_cleanup);

	mstream=MPEGA_open((char *)fullname, &mctrl);
	if(!mstream) { return (-1); }

	mp3data->channels   = mstream->dec_channels;
	mp3data->samplerate = mstream->dec_frequency;
	mp3data->bitrate    = mstream->bitrate;
	mp3data->nsamp      = (FLOAT)mstream->ms_duration/1000 * mstream->dec_frequency;

	return 0;
}

int lame_decode_fromfile(FILE *fd, sample_t pcm_l[], sample_t pcm_r[], mp3data_struct *mp3data)
{
	int outsize=0;
	WORD *b[MPEGA_MAX_CHANNELS];

	b[0] = pcm_l;
	b[1] = pcm_r;

	while ((outsize == 0) || (outsize == MPEGA_ERR_BADFRAME))	/* Skip bad frames */
		outsize = MPEGA_decode_frame(mstream, b);

	mp3data->channels   = mstream->dec_channels;
	mp3data->samplerate = mstream->dec_frequency;
	mp3data->bitrate    = mstream->bitrate;
	mp3data->nsamp      = (FLOAT)mstream->ms_duration/1000 * mstream->dec_frequency;

	if (outsize < 0) { return (-1); }
	else { return outsize; }
}

#endif /* AMIGA_MPEGA */
