; new count bit routine
;	part of this code is origined from
;	new GOGO-no-coda (1999, 2000)
;	Copyright (C) 1999 shigeo
;	modified by Keiichi SAKAI

%include "nasm.h"

	globaldef	choose_table_MMX
	globaldef	MMX_masking

	externdef	largetbl
	externdef	t1l
	externdef	table23
	externdef	table56
	externdef	tableABC
	externdef	tableDEF
	externdef	linbits32
	externdef	choose_table_H

	segment_data
	align	16
D14_14_14_14	dd	0x000E000E, 0x000E000E
D15_15_15_15	dd	0xfff0fff0, 0xfff0fff0
mul_add		dd	0x00010010, 0x00010010
mul_add23	dd	0x00010003, 0x00010003
mul_add56	dd	0x00010004, 0x00010004

choose_jump_table_L:
	dd	table_MMX.L_case_0
	dd	table_MMX.L_case_1
	dd	table_MMX.L_case_2
	dd	table_MMX.L_case_3
	dd	table_MMX.L_case_45
	dd	table_MMX.L_case_45
	dd	table_MMX.L_case_67
	dd	table_MMX.L_case_67
	dd	table_MMX.L_case_8_15
	dd	table_MMX.L_case_8_15
	dd	table_MMX.L_case_8_15
	dd	table_MMX.L_case_8_15
	dd	table_MMX.L_case_8_15
	dd	table_MMX.L_case_8_15
	dd	table_MMX.L_case_8_15
	dd	table_MMX.L_case_8_15

	segment_code
;
; use MMX
;

	align	16
; int choose_table(int *ix, int *end, int *s)
choose_table_MMX:
	mov	ecx,[esp+4]	;ecx = begin
	mov	edx,[esp+8]	;edx = end
	xor	eax,eax
	sub	ecx,edx		;ecx = begin-end(should be minus)
	test	ecx,8
 	pxor	mm0,mm0		;mm0=[0:0]
	movq	mm1,[edx+ecx]
	jz	.lp

	add	ecx,8
	jz	.exit

	align	4
.lp:
	movq	mm4,[edx+ecx]
	movq	mm5,[edx+ecx+8]
	add	ecx,16
	psubusw	mm4,mm0	; 本当は dword でないといけないのだが
	psubusw	mm5,mm1	; そんなコマンドはない :-p
	paddw	mm0,mm4 ; が, ここで扱う値の範囲は 8191+15 以下なので問題ない
	paddw	mm1,mm5
	jnz	.lp
.exit:
	psubusw	mm1,mm0	; これも本当は dword でないといけない
	paddw	mm0,mm1

	movq	mm4,mm0
	punpckhdq	mm4,mm4
	psubusw	mm4,mm0	; これも本当は dword でないといけない
	paddw	mm0,mm4
	movd	eax,mm0

	cmp	eax,15
	ja	.with_ESC
	jmp	[choose_jump_table_L+eax*4]

.with_ESC1:
	emms
	mov	ecx, [esp+12]	; *s
	mov	[ecx], eax
	or	eax,-1
	ret

.with_ESC:
	cmp	eax, 8191+15
	ja	.with_ESC1

	sub	eax,15
	push	ebx
	push	esi
	bsr	eax, eax
%assign _P 4*2
	movq    mm5, [D15_15_15_15]
	movq	mm6, [D14_14_14_14]
	movq	mm3, [mul_add]

	mov	ecx, [esp+_P+4]		; = ix
;	mov	edx, [esp+_P+8]		; = end
	sub	ecx, edx

	xor	esi, esi	; sum = 0
	test    ecx, 8
	pxor	mm7, mm7	; linbits_sum, 14を越えたものの数
	jz	.H_dual_lp1

	movq	mm0, [edx+ecx]
	add	ecx,8
	packssdw	mm0,mm7
	movq	mm2, mm0
	paddusw	mm0, mm5	; mm0 = min(ix, 15)+0xfff0
	pcmpgtw	mm2, mm6	; 14より大きいか？
	psubw	mm7, mm2	; 14より大きいとき linbits_sum++;
	pmaddwd	mm0, mm3	; {0, 0, y, x}*{1, 16, 1, 16}
	movd	ebx, mm0
	mov	esi, [largetbl+ebx*4+(16*16+16)*4]

	jz	.H_dual_exit

	align   4
.H_dual_lp1:
	movq	mm0, [edx+ecx]
	movq	mm1, [edx+ecx+8]
	packssdw	mm0,mm1
	movq	mm2, mm0
	paddusw	mm0, mm5	; mm0 = min(ix, 15)+0xfff0
	pcmpgtw	mm2, mm6	; 14より大きいか？
	pmaddwd	mm0, mm3	; {y, x, y, x}*{1, 16, 1, 16}
	movd	ebx, mm0
	punpckhdq	mm0,mm0
	add	esi, [largetbl+ebx*4+(16*16+16)*4]
	movd	ebx, mm0
	add	esi, [largetbl+ebx*4+(16*16+16)*4]
	add	ecx, 16
	psubw	mm7, mm2	; 14より大きいとき linbits_sum++;
	jnz	.H_dual_lp1

