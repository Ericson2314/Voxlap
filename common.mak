# sub-directory macros
# locSRC                      =$(MAKEDIR)/source
# locINC                      =$(MAKEDIR)/include
# locLIB                      =$(MAKEDIR)/libraries
# locBIN                      =$(MAKEDIR)/binaries

Phony:                        default
default:                      $(locBIN)/game.$(EXESuf) $(locBIN)/simple.$(EXESuf) $(locBIN)/voxed.$(EXESuf) $(locBIN)/kwalk.$(EXESuf)

# executable (.$(EXESuf)) (meta)targets
game:                         $(locBIN)/game.$(EXESuf)
$(locBIN)/game.$(EXESuf):     $(locBIN)/game.$(OBJSuf)   $(locBIN)/voxlap5.$(OBJSuf) $(locBIN)/v5.$(OBJSuf) $(locBIN)/kplib.$(OBJSuf) $(locBIN)/$(GFXdep)main1.$(OBJSuf)
	$(LNK) $(LNKFLAGS)        $(locBIN)/game.$(OBJSuf)   $(locBIN)/voxlap5.$(OBJSuf) $(locBIN)/v5.$(OBJSuf) $(locBIN)/kplib.$(OBJSuf) $(locBIN)/$(GFXdep)main1.$(OBJSuf) $(gameLIBs)

simple:                       $(locBIN)/simple.$(OBJSuf)
$(locBIN)/simple.$(EXESuf):   $(locBIN)/simple.$(OBJSuf) $(locBIN)/voxlap5.$(OBJSuf) $(locBIN)/v5.$(OBJSuf) $(locBIN)/kplib.$(OBJSuf) $(locBIN)/$(GFXdep)main1.$(OBJSuf)
	$(LNK) $(LNKFLAGS)        $(locBIN)/simple.$(OBJSuf) $(locBIN)/voxlap5.$(OBJSuf) $(locBIN)/v5.$(OBJSuf) $(locBIN)/kplib.$(OBJSuf) $(locBIN)/$(GFXdep)main1.$(OBJSuf) $(simpleLIBs)

voxed:                        $(locBIN)/voxed.$(OBJSuf)
$(locBIN)/voxed.$(EXESuf):    $(locBIN)/voxed.$(OBJSuf)  $(locBIN)/voxlap5.$(OBJSuf) $(locBIN)/v5.$(OBJSuf) $(locBIN)/kplib.$(OBJSuf) $(locBIN)/$(GFXdep)main2.$(OBJSuf)
	$(LNK) $(LNKFLAGS)        $(locBIN)/voxed.$(OBJSuf)  $(locBIN)/voxlap5.$(OBJSuf) $(locBIN)/v5.$(OBJSuf) $(locBIN)/kplib.$(OBJSuf) $(locBIN)/$(GFXdep)main2.$(OBJSuf) $(voxedLIBs)

kwalk:                        $(locBIN)/kwalk.$(OBJSuf)
$(locBIN)/kwalk.$(EXESuf):    $(locBIN)/kwalk.$(OBJSuf)  $(locBIN)/voxlap5.$(OBJSuf) $(locBIN)/v5.$(OBJSuf) $(locBIN)/kplib.$(OBJSuf) $(locBIN)/$(GFXdep)main2.$(OBJSuf)
	$(LNK) $(LNKFLAGS)        $(locBIN)/kwalk.$(OBJSuf)  $(locBIN)/voxlap5.$(OBJSuf) $(locBIN)/v5.$(OBJSuf) $(locBIN)/kplib.$(OBJSuf) $(locBIN)/$(GFXdep)main2.$(OBJSuf) $(kwalkLIBs)


# binary object (.$(OBJSuf)) targets

# Primary Objects
game:                         $(locSRC)/game.cpp
$(locBIN)/game.$(OBJSuf):     $(locSRC)/game.cpp   $(locINC)/voxlap5.h $(locINC)/sysmain.h
	$(CXX) $(CXXFLAGS)        $(locSRC)/game.cpp
#   used to use /QIfist

simple:                       $(locSRC)/simple.cpp
$(locBIN)/simple.$(OBJSuf):   $(locSRC)/simple.cpp $(locINC)/voxlap5.h $(locINC)/sysmain.h
	$(CXX) $(CXXFLAGS)        $(locSRC)/simple.cpp
#   used to use /QIfist

voxed:                        $(locSRC)/voxed.cpp
$(locBIN)/voxed.$(OBJSuf):    $(locSRC)/voxed.cpp  $(locINC)/voxlap5.h $(locINC)/sysmain.h
	$(CXX) $(CXXFLAGS)        $(locSRC)/voxed.cpp

kwalk:                        $(locSRC)/kwalk.cpp
$(locBIN)/kwalk.$(OBJSuf):    $(locSRC)/kwalk.cpp  $(locINC)/voxlap5.h $(locINC)/sysmain.h
	$(CXX) $(CXXFLAGS)        $(locSRC)/kwalk.cpp

# Secondary Objects
voxlap:                       $(locSRC)/voxlap5.cpp 
$(locBIN)/voxlap5.$(OBJSuf):  $(locSRC)/voxlap5.cpp $(locINC)/voxlap5.h
	$(CXX) $(CXXFLAGS)        $(locSRC)/voxlap5.cpp

v5:                           $(locBIN)/v5.$(OBJSuf)
$(locBIN)/v5.$(OBJSuf):       $(locSRC)/v5.$(AsmName)
	$(AS)  $(AFLAGS)          $(locSRC)/v5.$(AsmName)

kplib:                        $(locBIN)/kplib.$(OBJSuf)
$(locBIN)/kplib.$(OBJSuf):    $(locSRC)/kplib.cpp
	$(CXX) $(CXXFLAGS)        $(locSRC)/kplib.cpp

winmain1:                     $(locBIN)/winmain1.$(OBJSuf)
$(locBIN)/winmain1.$(OBJSuf): $(locSRC)/winmain.cpp $(locINC)/sysmain.h
	$(CXX) $(CXXFLAGS)        $(locSRC)/winmain.cpp $(CMacroPre)USEKZ $(CMacroPre)ZOOM_TEST

winmain2:                     $(locBIN)/winmain2.$(OBJSuf)
$(locBIN)/winmain2.$(OBJSuf): $(locSRC)/winmain.cpp $(locINC)/sysmain.h
	$(CXX) $(CXXFLAGS)        $(locSRC)/winmain.cpp $(CMacroPre)NOSOUND
	
sdlmain1:                     $(locBIN)/sdlmain1.$(OBJSuf)
$(locBIN)/sdlmain1.$(OBJSuf): $(locSRC)/sdlmain.c $(locINC)/sysmain.h
	$(CXX) $(CXXFLAGS)        $(locSRC)/sdlmain.c $(CMacroPre)USEKZ $(CMacroPre)ZOOM_TEST

sdlmain2:                     $(locBIN)/sdlmain2.$(OBJSuf)
$(locBIN)/sdlmain2.$(OBJSuf): $(locSRC)/sdlmain.c $(locINC)/sysmain.h
	$(CXX) $(CXXFLAGS)        $(locSRC)/sdlmain.c $(CMacroPre)NOSOUND

# Clearn Script
cleanall: clean
	$(rm) $(locBIN)/*
	$(rm) ./decompv*
clean:
	$(rm) "* - debug.txt"
	$(rm) asmcompare.txt
	$(rm) $(locBIN)/*.elf.o $(locBIN)/*.obj
