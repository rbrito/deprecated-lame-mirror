	%include "nasm.h"

; float_t  float32_scalar04_float32_i387 ( 
;         const float32* const p, 
;         const float32* const q );

        segment_code

proc	float32_scalar04_float32_i387
%$p	arg	4
%$q	arg	4
;;;	alloc

        mov     ecx,[sp(%$p)]
        mov     edx,[sp(%$q)]
	fld     dword [ecx]
        fmul    dword [edx]
	fld     dword [ecx +  4]
        fmul    dword [edx +  4]
        faddp   st1,st0
	fld     dword [ecx +  8]
        fmul    dword [edx +  8]
        faddp   st1,st0
	fld     dword [ecx + 12]
        fmul    dword [edx + 12]
        faddp   st1,st0    
endproc

proc	float32_scalar08_float32_i387
%$p	arg	4
%$q	arg	4
;;;	alloc

        mov     ecx,[sp(%$p)]
        mov     edx,[sp(%$q)]
	fld     dword [ecx]
        fmul    dword [edx]
	fld     dword [ecx +  4]
        fmul    dword [edx +  4]
        faddp   st1,st0
	fld     dword [ecx +  8]
        fmul    dword [edx +  8]
        faddp   st1,st0
	fld     dword [ecx + 12]
        fmul    dword [edx + 12]
        faddp   st1,st0    
	fld     dword [ecx + 16]
        fmul    dword [edx + 16]
        faddp   st1,st0    
	fld     dword [ecx + 20]
        fmul    dword [edx + 20]
        faddp   st1,st0    
	fld     dword [ecx + 24]
        fmul    dword [edx + 24]
        faddp   st1,st0    
	fld     dword [ecx + 28]
        fmul    dword [edx + 28]
        faddp   st1,st0    
endproc

proc	float32_scalar12_float32_i387
%$p	arg	4
%$q	arg	4
;;;	alloc

        mov     ecx,[sp(%$p)]
        mov     edx,[sp(%$q)]
	fld     dword [ecx]
        fmul    dword [edx]
	fld     dword [ecx +  4]
        fmul    dword [edx +  4]
        faddp   st1,st0
	fld     dword [ecx +  8]
        fmul    dword [edx +  8]
        faddp   st1,st0
	fld     dword [ecx + 12]
        fmul    dword [edx + 12]
        faddp   st1,st0    
	fld     dword [ecx + 16]
        fmul    dword [edx + 16]
        faddp   st1,st0    
	fld     dword [ecx + 20]
        fmul    dword [edx + 20]
        faddp   st1,st0    
	fld     dword [ecx + 24]
        fmul    dword [edx + 24]
        faddp   st1,st0    
	fld     dword [ecx + 28]
        fmul    dword [edx + 28]
        faddp   st1,st0    
	fld     dword [ecx + 32]
        fmul    dword [edx + 32]
        faddp   st1,st0    
	fld     dword [ecx + 36]
        fmul    dword [edx + 36]
        faddp   st1,st0    
	fld     dword [ecx + 40]
        fmul    dword [edx + 40]
        faddp   st1,st0    
	fld     dword [ecx + 44]
        fmul    dword [edx + 44]
        faddp   st1,st0    
endproc

proc	float32_scalar16_float32_i387
%$p	arg	4
%$q	arg	4
;;;	alloc

        mov     ecx,[sp(%$p)]
        mov     edx,[sp(%$q)]
	fld     dword [ecx]
        fmul    dword [edx]
	fld     dword [ecx +  4]
        fmul    dword [edx +  4]
        faddp   st1,st0
	fld     dword [ecx +  8]
        fmul    dword [edx +  8]
        faddp   st1,st0
	fld     dword [ecx + 12]
        fmul    dword [edx + 12]
        faddp   st1,st0    
	fld     dword [ecx + 16]
        fmul    dword [edx + 16]
        faddp   st1,st0    
	fld     dword [ecx + 20]
        fmul    dword [edx + 20]
        faddp   st1,st0    
	fld     dword [ecx + 24]
        fmul    dword [edx + 24]
        faddp   st1,st0    
	fld     dword [ecx + 28]
        fmul    dword [edx + 28]
        faddp   st1,st0    
	fld     dword [ecx + 32]
        fmul    dword [edx + 32]
        faddp   st1,st0    
	fld     dword [ecx + 36]
        fmul    dword [edx + 36]
        faddp   st1,st0    
	fld     dword [ecx + 40]
        fmul    dword [edx + 40]
        faddp   st1,st0    
	fld     dword [ecx + 44]
        fmul    dword [edx + 44]
        faddp   st1,st0    
	fld     dword [ecx + 48]
        fmul    dword [edx + 48]
        faddp   st1,st0    
	fld     dword [ecx + 52]
        fmul    dword [edx + 52]
        faddp   st1,st0    
	fld     dword [ecx + 56]
        fmul    dword [edx + 56]
        faddp   st1,st0    
	fld     dword [ecx + 60]
        fmul    dword [edx + 60]
        faddp   st1,st0    
endproc

