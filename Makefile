game.exe: game.obj voxlap5.obj v5.obj kplib.obj winmain1.obj
	link game voxlap5 v5 kplib winmain1 ddraw.lib dinput.lib ole32.lib dxguid.lib user32.lib gdi32.lib

kwalk.exe: kwalk.obj voxlap5.obj v5.obj kplib.obj winmain2.obj
	link kwalk voxlap5 v5 kplib winmain2 ddraw.lib dinput.lib ole32.lib dxguid.lib user32.lib gdi32.lib comdlg32.lib

simple.exe:  simple.obj voxlap5.obj v5.obj kplib.obj winmain2.obj
	link simple voxlap5 v5 kplib winmain2 ddraw.lib dinput.lib ole32.lib dxguid.lib user32.lib gdi32.lib

voxed.exe: voxed.obj voxlap5.obj v5.obj kplib.obj winmain2.obj
	link voxed voxlap5 v5 kplib winmain2 ddraw.lib dinput.lib dxguid.lib user32.lib gdi32.lib comdlg32.lib

game.obj:     game.c   voxlap5.h sysmain.h
	cl /c /J /TP game.c      /Ox /Ob2 /G6Fy /Gs /MD /QIfist
kwalk.obj:    kwalk.c  voxlap5.h sysmain.h
	cl /c /J /TP kwalk.c     /Ox /Ob2 /G6Fy /Gs /MD
simple.obj:   simple.c voxlap5.h sysmain.h
	cl /c /J /TP simple.c    /Ox /Ob2 /G6Fy /Gs /MD /QIfist
voxed.obj:    voxed.c  voxlap5.h sysmain.h
	cl /c /J /TP voxed.c     /Ox /Ob2 /G6Fy /Gs /MD

voxlap5.obj:  voxlap5.c voxlap5.h
	cl /c /J /TP voxlap5.c   /Ox /Ob2 /G6Fy /Gs /MD
v5.obj:       v5.asm
	ml /c /coff v5.asm
kplib.obj:    kplib.c
	cl /c /J /TP kplib.c     /Ox /Ob2 /G6Fy /Gs /MD
winmain1.obj: winmain.cpp sysmain.h
	cl /c /J /TP winmain.cpp /Ox /Ob2 /G6Fy /Gs /MD /DUSEKZ /DZOOM_TEST /Fowinmain1.obj
winmain2.obj: winmain.cpp
	cl /c /J /TP winmain.cpp /Ox /Ob2 /G6Fy /Gs /MD /DNOSOUND           /Fowinmain2.obj
