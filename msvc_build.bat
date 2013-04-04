@rem nmake
@rem MSVC
@rem DirectX

@rem change to build directory
cd /d %~dp0

@rem SETUP Visual Studio environment for command line
@rem Fix path if incorrect!
call "C:\Program Files\Microsoft Visual Studio 10.0\VC\bin\vcvars32.bat"

@rem SETUP DirectX environment for command line
@rem Fix path if incorrect!
set LIB=/Program Files/Microsoft DirectX SDK (June 2010)/LIB/x86;%LIB%

@rem Assembler Choice
set _AsmName=nasm

@rem Random options
set _GFX=sdl
set USEV5ASM=1
set BUILD=Debug

@rem compile game
nmake -y -f nmake.mak
pause
