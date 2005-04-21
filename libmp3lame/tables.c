/*
 *	MPEG layer 3 tables source file
 *
 *	Copyright (c) 1999 Albert L Faber
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

/* $Id$ */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#ifdef WITH_DMALLOC
# include <dmalloc.h>
#endif
#include <assert.h>

#include "encoder.h"
#include "tables.h"
#include "psymodel.h"
#include "quantize_pvt.h"

const int  samplerate_table [3]  [4] = {
    { 22050, 24000, 16000, -1 },      /* MPEG 2 */
    { 44100, 48000, 32000, -1 },      /* MPEG 1 */
    { 11025, 12000,  8000, -1 },      /* MPEG 2.5 */
};

const int  bitrate_table    [3] [16] = {
    { 0,  8, 16, 24, 32, 40, 48, 56,  64,  80,  96, 112, 128, 144, 160, -1 },
    { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, -1 },
    { 0,  8, 16, 24, 32, 40, 48, 56,  64,  80,  96, 112, 128, 144, 160, -1 },
};

/*
  Here are MPEG1 Table B.8 and MPEG2 Table B.1
  -- Layer III scalefactor bands. 
  Index into this using a method such as:
    idx  = fr_ps->header->sampling_frequency + (fr_ps->header->version * 3)
*/

static const scalefac_struct sfBandIndex[9] = {
  { /* Table B.2.b: 22.05 kHz */
    {0,6,12,18,24,30,36,44,54,66,80,96,116,140,168,200,238,284,336,396,464,522,576},
    {0,4,8,12,18,24,32,42,56,74,100,132,174,192}
  },
  { /* Table B.2.c: 24 kHz */                 /* docs: 332. mpg123(broken): 330 */
    {0,6,12,18,24,30,36,44,54,66,80,96,114,136,162,194,232,278, 332, 394,464,540,576},
    {0,4,8,12,18,26,36,48,62,80,104,136,180,192}
  },
  { /* Table B.2.a: 16 kHz */
    {0,6,12,18,24,30,36,44,54,66,80,96,116,140,168,200,238,284,336,396,464,522,576},
    {0,4,8,12,18,26,36,48,62,80,104,134,174,192}
  },
  { /* Table B.8.b: 44.1 kHz */
    {0,4,8,12,16,20,24,30,36,44,52,62,74,90,110,134,162,196,238,288,342,418,576},
    {0,4,8,12,16,22,30,40,52,66,84,106,136,192}
  },
  { /* Table B.8.c: 48 kHz */
    {0,4,8,12,16,20,24,30,36,42,50,60,72,88,106,128,156,190,230,276,330,384,576},
    {0,4,8,12,16,22,28,38,50,64,80,100,126,192}
  },
  { /* Table B.8.a: 32 kHz */
    {0,4,8,12,16,20,24,30,36,44,54,66,82,102,126,156,194,240,296,364,448,550,576},
    {0,4,8,12,16,22,30,42,58,78,104,138,180,192}
  },
  { /* MPEG-2.5 11.025 kHz */
    {0,6,12,18,24,30,36,44,54,66,80,96,116,140,168,200,238,284,336,396,464,522,576},
    {0, 4, 8, 12, 18, 26, 36, 48, 62, 80, 104, 134, 174, 192}
  },
  { /* MPEG-2.5 12 kHz */
    {0,6,12,18,24,30,36,44,54,66,80,96,116,140,168,200,238,284,336,396,464,522,576},
    {0, 4, 8, 12, 18, 26, 36, 48, 62, 80, 104, 134, 174, 192}
  },
  { /* MPEG-2.5 8 kHz */
    {0,12,24,36,48,60,72,88,108,132,160,192,232,280,336,400,476,566,568,570,572,574,576},
    {0, 8, 16, 24, 36, 52, 72, 96, 124, 160, 162, 164, 166, 192}
  }
};



/*
  The following table is used to implement the scalefactor
  partitioning for MPEG2 as described in section
  2.4.3.2 of the IS. The indexing corresponds to the
  way the tables are presented in the IS:

  [table_number*3+row_in_table][column of nr_of_sfb]
*/
const char nr_of_sfb_block [6*3][4] = {
    {6, 5, 5, 5}, /* long/start/stop */
    {9, 9, 9, 9}, /* short */
    {6, 9, 9, 9}  /* mixed */
    ,
    {6, 5,  7, 3}, /* long/start/stop */
    {9, 9, 12, 6}, /* short */
    {6, 9, 12, 6}  /* mixed */
    ,
    {11, 10, 0, 0}, /* long/start/stop, preflag */
    {18, 18, 0, 0}, /* short, preflag ? */
    {15, 18, 0, 0}  /* mixed, preflag ? */
    ,
    { 7,  7,  7, 0}, /* long/start/stop, IS */
    {12, 12, 12, 0}, /* short, IS */
    { 6, 15, 12, 0}  /* mixed, IS */
    ,
    { 6,  6, 6, 3}, /* long/start/stop, IS */
    {12,  9, 9, 6}, /* short, IS */
    { 6, 12, 9, 6}  /* mixed, IS */
    ,
    { 8, 8, 5, 0}, /* long/start/stop, IS  */
    {15,12, 9, 0}, /* short, IS */
    { 6,18, 9, 0}  /* mixed, IS */
};


