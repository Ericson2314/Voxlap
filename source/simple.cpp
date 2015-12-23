﻿// VOXLAP engine by Ken Silverman (http://advsys.net/ken): Example skeleton program
// This file has been modified from Ken Silverman's original release


/**
 *Function and variable declarations for winmain.cpp, dosmain.c, or sdlmain.c
 */
//#define SYSMAIN_C //if sysmain is compiled as C
#include "../include/sysmain.h"

/**
 * I have all my function and structure declarations in here, so if you
 * include this, you will be sure that you're calling my functions with the
 * correct parameters.
 */
//#define VOXLAP_C  //if voxlap5 is compiled as C
#include "../include/voxlap5.h"

#include "../include/porthacks.h"

/**
 * The dpoint3d structure is defined in VOXLAP5.H as "double x, y, z".
 * 	ipos           : specifies (x,y,z) position
 * 	istr,ihei,ifor : specifies orientation, where:
 * 		istr is the RIGHT unit vector ("strafe" direction)
 * 		ihei is the DOWN unit vector ("height" direction)
 * 		ifor is the FORWARD unit vector ("forward" direction)
 * 	I like to put the letter "i" in front of these variables because "i"
 * 	is pronounced like "eye". Cute, huh?
 */
dpoint3d ipos, istr, ihei, ifor;


/**
 * Example voxel sprite structure declaration. The sprite structure holds
 * its position, orientation (3x3 matrix), flags and timing (for animation)
 * and a pointer to the voxel data (voxnum).
 */
vx5sprite desklamp;

/**
 * I have all the system dependent code (such as graphics initialization,
 * page flipping, keyboard, mouse, timer, and file finding) inside
 * WINMAIN/DOSMAIN. Since operating systems handle the program entry point
 * differently (such as "main" vs. "WinMain"), I put the program entry
 * point in WINMAIN/DOSMAIN. The first thing they do is call initapp().
 *
 * @param The parameters are formatted exactly like ANSI C's "main" function.
 
 * @param The parameters are formatted exactly like ANSI C's "main" function.
 */
long initapp (long argc, char **argv)
{
	/**
	 * As soon as you return from your initapp function, WINMAIN/DOSMAIN will
	 * set the video mode. You MUST set these 4 variables (xres,yres,colbits,
	 * fullscreen) somewhere inside your initapp function.
	 */
	xres = 640; yres = 480; colbits = 32; fullscreen = 0;

	/*	
	* Call this before you use any other functions from VOXLAP5.H. I allocate
	* memory and initialize lookup tables in this function.
	*/
	initvoxlap();
	
	/**
	 * This call is not mandatory, but if you want your game to support files
	 * stored inside a .ZIP file, then you must call this before any functions
	 * that load those file types. You can use multiple .ZIP files for user
	 * patches. It gives highest priority to the last .ZIP file passed to this
	 * function. Any stand-alone files (not in a .ZIP) but with matching path&
	 * filename will have the highest priority.
	 */
	kzaddstack("voxdata.zip");
	
	/**
	 * Call this function to load a map from file into memory. The voxlap engine
	 * handles the map structure in its own memory. You are responsible for
	 * dealing with the starting position and orientation. Loadvxl parameters:
	 * char *mapfilename : voxel map filename
	 * @param dpoint3d * : pointer to starting position
	 * @param dpoint3d * : pointer to starting RIGHT unit vector
	 * @param dpoint3d * : pointer to starting DOWN unit vector
	 * @param dpoint3d * : pointer to starting FORWARD unit vector
	 */
	loadvxl("vxl/untitled.vxl",&ipos,&istr,&ihei,&ifor);
	
	/**
	 * Getkv6 loads the voxel object (desklamp.kv6) into memory and returns a
	 * pointer to its kv6data structure. The voxel data is cached, so if you call
	 * getkv6 again, it will share memory and return a pointer to the same data.
	 */
	desklamp.voxnum = getkv6("kv6\\desklamp.kv6");
	
	/**
	 * Place the lamp's position at a spot in the middle of the VXL map. Usually,
	 * you would load the sprite's location from the SXL file. This is just for
	 * demonstration.
	 */
	desklamp.p.x = 652; desklamp.p.y = 620; desklamp.p.z = 103.5;
	
	/**
	 * Select the lamp's orientation. I use an identity matrix here, scaled by
	 * 0.4 to make it 2.5 times smaller.
	 */
	desklamp.s.x = .4;  desklamp.s.y = 0;   desklamp.s.z = 0;
	desklamp.h.x = 0;   desklamp.h.y = .4;  desklamp.h.z = 0;
	desklamp.f.x = 0;   desklamp.f.y = 0;   desklamp.f.z = .4;
	
	/**
	 * Kfatim and okfatim are used for animation. See the description of vx5sprite
	 * in VOXLAP5.H for a description of flags. Flags=0 is fine for most purposes.
	 */
	desklamp.kfatim = 0; desklamp.okfatim = 0; desklamp.flags = 0;
	
	/**
	 * @return Return a 0 to tell WINMAIN/DOSMAIN that everything's ok and continue.
	 * 	If you return a -1 here, WINMAIN/DOSMAIN will clean all buffers and exit
	 * 	immediately. This is useful when you have a critical error like an
	 * 	invalid graphics mode or map file not found.
	 */
	return(0);
}

