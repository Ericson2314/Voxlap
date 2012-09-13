# sub-directory macros
locSRC                 =$(MAKEDIR)/source
locINC                 =$(MAKEDIR)/include
locLIB                 =$(MAKEDIR)/libraries
locBIN                 =$(MAKEDIR)/binaries

# adding local equivalents to environment variables
INCLUDE                =$(locINC);$(INCLUDE)
LIB                    =$(locLIB);$(LIB)
#PATH                  =$(locBIN);$(PATH)

# -----------------------------------
# Importing Environment Variables
!IFNDEF USEV5ASM
USEV5ASM               =1
!ENDIF

AsmName                =masm
AsmName                =$(_ASMNAME)

!IFDEF BUILD
!IF "$(BUILD)"=="Release"
CXX_MODE               =$(CXX_Release)
LNK_MODE               =$(LNK_Release)
!ELSE IF "$(BUILD)"=="Debug"
CXX_MODE               =$(CXX_Debug)
LNK_MODE               =$(LNK_Debug)
!ENDIF
!ELSE
CXX_MODE               =$(CXX_Debug)
LNK_MODE               =$(LNK_Debug)
!ENDIF

# END Importing Environment Variables
# -----------------------------------

# -----------------------------------
# Micrososft Visual C Macros

CXX                    =cl   #for Micrsoft Compiler
LNK                    =link #for Microsfoft Linker

# Flags
CXXFLAGS               =/Fo$(@R) /c /J $(CXX_MODE) $(GFXCFLAGS) $(Random_Macros) # for Micrsoft Compiler(cl)
CXX_Debug              =/MLd /ZI /Fdbinaries\ /GZ /RTCsuc /Od
CXX_Release            =/Ox

LNKFLAGS               =/out:$(@) $(LNK_MODE) # for Microsfoft Linker (link)
LNK_Debug              =/DEBUG
LNK_Release            =

# END Micrososft Visual C Macros
# -----------------------------------

# -----------------------------------
# Assembler Macros
# Japheth (Open) Watcomm Assembler (jwasm)
!IF "$(AsmName)"=="jwasm"
AS                     =jwasm
AFLAGS                 =-Fo$(@R) -c -coff -8 -DWIN32
!ENDIF

# Netwide Assembler (nasm)
!IF "$(AsmName)"=="nasm"
AS                     =nasm
AFLAGS                 =-o $(@) -f win32 -DWIN32 --prefix _
!ENDIF

# Micrsoft Macro Assembler (masm)
!IF "$(AsmName)"=="masm"
AS                     =ml
AFLAGS                 =/Fo$(@R) /c /coff
!ENDIF
# END Assembler Macros
# -----------------------------------

# -----------------------------------
# Toggle Random Macros
!IF "$(USEV5ASM)"=="1"
if_USEV5ASM            =$(locBIN)/v5$(OBJSuf)
!ENDIF
Random_Macros          =/DUSEV5ASM=$(USEV5ASM)
# END Toggle Random Macros
# -----------------------------------

gameLIBs	           =ddraw.lib dinput.lib dxguid.lib $(LNKlibPre)user32.lib $(LNKlibPre)gdi32.lib $(LNKlibPre)ole32.lib
simpleLIBs             =ddraw.lib dinput.lib dxguid.lib $(LNKlibPre)user32.lib $(LNKlibPre)gdi32.lib $(LNKlibPre)ole32.lib
voxedLIBs	           =ddraw.lib dinput.lib dxguid.lib $(LNKlibPre)user32.lib $(LNKlibPre)gdi32.lib $(LNKlibPre)comdlg32.lib
kwalkLIBs	           =ddraw.lib dinput.lib dxguid.lib $(LNKlibPre)user32.lib $(LNKlibPre)gdi32.lib $(LNKlibPre)ole32.lib $(LNKlibPre)comdlg32.lib

OBJSuf                 =.obj
EXESuf                 =.exe

# sub-directory macros
# locSRC                      =$(MAKEDIR)/source
# locINC                      =$(MAKEDIR)/include
# locLIB                      =$(MAKEDIR)/libraries
# locBIN                      =$(MAKEDIR)/binaries

Phony:                        default
default:                      $(locBIN)/game$(EXESuf) $(locBIN)/simple$(EXESuf) $(locBIN)/voxed$(EXESuf) $(locBIN)/kwalk$(EXESuf)

# executable ($(EXESuf)) (meta)targets
game:                         $(locBIN)/game$(EXESuf)
$(locBIN)/game$(EXESuf):      $(locBIN)/game$(OBJSuf)   $(locBIN)/voxlap5$(OBJSuf) $(if_USEV5ASM) $(locBIN)/kplib$(OBJSuf) $(locBIN)/winmain1$(OBJSuf)
	$(LNK) $(LNKFLAGS)        $(locBIN)/game$(OBJSuf)   $(locBIN)/voxlap5$(OBJSuf) $(if_USEV5ASM) $(locBIN)/kplib$(OBJSuf) $(locBIN)/winmain1$(OBJSuf) $(gameLIBs)

simple:                       $(locBIN)/simple$(EXESuf)
$(locBIN)/simple$(EXESuf):    $(locBIN)/simple$(OBJSuf) $(locBIN)/voxlap5$(OBJSuf) $(if_USEV5ASM) $(locBIN)/kplib$(OBJSuf) $(locBIN)/winmain1$(OBJSuf)
	$(LNK) $(LNKFLAGS)        $(locBIN)/simple$(OBJSuf) $(locBIN)/voxlap5$(OBJSuf) $(if_USEV5ASM) $(locBIN)/kplib$(OBJSuf) $(locBIN)/winmain1$(OBJSuf) $(simpleLIBs)

