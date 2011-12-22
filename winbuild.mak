# Macros

# programs
CC						=cl
LINK					=link
AS						=ml

# SETUP DirectX 8.x environment for command line
# Fix directory name if incorrect!
# include				=/dxsdk/include;$(include)
LIB						=C:/Program Files/Microsoft DirectX SDK (June 2010)/Lib/x86;$(LIB)

# adding sub-directories
SRC						=$(MAKEDIR)/src
iBN						=$(MAKEDIR)/inbin
oBN						=$(MAKEDIR)/outbin

OSdep					=win
#OSdep					=sdl

# FOSS			
MinGW					=C:/Mingw
NASM					="C:/Program Files/nasm/nasm.exe"
PATH					=$(PATH);$(MinGW)/bin;

# adding sub-directories to paths
PATH					=$(iBN);$(PATH)
INCLUDE					=$(iBN);$(INCLUDE)
LIB						=$(iBN);$(LIB)


default:	$(oBN)/game.exe	$(oBN)/simple.exe $(oBN)/voxed.exe $(oBN)/kwalk.exe
# $(oBN)/ $(**B)

# executable (.exe) (meta)targets
$(oBN)/game.exe:		$(oBN)/game.obj   $(oBN)/voxlap5.obj $(oBN)/v5.obj $(oBN)/kplib.obj $(oBN)/$(OSdep)main1.obj;
	$(LINK) /out:$(@)	$(oBN)/game.obj   $(oBN)/voxlap5     $(oBN)/v5     $(oBN)/kplib     $(oBN)/$(OSdep)main1     ddraw.lib dinput.lib ole32.lib dxguid.lib user32.lib gdi32.lib
											
$(oBN)/simple.exe:		$(oBN)/simple.obj $(oBN)/voxlap5.obj $(oBN)/v5.obj $(oBN)/kplib.obj $(oBN)/$(OSdep)main1.obj;
	$(LINK) /out:$(@)	$(oBN)/simple     $(oBN)/voxlap5     $(oBN)/v5     $(oBN)/kplib     $(oBN)/$(OSdep)main1     ddraw.lib dinput.lib ole32.lib dxguid.lib user32.lib gdi32.lib
											
$(oBN)/voxed.exe:		$(oBN)/voxed.obj  $(oBN)/voxlap5.obj $(oBN)/v5.obj $(oBN)/kplib.obj $(oBN)/$(OSdep)main2.obj;
	$(LINK) /out:$(@)	$(oBN)/voxed      $(oBN)/voxlap5     $(oBN)/v5     $(oBN)/kplib     $(oBN)/$(OSdep)main2     ddraw.lib dinput.lib           dxguid.lib user32.lib gdi32.lib comdlg32.lib
											
$(oBN)/kwalk.exe:		$(oBN)/kwalk.obj  $(oBN)/voxlap5.obj $(oBN)/v5.obj $(oBN)/kplib.obj $(oBN)/$(OSdep)main2.obj;
	$(LINK) /out:$(@)	$(oBN)/kwalk      $(oBN)/voxlap5     $(oBN)/v5     $(oBN)/kplib     $(oBN)/$(OSdep)main2     ddraw.lib dinput.lib ole32.lib dxguid.lib user32.lib gdi32.lib comdlg32.lib
											

# binary object (.obj) targets
$(oBN)/game.obj:              $(SRC)/game.cpp   $(SRC)/voxlap5.h $(SRC)/sysmain.h;
	$(CC) /Fo$(@R) /c /J /TP  $(SRC)/game.cpp    /Ox /Ob2 /G6Fy /Gs /MD /QIfist
$(oBN)/simple.obj:            $(SRC)/simple.cpp $(SRC)/voxlap5.h $(SRC)/sysmain.h;
	$(CC) /Fo$(@R) /c /J /TP  $(SRC)/simple.cpp  /Ox /Ob2 /G6Fy /Gs /MD /QIfist
$(oBN)/voxed.obj:             $(SRC)/voxed.cpp  $(SRC)/voxlap5.h $(SRC)/sysmain.h;
	$(CC) /Fo$(@R) /c /J /TP  $(SRC)/voxed.cpp   /Ox /Ob2 /G6Fy /Gs /MD
$(oBN)/kwalk.obj:             $(SRC)/kwalk.cpp  $(SRC)/voxlap5.h $(SRC)/sysmain.h;
	$(CC) /Fo$(@R) /c /J /TP  $(SRC)/kwalk.cpp   /Ox /Ob2 /G6Fy /Gs /MD
                                     
                                     
$(oBN)/voxlap5.obj:           $(SRC)/voxlap5.cpp $(SRC)/voxlap5.h;
	$(CC) /Fo$(@R) /c /J /TP  $(SRC)/voxlap5.cpp /Ox /Ob2 /G6Fy /Gs /MD
$(oBN)/v5.obj:                $(SRC)/v5.masm;
	$(AS) /Fo$(@R) /c /coff   $(SRC)/v5.masm
#$(oBN)/v5.obj:               $(SRC)/v5.nasm;
#    $(NASM) -o $(@) -f win32 $(SRC)/v5.nasm
$(oBN)/kplib.obj:             $(SRC)/kplib.cpp;
	$(CC) /Fo$(@R) /c /J /TP  $(SRC)/kplib.cpp   /Ox /Ob2 /G6Fy /Gs /MD
$(oBN)/winmain1.obj:          $(SRC)/winmain.cpp $(SRC)/sysmain.h;
	$(CC) /Fo$(@R) /c /J /TP  $(SRC)/winmain.cpp /Ox /Ob2 /G6Fy /Gs /MD /DUSEKZ /DZOOM_TEST                                        
$(oBN)/winmain2.obj:          $(SRC)/winmain.cpp $(SRC)/sysmain.h;
	$(CC) /Fo$(@R) /c /J /TP  $(SRC)/winmain.cpp /Ox /Ob2 /G6Fy /Gs /MD /DNOSOUND

# Clearn Script
clean:
	cd $(oBN)
	del *.exe *.obj