/**
 * Once you return from initapp, WINMAIN/DOSMAIN will call this function
 * continuously. WINMAIN handles its windows message loop (PeekMessage,etc.)
 * between doframe() calls. You should render exactly 1 frame in here,
 * process movement/network code, and then return back to WINMAIN/DOSMAIN so
 * it can update the status of things.
 */
void doframe ()
{
	long frameptr, pitch, xdim, ydim;
	
	/**
	 * This function obtains and locks an offscreen surface for you to draw to.
	 * The 4 parameters fully specify a frame buffer:
	 * 
	 * @param frameptr 32-bit address pointing to the top-left corner of the frame
	 * 	You can cast this as (long *)frameptr but but be careful with
	 * 	the pitch when you do this, since pitch is in BYTES.
	 * @param pitch the number of BYTES per scan line (NOTE: bytes, not pixels)
	 * @param xdim the horizontal resolution of the frame in PIXELS
	 * @param ydim the vertical resolution of the frame in PIXELS
	 * 
	 * This example shows how you could draw a green pixel at coordinate (30,20):
	 * 	*(long *)((30<<2)+20*pitch+frameptr) = 0x00ff00;
	 * 
	 * WARNING: Do NOT call hardware accelerator functions between startdirectdraw
	 * 	and stopdirectdraw since video memory will be locked between these 2
	 * 	calls. If you forget this rule, then your system may freeze.
	 */
	startdirectdraw(&frameptr,&pitch,&xdim,&ydim);
	
	/**
	 * Since voxlap is currently a software renderer and I don't have any system
	 * dependent code in it, you must provide it with the frame buffer. You MUST
	 * call this once per frame, AFTER startdirectdraw(), but BEFORE any functions
	 * that access the frame buffer.
	 */
	voxsetframebuffer(frameptr,pitch,xdim,ydim);
	
	/**
	 * Call this to set the current camera orientation. This information is used
	 * for future calls, such as opticast, drawsprite, spherefill, etc...
	 * 
	 * @param ydim*.5 The 5th & 6th parameters define the center of the screen projection - This
     * 	is the point on the screen that the <ipos + ifor*t> vector intersects.
	 * 
	 * @param xdim*.5 The last parameter is the focal length - use it to control zoom. If you
	 * 	want a 90 degree field of view (left to right side of screen), then
	 * 	set it to half of the screen's width: (xdim*.5).
	 */
	setcamera(&ipos,&istr,&ihei,&ifor,xdim*.5,ydim*.5,xdim*.5);
	
	/**
	 * This is the where the screen actually gets drawn! Assuming you loaded a map
	 * into memory, it uses the current camera position & orientation.
	 */
	opticast();
	
	/**
	 * Renders the voxel sprite, using the data from the desklamp structure.
	 * Drawsprite should be called after opticast and before stopdirectdraw. There
	 * is no need to sort sprites since Voxlap uses a depth buffer.
	 */
	drawsprite(&desklamp);
	
	/**
	 * If you called startdirectdraw, then you MUST call this before flipping
	 * pages. Once you do this, video memory is unlocked and you're free to draw
	 * to the frame buffer using hardware accelerator functions or GDI, etc...
	 */
	stopdirectdraw();

	/**
	 * Call this WINMAIN/DOSMAIN function when you're finished drawing to the
	 * frame. This function flips video pages for double-buffering.
	 */
	nextpage();
	
	/**
	 * The keystatus array is defined in SYSMAIN.H, and it tells you the status of
	 * every key on the keyboard. 0 means the key is up and 1 means the key is
	 * down. You MUST call readkeyboard() to update the keystatus array for
	 * WINMAIN to work properly.
	 * The scancode for ESC is 1. You tell WINMAIN/DOSMAIN to stop calling doframe
	 * by calling the quitloop() function.
	 */
	readkeyboard(); if (keystatus[1]) quitloop();
}

/**
 * When you call quitloop(), WINMAIN/DOSMAIN stops calling doframe, and it
 * calls uninitapp once before exiting. Call uninitvoxlap() in here so the
 * voxlap engine can free any buffers that it created.
 */
void uninitapp () { freekv6(desklamp.voxnum); /*uninitvoxlap();*/ kzuninit(); }