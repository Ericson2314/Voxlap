@rem nmake debug
@rem GNUC
@rem DirectX

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
set _GFXdep=win
set LIB=/mingw/lib;%LIB%
set GFXOBJ=-l mingw32 -l ddraw -l dinput -l dxguid



set USEV5ASM=1

@rem compile game
nmake -y -f nmake.mak
pause