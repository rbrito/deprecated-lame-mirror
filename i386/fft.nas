
;	for new GOGO-no-coda (1999/09)
;	Copyright (C) 1999 shigeo
;	special thanks to Keiichi SAKAI, URURI
%include "nasm.h"

	globaldef fht_3DN
	globaldef fft1k_FPU
	globaldef fft256_FPU
	globaldef fht
	externdef costab_fft
	externdef sintab_fft
	externdef gray_index

	segment_data
		align 16
D_MSB1_0	dd	0         ,0x80000000
D_SQRT2		dd	1.414213562,1.414213562
t_s0		dd	0	;[ t_c:t_s]
t_c0		dd	0
t_c1		dd	0	;[-t_s:t_c]
t_s1		dd	0
D_s1c1		dd	0, 0
D_Mc1s1		dd	0, 0
D_s2c2		dd	0, 0
D_Mc2s2		dd	0, 0
D_0_1		dd	1.0, 0.0
S_05		DD	0.5
S_00005		DD	0.0005
fht			dd	0		;関数ポインタ

	segment_code

;************************************************************************

;	by shigeo
;	99/08/16
;	23000clk 辛かった〜
;	18500clk bit reversal from gogo1 by URURI

;void fht(float *fz, int n);
		align 16
fht_3DN:
		push	ebx
		push	esi
		push	edi
		push	ebp
%assign _P 4*4
		;まず最初のループ... はfht()の外へ移動

		mov		esi,[esp+_P+4]		;esi=fz
		mov		ecx,[esp+_P+8]		;ecx=n

		;2つ目のループ
		femms
		mov		eax,esi				;fi=fz
		shr		ecx,2				;ecx=n/4
		movq	mm7,[D_MSB1_0]		;mm7=[0x80000000:0]
		movq	mm2,mm7
		movq	mm3,mm7
		jmp		.lp10
		align	16
.lp10:
		movq	mm0,[eax]			;mm0=[ fi1:fi0]
		movq	mm1,[eax+8]			;mm1=[ fi3:fi2]
		movq	mm2,mm0
		pxor	mm2,mm7				;mm2=[-fi1:fi0]
		movq	mm3,mm1
		pxor	mm3,mm7				;mm3=[-fi3:fi2]
		pfacc	mm0,mm2				;mm0=[f1:f0]=[fi0-fi1:fi0+fi1]
		pfacc	mm1,mm3				;mm1=[f3:f2]=[fi2-fi3:fi2+fi3]
		movq	mm3,mm0				;mm3=[f1:f0]
		pfadd	mm3,mm1				;mm3=[fi1:fi0]=[f1+f3:f0+f2]
		pfsub	mm0,mm1				;mm0=[fi3:fi2]=[f1-f3:f0-f2]
		movq	[eax],mm3
		movq	[eax+8],mm0
		add		eax,16
		loop	.lp10

		;メインループ
		movq	mm7,[D_MSB1_0]	;mm7=[1<<31:0]

%assign LOCAL_STACK	16
		sub		esp,LOCAL_STACK
%assign _P (_P+LOCAL_STACK)
		xor		eax,eax
		mov		[esp],eax		;k=0
%define k dword [esp]
%define kx	dword [esp+4]
%define fn dword [esp+8]

.lp30:		;k=0; do{
		mov		ecx,k
		add		ecx,2
		mov		k,ecx
		mov		eax,1
		shl		eax,cl			;eax=k1 = 1<<k
		lea		ebx,[eax+eax]	;ebx=k2 = k1*2
		lea		ecx,[eax+eax*2]	;ecx=k3 = k2 + k1 = k1*3
		lea		edx,[ebx+ebx]	;edx=k4 = k1*4
		mov		esi,eax
		shr		esi,1			;esi=kx=k1>>1
		mov		kx,esi			;保存(後で使う)
		mov		edi,[esp+_P+4]	;edi=fi=fz
		lea		ebp,[edi+esi*4]	;ebp=gi=fz+kx
		mov		esi,[esp+_P+8]	;esi=n
		lea		esi,[edi+esi*4]	;esi=fn=fz+n
		movq	mm6,[D_SQRT2]	;mm6=[√2:√2]

.lp31:		;fn=fz+n; do{ FLOAT g0,f0,f1,...
		movd	mm0,[edi]		;mm0=[0:fi[ 0]]
		movd	mm1,[edi+eax*4]	;mm1=[0:fi[k1]]
		punpckldq	mm0,mm0		;mm0=[fi_0 :fi_0 ]
		punpckldq	mm1,mm1		;mm1=[fi_k1:fi_k1]
		movd	mm2,[edi+ebx*4]
		movd	mm3,[edi+ecx*4]
		punpckldq	mm2,mm2		;mm2=[fi_k2:fi_k2]
		punpckldq	mm3,mm3		;mm3=[fi_k3:fi_k3]
		pxor	mm1,mm7			;mm1=[-fi_k1:fi_k1]
		pxor	mm3,mm7			;mm3=[-fi_k3:fi_k3]
		pfadd	mm0,mm1			;mm0=[f1:f0]=[fi_0 -fi_k1 : fi_0 +fi_k1]
		pfadd	mm2,mm3			;mm2=[f3:f2]=[fi_k2-fi_k3 : fi_k2+fi_k3]
		movq	mm3,mm0			;mm3=[f1:f0]
		pfadd	mm0,mm2			;mm0=[f1+f3:f0+f2]
		movd	[edi],mm0		;fi[0]=f0+f2
		psrlq	mm0,32			;mm0=[0:f1+f3]
		pfsub	mm3,mm2			;mm3=[f1-f3:f0-f2]
		movd	[edi+eax*4],mm0	;fi[k1]=f1+f3
		movd	[edi+ebx*4],mm3	;fi[k2]=f0-f2
		psrlq	mm3,32			;mm3=[0:f1-f3]
		movd	[edi+ecx*4],mm3	;fi[k3]=f1-f3

		movd	mm0,[ebp]		;mm0=[0:gi_0]
		movd	mm1,[ebp+eax*4]	;mm1=[0:gi_k1]
		punpckldq	mm0,mm0		;mm0=[gi_0 :gi_0 ]
		punpckldq	mm1,mm1		;mm1=[gi_k1:gi_k1]
		movd	mm2,[ebp+ebx*4]	;mm2=[0:gi_k2]
		pxor	mm1,mm7			;mm1=[-gi_k1:gi_k1]
		punpckldq	mm2,[ebp+ecx*4]	;mm2=[gi_k3:gi_k2]
		pfadd	mm0,mm1			;mm0=[g1:g0]=[gi_0 -gi_k1:gi_0 +gi_k1]
		pfmul	mm2,mm6			;mm2=[g3:g2]=sqrt2 * [gi_k3:gi_k2]
		movq	mm1,mm0			;mm1=[g1:g0]
		pfadd	mm0,mm2			;mm0=[g1+g3:g0+g2]
		movd	[ebp],mm0		;gi[0]=g0+g2
		psrlq	mm0,32			;mm0=[0:g1+g3]
		pfsub	mm1,mm2			;mm1=[g1-g3:g0-g2]
		movd	[ebp+eax*4],mm0	;gi[k1]=g1+g3
		movd	[ebp+ebx*4],mm1	;gi[k2]=g0-g2
		psrlq	mm1,32			;mm1=[0:g1-g3]
		movd	[ebp+ecx*4],mm1	;gi[k3]=g1-g3
		lea		edi,[edi+edx*4]	;fi += k4
		lea		ebp,[ebp+edx*4]	;gi += k4
		cmp		edi,esi
		jc		near .lp31		;}while(fi<fn);

;	ここまでは多分O.K.

		mov		fn,esi			;fn=fz+n
		;次の値は引き続き使う
		;eax=k1,ebx=k2,ecx=k3,edx=k4

		mov		edi,k
		lea		ebp,[costab_fft+edi*4]
		mov		ebp,[ebp]		;ebp=t_c
		mov		[t_c0],ebp
		mov		[t_c1],ebp		;t_c
		lea		ebp,[sintab_fft+edi*4]
		mov		ebp,[ebp]		;ebx=t_s
		mov		[t_s0],ebp
		xor		ebp,0x80000000
		mov		[t_s1],ebp		;-t_s

		movq	mm1,[D_0_1]		;mm1=[0:1]
		movq	[D_s1c1],mm1	;mm1=[s1:c1]
		mov		esi,1			;esi=i=1

.lp32:	;	for(i=1;i<kx;i++){
		movq	mm0,[D_s1c1]	;mm1=[s1:t]=[s1:c1]
		movq	mm2,mm0
		pfmul	mm0,[t_c1]		;mm0=[-s1*t_s: t*t_c]
		pfmul	mm2,[t_s0]		;mm2=[ s1*t_c: t*t_s]
		pfacc	mm0,mm2			;mm0=[s1:c1]=[ s1*t_c+t*t_s:-s1*t_s+t*t_c]
		movq	mm2,mm0			;mm2=[s1:c1]
		movq	[D_s1c1],mm0	;保存
		movq	mm6,mm2
		punpckldq	mm5,mm6
		punpckhdq	mm6,mm5		;mm6=[ c1:s1]
		pxor	mm6,mm7			;mm6=[-c1:s1]
		movq	[D_Mc1s1],mm6	;保存
		pfmul	mm2,mm2			;mm2=[s1*s1:c1*c1]
		movq	mm3,mm0			;mm3=[s1:c1]
		pxor	mm2,mm7			;mm2=[-s1*s1:c1*c1]
		psrlq	mm3,32			;mm3=[ 0:s1]
		pfacc	mm2,mm2			;mm2=[c2:c2]=[c1*c1-s1*s1:<]
		pfmul	mm0,mm3			;mm0=[ 0:c1*s1]
		pfadd	mm0,mm0			;mm0=[0:s2]=[ 0:2*c1*s1]
		punpckldq	mm2,mm0		;mm2=[s2:c2]
		movq	[D_s2c2],mm2	;保存

		punpckldq	mm0,mm2
		punpckhdq	mm2,mm0		;mm2=[c2:s2]
		pxor	mm2,mm7			;mm2=[-c2:s2]
		movq	[D_Mc2s2],mm2	;保存

		mov		edi,[esp+_P+4]	;edi=fz
		lea		edi,[edi+esi*4]	;edi=fz+i

		mov		ebp,[esp+_P+4]	;ebp=fz
		neg		esi				;esi=-i
		lea		ebp,[ebp+eax*4]	;ebp=fz+k1
		lea		ebp,[ebp+esi*4]	;ebp=gi=fz+k1-i
		neg		esi				;esi=i

.lp33:	;	do{ FLOAT a,b,g0,f0,f1,g1,f2,g2,f3,g3;

		movd	mm0,[edi+eax*4]	;mm0=[0:fi_k1]
		punpckldq	mm0,[ebp+eax*4]	;mm0=[gi_k1:fi_k1]
		movq	mm1,mm0
		pfmul	mm0,[D_s2c2]	;mm0=[ s2*gi_k1:c2*fi_k1]
		pfmul	mm1,[D_Mc2s2]	;mm1=[-c2*gi_k1:s2*fi_k1]
		pfacc	mm0,mm1			;mm0=[b:a]
		movd	mm4,[edi]		;mm4=[0:fi_0]
		movq	mm3,mm0			;mm3=[b:a]
		punpckldq	mm4,[ebp]	;mm4=[gi_0:fi_0]
		pfadd	mm3,mm4			;mm3=[g0:f0]=[gi_0+b:fi_0+a]
		pfsub	mm4,mm0			;mm4=[g1:f1]=[gi_0-b:fi_0-a]

		movd	mm0,[edi+ecx*4]	;mm0=[0:fi_k3]
		punpckldq	mm0,[ebp+ecx*4]	;mm0=[gi_k3:fi_k3]
		movq	mm1,mm0
		pfmul	mm0,[D_s2c2]	;mm0=[ s2*gi_k3:c2*fi_k3]
		pfmul	mm1,[D_Mc2s2]	;mm1=[-c2*gi_k3:s2*fi_k3]
		pfacc	mm0,mm1			;mm0=[b:a]
		movd	mm5,[edi+ebx*4]	;mm5=[0:fi_k2]
		movq	mm6,mm0			;mm6=[b:a]
		punpckldq	mm5,[ebp+ebx*4]	;mm5=[gi_k2:fi_k2]
		pfadd	mm6,mm5			;mm6=[g2:f2]=[gi_k2+b:fi_k2+a]
		pfsub	mm5,mm0			;mm5=[g3:f3]=[gi_k2-b:fi_k2-a]

		punpckldq	mm1,mm6		;mm1=[f2:*]
		movq	mm0,[D_s1c1]	;mm0=[s1:c1]
		punpckhdq	mm1,mm5		;mm1=[g3:f2]
		pfmul	mm0,mm1			;mm0=[ s1*g3:c1*f2]
		movq	mm2,[D_Mc1s1]	;mm2=[-c1:s1]
		pfmul	mm2,mm1			;mm2=[-c1*g3:s1*f2]
		pfacc	mm0,mm2			;mm0=[b:a]

		punpckldq	mm1,mm3		;mm1=[f0:*]
		punpckhdq	mm1,mm4		;mm1=[g1:f0]
		movq	mm2,mm0			;mm2=[b:a]
		pfadd	mm0,mm1			;mm0=[g1+b:f0+a]
		pfsubr	mm2,mm1			;mm2=[g1-b:f0-a]
		movd	[edi],mm0		;fi[0]=f0+a
		psrlq	mm0,32			;mm0=[0:g1+b]
		movd	[edi+ebx*4],mm2	;fi[k2]=f0-a
		psrlq	mm2,32			;mm2=[0:g1-b]
		movd	[ebp+eax*4],mm0	;gi[k1]=g1+b
		movd	[ebp+ecx*4],mm2	;gi[k3]=g1-b
		psrlq	mm6,32			;mm6=[0:g2]
		movq	mm0,[D_s1c1]	;mm0=[s1:c1]
		punpckldq	mm5,mm6		;mm5=[g2:f3]
		pfmul	mm0,mm5			;mm0=[g2* s1:f3*c1]
		pfmul	mm5,[D_Mc1s1]	;mm5=[g2*-c1:f3*s1]
		pfacc	mm0,mm5			;mm0=[-b:a]
		psrlq	mm3,32			;mm3=[0:g0]
		movq	mm1,mm0			;mm1=[-b:a]
		punpckldq	mm3,mm4		;mm3=[f1:g0]
		pfadd	mm0,mm3			;mm0=[f1-b:g0+a]
		pfsubr	mm1,mm3			;mm1=[f1+b:g0-a]
		movd	[ebp],mm0		;gi[0]=g0+a
		psrlq	mm0,32			;mm0=[0:f1-b]
		movd	[ebp+ebx*4],mm1	;gi[k2]=g0-a
		psrlq	mm1,32			;mm1=[0:f1+b]
		movd	[edi+ecx*4],mm0	;fi[k3]=f1-b
		movd	[edi+eax*4],mm1	;fi[k1]=f1+b

		lea		edi,[edi+edx*4]	;fi += k4
		lea		ebp,[ebp+edx*4]	;gi += k4
		cmp		edi,fn
		jc		near .lp33		;}while(fi<fn)
		inc		esi
		cmp		esi,kx
		jnz		near .lp32		;}
		cmp		edx,[esp+_P+8]
		jnz		near .lp30		;}while(k4<n)


.exit:
		add		esp,LOCAL_STACK
		femms
		pop		ebp
		pop		edi
		pop		esi
		pop		ebx
		ret

; by K.SAKAI
; 2つのループを融合
; index の bit reversal によるシャッフルは、src/dest 双方を
; グレイコードを使用することで双方のカウントを簡素化。
; バイナリコード＋ビットリバースよりコード化は容易。

;
; void fft1k_FPU(FLOAT *x_real, FLOAT *energy, FLOAT (*abx)[2], float *window, int *savebuf)
; ※ unroll かましたほうがインデックステーブルが小さくなる。
;
%assign	NB	10
		align	16
fft1k_FPU:
		push	ebx
		push	esi
		push	edi
		push	ebp
%assign _P 4*4
		xor		esi,esi			; source index
		mov		edx,[esp+_P+20]	; edx=savebuf
		xor		edi,edi			; destination index
		mov		eax,[esp+_P+16]	; eax=window
		mov		ebx,gray_index
		mov		ebp,[esp+_P+4]	; ebp=x_real
		jmp		short .F0

		align	16
.lp0:
		btc		edi,ecx
.F0:
		fild	dword [edx+esi*4+0*4]
		fld		dword [eax+esi*4+0*4]
		fmulp	st1,st0
		fstp	dword [ebp+edi*4+0*4]

		fild	dword [edx+esi*4+1*4]
		fld		dword [eax+esi*4+1*4]
		btc		esi,1
		fmulp	st1,st0
		fstp	dword [ebp+edi*4+(4 << (NB-1))]
		btc		edi,(NB-2)

		fild	dword [edx+esi*4+1*4]
		fld		dword [eax+esi*4+1*4]
		fmulp	st1,st0
		fstp	dword [ebp+edi*4+(4 << (NB-1))]
		movzx	ecx,byte [ebx]
		inc		ebx

		fild	dword [edx+esi*4+0*4]
		fld		dword [eax+esi*4+0*4]
		btc		esi,ecx
		fmulp	st1,st0
		neg		ecx
		fstp	dword [ebp+edi*4+0*4]
		add		ecx,(NB-1)
		jns		.lp0

;		fht(x_real, N);
		push	dword (1 << NB)
		push	ebp
		mov		eax,[fht]
		call	eax
		add		esp,8

		mov		ecx,[esp+_P+12]		; = abx
		mov		ebx,[esp+_P+8]		; = energy

;       energy[0] = x_real[0] * x_real[0];
;       abx[0][0] = abx[0][1] = x_real[0];
		fld		dword [ebp]
		fst		dword [ecx]
		fst		dword [ecx+4]
		fmul	st0,st0
		fstp	dword [ebx]

		fld		dword [S_05]		; 0.5
		fld		dword [S_00005]		; 0.0005
;       for( i = 1, j = N-1; i < 6; i++, j-- ){
		mov		esi,1				; = i
		mov		edi,((1 << NB)-1)	; = j
.lp1:
;               a = abx[i][0] = x_real[i];
;               b = abx[i][1] = x_real[j];
;               energy[i]=(a*a + b*b) * 0.5;
		fld		dword [ebp+esi*4]
		fld		dword [ebp+edi*4]
		fxch
		fst		dword [ecx+esi*8]	; a = abx[i][0]
		fmul	st0,st0
		fxch
		fst		dword [ecx+esi*8+4]	; b = abx[i][1]
		fmul	st0,st0
		faddp	st1,st0
		fmul	st0,st2				; *0.5
		fst		dword [ebx+esi*4]	; energy[i]
;               if (energy[i] < 0.0005){
		fcomp	st1
		fnstsw	ax
		and		ah,69
		cmp		ah,1
		jne		.F1
;                       energy[i] = 0.0005;
;                       abx[i][0] = abx[i][1] = 0.022360679774997897; /* √0.0005 */
		mov		dword [ebx+esi*4],0x3A03126F	; energy[i]
		mov		eax,0x3CB72DBF		; = 0.022360679774997897
		mov		[ecx+esi*8],eax		; a = abx[i][0]
		mov		[ecx+esi*8+4],eax	; b = abx[i][1]
;               }
.F1:
		inc		esi
		dec		edi
		cmp		esi,6
		jl		.lp1
;       }

;       for( ; i < N/2; i++, j-- ){
.lp2:
;               a = x_real[i];
;               b = x_real[j];
		fld		dword [ebp+esi*4]
		fld		dword [ebp+edi*4]
;               energy[i]=(a*a + b*b) * 0.5;
		fxch
		fmul	st0,st0
		fxch
		fmul	st0,st0
		faddp	st1,st0
		fmul	st0,st2				; *0.5
%if 1
		fstp	dword [ebx+esi*4]	; energy[i]
%else
; /* 大きな値のほうしか見ないので省略してよいと思うが... */
		fst		dword [ebx+esi*4]	; energy[i]
;               if (energy[i] < 0.0005){
		fcomp	st1
		fnstsw	ax
		and		ah,69
		cmp		ah,1
		jne		.F2
;                       energy[i] = 0.0005;
		mov		dword [ebx+esi*4],0x3A03126F	; energy[i]
;               }
.F2:
%endif
		inc		esi
		dec		edi
		cmp		esi,edi
		jne		.lp2
;       }

;       energy[N/2] = x_real[N/2] * x_real[N/2];
		fld		dword [ebp+esi*4]
		fmul	st0,st0
		fstp	dword [ebx+esi*4]	; energy[i]

		fcompp						; pop up 0.5 and 0.0005

		pop		ebp
		pop		edi
		pop		esi
		pop		ebx
		ret