/* MPEG1 scalefactor bits */
const char s1bits[] = { 0, 0, 0, 0, 3, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4 };
const char s2bits[] = { 0, 1, 2, 3, 0, 1, 2, 3, 1, 2, 3, 1, 2, 3, 2, 3 };

/* Table B.6: layer3 preemphasis */
const char  pretab[SBMAX_l] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,    1, 1, 1, 1, 2, 2, 3, 3, 3, 2, 0
};

const unsigned char quadcode[2][16*2]  = {
    {
	1+0,  4+1,  4+1,  5+2,  4+1,  6+2,  5+2,  6+3,
	4+1,  5+2,  5+2,  6+3,  5+2,  6+3,  6+3,  6+4,

	1 << 0,  5 << 1,  4 << 1,  5 << 2,  6 << 1,  5 << 2,  4 << 2,  4 << 3,
	7 << 1,  3 << 2,  6 << 2,  0 << 3,  7 << 2,  2 << 3,  3 << 3,  1 << 4
    },
    {
	4+0,  4+1,  4+1,  4+2,  4+1,  4+2,  4+2,  4+3,
	4+1,  4+2,  4+2,  4+3,  4+2,  4+3,  4+3,  4+4,

	15 << 0, 14 << 1, 13 << 1, 12 << 2, 11 << 1, 10 << 2,  9 << 2,  8 << 3,
	 7 << 1,  6 << 2,  5 << 2,  4 << 3,  3 << 2,  2 << 3,  1 << 3,  0 << 4
    }
};

const char htESC_xlen[] = {
    1,    2,    3,    4,    6,    8,    10,    13,
    4,    5,    6,    7,    8,    9,    11,    13
};

/* for subband filtering */
const unsigned char mdctorder[] = {
    0,30,15,17, 8,22, 7,25, 4,26,11,21,12,18, 3,29,
    2,28,13,19,10,20, 5,27, 6,24, 9,23,14,16, 1,31
};

/* for fast quantization */
FLOAT pow20[Q_MAX+Q_MAX2];
FLOAT ipow20[Q_MAX+Q_MAX2];

/* initialized in first call to iteration_init */
#ifdef USE_IEEE754_HACK
FLOAT pow43[PRECALC_SIZE*3];
#else
FLOAT pow43[PRECALC_SIZE*2];
#endif

/* psymodel window */
FLOAT window[BLKSIZE], window_s[BLKSIZE_s];

#ifdef USE_FAST_LOG
/***********************************************************************
 *
 * Fast Log Approximation for log2, used to approximate every other log
 * (log10 and log)
 * maximum absolute error for log10 is around 10-6
 * maximum *relative* error can be high when x is almost 1 because error/log10(x) tends toward x/e
 *
 * use it if typical RESULT values are > 1e-5 (for example if x>1.00001 or x<0.99999)
 * or if the relative precision in the domain around 1 is not important (result in 1 is exact and 0)
 *
 ***********************************************************************/

ieee754_float32_t log_table[LOG2_SIZE*2];

static void
init_log_table(void)
{
    int j;
    /* Range for log2(x) over [1,2[ is [0,1[ */
    for (j = 0; j < LOG2_SIZE; j++) {
	double a, b, x;
	x = log(1.0 + (j+0.5)/(double)LOG2_SIZE) / LOG2;
	a = log(1.0 +  j     /(double)LOG2_SIZE) / LOG2;
	b = log(1.0 + (j+1.0)/(double)LOG2_SIZE) / LOG2;

	x = ((a + x) + (x + b)) / 4.0;
	a = (b-a) * (1.0 / (1 << (23 - LOG2_SIZE_L2)));
	log_table[j*2+1] = a;
	log_table[j*2  ] = x - a*(1<<(23-LOG2_SIZE_L2-1))*(j*2+1) - 0x7f;
    }
}
#endif /* define FAST_LOG */


static FLOAT
ATHformula(lame_t gfc ,FLOAT f, int cbflag)
{
    /* from Painter & Spanias
       modified by Gabriel Bouvigne to better fit the reality

       ath =    3.640 * pow(f,-0.8)
         - 6.800 * exp(-0.6*pow(f-3.4,2.0))
         + 6.000 * exp(-0.15*pow(f-8.7,2.0))
         + 0.6* 0.001 * pow(f,4.0);

	In the past LAME was using the Painter &Spanias formula.
	But we had some recurrent problems with HF content.
	We measured real ATH values, and found the older formula
	to be inacurate in the higher part. So we made this new
	formula and this solved most of HF problematic testcases.
	The tradeoff is that in VBR mode it increases a lot the
	bitrate.*/

    f /= 1000;  /* convert to khz */
    f  = Max(0.01, f);
    f  = Min(18.0, f);

    if (cbflag)
	return db2pow(3.640 * pow(f,-0.8)
		      - 6.800 * exp(-0.6*pow(f-3.4,2.0))
		      + 6.000 * exp(-0.15*pow(f-8.7,2.0))
		      + 0.6* 0.001 * pow(f,4.0));
    else
	return db2pow(3.640 * pow(f,-0.8)
		      - 6.800 * exp(-0.6*pow(f-3.4,2.0))
		      + 6.000 * exp(-0.15*pow(f-8.7,2.0))
		      + (0.6+0.04*gfc->ATHcurve)* 0.001 * pow(f,4.0))
	    * gfc->ATHlower;
}

