@rem nmake debug
@rem MSVC
@rem DirectX

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
set _GFXdep=win
set LIB=/Program Files/Microsoft DirectX SDK (June 2010)/LIB/x86;%LIB%
set GFXOBJ=ddraw.lib dinput.lib dxguid.lib



set USEV5ASM=1

@rem compile game
nmake -y -f nmake.mak /d /n /p > "dx.msvc.nmake.mak - debug.txt"
pause