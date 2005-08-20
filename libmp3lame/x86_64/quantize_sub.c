#include "../encoder.h"
#include "../util.h"
#include <xmmintrin.h>

static const float Qfsqrt2[4] __attribute__((aligned(16))) = {
    SQRT2*0.5,SQRT2*0.5,SQRT2*0.5,SQRT2*0.5};
static const float Qfm0p25[4] __attribute__((aligned(16))) = {
    -0.25, -0.25, -0.25, -0.25};
static const float Qf1p25[4] __attribute__((aligned(16))) = {
    1.25, 1.25, 1.25, 1.25};
static const int Q_ABS[4] __attribute__((aligned(16))) = {
    0x7fffffff,0x7fffffff,0x7fffffff,0x7fffffff};
static float Q_ROUNDFAC[4]  __attribute__((aligned(16))) = {
    0.4054,0.4054,0.4054,0.4054};

void
lr2ms_SSE(float *pl, float *pr, int i)
{
    __m128 c = _mm_load_ps(Qfsqrt2);
    int l0, l1, r0, r1;
    l0 = ((int*)pl)[i-1];
    l1 = ((int*)pl)[i-2];
    r0 = ((int*)pr)[i-1];
    r1 = ((int*)pr)[i-2];
    do {
	__m128 l, r;
	l = _mm_load_ps(pl);
	r = _mm_load_ps(pr);
	_mm_store_ps(pl, _mm_mul_ps(_mm_add_ps(l, r), c));
	_mm_store_ps(pr, _mm_mul_ps(_mm_sub_ps(l, r), c));
	pl += 4;
	pr += 4;
    } while ((i -= 4) > 0);
    if (i) {
	((int*)pl)[-3] = l0;
	((int*)pl)[-4] = l1;
	((int*)pr)[-3] = r0;
	((int*)pr)[-4] = r1;
    }
}

void sumofsqr_SSE(const float *end, int l, float *res)
{
    union {
        __m128 m;
        int i[4];
	float f[4];
    } u;
    __m128 m, s;
    float x;

    float *p = end+l;
    m = _mm_loadu_ps(p);
    s = _mm_xor_ps(s, s);
    if (l & 2) {
	m = _mm_unpacklo_ps(m, s);
	l += 2;
	if (l == 0)
	    goto exit;
	p += 2;
	s = _mm_mul_ps(m,m);
    }
    do {
	m = _mm_loadu_ps(p);
	p += 4;
	m = _mm_mul_ps(m,m);
	s = _mm_add_ps(s,m);
    } while ((l += 4) < 0);

 exit:
    _mm_storeu_ps(&u.m, s);
    x = u.f[0] + u.f[1] + u.f[2] + u.f[3];
    *res = *res * x;
}

void
pow075_SSE(float *xr, float *xr34, int end, float *max)
{
    int i = 0;
    __m128 srmax, zero;
    __m128 fm0p25 = _mm_load_ps(Qfm0p25);
    zero = srmax = _mm_xor_ps(srmax, srmax);
    
    for (i = 0; i < end; i += 8) {
	__mm128 w0, w1;
	__mm128 s0, s1, s2;
	w0 = _mm_load_ps(xr);
	w1 = _mm_load_ps(xr+4);
	w0 = _mm_and_ps(w0, Q_ABS);
	w1 = _mm_and_ps(w1, Q_ABS);
	_mm_store_ps(xr + 576  , w0);
	_mm_store_ps(xr + 576+4, w1);
	xr += 8;

	s0 = _mm_rsqrt_ps(w0);
	srmax = _mm_max_ps(srmax, w0);
	s1 = s0;
	srmax = _mm_max_ps(srmax, w1);
	s0 = _mm_rsqrt_ps(s0);
	s2 = s0;
	s0 = _mm_mul_ps(s0, s0);
	w0 = _mm_mul_ps(w0, s2);
	s1 = _mm_mul_ps(s1, s0);
	s1 = _mm_mul_ps(s1, s0);
	s1 = _mm_mul_ps(s1, Q_f1p25);
	w0 = _mm_mul_ps(w0, s1);

	s0 = _mm_rsqrt_ps(w1);
	s1 = s0;
	s0 = _mm_rsqrt_ps(s0);
	s2 = s0;
	s0 = _mm_mul_ps(s0, s0);
	w1 = _mm_mul_ps(w1, s2);
	s1 = _mm_mul_ps(s1, s0);
	s1 = _mm_mul_ps(s1, s0);
	s1 = _mm_mul_ps(s1, Q_f1p25);
	w1 = _mm_mul_ps(w1, s1);

	w0 = _mm_max_ps(w0, zero);
	w1 = _mm_max_ps(w1, zero);
	_mm_store_ps(xr34,   w0);
	_mm_store_ps(xr34+4, w1);
	xr34 += 8;
    }
    zero = _mm_movhl_ps(srmax);
    srmax = _mm_max_ps(srmax, zero);
    zero = _mm_shuffle_ps(srmax, _MM_SHUFFLE(1,1,1,1));
    srmax = _mm_max_ps(srmax, zero);
    _mm_store_ss(psum, srmax);
}

void qnatize_ISO_SSE2(float xrend[], int bw, int sf, int xr34end[])
{
    __mm128 istep = _mm_load1_ps(&IPOW20(sf));
    __mm128 roundfac = _mm_load_ps(Q_ROUNDFAC);
    do {
	__mm128 w0, w1, w2, w3;
	w0 = _mm_load_ps(xrend+bw   );
	w1 = _mm_load_ps(xrend+bw+ 4);
	w2 = _mm_load_ps(xrend+bw+ 8);
	w3 = _mm_load_ps(xrend+bw+12);
	bw += 16;
	w0 = _mm_mul_ps(w0, istep);
	w1 = _mm_mul_ps(w1, istep);
	w2 = _mm_mul_ps(w2, istep);
	w3 = _mm_mul_ps(w3, istep);
	w0 = _mm_cvtps_pd(w0);
	w1 = _mm_cvtps_pd(w1);
	w2 = _mm_cvtps_pd(w2);
	w3 = _mm_cvtps_pd(w3);
	_mm_store_ps(xr34end+bw-16   );
	_mm_store_ps(xr34end+bw-16+ 4);
	_mm_store_ps(xr34end+bw-16+ 8);
	_mm_store_ps(xr34end+bw-16+12);
    } while (bw < 0);
}
