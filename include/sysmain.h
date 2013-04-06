/**************************************************************************************************
 * WINMAIN.CPP & SYSMAIN.H                                                                        *
 *                                                                                                *
 * Windows layer code written by Ken Silverman (http://advsys.net/ken) (1997-2009)                *
 * Additional modifications by Tom Dobrowolski (http://ged.ax.pl/~tomkh)                          *
 * You may use this code for non-commercial purposes as long as credit is maintained.             *
 **************************************************************************************************/

// This file has been modified from Ken Silverman's original release

#pragma once

#if !defined(SYSMAIN_C) && !defined(__cplusplus) && !defined(SYSMAIN)
#error "Cannot link C frontend to C++ Backend"
#endif


#if defined(SYSMAIN_C) && defined(__cplusplus)
	extern "C" {
	#define EXTERN_SYSMAIN extern "C"
#else
	#define EXTERN_SYSMAIN extern
#endif

	//System specific:
#ifdef _WIN32
	#define NOMAXMIN
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	extern HWND ghwnd;
	extern HINSTANCE ghinst;
	extern HDC startdc ();
	extern void stopdc ();
	extern void ddflip2gdi ();
	extern void setalwaysactive (long);
	extern void smartsleep (long timeoutms);
	extern long canrender ();
	extern long ddrawuseemulation;
	extern long ddrawdebugmode; // -1 = off, old ddrawuseemulation = on
	extern void debugdirectdraw (); //toggle debug mode
	extern long (CALLBACK *peekwindowproc)(HWND,UINT,WPARAM,LPARAM);
#else
	#pragma pack(push,1)
	typedef struct { unsigned char peRed, peGreen, peBlue, peFlags; } PALETTEENTRY;
	#pragma pack(pop)
	#define MAX_PATH 260
#endif
extern long cputype;
extern void setacquire (long mouse, long kbd);


	//Program Flow:
extern const char *prognam;
extern long progresiz;
long initapp (long argc, char **argv);
void doframe ();
extern void quitloop ();
void uninitapp ();

	//Video:
typedef struct { long x, y; char c, r0, g0, b0, a0, rn, gn, bn, an; } validmodetype;
extern validmodetype curvidmodeinfo;
extern long xres, yres, colbits, fullscreen, maxpages;
extern PALETTEENTRY pal[256];
extern long startdirectdraw (long *, long *, long *, long *);
extern void stopdirectdraw ();
extern void nextpage ();
extern long clearscreen (long fillcolor);
extern void updatepalette (long start, long danum);
extern long getvalidmodelist (validmodetype **validmodelist);
extern long changeres (long, long, long, long);

#ifndef NOSOUND
extern void playsound (const char *, long, float, void *, long);
extern void playsoundupdate (void *, void *);
extern void setears3d (float, float, float, float, float, float, float, float, float);
extern void setvolume (long);
extern long umixerstart (void (*mixfunc)(void *, long), long, long, long);
extern void umixerkill (long);
extern void umixerbreathe ();
#endif

#ifndef NOINPUT
	//Keyboard:
extern char keystatus[256];     //bit0=1:down
extern char ext_keystatus[256]; //bit0=1:down,bit1=1:was down
extern void readkeyboard ();
extern long keyread ();         //similar to kbhit()&getch() combined

	//Mouse:
extern char ext_mbstatus[8];    //bit0=1:down,bit1=1:was down, bits6&7=mwheel
extern long ext_mwheel;
extern long mousmoth;           //1:mouse smoothing (default), 0 otherwise
extern float mousper;           //Estimated mouse interrupt rate
extern void readmouse (float *, float *, long *);
#if !defined(SYSMAIN_C) && defined(__cplusplus)
extern void readmouse (float *, float *, float *, long *);
#endif
extern long ismouseout (long, long);
extern void setmouseout (void (*)(long, long), long, long);
#endif //NOINPUT

	//Timer:
extern void readklock (double *);

	//code to change memory permissions now moved here because used in slab6 and voxlap
extern void code_rwx_unlock ( void *, void *);

#if defined(SYSMAIN_C) && defined(__cplusplus)
}
#endif
