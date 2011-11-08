@rem SETUP Visual C 6.0 (SP5) environment for command line
@rem Fix directory name if incorrect!
rem call "\Program Files\Microsoft Visual Studio\vc98\bin\vcvars32.bat"

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
