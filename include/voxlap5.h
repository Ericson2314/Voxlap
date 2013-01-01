// VOXLAP engine by Ken Silverman (http://advsys.net/ken)
// This file has been modified from Ken Silverman's original release

#pragma once

#define MAXXDIM 1024
#define MAXYDIM 768
#define PI 3.141592653589793
#define VSID 1024   //Maximum .VXL dimensions in both x & y direction
#define MAXZDIM 256 //Maximum .VXL dimensions in z direction (height)

#pragma pack(push,1)

#if !defined(VOXLAP_C) && !defined(__cplusplus) && !defined(VOXLAP5)
#error "Cannot link C frontend to C++ Backend"
#endif


#if defined(VOXLAP_C) && defined(__cplusplus)
	extern "C" {
	#define EXTERN_VOXLAP extern "C"
#else
	#define EXTERN_VOXLAP extern
#endif

typedef struct { long x, y, z; } lpoint3d;
typedef struct { float x, y, z; } point3d;
typedef union
{
	struct { float x, y, z, z2; };
#ifdef __GNUC__
	float vec __attribute__ ((vector_size (4)));
#endif
#ifdef _MSC_VER
	__m128 vec;
#endif
} point4d;
typedef struct { double x, y, z; } dpoint3d;

	//Sprite structures:
typedef struct { long col; unsigned short z; char vis, dir; } kv6voxtype;

typedef struct kv6data
{
	long leng, xsiz, ysiz, zsiz;
	float xpiv, ypiv, zpiv;
	unsigned long numvoxs;
	long namoff;
	struct kv6data *lowermip;
	kv6voxtype *vox;      //numvoxs*sizeof(kv6voxtype)
	unsigned long *xlen;  //xsiz*sizeof(long)
	unsigned short *ylen; //xsiz*ysiz*sizeof(short)
} kv6data;

typedef struct
{
	long parent;      //index to parent sprite (-1=none)
	point3d p[2];     //"velcro" point of each object
	point3d v[2];     //axis of rotation for each object
	short vmin, vmax; //min value / max value
	char htype, filler[7];
} hingetype;

typedef struct { long tim, frm; } seqtyp;

typedef struct
{
	long numspr, numhin, numfrm, seqnum;
	long namoff;
	kv6data *basekv6;      //Points to original unconnected KV6 (maybe helpful?)
	struct vx5sprite *spr; //[numspr]
	hingetype *hinge;      //[numhin]
	long *hingesort;       //[numhin]
	short *frmval;         //[numfrm][numhin]
	seqtyp *seq;           //[seqnum]
} kfatype;

	//Notice that I aligned each point3d on a 16-byte boundary. This will be
	//   helpful when I get around to implementing SSE instructions someday...
typedef struct vx5sprite
{
	point3d p; //position in VXL coordinates
	long flags; //flags bit 0:0=use normal shading, 1=disable normal shading
					//flags bit 1:0=points to kv6data, 1=points to kfatype
					//flags bit 2:0=normal, 1=invisible sprite
	static union { point3d s, x; }; //kv6data.xsiz direction in VXL coordinates
	static union
	{
		kv6data *voxnum; //pointer to KV6 voxel data (bit 1 of flags = 0)
		kfatype *kfaptr; //pointer to KFA animation  (bit 1 of flags = 1)
	};
	static union { point3d h, y; }; //kv6data.ysiz direction in VXL coordinates
	long kfatim;        //time (in milliseconds) of KFA animation
	static union { point3d f, z; }; //kv6data.zsiz direction in VXL coordinates
	long okfatim;       //make vx5sprite exactly 64 bytes :)
} vx5sprite;

	//Falling voxels shared data: (flst = float list)
#define FLPIECES 256 //Max # of separate falling pieces
typedef struct //(68 bytes)
{
	lpoint3d chk; //a solid point on piece (x,y,pointer) (don't touch!)
	long i0, i1; //indices to start&end of slab list (don't touch!)
	long x0, y0, z0, x1, y1, z1; //bounding box, written by startfalls
	long mass; //mass of piece, written by startfalls (1 unit per voxel)
	point3d centroid; //centroid of piece, written by startfalls

		//userval is set to -1 when a new piece is spawned. Voxlap does not
		//read or write these values after that point. You should use these to
		//play an initial sound and track velocity
	long userval, userval2;
} flstboxtype;

	//Lighting variables: (used by updatelighting)
#define MAXLIGHTS 256
typedef struct { point3d p; float r2, sc; } lightsrctype;

	//Used by setspans/meltspans. Ordered this way to allow sorting as longs!
typedef struct { char z1, z0, x, y; } vspans;

#pragma pack(pop)

#define MAXFRM 1024 //MUST be even number for alignment!

	//Voxlap5 shared global variables:
struct vx5_interface
{
	//------------------------ DATA coming from VOXLAP5 ------------------------

		//Clipmove hit point info (use this after calling clipmove):
	double clipmaxcr; //clipmove always calls findmaxcr even with no movement
	dpoint3d cliphit[3];
	long cliphitnum;

		//Bounding box written by last set* VXL writing call
	long minx, miny, minz, maxx, maxy, maxz;

		//Falling voxels shared data:
	long flstnum;
	flstboxtype flstcnt[FLPIECES];

		//Total count of solid voxels in .VXL map (included unexposed voxels)
	long globalmass;

		//Temp workspace for KFA animation (hinge angles)
		//Animsprite writes these values&you may modify them before drawsprite
	short kfaval[MAXFRM];

	//------------------------ DATA provided to VOXLAP5 ------------------------

		//Opticast variables:
	long anginc, sideshademode, mipscandist, maxscandist, vxlmipuse, fogcol;

		//Drawsprite variables:
	long kv6mipfactor, kv6col;
		//Drawsprite x-plane clipping (reset to 0,(high int) after use!)
		//For example min=8,max=12 permits only planes 8,9,10,11 to draw
	long xplanemin, xplanemax;

		//Map modification function data:
	long curcol, currad, curhei;
	float curpow;

		//Procedural texture function data:
	long (*colfunc)(lpoint3d *);
	long cen, amount, *pic, bpl, xsiz, ysiz, xoru, xorv, picmode;
	point3d fpico, fpicu, fpicv, fpicw;
	lpoint3d pico, picu, picv;
	float daf;

		//Lighting variables: (used by updatelighting)
	long lightmode; //0 (default), 1:simple lighting, 2:lightsrc lighting
	lightsrctype lightsrc[MAXLIGHTS]; //(?,?,?),128*128,262144
	long numlights;

	long fallcheck;
};
#ifdef VOXLAP5
struct vx5_interface vx5;
#else
extern struct vx5_interface vx5;
#endif 

	//Initialization functions:
extern long initvoxlap ();
extern void uninitvoxlap ();

	//File related functions:
extern long loadsxl (const char *, char **, char **, char **);
extern char *parspr (vx5sprite *, char **);
extern void loadnul (dpoint3d *, dpoint3d *, dpoint3d *, dpoint3d *);
extern long loaddta (const char *, dpoint3d *, dpoint3d *, dpoint3d *, dpoint3d *);
extern long loadpng (const char *, dpoint3d *, dpoint3d *, dpoint3d *, dpoint3d *);
extern void loadbsp (const char *, dpoint3d *, dpoint3d *, dpoint3d *, dpoint3d *);
extern long loadvxl (const char *, dpoint3d *, dpoint3d *, dpoint3d *, dpoint3d *);
extern long savevxl (const char *, dpoint3d *, dpoint3d *, dpoint3d *, dpoint3d *);
extern long loadsky (const char *);

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

	//Sprite related functions:
extern kv6data *getkv6 (const char *);
extern kfatype *getkfa (const char *);
extern void freekv6 (kv6data *kv6);
extern void savekv6 (const char *, kv6data *);
extern void getspr (vx5sprite *, const char *);
extern kv6data *genmipkv6 (kv6data *);
extern char *getkfilname (long);
extern void animsprite (vx5sprite *, long);
extern void drawsprite (vx5sprite *);
extern long meltsphere (vx5sprite *, lpoint3d *, long);
extern long meltspans (vx5sprite *, vspans *, long, lpoint3d *);

	//Physics helper functions:
extern void orthonormalize (point3d *, point3d *, point3d *);
extern void dorthonormalize (dpoint3d *, dpoint3d *, dpoint3d *);
extern void orthorotate (float, float, float, point3d *, point3d *, point3d *);
extern void dorthorotate (double, double, double, dpoint3d *, dpoint3d *, dpoint3d *);
extern void axisrotate (point3d *, point3d *, float);
extern void slerp (point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, float);
extern long cansee (point3d *, point3d *, lpoint3d *);
extern void hitscan (dpoint3d *, dpoint3d *, lpoint3d *, long **, long *);
extern void sprhitscan (dpoint3d *, dpoint3d *, vx5sprite *, lpoint3d *, kv6voxtype **, float *vsc);
extern double findmaxcr (double, double, double, double);
extern void clipmove (dpoint3d *, dpoint3d *, double);
extern long triscan (point3d *, point3d *, point3d *, point3d *, lpoint3d *);
extern void estnorm (long, long, long, point3d *);

	//VXL reading functions (fast!):
extern long isvoxelsolid (long, long, long);
extern long anyvoxelsolid (long, long, long, long);
extern long anyvoxelempty (long, long, long, long);
extern long getfloorz (long, long, long);
extern long getcube (long, long, long);

	//VXL writing functions (optimized & bug-free):
extern void setcube (long, long, long, long);
extern void setsphere (lpoint3d *, long, long);
extern void setellipsoid (lpoint3d *, lpoint3d *, long, long, long);
extern void setcylinder (lpoint3d *, lpoint3d *, long, long, long);
extern void setrect (lpoint3d *, lpoint3d *, long);
extern void settri (point3d *, point3d *, point3d *, long);
extern void setsector (point3d *, long *, long, float, long, long);
extern void setspans (vspans *, long, lpoint3d *, long);
extern void setheightmap (const unsigned char *, long, long, long, long, long, long, long);
extern void setkv6 (vx5sprite *, long);

	//VXL writing functions (slow or buggy):
extern void sethull3d (point3d *, long, long, long);
extern void setlathe (point3d *, long, long, long);
extern void setblobs (point3d *, long, long, long);
extern void setfloodfill3d (long, long, long, long, long, long, long, long, long);
extern void sethollowfill ();
extern void setkvx (const char *, long, long, long, long, long);
extern void setflash (float, float, float, long, long, long);
extern void setnormflash (float, float, float, long, long);

	//VXL MISC functions:
extern void updatebbox (long, long, long, long, long, long, long);
extern void updatevxl ();
extern void genmipvxl (long, long, long, long);
extern void updatelighting (long, long, long, long, long, long);

	//Falling voxels functions:
extern void checkfloatinbox (long, long, long, long, long, long);
extern void startfalls ();
extern void dofall (long);
extern long meltfall (vx5sprite *, long, long);
extern void finishfalls ();

	//Procedural texture functions:
extern long curcolfunc (lpoint3d *);
extern long floorcolfunc (lpoint3d *);
extern long jitcolfunc (lpoint3d *);
extern long manycolfunc (lpoint3d *);
extern long sphcolfunc (lpoint3d *);
extern long woodcolfunc (lpoint3d *);
extern long pngcolfunc (lpoint3d *);
extern long kv6colfunc (lpoint3d *);

	//Editing backup/restore functions
extern void voxbackup (long, long, long, long, long);
extern void voxdontrestore ();
extern void voxrestore ();
extern void voxredraw ();

#if defined(VOXLAP_C) && defined(__cplusplus)
}
#endif
