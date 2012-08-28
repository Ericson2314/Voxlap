=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
Porting Notes by Ericson2314
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

Making Ken Silverman's "Voxlap" voxel graphics engine run on any platform
with SDL and C++.

------------------------------------------------------------------------------
Build Prerequisites:

Build Tools:
	Option 1
		MSVS utilities (nmake, ml, cl)
		Windows SDK (libs)
	Option 2
		MinGW/GCC
		Nasm in %MinGW%/bin
Graphics
	Option 1
		DirectX SDK (libs)
	Option 2
		SDL DEV-MinGW/GCC and RUNTIME (libs)

------------------------------------------------------------------------------
Quick Porting Progress Overview :

* -- Done | $ -- In Progress | # -- not started

1. Compile with MS Visual Studio 2010 utilities, and June 2010 DirectX sdk (*)
	* Makefile mead from *.c headers
	* Old DirectX SDK *.LIBs included
	* v5.asm modified as per
	  http://natural-satellite.com/2011/06/19/visual-studio-and-voxlap/
2. Use cross-platform assembler: (*)
	$ make v5.asm compile in NASM. [I give up on this]
	  Thanks, http://www.drpaulcarter.com/pcasm/
	  and http://www.devoresoftware.com/nomyso/
	* make v5.asm compile in JWASM. [I did this instead.]
	  JWASM uses MASM syntax, making this trivially easy.
	  Thanks person on ##ASM on freenode for giving me the idea.
		* Links with MSVC too
	# make macro to support both cdecl and stdcall naming conventions.
3. Compile with GCC/MinGW: ($)
	* Update makefile for MinGW
		* Move all program names, directories, and flags to macros
		* Add Corosponing MinGW macro defs.
		* Makefile is now very modular
	* Convert all C++ to compile as both C and C++
	  This allows me to compare mscv and gcc versions more dodeirectly, as I the
	  ABI for C is more consistant between the two toolchains.
		# Bug: game in C does not allow weapons to be fired, but that shouldn't
		  be a voxlap issue.
	* Convert inline assembly to work with gcc
	  GCC now supports ".intel_syntax noprefix" making this much easier
		# make local variables work
		  for now, with intel syntax GCC treats all lables as global
	* Compiles
	# Link with ld
		# Fix Bugs
