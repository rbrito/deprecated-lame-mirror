#include <xmmintrin.h>

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