voxed:                        $(locBIN)/voxed$(EXESuf)
$(locBIN)/voxed$(EXESuf):     $(locBIN)/voxed$(OBJSuf)  $(locBIN)/voxlap5$(OBJSuf) $(if_USEV5ASM) $(locBIN)/kplib$(OBJSuf) $(locBIN)/winmain2$(OBJSuf)
	$(LNK) $(LNKFLAGS)        $(locBIN)/voxed$(OBJSuf)  $(locBIN)/voxlap5$(OBJSuf) $(if_USEV5ASM) $(locBIN)/kplib$(OBJSuf) $(locBIN)/winmain2$(OBJSuf) $(voxedLIBs)
	
kwalk:                        $(locBIN)/kwalk$(EXESuf)
$(locBIN)/kwalk$(EXESuf):     $(locBIN)/kwalk$(OBJSuf)  $(locBIN)/voxlap5$(OBJSuf) $(if_USEV5ASM) $(locBIN)/kplib$(OBJSuf) $(locBIN)/winmain2$(OBJSuf)
	$(LNK) $(LNKFLAGS)        $(locBIN)/kwalk$(OBJSuf)  $(locBIN)/voxlap5$(OBJSuf) $(if_USEV5ASM) $(locBIN)/kplib$(OBJSuf) $(locBIN)/winmain2$(OBJSuf) $(kwalkLIBs)

slab6:                        $(locBIN)/slab6$(EXESuf)
$(locBIN)/slab6$(EXESuf):     $(locBIN)/slab6$(OBJSuf)  $(locBIN)/s6$(OBJSuf) $(locBIN)/slab6.res                          $(locBIN)/winmain2$(OBJSuf)
	$(LNK) $(LNKFLAGS)        $(locBIN)/slab6$(OBJSuf)  $(locBIN)/s6$(OBJSuf) $(locBIN)/slab6.res                          $(locBIN)/winmain2$(OBJSuf) $(kwalkLIBs)

# binary object ($(OBJSuf)) targets

# Primary Objects
game_o:                            $(locSRC)/game.cpp
$(locBIN)/game$(OBJSuf):           $(locSRC)/game.cpp   $(locINC)/voxlap5.h $(locINC)/sysmain.h
	$(CXX) $(CXXFLAGS)             $(locSRC)/game.cpp
#   used to use /QIfist             

simple_o:                          $(locSRC)/simple.cpp
$(locBIN)/simple$(OBJSuf):         $(locSRC)/simple.cpp $(locINC)/voxlap5.h $(locINC)/sysmain.h
	$(CXX) $(CXXFLAGS)             $(locSRC)/simple.cpp
#   used to use /QIfist             

voxed_o:                           $(locSRC)/voxed.cpp
$(locBIN)/voxed$(OBJSuf):          $(locSRC)/voxed.cpp  $(locINC)/voxlap5.h $(locINC)/sysmain.h
	$(CXX) $(CXXFLAGS)             $(locSRC)/voxed.cpp

kwalk_o:                           $(locSRC)/kwalk.cpp
$(locBIN)/kwalk$(OBJSuf):          $(locSRC)/kwalk.cpp  $(locINC)/voxlap5.h $(locINC)/sysmain.h
	$(CXX) $(CXXFLAGS)             $(locSRC)/kwalk.cpp

slab6_0:                           $(locSRC)/slab6.cpp
$(locBIN)/slab6$(OBJSuf):          $(locSRC)/slab6.cpp  $(locINC)/sysmain.h
	$(CXX) $(CXXFLAGS)             $(locSRC)/slab6.cpp

$(locBIN)/slab6.res:               $(locSRC)/slab6.rc $(locSRC)/slab6.ico
	rc -r /fo $(locBIN)/slab6.res  $(locSRC)/slab6.rc

# Secondary Objects                 
voxlap:                            $(locSRC)/voxlap5.cpp 
$(locBIN)/voxlap5$(OBJSuf):        $(locSRC)/voxlap5.cpp $(locINC)/voxlap5.h
	$(CXX) $(CXXFLAGS)             $(locSRC)/voxlap5.cpp

v5:                                $(locBIN)/v5$(OBJSuf)
$(locBIN)/v5$(OBJSuf):             $(locSRC)/v5.$(AsmName)
	$(AS)  $(AFLAGS)               $(locSRC)/v5.$(AsmName)

s6:
$(locBIN)/s6$(OBJSuf):             $(locSRC)/s6.asm
	$(AS)  $(AFLAGS)               $(locSRC)/s6.asm

kplib:                             $(locBIN)/kplib$(OBJSuf)
$(locBIN)/kplib$(OBJSuf):          $(locSRC)/kplib.c
	$(CXX) $(CXXFLAGS) /TP         $(locSRC)/kplib.c

main1:                             $(locBIN)/winmain1$(OBJSuf)
$(locBIN)/winmain1$(OBJSuf): $(locSRC)/winmain.cpp $(locINC)/sysmain.h
	$(CXX) $(CXXFLAGS) /TP         $(locSRC)/winmain.cpp /DUSEKZ /DZOOM_TEST

main2:                             $(locBIN)/winmain2$(OBJSuf)
$(locBIN)/winmain2$(OBJSuf): $(locSRC)/winmain.cpp $(locINC)/sysmain.h
	$(CXX) $(CXXFLAGS) /TP         $(locSRC)/winmain.cpp /DNOSOUND

# Clearn Script
cleanall: clean
	cd "$(locBIN)"
	del /f "*"
clean:
	cd "$(locBIN)"
	del /f "*$(OBJSuf)"
	del /f "*$(EXESuf)"
