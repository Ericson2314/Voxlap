; This file has been modified from Ken Silverman's original release
;CPU 686
CPU P3


%DEFINE USEZBUFFER 1      ;To disable, put ; in front of line
%DEFINE LVSID 10          ;log2(VSID) - used for mip-mapping index adjustment
%DEFINE	VSID (1 << LVSID) ;should match VSID in VOXLAP5.H (adjust LVSID, not this)
%DEFINE	LOGPREC	(8+12)

SEGMENT	.data

EXTERN gi       ; dword
EXTERN gpixy    ; dword
EXTERN gixy     ; dword      ;long[2]
EXTERN gpz      ; dword      ;long[2]
EXTERN gdz      ; dword      ;long[2]
EXTERN gxmip    ; dword
EXTERN gxmax    ; dword
EXTERN gcsub    ; dword      ;long[4]
EXTERN gylookup ; dword      ;long[256+4+128+4+...]
EXTERN gmipnum  ; dword
;EXTERN cf       ; dword      ;{ long i0,i1,z0,z1,cx0,cy0,cx1,cy1; }[128]

EXTERN sptr     ; dword

EXTERN skyoff   ; dword      ;Memory offset to start of longitude line
EXTERN skyxsiz  ; dword      ;Size of longitude line
EXTERN skylat   ; dword      ;long[skyxsiz] : latitude's unit dir. vector

;How to declare C-ASM shared variables in the ASM code:
;ASM:                       C:
;   GLOBAL xr0        extern void *xr0;
;   ALIGN 16                 #define lxr0 ((long *)&xr0)
;   xr0: dd 0,0,0,0   #define fxr0 ((float *)&xr0)
;   Use: xr0          Use: lxr0[0-3]  or:  fxr0[0-3]

;EXTERN reax; dword
;EXTERN rebx; dword
;EXTERN recx; dword
;EXTERN redx; dword
;EXTERN resi; dword
;EXTERN redi; dword
;EXTERN rebp; dword
;EXTERN resp; dword
;EXTERN remm; dword  ;long[16]

GLOBAL	cfasm, skycast
ALIGN 16
cfasm times 256*32 db 0
w8bmask0 dq 000ff00ff00ff00ffh
w8bmask1 dq 000f000f000f000f0h
w8bmask2 dq 000e000e000e000e0h
;gyadd dq ((-1) SHL (LOGPREC-16))
mmask dq 0ffff0000ffff0000h
skycast dq 0
gylookoff dd 0
ngxmax dd 0
ce dd 0
espbak dd 0

gylut  dd gylookup
		 dd gylookup+(4*1+256)*4
		 dd gylookup+(4*2+384)*4
		 dd gylookup+(4*3+448)*4
		 dd gylookup+(4*4+480)*4
		 dd gylookup+(4*5+496)*4
		 dd gylookup+(4*6+504)*4
		 dd gylookup+(4*7+508)*4
		 dd gylookup+(4*8+510)*4

gxmipk dd ((1 << (LVSID-0))-1)*2
		 dd ((1 << (LVSID-1))-1)*2
		 dd ((1 << (LVSID-2))-1)*2
		 dd ((1 << (LVSID-3))-1)*2
		 dd ((1 << (LVSID-4))-1)*2
		 dd ((1 << (LVSID-5))-1)*2
		 dd ((1 << (LVSID-6))-1)*2
		 dd ((1 << (LVSID-7))-1)*2
		 dd ((1 << (LVSID-8))-1)*2

gymipk dd ((1 << (LVSID-0))-1) << (LVSID+2)
		 dd ((1 << (LVSID-1))-1) << (LVSID+1)
		 dd ((1 << (LVSID-2))-1) << (LVSID  )
		 dd ((1 << (LVSID-3))-1) << (LVSID-1)
		 dd ((1 << (LVSID-4))-1) << (LVSID-2)
		 dd ((1 << (LVSID-5))-1) << (LVSID-3)
		 dd ((1 << (LVSID-6))-1) << (LVSID-4)
		 dd ((1 << (LVSID-7))-1) << (LVSID-5)
		 dd ((1 << (LVSID-8))-1) << (LVSID-6)

gamipk dd sptr
		 dd sptr+(VSID*VSID)*4
		 dd sptr+(VSID*VSID + (VSID*VSID >> 2))*4
		 dd sptr+(VSID*VSID + (VSID*VSID >> 2) + (VSID*VSID >> 4))*4
		 dd sptr+(VSID*VSID + (VSID*VSID >> 2) + (VSID*VSID >> 4) + (VSID*VSID >> 6))*4
		 dd sptr+(VSID*VSID + (VSID*VSID >> 2) + (VSID*VSID >> 4) + (VSID*VSID >> 6) + (VSID*VSID >> 8))*4
		 dd sptr+(VSID*VSID + (VSID*VSID >> 2) + (VSID*VSID >> 4) + (VSID*VSID >> 6) + (VSID*VSID >> 8) + (VSID*VSID >> 10))*4
		 dd sptr+(VSID*VSID + (VSID*VSID >> 2) + (VSID*VSID >> 4) + (VSID*VSID >> 6) + (VSID*VSID >> 8) + (VSID*VSID >> 10) + (VSID*VSID >> 12))*4
		 dd sptr+(VSID*VSID + (VSID*VSID >> 2) + (VSID*VSID >> 4) + (VSID*VSID >> 6) + (VSID*VSID >> 8) + (VSID*VSID >> 10) + (VSID*VSID >> 12) + (VSID*VSID >> 14))*4

gmipcnt db 0

%DEFINE MAXZSIZ 1024 ;WARNING: THIS IS BAD SINCE KV6 format supports up to 65535!

%ifdef USEZBUFFER
EXTERN zbufoff         ; dword
%endif
EXTERN ptfaces16       ; dword

EXTERN caddasm         ; XMMWORD[8]
EXTERN ztabasm         ; XMMWORD[MAXZSIZ+3]
EXTERN kv6colmul       ; qword
EXTERN kv6coladd       ; qword
EXTERN qsum0           ; qword
EXTERN qsum1           ; qword
EXTERN qbplbpp         ; qword
EXTERN kv6frameplace   ; dword[256]
EXTERN kv6bytesperline ; dword[256]
EXTERN scisdist        ; dword


