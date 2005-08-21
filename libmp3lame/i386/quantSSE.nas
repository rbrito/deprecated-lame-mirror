; from gogo3.12 (by shigeo, ururi, and kei)
; modified for LAME4 by takehiro.

%include "nasm.h"

	globaldef pow075_SSE

; calculte xr^(3/4)
;void pow075(
;	float xr[576],
;	float xr34[576*2],
;	int end,
;	float *pmax);

	segment_data
	align	16
Q_f1p25		dd	1.25, 1.25, 1.25, 1.25
Q_fm0p25	dd	-0.25, -0.25, -0.25, -0.25
Q_ABS		dd	0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF
Q_sqrt2		dd	1060439283,1060439283,1060439283,1060439283

D_ROUNDFAC	dd	0.4054,0.4054
D_1ROUNDFAC	dd	0.5946,0.5946
D_IXMAXVAL	dd	8206.0,8206.0
ROUNDFAC_NEAR	dd	-0.0946
magicfloat	dd	8388608.0
maskpattern	dd	(1<<23)-1

	segment_code
proc	pow075_3DN
	mov	eax, [esp+4]	; eax = xr
	mov	edx, [esp+8]	; edx = xr34
	mov	ecx, [esp+12]	; ecx = end
	pxor	mm7, mm7	; srpow_max
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
	pfmax		mm7, mm4
	movq		mm1, mm5
	pfmax		mm7, mm5
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
	add	ecx, byte 4
	pfmul		mm4, mm2 		; - 1/4 * x (y^4)
	pfmul		mm5, mm2		; - 1/4 * x (y^4)
	pfadd		mm4, mm3		; 5/4 - 1/4 * x (y^4)
	pfadd		mm5, mm3		; 5/4 - 1/4 * x (y^4)
	pxor		mm2, mm2
	pfmul		mm4, mm0
	pfmul		mm5, mm1
	pfmax		mm4, mm2
	pfmax		mm5, mm2
	movq		[edx+ecx*4-16], mm4		; xr34
	movq		[edx+ecx*4- 8], mm5		; xr34
	jnz	.lp

	mov		eax, [esp+16]	; pmax
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
	xorps	xm2, xm2		; xm2 = srpow_max
	add	eax, ecx
	add	edx, ecx
	neg	ecx
	movaps	xm7, [Q_fm0p25]
	loopalign 16
.lp:
	movaps	xm0, [eax+ecx+ 0]
	movaps	xm1, [eax+ecx+16]
	andps	xm0, [Q_ABS]		; xm0 = |xr|
	andps	xm1, [Q_ABS]
	movaps	[edx+ecx+ 0+576*4], xm0 ; absxr[]
	movaps	[edx+ecx+16+576*4], xm1
	add	ecx, byte 8*4

	;------x^(3/4)------
	rsqrtps	xm4, xm0
	maxps	xm2, xm0
	movaps	xm5, xm0
	maxps	xm2, xm1
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
	jnz	.lp

	movhlps	xm4, xm2		;* * 3 2
	maxps	xm4, xm2		;* * 1 0
	mov	eax, [esp+_P+16]	; pmax
	movaps	xm2, xm4
	shufps	xm4, xm4, PACK(1,1,1,1)	;* * * 1
	maxps	xm4, xm2		;* * * 0
	movss	[eax], xm4
	ret

;*psum = sum_{i=0}^{i=n} x_i^2,
;	n should be even number and greater than or equal to 2.
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
	add	ecx, byte 2
	pfmul	mm0, mm0
	jz	.exit
	loopalign 16
.lp:
	movq	mm2, [eax+ecx*4+ 0]
	movq	mm3, [eax+ecx*4+ 8]
	pfmul	mm2, mm2
	pfmul	mm3, mm3
	add	ecx, byte 4
	pfadd	mm0, mm2
	pfadd	mm1, mm3
	jnz	.lp
	pfadd	mm0, mm1
.exit:
	movd	mm2, [edx]
	pfacc	mm0, mm0
	pfmul	mm0, mm2
	movd	[edx], mm0
	femms
	ret





	externdef	pow43
	externdef	pow20
	externdef	ipow20

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
	jz		.exit4

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
.exit4:
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

