/*
 *	MP3 window subband -> subband filtering -> mdct routine
 *
 *	Copyright (c) 1999 Takehiro TOMINAGA
 *
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

/*
 *         Special Thanks to Patrick De Smet for your advices.
 */


#include "util.h"
#include "l3side.h"
#include "newmdct.h"

#define SCALE 32768

static const FLOAT8 enwindow[] = 
{
  3.5758972e-02, 3.401756e-03,  9.83715e-04,   9.9182e-05,
      -4.77e-07,  1.03951e-04,  9.53674e-04, 2.841473e-03,
     1.2398e-05,  1.91212e-04, 2.283096e-03,1.6994476e-02,
  1.8756866e-02, 2.630711e-03,  2.47478e-04,   1.4782e-05,

  3.5694122e-02, 3.643036e-03,  9.91821e-04,   9.6321e-05,
      -4.77e-07,  1.05858e-04,  9.30786e-04, 2.521515e-03,
     1.1444e-05,  1.65462e-04, 2.110004e-03,1.6112804e-02,
  1.9634247e-02, 2.803326e-03,  2.77042e-04,   1.6689e-05,

  3.5586357e-02, 3.858566e-03,  9.95159e-04,   9.3460e-05,
      -4.77e-07,  1.07288e-04,  9.02653e-04, 2.174854e-03,
     1.0014e-05,  1.40190e-04, 1.937389e-03,1.5233517e-02,
  2.0506859e-02, 2.974033e-03,  3.07560e-04,   1.8120e-05,

  3.5435200e-02, 4.049301e-03,  9.94205e-04,   9.0599e-05,
      -4.77e-07,  1.08242e-04,  8.68797e-04, 1.800537e-03,
      9.060e-06,  1.16348e-04, 1.766682e-03,1.4358521e-02,
  2.1372318e-02,  3.14188e-03,  3.39031e-04,   1.9550e-05,

  3.5242081e-02, 4.215240e-03,  9.89437e-04,   8.7261e-05,
      -4.77e-07,  1.08719e-04,  8.29220e-04, 1.399517e-03,
      8.106e-06,   9.3937e-05, 1.597881e-03,1.3489246e-02,
  2.2228718e-02, 3.306866e-03,  3.71456e-04,   2.1458e-05,

  3.5007000e-02, 4.357815e-03,  9.80854e-04,   8.3923e-05,
      -4.77e-07,  1.08719e-04,   7.8392e-04,  9.71317e-04,
      7.629e-06,   7.2956e-05, 1.432419e-03,1.2627602e-02,
  2.3074150e-02, 3.467083e-03,  4.04358e-04,   2.3365e-05,

  3.4730434e-02, 4.477024e-03,  9.68933e-04,   8.0585e-05,
      -9.54e-07,  1.08242e-04,  7.31945e-04,  5.15938e-04,
      6.676e-06,   5.2929e-05, 1.269817e-03,1.1775017e-02,
  2.3907185e-02, 3.622532e-03,  4.38213e-04,   2.5272e-05,

  3.4412861e-02, 4.573822e-03,  9.54151e-04,   7.6771e-05,
      -9.54e-07,  1.06812e-04,  6.74248e-04,   3.3379e-05,
      6.199e-06,   3.4332e-05, 1.111031e-03,1.0933399e-02,
  2.4725437e-02, 3.771782e-03,  4.72546e-04,   2.7657e-05,

  3.4055710e-02, 4.649162e-03,  9.35555e-04,   7.3433e-05,
      -9.54e-07,  1.05381e-04,  6.10352e-04, -4.75883e-04,
      5.245e-06,   1.7166e-05,  9.56535e-04,1.0103703e-02,
  2.5527000e-02, 3.914356e-03,  5.07355e-04,   3.0041e-05,

  3.3659935e-02, 4.703045e-03,  9.15051e-04,   7.0095e-05,
      -9.54e-07,  1.02520e-04,  5.39303e-04,-1.011848e-03,
      4.768e-06,     9.54e-07,  8.06808e-04, 9.287834e-03,
  2.6310921e-02, 4.048824e-03,  5.42164e-04,   3.2425e-05,

  3.3225536e-02, 4.737377e-03,  8.91685e-04,   6.6280e-05,
     -1.431e-06,   9.9182e-05,  4.62532e-04,-1.573563e-03,
      4.292e-06,  -1.3828e-05,  6.61850e-04, 8.487225e-03,
  2.7073860e-02, 4.174709e-03,  5.76973e-04,   3.4809e-05,

  3.2754898e-02, 4.752159e-03,  8.66413e-04,   6.2943e-05,
     -1.431e-06,   9.5367e-05,  3.78609e-04,-2.161503e-03,
      3.815e-06,   -2.718e-05,  5.22137e-04, 7.703304e-03,
  2.7815342e-02, 4.290581e-03,  6.11782e-04,   3.7670e-05,

  3.2248020e-02, 4.748821e-03,  8.38757e-04,   5.9605e-05,
     -1.907e-06,   9.0122e-05,  2.88486e-04,-2.774239e-03,
      3.338e-06,  -3.9577e-05,  3.88145e-04, 6.937027e-03,
  2.8532982e-02, 4.395962e-03,  6.46591e-04,   4.0531e-05,

  3.1706810e-02, 4.728317e-03,  8.09669e-04,    5.579e-05,
     -1.907e-06,   8.4400e-05,  1.91689e-04,-3.411293e-03,
      3.338e-06,  -5.0545e-05,  2.59876e-04, 6.189346e-03,
  2.9224873e-02, 4.489899e-03,  6.80923e-04,   4.3392e-05,

  3.1132698e-02, 4.691124e-03,  7.79152e-04,   5.2929e-05,
     -2.384e-06,   7.7724e-05,   8.8215e-05,-4.072189e-03,
      2.861e-06,  -6.0558e-05,  1.37329e-04, 5.462170e-03,
  2.9890060e-02, 4.570484e-03,  7.14302e-04,   4.6253e-05,

  3.5780907e-02,1.7876148e-02,3.134727e-03,2.457142e-03,
    9.71317e-04,  2.18868e-04, 1.01566e-04,  1.3828e-05,

  3.0526638e-02, 4.638195e-03,  7.47204e-04,   4.9591e-05,
   4.756451e-03,   2.1458e-05,  -6.9618e-05,    2.384e-06
};


