# Macros

# programs
CC													=cl
LINK												=link
AS													=ml

# SETUP DirectX 8.x environment for command line
# Fix directory name if incorrect!
# include											=\dxsdk\include;$(include)
LIB													=$(LIB);C:\Program Files\Microsoft DirectX SDK (June 2010)\Lib\x86

# adding sub-directories
SRC													=$(MAKEDIR)\src
INBIN												=$(MAKEDIR)\inbin
OUTBIN												=$(MAKEDIR)\outbin

# adding sub-directories to paths
INCLUDE												=$(INCLUDE);$(INBIN);$(OUTBIN);$(SRC)
LIB													=$(LIB);$(INBIN);$(OUTBIN);$(SRC)
PATH												=$(PATH);$(INBIN);$(OUTBIN);$(SRC)


default:		game.exe simple.exe voxed.exe kwalk.exe
# $(OUTBIN)\ $(**B)

# executable (.exe) (meta)targets
game.exe:		game.obj voxlap5.obj $(SRC)\v5.obj kplib.obj winmain.obj-1 $(SRC)\game.c;
													$(LINK) /out:$(OUTBIN)\$(@F) game.obj voxlap5 v5 kplib winmain ddraw.lib dinput.lib ole32.lib dxguid.lib user32.lib gdi32.lib 
simple.exe:		simple.obj voxlap5.obj $(SRC)\v5.obj kplib.obj winmain.obj-1
													$(LINK) /out:$(OUTBIN)\$(@F) simple voxlap5 v5 kplib winmain ddraw.lib dinput.lib ole32.lib dxguid.lib user32.lib gdi32.lib
voxed.exe:		voxed.obj voxlap5.obj $(SRC)\v5.obj kplib.obj winmain.obj-2 $(SRC)\voxed.c
													$(LINK) /out:$(OUTBIN)\$(@F) voxed voxlap5 v5 kplib winmain ddraw.lib dinput.lib dxguid.lib user32.lib gdi32.lib comdlg32.lib
kwalk.exe:		kwalk.obj voxlap5.obj $(SRC)\v5.obj kplib.obj winmain.obj-2
													$(LINK) /out:$(OUTBIN)\$(@F) kwalk voxlap5 v5 kplib winmain ddraw.lib dinput.lib ole32.lib dxguid.lib user32.lib gdi32.lib comdlg32.lib

# binary object (.obj) targets
game.obj:		$(SRC)\game.c $(SRC)\voxlap5.h $(SRC)\sysmain.h;	$(CC) /Fo$(OUTBIN)\$(@B) /c /J /TP $(SRC)\game.c      /Ox /Ob2 /G6Fy /Gs /MD /QIfist
simple.obj:		$(SRC)\simple.c $(SRC)\voxlap5.h $(SRC)\sysmain.h;	$(CC) /Fo$(OUTBIN)\$(@B) /c /J /TP $(SRC)\simple.c    /Ox /Ob2 /G6Fy /Gs /MD /QIfist
voxed.obj:		$(SRC)\voxed.c $(SRC)\voxlap5.h $(SRC)\sysmain.h;	$(CC) /Fo$(OUTBIN)\$(@B) /c /J /TP $(SRC)\voxed.c     /Ox /Ob2 /G6Fy /Gs /MD
kwalk.obj:		$(SRC)\kwalk.c $(SRC)\voxlap5.h $(SRC)\sysmain.h;	$(CC) /Fo$(OUTBIN)\$(@B) /c /J /TP $(SRC)\kwalk.c     /Ox /Ob2 /G6Fy /Gs /MD


voxlap5.obj:	$(SRC)\voxlap5.c $(SRC)\voxlap5.h;	$(CC) /Fo$(OUTBIN)\$(@B) /c /J /TP $(SRC)\voxlap5.c   /Ox /Ob2 /G6Fy /Gs /MD
v5.obj:			$(SRC)\v5.asm;						$(AS) /Fo$(OUTBIN)\$(@B) /c /coff $(SRC)\v5.asm
kplib.obj:		$(SRC)\kplib.c;						$(CC) /Fo$(OUTBIN)\$(@B) /c /J /TP $(SRC)\kplib.c     /Ox /Ob2 /G6Fy /Gs /MD
winmain.obj-1:	$(SRC)\winmain.cpp $(SRC)\sysmain.h    
#													del winmain.obj
													$(CC) /Fo$(OUTBIN)\$(@B) /c /J /TP $(SRC)\winmain.cpp /Ox /Ob2 /G6Fy /Gs /MD /DUSEKZ /DZOOM_TEST										
winmain.obj-2:	$(SRC)\winmain.cpp $(SRC)\sysmain.h
#													del winmain.obj
													$(CC) /Fo$(OUTBIN)\$(@B) /c /J /TP $(SRC)\winmain.cpp /Ox /Ob2 /G6Fy /Gs /MD /DNOSOUND
												
# Clearn Script
clean:
	cd $(OUTBIN)
	del *.exe *.obj