static void
compute_ath(lame_t gfc)
{
    FLOAT ATH_f, sfreq = gfc->out_samplerate;
    int sfb, i, start, end;

    /*  no-ATH mode: reduce ATH to -370 dB */
    if (gfc->noATH) {
        for (sfb = 0; sfb < SBMAX_l; sfb++) {
            gfc->ATH.l[sfb] = 1E-37;
        }
        for (sfb = 0; sfb < SBMAX_s; sfb++) {
            gfc->ATH.s[sfb] = 1E-37;
        }
	return;
    }

    for (sfb = 0; sfb < SBMAX_l; sfb++) {
	start = gfc->scalefac_band.l[ sfb ];
	end   = gfc->scalefac_band.l[ sfb+1 ];
	gfc->ATH.l[sfb] = FLOAT_MAX;
	for (i = start ; i < end; i++) {
	    ATH_f = ATHformula(gfc, i*sfreq/(2.0*576), 0);
	    if (gfc->ATH.l[sfb] > ATH_f)
		gfc->ATH.l[sfb] = ATH_f;
	}
	gfc->ATH.l[sfb] *= (end-start) / FFT2MDCT;
    }

    for (sfb = 0; sfb < SBMAX_s; sfb++){
	start = gfc->scalefac_band.s[ sfb ];
	end   = gfc->scalefac_band.s[ sfb+1 ];
	gfc->ATH.s[sfb] = FLOAT_MAX;
	for (i = start ; i < end; i++) {
	    ATH_f = ATHformula(gfc, i*sfreq/(2.0*192), 0);
	    if (gfc->ATH.s[sfb] > ATH_f)
		gfc->ATH.s[sfb] = ATH_f;
	}
	gfc->ATH.s[sfb] *= (end-start) / FFT2MDCT;
    }
}

/************************************************************************/
/*  initialization for iteration_loop */
/************************************************************************/
static const char subdv_table[23] = {
    0 + 0*16, /* 0 bands */
    0 + 0*16, /* 1 bands */
    0 + 0*16, /* 2 bands */
    0 + 0*16, /* 3 bands */
    0 + 0*16, /* 4 bands */
    0 + 1*16, /* 5 bands */
    1 + 1*16, /* 6 bands */
    1 + 1*16, /* 7 bands */
    1 + 2*16, /* 8 bands */
    2 + 2*16, /* 9 bands */
    2 + 3*16, /* 10 bands */
    2 + 3*16, /* 11 bands */
    3 + 4*16, /* 12 bands */
    3 + 4*16, /* 13 bands */
    3 + 4*16, /* 14 bands */
    4 + 5*16, /* 15 bands */
    4 + 5*16, /* 16 bands */
    4 + 6*16, /* 17 bands */
    5 + 6*16, /* 18 bands */
    5 + 6*16, /* 19 bands */
    5 + 7*16, /* 20 bands */
    6 + 7*16, /* 21 bands */
    6 + 7*16, /* 22 bands */
};

static void
huffman_init(lame_t gfc)
{
    int i;

#if HAVE_NASM
    gfc->ix_max = ix_max;
    if (gfc->CPU_features.MMX) {
	extern int ix_max_MMX(const int *ix, const int *end);
	gfc->ix_max = ix_max_MMX;
    }
    if (gfc->CPU_features.MMX2) {
	extern int ix_max_MMX2(const int *ix, const int *end);
	gfc->ix_max = ix_max_MMX2;
    }
#endif

    gfc->scale_bitcounter = scale_bitcount;
    if (gfc->mode_gr < 2)
	gfc->scale_bitcounter = scale_bitcount_lsf;

    for (i = 2; i <= 576; i += 2) {
	int scfb_anz = 0, j, k;
	while (gfc->scalefac_band.l[++scfb_anz] < i)
	    ;

	j = subdv_table[scfb_anz] & 0xf;
	while (gfc->scalefac_band.l[j + 1] > i)
	    j--;

	if (j < 0) {
	    /* this is an indication that everything is going to
	       be encoded as region0:  bigvalues < region0 < region1
	       so lets set region0, region1 to some value larger
	       than bigvalues */
	    j = subdv_table[scfb_anz] & 0xf;
	}

	gfc->bv_scf[i-2] = k = j;

	j = subdv_table[scfb_anz] >> 4;
	while (gfc->scalefac_band.l[j + k + 2] > i)
	    j--;

	if (j < 0)
	    j = subdv_table[scfb_anz] >> 4;

	gfc->bv_scf[i-1] = j;
    }
}

static FLOAT
filter_coef(FLOAT x)
{
    if (x > 1.0) return 0.0;
    if (x <= 0.0) return 1.0;

    return cos(PI/2 * x);
}