;----------------------------------------------------------------------------

SEGMENT	.text

GLOBAL dep_protect_start ;Data Execution Prevention unlock (works under XP2 SP2)
dep_protect_start:
	ret

ALIGN 16

	;THE INNER LOOP:
	;#ifdef CPU <= PENTIUM II
	;
	;   movd mm3, gylookup[ecx*4] ;mm3: [ 0   0   0  -gy]
	;   por mm3, mm6               ;mm3: [ogx  0   gx -gy]
	;      or:
	;   paddd mm3, gyadd           ;where: gyadd: dq ((-1) SHL (LOGPREC-16))
	;
	;   ...
	;
	;   movq mm7, mm0           ;mm7: [cy0.... cx0....]
	;   psrad mm7, 16           ;mm7: [----cy0 ----cx0]
	;   packssdw mm7, mm7       ;mm7: [cy0 cx0 cy0 cx0]
	;   pmaddwd mm7, mm3        ;mm7: [ 0   0  -decide]
	;   movd eax, mm7
	;   test eax, eax
	;   j ?
	;   ...
	;   paddd mm0, gi
	;
	;#else
	;      ;Do this only when gx/ogx changes
	;   movd mm3, ogx                   ;mm3: [ 0   0  ogx  0 ]
	;      or:
	;   pshufw mm3, mm3, 0e8h            ;mm3: [ gx ogx ogx  0 ]
	;
	;      ;Do this only when ecx/edx changes
	;   pinsrw mm3, gylookup[ecx*2], 0 ;mm3: [ 0   0  ogx -gy]
	;      or:
	;   paddd mm3, gyadd                ;where: gyadd: dq (1 SHL LOGPREC)
	;
	;   ...
	;
	;   pshufw mm7, mm0, 0ddh      ;mm7: [cy0 cx0 cy0 cx0]
	;   pmaddwd mm7, mm3           ;mm7: [ 0   0  -decide]
	;   movd eax, mm7
	;   test eax, eax
	;   j ?
	;   ...
	;   paddd mm0, gi
	;
	;#endif


	;   Register allocation:
	;eax: [.temp1.]     mm0: [cy0.... cx0....]
	;ebx: [.temp2.]     mm1: [cy1.... cx1....]
	;ecx: [.....z0]     mm2: [    temp!!!    ]   //gi[1].. gi[0]..]
	;edx: [.....z1]     mm3: [     temp      ]
	;esi: [..ixy..]     mm4: [??????? csub...]
	;edi: [..v[]..]     mm5: [??????? coltemp]
	;ebp: [...bakj]     mm6: [gx. 0.. ogx 0..]
	;esp: [..c->..]     mm7: [     temp      ]

GLOBAL	grouscanasm ;Visual C entry point (passes parameters by stack)
grouscanasm:
	mov eax, [esp+4]
	push ebx   ;Visual C's _cdecl requires EBX,ESI,EDI,EBP to be preserved
	push esi
	push edi
	push ebp
	mov dword [espbak], esp

	mov edi, eax

		;cfasm:   0-2047  (extra memory for stack)
		;      2048-4095  c and ce always sit in this range ((esp = c) <= ce)
		;      4096-6143  This is where memory for cfasm is actually stored!
		;      6144-8191  (memory never used - this seems unnecessary?)
	mov esp, cfasm+2048 ; _MANUAL FIX_ "offset" in masm means don't bracket
	mov eax, cfasm+4096 ; _MANUAL FIX_ "offset" in masm means don't bracket
	mov ecx, [eax+8]
	mov edx, [eax+12]
	movq mm0, [eax+16]
	movq mm1, [eax+24]
	mov dword [ce], esp

	mov dword [gylookoff], gylookup
	mov byte [gmipcnt], 0

	mov ebp, [gxmax]
	cmp byte [gmipnum], 1
	jle short skipngxmax0
	cmp ebp, [gxmip]
	jle short skipngxmax0
	mov ebp, [gxmip]
skipngxmax0:
	mov [ngxmax], ebp

	mov ebp, [gpz+4]
	sub ebp, [gpz+0]
	shr ebp, 31
	movd mm6, DWORD [gpz+ebp*4]        ;update gx in mm6
	pand mm6, qword [mmask] ; _MANUAL FIX_ square-bracketed operand 2
	mov eax, [gdz+ebp*4]
	add DWORD [gpz+ebp*4], eax

	mov esi, [gpixy]
	cmp edi, [esi]
	je drawflor
	jmp drawceil

drawfwall:
	movzx eax, byte [edi+1]
	cmp eax, edx
	jge drawcwall
	mov ebx, [esp+4+2048]
loop0:
	neg eax
	add eax, edx
	dec edx
	punpcklbw mm5, [edi+eax*4]
	mov eax, [gylookoff]
	movd mm3, dword [eax+edx*4] ;mm3: [ 0   0   0  -gy]
	psubusb mm5, mm4
	pshufw mm2, mm5, 0ffh
	pmulhuw mm5, mm2
	psrlw mm5, 7
	packuswb mm5, mm5
%ifdef USEZBUFFER
	punpckldq mm5, mm6         ;Stuff ogx into hi part of color for Z buffer
%endif
	por mm3, mm6               ;mm3: [ gx  0  ogx -gy]
loop1: ;if (dmulrethigh(gylookup[edx*4],c->cx1,c->cy1,ogx) >= 0) jmp endloop1
	pshufw mm7, mm1, 0ddh      ;mm7: [cy1 cx1 cy1 cx1]
	pmaddwd mm7, mm3           ;mm7: [ 0   0  -decide]
	movd eax, mm7
	test eax, eax              ;if (cy1*ogx ? gy*cx1)
	jle endloop1
	psubd mm1, qword [gi]
%ifdef USEZBUFFER
	movntq [ebx], mm5
	sub ebx, 8
%else
	movd [ebx], mm5
	sub ebx, 4
%endif
	cmp ebx, [esp+2048]
	jnb loop1
	jmp predeletez
