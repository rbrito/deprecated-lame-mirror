/*
 *      Command line frontend program
 *
 *      Copyright (c) 1999 Mark Taylor
 *                    2000 Takehiro TOMIANGA
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* $Id$ */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <assert.h>
#include <stdio.h>

#ifdef STDC_HEADERS
# include <stdlib.h>
# include <string.h>
#endif

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#if defined(__riscos__)
# include <kernel.h>
# include <sys/swis.h>
#elif defined(_WIN32)
# include <windows.h>
#endif


/*
 main.c is example code for how to use libmp3lame.a.  To use this library,
 you only need the library and lame.h.  All other .h files are private
 to the library.
*/
#include "lame.h"

#include "brhist.h"
#include "parse.h"
#include "main.h"
#include "get_audio.h"
#include "timestatus.h"

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif




static int
parse_args_from_string(lame_t gfp, const char *p, char *inPath, char *outPath)
{                       /* Quick & very Dirty */
    char   *q;
    char   *f;
    char   *r[128];
    int     c = 0;
    int     ret;

    if (p == NULL || *p == '\0')
        return 0;

    f = q = malloc(strlen(p) + 1);
    strcpy(q, p);

    r[c++] = "lhama";
    while (1) {
        r[c++] = q;
	while (*q != ' ' && *q != '\0')
	    q++;
	if (*q == '\0')
	    break;
	*q++ = '\0';
    }
    r[c] = NULL;

    ret = parse_args(gfp, c, r, inPath, outPath, NULL, NULL);
    free(f);
    return ret;
}





static FILE   *
init_files(lame_t gfp, char *inPath, char *outPath)
{
    FILE   *outf;
    /* Mostly it is not useful to use the same input and output name.
       This test is very easy and buggy and don't recognize different names
       assigning the same file
     */
    if (0 != strcmp("-", outPath) && 0 == strcmp(inPath, outPath)) {
        fprintf(stderr, "Input file and Output file are the same. Abort.\n");
        return NULL;
    }

    /* open the wav/aiff/raw pcm or mp3 input file.  This call will
     * open the file, try to parse the headers and set the internal 
     * encoding flags in gfp.
     * if you want to do your own file input, skip this call and set
     * samplerate, num_channels and num_samples yourself.
     */
    init_infile(gfp, inPath);
    if (!(outf = init_outfile(outPath))) {
        fprintf(stderr, "Can't init outfile '%s'\n", outPath);
        return NULL;
    }
#ifdef __riscos__
    {
	char   *p;
        /* Assign correct file type */
        for (p = outPath; *p; p++) /* ugly, ugly to modify a string */
            switch (*p) {
            case '.':
                *p = '/';
                break;
            case '/':
                *p = '.';
                break;
            }
        SetFiletype(outPath, decode_only ? 0xFB1 /*WAV*/ : 0x1AD /*AMPEG*/);
    }
#endif

    return outf;
}





static void
WriteShort(FILE * fp, float s)
{
    union {
	short s;
	char c[2];
    } sc;
    int i;

    s *= (1.0/65536);
    if (s > 0.0) {
	i = (int)(s+0.5);
	if (i > 32767)
	    i = 32767;
    } else {
	i = -(int)(-s+0.5);
	if (i < -32768)
	    i = -32768;
    }
    sc.s = i;
    if (outputPCMendian != order_nativeEndian) {
	int l = sc.c[0];
	sc.c[0] = sc.c[1];
	sc.c[1] = l;
    }
    fwrite(&sc, 1, sizeof(short), fp );
}


/* the simple lame decoder */
/* After calling lame_init(), lame_init_params() and
 * init_infile(), call this routine to read the input MP3 file
 * and output .wav data to the specified file pointer*/
/* decoder will ignore the first 528 samples, since these samples
 * represent the mpglib delay (and are all 0).  skip = number of additional
 * samples to skip, to (for example) compensate for the encoder delay */

