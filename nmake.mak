# sub-directory macros
SRC                    =$(MAKEDIR)/src
iBN                    =$(MAKEDIR)/inbin
oBN                    =$(MAKEDIR)/outbin

# adding inbin directory to paths
!IFDEF __MSVC__
PATH                   =$(iBN);$(PATH)
INCLUDE                =$(iBN);$(INCLUDE)
LIB                    =$(iBN);$(LIB)
!ENDIF

# -----------------------------------
# GNU Compiler Collection Macros
!IFDEF __GNUC__
CPP                    =gcc  #for GNU C++ Compiler
LNK                    =ld   #for GNU linker

# Flags
CPPFLAGS               =-o $(@) -c -funsigned-char -mwindows #for GNU C++ Compiler (gcc)
CMacroPre              =-D "" #for GNU C++ Compiler (gcc)

ldFLAGS                =-o $(@) #for GNU linker (ld)
LNKlibPre              =-l "" #for GNU linker (ld)
!ENDIF
# END GNU Compiler Collection Macros
# -----------------------------------

# -----------------------------------
# Micrososft Visual C Macros
!IFDEF __MSVC__
CPP                    =cl   #for Micrsoft Compiler
LNK                    =link #for Microsfoft Linker

# Flags
CPPFLAGS               =/Fo$(@R) /c /J # for Micrsoft Compiler(cl)
CMacroPre              =/D # for Micrsoft Compiler(cl)

LNKFLAGS               =/out:$(@) # for Microsfoft Linker (link)
LNKlibSuf              =.lib
!ENDIF
# END VMicrososft Visual C Macros
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

# Call Common
!include common.mak