Voxlap engine notes by Ken Silverman (http://advsys.net/ken)
------------------------------------------------------------------------------
Introduction:

The Voxlap engine includes all the tools you need to create your own game
using a high-speed software voxel engine. I have included 4 applications:

   * GAME: An unfinished sample game
   * SIMPLE: Minimalist application code to help get you started.
   * VOXED: My voxel world editor
   * KWALK: My voxel model animation editor

I compiled the code with Microsoft Visual C/C++ 6.0. It should work with later
versions of Visual C. If you don't have a copy of MSVC, you can download the
VC 2003 Toolkit from Microsoft. (See note in requirements).

After unzipping, the first thing you should do is look at this file :) First,
I'll describe my way of compiling. I like to compile at the command line. To
set up your VC environment, run VCVARS32.BAT (as described at the top of
MAKESTUF.BAT) Then in MAKESTUF.BAT, check to make sure your directories are
set up correctly. Make sure your environment variables (lib and include)
point to the right path. You'll need them to point to the Visual C directories
as well as the directories from the DirectX SDK. Once this is done, you are
ready to run MAKESTUF.BAT. I put an in-line makefile at the top of each source
file. This is why there is no "makefile" in the directory. Cool trick, eh?

You can compile the examples from the VC GUI environment as well, but you'll
need to create your own project if you do so. Here are some hints on setting it
up to compile Voxlap:

   * Be sure to specify these non-default compiler settings:
        /J   default char type is unsigned
        /TP  compile all files as .cpp (I incorrectly use .c to save typing :)

   * Add these libraries to your linker settings:
        ddraw.lib
        dinput.lib
        dxguid.lib

   * To compile V5.ASM, you'll need an assembler. I use MASM. I'm not quite
        sure how to set this up in the VC environment. To make things easier,
        I included V5.OBJ. You can add this to your project if you have
        trouble assembling V5.ASM.

To compile with VC Toolkit 2003:
   * Change the linker option: /MD to /ML
        (see top of GAME.C, SIMPLE.C, VOXED.C, KWALK.C)

   * To fix the "MSVCRT.LIB not found" error, in WINMAIN.CPP, change:
      #define USETHREADS 20
         To:
      #define USETHREADS 0

-Ken S.

------------------------------------------------------------------------------
Requirements:

   CPU: Pentium III, Athlon, or above
   System memory: 128MB RAM or above
   Graphics card: Any old piece of junk should be fine :)
   OS: Microsoft Windows 98/ME/2K/XP
   DirectX 8.0 or above
   Compiler: Microsoft Visual C/C++ 6.0 or above.

   NOTE: WINMAIN.CPP uses SSE or 3DNow! instructions, so if you are compiling
   with VC6, you will need to install the Processor Pack (VCPP.ZIP). VCPP
   will only install over VC6 SP5 Professional edition, although I have had
   some success in fooling the installation program :) Fortunately, this is
   not an issue with VC7. You can also download a "free" version of the VC
   compiler: google for "c++ toolkit 2003". Unfortunately, it's missing a few
   files, such as WINDOWS.H and NMAKE.EXE, but they can be found elsewhere -
   either from the Platform SDK, or by googling for them.

------------------------------------------------------------------------------
File description:

MAKESTUF.BAT: Quick&dirty batch file showing how to compile things at the
   command prompt.
SIMPLE.C: Minimalist sample code showing how to interface Voxlap. Study this
   code before looking at GAME.C. It should also be easier to start a new
   project from this code.
GAME.*: A big ugly testing ground for many Voxlap features. Think of this
   as the example code for most functions in VOXLIB.TXT.

VOXLAP5.H: Include file for VOXLAP5.C, V5.ASM, KPLIB.C
VOXLAP5.C: Voxlap engine C routines
V5.*: Voxlap engine assembler routines
KPLIB.C: Ken's Picture LIBrary. Decoding for: PNG,JPG,GIF,TGA,BMP,PCX and ZIP.
   This library can be used independently from the engine. Voxlap depends on
   this library for various functions.
