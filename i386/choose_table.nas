; new count bit routine
;	part of this code is origined from
;	new GOGO-no-coda (1999, 2000)
;	Copyright (C) 1999 shigeo
;	modified by Keiichi SAKAI

%include "nasm.h"

	globaldef	choose_table_from3_MMX

	segment_data

D14_14		dd	0x000E000E, 0x000E000E
mul_add		dd	0x00010010, 0x00010010

	segment_code
;
; use MMX
;
	align	16
choose_table_from3_MMX:
%assign _P 4
	mov	eax,[esp+_P+0]	;eax = *begin
	mov	ecx,[esp+_P+4]	;ecx = *end
	mov	edx,[esp+_P+16]  ;edx = *hlen

	push	ebx

	movq	mm5,[mul_add]
	pxor	mm2,mm2		;mm2 = sum

	sub	eax, ecx
	test	eax, 8
	jz	.choose3_lp1
; odd length
	movq	mm0,[ecx+eax]	;mm0 = ix[0] | ix[1]
	packssdw	mm0,mm2

	pmaddwd	mm0,mm5
	movd	ebx,mm0

	movq	mm2,  [edx+ebx*8]

	add	eax,8
	jz	.choose3_exit

	align	4
.choose3_lp1
	movq	mm0,[ecx+eax]
	movq	mm1,[ecx+eax+8]
	packssdw	mm0,mm1 ;mm0 = ix[0]|ix[1]|ix[2]|ix[3]
	pmaddwd	mm0,mm5
	movd	ebx,mm0
	punpckhdq	mm0,mm0
	paddd	mm2, [edx+ebx*8]
	movd	ebx,mm0
	add	eax,16
	paddd	mm2, [edx+ebx*8]
	jnz	.choose3_lp1
.choose3_exit
;	xor	eax,eax
	movd	ebx, mm2
	punpckhdq	mm2,mm2
	mov	edx, ebx
	and	edx, 0xffff	; edx = sum2
	shr	ebx, 16		; ebx = sum1
	movd	ecx, mm2	; ecx = sum

	cmp	ecx, ebx
	jle	.choose3_s1
	mov	ecx, ebx
	inc	eax
.choose3_s1:
	emms
	pop	ebx
	cmp	ecx, edx
	jle	.choose3_s2
	mov	ecx, edx
	mov	eax, 2
.choose3_s2:
	add	eax, [esp+_P+8]
	mov	edx, [esp+_P+12] ; *s
	add	[edx], ecx
	ret

	end
