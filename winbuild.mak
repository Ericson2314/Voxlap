
Defult: game.exe simple.exe voxed.exe kwalk.exe

# executable (.exe) (meta)targets
game.exe:    game.obj voxlap5.obj v5.obj kplib.obj winmain.obj(1) game.c;
												link game voxlap5 v5 kplib winmain ddraw.lib dinput.lib ole32.lib dxguid.lib user32.lib gdi32.lib
simple.exe : simple.obj voxlap5.obj v5.obj kplib.obj winmain.obj(1)
												link simple voxlap5 v5 kplib winmain ddraw.lib dinput.lib ole32.lib dxguid.lib user32.lib gdi32.lib
voxed.exe: voxed.obj voxlap5.obj v5.obj kplib.obj winmain.obj(2) voxed.c
												link voxed voxlap5 v5 kplib winmain ddraw.lib dinput.lib dxguid.lib user32.lib gdi32.lib comdlg32.lib
kwalk.exe: kwalk.obj voxlap5.obj v5.obj kplib.obj winmain.obj(2)
												link kwalk voxlap5 v5 kplib winmain ddraw.lib dinput.lib ole32.lib dxguid.lib user32.lib gdi32.lib comdlg32.lib

# binary object (.obj) targets
game.obj:		game.c voxlap5.h sysmain.h;		cl /c /J /TP game.c      /Ox /Ob2 /G6Fy /Gs /MD /QIfist
simple.obj:		simple.c voxlap5.h sysmain.h;	cl /c /J /TP simple.c    /Ox /Ob2 /G6Fy /Gs /MD /QIfist
voxed.obj:		voxed.c voxlap5.h sysmain.h;	cl /c /J /TP voxed.c     /Ox /Ob2 /G6Fy /Gs /MD
kwalk.obj:		kwalk.c voxlap5.h sysmain.h;	cl /c /J /TP kwalk.c     /Ox /Ob2 /G6Fy /Gs /MD


voxlap5.obj:	voxlap5.c voxlap5.h;			cl /c /J /TP voxlap5.c   /Ox /Ob2 /G6Fy /Gs /MD
v5.obj:			v5.asm;							ml /c /coff v5.asm
kplib.obj:		kplib.c;						cl /c /J /TP kplib.c     /Ox /Ob2 /G6Fy /Gs /MD
winmain.obj(1):	winmain.cpp sysmain.h    
												del winmain.obj
												cl /c /J /TP winmain.cpp /Ox /Ob2 /G6Fy /Gs /MD /DUSEKZ /DZOOM_TEST										
winmain.obj(2):	winmain.cpp sysmain.h
												del winmain.obj
												cl /c /J /TP winmain.cpp /Ox /Ob2 /G6Fy /Gs /MD /DNOSOUND
# _Original_
# game.exe:    game.obj voxlap5.obj v5.obj kplib.obj winmain.obj game.c; link game voxlap5 v5 kplib winmain ddraw.lib dinput.lib ole32.lib dxguid.lib user32.lib gdi32.lib
# game.obj:    game.c voxlap5.h sysmain.h;  cl /c /J /TP game.c      /Ox /Ob2 /G6Fy /Gs /MD /QIfist
# voxlap5.obj: voxlap5.c voxlap5.h;         cl /c /J /TP voxlap5.c   /Ox /Ob2 /G6Fy /Gs /MD
# v5.obj:      v5.asm;                      ml /c /coff v5.asm
# kplib.obj:   kplib.c;                     cl /c /J /TP kplib.c     /Ox /Ob2 /G6Fy /Gs /MD
# winmain.obj: winmain.cpp sysmain.h;       cl /c /J /TP winmain.cpp /Ox /Ob2 /G6Fy /Gs /MD /DUSEKZ /DZOOM_TEST


# simple.exe : simple.obj voxlap5.obj v5.obj kplib.obj winmain.obj
# 	link simple voxlap5 v5 kplib winmain ddraw.lib dinput.lib ole32.lib dxguid.lib user32.lib gdi32.lib
# simple.obj : simple.c voxlap5.h sysmain.h
# 	cl /c /J /TP simple.c /Ox /Ob2 /G6Fy /Gs /MD /QIfist


# voxed.exe: delwinmain voxed.obj voxlap5.obj v5.obj kplib.obj winmain.obj voxed.c
# 	link voxed voxlap5 v5 kplib winmain ddraw.lib dinput.lib dxguid.lib user32.lib gdi32.lib comdlg32.lib
# delwinmain:
# 	del winmain.obj
# voxed.obj:   voxed.c voxlap5.h sysmain.h; cl /c /J /TP voxed.c     /Ox /Ob2 /G6Fy /Gs /MD
# voxlap5.obj: voxlap5.c voxlap5.h;         cl /c /J /TP voxlap5.c   /Ox /Ob2 /G6Fy /Gs /MD
# v5.obj:      v5.asm;                      ml /c /coff v5.asm
# kplib.obj:   kplib.c;                     cl /c /J /TP kplib.c     /Ox /Ob2 /G6Fy /Gs /MD
# winmain.obj: winmain.cpp;                 cl /c /J /TP winmain.cpp /Ox /Ob2 /G6Fy /Gs /MD /DNOSOUND


# kwalk.exe: delwinmain kwalk.obj voxlap5.obj v5.obj kplib.obj winmain.obj
# 	link kwalk voxlap5 v5 kplib winmain ddraw.lib dinput.lib ole32.lib dxguid.lib user32.lib gdi32.lib comdlg32.lib
# delwinmain:
# 	del winmain.obj
# kwalk.obj:   kwalk.c voxlap5.h sysmain.h; cl /c /J /TP kwalk.c     /Ox /Ob2 /G6Fy /Gs /MD
# voxlap5.obj: voxlap5.c voxlap5.h;         cl /c /J /TP voxlap5.c   /Ox /Ob2 /G6Fy /Gs /MD
# v5.obj:      v5.asm;                      ml /c /coff v5.asm
# kplib.obj:   kplib.c;                     cl /c /J /TP kplib.c     /Ox /Ob2 /G6Fy /Gs /MD
# winmain.obj: winmain.cpp;                 cl /c /J /TP winmain.cpp /Ox /Ob2 /G6Fy /Gs /MD /DNOSOUND