#define NS 12
#define NL 36

static FLOAT8 ca[8], cs[8];
static FLOAT8 win[4][36];
static FLOAT8 tantab_l[NL/4];
static FLOAT8 tritab_s[NS/4*2];

/************************************************************************
*
* window_subband()
*
* PURPOSE:  Overlapping window on PCM samples
*
* SEMANTICS:
* 32 16-bit pcm samples are scaled to fractional 2's complement and
* concatenated to the end of the window buffer #x#. The updated window
* buffer #x# is then windowed by the analysis window #c# to produce the
* windowed sample #z#
*
************************************************************************/

/*
 *      new IDCT routine written by Naoki Shibata
 */
INLINE static void idct32(FLOAT8 a[])
{
    /* returns sum_j=0^31 a[j]*cos(PI*j*(k+1/2)/32), 0<=k<32 */

    static const FLOAT8 costab[] = {
	0.54119610014619701222,
	1.3065629648763763537,

	0.50979557910415917998,
	2.5629154477415054814,
	0.89997622313641556513,
	0.60134488693504528634,

	0.5024192861881556782,
	5.1011486186891552563,
	0.78815462345125020249,
	0.64682178335999007679,
	0.56694403481635768927,
	1.0606776859903470633,
	1.7224470982383341955,
	0.52249861493968885462,

	10.19000812354803287,
	0.674808341455005678,
	1.1694399334328846596,
	0.53104259108978413284,	
	2.0577810099534108446,
	0.58293496820613388554,
	0.83934964541552681272,
	0.50547095989754364798,
	3.4076084184687189804,
	0.62250412303566482475,
	0.97256823786196078263,
	0.51544730992262455249,
	1.4841646163141661852,
	0.5531038960344445421,
	0.74453627100229857749,
	0.5006029982351962726,
    };

    int i;

    a[31] += a[29]; a[29] += a[27];
    a[27] += a[25]; a[25] += a[23];
    a[23] += a[21]; a[21] += a[19];
    a[19] += a[17]; a[17] += a[15];
    a[15] += a[13]; a[13] += a[11];
    a[11] += a[ 9]; a[ 9] += a[ 7];
    a[ 7] += a[ 5]; a[ 5] += a[ 3];
    a[ 3] += a[ 1];

    a[31] += a[27]; a[27] += a[23];
    a[30] += a[26]; a[26] += a[22];
    a[23] += a[19]; a[19] += a[15];
    a[22] += a[18]; a[18] += a[14];
    a[15] += a[11]; a[11] += a[ 7];
    a[14] += a[10]; a[10] += a[ 6];
    a[ 7] += a[ 3];
    a[ 6] += a[ 2];

    a[31] += a[23]; a[23] += a[15];    a[15] += a[ 7];
    a[30] += a[22]; a[22] += a[14];    a[14] += a[ 6];
    a[29] += a[21]; a[21] += a[13];    a[13] += a[ 5];
    a[28] += a[20]; a[20] += a[12];    a[12] += a[ 4];

    a[ 6] = -a[ 6];    a[22] = -a[22];
    a[12] = -a[12];    a[28] = -a[28];
    a[13] = -a[13];    a[29] = -a[29];
    a[15] = -a[15];    a[31] = -a[31];
    a[ 3] = -a[ 3];    a[19] = -a[19];
    a[11] = -a[11];    a[27] = -a[27];

    {
	FLOAT8 const *xp = costab;
	int p0,p1;
	for (i = 0; i < 8; i++) {
	    FLOAT8 x1, x2, x3, x4;

	    x3 = a[i+16] * (SQRT2*0.5);
	    x4 = a[i] - x3;
	    x3 = a[i] + x3;

	    x2 = -(a[24+i] + a[i+8]) * (SQRT2*0.5);
	    x1 = (a[i+8] - x2) * xp[0];
	    x2 = (a[i+8] + x2) * xp[1];

	    a[i   ] = x3 + x1;
	    a[i+ 8] = x4 - x2;
	    a[i+16] = x4 + x2;
	    a[i+24] = x3 - x1;
	}

	xp += 2;

	for (i = 0; i < 4; i++) {
	    FLOAT8 xr;

	    xr = a[i+28] * xp[0];
	    a[i+28] = (a[i] - xr);
	    a[i   ] = (a[i] + xr);

	    xr = a[i+4] * xp[1];
	    a[i+ 4] = (a[i+24] - xr);
	    a[i+24] = (a[i+24] + xr);

	    xr = a[i+20] * xp[2];
	    a[i+20] = (a[i+8] - xr);
	    a[i+ 8] = (a[i+8] + xr);

	    xr = a[i+12] * xp[3];
	    a[i+12] = (a[i+16] - xr);
	    a[i+16] = (a[i+16] + xr);
	}
	xp += 4;

	for (i = 0; i < 4; i++) {
	    FLOAT8 xr;

	    xr = a[30-i*4] * xp[0];
	    a[30-i*4] = (a[i*4] - xr);
	    a[   i*4] = (a[i*4] + xr);

	    xr = a[ 2+i*4] * xp[1];
	    a[ 2+i*4] = (a[28-i*4] - xr);
	    a[28-i*4] = (a[28-i*4] + xr);

	    xr = a[31-i*4] * xp[0];
	    a[31-i*4] = (a[1+i*4] - xr);
	    a[ 1+i*4] = (a[1+i*4] + xr);

	    xr = a[ 3+i*4] * xp[1];
	    a[ 3+i*4] = (a[29-i*4] - xr);
	    a[29-i*4] = (a[29-i*4] + xr);

	    xp += 2;
	}

	p0 = 30;
	p1 = 1;
	do {
	    FLOAT8 xr = a[p1] * *xp++;
	    a[p1] = (a[p0] - xr);
	    a[p0] = (a[p0] + xr);
	    p0 -= 2; p1 += 2;
	} while (p0 >= 0);
    }
}

