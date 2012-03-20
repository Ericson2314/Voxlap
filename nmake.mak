# sub-directory macros
locSRC                 =$(MAKEDIR)/source
locINC                 =$(MAKEDIR)/include
locLIB                 =$(MAKEDIR)/libraries
locBIN                 =$(MAKEDIR)/binaries

# adding local equivalents to environment variables
INCLUDE                =$(locINC);$(INCLUDE)
!IFDEF __MSVC__
LIB                    =$(locLIB);$(LIB)
!ENDIF
#PATH                   =$(locBIN);$(PATH)

# "1" to use v5.$(AsmName), "0" to not use
!IFNDEF USEV5ASM
USEV5ASM               =1
!ENDIF

# -----------------------------------
# GNU Compiler Collection Macros
!IFDEF __GNUC__
CXX                    =gcc  #for GNU C++ Compiler
LNK                    =ld   #for GNU linker

# Flags
CXXFLAGS               =-o $(@) -c -funsigned-char -mwindows $(GFXCFLAGS) $(Random_Macros) #for GNU C++ Compiler (gcc)
CMacroPre              =-D # #for GNU C++ Compiler (gcc)

LNKFLAGS                =-o $(@) #for GNU linker (ld)
LNKlibPre              =-l # #for GNU linker (ld)

!ENDIF
# END GNU Compiler Collection Macros
# -----------------------------------

# -----------------------------------
# Micrososft Visual C Macros
!IFDEF __MSVC__
CXX                    =cl   #for Micrsoft Compiler
LNK                    =link #for Microsfoft Linker

# Flags
CXXFLAGS               =/Fo$(@R) /c /J $(GFXCFLAGS) $(Random_Macros) # for Micrsoft Compiler(cl)
CMacroPre              =/D # for Micrsoft Compiler(cl)

LNKFLAGS               =/out:$(@) # for Microsfoft Linker (link)
LNKlibSuf              =.lib

!ENDIF
# END Micrososft Visual C Macros
# -----------------------------------

# -----------------------------------
# Assembler Macros
# Micrsoft Macro Assembler (masm)
!IF "$(AsmName)"=="masm"
AS                     =ml
AFLAGS                 =/Fo$(@R) /c /coff
!ENDIF

# Netwide Assembler (nasm)
!IF "$(AsmName)"=="nasm"
AS                     =nasm
AFLAGS                 =-o $(@) -f win32
!ENDIF
# END Assembler Macros
# -----------------------------------

# -----------------------------------
# Toggle Random Macros
!IFDEF  "$(USEV5ASM)"=="1"
if_USEV5ASM            =$(locBIN)/v5.$(OBJSuf)
!ENDIF
Random_Macros          =$(CMacroPre)USEV5ASM=$(USEV5ASM)
# END Toggle Random Macros
# -----------------------------------

gameLIBs	           =$(GFXOBJ) $(LNKlibPre)user32$(LNKlibSuf) $(LNKlibPre)gdi32$(LNKlibSuf) $(LNKlibPre)ole32$(LNKlibSuf)
simpleLIBs             =$(GFXOBJ) $(LNKlibPre)user32$(LNKlibSuf) $(LNKlibPre)gdi32$(LNKlibSuf) $(LNKlibPre)ole32$(LNKlibSuf)
voxedLIBs	           =$(GFXOBJ) $(LNKlibPre)user32$(LNKlibSuf) $(LNKlibPre)gdi32$(LNKlibSuf) $(LNKlibPre)comdlg32$(LNKlibSuf)
kwalkLIBs	           =$(GFXOBJ) $(LNKlibPre)user32$(LNKlibSuf) $(LNKlibPre)gdi32$(LNKlibSuf) $(LNKlibPre)ole32$(LNKlibSuf) $(LNKlibPre)comdlg32$(LNKlibSuf)



OBJSuf                 =obj
EXESuf                 =exe
rm                     =del /f

# Call Common
!include common.mak