static int
decoder(lame_t gfp, FILE * outf, int skip, char *inPath, char *outPath)
{
    double  wavsize;
    float Buffer[2][1152];
    int iread;
    int i;
    int tmp_num_channels = lame_get_num_channels( gfp );

    if (silent < 10)
	fprintf(stderr, "\rinput:  %s%s(%g kHz, %i channel%s, ",
		strcmp(inPath, "-") ? inPath : "<stdin>",
		strlen(inPath) > 26 ? "\n\t" : "  ",
		lame_get_in_samplerate( gfp ) / 1.e3,
		tmp_num_channels, tmp_num_channels > 1 ? "s":"");

    switch (input_format) {
    case sf_mp3:
        if (skip==0) {
            if (enc_delay>-1) skip = enc_delay;
	    else skip=lame_get_encoder_delay(gfp);
        }
	/* mp3 decoder has a 528 sample delay */
	skip += 528+1;

        if (silent < 10) fprintf(stderr, "MPEG-%u%s Layer %s", 2 - lame_get_version(gfp),
                lame_get_out_samplerate( gfp ) < 16000 ? ".5" : "", "III");
        break;
    case sf_mp2:
        skip += 240 + 1;
        if (silent < 10) fprintf(stderr, "MPEG-%u%s Layer %s", 2 - lame_get_version(gfp),
                lame_get_out_samplerate( gfp ) < 16000 ? ".5" : "", "II");
        break;
    case sf_mp1:
        skip += 240 + 1;
        if (silent < 10) fprintf(stderr, "MPEG-%u%s Layer %s", 2 - lame_get_version(gfp),
                lame_get_out_samplerate( gfp ) < 16000 ? ".5" : "", "I");
        break;
    default:
	if (silent < 10) fprintf(stderr, "not MPEG file and cannot decode.");
	return 1;
    }

    if (silent < 10) {
	fprintf(stderr, ")\noutput: %s%s(16 bit, Microsoft WAVE)\n",
		strcmp(outPath, "-") ? outPath : "<stdout>",
		strlen(outPath) > 45 ? "\n\t" : "  ");

	if (skip > 0)
	    fprintf(stderr,
		    "skipping initial %i samples (encoder+decoder delay)\n",
		    skip);
    }
    /* at this time, size is unknown. so write maximum 32 bit signed value */
    if (!disable_wav_header)
	WriteWaveHeader(outf, 0x7FFFFFFF, lame_get_in_samplerate( gfp ),
			tmp_num_channels, 16);

    wavsize = -skip;
    mp3input_data.totalframes = mp3input_data.nsamp / mp3input_data.framesize;

    assert(1 <= tmp_num_channels && tmp_num_channels <= 2);

    do {
	iread = get_audio(gfp, Buffer); /* read in 'iread' samples */
	mp3input_data.framenum += iread / mp3input_data.framesize;
	wavsize += iread;

	if (silent <= 0)
	    decoder_progress(&mp3input_data);

	i = skip;
	if (i > iread)
	    i = iread;
	skip -= i;
	for (; i < iread; i++) {
	    WriteShort(outf, Buffer[0][i]);
	    if (tmp_num_channels == 2)
		WriteShort(outf, Buffer[1][i]);
	}
    } while (iread);

    i = (16 / 8) * tmp_num_channels;
    assert(i > 0);
    if (wavsize <= 0) {
	if (silent < 10) fprintf(stderr, "WAVE file contains 0 PCM samples\n");
	wavsize = 0;
    }
    else if (wavsize > 0xFFFFFFD0 / i) {
	if (silent < 10)
	    fprintf(stderr,
		    "Very huge WAVE file, can't set filesize accordingly\n");
	wavsize = 0xFFFFFFD0;
    }
    else {
	wavsize *= i;
    }

    if (!disable_wav_header && !fseek(outf, 0l, SEEK_SET))
	WriteWaveHeader(outf, wavsize, lame_get_in_samplerate( gfp ),
			tmp_num_channels, 16);
    fclose(outf);

    if (silent <= 0)
	decoder_progress_finish();
    return 0;
}