INLINE static void window_subband(short *x1, FLOAT8 a[SBLIMIT])
{
    int i;
    FLOAT8 const *wp = enwindow;

    short *x2 = &x1[238-14-286];

    for (i = -15; i < 0; i++) {
	FLOAT8 w, s, t;

	w = wp[ 0]; s  = x2[  32] * w; t  = x1[- 32] * w;
	w = wp[ 1]; s += x2[  96] * w; t += x1[- 96] * w;
	w = wp[ 2]; s += x2[ 160] * w; t += x1[-160] * w;
	w = wp[ 3]; s += x2[ 224] * w; t += x1[-224] * w;
	w = wp[ 4]; s += x2[-224] * w; t += x1[ 224] * w;
	w = wp[ 5]; s += x2[-160] * w; t += x1[ 160] * w;
	w = wp[ 6]; s += x2[- 96] * w; t += x1[  96] * w;
	w = wp[ 7]; s += x2[- 32] * w; t += x1[  32] * w;

	w = wp[ 8]; s += x1[-256] * w; t -= x2[ 256] * w;
	w = wp[ 9]; s += x1[-192] * w; t -= x2[ 192] * w;
	w = wp[10]; s += x1[-128] * w; t -= x2[ 128] * w;
	w = wp[11]; s += x1[- 64] * w; t -= x2[  64] * w;
	w = wp[12]; s -= x1[   0] * w; t += x2[   0] * w;
	w = wp[13]; s -= x1[  64] * w; t += x2[- 64] * w;
	w = wp[14]; s -= x1[ 128] * w; t += x2[-128] * w;
	w = wp[15]; s -= x1[ 192] * w; t += x2[-192] * w;
	wp += 16;

	*--a = t;
	a[(i+16)*2] = s;
	x1--;
	x2++;
    }
    {
	FLOAT8 s,t;
	t  =  x1[- 16] * wp[0];               s  = x1[ -32] * wp[ 8];
	t += (x1[- 48] - x1[ 16]) * wp[1];    s += x1[ -96] * wp[ 9];
	t += (x1[- 80] + x1[ 48]) * wp[2];    s += x1[-160] * wp[10];
	t += (x1[-112] - x1[ 80]) * wp[3];    s += x1[-224] * wp[11];
	t += (x1[-144] + x1[112]) * wp[4];    s -= x1[  32] * wp[12];
	t += (x1[-176] - x1[144]) * wp[5];    s -= x1[  96] * wp[13];
	t += (x1[-208] + x1[176]) * wp[6];    s -= x1[ 160] * wp[14];
	t += (x1[-240] - x1[208]) * wp[7];    s -= x1[ 224] * wp[15];

	a[15] = t;
	a[-1] = s;
    }
}