WINMAIN.CPP: Windows layer (setup screen, sound, input). This code is
   independent of Voxlap and can be replaced if you want to use your own
   windows layer, such as SDL. I've been maintaining WINMAIN.CPP for years, so
   it is rather stable now.
SYSMAIN.H: Include file for my windows layer (WINMAIN)
VOXDATA.ZIP: Game data. You do not need to extract this data to run GAME.EXE,
   SIMPLE.EXE, or VOXED.EXE. You will need to extract it to select models for
   loading in KWALK though.

VOXED.*: My VXL voxel editor. I made UNTITLED.VXL using VOXED. There are a lot
   of keys to learn - so have patience. As a convenience, you can view the
   help file (VOXEDHLP.TXT) inside the editor by pressing F1.
KWALK.*: An animation editor for .KV6 voxel sprites using .KFA animations.
   You'll need to extract files from VOXDATA.ZIP to see my example models.

VXLFORM.TXT: Description of the VXL format
KWALKHLP.TXT: Help file for KWALK
VOXEDHLP.TXT: Help file for VOXED
VOXLIB.TXT: Help file for VOXLAP5, KPLIB, and WINMAIN.
README.TXT: This help file

------------------------------------------------------------------------------
GAME.EXE information:

Command line options:
   /win                     Run in a window
   /3dn                     Force 3DNow! code. Crashes if not Athlon series.
   /sse                     Force Pentium III code. Crashes if not P3+/AthXP+!
   /sse2                    Force Pentium 4 code (ignored currently)
   /#x#x#                   Specify resolution. Max resolution is 1024x768

In-game controls:
   ESC                      Quit
   Arrows                   Move forward/backward/left/right
   Rt.Ctrl                  Move up
   KP0                      Move down
   Lt.Shift                 Move 16 times slower
   Rt.Shift                 Move 16 times faster
   1,2,3,4                  Select weapon
   Lt.Ctrl/Lt.Mouse button  Shoot weapon
   Space/Rt.Mouse button    Activate object (open/close door for example)
   `                        Change animation of female character. Try it! :)
   PrintScreen              Capture screen to uncompressed PNG file.
   Ctrl+PrintScreen         Capture panorama to uncompressed PNG file. Slow!
                               (compatible with KUBE/KUBEGL)
   KP-/+                    Decrease/increase sound volume
   F1                       Play a 3D looping sound (tries to load
                               "wav/airshoot.wav" but fails since not included
                               in VOXDATA.ZIP)
   F2                       Stop least recently started looping sound
   KP Enter                 Toggle DirectInput exclusive mouse
   F8                       Change video mode to random settings :)

In-game commands (press 'T' to enter typing mode):
   /fps                     Toggle fps meter
   /fallcheck               Toggle fall checking
   /sideshademode           Toggle cube face shading
   /faceshade=#,#,#,#,#,#   Specify shade offsets for each of the 6 faces.
                               Example: /faceshade=0,5,10,15,20,25
   /monst                   Toggle monsters active/inactive.
   /curvy                   Toggle KV6 sprite in front of view (cycles through
                               all 3 bending/twisting modes) Try it!
   /curvy=[KV6_file]        Toggle and select KV6 sprite in front of view.
                            Example: /curvy=caco
   /curvystp=#              Set stepsize for /curvy mode
   /light                   Add a light source (lightmode 2 only)
   /lightclear              Clear all light sources
   /lightmode=#             Specify lightmode (0=no lighting,1=normal
                               lighting, 2=point source lighting)
   /scandist=#              Set maximum radius to scan for VXL raycasting
   /fogcol=#                Set RGB fogcol. Example: /fogcol=0xff0000 sets fog
                               color to bright red. /fogcol=-1 to disable
   /kv6col=#                Set RGB color to blend with all KV6 sprites with
                               (default:0x808080)
   /anginc=#                Select raycast width (affects framerate/detail
                               significantly) Default:1, Typical range:1-8
   /vxlmip=#                Select raycast distance where VXL (world map)
                               MIP-mapping begins. (Default:192)
   /kv6mip=#                Select distance where KV6 (sprite) MIP-mapping
                               begins. (Default:128)
   /health=#                Set player health (Default:100)
   /numdynamite=#           Set number of dynamite (choose a high number :)
   /numjoystick=#           Set number of joysticks (choose a high number :)
   /showtarget=#            Toggle target in center of screen
   /showhealth=#            Toggle health display

Here are some effects that you should not overlook: Did you try them all? :)
   * Press '1','2','3','4' in game to select different weapons.
   * Press '`' in game to change the female character animation.
   * Press 't', then type '/curvy=caco' to see some cool effects!
   * Press 't', then type '/numdynamite=10000', and enjoy :)

