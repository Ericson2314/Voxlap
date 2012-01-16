# XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX=BEGIN MACROS=XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX

# adding inbin directory to paths
PATH                   =$(iBN);$(PATH)
INCLUDE                =$(iBN);$(INCLUDE)
LIB                    =$(iBN);$(LIB)

# sub-directory macros
SRC                    =$(MAKEDIR)/src
iBN                    =$(MAKEDIR)/inbin
oBN                    =$(MAKEDIR)/outbin

# DirectX directories (OLD: SETUP DirectX 8.x environment for command line. Fix directory name if incorrect!)
# winINCLUDE             =/dxsdk/INCLUDE
winLIB                 =C:/Program Files/Microsoft DirectX SDK (June 2010)/LIB/x86
# Objects
winOBJ                 =ddraw.lib dinput.lib dxguid.lib


# SDL directories WINDOWS (WINDOWS ONLY)
sdlINCLUDE             =C:/Program Files/Microsoft Visual Studio 10.0/VC/lib/
sdlLIB                 =C:/Program Files/Microsoft Visual Studio 10.0/VC/include/SDL
# Objects (WINDOWS ONLY)
sdlOBJ                 =sdl.lib sdlmain.lib opengl32.lib glu32.lib
# SDL root dir (UNIX, WINDOWS)


# Compiler/assembler/linker directories
VSTUDIO                =
MinGW                  =C:/MinGW/bin

# Per-Program Flags
masmFLAGS              =/Fo$(@R) /c /coff
nasmFLAGS              =-o $(@) -f win32

clFLAGS                =/Fo$(@R) /c /J
gccFLAGS               =-o $(@) -c -funsigned-char `sdl-config --cflags`

clMacroPre             =/D
gccMacroPre            =-D" "

linkFLAGS              =/out:$(@)
ldFLAGS                =-o $(@) `sdl-config --libs`
# [XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX]
# [XXXXXXXXXXXXXXXXXXXXXXX=CHANGE BELOW FOR UNIX COMPILING=XXXXXXXXXXXXXXXXXXXXXXXX]
# [XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX]
# Compiler Choices
AsmName                =masm #winSDK: =masm #GCC: =nasm _NOT gas_
CPP                    =cl   #winSDK: =cl   #GCC: =gcc
LNK                    =link #winSDK: =LNK  #GCC: =ld

#  Graphics Codepath
GFXdep                 =win #DirectX: =win SDL: =sdl
# [XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX]
# [XXXXXXXXXXXXXXXXXXXXXXX=CHANGE ABOVE FOR UNIX COMPILING=XXXXXXXXXXXXXXXXXXXXXXXX]
# [XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX]

# Conditionl Hacks
!IF "$(AsmName)"=="masm"
AS=ml
AFLAGS                 =$(masmFLAGS) #$($(AsmName)FLAGS)
ifVSTUDIO              =true
!ELSE
AS=$(AsmName)
!IF "$(AsmName)"=="nasm"
AFLAGS                 =$(nasmFLAGS) #$($(AsmName)FLAGS)
ifMinGW                =true
!ENDIF
!ENDIF

!IF "$(CPP)"=="cl"
CPPFLAGS               =$(clFLAGS) #$($(CPP)FLAGS)
CMacroPre              =$(clMacroPre) #$($(CPP)MacroPre)
ifVSTUDIO              =true
!ELSEIF "$(CPP)"=="gcc"
CPPFLAGS               =$(gccFLAGS) #$($(CPP)FLAGS)
CMacroPre              =$(gccMacroPre) #$($(CPP)MacroPre)
ifMinGW                =true
!ENDIF

!IF "$(LNK)"=="link"
LNKFLAGS               =$(linkFLAGS) #$($(LNK)FLAGS)
ifVSTUDIO              =true
!ELSEIF "$(LNK)"=="ld"
LNKFLAGS               =$(ldFLAGS) #$($(LNK)FLAGS)
ifMinGW                =true
!ENDIF

!IFDEF VSTUDIO
PATH                   =$(VSTUDIO);$(PATH)
!ENDIF
!IFDEF MinGW
PATH                   =$(MinGW);$(PATH)
!ENDIF

!IF "$(GFXdep)"=="win"
INCLUDE                =$(winINCLUDE);$(INCLUDE) #$($(GFXdep)INCLUDE);$(INCLUDE)
LIB                    =$(winLIB);$(LIB) #$($(GFXdep)LIB);$(LIB)
OBJ                    =$(winOBJ) #$($(GFXdep)OBJ)
!ELSEIF "$(GFXdep)"=="sdl"
INCLUDE                =$(sdlINCLUDE);$(INCLUDE) #$($(GFXdep)INCLUDE);$(INCLUDE)
LIB                    =$(sdlLIB);$(LIB) #$($(GFXdep)LIB);$(LIB)
GFXOBJ                 =$(sdlOBJ) #$($(GFXdep)OBJ)
!ENDIF

# XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX=END MACROS=XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX