# sub-directory macros
locSRC            =./source
locINC            =./include
locLIB            =./libraries
locBIN            =./binaries

# -----------------------------------
# Choices

# "Debug" for debug build, "Release" for release build
build            ?=Debug

# "win" for Windows 32-bit, "posix" for POSIX (32-bit for now)
PLATdep          ?=posix

# "1" to use v5.$(AS), "0" to not use
USEV5ASM         ?=1

# "nasm" for Netwide Assembler, "jwasm" for Japeth Open-Watcomm Assembler
AS                =nasm

# "win" for winmain (DirectX), "sdl" for sdlmain (libsdl)
GFX               =sdl

# C compiler must be able to accept gcc flags
CC               ?=gcc
CXX              ?=g++

# END Choices
# -----------------------------------

# -----------------------------------
# Build Flags



nasm_FLAGS        =-o $(@)           # Netwide Assembler (nasm)
jwasm_FLAGS       =-Fo $(@) -c -8    # Micrsoft Macro Assembler (masm)
AFLAGS            =$($(AS)_FLAGS) $($(PLATdep)_$(AS)_FLAGS)

CFLAGS            =-o $(@) -funsigned-char -m32 -mfpmath=sse -msse -m3dnow -ffast-math $(CC_$(build)) $($(GFX)_CFLAGS) $(Random_Macros) -I $(locINC)
CC_Debug          =-ggdb -Wall -Wextra -Wno-unused-variable -Wno-unused-parameter -Wno-unused-but-set-variable
CC_Release        =-O3

LDFLAGS           =-o $(@) -m32 $($(LNK)_FLAGS) $(LD_$(build))
LD_Debug          =-ggdb -Wall -Wextra -Wno-unused-variable -Wno-unused-parameter -Wno-unused-but-set-variable
LD_Release        =-O3
LDLIBS            =$($(GFX)_LDLIBS)

CCX              ?=$(CC)
CXXFLAGS          =$(CFLAGS)

OBJSuf            =$($(PLATdep)_OBJSuf)
EXESuf            =$($(PLATdep)_EXESuf)

# END Build Flags
# -----------------------------------

# -----------------------------------
# Platform

posix_OBJSuf      =.elf.o
posix_EXESuf      =

posix_nasm_FLAGS  =-f elf32
posix_jwasm_FLAGS =-elf

win_OBJSuf        =.obj
win_EXESuf        =.exe

win_nasm_FLAGS    =-f win -DWIN32 --prefix _
win_jwasm_FLAGS   =-coff -DWIN32

# END Platform
# -----------------------------------

# -----------------------------------
# Graphics Backend

sdl_CFLAGS        =`sdl-config --cflags`
sdl_LDLIBS        =`sdl-config --libs`

# END Platform
# -----------------------------------

# -----------------------------------
# Toggle Random Macros
ifeq "$(USEV5ASM)" "1"
if_USEV5ASM            =$(locBIN)/v5$(OBJSuf)
endif
Random_Macros          =-D USEV5ASM=$(USEV5ASM)
# END Toggle Random Macros
# -----------------------------------

Phony:                        all
all:                          voxlap slab6
voxlap:                       $(locBIN)/simple$(EXESuf) $(locBIN)/game$(EXESuf) $(locBIN)/voxed$(EXESuf) $(locBIN)/kwalk$(EXESuf)

# executable ($(EXESuf)) (meta)targets
simple:                    $(locBIN)/simple$(EXESuf)
$(locBIN)/simple$(EXESuf): $(locBIN)/simple$(OBJSuf) $(locBIN)/voxlap5$(OBJSuf) $(if_USEV5ASM) $(locBIN)/kplib$(OBJSuf) $(locBIN)/$(GFX)main1$(OBJSuf)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS)

game:                      $(locBIN)/game$(EXESuf)
$(locBIN)/game$(EXESuf):   $(locBIN)/game$(OBJSuf)   $(locBIN)/voxlap5$(OBJSuf) $(if_USEV5ASM) $(locBIN)/kplib$(OBJSuf) $(locBIN)/$(GFX)main1$(OBJSuf)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS)

voxed:                     $(locBIN)/voxed$(EXESuf)
$(locBIN)/voxed$(EXESuf):  $(locBIN)/voxed$(OBJSuf)  $(locBIN)/voxlap5$(OBJSuf) $(if_USEV5ASM) $(locBIN)/kplib$(OBJSuf) $(locBIN)/$(GFX)main2$(OBJSuf)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS)

kwalk:                     $(locBIN)/kwalk$(EXESuf)
$(locBIN)/kwalk$(EXESuf):  $(locBIN)/kwalk$(OBJSuf)  $(locBIN)/voxlap5$(OBJSuf) $(if_USEV5ASM) $(locBIN)/kplib$(OBJSuf) $(locBIN)/$(GFX)main2$(OBJSuf)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS)

slab6:                     $(locBIN)/slab6$(EXESuf)
$(locBIN)/slab6$(EXESuf):  $(locBIN)/slab6$(OBJSuf)  $(locBIN)/s6$(OBJSuf)                                              $(locBIN)/$(GFX)main2$(OBJSuf)
	$(CXX) $(LDFLAGS) $^ $(LDLIBS)

# binary object ($(OBJSuf)) targets

$(locBIN)/%$(OBJSuf):  $(locSRC)/%.cpp   $(locBIN)
	$(CXX) $(CXXFLAGS) -c $<

$(locBIN)/%$(OBJSuf):  $(locSRC)/%.c     $(locBIN)
	$(CC)  $(CFLAGS)   -c $<

$(locBIN)/%$(OBJSuf):  $(locSRC)/%.$(AS) $(locBIN)
	$(AS)  $(AFLAGS)      $<

$(locBIN)/%1$(OBJSuf): $(locSRC)/%.cpp   $(locBIN)
	$(CXX) $(CXXFLAGS) -c $< -D USEKZ -D ZOOM_TEST

$(locBIN)/%2$(OBJSuf): $(locSRC)/%.cpp   $(locBIN)
	$(CXX) $(CXXFLAGS) -c $< -D NOSOUND

$(locBIN)/%1$(OBJSuf): $(locSRC)/%.c     $(locBIN)
	$(CC)  $(CFLAGS)   -c $< -D USEKZ -D ZOOM_TEST

$(locBIN)/%2$(OBJSuf): $(locSRC)/%.c     $(locBIN)
	$(CC)  $(CFLAGS)   -c $< -D NOSOUND

$(locBIN):
	mkdir -p $(locBIN)

# Clearn Script
cleanall: clean
	rm "$(locBIN)/*"

clean:
	rm "$(locBIN)/*$(OBJSuf)"
	rm "$(locBIN)/*$(EXESuf)"
