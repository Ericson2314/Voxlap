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
AS                =jwasm

# C compiler must be able to accept gcc flags
CC               ?=gcc
ifeq "$(CC)" "cc"
CC                =gcc
endif

# END Choices
# -----------------------------------

# -----------------------------------
# Build Flags



nasm_FLAGS        =-o $(@)           # Netwide Assembler (nasm)
jwasm_FLAGS       =-Fo $(@) -c -8    # Micrsoft Macro Assembler (masm)
AFLAGS            =$($(AS)_FLAGS) $($(PLATdep)_$(AS)_FLAGS)

CFLAGS            =-o $(@) -funsigned-char -msse -Wattributes $(CC_$(build)) `sdl-config --cflags` $(Random_Macros)
CC_Debug          =-ggdb
CC_Release        =-O3

LDFLAGS           =-o $(@) $($(LNK)_FLAGS)                    $(LD_$(build))
LD_Debug          =-ggdb
LD_Release        =-O3
LDLIBS            =`sdl-config --libs`

CCX               =$(CC)
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

win_nasm_FLAGS    =-f win -DWIN32
win_jwasm_FLAGS   =-coff -DWIN32

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

rm                     =rm -f

Phony:                     default
default:                   $(locBIN)/game$(EXESuf) $(locBIN)/simple$(EXESuf) $(locBIN)/voxed$(EXESuf) $(locBIN)/kwalk$(EXESuf)

# executable ($(EXESuf)) (meta)targets
game:                      $(locBIN)/game$(EXESuf)
$(locBIN)/game$(EXESuf):   $(locBIN)/game$(OBJSuf)   $(locBIN)/voxlap5$(OBJSuf) $(if_USEV5ASM) $(locBIN)/kplib$(OBJSuf) $(locBIN)/sdlmain1$(OBJSuf)
	$(CC) $(LDFLAGS) $^ $(LDLIBS)

simple:                    $(locBIN)/simple$(OBJSuf)
$(locBIN)/simple$(EXESuf): $(locBIN)/simple$(OBJSuf) $(locBIN)/voxlap5$(OBJSuf) $(if_USEV5ASM) $(locBIN)/kplib$(OBJSuf) $(locBIN)/sdlmain1$(OBJSuf)
	$(CC) $(LDFLAGS) $^ $(LDLIBS)

voxed:                     $(locBIN)/voxed$(OBJSuf)
$(locBIN)/voxed$(EXESuf):  $(locBIN)/voxed$(OBJSuf)  $(locBIN)/voxlap5$(OBJSuf) $(if_USEV5ASM) $(locBIN)/kplib$(OBJSuf) $(locBIN)/sdlmain2$(OBJSuf)
	$(CC) $(LDFLAGS) $^ $(LDLIBS)
	
kwalk:                     $(locBIN)/kwalk$(OBJSuf)
$(locBIN)/kwalk$(EXESuf):  $(locBIN)/kwalk$(OBJSuf)  $(locBIN)/voxlap5$(OBJSuf) $(if_USEV5ASM) $(locBIN)/kplib$(OBJSuf) $(locBIN)/sdlmain2$(OBJSuf)
	$(CC) $(LDFLAGS) $^ $(LDLIBS)

# binary object ($(OBJSuf)) targets

$(locBIN)/%$(OBJSuf):  $(locSRC)/%.cpp
	$(CXX) $(CXXFLAGS) -c $<

$(locBIN)/%$(OBJSuf):  $(locSRC)/%.c
	$(CC)  $(CFLAGS)   -c $<

$(locBIN)/%$(OBJSuf):  $(locSRC)/%.$(AS)
	$(AS)  $(AFLAGS)      $<

$(locBIN)/%1$(OBJSuf): $(locSRC)/%.cpp
	$(CC)  $(CFLAGS)   -c $< -D USEKZ -D ZOOM_TEST

$(locBIN)/%2$(OBJSuf): $(locSRC)/%.cpp
	$(CC)  $(CFLAGS)   -c $< -D NOSOUND

$(locBIN)/%1$(OBJSuf): $(locSRC)/%.c
	$(CC)  $(CFLAGS)   -c $< -D USEKZ -D ZOOM_TEST

$(locBIN)/%2$(OBJSuf): $(locSRC)/%.c
	$(CC)  $(CFLAGS)   -c $< -D NOSOUND

# Clearn Script
cleanall: clean
	rm "$(locBIN)/*"

clean:
	rm "$(locBIN)/*$(OBJSuf)"
	rm "$(locBIN)/*$(EXESuf)"
