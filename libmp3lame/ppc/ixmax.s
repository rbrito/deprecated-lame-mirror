;******************************************************
; LAME MP3 project
;
; Copyright (C) 2004 Robert Stiles
;
; This library is free software; you can redistribute it and/or
; modify it under the terms of the GNU Library General Public
; License as published by the Free Software Foundation; either
; version 2 of the License, or (at your option) any later version.
;
; This library is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
; Library General Public License for more details.
;
; You should have received a copy of the GNU Library General Public
; License along with this library; if not, write to the
; Free Software Foundation, Inc., 59 Temple Place - Suite 330,
; Boston, MA 02111-1307, USA.
;
; Notes:
;
; This has only been tested on Darwin 7.4.0 using a
; PowerPC 7410 processor and GCC 3.3 (Apple version 1640 & 1666) 
compiler.
; Your mileage may vary.
;
; Register passing conventions used as detailed in
; Mac OS X 10 Mach-O Runtime Reference Manual.
;
; int ix_max_asm(const int *ix, const int *end)
; ix = r3
; end = r4
; return value = r3
;******************************************************

;******************************************************
; Stack Pointer
;******************************************************

#define sp      r1

;******************************************************
; Parameter variables
;******************************************************

#define ix      r3
#define end     r4

;******************************************************
; Local register variables
;******************************************************

#define count   r5
#define max     r6
#define temp    r7
#define temp1   r8
#define temp2   r9
#define sixteen r10
#define zero    r11
#define vend    r12

;******************************************************
; Local vector register variables
;******************************************************

#define vtemp   v1
#define vtemp1  v2
#define vtemp2  v3
#define vval    v4
#define vmax    v5
#define vshift  v6

    .text
    .align  2
    .globl  _ix_max_altivec

;******************************************************
; Function ix_max
;******************************************************
_ix_max_altivec:

    mfspr    r0,256                     ; save old contents
    stw      r0,-4(sp)

    lis      r0,0xFE00                  ; Tell the OS which vector 
register to save
    ori      r0,r0,0x0000               ; $FE000000 = v0,v1,v2,v3, 
v4,v5,v6
    mtspr    256,r0

    stw      count,-8(sp)               ; Save used registers
    stw      max,-12(sp)
    stw      temp,-16(sp)
    stw      temp1,-20(sp)
    stw      temp2,-24(sp)
    stw      sixteen,-28(sp)
    stw      zero,-32(sp)
    stw      vend,-36(sp)

    xor      max,max,max                ; max = 0
    xor      zero,zero,zero             ; zero = 0
    li       sixteen,16                 ; sixteen = 16

    sub      count,end,ix               ; Calculate the number of loops 
the altivec
    srwi     count,count,4              ; unit can process.

;******************************************************
; Do we bother with altivec?
;******************************************************
    cmpwi    count,2                    ; less then two ?
    blt      nvLoop                     ; if so, overhead to great, skip 
altivec routines

;******************************************************
; Yes, now set up registers
;******************************************************
    vxor     vmax,vmax,vmax             ; clear vmax register
    lvx      vtemp1,ix,zero             ; Pre-load memory
    lvx      vtemp2,ix,sixteen
    lvsl     vshift,0,ix                ; for aligning memory
    subi     vend,end,16                ; n-1

;******************************************************
; Altivec max routine loop
;******************************************************
    .align 5                            ; for cache alignment
vLoop:
    add      ix,ix,sixteen              ; inc pointer to next set of data
    vperm    vval,vtemp1,vtemp2,vshift  ; align memory
    vor      vtemp1,vtemp2,vtemp2       ; copy register
    lvx      vtemp2,ix,sixteen          ; load next set of data
    vmaxsw   vmax,vmax,vval             ; max
    cmpw     ix,vend                    ; at end of loop ?
    blt+     vLoop                      ; branch if not

;******************************************************
; Store vector results on the stack making sure 16
; byte aligned.
;******************************************************
    subi     temp,sp,64                 ; find a safe place on the stack
    sub      count,zero,sixteen         ; $FFFFFFF0 (-16) mask
    and      temp,temp,count            ; make sure it's 16 byte aligned
    stvx     vmax,temp,zero             ; store the data on the stack

;******************************************************
; Load the results into register for comparing.
;******************************************************
    lwz      count,0(temp)              ; load the data into each 
register
    lwz      temp1,4(temp)
    lwz      temp2,8(temp)
    lwz      sixteen,12(temp)

;******************************************************
; perform max compare on the vector results
;******************************************************
L_1:;
    cmpw     count,max                  ; Greater then max ?
    blt      L_2                        ; No, next check
    mr       max,count                  ; Yes, update max

L_2:;
    cmpw     temp1,max
    blt      L_3
    mr       max,temp1

L_3:;
    cmpw     temp2,max
    blt      L_4
    mr       max,temp2

L_4:;
    cmpw     sixteen,max
    blt      nvLoop
    mr       max,sixteen

;******************************************************
; Process any remaining data thats not in 16 bytes long
; sections.
;******************************************************
nvLoop:

    lwz      temp,0(ix)                 ; preload memory

;******************************************************
; non vector max loop
;******************************************************
    .align 5
loop:

    cmpw     ix,end                     ; check for end of loop
    bge      loopEnd                    ; branch if so
    mr       count,temp                 ; copy data
    lwz      temp,4(ix)                 ; load next data
    addi     ix,ix,4                    ; point to next data set
    cmpw     count,max                  ; new data larger then current?
    blt      loop                       ; branch if not.
    mr       max,count                  ; update max value
    b        loop                       ; next loop

;******************************************************
; return register states
;******************************************************
loopEnd:

    mr       r3,max                     ; return value

    lwz      r0,-4(sp)                  ; restore registers
    mtspr    256,r0
    lwz      count,-8(sp)
    lwz      max,-12(sp)
    lwz      temp,-16(sp)
    lwz      temp1,-20(sp)
    lwz      temp2,-24(sp)
    lwz      sixteen,-28(sp)
    lwz      zero,-32(sp)
    lwz      vend,-36(sp)

    blr                                 ; return to calling function

;******************************************************
; End ix_max
;******************************************************