static int
encoder(lame_t gfp, FILE * outf, int nogap, char *inPath, char *outPath)
{
    unsigned char mp3buffer[LAME_MAXMP3BUFFER];
    float   Buffer[2][1152];
    int     iread, imp3;
    static const char *mode_names[2][4] = {
        {"stereo", "j-stereo", "dual-ch", "single-ch"},
        {"stereo", "force-ms", "dual-ch", "single-ch"}
    };
    int     frames;
    const char *appendix = "";

    if (silent < 10) {
        lame_print_config(gfp); /* print useful information about options being used */

        fprintf(stderr, "Encoding %s%s to %s\n",
                strcmp(inPath, "-") ? inPath : "<stdin>",
                strlen(inPath) + strlen(outPath) < 66 ? "" : "\n     ",
                strcmp(outPath, "-") ? outPath : "<stdout>");

        fprintf(stderr,
                "Encoding as %g kHz ", 1.e-3 * lame_get_out_samplerate(gfp));

	switch (lame_get_VBR(gfp)) {
	case vbr:
	    appendix = "ca. ";
	    fprintf(stderr, "VBR(q=%i)", lame_get_VBR_q(gfp));
	    break;
	case vbr_abr:
	    fprintf(stderr, "average %d kbps",
		    lame_get_VBR_mean_bitrate_kbps(gfp));
	    break;
	default:
	    fprintf(stderr, "%3d kbps", lame_get_brate(gfp));
	    break;
	}
	fprintf(stderr, " %s MPEG-%u%s Layer III (%s%gx) qval=%i\n",
		mode_names[lame_get_force_ms(gfp)][lame_get_mode(gfp)],
		2 - lame_get_version(gfp),
		lame_get_out_samplerate(gfp) < 16000 ? ".5" : "",
		appendix,
		0.1 * (int) (10. * lame_get_compression_ratio(gfp) + 0.5),
		lame_get_quality(gfp));
    }

    if (silent <= -10)
	lame_print_internals(gfp);

    fflush(stderr);
    if (input_format == sf_mp3 && keeptag && id3v2taglen)
	fseek(outf, id3v2taglen, SEEK_CUR);

    /* encode until we hit eof */
    do {
        /* read in 'iread' samples */
        iread = get_audio(gfp, Buffer);

	/********************** status display  *****************************/
	if (silent <= 0) {
	    timestatus_klemm(gfp);
        }

        /* encode */
        imp3 = lame_encode_buffer_float2(gfp, Buffer[0], Buffer[1], iread,
					 mp3buffer, sizeof(mp3buffer));

        /* was our output buffer big enough? */
        if (imp3 < 0) {
            if (imp3 == -1)
                fprintf(stderr, "mp3 buffer is not big enough... \n");
            else
                fprintf(stderr, "mp3 internal error:  error code=%i\n", imp3);
            return 1;
        }

        if (fwrite(mp3buffer, 1, imp3, outf) != (unsigned int)imp3) {
            fprintf(stderr, "Error writing mp3 output  %d\n", imp3);
            return 1;
        }
    } while (iread);

    if (nogap)
        imp3 = lame_encode_flush_nogap(gfp, mp3buffer, sizeof(mp3buffer)); /* may return one more mp3 frame */
    else
        imp3 = lame_encode_flush(gfp, mp3buffer, sizeof(mp3buffer)); /* may return one more mp3 frame */

    if (imp3 < 0) {
        if (imp3 == -1)
            fprintf(stderr, "mp3 buffer is not big enough... \n");
        else
            fprintf(stderr, "mp3 internal error:  error code=%i\n", imp3);
        return 1;

    }

    if (silent <= 0) {
#ifdef BRHIST
	brhist_jump_back();
#endif
        frames = lame_get_frameNum(gfp);
	timestatus(lame_get_out_samplerate(gfp),
		   frames, lame_get_totalframes(gfp), lame_get_framesize(gfp));
#ifdef BRHIST
	brhist_disp(gfp);
	brhist_disp_total(gfp);
#endif
	timestatus_finish();
    }

    fwrite(mp3buffer, 1, imp3, outf);

    /* id3 tag */
    if (input_format == sf_mp3 && keeptag) {
#define ID3TAGSIZE 128
	char id3tag[ID3TAGSIZE];
	fseek(g_inputHandler, -ID3TAGSIZE, SEEK_CUR);
	fread(id3tag, 1, ID3TAGSIZE, g_inputHandler);
	if (memcmp(id3tag, "TAG", 3) == 0)
	    fwrite(id3tag, 1, ID3TAGSIZE, outf);

	if (id3v2taglen) {
	    char *id3v2tag = malloc(id3v2taglen);
	    if (id3v2tag) {
		fseek(outf, 0, SEEK_SET);
		fseek(g_inputHandler, 0, SEEK_SET);
		fread(id3v2tag, 1, id3v2taglen, g_inputHandler);
		fwrite(id3v2tag, 1, id3v2taglen, outf);
		free(id3v2tag);
	    } else {
		fprintf(stderr, "Oops, too big ID3v2 Tag to copy...\n");
	    }
	}
    }
    return 0;
}






static void
brhist_init_package(lame_t gfp)
{
#ifdef BRHIST
    if (disp_brhist) {
        if (brhist_init
            (gfp, lame_get_VBR_min_bitrate_kbps(gfp),
             lame_get_VBR_max_bitrate_kbps(gfp))) {
            /* fail to initialize */
            disp_brhist = 0;
        }
    }
    else {
        brhist_init(gfp, 128, 128); /* Dirty hack */
    }
#endif
}




