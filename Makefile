CFLAGS = /J /Ox /Ob2 /G6Fy /Gs /MD

game.exe: game.obj voxlap5.obj v5.obj kplib.obj winmain1.obj
	link game voxlap5 v5 kplib winmain1 ddraw.lib dinput.lib ole32.lib dxguid.lib user32.lib gdi32.lib

kwalk.exe: kwalk.obj voxlap5.obj v5.obj kplib.obj winmain2.obj
	link kwalk voxlap5 v5 kplib winmain2 ddraw.lib dinput.lib ole32.lib dxguid.lib user32.lib gdi32.lib comdlg32.lib

simple.exe:  simple.obj voxlap5.obj v5.obj kplib.obj winmain2.obj
	link simple voxlap5 v5 kplib winmain2 ddraw.lib dinput.lib ole32.lib dxguid.lib user32.lib gdi32.lib

voxed.exe: voxed.obj voxlap5.obj v5.obj kplib.obj winmain2.obj
	link voxed voxlap5 v5 kplib winmain2 ddraw.lib dinput.lib dxguid.lib user32.lib gdi32.lib comdlg32.lib

game.obj:     src/game.c   inc/voxlap5.h inc/sysmain.h
	cl $(CFLAGS) /c /TP $< /QIfist
kwalk.obj:    src/kwalk.c  inc/voxlap5.h inc/sysmain.h
	cl $(CFLAGS) /c /TP $<
simple.obj:   src/simple.c inc/voxlap5.h inc/sysmain.h
	cl $(CFLAGS) /c /TP $< /QIfist
voxed.obj:    src/voxed.c  inc/voxlap5.h inc/sysmain.h
	cl $(CFLAGS) /c /TP $<

voxlap5.obj:  src/voxlap5.c inc/voxlap5.h
	cl $(CFLAGS) /c /TP $<
v5.obj:       src/v5.asm
	ml $(CFLAGS) /c /coff $<
kplib.obj:    src/kplib.c
	cl $(CFLAGS) /c /TP $<
winmain1.obj: src/winmain.cpp inc/sysmain.h
	cl $(CFLAGS) /c /TP $<  /DUSEKZ /DZOOM_TEST /Fowinmain1.obj
winmain2.obj: src/winmain.cpp
	cl $(CFLAGS) /c /TP $<  /DNOSOUND           /Fowinmain2.obj
