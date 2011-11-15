@rem change to build directory
cd /d %~dp0

@rem SETUP Visual C 6.0 (SP5) environment for command line
@rem Fix directory name if incorrect!
call "C:\Program Files\Microsoft Visual Studio 10.0\VC\bin\vcvars32.bat"

@rem compile game
@rem compile simple (must do this after game.c)
@rem if exist winmain.obj del winmain.obj
@rem nmake kwalk.c
@rem compile voxed
nmake -f winbuild.mak /d /n /p > nmakestout.txt
pause