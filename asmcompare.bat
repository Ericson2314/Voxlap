cd /d %~dp0
echo CURRENT DIRECTORY: %~dp0
mkdir %~dp0decomp
call "C:\Program Files\Microsoft Visual Studio 10.0\VC\bin\vcvars32.bat"

@rem COMPILING

ml /Fl./outbin/v5.lst /Fo./outbin/v5.obj /c /coff /Zd ./src/v5.masm > asmcompare.txt 2>&1

echo. >> asmcompare.txt 2>&1
echo ============================= >> asmcompare.txt 2>&1
echo. >> asmcompare.txt 2>&1

"/Program Files/nasm/nasm" ./src/v5.nasm -f win32 -o ./outbin/v6.obj -l ./outbin/v6.lst >> asmcompare.txt 2>&1

echo. >> asmcompare.txt 2>&1
echo ============================= >> asmcompare.txt 2>&1
echo. >> asmcompare.txt 2>&1

/MinGW/bin/as.exe -mmnemonic=intel -msyntax=intel -march=i686 -o ./outbin/v7.obj ./src/v5.nasm >> asmcompare.txt 2>&1

@rem DECOMPILING
"/Program Files/IDA Pro 6.1/idaq" -B %~dp0outbin\v5.obj
"/Program Files/IDA Pro 6.1/idaq" -B %~dp0outbin\v6.obj

"/Program Files/nasm/ndisasm" %~dp0outbin\v5.obj >  %~dp0decomp\v5.masm.nasm 
"/Program Files/nasm/ndisasm" %~dp0outbin\v6.obj >  %~dp0decomp\v5.nasm.nasm

@rem MOVING
move %~dp0outbin\v5.asm %~dp0decomp\v5.masm.masm
move %~dp0outbin\v5.idb %~dp0decomp\v5.masm.idb
move %~dp0outbin\v5.lst %~dp0decomp\v5.masm.lst

move %~dp0outbin\v6.asm %~dp0decomp\v5.nasm.masm
move %~dp0outbin\v6.idb %~dp0decomp\v5.nasm.idb
move %~dp0outbin\v6.lst %~dp0decomp\v5.nasm.lst

pause