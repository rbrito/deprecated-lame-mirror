; calculte xr^(3/4)
; from gogo3.12 (by shigeo, ururi, and kei)
; modified for LAME4 by takehiro.

%include "nasm.h"

	globaldef pow075_SSE

;void pow075(
;	float xr[576],
;	float xrpow[576],
;	int end,
;	float *psum);

	segment_data
	align	16
Q_f1p25		dd	1.25, 1.25, 1.25, 1.25
Q_fm0p25	dd	-0.25, -0.25, -0.25, -0.25
Q_ABS		dd	0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF

	segment_text
proc	pow075_SSE
%assign _P 0
	mov	eax, [esp+_P+4]		; eax = xr
	mov	edx, [esp+_P+8]		; edx = xr34
	mov	ecx, [esp+_P+12]	; ecx = end
	shl	ecx, 2
	xorps	xm3, xm3
	xorps	xm2, xm2		; xm2 = srpow_sum
	add	eax, ecx
	add	edx, ecx
	neg	ecx
	movaps	xm7, [Q_fm0p25]
	jmp	.lp
	align 16
.lp:
	movaps	xm0, [eax+ecx+ 0]
	movaps	xm1, [eax+ecx+16]
	andps	xm0, [Q_ABS]		; xm0 = |xr|
	movaps	xm6, xm1
	andps	xm1, [Q_ABS]
	movaps	[edx+ecx+ 0+576*4], xm0 ; absxr[]
	movaps	[edx+ecx+16+576*4], xm1
	add	ecx, 8*4

	;------x^(3/4)------
	rsqrtps	xm4, xm0
	addps	xm2, xm0
	movaps	xm5, xm0
	addps	xm2, xm1
	rsqrtps	xm4, xm4
	mulps	xm5, xm7		; -x/4
	rcpps	xm4, xm4		; y = approx. x^-0.25
	movaps	xm6, xm4
	mulps	xm4, xm4		; y^2
	mulps	xm0, xm6		; yx
	mulps	xm5, xm4
	mulps	xm5, xm4
	addps	xm5, [Q_f1p25]		; 5/4 - 1/4 * x (y^4)
	mulps	xm0, xm5

	rsqrtps	xm4, xm1
	movaps	xm5, xm1
	rsqrtps	xm4, xm4
	mulps	xm5, xm7		; -x/4
	rcpps	xm4, xm4		; y = approx. x^-0.25
	movaps	xm6, xm4
	mulps	xm4, xm4		; y^2
	mulps	xm1, xm6		; yx
	mulps	xm5, xm4
	mulps	xm5, xm4
	addps	xm5, [Q_f1p25]		; 5/4 - 1/4 * x (y^4)
	mulps	xm1, xm5

	maxps	xm0, xm3		; if(x == 0) then x^(3/4) = 0
	maxps	xm1, xm3
	;------------------

	movaps	[edx+ecx+ 0-32], xm0	; xr34
	movaps	[edx+ecx+16-32], xm1
	jnz near	.lp

	movhlps	xm4, xm2		;* * 3 2
	addps	xm4, xm2		;* * 1 0
	mov	eax, [esp+_P+16]	; psum
	movaps	xm2, xm4
	shufps	xm4, xm4, PACK(1,1,1,1)	;* * * 1
	addps	xm4, xm2		;* * * 0
	movss	[eax], xm4
.exit:
	ret