static void
parse_nogap_filenames(int nogapout, char *inPath, char *outPath, char *outdir)
{
    char    *slasher;
    int     n;

    strcpy(outPath,outdir);
    if (!nogapout) 	{
        strncpy(outPath, inPath, PATH_MAX + 1 - 4);
    } else {
        slasher = inPath;
        slasher += PATH_MAX + 1 - 4;

        /* backseek to last dir delemiter */
        while (*slasher != '/' && *slasher != '\\'
	       && slasher != inPath && *slasher != ':')
	{
	    slasher--;
	}

        /* skip one foward if needed */
        if (slasher != inPath 
            && (outPath[strlen(outPath)-1] == '/'
                || outPath[strlen(outPath)-1] == '\\'
                || outPath[strlen(outPath)-1] == ':'))
	    slasher++;
        else if (slasher == inPath
                 && (outPath[strlen(outPath)-1] != '/'
                     && outPath[strlen(outPath)-1] != '\\'
                     && outPath[strlen(outPath)-1] != ':'))
#ifdef _WIN32
	    strcat(outPath, "\\");
#elif __OS2__
        strcat(outPath, "\\");
#else
        strcat(outPath, "/");
#endif
	strncat(outPath, slasher, PATH_MAX + 1 - 4);
    }
    n=strlen(outPath);
    /* nuke old extension  */
    if (n > 4
	&& (!strcmp(&outPath[n-4], ".wav") || strcmp(&outPath[n-4], ".raw")))
	n -= 4;

    outPath[n+0] = '.';
    outPath[n+1] = 'm';
    outPath[n+2] = 'p';
    outPath[n+3] = '3';
    outPath[n+4] = 0;
}

void print_trailing_info(lame_t gf)
{
    int RadioGain;

    if (lame_get_bWriteVbrTag(gf))
	printf("done\n");

    if (!lame_get_findReplayGain(gf))
	return;

    RadioGain = lame_get_RadioGain(gf);
    printf("ReplayGain: %s%.1fdB\n", RadioGain > 0 ? "+" : "", ((float)RadioGain) / 10.0);
    if (RadioGain > 0x1FE || RadioGain < -0x1FE) 
	printf("WARNING: ReplayGain exceeds the -51dB to +51dB range. Such a result is too\n"
	       "         high to be stored in the header.\n" );

    /* if (the user requested printing info about clipping) and (decoding
       on the fly has actually been performed) */
    if (print_clipping_info) {
	float noclipGainChange = (float)lame_get_noclipGainChange(gf) / 10.0;
	float noclipScale = lame_get_noclipScale(gf);

	if (noclipGainChange > 0.0) { /* clipping occurs */
	    printf("WARNING: clipping occurs at the current gain. Set your decoder to decrease\n"
		   "         the  gain  by  at least %.1fdB or encode again ", noclipGainChange);

	    /* advice the user on the scale factor */
	    if (noclipScale > 0) {
		printf("using  --scale %.2f\n", noclipScale);
		printf("         or less (the value under --scale is approximate).\n" );
	    }
	    else {
		/* the user specified his own scale factor. We could suggest 
		 * the scale factor of (32767.0/gfp->PeakSample)*(gfp->scale)
		 * but it's usually very inaccurate. So we'd rather advice him to 
		 * disable scaling first and see our suggestion on the scale factor then. */
		printf("using --scale <arg>\n"
		       "         (For   a   suggestion  on  the  optimal  value  of  <arg>  encode\n"
		       "         with  --scale 1  first)\n" );
	    }
	}
	else { /* no clipping */
	    if (noclipGainChange > -0.1)
		printf("\nThe waveform does not clip and is less than 0.1dB away from full scale.\n" );
	    else
		printf("\nThe waveform does not clip and is at least %.1fdB away from full scale.\n", -noclipGainChange);
	}
    }
}