endloop1:
	movzx eax, byte [edi+1]
	cmp eax, edx
	jne loop0
	mov [esp+4+2048], ebx

drawcwall:
	cmp edi, [esi]
	mov edx, eax
	je predrawflor

	movzx eax, byte [edi+3]
	cmp eax, ecx
	jle predrawceil
	mov ebx, [esp+2048]
loop2:
	neg eax
	add eax, ecx
	inc ecx
	punpcklbw mm5, [edi+eax*4]
	mov eax, [gylookoff]
	movd mm3, dword [eax+ecx*4] ;mm3: [ 0   0   0  -gy]
	psubusb mm5, mm4
	pshufw mm2, mm5, 0ffh
	pmulhuw mm5, mm2
	psrlw mm5, 7
	packuswb mm5, mm5
%ifdef USEZBUFFER
	punpckldq mm5, mm6         ;Stuff ogx into hi part of color for Z buffer
%endif
	por mm3, mm6               ;mm3: [ gx  0  ogx -gy]
loop3: ;if (dmulrethigh(gylookup[ecx*4],c->cx0,c->cy0,ogx) < 0) jmp endloop3
	pshufw mm7, mm0, 0ddh      ;mm7: [cy0 cx0 cy0 cx0]
	pmaddwd mm7, mm3           ;mm7: [ 0   0  -decide]
	movd eax, mm7
	test eax, eax              ;if (cy0*ogx ? gy*cx0)
	jg endloop3
	paddd mm0, qword [gi]
%ifdef USEZBUFFER
	movntq [ebx], mm5
	add ebx, 8
%else
	movd [ebx], mm5
	add ebx, 4
%endif
	cmp ebx, [esp+4+2048]
	jna loop3
	jmp predeletez
endloop3:
	movzx eax, byte [edi+3]
	cmp eax, ecx
	jne loop2
	mov [esp+2048], ebx

predrawceil:
	mov ecx, eax
	pshufw mm6, mm6, 04eh       ;swap hi & lo of mm6
drawceil: ;if (dmulrethigh(gylookup[ecx*4],c->cx0,c->cy0,gx) < 0) jmp drawflor
	mov eax, [gylookoff]
	movd mm3, dword [eax+ecx*4] ;mm3: [ 0   0   0  -gy]
	por mm3, mm6               ;mm3: [ogx  0   gx -gy]
drawceilloop:
	pshufw mm7, mm0, 0ddh      ;mm7: [cy0 cx0 cy0 cx0]
	pmaddwd mm7, mm3           ;mm7: [ 0   0  -decide]
	movd eax, mm7
	test eax, eax              ;if (cy0*gx ? gy*cx0)
	jg drawflor
	paddd mm0, qword [gi]
	mov eax, [esp+2048]

	punpcklbw mm5, [edi-4]
	psubusb mm5, qword [gcsub+16]
	pshufw mm2, mm5, 0ffh
	pmulhuw mm5, mm2
	psrlw mm5, 7
	packuswb mm5, mm5
%ifdef USEZBUFFER
	punpckldq mm5, mm6         ;Stuff gx into hi part of color for Z buffer
	movntq [eax], mm5
	add eax, 8
%else
	movd [eax], mm5
	add eax, 4
%endif
	mov [esp+2048], eax
	cmp eax, [esp+4+2048]
	jna drawceilloop
	jmp deletez

predrawflor:
	pshufw mm6, mm6, 04eh       ;swap hi & lo of mm6
drawflor: ;if (dmulrethigh(gylookup[edx*4],c->cx1,c->cy1,gx) >= 0) jmp enddrawflor
	mov eax, [gylookoff]
	movd mm3, dword [eax+edx*4] ;mm3: [ 0   0   0  -gy]
	por mm3, mm6               ;mm3: [ogx  0   gx -gy]
drawflorloop:
	pshufw mm7, mm1, 0ddh      ;mm7: [cy1 cx1 cy1 cx1]
	pmaddwd mm7, mm3           ;mm7: [ 0   0  -decide]
	movd eax, mm7
	test eax, eax              ;if (cy1*gx ? gy*cx1)
	jle enddrawflor
	psubd mm1, qword [gi]
	mov eax, [esp+4+2048]

	punpcklbw mm5, [edi+4]
	psubusb mm5, qword [gcsub+24]
	pshufw mm2, mm5, 0ffh
	pmulhuw mm5, mm2
	psrlw mm5, 7
	packuswb mm5, mm5
%ifdef USEZBUFFER
	punpckldq mm5, mm6         ;Stuff gx into hi part of color for Z buffer
	movntq [eax], mm5
	sub eax, 8
%else
	movd [eax], mm5   ;(Used to page fault here)
	sub eax, 4
%endif
	mov [esp+4+2048], eax
	cmp eax, [esp+2048]
	jnb drawflorloop
	jmp deletez

enddrawflor:
	mov ebx, esp
afterdelete:
	sub esp, 32
	cmp esp, cfasm+2048 ; _MANUAL FIX_ "offset" in masm means don't bracket
	jae skipixy

	movq mm4, qword [gcsub+ebp*8]
	add esi, [gixy+ebp*4]
	mov ebp, [gpz+4]
	mov edi, [esi]
	sub ebp, [gpz+0]
	shr ebp, 31
	mov eax, [gpz+ebp*4]
	movd mm7, eax
	punpckldq mm6, mm7
	pand mm6, qword [mmask] ; _MANUAL FIX_ square-bracketed operand 2
	cmp eax, [ngxmax]
	ja remiporend
	add eax, [gdz+ebp*4]
	mov DWORD [gpz+ebp*4], eax
	mov esp, [ce]
	jmp skipixy2

skipixy:
	pshufw mm6, mm6, 04eh       ;swap hi & lo of mm6
skipixy2:
	cmp ebx, esp
	je skipixy3
	add ebx, 2048
	mov [ebx+8], ecx
	mov [ebx+12], edx
	movq [ebx+16], mm0
	movq [ebx+24], mm1
	lea ebx, [esp+2048]
	mov ecx, [ebx+8]
	mov edx, [ebx+12]
	movq mm0, [ebx+16]
	movq mm1, [ebx+24]
