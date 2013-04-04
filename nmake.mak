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

!IFDEF _ASMNAME
AsmName                =$(_ASMNAME)
!ELSE
AsmName                =nasm
!ENDIF

!IFNDEF BUILD
BUILD                  =Debug
!ENDIF

!IF "$(BUILD)"=="Release"
CXX_MODE               =$(CXX_Release)
LNK_MODE               =$(LNK_Release)
!ELSE IF "$(BUILD)"=="Debug"
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
CXXFLAGS               =/Fo$(@R) /fp:fast /arch:SSE2 /c /J $(CXX_MODE) $(GFX_CFLAGS) $(Random_Macros) /EHsc /I $(locINC) # for Micrsoft Compiler(cl)
CXX_Debug              =$(GFX_CXX_Debug) /ZI /Fdbinaries\ /Gz /RTCsuc /Od
CXX_Release            =$(GFX_CXX_Release) /Ox

LNKFLAGS               =/out:$(@) /SUBSYSTEM:WINDOWS $(EXCLUDE_DEF_LIB) $(LNK_MODE)# for Microsfoft Linker (link)
LNK_Debug              =/DEBUG
LNK_Release            =
EXCLUDE_DEF_LIB        =/NODEFAULTLIB:msvcrt.lib

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
# Graphics Backend

GFX                    =win
GFX                    =$(_GFX)

!IF "$(GFX)"=="win"
#C_TYPE                 =/TP
GFX_LIBS               =ddraw.lib dinput.lib dxguid.lib

GFX_CXX_Debug          =/MLd
GFX_CXX_Release        =/ML

!ELSE IF "$(GFX)"=="sdl"
#GFX_CFLAGS             =-D SYSMAIN_C -D KPLIB_C
#C_TYPE                 =/TC
GFX_LIBS               =SDL.lib SDLmain.lib


GFX_CXX_Debug          =/MDd
GFX_CXX_Release        =/MD
!ENDIF

# END Importing Environment Variables
# -----------------------------------

# -----------------------------------
# Toggle Random Macros
!IF "$(USEV5ASM)"=="1"
if_USEV5ASM            =$(locBIN)/v5$(OBJSuf)
!ENDIF
Random_Macros          =/DUSEV5ASM=$(USEV5ASM)
# END Toggle Random Macros
# -----------------------------------

simpleLIBs             =$(GFX_LIBS) user32.lib gdi32.lib ole32.lib
gameLIBs               =$(GFX_LIBS) user32.lib gdi32.lib ole32.lib
voxedLIBs              =$(GFX_LIBS) user32.lib gdi32.lib           comdlg32.lib
kwalkLIBs              =$(GFX_LIBS) user32.lib gdi32.lib ole32.lib comdlg32.lib

OBJSuf                 =.obj
EXESuf                 =.exe

Phony:                        all
all:                          voxlap slab6
voxlap:                       $(locBIN)/simple$(EXESuf) $(locBIN)/game$(EXESuf) $(locBIN)/voxed$(EXESuf) $(locBIN)/kwalk$(EXESuf)

# executable ($(EXESuf)) (meta)targets
simple:                       $(locBIN)/simple$(EXESuf)
$(locBIN)/simple$(EXESuf):    $(locBIN)/simple$(OBJSuf) $(locBIN)/voxlap5$(OBJSuf) $(if_USEV5ASM) $(locBIN)/kplib$(OBJSuf) $(locBIN)/$(GFX)main1$(OBJSuf)
	$(LNK) $(LNKFLAGS)        $(locBIN)/simple$(OBJSuf) $(locBIN)/voxlap5$(OBJSuf) $(if_USEV5ASM) $(locBIN)/kplib$(OBJSuf) $(locBIN)/$(GFX)main1$(OBJSuf) $(simpleLIBs)

game:                         $(locBIN)/game$(EXESuf)
$(locBIN)/game$(EXESuf):      $(locBIN)/game$(OBJSuf)   $(locBIN)/voxlap5$(OBJSuf) $(if_USEV5ASM) $(locBIN)/kplib$(OBJSuf) $(locBIN)/$(GFX)main1$(OBJSuf)
	$(LNK) $(LNKFLAGS)        $(locBIN)/game$(OBJSuf)   $(locBIN)/voxlap5$(OBJSuf) $(if_USEV5ASM) $(locBIN)/kplib$(OBJSuf) $(locBIN)/$(GFX)main1$(OBJSuf) $(gameLIBs)