static void
init_filter0(lame_t gfc)
{
    int band, maxband, minband;
    int lowpass_band = SBLIMIT;
    int highpass_band = -1;
    FLOAT   freq;

    if (gfc->lowpass1 > 0) {
	minband = 999;
	for (band = 0; band <= 31; band++) {
	    freq = band / 31.0;
	    /* this band and above will be zeroed: */
	    if (freq >= gfc->lowpass2) {
		lowpass_band = Min(lowpass_band, band);
	    }
	    if (gfc->lowpass1 < freq && freq < gfc->lowpass2) {
		minband = Min(minband, band);
	    }
	}

        /* compute the *actual* transition band implemented by
         * the polyphase filter */
	if (minband == 999) {
	    gfc->lowpass1 = (lowpass_band - .75) / 31.0;
	}
	else {
	    gfc->lowpass1 = (minband - .75) / 31.0;
	}
	gfc->lowpass2 = lowpass_band / 31.0;
    }

    /* make sure highpass filter is within 90% of what the effective
     * highpass frequency will be */
    if (gfc->highpass2 > 0 && gfc->highpass2 < .9 * (.75 / 31.0)) {
	gfc->highpass1 = gfc->highpass2 = 0;
	gfc->report.msgf(
	    "Warning: highpass filter disabled (the frequency too small)\n");
    }

    if (gfc->highpass2 > 0) {
	maxband = -1;
	for (band = 0; band <= 31; band++) {
	    freq = band / 31.0;
	    /* this band and below will be zereod */
	    if (freq <= gfc->highpass1) {
		highpass_band = Max(highpass_band, band);
	    }
	    if (gfc->highpass1 < freq && freq < gfc->highpass2) {
		maxband = Max(maxband, band);
	    }
	}
	/* compute the *actual* transition band implemented by
	 * the polyphase filter */
	gfc->highpass1 = highpass_band / 31.0;
	if (maxband == -1) {
	    gfc->highpass2 = (highpass_band + .75) / 31.0;
	}
	else {
	    gfc->highpass2 = (maxband + .75) / 31.0;
	}
    }

    for (band = 0; band < SBLIMIT; band++) {
	freq = band / 31.0;
	gfc->amp_filter0[band]
	    = filter_coef((gfc->highpass2 - freq)
			  / (gfc->highpass2 - gfc->highpass1 + 1e-37))
	    * filter_coef((freq - gfc->lowpass1)
			  / (gfc->lowpass2 - gfc->lowpass1 - 1e-37));
    }
    while (--band >= 0) {
	if (gfc->amp_filter0[band] > 1e-20)
	    break;
    }
    gfc->xrNumMax_longblock = (band+1) * 18;
}


static void
init_filter1(lame_t gfc)
{
    int band, maxband, minband;
    int lowpass_band = 192;
    int highpass_band = -1;
    FLOAT   freq;

    if (gfc->lowpass1 > 0) {
	minband = 999;
	for (band = 0; band <= 191; band++) {
	    freq = band / 191.0;
	    /* this band and above will be zeroed: */
	    if (freq >= gfc->lowpass2) {
		lowpass_band = Min(lowpass_band, band);
	    }
	    if (gfc->lowpass1 < freq && freq < gfc->lowpass2) {
		minband = Min(minband, band);
	    }
	}

        /* compute the *actual* transition band implemented by
         * the polyphase filter */
	if (minband == 999) {
	    gfc->lowpass1 = (lowpass_band - .75) / 191.0;
	}
	else {
	    gfc->lowpass1 = (minband - .75) / 191.0;
	}
	gfc->lowpass2 = lowpass_band / 191.0;
    }

    /* make sure highpass filter is within 90% of what the effective
     * highpass frequency will be */
    if (gfc->highpass2 > 0 && gfc->highpass2 < .9 * (.75 / 191.0)) {
	gfc->highpass1 = gfc->highpass2 = 0;
	gfc->report.msgf(
	    "Warning: highpass filter disabled (the frequency too small)\n");
    }

    if (gfc->highpass2 > 0) {
	maxband = -1;
	for (band = 0; band <= 191; band++) {
	    freq = band / 191.0;
	    /* this band and below will be zereod */
	    if (freq <= gfc->highpass1) {
		highpass_band = Max(highpass_band, band);
	    }
	    if (gfc->highpass1 < freq && freq < gfc->highpass2) {
		maxband = Max(maxband, band);
	    }
	}
	/* compute the *actual* transition band implemented by
	 * the polyphase filter */
	gfc->highpass1 = highpass_band / 191.0;
	if (maxband == -1) {
	    gfc->highpass2 = (highpass_band + .75) / 191.0;
	}
	else {
	    gfc->highpass2 = (maxband + .75) / 191.0;
	}
    }

    for (band = 0; band < 192; band++) {
	freq = band / 191.0;
	gfc->amp_filter1[band]
	    = filter_coef((gfc->highpass2 - freq)
			  / (gfc->highpass2 - gfc->highpass1 + 1e-37))
	    * filter_coef((freq - gfc->lowpass1)
			  / (gfc->lowpass2 - gfc->lowpass1 - 1e-37));
    }
    band = SBLIMIT;
    while (--band >= 0) {
	if (gfc->amp_filter1[band*6] > 1e-20)
	    break;
    }
    gfc->xrNumMax_longblock = (band+1) * 18;

    memset(gfc->amp_filter0, 0, sizeof(gfc->amp_filter0));
    while (--band >= 0)
	gfc->amp_filter0[band] = 1.0;
}