;
; FLOAT calc_sfb_noise_fast(FLOAT xr[576*2], int end, int bw, int sf)
;
proc	calc_sfb_noise_fast_3DN
%assign _P 12
	push		ebx
	push		edi
	push		ebp

	mov		eax, [esp+_P+4]  ; xr
	mov		edx, [esp+_P+8]  ; start
	lea		eax, [eax+edx*4]
	mov		edx, [esp+_P+16] ; scalefact
	movd		mm6, [ipow20+116*4+edx*4] ; sfpow34
	movd		mm7, [pow20+116*4+edx*4]  ; sfpow
	movq		mm2, [D_ROUNDFAC]
	mov		edx, [esp+_P+12] ; -n

	pxor		mm4, mm4
	pxor		mm1, mm1
	punpckldq	mm6, mm6
	punpckldq	mm7, mm7

	test		edx, 2
	jz		.lp4
	movq		mm4, [eax+ 0+ edx*4]
	pfmul		mm4, mm6
	pfadd		mm4, mm2
	pf2id		mm4, mm4
	movd		ebp, mm4
	punpckhdq	mm4, mm4
	movd		ecx, mm4

	movd		mm4, [pow43+ebp*4]
	punpckldq	mm4, [pow43+ecx*4]
	pfmul		mm4, mm7
	pfsubr		mm4, [eax+ 0+ edx*4 + 576*4]
	add		edx, byte 2
	pfmul		mm4, mm4

	loopalignK7	16
.lp4:
	pfadd		mm4, mm1
	movq		mm0, [eax+ 0+ edx*4]
	movq		mm1, [eax+ 8+ edx*4]
	pfmul		mm0, mm6
	pfmul		mm1, mm6
	pfadd		mm0, mm2
	pfadd		mm1, mm2
	pf2id		mm0, mm0
	pf2id		mm1, mm1
	movd		ebp, mm0
	movd		edi, mm1
	punpckhdq	mm0, mm0
	punpckhdq	mm1, mm1
	movd		ecx, mm0
	movd		ebx, mm1

	movd		mm0, [pow43+ebp*4]
	movd		mm1, [pow43+edi*4]
	punpckldq	mm0, [pow43+ecx*4]
	punpckldq	mm1, [pow43+ebx*4]
	pfmul		mm0, mm7
	pfmul		mm1, mm7
	pfsubr		mm0, [eax+ 0+ edx*4 + 576*4]
	pfsubr		mm1, [eax+ 8+ edx*4 + 576*4]
	add		edx, byte 4
	pfmul		mm0, mm0
	pfmul		mm1, mm1
	pfadd		mm4, mm0
	jnz		.lp4

	pfadd		mm4, mm1
	pfacc		mm4, mm4
	movd		eax, mm4
	push		eax
	femms
	fld		dword [esp]

	pop		eax
	pop		ebp
	pop		edi
	pop		ebx
	ret


;
; FLOAT calc_sfb_noise(FLOAT xr[576*2], int end, int bw, int sf)
;
proc	calc_sfb_noise_3DN
%assign _P 16
	push		ebx
	push		edi
	push		ebp
	push		esi

	mov		eax, [esp+_P+4]  ; xr
	mov		edx, [esp+_P+8]  ; end
	lea		eax, [eax+edx*4]
	mov		edx, [esp+_P+16] ; scalefact
	movd		mm6, [ipow20+116*4+edx*4] ; sfpow34
	movd		mm7, [pow20+116*4+edx*4]  ; sfpow
	mov		edx, [esp+_P+12]	; -n
	mov		esi, pow43

	pxor		mm4, mm4		; sum of noise
	punpckldq	mm6, mm6		; sfpow34 x 2
	punpckldq	mm7, mm7		; sfpow x 2

