;
;
; 	assembler routines to detect CPU-features
;
;	MMX / 3DNow! / SSE / SSE2
;
;	for the LAME project
;	Frank Klemm, Robert Hegemann 2000-10-12
;

%include "nasm.h"

	globaldef	has_MMX
	globaldef	has_3DNow
	globaldef	has_E3DNow
	globaldef	has_SSE
	globaldef	has_SSE2

        segment_code

testCPUID:
	pushfd	                        
	pop	eax
	mov	ecx,eax
	xor	eax,0x200000
	push	eax
	popfd
	pushfd
	pop	eax
	cmp	eax,ecx
	ret

;---------------------------------------
;	int  has_MMX (void)
;---------------------------------------

has_MMX:
        pushad
	call	testCPUID
	jz	return0		; no CPUID command, so no MMX

	mov	eax,0x1
	CPUID
	test	edx,0x800000
	jz	return0		; no MMX support
	jmp	return1		; MMX support
        
;---------------------------------------
;	int  has_SSE (void)
;---------------------------------------

has_SSE:
        pushad
	call	testCPUID
	jz	return0		; no CPUID command, so no SSE
        
	mov	eax,0x1
	CPUID
	test	edx,0x02000000
	jz	return0		; no SSE support
	jmp	return1		; SSE support
        
;---------------------------------------
;	int  has_SSE2 (void)
;---------------------------------------

has_SSE2:
        pushad
	call	testCPUID
	jz	return0		; no CPUID command, so no SSE2
        
	mov	eax,0x1
	CPUID
	test	edx,0x04000000
	jz	return0		; no SSE2 support
	jmp	return1		; SSE2 support
        
;---------------------------------------
;	int  has_E3DNow (void)
;---------------------------------------

has_E3DNow:
        pushad
	call	testCPUID
	jz	return0		; no CPUID command, so no E3DNow!

	mov	eax,0x80000000
	CPUID
	cmp	eax,0x80000000
	jbe	return0		; no extended MSR(1), so no E3DNow!

	mov	eax,0x80000001
	CPUID
	test	edx,0x40000000
	jz	return0		; no E3DNow! support
				; E3DNow! support
return1:
	popad
	xor	eax,eax
	inc	eax
	ret

return0:
	popad
	xor	eax,eax
	ret

;---------------------------------------
;	int  has_3DNow (void)
;---------------------------------------

has_3DNow:
        pushad
	call	testCPUID
	jz	return0		; no CPUID command, so no 3DNow!

	mov	eax,0x80000000
	CPUID
	cmp	eax,0x80000000
	jbe	return0		; no extended MSR(1), so no 3DNow!

	mov	eax,0x80000001
	CPUID
	test	edx,0x80000000
	jz	return0		; no 3DNow! support
	jmp	return1		; 3DNow! support
        
        end

