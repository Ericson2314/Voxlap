# Macros

# programs
CC												=cl
LINK											=link
AS												=ml

# SETUP DirectX 8.x environment for command line
# Fix directory name if incorrect!
# include										=\dxsdk\include;$(include)
LIB												=$(LIB);C:\Program Files\Microsoft DirectX SDK (June 2010)\Lib\x86

# adding ./src
SOURCE_DIR										=.\src

# adding ./inbin to paths
INCLUDE											=$(INCLUDE);$(MAKEDIR)\inbin;$(MAKEDIR)\outbin;$(MAKEDIR)\$(SOURCE_DIR)
LIB												=$(LIB);$(MAKEDIR)\inbin;$(MAKEDIR)\outbin;$(MAKEDIR)\$(SOURCE_DIR)
PATH											=$(PATH);$(MAKEDIR)\inbin;$(MAKEDIR)\outbin;$(MAKEDIR)\$(SOURCE_DIR)

# adding ./bin
OUTPUT_DIR										=.\outbin


default: game.exe simple.exe voxed.exe kwalk.exe
# $(OUTPUT_DIR)\ $(**B)
# executable (.exe) (meta)targets
game.exe: game.obj voxlap5.obj $(SOURCE_DIR)\v5.obj kplib.obj winmain.obj(1) $(SOURCE_DIR)\game.c;
												$(LINK) /out:$(OUTPUT_DIR)\$(@F) game voxlap5 v5 kplib winmain ddraw.lib dinput.lib ole32.lib dxguid.lib user32.lib gdi32.lib 
simple.exe: simple.obj voxlap5.obj $(SOURCE_DIR)\v5.obj kplib.obj winmain.obj(1)
												$(LINK) /out:$(OUTPUT_DIR)\$(@F) simple voxlap5 v5 kplib winmain ddraw.lib dinput.lib ole32.lib dxguid.lib user32.lib gdi32.lib
voxed.exe: voxed.obj voxlap5.obj $(SOURCE_DIR)\v5.obj kplib.obj winmain.obj(2) $(SOURCE_DIR)\voxed.c
												$(LINK) /out:$(OUTPUT_DIR)\$(@F) voxed voxlap5 v5 kplib winmain ddraw.lib dinput.lib dxguid.lib user32.lib gdi32.lib comdlg32.lib
kwalk.exe: kwalk.obj voxlap5.obj $(SOURCE_DIR)\v5.obj kplib.obj winmain.obj(2)
												$(LINK) /out:$(OUTPUT_DIR)\$(@F) kwalk voxlap5 v5 kplib winmain ddraw.lib dinput.lib ole32.lib dxguid.lib user32.lib gdi32.lib comdlg32.lib

# binary object (.obj) targets
game.obj:		$(SOURCE_DIR)\game.c $(SOURCE_DIR)\voxlap5.h $(SOURCE_DIR)\sysmain.h;		$(CC) /Fo$(OUTPUT_DIR)\$(@B) /c /J /TP $(SOURCE_DIR)\game.c      /Ox /Ob2 /G6Fy /Gs /MD /QIfist
simple.obj:		$(SOURCE_DIR)\simple.c $(SOURCE_DIR)\voxlap5.h $(SOURCE_DIR)\sysmain.h;	$(CC) /Fo$(OUTPUT_DIR)\$(@B) /c /J /TP $(SOURCE_DIR)\simple.c    /Ox /Ob2 /G6Fy /Gs /MD /QIfist
voxed.obj:		$(SOURCE_DIR)\voxed.c $(SOURCE_DIR)\voxlap5.h $(SOURCE_DIR)\sysmain.h;	$(CC) /Fo$(OUTPUT_DIR)\$(@B) /c /J /TP $(SOURCE_DIR)\voxed.c     /Ox /Ob2 /G6Fy /Gs /MD
kwalk.obj:		$(SOURCE_DIR)\kwalk.c $(SOURCE_DIR)\voxlap5.h $(SOURCE_DIR)\sysmain.h;	$(CC) /Fo$(OUTPUT_DIR)\$(@B) /c /J /TP $(SOURCE_DIR)\kwalk.c     /Ox /Ob2 /G6Fy /Gs /MD


voxlap5.obj:	$(SOURCE_DIR)\voxlap5.c $(SOURCE_DIR)\voxlap5.h;			$(CC) /Fo$(OUTPUT_DIR)\$(@B) /c /J /TP $(SOURCE_DIR)\voxlap5.c   /Ox /Ob2 /G6Fy /Gs /MD
v5.obj:			$(SOURCE_DIR)\v5.asm;							$(AS) /Fo$(OUTPUT_DIR)\$(@B) /c /coff $(SOURCE_DIR)\v5.asm
kplib.obj:		$(SOURCE_DIR)\kplib.c;						$(CC) /Fo$(OUTPUT_DIR)\$(@B) /c /J /TP $(SOURCE_DIR)\kplib.c     /Ox /Ob2 /G6Fy /Gs /MD
winmain.obj(1):	$(SOURCE_DIR)\winmain.cpp $(SOURCE_DIR)\sysmain.h    
												del winmain.obj
												$(CC) /Fo$(OUTPUT_DIR)\$(@B) /c /J /TP $(SOURCE_DIR)\winmain.cpp /Ox /Ob2 /G6Fy /Gs /MD /DUSEKZ /DZOOM_TEST										
winmain.obj(2):	$(SOURCE_DIR)\winmain.cpp $(SOURCE_DIR)\sysmain.h
												del winmain.obj
												$(CC) /Fo$(OUTPUT_DIR)\$(@B) /c /J /TP $(SOURCE_DIR)\winmain.cpp /Ox /Ob2 /G6Fy /Gs /MD /DNOSOUND
												
# Clearn Script
clean:
	cd $(OUTPUT_DIR)
	del *.exe *.obj