skipixy3:

		;Find highest intersecting vbuf slab
	cmp byte [edi], 0
	je drawfwall
	mov ebx, [gylookoff]
	jmp intoslabloop
findslabloop:
	lea edi, [edi+eax*4]
	cmp byte [edi], 0
	je drawfwall
intoslabloop:
	movzx eax, byte [edi+2]
		;if (dmulrethigh(gylookup[[edi+2]*4+4],c->cx0,c->cy0,ogx) >= 0)
		;   jmp findslabloopbreak
	movd mm3, dword [ebx+eax*4+4]    ;mm3: [ 0   0   0  -gy]
	por mm3, mm6               ;mm3: [ gx  0  ogx -gy]
	pshufw mm7, mm0, 0ddh      ;mm7: [cy0 cx0 cy0 cx0]
	pmaddwd mm7, mm3           ;mm7: [ 0   0  -decide]
	movd eax, mm7
	test eax, eax              ;if (cy0*ogx ? ?y*cx0)

	movzx eax, byte [edi]
	jg findslabloop

		;If next slab ALSO intersects, split cfasm!
		;if (dmulrethigh(v[v[0]*4+3],c->cx1,c->cy1,ogx) >= 0) jmp drawfwall
	movzx eax, byte [3+edi+eax*4]
	movd mm3, dword [ebx+eax*4]      ;mm3: [ 0   0   0  -gy]
	por mm3, mm6               ;mm3: [ gx  0  ogx -gy]
	pshufw mm7, mm1, 0ddh      ;mm7: [cy1 cx1 cy1 cx1]
	pmaddwd mm7, mm3           ;mm7: [ 0   0  -decide]
	movd eax, mm7
	test eax, eax              ;if (cy1*ogx ? ?y*cx1)
	jle drawfwall


		;Make sure everything is in memory at this point
	lea eax, [esp+2048]
	mov [eax+8], ecx
	mov [eax+12], edx
	movq [eax+16], mm0
	movq [eax+24], mm1

		;(ecx and edx are free registers at this point)

	mov edx, [eax+4]             ;col = (long)c->i1;
	movzx eax, byte [edi+2]  ;dax = c->cx1; day = c->cy1;
	movd mm3, dword [ebx+eax*4+4]      ;mm3: [ 0   0   0  -gy]
	por mm3, mm6                 ;mm3: [ gx  0  ogx -gy]

		;WARNING: NEW CODE!!!!!!!
prebegsearchi16:
	movq mm7, qword [gi]
	pslld mm7, 4
	movq mm5, mm1
	psubd mm5, mm7             ;mm7: [day.... dax....]
	pshufw mm7, mm5, 0ddh      ;mm7: [day dax day dax]
	pmaddwd mm7, mm3           ;mm7: [ 0   0  -decide]
	movd eax, mm7
	test eax, eax              ;if (day*ogx ? gy*dax)
	jle begsearchi
	movq mm1, mm5
%ifdef USEZBUFFER
	sub edx, 16 << 3
%else
	sub edx, 16 << 2
%endif
	jmp prebegsearchi16

	jmp begsearchi
		;while (dmulrethigh(gylookup[v[2]+1],dax,day,ogx) < 0)
prebegsearchi:
%ifdef USEZBUFFER
	sub edx, 4 << 1             ;col -= 8;
%else
	sub edx, 4                   ;col -= 4;
%endif
	psubd mm1, qword [gi]     ;dax -= gi[0]; day -= gi[1];
begsearchi:
	pshufw mm7, mm1, 0ddh      ;mm7: [day dax day dax]
	pmaddwd mm7, mm3           ;mm7: [ 0   0  -decide]
	movd eax, mm7
	test eax, eax              ;if (day*ogx ? gy*dax)
	jg prebegsearchi

	mov eax, [ce]            ;ce++;
	add eax, 32

	cmp eax, cfasm+4096 ;VERY BAD!!! - Interrupt would overwrite data! ; _MANUAL FIX_ "offset" in masm means don't bracket
	ja retsub                    ;Just in case, return early to prevent lockup.

	mov dword [ce], eax
	cmp eax, esp           ;for(c2=ce;c2>c;c2--)   //(c2 = eax)
	jbe skipinsertloop
beginsertloop:
	movq mm5, [eax-32+24+2048]  ;c2[0] = c2[-1];
	movq mm7, [eax-32+16+2048]
	movq [eax+24+2048], mm5
	movq [eax+16+2048], mm7
	movq mm5, [eax-32+8+2048]
	movq mm7, [eax-32+0+2048]
	movq [eax+8+2048], mm5
	movq [eax+0+2048], mm7
	sub eax, 32
	cmp eax, esp
	ja beginsertloop
skipinsertloop:

	movzx eax, byte [edi]
	movq mm7, mm1              ;c[1].cx1 = dax; c[1].cy1 = day;
	paddd mm7, qword [gi]
	movzx eax, byte [3+edi+eax*4]
	mov [esp+32+4+2048], edx        ;c[1].i1 = (long *)col;
%ifdef USEZBUFFER
	add edx, 8                      ;c[0].i0 = (long *)(col+(4<<1));
%else
	add edx, 4                      ;c[0].i0 = (long *)(col+4);
%endif
	mov [esp+2048], edx
	mov edx, eax               ;c[1].z1 = c[0].z0 = v[(v[0]<<2)+3];
	mov [esp+8+2048], eax
	movq [esp+16+2048], mm7         ;c[0].cx0 = dax+gi[0]; c[0].cy0 = day+gi[1];
	add esp, 32                ;c++;
	jmp drawfwall

remiporend:
	mov al, [gmipcnt]
	inc al
	cmp al, byte [gmipnum]
	jge startsky
	mov [gmipcnt], al

	sub esi, sptr

	mov eax, esi
	shl eax, 29
	xor eax, [gixy+0]
	mov eax, [gdz+0]
	js short skipbladd0
	add DWORD [gpz+0], eax
skipbladd0:
	add eax, eax
	jno short skipremip0
	mov DWORD [gpz+0], 7fffffffh
	xor eax, eax