/*-------------------------------------------------------------------*/
/*                                                                   */
/*   Function: Calculation of the MDCT                               */
/*   In the case of long blocks (type 0,1,3) there are               */
/*   36 coefficents in the time domain and 18 in the frequency       */
/*   domain.                                                         */
/*   In the case of short blocks (type 2) there are 3                */
/*   transformations with short length. This leads to 12 coefficents */
/*   in the time and 6 in the frequency domain. In this case the     */
/*   results are stored side by side in the vector out[].            */
/*                                                                   */
/*   New layer3                                                      */
/*                                                                   */
/*-------------------------------------------------------------------*/

INLINE static void mdct_short(FLOAT8 *out, FLOAT8 *in)
{
    int l;
    for ( l = 0; l < 3; l++ ) {
	FLOAT tc0,tc1,tc2,ts0,ts1,ts2;

	ts0 = in[5];
	tc0 = in[3];
	tc1 = ts0 + tc0;
	tc2 = ts0 - tc0;

	ts0 = in[2];
	tc0 = in[0];
	ts1 = ts0 + tc0;
	ts2 =-ts0 + tc0;

	tc0 = in[4];
	ts0 = in[1];

	out[3*0] = tc1 + tc0;
	out[3*5] =-ts1 + ts0;

	tc2 = tc2 * 0.86602540378443870761;
	ts1 = ts1 * 0.5 + ts0;
	out[3*1] = tc2-ts1;
	out[3*2] = tc2+ts1;

	tc1 = tc1 * 0.5 - tc0;
	ts2 = ts2 * 0.86602540378443870761;
	out[3*3] = tc1+ts2;
	out[3*4] = tc1-ts2;

	in += 6; out++;
    }
}

