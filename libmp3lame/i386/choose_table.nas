; new count bit routine
;	part of this code is origined from
;	new GOGO-no-coda (1999, 2000)
;	Copyright (C) 1999 shigeo
;	modified by Keiichi SAKAI

%include "nasm.h"

	globaldef	choose_table_MMX
	globaldef	MMX_masking

	externdef	largetbl
	externdef	table13
	externdef	table7B89
	externdef	tableDxEF
	externdef	table5967

	segment_data
	align	16
D14_14_14_14	dd	0x000E000E, 0x000E000E
D15_15_15_15	dd	0xfff0fff0, 0xfff0fff0
mul_add		dd	0x00010010, 0x00010010
mul_add13	dd	0x00010002, 0x00010002
mul_add56	dd	0x00010004, 0x00010004

linbits32
	dd	0x40004,0x10001,0x40004,0x20002,0x40004,0x30003,0x40004,0x40004
	dd	0x50005,0x60006,0x60006,0x60006,0x70007,0x80008,0x80008,0x80008
	dd	0x90009,0xa000a,0xb000b,0xa000a,0xb000b,0xd000d,0xd000d,0xd000d
	dd	0xd000d,0xd000d

choose_table_H
	dw	0x1810, 0x1811, 0x1812, 0x1813, 0x1914, 0x1a14, 0x1b15, 0x1c15
	dw	0x1d16, 0x1e16, 0x1e17, 0x1f17, 0x1f17

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

	segment_text
;
; use MMX
;

	align	16
; int choose_table(int *ix, int *end, int *s)
choose_table_MMX:
	mov	ecx, [esp+4]	;ecx = begin
	mov	edx, [esp+8]	;edx = end
	mov	eax, [esp+12]
	mov	eax, [eax]	;eax = *s
	test	eax, eax
	jge	.ixmax_end

	sub	ecx, edx	;ecx = begin-end(should be minus)
	test	ecx, 8
 	pxor	mm0, mm0	;mm0=[0:0]
	movq	mm1, [edx+ecx]
	jz	.lp

	add	ecx,8
	jz	.exit

	loopalign	16
.lp:
	movq	mm4, [edx+ecx]
	movq	mm5, [edx+ecx+8]
	add	ecx, 16
; below operations should be done as dword (32bit),
; an MMX has no such instruction, however.
; but! because the maximum value of IX is 8191+15,
; we can safely use "word" operation.
	psubusw	mm4, mm0
	psubusw	mm5, mm1
	paddw	mm0, mm4
	paddw	mm1, mm5
	jnz	.lp
.exit:
	psubusw	mm1, mm0
	paddw	mm0, mm1

	movq	mm4, mm0
	punpckhdq	mm4, mm4
	psubusw	mm4, mm0
	paddw	mm0, mm4
	movd	eax, mm0
.ixmax_end:
	cmp	eax, 15
	ja	with_ESC
	jmp	[choose_jump_table_L+eax*4]

table_MMX.L_case_0:
with_ESC1:
	emms
	mov	ecx, [esp+12]	; *s
	mov	[ecx], eax
	ret

with_ESC:
	cmp	eax, 8191+15
	ja	with_ESC1	; error !

	sub	eax, 15
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
	pxor	mm7, mm7	; linbits_sum, number of IXs over 14
	jz	.H_dual_lp1

	movq	mm0, [edx+ecx]
	add	ecx,8
	packssdw	mm0,mm7
	movq	mm2, mm0
	paddusw	mm0, mm5	; mm0 = min(ix, 15)+0xfff0
	pcmpgtw	mm2, mm6	; over 14 or not
	psubw	mm7, mm2	; if (over 14) then linbits_sum++
	pmaddwd	mm0, mm3	; {0, 0, y, x}*{1, 16, 1, 16}
	movd	ebx, mm0
	mov	esi, [largetbl+ebx*4+(16*16+16)*4]

	jz	.H_dual_exit

	loopalign	16
.H_dual_lp1:
	movq	mm0, [edx+ecx]
	movq	mm1, [edx+ecx+8]
	packssdw	mm0,mm1
	movq	mm2, mm0
	paddusw	mm0, mm5	; mm0 = min(ix, 15)+0xfff0
	pcmpgtw	mm2, mm6	; over 14 or not.
	pmaddwd	mm0, mm3	; {y, x, y, x}*{1, 16, 1, 16}
	movd	ebx, mm0
	punpckhdq	mm0,mm0
	add	esi, [largetbl+ebx*4+(16*16+16)*4]
	movd	ebx, mm0
	add	esi, [largetbl+ebx*4+(16*16+16)*4]
	add	ecx, 16
	psubw	mm7, mm2	; if (over 14) then linbits_sum++
	jnz	.H_dual_lp1

