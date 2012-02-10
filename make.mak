# adding inbin directory to paths
ifdef __MSVC__
#PATH                   =$(iBN);$(PATH)
#INCLUDE                =$(iBN);$(INCLUDE)
#LIB                    =$(iBN);$(LIB)
endif

# sub-directory macros
SRC                    =$(MAKEDIR)/src
iBN                    =$(MAKEDIR)/inbin
oBN                    =$(MAKEDIR)/outbin
# -----------------------------------
# GNU Compiler Collection Macros
ifdef __GNUC__
AsmName                =nasm #for Netwide Assembler
AS=$(AsmName)
CPP                    =gcc  #for GNU C++ Compiler
LNK                    =ld   #for GNU linker

# Flags
AFLAGS                 =-o $(@) -f win32 #for Netwide Assembler (nasm)
CPPFLAGS               =-o $(@) -c -funsigned-char `sdl-config --cflags` #for GNU C++ Compiler (gcc)
CMacroPre              =-D" " #for GNU C++ Compiler (gcc)

ldFLAGS                =-o $(@) `sdl-config --libs` #for GNU linker (ld)
endif
# END GNU Compiler Collection Macros
# -----------------------------------

# -----------------------------------
# Micrososft Visual C Macros
ifdef __MSVC__
AsmName                =masm #masm or nasm
CPP                    =cl   #for Micrsoft Compiler
LNK                    =link #for Microsfoft Linker

# Flags
CPPFLAGS               =/Fo$(@R) /c /J # for Micrsoft Compiler(cl)
CMacroPre              =/D # for Micrsoft Compiler(cl)

LNKFLAGS               =/out:$(@) # for Microsfoft Linker (link)
endif
# END Micrososft Visual C Macros
# -----------------------------------

# -----------------------------------
# Assembler Macros
if AsmName=masm
AS=ml
masmFLAGS              =/Fo$(@R) /c /coff
endif

if AsmName=nasm
AS=nasm
nasmFLAGS              =-o $(@) -f win32
endif
# END Assembler Macros
# -----------------------------------

# Call Common
include common.mak