skipremip0:
	mov DWORD [gdz+0], eax

	mov [ebx+8+2048], ecx ;this is the official place to backup ecx

	mov eax, esi
	mov cl, byte [gmipcnt]
	add cl, 31-1-2-LVSID
	shl eax, cl
	xor eax, [gixy+4]
	mov eax, [gdz+4]
	js short skipbladd1
	add DWORD [gpz+4], eax
skipbladd1:
	add eax, eax
	jno short skipremip1
	mov DWORD [gpz+4], 7fffffffh
	xor eax, eax
skipremip1:
	mov DWORD [gdz+4], eax

	shr esi, 2
	mov eax, esi
	movzx ecx, byte [gmipcnt]
	and esi, [gxmipk+ecx*4] ;mask for x (1:1024->512, etc...)
	and eax, [gymipk+ecx*4] ;mask for y (1:1024->512, etc...)
	lea esi, [eax+esi*2]
	add esi, [gamipk+ecx*4] ;add offset (1:sptr+1024*1024*4, etc...)

	movzx eax, byte [gmipcnt]
	mov eax, [gylut+eax*4]
	mov [gylookoff], eax

	sar DWORD [gixy+4], 1

	mov eax, cfasm+2048 ; _MANUAL FIX_ "offset" in masm means don't bracket
startremip0:
	shr dword [eax+8+2048], 1
	inc dword [eax+12+2048]
	shr dword [eax+12+2048], 1
	add eax, 32
	cmp eax, [ce]
	jbe short startremip0

	mov eax, [ngxmax]
	cmp eax, [gxmax]
	jae short startsky
	add eax, eax
	jo skipngxmax1 ;Make sure it doesn't overflow to negative!
	cmp eax, [gxmax]
	jl short skipngxmax2
skipngxmax1:
	mov eax, [gxmax]
skipngxmax2:
	mov [ngxmax], eax

		;register fix-ups after here:
	mov ecx, [ebx+8+2048] ;this is the official place to restore ecx
	shr ecx, 1
	inc edx
	shr edx, 1

		;this makes grid transition clean
	mov ebp, [gpz+4]
	sub ebp, [gpz+0]
	shr ebp, 31
	mov eax, [gpz+ebp*4]
	add eax, [gdz+ebp*4]
	mov DWORD [gpz+ebp*4], eax
	mov edi, [esi]

	mov esp, [ce]
	jmp skipixy2

startsky:
	mov esp, cfasm+2048
	cmp esp, [ce]
	ja retsub
	mov esi, [skyoff]
	test esi, esi
	jnz short prestartskyloop

;Sky not loaded, so fill with black ------------------------------------------
endprebegloop:
	movq mm5, [skycast] ; _MANUAL FIX_ square-bracketed operand 2
	mov eax, [esp+2048]
	mov ebx, [esp+4+2048]
	cmp eax, ebx
	ja short endnextloop
endbegloop:
%ifdef USEZBUFFER
	movntq [eax], mm5
	add eax, 8
%else
	movd [eax], mm5
	add eax, 4
%endif
	cmp eax, ebx
	jbe short endbegloop
endnextloop:
	add esp, 32
	cmp esp, [ce]
	jbe short endprebegloop
	jmp short retsub

;Sky loaded: do texture mapping ----------------------------------------------

prestartskyloop:
	movq qword [ebx+24+2048], mm1 ;Hack to make sure [cy0,cx0] is in memory for sky  ; _MANUAL FIX_ square-bracket now in correct spot. OLD: movq [qword ebx+24+2048],

	mov esi, [skyoff]
	mov ecx, [skylat]
	movd mm5, dword [skycast+4]
	mov edi, [skyxsiz]
startskyloop:
	mov eax, [esp+2048]
	mov ebx, [esp+4+2048]
	cmp eax, ebx
	ja short endskyslab
	movq mm1, [esp+24+2048]    ;mm1: [cy1.... cx1....]
preskysearch:
	psubd mm1, qword [gi]
skysearch:
	pshufw mm7, mm1, 0ddh      ;mm7: [cy1 cx1 cy1 cx1]
	movd mm3, dword [ecx+edi*4]      ;mm3: [       xvi -yvi]
	pmaddwd mm7, mm3           ;mm7: [ 0   0  -decide]
	movd edx, mm7
	sar edx, 31
	lea edi, [edi+edx]
	jnz short skysearch        ;if (cy1*xvi ? -yvi*cx1)

	movd mm6, dword [esi+edi*4]
%ifdef USEZBUFFER
	punpckldq mm6, mm5
	movntq [ebx], mm6
	sub ebx, 8
%else
	movd [ebx], mm6
	sub ebx, 4
%endif
	cmp eax, ebx
	jbe short preskysearch
endskyslab:
	add esp, 32
	cmp esp, [ce]
	jbe short startskyloop

;-----------------------------------------------------------------------

retsub:
	emms
	mov esp, dword [espbak]
	pop ebp    ;Visual C's _cdecl requires EBX,ESI,EDI,EBP to be preserved
	pop edi
	pop esi
	pop ebx
	retn

predeletez:
	pshufw mm6, mm6, 04eh       ;swap hi & lo of mm6
deletez:
	mov ebx, [ce]
	sub ebx, 32
	cmp ebx, [cfasm+2048]
	jb retsub          ;nothing to fill - skip remiporend stuff!
	mov dword [ce], ebx

	add ebx, 32

	cmp esp, ebx       ;while (eax <= ce)
	jae afterdelete
	mov eax, esp
deleteloop:
	movq mm5, [eax+32+0+2048]
	movq mm7, [eax+32+8+2048]
	movq [eax+0+2048], mm5
	movq [eax+8+2048], mm7
	movq mm5, [eax+32+16+2048]
	movq mm7, [eax+32+24+2048]
	movq [eax+16+2048], mm5
	movq [eax+24+2048], mm7
	add eax, 32
	cmp eax, ebx
	jb deleteloop
	jmp afterdelete

