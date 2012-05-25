@rem nmake
@rem MSVC
@rem SDL

@rem change to build directory
cd /d %~dp0

@rem SETUP Visual Studio environment for command line
@rem Fix directory name if incorrect!
call "C:\Program Files\Microsoft Visual Studio 10.0\VC\bin\vcvars32.bat"

@rem Assembler Choice
set _AsmName=masm


@rem Compiler Choice
set __MSVC__=1




@rem DirectX
set _GFXdep=sdl
set INCLUDE=/Program Files/Microsoft Visual Studio 10.0/VC/include/SDL;%INCLUDE%
set LIB=/Program Files/Microsoft Visual Studio 10.0/VC/lib/;%LIB%
set GFXOBJ=opengl32.lib glu32.lib sdl.lib sdlmain.lib
set GFXCFLAGS=/D_GNU_SOURCE=1 /Dmain=SDL_main

set USEV5ASM=0
set BUILD=Debug

@rem compile game
nmake -y -f nmake.mak
pause