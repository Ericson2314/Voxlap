# nmake
# MSVC
# SDL

# Assembler Choice
AsmName  =masm

# Compiler Choice
__MSVC__ =1


# MSVC SDL
GFXdep   =sdl
LIB      =/Program Files/Microsoft Visual Studio 10.0/VC/lib/;$(LIB)
INCLUDE  =/Program Files/Microsoft Visual Studio 10.0/VC/include/SDL;$(INCLUDE)
GFXOBJ   =opengl32.lib glu32.lib sdl.lib sdlmain.lib

!include nmake.mak