.H_dual_exit:
	movq	mm1,mm7
	punpckhdq	mm7,mm7
	paddd	mm7,mm1
	punpckldq	mm7,mm7

	pmaddwd	mm7, [linbits32+eax*8]	; linbits
	mov	dx, [choose_table_H+eax*2]

	movd	ecx, mm7
	punpckhdq	mm7,mm7
	movd	eax, mm7
	emms
	shl	eax, 16
	add	ecx, eax

	add	ecx, esi

	pop	esi
	pop	ebx

	mov	eax, ecx
	and	ecx, 0xffff	; ecx = sum2
	shr	eax, 16	; eax = sum

	cmp	eax, ecx
	jle	.chooseE_s1
	mov	eax, ecx
	shr	edx, 8
.chooseE_s1:
	mov	ecx, [esp+12] ; *s
	and	edx, 0xff
	mov	[ecx], edx
	ret

table_MMX.L_case_2:
	push	eax	; dword 2
	mov	ecx, table7B89+96*8
	jmp	near table.from4

table_MMX.L_case_3:
	push	dword 5
	mov	ecx, table5967
	movq	mm5, [mul_add56]
	jmp	near table.from4_2

table_MMX.L_case_45:
	push	dword 7
	mov	ecx, table7B89
	jmp	near table.from4

table_MMX.L_case_67:
	push	dword 10
	mov	ecx, table7B89+8*8
	jmp	near table.from4

table_MMX.L_case_8_15:
	push	dword 13
	mov	ecx, tableDxEF
table.from4:
	movq	mm5, [mul_add]
table.from4_2:
	mov	edx, [esp+8]	;edx = *begin
	mov	eax, [esp+12]	;eax = *end
	push	ebx
	sub	edx, eax

	pxor	mm2, mm2	;mm2 = sum

	test	edx, 8
	jz	.from4_lp1
; odd length
	movq	mm0,[eax+edx]	;mm0 = ix[0] | ix[1]
	add	edx,8
	packssdw	mm0,mm2

	pmaddwd	mm0,mm5
	movd	ebx,mm0

	movq	mm2,  [ecx+ebx*8]

	jz	.from4_exit

	loopalign	16
.from4_lp1
	movq	mm0,[eax+edx]
	movq	mm1,[eax+edx+8]
	add	edx,16
	packssdw	mm0,mm1 ;mm0 = ix[0]|ix[1]|ix[2]|ix[3]
	pmaddwd	mm0,mm5
	movd	ebx,mm0
	punpckhdq	mm0,mm0
	paddd	mm2, [ecx+ebx*8]
	movd	ebx,mm0
	paddd	mm2, [ecx+ebx*8]
	jnz	.from4_lp1
.from4_exit
;	xor	edx,edx
	movd	eax, mm2
	mov	ebx, eax
	shr	ebx, 16
	and	eax, 0xffff
	cmp	eax, ebx
	jle	near .from4_s3
	mov	eax, ebx
	cmp	ecx, table7B89+8*8
	sbb	edx, -5
	cmp	ecx, table7B89+96*8
	sbb	edx, -1
.from4_s3:
	punpckhdq	mm2,mm2
	movd	ebx, mm2	; eax = sum
	mov	ecx, ebx
	and	ecx, 0xffff	; ecx = sum2
	shr	ebx, 16	; ebx = sum1

	cmp	eax, ebx
	jle	near .from4_s1
	mov	eax, ebx
	mov	edx, 1
.from4_s1:
	emms
	pop	ebx
	cmp	eax, ecx
	jle	.from4_s2
	mov	eax, ecx
	mov	edx, 2
.from4_s2:
	pop	ecx
	add	edx, ecx
	mov	ecx, [esp+12] ; *s
	mov	[ecx], edx
	ret

table_MMX.L_case_1:
	movq	mm5, [mul_add13]
	mov	eax, [esp+4]	;eax = *begin
;	mov	edx, [esp+8]	;edx = *end
	push	ebx

	sub	eax, edx
;	xor	ecx, ecx
	test	eax, 8
	jz	.from2_lp1
; odd length
	movq	mm0, [edx+eax]	;mm0 = ix[0] | ix[1]
	pxor	mm2, mm2		;mm2 = sum
	packssdw	mm0,mm2

	pmaddwd	mm0, mm5
	movd	ebx, mm0

	mov	ecx,  [table13+ebx*4]

	add	eax, 8
	jz	.from2_exit

	loopalign	16
.from2_lp1
	movq	mm0, [edx+eax]
	movq	mm1, [edx+eax+8]
	packssdw	mm0,mm1 ;mm0 = ix[0]|ix[1]|ix[2]|ix[3]
	pmaddwd	mm0,mm5
	movd	ebx,mm0
	punpckhdq	mm0,mm0
	add	ecx, [table13+ebx*4]
	movd	ebx, mm0
	add	ecx, [table13+ebx*4]
	add	eax, 16
	jnc	.from2_lp1
.from2_exit
	pop	ebx
	emms

	mov	edx, 1
	mov	eax, ecx
	and	ecx, 0xffff	; ecx = sum2
	shr	eax, 16	; edx = sum1

	cmp	eax, ecx
	jle	.from2_s1
	mov	eax, ecx
	or	dl, 2
.from2_s1:
	mov	ecx, [esp+12] ; *s
	mov	[ecx], edx
	ret

	end
