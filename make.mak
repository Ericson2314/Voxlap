# sub-directory macros
locSRC                 =$(realpath $(lastword $(MAKEFILE_LIST)))/source
locINC                 =$(realpath $(lastword $(MAKEFILE_LIST)))/include
locLIB                 =$(realpath $(lastword $(MAKEFILE_LIST)))/libraries
locBIN                 =$(realpath $(lastword $(MAKEFILE_LIST)))/binaries

# adding inbin directory to paths
ifdef __MSVC__
PATH                   =$(locLIB);$(PATH)
INCLUDE                =$(locLIB);$(INCLUDE)
LIB                    =$(locLIB);$(LIB)
endif

# -----------------------------------
# Choices

# "Debug" for debug build, "Release" for release build
build                  =Debug

# "gcc" for GNU C Compiler, "cl" for Micrsoft Compiler
CXX                    =gcc

# "nasm" for Netwide Assembler, masm" for Micrsoft Macro Assembler
AsmName                =nasm

# "ld" for GNU linker, "link" for Microsfoft Linker
LNK                    =ld

# "sdl" for Simple DirectMedia Layer, "win" for DirectX
GFXdep                 =sdl

# "win32" for Windows 32-bit, "posix" for POSIX (32-bit for now)
PLATdep                =posix

# "1" to use v5.$(AsmName), "0" to not use
USEV5ASM               =0

# END Choices 
# -----------------------------------

# -----------------------------------
# Build Flags

ifdef __GNUC__
endif
ifdef __MSVC__
endif

nasm_FLAGS             =-o $(@)              # Netwide Assembler (nasm)

jwasm_FLAGS            =-Fo$(@R) -c -coff -8 # Micrsoft Macro Assembler (masm)

masm_FLAGS             =/Fo$(@R) /c /coff    # Micrsoft Macro Assembler (masm)

cl_FLAGS               =/Fo$(@R) /c /J       # for Micrsoft Compiler(cl)
cl_Debug               =/MLd /ZI /GZ /RTCsuc /Od
cl_Release             =/Ox
cl_MacroPre            =/D                   # for Micrsoft Compiler(cl)

gcc_FLAGS              =-o $(@) -c -funsigned-char    # for GNU C++ Compiler (gcc)
gcc_Debug              =
gcc_Release            =
gcc_MacroPre           =-D #                          # for GNU C++ Compiler (gcc)

ld_FLAGS               =-o $(@)                       # for GNU linker (ld)
ld_Debug               =
ld_Release             =
ld_libPre              =-l #                          # for GNU linker (ld)
ld_libSuf              =

link_FLAGS             =/out:$(@)                     # for Microsfoft Linker (link)
link_Debug             =/DEBUG
link_Release           =
link_libPre            =
link_libSuf            =.lib                          # for Microsfoft Linker (link)




# END Build Flags
# -----------------------------------

# -----------------------------------
# Graphics
sdl_gcc_FLAGS          =`sdl-config --cflags`
sdl_ld_LIBs            =`sdl-config --static-libs`

sdl_cl_FLAGS           =/D_GNU_SOURCE=1 /Dmain=SDL_main
sdl_link_LIBs          =$(LNKlibPre)opengl32$(LNKlibSuf) $(LNKlibPre)glu32$(LNKlibSuf) $(LNKlibPre)sdl$(LNKlibSuf) $(LNKlibPre)sdlmain$(LNKlibSuf)

win_gcc_FLAGS          =
win_cl_FLAGS           =
win_$(LNK)_LIBs        =$(LNKlibPre)ddraw$(LNKlibSuf) $(LNKlibPre)dinput$(LNKlibSuf) $(LNKlibPre)dxguid$(LNKlibSuf)


# END Graphics
# -----------------------------------

# -----------------------------------
# Platform

posix_OBJSuf           =elf.o
posix_EXESuf           =

posix_nasm_FLAGS       =-f elf32
posix_ld_LIBs          =

Win32_LIBs             =

win32_OBJSuf           =obj
win32_EXESuf           =exe

win32_nasm_FLAGS       =-f win32
win32_ld_LIBs          =$(LNKlibPre)mingw32$(LNKlibSuf)

win32_gameLIBs	       =$(LNKlibPre)ole32$(LNKlibSuf)
win32_simpleLIBs       =$(LNKlibPre)ole32$(LNKlibSuf)
win32_voxedLIBs	       =                              $(LNKlibPre)comdlg32$(LNKlibSuf)
win32_kwalkLIBs	       =$(LNKlibPre)ole32$(LNKlibSuf) $(LNKlibPre)comdlg32$(LNKlibSuf)

win32_LIBs             =$(LNKlibPre)user32$(LNKlibSuf) $(LNKlibPre)gdi32$(LNKlibSuf)

# END Platform
# -----------------------------------

# -----------------------------------
# Toggle Random Macros
ifeq "$(USEV5ASM)" "1"
if_USEV5ASM            =$(locBIN)/v5.$(OBJSuf)
endif
Random_Macros          =$(CMacroPre)USEV5ASM=$(USEV5ASM)
# END Toggle Random Macros
# -----------------------------------

# -----------------------------------
# Choosing...
ifeq "$(AsmName)" "masm"
AS                     =ml
else
AS                     =$(AsmName)
endif
AFLAGS                 =$($(AsmName)_FLAGS) $($(PLATdep)_$(AsmName)_FLAGS)

CXXFLAGS               =$($(CXX)_FLAGS) $($(CXX)_$(build)) $($(GFXdep)_$(CXX)_FLAGS) $($(PLATdep)_$(CXX)_FLAGS) $(Random_Macros)
CMacroPre              =$($(CXX)_MacroPre)

LNKFLAGS               =$($(LNK)_FLAGS) $($(LNK)_$(build)) $($(GFXdep)_$(LNK)_FLAGS) $($(PLATdep)_$(LNK)_FLAGS) $(if_USEV5ASM)
LNKlibPre              =$($(LNK)_libPre)
LNKlibSuf              =$($(LNK)_libSuf)

gameLIBs	           =$($(GFXdep)_gameLIBs)   $($(GFXdep)_$(LNK)_LIBs) $($(PLATdep)_gameLIBs)   $($(PLATdep)_$(LNK)_LIBs) $($(PLATdep)_LIBs)
simpleLIBs             =$($(GFXdep)_simpleLIBs) $($(GFXdep)_$(LNK)_LIBs) $($(PLATdep)_simpleLIBs) $($(PLATdep)_$(LNK)_LIBs) $($(PLATdep)_LIBs)
voxedLIBs	           =$($(GFXdep)_voxedLIBs)  $($(GFXdep)_$(LNK)_LIBs) $($(PLATdep)_voxedLIBs)  $($(PLATdep)_$(LNK)_LIBs) $($(PLATdep)_LIBs)
kwalkLIBs	           =$($(GFXdep)_kwalkLIBs)  $($(GFXdep)_$(LNK)_LIBs) $($(PLATdep)_kwalkLIBs)  $($(PLATdep)_$(LNK)_LIBs) $($(PLATdep)_LIBs) 

OBJSuf                 =$($(PLATdep)_OBJSuf)
EXESuf                 =$($(PLATdep)_EXESuf)

# END Choosing...
# -----------------------------------

rm                     =rm -f

# Call Common
include common.mak