proc	float32_scalar20_float32_i387
%$p	arg	4
%$q	arg	4
;;;	alloc

        mov     ecx,[sp(%$p)]
        mov     edx,[sp(%$q)]
	fld     dword [ecx]
        fmul    dword [edx]
	fld     dword [ecx +  4]
        fmul    dword [edx +  4]
        faddp   st1,st0
	fld     dword [ecx +  8]
        fmul    dword [edx +  8]
        faddp   st1,st0
	fld     dword [ecx + 12]
        fmul    dword [edx + 12]
        faddp   st1,st0    
	fld     dword [ecx + 16]
        fmul    dword [edx + 16]
        faddp   st1,st0    
	fld     dword [ecx + 20]
        fmul    dword [edx + 20]
        faddp   st1,st0    
	fld     dword [ecx + 24]
        fmul    dword [edx + 24]
        faddp   st1,st0    
	fld     dword [ecx + 28]
        fmul    dword [edx + 28]
        faddp   st1,st0    
	fld     dword [ecx + 32]
        fmul    dword [edx + 32]
        faddp   st1,st0    
	fld     dword [ecx + 36]
        fmul    dword [edx + 36]
        faddp   st1,st0    
	fld     dword [ecx + 40]
        fmul    dword [edx + 40]
        faddp   st1,st0    
	fld     dword [ecx + 44]
        fmul    dword [edx + 44]
        faddp   st1,st0    
	fld     dword [ecx + 48]
        fmul    dword [edx + 48]
        faddp   st1,st0    
	fld     dword [ecx + 52]
        fmul    dword [edx + 52]
        faddp   st1,st0    
	fld     dword [ecx + 56]
        fmul    dword [edx + 56]
        faddp   st1,st0    
	fld     dword [ecx + 60]
        fmul    dword [edx + 60]
        faddp   st1,st0    
	fld     dword [ecx + 64]
        fmul    dword [edx + 64]
        faddp   st1,st0    
	fld     dword [ecx + 68]
        fmul    dword [edx + 68]
        faddp   st1,st0    
	fld     dword [ecx + 72]
        fmul    dword [edx + 72]
        faddp   st1,st0    
	fld     dword [ecx + 76]
        fmul    dword [edx + 76]
        faddp   st1,st0    
endproc

proc	float32_scalar24_float32_i387
%$p	arg	4
%$q	arg	4
;;;	alloc

        mov     ecx,[sp(%$p)]
        mov     edx,[sp(%$q)]
	fld     dword [ecx]
        fmul    dword [edx]
	fld     dword [ecx +  4]
        fmul    dword [edx +  4]
        faddp   st1,st0
	fld     dword [ecx +  8]
        fmul    dword [edx +  8]
        faddp   st1,st0
	fld     dword [ecx + 12]
        fmul    dword [edx + 12]
        faddp   st1,st0    
	fld     dword [ecx + 16]
        fmul    dword [edx + 16]
        faddp   st1,st0    
	fld     dword [ecx + 20]
        fmul    dword [edx + 20]
        faddp   st1,st0    
	fld     dword [ecx + 24]
        fmul    dword [edx + 24]
        faddp   st1,st0    
	fld     dword [ecx + 28]
        fmul    dword [edx + 28]
        faddp   st1,st0    
	fld     dword [ecx + 32]
        fmul    dword [edx + 32]
        faddp   st1,st0    
	fld     dword [ecx + 36]
        fmul    dword [edx + 36]
        faddp   st1,st0    
	fld     dword [ecx + 40]
        fmul    dword [edx + 40]
        faddp   st1,st0    
	fld     dword [ecx + 44]
        fmul    dword [edx + 44]
        faddp   st1,st0    
	fld     dword [ecx + 48]
        fmul    dword [edx + 48]
        faddp   st1,st0    
	fld     dword [ecx + 52]
        fmul    dword [edx + 52]
        faddp   st1,st0    
	fld     dword [ecx + 56]
        fmul    dword [edx + 56]
        faddp   st1,st0    
	fld     dword [ecx + 60]
        fmul    dword [edx + 60]
        faddp   st1,st0    
	fld     dword [ecx + 64]
        fmul    dword [edx + 64]
        faddp   st1,st0    
	fld     dword [ecx + 68]
        fmul    dword [edx + 68]
        faddp   st1,st0    
	fld     dword [ecx + 72]
        fmul    dword [edx + 72]
        faddp   st1,st0    
	fld     dword [ecx + 76]
        fmul    dword [edx + 76]
        faddp   st1,st0    
	fld     dword [ecx + 80]
        fmul    dword [edx + 80]
        faddp   st1,st0    
	fld     dword [ecx + 84]
        fmul    dword [edx + 84]
        faddp   st1,st0    
	fld     dword [ecx + 88]
        fmul    dword [edx + 88]
        faddp   st1,st0    
	fld     dword [ecx + 92]
        fmul    dword [edx + 92]
        faddp   st1,st0    
endproc



float32_scalar4n_float32_i387:
float32_scalar1n_float32_i387:

float32_scalar04_float32_3DNow:
float32_scalar08_float32_3DNow:
float32_scalar12_float32_3DNow:
float32_scalar16_float32_3DNow:
float32_scalar20_float32_3DNow:
float32_scalar24_float32_3DNow:
float32_scalar4n_float32_3DNow:
float32_scalar1n_float32_3DNow:

float32_scalar04_float32_SIMD:
float32_scalar08_float32_SIMD:
float32_scalar12_float32_SIMD:
float32_scalar16_float32_SIMD:
float32_scalar20_float32_SIMD:
float32_scalar24_float32_SIMD:
float32_scalar4n_float32_SIMD:
float32_scalar1n_float32_SIMD:


