
;	Copyright (C) 1999 URURI

;	nasm用マクロ
;	99/08/21(うるり) 作成

;単精度浮動小数点演算

%idefine float dword
%idefine fsize 4
%idefine fsizen(a) (fsize*(a))

;REG

%define r0 eax
%define r1 ebx
%define r2 ecx
%define r3 edx
%define r4 esi
%define r5 edi
%define r6 ebp
%define r7 esp

;MMX,3DNow!,SSE

%define pmov	movq
%define pmovd	movd

%define pupldq	punpckldq
%define puphdq	punpckhdq

%define xm0 xmm0
%define xm1 xmm1
%define xm2 xmm2
%define xm3 xmm3
%define xm4 xmm4
%define xm5 xmm5
%define xm6 xmm6
%define xm7 xmm7

;シャッフル用の4進マクロ

%define R4(a,b,c,d) (a*64+b*16+c*4+d)

;Cライクな簡易マクロ

%imacro globaldef 1
	%ifdef _NAMING
		%define %1 _%1
	%endif
	global %1
%endmacro

%imacro externdef 1
	%ifdef _NAMING
		%define %1 _%1
	%endif
	extern %1
%endmacro

%imacro proc 1
	%push	proc
	%ifdef _NAMING
		global _%1
	%else
		global %1
	%endif

	align 32
%1:
_%1:

	%assign %$STACK 0
	%assign %$STACKN 0
	%assign %$ARG 4
%endmacro

%imacro endproc 0
	%ifnctx proc
		%error expected 'proc' before 'endproc'.
	%else
		%if %$STACK > 0
			add esp, %$STACK
		%endif

		%if %$STACK <> (-%$STACKN)
			%error STACKLEVEL mismatch check 'local', 'alloc', 'pushd', 'popd'
		%endif

		ret
		%pop
	%endif
%endmacro

%idefine sp(a) esp+%$STACK+a

%imacro arg 1
	%00	equ %$ARG
	%assign %$ARG %$ARG+%1
%endmacro

%imacro local 1
	%assign %$STACKN %$STACKN-%1
	%00 equ %$STACKN
%endmacro

%imacro alloc 0
	sub esp, (-%$STACKN)-%$STACK
	%assign %$STACK (-%$STACKN)
%endmacro

%imacro pushd 1-*
	%rep %0
		push %1
		%assign %$STACK %$STACK+4
	%rotate 1
	%endrep
%endmacro

%imacro popd 1-*
	%rep %0
	%rotate -1
		pop %1
		%assign %$STACK %$STACK-4
	%endrep
%endmacro

%ifdef __tos__
group CGROUP text
group DGROUP data
%endif

%ifdef WIN32
	%define _NAMING
%endif