------------------------------------------------------------------------------
Credits:

Ken Silverman: (http://advsys.net/ken)
   *.C, *.H, *.BAT, *.OBJ, *.EXE, *.TXT (Voxlap engine, tools, documentation)
   *.VXL, *.SXL, *.WAV, *.PNG (Most of the game content in VOXDATA.ZIP)
   *.KV6 (except ANASPLIT.KV6, which was drawn by local artist, Ana Lorenzo)
   *.KFA (I separated the limbs & animated the female character)

Tom Dobrowolski: (http://moonedit.com/tom)
   WINMAIN.CPP: This is 95% my code. Tom added a few small things to improve
      it over the years.
   TOONSKY.JPG: Tom originally made this sky for his own Voxlap game.
   KASCI9x12.PNG: Tom added Polish characters to the bottom of my font :)

   Tom has been a great help to me over the past few years. I thank him for
   his many ideas, help, and keeping me motivated to work on Voxlap for as
   long as I did. He has written many things for Voxlap in addition to the
   Voxlap Cave Demo and his untitled game from 2002 which was never released.

Jonathon Fowler: (http://www.jonof.id.au)
   KPLIB.C (1%): JonoF upgraded this library to be compatible with Linux for
      his Build port.

------------------------------------------------------------------------------
Voxlap Engine non-commercial license:

[1] Any derivative works based on Voxlap may be distributed as long as it is
    free of charge and through noncommercial means.

[2] You must give me proper credit. This line of text is sufficient:

       VOXLAP engine by Ken Silverman (http://advsys.net/ken)

    Make sure it is clearly visible somewhere in your archive.

[3] If you wish to release modified source code to your game, please add the
    following line to each source file changed:

   // This file has been modified from Ken Silverman's original release

[4] I am open to commercial applications based on Voxlap, however you must
    consult with me first to acquire a commercial license. Using Voxlap as a
    test platform or as an advertisement to another commercial game is
    commercial exploitation and prohibited without a commercial license.

------------------------------------------------------------------------------
Release history:

09/14/2005: Full voxlap source code released. Changes since 11/09/2004:
   * In-line makefile at top of GAME.C now includes VOXLAP5.C and V5.ASM.
   * KPLIB.C updated with Linux support (thanks to JonoF). NOTE: the other
        files are not Linux-compatible!
   * WINMAIN.CPP updated. For sound, link OLE32.LIB instead of DSOUND.LIB
   * VOXLAP5.H: kzaddstack and kzseek now return a value
   * Added GAME.EXE, VOXED.C, KWALK.C, VOXLAP5.C, and V5.ASM to distribution
   * SIMPLE.C: Added sample code showing how to render a simple voxel sprite
   * MAKESTUF.BAT: Updated to include VOXED and KWALK

11/09/2004: Voxlap library released.

------------------------------------------------------------------------------
Contact info:

If you do anything cool with Voxlap, I would love to hear about it! I prefer
that you contact me in either of these ways:

 1. Write about it on my personal forum ("advsys.net/ken" topic) at:
    http://www.jonof.id.au

 2. You can write me a private E-mail (address can be found at the bottom of
    my main page). Please use this for license requests.

If you have any questions or feature requests about other things in Voxlap, it
doesn't hurt to ask, although keep in mind that I haven't really worked on the
engine since 2003.

Ken's official website: http://advsys.net/ken
------------------------------------------------------------------------------
