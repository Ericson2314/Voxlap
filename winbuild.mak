# XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
# XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX=BEGIN MACROS=XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
# XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

# adding inbin directory to paths
PATH					=$(iBN);$(PATH)
INCLUDE					=$(iBN);$(INCLUDE)
LIB						=$(iBN);$(LIB)

# sub-directory macros
SRC						=$(MAKEDIR)/src
iBN						=$(MAKEDIR)/inbin
oBN						=$(MAKEDIR)/outbin

# DirectX directories (OLD: SETUP DirectX 8.x environment for command line. Fix directory name if incorrect!)
# include				  =/dxsdk/include;$(include)
LIB						=C:/Program Files/Microsoft DirectX SDK (June 2010)/Lib/x86;$(LIB)

# FOSS complation directories
PATH					=C:/Mingw;$(PATH)

# Per-Program Flags
masmFLAGS				=/Fo$(@R) /c /coff 
nasmFLAGS				=-o $(@) -f win32

clFLAGS					=/Fo$(@R) /c /J /Tp 
gppFLAGS				=

# Compiler Choices [XXXXXXXXXXXXXXXX=CHANGE THIS FOR UNIX COMPILE:XXXXXXXXXXXXXXXX]
AsmName					=masm #winSDK: =masm #GCC: =nasm NOT gas
CPP						=cl   #winSDK: =cl   #GCC: =gpp 
LNK						=link #winSDK: =LNK  #GCC: =gLNK



#  Graphics Codepath [XXXXXXXXXXXXXXXX=CHANGE THIS FOR UNIX COMPILE=XXXXXXXXXXXXXXXX]
GFXdep					=win #DirectX: =win SDL: =sdl

# Conditionl Hacks
!IF "$(AsmName)"=="masm"
AS=ml
AFLAGS					=$(masmFLAGS) #$($(AsmName)FLAGS)
!ELSE
AS=$(AsmName)
!IF "$(AsmName)"=="nasm"
AFLAGS					=$(nasmFLAGS) #$($(AsmName)FLAGS)
!ENDIF
!ENDIF

!IF "$(CPP)"=="cl"
CPPFLAGS				=$(clFLAGS) #$($(CPP)FLAGS)
!ELSEIF "$(CPP)"=="gpp"
CPPFLAGS				=$(gppFLAGS) #$($(CPP)FLAGS)
!ENDIF

!IF "$(LNK)"=="link"
LNKFLAGS				=$(linkFLAGS) #$($(LNK)FLAGS)
!ELSEIF "$(LNK)"=="glink"
CPPFLAGS				=$(glinkFLAGS) #$($(LNK)FLAGS)
!ENDIF

# XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
# XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX=END MACROS=XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
# XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

default:	$(oBN)/game.exe	$(oBN)/simple.exe $(oBN)/voxed.exe $(oBN)/kwalk.exe
# $(oBN)/ $(**B)

# executable (.exe) (meta)targets
$(oBN)/game.exe:		$(oBN)/game.obj   $(oBN)/voxlap5.obj $(oBN)/v5.obj $(oBN)/kplib.obj $(oBN)/$(GFXdep)main1.obj;
	$(LNK) /out:$(@)	$(oBN)/game.obj   $(oBN)/voxlap5     $(oBN)/v5     $(oBN)/kplib     $(oBN)/$(GFXdep)main1     ddraw.lib dinput.lib ole32.lib dxguid.lib user32.lib gdi32.lib
											
$(oBN)/simple.exe:		$(oBN)/simple.obj $(oBN)/voxlap5.obj $(oBN)/v5.obj $(oBN)/kplib.obj $(oBN)/$(GFXdep)main1.obj;
	$(LNK) /out:$(@)	$(oBN)/simple     $(oBN)/voxlap5     $(oBN)/v5     $(oBN)/kplib     $(oBN)/$(GFXdep)main1     ddraw.lib dinput.lib ole32.lib dxguid.lib user32.lib gdi32.lib
											
$(oBN)/voxed.exe:		$(oBN)/voxed.obj  $(oBN)/voxlap5.obj $(oBN)/v5.obj $(oBN)/kplib.obj $(oBN)/$(GFXdep)main2.obj;
	$(LNK) /out:$(@)	$(oBN)/voxed      $(oBN)/voxlap5     $(oBN)/v5     $(oBN)/kplib     $(oBN)/$(GFXdep)main2     ddraw.lib dinput.lib           dxguid.lib user32.lib gdi32.lib comdlg32.lib
											
$(oBN)/kwalk.exe:		$(oBN)/kwalk.obj  $(oBN)/voxlap5.obj $(oBN)/v5.obj $(oBN)/kplib.obj $(oBN)/$(GFXdep)main2.obj;
	$(LNK) /out:$(@)	$(oBN)/kwalk      $(oBN)/voxlap5     $(oBN)/v5     $(oBN)/kplib     $(oBN)/$(GFXdep)main2     ddraw.lib dinput.lib ole32.lib dxguid.lib user32.lib gdi32.lib comdlg32.lib
											

# binary object (.obj) targets

# Primary Objects
$(oBN)/game.obj:       $(SRC)/game.cpp   $(SRC)/voxlap5.h $(SRC)/sysmain.h;
	$(CPP) $(CPPFLAGS) $(SRC)/game.cpp   /QIfist

$(oBN)/simple.obj:     $(SRC)/simple.cpp $(SRC)/voxlap5.h $(SRC)/sysmain.h;
	$(CPP) $(CPPFLAGS) $(SRC)/simple.cpp /QIfist

$(oBN)/voxed.obj:      $(SRC)/voxed.cpp  $(SRC)/voxlap5.h $(SRC)/sysmain.h;
	$(CPP) $(CPPFLAGS) $(SRC)/voxed.cpp
	
$(oBN)/kwalk.obj:      $(SRC)/kwalk.cpp  $(SRC)/voxlap5.h $(SRC)/sysmain.h;
	$(CPP) $(CPPFLAGS) $(SRC)/kwalk.cpp

# Secondary Objects
$(oBN)/voxlap5.obj:    $(SRC)/voxlap5.cpp $(SRC)/voxlap5.h;
	$(CPP) $(CPPFLAGS) $(SRC)/voxlap5.cpp

$(oBN)/v5.obj:         $(SRC)/v5.$(AsmName);
	$(AS)  $(AFLAGS)   $(SRC)/v5.$(AsmName)

$(oBN)/kplib.obj:      $(SRC)/kplib.cpp;
	$(CPP) $(CPPFLAGS) $(SRC)/kplib.cpp

$(oBN)/winmain1.obj:   $(SRC)/winmain.cpp $(SRC)/sysmain.h;
	$(CPP) $(CPPFLAGS) $(SRC)/winmain.cpp /DUSEKZ /DZOOM_TEST 

$(oBN)/winmain2.obj:   $(SRC)/winmain.cpp $(SRC)/sysmain.h;
	$(CPP) $(CPPFLAGS) $(SRC)/winmain.cpp /DNOSOUND

# Clearn Script
clean:
	cd $(oBN)
	del *.exe *.obj