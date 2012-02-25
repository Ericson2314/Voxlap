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


# -----------------------------------
# GNU Compiler Collection Macros
!IFDEF __GNUC__
CXX                    =gcc  #for GNU C++ Compiler
LNK                    =ld   #for GNU linker

# Flags
CXXFLAGS               =-o $(@) -c -funsigned-char -mwindows $(GFXCFLAGS) #for GNU C++ Compiler (gcc)
CMacroPre              =-D "" #for GNU C++ Compiler (gcc)

ldFLAGS                =-o $(@) #for GNU linker (ld)
LNKlibPre              =-l "" #for GNU linker (ld)

gameLIBs	           =$(GFXOBJ) $(LNKlibPre)user32$(LNKlibSuf) $(LNKlibPre)gdi32$(LNKlibSuf) $(LNKlibPre)ole32$(LNKlibSuf)
simpleLIBs             =$(GFXOBJ) $(LNKlibPre)user32$(LNKlibSuf) $(LNKlibPre)gdi32$(LNKlibSuf) $(LNKlibPre)ole32$(LNKlibSuf)
voxedLIBs	           =$(GFXOBJ) $(LNKlibPre)user32$(LNKlibSuf) $(LNKlibPre)gdi32$(LNKlibSuf)                               $(LNKlibPre)comdlg32$(LNKlibSuf)
kwalkLIBs	           =$(GFXOBJ) $(LNKlibPre)user32$(LNKlibSuf) $(LNKlibPre)gdi32$(LNKlibSuf) $(LNKlibPre)ole32$(LNKlibSuf)

!ENDIF
# END GNU Compiler Collection Macros
# -----------------------------------

# -----------------------------------
# Micrososft Visual C Macros
!IFDEF __MSVC__
CXX                    =cl   #for Micrsoft Compiler
LNK                    =link #for Microsfoft Linker

# Flags
CXXFLAGS               =/Fo$(@R) /c /J $(GFXCFLAGS) # for Micrsoft Compiler(cl)
CMacroPre              =/D # for Micrsoft Compiler(cl)

LNKFLAGS               =/out:$(@) # for Microsfoft Linker (link)
LNKlibSuf              =.lib

gameLIBs	           =$(GFXOBJ) $(LNKlibPre)user32$(LNKlibSuf) $(LNKlibPre)gdi32$(LNKlibSuf) $(LNKlibPre)ole32$(LNKlibSuf)
simpleLIBs             =$(GFXOBJ) $(LNKlibPre)user32$(LNKlibSuf) $(LNKlibPre)gdi32$(LNKlibSuf) $(LNKlibPre)ole32$(LNKlibSuf)
voxedLIBs	           =$(GFXOBJ) $(LNKlibPre)user32$(LNKlibSuf) $(LNKlibPre)gdi32$(LNKlibSuf)                               $(LNKlibPre)comdlg32$(LNKlibSuf)
kwalkLIBs	           =$(GFXOBJ) $(LNKlibPre)user32$(LNKlibSuf) $(LNKlibPre)gdi32$(LNKlibSuf) $(LNKlibPre)ole32$(LNKlibSuf)

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

OBJSuf                 =$(win_OBJSuf)
EXESuf                 =$(win_EXESuf)
rm                     =del

# Call Common
!include common.mak