;debugret:
;   mov reax, eax
;   mov rebx, ebx
;   mov recx, ecx
;   mov redx, edx
;   mov resi, esi
;   mov redi, edi
;   mov rebp, ebp
;   mov resp, esp
;   movq remm[0], mm0
;   movq remm[8], mm1
;   movq remm[16], mm2
;   movq remm[24], mm3
;   movq remm[32], mm4
;   movq remm[40], mm5
;   movq remm[48], mm6
;   movq remm[56], mm7
;   emms
;   pop ebp
;   ret


GLOBAL	drawboundcubesseinit   ;Visual C entry point (pass by stack)
drawboundcubesseinit:
	mov eax, [kv6frameplace]
	mov dword [bcmod0-4], eax
	mov eax, [kv6bytesperline]
	mov dword [bcmod3-4], eax
%ifdef USEZBUFFER
	;mov eax, kv6bytesperline
	mov dword [bcmod2-4], eax
	mov eax, [zbufoff]
	mov dword [bcmod1-4], eax
%endif
	retn       ;Visual C's _cdecl requires EBX,ESI,EDI,EBP to be preserved

ALIGN 16
GLOBAL	drawboundcubesse       ;Visual C entry point (pass by stack)
drawboundcubesse:
	mov eax, [esp+4]
	mov ecx, [esp+8]
	push ebx   ;Visual C's _cdecl requires EBX,ESI,EDI,EBP to be preserved
	push edi

	movzx edi, byte [eax+6]
	and ecx, edi
	jz retboundcube

	movaps xmm7, [ztabasm+MAXZSIZ*16] ; _MANUAL FIX_ Remove DWORD prefix from operand 2
	movzx edi, word [eax+4]
	shl edi, 4
	addps xmm7, [ztabasm+edi] ; _MANUAL FIX_ Remove DWORD prefix from operand 2
	movhlps xmm0, xmm7
	ucomiss xmm0, [scisdist] ; _MANUAL FIX_ Remove DWORD prefix from operand 2
	jc retboundcube

	lea ecx, [ptfaces16+ecx*8]

	movzx ebx, byte [ecx+1] ;                           Ý
	movzx edi, byte [ecx+2] ;                           Ý
	movaps xmm0, [caddasm+ebx] ;xmm0: [ z0, z0, y0, x0]    Û ; _MANUAL FIX_ Remove DWORD prefix from operand 2
	addps xmm0, xmm7            ;                           ÛÛ±
	movaps xmm1, [caddasm+edi] ;xmm1: [ z1, z1, y1, x1]    Û ; _MANUAL FIX_ Remove DWORD prefix from operand 2
	addps xmm1, xmm7            ;                           ÛÛ±
	movaps xmm6, xmm0           ;xmm6: [ z0, z0, y0, x0]    Û
	movhlps xmm0, xmm1          ;xmm0: [ z0, z0, z1, z1]    Û
	movlhps xmm1, xmm6          ;xmm1: [ y0, x0, y1, x1]    Û
	rcpps xmm0, xmm0            ;xmm6: [/z0,/z0,/z1,/z1]    ÛÛ
	mulps xmm0, xmm1            ;xmm0: [sy0,sx0,sy1,sx1]    ÛÛ±±

	movzx ebx, byte [ecx+3] ;                           Ý
	movzx edi, byte [ecx+4] ;                           Ý
	movaps xmm2, [caddasm+ebx] ;xmm2: [ z2, z2, y2, x2]    Û ; _MANUAL FIX_ Remove DWORD prefix from operand 2
	addps xmm2, xmm7            ;                           ÛÛ±
	movaps xmm3, [caddasm+edi] ;xmm3: [ z3, z3, y3, x3]    Û ; _MANUAL FIX_ Remove DWORD prefix from operand 2
	addps xmm3, xmm7            ;                           ÛÛ±
	movaps xmm6, xmm2           ;xmm6: [ z2, z2, y2, x2]    Û
	movhlps xmm2, xmm3          ;xmm2: [ z2, z2, z3, z3]    Û
	movlhps xmm3, xmm6          ;xmm3: [ y2, x2, y3, x3]    Û
	rcpps xmm2, xmm2            ;xmm6: [/z2,/z2,/z3,/z3]    ÛÛ
	mulps xmm2, xmm3            ;xmm2: [sy2,sx2,sy3,sx3]    ÛÛ±±

	cvttps2pi mm0, xmm0         ;                           Û
	movhlps xmm0, xmm0          ;                           Û
	cvttps2pi mm2, xmm2         ;                           Û
	cvttps2pi mm1, xmm0         ;                           Û
	movhlps xmm2, xmm2          ;                           Û
	packssdw mm0, mm1           ;                           Ý
	movq mm1, mm0               ;                           Ý
	cvttps2pi mm3, xmm2         ;                           Û
	packssdw mm2, mm3           ;                           Ý
	pminsw mm0, mm2             ;                           Ý
	pmaxsw mm1, mm2             ;                           Ý

	cmp byte [ecx], 4
	je short bcskip6case

	movzx ebx, byte [ecx+5] ;                           Ý
	movzx edi, byte [ecx+6] ;                           Ý
	movaps xmm4, [caddasm+ebx] ;xmm4: [ z4, z4, y4, x4]    Û ; _MANUAL FIX_ Remove DWORD prefix from operand 2
	addps xmm4, xmm7            ;                           ÛÛ±
	movaps xmm5, [caddasm+edi] ;xmm5: [ z5, z5, y5, x5]    Û ; _MANUAL FIX_ Remove DWORD prefix from operand 2
	addps xmm5, xmm7            ;                           ÛÛ±
	movaps xmm6, xmm4           ;xmm6: [ z4, z4, y4, x4]    Û
	movhlps xmm4, xmm5          ;xmm4: [ z4, z4, z5, z5]    Û
	movlhps xmm5, xmm6          ;xmm5: [ y4, x4, y5, x5]    Û
	rcpps xmm4, xmm4            ;xmm6: [/z4,/z4,/z5,/z5]    ÛÛ
	mulps xmm4, xmm5            ;xmm4: [sy4,sx4,sy5,sx5]    ÛÛ±±

	cvttps2pi mm4, xmm4         ;                           Û
	movhlps xmm4, xmm4          ;                           Û
	cvttps2pi mm5, xmm4         ;                           Û
	packssdw mm4, mm5           ;                           Ý
	pminsw mm0, mm4             ; mm0: [my1,mx1,my0,mx0]    Ý
	pmaxsw mm1, mm4             ; mm1: [My1,Mx1,My0,Mx0]    Ý