4. Compile on *NIX (Linux in my case) ($)
	* Compiles (including sdlmain
	# link
Alternative step 2:
2. Compiler on MSVC with minimal asm ($)
	$ Compile game without v5.asm (the only external assembly)
	$ Remove inline asm to simplify toolchain ineroptability.
Misc
	* Heirarchy implemented. (Makefile ready; code needs to be adjusted so bin
	  and data don't need to be in same folder.)
	* .c to .cpp (until it all works as true C), accuracy is great.
	# PreMake? CMake? autobuild?



------------------------------------------------------------------------------
Introduction:

So, the basic idea is to get Ken Silverman's Voxlap voxel graphics engine to
run on just about any platform that supports C++ and SDL (DirectX is
still kept operational for windows). This all boils down to 2 fundamental
things: getting the code to compile in GCC, and making sure that now windows
libraries are in use.


Compiler issues:

The compiler part is pretty easy, except for one thing: the assembly code.
Voxlap uses both a single file of routines in assembly, and copious
amounts of inline assembly within it's C++ files. Now, most of this assembly
should be perfectly portable, but every compiler collection has a different
format for both external and internal assembly.

I say format, because converting from one to another is a fairly trivial task,
but not quite trivial enough where I can confidently do it with my limited
assembly knowledge in an error-free manor. I have tried various scripts to
convert assembly (inline and external), but the bottom line is that I don't
really trust them. Most significantly, v5.asm's nasm port won't work with MSVC
(though it should), so why should I trust it with GCC? Dealing with the inline
assembly is even simpler, but the conversion tools are correspondingly more
primitive, and I can't test them anyways until v5.asm works.

Therefore I HAD opted to dispense with assembly code altogether. Voxlap5 (the
main Voxlap code) is littered with commented out replacement C for various
assembly routines. Not all of it works as well as the assembly -- I think Ken
mainly used it as a guide for writing the assembly and didn’t debug it as the
program evolved -- but it’s certainly better than nothing. As to the external
assembly, there is a macro that’s supposed to let the program run without it,
but it leaves in a few missing linkages -- it was probably also ignored as the
program evolved. For better or worse, the inline assembly and external
assembly often relies on each other.  On one hand, this means taking out one
can help get rid of the other. On the other hand, it means it’s very difficult
to replace the assembly incrementally and debug as you go.

I learned of another assembly, JWASM, on IRC which is both cross-platform and
Supports NASM syntax as close as possible. This gave me the opportunity to go
back to my original plan of porting all the assembly. This of course came with
the added benefit of leaving the original program as intact as possible, and
utilizing the inline assembly that was admittedly still faster than the C. For
ARM ports it will still have to go but this seems to be the easier solution
for now. Now my plan is to work along both methods. I don't know whether
converting or removing inline assembly will get to a finished port faster, so
i've made a git branch for each method to work on both concurrently.

I was able to quickly fix the few syntax errors that existed, and get a
working Game.exe with MSVC and JWASM. But JWASM in itself did nothing to
prevent the link errors with MSVC external assembly together is often made
easier. The cacophony of link errors, was a bit hard to decipher, and in order
to better separate assembly-caused linking errors from other issues, I decided
to get the whole thing to be compiled as C. C, unlike C++, has no name
mangling, and in addition C-derived objects files made by different compilers
have a better chance of linking correctly (especially on windows). I figured
this would open up new doors testing.


Library issues :

I can't really dive into this until the former step is complete, but it seems
that again a good bit of the work is already done. Voxlap itself is a
surprisingly self-contained beast, primary because it’s a software render so
it only uses DirectX to copy frame buffers and other trivial stuff. The
backend it currently uses Winmain, is not Voxlap-Specific, but rather used by
a couple of Ken’s things (maybe even the build engine).

On the flip side, the example game, and probably other games based around
Voxlap don’t just use voxlap’s header, but also directly use Sysmain.h,
Winmain’s interface. So to have a genuinely useful Voxlap port, seemingly
unused features of Sysmain.h must also be ported.

Thankfully, available on Ken’s site is SDLmain, a non-Voxlap-Specific port of
Winmain. I’m not sure how complete it is, but I’d hope it has the most basic
stuff that Voxlap itself requires available. As to the example game and other
programs that use Voxlap, I’m not so hopeful. Some of Sysmain.h’s routines are
very DirectX specific (like “startdirectdraw”) and even if they are “ported”,
probably don’t cause some idiosyncrasy that the games are expecting. We’ll see
once the assembly is dealt with.

Lastly, SDLmain is GCC only, and Winmain is MSVC only.

------------------------------------------------------------------------------
What Next:

A true port should change as little of the old program as possible in order to
maintain maximal compatibility. At the same time, porting is a perfect time to
introduce deep architectural changes into a program. I believe a few obvious
improvements that would not effect existing programs ability to interface with
Voxlap to much extent.

Voxlap is a library, nothing more, nothing less. As such it should be able to
be both statically and dynamically linked with another program. Currently, only
static linking is tested to work, but with a couple trivial changes, dynamic
linking should be too.

Voxlap is C++, but in name only. In practice it contains little-to-none
Object-Oriented components, and only a few C++ idiosyncrasies prevent it from
being compiled as C. In fact, it even contains a couple of preprocessor
conditionals to test for C or C++ completion. Ideally it should fully support
being compiled as C or C++ in order to work in many situations.

------------------------------------------------------------------------------
License:

Based on my concept of a “true port” I should keep the license the same as the
original: basically attribution + non-commercial only without Ken Silverman’s
explicit permission. But my inner Free Software evangelist tells me not to. So
here is the revised license for those wishing to use my modifications to Ken’s
work:

Option 1: LGPL 2 or later, non-commercial only.

Option 1: LGPL 2 or later, commercial with Ken’s permission.

Option 3: Any other scenario allowed under Ken’s license, with my explicit
          permission.

I tried to write this to be compatible with Ken’s license, but of course I
cannot modify his license, so if a conflict arises, ask me for details.

------------------------------------------------------------------------------
Acknolodgements:

Freenode IRC network: It about as indispensable a resource as exists for
programming these days. #C and especially #asm in particular for this project.
Many people on the network offered excellent advice, and copious amounts of
patience and time, but I'd especially like to thank:
	- vulture: (on #asm) for helping me get v5 to change memory permissions on
	  Linux for the sake of self-modifying code.

=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
[Original] Voxlap engine notes by Ken Silverman (http://advsys.net/ken)
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
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