;  mm0, mm1, mm2, mm3 : general work

	test		edx, 2
	jz		.lp4
	movq		mm4, [eax+ 0+ edx*4]
	pfmul		mm4, mm6
	pf2id		mm2, mm4
	movd		ebp, mm2
	punpckhdq	mm2, mm2
	movd		ecx, mm2
	movd		mm2, [esi+ebp*4+8208*4]
	punpckldq	mm2, [esi+ecx*4+8208*4]	; mm2=adj43 value
	pfadd		mm2, mm4
	pf2id		mm2, mm2
	movd		ebp, mm2
	punpckhdq	mm2, mm2
	movd		ecx, mm2
	movd		mm4, [esi+ebp*4]
	punpckldq	mm4, [esi+ecx*4]
	pfmul		mm4, mm7
	pfsubr		mm4, [eax+ 0+ edx*4 + 576*4]
	add		edx, byte 2
	pfmul		mm4, mm4

	loopalignK7	16
.lp4:
	movq		mm0, [eax+ 0+ edx*4]
	movq		mm1, [eax+ 8+ edx*4]
	pfmul		mm0, mm6
	pfmul		mm1, mm6
	pf2id		mm2, mm0
	pf2id		mm3, mm1
	movd		ebp, mm2
	movd		edi, mm3
	punpckhdq	mm2, mm2
	punpckhdq	mm3, mm3
	movd		ecx, mm2
	movd		ebx, mm3
	movd		mm2, [esi+ebp*4+8208*4]
	movd		mm3, [esi+edi*4+8208*4]
	punpckldq	mm2, [esi+ecx*4+8208*4]	; mm2=adj43 value
	punpckldq	mm3, [esi+ebx*4+8208*4]	; mm3=adj43 value
	pfadd		mm0, mm2
	pfadd		mm1, mm3
	pf2id		mm0, mm0
	pf2id		mm1, mm1
	movd		ebp, mm0
	movd		edi, mm1
	punpckhdq	mm0, mm0
	punpckhdq	mm1, mm1
	movd		ecx, mm0
	movd		ebx, mm1

	movd		mm0, [esi+ebp*4]
	movd		mm1, [esi+edi*4]
	punpckldq	mm0, [esi+ecx*4]
	punpckldq	mm1, [esi+ebx*4]
	pfmul		mm0, mm7
	pfmul		mm1, mm7
	pfsubr		mm0, [eax+ 0+ edx*4 + 576*4]
	pfsubr		mm1, [eax+ 8+ edx*4 + 576*4]
	pfmul		mm0, mm0
	pfmul		mm1, mm1
	pfadd		mm4, mm0
	add		edx, byte 4
	pfadd		mm4, mm1
	jnz		.lp4
.exit:
	pfacc		mm4, mm4
	movd		eax, mm4
	femms
	push		eax
	fld		dword [esp]

	pop		eax

	pop		esi
	pop		ebp
	pop		edi
	pop		ebx
	ret


;
; void qnatize_sfb_3DN(FLOAT xr34[], int end, int bw, int sf, int l3_enc[])
;
proc	quantize_sfb_3DN
%assign _P 8
	push		edi
	push		ebp

	mov		eax, [esp+_P+4]  ; xr[]
	mov		ecx, [esp+_P+16] ; l3_enc[]
	mov		edx, [esp+_P+12] ; scalefact
	movq		mm5, [D_IXMAXVAL]
	movd		mm4, [ipow20+116*4+edx*4] ; sfpow34
	mov		edx, [esp+_P+8]	; -n
	punpckldq	mm4, mm4		; sfpow34 x 2

;  mm0, mm1, mm2, mm3 : general work

	test		edx, 2
	jz		.lp4
	movq		mm0, [eax+ 0+ edx*4]
	pfmul		mm0, mm4
	pfmin		mm0, mm5
	pf2id		mm2, mm0
	movd		ebp, mm2
	punpckhdq	mm2, mm2
	movd		edi, mm2
	add		edx, byte 2
	movd		mm2, [pow43+8208*4+ebp*4]
	punpckldq	mm2, [pow43+8208*4+edi*4]	; mm2=adj43 value
	pfadd		mm2, mm0
	pf2id		mm2, mm2
	movq		[ecx - 8+ 0+ edx*4], mm2
	jz		.exit
	loopalignK7	16