/* resampling via FIR filter, blackman window */
static FLOAT
blackman(FLOAT x, FLOAT fcn, int l)
{
    /* This algorithm from:
       SIGNAL PROCESSING ALGORITHMS IN FORTRAN AND C
       S.D. Stearns and R.A. David, Prentice-Hall, 1992
    */
    FLOAT x2, wcn = PI * fcn;

    if (x<0) x=0.0;
    else if (x>l) x=1.0;
    else x /= l;
    x2 = x - .5;

    if (fabs(x2)<1e-9)
	return wcn/PI;

    return (0.42 - 0.5*cos(2*x*PI)  + 0.08*cos(4*x*PI))*sin(l*wcn*x2)
	/ (PI*l*x2);
}

/* gcd - greatest common divisor */
/* Joint work of Euclid and M. Hendry */
static int
gcd(int i, int j)
{
    return j ? gcd(j, i % j) : i;
}

void
iteration_init(lame_t gfc)
{
    int i, j;

    /******************************************************/
    /* compute info needed for frequency region filtering */
    /******************************************************/
    if (gfc->filter_type == 0)
	init_filter0(gfc);
    if (gfc->filter_type == 1)
	init_filter1(gfc);
#if 0
    /* not yet coded */
    if (gfc->filter_type == 2)
	init_filter2(gfc);
#endif

    /* scalefactor band start/end position */
    gfc->scalefac_band
	= sfBandIndex[gfc->samplerate_index
		     + (3 * gfc->version) + 6 * (gfc->out_samplerate < 16000)];

    /* cutoff and intensity stereo */
    for (i = 0; i < SBMAX_l; i++) {
	if (gfc->scalefac_band.l[i] > gfc->lowpass2*576)
	    break;
    }
    gfc->cutoff_sfb_l = i;
    for (i = 0; i < SBMAX_s; i++) {
	if (gfc->scalefac_band.s[i] > gfc->lowpass2*192)
	    break;
    }
    gfc->cutoff_sfb_s = i;

    gfc->use_istereo = 0;
    gfc->is_start_sfb_l_next[0] = gfc->is_start_sfb_l_next[1]
	= gfc->is_start_sfb_l[0] = gfc->is_start_sfb_l[1]
	= gfc->cutoff_sfb_l;
    gfc->is_start_sfb_s_next[0] = gfc->is_start_sfb_s_next[1]
	= gfc->is_start_sfb_s[0] = gfc->is_start_sfb_s[1]
	= gfc->cutoff_sfb_s;
    if (gfc->mode != MONO && gfc->compression_ratio > 12.0
	&& gfc->mode_gr == 2 /* currently only MPEG1 */)
	gfc->use_istereo = 1;

    gfc->main_data_begin = 0;
    compute_ath(gfc);
#ifdef USE_FAST_LOG
    init_log_table();
#endif

    pow43[0] = 0.0;
    for (i=1;i<PRECALC_SIZE;i++)
        pow43[i] = pow((FLOAT)i, 4.0/3.0);

    for (i = 0; i < PRECALC_SIZE-1; i++)
	adj43[i] = (i + 1) - pow(0.5 * (pow43[i] + pow43[i + 1]), 0.75);
    adj43[i] = 0.5;
#ifdef USE_IEEE754_HACK
    for (i = 0; i < PRECALC_SIZE; i++)
	adj43asm[i] = (int)(adj43[i] * (1 << FIXEDPOINT)) - MAGIC_INT2;
#endif

    for (i = 0; i < Q_MAX+Q_MAX2; i++) {
	ipow20[i] = pow(2.0, (double)(i - 210 - Q_MAX2) * -0.1875);
	pow20[i] = pow(2.0, (double)(i - 210 - Q_MAX2) * 0.25);
    }

    huffman_init(gfc);

    if (gfc->resample.ratio != 1.0) {
	FLOAT fcn = 1.00, *pFilter;
	int bpc = gcd(gfc->out_samplerate, gfc->in_samplerate);
	int filter_l = 31;
	if (bpc == gfc->out_samplerate || bpc == gfc->in_samplerate)
	    filter_l++; /* if the resample ratio is int, it must be even */

	bpc = gfc->out_samplerate/bpc;
	if (bpc > BPC)
	    bpc = BPC;

	if (fcn < gfc->resample.ratio)
	    fcn /= gfc->resample.ratio;

	gfc->resample.inbuf_old[0] = calloc(filter_l+1,sizeof(FLOAT));
	gfc->resample.inbuf_old[1] = calloc(filter_l+1,sizeof(FLOAT));
	gfc->resample.itime[0]=0;
	gfc->resample.itime[1]=0;

	/* precompute blackman filter coefficients */
	pFilter = gfc->resample.blackfilt
	    = malloc((filter_l+1)*sizeof(FLOAT)*(2*bpc+1));
	if (!pFilter)
	    return;
	for (j = 0; j <= 2*bpc; j++) {
	    FLOAT sum = 0.; 
	    FLOAT offset = (j-bpc) / (2.*bpc);

	    for (i = 0; i <= filter_l; i++)
		sum += (pFilter[i] = blackman(i-offset, fcn, filter_l));
	    sum = 1.0 / sum;
	    for (i = 0; i <= filter_l; i++)
		*pFilter++ *= sum;
	}
	gfc->resample.filter_l = filter_l;
	gfc->resample.bpc = bpc;
    }
}

