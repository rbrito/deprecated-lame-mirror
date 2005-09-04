; ix_max()
;	part of this code is origined from
;	new GOGO-no-coda (1999, 2000)
;	Copyright (C) 1999 shigeo
;	modified by Keiichi SAKAI

;	Copyright 2004 Takehiro TOMINAGA/LAME Project

%include "nasm.h"

;
; int ix_max_MMX(int *ix, int *end)
;
	segment_code
	proc	ix_max_MMX
	mov	ecx, [esp+4]	;ecx = begin
	mov	edx, [esp+8]	;edx = end

	movq	mm0, [ecx]
	sub	ecx, edx	;ecx = begin-end(should be minus)
	add	ecx, byte 8
	jz	.exit
	and	ecx, byte 0xf0
 	pxor	mm1, mm1	;mm1=[0:0]

	loopalign	16
.lp:
	movq	mm4, [edx+ecx]
	movq	mm5, [edx+ecx+8]
	add	ecx, byte 16
; below operations should be done as dword (32bit),
; but an MMX has no such instruction.
; but! because the maximum value of IX is 8191+15,
; we can safely use "word(16bit)" operation.
	psubusw	mm4, mm0
	psubusw	mm5, mm1
	paddusw	mm0, mm4
	paddusw	mm1, mm5
	jnz	.lp

	psubusw	mm1, mm0
	paddusw	mm0, mm1
.exit:
	movq	mm4, mm0
	punpckhdq	mm4, mm4
	psubusw	mm4, mm0
	paddw	mm0, mm4
	movd	eax, mm0
	emms
	ret

;
; int xrmax_MMX(float *end, int -len)
;
	proc	xrmax_MMX
	mov	ecx, [esp+8]	;ecx = -l
	mov	edx, [esp+4]	;edx = end

	movq	mm0, [edx+ecx*4]
	pxor	mm1, mm1
	add	ecx, byte 2
	and	ecx, byte 0xfffffffc

	loopalign	16
.lp:
	movq	mm4,[edx+ecx*4]
	movq	mm5,[edx+ecx*4+8]
	add	ecx, byte 4
	movq	mm2,mm0
	movq	mm3,mm1
	pcmpgtd	mm2,mm4
	pcmpgtd	mm3,mm5
	pand	mm0,mm2
	pand	mm1,mm3
	pandn	mm2,mm4
	pandn	mm3,mm5
	por	mm0,mm2
	por	mm1,mm3
	jnz	.lp

	movq	mm2,mm0
	pcmpgtd	mm2,mm1
	pand	mm0,mm2
	pandn	mm2,mm1
	por	mm0,mm2

	movq	mm1, mm0
	punpckhdq	mm1, mm1
	movq	mm2,mm0
	pcmpgtd	mm2,mm1
	pand	mm0,mm2
	pandn	mm2,mm1
	por	mm0,mm2

	movd	eax, mm0
	emms
	mov	[esp+4], eax
	fld	dword [esp+4]
	ret

;
; float xrmax_SSE(float *end, int -len)
;
	proc	xrmax_SSE
	mov	ecx, [esp+4]	;ecx = end
	xorps	xmm1, xmm1
	mov	edx, [esp+8]	;edx = -len

	test	ecx, 8
	je	.evenend
	sub	ecx, 8
	add	edx, 2
	movhps	xmm1, [ecx]
.evenend:
	test	edx, 2
	je	.evenlen
	movlps	xmm1, [ecx+edx*4]
	add	edx, 2
.evenlen:
	test	edx, edx
	je	.end
.loop:
	movaps	xmm0, [ecx+edx*4]
	add	edx, byte 4
	maxps	xmm1, xmm0
	js	.loop
.end:
	movhlps	xmm0, xmm1
	maxps	xmm1, xmm0
	movaps	xmm0, xmm1
	shufps	xmm0, xmm0, R4(1,1,1,1)
	maxss	xmm0, xmm1
	movss	[esp+4], xmm0
	fld	dword [esp+4]
	ret

;
; int ixmax_SSE2(int *ix, int *end)
;
	proc	ix_max_SSE2
	mov	ecx, [esp+4]	;ecx = begin
	mov	edx, [esp+8]	;edx = end

	movq	xmm0, [ecx]
	add	ecx, byte 8
	movq	xmm1, [edx-8]
	psubusw	xmm0, xmm1
	paddusw	xmm0, xmm1
	and	ecx, byte 0xfffffff0
	and	edx, byte 0xfffffff0
	sub	ecx, edx	;ecx = begin-end(should be minus)
	jz	.exit

	loopalign	16
.lp:
	movdqa	xmm1, [edx+ecx]
	add	ecx, byte 16
; below operations should be done as dword (32bit),
; but an MMX has no such instruction.
; but! because the maximum value of IX is 8191+15,
; we can safely use "word(16bit)" operation.
	psubusw	xmm0, xmm1
	paddusw	xmm0, xmm1
	jnz	.lp

	movdqa	xmm1, xmm0
	psrldq	xmm0, 8
	psubusw	xmm0, xmm1
	paddusw	xmm0, xmm1
.exit:
	movdqa	xmm1, xmm0
	psrldq	xmm0, 4
	psubusw	xmm0, xmm1
	paddusw	xmm0, xmm1
	movd	eax, xmm0
	ret
