#pragma once
	//Screen related functions:
extern void voxsetframebuffer (long, long, long, long);
extern void setsideshades (char, char, char, char, char, char);
extern void setcamera (dpoint3d *, dpoint3d *, dpoint3d *, dpoint3d *, float, float, float);
extern void opticast ();
extern void drawpoint2d (long, long, long);
extern void drawpoint3d (float, float, float, long);
extern void drawline2d (float, float, float, float, long);
extern void drawline3d (float, float, float, float, float, float, long);
extern long project2d (float, float, float, float *, float *, float *);
extern void drawspherefill (float, float, float, float, long);
extern void drawpicinquad (long, long, long, long, long, long, long, long, float, float, float, float, float, float, float, float);
extern void drawpolyquad (long, long, long, long, float, float, float, float, float, float, float, float, float, float, float, float, float, float, float, float, float, float);
extern void print4x6 (long, long, long, long, const char *, ...);
extern void print6x8 (long, long, long, long, const char *, ...);
extern void drawtile (long, long, long, long, long, long, long, long, long, long, long, long);
extern long screencapture32bit (const char *);
extern long surroundcapture32bit (dpoint3d *, const char *, long);