int
main(int argc, char **argv)
{
    int     ret;
    lame_t  gfp;
    char    outPath[PATH_MAX + 1];
    char    nogapdir[PATH_MAX + 1];
    char    inPath[PATH_MAX + 1] = {0};

    /* support for "nogap" encoding of up to 200 .wav files */
#define MAX_NOGAP 200
    int    nogapout = 0;
    int     max_nogap = MAX_NOGAP;
    char   *nogap_inPath[MAX_NOGAP];

    int     i;
    FILE   *outf;

#if macintosh
    argc = ccommand(&argv);
#endif

#ifdef __EMX__
    /* This gives wildcard expansion on Non-POSIX shells with OS/2 */
    _wildcard(&argc, &argv);
#endif

    for (i = 0; i < max_nogap; ++i) {
        nogap_inPath[i] = malloc(PATH_MAX + 1);
    }

    /* initialize libmp3lame */
    input_format = sf_unknown;
    if (!(gfp = lame_init())) {
        fprintf(stderr, "fatal error during initialization\n");
        return 1;
    }
    if (argc <= 1) {
        usage(stderr, argv[0]); /* no command-line args, print usage, exit  */
        return 1;
    }

    /* parse the command line arguments, setting various flags in the
     * encoding flags in 'gfp'.  If you want to parse your own arguments,
     * or call libmp3lame from a program which uses a GUI to set arguments,
     * skip this call and set the values of interest in the gfp struct.
     * (see the file API and lame.h for documentation about these parameters)
     */
    parse_args_from_string(gfp, getenv("LAMEOPT"), inPath, outPath);
    ret = parse_args(gfp, argc, argv, inPath, outPath, nogap_inPath, &max_nogap);
    if (silent <= 0)
	print_version (stdout);

    if (ret < 0)
        return ret == -2 ? 0 : 1;

    if (update_interval <= 0.)
	update_interval = .5;

    if (outPath[0] != '\0' && max_nogap>0) {
	strncpy(nogapdir, outPath, PATH_MAX + 1);
	nogapout = 1;
    }

    /* initialize input file.  This also sets samplerate and as much
       other data on the input file as available in the headers */
    if (max_nogap > 0) {
        /* for nogap encoding of multiple input files, it is not possible to
         * specify the output file name, only an optional output directory. */
        parse_nogap_filenames(nogapout,nogap_inPath[0],outPath,nogapdir);
        outf = init_files(gfp, nogap_inPath[0], outPath);
    }
    else {
        outf = init_files(gfp, inPath, outPath);
    }
    if (outf == NULL) {
        return -1;
    }

    /* Now that all the options are set, lame needs to analyze them and
     * set some more internal options and check for problems
     */
    i = lame_init_params(gfp);
    if (i < 0) {
        if (i == -1) {
            display_bitrates(stderr);
        }
        fprintf(stderr, "fatal error during initialization\n");
        return i;
    }

    if (silent > 0 || lame_get_VBR(gfp) == vbr_off) {
        disp_brhist = 0;     /* turn off VBR histogram */
    }


    if (decode_only) {
        /* decode an mp3 file to a .wav */
        if (mp3_delay_set)
            decoder(gfp, outf, mp3_delay, inPath, outPath);
        else
            decoder(gfp, outf, 0, inPath, outPath);
    }
    else {
        if (max_nogap > 0) {
            /*
             * encode multiple input files using nogap option
             */
            for (i = 0; i < max_nogap; ++i) {
                int     use_flush_nogap = (i != (max_nogap - 1));
                if (i > 0) {
                    parse_nogap_filenames(nogapout,nogap_inPath[i],outPath,nogapdir);
                    /* note: if init_files changes anything, like
                       samplerate, num_channels, etc, we are screwed */
                    outf = init_files(gfp, nogap_inPath[i], outPath);
                }
                brhist_init_package(gfp);
                ret = encoder(gfp, outf, use_flush_nogap, nogap_inPath[i],
			      outPath);	
                
		if (silent<=0 && lame_get_bWriteVbrTag(gfp))
		    printf("Writing LAME Tag...");
		lame_mp3_tags_fid(gfp, outf); /* add VBR tags to mp3 file */

                if (silent<=0) print_trailing_info(gfp);

                fclose(outf); /* close the output file */
                close_infile(); /* close the input file */
                /* reinitialize bitstream for next encoding.  this is normally done
                 * by lame_init_params(), but we cannot call that routine twice */
                if (use_flush_nogap) 
                    lame_init_bitstream(gfp);
            }
            lame_close(gfp);
        }
        else {
            /*
             * encode a single input file
             */
            brhist_init_package(gfp);
            ret = encoder(gfp, outf, 0, inPath, outPath);
            
	    if (silent<=0 && lame_get_bWriteVbrTag(gfp))
		printf("Writing LAME Tag...");
            lame_mp3_tags_fid(gfp, outf); /* add VBR tags to mp3 file */
	    if (silent<=0) print_trailing_info(gfp);
            
            fclose(outf); /* close the output file */
            close_infile(); /* close the input file */
            lame_close(gfp);
        }
    }
    return ret;
}
