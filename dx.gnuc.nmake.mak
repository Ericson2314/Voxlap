# nmake
# GNUC
# DirectX

# Assembler Choice
AsmName  =nasm

# Compiler Choice
__GNUC__ =1
PATH     =/mingw/bin/;$(PATH)
INCLUDE  =/mingw/include/;$(INCLUDE)
LIB      =/mingw/lib;$(LIB)
        
# DirectX
GFXdep   =win
#LIB      =/Program Files/Microsoft DirectX SDK (June 2010)/LIB/x86;$(LIB)
LIB      =/mingw/lib;$(LIB)
GFXOBJ   =-l mingw32 -l ddraw -l dinput -l dxguid

!include nmake.mak