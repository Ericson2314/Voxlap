@rem SETUP Visual C 6.0 (SP5) environment for command line
@rem Fix directory name if incorrect!
call "C:\Program Files\Microsoft Visual Studio 10.0\VC\bin\vcvars32.bat"

@rem SETUP DirectX 8.x environment for command line
@rem Fix directory name if incorrect!
rem set include=\dxsdk\include;%include%
rem set lib=\dxsdk\lib;%lib%

@rem compile game
nmake game.c

@rem compile simple (must do this after game.c)
nmake simple.c

if exist winmain.obj del winmain.obj

@rem compile voxed
nmake voxed.c

@rem compile kwalk
nmake kwalk.c
pause