INLINE static void mdct_long(FLOAT8 *out, FLOAT8 *in)
{
#define inc(x) in[17-(x)]
#define ins(x) -in[8-(x)]

    const FLOAT8 c0=0.98480775301220802032, c1=0.64278760968653936292, c2=0.34202014332566882393;
    const FLOAT8 c3=0.93969262078590842791, c4=-0.17364817766693030343, c5=-0.76604444311897790243;
    FLOAT8 tc1 = inc(0)-inc(8),tc2 = (inc(1)-inc(7))*0.86602540378443870761, tc3 = inc(2)-inc(6), tc4 = inc(3)-inc(5);
    FLOAT8 tc5 = inc(0)+inc(8),tc6 = (inc(1)+inc(7))*0.5,                    tc7 = inc(2)+inc(6), tc8 = inc(3)+inc(5);
    FLOAT8 ts1 = ins(0)-ins(8),ts2 = (ins(1)-ins(7))*0.86602540378443870761, ts3 = ins(2)-ins(6), ts4 = ins(3)-ins(5);
    FLOAT8 ts5 = ins(0)+ins(8),ts6 = (ins(1)+ins(7))*0.5,                    ts7 = ins(2)+ins(6), ts8 = ins(3)+ins(5);
    FLOAT8 ct,st;

    ct = tc5+tc7+tc8+inc(1)+inc(4)+inc(7);
    out[0] = ct;

    ct = tc1*c0 + tc2 + tc3*c1 + tc4*c2;
    st = -ts5*c4 + ts6 - ts7*c5 + ts8*c3 + ins(4);
    out[1] = ct+st;
    out[2] = ct-st;

    ct =  tc5*c3 + tc6 + tc7*c4 + tc8*c5 - inc(4);
    st = ts1*c2 + ts2 + ts3*c0 + ts4*c1;
    out[3] = ct+st;
    out[4] = ct-st;

    ct = (tc1-tc3-tc4)*0.86602540378443870761;
    st = (ts5+ts7-ts8)*0.5+ins(1)-ins(4)+ins(7);
    out[5] = ct+st;
    out[6] = ct-st;

    ct = -tc5*c5 - tc6 - tc7*c3 - tc8*c4 + inc(4);
    st = ts1*c1 + ts2 - ts3*c2 - ts4*c0;
    out[7] = ct+st;
    out[8] = ct-st;

    ct = tc1*c1 - tc2 - tc3*c2 + tc4*c0;
    st = -ts5*c5 + ts6 - ts7*c3 + ts8*c4 + ins(4);
    out[ 9] = ct+st;
    out[10] = ct-st;

    ct = (tc5+tc7+tc8)*0.5-inc(1)-inc(4)-inc(7);
    st = (ts1-ts3+ts4)*0.86602540378443870761;
    out[11] = ct+st;
    out[12] = ct-st;

    ct = tc1*c2 - tc2 + tc3*c0 - tc4*c1;
    st =  ts5*c3 - ts6 + ts7*c4 - ts8*c5 - ins(4);
    out[13] = ct+st;
    out[14] = ct-st;

    ct = -tc5*c4 - tc6 - tc7*c5 - tc8*c3 + inc(4);
    st = ts1*c0 - ts2 + ts3*c1 - ts4*c2;
    out[15] = ct+st;
    out[16] = ct-st;

    st = ts5+ts7-ts8-ins(1)+ins(4)-ins(7);
    out[17] = st;
}

static const int order[] = {
    0,  16,  8, 24,  4,  20,  12,  28,
    2,  18, 10, 26,  6,  22,  14,  30,
    1,  17,  9, 25,  5,  21,  13,  29,
    3,  19, 11, 27,  7,  23,  15,  31
};