.lp4:
	movq		mm0, [eax+ 0+ edx*4]
	movq		mm1, [eax+ 8+ edx*4]
	pfmul		mm0, mm4
	pfmul		mm1, mm4
	pfmin		mm0, mm5
	pfmin		mm1, mm5
	pf2id		mm2, mm0
	pf2id		mm3, mm1
	movd		ebp, mm2
	movd		edi, mm3
	punpckhdq	mm2, mm2
	punpckhdq	mm3, mm3
	movd		mm6, [pow43+8208*4+ebp*4]
	movd		mm7, [pow43+8208*4+edi*4]
	movd		ebp, mm2
	movd		edi, mm3
	add		edx, byte 4
	punpckldq	mm6, [pow43+8208*4+ebp*4]	; mm6=adj43 value
	punpckldq	mm7, [pow43+8208*4+edi*4]	; mm7=adj43 value
	pfadd		mm0, mm6
	pfadd		mm1, mm7
	pf2id		mm0, mm0
	pf2id		mm1, mm1
	movq		[ecx -16 + 0+ edx*4], mm0
	movq		[ecx -16 + 8+ edx*4], mm1
	jnz		.lp4
.exit:
	femms
	pop		ebp
	pop		edi
	ret


;
; void qnatize_sfb_3DN(FLOAT xr34end[], int -bw, int sf, int l3enc_end[],
;                      int len01)
;
proc	quantize_ISO_3DN
%assign _P 0
	mov		eax, [esp+_P+4]  ; xend[]
	mov		ecx, [esp+_P+16] ; l3_enc[]
	mov		edx, [esp+_P+12] ; scalefact
	movq		mm5, [D_ROUNDFAC]
	movd		mm4, [ipow20+116*4+edx*4] ; sfpow34
	mov		edx, [esp+_P+8]	; -n
	test		edx, edx
	jz		.exit4
	punpckldq	mm4, mm4		; sfpow34 x 2

;  mm0, mm1, mm2, mm3 : general work

	test		edx, 2
	jz		.lp4
	movq		mm0, [eax+ 0+ edx*4]
	add		edx, byte 2
	pfmul		mm0, mm4
	pfadd		mm0, mm5
	pf2id		mm0, mm0
	movq		[ecx - 8+ 0+ edx*4], mm0
	jz		.exit4
	loopalignK7	16
.lp4:
	movq		mm0, [eax+ 0+ edx*4]
	movq		mm1, [eax+ 8+ edx*4]
	add		edx, byte 4
	pfmul		mm0, mm4
	pfmul		mm1, mm4
	pfadd		mm0, mm5
	pfadd		mm1, mm5
	pf2id		mm0, mm0
	pf2id		mm1, mm1
	movq		[ecx -16 + 0+ edx*4], mm0
	movq		[ecx -16 + 8+ edx*4], mm1
	jnz		.lp4
.exit4:
	pfrcp		mm0, mm4
	mov		edx, [esp+_P+20]
	lea		ecx, [ecx+edx*4]
	lea		eax, [eax+edx*4]
	neg		edx
	jz		.exit2
	pfrcpit1	mm4, mm0
	pfrcpit2	mm4, mm0
	punpckldq	mm4, mm4	; (1.0 / sfpow34) x 2
	pfmul		mm4, [D_1ROUNDFAC]
.lp2:
	movq		mm0, [eax+ 0+ edx*4]
	add		edx, byte 2
	pcmpgtd		mm0, mm4
	psrld		mm0, 31
	movq		[ecx - 8 + 0+ edx*4], mm0
	jnz		.lp2
.exit2
	femms
	ret

;
; void qnatize_ISO_SSE(FLOAT xr34end[], int -bw, int sf, int l3enc_end[])
;
proc	quantize_ISO_SSE
%assign _P 0
	mov		edx, [esp+_P+12]
	movss		xm0, [ipow20+116*4+edx*4] ; sfpow34
	mov		eax, [esp+_P+4]			;xrend
	mov		edx, [esp+_P+16]		;ixend
	movss		xm1, [ROUNDFAC_NEAR]
	mov		ecx, [esp+_P+8]		; bw
	movss		xm6, [magicfloat]
	movss		xm7, [maskpattern]
	shufps		xm0, xm0, 0			;xm0 = [istep]
	shufps		xm1, xm1, 0			;xm1 = [0.4054]
	shufps		xm6, xm6, 0			;xm6 = magic float
	shufps		xm7, xm7, 0			;xm7 = mask pattern