bcskip6case:

	pshufw mm2, mm0, 0eh        ; mm2: [   ,   ,my1,mx1]    Û
	pshufw mm3, mm1, 0eh        ; mm3: [   ,   ,My1,Mx1]    Û
	pminsw mm0, mm2             ; mm0: [  ?,  ?, my, mx]    Ý
	pmaxsw mm1, mm3             ; mm1: [  ?,  ?, My, Mx]    Ý
	punpckldq mm0, mm1          ; mm0: [ My, Mx, my, mx]    Ý

		;See SCRCLP2D.BAS for a derivation of these 4 lines:
	paddsw mm0, [qsum0]           ; mm0: ["+?,"+?,"+?,"+?]    Û
	pmaxsw mm0, [qsum1]           ; mm0: [sy1,sx1,sy0,sx0]    Û
	pshufw mm1, mm0, 0eeh       ; mm1: [sy1,sx1,sy1,sx1]    Û
	psubusw mm1, mm0            ; mm1: [  0,  0, dy, dx]    Ý
		;kv6frameplace -= ((32767-yres)*bpl + (32767-xres)*4);

	movd edx, mm1               ; edx: [ dy, dx]            Û
	pmaddwd mm0, [qbplbpp]     ; mm0: [      ?,   offs]    Û±± (=y*bpl+x*bpp) ; _MANUAL FIX_ OLD: pmaddwd mm0, qbplbpp
	movd ebx, mm1               ; ebx: [ dy, dx]            Ý
	and edx, 0ffffh             ; ebx: [  0, dx]            Ý
	jz short retboundcube       ;                           Ý
	sub ebx, 65536              ;                           Ý
	jc short retboundcube       ;                           Ý

	movzx edi, byte [eax+7]
	punpcklbw mm5, [eax]
	pmulhuw mm5, [kv6colmul+edi*8]
	paddw mm5, [kv6coladd] ; _MANUAL FIX_ OLD: paddw mm5, kv6coladd 
	packuswb mm5, mm5
	movd edi, mm0               ; edi: offs

	lea edi, [edi+edx*4+88888888h] ;kv6frameplace
bcmod0:
	neg edx
%ifdef USEZBUFFER
	movhlps xmm0, xmm7
	lea eax, [edi+88888888h] ;zbufoff
bcmod1:
%endif
boundcubenextline:
	mov ecx, edx
begstosb:
%ifdef USEZBUFFER
	ucomiss xmm0, [eax+ecx*4]
	jnc short skipdrawpix
	movss [eax+ecx*4], xmm0
%endif
	movd [edi+ecx*4], mm5
skipdrawpix:
	inc ecx
	jnz begstosb
%ifdef USEZBUFFER
	add eax, 88888888h; kv6bytesperline
bcmod2:
%endif
	add edi, 88888888h ;kv6bytesperline
bcmod3:

	sub ebx, 65536
	jnc short boundcubenextline

retboundcube:
	pop edi    ;Visual C's _cdecl requires EBX,ESI,EDI,EBP to be preserved
	pop ebx
	retn

GLOBAL	drawboundcube3dninit   ;Visual C entry point (pass by stack)
drawboundcube3dninit:
	mov eax, [kv6frameplace]
	mov dword [bcmod0_3dn-4], eax
	mov eax, [kv6bytesperline]
	mov dword [bcmod3_3dn-4], eax
%ifdef USEZBUFFER
	;mov eax, kv6bytesperline
	mov dword [bcmod2_3dn-4], eax
	mov eax, [zbufoff]
	mov dword [bcmod1_3dn-4], eax
%endif
	retn       ;Visual C's _cdecl requires EBX,ESI,EDI,EBP to be preserved