voxed:                        $(locBIN)/voxed$(EXESuf)
$(locBIN)/voxed$(EXESuf):     $(locBIN)/voxed$(OBJSuf)  $(locBIN)/voxlap5$(OBJSuf) $(if_USEV5ASM) $(locBIN)/kplib$(OBJSuf) $(locBIN)/$(GFX)main2$(OBJSuf)
	$(LNK) $(LNKFLAGS)        $(locBIN)/voxed$(OBJSuf)  $(locBIN)/voxlap5$(OBJSuf) $(if_USEV5ASM) $(locBIN)/kplib$(OBJSuf) $(locBIN)/$(GFX)main2$(OBJSuf) $(voxedLIBs)

kwalk:                        $(locBIN)/kwalk$(EXESuf)
$(locBIN)/kwalk$(EXESuf):     $(locBIN)/kwalk$(OBJSuf)  $(locBIN)/voxlap5$(OBJSuf) $(if_USEV5ASM) $(locBIN)/kplib$(OBJSuf) $(locBIN)/$(GFX)main2$(OBJSuf)
	$(LNK) $(LNKFLAGS)        $(locBIN)/kwalk$(OBJSuf)  $(locBIN)/voxlap5$(OBJSuf) $(if_USEV5ASM) $(locBIN)/kplib$(OBJSuf) $(locBIN)/$(GFX)main2$(OBJSuf) $(kwalkLIBs)

slab6:                        $(locBIN)/slab6$(EXESuf)
$(locBIN)/slab6$(EXESuf):     $(locBIN)/slab6$(OBJSuf)  $(locBIN)/s6$(OBJSuf) $(locBIN)/slab6.res                          $(locBIN)/$(GFX)main2$(OBJSuf)
	$(LNK) $(LNKFLAGS)        $(locBIN)/slab6$(OBJSuf)  $(locBIN)/s6$(OBJSuf) $(locBIN)/slab6.res                          $(locBIN)/$(GFX)main2$(OBJSuf) $(kwalkLIBs)

# binary object ($(OBJSuf)) targets

# Primary Object
simple_o:                          $(locSRC)/simple.cpp
$(locBIN)/simple$(OBJSuf):         $(locSRC)/simple.cpp $(locINC)/voxlap5.h $(locINC)/sysmain.h
	$(CXX) $(CXXFLAGS)             $(locSRC)/simple.cpp
#   used to use /QIfist

game_o:                            $(locSRC)/game.cpp
$(locBIN)/game$(OBJSuf):           $(locSRC)/game.cpp   $(locINC)/voxlap5.h $(locINC)/sysmain.h
	$(CXX) $(CXXFLAGS)             $(locSRC)/game.cpp
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

lab6_res:
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
$(locBIN)/s6$(OBJSuf):             $(locSRC)/s6.$(AsmName)
	$(AS)  $(AFLAGS)               $(locSRC)/s6.$(AsmName)

kplib:                             $(locBIN)/kplib$(OBJSuf)
$(locBIN)/kplib$(OBJSuf):          $(locSRC)/kplib.cpp
	$(CXX) $(CXXFLAGS)             $(locSRC)/kplib.cpp

main1:                             $(locBIN)/$(GFX)main1$(OBJSuf)
$(locBIN)/$(GFX)main1$(OBJSuf):    $(locSRC)/$(GFX)main.cpp $(locINC)/sysmain.h
	$(CXX) $(CXXFLAGS)             $(locSRC)/$(GFX)main.cpp /DUSEKZ /DZOOM_TEST

main2:                             $(locBIN)/$(GFX)main2$(OBJSuf)
$(locBIN)/$(GFX)main2$(OBJSuf):    $(locSRC)/$(GFX)main.cpp $(locINC)/sysmain.h
	$(CXX) $(CXXFLAGS)             $(locSRC)/$(GFX)main.cpp /DNOSOUND

# Clearn Script
cleanall: clean
	cd "$(locBIN)"
	del /f "*"
clean:
	cd "$(locBIN)"
	del /f "*$(OBJSuf)"
	del /f "*$(EXESuf)"
