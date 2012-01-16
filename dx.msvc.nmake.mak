# nmake
# MSVC
# DirectX

# Assembler Choice
AsmName  =nasm
PATH     =/mingw/bin/;$(PATH)

# Compiler Choice
__MSVC__ =1

# DirectX
GFXdep   =win
LIB      =/Program Files/Microsoft DirectX SDK (June 2010)/LIB/x86;$(LIB)
GFXOBJ   =ddraw.lib dinput.lib dxguid.lib

!include nmake.mak