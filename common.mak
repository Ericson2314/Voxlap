# sub-directory macros
# locSRC                      =$(MAKEDIR)/source
# locINC                      =$(MAKEDIR)/include
# locLIB                      =$(MAKEDIR)/libraries
# locBIN                      =$(MAKEDIR)/binaries

Phony:                        default
default:                      $(locBIN)/game$(EXESuf) $(locBIN)/simple$(EXESuf) $(locBIN)/voxed$(EXESuf) $(locBIN)/kwalk$(EXESuf)

# executable ($(EXESuf)) (meta)targets
game:                         $(locBIN)/game$(EXESuf)
$(locBIN)/game$(EXESuf):      $(locBIN)/game$(OBJSuf)   $(locBIN)/voxlap5$(OBJSuf) $(if_USEV5ASM) $(locBIN)/kplib$(OBJSuf) $(locBIN)/$(GFXdep)main1$(OBJSuf)
	$(LNK) $(LNKFLAGS)        $(locBIN)/game$(OBJSuf)   $(locBIN)/voxlap5$(OBJSuf) $(if_USEV5ASM) $(locBIN)/kplib$(OBJSuf) $(locBIN)/$(GFXdep)main1$(OBJSuf) $(gameLIBs)

simple:                       $(locBIN)/simple$(OBJSuf)
$(locBIN)/simple$(EXESuf):    $(locBIN)/simple$(OBJSuf) $(locBIN)/voxlap5$(OBJSuf) $(if_USEV5ASM) $(locBIN)/kplib$(OBJSuf) $(locBIN)/$(GFXdep)main1$(OBJSuf)
	$(LNK) $(LNKFLAGS)        $(locBIN)/simple$(OBJSuf) $(locBIN)/voxlap5$(OBJSuf) $(if_USEV5ASM) $(locBIN)/kplib$(OBJSuf) $(locBIN)/$(GFXdep)main1$(OBJSuf) $(simpleLIBs)

voxed:                        $(locBIN)/voxed$(OBJSuf)
$(locBIN)/voxed$(EXESuf):     $(locBIN)/voxed$(OBJSuf)  $(locBIN)/voxlap5$(OBJSuf) $(if_USEV5ASM) $(locBIN)/kplib$(OBJSuf) $(locBIN)/$(GFXdep)main2$(OBJSuf)
	$(LNK) $(LNKFLAGS)        $(locBIN)/voxed$(OBJSuf)  $(locBIN)/voxlap5$(OBJSuf) $(if_USEV5ASM) $(locBIN)/kplib$(OBJSuf) $(locBIN)/$(GFXdep)main2$(OBJSuf) $(voxedLIBs)
	
kwalk:                        $(locBIN)/kwalk$(OBJSuf)
$(locBIN)/kwalk$(EXESuf):     $(locBIN)/kwalk$(OBJSuf)  $(locBIN)/voxlap5$(OBJSuf) $(if_USEV5ASM) $(locBIN)/kplib$(OBJSuf) $(locBIN)/$(GFXdep)main2$(OBJSuf)
	$(LNK) $(LNKFLAGS)        $(locBIN)/kwalk$(OBJSuf)  $(locBIN)/voxlap5$(OBJSuf) $(if_USEV5ASM) $(locBIN)/kplib$(OBJSuf) $(locBIN)/$(GFXdep)main2$(OBJSuf) $(kwalkLIBs)


# binary object ($(OBJSuf)) targets

# Primary Objects
game_o:                            $(locSRC)/game.cpp
$(locBIN)/game$(OBJSuf):           $(locSRC)/game.cpp   $(locINC)/voxlap5.h $(locINC)/sysmain.h
	$(CXX) $(CXXFLAGS)             $(locSRC)/game.cpp
#   used to use /QIfist             

simple_o:                          $(locSRC)/simple.cpp
$(locBIN)/simple$(OBJSuf):         $(locSRC)/simple.cpp $(locINC)/voxlap5.h $(locINC)/sysmain.h
	$(CXX) $(CXXFLAGS)             $(locSRC)/simple.cpp
#   used to use /QIfist             

voxed_o:                           $(locSRC)/voxed.cpp
$(locBIN)/voxed$(OBJSuf):          $(locSRC)/voxed.cpp  $(locINC)/voxlap5.h $(locINC)/sysmain.h
	$(CXX) $(CXXFLAGS)             $(locSRC)/voxed.cpp

kwalk_o:                           $(locSRC)/kwalk.cpp
$(locBIN)/kwalk$(OBJSuf):          $(locSRC)/kwalk.cpp  $(locINC)/voxlap5.h $(locINC)/sysmain.h
	$(CXX) $(CXXFLAGS)             $(locSRC)/kwalk.cpp

# Secondary Objects                 
voxlap:                            $(locSRC)/voxlap5.cpp 
$(locBIN)/voxlap5$(OBJSuf):        $(locSRC)/voxlap5.cpp $(locINC)/voxlap5.h
	$(CXX) $(CXXFLAGS)             $(locSRC)/voxlap5.cpp

v5:                                $(locBIN)/v5$(OBJSuf)
$(locBIN)/v5$(OBJSuf):             $(locSRC)/v5.$(AsmName)
	$(AS)  $(AFLAGS)               $(locSRC)/v5.$(AsmName)

kplib:                             $(locBIN)/kplib$(OBJSuf)
$(locBIN)/kplib$(OBJSuf):          $(locSRC)/kplib.cpp
	$(CXX) $(CXXFLAGS)             $(locSRC)/kplib.cpp

main1:                             $(locBIN)/$(GFXdep)main1$(OBJSuf)
$(locBIN)/$(GFXdep)main1$(OBJSuf): $(locSRC)/$(GFXdep)main.cpp $(locINC)/sysmain.h
	$(CXX) $(CXXFLAGS)             $(locSRC)/$(GFXdep)main.cpp $(CMacroPre)USEKZ $(CMacroPre)ZOOM_TEST

main2:                             $(locBIN)/$(GFXdep)main2$(OBJSuf)
$(locBIN)/$(GFXdep)main2$(OBJSuf): $(locSRC)/$(GFXdep)main.cpp $(locINC)/sysmain.h
	$(CXX) $(CXXFLAGS)             $(locSRC)/$(GFXdep)main.cpp $(CMacroPre)NOSOUND

# Clearn Script
cleanall: clean
	cd "$(locBIN)"
	$(rm) "*"
clean:
	cd "$(locBIN)"
	$(rm) "*$(OBJSuf)"
	$(rm) "*$(EXESuf)"
