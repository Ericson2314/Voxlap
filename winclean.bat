@rem change to build directory
call "C:\Program Files\Microsoft Visual Studio 10.0\VC\bin\vcvars32.bat"
cd /d %~dp0
nmake -y -f winbuild.mak clean
pause