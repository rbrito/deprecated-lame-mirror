; calculte xr^(3/4)
; from gogo3.12 (by shigeo, ururi, and kei)
; modified for LAME4 by takehiro.

%include "nasm.h"

	globaldef pow075_SSE

;void pow075(
;	float xr[576],
;	float xr34[576*2],
;	int end,
;	float *psum);

	segment_data
	align	16
Q_f1p25		dd	1.25, 1.25, 1.25, 1.25
Q_fm0p25	dd	-0.25, -0.25, -0.25, -0.25
Q_ABS		dd	0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF

	segment_text
proc	pow075_3DN
	mov	eax, [esp+4]	; eax = xr
	mov	edx, [esp+8]	; edx = xr34
	mov	ecx, [esp+12]	; ecx = end
	pxor	mm7, mm7	; srpow_sum
	lea	eax, [eax + ecx*4]
	lea	edx, [edx + ecx*4]
	neg	ecx
	movq	mm6, [Q_ABS]	; D_ABS

	loopalign	16
.lp:
	movq		mm4, [eax+ecx*4+ 0]
	movq		mm5, [eax+ecx*4+ 8]
	pand		mm4, mm6
	pand		mm5, mm6
	movq		[edx+ecx*4+ 0+576*4], mm4 ; absxr
	movq		[edx+ecx*4+ 8+576*4], mm5 ; absxr
	movq		mm0, mm4
	pfadd		mm7, mm4
	movq		mm1, mm5
	pfadd		mm7, mm5
	punpckhdq	mm0, mm0
	punpckhdq	mm1, mm1

	pfrsqrt		mm4, mm4
	pfrsqrt		mm0, mm0
	pfrsqrt		mm5, mm5
	pfrsqrt		mm1, mm1
	pfrsqrt		mm4, mm4
	pfrsqrt		mm0, mm0
	pfrsqrt		mm5, mm5
	pfrsqrt		mm1, mm1
	pfrcp		mm4, mm4
	pfrcp		mm0, mm0
	pfrcp		mm5, mm5
	pfrcp		mm1, mm1
	punpckldq	mm4, mm0	; y = approx. x^-0.25
	punpckldq	mm5, mm1	; y = approx. x^-0.25
	movq		mm0, mm4
	movq		mm1, mm5
	movq		mm2, [Q_fm0p25]	; = D_fm0p25
	movq		mm3, [Q_f1p25]	; = D_f1p25
	pfmul		mm4, mm4
	pfmul		mm5, mm5
	pfmul		mm4, mm4
	pfmul		mm5, mm5

	pfmul		mm4, [edx+ecx*4+ 0+576*4]
	pfmul		mm0, [edx+ecx*4+ 0+576*4]
	pfmul		mm5, [edx+ecx*4+ 8+576*4]
	pfmul		mm1, [edx+ecx*4+ 8+576*4]
	pfmul		mm4, mm2 		; - 1/4 * x (y^4)
	pfmul		mm5, mm2		; - 1/4 * x (y^4)
	pfadd		mm4, mm3		; 5/4 - 1/4 * x (y^4)
	pfadd		mm5, mm3		; 5/4 - 1/4 * x (y^4)
	pxor		mm2, mm2
	pfmul		mm4, mm0
	pfmul		mm5, mm1
	pfmax		mm4, mm2
	pfmax		mm5, mm2
	movq		[edx+ecx*4+ 0], mm4		; xr34
	movq		[edx+ecx*4+ 8], mm5		; xr34
	add	ecx, 4
	jnz	near .lp

	mov		eax, [esp+16]	; psum
	pfacc		mm7, mm7
	movd		[eax], mm7
	femms
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
	loopalign 16
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
	mov	eax, [esp+_P+4]		; eax = xr
	mov	ecx, [esp+_P+8]		; ecx = n
	mov	edx, [esp+_P+12]	; edx = psum

	pxor	mm0, mm0		; mm0 = sum0
	pxor	mm1, mm1		; mm1 = sum1
	test	ecx, 2
	jz	.lp
	movq	mm0, [eax+ecx*4]
	add	ecx, 2
	pfmul	mm0, mm0
	loopalign 16
.lp:
	movq	mm2, [eax+ecx*4+ 0]
	movq	mm3, [eax+ecx*4+ 8]
	pfmul	mm2, mm2
	pfmul	mm3, mm3
	add	ecx, 4
	pfadd	mm0, mm2
	pfadd	mm1, mm3
	jnz near	.lp

	movd	mm2, [edx]
	pfadd	mm0, mm1
	pfacc	mm0, mm0
	pfmul	mm0, mm2
	movd	[edx], mm0
	femms
	ret





	externdef	pow43
	externdef	pow20

proc	calc_noise_sub_3DN
%assign _P 16
	push		ebx
	push		esi
	push		edi
	push		ebp

	mov		edx, [esp+_P+16] ; scalefact
	mov		eax, [esp+_P+4]  ; absxr(end)
	mov		ecx, [esp+_P+8]  ; ix(end)
	movd		mm6, [pow20+116*4+edx*4] ; step
	mov		edx, [esp+_P+12] ; -n

	pxor		mm4, mm4
	pxor		mm5, mm5
	punpckldq	mm6, mm6

	test		edx, 2
	jz		.lp4
	mov		ebp, [ecx+ 0+ edx*4]
	mov		esi, [ecx+ 4+ edx*4]
	movd		mm4, [pow43+ebp*4]
	punpckldq	mm4, [pow43+esi*4]
	pfmul		mm4, mm6
	pfsubr		mm4, [eax+ 0+ edx*4]
	add		edx, byte 2
	pfmul		mm4, mm4

	loopalignK7	16
.lp4:
	mov		ebp, [ecx+ 0+ edx*4]
	mov		edi, [ecx+ 8+ edx*4]
	mov		esi, [ecx+ 4+ edx*4]
	mov		ebx, [ecx+12+ edx*4]
	movd		mm0, [pow43+ebp*4]
	movd		mm1, [pow43+edi*4]
	punpckldq	mm0, [pow43+esi*4]
	punpckldq	mm1, [pow43+ebx*4]
	pfmul		mm0, mm6
	pfmul		mm1, mm6
	pfsubr		mm0, [eax+ 0+ edx*4]
	pfsubr		mm1, [eax+ 8+ edx*4]
	add		edx, byte 4
	pfmul		mm0, mm0
	pfmul		mm1, mm1
	pfadd		mm4, mm0
	pfadd		mm5, mm1
	jnz		.lp4

	pfadd		mm4, mm5
	mov		edx, [esp+_P+20] ; result
	pfacc		mm4, mm4
	movd		[edx], mm4
	femms

	pop		ebp
	pop		edi
	pop		esi
	pop		ebx
	ret
