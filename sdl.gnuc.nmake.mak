# nmake
# GNUC
# SDL

# Assembler Choice
AsmName   =nasm

# Compiler Choice
__GNUC__  =1
PATH      =/mingw/bin/;$(PATH)
INCLUDE   =/mingw/include/;$(INCLUDE)
LIB       =/mingw/lib;$(LIB)
        
# MinGW SDL
GFXdep    =sdl
INCLUDE   =/mingw/include/SDL;$(INCLUDE)
LIB       =/mingw/lib;$(LIB)
GFXOBJ    =-l opengl32.lib -l glu32.lib -l mingw32.lib -l sdl.lib -l sdlmain.lib
GFXCFLAGS =-D_GNU_SOURCE=1 -Dmain=SDL_main
!include nmake.mak