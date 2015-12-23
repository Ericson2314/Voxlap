@rem SETUP Visual C 6.0 (SP5) environment for command line
@rem Fix directory name if incorrect!
call "C:\Program Files\Microsoft Visual Studio 10.0\VC\vcvarsall.bat"

@rem SETUP LLVM environment for command line
@rem Fix directory name if incorrect!
set INCLUDE=%ProgramFiles%\LLVM\include;%INCLUDE%
set LIB=%ProgramFiles%\LLVM\lib\;%LIB%

@rem SETUP DirectX 8.x environment for command line
@rem Fix directory name if incorrect!
set INCLUDE=%ProgramFiles%\Microsoft DirectX SDK (June 2010)\include;%INCLUDE%
set LIB=%ProgramFiles%\Microsoft DirectX SDK (June 2010)\Lib\x86;%LIB%