.H_dual_exit:
	pmov	mm1,mm7
	punpckhdq	mm7,mm7
	paddd	mm7,mm1
	punpckldq	mm7,mm7

	pmaddwd	mm7, [linbits32+eax*8]	; linbits
	mov	ax, [choose_table_H+eax*2]

	movd	ecx, mm7
	punpckhdq	mm7,mm7
	movd	edx,mm7
	emms
	shl	edx, 16
	add	ecx, edx

	add	ecx, esi

	pop	esi
	pop	ebx

	mov	edx, ecx
	and	ecx, 0xffff	; ecx = sum2
	shr	edx, 16	; edx = sum

	cmp	edx, ecx
	jle	.chooseE_s1
	mov	edx, ecx
	shr	eax, 8
.chooseE_s1:
	mov	ecx, [esp+12] ; *s
	and	eax, 0xff
	add	[ecx], edx
	ret

table_MMX.L_case_0:
	emms
	ret

table_MMX.L_case_1:
	emms
	mov	eax, [esp+12] ; *s
	mov	ecx, [esp+4] ; *ix
	sub	ecx, edx
	push	ebx
.lp:
	mov	ebx, [edx+ecx]
	add	ebx, ebx
	add	ebx, [edx+ecx+4]
	movzx	ebx, byte [ebx+t1l]
	add	[eax], ebx
	add	ecx, 8
	jnz	.lp
	pop	ebx
	mov	eax, 1
	ret

table_MMX.L_case_45:
	push	dword 7
	mov	ecx, tableABC+9*8
	jmp	from3

table_MMX.L_case_67:
	push	dword 10
	mov	ecx, tableABC
	jmp	from3

table_MMX.L_case_8_15:
	push	dword 13
	mov	ecx, tableDEF
from3:
	mov	eax,[esp+8]	;eax = *begin
;	mov	edx,[esp+12]	;edx = *end

	push	ebx
	sub	eax, edx

	movq	mm5,[mul_add]
	pxor	mm2,mm2	;mm2 = sum

	test	eax, 8
	jz	.choose3_lp1
; odd length
	movq	mm0,[edx+eax]	;mm0 = ix[0] | ix[1]
	add	eax,8
	packssdw	mm0,mm2

	pmaddwd	mm0,mm5
	movd	ebx,mm0

	movq	mm2,  [ecx+ebx*8]

	jz	.choose3_exit

	align	4
.choose3_lp1
	movq	mm0,[edx+eax]
	movq	mm1,[edx+eax+8]
	add	eax,16
	packssdw	mm0,mm1 ;mm0 = ix[0]|ix[1]|ix[2]|ix[3]
	pmaddwd	mm0,mm5
	movd	ebx,mm0
	punpckhdq	mm0,mm0
	paddd	mm2, [ecx+ebx*8]
	movd	ebx,mm0
	paddd	mm2, [ecx+ebx*8]
	jnz	.choose3_lp1
.choose3_exit
;	xor	eax,eax
	movd	ebx, mm2
	punpckhdq	mm2,mm2
	mov	ecx, ebx
	and	ecx, 0xffff	; ecx = sum2
	shr	ebx, 16	; ebx = sum1
	movd	edx, mm2	; edx = sum

	cmp	edx, ebx
	jle	.choose3_s1
	mov	edx, ebx
	inc	eax
.choose3_s1:
	emms
	pop	ebx
	cmp	edx, ecx
	jle	.choose3_s2
	mov	edx, ecx
	mov	eax, 2
.choose3_s2:
	pop	ecx
	add	eax, ecx
	mov	ecx, [esp+12] ; *s
	add	[ecx], edx
	ret

table_MMX.L_case_2:
	push	dword 2
	mov	ecx,table23
	pmov	mm5,[mul_add23]
	jmp	from2
table_MMX.L_case_3:
	push	dword 5
	mov	ecx,table56
	pmov	mm5,[mul_add56]
from2:
	mov	eax,[esp+8]	;eax = *begin
;	mov	edx,[esp+12]	;edx = *end
	push	ebx
	push	edi

	sub	eax, edx
	xor	edi, edi
	test	eax, 8
	jz	.choose2_lp1
; odd length
	movq	mm0,[edx+eax]	;mm0 = ix[0] | ix[1]
	pxor	mm2,mm2		;mm2 = sum
	packssdw	mm0,mm2

	pmaddwd	mm0,mm5
	movd	ebx,mm0

	mov	edi,  [ecx+ebx*4]

	add	eax,8
	jz	.choose2_exit

	align	4
.choose2_lp1
	movq	mm0,[edx+eax]
	movq	mm1,[edx+eax+8]
	packssdw	mm0,mm1 ;mm0 = ix[0]|ix[1]|ix[2]|ix[3]
	pmaddwd	mm0,mm5
	movd	ebx,mm0
	punpckhdq	mm0,mm0
	add	edi, [ecx+ebx*4]
	movd	ebx, mm0
	add	edi, [ecx+ebx*4]
	add	eax,16
	jnc	.choose2_lp1
.choose2_exit
	mov	ecx, edi
	pop	edi
	pop	ebx
	pop	eax ; table num.
	emms

	mov	edx, ecx
	and	ecx, 0xffff	; ecx = sum2
	shr	edx, 16	; edx = sum1

	cmp	edx, ecx
	jle	.choose2_s1
	mov	edx, ecx
	inc	eax
.choose2_s1:
	mov	ecx, [esp+12] ; *s
	add	[ecx], edx
	ret

	end