/* Mapping from frequency to barks */
/* input: freq in hz  output: barks */
/* see for example "Zwicker: Psychoakustik, 1982; ISBN 3-540-11401-7 */
static FLOAT
freq2bark(FLOAT freq)
{
    freq = freq * 0.001;
    assert(freq>=0.0);

    if (freq < FREQ_BOUND) {
	return 13.0*atan(FREQ_BOUND*.76)
	    + 3.5*atan(FREQ_BOUND*FREQ_BOUND/(7.5*7.5))
	    + (freq-FREQ_BOUND)*5;
    } else
	return 13.0*atan(.76*freq) + 3.5*atan(freq*freq/(7.5*7.5));
}

/* 
 *   The spreading function.  Values returned in units of energy
 */
static FLOAT
s3_func(FLOAT bark)
{
    FLOAT x, tempy;
    bark *= 1.5;
    if (bark >= 0.0)
	bark += bark;

    x = 0.0;
    if (0.5 <= bark && bark <= 2.5)
	x = 8.0 * ((bark-1.5)*(bark-1.5) - 1.0);

    bark += 0.474;
    tempy = 15.811389 + 7.5*bark - 17.5*sqrt(1.0 + bark*bark) + x;

    if (tempy <= -60.0) return 0.0;

    return db2pow(tempy);
}

/* compute numlines (the number of spectral lines in each partition band),
   bark value and partition number id of each sfb.
   each partition band should be about DELBARK wide, but sometimes not. */
static int
init_numline(int *numlines, int *bo, int *bm, FLOAT *bval, FLOAT *mld,
	     FLOAT sfreq, int blksize, short *scalepos, FLOAT deltafreq,
	     int sbmax)
{
    int partition[HBLKSIZE], i, j, sfb;

    sfreq /= blksize;
    for (i = j = 0; i < CBANDS; i++) {
	FLOAT bark1 = freq2bark(sfreq*j);
	int j2;
	for (j2 = j+2;
	     freq2bark(sfreq*j2) - bark1 < DELBARK && j2 <= blksize/2;
	     j2++)
	    ;
	if (DELBARK - (freq2bark(sfreq*(j2-1)) - bark1)
	    < (freq2bark(sfreq*j2) - bark1) - DELBARK)
	    j2--;

	if (j2 > blksize/2+1 || i == CBANDS-1)
	    j2 = blksize/2+1;
	bval[i] = freq2bark(sfreq*(j+j2)*0.5);
	numlines[i] = j2 - j;
	while (j<j2)
	    partition[j++]=i;
	if (j > blksize/2) break;
    }

    for (sfb = 0; sfb < sbmax; sfb++) {
	FLOAT arg;
	int i1 = (int)(.5 + deltafreq*scalepos[sfb]);
	int i2 = (int)(.5 + deltafreq*scalepos[sfb+1]);

	if (i2 > blksize/2)
	    i2 = blksize/2;

	bo[sfb] = partition[i2];
	bm[sfb] = (partition[i1]+partition[i2])/2;

	/* setup stereo demasking thresholds */
	/* formula reverse enginerred from plot in paper */
	arg = freq2bark(sfreq*(i1+i2)/2*deltafreq)/15.5;
	mld[sfb] = db2pow(-12.5*(1+cos(PI*Min(arg, 1.0))));
    }

    return i+1;
}

static void
init_numline_l2s(int *bo, FLOAT sfreq, int blksize, short *scalepos,
		 FLOAT deltafreq, int sbmax)
{
    int partition[HBLKSIZE], i, j, sfb;

    sfreq /= blksize;
    for (i = j = 0; i < CBANDS; i++) {
	FLOAT bark1 = freq2bark(sfreq*j);
	int j2;
	for (j2 = j+2;
	     freq2bark(sfreq*j2) - bark1 < DELBARK && j2 <= blksize/2;
	     j2++)
	    ;

	if (DELBARK - (freq2bark(sfreq*(j2-1)) - bark1)
	    < (freq2bark(sfreq*j2) - bark1) - DELBARK)
	    j2--;

	if (j2 > blksize/2+1 || i == CBANDS-1)
	    j2 = blksize/2+1;
	while (j<j2)
	    partition[j++]=i;
	if (j > blksize/2) break;
    }

    for (sfb = 0; sfb < sbmax; sfb++) {
	int i2 = (int)(.5 + deltafreq*scalepos[sfb+1]);
	if (i2>blksize/2)
	    i2=blksize/2;
	bo[sfb] = partition[i2];
    }
}

static FLOAT *
init_s3_values(lame_t gfc, int (*s3ind)[2],
	       int npart, FLOAT *bval, FLOAT *norm)
{
    FLOAT s3[CBANDS][CBANDS], *p;
    int i, j, k;
    int numberOfNoneZero = 0;

    /* s[i][j], the value of the spreading function,
     * centered at band j(masker), for band i(maskee)
     */
    for (i = 0; i < npart; i++)
	for (j = 0; j < npart; j++) 
	    s3[i][j] = s3_func(bval[i] - bval[j]) * norm[i] * 0.3;

    for (i = 0; i < npart; i++) {
	for (j = 0; j < npart; j++) {
	    if (s3[i][j] != 0.0)
		break;
	}
	s3ind[i][0] = j;

	for (j = npart - 1; j > 0; j--) {
	    if (s3[i][j] != 0.0)
		break;
	}
	s3ind[i][1] = j;
	numberOfNoneZero += (s3ind[i][1] - s3ind[i][0] + 1);
    }
    if (!(p = malloc(sizeof(FLOAT)*numberOfNoneZero)))
	return p;

    k = 0;
    for (i = 0; i < npart; i++) {
	FLOAT fact;
	if (bval[i] < 6.0) {
	    fact = gfc->nsPsy.tuneBass;
	}
	else if (bval[i] < 12.0) {
	    fact = gfc->nsPsy.tuneAlto;
	}
	else if (bval[i] < 18.0) {
	    fact = gfc->nsPsy.tuneTreble;
	}
	else {
	    fact = gfc->nsPsy.tuneSFB21;
	}
	fact = db2pow(fact*0.25);
	for (j = s3ind[i][0]; j <= s3ind[i][1]; j++)
	    p[k++] = s3[i][j] * fact;
    }
    return p;
}

