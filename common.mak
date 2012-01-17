# sub-directory macros
SRC                        =$(MAKEDIR)/src
iBN                        =$(MAKEDIR)/inbin
oBN                        =$(MAKEDIR)/outbin

Phony:                     default
default:                   $(oBN)/game.exe    $(oBN)/simple.exe $(oBN)/voxed.exe $(oBN)/kwalk.exe

# executable (.exe) (meta)targets
$(oBN)/game.exe:           $(oBN)/game.obj   $(oBN)/voxlap5.obj $(oBN)/v5.obj $(oBN)/kpLIB.obj $(oBN)/$(GFXdep)main1.obj;
	$(LNK) $(LNKFLAGS)     $(oBN)/game.obj   $(oBN)/voxlap5.obj $(oBN)/v5.obj $(oBN)/kpLIB.obj $(oBN)/$(GFXdep)main1.obj \
	                       $(GFXOBJ) $(LNKlibPre)ole32$(LNKlibSuf) $(LNKlibPre)user32$(LNKlibSuf) $(LNKlibPre)gdi32$(LNKlibSuf)

$(oBN)/simple.exe:         $(oBN)/simple.obj $(oBN)/voxlap5.obj $(oBN)/v5.obj $(oBN)/kpLIB.obj $(oBN)/$(GFXdep)main1.obj;
	$(LNK) $(LNKFLAGS)     $(oBN)/simple.obj $(oBN)/voxlap5.obj $(oBN)/v5.obj $(oBN)/kpLIB.obj $(oBN)/$(GFXdep)main1.obj \
	                       $(GFXOBJ) $(LNKlibPre)ole32$(LNKlibSuf) $(LNKlibPre)user32$(LNKlibSuf) $(LNKlibPre)gdi32$(LNKlibSuf)

$(oBN)/voxed.exe:          $(oBN)/voxed.obj  $(oBN)/voxlap5.obj $(oBN)/v5.obj $(oBN)/kpLIB.obj $(oBN)/$(GFXdep)main2.obj;
	$(LNK) $(LNKFLAGS)     $(oBN)/voxed.obj  $(oBN)/voxlap5.obj $(oBN)/v5.obj $(oBN)/kpLIB.obj $(oBN)/$(GFXdep)main2.obj \
	                       $(GFXOBJ)                               $(LNKlibPre)user32$(LNKlibSuf) $(LNKlibPre)gdi32$(LNKlibSuf) $(LNKlibPre)comdlg32$(LNKlibSuf)

$(oBN)/kwalk.exe:          $(oBN)/kwalk.obj  $(oBN)/voxlap5.obj $(oBN)/v5.obj $(oBN)/kpLIB.obj $(oBN)/$(GFXdep)main2.obj;
	$(LNK) $(LNKFLAGS)     $(oBN)/kwalk.obj  $(oBN)/voxlap5.obj $(oBN)/v5.obj $(oBN)/kpLIB.obj $(oBN)/$(GFXdep)main2.obj \
	                       $(GFXOBJ) $(LNKlibPre)ole32$(LNKlibSuf) $(LNKlibPre)user32$(LNKlibSuf) $(LNKlibPre)gdi32$(LNKlibSuf) $(LNKlibPre)comdlg32$(LNKlibSuf)


# binary object (.obj) targets

# Primary Objects
$(oBN)/game.obj:           $(SRC)/game.cpp   $(SRC)/voxlap5.h $(SRC)/sysmain.h;
	$(CPP) $(CPPFLAGS)     $(SRC)/game.cpp
#   used to use /QIfist

$(oBN)/simple.obj:         $(SRC)/simple.cpp $(SRC)/voxlap5.h $(SRC)/sysmain.h;
	$(CPP) $(CPPFLAGS)     $(SRC)/simple.cpp
#   used to use /QIfist

$(oBN)/voxed.obj:          $(SRC)/voxed.cpp  $(SRC)/voxlap5.h $(SRC)/sysmain.h;
	$(CPP) $(CPPFLAGS)     $(SRC)/voxed.cpp

$(oBN)/kwalk.obj:          $(SRC)/kwalk.cpp  $(SRC)/voxlap5.h $(SRC)/sysmain.h;
	$(CPP) $(CPPFLAGS)     $(SRC)/kwalk.cpp

# Secondary Objects
$(oBN)/voxlap5.obj:        $(SRC)/voxlap5.cpp $(SRC)/voxlap5.h;
	$(CPP) $(CPPFLAGS)     $(SRC)/voxlap5.cpp

$(oBN)/v5.obj:             $(SRC)/v5.$(AsmName);
	$(AS)  $(AFLAGS)       $(SRC)/v5.$(AsmName)

$(oBN)/kpLIB.obj:          $(SRC)/kpLIB.cpp;
	$(CPP) $(CPPFLAGS)     $(SRC)/kpLIB.cpp

$(oBN)/$(GFXdep)main1.obj: $(SRC)/$(GFXdep)main.cpp $(SRC)/sysmain.h;
	$(CPP) $(CPPFLAGS)     $(SRC)/$(GFXdep)main.cpp $(CMacroPre)USEKZ $(CMacroPre)ZOOM_TEST

$(oBN)/$(GFXdep)main2.obj: $(SRC)/$(GFXdep)main.cpp $(SRC)/sysmain.h;
	$(CPP) $(CPPFLAGS)     $(SRC)/$(GFXdep)main.cpp $(CMacroPre)NOSOUND

# Clearn Script
cleanall: clean
	cd $(oBN)
	del *.exe *.dll
	cd ../decomp
	del v*
clean:
	del "* - debug.txt"
	del asmcompare.txt
	cd $(oBN)
	del *.obj