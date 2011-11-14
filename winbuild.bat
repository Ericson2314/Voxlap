@rem SETUP Visual C 6.0 (SP5) environment for command line
@rem Fix directory name if incorrect!
call "C:\Program Files\Microsoft Visual Studio 10.0\VC\bin\vcvars32.bat"

@rem SETUP DirectX 8.x environment for command line
@rem Fix directory name if incorrect!
rem set include=\dxsdk\include;%include%
set lib=%ProgramFiles%\Microsoft DirectX SDK (June 2010)\Lib\x86;%lib%

@rem adding ./inbin to paths
set include=%cd%\inbin;%include%
set lib=%cd%\inbin;%lib%
set path=%cd%\inbin;%path%

@rem compile game
@rem compile simple (must do this after game.c)
@rem if exist winmain.obj del winmain.obj
@rem nmake kwalk.c
@rem compile voxed
nmake -f winbuild.mak
pause