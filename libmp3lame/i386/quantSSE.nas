; calculte xr^(3/4)
; from gogo3.12 (by shigeo, ururi, and kei)
; modified for LAME4 by takehiro.

%include "nasm.h"

	globaldef pow075_SSE

;void pow075(
;	float xr[576],
;	float xr34[576],
;	int end,
;	float *psum);

	segment_data
	align	16
Q_f1p25		dd	1.25, 1.25, 1.25, 1.25
Q_fm0p25	dd	-0.25, -0.25, -0.25, -0.25
Q_ABS		dd	0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF

	segment_text
proc	pow075_3DN
	femms
	mov	ecx, [esp+12]	; ecx = end
	mov	edx, [esp+8]	; edx = xr34
	mov	eax, [esp+4]	; eax = xr
	shl	ecx, 2
	add	eax, ecx
	add	edx, ecx
	neg	ecx
	pxor	mm7, mm7
	jmp		.lp

	align 16
.lp:
%assign i 0
%rep 4
	movq		mm0, [eax+ecx+fsizen(i)]
	pand		mm0, [Q_ABS]	; = D_ABS
	movq		mm4, mm0
	pfadd		mm7, mm0
	movq		[edx+ecx+576*4+fsizen(i)], mm0
	punpckhdq	mm4,mm4
	pfrsqrt		mm1, mm0
	pfrsqrt		mm5, mm4
	movq		mm2, mm1
	movq		mm6, mm5
	pfmul		mm1, mm1
	pfmul		mm5, mm5
	pfrsqit1	mm1, mm0
	pfrsqit1	mm5, mm4
	pfrcpit2	mm1, mm2
	pfrcpit2	mm5, mm6
	pfrsqrt		mm3, mm1

	pfrsqrt		mm0, mm5
	movq		mm2, mm3
	movq		mm6, mm0
	pfmul		mm3, mm3
	pfmul		mm0, mm0
	pfrsqit1	mm3, mm1
	pfrsqit1	mm0, mm5
	pfrcpit2	mm3, mm2
	pfrcpit2	mm0, mm6
	punpckldq	mm3,mm0
	movq		mm0, mm3
	pfmul		mm3, mm3
	pfmul		mm3, mm0
	movq		[edx+ecx+fsizen(i)], mm3

%assign i i+2
%endrep
	add	ecx, 32
	jnz	near .lp

	movq	mm1, mm7
	punpckhdq	mm1,mm1
	pfadd	mm7, mm1
	movd	eax, mm7

	femms
	mov	edx, [esp+16]	; psum
	mov	[edx], eax
	ret


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

;*psum = sum_{i=0}^{i=n} x_i^2,
;	n should be even number and greater than 4.
;	x should be aligned to 8
;void sumofsqr(
;	float x[],
;	int n,
;	float *psum);


proc	sumofsqr_3DN
%assign _P 0
	femms
	mov	eax, [esp+_P+4]		; eax = xr
	mov	ecx, [esp+_P+8]		; edx = n
	mov	edx, [esp+_P+12]	; edx = psum

	pxor	mm0, mm0		; mm0 = sum0
	pxor	mm1, mm1		; mm1 = sum1
	test	ecx, 2
	jz	.even
	movq	mm0, [eax]
	add	eax, 8
	sub	ecx, 2
	pfmul	mm0, mm0
.even:
	shl	ecx, 2
	add	eax, ecx
	neg	ecx
	jmp	.lp
	align 16
.lp:
	movq	mm2, [eax+ecx+ 0]
	movq	mm3, [eax+ecx+ 8]
	pfmul	mm2, mm2
	pfmul	mm3, mm3
	add	ecx, 16
	pfadd	mm0, mm2
	pfadd	mm1, mm3
	jnz near	.lp

	pfadd	mm0, mm1
	movq	mm1, mm0
	punpckhdq	mm1,mm1
	pfadd	mm0, mm1
	movd	eax, mm0
	femms
	mov	[edx], eax
	ret