ALIGN 16
GLOBAL	drawboundcube3dn       ;Visual C entry point (pass by stack)
drawboundcube3dn:
	mov eax, [esp+4]
	mov ecx, [esp+8]
	push ebx   ;Visual C's _cdecl requires EBX,ESI,EDI,EBP to be preserved
	push edi

	movzx edi, byte [eax+6]
	and ecx, edi
	jz retboundcube_3dn

	movq mm6, qword [ztabasm+MAXZSIZ*16]
	movq mm7, qword [ztabasm+MAXZSIZ*16+8]
	movzx edi, word [eax+4]
	shl edi, 4
	pfadd mm6, qword [ztabasm+edi]
	pfadd mm7, qword [ztabasm+edi+8]
	movq mm0, mm7
	pcmpgtd mm0, qword [scisdist]
	movd edx, mm0
	test edx, edx
	jz retboundcube_3dn

	lea ecx, [ptfaces16+ecx*8]

	movzx ebx, byte [ecx+1]
	movzx edi, byte [ecx+2]
	movq mm0, qword [caddasm+ebx]
	movq mm1, qword [caddasm+edi]
	pfadd mm0, mm6              ;mm0: [   y0    x0]
	pfadd mm1, mm6              ;mm1: [   y1    x1]
	movd mm5, dword [caddasm+ebx+8]
	punpckldq mm5, [caddasm+edi+8]
	pfadd mm5, mm7              ;mm5: [   z1    z0]
	pfrcp mm4, mm5              ;mm4: [ 1/z0  1/z0]
	punpckhdq mm5, mm5          ;mm5: [   z1    z1]
	pfrcp mm5, mm5              ;mm5: [ 1/z1  1/z1]
	pfmul mm0, mm4              ;mm0: [y0/z0 x0/z0]
	pfmul mm1, mm5              ;mm1: [y1/z1 x1/z1]
	pf2id mm0, mm0              ;mm0: [  sy0   sx0]
	pf2id mm1, mm1              ;mm1: [  sy1   sx1]
	packssdw mm0, mm1           ;mm0: [sy1 sx1 sy0 sx0]

	movzx ebx, byte [ecx+3]
	movzx edi, byte [ecx+4]
	movq mm2, qword [caddasm+ebx]
	movq mm3, qword [caddasm+edi]
	pfadd mm2, mm6              ;mm2: [   y2    x2]
	pfadd mm3, mm6              ;mm3: [   y3    x3]
	movd mm5, dword [caddasm+ebx+8]
	punpckldq mm5, [caddasm+edi+8]
	pfadd mm5, mm7              ;mm5: [   z3    z2]
	pfrcp mm4, mm5              ;mm4: [ 1/z2  1/z2]
	punpckhdq mm5, mm5          ;mm5: [   z3    z3]
	pfrcp mm5, mm5              ;mm5: [ 1/z3  1/z3]
	pfmul mm2, mm4              ;mm2: [y2/z2 x2/z2]
	pfmul mm3, mm5              ;mm3: [y3/z3 x3/z3]
	pf2id mm2, mm2              ;mm2: [  sy2   sx2]
	pf2id mm3, mm3              ;mm3: [  sy3   sx3]
	packssdw mm2, mm3           ;mm2: [sy3 sx3 sy2 sx2]

	movq mm1, mm0
	pminsw mm0, mm2             ;mm0: [sy1 sx1 sy0 sx0] <-min
	pmaxsw mm1, mm2             ;mm1: [sy1 sx1 sy0 sx0] <-max

	cmp byte [ecx], 4
	je short bcskip6case_3dn

	movzx ebx, byte [ecx+5]
	movzx edi, byte [ecx+6]
	movq mm2, qword [caddasm+ebx]
	movq mm3, qword [caddasm+edi]
	pfadd mm2, mm6              ;mm2: [   y4    x4]
	pfadd mm3, mm6              ;mm3: [   y5    x5]
	movd mm5, dword [caddasm+ebx+8]
	punpckldq mm5, [caddasm+edi+8]
	pfadd mm5, mm7              ;mm5: [   z5    z4]
	pfrcp mm4, mm5              ;mm4: [ 1/z4  1/z4]
	punpckhdq mm5, mm5          ;mm5: [   z5    z5]
	pfrcp mm5, mm5              ;mm5: [ 1/z5  1/z5]
	pfmul mm2, mm4              ;mm2: [y4/z4 x4/z4]
	pfmul mm3, mm5              ;mm3: [y5/z5 x5/z5]
	pf2id mm2, mm2              ;mm2: [  sy4   sx4]
	pf2id mm3, mm3              ;mm3: [  sy5   sx5]
	packssdw mm2, mm3           ;mm2: [sy5 sx5 sy4 sx4]

	pminsw mm0, mm2             ; mm0: [my1,mx1,my0,mx0]
	pmaxsw mm1, mm2             ; mm1: [My1,Mx1,My0,Mx0]
bcskip6case_3dn:

	pshufw mm2, mm0, 0eh        ; mm2: [my0,mx0,my1,mx1]
	pshufw mm3, mm1, 0eh        ; mm3: [My0,Mx0,My1,Mx1]
	pminsw mm0, mm2             ; mm0: [  ?,  ?, my, mx]
	pmaxsw mm1, mm3             ; mm1: [  ?,  ?, My, Mx]
	punpckldq mm0, mm1          ; mm0: [ My, Mx, my, mx]

		;See SCRCLP2D.BAS for a derivation of these 4 lines:
	paddsw mm0, [qsum0]  ; mm0: ["+?,"+?,"+?,"+?]    Û ; _MANUAL FIX_ square-bracketed operand 2
	pmaxsw mm0, [qsum1]  ; mm0: [sy1,sx1,sy0,sx0]    Û ; _MANUAL FIX_ square-bracketed operand 2
	pshufw mm1, mm0, 0eeh       ; mm1: [sy1,sx1,sy1,sx1]    Û
	psubusw mm1, mm0            ; mm1: [  0,  0, dy, dx]    Ý
		;kv6frameplace -= ((32767-yres)*bpl + (32767-xres)*4);

	movd edx, mm1               ; edx: [ dy, dx]            Û
	pmaddwd mm0, [qbplbpp]     ; mm0: [      ?,   offs]    Û±± (=y*bpl+x*bpp) ; _MANUAL FIX_ square-bracketed operand 2
	movd ebx, mm1               ; ebx: [ dy, dx]            Ý
	and edx, 0ffffh             ; ebx: [  0, dx]            Ý
	jz short retboundcube_3dn   ;                           Ý
	sub ebx, 65536              ;                           Ý
	jc short retboundcube_3dn   ;                           Ý

	movzx edi, byte [eax+7]
	punpcklbw mm5, [eax]
	pmulhuw mm5, [kv6colmul+edi*8]
	paddw mm5, [kv6coladd] ; _MANUAL FIX_ square-bracketed operand 2
	packuswb mm5, mm5
	movd edi, mm0               ; edi: offs

	lea edi, [edi+edx*4+88888888h] ;kv6frameplace
bcmod0_3dn:
	neg edx
	movd mm1, edx
%ifdef USEZBUFFER
	lea eax, [edi+88888888h] ;zbufoff
bcmod1_3dn:
%endif
boundcubenextline_3dn:
	movd ecx, mm1
begstosb_3dn:
%ifdef USEZBUFFER
	movq mm0, mm7
	pcmpgtd mm0, [eax+ecx*4]
	movd edx, mm0
	test edx, edx
	jnz short skipdrawpix_3dn
	movd dword [eax+ecx*4], mm7
%endif
	movd dword [edi+ecx*4], mm5
skipdrawpix_3dn:
	inc ecx
	jnz begstosb_3dn
%ifdef USEZBUFFER
	add eax, 88888888h ;kv6bytesperline
bcmod2_3dn:
%endif
	add edi, 88888888h ;kv6bytesperline
bcmod3_3dn:

	sub ebx, 65536
	jnc short boundcubenextline_3dn

retboundcube_3dn:
	pop edi    ;Visual C's _cdecl requires EBX,ESI,EDI,EBP to be preserved
	pop ebx
	retn

Global dep_protect_end
dep_protect_end:
;END
