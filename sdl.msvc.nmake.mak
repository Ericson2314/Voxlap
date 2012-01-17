# nmake
# MSVC
# SDL

# Assembler Choice
AsmName   =masm

# Compiler Choice
__MSVC__  =1


# MSVC SDL
GFXdep    =sdl
INCLUDE   =/Program Files/Microsoft Visual Studio 10.0/VC/include/SDL;$(INCLUDE)
LIB       =/Program Files/Microsoft Visual Studio 10.0/VC/lib/;$(LIB)
GFXOBJ    =opengl32.lib glu32.lib sdl.lib sdlmain.lib
GFXCFLAGS =/D_GNU_SOURCE=1 /Dmain=SDL_main
!include nmake.mak