;
; void fft256_FPU(FLOAT *x_real, FLOAT *energy, FLOAT (*abx)[2], float *window, int *savebuf)
; ※ unroll かましたほうがインデックステーブルが小さくなる。
;
%assign	NB	8
		align	16
fft256_FPU:
		push	ebx
		push	esi
		push	edi
		push	ebp
%assign _P 4*4
		xor		esi,esi			; source index
		mov		edx,[esp+_P+20]	; edx=savebuf
		xor		edi,edi			; destination index
		mov		eax,[esp+_P+16]	; eax=window
		mov		ebx,gray_index
		mov		ebp,[esp+_P+4]	; ebp=x_real
		jmp		short .F0

		align	16
.lp0:
		btc		edi,ecx
.F0:
		fild	dword [edx+esi*4+0*4]
		fld		dword [eax+esi*4+0*4]
		fmulp	st1,st0
		fstp	dword [ebp+edi*4+0*4]

		fild	dword [edx+esi*4+1*4]
		fld		dword [eax+esi*4+1*4]
		btc		esi,1
		fmulp	st1,st0
		fstp	dword [ebp+edi*4+(4 << (NB-1))]
		btc		edi,(NB-2)

		fild	dword [edx+esi*4+1*4]
		fld		dword [eax+esi*4+1*4]
		fmulp	st1,st0
		fstp	dword [ebp+edi*4+(4 << (NB-1))]
		movzx	ecx,byte [ebx]
		inc		ebx

		fild	dword [edx+esi*4+0*4]
		fld		dword [eax+esi*4+0*4]
		btc		esi,ecx
		fmulp	st1,st0
		neg		ecx
		fstp	dword [ebp+edi*4+0*4]
		add		ecx,(NB-1)
		jns		.lp0

;		fht(x_real, N);
		push	dword (1 << NB)
		push	ebp
		mov		eax,[fht]
		call	eax
		add		esp,8

		mov		ecx,[esp+_P+12]		; = abx
		mov		ebx,[esp+_P+8]		; = energy

;       energy[0] = x_real[0] * x_real[0];
;       abx[0][0] = abx[0][1] = x_real[0];
		fld		dword [ebp]
		fst		dword [ecx]
		fst		dword [ecx+4]
		fmul	st0,st0
		fstp	dword [ebx]

		fld		dword [S_05]		; 0.5
		fld		dword [S_00005]		; 0.0005
;       for( i = 1, j = N-1; i < N/2; i++, j-- ){
		mov		esi,1				; = i
		mov		edi,((1 << NB)-1)	; = j
.lp1:
;               a = abx[i][0] = x_real[i];
;               b = abx[i][1] = x_real[j];
;               energy[i]=(a*a + b*b) * 0.5;
		fld		dword [ebp+esi*4]
		fld		dword [ebp+edi*4]
		fxch
		fst		dword [ecx+esi*8]	; a = abx[i][0]
		fmul	st0,st0
		fxch
		fst		dword [ecx+esi*8+4]	; b = abx[i][1]
		fmul	st0,st0
		faddp	st1,st0
		fmul	st0,st2				; *0.5
		fst		dword [ebx+esi*4]	; energy[i]
;               if (energy[i] < 0.0005){
		fcomp	st1
		fnstsw	ax
		and		ah,69
		cmp		ah,1
		jne		.F1
;                       energy[i] = 0.0005;
;                       abx[i][0] = abx[i][1] = 0.022360679774997897; /* √0.0005 */
		mov		dword [ebx+esi*4],0x3A03126F	; energy[i]
		mov		eax,0x3CB72DBF		; = 0.022360679774997897
		mov		[ecx+esi*8],eax		; a = abx[i][0]
		mov		[ecx+esi*8+4],eax	; b = abx[i][1]
;               }
.F1:
		inc		esi
		dec		edi
		cmp		esi,edi
		jne		.lp1
;       }
;       energy[N/2] = x_real[N/2] * x_real[N/2];
		fld		dword [ebp+esi*4]
		fst		dword [ecx+esi*8]	; abx[N/2][0]
		fst		dword [ecx+esi*8+4]	; abx[N/2][1]
		fmul	st0,st0
		fstp	dword [ebx+esi*4]	; energy[N/2]

		fcompp						; pop up 0.5 and 0.0005

		pop		ebp
		pop		edi
		pop		esi
		pop		ebx
		ret

		end
