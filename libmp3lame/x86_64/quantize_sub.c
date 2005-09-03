#include <xmmintrin.h>
#include <emmintrin.h>

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
    __m128 oldval;
    oldval = _mm_loadh_pi(oldval, pl+i-2);
    oldval = _mm_loadl_pi(oldval, pr+i-2);
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
	_mm_storeh_pi(pl-4, oldval);
	_mm_storel_pi(pr-4, oldval);
    }
}

float xrmax_SSE(const float *end, int l)
{
    __m128 m, w;
    float res;

    m = _mm_xor_ps(m, m);
    if ((unsigned long)end & 8) {
	end -= 2;
	l += 2;
	m = _mm_loadl_pi(m, end);
    }
    if (l & 2) {
	m = _mm_loadh_pi(m, end + l);
	l += 2;
    }
    for (; l < 0; l += 4) {
	m = _mm_max_ps(m, _mm_load_ps(end+l));
    }

    w = _mm_movehl_ps(w, m);
    m = _mm_max_ps(w, m);
    w = _mm_shuffle_ps(m, m, _MM_SHUFFLE(1,1,1,1));
    m = _mm_max_ss(w, m);
    _mm_store_ss(&res, w);
    return res;
}

void sumofsqr_SSE(const float *end, int l, float *res)
{
    __m128 m, s;
    s = _mm_xor_ps(s, s);
    if ((unsigned long)end & 8) {
	end -= 2;
	l += 2;
	s = _mm_loadl_pi(s, end);
    }
    if (l & 2) {
	s = _mm_loadh_pi(s, end + l);
	l += 2;
    }
    s = _mm_mul_ps(s, s);
    for (; l < 0; l += 4) {
	m = _mm_load_ps(end + l);
	m = _mm_mul_ps(m,m);
	s = _mm_add_ps(s,m);
    }

    m = _mm_movehl_ps(m, s);
    s = _mm_add_ps(s, m);
    m = _mm_shuffle_ps(s, s, _MM_SHUFFLE(1,1,1,1));
    s = _mm_add_ss(s, m);
    m = _mm_load_ss(res);
    s = _mm_mul_ss(s, m);
    _mm_store_ss(res, s);
}

void
pow075_SSE(float *x, float *x34, int end, float *max)
{
    __m128 srmax, zero;
    __m128 fm0p25 = _mm_load_ps(Qfm0p25);
    zero = srmax = _mm_xor_ps(srmax, srmax);

    do {
	__m128 w0, w1;
	__m128 s0, s1, s2;
	w0 = _mm_load_ps(x);
	w1 = _mm_load_ps(x+4);
	w0 = _mm_and_ps(w0, *(__m128*)Q_ABS);
	w1 = _mm_and_ps(w1, *(__m128*)Q_ABS);
	_mm_store_ps(x34 + 576  , w0);
	_mm_store_ps(x34 + 576+4, w1);
	x += 8;

	s0 = _mm_rsqrt_ps(w0);
	srmax = _mm_max_ps(srmax, w0);
	s1 = w0;
	srmax = _mm_max_ps(srmax, w1);
	s0 = _mm_rsqrt_ps(s0);
	s1 = _mm_mul_ps(s1, fm0p25);
	s0 = _mm_rcp_ps(s0);
	s2 = s0;
	s0 = _mm_mul_ps(s0, s0);
	w0 = _mm_mul_ps(w0, s2);
	s1 = _mm_mul_ps(s1, s0);
	s1 = _mm_mul_ps(s1, s0);
	s1 = _mm_add_ps(s1, *(__m128*)Qf1p25);
	w0 = _mm_mul_ps(w0, s1);

	s0 = _mm_rsqrt_ps(w1);
	s1 = w1;
	s0 = _mm_rsqrt_ps(s0);
	s1 = _mm_mul_ps(s1, fm0p25);
	s0 = _mm_rcp_ps(s0);
	s2 = s0;
	s0 = _mm_mul_ps(s0, s0);
	w1 = _mm_mul_ps(w1, s2);
	s1 = _mm_mul_ps(s1, s0);
	s1 = _mm_mul_ps(s1, s0);
	s1 = _mm_add_ps(s1, *(__m128*)Qf1p25);
	w1 = _mm_mul_ps(w1, s1);

	w0 = _mm_max_ps(w0, zero);
	w1 = _mm_max_ps(w1, zero);
	_mm_store_ps(x34,   w0);
	_mm_store_ps(x34+4, w1);
	x34 += 8;
    } while ((end -= 8) > 0);

    zero = _mm_movehl_ps(zero, srmax);
    srmax = _mm_max_ps(srmax, zero);
    zero = _mm_shuffle_ps(srmax, srmax, _MM_SHUFFLE(1,1,1,1));
    srmax = _mm_max_ss(srmax, zero);
    _mm_store_ss(max, srmax);
}

void quantize_ISO_SSE2(float xrend[], int bw, int sf, int ixend[])
{
    float x = IPOW20(sf);
    __m128 roundfac = _mm_load_ps(Q_ROUNDFAC);
    __m128 istep = _mm_load1_ps(&x);

    do {
	__m128 w0, w1, w2, w3;
	w0 = _mm_load_ps(xrend+bw   );
	w1 = _mm_load_ps(xrend+bw+ 4);
	w2 = _mm_load_ps(xrend+bw+ 8);
	w3 = _mm_load_ps(xrend+bw+12);
	bw += 16;
	w0 = _mm_mul_ps(w0, istep);
	w1 = _mm_mul_ps(w1, istep);
	w2 = _mm_mul_ps(w2, istep);
	w3 = _mm_mul_ps(w3, istep);
	w0 = _mm_add_ps(w0, roundfac);
	w1 = _mm_add_ps(w1, roundfac);
	w2 = _mm_add_ps(w2, roundfac);
	w3 = _mm_add_ps(w3, roundfac);
	w0 = (__m128)_mm_cvttps_epi32(w0);
	w1 = (__m128)_mm_cvttps_epi32(w1);
	w2 = (__m128)_mm_cvttps_epi32(w2);
	w3 = (__m128)_mm_cvttps_epi32(w3);
	_mm_store_ps(ixend+bw-16   , w0);
	_mm_store_ps(ixend+bw-16+ 4, w1);
	_mm_store_ps(ixend+bw-16+ 8, w2);
	_mm_store_ps(ixend+bw-16+12, w3);
    } while (bw < 0);
}
