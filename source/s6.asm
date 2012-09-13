;S6.ASM by Ken Silverman (http://advsys.net/ken)
;
;License for this code:
;   * No commercial exploitation please
;   * Do not remove my name or credit
;   * You may distribute modified code/executables but please make it clear that it is modified

.686
ifdef __WASM__
include xmm.inc ;To compile with   Watcom  assembler: >wasm s6
else
.XMM            ;To compile with Microsoft assembler: >ml /c /coff s6.asm
endif

BUFZSIZ EQU 256

EXTRN _ptfaces16 : dword
EXTRN _lcol : dword

CODE SEGMENT PUBLIC USE32 'CODE'
ASSUME cs:CODE,ds:CODE

PUBLIC _s6_asm_dep_unlock ;Data Execution Prevention unlock (works under XP2 SP2)
_s6_asm_dep_unlock:
	EXTRN __imp__VirtualProtect@16:NEAR
	sub esp, 4
	push esp
	push 40h ;PAGE_EXECUTE_READWRITE
	push offset _dep_protect_end - offset _s6_asm_dep_unlock
	push offset _s6_asm_dep_unlock
	call dword ptr __imp__VirtualProtect@16
	add esp, 4
	ret

PUBLIC _caddasm, _ztabasm, _qsum0, _qsum1, _qbplbpp, _kv6frameplace, _kv6bpl
ALIGN 16
_caddasm dd 8*4 dup(0)
_ztabasm dd (BUFZSIZ+7)*4 dup(0)
_qsum0 dq 2   ;[7fffh-hy,7fffh-hx,7fffh-hy,7fffh-hx]
_qsum1 dq 3   ;[7fffh-fy,7fffh-fx,7fffh-fy,7fffh-fx]
_qbplbpp dq 5 ;[0,0,bpl,bpp]
_kv6frameplace dd 0
_kv6bpl dd 0

ALIGN 16
PUBLIC _drawboundcubeasm       ;Visual C entry point (pass by stack)
_drawboundcubeasm:
	mov edx, [esp+4]
	mov eax, [esp+8]
	mov ecx, [esp+12]
PUBLIC drawboundcubeasm_       ;Watcom C entry point (pass by register)
drawboundcubeasm_:
	push ebx   ;Visual C's _cdecl requires EBX,ESI,EDI,EBP to be preserved
	push edi
	cmp dword ptr [edx+8], 40000000h
	jle retboundcube

	lea ecx, _ptfaces16[ecx*8]

	movzx ebx, byte ptr [ecx+1] ;                           ›
	movzx edi, byte ptr [ecx+2] ;                           ›
	movaps xmm0, _caddasm[ebx]  ;xmm0: [ z0, z0, y0, x0]    €
	addps xmm0, xmm7            ;                           €€±
	movaps xmm1, _caddasm[edi]  ;xmm1: [ z1, z1, y1, x1]    €
	addps xmm1, xmm7            ;                           €€±
	movaps xmm6, xmm0           ;xmm6: [ z0, z0, y0, x0]    €
	movhlps xmm0, xmm1          ;xmm0: [ z0, z0, z1, z1]    €
	movlhps xmm1, xmm6          ;xmm1: [ y0, x0, y1, x1]    €
	rcpps xmm0, xmm0            ;xmm6: [/z0,/z0,/z1,/z1]    €€
	mulps xmm0, xmm1            ;xmm0: [sy0,sx0,sy1,sx1]    €€±±

	movzx ebx, byte ptr [ecx+3] ;                           ›
	movzx edi, byte ptr [ecx+4] ;                           ›
	movaps xmm2, _caddasm[ebx]  ;xmm2: [ z2, z2, y2, x2]    €
	addps xmm2, xmm7            ;                           €€±
	movaps xmm3, _caddasm[edi]  ;xmm3: [ z3, z3, y3, x3]    €
	addps xmm3, xmm7            ;                           €€±
	movaps xmm6, xmm2           ;xmm6: [ z2, z2, y2, x2]    €
	movhlps xmm2, xmm3          ;xmm2: [ z2, z2, z3, z3]    €
	movlhps xmm3, xmm6          ;xmm3: [ y2, x2, y3, x3]    €
	rcpps xmm2, xmm2            ;xmm6: [/z2,/z2,/z3,/z3]    €€
	mulps xmm2, xmm3            ;xmm2: [sy2,sx2,sy3,sx3]    €€±±

	cvttps2pi mm0, xmm0         ;                           €
	movhlps xmm0, xmm0          ;                           €
	cvttps2pi mm2, xmm2         ;                           €
	cvttps2pi mm1, xmm0         ;                           €
	movhlps xmm2, xmm2          ;                           €
	packssdw mm0, mm1           ;                           ›
	movq mm1, mm0               ;                           ›
	cvttps2pi mm3, xmm2         ;                           €
	packssdw mm2, mm3           ;                           ›
	pminsw mm0, mm2             ;                           ›
	pmaxsw mm1, mm2             ;                           ›

	cmp byte ptr [ecx], 4
	je short skip6case

	movzx ebx, byte ptr [ecx+5] ;                           ›
	movzx edi, byte ptr [ecx+6] ;                           ›
	movaps xmm4, _caddasm[ebx]  ;xmm4: [ z4, z4, y4, x4]    €
	addps xmm4, xmm7            ;                           €€±
	movaps xmm5, _caddasm[edi]  ;xmm5: [ z5, z5, y5, x5]    €
	addps xmm5, xmm7            ;                           €€±
	movaps xmm6, xmm4           ;xmm6: [ z4, z4, y4, x4]    €
	movhlps xmm4, xmm5          ;xmm4: [ z4, z4, z5, z5]    €
	movlhps xmm5, xmm6          ;xmm5: [ y4, x4, y5, x5]    €
	rcpps xmm4, xmm4            ;xmm6: [/z4,/z4,/z5,/z5]    €€
	mulps xmm4, xmm5            ;xmm4: [sy4,sx4,sy5,sx5]    €€±±

	cvttps2pi mm4, xmm4         ;                           €
	movhlps xmm4, xmm4          ;                           €
	cvttps2pi mm5, xmm4         ;                           €
	packssdw mm4, mm5           ;                           ›
	pminsw mm0, mm4             ; mm0: [my1,mx1,my0,mx0]    ›
	pmaxsw mm1, mm4             ; mm1: [My1,Mx1,My0,Mx0]    ›
skip6case:

	pshufw mm2, mm0, 0eh        ; mm2: [   ,   ,my1,mx1]    €
	pshufw mm3, mm1, 0eh        ; mm3: [   ,   ,My1,Mx1]    €
	pminsw mm0, mm2             ; mm0: [  ?,  ?, my, mx]    ›
	pmaxsw mm1, mm3             ; mm1: [  ?,  ?, My, Mx]    ›
	punpckldq mm0, mm1          ; mm0: [ My, Mx, my, mx]    ›

		;See SCRCLP2D.BAS for a derivation of these 4 lines:
	paddsw mm0, _qsum0          ; mm0: ["+?,"+?,"+?,"+?]    €
	pmaxsw mm0, _qsum1          ; mm0: [sy1,sx1,sy0,sx0]    €
	pshufw mm1, mm0, 0eeh       ; mm1: [sy1,sx1,sy1,sx1]    €
	psubusw mm1, mm0            ; mm1: [  0,  0, dy, dx]    ›
		;kv6frameplace -= ((32767-yres)*bpl + (32767-xres)*4);

	pmaddwd mm0, _qbplbpp       ; mm0: [      ?,   offs]    €±± (=y*bpl+x*bpp)
	movd edx, mm1               ; edx: [ dy, dx]            €
	mov ebx, edx                ; ebx: [ dy, dx]            ›
	and edx, 0ffffh             ; ebx: [  0, dx]            ›
	jz short retboundcube       ;                           ›
	sub ebx, 65536              ;                           ›
	jc short retboundcube       ;                           ›
	movd edi, mm0               ; edi: offs                 €
	add edi, _kv6frameplace     ; edi: frameplace           €

boundcubenextline:
	mov ecx, edx
begstosb:
	mov [edi+ecx-1], al
	dec ecx
	jnz begstosb

	add edi, _kv6bpl
	sub ebx, 65536
	jnc short boundcubenextline
retboundcube:
	pop edi    ;Visual C's _cdecl requires EBX,ESI,EDI,EBP to be preserved
	pop ebx
	ret

_dep_protect_end:
CODE ENDS
END
