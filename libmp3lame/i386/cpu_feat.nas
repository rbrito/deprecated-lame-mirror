;
;
; 	assembler routines to detect CPU-features
;
;	MMX / 3DNow! / SIMD
;
;	for the LAME project
;	Frank Klemm, Robert Hegemann 2000-10-12
;

%include "nasm.h"

	globaldef	has_MMX_nasm
	globaldef	has_3DNow_nasm
	globaldef	has_SIMD_nasm

        segment_code



;---------------------------------------
;	int has_MMX_nasm(void)
;---------------------------------------

	align	16
        
has_MMX_nasm:

	push	ebp
	mov	ebp,esp
        
        pushad
	pushfd	                        
	pop	eax
	mov	ecx,eax
	xor	eax,0x200000
	push	eax
	popfd
	pushfd
	pop	eax
	cmp	eax,ecx
	jz	NO_MMX_FEATURE	; no CPUID command

	mov	eax,0x1
	CPUID
	test	edx,0x800000
	jz	NO_MMX_FEATURE	; no MMX support
	popad
	mov	eax,0x1
	jmp	return_MMX
        
NO_MMX_FEATURE:

	popad
	xor	eax,eax

return_MMX:    

	mov	esp,ebp
	pop	ebp
        ret

        
        
;---------------------------------------
;	int has_SIMD_nasm(void)
;---------------------------------------

	align	16
        
has_SIMD_nasm:

	push	ebp
	mov	ebp,esp
        
        pushad
	pushfd	                        
	pop	eax
	mov	ecx,eax
	xor	eax,0x200000
	push	eax
	popfd
	pushfd
	pop	eax
	cmp	eax,ecx
	jz	NO_SIMD_FEATURE	; no CPUID command
        
	mov	eax,0x1
	CPUID
	test	edx,0x2000000
	jz	NO_SIMD_FEATURE	; no SIMD support
	popad
	mov	eax,0x1

	jmp	return_SIMD

NO_SIMD_FEATURE:

	popad
	xor	eax,eax

return_SIMD:    

	mov	esp,ebp
	pop	ebp
        ret


        
;---------------------------------------
;	int has_3DNow_nasm(void)
;---------------------------------------

	align	16
        
has_3DNow_nasm:

	push	ebp
	mov	ebp,esp
        
        pushad
	pushfd	                        
	pop	eax
	mov	ecx,eax
	xor	eax,0x200000
	push	eax
	popfd
	pushfd
	pop	eax
	cmp	eax,ecx
	jz	NO_3DNow_FEATURE	; no CPUID command

	mov	eax,0x80000000
	CPUID
	cmp	eax,0x80000000
	jbe	NO_3DNow_FEATURE	; no extended MSR1

	mov	eax,0x80000001
	CPUID
	test	edx,0x80000000
	jz	NO_3DNow_FEATURE	; no 3DNow!
	popad
	mov	eax,0x1

	jmp	return_3DNow

NO_3DNow_FEATURE:

	popad
	xor	eax,eax

return_3DNow:    

	mov	esp,ebp
	pop	ebp
        ret
        
        
        
        end