void mdct_sub48(lame_global_flags *gfp,
    short *w0, short *w1,
    FLOAT8 mdct_freq[2][2][576],
    III_side_info_t *l3_side)
{
    int gr, k, ch;
    short *wk;
    static int init = 0;
    lame_internal_flags *gfc=gfp->internal_flags;

    FLOAT8 work[18];

    if ( gfc->mdct_sub48_init == 0 ) {
        void mdct_init48(lame_global_flags *gfp);
        gfc->mdct_sub48_init=1;
	mdct_init48(gfp);
	init++;
    }

    wk = w0 + 286;
    /* thinking cache performance, ch->gr loop is better than gr->ch loop */
    for (ch = 0; ch < gfc->stereo; ch++) {
	for (gr = 0; gr < gfc->mode_gr; gr++) {
	    int	band;
	    FLOAT8 *mdct_enc = &mdct_freq[gr][ch][0];
	    gr_info *gi = &(l3_side->gr[gr].ch[ch].tt);
	    FLOAT8 *samp = gfc->sb_sample[ch][1 - gr][0];

	    for (k = 0; k < 18 / 2; k++) {
		window_subband(wk, samp + 16);
		idct32(samp);
		window_subband(wk + 32, samp + 32+16);
		idct32(samp+32);
		/*
		 * Compensate for inversion in the analysis filter
		 */
		samp += 64;
		wk += 64;
		samp[16+32-64] *= -1;
		samp[24+32-64] *= -1;
		samp[20+32-64] *= -1;
		samp[28+32-64] *= -1;
		samp[18+32-64] *= -1;
		samp[26+32-64] *= -1;
		samp[22+32-64] *= -1;
		samp[30+32-64] *= -1;
		samp[17+32-64] *= -1;
		samp[25+32-64] *= -1;
		samp[21+32-64] *= -1;
		samp[29+32-64] *= -1;
		samp[19+32-64] *= -1;
		samp[27+32-64] *= -1;
		samp[23+32-64] *= -1;
		samp[31+32-64] *= -1;
	    }


	    /* apply filters on the polyphase filterbank outputs */
	    /* bands <= gfc->highpass_band will be zeroed out below */
	    /* bands >= gfc->lowpass_band  will be zeroed out below */
	    if (gfc->filter_type==0) {
              for (band=gfc->highpass_start_band;  band <= gfc->highpass_end_band; band++) { 
		  for (k=0; k<18; k++) 
		    gfc->sb_sample[ch][1-gr][k][order[band]]*=gfc->amp_highpass[band];
	      }
              for (band=gfc->lowpass_start_band;  band <= gfc->lowpass_end_band; band++) { 
		  for (k=0; k<18; k++) 
		    gfc->sb_sample[ch][1-gr][k][order[band]]*=gfc->amp_lowpass[band];
	      }
	    }
	    


	    /*
	     * Perform imdct of 18 previous subband samples
	     * + 18 current subband samples
	     */
	    for (band = 0; band < 32; band++, mdct_enc += 18) 
              {
		int type = gi->block_type;
		int band_swapped;
		band_swapped = order[band];
#ifdef ALLOW_MIXED
		if (gi->mixed_block_flag && band < 2)
		    type = 0;
#endif
		if (band >= gfc->lowpass_band || band <= gfc->highpass_band) {
		    memset((char *)mdct_enc,0,18*sizeof(FLOAT8));
		}else {
		  if (type == SHORT_TYPE) {
		    for (k = 2; k >= 0; --k) {
			FLOAT8 w1 = win[SHORT_TYPE][k];
			FLOAT8 a, b;

			a = gfc->sb_sample[ch][gr][k+6][band_swapped] * w1 -
			    gfc->sb_sample[ch][gr][11-k][band_swapped];

			b = gfc->sb_sample[ch][gr][k+12][band_swapped] +
			    gfc->sb_sample[ch][gr][17-k][band_swapped] * w1;

			work[k+3] = -b*tritab_s[k*2  ] + a * tritab_s[k*2+1];
			work[k  ] =  b*tritab_s[k*2+1] + a * tritab_s[k*2  ];

			a = gfc->sb_sample[ch][gr][k+12][band_swapped] * w1 -
			    gfc->sb_sample[ch][gr][17-k][band_swapped];

			b = gfc->sb_sample[ch][1-gr][k][band_swapped] +
			    gfc->sb_sample[ch][1-gr][5-k][band_swapped] * w1;

			work[k+9] = -b*tritab_s[k*2  ] + a * tritab_s[k*2+1];
			work[k+6] =  b*tritab_s[k*2+1] + a * tritab_s[k*2  ];

			a = gfc->sb_sample[ch][1-gr][k][band_swapped] * w1 -
			    gfc->sb_sample[ch][1-gr][5-k][band_swapped];

			b = gfc->sb_sample[ch][1-gr][k+6][band_swapped] +
			    gfc->sb_sample[ch][1-gr][11-k][band_swapped] * w1;

			work[k+15] = -b*tritab_s[k*2  ] + a * tritab_s[k*2+1];
			work[k+12] =  b*tritab_s[k*2+1] + a * tritab_s[k*2  ];
		    }
		    mdct_short(mdct_enc, work);
		  } else {
		    for (k = -NL/4; k < 0; k++) {
			FLOAT8 a, b;
			a = win[type][k+27] * gfc->sb_sample[ch][1-gr][k+9][band_swapped]
			  + win[type][k+36] * gfc->sb_sample[ch][1-gr][8-k][band_swapped];
			b = win[type][k+ 9] * gfc->sb_sample[ch][gr][k+9][band_swapped]
			  - win[type][k+18] * gfc->sb_sample[ch][gr][8-k][band_swapped];
			work[k+ 9] =  a + b*tantab_l[k+9];
			work[k+18] = -a*tantab_l[k+9] + b;
		    }

		    mdct_long(mdct_enc, work);
		  }
		}
		
		
		/*
		  Perform aliasing reduction butterfly
		*/
		if (type != SHORT_TYPE) {
		  if (band == 0)
		    continue;
		  for (k = 7; k >= 0; --k) {
		    FLOAT8 bu,bd;
		    bu = mdct_enc[k] * ca[k] + mdct_enc[-1-k] * cs[k];
		    bd = mdct_enc[k] * cs[k] - mdct_enc[-1-k] * ca[k];
		    
		    mdct_enc[-1-k] = bu;
		    mdct_enc[k]    = bd;
		  }
		}
	      }
	}
	wk = w1 + 286;
	if (gfc->mode_gr == 1) {
	    memcpy(gfc->sb_sample[ch][0], gfc->sb_sample[ch][1], 576 * sizeof(FLOAT8));
	}
    }
}



