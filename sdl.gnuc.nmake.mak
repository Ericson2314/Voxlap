# nmake
# GNUC
# SDL

# Assembler Choice
AsmName  =nasm

# Compiler Choice
__GNUC__ =1
PATH     =/mingw/bin/;$(PATH)
INCLUDE  =/mingw/include/;$(INCLUDE)
LIB      =/mingw/lib;$(LIB)
        
# MinGW SDL
GFXdep   =sdl
INCLUDE  =/mingw/include/SDL;$(INCLUDE)
LIB      =/mingw/lib;$(LIB)
GFXOBJ   =opengl32.lib glu32.lib mingw32.lib sdl.lib sdlmain.lib

!include nmake.mak