int
psymodel_init(lame_t gfc)
{
    int i,j,sb,k;
    int bm[SBMAX_l];

    FLOAT bval[CBANDS];
    FLOAT norm[CBANDS];
    FLOAT sfreq = gfc->out_samplerate;
    int numlines_s[CBANDS], npart_s;

    for (i=0; i<MAX_CHANNELS*2; ++i) {
	for (sb = 0; sb < SBMAX_l; sb++) {
	    gfc->masking_next[0][i].en.l[sb] = 1e20;
	    gfc->masking_next[1][i].en.l[sb] = 1e20;
	    gfc->masking_next[0][i].thm.l[sb] = 1e20;
	    gfc->masking_next[1][i].thm.l[sb] = 1e20;
	}
	for (sb = 0; sb < SBMAX_s; sb++) {
	    for (j = 0; j < 3; j++) {
		gfc->masking_next[0][i].en.s[sb][j] = 1e20;
		gfc->masking_next[1][i].en.s[sb][j] = 1e20;
		gfc->masking_next[0][i].thm.s[sb][j] = 1e20;
		gfc->masking_next[1][i].thm.s[sb][j] = 1e20;
	    }
	}
	for (j = 0; j < 12; j++)
	    gfc->nsPsy.subbk_ene[i][j] = 1.0;
	gfc->blocktype_next[0][i] = gfc->blocktype_next[1][i] = NORM_TYPE;

	gfc->ATH.adjust[i] = 0.0;
    }

    /*************************************************************************
     * now compute the psychoacoustic model specific constants
     ************************************************************************/
    /* compute numlines, bo, bm, bval, mld */
    gfc->npart_l
	= init_numline(gfc->numlines_l, gfc->bo_l, bm,
		       bval, gfc->mld_l, sfreq, BLKSIZE, 
		       gfc->scalefac_band.l, BLKSIZE/(2.0*576), SBMAX_l);
    assert(gfc->npart_l <= CBANDS);
    /* compute the spreading function */
    for (i=0;i<gfc->npart_l;i++) {
	int l;
	l = gfc->numlines_l[i];
	if (i != 0)
	    l += gfc->numlines_l[i-1];
	if (i != gfc->npart_l-1)
	    l += gfc->numlines_l[i+1];

	norm[i] = 0.11749/5.0;
	if (i > 50)
	    norm[i] *= 2;
	gfc->rnumlines_ls[i] = 20.0/(l-1);
	gfc->rnumlines_l[i] = 1.0 / (gfc->numlines_l[i] * 3);
	if (gfc->ATHonly)
	    norm[i] = 1e-37;
    }
    gfc->s3_ll = init_s3_values(gfc, gfc->s3ind, gfc->npart_l, bval, norm);
    if (!gfc->s3_ll)
	return LAME_NOMEM;

    /* compute long block specific values, ATH */
    j = 0;
    for (i = 0; i < gfc->npart_l; i++) {
	/* ATH */
	FLOAT x = FLOAT_MAX;
	for (k=0; k < gfc->numlines_l[i]; k++, j++) {
	    FLOAT level = ATHformula(gfc, sfreq*j/BLKSIZE, 1);
	    if (x > level)
		x = level;
	}
	gfc->ATH.cb[i] = x * gfc->numlines_l[i] * 0.01;
	gfc->ATH.eql_w[i]
	    = ATHformula(gfc, 3300, 1) / (x * FFT2MDCT * gfc->numlines_l[i]);
    }
    for (i = 0; i < 8; i++)
	gfc->ATH.eql_w[i] = gfc->ATH.eql_w[0];
    for (i = 0; i < SBMAX_l; i++)
	gfc->ATH.l_avg[i] = gfc->ATH.cb[bm[i]];

    /* table for long block threshold -> short block threshold conversion */
    init_numline_l2s(gfc->bo_l2s, sfreq, BLKSIZE, 
		     gfc->scalefac_band.s, BLKSIZE/(2.0*192), SBMAX_s);

    /************************************************************************
     * do the same things for short blocks
     ************************************************************************/
    npart_s
	= init_numline(numlines_s, gfc->bo_s, bm,
		       bval, gfc->mld_s, sfreq, BLKSIZE_s,
		       gfc->scalefac_band.s, BLKSIZE_s/(2.0*192), SBMAX_s);
    assert(npart_s <= CBANDS);

    /* SNR formula is removed. but we should tune s3_s more */
    for (i = 0; i < npart_s; i++) {
	norm[i] = db2pow(-0.25);

	gfc->endlines_s[i] = numlines_s[i];

	if (i != 0)
	    gfc->endlines_s[i] += gfc->endlines_s[i-1];
	if (gfc->ATHonly || gfc->ATHshort)
	    norm[i] = 1e-37;
    }
    for (i = 0; i < SBMAX_s; i++)
	gfc->ATH.s_avg[i]
	    = gfc->ATH.cb[bm[i]] * (BLKSIZE_s*BLKSIZE_s) / (BLKSIZE*BLKSIZE);

    gfc->s3_ss = init_s3_values(gfc, gfc->s3ind_s, npart_s, bval, norm);
    if (!gfc->s3_ss)
	return LAME_NOMEM;

    assert(gfc->bo_l[SBMAX_l-1] <= gfc->npart_l);
    assert(gfc->bo_s[SBMAX_s-1] <= npart_s);
    assert(gfc->bo_l2s[SBMAX_s-1] <= gfc->npart_l);
    init_mask_add_max_values(gfc);

    /* setup temporal masking */
    gfc->nsPsy.decay = db2pow(-(576.0/3)/(TEMPORALMASK_SUSTAIN_SEC*sfreq)*20);

    /* long/short switching, use subbandded sample in f > 2kHz */
    i = (int) (4000.0 / (sfreq / 2.0 / SBLIMIT) + 0.5);
    gfc->nsPsy.switching_band = Min(i, 30);

    /*  prepare for ATH auto adjustment:
     *  we want to decrease the ATH by 12 dB per second
     */
    gfc->ATH.aa_decay = 0.9;

    /* The type of window used here will make no real difference, but
     * use blackman window for long block. */
    for (i = 0; i < BLKSIZE ; i++)
      window[i]
	  = 0.42-0.5*cos(2*PI*(i+.5)/BLKSIZE) + 0.08*cos(4*PI*(i+.5)/BLKSIZE);

    for (i = 0; i < BLKSIZE_s; i++)
	window_s[i] = 0.5 * (1.0 - cos(2.0 * PI * (i + 0.5) / BLKSIZE_s));

#ifdef HAVE_NASM
    {
	extern void fht(FLOAT *fz, int n);
	gfc->fft_fht = fht;
    }
    if (gfc->CPU_features.AMD_E3DNow) {
	extern void fht_E3DN(FLOAT *fz, int n);
	gfc->fft_fht = fht_E3DN;
    } else
    if (gfc->CPU_features.AMD_3DNow) {
	extern void fht_3DN(FLOAT *fz, int n);
	gfc->fft_fht = fht_3DN;
    } else
    if (gfc->CPU_features.SSE) {
	extern void fht_SSE(FLOAT *fz, int n);
	gfc->fft_fht = fht_SSE;
    }
#endif

    if (gfc->channels_out == 1)
	gfc->interChRatio = 0.0;
    return 0;
}


