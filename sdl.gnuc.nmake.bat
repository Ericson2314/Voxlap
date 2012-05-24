@rem nmake
@rem GNUC
@rem SDL

@rem change to build directory
cd /d %~dp0

@rem SETUP Visual Studio environment for command line
@rem Fix directory name if incorrect!
call "C:\Program Files\Microsoft Visual Studio 10.0\VC\bin\vcvars32.bat"

@rem Assembler Choice
set _AsmName=nasm
@rem set PATH=/mingw/bin/;%PATH%

@rem Compiler Choice
set __GNUC__=1
set PATH=/mingw/bin/;%PATH%
set INCLUDE=/mingw/include/;%INCLUDE%
set LIB=/mingw/lib;%LIB%

@rem DirectX
set _GFXdep=sdl
set INCLUDE=/mingw/include/SDL;%INCLUDE%
set LIB=/mingw/lib;%LIB%
set GFXOBJ=-l opengl32.lib -l glu32.lib -l mingw32.lib -l sdl.lib -l sdlmain.lib
set GFXCFLAGS=-D_GNU_SOURCE=1 -Dmain=SDL_main



@rem compile game
nmake -y -f nmake.mak
pause