.lp:
	movaps		xm2, [eax+4* 0 + ecx*4]
	movaps		xm3, [eax+4* 4 + ecx*4]
	movaps		xm4, [eax+4* 8 + ecx*4]
	movaps		xm5, [eax+4*12 + ecx*4]
	add		ecx, byte 16
	mulps		xm2, xm0
	mulps		xm3, xm0
	mulps		xm4, xm0
	mulps		xm5, xm0
	addps		xm2, xm1
	addps		xm3, xm1
	addps		xm4, xm1
	addps		xm5, xm1
	addps		xm2, xm6
	addps		xm3, xm6
	addps		xm4, xm6
	addps		xm5, xm6
	andps		xm2, xm7
	andps		xm3, xm7
	andps		xm4, xm7
	andps		xm5, xm7
	movaps		[edx+4* 0 + ecx*4-64], xm2
	movaps		[edx+4* 4 + ecx*4-64], xm3
	movaps		[edx+4* 8 + ecx*4-64], xm4
	movaps		[edx+4*12 + ecx*4-64], xm5
	jnz		.lp
	ret

;
; void qnatize_ISO_SSE2(FLOAT xr34end[], int -bw, int sf, int l3enc_end[])
;
proc	quantize_ISO_SSE2
%assign _P 0
	mov		edx, [esp+_P+12]
	movss		xm0, [ipow20+116*4+edx*4] ; sfpow34
	mov		eax, [esp+_P+4]			;xrend
	mov		edx, [esp+_P+16]		;ixend
	movss		xm1, [D_ROUNDFAC]
	mov		ecx, [esp+_P+8]		; bw
	shufps		xm0, xm0, 0			;xm0 = [istep]
	shufps		xm1, xm1, 0			;xm1 = [0.4054]
.lp:
	movaps		xm2, [eax+4* 0 + ecx*4]
	movaps		xm3, [eax+4* 4 + ecx*4]
	movaps		xm4, [eax+4* 8 + ecx*4]
	movaps		xm5, [eax+4*12 + ecx*4]
	add		ecx, byte 16
	mulps		xm2, xm0
	mulps		xm3, xm0
	mulps		xm4, xm0
	mulps		xm5, xm0
	addps		xm2, xm1
	addps		xm3, xm1
	addps		xm4, xm1
	addps		xm5, xm1
	cvttps2dq	xm2, xm2
	cvttps2dq	xm3, xm3
	cvttps2dq	xm4, xm4
	cvttps2dq	xm5, xm5
	movaps		[edx+4* 0 + ecx*4-64], xm2
	movaps		[edx+4* 4 + ecx*4-64], xm3
	movaps		[edx+4* 8 + ecx*4-64], xm4
	movaps		[edx+4*12 + ecx*4-64], xm5
	jnz		.lp
	ret

;
; void lr2ms(float l[], float r[], int len)
;
proc	lr2ms_SSE
%assign _P 16
	push	ebp
	movaps	xm3, [Q_sqrt2]
	push	edi
	push	esi
	push	ebx
	mov	eax, [esp+_P+4]		; eax = l
	mov	ecx, [esp+_P+8]		; ecx = r
	mov	edx, [esp+_P+12]	; edx = len
	mov	ebx, [eax+edx*4-4]
	mov	esi, [eax+edx*4-8]
	mov	edi, [ecx+edx*4-4]
	mov	ebp, [ecx+edx*4-8]
.lp:
	movaps	xm0, [eax]
	movaps	xm2, [ecx]
	movaps	xm1, xm0
	addps	xm0, xm2
	subps	xm1, xm2
	mulps	xm0, xm3
	mulps	xm1, xm3
	movaps	[eax], xm0
	movaps	[ecx], xm1
	add	eax, byte 16
	add	ecx, byte 16
	sub	edx, byte 4
	jg	.lp
	je	.end
	mov	[eax-12], ebx
	mov	[eax-16], esi
	mov	[ecx-12], edi
	mov	[ecx-16], ebp
.end:
	pop	ebx
	pop	esi
	pop	edi
	pop	ebp
	ret
