@rem change to build directory
cd /d %~dp0

@rem SETUP Visual Studio environment for command line
@rem Fix directory name if incorrect!
call "C:\Program Files\Microsoft Visual Studio 10.0\VC\bin\vcvars32.bat"

@rem compile game
nmake -y -f dx.msvc.nmake.mak /d /n /p > "dx.msvc.nmake.mak - debug.txt"
pause