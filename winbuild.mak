# Macros

# programs
CC													=cl
LINK										=link
AS											=ml

# SETUP DirectX 8.x environment for command line
# Fix directory name if incorrect!
# include									=/dxsdk/include;$(include)
LIB											=$(LIB);C:/Program Files/Microsoft DirectX SDK (June 2010)/Lib/x86

# adding sub-directories
SRC											=$(MAKEDIR)/src
INBIN										=$(MAKEDIR)/inbin
OUTBIN										=$(MAKEDIR)/outbin

# adding sub-directories to paths
INCLUDE										=$(INCLUDE);$(INBIN);$(OUTBIN);$(SRC)
LIB											=$(LIB);$(INBIN);$(OUTBIN);$(SRC)
PATH										=$(PATH);$(INBIN);$(OUTBIN);$(SRC)


default:		game.exe simple.exe voxed.exe kwalk.exe
# $(OUTBIN)/ $(**B)

# executable (.exe) (meta)targets
game.exe:		game.obj    voxlap5.obj v5.obj kplib.obj winmain1.obj $(SRC)/game.cpp;
											$(LINK) /out:$(OUTBIN)/$(@F) game.obj voxlap5 v5 kplib winmain1 ddraw.lib dinput.lib ole32.lib dxguid.lib user32.lib gdi32.lib 
simple.exe:		simple.obj  voxlap5.obj v5.obj kplib.obj winmain1.obj;
											$(LINK) /out:$(OUTBIN)/$(@F) simple   voxlap5 v5 kplib winmain1 ddraw.lib dinput.lib ole32.lib dxguid.lib user32.lib gdi32.lib
voxed.exe:		voxed.obj   voxlap5.obj v5.obj kplib.obj winmain2.obj $(SRC)/voxed.cpp;
											$(LINK) /out:$(OUTBIN)/$(@F) voxed    voxlap5 v5 kplib winmain2 ddraw.lib dinput.lib           dxguid.lib user32.lib gdi32.lib comdlg32.lib
kwalk.exe:		kwalk.obj   voxlap5.obj v5.obj kplib.obj winmain2.obj;
											$(LINK) /out:$(OUTBIN)/$(@F) kwalk    voxlap5 v5 kplib winmain2 ddraw.lib dinput.lib ole32.lib dxguid.lib user32.lib gdi32.lib comdlg32.lib

# binary object (.obj) targets
game.obj:		$(SRC)/game.cpp $(SRC)/voxlap5.h $(SRC)/sysmain.h;
											$(CC) /Fo$(OUTBIN)/$(@B) /c /J /TP $(SRC)/game.cpp      /Ox /Ob2 /G6Fy /Gs /MD /QIfist
simple.obj:		$(SRC)/simple.cpp $(SRC)/voxlap5.h $(SRC)/sysmain.h;
											$(CC) /Fo$(OUTBIN)/$(@B) /c /J /TP $(SRC)/simple.cpp    /Ox /Ob2 /G6Fy /Gs /MD /QIfist
voxed.obj:		$(SRC)/voxed.cpp $(SRC)/voxlap5.h $(SRC)/sysmain.h;
											$(CC) /Fo$(OUTBIN)/$(@B) /c /J /TP $(SRC)/voxed.cpp     /Ox /Ob2 /G6Fy /Gs /MD
kwalk.obj:		$(SRC)/kwalk.cpp $(SRC)/voxlap5.h $(SRC)/sysmain.h;
											$(CC) /Fo$(OUTBIN)/$(@B) /c /J /TP $(SRC)/kwalk.cpp     /Ox /Ob2 /G6Fy /Gs /MD


voxlap5.obj:	$(SRC)/voxlap5.cpp $(SRC)/voxlap5.h;
											$(CC) /Fo$(OUTBIN)/$(@B) /c /J /TP $(SRC)/voxlap5.cpp   /Ox /Ob2 /G6Fy /Gs /MD
#v5.obj:			$(SRC)/v5.masm;
#											$(AS) /Fo$(OUTBIN)/$(@B) /c /coff $(SRC)/v5.masm
v5.obj:			$(SRC)/v5.nasm;
											"C:/Program Files/nasm/nasm.exe" -o $(OUTBIN)/$(@B).obj $(SRC)/v5.nasm -f win32
kplib.obj:		$(SRC)/kplib.cpp;
											$(CC) /Fo$(OUTBIN)/$(@B) /c /J /TP $(SRC)/kplib.cpp     /Ox /Ob2 /G6Fy /Gs /MD
winmain1.obj:	$(SRC)/winmain.cpp $(SRC)/sysmain.h;
											$(CC) /Fo$(OUTBIN)/$(@B) /c /J /TP $(SRC)/winmain.cpp /Ox /Ob2 /G6Fy /Gs /MD /DUSEKZ /DZOOM_TEST										
winmain2.obj:	$(SRC)/winmain.cpp $(SRC)/sysmain.h;
											$(CC) /Fo$(OUTBIN)/$(@B) /c /J /TP $(SRC)/winmain.cpp /Ox /Ob2 /G6Fy /Gs /MD /DNOSOUND

# Clearn Script
clean:
	cd $(OUTBIN)
	del *.exe *.obj