void
init_bitstream_w(lame_t gfc)
{
    gfc->bs.h_ptr = gfc->bs.w_ptr = 0;
    gfc->bs.header[gfc->bs.h_ptr].write_timing = 0;
    gfc->bs.bitidx = gfc->bs.totbyte = 0;
    memset(gfc->bs.buf, 0, sizeof(gfc->bs.buf));

    /* determine the mean bitrate for main data */
    if (gfc->version == 1) /* MPEG 1 */
        gfc->sideinfo_len = (gfc->channels_out == 1) ? 4 + 17 : 4 + 32;
    else                /* MPEG 2 */
        gfc->sideinfo_len = (gfc->channels_out == 1) ? 4 + 9 : 4 + 17;

    if (gfc->error_protection)
        gfc->sideinfo_len += 2;

    /* padding method as described in 
     * "MPEG-Layer3 / Bitstream Syntax and Decoding"
     * by Martin Sieler, Ralph Sperschneider
     *
     * note: there is no padding for the very first frame
     *
     * Robert.Hegemann@gmx.de 2000-06-22
     */
    gfc->slot_lag = gfc->frac_SpF = 0;
    if (gfc->VBR == cbr && !gfc->disable_reservoir)
	gfc->slot_lag = gfc->frac_SpF
	    = ((gfc->version+1)*72000L*gfc->mean_bitrate_kbps)
	    % gfc->out_samplerate;

    gfc->maxmp3buf = 0;
    /* we cannot use reservoir over 320kbp or when indicated not to use */
    if ((gfc->VBR != cbr || gfc->mean_bitrate_kbps < 320)
	&& !gfc->disable_reservoir) {
	/* all mp3 decoders should have enough buffer to handle 1440byte:
	 * size of a 320kbps 32kHz frame
	 * Bouvigne suggests this more lax interpretation of the ISO doc 
	 * instead of using 8*960.
	 */
	gfc->maxmp3buf = 1440;
        if (gfc->strict_ISO) {
	    /* maximum allowed frame size.  dont use more than this number of
	       bits, even if the frame has the space for them: */
	    gfc->maxmp3buf
		= (320000*1152 / gfc->out_samplerate + 7) >> 3;
	    if (gfc->maxmp3buf > MAX_BITS*gfc->mode_gr)
		gfc->maxmp3buf = MAX_BITS*gfc->mode_gr;
	}
	gfc->maxmp3buf -= gfc->sideinfo_len;
    }
}
