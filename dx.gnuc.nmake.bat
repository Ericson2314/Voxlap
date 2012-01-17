@rem change to build directory
cd /d %~dp0

@rem SETUP Visual Studio environment for command line
@rem Fix directory name if incorrect!
call "C:\Program Files\Microsoft Visual Studio 10.0\VC\bin\vcvars32.bat"

@rem compile game
nmake -y -f dx.gnuc.nmake.mak
pause