void mdct_init48(lame_global_flags *gfp)
{
    int i, k;
    FLOAT8 sq;

    /* prepare the aliasing reduction butterflies */
    for (k = 0; k < 8; k++) {
	/*
	  This is table B.9: coefficients for aliasing reduction
	  */
	static const FLOAT8 c[8] = {
	    -0.6,-0.535,-0.33,-0.185,-0.095,-0.041,-0.0142, -0.0037
	};
	sq = 1.0 + c[k] * c[k];
	sq = sqrt(sq);
	ca[k] = c[k] / sq;
	cs[k] = 1.0 / sq;
    }

    /* type 0*/
    for (i = 0; i < 36; i++)
	win[0][i] = sin(PI/36 * (i + 0.5));
    /* type 1*/
    for (i = 0; i < 18; i++) 
	win[1][i] = win[0][i];
    for (; i < 24; i++)
	win[1][i] = 1.0;
    for (; i < 30; i++)
	win[1][i] = cos(PI/12 * (i + 0.5));
    for (; i < 36; i++)
	win[1][i] = 0.0;
    /* type 3*/
    for (i = 0; i < 36; i++)
	win[3][i] = win[1][35 - i];

    /* swap window data*/
    for (k = 0; k < 4; k++) {
	FLOAT8 a;

	a = win[0][17-k];
	win[0][17-k] = win[0][9+k];
	win[0][9+k] = a;

	a = win[0][35-k];
	win[0][35-k] = win[0][27+k];
	win[0][27+k] = a;

	a = win[1][17-k];
	win[1][17-k] = win[1][9+k];
	win[1][9+k] = a;

	a = win[1][35-k];
	win[1][35-k] = win[1][27+k];
	win[1][27+k] = a;

	a = win[3][17-k];
	win[3][17-k] = win[3][9+k];
	win[3][9+k] = a;

	a = win[3][35-k];
	win[3][35-k] = win[3][27+k];
	win[3][27+k] = a;
    }

    for (i = 0; i < NL; i++) {
	win[0][i] *= cos((NL/4+0.5+i%9)*M_PI/NL) / SCALE/(NL/4);
	win[1][i] *= cos((NL/4+0.5+i%9)*M_PI/NL) / SCALE/(NL/4);
	win[3][i] *= cos((NL/4+0.5+i%9)*M_PI/NL) / SCALE/(NL/4);
    }

    for (i = 0; i < NL/4; i++)
	tantab_l[i] = tan((NL/4+0.5+i)*M_PI/NL);

    /* type 2(short)*/
    for (i = 0; i < NS / 4; i++) {
	win[SHORT_TYPE][i] = tan(M_PI / NS * (i + 0.5));
    }

    for (i = 0; i < NS / 4; i++) {
	FLOAT8 w2 = cos(M_PI / NS * (i + 0.5)) * (4.0/NS) / SCALE;
	tritab_s[i*2  ] = cos((0.5+2-i)*M_PI/NS) * w2;
	tritab_s[i*2+1] = sin((0.5+2-i)*M_PI/NS) * w2;
    }
}
