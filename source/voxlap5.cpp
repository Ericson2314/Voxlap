/**************************************************************************************************
 * VOXLAP engine                                                                                  *
 * by Ken Silverman (http://advsys.net/ken)                                                       *
 **************************************************************************************************/

	//This file has been modified from Ken Silverman's original release

	//C Standard Library includes
#include <math.h>
#include <stdio.h>
#include <stdlib.h> //was below system-specific includes before

	//Ericson2314's dirty porting tricks
#include "porthacks.h"

	//Ken's short, general-purpose to-be-inlined functions mainly consisting of inline assembly are now here
#include "ksnippits.h"

	//Basic System Specific Stuff
#ifndef _WIN32 //Windows (hypothetically 6 64-bit too)
	#include <stdarg.h> //Moved from #if _DOS, included via windows.h for Windows
	#include <string.h> //Moved from #if _DOS, included via windows.h for Windows
	#ifdef _DOS //MS-DOS
		#include <conio.h>
		#include <dos.h>
		#define MAX_PATH 260
	#else //POSIX
		#define MAX_PATH PATH_MAX //is a bad variable, should not be used for filename related things
	#endif
#endif

	//SYSMAIN Preprocessor stuff
//#define SYSMAIN_C //if sysmain is compiled as C
#include "sysmain.h"
#include "voxlap5.h"
	//Voxlap Preprocessor stuff
#define VOXLAP5
				//We never want to define C bindings if this is compiled as C++.
#undef VOXLAP_C	//Putting this here just in case.


	//mmxcolor* now defined here
#include "voxflash.h"

	//KPlib Preprocessor stuff
//#define KPLIB_C  //if kplib is compiled as C
#include "kplib.h"
#include "kfonts.h"
#include "kcolors.h"

#define USEZBUFFER 1                 //Should a Z-Buffer be Used?
//#define NOASM                  //Instructs compiler to use C(++) alternatives
#define PREC (256*4096)
#define CMPPREC (256*4096)
#define FPREC (256*4096)
//#define USEV5ASM 1                 //now done via makefile
#define SCISDIST 1.0
#define GOLDRAT 0.3819660112501052 //Golden Ratio: 1 - 1/((sqrt(5)+1)/2)
#define ESTNORMRAD 2               //Specially optimized for 2: DON'T CHANGE unless testing!

EXTERN_SYSMAIN void breath ();
EXTERN_SYSMAIN void evilquit (const char *);

#define VOXSIZ VSID*VSID*128

#ifdef __cplusplus
extern "C" {
#endif
char *sptr[(VSID*VSID*4)/3];
#ifdef __cplusplus
}
#endif
static long *vbuf = 0, *vbit = 0, vbiti;
	//WARNING: loaddta uses last 2MB of vbuf; vbuf:[VOXSIZ>>2], vbit:[VOXSIZ>>7]
	//WARNING: loadpng uses last 4MB of vbuf; vbuf:[VOXSIZ>>2], vbit:[VOXSIZ>>7]

//                     ÚÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄ¿
//        vbuf format: ³   0:   ³   1:   ³   2:   ³   3:   ³
//ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÅÄÄÄÄÄÄÄÄÅÄÄÄÄÄÄÄÄÅÄÄÄÄÄÄÄÄÅÄÄÄÄÄÄÄÄ´
//³      First header: ³ nextptr³   z1   ³   z1c  ³  dummy ³
//³           Color 1: ³    b   ³    g   ³    r   ³ intens ³
//³           Color 2: ³    b   ³    g   ³    r   ³ intens ³
//³             ...    ³    b   ³    g   ³    r   ³ intens ³
//³           Color n: ³    b   ³    g   ³    r   ³ intens ³
//³ Additional header: ³ nextptr³   z1   ³   z1c  ³   z0   ³
//ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÁÄÄÄÄÄÄÄÄÁÄÄÄÄÄÄÄÄÁÄÄÄÄÄÄÄÄÁÄÄÄÄÄÄÄÄÙ
//  nextptr: add this # <<2 to index to get to next header (0 if none)
//       z1: z floor (top of floor color list)
//      z1c: z bottom of floor color list MINUS 1! - needed to calculate
//             slab size with slng() and used as a separator for fcol/ccol
//       z0: z ceiling (bottom of ceiling color list)

	//Memory management variables:
#define MAXCSIZ 1028
char tbuf[MAXCSIZ];
long tbuf2[MAXZDIM*3];
long templongbuf[MAXZDIM];

static char nullst = 0; //nullst always NULL string

#define SETSPHMAXRAD 256
static double logint[SETSPHMAXRAD];
static float tempfloatbuf[SETSPHMAXRAD];
static long factr[SETSPHMAXRAD][2];

#pragma pack(push,1)
	//Rendering variables:
#if (USEZBUFFER == 0)
typedef struct { long col; } castdat;
#else
typedef struct { long col, dist; } castdat;
#endif
typedef struct { castdat *i0, *i1; long z0, z1, cx0, cy0, cx1, cy1; } cftype;
#pragma pack(pop)

#if (defined(USEV5ASM) && (USEV5ASM != 0)) //if true
	EXTERN_C cftype cfasm[256];
	EXTERN_C castdat skycast;
#else
	cftype cfasm[256];
#endif
	//Screen related variables:
static long xres_voxlap, yres_voxlap, bytesperline, frameplace, xres4_voxlap;
long ylookup[MAXYDIM+1];

static lpoint3d glipos;
static point3d gipos, gistr, gihei, gifor;
static point3d gixs, giys, gizs, giadd;
static float gihx, gihy, gihz, gposxfrac[2], gposyfrac[2], grd;
static long gposz, giforzsgn, gstartz0, gstartz1, gixyi[2];
static char *gstartv;

long backtag, backedup = -1, bacx0, bacy0, bacx1, bacy1;
char *bacsptr[262144];

	//Flash variables
#define LOGFLASHVANG 9
static lpoint2d gfc[(1<<LOGFLASHVANG)*8];
static long gfclookup[8] = {4,7,2,5,0,3,6,1}, flashcnt = 0;
int64_t flashbrival;

	//Norm flash variables
#define GSIZ 512  //NOTE: GSIZ should be 1<<x, and must be <= 65536
static long bbuf[GSIZ][GSIZ>>5], p2c[32], p2m[32];      //bbuf: 2.0K
static uspoint2d ffx[((GSIZ>>1)+2)*(GSIZ>>1)], *ffxptr; // ffx:16.5K
static long xbsox = -17, xbsoy, xbsof;
static int64_t xbsbuf[25*5+1]; //need few bits before&after for protection

	//Look tables for expandbitstack256:
static long xbsceil[32], xbsflor[32]; //disabling mangling for inline asm

	//float detection & falling code variables...
	//WARNING: VLSTSIZ,FSTKSIZ,FLCHKSIZ can all have bounds errors! :(
#define VLSTSIZ 65536 //Theoretically should be at least: VOXSIZ\8
#define LOGHASHEAD 12
#define FSTKSIZ 8192
typedef struct { long v, b; } vlstyp;
vlstyp vlst[VLSTSIZ];
long hhead[1<<LOGHASHEAD], vlstcnt = 0x7fffffff;
lpoint3d fstk[FSTKSIZ]; //Note .z is actually used as a pointer, not z!
#define FLCHKSIZ 4096
lpoint3d flchk[FLCHKSIZ]; long flchkcnt = 0;

	//Opticast global variables:
	//radar: 320x200 requires  419560*2 bytes (area * 6.56*2)
	//radar: 400x300 requires  751836*2 bytes (area * 6.27*2)
	//radar: 640x480 requires 1917568*2 bytes (area * 6.24*2)
#define SCPITCH 256
long *radar = 0, *radarmem = 0;
#if (USEZBUFFER == 1)
static long *zbuffermem = 0, zbuffersiz = 0;
#endif
static castdat *angstart[MAXXDIM*4], *gscanptr;
#define CMPRECIPSIZ MAXXDIM+32
static float cmprecip[CMPRECIPSIZ], wx0, wy0, wx1, wy1;
static long iwx0, iwy0, iwx1, iwy1;
static point3d gcorn[4];
		 point3d ginor[4]; //Should be static, but... necessary for stupid pingball hack :/
static long lastx[MAX(MAXYDIM,VSID)], uurendmem[MAXXDIM*2+8], *uurend;

void mat0(point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *);
void mat1(point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *);
void mat2(point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *);

	//Parallaxing sky variables:
static long skypic = 0, nskypic = 0, skybpl, skyysiz, skycurlng, skycurdir;
static float skylngmul;
static point2d *skylng = 0;

#ifdef __cplusplus
extern "C" {
#endif

	//Parallaxing sky variables (accessed by assembly code)
long skyoff = 0, skyxsiz, *skylat = 0;

int64_t gi, gcsub[8] =
{
	0xff00ff00ff00ff,0xff00ff00ff00ff,0xff00ff00ff00ff,0xff00ff00ff00ff,
	0xff00ff00ff00ff,0xff00ff00ff00ff,0xff00ff00ff00ff,0xff00ff00ff00ff
};
long gylookup[512+36], gmipnum = 0; //256+4+128+4+64+4+...
long gpz[2], gdz[2], gxmip, gxmax, gixy[2], gpixy;
static long gmaxscandist;

//long reax, rebx, recx, redx, resi, redi, rebp, resp, remm[16];



EXTERN_C void grouscanasm (long);
#if (USEZBUFFER == 1)
long zbufoff;
#endif
#ifdef __cplusplus
}
#endif
#define gi0 (((long *)&gi)[0])
#define gi1 (((long *)&gi)[1])


	//Ken Silverman knows how to use EMMS
#if defined(_MSC_VER) && !defined(NOASM)
	#pragma warning(disable:4799)
#endif

#if (defined(USEV5ASM) && (USEV5ASM != 0))
/** TODO: still having issues with these in MSVC build */
EXTERN_C void dep_protect_start();
EXTERN_C void dep_protect_end();
#endif

/* if (a < 0) return(0); else if (a > b) return(b); else return(a); */
static inline long lbound0 (long a, long b) //b MUST be >= 0
{
	if ((unsigned long)a <= b) return(a);
	return((~(a>>31))&b);
}

/* if (a < b) return(b); else if (a > c) return(c); else return(a); */
static inline long lbound (long a, long b, long c) //c MUST be >= b
{
	c -= b;
	if ((unsigned long)(a-b) <= c) return(a);
	return((((b-a)>>31)&c) + b);
}

#define LSINSIZ 8 //Must be >= 2!
static point2d usintab[(1<<LSINSIZ)+(1<<(LSINSIZ-2))];
/** Create a cos / sin lookup table, with LSINSIZ determining number of elements
 * ( and ultimately precision )
 */
static void ucossininit ()
{
	long i, j;
	double a, ai, s, si, m;

	j = 0; usintab[0].y = 0.0;
	i = (1<<LSINSIZ)-1;
	ai = PI*(-2)/((float)(1<<LSINSIZ)); a = ((float)(-i))*ai;
	ai *= .5; m = sin(ai)*2; s = sin(a); si = cos(a+ai)*m; m = -m*m;
	for(;i>=0;i--)
	{
		usintab[i].y = s; s += si; si += s*m; //MUCH faster than next line :)
		//usintab[i].y = sin(i*PI*2/((float)(1<<LSINSIZ)));
		usintab[i].x = (usintab[j].y-usintab[i].y)/((float)(1<<(32-LSINSIZ)));
		j = i;
	}
	for(i=(1<<(LSINSIZ-2))-1;i>=0;i--) usintab[i+(1<<LSINSIZ)] = usintab[i];
}

/** Calculates cos & sin of 32-bit unsigned long angle in ~15 clock cycles
 *  Uses bitshift to find offset into table, and places sin/cos of angle into
 *  *cosin's memory location.
 *  ( Gotta love these old school optimizations ;)
 *  @param a Angle to look up in table
 *  @param cosin Modified by function
 *  Accuracy is approximately +/-.0001
 */
static inline void ucossin (unsigned long a, float *cosin)
{
	float f = ((float)(a&((1<<(32-LSINSIZ))-1))); a >>= (32-LSINSIZ);
	cosin[0] = usintab[a+(1<<(LSINSIZ-2))].x*f+usintab[a+(1<<(LSINSIZ-2))].y;
	cosin[1] = usintab[a                 ].x*f+usintab[a                 ].y;
}

/**
 * Draws 4x6 font on screen (very fast!)
 * @param x x of top-left corner
 * @param y y of top-left corner
 * @param fcol foreground color (32-bit RGB format)
 * @param bcol background color (32-bit RGB format) or -1 for transparent
 * @param fmt string - same syntax as printf
 */
void print4x6 (long x, long y, long fcol, long bcol, const char *fmt, ...)
{
	va_list arglist;
	char st[280], *c;
	long i, j;

	if (!fmt) return;
	va_start(arglist,fmt);
	vsprintf(st,fmt,arglist);
	va_end(arglist);

	y = y*bytesperline+(x<<2)+frameplace;
	if (bcol < 0)
	{
		for(j=20;j>=0;y+=bytesperline,j-=4)
			for(c=st,x=y;*c;c++,x+=16)
			{
				i = (font4x6[*c]>>j);
				if (i&8) *(long *)(x   ) = fcol;
				if (i&4) *(long *)(x+ 4) = fcol;
				if (i&2) *(long *)(x+ 8) = fcol;
				if (i&1) *(long *)(x+12) = fcol;
				if ((*c) == 9) x += 32;
			}
		return;
	}
	fcol -= bcol;
	for(j=20;j>=0;y+=bytesperline,j-=4)
		for(c=st,x=y;*c;c++,x+=16)
		{
			i = (font4x6[*c]>>j);
			*(long *)(x   ) = (((i<<28)>>31)&fcol)+bcol;
			*(long *)(x+ 4) = (((i<<29)>>31)&fcol)+bcol;
			*(long *)(x+ 8) = (((i<<30)>>31)&fcol)+bcol;
			*(long *)(x+12) = (((i<<31)>>31)&fcol)+bcol;
			if ((*c) == 9) { for(i=16;i<48;i+=4) *(long *)(x+i) = bcol; x += 32; }
		}
}
/**
 * Draws 6x8 font on screen (very fast!)
 * @param x x of top-left corner
 * @param y y of top-left corner
 * @param fcol foreground color (32-bit RGB format)
 * @param bcol background color (32-bit RGB format) or -1 for transparent
 * @param fmt string - same syntax as printf
 */
void print6x8 (long x, long y, long fcol, long bcol, const char *fmt, ...)
{
	va_list arglist;
	char st[280], *c, *v;
	long i, j;

	if (!fmt) return;
	va_start(arglist,fmt);
	vsprintf(st,fmt,arglist);
	va_end(arglist);

	y = y*bytesperline+(x<<2)+frameplace;
	if (bcol < 0)
	{
		for(j=1;j<256;y+=bytesperline,j<<=1)
			for(c=st,x=y;*c;c++,x+=24)
			{
				v = (char *)(((long)font6x8) + ((long)c[0])*6);
				if (v[0]&j) *(long *)(x   ) = fcol;
				if (v[1]&j) *(long *)(x+ 4) = fcol;
				if (v[2]&j) *(long *)(x+ 8) = fcol;
				if (v[3]&j) *(long *)(x+12) = fcol;
				if (v[4]&j) *(long *)(x+16) = fcol;
				if (v[5]&j) *(long *)(x+20) = fcol;
				if ((*c) == 9) x += ((2*6)<<2);
			}
		return;
	}
	fcol -= bcol;
	for(j=1;j<256;y+=bytesperline,j<<=1)
		for(c=st,x=y;*c;c++,x+=24)
		{
			v = (char *)(((long)font6x8) + ((long)c[0])*6);
			*(long *)(x   ) = (((-(v[0]&j))>>31)&fcol)+bcol;
			*(long *)(x+ 4) = (((-(v[1]&j))>>31)&fcol)+bcol;
			*(long *)(x+ 8) = (((-(v[2]&j))>>31)&fcol)+bcol;
			*(long *)(x+12) = (((-(v[3]&j))>>31)&fcol)+bcol;
			*(long *)(x+16) = (((-(v[4]&j))>>31)&fcol)+bcol;
			*(long *)(x+20) = (((-(v[5]&j))>>31)&fcol)+bcol;
			if ((*c) == 9) { for(i=24;i<72;i+=4) *(long *)(x+i) = bcol; x += ((2*6)<<2); }
		}
}
/** This should move to kcolors.h , but need to deal with global sptr first */
long floorcolfunc (lpoint3d *p)
{
	char *v;
	for(v=sptr[p->y*VSID+p->x];(p->z>v[2]) && (v[0]);v+=v[0]*4);
	return(*(long *)&v[4]);
}

/**
*   QUESS: Slab length
*/
static long slng (const char *s)
{
	const char *v;

	for(v=s;v[0];v+=v[0]*4);
	return((long)v-(long)s+(v[2]-v[1]+1)*4+4);
}

void voxdealloc (const char *v)
{
	long i, j;
	i = (((long)v-(long)vbuf)>>2); j = (slng(v)>>2)+i;
#if 0
	while (i < j) { vbit[i>>5] &= ~(1<<i); i++; }
#else
	if (!((j^i)&~31))
		vbit[i>>5] &= ~(p2m[j&31]^p2m[i&31]);
	else
	{
		vbit[i>>5] &=   p2m[i&31];  i >>= 5;
		vbit[j>>5] &= (~p2m[j&31]); j >>= 5;
		for(j--;j>i;j--) vbit[j] = 0;
	}
#endif
}

/** @note  danum MUST be a multiple of 4! */
char *voxalloc (long danum)
{
	long i, badcnt, p0, p1, vend;

	badcnt = 0; danum >>= 2; vend = (VOXSIZ>>2)-danum;
	do
	{
		for(;vbiti<vend;vbiti+=danum)
		{
			if (vbit[vbiti>>5]&(1<<vbiti)) continue;
			for(p0=vbiti;(!(vbit[(p0-1)>>5]&(1<<(p0-1))));p0--);
			for(p1=p0+danum-1;p1>vbiti;p1--)
				if (vbit[p1>>5]&(1<<p1)) goto allocnothere;

			vbiti = p0+danum;
			for(i=p0;i<vbiti;i++) vbit[i>>5] |= (1<<i);
			return((char *)(&vbuf[p0]));
allocnothere:;
		}
		vbiti = 0; badcnt++;
	} while (badcnt < 2);
	evilquit("voxalloc: vbuf full"); return(0);
}

long isvoxelsolid (long x, long y, long z)
{
	char *v;

	if ((unsigned long)(x|y) >= VSID) return(0);
	v = sptr[y*VSID+x];
	while (1)
	{
		if (z < v[1]) return(0);
		if (!v[0]) return(1);
		v += v[0]*4;
		if (z < v[3]) return(1);
	}
}

/** Returns 1 if any voxels in range (x,y,z0) to (x,y,z1-1) are solid, else 0 */
long anyvoxelsolid (long x, long y, long z0, long z1)
{
	char *v;

		//         v1.....v3   v1.....v3    v1.......................>
		//                z0.........z1
	if ((unsigned long)(x|y) >= VSID) return(0);
	v = sptr[y*VSID+x];
	while (1)
	{
		if (z1 <= v[1]) return(0);
		if (!v[0]) return(1);
		v += v[0]*4;
		if (z0 < v[3]) return(1);
	}
}

/** Returns 1 if any voxels in range (x,y,z0) to (x,y,z1-1) are empty, else 0 */
long anyvoxelempty (long x, long y, long z0, long z1)
{
	char *v;

		//         v1.....v3   v1.....v3    v1.......................>
		//                z0.........z1
	if ((unsigned long)(x|y) >= VSID) return(1);
	v = sptr[y*VSID+x];
	while (1)
	{
		if (z0 < v[1]) return(1);
		if (!v[0]) return(0);
		v += v[0]*4;
		if (z1 <= v[3]) return(0);
	}
}

/** Returns z of first solid voxel under (x,y,z). Returns z if in solid. */
long getfloorz (long x, long y, long z)
{
	char *v;

	if ((unsigned long)(x|y) >= VSID) return(z);
	v = sptr[y*VSID+x];
	while (1)
	{
		if (z <= v[1]) return(v[1]);
		if (!v[0]) break;
		v += v[0]*4;
		if (z < v[3]) break;
	}
	return(z);
}

/** Given xyz coordinate
 *  Returns: 0: air   1: unexposed solid  or address to color in vbuf
 *  @note this can never be 0 or 1)
 */
long getcube (long x, long y, long z)
{
	long ceilnum;
	char *v;

	if ((unsigned long)(x|y) >= VSID) return(0);
	v = sptr[y*VSID+x];
	while (1)
	{
		if (z <= v[2])
		{
			if (z < v[1]) return(0);
			return((long)&v[(z-v[1])*4+4]);
		}
		ceilnum = v[2]-v[1]-v[0]+2;

		if (!v[0]) return(1);
		v += v[0]*4;

		if (z < v[3])
		{
			if (z-v[3] < ceilnum) return(1);
			return((long)&v[(z-v[3])*4]);
		}
	}
}

	// Inputs: uind[MAXZDIM]: uncompressed 32-bit color buffer (-1: air)
	//         nind?[MAXZDIM]: neighbor buf:
	//            -2: unexposed solid
	//            -1: air
	//    0-16777215: exposed solid (color)
	//         px,py: parameters for setting unexposed voxel colors
	//Outputs: cbuf[MAXCSIZ]: compressed output buffer
	//Returns: n: length of compressed buffer (in bytes)
long compilestack (long *uind, long *n0, long *n1, long *n2, long *n3, char *cbuf, long px, long py)
{
	long oz, onext, n, cp2, cp1, cp0, rp1, rp0;
	lpoint3d p;

	p.x = px; p.y = py;

		//Do top slab (sky)
	oz = -1;
	p.z = -1; while (uind[p.z+1] == -1) p.z++;
	onext = 0;
	cbuf[1] = p.z+1;
	cbuf[2] = p.z+1;
	cbuf[3] = 0;  //Top z0 (filler, not used yet)
	n = 4;
	cp1 = 1; cp0 = 0;
	rp1 = -1; rp0 = -1;

	do
	{
			//cp2 = state at p.z-1 (0 = air, 1 = next2air, 2 = solid)
			//cp1 = state at p.z   (0 = air, 1 = next2air, 2 = solid)
			//cp0 = state at p.z+1 (0 = air, 1 = next2air, 2 = solid)
		cp2 = cp1; cp1 = cp0; cp0 = 2;
		if (p.z < MAXZDIM-2)  //Bottom must be solid!
		{
			if (uind[p.z+1] == -1)
				cp0 = 0;
			else if ((n0[p.z+1] == -1) || (n1[p.z+1] == -1) ||
						(n2[p.z+1] == -1) || (n3[p.z+1] == -1))
				cp0 = 1;
		}

			//Add slab
		if (cp1 != rp0)
		{
			if ((!cp1) && (rp0 > 0)) { oz = p.z; }
			else if ((rp0 < cp1) && (rp0 < rp1))
			{
				if (oz < 0) oz = p.z;
				cbuf[onext] = ((n-onext)>>2); onext = n;
				cbuf[n+1] = p.z;
				cbuf[n+2] = p.z-1;
				cbuf[n+3] = oz;
				n += 4; oz = -1;
			}
			rp1 = rp0; rp0 = cp1;
		}

			//Add color
		if ((cp1 == 1) || ((cp1 == 2) && ((!cp0) || (!cp2))))
		{
			if (cbuf[onext+2] == p.z-1) cbuf[onext+2] = p.z;
			if (uind[p.z] == -2) *(long *)&cbuf[n] = vx5.colfunc(&p);
								 else *(long *)&cbuf[n] = uind[p.z];
			n += 4;
		}

		p.z++;
	} while (p.z < MAXZDIM);
	cbuf[onext] = 0;
	return(n);
}

/** Expand compressed span data, in to uncompressed bitmap */
static inline void expandbit256 (void *s, void *d)
{
	#ifdef NOASM
	int32_t eax;
	int32_t ecx = 32; //current bit index
	uint32_t edx = 0; //value of current 32-bit bits

	goto in2it;

	while (eax != 0)
	{
		s += eax * 4;
		eax = ((uint8_t*)s)[3];

		if ((eax -= ecx) >= 0) //xor mask [eax] for ceiling begins
		{
			do
			{
				*(uint32_t*)d = edx;
				d += 4;
				edx = -1;
				ecx += 32;
			}
			while ((eax -= 32) >= 0);
		}

		edx &= xbsceil[32+eax];
		//no jump

	in2it:
		eax = ((uint8_t*)s)[1];

		if ((eax -= ecx) > 0) //xor mask [eax] for floor begins
		{
			do
			{
				*(uint32_t*)d = edx;
				d += 4;
				edx = 0;
				ecx += 32;
			}
			while ((eax -= 32) >= 0);
		}

		edx |= xbsflor[32+eax];
		eax = *(uint8_t*)s;
	}

	if ((ecx -= 256) <= 0)
	{
		do
		{
			*(uint32_t*)d = edx;
			d += 4;
			edx = -1;
		}
		while ((ecx += 32) <= 0);
	}
	#else
	#ifdef __GNUC__ //gcc inline asm
	__asm__ __volatile__
	(
		".intel_syntax noprefix\n"
		"mov	ecx, 32\n"   //current bit index
		"xor	edx, edx\n"  //value of current 32-bit bits
		"jmp	short 0f\n"
	"1:\n" //begit
		"lea	esi, [esi+eax*4]\n"
		"movzx	eax, byte ptr [esi+3]\n"
		"sub	eax, ecx\n"                //xor mask [eax] for ceiling begins
		"jl	short 3f\n"
	"2:\n" //xdoc
		"mov	[edi], edx\n"
		"add	edi, 4\n"
		"mov	edx, -1\n"
		"add	ecx, 32\n"
		"sub	eax, 32\n"
		"jge	short 2b\n"
	"3:\n" //xskpc
		".att_syntax prefix\n"
		"andl	%c[ceil]+0x80(,%%eax,4), %%edx\n" //~(-1<<eax)
		".intel_syntax noprefix\n"

	"0:\n" //in2it
		"movzx	eax, byte ptr [esi+1]\n"
		"sub	eax, ecx\n"                //xor mask [eax] for floor begins
		"jl	short 5f\n"
	"4:\n" //xdof
		"mov	[edi], edx\n"
		"add	edi, 4\n"
		"xor	edx, edx\n"
		"add	ecx, 32\n"
		"sub	eax, 32\n"
		"jge	short 4b\n"
	"5:\n" //xskpf
		".att_syntax prefix\n"
		"orl	%c[floor]+0x80(,%%eax,4), %%edx\n" //(-1<<eax)
		".intel_syntax noprefix\n"

		"movzx	eax, byte ptr [esi]\n"
		"test	eax, eax\n"
		"jnz	short 1b\n"
		"sub	ecx, 256\n"              //finish writing buffer to [edi]
		"jg	short 7f\n"
	"6:\n" //xdoe
		"mov	[edi], edx\n"
		"add	edi, 4\n"
		"mov	edx, -1\n"
		"add	ecx, 32\n"
		"jle	short 6b\n"
	"7:\n" //xskpe
		".att_syntax prefix\n"
		:
		: "S" (s), "D" (d), [ceil] "p" (xbsceil), [floor] "p" (xbsflor)
		: "cc", "eax", "ecx", "edx"
	);
	#endif
	#ifdef _MSC_VER //msvc inline asm
	_asm
	{
		push	esi
		push	edi
		mov	esi, s
		mov	edi, d
		mov	ecx, 32   //current bit index
		xor	edx, edx  //value of current 32-bit bits
		jmp	short in2it
	begit:
		lea	esi, [esi+eax*4]
		movzx	eax, byte ptr [esi+3]
		sub	eax, ecx                //xor mask [eax] for ceiling begins
		jl	short xskpc
	xdoc:
		mov	[edi], edx
		add	edi, 4
		mov	edx, -1
		add	ecx, 32
		sub	eax, 32
		jge	short xdoc
	xskpc:
		and	edx, xbsceil[eax*4+128] //~(-1<<eax); xor mask [eax] for ceiling ends
	in2it:
		movzx	eax, byte ptr [esi+1]
		sub	eax, ecx                //xor mask [eax] for floor begins
		jl	short xskpf
	xdof:
		mov	[edi], edx
		add	edi, 4
		xor	edx, edx
		add	ecx, 32
		sub	eax, 32
		jge	short xdof
	xskpf:
		or	edx, xbsflor[eax*4+128] //(-1<<eax); xor mask [eax] for floor ends
		movzx	eax, byte ptr [esi]
		test	eax, eax
		jnz	short begit
		sub	ecx, 256                //finish writing buffer to [edi]
		jg	short xskpe
	xdoe:
		mov	[edi], edx
		add	edi, 4
		mov	edx, -1
		add	ecx, 32
		jle	short xdoe
	xskpe:
		pop	edi
		pop	esi
	}
	#endif
	#endif
}

void expandbitstack (long x, long y, int64_t *bind)
{
	if ((x|y)&(~(VSID-1))) { clearbuf((void *)bind,8,0L); return; }
	expandbit256(sptr[y*VSID+x],(void *)bind);
}
/** Expands compiled voxel info to 32-bit uind[?] */
void expandstack (long x, long y, long *uind)
{
	long z, topz;
	char *v, *v2;

	if ((x|y)&(~(VSID-1))) { clearbuf((void *)uind,MAXZDIM,0); return; }


	v = sptr[y*VSID+x]; z = 0;
	while (1)
	{
		while (z < v[1]) { uind[z] = -1; z++; }
		while (z <= v[2]) { uind[z] = (*(long *)&v[(z-v[1])*4+4]); z++; }
		v2 = &v[(v[2]-v[1]+1)*4+4];

		if (!v[0]) break;
		v += v[0]*4;

		topz = v[3]+(((long)v2-(long)v)>>2);
		while (z < topz) { uind[z] = -2; z++; }
		while (z < v[3]) { uind[z] = *(long *)v2; z++; v2 += 4; }
	}
	while (z < MAXZDIM) { uind[z] = -2; z++; }
}

void gline (long leng, float x0, float y0, float x1, float y1)
{
	uint64_t q;
	float f, f1, f2, vd0, vd1, vz0, vx1, vy1, vz1;
	long j;
	cftype *c;
#if (!defined(USEV5ASM) || (USEV5ASM == 0)) //if false
	long gx, ogx, gy, ixy, col, dax, day;
	cftype *c2, *ce;
	char *v;
#endif

	vd0 = x0*gistr.x + y0*gihei.x + gcorn[0].x;
	vd1 = x0*gistr.y + y0*gihei.y + gcorn[0].y;
	vz0 = x0*gistr.z + y0*gihei.z + gcorn[0].z;
	vx1 = x1*gistr.x + y1*gihei.x + gcorn[0].x;
	vy1 = x1*gistr.y + y1*gihei.y + gcorn[0].y;
	vz1 = x1*gistr.z + y1*gihei.z + gcorn[0].z;

	f = sqrt(vx1*vx1 + vy1*vy1);
	f1 = f / vx1;
	f2 = f / vy1;
	if (fabs(vx1) > fabs(vy1)) vd0 = vd0*f1; else vd0 = vd1*f2;
	if (*(long *)&vd0 < 0) vd0 = 0; //vd0 MUST NOT be negative: bad for asm
	vd1 = f;
	ftol(fabs(f1)*PREC,&gdz[0]);
	ftol(fabs(f2)*PREC,&gdz[1]);

	gixy[0] = (((*(signed long *)&vx1)>>31)<<3)+4; //=sgn(vx1)*4
	gixy[1] = gixyi[(*(unsigned long *)&vy1)>>31]; //=sgn(vy1)*4*VSID
	if (gdz[0] <= 0) { ftol(gposxfrac[(*(unsigned long *)&vx1)>>31]*fabs(f1)*PREC,&gpz[0]); if (gpz[0] <= 0) gpz[0] = 0x7fffffff; gdz[0] = 0x7fffffff-gpz[0]; } //Hack for divide overflow
	else ftol(gposxfrac[(*(unsigned long *)&vx1)>>31]*(float)gdz[0],&gpz[0]);
	if (gdz[1] <= 0) { ftol(gposyfrac[(*(unsigned long *)&vy1)>>31]*fabs(f2)*PREC,&gpz[1]); if (gpz[1] <= 0) gpz[1] = 0x7fffffff; gdz[1] = 0x7fffffff-gpz[1]; } //Hack for divide overflow
	else ftol(gposyfrac[(*(unsigned long *)&vy1)>>31]*(float)gdz[1],&gpz[1]);

	c = &cfasm[128];
	c->i0 = gscanptr; c->i1 = &gscanptr[leng];
	c->z0 = gstartz0; c->z1 = gstartz1;
	if (giforzsgn < 0)
	{
		ftol((vd1-vd0)*cmprecip[leng],&gi0); ftol(vd0*CMPPREC,&c->cx0);
		ftol((vz1-vz0)*cmprecip[leng],&gi1); ftol(vz0*CMPPREC,&c->cy0);
	}
	else
	{
		ftol((vd0-vd1)*cmprecip[leng],&gi0); ftol(vd1*CMPPREC,&c->cx0);
		ftol((vz0-vz1)*cmprecip[leng],&gi1); ftol(vz1*CMPPREC,&c->cy0);
	}
	c->cx1 = leng*gi0 + c->cx0;
	c->cy1 = leng*gi1 + c->cy0;

	gxmax = gmaxscandist;

		//Hack for early-out case when looking up towards sky
#if 0  //DOESN'T WORK WITH LOWER MIPS!
	if (c->cy1 < 0)
		if (gposz > 0)
		{
			if (dmulrethigh(-gposz,c->cx1,c->cy1,gxmax) >= 0)
			{
				j = scale(-gposz,c->cx1,c->cy1)+PREC; //+PREC for good luck
				if ((unsigned long)j < (unsigned long)gxmax) gxmax = j;
			}
		} else gxmax = 0;
#endif

		//Clip borders safely (MUST use integers!) - don't wrap around
#if ((USEZBUFFER == 1) && (defined(USEV5ASM) && (USEV5ASM != 0))) //if USEV5ASM is true
	skycast.dist = gxmax;
#endif
	if (gixy[0] < 0) j = glipos.x; else j = VSID-1-glipos.x;
	q = mul64(gdz[0],j); q += (uint64_t)gpz[0];
	if (q < (uint64_t)gxmax)
	{
		gxmax = (long)q;
#if ((USEZBUFFER == 1) && (defined(USEV5ASM) && (USEV5ASM != 0)))
		skycast.dist = 0x7fffffff;
#endif
	}
	if (gixy[1] < 0) j = glipos.y; else j = VSID-1-glipos.y;
	q = mul64(gdz[1],j); q += (uint64_t)gpz[1];
	if (q < (uint64_t)gxmax)
	{
		gxmax = (long)q;
#if ((USEZBUFFER == 1) && (defined(USEV5ASM) && (USEV5ASM != 0)))
		skycast.dist = 0x7fffffff;
#endif
	}

	if (vx5.sideshademode)
	{
		gcsub[0] = gcsub[(((unsigned long)gixy[0])>>31)+4];
		gcsub[1] = gcsub[(((unsigned long)gixy[1])>>31)+6];
	}

#if (defined(USEV5ASM) && (USEV5ASM != 0)) //if true
	if (nskypic)
	{
		if (skycurlng < 0)
		{
			ftol((atan2(vy1,vx1)+PI)*skylngmul-.5,&skycurlng);
			if ((unsigned long)skycurlng >= skyysiz)
				skycurlng = ((skyysiz-1)&(j>>31));
		}
		else if (skycurdir < 0)
		{
			j = skycurlng+1; if (j >= skyysiz) j = 0;
			while (skylng[j].x*vy1 > skylng[j].y*vx1)
				{ skycurlng = j++; if (j >= skyysiz) j = 0; }
		}
		else
		{
			while (skylng[skycurlng].x*vy1 < skylng[skycurlng].y*vx1)
				{ skycurlng--; if (skycurlng < 0) skycurlng = skyysiz-1; }
		}
		skyoff = skycurlng*skybpl + nskypic;
	}

	//resp = 0;
	grouscanasm((long)gstartv);
	//if (resp)
	//{
	//   static char tempbuf[2048], tempbuf2[256];
	//   sprintf(tempbuf,"eax:%08x\tmm0:%08x%08x\nebx:%08x\tmm1:%08x%08x\necx:%08x\tmm2:%08x%08x\nedx:%08x\tmm3:%08x%08x\nesi:%08x\tmm4:%08x%08x\nedi:%08x\tmm5:%08x%08x\nebp:%08x\tmm6:%08x%08x\nesp:%08x\tmm7:%08x%08x\n",
	//      reax,remm[ 1],remm[ 0], rebx,remm[ 3],remm[ 2],
	//      recx,remm[ 5],remm[ 4], redx,remm[ 7],remm[ 6],
	//      resi,remm[ 9],remm[ 8], redi,remm[11],remm[10],
	//      rebp,remm[13],remm[12], resp,remm[15],remm[14]);
	//
	//   for(j=0;j<3;j++)
	//   {
	//      sprintf(tempbuf2,"%d i0:%d i1:%d z0:%ld z1:%ld cx0:%08x cy0:%08x cx1:%08x cy1:%08x\n",
	//         j,(long)cfasm[j].i0-(long)gscanptr,(long)cfasm[j].i1-(long)gscanptr,cfasm[j].z0,cfasm[j].z1,cfasm[j].cx0,cfasm[j].cy0,cfasm[j].cx1,cfasm[j].cy1);
	//      strcat(tempbuf,tempbuf2);
	//   }
	//   evilquit(tempbuf);
	//}
#else
//------------------------------------------------------------------------
	ce = c; v = gstartv;
	j = (((unsigned long)(gpz[1]-gpz[0]))>>31);
	gx = gpz[j];
	ixy = gpixy;
	if (v == (char *)*(long *)gpixy) goto drawflor; goto drawceil;

	while (1)
	{

drawfwall:;
		if (v[1] != c->z1)
		{
			if (v[1] > c->z1) c->z1 = v[1];
			else { do
			{
				c->z1--; col = *(long *)&v[(c->z1-v[1])*4+4];
				while (dmulrethigh(gylookup[c->z1],c->cx1,c->cy1,ogx) < 0)
				{
					c->i1->col = col; c->i1--; if (c->i0 > c->i1) goto deletez;
					c->cx1 -= gi0; c->cy1 -= gi1;
				}
			} while (v[1] != c->z1); }
		}

		if (v == (char *)*(long *)ixy) goto drawflor;

//drawcwall:;
		if (v[3] != c->z0)
		{
			if (v[3] < c->z0) c->z0 = v[3];
			else { do
			{
				c->z0++; col = *(long *)&v[(c->z0-v[3])*4-4];
				while (dmulrethigh(gylookup[c->z0],c->cx0,c->cy0,ogx) >= 0)
				{
					c->i0->col = col; c->i0++; if (c->i0 > c->i1) goto deletez;
					c->cx0 += gi0; c->cy0 += gi1;
				}
			} while (v[3] != c->z0); }
		}

drawceil:;
		while (dmulrethigh(gylookup[c->z0],c->cx0,c->cy0,gx) >= 0)
		{
			c->i0->col = (*(long *)&v[-4]); c->i0++; if (c->i0 > c->i1) goto deletez;
			c->cx0 += gi0; c->cy0 += gi1;
		}

drawflor:;
		while (dmulrethigh(gylookup[c->z1],c->cx1,c->cy1,gx) < 0)
		{
			c->i1->col = *(long *)&v[4]; c->i1--; if (c->i0 > c->i1) goto deletez;
			c->cx1 -= gi0; c->cy1 -= gi1;
		}

afterdelete:;
		c--;
		if (c < &cfasm[128])
		{
			ixy += gixy[j];
			gpz[j] += gdz[j];
			j = (((unsigned long)(gpz[1]-gpz[0]))>>31);
			ogx = gx; gx = gpz[j];

			if (gx > gxmax) break;
			v = (char *)*(long *)ixy; c = ce;
		}
			//Find highest intersecting vbuf slab
		while (1)
		{
			if (!v[0]) goto drawfwall;
			if (dmulrethigh(gylookup[v[2]+1],c->cx0,c->cy0,ogx) >= 0) break;
			v += v[0]*4;
		}
			//If next slab ALSO intersects, split cfasm!
		gy = gylookup[v[v[0]*4+3]];
		if (dmulrethigh(gy,c->cx1,c->cy1,ogx) < 0)
		{
			col = (long)c->i1; dax = c->cx1; day = c->cy1;
			while (dmulrethigh(gylookup[v[2]+1],dax,day,ogx) < 0)
				{ col -= sizeof(castdat); dax -= gi0; day -= gi1; }
			ce++; if (ce >= &cfasm[192]) return; //Give it max=64 entries like ASM
			for(c2=ce;c2>c;c2--) c2[0] = c2[-1];
			c[1].i1 = (castdat *)col; c->i0 = ((castdat *)col)+1;
			c[1].cx1 = dax; c->cx0 = dax+gi0;
			c[1].cy1 = day; c->cy0 = day+gi1;
			c[1].z1 = c->z0 = v[v[0]*4+3];
			c++;
		}
	}
//------------------------------------------------------------------------

	for(c=ce;c>=&cfasm[128];c--)
		while (c->i0 <= c->i1) { c->i0->col = 0; c->i0++; }
	return;

deletez:;
	ce--; if (ce < &cfasm[128]) return;
	for(c2=c;c2<=ce;c2++) c2[0] = c2[1];
	goto afterdelete;
#endif
}

/** add b to *a and clamp to maximum char val of 255 */
static inline void addusb (char *a, long b)
{
	(*a) += b; if ((*a) < b) (*a) = 255;
}

	// (cone diameter vs. % 3D angular area) or: (a vs. 2/(1-cos(a*.5*PI/180)))
	// ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄ¿
	// ³  0: inf     ³ 25: 84.37 ³ 50: 21.35 ³ 75: 9.68 ³ 100: 5.60 ³ 180: 2  ³
	// ³  5: 2101.33 ³ 30: 58.70 ³ 55: 17.70 ³ 80: 8.55 ³ 105: 5.11 ³ 360: 1  ³
	// ³ 10:  525.58 ³ 35: 43.21 ³ 60: 14.93 ³ 85: 7.61 ³ 110: 4.69 ÃÄÄÄÄÄÄÄÄÄÙ
	// ³ 15:  233.78 ³ 40: 33.16 ³ 65: 12.77 ³ 90: 6.83 ³ 115: 4.32 ³
	// ³ 20:  131.65 ³ 45: 26.27 ³ 70: 11.06 ³ 95: 6.17 ³ 120: 4    ³
	// ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÁÄÄÄÄÄÄÄÄÄÄÄÁÄÄÄÄÄÄÄÄÄÄÄÁÄÄÄÄÄÄÄÄÄÄÁÄÄÄÄÄÄÄÄÄÄÄÙ
void setflash (float px, float py, float pz, long flashradius, long numang, long intens)
{
	uint64_t q;
	float vx, vy;
	long i, j, gx, ogx, ixy, col, angoff;
	long ipx, ipy, ipz, sz0, sz1;
	cftype *c, *c2, *ce;
	char *v, *vs;

	ipx = (long)px; ipy = (long)py; ipz = (long)pz;
	vx5.minx = ipx-flashradius; vx5.maxx = ipx+flashradius+1;
	vx5.miny = ipy-flashradius; vx5.maxy = ipy+flashradius+1;
	vx5.minz = ipz-flashradius; vx5.maxz = ipz+flashradius+1;

	if (flashradius > 2047) flashradius = 2047;
	flashradius *= FPREC;

	flashbrival = (intens<<24);
	angoff = ((0x52741630>>(flashcnt<<2))&15); flashcnt++;

	gposxfrac[1] = px - (float)(ipx); gposxfrac[0] = 1 - gposxfrac[1];
	gposyfrac[1] = py - (float)(ipy); gposyfrac[0] = 1 - gposyfrac[1];
	gpixy = (long)&sptr[ipy*VSID + ipx];
	ftol(pz*FPREC-.5f,&gposz);
	for(gylookup[0]=-gposz,i=1;i<260;i++) gylookup[i] = gylookup[i-1]+FPREC;

	vs = (char *)*(long *)gpixy;
	if (ipz >= vs[1])
	{
		do
		{
			if (!vs[0]) return;
			vs += vs[0]*4;
		} while (ipz >= vs[1]);
		if (ipz < vs[3]) return;
		sz0 = vs[3];
	} else sz0 = 0;
	sz1 = vs[1];

	for(i=0;i<numang;i++)
	{
		clearMMX();

		fcossin(((float)i+(float)angoff*.125f)*PI*2.0f/(float)numang,&vx,&vy);

		ftol(FPREC/fabs(vx),&gdz[0]);
		ftol(FPREC/fabs(vy),&gdz[1]);

		gixy[0] = (((*(signed long *)&vx)>>31) & (     -8)) +      4;
		gixy[1] = (((*(signed long *)&vy)>>31) & (VSID*-8)) + VSID*4;
		if (gdz[0] < 0) { gpz[0] = 0x7fffffff; gdz[0] = 0; } //Hack for divide overflow
		else ftol(gposxfrac[(*(unsigned long *)&vx)>>31]*(float)gdz[0],&gpz[0]);
		if (gdz[1] < 0) { gpz[1] = 0x7fffffff; gdz[1] = 0; } //Hack for divide overflow
		else ftol(gposyfrac[(*(unsigned long *)&vy)>>31]*(float)gdz[1],&gpz[1]);

		c = ce = &cfasm[128];
		v = vs; c->z0 = sz0; c->z1 = sz1;
			//Note!  These substitions are used in flashscan:
			//   c->i0 in flashscan is now: c->cx0
			//   c->i1 in flashscan is now: c->cx1
		c->cx0 = (((i+flashcnt+rand())&7)<<LOGFLASHVANG);
		c->cx1 = c->cx0+(1<<LOGFLASHVANG)-1;

		gxmax = flashradius;

			//Clip borders safely (MUST use integers!) - don't wrap around
		if (gixy[0] < 0) j = ipx; else j = VSID-1-ipx;
		q = mul64(gdz[0],j); q += (uint64_t)gpz[0];
		if (q < (uint64_t)gxmax) gxmax = (long)q;
		if (gixy[1] < 0) j = ipy; else j = VSID-1-ipy;
		q = mul64(gdz[1],j); q += (uint64_t)gpz[1];
		if (q < (uint64_t)gxmax) gxmax = (long)q;

	//------------------------------------------------------------------------
		j = (((unsigned long)(gpz[1]-gpz[0]))>>31);
		gx = gpz[j];
		ixy = gpixy;
		if (v == (char *)*(long *)gpixy) goto fdrawflor; goto fdrawceil;

		while (1)
		{

fdrawfwall:;
			if (v[1] != c->z1)
			{
				if (v[1] > c->z1) c->z1 = v[1];
				else { do
				{
					c->z1--; col = (long)&v[(c->z1-v[1])*4+4];
					while (dmulrethigh(gylookup[c->z1],gfc[c->cx1].x,gfc[c->cx1].y,ogx) < 0)
					{
						mmxcoloradd((long *)col); c->cx1--;
						if (c->cx0 > c->cx1) goto fdeletez;
					}
				} while (v[1] != c->z1); }
			}

			if (v == (char *)*(long *)ixy) goto fdrawflor;

//fdrawcwall:;
			if (v[3] != c->z0)
			{
				if (v[3] < c->z0) c->z0 = v[3];
				else { do
				{
					c->z0++; col = (long)&v[(c->z0-v[3])*4-4];
					while (dmulrethigh(gylookup[c->z0],gfc[c->cx0].x,gfc[c->cx0].y,ogx) >= 0)
					{
						mmxcoloradd((long *)col); c->cx0++;
						if (c->cx0 > c->cx1) goto fdeletez;
					}
				} while (v[3] != c->z0); }
			}

fdrawceil:;
			while (dmulrethigh(gylookup[c->z0],gfc[c->cx0].x,gfc[c->cx0].y,gx) >= 0)
			{
				mmxcoloradd((long *)&v[-4]); c->cx0++;
				if (c->cx0 > c->cx1) goto fdeletez;
			}

fdrawflor:;
			while (dmulrethigh(gylookup[c->z1],gfc[c->cx1].x,gfc[c->cx1].y,gx) < 0)
			{
				mmxcoloradd((long *)&v[4]); c->cx1--;
				if (c->cx0 > c->cx1) goto fdeletez;
			}

fafterdelete:;
			c--;
			if (c < &cfasm[128])
			{
				ixy += gixy[j];
				gpz[j] += gdz[j];
				j = (((unsigned long)(gpz[1]-gpz[0]))>>31);
				ogx = gx; gx = gpz[j];

				if (gx > gxmax) break;
				v = (char *)*(long *)ixy; c = ce;
			}
				//Find highest intersecting vbuf slab
			while (1)
			{
				if (!v[0]) goto fdrawfwall;
				if (dmulrethigh(gylookup[v[2]+1],gfc[c->cx0].x,gfc[c->cx0].y,ogx) >= 0) break;
				v += v[0]*4;
			}
				//If next slab ALSO intersects, split cfasm!
			if (dmulrethigh(gylookup[v[v[0]*4+3]],gfc[c->cx1].x,gfc[c->cx1].y,ogx) < 0)
			{
				col = c->cx1;
				while (dmulrethigh(gylookup[v[2]+1],gfc[col].x,gfc[col].y,ogx) < 0)
					col--;
				ce++; if (ce >= &cfasm[192]) break; //Give it max=64 entries like ASM
				for(c2=ce;c2>c;c2--) c2[0] = c2[-1];
				c[1].cx1 = col; c->cx0 = col+1;
				c[1].z1 = c->z0 = v[v[0]*4+3];
				c++;
			}
		}
fcontinue:;
	}

	clearMMX();
	updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,0);
	return;

fdeletez:;
	ce--; if (ce < &cfasm[128]) goto fcontinue;
	for(c2=c;c2<=ce;c2++) c2[0] = c2[1];
	goto fafterdelete;
}

#if (ESTNORMRAD == 2)
/** Used by estnorn */
static signed char bitnum[32] =
{
	0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4,
	1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5
};
//static long bitsum[32] =
//{
//   0,-2,-1,-3, 0,-2,-1,-3, 1,-1, 0,-2, 1,-1, 0,-2,
//   2, 0, 1,-1, 2, 0, 1,-1, 3, 1, 2, 0, 3, 1, 2, 0
//};
/** Used by estnorn */
static long bitsnum[32] =
{
	0        ,1-(2<<16),1-(1<<16),2-(3<<16),
	1        ,2-(2<<16),2-(1<<16),3-(3<<16),
	1+(1<<16),2-(1<<16),2        ,3-(2<<16),
	2+(1<<16),3-(1<<16),3        ,4-(2<<16),
	1+(2<<16),2        ,2+(1<<16),3-(1<<16),
	2+(2<<16),3        ,3+(1<<16),4-(1<<16),
	2+(3<<16),3+(1<<16),3+(2<<16),4,
	3+(3<<16),4+(1<<16),4+(2<<16),5
};
static float fsqrecip[5860]; //75*75 + 15*15 + 3*3 = 5859 is max value (5*5*5 box)
#endif

void estnorm (long x, long y, long z, point3d *fp)
{
	lpoint3d n;
	long *lptr, xx, yy, zz, b[5], i, j, k;
	float f;

	n.x = 0; n.y = 0; n.z = 0;

#if (ESTNORMRAD == 2)
	if (labs(x-xbsox) + labs(y-xbsoy) > 1)
	{
			//x,y not close enough to cache: calls expandbitstack 25 times :(
		xbsox = x; xbsoy = y; xbsof = 24*5;
		lptr = (long *)(&xbsbuf[24*5+1]);
		for(yy=-2;yy<=2;yy++)
			for(xx=-2;xx<=2;xx++,lptr-=10)
				expandbitstack(x+xx,y+yy,(int64_t *)lptr);
	}
	else if (x != xbsox)
	{
			//shift xbsbuf cache left/right: calls expandbitstack 5 times :)
		if (x < xbsox) { xx = -2; xbsof -= 24*5; lptr = (long *)(&xbsbuf[xbsof+1]); }
					 else { xx = 2; lptr = (long *)(&xbsbuf[xbsof-5*5+1]); xbsof -= 1*5; }
		xbsox = x; if (xbsof < 0) xbsof += 25*5;
		for(yy=-2;yy<=2;yy++)
		{
			if (lptr < (long *)&xbsbuf[1]) lptr += 25*10;
			expandbitstack(x+xx,y+yy,(int64_t *)lptr);
			lptr -= 5*10;
		}
	}
	else if (y != xbsoy)
	{
			//shift xbsbuf cache up/down: calls expandbitstack 5 times :)
		if (y < xbsoy) { yy = -2; xbsof -= 20*5; lptr = (long *)(&xbsbuf[xbsof+1]); }
					 else { yy = 2; lptr = (long *)(&xbsbuf[xbsof+1]); xbsof -= 5*5; }
		xbsoy = y; if (xbsof < 0) xbsof += 25*5;
		for(xx=-2;xx<=2;xx++)
		{
			if (lptr < (long *)&xbsbuf[1]) lptr += 25*10;
			expandbitstack(x+xx,y+yy,(int64_t *)lptr);
			lptr -= 1*10;
		}
	}

	z -= 2;
	if ((z&31) <= 27) //2 <= (z&31) <= 29
		{ lptr = (long *)((long)(&xbsbuf[xbsof+1]) + ((z&~31)>>3)); z &= 31; }
	else
		{ lptr = (long *)((long)(&xbsbuf[xbsof+1]) + (z>>3)); z &= 7; }

	for(yy=-2;yy<=2;yy++)
	{
		if (lptr >= (long *)&xbsbuf[1+10*5])
		{
			b[0] = ((lptr[  0]>>z)&31); b[1] = ((lptr[-10]>>z)&31);
			b[2] = ((lptr[-20]>>z)&31); b[3] = ((lptr[-30]>>z)&31);
			b[4] = ((lptr[-40]>>z)&31); lptr -= 50;
		}
		else
		{
			b[0] = ((lptr[0]>>z)&31); lptr -= 10; if (lptr < (long *)&xbsbuf[1]) lptr += 25*10;
			b[1] = ((lptr[0]>>z)&31); lptr -= 10; if (lptr < (long *)&xbsbuf[1]) lptr += 25*10;
			b[2] = ((lptr[0]>>z)&31); lptr -= 10; if (lptr < (long *)&xbsbuf[1]) lptr += 25*10;
			b[3] = ((lptr[0]>>z)&31); lptr -= 10; if (lptr < (long *)&xbsbuf[1]) lptr += 25*10;
			b[4] = ((lptr[0]>>z)&31); lptr -= 10; if (lptr < (long *)&xbsbuf[1]) lptr += 25*10;
		}

			//Make filter spherical
		//if (yy&1) { b[0] &= 0xe; b[4] &= 0xe; }
		//else if (yy) { b[0] &= 0x4; b[1] &= 0xe; b[3] &= 0xe; b[4] &= 0x4; }

		n.x += ((bitnum[b[4]]-bitnum[b[0]])<<1)+bitnum[b[3]]-bitnum[b[1]];
		j = bitsnum[b[0]]+bitsnum[b[1]]+bitsnum[b[2]]+bitsnum[b[3]]+bitsnum[b[4]];
		n.z += j; n.y += (*(signed short *)&j)*yy;
	}
	n.z >>= 16;
#else
	for(yy=-ESTNORMRAD;yy<=ESTNORMRAD;yy++)
		for(xx=-ESTNORMRAD;xx<=ESTNORMRAD;xx++)
			for(zz=-ESTNORMRAD;zz<=ESTNORMRAD;zz++)
				if (isvoxelsolid(x+xx,y+yy,z+zz))
					{ n.x += xx; n.y += yy; n.z += zz; }
#endif

#if 1
	f = fsqrecip[n.x*n.x + n.y*n.y + n.z*n.z];
	fp->x = ((float)n.x)*f; fp->y = ((float)n.y)*f; fp->z = ((float)n.z)*f;
#else

		//f = 1.0 / sqrt((double)(n.x*n.x + n.y*n.y + n.z*n.z));
		//fp->x = f*(float)n.x; fp->y = f*(float)n.y; fp->z = f*(float)n.z;
	zz = n.x*n.x + n.y*n.y + n.z*n.z;
	if (cputype&(1<<25))
	{
		#ifdef __GNUC__ //gcc inline asm
		__asm__ __volatile__
		(
			"cvtsi2ss	zz,%xmm0\n"
			"rsqrtss	%xmm0,%xmm0\n"
			//"movss	%xmm0, f\n"

				//fp->x = f*(float)n.x; fp->y = f*(float)n.y; fp->z = f*(float)n.z;\n"
			"cvtsi2ss	%xmm1,n.z\n"
			"shufps	0,%xmm0,%xmm0\n"
			"mov	fp,%eax\n"
			"movlhps	%xmm1,%xmm1\n"
			"cvtpi2ps	n,%xmm1\n"
			"mulps	%xmm1,%xmm0\n"
			"movlps	%xmm0,(%eax)\n"
			"movhlps	%xmm0,%xmm0\n"
			"movss	%xmm0,8(%eax)\n"
		);
		#endif
		#ifdef _MSC_VER //msvc inline asm
		_asm
		{
			cvtsi2ss	xmm0, zz
			rsqrtss	xmm0, xmm0
			//movss	f, xmm0

				//fp->x = f*(float)n.x; fp->y = f*(float)n.y; fp->z = f*(float)n.z;
			cvtsi2ss	xmm1, n.z
			shufps	xmm0, xmm0, 0
			mov	eax, fp
			movlhps	xmm1, xmm1
			cvtpi2ps	xmm1, n
			mulps	xmm0, xmm1
			movlps	[eax], xmm0
			movhlps	xmm0, xmm0
			movss	[eax+8], xmm0
		}
		#endif
	}
	else
	{
		#ifdef NOASM
		#endif
		#ifdef __GNUC__ //gcc inline asm
		__asm__ __volatile__
		(
			"pi2fd	zz,%mm0\n"       //mm0:     0          zz
			"pfrsqrt	%mm0,%mm0\n" //mm0: 1/sqrt(zz) 1/sqrt(zz)
			"pi2fd	n.x,%mm1\n"      //mm1:     0         n.x
			"pi2fd	n.y,%mm2\n"      //mm2:     0         n.y
			"punpckldq	%mm2,%mm1\n" //mm1:    n.y        n.x
			"pi2fd	n.z,%mm2\n"      //mm2:     0         n.z
			"pfmul	%mm0,%mm1\n"     //mm1:n.y/sqrt(zz) n.x/sqrt(zz)
			"pfmul	%mm0,%mm2\n"     //mm2:     0       n.z/sqrt(zz)
			"mov	fp,%eax\n"
			"movq	%mm1,(%eax)\n"
			"movl	%mm2,8(%eax)\n"
			"femms\n"
		);
		#endif
		#ifdef _MSC_VER //msvc inline asm
		_asm
		{
			pi2fd	mm0, zz      //mm0:      0          zz
			pfrsqrt	mm0, mm0     //mm0:  1/sqrt(zz) 1/sqrt(zz)
			pi2fd	mm1, n.x     //mm1:      0         n.x
			pi2fd	mm2, n.y     //mm2:      0         n.y
			punpckldq	mm1, mm2 //mm1:     n.y        n.x
			pi2fd	mm2, n.z     //mm2:      0         n.z
			pfmul	mm1, mm0     //mm1: n.y/sqrt(zz) n.x/sqrt(zz)
			pfmul	mm2, mm0     //mm2:      0       n.z/sqrt(zz)
			mov	eax, fp
			movq	[eax], mm1
			movd	[eax+8], mm2
			femms
		}
		#endif
	}
#endif
}

static long vspan (long x, long y0, long y1)
{
	long y, yy, *bbufx;

	y = (y0>>5); bbufx = &bbuf[x][0];
	if ((y1>>5) == y)
	{
		yy = bbufx[y]; bbufx[y] &= ~(p2m[y1&31]^p2m[y0&31]);
		return(bbufx[y] ^ yy);
	}

	if (!(bbufx[y]&(~p2m[y0&31])))
		if (!(bbufx[y1>>5]&p2m[y1&31]))
		{
			for(yy=(y1>>5)-1;yy>y;yy--)
				if (bbufx[yy]) goto vspan_skip;
			return(0);
		}
vspan_skip:;
	bbufx[y] &= p2m[y0&31];
	bbufx[y1>>5] &= (~p2m[y1&31]);
	for(yy=(y1>>5)-1;yy>y;yy--) bbufx[yy] = 0;
	return(1);
}

static long docube (long x, long y, long z)
{
	long x0, y0, x1, y1, g;

	ffxptr = &ffx[(z+1)*z-1];
	x0 = (long)ffxptr[x].x; x1 = (long)ffxptr[x].y;
	y0 = (long)ffxptr[y].x; y1 = (long)ffxptr[y].y;
	for(g=0;x0<x1;x0++) g |= vspan(x0,y0,y1);
	return(g);
}

void setnormflash (float px, float py, float pz, long flashradius, long intens)
{
	point3d fp;
	float f, fintens;
	long i, j, k, l, m, x, y, z, xx, yy, xi, yi, xe, ye, ipx, ipy, ipz;
	long ceilnum, sq;
	char *v;

	ipx = (long)px; ipy = (long)py; ipz = (long)pz;
	vx5.minx = ipx-flashradius+1; vx5.maxx = ipx+flashradius;
	vx5.miny = ipy-flashradius+1; vx5.maxy = ipy+flashradius;
	vx5.minz = ipz-flashradius+1; vx5.maxz = ipz+flashradius;

	if (isvoxelsolid(ipx,ipy,ipz)) return;

	fintens = intens;
	if (flashradius > (GSIZ>>1)) flashradius = (GSIZ>>1);

	xbsox = -17;

		//       ÚÄ 7Ä¿
		//      11  . 8
		//  ÚÄ11ÄÅÄ 4ÄÅÄ 8ÄÂÄ 7Ä¿
		//  3 |  0  + 1  | 2  + 3
		//  ÀÄ10ÄÅÄ 5ÄÅÄ 9ÄÁÄ 6ÄÙ
		//      10  . 9
		//       ÀÄ 6ÄÙ

		//Do left&right faces of the cube
	for(j=1;j>=0;j--)
	{
		clearbuf((void *)bbuf,GSIZ*(GSIZ>>5),0xffffffff);
		for(y=1;y<flashradius;y++)
		{
			if (j) yy = ipy-y; else yy = ipy+y;
			for(xi=1,xe=y+1;xi>=-1;xi-=2,xe=-xe)
				for(x=(xi>>1);x!=xe;x+=xi)
				{
					xx = ipx+x;
					if ((unsigned long)(xx|yy) >= VSID) continue;
					v = sptr[yy*VSID+xx]; i = 0; sq = x*x+y*y;
					while (1)
					{
						for(z=v[1];z<=v[2];z++)
						{
							if (z-ipz < 0) { tbuf2[i] = z-ipz; tbuf2[i+1] = (long)&v[(z-v[1])*4+4]; i += 2; }
							else
							{
								//if (z-ipz < -y) continue; //TEMP HACK!!!
								if (z-ipz > y) goto normflash_exwhile1;
								if (!docube(x,z-ipz,y)) continue;
								estnorm(xx,yy,z,&fp); if (j) fp.y = -fp.y;
								f = fp.x*x + fp.y*y + fp.z*(z-ipz);
								if (*(long *)&f > 0) addusb(&v[(z-v[1])*4+7],f*fintens/((z-ipz)*(z-ipz)+sq));
							}
						}
						if (!v[0]) break;
						ceilnum = v[2]-v[1]-v[0]+2; v += v[0]*4;
						for(z=v[3]+ceilnum;z<v[3];z++)
						{
							if (z < ipz) { tbuf2[i] = z-ipz; tbuf2[i+1] = (long)&v[(z-v[3])*4]; i += 2; }
							else
							{
								//if (z-ipz < -y) continue; //TEMP HACK!!!
								if (z-ipz > y) goto normflash_exwhile1;
								if (!docube(x,z-ipz,y)) continue;
								estnorm(xx,yy,z,&fp); if (j) fp.y = -fp.y;
								f = fp.x*x + fp.y*y + fp.z*(z-ipz);
								if (*(long *)&f > 0) addusb(&v[(z-v[3])*4+3],f*fintens/((z-ipz)*(z-ipz)+sq));
							}
						}
					}
normflash_exwhile1:;
					while (i > 0)
					{
						i -= 2; if (tbuf2[i] < -y) break;
						if (!docube(x,tbuf2[i],y)) continue;
						estnorm(xx,yy,tbuf2[i]+ipz,&fp); if (j) fp.y = -fp.y;
						f = fp.x*x + fp.y*y + fp.z*tbuf2[i];
						if (*(long *)&f > 0) addusb(&((char *)tbuf2[i+1])[3],f*fintens/(tbuf2[i]*tbuf2[i]+sq));
					}
				}
		}
	}

		//Do up&down faces of the cube
	for(j=1;j>=0;j--)
	{
		clearbuf((void *)bbuf,GSIZ*(GSIZ>>5),0xffffffff);
		for(y=1;y<flashradius;y++)
		{
			if (j) xx = ipx-y; else xx = ipx+y;
			for(xi=1,xe=y+1;xi>=-1;xi-=2,xe=-xe)
				for(x=(xi>>1);x!=xe;x+=xi)
				{
					yy = ipy+x;
					if ((unsigned long)(xx|yy) >= VSID) continue;
					v = sptr[yy*VSID+xx]; i = 0; sq = x*x+y*y; m = x+xi-xe;
					while (1)
					{
						for(z=v[1];z<=v[2];z++)
						{
							if (z-ipz < 0) { tbuf2[i] = z-ipz; tbuf2[i+1] = (long)&v[(z-v[1])*4+4]; i += 2; }
							else
							{
								//if (z-ipz < -y) continue; //TEMP HACK!!!
								if (z-ipz > y) goto normflash_exwhile2;
								if ((!docube(x,z-ipz,y)) || (!m)) continue;
								estnorm(xx,yy,z,&fp); if (j) fp.x = -fp.x;
								f = fp.x*y + fp.y*x + fp.z*(z-ipz);
								if (*(long *)&f > 0) addusb(&v[(z-v[1])*4+7],f*fintens/((z-ipz)*(z-ipz)+sq));
							}
						}
						if (!v[0]) break;
						ceilnum = v[2]-v[1]-v[0]+2; v += v[0]*4;
						for(z=v[3]+ceilnum;z<v[3];z++)
						{
							if (z < ipz) { tbuf2[i] = z-ipz; tbuf2[i+1] = (long)&v[(z-v[3])*4]; i += 2; }
							else
							{
								//if (z-ipz < -y) continue; //TEMP HACK!!!
								if (z-ipz > y) goto normflash_exwhile2;
								if ((!docube(x,z-ipz,y)) || (!m)) continue;
								estnorm(xx,yy,z,&fp); if (j) fp.x = -fp.x;
								f = fp.x*y + fp.y*x + fp.z*(z-ipz);
								if (*(long *)&f > 0) addusb(&v[(z-v[3])*4+3],f*fintens/((z-ipz)*(z-ipz)+sq));
							}
						}
					}
normflash_exwhile2:;
					while (i > 0)
					{
						i -= 2; if (tbuf2[i] < -y) break;
						if ((!docube(x,tbuf2[i],y)) || (!m)) continue;
						estnorm(xx,yy,tbuf2[i]+ipz,&fp); if (j) fp.x = -fp.x;
						f = fp.x*y + fp.y*x + fp.z*tbuf2[i];
						if (*(long *)&f > 0) addusb(&((char *)tbuf2[i+1])[3],f*fintens/(tbuf2[i]*tbuf2[i]+sq));
					}
				}
		}
	}

		//Do the bottom face of the cube
	clearbuf((void *)bbuf,GSIZ*(GSIZ>>5),0xffffffff);
	for(yi=1,ye=flashradius+1;yi>=-1;yi-=2,ye=-ye)
		for(y=(yi>>1);y!=ye;y+=yi)
			for(xi=1,xe=flashradius+1;xi>=-1;xi-=2,xe=-xe)
				for(x=(xi>>1);x!=xe;x+=xi)
				{
					xx = ipx+x; yy = ipy+y;
					if ((unsigned long)(xx|yy) >= VSID) goto normflash_exwhile3;
					k = MAX(labs(x),labs(y));

					v = sptr[yy*VSID+xx]; sq = x*x+y*y;
					while (1)
					{
						for(z=v[1];z<=v[2];z++)
						{
							if (z-ipz < k) continue;
							if (z-ipz >= flashradius) goto normflash_exwhile3;
							if ((!docube(x,y,z-ipz)) || (z-ipz == k)) continue;
							estnorm(xx,yy,z,&fp);
							f = fp.x*x + fp.y*y + fp.z*(z-ipz);
							if (*(long *)&f > 0) addusb(&v[(z-v[1])*4+7],f*fintens/((z-ipz)*(z-ipz)+sq));
						}
						if (!v[0]) break;
						ceilnum = v[2]-v[1]-v[0]+2; v += v[0]*4;
						for(z=v[3]+ceilnum;z<v[3];z++)
						{
							if (z-ipz < k) continue;
							if (z-ipz >= flashradius) goto normflash_exwhile3;
							if ((!docube(x,y,z-ipz)) || (z-ipz <= k)) continue;
							estnorm(xx,yy,z,&fp);
							f = fp.x*x + fp.y*y + fp.z*(z-ipz);
							if (*(long *)&f > 0) addusb(&v[(z-v[3])*4+3],f*fintens/((z-ipz)*(z-ipz)+sq));
						}
					}
normflash_exwhile3:;
				}


		//Do the top face of the cube
	clearbuf((void *)bbuf,GSIZ*(GSIZ>>5),0xffffffff);
	for(yi=1,ye=flashradius+1;yi>=-1;yi-=2,ye=-ye)
		for(y=(yi>>1);y!=ye;y+=yi)
			for(xi=1,xe=flashradius+1;xi>=-1;xi-=2,xe=-xe)
				for(x=(xi>>1);x!=xe;x+=xi)
				{
					xx = ipx+x; yy = ipy+y;
					if ((unsigned long)(xx|yy) >= VSID) goto normflash_exwhile4;
					k = MAX(labs(x),labs(y)); m = ((x+xi != xe) && (y+yi != ye));

					v = sptr[yy*VSID+xx]; i = 0; sq = x*x+y*y;
					while (1)
					{
						for(z=v[1];z<=v[2];z++)
						{
							if (ipz-z >= flashradius) continue;
							if (ipz-z < k) goto normflash_exwhile4;
							tbuf2[i] = ipz-z; tbuf2[i+1] = (long)&v[(z-v[1])*4+4]; i += 2;
						}
						if (!v[0]) break;
						ceilnum = v[2]-v[1]-v[0]+2; v += v[0]*4;
						for(z=v[3]+ceilnum;z<v[3];z++)
						{
							if (ipz-z >= flashradius) continue;
							if (ipz-z < k) goto normflash_exwhile4;
							tbuf2[i] = ipz-z; tbuf2[i+1] = (long)&v[(z-v[3])*4]; i += 2;
						}
					}
normflash_exwhile4:;
					while (i > 0)
					{
						i -= 2;
						if ((!docube(x,y,tbuf2[i])) || (tbuf2[i] <= k)) continue;
						estnorm(xx,yy,ipz-tbuf2[i],&fp);
						f = fp.x*x + fp.y*y - fp.z*tbuf2[i];
						if (*(long *)&f > 0) addusb(&((char *)tbuf2[i+1])[3],f*fintens/(tbuf2[i]*tbuf2[i]+sq));
					}
				}
	updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,0);
}

void hline (float x0, float y0, float x1, float y1, long *ix0, long *ix1)
{
	float dyx;

	dyx = (y1-y0) * grd; //grd = 1/(x1-x0)

		  if (y0 < wy0) ftol((wy0-y0)/dyx+x0,ix0);
	else if (y0 > wy1) ftol((wy1-y0)/dyx+x0,ix0);
	else ftol(x0,ix0);
		  if (y1 < wy0) ftol((wy0-y0)/dyx+x0,ix1);
	else if (y1 > wy1) ftol((wy1-y0)/dyx+x0,ix1);
	else ftol(x1,ix1);
	if ((*ix0) < iwx0) (*ix0) = iwx0;
	if ((*ix0) > iwx1) (*ix0) = iwx1; //(*ix1) = MIN(MAX(*ix1,wx0),wx1);
	gline(
        labs((*ix1)-(*ix0))
        ,(float)(*ix0),((*ix0)-x1)*dyx + y1
        ,(float)(*ix1),((*ix1)-x1)*dyx + y1
    );
}



void vline (float x0, float y0, float x1, float y1, long *iy0, long *iy1)
{
	float dxy;

	dxy = (x1-x0) * grd; //grd = 1/(y1-y0)

		  if (x0 < wx0) ftol((wx0-x0)/dxy+y0,iy0);
	else if (x0 > wx1) ftol((wx1-x0)/dxy+y0,iy0);
	else ftol(y0,iy0);
		  if (x1 < wx0) ftol((wx0-x0)/dxy+y0,iy1);
	else if (x1 > wx1) ftol((wx1-x0)/dxy+y0,iy1);
	else ftol(y1,iy1);
	if ((*iy0) < iwy0) (*iy0) = iwy0;
	if ((*iy0) > iwy1) (*iy0) = iwy1;
	gline(labs((*iy1)-(*iy0)),((*iy0)-y1)*dxy + x1,(float)(*iy0),
									  ((*iy1)-y1)*dxy + x1,(float)(*iy1));
}

static float optistrx, optistry, optiheix, optiheiy, optiaddx, optiaddy;

static int64_t foglut[2048] FORCE_NAME("foglut"), fogcol;
static long ofogdist = -1;

void (*hrend)(long,long,long,long,long,long);
void (*vrend)(long,long,long,long,long);


#if (USEZBUFFER != 1) //functions without Z Buffer

/** Horizontal render with no zbuffer */
void hrendnoz (long sx, long sy, long p1, long plc, long incr, long j)
{
	sy = ylookup[sy]+frameplace; p1 = sy+(p1<<2); sy += (sx<<2);
	do
	{
		*(long *)sy = angstart[plc>>16][j].col;
		plc += incr; sy += 4;
	} while (sy != p1);
}
/** Vertical render with no zbuffer */
void vrendnoz (long sx, long sy, long p1, long iplc, long iinc)
{
	sy = ylookup[sy]+(sx<<2)+frameplace;
	for(;sx<p1;sx++)
	{
		*(long *)sy = angstart[uurend[sx]>>16][iplc].col;
		uurend[sx] += uurend[sx+MAXXDIM]; sy += 4; iplc += iinc;
	}
}

#else //functions with Z Buffer
/** Horizontal render with zbuffer */
void hrendz (long sx, long sy, long p1, long plc, long incr, long j)
{
	long p0, i; float dirx, diry;
	p0 = ylookup[sy]+(sx<<2)+frameplace;
	p1 = ylookup[sy]+(p1<<2)+frameplace;
	dirx = optistrx*(float)sx + optiheix*(float)sy + optiaddx;
	diry = optistry*(float)sx + optiheiy*(float)sy + optiaddy;
	i = zbufoff;
	do
	{
		*(long *)p0 = angstart[plc>>16][j].col;
		*(float *)(p0+i) = (float)angstart[plc>>16][j].dist/sqrt(dirx*dirx+diry*diry);
		dirx += optistrx; diry += optistry; plc += incr; p0 += 4;
	} while (p0 != p1);
}
/** Vertical render with zbuffer */
void vrendz (long sx, long sy, long p1, long iplc, long iinc)
{
	float dirx, diry; long i, p0;
	p0 = ylookup[sy]+(sx<<2)+frameplace;
	p1 = ylookup[sy]+(p1<<2)+frameplace;
	dirx = optistrx*(float)sx + optiheix*(float)sy + optiaddx;
	diry = optistry*(float)sx + optiheiy*(float)sy + optiaddy;
	i = zbufoff;
	while (p0 < p1)
	{
		*(long *)p0 = angstart[uurend[sx]>>16][iplc].col;
		*(float *)(p0+i) = (float)angstart[uurend[sx]>>16][iplc].dist/sqrt(dirx*dirx+diry*diry);
		dirx += optistrx; diry += optistry; uurend[sx] += uurend[sx+MAXXDIM]; p0 += 4; iplc += iinc; sx++;
	}
}
/** Horizontal render with zbuffer + fog */
void hrendzfog (long sx, long sy, long p1, long plc, long incr, long j)
{
	long p0, i, k, l; float dirx, diry;
	p0 = ylookup[sy]+(sx<<2)+frameplace;
	p1 = ylookup[sy]+(p1<<2)+frameplace;
	dirx = optistrx*(float)sx + optiheix*(float)sy + optiaddx;
	diry = optistry*(float)sx + optiheiy*(float)sy + optiaddy;
	i = zbufoff;
	do
	{
		k = angstart[plc>>16][j].col;
		l = angstart[plc>>16][j].dist;
		l = (foglut[l>>20]&32767);
		*(long *)p0 = ((((( vx5.fogcol     &255)-( k     &255))*l)>>15)    ) +
						  ((((((vx5.fogcol>> 8)&255)-((k>> 8)&255))*l)>>15)<< 8) +
						  ((((((vx5.fogcol>>16)&255)-((k>>16)&255))*l)>>15)<<16)+k;
		*(float *)(p0+i) = (float)angstart[plc>>16][j].dist/sqrt(dirx*dirx+diry*diry);
		dirx += optistrx; diry += optistry; plc += incr; p0 += 4;
	} while (p0 != p1);
}
/** Vertical render with zbuffer + fog */
void vrendzfog (long sx, long sy, long p1, long iplc, long iinc)
{
	float dirx, diry; long i, k, l, p0;
	p0 = ylookup[sy]+(sx<<2)+frameplace;
	p1 = ylookup[sy]+(p1<<2)+frameplace;
	dirx = optistrx*(float)sx + optiheix*(float)sy + optiaddx;
	diry = optistry*(float)sx + optiheiy*(float)sy + optiaddy;
	i = zbufoff;
	while (p0 < p1)
	{
		k = angstart[uurend[sx]>>16][iplc].col;
		l = angstart[uurend[sx]>>16][iplc].dist;
		l = (foglut[l>>20]&32767);
		*(long *)p0 = ((((( vx5.fogcol     &255)-( k     &255))*l)>>15)    ) +
						  ((((((vx5.fogcol>> 8)&255)-((k>> 8)&255))*l)>>15)<< 8) +
						  ((((((vx5.fogcol>>16)&255)-((k>>16)&255))*l)>>15)<<16)+k;
		*(float *)(p0+i) = (float)angstart[uurend[sx]>>16][iplc].dist/sqrt(dirx*dirx+diry*diry);
		dirx += optistrx; diry += optistry; uurend[sx] += uurend[sx+MAXXDIM]; p0 += 4; iplc += iinc; sx++;
	}
}

#endif

/** Set global camera position for future voxlap5 engine calls.
 *  Functions that depend on this include: opticast, drawsprite, spherefill, etc...
 *
 *  The 5th & 6th parameters define the center of the screen projection. This
 *  is the point on the screen that intersects the <ipos + ifor*t> vector.
 *
 *  The last parameter is the focal length - use it to control zoom. If you
 *  want a 90 degree field of view (left to right side of screen), then
 *  set it to half of the screen's width: (xdim*.5).
 *
 *  @param ipo camera position
 *  @param ist camera's unit RIGHT vector
 *  @param ihe camera's unit DOWN vector
 *  @param ifo camera's unit FORWARD vector
 *  @param dahx x-dimension of viewing window
 *  @param dahy y-dimension of viewing window
 *  @param dahz z-dimension of viewing window (should = dahx for 90 degree FOV)
 */
void setcamera (dpoint3d *ipo, dpoint3d *ist, dpoint3d *ihe, dpoint3d *ifo,
					 float dahx, float dahy, float dahz)
{
	long i, j;

	gipos.x = ipo->x; gipos.y = ipo->y; gipos.z = ipo->z;
	gistr.x = ist->x; gistr.y = ist->y; gistr.z = ist->z;
	gihei.x = ihe->x; gihei.y = ihe->y; gihei.z = ihe->z;
	gifor.x = ifo->x; gifor.y = ifo->y; gifor.z = ifo->z;
	gihx = dahx; gihy = dahy; gihz = dahz;

	gixs.x = gistr.x; gixs.y = gihei.x; gixs.z = gifor.x;
	giys.x = gistr.y; giys.y = gihei.y; giys.z = gifor.y;
	gizs.x = gistr.z; gizs.y = gihei.z; gizs.z = gifor.z;
	giadd.x = -(gipos.x*gistr.x + gipos.y*gistr.y + gipos.z*gistr.z);
	giadd.y = -(gipos.x*gihei.x + gipos.y*gihei.y + gipos.z*gihei.z);
	giadd.z = -(gipos.x*gifor.x + gipos.y*gifor.y + gipos.z*gifor.z);

	gcorn[0].x = -gihx*gistr.x - gihy*gihei.x + gihz*gifor.x;
	gcorn[0].y = -gihx*gistr.y - gihy*gihei.y + gihz*gifor.y;
	gcorn[0].z = -gihx*gistr.z - gihy*gihei.z + gihz*gifor.z;
	gcorn[1].x = xres_voxlap*gistr.x+gcorn[0].x;
	gcorn[1].y = xres_voxlap*gistr.y+gcorn[0].y;
	gcorn[1].z = xres_voxlap*gistr.z+gcorn[0].z;
	gcorn[2].x = yres_voxlap*gihei.x+gcorn[1].x;
	gcorn[2].y = yres_voxlap*gihei.y+gcorn[1].y;
	gcorn[2].z = yres_voxlap*gihei.z+gcorn[1].z;
	gcorn[3].x = yres_voxlap*gihei.x+gcorn[0].x;
	gcorn[3].y = yres_voxlap*gihei.y+gcorn[0].y;
	gcorn[3].z = yres_voxlap*gihei.z+gcorn[0].z;
	for(j=0,i=3;j<4;i=j++)
	{
		ginor[i].x = gcorn[i].y*gcorn[j].z - gcorn[i].z*gcorn[j].y;
		ginor[i].y = gcorn[i].z*gcorn[j].x - gcorn[i].x*gcorn[j].z;
		ginor[i].z = gcorn[i].x*gcorn[j].y - gcorn[i].y*gcorn[j].x;
	}
}

/**
 * Render VXL screen (this is where it all happens!)
 * Make sure you have .VXL loaded in memory by using one of the loadnul(),
 * loadvxl(), loadbsp(), loaddta() functions.
 * Also make sure to call setcamera() and setvoxframebuffer() before this.
 */
void opticast ()
{
	float f, ff, cx, cy, fx, fy, gx, gy, x0, y0, x1, y1, x2, y2, x3, y3;
	long i, j, sx, sy, p0, p1, cx16, cy16, kadd, kmul, u, u1, ui;

	if (gifor.z < 0) giforzsgn = -1; else giforzsgn = 1; //giforzsgn = (gifor.z < 0);

	gixyi[0] = (VSID<<2); gixyi[1] = -gixyi[0];
	glipos.x = ((long)gipos.x);
	glipos.y = ((long)gipos.y);
	glipos.z = ((long)gipos.z);
	gpixy = (long)&sptr[glipos.y*VSID + glipos.x];
	ftol(gipos.z*PREC-.5f,&gposz);
	gposxfrac[1] = gipos.x - (float)glipos.x; gposxfrac[0] = 1-gposxfrac[1];
	gposyfrac[1] = gipos.y - (float)glipos.y; gposyfrac[0] = 1-gposyfrac[1];
#if (defined(USEV5ASM) && (USEV5ASM != 0)) //if true
	for(j=u=0;j<gmipnum;j++,u+=i)
		for(i=0;i<(256>>j)+4;i++)
			gylookup[i+u] = ((((gposz>>j)-i*PREC)>>(16-j))&0x0000ffff);
	gxmip = MAX(vx5.mipscandist,4)*PREC;
#else
	for(i=0;i<256+4;i++) gylookup[i] = (i*PREC-gposz);
#endif
	gmaxscandist = MIN(MAX(vx5.maxscandist,1),2047)*PREC;

// Selecting functions
#if (USEZBUFFER != 1)
	hrend = hrendnoz; vrend = vrendnoz;
#else
	if (ofogdist < 0)
	{
		hrend = hrendz; vrend = vrendz;
	}
	else
	{
		hrend = hrendzfog; vrend = vrendzfog;
	}
#endif
// END Selecting functions

	if (ofogdist < 0) nskypic = skypic;
				  else { nskypic = skyoff = 0; } //Optimization hack: draw sky as pure black when using fog

	gstartv = (char *)*(long *)gpixy;
	if (glipos.z >= gstartv[1])
	{
		do
		{
			if (!gstartv[0]) return;
			gstartv += gstartv[0]*4;
		} while (glipos.z >= gstartv[1]);
		if (glipos.z < gstartv[3]) return;
		gstartz0 = gstartv[3];
	} else gstartz0 = 0;
	gstartz1 = gstartv[1];

	if (gifor.z == 0) f = 32000; else f = gihz/gifor.z;
	f = MIN(MAX(f,-32000),32000);
	cx = gistr.z*f + gihx;
	cy = gihei.z*f + gihy;

	wx0 = (float)(-(vx5.anginc)); wx1 = (float)(xres_voxlap-1+(vx5.anginc));
	wy0 = (float)(-(vx5.anginc)); wy1 = (float)(yres_voxlap-1+(vx5.anginc));
	ftol(wx0,&iwx0); ftol(wx1,&iwx1);
	ftol(wy0,&iwy0); ftol(wy1,&iwy1);

	fx = wx0-cx; fy = wy0-cy; gx = wx1-cx; gy = wy1-cy;
	x0 = x3 = wx0; y0 = y1 = wy0; x1 = x2 = wx1; y2 = y3 = wy1;
	if (fy < 0)
	{
		if (fx < 0) { f = sqrt(fx*fy); x0 = cx-f; y0 = cy-f; }
		if (gx > 0) { f = sqrt(-gx*fy); x1 = cx+f; y1 = cy-f; }
	}
	if (gy > 0)
	{
		if (gx > 0) { f = sqrt(gx*gy); x2 = cx+f; y2 = cy+f; }
		if (fx < 0) { f = sqrt(-fx*gy); x3 = cx-f; y3 = cy+f; }
	}
	if (x0 > x1) { if (fx < 0) y0 = fx/gx*fy + cy; else y1 = gx/fx*fy + cy; }
	if (y1 > y2) { if (fy < 0) x1 = fy/gy*gx + cx; else x2 = gy/fy*gx + cx; }
	if (x2 < x3) { if (fx < 0) y3 = fx/gx*gy + cy; else y2 = gx/fx*gy + cy; }
	if (y3 < y0) { if (fy < 0) x0 = fy/gy*fx + cx; else x3 = gy/fy*fx + cx; }
		//This makes precision errors cause pixels to overwrite rather than omit
	x0 -= .01; x1 += .01;
	y1 -= .01; y2 += .01;
	x3 -= .01; x2 += .01;
	y0 -= .01; y3 += .01;

	f = (float)PREC / gihz;
	optistrx = gistr.x*f; optiheix = gihei.x*f; optiaddx = gcorn[0].x*f;
	optistry = gistr.y*f; optiheiy = gihei.y*f; optiaddy = gcorn[0].y*f;

	ftol(cx*65536,&cx16);
	ftol(cy*65536,&cy16);

	ftol((x1-x0)/vx5.anginc,&j);
	if ((fy < 0) && (j > 0)) //(cx,cy),(x0,wy0),(x1,wy0)
	{
		ff = (x1-x0) / (float)j; grd = 1.0f / (wy0-cy);
		gscanptr = (castdat *)radar; skycurlng = -1; skycurdir = -giforzsgn;
		for(i=0,f=x0+ff*.5f;i<j;f+=ff,i++)
		{
			vline(cx,cy,f,wy0,&p0,&p1);
			if (giforzsgn < 0) angstart[i] = gscanptr+p0; else angstart[i] = gscanptr-p1;
			gscanptr += labs(p1-p0)+1;
		}

		j <<= 16; f = (float)j / ((x1-x0)*grd); ftol((cx-x0)*grd*f,&kadd);
		ftol(cx-.5f,&p1); p0 = lbound0(p1+1,xres_voxlap); p1 = lbound0(p1,xres_voxlap);
		ftol(cy-0.50005f,&sy); if (sy >= yres_voxlap) sy = yres_voxlap-1;
		ff = (fabs((float)p1-cx)+1)*f/2147483647.0 + cy; //Anti-crash hack
		while ((ff < sy) && (sy >= 0)) sy--;
		if (sy >= 0)
		{
			ftol(f,&kmul);
			for(;sy>=0;sy--) if (isshldiv16safe(kmul,(sy<<16)-cy16)) break; //Anti-crash hack
			if (giforzsgn < 0) i = -sy; else i = sy;
			for(;sy>=0;sy--,i-=giforzsgn)
			{
				ui = shldiv16(kmul,(sy<<16)-cy16);
				u = mulshr16((p0<<16)-cx16,ui)+kadd;
				while ((p0 > 0) && (u >= ui)) { u -= ui; p0--; }
				u1 = (p1-p0)*ui + u;
				while ((p1 < xres_voxlap) && (u1 < j)) { u1 += ui; p1++; }
				if (p0 < p1) hrend(p0,sy,p1,u,ui,i);
			}
		}
	}

	ftol((y2-y1)/vx5.anginc,&j);
	if ((gx > 0) && (j > 0)) //(cx,cy),(wx1,y1),(wx1,y2)
	{
		ff = (y2-y1) / (float)j; grd = 1.0f / (wx1-cx);
		gscanptr = (castdat *)radar; skycurlng = -1; skycurdir = -giforzsgn;
		for(i=0,f=y1+ff*.5f;i<j;f+=ff,i++)
		{
			hline(cx,cy,wx1,f,&p0,&p1);
			if (giforzsgn < 0) angstart[i] = gscanptr-p0; else angstart[i] = gscanptr+p1;
			gscanptr += labs(p1-p0)+1;
		}

		j <<= 16; f = (float)j / ((y2-y1)*grd); ftol((cy-y1)*grd*f,&kadd);
		ftol(cy-.5f,&p1); p0 = lbound0(p1+1,yres_voxlap); p1 = lbound0(p1,yres_voxlap);
		ftol(cx+0.50005f,&sx); if (sx < 0) sx = 0;
		ff = (fabs((float)p1-cy)+1)*f/2147483647.0 + cx; //Anti-crash hack
		while ((ff > sx) && (sx < xres_voxlap)) sx++;
		if (sx < xres_voxlap)
		{
			ftol(f,&kmul);
			for(;sx<xres_voxlap;sx++) if (isshldiv16safe(kmul,(sx<<16)-cx16)) break; //Anti-crash hack
			for(;sx<xres_voxlap;sx++)
			{
				ui = shldiv16(kmul,(sx<<16)-cx16);
				u = mulshr16((p0<<16)-cy16,ui)+kadd;
				while ((p0 > 0) && (u >= ui)) { u -= ui; lastx[--p0] = sx; }
				uurend[sx] = u; uurend[sx+MAXXDIM] = ui; u += (p1-p0)*ui;
				while ((p1 < yres_voxlap) && (u < j)) { u += ui; lastx[p1++] = sx; }
			}
			if (giforzsgn < 0)
				  { for(sy=p0;sy<p1;sy++) vrend(lastx[sy],sy,xres_voxlap,lastx[sy],1); }
			else { for(sy=p0;sy<p1;sy++) vrend(lastx[sy],sy,xres_voxlap,-lastx[sy],-1); }
		}
	}

	ftol((x2-x3)/vx5.anginc,&j);
	if ((gy > 0) && (j > 0)) //(cx,cy),(x2,wy1),(x3,wy1)
	{
		ff = (x2-x3) / (float)j; grd = 1.0f / (wy1-cy);
		gscanptr = (castdat *)radar; skycurlng = -1; skycurdir = giforzsgn;
		for(i=0,f=x3+ff*.5f;i<j;f+=ff,i++)
		{
			vline(cx,cy,f,wy1,&p0,&p1);
			if (giforzsgn < 0) angstart[i] = gscanptr-p0; else angstart[i] = gscanptr+p1;
			gscanptr += labs(p1-p0)+1;
		}

		j <<= 16; f = (float)j / ((x2-x3)*grd); ftol((cx-x3)*grd*f,&kadd);
		ftol(cx-.5f,&p1); p0 = lbound0(p1+1,xres_voxlap); p1 = lbound0(p1,xres_voxlap);
		ftol(cy+0.50005f,&sy); if (sy < 0) sy = 0;
		ff = (fabs((float)p1-cx)+1)*f/2147483647.0 + cy; //Anti-crash hack
		while ((ff > sy) && (sy < yres_voxlap)) sy++;
		if (sy < yres_voxlap)
		{
			ftol(f,&kmul);
			for(;sy<yres_voxlap;sy++) if (isshldiv16safe(kmul,(sy<<16)-cy16)) break; //Anti-crash hack
			if (giforzsgn < 0) i = sy; else i = -sy;
			for(;sy<yres_voxlap;sy++,i-=giforzsgn)
			{
				ui = shldiv16(kmul,(sy<<16)-cy16);
				u = mulshr16((p0<<16)-cx16,ui)+kadd;
				while ((p0 > 0) && (u >= ui)) { u -= ui; p0--; }
				u1 = (p1-p0)*ui + u;
				while ((p1 < xres_voxlap) && (u1 < j)) { u1 += ui; p1++; }
				if (p0 < p1) hrend(p0,sy,p1,u,ui,i);
			}
		}
	}

	ftol((y3-y0)/vx5.anginc,&j);
	if ((fx < 0) && (j > 0)) //(cx,cy),(wx0,y3),(wx0,y0)
	{
		ff = (y3-y0) / (float)j; grd = 1.0f / (wx0-cx);
		gscanptr = (castdat *)radar; skycurlng = -1; skycurdir = giforzsgn;
		for(i=0,f=y0+ff*.5f;i<j;f+=ff,i++)
		{
			hline(cx,cy,wx0,f,&p0,&p1);
			if (giforzsgn < 0) angstart[i] = gscanptr+p0; else angstart[i] = gscanptr-p1;
			gscanptr += labs(p1-p0)+1;
		}

		j <<= 16; f = (float)j / ((y3-y0)*grd); ftol((cy-y0)*grd*f,&kadd);
		ftol(cy-.5f,&p1); p0 = lbound0(p1+1,yres_voxlap); p1 = lbound0(p1,yres_voxlap);
		ftol(cx-0.50005f,&sx); if (sx >= xres_voxlap) sx = xres_voxlap-1;
		ff = (fabs((float)p1-cy)+1)*f/2147483647.0 + cx; //Anti-crash hack
		while ((ff < sx) && (sx >= 0)) sx--;
		if (sx >= 0)
		{
			ftol(f,&kmul);
			for(;sx>=0;sx--) if (isshldiv16safe(kmul,(sx<<16)-cx16)) break; //Anti-crash hack
			for(;sx>=0;sx--)
			{
				ui = shldiv16(kmul,(sx<<16)-cx16);
				u = mulshr16((p0<<16)-cy16,ui)+kadd;
				while ((p0 > 0) && (u >= ui)) { u -= ui; lastx[--p0] = sx; }
				uurend[sx] = u; uurend[sx+MAXXDIM] = ui; u += (p1-p0)*ui;
				while ((p1 < yres_voxlap) && (u < j)) { u += ui; lastx[p1++] = sx; }
			}
			for(sy=p0;sy<p1;sy++) vrend(0,sy,lastx[sy]+1,0,giforzsgn);
		}
	}
}

	//0: asm temp for current x
	//1: asm temp for current y
	//2: bottom (28)
	//3: top    ( 0)
	//4: left   ( 8)
	//5: right  (24)
	//6: up     (12)
	//7: down   (12)
	//setsideshades(0,0,0,0,0,0);
	//setsideshades(0,28,8,24,12,12);
/**
 * Shade offset for each face of the cube: useful for editing
 * @param sto top face (z minimum) shade offset
 * @param sbo bottom face (z maximum) shade offset
 * @param sle left face (x minimum) shade offset
 * @param sri right face (x maximum) shade offset
 * @param sup top face (y minimum) shade offset
 * @param sdo bottom face (y maximum) shade offset
 */
void setsideshades (char sto, char sbo, char sle, char sri, char sup, char sdo)
{
	((char *)&gcsub[2])[7] = sbo; ((char *)&gcsub[3])[7] = sto;
	((char *)&gcsub[4])[7] = sle; ((char *)&gcsub[5])[7] = sri;
	((char *)&gcsub[6])[7] = sup; ((char *)&gcsub[7])[7] = sdo;
	if (!(sto|sbo|sle|sri|sup|sdo))
	{
		vx5.sideshademode = 0;
		((char *)&gcsub[0])[7] = ((char *)&gcsub[1])[7] = 0x00;
	}
	else vx5.sideshademode = 1;
}

	//MUST have more than: CEILING(max possible CLIPRADIUS) * 4 entries!
#define MAXCLIPIT (VSID*4) //VSID*2+4 is not a power of 2!
static lpoint2d clipit[MAXCLIPIT];

double findmaxcr (double px, double py, double pz, double cr)
{
	double f, g, maxcr, thresh2;
	long x, y, z, i0, i1, ix, y0, y1, z0, z1;
	char *v;

	thresh2 = cr+1.7321+1; thresh2 *= thresh2;
	maxcr = cr*cr;

		//Find closest point of all nearby cubes to (px,py,pz)
	x = (long)px; y = (long)py; z = (long)pz; i0 = i1 = 0; ix = x; y0 = y1 = y;
	while (1)
	{
		f = MAX(fabs((double)x+.5-px)-.5,0);
		g = MAX(fabs((double)y+.5-py)-.5,0);
		f = f*f + g*g;
		if (f < maxcr)
		{
			if (((unsigned long)x >= VSID) || ((unsigned long)y >= VSID))
				{ z0 = z1 = 0; }
			else
			{
				v = sptr[y*VSID+x];
				if (z >= v[1])
				{
					while (1)
					{
						if (!v[0]) { z0 = z1 = 0; break; }
						v += v[0]*4;
						if (z < v[1]) { z0 = v[3]; z1 = v[1]; break; }
					}
				}
				else { z0 = MAXZDIM-2048; z1 = v[1]; }
			}

			if ((pz <= z0) || (pz >= z1))
				maxcr = f;
			else
			{
				g = MIN(pz-(double)z0,(double)z1-pz);
				f += g*g; if (f < maxcr) maxcr = f;
			}
		}

		if ((x-px)*(x-px)+(y-py)*(y-py) < thresh2)
		{
			if ((x <= ix) && (x > 0))
				{ clipit[i1].x = x-1; clipit[i1].y = y; i1 = ((i1+1)&(MAXCLIPIT-1)); }
			if ((x >= ix) && (x < VSID-1))
				{ clipit[i1].x = x+1; clipit[i1].y = y; i1 = ((i1+1)&(MAXCLIPIT-1)); }
			if ((y <= y0) && (y > 0))
				{ clipit[i1].x = x; clipit[i1].y = y-1; i1 = ((i1+1)&(MAXCLIPIT-1)); y0--; }
			if ((y >= y1) && (y < VSID-1))
				{ clipit[i1].x = x; clipit[i1].y = y+1; i1 = ((i1+1)&(MAXCLIPIT-1)); y1++; }
		}
		if (i0 == i1) break;
		x = clipit[i0].x; y = clipit[i0].y; i0 = ((i0+1)&(MAXCLIPIT-1));
	}
	return(sqrt(maxcr));
}

#if 0

	//Point: (x,y), line segment: (px,py)-(px+vx,py+vy)
	//Returns 1 if point is closer than sqrt(cr2) to line
long dist2linept2d (double x, double y, double px, double py, double vx, double vy, double cr2)
{
	double f, g;
	x -= px; y -= py; f = x*vx + y*vy; if (f <= 0) return(x*x + y*y <= cr2);
	g = vx*vx + vy*vy; if (f >= g) { x -= vx; y -= vy; return(x*x + y*y <= cr2); }
	x = x*g-vx*f; y = y*g-vy*f; return(x*x + y*y <= cr2*g*g);
}

static char clipbuf[MAXZDIM+16]; //(8 extra on each side)
long sphtraceo (double px, double py, double pz,    //start pt
					double vx, double vy, double vz,    //move vector
					double *nx, double *ny, double *nz, //new pt after collision
					double *fx, double *fy, double *fz, //pt that caused collision
					double cr, double acr)
{
	double t, u, ex, ey, ez, Za, Zb, Zc, thresh2;
	double vxyz, vyz, vxz, vxy, rvxyz, rvyz, rvxz, rvxy, rvx, rvy, rvz, cr2;
	long i, i0, i1, x, y, z, xx, yy, zz, v, vv, ix, y0, y1, z0, z1;
	char *vp;

	t = 1;
	(*nx) = px + vx;
	(*ny) = py + vy;
	(*nz) = pz + vz;

	z0 = MAX((long)(MIN(pz,*nz)-cr)-2,-1);
	z1 = MIN((long)(MAX(pz,*nz)+cr)+2,MAXZDIM);

	thresh2 = cr+1.7321+1; thresh2 *= thresh2;

	vyz = vz*vz; vxz = vx*vx; vxy = vy*vy; cr2 = cr*cr;
	vyz += vxy; vxy += vxz; vxyz = vyz + vxz; vxz += vz*vz;
	rvx = 1.0 / vx; rvy = 1.0 / vy; rvz = 1.0 / vz;
	rvyz = 1.0 / vyz; rvxz = 1.0 / vxz; rvxy = 1.0 / vxy;
	rvxyz = 1.0 / vxyz;

		//Algorithm fails (stops short) if cr < 2 :(
	i0 = i1 = 0; ix = x = (long)px; y = y0 = y1 = (long)py;
	while (1)
	{
		for(z=z0;z<=z1;z++) clipbuf[z+8] = 0;
		i = 16;
		for(yy=y;yy<y+2;yy++)
			for(xx=x;xx<x+2;xx++,i<<=1)
			{
				z = z0;
				if ((unsigned long)(xx|yy) < VSID)
				{
					vp = sptr[yy*VSID+xx];
					while (1)
					{
						if (vp[1] > z) z = vp[1];
						if (!vp[0]) break;
						vp += vp[0]*4;
						zz = vp[3]; if (zz > z1) zz = z1;
						while (z < zz) clipbuf[(z++)+8] |= i;
					}
				}
				while (z <= z1) clipbuf[(z++)+8] |= i;
			}

		xx = x+1; yy = y+1; v = clipbuf[z0+8];
		for(z=z0;z<z1;z++)
		{
			zz = z+1; v = (v>>4)|clipbuf[zz+8];
			if ((!v) || (v == 255)) continue;

//---------------Check 1(8) corners of cube (sphere intersection)-------------

			//if (((v-1)^v) >= v)  //True if v is: {1,2,4,8,16,32,64,128}
			if (!(v&(v-1)))      //Same as above, but {0,1,2,4,...} (v's never 0)
			{
				ex = xx-px; ey = yy-py; ez = zz-pz;
				Zb = ex*vx + ey*vy + ez*vz;
				Zc = ex*ex + ey*ey + ez*ez - cr2;
				u = Zb*Zb - vxyz*Zc;
				if ((((long *)&u)[1] | ((long *)&Zb)[1]) >= 0)
				//if ((u >= 0) && (Zb >= 0))
				{
						//   //Proposed compare optimization:
						//f = Zb*Zb-u; g = vxyz*t; h = (Zb*2-g)*g;
						//if ((uint64_t *)&f < (uint64_t *)&h)
					u = (Zb - sqrt(u)) * rvxyz;
					if ((u >= 0) && (u < t))
					{
						*fx = xx; *fy = yy; *fz = zz; t = u;
						*nx = vx*u + px; *ny = vy*u + py; *nz = vz*u + pz;
					}
				}
			}

//---------------Check 3(12) edges of cube (cylinder intersection)-----------

			vv = v&0x55; if (((vv-1)^vv) >= vv)  //True if (v&0x55)={1,4,16,64}
			{
				ey = yy-py; ez = zz-pz;
				Zb = ey*vy + ez*vz;
				Zc = ey*ey + ez*ez - cr2;
				u = Zb*Zb - vyz*Zc;
				if ((((long *)&u)[1] | ((long *)&Zb)[1]) >= 0)
				//if ((u >= 0) && (Zb >= 0))
				{
					u = (Zb - sqrt(u)) * rvyz;
					if ((u >= 0) && (u < t))
					{
						ex = vx*u + px;
						if ((ex >= x) && (ex <= xx))
						{
							*fx = ex; *fy = yy; *fz = zz; t = u;
							*nx = ex; *ny = vy*u + py; *nz = vz*u + pz;
						}
					}
				}
			}
			vv = v&0x33; if (((vv-1)^vv) >= vv) //True if (v&0x33)={1,2,16,32}
			{
				ex = xx-px; ez = zz-pz;
				Zb = ex*vx + ez*vz;
				Zc = ex*ex + ez*ez - cr2;
				u = Zb*Zb - vxz*Zc;
				if ((((long *)&u)[1] | ((long *)&Zb)[1]) >= 0)
				//if ((u >= 0) && (Zb >= 0))
				{
					u = (Zb - sqrt(u)) * rvxz;
					if ((u >= 0) && (u < t))
					{
						ey = vy*u + py;
						if ((ey >= y) && (ey <= yy))
						{
							*fx = xx; *fy = ey; *fz = zz; t = u;
							*nx = vx*u + px; *ny = ey; *nz = vz*u + pz;
						}
					}
				}
			}
			vv = v&0x0f; if (((vv-1)^vv) >= vv) //True if (v&0x0f)={1,2,4,8}
			{
				ex = xx-px; ey = yy-py;
				Zb = ex*vx + ey*vy;
				Zc = ex*ex + ey*ey - cr2;
				u = Zb*Zb - vxy*Zc;
				if ((((long *)&u)[1] | ((long *)&Zb)[1]) >= 0)
				//if ((u >= 0) && (Zb >= 0))
				{
					u = (Zb - sqrt(u)) * rvxy;
					if ((u >= 0) && (u < t))
					{
						ez = vz*u + pz;
						if ((ez >= z) && (ez <= zz))
						{
							*fx = xx; *fy = yy; *fz = ez; t = u;
							*nx = vx*u + px; *ny = vy*u + py; *nz = ez;
						}
					}
				}
			}

//---------------Check 3(6) faces of cube (plane intersection)---------------

			if (vx)
			{
				switch(v&0x03)
				{
					case 0x01: ex = xx+cr; if ((vx > 0) || (px < ex)) goto skipfacex; break;
					case 0x02: ex = xx-cr; if ((vx < 0) || (px > ex)) goto skipfacex; break;
					default: goto skipfacex;
				}
				u = (ex - px) * rvx;
				if ((u >= 0) && (u < t))
				{
					ey = vy*u + py;
					ez = vz*u + pz;
					if ((ey >= y) && (ey <= yy) && (ez >= z) && (ez <= zz))
					{
						*fx = xx; *fy = ey; *fz = ez; t = u;
						*nx = ex; *ny = ey; *nz = ez;
					}
				}
			}
skipfacex:;
			if (vy)
			{
				switch(v&0x05)
				{
					case 0x01: ey = yy+cr; if ((vy > 0) || (py < ey)) goto skipfacey; break;
					case 0x04: ey = yy-cr; if ((vy < 0) || (py > ey)) goto skipfacey; break;
					default: goto skipfacey;
				}
				u = (ey - py) * rvy;
				if ((u >= 0) && (u < t))
				{
					ex = vx*u + px;
					ez = vz*u + pz;
					if ((ex >= x) && (ex <= xx) && (ez >= z) && (ez <= zz))
					{
						*fx = ex; *fy = yy; *fz = ez; t = u;
						*nx = ex; *ny = ey; *nz = ez;
					}
				}
			}
skipfacey:;
			if (vz)
			{
				switch(v&0x11)
				{
					case 0x01: ez = zz+cr; if ((vz > 0) || (pz < ez)) goto skipfacez; break;
					case 0x10: ez = zz-cr; if ((vz < 0) || (pz > ez)) goto skipfacez; break;
					default: goto skipfacez;
				}
				u = (ez - pz) * rvz;
				if ((u >= 0) && (u < t))
				{
					ex = vx*u + px;
					ey = vy*u + py;
					if ((ex >= x) && (ex <= xx) && (ey >= y) && (ey <= yy))
					{
						*fx = ex; *fy = ey; *fz = zz; t = u;
						*nx = ex; *ny = ey; *nz = ez;
					}
				}
			}
skipfacez:;
		}

		if ((x <= ix) && (x > 0) && (dist2linept2d(x-1,y,px,py,vx,vy,thresh2)))
			{ clipit[i1].x = x-1; clipit[i1].y = y; i1 = ((i1+1)&(MAXCLIPIT-1)); }
		if ((x >= ix) && (x < VSID-1) && (dist2linept2d(x+1,y,px,py,vx,vy,thresh2)))
			{ clipit[i1].x = x+1; clipit[i1].y = y; i1 = ((i1+1)&(MAXCLIPIT-1)); }
		if ((y <= y0) && (y > 0) && (dist2linept2d(x,y-1,px,py,vx,vy,thresh2)))
			{ clipit[i1].x = x; clipit[i1].y = y-1; i1 = ((i1+1)&(MAXCLIPIT-1)); y0--; }
		if ((y >= y1) && (y < VSID-1) && (dist2linept2d(x,y+1,px,py,vx,vy,thresh2)))
			{ clipit[i1].x = x; clipit[i1].y = y+1; i1 = ((i1+1)&(MAXCLIPIT-1)); y1++; }
		if (i0 == i1) break;
		x = clipit[i0].x; y = clipit[i0].y; i0 = ((i0+1)&(MAXCLIPIT-1));
	}

	if ((*nx) < acr) (*nx) = acr;
	if ((*ny) < acr) (*ny) = acr;
	if ((*nx) > VSID-acr) (*nx) = VSID-acr;
	if ((*ny) > VSID-acr) (*ny) = VSID-acr;
	if ((*nz) > MAXZDIM-1-acr) (*nz) = MAXZDIM-1-acr;
	if ((*nz) < MAXZDIM-2048) (*nz) = MAXZDIM-2048;

	return (t == 1);
}

#endif

static double gx0, gy0, gcrf2, grdst, gendt, gux, guy;
static long gdist2square (double x, double y)
{
	double t;
	x -= gx0; y -= gy0; t = x*gux + y*guy; if (t <= 0) t = gcrf2;
	else if (t*grdst >= gendt) { x -= gux*gendt; y -= guy*gendt; t = gcrf2; }
	else t = t*t*grdst + gcrf2;
	return(x*x + y*y <= t);
}

long sphtrace (double x0, double y0, double z0,          //start pt
					double vx, double vy, double vz,          //move vector
					double *hitx, double *hity, double *hitz, //new pt after collision
					double *clpx, double *clpy, double *clpz, //pt causing collision
					double cr, double acr)
{
	double f, t, dax, day, daz, vyx, vxy, vxz, vyz, rvz, cr2, fz, fc;
	double dx, dy, dx1, dy1;
	double nx, ny, intx, inty, intz, dxy, dxz, dyz, dxyz, rxy, rxz, ryz, rxyz;
	long i, j, x, y, ix, iy0, iy1, i0, i1, iz[2], cz0, cz1;
	char *v;

		 //Precalculate global constants for ins & getval functions
	if ((vx == 0) && (vy == 0) && (vz == 0))
		{ (*hitx) = x0; (*hity) = y0; (*hitz) = z0; return(1); }
	gux = vx; guy = vy; gx0 = x0; gy0 = y0; dxy = vx*vx + vy*vy;
	if (dxy != 0) rxy = 1.0 / dxy; else rxy = 0;
	grdst = rxy; gendt = 1; cr2 = cr*cr; t = cr + 0.7072; gcrf2 = t*t;

	if (((long *)&vz)[1] >= 0) { dtol(   z0-cr-.5,&cz0); dtol(vz+z0+cr-.5,&cz1); }
								 else { dtol(vz+z0-cr-.5,&cz0); dtol(   z0+cr-.5,&cz1); }

		//Precalculate stuff for closest point on cube finder
	dax = 0; day = 0; vyx = 0; vxy = 0; rvz = 0; vxz = 0; vyz = 0;
	if (vx != 0) { vyx = vy/vx; if (((long *)&vx)[1] >= 0) dax = x0+cr; else dax = x0-cr-1; }
	if (vy != 0) { vxy = vx/vy; if (((long *)&vy)[1] >= 0) day = y0+cr; else day = y0-cr-1; }
	if (vz != 0)
	{
		rvz = 1.0/vz; vxz = vx*rvz; vyz = vy*rvz;
		if (((long *)&vz)[1] >= 0) daz = z0+cr; else daz = z0-cr;
	}

	dxyz = vz*vz;
	dxz = vx*vx+dxyz; if (dxz != 0) rxz = 1.0 / dxz;
	dyz = vy*vy+dxyz; if (dyz != 0) ryz = 1.0 / dyz;
	dxyz += dxy; rxyz = 1.0 / dxyz;

	dtol(x0-.5,&x); dtol(y0-.5,&y);
	ix = x; iy0 = iy1 = y;
	i0 = 0; clipit[0].x = x; clipit[0].y = y; i1 = 1;
	do
	{
		x = clipit[i0].x; y = clipit[i0].y; i0 = ((i0+1)&(MAXCLIPIT-1));

		dx = (double)x; dx1 = (double)(x+1);
		dy = (double)y; dy1 = (double)(y+1);

			//closest point on cube finder
			//Plane intersection (both vertical planes)
#if 0
		intx = dbound((dy-day)*vxy + x0,dx,dx1);
		inty = dbound((dx-dax)*vyx + y0,dy,dy1);
#else
		intx = (dy-day)*vxy + x0;
		inty = (dx-dax)*vyx + y0;
		if (((long *)&intx)[1] < ((long *)&dx)[1]) intx = dx;
		if (((long *)&inty)[1] < ((long *)&dy)[1]) inty = dy;
		if (((long *)&intx)[1] >= ((long *)&dx1)[1]) intx = dx1;
		if (((long *)&inty)[1] >= ((long *)&dy1)[1]) inty = dy1;
		//if (intx < (double)x) intx = (double)x;
		//if (inty < (double)y) inty = (double)y;
		//if (intx > (double)(x+1)) intx = (double)(x+1);
		//if (inty > (double)(y+1)) inty = (double)(y+1);
#endif

		do
		{
			if (((long *)&dxy)[1] == 0) { t = -1.0; continue; }
			nx = intx-x0; ny = inty-y0; t = vx*nx + vy*ny; if (((long *)&t)[1] < 0) continue;
			f = cr2 - nx*nx - ny*ny; if (((long *)&f)[1] >= 0) { t = -1.0; continue; }
			f = f*dxy + t*t; if (((long *)&f)[1] < 0) { t = -1.0; continue; }
			t = (t-sqrt(f))*rxy;
		} while (0);
		if (t >= gendt) goto sphtracecont;
		if (((long *)&t)[1] < 0) intz = z0; else intz = vz*t + z0;

			//Find closest ceil(iz[0]) & flor(iz[1]) in (x,y) column
		dtol(intz-.5,&i);
		if ((unsigned long)(x|y) < VSID)
		{
			v = sptr[y*VSID+x]; iz[0] = MAXZDIM-2048; iz[1] = v[1];
			while (i >= iz[1])
			{
				if (!v[0]) { iz[1] = -1; break; }
				v += v[0]*4;
				iz[0] = v[3]; if (i < iz[0]) { iz[1] = -1; break; }
				iz[1] = v[1];
			}
		}
		else iz[1] = -1;

			//hit xz plane, yz plane or z-axis edge?
		if (iz[1] < 0) //Treat whole column as solid
		{
			if (((long *)&t)[1] >= 0) { gendt = t; (*clpx) = intx; (*clpy) = inty; (*clpz) = intz; goto sphtracecont; }
		}

			//Must check tops & bottoms of slab
		for(i=1;i>=0;i--)
		{
				//Ceil/flor outside of quick&dirty bounding box
			if ((iz[i] < cz0) || (iz[i] > cz1)) continue;

				//Plane intersection (parallel to ground)
			intz = (double)iz[i]; t = intz-daz;
			intx = t*vxz + x0;
			inty = t*vyz + y0;

			j = 0;                         // A ³ 8 ³ 9
			//     if (intx < dx)  j |= 2; //ÄÄÄÅÄÄÄÅÄÄÄ
			//else if (intx > dx1) j |= 1; // 2 ³ 0 ³ 1
			//     if (inty < dy)  j |= 8; //ÄÄÄÅÄÄÄÅÄÄÄ
			//else if (inty > dy1) j |= 4; // 6 ³ 4 ³ 5
				  if (((long *)&intx)[1] <  ((long *)&dx)[1])  j |= 2;
			else if (((long *)&intx)[1] >= ((long *)&dx1)[1]) j |= 1;
				  if (((long *)&inty)[1] <  ((long *)&dy)[1])  j |= 8;
			else if (((long *)&inty)[1] >= ((long *)&dy1)[1]) j |= 4;

				//NOTE: only need to check once per "for"!
			if ((!j) && (vz != 0)) //hit xy plane?
			{
				t *= rvz;
				if ((((long *)&t)[1] >= 0) && (t < gendt)) { gendt = t; (*clpx) = intx; (*clpy) = inty; (*clpz) = intz; }
				continue;
			}

				//common calculations used for rest of checks...
			fz = intz-z0; fc = cr2-fz*fz; fz *= vz;

			if (j&3)
			{
				nx = (double)((j&1)+x);
				if (((long *)&dxz)[1] != 0) //hit y-axis edge?
				{
					f = nx-x0; t = vx*f + fz; f = (fc - f*f)*dxz + t*t;
					if (((long *)&f)[1] >= 0) t = (t-sqrt(f))*rxz; else t = -1.0;
				} else t = -1.0;
				ny = vy*t + y0;
					  if (((long *)&ny)[1] > ((long *)&dy1)[1]) j |= 0x10;
				else if (((long *)&ny)[1] >= ((long *)&dy)[1])
				{
					if ((((long *)&t)[1] >= 0) && (t < gendt)) { gendt = t; (*clpx) = nx; (*clpy) = ny; (*clpz) = intz; }
					continue;
				}
				inty = (double)(((j>>4)&1)+y);
			}
			else inty = (double)(((j>>2)&1)+y);

			if (j&12)
			{
				ny = (double)(((j>>2)&1)+y);
				if (((long *)&dyz)[1] != 0) //hit x-axis edge?
				{
					f = ny-y0; t = vy*f + fz; f = (fc - f*f)*dyz + t*t;
					if (((long *)&f)[1] >= 0) t = (t-sqrt(f))*ryz; else t = -1.0;
				} else t = -1.0;
				nx = vx*t + x0;
					  if (((long *)&nx)[1] > ((long *)&dx1)[1]) j |= 0x20;
				else if (((long *)&nx)[1] >= ((long *)&dx)[1])
				{
					if ((((long *)&t)[1] >= 0) && (t < gendt)) { gendt = t; (*clpx) = nx; (*clpy) = ny; (*clpz) = intz; }
					continue;
				}
				intx = (double)(((j>>5)&1)+x);
			}
			else intx = (double)((j&1)+x);

				//hit corner?
			nx = intx-x0; ny = inty-y0;
			t = vx*nx + vy*ny + fz; if (((long *)&t)[1] < 0) continue;
			f = fc - nx*nx - ny*ny; if (((long *)&f)[1] >= 0) continue;
			f = f*dxyz + t*t; if (((long *)&f)[1] < 0) continue;
			t = (t-sqrt(f))*rxyz;
			if (t < gendt) { gendt = t; (*clpx) = intx; (*clpy) = inty; (*clpz) = intz; }
		}
sphtracecont:;
		if ((x <= ix)  && (x >      0) && (gdist2square(dx- .5,dy+ .5))) { clipit[i1].x = x-1; clipit[i1].y = y; i1 = ((i1+1)&(MAXCLIPIT-1)); }
		if ((x >= ix)  && (x < VSID-1) && (gdist2square(dx+1.5,dy+ .5))) { clipit[i1].x = x+1; clipit[i1].y = y; i1 = ((i1+1)&(MAXCLIPIT-1)); }
		if ((y <= iy0) && (y >      0) && (gdist2square(dx+ .5,dy- .5))) { clipit[i1].x = x; clipit[i1].y = y-1; i1 = ((i1+1)&(MAXCLIPIT-1)); iy0 = y-1; }
		if ((y >= iy1) && (y < VSID-1) && (gdist2square(dx+ .5,dy+1.5))) { clipit[i1].x = x; clipit[i1].y = y+1; i1 = ((i1+1)&(MAXCLIPIT-1)); iy1 = y+1; }
	} while (i0 != i1);
#if 1
	(*hitx) = dbound(vx*gendt + x0,acr,VSID-acr);
	(*hity) = dbound(vy*gendt + y0,acr,VSID-acr);
	(*hitz) = dbound(vz*gendt + z0,MAXZDIM-2048+acr,MAXZDIM-1-acr);
#else
	(*hitx) = MIN(MAX(vx*gendt + x0,acr),VSID-acr);
	(*hity) = MIN(MAX(vy*gendt + y0,acr),VSID-acr);
	(*hitz) = MIN(MAX(vz*gendt + z0,MAXZDIM-2048+acr),MAXZDIM-1-acr);
#endif
	return(gendt == 1);
}

void clipmove (dpoint3d *p, dpoint3d *v, double acr)
{
	double f, gx, gy, gz, nx, ny, nz, ex, ey, ez, hitx, hity, hitz, cr;
	//double nx2, ny2, nz2, ex2, ey2, ez2; //double ox, oy, oz;
	long i, j, k;

	//ox = p->x; oy = p->y; oz = p->z;
	gx = p->x+v->x; gy = p->y+v->y; gz = p->z+v->z;

	cr = findmaxcr(p->x,p->y,p->z,acr);
	vx5.clipmaxcr = cr;

	vx5.cliphitnum = 0;
	for(i=0;i<3;i++)
	{
		if ((v->x == 0) && (v->y == 0) && (v->z == 0)) break;

		cr -= 1e-7;  //Shrinking radius error control hack

		//j = sphtraceo(p->x,p->y,p->z,v->x,v->y,v->z,&nx,&ny,&nz,&ex,&ey,&ez,cr,acr);
		//k = sphtraceo(p->x,p->y,p->z,v->x,v->y,v->z,&nx2,&ny2,&nz2,&ex2,&ey2,&ez2,cr,acr);

		j = sphtrace(p->x,p->y,p->z,v->x,v->y,v->z,&nx,&ny,&nz,&ex,&ey,&ez,cr,acr);

		//if ((j != k) || (fabs(nx-nx2) > .000001) || (fabs(ny-ny2) > .000001) || (fabs(nz-nz2) > .000001) ||
		//   ((j == 0) && ((fabs(ex-ex2) > .000001) || (fabs(ey-ey2) > .000001) || (fabs(ez-ez2) > .000001))))
		//{
		//   printf("%d %f %f %f %f %f %f\n",i,p->x,p->y,p->z,v->x,v->y,v->z);
		//   printf("%f %f %f ",nx,ny,nz); if (!j) printf("%f %f %f\n",ex,ey,ez); else printf("\n");
		//   printf("%f %f %f ",nx2,ny2,nz2); if (!k) printf("%f %f %f\n",ex2,ey2,ez2); else printf("\n");
		//   printf("\n");
		//}
		if (j) { p->x = nx; p->y = ny; p->z = nz; break; }

		vx5.cliphit[i].x = ex; vx5.cliphit[i].y = ey; vx5.cliphit[i].z = ez;
		vx5.cliphitnum = i+1;
		p->x = nx; p->y = ny; p->z = nz;

			//Calculate slide vector
		v->x = gx-nx; v->y = gy-ny; v->z = gz-nz;
		switch(i)
		{
			case 0:
				hitx = ex-nx; hity = ey-ny; hitz = ez-nz;
				f = (v->x*hitx + v->y*hity + v->z*hitz) / (cr * cr);
				v->x -= hitx*f; v->y -= hity*f; v->z -= hitz*f;
				break;
			case 1:
				nx -= ex; ny -= ey; nz -= ez;
				ex = hitz*ny - hity*nz;
				ey = hitx*nz - hitz*nx;
				ez = hity*nx - hitx*ny;
				f = ex*ex + ey*ey + ez*ez; if (f <= 0) break;
				f = (v->x*ex + v->y*ey + v->z*ez) / f;
				v->x = ex*f; v->y = ey*f; v->z = ez*f;
				break;
			default: break;
		}
	}

		//If you didn't move much, then don't move at all. This helps prevents
		//cliprad from shrinking, but you get stuck too much :(
	//if ((p->x-ox)*(p->x-ox) + (p->y-oy)*(p->y-oy) + (p->z-oz)*(p->z-oz) < 1e-12)
	//   { p->x = ox; p->y = oy; p->z = oz; }
}

long cansee (point3d *p0, point3d *p1, lpoint3d *hit)
{
	lpoint3d a, c, d, p, i;
	point3d f, g;
	long cnt;

	ftol(p0->x-.5,&a.x); ftol(p0->y-.5,&a.y); ftol(p0->z-.5,&a.z);
	if (isvoxelsolid(a.x,a.y,a.z)) { hit->x = a.x; hit->y = a.y; hit->z = a.z; return(0); }
	ftol(p1->x-.5,&c.x); ftol(p1->y-.5,&c.y); ftol(p1->z-.5,&c.z);
	cnt = 0;

		  if (c.x <  a.x) { d.x = -1; f.x = p0->x-a.x;   g.x = (p0->x-p1->x)*1024; cnt += a.x-c.x; }
	else if (c.x != a.x) { d.x =  1; f.x = a.x+1-p0->x; g.x = (p1->x-p0->x)*1024; cnt += c.x-a.x; }
	else f.x = g.x = 0;
		  if (c.y <  a.y) { d.y = -1; f.y = p0->y-a.y;   g.y = (p0->y-p1->y)*1024; cnt += a.y-c.y; }
	else if (c.y != a.y) { d.y =  1; f.y = a.y+1-p0->y; g.y = (p1->y-p0->y)*1024; cnt += c.y-a.y; }
	else f.y = g.y = 0;
		  if (c.z <  a.z) { d.z = -1; f.z = p0->z-a.z;   g.z = (p0->z-p1->z)*1024; cnt += a.z-c.z; }
	else if (c.z != a.z) { d.z =  1; f.z = a.z+1-p0->z; g.z = (p1->z-p0->z)*1024; cnt += c.z-a.z; }
	else f.z = g.z = 0;

	ftol(f.x*g.z - f.z*g.x,&p.x); ftol(g.x,&i.x);
	ftol(f.y*g.z - f.z*g.y,&p.y); ftol(g.y,&i.y);
	ftol(f.y*g.x - f.x*g.y,&p.z); ftol(g.z,&i.z);

		//NOTE: GIGO! This can happen if p0,p1 (cansee input) is NaN, Inf, etc...
	if ((unsigned long)cnt > (VSID+VSID+2048)*2) cnt = (VSID+VSID+2048)*2;
	while (cnt > 0)
	{
		if (((p.x|p.y) >= 0) && (a.z != c.z)) { a.z += d.z; p.x -= i.x; p.y -= i.y; }
		else if ((p.z >= 0) && (a.x != c.x))  { a.x += d.x; p.x += i.z; p.z -= i.y; }
		else                                  { a.y += d.y; p.y += i.z; p.z += i.x; }
		if (isvoxelsolid(a.x,a.y,a.z)) break;
		cnt--;
	}
	hit->x = a.x; hit->y = a.y; hit->z = a.z; return(!cnt);
}

	//  p: start position
	//  d: direction
	//  h: coordinate of voxel hit (if any)
	//ind: pointer to surface voxel's 32-bit color (0 if none hit)
	//dir: 0-5: last direction moved upon hit (-1 if inside solid)
void hitscan (dpoint3d *p, dpoint3d *d, lpoint3d *h, long **ind, long *dir)
{
	long ixi, iyi, izi, dx, dy, dz, dxi, dyi, dzi, z0, z1, minz;
	float f, kx, ky, kz;
	char *v;

		//Note: (h->x,h->y,h->z) MUST be rounded towards -inf
	(h->x) = (long)p->x;
	(h->y) = (long)p->y;
	(h->z) = (long)p->z;
	if ((unsigned long)(h->x|h->y) >= VSID) { (*ind) = 0; (*dir) = -1; return; }
	ixi = (((((signed long *)&d->x)[1])>>31)|1);
	iyi = (((((signed long *)&d->y)[1])>>31)|1);
	izi = (((((signed long *)&d->z)[1])>>31)|1);

	minz = MIN(h->z,0);

	f = 0x3fffffff/VSID; //Maximum delta value
	if ((fabs(d->x) >= fabs(d->y)) && (fabs(d->x) >= fabs(d->z)))
	{
		kx = 1024.0;
		if (d->y == 0) ky = f; else ky = MIN(fabs(d->x/d->y)*1024.0,f);
		if (d->z == 0) kz = f; else kz = MIN(fabs(d->x/d->z)*1024.0,f);
	}
	else if (fabs(d->y) >= fabs(d->z))
	{
		ky = 1024.0;
		if (d->x == 0) kx = f; else kx = MIN(fabs(d->y/d->x)*1024.0,f);
		if (d->z == 0) kz = f; else kz = MIN(fabs(d->y/d->z)*1024.0,f);
	}
	else
	{
		kz = 1024.0;
		if (d->x == 0) kx = f; else kx = MIN(fabs(d->z/d->x)*1024.0,f);
		if (d->y == 0) ky = f; else ky = MIN(fabs(d->z/d->y)*1024.0,f);
	}
	ftol(kx,&dxi); ftol((p->x-(float)h->x)*kx,&dx); if (ixi >= 0) dx = dxi-dx;
	ftol(ky,&dyi); ftol((p->y-(float)h->y)*ky,&dy); if (iyi >= 0) dy = dyi-dy;
	ftol(kz,&dzi); ftol((p->z-(float)h->z)*kz,&dz); if (izi >= 0) dz = dzi-dz;

	v = sptr[h->y*VSID+h->x];
	if (h->z >= v[1])
	{
		do
		{
			if (!v[0]) { (*ind) = 0; (*dir) = -1; return; }
			v += v[0]*4;
		} while (h->z >= v[1]);
		z0 = v[3];
	} else z0 = minz;
	z1 = v[1];

	while (1)
	{
		//Check cube at: h->x,h->y,h->z

		if ((dz <= dx) && (dz <= dy))
		{
			h->z += izi; dz += dzi; (*dir) = 5-(izi>0);

				//Check if h->z ran into anything solid
			if (h->z < z0)
			{
				if (h->z < minz) (*ind) = 0; else (*ind) = (long *)&v[-4];
				return;
			}
			if (h->z >= z1) { (*ind) = (long *)&v[4]; return; }
		}
		else
		{
			if (dx < dy)
			{
				h->x += ixi; dx += dxi; (*dir) = 1-(ixi>0);
				if ((unsigned long)h->x >= VSID) { (*ind) = 0; return; }
			}
			else
			{
				h->y += iyi; dy += dyi; (*dir) = 3-(iyi>0);
				if ((unsigned long)h->y >= VSID) { (*ind) = 0; return; }
			}

				//Check if (h->x, h->y) ran into anything solid
			v = sptr[h->y*VSID+h->x];
			while (1)
			{
				if (h->z < v[1])
				{
					if (v == sptr[h->y*VSID+h->x]) { z0 = minz; z1 = v[1]; break; }
					if (h->z < v[3]) { (*ind) = (long *)&v[(h->z-v[3])*4]; return; }
					z0 = v[3]; z1 = v[1]; break;
				}
				else if ((h->z <= v[2]) || (!v[0]))
					{ (*ind) = (long *)&v[(h->z-v[1])*4+4]; return; }

				v += v[0]*4;
			}
		}
	}
}

	// p0: start position
	// v0: direction
	//spr: pointer of sprite to test collision with
	//  h: coordinate of voxel hit in sprite coordinates (if any)
	//ind: pointer to voxel hit (kv6voxtype) (0 if none hit)
	//vsc:  input: max multiple/fraction of v0's length to scan (1.0 for |v0|)
	//     output: multiple/fraction of v0's length of hit point
void sprhitscan (dpoint3d *p0, dpoint3d *v0, vx5sprite *spr, lpoint3d *h, kv6voxtype **ind, float *vsc)
{
	kv6voxtype *vx[4];
	kv6data *kv;
	point3d t, u, v;
	lpoint3d a, d, p, q;
	float f, g;
	long i, x, y, xup, ix0, ix1;

	(*ind) = 0;
	if (spr->flags&2)
	{
		kfatype *kf = spr->kfaptr;
			//This sets the sprite pointer to be the parent sprite (voxnum
			//   of the main sprite is invalid for KFA sprites!)
		spr = &kf->spr[kf->hingesort[(kf->numhin)-1]];
	}
	kv = spr->voxnum; if (!kv) return;

		//d transformed to spr space (0,0,0,kv->xsiz,kv->ysiz,kv->zsiz)
	v.x = v0->x*spr->s.x + v0->y*spr->s.y + v0->z*spr->s.z;
	v.y = v0->x*spr->h.x + v0->y*spr->h.y + v0->z*spr->h.z;
	v.z = v0->x*spr->f.x + v0->y*spr->f.y + v0->z*spr->f.z;

		//p transformed to spr space (0,0,0,kv->xsiz,kv->ysiz,kv->zsiz)
	t.x = p0->x-spr->p.x;
	t.y = p0->y-spr->p.y;
	t.z = p0->z-spr->p.z;
	u.x = t.x*spr->s.x + t.y*spr->s.y + t.z*spr->s.z;
	u.y = t.x*spr->h.x + t.y*spr->h.y + t.z*spr->h.z;
	u.z = t.x*spr->f.x + t.y*spr->f.y + t.z*spr->f.z;
	u.x /= (spr->s.x*spr->s.x + spr->s.y*spr->s.y + spr->s.z*spr->s.z);
	u.y /= (spr->h.x*spr->h.x + spr->h.y*spr->h.y + spr->h.z*spr->h.z);
	u.z /= (spr->f.x*spr->f.x + spr->f.y*spr->f.y + spr->f.z*spr->f.z);
	u.x += kv->xpiv; u.y += kv->ypiv; u.z += kv->zpiv;

	ix0 = MAX(vx5.xplanemin,0);
	ix1 = MIN(vx5.xplanemax,kv->xsiz);

		//Increment ray until it hits bounding box
		// (ix0,0,0,ix1-1ulp,kv->ysiz-1ulp,kv->zsiz-1ulp)
	g = (float)ix0;
	t.x = (float)ix1;      (*(long *)&t.x)--;
	t.y = (float)kv->ysiz; (*(long *)&t.y)--;
	t.z = (float)kv->zsiz; (*(long *)&t.z)--;
		  if (u.x <   g) { if (v.x <= 0) return; f = (  g-u.x)/v.x; u.x =   g; u.y += v.y*f; u.z += v.z*f; }
	else if (u.x > t.x) { if (v.x >= 0) return; f = (t.x-u.x)/v.x; u.x = t.x; u.y += v.y*f; u.z += v.z*f; }
		  if (u.y <   0) { if (v.y <= 0) return; f = (  0-u.y)/v.y; u.y =   0; u.x += v.x*f; u.z += v.z*f; }
	else if (u.y > t.y) { if (v.y >= 0) return; f = (t.y-u.y)/v.y; u.y = t.y; u.x += v.x*f; u.z += v.z*f; }
		  if (u.z <   0) { if (v.z <= 0) return; f = (  0-u.z)/v.z; u.z =   0; u.x += v.x*f; u.y += v.y*f; }
	else if (u.z > t.z) { if (v.z >= 0) return; f = (t.z-u.z)/v.z; u.z = t.z; u.x += v.x*f; u.y += v.y*f; }

	ix1 -= ix0;

	g = 262144.0 / sqrt(v.x*v.x + v.y*v.y + v.z*v.z);

		//Note: (a.x,a.y,a.z) MUST be rounded towards -inf
	ftol(u.x-.5,&a.x); if ((unsigned long)(a.x-ix0) >= ix1) return;
	ftol(u.y-.5,&a.y); if ((unsigned long)a.y >= kv->ysiz) return;
	ftol(u.z-.5,&a.z); if ((unsigned long)a.z >= kv->zsiz) return;
	if (*(long *)&v.x < 0) { d.x = -1; u.x -= a.x;      v.x *= -g; }
							else { d.x =  1; u.x = a.x+1-u.x; v.x *=  g; }
	if (*(long *)&v.y < 0) { d.y = -1; u.y -= a.y;      v.y *= -g; }
							else { d.y =  1; u.y = a.y+1-u.y; v.y *=  g; }
	if (*(long *)&v.z < 0) { d.z = -1; u.z -= a.z;      v.z *= -g; }
							else { d.z =  1; u.z = a.z+1-u.z; v.z *=  g; }
	ftol(u.x*v.z - u.z*v.x,&p.x); ftol(v.x,&q.x);
	ftol(u.y*v.z - u.z*v.y,&p.y); ftol(v.y,&q.y);
	ftol(u.y*v.x - u.x*v.y,&p.z); ftol(v.z,&q.z);

		//Check if voxel at: (a.x,a.y,a.z) is solid
	vx[0] = kv->vox;
	for(x=0;x<a.x;x++) vx[0] += kv->xlen[x];
	vx[1] = vx[0]; xup = x*kv->ysiz;
	for(y=0;y<a.y;y++) vx[1] += kv->ylen[xup+y];
	vx[2] = vx[1]; vx[3] = &vx[1][kv->ylen[xup+y]];

	while (1)
	{
		//vs = kv->vox; //Brute force: remove all vx[?] code to enable this
		//for(x=0;x<a.x;x++) vs += kv->xlen[x];
		//for(y=0;y<a.y;y++) vs += kv->ylen[x*kv->ysiz+y];
		//for(ve=&vs[kv->ylen[x+y]];vs<ve;vs++) if (vs->z == a.z) break;

			//Check if voxel at: (a.x,a.y,a.z) is solid
		if (vx[1] < vx[3])
		{
			while ((a.z < vx[2]->z) && (vx[2] > vx[1]  )) vx[2]--;
			while ((a.z > vx[2]->z) && (vx[2] < vx[3]-1)) vx[2]++;
			if (a.z == vx[2]->z) break;
		}

		if ((p.x|p.y) >= 0)
		{
			a.z += d.z; if ((unsigned long)a.z >= kv->zsiz) return;
			p.x -= q.x; p.y -= q.y;
		}
		else if (p.z < 0)
		{
			a.y += d.y; if ((unsigned long)a.y >= kv->ysiz) return;
			p.y += q.z; p.z += q.x;

			if (a.y < y) { y--; vx[1] -= kv->ylen[xup+y];      }
			if (a.y > y) {      vx[1] += kv->ylen[xup+y]; y++; }
			vx[2] = vx[1]; vx[3] = &vx[1][kv->ylen[xup+y]];
		}
		else
		{
			a.x += d.x; if ((unsigned long)(a.x-ix0) >= ix1) return;
			p.x += q.z; p.z -= q.y;

			if (a.x < x) { x--; vx[0] -= kv->xlen[x];      xup -= kv->ysiz; }
			if (a.x > x) {      vx[0] += kv->xlen[x]; x++; xup += kv->ysiz; }
			if ((a.y<<1) < kv->ysiz) //Start y-slice search from closer side
			{
				vx[1] = vx[0];
				for(y=0;y<a.y;y++) vx[1] += kv->ylen[xup+y];
			}
			else
			{
				vx[1] = &vx[0][kv->xlen[x]];
				for(y=kv->ysiz;y>a.y;y--) vx[1] -= kv->ylen[xup+y-1];
			}
			vx[2] = vx[1]; vx[3] = &vx[1][kv->ylen[xup+y]];
		}
	}

		//given: a = kv6 coordinate, find: v = vxl coordinate
	u.x = (float)a.x-kv->xpiv;
	u.y = (float)a.y-kv->ypiv;
	u.z = (float)a.z-kv->zpiv;
	v.x = u.x*spr->s.x + u.y*spr->h.x + u.z*spr->f.x + spr->p.x;
	v.y = u.x*spr->s.y + u.y*spr->h.y + u.z*spr->f.y + spr->p.y;
	v.z = u.x*spr->s.z + u.y*spr->h.z + u.z*spr->f.z + spr->p.z;

		//Stupid dot product stuff...
	f = ((v.x-p0->x)*v0->x + (v.y-p0->y)*v0->y + (v.z-p0->z)*v0->z) /
		  (v0->x*v0->x + v0->y*v0->y + v0->z*v0->z);
	if (f >= (*vsc)) return;
	{ (*vsc) = f; (*h) = a; (*ind) = vx[2]; (*vsc) = f; }
}

unsigned long calcglobalmass ()
{
	unsigned long i, j;
	char *v;

	j = VSID*VSID*256;
	for(i=0;i<VSID*VSID;i++)
	{
		v = sptr[i]; j -= v[1];
		while (v[0]) { v += v[0]*4; j += v[3]-v[1]; }
	}
	return(j);
}

/**
 * Sticks a default VXL map into memory (puts you inside a brownish box)
 * This is useful for VOXED when you want to start a new map.
 * @param ipo default starting camera position
 * @param ist RIGHT unit vector
 * @param ihe DOWN unit vector
 * @param ifo FORWARD unit vector
 */
void loadnul (dpoint3d *ipo, dpoint3d *ist, dpoint3d *ihe, dpoint3d *ifo)
{
	lpoint3d lp0, lp1;
	long i, x, y;
	char *v;
	float f;

	if (!vbuf) { vbuf = (long *)malloc((VOXSIZ>>2)<<2); if (!vbuf) evilquit("vbuf malloc failed"); }
	if (!vbit) { vbit = (long *)malloc((VOXSIZ>>7)<<2); if (!vbit) evilquit("vbuf malloc failed"); }

	v = (char *)(&vbuf[1]); //1st dword for voxalloc compare logic optimization

		//Completely re-compile vbuf
	for(x=0;x<VSID;x++)
		for(y=0;y<VSID;y++)
		{
			sptr[y*VSID+x] = v;
			i = 0; // + (rand()&1);  //i = default height of plain
			v[0] = 0;
			v[1] = i;
			v[2] = i;
			v[3] = 0;  //z0 (Dummy filler)
			//i = ((((x+y)>>3) + ((x^y)>>4)) % 231) + 16;
			//i = (i<<16)+(i<<8)+i;
			v += 4;
			(*(long *)v) = ((x^y)&15)*0x10101+0x807c7c7c; //colorjit(i,0x70707)|0x80000000;
			v += 4;
		}

	memset(&sptr[VSID*VSID],0,sizeof(sptr)-VSID*VSID*4);
	vbiti = (((long)v-(long)vbuf)>>2); //# vbuf longs/vbit bits allocated
	clearbuf((void *)vbit,vbiti>>5,-1);
	clearbuf((void *)&vbit[vbiti>>5],(VOXSIZ>>7)-(vbiti>>5),0);
	vbit[vbiti>>5] = (1<<vbiti)-1;

		//Blow out sphere and stick you inside map
	vx5.colfunc = jitcolfunc; vx5.curcol = 0x80704030;

	lp0.x = VSID*.5-90; lp0.y = VSID*.5-90; lp0.z = MAXZDIM*.5-45;
	lp1.x = VSID*.5+90; lp1.y = VSID*.5+90; lp1.z = MAXZDIM*.5+45;
	setrect(&lp0,&lp1,-1);
	//lp.x = VSID*.5; lp.y = VSID*.5; lp.z = MAXZDIM*.5; setsphere(&lp,64,-1);

	vx5.globalmass = calcglobalmass();

	ipo->x = VSID*.5; ipo->y = VSID*.5; ipo->z = MAXZDIM*.5; //ipo->z = -16;
	f = 0.0*PI/180.0;
	ist->x = cos(f); ist->y = sin(f); ist->z = 0;
	ihe->x = 0; ihe->y = 0; ihe->z = 1;
	ifo->x = sin(f); ifo->y = -cos(f); ifo->z = 0;

	gmipnum = 1; vx5.flstnum = 0;
	updatebbox(0,0,0,VSID,VSID,MAXZDIM,0);
}

/**
 * Loads a Comanche format map into memory.
 * @param filename Should be formatted like this: "C1.DTA"
 *                 It replaces the first letter with C&D to get both height&color
 * @return 0:bad, 1:good
 */
long loaddta (const char *filename, dpoint3d *ipo, dpoint3d *ist, dpoint3d *ihe, dpoint3d *ifo)
{
	long i, j, p, leng, minz = 255, maxz = 0, h[5], longpal[256];
	char dat, *dtahei, *dtacol, *v, dafilename[MAX_PATH];
	float f;
	FILE *fp;

	if (!vbuf) { vbuf = (long *)malloc((VOXSIZ>>2)<<2); if (!vbuf) evilquit("vbuf malloc failed"); }
	if (!vbit) { vbit = (long *)malloc((VOXSIZ>>7)<<2); if (!vbit) evilquit("vbuf malloc failed"); }

	if (VSID != 1024) return(0);
	v = (char *)(&vbuf[1]); //1st dword for voxalloc compare logic optimization

	strcpy(dafilename,filename);

	dtahei = (char *)(&vbuf[(VOXSIZ-2097152)>>2]);
	dtacol = (char *)(&vbuf[(VOXSIZ-1048576)>>2]);

	dafilename[0] = 'd';
	if (!kzopen(dafilename)) return(0);
	kzseek(128,SEEK_SET); p = 0;
	while (p < 1024*1024)
	{
		dat = kzgetc();
		if (dat >= 192) { leng = dat-192; dat = kzgetc(); }
					  else { leng = 1; }
		dat = 255-dat;
		if (dat < minz) minz = dat;
		if (dat > maxz) maxz = dat;
		while (leng-- > 0) dtahei[p++] = dat;
	}
	kzclose();

	dafilename[0] = 'c';
	if (!kzopen(dafilename)) return(0);
	kzseek(128,SEEK_SET);
	p = 0;
	while (p < 1024*1024)
	{
		dat = kzgetc();
		if (dat >= 192) { leng = dat-192; dat = kzgetc(); }
					  else { leng = 1; }
		while (leng-- > 0) dtacol[p++] = dat;
	}

	dat = kzgetc();
	if (dat == 0xc)
		for(i=0;i<256;i++)
		{
			longpal[i] = kzgetc();
			longpal[i] = (longpal[i]<<8)+kzgetc();
			longpal[i] = (longpal[i]<<8)+kzgetc() + 0x80000000;
		}

	kzclose();

		//Fill board data
	minz = lbound(128-((minz+maxz)>>1),-minz,255-maxz);
	for(p=0;p<1024*1024;p++)
	{
		h[0] = (long)dtahei[p];
		h[1] = (long)dtahei[((p-1)&0x3ff)+((p     )&0xffc00)];
		h[2] = (long)dtahei[((p+1)&0x3ff)+((p     )&0xffc00)];
		h[3] = (long)dtahei[((p  )&0x3ff)+((p-1024)&0xffc00)];
		h[4] = (long)dtahei[((p  )&0x3ff)+((p+1024)&0xffc00)];

		j = 1;
		for(i=4;i>0;i--) if (h[i]-h[0] > j) j = h[i]-h[0];

		sptr[p] = v;
		v[0] = 0;
		v[1] = dtahei[p]+minz;
		v[2] = dtahei[p]+minz+j-1;
		v[3] = 0; //dummy (z top)
		v += 4;
		for(;j;j--) { *(long *)v = colorjit(longpal[dtacol[p]],0x70707); v += 4; }
	}

	memset(&sptr[VSID*VSID],0,sizeof(sptr)-VSID*VSID*4);
	vbiti = (((long)v-(long)vbuf)>>2); //# vbuf longs/vbit bits allocated
	clearbuf((void *)vbit,vbiti>>5,-1);
	clearbuf((void *)&vbit[vbiti>>5],(VOXSIZ>>7)-(vbiti>>5),0);
	vbit[vbiti>>5] = (1<<vbiti)-1;

	vx5.globalmass = calcglobalmass();

	ipo->x = VSID*.5; ipo->y = VSID*.5; ipo->z = 128;
	f = 0.0*PI/180.0;
	ist->x = cos(f); ist->y = sin(f); ist->z = 0;
	ihe->x = 0; ihe->y = 0; ihe->z = 1;
	ifo->x = sin(f); ifo->y = -cos(f); ifo->z = 0;

	gmipnum = 1; vx5.flstnum = 0;
	updatebbox(0,0,0,VSID,VSID,MAXZDIM,0);
	return(1);
}

/**
 * Loads a heightmap from PNG (or TGA) into memory; alpha channel is height.
 * @param filename Any 1024x1024 PNG or TGA file with alpha channel.
 * @return 0:bad, 1:good
 */
long loadpng (const char *filename, dpoint3d *ipo, dpoint3d *ist, dpoint3d *ihe, dpoint3d *ifo)
{
	unsigned long *pngdat, dat[5];
	long i, j, k, l, p, leng, minz = 255, maxz = 0;
	char *v, *buf;
	float f;
	FILE *fp;

	if (!vbuf) { vbuf = (long *)malloc((VOXSIZ>>2)<<2); if (!vbuf) evilquit("vbuf malloc failed"); }
	if (!vbit) { vbit = (long *)malloc((VOXSIZ>>7)<<2); if (!vbit) evilquit("vbuf malloc failed"); }

	if (VSID != 1024) return(0);
	v = (char *)(&vbuf[1]); //1st dword for voxalloc compare logic optimization

	if (!kzopen(filename)) return(0);
	leng = kzfilelength();
	buf = (char *)malloc(leng); if (!buf) { kzclose(); return(0); }
	kzread(buf,leng);
	kzclose();

	kpgetdim(buf,leng,(int *)&i,(int *)&j); if ((i != VSID) && (j != VSID)) { free(buf); return(0); }
	pngdat = (unsigned long *)(&vbuf[(VOXSIZ-VSID*VSID*4)>>2]);
	if (kprender(buf,leng,(long)pngdat,VSID<<2,VSID,VSID,0,0) < 0) return(0);
	free(buf);

	for(i=0;i<VSID*VSID;i++)
	{
		if ((pngdat[i]>>24) < minz) minz = (pngdat[i]>>24);
		if ((pngdat[i]>>24) > maxz) maxz = (pngdat[i]>>24);
	}

		//Fill board data
	minz = lbound(128-((minz+maxz)>>1),-minz,255-maxz);
	for(p=0;p<VSID*VSID;p++)
	{
		dat[0] = pngdat[p];
		dat[1] = pngdat[((p-1)&(VSID-1))+((p     )&((VSID-1)*VSID))];
		dat[2] = pngdat[((p+1)&(VSID-1))+((p     )&((VSID-1)*VSID))];
		dat[3] = pngdat[((p  )&(VSID-1))+((p-VSID)&((VSID-1)*VSID))];
		dat[4] = pngdat[((p  )&(VSID-1))+((p+VSID)&((VSID-1)*VSID))];

		j = 1; l = dat[0];
		for(i=4;i>0;i--)
			if (((signed long)((dat[i]>>24)-(dat[0]>>24))) > j)
				{ j = (dat[i]>>24)-(dat[0]>>24); l = dat[i]; }

		sptr[p] = v;
		v[0] = 0;
		v[1] = (pngdat[p]>>24)+minz;
		v[2] = (pngdat[p]>>24)+minz+j-1;
		v[3] = 0; //dummy (z top)
		v += 4;
		k = (pngdat[p]&0xffffff)|0x80000000;
		if (j == 2)
		{
			l = (((  l     &255)-( k     &255))>>1)      +
				 (((((l>> 8)&255)-((k>> 8)&255))>>1)<< 8) +
				 (((((l>>16)&255)-((k>>16)&255))>>1)<<16);
		}
		else if (j > 2)
		{
			l = (((  l     &255)-( k     &255))/j)      +
				 (((((l>> 8)&255)-((k>> 8)&255))/j)<< 8) +
				 (((((l>>16)&255)-((k>>16)&255))/j)<<16);
		}
		*(long *)v = k; v += 4; j--;
		while (j) { k += l; *(long *)v = colorjit(k,0x30303); v += 4; j--; }
	}

	memset(&sptr[VSID*VSID],0,sizeof(sptr)-VSID*VSID*4);
	vbiti = (((long)v-(long)vbuf)>>2); //# vbuf longs/vbit bits allocated
	clearbuf((void *)vbit,vbiti>>5,-1);
	clearbuf((void *)&vbit[vbiti>>5],(VOXSIZ>>7)-(vbiti>>5),0);
	vbit[vbiti>>5] = (1<<vbiti)-1;

	vx5.globalmass = calcglobalmass();

	ipo->x = VSID*.5; ipo->y = VSID*.5; ipo->z = 128;
	f = 0.0*PI/180.0;
	ist->x = cos(f); ist->y = sin(f); ist->z = 0;
	ihe->x = 0; ihe->y = 0; ihe->z = 1;
	ifo->x = sin(f); ifo->y = -cos(f); ifo->z = 0;

	gmipnum = 1; vx5.flstnum = 0;
	updatebbox(0,0,0,VSID,VSID,MAXZDIM,0);
	return(1);
}


//Quake3 .BSP loading code begins --------------------------------------------
typedef struct { long c, i; float z, z1; } vlinerectyp;
static point3d q3pln[5250];
static float q3pld[5250], q3vz[256];
static long q3nod[4850][3], q3lf[4850];
long vlinebsp (float x, float y, float z0, float z1, float *dvz)
{
	vlinerectyp vlrec[64];
	float z, t;
	long i, j, vcnt, vlcnt;
	char vt[256];

	vcnt = 1; i = 0; vlcnt = 0; vt[0] = 17;
	while (1)
	{
		if (i < 0)
		{
			if (vt[vcnt-1] != (q3lf[~i]&255))
				{ dvz[vcnt] = z0; vt[vcnt] = (q3lf[~i]&255); vcnt++; }
		}
		else
		{
			j = q3nod[i][0]; z = q3pld[j] - q3pln[j].x*x - q3pln[j].y*y;
			t = q3pln[j].z*z0-z;
			if ((t < 0) == (q3pln[j].z*z1 < z))
				{ vlrec[vlcnt].c = 0; i = q3nod[i][(t<0)+1]; }
			else
			{
				z /= q3pln[j].z; j = (q3pln[j].z<0)+1;
				vlrec[vlcnt].c = 1; vlrec[vlcnt].i = q3nod[i][j];
				vlrec[vlcnt].z = z; vlrec[vlcnt].z1 = z1;
				i = q3nod[i][3-j]; z1 = z;
			}
			vlcnt++; continue;
		}
		do { vlcnt--; if (vlcnt < 0) return(vcnt); } while (!vlrec[vlcnt].c);
		vlrec[vlcnt].c = 0; i = vlrec[vlcnt].i;
		z0 = vlrec[vlcnt].z; z1 = vlrec[vlcnt].z1;
		vlcnt++;
	}
	return(0);
}

	//Stupidly useless declarations:
void delslab(long *b2, long y0, long y1);
long *scum2(long x, long y);
void scum2finish();

/**
 * Loads a Quake 3 Arena .BSP format map into memory. First extract the map
 * from the .PAK file. NOTE: only tested with Q3DM(1,7,17) and Q3TOURNEY
 * @param filnam .BSP map formatted like this: "Q3DM17.BSP"
 * @param ipo default starting camera position
 * @param ist RIGHT unit vector
 * @param ihe DOWN unit vector
 * @param ifo FORWARD unit vector
 */
void loadbsp (const char *filnam, dpoint3d *ipo, dpoint3d *ist, dpoint3d *ihe, dpoint3d *ifo)
{
	FILE *fp;
	dpoint3d dp;
	float f, xof, yof, zof, sc, rsc;
	long numplanes, numnodes, numleafs, fpos[17], flng[17];
	long i, x, y, z, z0, z1, vcnt, *lptr, minx, miny, minz, maxx, maxy, maxz;
	char *v;

	if (!vbuf) { vbuf = (long *)malloc((VOXSIZ>>2)<<2); if (!vbuf) evilquit("vbuf malloc failed"); }
	if (!vbit) { vbit = (long *)malloc((VOXSIZ>>7)<<2); if (!vbit) evilquit("vbuf malloc failed"); }

		//Completely re-compile vbuf
	v = (char *)(&vbuf[1]); //1st dword for voxalloc compare logic optimization
	for(x=0;x<VSID;x++)
		for(y=0;y<VSID;y++)
		{
			sptr[y*VSID+x] = v; v[0] = 0; v[1] = 0; v[2] = 0; v[3] = 0; v += 4;
			(*(long *)v) = ((x^y)&15)*0x10101+0x807c7c7c; v += 4;
		}

	memset(&sptr[VSID*VSID],0,sizeof(sptr)-VSID*VSID*4);
	vbiti = (((long)v-(long)vbuf)>>2); //# vbuf longs/vbit bits allocated
	clearbuf((void *)vbit,vbiti>>5,-1);
	clearbuf((void *)&vbit[vbiti>>5],(VOXSIZ>>7)-(vbiti>>5),0);
	vbit[vbiti>>5] = (1<<vbiti)-1;

	if (!kzopen(filnam)) return;
	kzread(&i,4); if (i != 0x50534249) { kzclose(); return; }
	kzread(&i,4); if (i != 0x2e) { kzclose(); return; }
	for(i=0;i<17;i++) { kzread(&fpos[i],4); kzread(&flng[i],4); }
	kzseek(fpos[2],SEEK_SET); numplanes = flng[2]/16;
	for(i=0;i<numplanes;i++) { kzread(&q3pln[i].x,12); kzread(&q3pld[i],4); }
	kzseek(fpos[3],SEEK_SET); numnodes = flng[3]/36;
	minx = 0x7fffffff; miny = 0x7fffffff; minz = 0x7fffffff;
	maxx = 0x80000000; maxy = 0x80000000; maxz = 0x80000000;
	for(i=0;i<numnodes;i++)
	{
		kzread(&q3nod[i][0],12);
		kzread(&x,4); if (x < minx) minx = x;
		kzread(&x,4); if (x < miny) miny = x;
		kzread(&x,4); if (x < minz) minz = x;
		kzread(&x,4); if (x > maxx) maxx = x;
		kzread(&x,4); if (x > maxy) maxy = x;
		kzread(&x,4); if (x > maxz) maxz = x;
	}
	kzseek(fpos[4]+4,SEEK_SET); numleafs = flng[4]/48;
	for(i=0;i<numleafs;i++) { kzread(&q3lf[i],4); kzseek(44,SEEK_CUR); }
	kzclose();

	sc = (float)(VSID-2)/(float)(maxx-minx);
	rsc = (float)(VSID-2)/(float)(maxy-miny); if (rsc < sc) sc = rsc;
	rsc = (float)(MAXZDIM-2)/(float)(maxz-minz); if (rsc < sc) sc = rsc;
	//i = *(long *)sc; i &= 0xff800000; sc = *(float *)i;
	xof = (-(float)(minx+maxx)*sc + VSID   )*.5;
	yof = (+(float)(miny+maxy)*sc + VSID   )*.5;
	zof = (+(float)(minz+maxz)*sc + MAXZDIM)*.5;

	rsc = 1.0 / sc;
	vx5.colfunc = curcolfunc; //0<x0<x1<VSID, 0<y0<y1<VSID, 0<z0<z1<256,
	for(y=0;y<VSID;y++)
		for(x=0;x<VSID;x++)
		{
			lptr = scum2(x,y);

				//voxx = q3x*+sc + xof;
				//voxy = q3y*-sc + yof;
				//voxz = q3z*-sc + zof;
			vcnt = vlinebsp(((float)x-xof)*rsc,((float)y-yof)*-rsc,-65536.0,65536.0,q3vz);
			for(i=vcnt-2;i>0;i-=2)
			{
				ftol(-q3vz[i+1]*sc+zof,&z0); if (z0 < 0) z0 = 0;
				ftol(-q3vz[i  ]*sc+zof,&z1); if (z1 > MAXZDIM) z1 = MAXZDIM;
				delslab(lptr,z0,z1);
			}
		}
	scum2finish();

	vx5.globalmass = calcglobalmass();

		//Find a spot that isn't too close to a wall
	sc = -1; ipo->x = VSID*.5; ipo->y = VSID*.5; ipo->z = -16;
	for(i=4096;i>=0;i--)
	{
		x = (rand()%VSID); y = (rand()%VSID); z = (rand()%MAXZDIM);
		if (!isvoxelsolid(x,y,z))
		{
			rsc = findmaxcr((double)x+.5,(double)y+.5,(double)z+.5,5.0);
			if (rsc <= sc) continue;
			ipo->x = (double)x+.5; ipo->y = (double)x+.5; ipo->z = (double)x+.5;
			sc = rsc; if (sc >= 5.0) break;
		}
	}
	f = 0.0*PI/180.0;
	ist->x = cos(f); ist->y = sin(f); ist->z = 0;
	ihe->x = 0; ihe->y = 0; ihe->z = 1;
	ifo->x = sin(f); ifo->y = -cos(f); ifo->z = 0;

	gmipnum = 1; vx5.flstnum = 0;
	updatebbox(0,0,0,VSID,VSID,MAXZDIM,0);
}

//Quake3 .BSP loading code ends ----------------------------------------------

/**
 * Loads a native Voxlap5 .VXL file into memory.
 * @param filnam .VXL map formatted like this: "UNTITLED.VXL"
 * @param ipo default starting camera position
 * @param ist RIGHT unit vector
 * @param ihe DOWN unit vector
 * @param ifo FORWARD unit vector
 * @return 0:bad, 1:good
 */
long loadvxl (const char *lodfilnam, dpoint3d *ipo, dpoint3d *ist, dpoint3d *ihe, dpoint3d *ifo)
{
	FILE *fil;
	long i, j, fsiz;
	char *v, *v2;

	if (!vbuf) { vbuf = (long *)malloc((VOXSIZ>>2)<<2); if (!vbuf) evilquit("vbuf malloc failed"); }
	if (!vbit) { vbit = (long *)malloc((VOXSIZ>>7)<<2); if (!vbit) evilquit("vbuf malloc failed"); }

	if (!kzopen(lodfilnam)) return(0);
	fsiz = kzfilelength();

	kzread(&i,4); if (i != 0x09072000) return(0);
	kzread(&i,4); if (i != VSID) return(0);
	kzread(&i,4); if (i != VSID) return(0);
	kzread(ipo,24);
	kzread(ist,24);
	kzread(ihe,24);
	kzread(ifo,24);

	v = (char *)(&vbuf[1]); //1st dword for voxalloc compare logic optimization
	kzread((void *)v,fsiz-kztell());

	for(i=0;i<VSID*VSID;i++)
	{
		sptr[i] = v;
		while (v[0]) v += (((long)v[0])<<2);
		v += ((((long)v[2])-((long)v[1])+2)<<2);
	}
	kzclose();

	memset(&sptr[VSID*VSID],0,sizeof(sptr)-VSID*VSID*4);
	vbiti = (((long)v-(long)vbuf)>>2); //# vbuf longs/vbit bits allocated
	clearbuf((void *)vbit,vbiti>>5,-1);
	clearbuf((void *)&vbit[vbiti>>5],(VOXSIZ>>7)-(vbiti>>5),0);
	vbit[vbiti>>5] = (1<<vbiti)-1;

	vx5.globalmass = calcglobalmass();
	backedup = -1;

	gmipnum = 1; vx5.flstnum = 0;
	updatebbox(0,0,0,VSID,VSID,MAXZDIM,0);
	return(1);
}

/**
 * Saves a native Voxlap5 .VXL file & specified position to disk
 * @param filnam .VXL map formatted like this: "UNTITLED.VXL"
 * @param ipo default starting camera position
 * @param ist RIGHT unit vector
 * @param ihe DOWN unit vector
 * @param ifo FORWARD unit vector
 * @return 0:bad, 1:good
 */
long savevxl (const char *savfilnam, dpoint3d *ipo, dpoint3d *ist, dpoint3d *ihe, dpoint3d *ifo)
{
	FILE *fil;
	long i;

	if (!(fil = fopen(savfilnam,"wb"))) return(0);
	i = 0x09072000; fwrite(&i,4,1,fil);  //Version
	i = VSID; fwrite(&i,4,1,fil);
	i = VSID; fwrite(&i,4,1,fil);
	fwrite(ipo,24,1,fil);
	fwrite(ist,24,1,fil);
	fwrite(ihe,24,1,fil);
	fwrite(ifo,24,1,fil);
	for(i=0;i<VSID*VSID;i++) fwrite((void *)sptr[i],slng(sptr[i]),1,fil);
	fclose(fil);
	return(1);
}

/**
 * Loads a sky into memory. Sky must be PNG,JPG,TGA,GIF,BMP,PCX formatted as
 * a Mercator projection on its side. This means x-coordinate is latitude
 * and y-coordinate is longitude. Loadsky() can be called at any time.
 *
 * If for some reason you don't want to load a textured sky, you call call
 * loadsky with these 2 built-in skies:
 * loadsky("BLACK");  //pitch black
 * loadsky("BLUE");   //a cool ramp of bright blue to blue to greenish
 *
 * @param skyfilnam the name of the image to load
 * @return -1:bad, 0:good
 */
long loadsky (const char *skyfilnam)
{
	long x, y, xoff, yoff;
	float ang, f;

	if (skypic) { free((void *)skypic); skypic = skyoff = 0; }
	xoff = yoff = 0;

	if (!strcasecmp(skyfilnam,"BLACK")) return(0);
	if (!strcasecmp(skyfilnam,"BLUE")) goto loadbluesky;

	kpzload(skyfilnam,(int *)&skypic,(int *)&skybpl,(int *)&skyxsiz,(int *)&skyysiz);
	if (!skypic)
	{
		long r, g, b, *p;
loadbluesky:;
			//Load default sky
		skyxsiz = 512; skyysiz = 1; skybpl = skyxsiz*4;
		if (!(skypic = (long)malloc(skyysiz*skybpl))) return(-1);

		p = (long *)skypic; y = skyxsiz*skyxsiz;
		for(x=0;x<=(skyxsiz>>1);x++)
		{
			p[x] = ((((x*1081 - skyxsiz*252)*x)/y + 35)<<16)+
					 ((((x* 950 - skyxsiz*198)*x)/y + 53)<<8)+
					  (((x* 439 - skyxsiz* 21)*x)/y + 98);
		}
		p[skyxsiz-1] = 0x50903c;
		r = ((p[skyxsiz>>1]>>16)&255);
		g = ((p[skyxsiz>>1]>>8)&255);
		b = ((p[skyxsiz>>1])&255);
		for(x=(skyxsiz>>1)+1;x<skyxsiz;x++)
		{
			p[x] = ((((0x50-r)*(x-(skyxsiz>>1)))/(skyxsiz-1-(skyxsiz>>1))+r)<<16)+
					 ((((0x90-g)*(x-(skyxsiz>>1)))/(skyxsiz-1-(skyxsiz>>1))+g)<<8)+
					 ((((0x3c-b)*(x-(skyxsiz>>1)))/(skyxsiz-1-(skyxsiz>>1))+b));
		}
		y = skyxsiz*skyysiz;
		for(x=skyxsiz;x<y;x++) p[x] = p[x-skyxsiz];
	}

		//Initialize look-up table for longitudes
	if (skylng) free((void *)skylng);
	if (!(skylng = (point2d *)malloc(skyysiz*8))) return(-1);
	f = PI*2.0 / ((float)skyysiz);
	for(y=skyysiz-1;y>=0;y--)
		fcossin((float)y*f+PI,&skylng[y].x,&skylng[y].y);
	skylngmul = (float)skyysiz/(PI*2);
		//This makes those while loops in gline() not lockup when skyysiz==1
	if (skyysiz == 1) { skylng[0].x = 0; skylng[0].y = 0; }

		//Initialize look-up table for latitudes
	if (skylat) free((void *)skylat);
	if (!(skylat = (long *)malloc(skyxsiz*4))) return(-1);
	f = PI*.5 / ((float)skyxsiz);
	for(x=skyxsiz-1;x;x--)
	{
		ang = (float)((x<<1)-skyxsiz)*f;
		ftol(cos(ang)*32767.0,&xoff);
		ftol(sin(ang)*32767.0,&yoff);
		skylat[x] = (xoff<<16)+((-yoff)&65535);
	}
	skylat[0] = 0; //Hack to make sure assembly index never goes < 0
	skyxsiz--; //Hack for assembly code

	return(0);
}

void orthonormalize (point3d *v0, point3d *v1, point3d *v2)
{
	float t;

	t = 1.0 / sqrt((v0->x)*(v0->x) + (v0->y)*(v0->y) + (v0->z)*(v0->z));
	(v0->x) *= t; (v0->y) *= t; (v0->z) *= t;
	t = (v1->x)*(v0->x) + (v1->y)*(v0->y) + (v1->z)*(v0->z);
	(v1->x) -= t*(v0->x); (v1->y) -= t*(v0->y); (v1->z) -= t*(v0->z);
	t = 1.0 / sqrt((v1->x)*(v1->x) + (v1->y)*(v1->y) + (v1->z)*(v1->z));
	(v1->x) *= t; (v1->y) *= t; (v1->z) *= t;
	(v2->x) = (v0->y)*(v1->z) - (v0->z)*(v1->y);
	(v2->y) = (v0->z)*(v1->x) - (v0->x)*(v1->z);
	(v2->z) = (v0->x)*(v1->y) - (v0->y)*(v1->x);
}

void dorthonormalize (dpoint3d *v0, dpoint3d *v1, dpoint3d *v2)
{
	double t;

	t = 1.0 / sqrt((v0->x)*(v0->x) + (v0->y)*(v0->y) + (v0->z)*(v0->z));
	(v0->x) *= t; (v0->y) *= t; (v0->z) *= t;
	t = (v1->x)*(v0->x) + (v1->y)*(v0->y) + (v1->z)*(v0->z);
	(v1->x) -= t*(v0->x); (v1->y) -= t*(v0->y); (v1->z) -= t*(v0->z);
	t = 1.0 / sqrt((v1->x)*(v1->x) + (v1->y)*(v1->y) + (v1->z)*(v1->z));
	(v1->x) *= t; (v1->y) *= t; (v1->z) *= t;
	(v2->x) = (v0->y)*(v1->z) - (v0->z)*(v1->y);
	(v2->y) = (v0->z)*(v1->x) - (v0->x)*(v1->z);
	(v2->z) = (v0->x)*(v1->y) - (v0->y)*(v1->x);
}

void orthorotate (float ox, float oy, float oz, point3d *ist, point3d *ihe, point3d *ifo)
{
	float f, t, dx, dy, dz, rr[9];

	fcossin(ox,&ox,&dx);
	fcossin(oy,&oy,&dy);
	fcossin(oz,&oz,&dz);
	f = ox*oz; t = dx*dz; rr[0] =  t*dy + f; rr[7] = -f*dy - t;
	f = ox*dz; t = dx*oz; rr[1] = -f*dy + t; rr[6] =  t*dy - f;
	rr[2] = dz*oy; rr[3] = -dx*oy; rr[4] = ox*oy; rr[8] = oz*oy; rr[5] = dy;
	ox = ist->x; oy = ihe->x; oz = ifo->x;
	ist->x = ox*rr[0] + oy*rr[3] + oz*rr[6];
	ihe->x = ox*rr[1] + oy*rr[4] + oz*rr[7];
	ifo->x = ox*rr[2] + oy*rr[5] + oz*rr[8];
	ox = ist->y; oy = ihe->y; oz = ifo->y;
	ist->y = ox*rr[0] + oy*rr[3] + oz*rr[6];
	ihe->y = ox*rr[1] + oy*rr[4] + oz*rr[7];
	ifo->y = ox*rr[2] + oy*rr[5] + oz*rr[8];
	ox = ist->z; oy = ihe->z; oz = ifo->z;
	ist->z = ox*rr[0] + oy*rr[3] + oz*rr[6];
	ihe->z = ox*rr[1] + oy*rr[4] + oz*rr[7];
	ifo->z = ox*rr[2] + oy*rr[5] + oz*rr[8];
	//orthonormalize(ist,ihe,ifo);
}

void dorthorotate (double ox, double oy, double oz, dpoint3d *ist, dpoint3d *ihe, dpoint3d *ifo)
{
	double f, t, dx, dy, dz, rr[9];

	dcossin(ox,&ox,&dx);
	dcossin(oy,&oy,&dy);
	dcossin(oz,&oz,&dz);
	f = ox*oz; t = dx*dz; rr[0] =  t*dy + f; rr[7] = -f*dy - t;
	f = ox*dz; t = dx*oz; rr[1] = -f*dy + t; rr[6] =  t*dy - f;
	rr[2] = dz*oy; rr[3] = -dx*oy; rr[4] = ox*oy; rr[8] = oz*oy; rr[5] = dy;
	ox = ist->x; oy = ihe->x; oz = ifo->x;
	ist->x = ox*rr[0] + oy*rr[3] + oz*rr[6];
	ihe->x = ox*rr[1] + oy*rr[4] + oz*rr[7];
	ifo->x = ox*rr[2] + oy*rr[5] + oz*rr[8];
	ox = ist->y; oy = ihe->y; oz = ifo->y;
	ist->y = ox*rr[0] + oy*rr[3] + oz*rr[6];
	ihe->y = ox*rr[1] + oy*rr[4] + oz*rr[7];
	ifo->y = ox*rr[2] + oy*rr[5] + oz*rr[8];
	ox = ist->z; oy = ihe->z; oz = ifo->z;
	ist->z = ox*rr[0] + oy*rr[3] + oz*rr[6];
	ihe->z = ox*rr[1] + oy*rr[4] + oz*rr[7];
	ifo->z = ox*rr[2] + oy*rr[5] + oz*rr[8];
	//dorthonormalize(ist,ihe,ifo);
}

void axisrotate (point3d *p, point3d *axis, float w)
{
	point3d ax;
	float t, c, s, ox, oy, oz, k[9];

	fcossin(w,&c,&s);
	t = axis->x*axis->x + axis->y*axis->y + axis->z*axis->z; if (t == 0) return;
	t = 1.0 / sqrt(t); ax.x = axis->x*t; ax.y = axis->y*t; ax.z = axis->z*t;

	t = 1.0-c;
	k[0] = ax.x*t; k[7] = ax.x*s; oz = ax.y*k[0];
	k[4] = ax.y*t; k[2] = ax.y*s; oy = ax.z*k[0];
	k[8] = ax.z*t; k[3] = ax.z*s; ox = ax.z*k[4];
	k[0] = ax.x*k[0] + c; k[5] = ox - k[7]; k[7] += ox;
	k[4] = ax.y*k[4] + c; k[6] = oy - k[2]; k[2] += oy;
	k[8] = ax.z*k[8] + c; k[1] = oz - k[3]; k[3] += oz;

	ox = p->x; oy = p->y; oz = p->z;
	p->x = ox*k[0] + oy*k[1] + oz*k[2];
	p->y = ox*k[3] + oy*k[4] + oz*k[5];
	p->z = ox*k[6] + oy*k[7] + oz*k[8];
}

void slerp (point3d *istr, point3d *ihei, point3d *ifor,
				point3d *istr2, point3d *ihei2, point3d *ifor2,
				point3d *ist, point3d *ihe, point3d *ifo, float rat)
{
	point3d ax;
	float c, s, t, ox, oy, oz, k[9];

	ist->x = istr->x; ist->y = istr->y; ist->z = istr->z;
	ihe->x = ihei->x; ihe->y = ihei->y; ihe->z = ihei->z;
	ifo->x = ifor->x; ifo->y = ifor->y; ifo->z = ifor->z;

	ax.x = istr->y*istr2->z - istr->z*istr2->y + ihei->y*ihei2->z - ihei->z*ihei2->y + ifor->y*ifor2->z - ifor->z*ifor2->y;
	ax.y = istr->z*istr2->x - istr->x*istr2->z + ihei->z*ihei2->x - ihei->x*ihei2->z + ifor->z*ifor2->x - ifor->x*ifor2->z;
	ax.z = istr->x*istr2->y - istr->y*istr2->x + ihei->x*ihei2->y - ihei->y*ihei2->x + ifor->x*ifor2->y - ifor->y*ifor2->x;
	t = ax.x*ax.x + ax.y*ax.y + ax.z*ax.z; if (t == 0) return;

		//Based on the vector suck-out method (see ROTATE2.BAS)
	ox = istr->x*ax.x + istr->y*ax.y + istr->z*ax.z;
	oy = ihei->x*ax.x + ihei->y*ax.y + ihei->z*ax.z;
	if (fabs(ox) < fabs(oy))
		{ c = istr->x*istr2->x + istr->y*istr2->y + istr->z*istr2->z; s = ox*ox; }
	else
		{ c = ihei->x*ihei2->x + ihei->y*ihei2->y + ihei->z*ihei2->z; s = oy*oy; }
	if (t == s) return;
	c = (c*t - s) / (t-s);
	if (c < -1) c = -1;
	if (c > 1) c = 1;
	fcossin(acos(c)*rat,&c,&s);

	t = 1.0 / sqrt(t); ax.x *= t; ax.y *= t; ax.z *= t;

	t = 1.0f-c;
	k[0] = ax.x*t; k[7] = ax.x*s; oz = ax.y*k[0];
	k[4] = ax.y*t; k[2] = ax.y*s; oy = ax.z*k[0];
	k[8] = ax.z*t; k[3] = ax.z*s; ox = ax.z*k[4];
	k[0] = ax.x*k[0] + c; k[5] = ox - k[7]; k[7] += ox;
	k[4] = ax.y*k[4] + c; k[6] = oy - k[2]; k[2] += oy;
	k[8] = ax.z*k[8] + c; k[1] = oz - k[3]; k[3] += oz;

	ox = ist->x; oy = ist->y; oz = ist->z;
	ist->x = ox*k[0] + oy*k[1] + oz*k[2];
	ist->y = ox*k[3] + oy*k[4] + oz*k[5];
	ist->z = ox*k[6] + oy*k[7] + oz*k[8];

	ox = ihe->x; oy = ihe->y; oz = ihe->z;
	ihe->x = ox*k[0] + oy*k[1] + oz*k[2];
	ihe->y = ox*k[3] + oy*k[4] + oz*k[5];
	ihe->z = ox*k[6] + oy*k[7] + oz*k[8];

	ox = ifo->x; oy = ifo->y; oz = ifo->z;
	ifo->x = ox*k[0] + oy*k[1] + oz*k[2];
	ifo->y = ox*k[3] + oy*k[4] + oz*k[5];
	ifo->z = ox*k[6] + oy*k[7] + oz*k[8];
}

void expandrle (long x, long y, long *uind)
{
	long i;
	char *v;

	if ((x|y)&(~(VSID-1))) { uind[0] = 0; uind[1] = MAXZDIM; return; }

	v = sptr[y*VSID+x]; uind[0] = v[1]; i = 2;
	while (v[0])
	{
		v += v[0]*4; if (v[3] >= v[1]) continue;
		uind[i-1] = v[3]; uind[i] = v[1]; i += 2;
	}
	uind[i-1] = MAXZDIM;
}

	//Inputs:  n0[<=MAXZDIM]: rle buffer of column to compress
	//         n1-4[<=MAXZDIM]: neighboring rle buffers
	//         top,bot,top,bot,... (ends when bot == MAXZDIM)
	//         px,py: takes color from original column (before modification)
	//            If originally unexposed, calls vx5.colfunc(.)
	//Outputs: cbuf[MAXCSIZ]: compressed output buffer
	//Returns: n: length of compressed buffer (in bytes)
long compilerle (long *n0, long *n1, long *n2, long *n3, long *n4, char *cbuf, long px, long py)
{
	long i, ia, ze, zend, onext, dacnt, n, *ic;
	lpoint3d p;
	char *v;

	p.x = px; p.y = py;

		//Generate pointers to color slabs in this format:
		//   0:z0,  1:z1,  2:(pointer to z0's color)-z0
	v = sptr[py*VSID+px]; ic = tbuf2;
	while (1)
	{
		ia = v[1]; p.z = v[2];
		ic[0] = ia; ic[1] = p.z+1; ic[2] = ((long)v)-(ia<<2)+4; ic += 3;
		i = v[0]; if (!i) break;
		v += i*4; ze = v[3];
		ic[0] = ze+p.z-ia-i+2; ic[1] = ze; ic[2] = ((long)v)-(ze<<2); ic += 3;
	}
	ic[0] = MAXZDIM; ic[1] = MAXZDIM;

	p.z = n0[0]; cbuf[1] = n0[0];
	ze = n0[1]; cbuf[2] = ze-1;
	cbuf[3] = 0;
	i = onext = 0; ic = tbuf2; ia = 15; n = 4;
	if (ze != MAXZDIM) zend = ze-1; else zend = -1;
	while (1)
	{
		dacnt = 0;
		while (1)
		{
			do
			{
				while (p.z >= ic[1]) ic += 3;
				if (p.z >= ic[0]) *(long *)&cbuf[n] = *(long *)(ic[2]+(p.z<<2));
								 else *(long *)&cbuf[n] = vx5.colfunc(&p);
				n += 4; p.z++; if (p.z >= ze) goto rlendit2;
				while (p.z >= n1[0]) { n1++; ia ^= 1; }
				while (p.z >= n2[0]) { n2++; ia ^= 2; }
				while (p.z >= n3[0]) { n3++; ia ^= 4; }
				while (p.z >= n4[0]) { n4++; ia ^= 8; }
			} while ((ia) || (p.z == zend));

			if (!dacnt) { cbuf[onext+2] = p.z-1; dacnt = 1; }
			else
			{
				cbuf[onext] = ((n-onext)>>2); onext = n;
				cbuf[n+1] = p.z; cbuf[n+2] = p.z-1; cbuf[n+3] = p.z; n += 4;
			}

			if ((n1[0] < n2[0]) && (n1[0] < n3[0]) && (n1[0] < n4[0]))
				{ if (n1[0] >= ze) { p.z = ze-1; } else { p.z = *n1++; ia ^= 1; } }
			else if ((n2[0] < n3[0]) && (n2[0] < n4[0]))
				{ if (n2[0] >= ze) { p.z = ze-1; } else { p.z = *n2++; ia ^= 2; } }
			else if (n3[0] < n4[0])
				{ if (n3[0] >= ze) { p.z = ze-1; } else { p.z = *n3++; ia ^= 4; } }
			else
				{ if (n4[0] >= ze) { p.z = ze-1; } else { p.z = *n4++; ia ^= 8; } }

			if (p.z == MAXZDIM-1) goto rlenditall;
		}
rlendit2:;
		if (ze >= MAXZDIM) break;

		i += 2;
		cbuf[onext] = ((n-onext)>>2); onext = n;
		p.z = n0[i]; cbuf[n+1] = n0[i]; cbuf[n+3] = ze;
		ze = n0[i+1]; cbuf[n+2] = ze-1;
		n += 4;
	}
rlenditall:;
	cbuf[onext] = 0;
	return(n);
}

	//Delete everything on b2() in y0<=y<y1
void delslab (long *b2, long y0, long y1)
{
	long i, j, z;

	if (y1 >= MAXZDIM) y1 = MAXZDIM-1;
	if ((y0 >= y1) || (!b2)) return;
	for(z=0;y0>=b2[z+1];z+=2);
	if (y0 > b2[z])
	{
		if (y1 < b2[z+1])
		{
			for(i=z;b2[i+1]<MAXZDIM;i+=2);
			while (i > z) { b2[i+3] = b2[i+1]; b2[i+2] = b2[i]; i -= 2; }
			b2[z+3] = b2[z+1]; b2[z+1] = y0; b2[z+2] = y1; return;
		}
		b2[z+1] = y0; z += 2;
	}
	if (y1 >= b2[z+1])
	{
		for(i=z+2;y1>=b2[i+1];i+=2);
		j = z-i; b2[z] = b2[i]; b2[z+1] = b2[i+1];
		while (b2[i+1] < MAXZDIM)
			{ i += 2; b2[i+j] = b2[i]; b2[i+j+1] = b2[i+1]; }
	}
	if (y1 > b2[z]) b2[z] = y1;
}

	//Insert everything on b2() in y0<=y<y1
void insslab (long *b2, long y0, long y1)
{
	long i, j, z;

	if ((y0 >= y1) || (!b2)) return;
	for(z=0;y0>b2[z+1];z+=2);
	if (y1 < b2[z])
	{
		for(i=z;b2[i+1]<MAXZDIM;i+=2);
		do { b2[i+3] = b2[i+1]; b2[i+2] = b2[i]; i -= 2; } while (i >= z);
		b2[z+1] = y1; b2[z] = y0; return;
	}
	if (y0 < b2[z]) b2[z] = y0;
	if ((y1 >= b2[z+2]) && (b2[z+1] < MAXZDIM))
	{
		for(i=z+2;(y1 >= b2[i+2]) && (b2[i+1] < MAXZDIM);i+=2);
		j = z-i; b2[z+1] = b2[i+1];
		while (b2[i+1] < MAXZDIM)
			{ i += 2; b2[i+j] = b2[i]; b2[i+j+1] = b2[i+1]; }
	}
	if (y1 > b2[z+1]) b2[z+1] = y1;
}

//------------------------ SETCOLUMN CODE BEGINS ----------------------------

static long scx0, scx1, scox0, scox1, scoox0, scoox1;
static long scex0, scex1, sceox0, sceox1, scoy = 0x80000000, *scoym3;

void scumline ()
{
	long i, j, k, x, y, x0, x1, *mptr, *uptr;
	char *v;

	x0 = MIN(scox0-1,MIN(scx0,scoox0)); scoox0 = scox0; scox0 = scx0;
	x1 = MAX(scox1+1,MAX(scx1,scoox1)); scoox1 = scox1; scox1 = scx1;

	uptr = &scoym3[SCPITCH]; if (uptr == &radar[SCPITCH*9]) uptr = &radar[SCPITCH*6];
	mptr = &uptr[SCPITCH];   if (mptr == &radar[SCPITCH*9]) mptr = &radar[SCPITCH*6];

	if ((x1 < sceox0) || (x0 > sceox1))
	{
		for(x=x0;x<=x1;x++) expandstack(x,scoy-2,&uptr[x*SCPITCH*3]);
	}
	else
	{
		for(x=x0;x<sceox0;x++) expandstack(x,scoy-2,&uptr[x*SCPITCH*3]);
		for(x=x1;x>sceox1;x--) expandstack(x,scoy-2,&uptr[x*SCPITCH*3]);
	}

	if ((scex1|x1) >= 0)
	{
		for(x=x1+2;x<scex0;x++) expandstack(x,scoy-1,&mptr[x*SCPITCH*3]);
		for(x=x0-2;x>scex1;x--) expandstack(x,scoy-1,&mptr[x*SCPITCH*3]);
	}
	if ((x1+1 < scex0) || (x0-1 > scex1))
	{
		for(x=x0-1;x<=x1+1;x++) expandstack(x,scoy-1,&mptr[x*SCPITCH*3]);
	}
	else
	{
		for(x=x0-1;x<scex0;x++) expandstack(x,scoy-1,&mptr[x*SCPITCH*3]);
		for(x=x1+1;x>scex1;x--) expandstack(x,scoy-1,&mptr[x*SCPITCH*3]);
	}
	sceox0 = MIN(x0-1,scex0);
	sceox1 = MAX(x1+1,scex1);

	if ((x1 < scx0) || (x0 > scx1))
	{
		for(x=x0;x<=x1;x++) expandstack(x,scoy,&scoym3[x*SCPITCH*3]);
	}
	else
	{
		for(x=x0;x<scx0;x++) expandstack(x,scoy,&scoym3[x*SCPITCH*3]);
		for(x=x1;x>scx1;x--) expandstack(x,scoy,&scoym3[x*SCPITCH*3]);
	}
	scex0 = x0;
	scex1 = x1;

	y = scoy-1; if (y&(~(VSID-1))) return;
	if (x0 < 0) x0 = 0;
	if (x1 >= VSID) x1 = VSID-1;
	i = y*VSID+x0; k = x0*SCPITCH*3;
	for(x=x0;x<=x1;x++,i++,k+=SCPITCH*3)
	{
		v = sptr[i]; vx5.globalmass += v[1];
		while (v[0]) { v += v[0]*4; vx5.globalmass += v[1]-v[3]; }

			//De-allocate column (x,y)
		voxdealloc(sptr[i]);

		j = compilestack(&mptr[k],&mptr[k-SCPITCH*3],&mptr[k+SCPITCH*3],&uptr[k],&scoym3[k],
							  tbuf,x,y);

			//Allocate & copy to new column (x,y)
		sptr[i] = v = voxalloc(j); copybuf((void *)tbuf,(void *)v,j>>2);

		vx5.globalmass -= v[1];
		while (v[0]) { v += v[0]*4; vx5.globalmass += v[3]-v[1]; }
	}
}

	//x: x on voxel map
	//y: y on voxel map
	//z0: highest z on column
	//z1: lowest z(+1) on column
	//nbuf: buffer of color data from nbuf[z0] to nbuf[z1-1];
	//           -3: don't modify voxel
	//           -2: solid voxel (unexposed): to be calculated in compilestack
	//           -1: write air voxel
	//   0-16777215: write solid voxel (exposed)
void scum (long x, long y, long z0, long z1, long *nbuf)
{
	long z, *mptr;

	if ((x|y)&(~(VSID-1))) return;

	if (y != scoy)
	{
		if (scoy >= 0)
		{
			scumline();
			while (scoy < y-1)
			{
				scx0 = 0x7fffffff; scx1 = 0x80000000;
				scoy++; scoym3 += SCPITCH; if (scoym3 == &radar[SCPITCH*9]) scoym3 = &radar[SCPITCH*6];
				scumline();
			}
			scoy++; scoym3 += SCPITCH; if (scoym3 == &radar[SCPITCH*9]) scoym3 = &radar[SCPITCH*6];
		}
		else
		{
			scoox0 = scox0 = 0x7fffffff;
			sceox0 = scex0 = x+1;
			sceox1 = scex1 = x;
			scoy = y; scoym3 = &radar[SCPITCH*6];
		}
		scx0 = x;
	}
	else
	{
		while (scx1 < x-1) { scx1++; expandstack(scx1,y,&scoym3[scx1*SCPITCH*3]); }
	}

	mptr = &scoym3[x*SCPITCH*3]; scx1 = x; expandstack(x,y,mptr);

		//Modify column (x,y):
	if (nbuf[MAXZDIM-1] == -1) nbuf[MAXZDIM-1] = -2; //Bottom voxel must be solid
	for(z=z0;z<z1;z++)
		if (nbuf[z] != -3) mptr[z] = nbuf[z];
}

void scumfinish ()
{
	long i;

	if (scoy == 0x80000000) return;
	for(i=2;i;i--)
	{
		scumline(); scx0 = 0x7fffffff; scx1 = 0x80000000;
		scoy++; scoym3 += SCPITCH; if (scoym3 == &radar[SCPITCH*9]) scoym3 = &radar[SCPITCH*6];
	}
	scumline(); scoy = 0x80000000;
}

	//Example of how to use this code:
	//vx5.colfunc = curcolfunc; //0<x0<x1<VSID, 0<y0<y1<VSID, 0<z0<z1<256,
	//clearbuf((void *)&templongbuf[z0],z1-z0,-1); //Ex: set all voxels to air
	//for(y=y0;y<y1;y++) //MUST iterate x&y in this order, but can skip around
	//   for(x=x0;x<x1;x++)
	//      if (rand()&8) scum(x,y,z0,z1,templongbuf));
	//scumfinish(); //MUST call this when done!

void scum2line ()
{
	long i, j, k, x, y, x0, x1, *mptr, *uptr;
	char *v;

	x0 = MIN(scox0-1,MIN(scx0,scoox0)); scoox0 = scox0; scox0 = scx0;
	x1 = MAX(scox1+1,MAX(scx1,scoox1)); scoox1 = scox1; scox1 = scx1;

	uptr = &scoym3[SCPITCH]; if (uptr == &radar[SCPITCH*9]) uptr = &radar[SCPITCH*6];
	mptr = &uptr[SCPITCH];   if (mptr == &radar[SCPITCH*9]) mptr = &radar[SCPITCH*6];

	if ((x1 < sceox0) || (x0 > sceox1))
	{
		for(x=x0;x<=x1;x++) expandrle(x,scoy-2,&uptr[x*SCPITCH*3]);
	}
	else
	{
		for(x=x0;x<sceox0;x++) expandrle(x,scoy-2,&uptr[x*SCPITCH*3]);
		for(x=x1;x>sceox1;x--) expandrle(x,scoy-2,&uptr[x*SCPITCH*3]);
	}

	if ((scex1|x1) >= 0)
	{
		for(x=x1+2;x<scex0;x++) expandrle(x,scoy-1,&mptr[x*SCPITCH*3]);
		for(x=x0-2;x>scex1;x--) expandrle(x,scoy-1,&mptr[x*SCPITCH*3]);
	}
	if ((x1+1 < scex0) || (x0-1 > scex1))
	{
		for(x=x0-1;x<=x1+1;x++) expandrle(x,scoy-1,&mptr[x*SCPITCH*3]);
	}
	else
	{
		for(x=x0-1;x<scex0;x++) expandrle(x,scoy-1,&mptr[x*SCPITCH*3]);
		for(x=x1+1;x>scex1;x--) expandrle(x,scoy-1,&mptr[x*SCPITCH*3]);
	}
	sceox0 = MIN(x0-1,scex0);
	sceox1 = MAX(x1+1,scex1);

	if ((x1 < scx0) || (x0 > scx1))
	{
		for(x=x0;x<=x1;x++) expandrle(x,scoy,&scoym3[x*SCPITCH*3]);
	}
	else
	{
		for(x=x0;x<scx0;x++) expandrle(x,scoy,&scoym3[x*SCPITCH*3]);
		for(x=x1;x>scx1;x--) expandrle(x,scoy,&scoym3[x*SCPITCH*3]);
	}
	scex0 = x0;
	scex1 = x1;

	y = scoy-1; if (y&(~(VSID-1))) return;
	if (x0 < 0) x0 = 0;
	if (x1 >= VSID) x1 = VSID-1;
	i = y*VSID+x0; k = x0*SCPITCH*3;
	for(x=x0;x<=x1;x++,i++,k+=SCPITCH*3)
	{
		j = compilerle(&mptr[k],&mptr[k-SCPITCH*3],&mptr[k+SCPITCH*3],&uptr[k],&scoym3[k],
							tbuf,x,y);

		v = sptr[i]; vx5.globalmass += v[1];
		while (v[0]) { v += v[0]*4; vx5.globalmass += v[1]-v[3]; }

			//De-allocate column (x,y)  Note: Must be AFTER compilerle!
		voxdealloc(sptr[i]);

			//Allocate & copy to new column (x,y)
		sptr[i] = v = voxalloc(j); copybuf((void *)tbuf,(void *)v,j>>2);

		vx5.globalmass -= v[1];
		while (v[0]) { v += v[0]*4; vx5.globalmass += v[3]-v[1]; }
	}
}

	//x: x on voxel map
	//y: y on voxel map
	//Returns pointer to rle column (x,y)
long *scum2 (long x, long y)
{
	long *mptr;

	if ((x|y)&(~(VSID-1))) return(0);

	if (y != scoy)
	{
		if (scoy >= 0)
		{
			scum2line();
			while (scoy < y-1)
			{
				scx0 = 0x7fffffff; scx1 = 0x80000000;
				scoy++; scoym3 += SCPITCH; if (scoym3 == &radar[SCPITCH*9]) scoym3 = &radar[SCPITCH*6];
				scum2line();
			}
			scoy++; scoym3 += SCPITCH; if (scoym3 == &radar[SCPITCH*9]) scoym3 = &radar[SCPITCH*6];
		}
		else
		{
			scoox0 = scox0 = 0x7fffffff;
			sceox0 = scex0 = x+1;
			sceox1 = scex1 = x;
			scoy = y; scoym3 = &radar[SCPITCH*6];
		}
		scx0 = x;
	}
	else
	{
		while (scx1 < x-1) { scx1++; expandrle(scx1,y,&scoym3[scx1*SCPITCH*3]); }
	}

	mptr = &scoym3[x*SCPITCH*3]; scx1 = x; expandrle(x,y,mptr);
	return(mptr);
}

void scum2finish ()
{
	long i;

	if (scoy == 0x80000000) return;
	for(i=2;i;i--)
	{
		scum2line(); scx0 = 0x7fffffff; scx1 = 0x80000000;
		scoy++; scoym3 += SCPITCH; if (scoym3 == &radar[SCPITCH*9]) scoym3 = &radar[SCPITCH*6];
	}
	scum2line(); scoy = 0x80000000;
}

//------------------- editing backup / restore begins ------------------------

void voxdontrestore ()
{
	long i;

	if (backedup == 1)
	{
		for(i=(bacx1-bacx0)*(bacy1-bacy0)-1;i>=0;i--) voxdealloc(bacsptr[i]);
	}
	backedup = -1;
}

void voxrestore ()
{
	long i, j, x, y;
	char *v, *daptr;

	if (backedup == 1)
	{
		i = 0;
		for(y=bacy0;y<bacy1;y++)
		{
			j = y*VSID;
			for(x=bacx0;x<bacx1;x++)
			{
				v = sptr[j+x]; vx5.globalmass += v[1];
				while (v[0]) { v += v[0]*4; vx5.globalmass += v[1]-v[3]; }

				voxdealloc(sptr[j+x]);
				sptr[j+x] = bacsptr[i]; i++;

				v = sptr[j+x]; vx5.globalmass -= v[1];
				while (v[0]) { v += v[0]*4; vx5.globalmass += v[3]-v[1]; }
			}
		}
		if (vx5.vxlmipuse > 1) genmipvxl(bacx0,bacy0,bacx1,bacy1);
	}
	else if (backedup == 2)
	{
		daptr = (char *)bacsptr;
		for(y=bacy0;y<bacy1;y++)
		{
			j = y*VSID;
			for(x=bacx0;x<bacx1;x++)
			{
				for(v=sptr[j+x];v[0];v+=v[0]*4)
					for(i=1;i<v[0];i++) v[(i<<2)+3] = *daptr++;
				for(i=1;i<=v[2]-v[1]+1;i++) v[(i<<2)+3] = *daptr++;
			}
		}
		if (vx5.vxlmipuse > 1) genmipvxl(bacx0,bacy0,bacx1,bacy1);
	}
	backedup = -1;
}

void voxbackup (long x0, long y0, long x1, long y1, long tag)
{
	long i, j, n, x, y;
	char *v, *daptr;

	voxdontrestore();

	x0 = MAX(x0-2,0); y0 = MAX(y0-2,0);
	x1 = MIN(x1+2,VSID); y1 = MIN(y1+2,VSID);
	if ((x1-x0)*(y1-y0) > 262144) return;

	bacx0 = x0; bacy0 = y0; bacx1 = x1; bacy1 = y1; backtag = tag;

	if (tag&0x10000)
	{
		backedup = 1;
		i = 0;
		for(y=bacy0;y<bacy1;y++)
		{
			j = y*VSID;
			for(x=bacx0;x<bacx1;x++)
			{
				bacsptr[i] = v = sptr[j+x]; i++;
				n = slng(v); sptr[j+x] = voxalloc(n);

				copybuf((void *)v,(void *)sptr[j+x],n>>2);
			}
		}
	}
	else if (tag&0x20000)
	{
		backedup = 2;
			//WARNING!!! Right now, function will crash if saving more than
			//   1<<20 colors :( This needs to be addressed!!!
		daptr = (char *)bacsptr;
		for(y=bacy0;y<bacy1;y++)
		{
			j = y*VSID;
			for(x=bacx0;x<bacx1;x++)
			{
				for(v=sptr[j+x];v[0];v+=v[0]*4)
					for(i=1;i<v[0];i++) *daptr++ = v[(i<<2)+3];
				for(i=1;i<=v[2]-v[1]+1;i++) *daptr++ = v[(i<<2)+3];
			}
		}
	}
	else backedup = 0;
}

//-------------------- editing backup / restore ends -------------------------

	//WARNING! Make sure to set vx5.colfunc before calling this function!
	//This function is here for simplicity only - it is NOT optimal.
	//
	//   -1: set air
	//   -2: use vx5.colfunc
void setcube (long px, long py, long pz, long col)
{
	long bakcol, (*bakcolfunc)(lpoint3d *), *lptr;

	vx5.minx = px; vx5.maxx = px+1;
	vx5.miny = py; vx5.maxy = py+1;
	vx5.minz = pz; vx5.maxz = pz+1;
	if ((unsigned long)pz >= MAXZDIM) return;
	if ((unsigned long)col >= (unsigned long)0xfffffffe) //-1 or -2
	{
		lptr = scum2(px,py);
		if (col == -1) delslab(lptr,pz,pz+1); else insslab(lptr,pz,pz+1);
		scum2finish();
		updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,col);
		return;
	}

	bakcol = getcube(px,py,pz);
	if (bakcol == 1) return; //Unexposed solid
	if (bakcol != 0) //Not 0 (air)
		*(long *)bakcol = col;
	else
	{
		bakcolfunc = vx5.colfunc; bakcol = vx5.curcol;
		vx5.colfunc = curcolfunc; vx5.curcol = col;
		insslab(scum2(px,py),pz,pz+1); scum2finish();
		vx5.colfunc = bakcolfunc; vx5.curcol = bakcol;
	}
	updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,0);
}

//-------------------------- SETCOLUMN CODE ENDS ----------------------------

static int64_t qmulmip[8] =
{
	0x7fff7fff7fff7fff,0x4000400040004000,0x2aaa2aaa2aaa2aaa,0x2000200020002000,
	0x1999199919991999,0x1555155515551555,0x1249124912491249,0x1000100010001000
};
static long mixc[MAXZDIM>>1][8]; //4K
static long mixn[MAXZDIM>>1];    //0.5K
void genmipvxl (long x0, long y0, long x1, long y1)
{
	long i, n, oldn, x, y, z, xsiz, ysiz, zsiz, oxsiz, oysiz;
	long cz, oz, nz, zz, besti, cstat, curz[4], curzn[4][4], mipnum, mipmax;
	char *v[4], *tv, **sr, **sw, **ssr, **ssw;

	if ((!(x0|y0)) && (x1 == VSID) && (y1 == VSID)) mipmax = vx5.vxlmipuse;
															 else mipmax = gmipnum;
	if (mipmax <= 0) return;
	mipnum = 1;

	vx5.colfunc = curcolfunc;
	xsiz = VSID; ysiz = VSID; zsiz = MAXZDIM;
	ssr = sptr; ssw = sptr+xsiz*ysiz;
	while ((xsiz > 1) && (ysiz > 1) && (zsiz > 1) && (mipnum < mipmax))
	{
		oxsiz = xsiz; xsiz >>= 1;
		oysiz = ysiz; ysiz >>= 1;
						  zsiz >>= 1;

		x0--; if (x0 < 0) x0 = 0;
		y0--; if (y0 < 0) y0 = 0;
		x1++; if (x1 > VSID) x1 = VSID;
		y1++; if (y1 > VSID) y1 = VSID;

		x0 >>= 1; x1 = ((x1+1)>>1);
		y0 >>= 1; y1 = ((y1+1)>>1);
		for(y=y0;y<y1;y++)
		{
			sr = ssr+oxsiz*(y<<1)+(x0<<1);
			sw = ssw+xsiz*y+x0;
			for(x=x0;x<x1;x++)
			{
					//ÚÄÄÄÂÄÄÄÂÄÄÄÂÄÄÄ¿
					//³npt³z1 ³z1c³dum³
					//³ b ³ g ³ r ³ i ³
					//³ b ³ g ³ r ³ i ³
					//³npt³z1 ³z1c³z0 ³
					//³ b ³ g ³ r ³ i ³
					//ÀÄÄÄÁÄÄÄÁÄÄÄÁÄÄÄÙ
				v[0] = sr[      0];
				v[1] = sr[      1];
				v[2] = sr[oysiz  ];
				v[3] = sr[oysiz+1];
				for(i=3;i>=0;i--)
				{
					curz[i] = curzn[i][0] = (long)v[i][1];
					curzn[i][1] = ((long)v[i][2])+1;

					tv = v[i];
					while (1)
					{
						oz = (long)tv[1];
						for(z=oz;z<=((long)tv[2]);z++)
						{
							nz = (z>>1);
							mixc[nz][mixn[nz]++] = *(long *)(&tv[((z-oz)<<2)+4]);
						}
						z = (z-oz) - (((long)tv[0])-1);
						if (!tv[0]) break;
						tv += (((long)tv[0])<<2);
						oz = (long)tv[3];
						for(;z<0;z++)
						{
							nz = ((z+oz)>>1);
							mixc[nz][mixn[nz]++] = *(long *)(&tv[z<<2]);
						}
					}
				}
				cstat = 0; oldn = 0; n = 4; tbuf[3] = 0; z = 0x80000000;
				while (1)
				{
					oz = z;

						//z,besti = min,argMIN(curz[0],curz[1],curz[2],curz[3])
					besti = (((unsigned long)(curz[1]-curz[    0]))>>31);
						 i = (((unsigned long)(curz[3]-curz[    2]))>>31)+2;
					besti +=(((( signed long)(curz[i]-curz[besti]))>>31)&(i-besti));
					z = curz[besti]; if (z >= MAXZDIM) break;

					if ((!cstat) && ((z>>1) >= ((oz+1)>>1)))
					{
						if (oz >= 0)
						{
							tbuf[oldn] = ((n-oldn)>>2);
							tbuf[oldn+2]--;
							tbuf[n+3] = ((oz+1)>>1);
							oldn = n; n += 4;
						}
						tbuf[oldn] = 0;
						tbuf[oldn+1] = tbuf[oldn+2] = (z>>1); cz = -1;
					}
					if (cstat&0x1111)
					{
						if (((((long)tbuf[oldn+2])<<1)+1 >= oz) && (cz < 0))
						{
							while ((((long)tbuf[oldn+2])<<1) < z)
							{
								zz = (long)tbuf[oldn+2];
								*(long *)&tbuf[n] = mixc[zz][rand()%mixn[zz]];
								mixn[zz] = 0;
								tbuf[oldn+2]++; n += 4;
							}
						}
						else
						{
							if (cz < 0) cz = (oz>>1);
							else if ((cz<<1)+1 < oz)
							{
									//Insert fake slab
								tbuf[oldn] = ((n-oldn)>>2);
								tbuf[oldn+2]--;
								tbuf[n] = 0;
								tbuf[n+1] = tbuf[n+2] = tbuf[n+3] = cz;
								oldn = n; n += 4;
								cz = (oz>>1);
							}
							while ((cz<<1) < z)
							{
								*(long *)&tbuf[n] = mixc[cz][rand()%mixn[cz]];
								mixn[cz] = 0;
								cz++; n += 4;
							}
						}
					}

					i = (besti<<2);
					cstat = (((1<<i)+cstat)&0x3333); //--33--22--11--00
					switch ((cstat>>i)&3)
					{
						case 0: curz[besti] = curzn[besti][0]; break;
						case 1: curz[besti] = curzn[besti][1]; break;
						case 2:
							if (!(v[besti][0])) { curz[besti] = MAXZDIM; }
							else
							{
								tv = v[besti]; i = (((long)tv[2])-((long)tv[1])+1)-(((long)tv[0])-1);
								tv += (((long)tv[0])<<2);
								curz[besti] = ((long)(tv[3])) + i;
								curzn[besti][3] = (long)(tv[3]);
								curzn[besti][0] = (long)(tv[1]);
								curzn[besti][1] = ((long)tv[2])+1;
								v[besti] = tv;
							}
							break;
						case 3: curz[besti] = curzn[besti][3]; break;
						//default: _gtfo(); //tells MSVC default can't be reached
					}
				}
				tbuf[oldn+2]--;
				if (cz >= 0)
				{
					tbuf[oldn] = ((n-oldn)>>2);
					tbuf[n] = 0;
					tbuf[n+1] = tbuf[n+3] = cz;
					tbuf[n+2] = cz-1;
					n += 4;
				}

					//De-allocate column (x,y) if it exists
				if (sw[0]) voxdealloc(sw[0]);

					//Allocate & copy to new column (x,y)
				sw[0] = voxalloc(n);
				copybuf((void *)tbuf,(void *)sw[0],n>>2);
				sw++; sr += 2;
			}
			sr += ysiz*2;
		}
		ssr = ssw; ssw += xsiz*ysiz;
		mipnum++; if (mipnum > gmipnum) gmipnum = mipnum;
	}

		//Remove extra mips (bbox must be 0,0,VSID,VSID to get inside this)
	while ((xsiz > 1) && (ysiz > 1) && (zsiz > 1) && (mipnum < gmipnum))
	{
		xsiz >>= 1; ysiz >>= 1; zsiz >>= 1;
		for(i=xsiz*ysiz;i>0;i--)
		{
			if (ssw[0]) voxdealloc(ssw[0]); //De-allocate column if it exists
			ssw++;
		}
		gmipnum--;
	}
}

void setsphere (lpoint3d *hit, long hitrad, long dacol)
{
	void (*modslab)(long *, long, long);
	long i, x, y, xs, ys, zs, xe, ye, ze, sq;
	float f, ff;

	xs = MAX(hit->x-hitrad,0); xe = MIN(hit->x+hitrad,VSID-1);
	ys = MAX(hit->y-hitrad,0); ye = MIN(hit->y+hitrad,VSID-1);
	zs = MAX(hit->z-hitrad,0); ze = MIN(hit->z+hitrad,MAXZDIM-1);
	vx5.minx = xs; vx5.maxx = xe+1;
	vx5.miny = ys; vx5.maxy = ye+1;
	vx5.minz = zs; vx5.maxz = ze+1;
	if ((xs > xe) || (ys > ye) || (zs > ze)) return;

	if (vx5.colfunc == sphcolfunc)
	{
		vx5.cen = hit->x+hit->y+hit->z;
		vx5.daf = 1.f/(hitrad*sqrt(3.f));
	}

	if (hitrad >= SETSPHMAXRAD-1) hitrad = SETSPHMAXRAD-2;
	if (dacol == -1) modslab = delslab; else modslab = insslab;

	tempfloatbuf[0] = 0.0f;
#if 0
		//Totally unoptimized
	for(i=1;i<=hitrad;i++) tempfloatbuf[i] = pow((float)i,vx5.curpow);
#else
	tempfloatbuf[1] = 1.0f;
	for(i=2;i<=hitrad;i++)
	{
		if (!factr[i][0]) tempfloatbuf[i] = exp(logint[i]*vx5.curpow);
		else tempfloatbuf[i] = tempfloatbuf[factr[i][0]]*tempfloatbuf[factr[i][1]];
	}
#endif
	*(long *)&tempfloatbuf[hitrad+1] = 0x7f7fffff; //3.4028235e38f; //Highest float

	sq = 0; //pow(fabs(x-hit->x),vx5.curpow) + "y + "z < pow(vx5.currad,vx5.curpow)
	for(y=ys;y<=ye;y++)
	{
		ff = tempfloatbuf[hitrad]-tempfloatbuf[labs(y-hit->y)];
		if (*(long *)&ff <= 0) continue;
		for(x=xs;x<=xe;x++)
		{
			f = ff-tempfloatbuf[labs(x-hit->x)]; if (*(long *)&f <= 0) continue;
			while (*(long *)&tempfloatbuf[sq] <  *(long *)&f) sq++;
			while (*(long *)&tempfloatbuf[sq] >= *(long *)&f) sq--;
			modslab(scum2(x,y),MAX(hit->z-sq,zs),MIN(hit->z+sq+1,ze));
		}
	}
	scum2finish();
	updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,dacol);
}

void setellipsoid (lpoint3d *hit, lpoint3d *hit2, long hitrad, long dacol, long bakit)
{
	void (*modslab)(long *, long, long);
	long x, y, xs, ys, zs, xe, ye, ze;
	float a, b, c, d, e, f, g, h, r, t, u, Za, Zb, fx0, fy0, fz0, fx1, fy1, fz1;

	xs = MIN(hit->x,hit2->x)-hitrad; xs = MAX(xs,0);
	ys = MIN(hit->y,hit2->y)-hitrad; ys = MAX(ys,0);
	zs = MIN(hit->z,hit2->z)-hitrad; zs = MAX(zs,0);
	xe = MAX(hit->x,hit2->x)+hitrad; xe = MIN(xe,VSID-1);
	ye = MAX(hit->y,hit2->y)+hitrad; ye = MIN(ye,VSID-1);
	ze = MAX(hit->z,hit2->z)+hitrad; ze = MIN(ze,MAXZDIM-1);
	vx5.minx = xs; vx5.maxx = xe+1;
	vx5.miny = ys; vx5.maxy = ye+1;
	vx5.minz = zs; vx5.maxz = ze+1;
	if ((xs > xe) || (ys > ye) || (zs > ze))
		{ if (bakit) voxbackup(xs,ys,xs,ys,bakit); return; }

	fx0 = (float)hit->x; fy0 = (float)hit->y; fz0 = (float)hit->z;
	fx1 = (float)hit2->x; fy1 = (float)hit2->y; fz1 = (float)hit2->z;

	r = (fx1-fx0)*(fx1-fx0) + (fy1-fy0)*(fy1-fy0) + (fz1-fz0)*(fz1-fz0);
	r = sqrt((float)hitrad*(float)hitrad + r*.25);
	c = fz0*fz0 - fz1*fz1; d = r*r*-4; e = d*4;
	f = c*c + fz1*fz1 * e; g = c + c; h = (fz1-fz0)*2; c = c*h - fz1*e;
	Za = -h*h - e; if (Za <= 0) { if (bakit) voxbackup(xs,ys,xs,ys,bakit); return; }
	u = 1 / Za;

	if (vx5.colfunc == jitcolfunc) vx5.amount = 0x70707;

	if (dacol == -1) modslab = delslab; else modslab = insslab;

	if (bakit) voxbackup(xs,ys,xe+1,ye+1,bakit);

	for(y=ys;y<=ye;y++)
		for(x=xs;x<=xe;x++)
		{
			a = (x-fx0)*(x-fx0) + (y-fy0)*(y-fy0);
			b = (x-fx1)*(x-fx1) + (y-fy1)*(y-fy1);
			t = a-b+d; Zb = t*h + c;
			t = ((t+g)*t + b*e + f)*Za + Zb*Zb; if (t <= 0) continue;
			t = sqrt(t);
			ftol((Zb - t)*u,&zs); if (zs < 0) zs = 0;
			ftol((Zb + t)*u,&ze); if (ze > MAXZDIM) ze = MAXZDIM;
			modslab(scum2(x,y),zs,ze);
		}
	scum2finish();
	updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,dacol);
}

	//Draws a cylinder, given: 2 points, a radius, and a color
	//Code mostly optimized - original code from CYLINDER.BAS:drawcylinder
void setcylinder (lpoint3d *p0, lpoint3d *p1, long cr, long dacol, long bakit)
{
	void (*modslab)(long *, long, long);

	float t, ax, ay, az, bx, by, bz, cx, cy, cz, ux, uy, uz, vx, vy, vz;
	float Za, Zb, Zc, tcr, xxyy, rcz, rZa;
	float fx, fxi, xof, vx0, vy0, vz0, vz0i, vxo, vyo, vzo;
	long i, j, ix, iy, ix0, ix1, iz0, iz1, minx, maxx, miny, maxy;
	long x0, y0, z0, x1, y1, z1;

		//Map generic cylinder into unit space:  (0,0,0), (0,0,1), cr = 1
		//   x*x + y*y < 1, z >= 0, z < 1
	if (p0->z > p1->z)
	{
		x0 = p1->x; y0 = p1->y; z0 = p1->z;
		x1 = p0->x; y1 = p0->y; z1 = p0->z;
	}
	else
	{
		x0 = p0->x; y0 = p0->y; z0 = p0->z;
		x1 = p1->x; y1 = p1->y; z1 = p1->z;
	}

	xxyy = (float)((x1-x0)*(x1-x0)+(y1-y0)*(y1-y0));
	t = xxyy + (float)(z1-z0)*(z1-z0);
	if ((t == 0) || (cr == 0))
	{
		vx5.minx = x0; vx5.maxx = x0+1;
		vx5.miny = y0; vx5.maxy = y0+1;
		vx5.minz = z0; vx5.maxz = z0+1;
		if (bakit) voxbackup(x0,y0,x0,y0,bakit);
		return;
	}
	t = 1 / t; cx = ((float)(x1-x0))*t; cy = ((float)(y1-y0))*t; cz = ((float)(z1-z0))*t;
	t = sqrt(t); ux = ((float)(x1-x0))*t; uy = ((float)(y1-y0))*t; uz = ((float)(z1-z0))*t;

	if (vx5.colfunc == jitcolfunc) vx5.amount = 0x70707;

	if (dacol == -1) modslab = delslab; else modslab = insslab;

	if (xxyy == 0)
	{
		iz0 = MAX(z0,0); iz1 = MIN(z1,MAXZDIM);
		minx = MAX(x0-cr,0); maxx = MIN(x0+cr,VSID-1);
		miny = MAX(y0-cr,0); maxy = MIN(y0+cr,VSID-1);

		vx5.minx = minx; vx5.maxx = maxx+1;
		vx5.miny = miny; vx5.maxy = maxy+1;
		vx5.minz = iz0; vx5.maxz = iz1;
		if (bakit) voxbackup(minx,miny,maxx+1,maxy+1,bakit);

		j = cr*cr;
		for(iy=miny;iy<=maxy;iy++)
		{
			i = j-(iy-y0)*(iy-y0);
			for(ix=minx;ix<=maxx;ix++)
				if ((ix-x0)*(ix-x0) < i) modslab(scum2(ix,iy),iz0,iz1);
		}
		scum2finish();
		updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,dacol);
		return;
	}

	if (x0 < x1) { minx = x0; maxx = x1; } else { minx = x1; maxx = x0; }
	if (y0 < y1) { miny = y0; maxy = y1; } else { miny = y1; maxy = y0; }
	tcr = cr / sqrt(xxyy); vx = fabs((float)(x1-x0))*tcr; vy = fabs((float)(y1-y0))*tcr;
	t = vx*uz + vy;
	ftol((float)minx-t,&minx); if (minx < 0) minx = 0;
	ftol((float)maxx+t,&maxx); if (maxx >= VSID) maxx = VSID-1;
	t = vy*uz + vx;
	ftol((float)miny-t,&miny); if (miny < 0) miny = 0;
	ftol((float)maxy+t,&maxy); if (maxy >= VSID) maxy = VSID-1;

	vx5.minx = minx; vx5.maxx = maxx+1;
	vx5.miny = miny; vx5.maxy = maxy+1;
	vx5.minz = z0-cr; vx5.maxz = z1+cr+1;
	if (bakit) voxbackup(minx,miny,maxx+1,maxy+1,bakit);

	vx = (fabs(ux) < fabs(uy)); vy = 1.0f-vx; vz = 0;
	ax = uy*vz - uz*vy; ay = uz*vx - ux*vz; az = ux*vy - uy*vx;
	t = 1.0 / (sqrt(ax*ax + ay*ay + az*az)*cr);
	ax *= t; ay *= t; az *= t;
	bx = ay*uz - az*uy; by = az*ux - ax*uz; bz = ax*uy - ay*ux;

	Za = az*az + bz*bz; rZa = 1.0f / Za;
	if (cz != 0) { rcz = 1.0f / cz; vz0i = -rcz*cx; }
	if (y0 != y1)
	{
		t = 1.0f / ((float)(y1-y0)); fxi = ((float)(x1-x0))*t;
		fx = ((float)miny-y0)*fxi + x0; xof = fabs(tcr*xxyy*t);
	}
	else { fx = (float)minx; fxi = 0.0; xof = (float)(maxx-minx); }

	vy = (float)(miny-y0);
	vxo = vy*ay - z0*az;
	vyo = vy*by - z0*bz;
	vzo = vy*cy - z0*cz;
	for(iy=miny;iy<=maxy;iy++)
	{
		ftol(fx-xof,&ix0); if (ix0 < minx) ix0 = minx;
		ftol(fx+xof,&ix1); if (ix1 > maxx) ix1 = maxx;
		fx += fxi;

		vx = (float)(ix0-x0);
		vx0 = vx*ax + vxo; vxo += ay;
		vy0 = vx*bx + vyo; vyo += by;
		vz0 = vx*cx + vzo; vzo += cy;

		if (cz != 0)   //(vx0 + vx1*t)ý + (vy0 + vy1*t)ý = 1
		{
			vz0 *= -rcz;
			for(ix=ix0;ix<=ix1;ix++,vx0+=ax,vy0+=bx,vz0+=vz0i)
			{
				Zb = vx0*az + vy0*bz; Zc = vx0*vx0 + vy0*vy0 - 1;
				t = Zb*Zb - Za*Zc; if (*(long *)&t <= 0) continue; t = sqrt(t);
				ftol(MAX((-Zb-t)*rZa,vz0    ),&iz0); if (iz0 < 0) iz0 = 0;
				ftol(MIN((-Zb+t)*rZa,vz0+rcz),&iz1); if (iz1 > MAXZDIM) iz1 = MAXZDIM;
				modslab(scum2(ix,iy),iz0,iz1);
			}
		}
		else
		{
			for(ix=ix0;ix<=ix1;ix++,vx0+=ax,vy0+=bx,vz0+=cx)
			{
				if (*(unsigned long *)&vz0 >= 0x3f800000) continue; //vz0<0||vz0>=1
				Zb = vx0*az + vy0*bz; Zc = vx0*vx0 + vy0*vy0 - 1;
				t = Zb*Zb - Za*Zc; if (*(long *)&t <= 0) continue; t = sqrt(t);
				ftol((-Zb-t)*rZa,&iz0); if (iz0 < 0) iz0 = 0;
				ftol((-Zb+t)*rZa,&iz1); if (iz1 > MAXZDIM) iz1 = MAXZDIM;
				modslab(scum2(ix,iy),iz0,iz1);
			}
		}
	}
	scum2finish();
	updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,dacol);
}

	//Draws a rectangle, given: 2 points as opposite corners, and a color
void setrect (lpoint3d *hit, lpoint3d *hit2, long dacol)
{
	long x, y, xs, ys, zs, xe, ye, ze;

		//WARNING: do NOT use lbound because 'c' not guaranteed to be >= 'b'
	xs = MAX(MIN(hit->x,hit2->x),0); xe = MIN(MAX(hit->x,hit2->x),VSID-1);
	ys = MAX(MIN(hit->y,hit2->y),0); ye = MIN(MAX(hit->y,hit2->y),VSID-1);
	zs = MAX(MIN(hit->z,hit2->z),0); ze = MIN(MAX(hit->z,hit2->z),MAXZDIM-1);
	vx5.minx = xs; vx5.maxx = xe+1;
	vx5.miny = ys; vx5.maxy = ye+1;
	vx5.minz = zs; vx5.maxz = ze+1;
	if ((xs > xe) || (ys > ye) || (zs > ze)) return;

	if (vx5.colfunc == jitcolfunc) vx5.amount = 0x70707;

	ze++;
	if (dacol == -1)
	{
		for(y=ys;y<=ye;y++)
			for(x=xs;x<=xe;x++)
				delslab(scum2(x,y),zs,ze);
	}
	else
	{
		for(y=ys;y<=ye;y++)
			for(x=xs;x<=xe;x++)
				insslab(scum2(x,y),zs,ze);
	}
	scum2finish();
	updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,dacol);
}

	//Does CSG using pre-sorted spanlist
void setspans (vspans *lst, long lstnum, lpoint3d *offs, long dacol)
{
	void (*modslab)(long *, long, long);
	long i, j, x, y, z0, z1, *lptr;
	char ox, oy;

	if (lstnum <= 0) return;
	if (dacol == -1) modslab = delslab; else modslab = insslab;
	vx5.minx = vx5.maxx = ((long)lst[0].x)+offs->x;
	vx5.miny = ((long)lst[       0].y)+offs->y;
	vx5.maxy = ((long)lst[lstnum-1].y)+offs->y+1;
	vx5.minz = vx5.maxz = ((long)lst[0].z0)+offs->z;

	i = 0; goto in2setlist;
	do
	{
		if ((ox != lst[i].x) || (oy != lst[i].y))
		{
in2setlist:;
			ox = lst[i].x; oy = lst[i].y;
			x = ((long)lst[i].x)+offs->x;
			y = ((long)lst[i].y)+offs->y;
				  if (x < vx5.minx) vx5.minx = x;
			else if (x > vx5.maxx) vx5.maxx = x;
			lptr = scum2(x,y);
		}
		if ((x|y)&(~(VSID-1))) { i++; continue; }
		z0 = ((long)lst[i].z0)+offs->z;   if (z0 < 0) z0 = 0;
		z1 = ((long)lst[i].z1)+offs->z+1; if (z1 > MAXZDIM) z1 = MAXZDIM;
		if (z0 < vx5.minz) vx5.minz = z0;
		if (z1 > vx5.maxz) vx5.maxz = z1;
		modslab(lptr,z0,z1);
		i++;
	} while (i < lstnum);
	vx5.maxx++; vx5.maxz++;
	if (vx5.minx < 0) vx5.minx = 0;
	if (vx5.miny < 0) vx5.miny = 0;
	if (vx5.maxx > VSID) vx5.maxx = VSID;
	if (vx5.maxy > VSID) vx5.maxy = VSID;

	scum2finish();
	updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,dacol);
}

void setheightmap (const unsigned char *hptr, long hpitch, long hxdim, long hydim,
						 long x0, long y0, long x1, long y1)
{
	long x, y, su, sv, u, v;

	if (x0 < 0) x0 = 0;
	if (y0 < 0) y0 = 0;
	if (x1 > VSID) x1 = VSID;
	if (y1 > VSID) y1 = VSID;
	vx5.minx = x0; vx5.maxx = x1;
	vx5.miny = y0; vx5.maxy = y1;
	vx5.minz = 0; vx5.maxz = MAXZDIM;
	if ((x0 >= x1) || (y0 >= y1)) return;

	su = x0%hxdim; sv = y0%hydim;
	for(y=y0,v=sv;y<y1;y++)
	{
		for(x=x0,u=su;x<x1;x++)
		{
			insslab(scum2(x,y),hptr[v*hpitch+u],MAXZDIM);
			u++; if (u >= hxdim) u = 0;
		}
		v++; if (v >= hydim) v = 0;
	}
	scum2finish();
	updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,0);
}

static long min0[VSID], max0[VSID]; //MAXY
static long min1[VSID], max1[VSID]; //MAXX
static long min2[VSID], max2[VSID]; //MAXY

static void canseerange (point3d *p0, point3d *p1)
{
	lpoint3d a, c, d, p, i;
	point3d f, g;
	long cnt, j;

	ftol(p0->x-.5,&a.x); ftol(p0->y-.5,&a.y); ftol(p0->z-.5,&a.z);
	ftol(p1->x-.5,&c.x); ftol(p1->y-.5,&c.y); ftol(p1->z-.5,&c.z);
	cnt = 0;

		  if (c.x <  a.x) { d.x = -1; f.x = p0->x-a.x;   g.x = (p0->x-p1->x)*1024; cnt += a.x-c.x; }
	else if (c.x != a.x) { d.x =  1; f.x = a.x+1-p0->x; g.x = (p1->x-p0->x)*1024; cnt += c.x-a.x; }
	else f.x = g.x = 0;
		  if (c.y <  a.y) { d.y = -1; f.y = p0->y-a.y;   g.y = (p0->y-p1->y)*1024; cnt += a.y-c.y; }
	else if (c.y != a.y) { d.y =  1; f.y = a.y+1-p0->y; g.y = (p1->y-p0->y)*1024; cnt += c.y-a.y; }
	else f.y = g.y = 0;
		  if (c.z <  a.z) { d.z = -1; f.z = p0->z-a.z;   g.z = (p0->z-p1->z)*1024; cnt += a.z-c.z; }
	else if (c.z != a.z) { d.z =  1; f.z = a.z+1-p0->z; g.z = (p1->z-p0->z)*1024; cnt += c.z-a.z; }
	else f.z = g.z = 0;

	ftol(f.x*g.z - f.z*g.x,&p.x); ftol(g.x,&i.x);
	ftol(f.y*g.z - f.z*g.y,&p.y); ftol(g.y,&i.y);
	ftol(f.y*g.x - f.x*g.y,&p.z); ftol(g.z,&i.z);
	for(;cnt;cnt--)
	{
			//use a.x, a.y, a.z
		if (a.x < min0[a.y]) min0[a.y] = a.x;
		if (a.x > max0[a.y]) max0[a.y] = a.x;
		if (a.z < min1[a.x]) min1[a.x] = a.z;
		if (a.z > max1[a.x]) max1[a.x] = a.z;
		if (a.z < min2[a.y]) min2[a.y] = a.z;
		if (a.z > max2[a.y]) max2[a.y] = a.z;

		if (((p.x|p.y) >= 0) && (a.z != c.z)) { a.z += d.z; p.x -= i.x; p.y -= i.y; }
		else if ((p.z >= 0) && (a.x != c.x))  { a.x += d.x; p.x += i.z; p.z -= i.y; }
		else                                  { a.y += d.y; p.y += i.z; p.z += i.x; }
	}
}

void settri (point3d *p0, point3d *p1, point3d *p2, long bakit)
{
	point3d n;
	float f, x0, y0, z0, x1, y1, z1, rx, ry, k0, k1;
	long i, x, y, z, iz0, iz1, minx, maxx, miny, maxy;

	if (p0->x < p1->x) { x0 = p0->x; x1 = p1->x; } else { x0 = p1->x; x1 = p0->x; }
	if (p2->x < x0) x0 = p2->x;
	if (p2->x > x1) x1 = p2->x;
	if (p0->y < p1->y) { y0 = p0->y; y1 = p1->y; } else { y0 = p1->y; y1 = p0->y; }
	if (p2->y < y0) y0 = p2->y;
	if (p2->y > y1) y1 = p2->y;
	if (p0->z < p1->z) { z0 = p0->z; z1 = p1->z; } else { z0 = p1->z; z1 = p0->z; }
	if (p2->z < z0) z0 = p2->z;
	if (p2->z > z1) z1 = p2->z;

	ftol(x0-.5,&minx); ftol(y0-.5,&miny);
	ftol(x1-.5,&maxx); ftol(y1-.5,&maxy);
	vx5.minx = minx; vx5.maxx = maxx+1;
	vx5.miny = miny; vx5.maxy = maxy+1;
	ftol(z0-.5,&vx5.minz); ftol(z1+.5,&vx5.maxz);
	if (bakit) voxbackup(minx,miny,maxx+1,maxy+1,bakit);

	for(i=miny;i<=maxy;i++) { min0[i] = 0x7fffffff; max0[i] = 0x80000000; }
	for(i=minx;i<=maxx;i++) { min1[i] = 0x7fffffff; max1[i] = 0x80000000; }
	for(i=miny;i<=maxy;i++) { min2[i] = 0x7fffffff; max2[i] = 0x80000000; }

	canseerange(p0,p1);
	canseerange(p1,p2);
	canseerange(p2,p0);

	n.x = (p1->z-p0->z)*(p2->y-p1->y) - (p1->y-p0->y) * (p2->z-p1->z);
	n.y = (p1->x-p0->x)*(p2->z-p1->z) - (p1->z-p0->z) * (p2->x-p1->x);
	n.z = (p1->y-p0->y)*(p2->x-p1->x) - (p1->x-p0->x) * (p2->y-p1->y);
	f = 1.0 / sqrt(n.x*n.x + n.y*n.y + n.z*n.z); if (n.z < 0) f = -f;
	n.x *= f; n.y *= f; n.z *= f;

	if (n.z > .01)
	{
		f = -1.0 / n.z; rx = n.x*f; ry = n.y*f;
		k0 = ((n.x>=0)-p0->x)*rx + ((n.y>=0)-p0->y)*ry - ((n.z>=0)-p0->z) + .5;
		k1 = ((n.x< 0)-p0->x)*rx + ((n.y< 0)-p0->y)*ry - ((n.z< 0)-p0->z) - .5;
	}
	else { rx = 0; ry = 0; k0 = -2147000000.0; k1 = 2147000000.0; }

	for(y=miny;y<=maxy;y++)
		for(x=min0[y];x<=max0[y];x++)
		{
			f = (float)x*rx + (float)y*ry; ftol(f+k0,&iz0); ftol(f+k1,&iz1);
			if (iz0 < min1[x]) iz0 = min1[x];
			if (iz1 > max1[x]) iz1 = max1[x];
			if (iz0 < min2[y]) iz0 = min2[y];
			if (iz1 > max2[y]) iz1 = max2[y];

				//set: (x,y,iz0) to (x,y,iz1) (inclusive)
			insslab(scum2(x,y),iz0,iz1+1);
	}
	scum2finish();
	updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,0);
}

	//Known problems:
	//1. Need to test faces for intersections on p1<->p2 line (not just edges)
	//2. Doesn't guarantee that hit point/line is purely air (but very close)
	//3. Piescan is more useful for parts of rope code :/
static long tripind[24] = {0,4,1,5,2,6,3,7,0,2,1,3,4,6,5,7,0,1,2,3,4,5,6,7};
long triscan (point3d *p0, point3d *p1, point3d *p2, point3d *hit, lpoint3d *lhit)
{
	point3d n, d[8], cp2;
	float f, g, x0, x1, y0, y1, rx, ry, k0, k1, fx, fy, fz, pval[8];
	long i, j, k, x, y, z, iz0, iz1, minx, maxx, miny, maxy, didhit;

	didhit = 0;

	if (p0->x < p1->x) { x0 = p0->x; x1 = p1->x; } else { x0 = p1->x; x1 = p0->x; }
	if (p2->x < x0) x0 = p2->x;
	if (p2->x > x1) x1 = p2->x;
	if (p0->y < p1->y) { y0 = p0->y; y1 = p1->y; } else { y0 = p1->y; y1 = p0->y; }
	if (p2->y < y0) y0 = p2->y;
	if (p2->y > y1) y1 = p2->y;
	ftol(x0-.5,&minx); ftol(y0-.5,&miny);
	ftol(x1-.5,&maxx); ftol(y1-.5,&maxy);
	for(i=miny;i<=maxy;i++) { min0[i] = 0x7fffffff; max0[i] = 0x80000000; }
	for(i=minx;i<=maxx;i++) { min1[i] = 0x7fffffff; max1[i] = 0x80000000; }
	for(i=miny;i<=maxy;i++) { min2[i] = 0x7fffffff; max2[i] = 0x80000000; }

	canseerange(p0,p1);
	canseerange(p1,p2);
	canseerange(p2,p0);

	n.x = (p1->z-p0->z)*(p2->y-p1->y) - (p1->y-p0->y) * (p2->z-p1->z);
	n.y = (p1->x-p0->x)*(p2->z-p1->z) - (p1->z-p0->z) * (p2->x-p1->x);
	n.z = (p1->y-p0->y)*(p2->x-p1->x) - (p1->x-p0->x) * (p2->y-p1->y);
	f = 1.0 / sqrt(n.x*n.x + n.y*n.y + n.z*n.z); if (n.z < 0) f = -f;
	n.x *= f; n.y *= f; n.z *= f;

	if (n.z > .01)
	{
		f = -1.0 / n.z; rx = n.x*f; ry = n.y*f;
		k0 = ((n.x>=0)-p0->x)*rx + ((n.y>=0)-p0->y)*ry - ((n.z>=0)-p0->z) + .5;
		k1 = ((n.x< 0)-p0->x)*rx + ((n.y< 0)-p0->y)*ry - ((n.z< 0)-p0->z) - .5;
	}
	else { rx = 0; ry = 0; k0 = -2147000000.0; k1 = 2147000000.0; }

	cp2.x = p2->x; cp2.y = p2->y; cp2.z = p2->z;

	for(y=miny;y<=maxy;y++)
		for(x=min0[y];x<=max0[y];x++)
		{
			f = (float)x*rx + (float)y*ry; ftol(f+k0,&iz0); ftol(f+k1,&iz1);
			if (iz0 < min1[x]) iz0 = min1[x];
			if (iz1 > max1[x]) iz1 = max1[x];
			if (iz0 < min2[y]) iz0 = min2[y];
			if (iz1 > max2[y]) iz1 = max2[y];
			for(z=iz0;z<=iz1;z++)
			{
				if (!isvoxelsolid(x,y,z)) continue;

				for(i=0;i<8;i++)
				{
					d[i].x = (float)(( i    &1)+x);
					d[i].y = (float)(((i>>1)&1)+y);
					d[i].z = (float)(((i>>2)&1)+z);
					pval[i] = (d[i].x-p0->x)*n.x + (d[i].y-p0->y)*n.y + (d[i].z-p0->z)*n.z;
				}
				for(i=0;i<24;i+=2)
				{
					j = tripind[i+0];
					k = tripind[i+1];
					if (((*(long *)&pval[j])^(*(long *)&pval[k])) < 0)
					{
						f = pval[j]/(pval[j]-pval[k]);
						fx = (d[k].x-d[j].x)*f + d[j].x;
						fy = (d[k].y-d[j].y)*f + d[j].y;
						fz = (d[k].z-d[j].z)*f + d[j].z;

							//         (p0->x,p0->y,p0->z)
							//             _|     |_
							//           _|     .   |_
							//         _|  (fx,fy,fz) |_
							//       _|                 |_
							//(p1->x,p1->y,p1->z)-.----(cp2.x,cp2.y,cp2.z)

						if ((fabs(n.z) > fabs(n.x)) && (fabs(n.z) > fabs(n.y)))
						{ //x,y
						  // ix = p1->x + (cp2.x-p1->x)*t;
						  // iy = p1->y + (cp2.y-p1->y)*t;
						  //(iz = p1->z + (cp2.z-p1->z)*t;)
						  // ix = p0->x + (fx-p0->x)*u;
						  // iy = p0->y + (fy-p0->y)*u;
						  // (p1->x-cp2.x)*t + (fx-p0->x)*u = p1->x-p0->x;
						  // (p1->y-cp2.y)*t + (fy-p0->y)*u = p1->y-p0->y;

							f = (p1->x-cp2.x)*(fy-p0->y) - (p1->y-cp2.y)*(fx-p0->x);
							if ((*(long *)&f) == 0) continue;
							f = 1.0 / f;
							g = ((p1->x-cp2.x)*(p1->y-p0->y) - (p1->y-cp2.y)*(p1->x-p0->x))*f;
							//NOTE: The following trick assumes g not * or / by f!
							//if (((*(long *)&g)-(*(long *)&f))^(*(long *)&f)) >= 0) continue;
							if ((*(long *)&g) < 0x3f800000) continue;
							g = ((p1->x-p0->x)*(fy-p0->y) - (p1->y-p0->y)*(fx-p0->x))*f;
						}
						else if (fabs(n.y) > fabs(n.x))
						{ //x,z
							f = (p1->x-cp2.x)*(fz-p0->z) - (p1->z-cp2.z)*(fx-p0->x);
							if ((*(long *)&f) == 0) continue;
							f = 1.0 / f;
							g = ((p1->x-cp2.x)*(p1->z-p0->z) - (p1->z-cp2.z)*(p1->x-p0->x))*f;
							if ((*(long *)&g) < 0x3f800000) continue;
							g = ((p1->x-p0->x)*(fz-p0->z) - (p1->z-p0->z)*(fx-p0->x))*f;
						}
						else
						{ //y,z
							f = (p1->y-cp2.y)*(fz-p0->z) - (p1->z-cp2.z)*(fy-p0->y);
							if ((*(long *)&f) == 0) continue;
							f = 1.0 / f;
							g = ((p1->y-cp2.y)*(p1->z-p0->z) - (p1->z-cp2.z)*(p1->y-p0->y))*f;
							if ((*(long *)&g) < 0x3f800000) continue;
							g = ((p1->y-p0->y)*(fz-p0->z) - (p1->z-p0->z)*(fy-p0->y))*f;
						}
						if ((*(unsigned long *)&g) >= 0x3f800000) continue;
						(hit->x) = fx; (hit->y) = fy; (hit->z) = fz;
						(lhit->x) = x; (lhit->y) = y; (lhit->z) = z; didhit = 1;
						(cp2.x) = (cp2.x-p1->x)*g + p1->x;
						(cp2.y) = (cp2.y-p1->y)*g + p1->y;
						(cp2.z) = (cp2.z-p1->z)*g + p1->z;
					}
				}
			}
		}
	return(didhit);
}

// ------------------------ CONVEX 3D HULL CODE BEGINS ------------------------

#define MAXPOINTS (256 *2) //Leave the *2 here for safety!
point3d nm[MAXPOINTS*2+2];
float nmc[MAXPOINTS*2+2];
long tri[MAXPOINTS*8+8], lnk[MAXPOINTS*8+8], tricnt;
char umost[VSID*VSID], dmost[VSID*VSID];

void initetrasid (point3d *pt, long z)
{
	long i, j, k;
	float x0, y0, z0, x1, y1, z1;

	i = tri[z*4]; j = tri[z*4+1]; k = tri[z*4+2];
	x0 = pt[i].x-pt[k].x; y0 = pt[i].y-pt[k].y; z0 = pt[i].z-pt[k].z;
	x1 = pt[j].x-pt[k].x; y1 = pt[j].y-pt[k].y; z1 = pt[j].z-pt[k].z;
	nm[z].x = y0*z1 - z0*y1;
	nm[z].y = z0*x1 - x0*z1;
	nm[z].z = x0*y1 - y0*x1;
	nmc[z] = nm[z].x*pt[k].x + nm[z].y*pt[k].y + nm[z].z*pt[k].z;
}

void inithull3d (point3d *pt, long nump)
{
	float px, py, pz;
	long i, k, s, z, szz, zz, zx, snzz, nzz, zzz, otricnt;

	tri[0] = 0; tri[4] = 0; tri[8] = 0; tri[12] = 1;
	tri[1] = 1; tri[2] = 2; initetrasid(pt,0);
	if (nm[0].x*pt[3].x + nm[0].y*pt[3].y + nm[0].z*pt[3].z >= nmc[0])
	{
		tri[1] = 1; tri[2] = 2; lnk[0] = 10; lnk[1] = 14; lnk[2] = 4;
		tri[5] = 2; tri[6] = 3; lnk[4] = 2; lnk[5] = 13; lnk[6] = 8;
		tri[9] = 3; tri[10] = 1; lnk[8] = 6; lnk[9] = 12; lnk[10] = 0;
		tri[13] = 3; tri[14] = 2; lnk[12] = 9; lnk[13] = 5; lnk[14] = 1;
	}
	else
	{
		tri[1] = 2; tri[2] = 1; lnk[0] = 6; lnk[1] = 12; lnk[2] = 8;
		tri[5] = 3; tri[6] = 2; lnk[4] = 10; lnk[5] = 13; lnk[6] = 0;
		tri[9] = 1; tri[10] = 3; lnk[8] = 2; lnk[9] = 14; lnk[10] = 4;
		tri[13] = 2; tri[14] = 3; lnk[12] = 1; lnk[13] = 5; lnk[14] = 9;
	}
	tricnt = 4*4;

	for(z=0;z<4;z++) initetrasid(pt,z);

	for(z=4;z<nump;z++)
	{
		px = pt[z].x; py = pt[z].y; pz = pt[z].z;
		for(zz=tricnt-4;zz>=0;zz-=4)
		{
			i = (zz>>2);
			if (nm[i].x*px + nm[i].y*py + nm[i].z*pz >= nmc[i]) continue;

			s = 0;
			for(zx=2;zx>=0;zx--)
			{
				i = (lnk[zz+zx]>>2);
				s += (nm[i].x*px + nm[i].y*py + nm[i].z*pz < nmc[i]) + s;
			}
			if (s == 7) continue;

			nzz = ((0x4a4>>(s+s))&3); szz = zz; otricnt = tricnt;
			do
			{
				snzz = nzz;
				do
				{
					zzz = nzz+1; if (zzz >= 3) zzz = 0;

						//Insert triangle tricnt: (p0,p1,z)
					tri[tricnt+0] = tri[zz+nzz];
					tri[tricnt+1] = tri[zz+zzz];
					tri[tricnt+2] = z;
					initetrasid(pt,tricnt>>2);
					k = lnk[zz+nzz]; lnk[tricnt] = k; lnk[k] = tricnt;
					lnk[tricnt+1] = tricnt+6;
					lnk[tricnt+2] = tricnt-3;
					tricnt += 4;

						//watch out for loop inside single triangle
					if (zzz == snzz) goto endit;
					nzz = zzz;
				} while (!(s&(1<<zzz)));
				do
				{
					i = zz+nzz;
					zz = (lnk[i]&~3);
					nzz = (lnk[i]&3)+1; if (nzz == 3) nzz = 0;
					s = 0;
					for(zx=2;zx>=0;zx--)
					{
						i = (lnk[zz+zx]>>2);
						s += (nm[i].x*px + nm[i].y*py + nm[i].z*pz < nmc[i]) + s;
					}
				} while (s&(1<<nzz));
			} while (zz != szz);
endit:;  lnk[tricnt-3] = otricnt+2; lnk[otricnt+2] = tricnt-3;

			for(zz=otricnt-4;zz>=0;zz-=4)
			{
				i = (zz>>2);
				if (nm[i].x*px + nm[i].y*py + nm[i].z*pz < nmc[i])
				{
					tricnt -= 4; //Delete triangle zz%
					nm[i] = nm[tricnt>>2]; nmc[i] = nmc[tricnt>>2];
					for(i=0;i<3;i++)
					{
						tri[zz+i] = tri[tricnt+i];
						lnk[zz+i] = lnk[tricnt+i];
						lnk[lnk[zz+i]] = zz+i;
					}
				}
			}
			break;
		}
	}
	tricnt >>= 2;
}

static long incmod3[3];
void tmaphulltrisortho (point3d *pt)
{
	point3d *i0, *i1;
	float r, knmx, knmy, knmc, xinc;
	long i, k, op, p, pe, y, yi, z, zi, sy, sy1, itop, ibot, damost;

	for(k=0;k<tricnt;k++)
	{
		if (nm[k].z >= 0)
			{ damost = (long)umost; incmod3[0] = 1; incmod3[1] = 2; incmod3[2] = 0; }
		else
			{ damost = (long)dmost; incmod3[0] = 2; incmod3[1] = 0; incmod3[2] = 1; }

		itop = (pt[tri[(k<<2)+1]].y < pt[tri[k<<2]].y); ibot = 1-itop;
			  if (pt[tri[(k<<2)+2]].y < pt[tri[(k<<2)+itop]].y) itop = 2;
		else if (pt[tri[(k<<2)+2]].y > pt[tri[(k<<2)+ibot]].y) ibot = 2;

			//Pre-calculations
		if (fabs(nm[k].z) < .000001) r = 0; else r = -65536.0 / nm[k].z;
		knmx = nm[k].x*r; knmy = nm[k].y*r;
		//knmc = 65536.0-nmc[k]*r-knmx-knmy;
		//knmc = -nmc[k]*r-(knmx+knmy)*.5f;
		knmc = /*65536.0*/  -nmc[k]*r+knmx;
		ftol(knmx,&zi);

		i = ibot;
		do
		{
			i1 = &pt[tri[(k<<2)+i]]; ftol(i1->y,&sy1); i = incmod3[i];
			i0 = &pt[tri[(k<<2)+i]]; ftol(i0->y,&sy); if (sy == sy1) continue;
			xinc = (i1->x-i0->x)/(i1->y-i0->y);
			ftol((((float)sy-i0->y)*xinc+i0->x)*65536,&y); ftol(xinc*65536,&yi);
			for(;sy<sy1;sy++,y+=yi) lastx[sy] = (y>>16);
		} while (i != itop);
		do
		{
			i0 = &pt[tri[(k<<2)+i]]; ftol(i0->y,&sy); i = incmod3[i];
			i1 = &pt[tri[(k<<2)+i]]; ftol(i1->y,&sy1); if (sy == sy1) continue;
			xinc = (i1->x-i0->x)/(i1->y-i0->y);
			ftol((((float)sy-i0->y)*xinc+i0->x)*65536,&y); ftol(xinc*65536,&yi);
			op = sy*VSID+damost;
			for(;sy<sy1;sy++,y+=yi,op+=VSID)
			{
				ftol(knmx*(float)lastx[sy] + knmy*(float)sy + knmc,&z);
				pe = (y>>16)+op; p = lastx[sy]+op;
				for(;p<pe;p++,z+=zi) *(char *)p = (z>>16);
			}
		} while (i != ibot);
	}
}

void sethull3d (point3d *pt, long nump, long dacol, long bakit)
{
	void (*modslab)(long *, long, long);
	float fminx, fminy, fminz, fmaxx, fmaxy, fmaxz;
	long i, x, y, xs, ys, xe, ye, z0, z1;

	if (nump > (MAXPOINTS>>1)) nump = (MAXPOINTS>>1); //DANGER!!!

	fminx = fminy = VSID; fminz = MAXZDIM; fmaxx = fmaxy = fmaxz = 0;
	for(i=0;i<nump;i++)
	{
		pt[i].x = MIN(MAX(pt[i].x,0),VSID-1);
		pt[i].y = MIN(MAX(pt[i].y,0),VSID-1);
		pt[i].z = MIN(MAX(pt[i].z,0),MAXZDIM-1);

		if (pt[i].x < fminx) fminx = pt[i].x;
		if (pt[i].y < fminy) fminy = pt[i].y;
		if (pt[i].z < fminz) fminz = pt[i].z;
		if (pt[i].x > fmaxx) fmaxx = pt[i].x;
		if (pt[i].y > fmaxy) fmaxy = pt[i].y;
		if (pt[i].z > fmaxz) fmaxz = pt[i].z;
	}

	ftol(fminx,&xs); if (xs < 0) xs = 0;
	ftol(fminy,&ys); if (ys < 0) ys = 0;
	ftol(fmaxx,&xe); if (xe >= VSID) xe = VSID-1;
	ftol(fmaxy,&ye); if (ye >= VSID) ye = VSID-1;
	vx5.minx = xs; vx5.maxx = xe+1;
	vx5.miny = ys; vx5.maxy = ye+1;
	ftol(fminz-.5,&vx5.minz); ftol(fmaxz+.5,&vx5.maxz);
	if ((xs > xe) || (ys > ye))
		{ if (bakit) voxbackup(xs,ys,xs,ys,bakit); return; }
	if (bakit) voxbackup(xs,ys,xe,ye,bakit);

	i = ys*VSID+(xs&~3); x = ((((xe+3)&~3)-(xs&~3))>>2)+1;
	for(y=ys;y<=ye;y++,i+=VSID)
		{ clearbuf((void *)&umost[i],x,-1); clearbuf((void *)&dmost[i],x,0); }

	inithull3d(pt,nump);
	tmaphulltrisortho(pt);

	if (vx5.colfunc == jitcolfunc) vx5.amount = 0x70707;

	if (dacol == -1) modslab = delslab; else modslab = insslab;

	for(y=ys;y<=ye;y++)
		for(x=xs;x<=xe;x++)
			modslab(scum2(x,y),(long)umost[y*VSID+x],(long)dmost[y*VSID+x]);
	scum2finish();
	updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,dacol);
}

// ------------------------- CONVEX 3D HULL CODE ENDS -------------------------

	//Old&Slow sector code, but only this one supports the 3D bumpmapping :(
static void setsectorb (point3d *p, long *point2, long n, float thick, long dacol, long bakit, long bumpmap)
{
	point3d norm, p2;
	float d, f, x0, y0, x1, y1;
	long i, j, k, got, x, y, z, xs, ys, zs, xe, ye, ze, maxis, ndacol;

	norm.x = 0; norm.y = 0; norm.z = 0;
	for(i=0;i<n;i++)
	{
		j = point2[i]; k = point2[j];
		norm.x += (p[i].y-p[j].y)*(p[k].z-p[j].z) - (p[i].z-p[j].z)*(p[k].y-p[j].y);
		norm.y += (p[i].z-p[j].z)*(p[k].x-p[j].x) - (p[i].x-p[j].x)*(p[k].z-p[j].z);
		norm.z += (p[i].x-p[j].x)*(p[k].y-p[j].y) - (p[i].y-p[j].y)*(p[k].x-p[j].x);
	}
	f = 1.0 / sqrt(norm.x*norm.x + norm.y*norm.y + norm.z*norm.z);
	norm.x *= f; norm.y *= f; norm.z *= f;

	if ((fabs(norm.z) >= fabs(norm.x)) && (fabs(norm.z) >= fabs(norm.y)))
		maxis = 2;
	else if (fabs(norm.y) > fabs(norm.x))
		maxis = 1;
	else
		maxis = 0;

	xs = xe = p[0].x;
	ys = ye = p[0].y;
	zs = ze = p[0].z;
	for(i=n-1;i;i--)
	{
		if (p[i].x < xs) xs = p[i].x;
		if (p[i].y < ys) ys = p[i].y;
		if (p[i].z < zs) zs = p[i].z;
		if (p[i].x > xe) xe = p[i].x;
		if (p[i].y > ye) ye = p[i].y;
		if (p[i].z > ze) ze = p[i].z;
	}
	xs = MAX(xs-thick-bumpmap,0); xe = MIN(xe+thick+bumpmap,VSID-1);
	ys = MAX(ys-thick-bumpmap,0); ye = MIN(ye+thick+bumpmap,VSID-1);
	zs = MAX(zs-thick-bumpmap,0); ze = MIN(ze+thick+bumpmap,MAXZDIM-1);
	vx5.minx = xs; vx5.maxx = xe+1;
	vx5.miny = ys; vx5.maxy = ye+1;
	vx5.minz = zs; vx5.maxz = ze+1;
	if ((xs > xe) || (ys > ye) || (zs > ze)) return;
	if (bakit) voxbackup(xs,ys,xe+1,ye+1,bakit);

	clearbuf((void *)&templongbuf[zs],ze-zs+1,-3);

	ndacol = (dacol==-1)-2;

	for(y=ys;y<=ye;y++)
		for(x=xs;x<=xe;x++)
		{
			got = 0;
			d = ((float)x-p[0].x)*norm.x + ((float)y-p[0].y)*norm.y + ((float)zs-p[0].z)*norm.z;
			for(z=zs;z<=ze;z++,d+=norm.z)
			{
				if (bumpmap)
				{
					if (d < -thick) continue;
					p2.x = (float)x - d*norm.x;
					p2.y = (float)y - d*norm.y;
					p2.z = (float)z - d*norm.z;
					if (d > (float)hpngcolfunc(&p2)+thick) continue;
				}
				else
				{
					if (fabs(d) > thick) continue;
					p2.x = (float)x - d*norm.x;
					p2.y = (float)y - d*norm.y;
					p2.z = (float)z - d*norm.z;
				}

				k = 0;
				for(i=n-1;i>=0;i--)
				{
					j = point2[i];
					switch(maxis)
					{
						case 0: x0 = p[i].z-p2.z; x1 = p[j].z-p2.z;
								  y0 = p[i].y-p2.y; y1 = p[j].y-p2.y; break;
						case 1: x0 = p[i].x-p2.x; x1 = p[j].x-p2.x;
								  y0 = p[i].z-p2.z; y1 = p[j].z-p2.z; break;
						case 2: x0 = p[i].x-p2.x; x1 = p[j].x-p2.x;
								  y0 = p[i].y-p2.y; y1 = p[j].y-p2.y; break;
						default: _gtfo(); //tells MSVC default can't be reached
					}
					if (((*(long *)&y0)^(*(long *)&y1)) < 0)
					{
						if (((*(long *)&x0)^(*(long *)&x1)) >= 0) k ^= (*(long *)&x0);
						else { f = (x0*y1-x1*y0); k ^= (*(long *)&f)^(*(long *)&y1); }
					}
				}
				if (k >= 0) continue;

				templongbuf[z] = ndacol; got = 1;
			}
			if (got)
			{
				scum(x,y,zs,ze+1,templongbuf);
				clearbuf((void *)&templongbuf[zs],ze-zs+1,-3);
			}
		}
	scumfinish();
	updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,dacol);
}

	//This is for ordfillpolygon&splitpoly
typedef struct { long p, i, t; } raster;
#define MAXCURS 100 //THIS IS VERY EVIL... FIX IT!!!
static raster rst[MAXCURS];
static long slist[MAXCURS];

	//Code taken from POLYOLD\POLYSPLI.BAS:splitpoly (06/09/2001)
void splitpoly (float *px, float *py, long *point2, long *bakn,
					 float x0, float y0, float dx, float dy)
{
	long i, j, s2, n, sn, splcnt, z0, z1, z2, z3;
	float t, t1;

	n = (*bakn); if (n < 3) return;
	i = 0; s2 = sn = n; splcnt = 0;
	do
	{
		t1 = (px[i]-x0)*dy - (py[i]-y0)*dx;
		do
		{
			j = point2[i]; point2[i] |= 0x80000000;
			t = t1; t1 = (px[j]-x0)*dy - (py[j]-y0)*dx;
			if ((*(long *)&t) < 0)
				{ px[n] = px[i]; py[n] = py[i]; point2[n] = n+1; n++; }
			if (((*(long *)&t) ^ (*(long *)&t1)) < 0)
			{
				if ((*(long *)&t) < 0) slist[splcnt++] = n;
				t /= (t-t1);
				px[n] = (px[j]-px[i])*t + px[i];
				py[n] = (py[j]-py[i])*t + py[i];
				point2[n] = n+1; n++;
			}
			i = j;
		} while (point2[i] >= 0);
		if (n > s2) { point2[n-1] = s2; s2 = n; }
		for(i=sn-1;(i) && (point2[i] < 0);i--);
	} while (i > 0);

	if (fabs(dx) > fabs(dy))
	{
		for(i=1;i<splcnt;i++)
		{
			z0 = slist[i];
			for(j=0;j<i;j++)
			{
				z1 = point2[z0]; z2 = slist[j]; z3 = point2[z2];
				if (fabs(px[z0]-px[z3])+fabs(px[z2]-px[z1]) < fabs(px[z0]-px[z1])+fabs(px[z2]-px[z3]))
					{ point2[z0] = z3; point2[z2] = z1; }
			}
		}
	}
	else
	{
		for(i=1;i<splcnt;i++)
		{
			z0 = slist[i];
			for(j=0;j<i;j++)
			{
				z1 = point2[z0]; z2 = slist[j]; z3 = point2[z2];
				if (fabs(py[z0]-py[z3])+fabs(py[z2]-py[z1]) < fabs(py[z0]-py[z1])+fabs(py[z2]-py[z3]))
					{ point2[z0] = z3; point2[z2] = z1; }
			}
		}
	}

	for(i=sn;i<n;i++)
		{ px[i-sn] = px[i]; py[i-sn] = py[i]; point2[i-sn] = point2[i]-sn; }
	(*bakn) = n-sn;
}

void ordfillpolygon (float *px, float *py, long *point2, long n, long day, long xs, long xe, void (*modslab)(long *, long, long))
{
	float f;
	long k, i, z, zz, z0, z1, zx, sx0, sy0, sx1, sy1, sy, nsy, gap, numrst;
	long np, ni;

	if (n < 3) return;

	for(z=0;z<n;z++) slist[z] = z;

		//Sort points by y's
	for(gap=(n>>1);gap;gap>>=1)
		for(z=0;z<n-gap;z++)
			for(zz=z;zz>=0;zz-=gap)
			{
				if (py[point2[slist[zz]]] <= py[point2[slist[zz+gap]]]) break;
				z0 = slist[zz]; slist[zz] = slist[zz+gap]; slist[zz+gap] = z0;
			}

	ftol(py[point2[slist[0]]]+.5,&sy); if (sy < xs) sy = xs;

	numrst = 0; z = 0; n--; //Note: n is local variable!
	while (z < n)
	{
		z1 = slist[z]; z0 = point2[z1];
		for(zx=0;zx<2;zx++)
		{
			ftol(py[z0]+.5,&sy0); ftol(py[z1]+.5,&sy1);
			if (sy1 > sy0) //Insert raster (z0,z1)
			{
				f = (px[z1]-px[z0]) / (py[z1]-py[z0]);
				ftol(((sy-py[z0])*f + px[z0])*65536.0 + 65535.0,&np);
				if (sy1-sy0 >= 2) ftol(f*65536.0,&ni); else ni = 0;
				k = (np<<1)+ni;
				for(i=numrst;i>0;i--)
				{
					if ((rst[i-1].p<<1)+rst[i-1].i < k) break;
					rst[i] = rst[i-1];
				}
				rst[i].i = ni; rst[i].p = np; rst[i].t = (z0<<16)+z1;
				numrst++;
			}
			else if (sy1 < sy0) //Delete raster (z1,z0)
			{
				numrst--;
				k = (z1<<16)+z0; i = 0;
				while ((i < numrst) && (rst[i].t != k)) i++;
				while (i < numrst) { rst[i] = rst[i+1]; i++; }
			}
			z1 = point2[z0];
		}

		z++;
		ftol(py[point2[slist[z]]]+.5,&nsy); if (nsy > xe) nsy = xe;
		for(;sy<nsy;sy++)
			for(i=0;i<numrst;i+=2)
			{
				modslab(scum2(sy,day),MAX(rst[i].p>>16,0),MIN(rst[i+1].p>>16,MAXZDIM));
				rst[i].p += rst[i].i; rst[i+1].p += rst[i+1].i;
			}
	}
}

	//Draws a flat polygon
	//given: p&point2: 3D points, n: # points, thick: thickness, dacol: color
static float ppx[MAXCURS*4], ppy[MAXCURS*4];
static long npoint2[MAXCURS*4];
void setsector (point3d *p, long *point2, long n, float thick, long dacol, long bakit)
{
	void (*modslab)(long *, long, long);
	point3d norm;
	float f, rnormy, xth, zth, dax, daz, t, t1;
	long i, j, k, x, y, z, sn, s2, nn, xs, ys, zs, xe, ye, ze;

	norm.x = 0; norm.y = 0; norm.z = 0;
	for(i=0;i<n;i++)
	{
		j = point2[i]; k = point2[j];
		norm.x += (p[i].y-p[j].y)*(p[k].z-p[j].z) - (p[i].z-p[j].z)*(p[k].y-p[j].y);
		norm.y += (p[i].z-p[j].z)*(p[k].x-p[j].x) - (p[i].x-p[j].x)*(p[k].z-p[j].z);
		norm.z += (p[i].x-p[j].x)*(p[k].y-p[j].y) - (p[i].y-p[j].y)*(p[k].x-p[j].x);
	}
	f = 1.0 / sqrt(norm.x*norm.x + norm.y*norm.y + norm.z*norm.z);
	norm.x *= f; norm.y *= f; norm.z *= f;

	if (vx5.colfunc == jitcolfunc) vx5.amount = 0x70707;
	else if ((vx5.colfunc == pngcolfunc) && (vx5.pic) && (vx5.xsiz > 0) && (vx5.ysiz > 0) && (vx5.picmode == 3))
	{
			//Find biggest height offset to minimize bounding box size
		j = k = vx5.pic[0];
		for(y=vx5.ysiz-1;y>=0;y--)
		{
			i = y*(vx5.bpl>>2);
			for(x=vx5.xsiz-1;x>=0;x--)
			{
				if (vx5.pic[i+x] < j) j = vx5.pic[i+x];
				if (vx5.pic[i+x] > k) k = vx5.pic[i+x];
			}
		}
		if ((j^k)&0xff000000) //If high bytes are !=, then use bumpmapping
		{
			setsectorb(p,point2,n,thick,dacol,bakit,MAX(labs(j>>24),labs(k>>24)));
			return;
		}
	}

	xs = xe = p[0].x;
	ys = ye = p[0].y;
	zs = ze = p[0].z;
	for(i=n-1;i;i--)
	{
		if (p[i].x < xs) xs = p[i].x;
		if (p[i].y < ys) ys = p[i].y;
		if (p[i].z < zs) zs = p[i].z;
		if (p[i].x > xe) xe = p[i].x;
		if (p[i].y > ye) ye = p[i].y;
		if (p[i].z > ze) ze = p[i].z;
	}
	xs = MAX(xs-thick,0); xe = MIN(xe+thick,VSID-1);
	ys = MAX(ys-thick,0); ye = MIN(ye+thick,VSID-1);
	zs = MAX(zs-thick,0); ze = MIN(ze+thick,MAXZDIM-1);
	vx5.minx = xs; vx5.maxx = xe+1;
	vx5.miny = ys; vx5.maxy = ye+1;
	vx5.minz = zs; vx5.maxz = ze+1;
	if ((xs > xe) || (ys > ye) || (zs > ze)) return;
	if (bakit) voxbackup(xs,ys,xe+1,ye+1,bakit);

	if (dacol == -1) modslab = delslab; else modslab = insslab;

	if (fabs(norm.y) >= .001)
	{
		rnormy = 1.0 / norm.y;
		for(y=ys;y<=ye;y++)
		{
			nn = n;
			for(i=0;i<n;i++)
			{
				f = ((float)y-p[i].y) * rnormy;
				ppx[i] = norm.z*f + p[i].z;
				ppy[i] = norm.x*f + p[i].x;
				npoint2[i] = point2[i];
			}
			if (fabs(norm.x) > fabs(norm.z))
			{
				splitpoly(ppx,ppy,npoint2,&nn,p[0].z,((p[0].y-(float)y)*norm.y-thick)/norm.x+p[0].x,norm.x,-norm.z);
				splitpoly(ppx,ppy,npoint2,&nn,p[0].z,((p[0].y-(float)y)*norm.y+thick)/norm.x+p[0].x,-norm.x,norm.z);
			}
			else
			{
				splitpoly(ppx,ppy,npoint2,&nn,((p[0].y-(float)y)*norm.y-thick)/norm.z+p[0].z,p[0].x,norm.x,-norm.z);
				splitpoly(ppx,ppy,npoint2,&nn,((p[0].y-(float)y)*norm.y+thick)/norm.z+p[0].z,p[0].x,-norm.x,norm.z);
			}
			ordfillpolygon(ppx,ppy,npoint2,nn,y,xs,xe,modslab);
		}
	}
	else
	{
		xth = norm.x*thick; zth = norm.z*thick;
		for(y=ys;y<=ye;y++)
		{
			for(z=0;z<n;z++) slist[z] = 0;
			nn = 0; i = 0; sn = n;
			do
			{
				s2 = nn; t1 = p[i].y-(float)y;
				do
				{
					j = point2[i]; slist[i] = 1; t = t1; t1 = p[j].y-(float)y;
					if (((*(long *)&t) ^ (*(long *)&t1)) < 0)
					{
						k = ((*(unsigned long *)&t)>>31); t /= (t-t1);
						daz = (p[j].z-p[i].z)*t + p[i].z;
						dax = (p[j].x-p[i].x)*t + p[i].x;
						ppx[nn+k] = daz+zth; ppx[nn+1-k] = daz-zth;
						ppy[nn+k] = dax+xth; ppy[nn+1-k] = dax-xth;
						npoint2[nn] = nn+1; npoint2[nn+1] = nn+2; nn += 2;
					}
					i = j;
				} while (!slist[i]);
				if (nn > s2) { npoint2[nn-1] = s2; s2 = nn; }
				for(i=sn-1;(i) && (slist[i]);i--);
			} while (i);
			ordfillpolygon(ppx,ppy,npoint2,nn,y,xs,xe,modslab);
		}
	}
	scum2finish();
	updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,dacol);
}

	//Given: p[>=3]: points 0,1 are the axis of rotation, others make up shape
	//      numcurs: number of points
	//        dacol: color
void setlathe (point3d *p, long numcurs, long dacol, long bakit)
{
	point3d norm, ax0, ax1, tp0, tp1;
	float d, f, x0, y0, x1, y1, px, py, pz;
	long i, j, cnt, got, x, y, z, xs, ys, zs, xe, ye, ze, maxis, ndacol;

	norm.x = (p[0].y-p[1].y)*(p[2].z-p[1].z) - (p[0].z-p[1].z)*(p[2].y-p[1].y);
	norm.y = (p[0].z-p[1].z)*(p[2].x-p[1].x) - (p[0].x-p[1].x)*(p[2].z-p[1].z);
	norm.z = (p[0].x-p[1].x)*(p[2].y-p[1].y) - (p[0].y-p[1].y)*(p[2].x-p[1].x);
	f = 1.0 / sqrt(norm.x*norm.x + norm.y*norm.y + norm.z*norm.z);
	norm.x *= f; norm.y *= f; norm.z *= f;

	ax0.x = p[1].x-p[0].x; ax0.y = p[1].y-p[0].y; ax0.z = p[1].z-p[0].z;
	f = 1.0 / sqrt(ax0.x*ax0.x + ax0.y*ax0.y + ax0.z*ax0.z);
	ax0.x *= f; ax0.y *= f; ax0.z *= f;

	ax1.x = ax0.y*norm.z - ax0.z*norm.y;
	ax1.y = ax0.z*norm.x - ax0.x*norm.z;
	ax1.z = ax0.x*norm.y - ax0.y*norm.x;

	x0 = 0; //Cylindrical thickness: Perp-dist from line (p[0],p[1])
	y0 = 0; //Cylindrical min dot product from line (p[0],p[1])
	y1 = 0; //Cylindrical max dot product from line (p[0],p[1])
	for(i=numcurs-1;i;i--)
	{
		d = (p[i].x-p[0].x)*ax0.x + (p[i].y-p[0].y)*ax0.y + (p[i].z-p[0].z)*ax0.z;
		if (d < y0) y0 = d;
		if (d > y1) y1 = d;
		px = (p[i].x-p[0].x) - d*ax0.x;
		py = (p[i].y-p[0].y) - d*ax0.y;
		pz = (p[i].z-p[0].z) - d*ax0.z;
		f = px*px + py*py + pz*pz;     //Note: f is thickness SQUARED
		if (f > x0) x0 = f;
	}
	x0 = sqrt(x0)+1.0;
	tp0.x = ax0.x*y0 + p[0].x; tp1.x = ax0.x*y1 + p[0].x;
	tp0.y = ax0.y*y0 + p[0].y; tp1.y = ax0.y*y1 + p[0].y;
	tp0.z = ax0.z*y0 + p[0].z; tp1.z = ax0.z*y1 + p[0].z;
	xs = MAX(MIN(tp0.x,tp1.x)-x0,0); xe = MIN(MAX(tp0.x,tp1.x)+x0,VSID-1);
	ys = MAX(MIN(tp0.y,tp1.y)-x0,0); ye = MIN(MAX(tp0.y,tp1.y)+x0,VSID-1);
	zs = MAX(MIN(tp0.z,tp1.z)-x0,0); ze = MIN(MAX(tp0.z,tp1.z)+x0,MAXZDIM-1);
	vx5.minx = xs; vx5.maxx = xe+1;
	vx5.miny = ys; vx5.maxy = ye+1;
	vx5.minz = zs; vx5.maxz = ze+1;
	if ((xs > xe) || (ys > ye) || (zs > ze)) return;
	if (bakit) voxbackup(xs,ys,xe,ye,bakit);

	if ((fabs(norm.z) >= fabs(norm.x)) && (fabs(norm.z) >= fabs(norm.y)))
		maxis = 2;
	else if (fabs(norm.y) > fabs(norm.x))
		maxis = 1;
	else
		maxis = 0;

	clearbuf((void *)&templongbuf[zs],ze-zs+1,-3);

	if (vx5.colfunc == jitcolfunc) vx5.amount = 0x70707;

	ndacol = (dacol==-1)-2;

	for(y=ys;y<=ye;y++)
		for(x=xs;x<=xe;x++)
		{
			got = 0;
			d = ((float)x-p[0].x)*ax0.x + ((float)y-p[0].y)*ax0.y + ((float)zs-p[0].z)*ax0.z;
			for(z=zs;z<=ze;z++,d+=ax0.z)
			{
					//Another way: p = sqrt((xyz dot ax1)^2 + (xyz dot norm)^2)
				px = ((float)x-p[0].x) - d*ax0.x;
				py = ((float)y-p[0].y) - d*ax0.y;
				pz = ((float)z-p[0].z) - d*ax0.z;
				f = sqrt(px*px + py*py + pz*pz);

				px = ax0.x*d + ax1.x*f + p[0].x;
				py = ax0.y*d + ax1.y*f + p[0].y;
				pz = ax0.z*d + ax1.z*f + p[0].z;

				cnt = j = 0;
				for(i=numcurs-1;i>=0;i--)
				{
					switch(maxis)
					{
						case 0: x0 = p[i].z-pz; x1 = p[j].z-pz;
								  y0 = p[i].y-py; y1 = p[j].y-py; break;
						case 1: x0 = p[i].x-px; x1 = p[j].x-px;
								  y0 = p[i].z-pz; y1 = p[j].z-pz; break;
						case 2: x0 = p[i].x-px; x1 = p[j].x-px;
								  y0 = p[i].y-py; y1 = p[j].y-py; break;
						default: _gtfo(); //tells MSVC default can't be reached
					}
					if (((*(long *)&y0)^(*(long *)&y1)) < 0)
					{
						if (((*(long *)&x0)^(*(long *)&x1)) >= 0) cnt ^= (*(long *)&x0);
						else { f = (x0*y1-x1*y0); cnt ^= (*(long *)&f)^(*(long *)&y1); }
					}
					j = i;
				}
				if (cnt >= 0) continue;

				templongbuf[z] = ndacol; got = 1;
			}
			if (got)
			{
				scum(x,y,zs,ze+1,templongbuf);
				clearbuf((void *)&templongbuf[zs],ze-zs+1,-3);
			}
		}
	scumfinish();
	updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,dacol);
}

	//Given: p[>=1]: centers
	//   vx5.currad: cutoff value
	//      numcurs: number of points
	//        dacol: color
void setblobs (point3d *p, long numcurs, long dacol, long bakit)
{
	float dx, dy, dz, v, nrad;
	long i, got, x, y, z, xs, ys, zs, xe, ye, ze, ndacol;

	if (numcurs <= 0) return;

		//Boundaries are quick hacks - rewrite this code!!!
	xs = MAX(p[0].x-64,0); xe = MIN(p[0].x+64,VSID-1);
	ys = MAX(p[0].y-64,0); ye = MIN(p[0].y+64,VSID-1);
	zs = MAX(p[0].z-64,0); ze = MIN(p[0].z+64,MAXZDIM-1);
	vx5.minx = xs; vx5.maxx = xe+1;
	vx5.miny = ys; vx5.maxy = ye+1;
	vx5.minz = zs; vx5.maxz = ze+1;
	if ((xs > xe) || (ys > ye) || (zs > ze)) return;
	if (bakit) voxbackup(xs,ys,xe,ye,bakit);

	clearbuf((void *)&templongbuf[zs],ze-zs+1,-3);

	if (vx5.colfunc == jitcolfunc) vx5.amount = 0x70707;

	ndacol = (dacol==-1)-2;

	nrad = (float)numcurs / ((float)vx5.currad*(float)vx5.currad + 256.0);
	for(y=ys;y<=ye;y++)
		for(x=xs;x<=xe;x++)
		{
			got = 0;
			for(z=zs;z<=ze;z++)
			{
				v = 0;
				for(i=numcurs-1;i>=0;i--)
				{
					dx = p[i].x-(float)x;
					dy = p[i].y-(float)y;
					dz = p[i].z-(float)z;
					v += 1.0f / (dx*dx + dy*dy + dz*dz + 256.0f);
				}
				if (*(long *)&v > *(long *)&nrad) { templongbuf[z] = ndacol; got = 1; }
			}
			if (got)
			{
				scum(x,y,zs,ze+1,templongbuf);
				clearbuf((void *)&templongbuf[zs],ze-zs+1,-3);
			}
		}
	scumfinish();
	updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,dacol);
}

//FLOODFILL3D begins --------------------------------------------------------

#define FILLBUFSIZ 16384 //Use realloc instead!
typedef struct { unsigned short x, y, z0, z1; } spoint4d; //128K
static spoint4d fbuf[FILLBUFSIZ];

long dntil0 (long x, long y, long z)
{
	char *v = sptr[y*VSID+x];
	while (1)
	{
		if (z < v[1]) break;
		if (!v[0]) return(MAXZDIM);
		v += v[0]*4;
		if (z < v[3]) return(v[3]);
	}
	return(z);
}

long dntil1 (long x, long y, long z)
{
	char *v = sptr[y*VSID+x];
	while (1)
	{
		if (z <= v[1]) return(v[1]);
		if (!v[0]) break;
		v += v[0]*4;
		if (z < v[3]) break;
	}
	return(z);
}

long uptil1 (long x, long y, long z)
{
	char *v = sptr[y*VSID+x];
	if (z < v[1]) return(0);
	while (v[0])
	{
		v += v[0]*4;
		if (z < v[3]) break;
		if (z < v[1]) return(v[3]);
	}
	return(z);
}

	//Conducts on air and writes solid
void setfloodfill3d (long x, long y, long z, long minx, long miny, long minz,
															long maxx, long maxy, long maxz)
{
	long wholemap, j, z0, z1, nz1, i0, i1, (*bakcolfunc)(lpoint3d *);
	spoint4d a;

	if (minx < 0) minx = 0;
	if (miny < 0) miny = 0;
	if (minz < 0) minz = 0;
	maxx++; maxy++; maxz++;
	if (maxx > VSID) maxx = VSID;
	if (maxy > VSID) maxy = VSID;
	if (maxz > MAXZDIM) maxz = MAXZDIM;
	vx5.minx = minx; vx5.maxx = maxx;
	vx5.miny = miny; vx5.maxy = maxy;
	vx5.minz = minz; vx5.maxz = maxz;
	if ((minx >= maxx) || (miny >= maxy) || (minz >= maxz)) return;

	if ((x < minx) || (x >= maxx) ||
		 (y < miny) || (y >= maxy) ||
		 (z < minz) || (z >= maxz)) return;

	if ((minx != 0) || (miny != 0) || (minz != 0) || (maxx != VSID) || (maxy != VSID) || (maxz != VSID))
		wholemap = 0;
	else wholemap = 1;

	if (isvoxelsolid(x,y,z)) return;

	bakcolfunc = vx5.colfunc; vx5.colfunc = curcolfunc;

	a.x = x; a.z0 = uptil1(x,y,z); if (a.z0 < minz) a.z0 = minz;
	a.y = y; a.z1 = dntil1(x,y,z+1); if (a.z1 > maxz) a.z1 = maxz;
	if (((!a.z0) && (wholemap)) || (a.z0 >= a.z1)) { vx5.colfunc = bakcolfunc; return; } //oops! broke free :/
	insslab(scum2(x,y),a.z0,a.z1); scum2finish();
	i0 = i1 = 0; goto floodfill3dskip;
	do
	{
		a = fbuf[i0]; i0 = ((i0+1)&(FILLBUFSIZ-1));
floodfill3dskip:;
		for(j=3;j>=0;j--)
		{
			if (j&1) { x = a.x+(j&2)-1; if ((x < minx) || (x >= maxx)) continue; y = a.y; }
				 else { y = a.y+(j&2)-1; if ((y < miny) || (y >= maxy)) continue; x = a.x; }

			if (isvoxelsolid(x,y,a.z0)) { z0 = dntil0(x,y,a.z0); z1 = z0; }
										  else { z0 = uptil1(x,y,a.z0); z1 = a.z0; }
			if ((!z0) && (wholemap)) { vx5.colfunc = bakcolfunc; return; } //oops! broke free :/
			while (z1 < a.z1)
			{
				z1 = dntil1(x,y,z1);

				if (z0 < minz) z0 = minz;
				nz1 = z1; if (nz1 > maxz) nz1 = maxz;
				if (z0 < nz1)
				{
					fbuf[i1].x = x; fbuf[i1].y = y;
					fbuf[i1].z0 = z0; fbuf[i1].z1 = nz1;
					i1 = ((i1+1)&(FILLBUFSIZ-1));
					//if (i0 == i1) floodfill stack overflow!
					insslab(scum2(x,y),z0,nz1); scum2finish();
				}
				z0 = dntil0(x,y,z1); z1 = z0;
			}
		}
	} while (i0 != i1);

	vx5.colfunc = bakcolfunc;

	updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,0);
}

void hollowfillstart (long x, long y, long z)
{
	spoint4d a;
	char *v;
	long i, j, z0, z1, i0, i1;

	a.x = x; a.y = y;

	v = sptr[y*VSID+x]; j = ((((long)v)-(long)vbuf)>>2); a.z0 = 0;
	while (1)
	{
		a.z1 = (long)(v[1]);
		if ((a.z0 <= z) && (z < a.z1) && (!(vbit[j>>5]&(1<<j)))) break;
		if (!v[0]) return;
		v += v[0]*4; j += 2;
		a.z0 = (long)(v[3]);
	}
	vbit[j>>5] |= (1<<j); //fill a.x,a.y,a.z0<=?<a.z1

	i0 = i1 = 0; goto floodfill3dskip2;
	do
	{
		a = fbuf[i0]; i0 = ((i0+1)&(FILLBUFSIZ-1));
floodfill3dskip2:;
		for(i=3;i>=0;i--)
		{
			if (i&1) { x = a.x+(i&2)-1; if ((unsigned long)x >= VSID) continue; y = a.y; }
				 else { y = a.y+(i&2)-1; if ((unsigned long)y >= VSID) continue; x = a.x; }

			v = sptr[y*VSID+x]; j = ((((long)v)-(long)vbuf)>>2); z0 = 0;
			while (1)
			{
				z1 = (long)(v[1]);
				if ((z0 < a.z1) && (a.z0 < z1) && (!(vbit[j>>5]&(1<<j))))
				{
					fbuf[i1].x = x; fbuf[i1].y = y;
					fbuf[i1].z0 = z0; fbuf[i1].z1 = z1;
					i1 = ((i1+1)&(FILLBUFSIZ-1));
					if (i0 == i1) return; //floodfill stack overflow!
					vbit[j>>5] |= (1<<j); //fill x,y,z0<=?<z1
				}
				if (!v[0]) break;
				v += v[0]*4; j += 2;
				z0 = (long)(v[3]);
			}
		}
	} while (i0 != i1);
}

	//hollowfill
void sethollowfill ()
{
	long i, j, l, x, y, z0, z1, *lptr, (*bakcolfunc)(lpoint3d *);
	char *v;

	vx5.minx = 0; vx5.maxx = VSID;
	vx5.miny = 0; vx5.maxy = VSID;
	vx5.minz = 0; vx5.maxz = MAXZDIM;

	for(i=0;i<VSID*VSID;i++)
	{
		j = ((((long)sptr[i])-(long)vbuf)>>2);
		for(v=sptr[i];v[0];v+=v[0]*4) { vbit[j>>5] &= ~(1<<j); j += 2; }
		vbit[j>>5] &= ~(1<<j);
	}

	for(y=0;y<VSID;y++)
		for(x=0;x<VSID;x++)
			hollowfillstart(x,y,0);

	bakcolfunc = vx5.colfunc; vx5.colfunc = curcolfunc;
	i = 0;
	for(y=0;y<VSID;y++)
		for(x=0;x<VSID;x++,i++)
		{
			j = ((((long)sptr[i])-(long)vbuf)>>2);
			v = sptr[i]; z0 = MAXZDIM;
			while (1)
			{
				z1 = (long)(v[1]);
				if ((z0 < z1) && (!(vbit[j>>5]&(1<<j))))
				{
					vbit[j>>5] |= (1<<j);
					insslab(scum2(x,y),z0,z1);
				}
				if (!v[0]) break;
				v += v[0]*4; j += 2;
				z0 = (long)(v[3]);
			}
		}
	scum2finish();
	vx5.colfunc = bakcolfunc;
	updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,0);
}

//FLOODFILL3D ends ----------------------------------------------------------

#define LPATBUFSIZ 14
static lpoint2d *patbuf;
#define LPATHASHSIZ 12
static lpoint3d *pathashdat;
static long *pathashead, pathashcnt, pathashmax;

static void initpathash ()
{
	patbuf = (lpoint2d *)radar;
	pathashead = (long *)(((long)patbuf)+(1<<LPATBUFSIZ)*sizeof(lpoint2d));
	pathashdat = (lpoint3d *)(((long)pathashead)+((1<<LPATHASHSIZ)*4));
	pathashmax = ((MAX((MAXXDIM*MAXYDIM*27)>>1,(VSID+4)*3*256*4)-((1<<LPATBUFSIZ)*sizeof(lpoint2d))-(1<<LPATHASHSIZ)*4)/12);
	memset(pathashead,-1,(1<<LPATHASHSIZ)*4);
	pathashcnt = 0;
}

static long readpathash (long i)
{
	long j = (((i>>LPATHASHSIZ)-i) & ((1<<LPATHASHSIZ)-1));
	for(j=pathashead[j];j>=0;j=pathashdat[j].x)
		if (pathashdat[j].y == i) return(pathashdat[j].z);
	return(-1);
}

static void writepathash (long i, long v)
{
	long k, j = (((i>>LPATHASHSIZ)-i) & ((1<<LPATHASHSIZ)-1));
	for(k=pathashead[j];k>=0;k=pathashdat[k].x)
		if (pathashdat[k].y == i) { pathashdat[k].z = v; return; }
	pathashdat[pathashcnt].x = pathashead[j]; pathashead[j] = pathashcnt;
	pathashdat[pathashcnt].y = i;
	pathashdat[pathashcnt].z = v;
	pathashcnt++;
}

static signed char cdir[26*4] = //sqrt(2) =~ 58/41, sqrt(3) =~ 71/41;
{
	-1, 0, 0,41,  1, 0, 0,41,  0,-1, 0,41,  0, 1, 0,41,  0, 0,-1,41,  0, 0, 1,41,
	-1,-1, 0,58, -1, 1, 0,58, -1, 0,-1,58, -1, 0, 1,58,  0,-1,-1,58,  0,-1, 1,58,
	 1,-1, 0,58,  1, 1, 0,58,  1, 0,-1,58,  1, 0, 1,58,  0, 1,-1,58,  0, 1, 1,58,
	-1,-1,-1,71, -1,-1, 1,71, -1, 1,-1,71, -1, 1, 1,71,
	 1,-1,-1,71,  1,-1, 1,71,  1, 1,-1,71,  1, 1, 1,71,
};

long findpath (long *pathpos, long pathmax, lpoint3d *p1, lpoint3d *p0)
{
	long i, j, k, x, y, z, c, nc, xx, yy, zz, bufr, bufw, pcnt;

	if (!(getcube(p0->x,p0->y,p0->z)&~1))
	{
		for(i=5;i>=0;i--)
		{
			x = p0->x+(long)cdir[i*4]; y = p0->y+(long)cdir[i*4+1]; z = p0->z+(long)cdir[i*4+2];
			if (getcube(x,y,z)&~1) { p0->x = x; p0->y = y; p0->z = z; break; }
		}
		if (i < 0) return(0);
	}
	if (!(getcube(p1->x,p1->y,p1->z)&~1))
	{
		for(i=5;i>=0;i--)
		{
			x = p1->x+(long)cdir[i*4]; y = p1->y+(long)cdir[i*4+1]; z = p1->z+(long)cdir[i*4+2];
			if (getcube(x,y,z)&~1) { p1->x = x; p1->y = y; p1->z = z; break; }
		}
		if (i < 0) return(0);
	}

	initpathash();
	j = (p0->x*VSID + p0->y)*MAXZDIM+p0->z;
	patbuf[0].x = j; patbuf[0].y = 0; bufr = 0; bufw = 1;
	writepathash(j,0);
	do
	{
		j = patbuf[bufr&((1<<LPATBUFSIZ)-1)].x;
		x = j/(VSID*MAXZDIM); y = ((j/MAXZDIM)&(VSID-1)); z = (j&(MAXZDIM-1));
		c = patbuf[bufr&((1<<LPATBUFSIZ)-1)].y; bufr++;
		for(i=0;i<26;i++)
		{
			xx = x+(long)cdir[i*4]; yy = y+(long)cdir[i*4+1]; zz = z+(long)cdir[i*4+2];
			j = (xx*VSID + yy)*MAXZDIM+zz;

			//nc = c+(long)cdir[i*4+3]; //More accurate but lowers max distance a lot!
			//if (((k = getcube(xx,yy,zz))&~1) && ((unsigned long)nc < (unsigned long)readpathash(j)))

			if (((k = getcube(xx,yy,zz))&~1) && (readpathash(j) < 0))
			{
				nc = c+(long)cdir[i*4+3];
				if ((xx == p1->x) && (yy == p1->y) && (zz == p1->z)) { c = nc; goto pathfound; }
				writepathash(j,nc);
				if (pathashcnt >= pathashmax) return(0);
				patbuf[bufw&((1<<LPATBUFSIZ)-1)].x = (xx*VSID + yy)*MAXZDIM+zz;
				patbuf[bufw&((1<<LPATBUFSIZ)-1)].y = nc; bufw++;
			}
		}
	} while (bufr != bufw);

pathfound:
	if (pathmax <= 0) return(0);
	pathpos[0] = (p1->x*VSID + p1->y)*MAXZDIM+p1->z; pcnt = 1;
	x = p1->x; y = p1->y; z = p1->z;
	do
	{
		for(i=0;i<26;i++)
		{
			xx = x+(long)cdir[i*4]; yy = y+(long)cdir[i*4+1]; zz = z+(long)cdir[i*4+2];
			nc = c-(long)cdir[i*4+3];
			if (readpathash((xx*VSID + yy)*MAXZDIM+zz) == nc)
			{
				if (pcnt >= pathmax) return(0);
				pathpos[pcnt] = (xx*VSID + yy)*MAXZDIM+zz; pcnt++;
				x = xx; y = yy; z = zz; c = nc; break;
			}
		}
	} while (i < 26);
	if (pcnt >= pathmax) return(0);
	pathpos[pcnt] = (p0->x*VSID + p0->y)*MAXZDIM+p0->z;
	return(pcnt+1);
}

//---------------------------------------------------------------------

static unsigned short xyoffs[256][256+1];
void setkvx (const char *filename, long ox, long oy, long oz, long rot, long bakit)
{
	long i, j, x, y, z, xsiz, ysiz, zsiz, longpal[256], zleng, oldz, vis;
	long d[3], k[9], x0, y0, z0, x1, y1, z1;
	char ch, typ;
	FILE *fp;

	typ = filename[strlen(filename)-3]; if (typ == 'k') typ = 'K';

	if (!(fp = fopen(filename,"rb"))) return;

	fseek(fp,-768,SEEK_END);
	for(i=0;i<255;i++)
	{
		longpal[i]  = (((long)fgetc(fp))<<18);
		longpal[i] += (((long)fgetc(fp))<<10);
		longpal[i] += (((long)fgetc(fp))<< 2) + 0x80000000;
	}
	longpal[255] = 0x7ffffffd;

	if (typ == 'K') //Load .KVX file
	{
		fseek(fp,4,SEEK_SET);
		fread(&xsiz,4,1,fp);
		fread(&ysiz,4,1,fp);
		fread(&zsiz,4,1,fp);
		fseek(fp,((xsiz+1)<<2)+28,SEEK_SET);
		for(i=0;i<xsiz;i++) fread(&xyoffs[i][0],(ysiz+1)<<1,1,fp);
	}
	else           //Load .VOX file
	{
		fseek(fp,0,SEEK_SET);
		fread(&xsiz,4,1,fp);
		fread(&ysiz,4,1,fp);
		fread(&zsiz,4,1,fp);
	}

		//rot: low 3 bits for axis negating, high 6 states for axis swapping
		//k[0], k[3], k[6] are indeces
		//k[1], k[4], k[7] are xors
		//k[2], k[5], k[8] are adds
	switch (rot&~7)
	{
		case  0: k[0] = 0; k[3] = 1; k[6] = 2; break; //can use scum!
		case  8: k[0] = 1; k[3] = 0; k[6] = 2; break; //can use scum!
		case 16: k[0] = 0; k[3] = 2; k[6] = 1; break;
		case 24: k[0] = 2; k[3] = 0; k[6] = 1; break;
		case 32: k[0] = 1; k[3] = 2; k[6] = 0; break;
		case 40: k[0] = 2; k[3] = 1; k[6] = 0; break;
		default: _gtfo(); //tells MSVC default can't be reached
	}
	k[1] = ((rot<<31)>>31);
	k[4] = ((rot<<30)>>31);
	k[7] = ((rot<<29)>>31);

	d[0] = xsiz; d[1] = ysiz; d[2] = zsiz;
	k[2] = ox-((d[k[0]]>>1)^k[1]);
	k[5] = oy-((d[k[3]]>>1)^k[4]);
	k[8] = oz-((d[k[6]]>>1)^k[7]); k[8] -= (d[k[6]]>>1);

	d[0] = d[1] = d[2] = 0;
	x0 = x1 = (d[k[0]]^k[1])+k[2];
	y0 = y1 = (d[k[3]]^k[4])+k[5];
	z0 = z1 = (d[k[6]]^k[7])+k[8];
	d[0] = xsiz; d[1] = ysiz; d[2] = zsiz;
	x0 = MIN(x0,(d[k[0]]^k[1])+k[2]); x1 = MAX(x1,(d[k[0]]^k[1])+k[2]);
	y0 = MIN(y0,(d[k[3]]^k[4])+k[5]); y1 = MAX(y1,(d[k[3]]^k[4])+k[5]);
	z0 = MIN(z0,(d[k[6]]^k[7])+k[8]); z1 = MAX(z1,(d[k[6]]^k[7])+k[8]);
	if (x0 < 1) { i = 1-x0; x0 += i; x1 += i; k[2] += i; }
	if (y0 < 1) { i = 1-y0; y0 += i; y1 += i; k[5] += i; }
	if (z0 < 0) { i = 0-z0; z0 += i; z1 += i; k[8] += i; }
	if (x1 > VSID-2)    { i = VSID-2-x1; x0 += i; x1 += i; k[2] += i; }
	if (y1 > VSID-2)    { i = VSID-2-y1; y0 += i; y1 += i; k[5] += i; }
	if (z1 > MAXZDIM-1) { i = MAXZDIM-1-z1; z0 += i; z1 += i; k[8] += i; }

	vx5.minx = x0; vx5.maxx = x1+1;
	vx5.miny = y0; vx5.maxy = y1+1;
	vx5.minz = z0; vx5.maxz = z1+1;
	if (bakit) voxbackup(x0,y0,x1+1,y1+1,bakit);

	j = (!(k[3]|(rot&3))); //if (j) { can use scum/scumfinish! }

	for(x=0;x<xsiz;x++)
	{
		d[0] = x;
		for(y=0;y<ysiz;y++)
		{
			d[1] = y;
			if (k[6] == 2) //can use scum!
			{
				clearbuf((void *)&templongbuf[z0],z1-z0+1,-3);
				if (typ == 'K')
				{
					oldz = -1;
					i = xyoffs[d[0]][d[1]+1] - xyoffs[d[0]][d[1]]; if (!i) continue;
					while (i > 0)
					{
						z = fgetc(fp); zleng = fgetc(fp); i -= (zleng+3);
						vis = fgetc(fp);

						if ((oldz >= 0) && (!(vis&16)))
							for(;oldz<z;oldz++)
								templongbuf[(oldz^k[7])+k[8]] = vx5.curcol;

						for(;zleng>0;zleng--,z++)
							templongbuf[(z^k[7])+k[8]] = longpal[fgetc(fp)];
						oldz = z;
					}
				}
				else
				{
					for(z=0;z<zsiz;z++)
						templongbuf[(z^k[7])+k[8]] = longpal[fgetc(fp)];
				}

				scum((d[k[0]]^k[1])+k[2],(d[k[3]]^k[4])+k[5],z0,z1+1,templongbuf);
				if (!j) scumfinish();
			}
			else
			{
				if (typ == 'K')
				{
					oldz = -1;
					i = xyoffs[d[0]][d[1]+1] - xyoffs[d[0]][d[1]]; if (!i) continue;
					while (i > 0)
					{
						z = fgetc(fp); zleng = fgetc(fp); i -= (zleng+3);
						vis = fgetc(fp);

						if ((oldz >= 0) && (!(vis&16)))
							for(;oldz<z;oldz++)
							{
								d[2] = oldz;
								setcube((d[k[0]]^k[1])+k[2],(d[k[3]]^k[4])+k[5],(d[k[6]]^k[7])+k[8],vx5.curcol);
							}

						for(;zleng>0;zleng--,z++)
						{
							ch = fgetc(fp);
							d[2] = z;
							setcube((d[k[0]]^k[1])+k[2],(d[k[3]]^k[4])+k[5],(d[k[6]]^k[7])+k[8],longpal[ch]);
						}
						oldz = z;
					}
				}
				else
				{
					for(z=0;z<zsiz;z++)
					{
						ch = fgetc(fp);
						if (ch != 255)
						{
							d[2] = z;
							setcube((d[k[0]]^k[1])+k[2],(d[k[3]]^k[4])+k[5],(d[k[6]]^k[7])+k[8],longpal[ch]);
						}
					}
				}
			}
		}
	}
	if (j) scumfinish();
	updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,0);

	fclose(fp);
}

	//This here for game programmer only. I would never use it!
/**
 * Draw a pixel on the screen.
 */
void drawpoint2d (long sx, long sy, long col)
{
	if ((unsigned long)sx >= (unsigned long)xres_voxlap) return;
	if ((unsigned long)sy >= (unsigned long)yres_voxlap) return;
	*(long *)(ylookup[sy]+(sx<<2)+frameplace) = col;
}

	//This here for game programmer only. I would never use it!
/**
 * Draw a pixel on the screen. (specified by VXL location) Ignores Z-buffer.
 */
void drawpoint3d (float x0, float y0, float z0, long col)
{
	float ox, oy, oz, r;
	long x, y;

	ox = x0-gipos.x; oy = y0-gipos.y; oz = z0-gipos.z;
	z0 = ox*gifor.x + oy*gifor.y + oz*gifor.z; if (z0 < SCISDIST) return;
	r = 1.0f / z0;
	x0 = (ox*gistr.x + oy*gistr.y + oz*gistr.z)*gihz;
	y0 = (ox*gihei.x + oy*gihei.y + oz*gihei.z)*gihz;

	ftol(x0*r + gihx-.5f,&x); if ((unsigned long)x >= (unsigned long)xres_voxlap) return;
	ftol(y0*r + gihy-.5f,&y); if ((unsigned long)y >= (unsigned long)yres_voxlap) return;
	*(long *)(ylookup[y]+(x<<2)+frameplace) = col;
}

/**
 * Transform & Project a 3D point to a 2D screen coordinate. This could be
 * used for billboard sprites.
 *
 * @param x VXL location to transform & project
 * @param y VXL location to transform & project
 * @param z VXL location to transform & project
 * @param px screen coordinate returned
 * @param py screen coordinate returned
 * @param sx depth of screen coordinate
 * @return 1 if visible else 0
 */
long project2d (float x, float y, float z, float *px, float *py, float *sx)
{
	float ox, oy, oz;

	ox = x-gipos.x; oy = y-gipos.y; oz = z-gipos.z;
	z = ox*gifor.x + oy*gifor.y + oz*gifor.z; if (z < SCISDIST) return(0);

	z = gihz / z;
	*px = (ox*gistr.x + oy*gistr.y + oz*gistr.z)*z + gihx;
	*py = (ox*gihei.x + oy*gihei.y + oz*gihei.z)*z + gihy;
	*sx = z;
	return(1);
}

static int64_t mskp255 = 0x00ff00ff00ff00ff;
static int64_t mskn255 = 0xff01ff01ff01ff01;
static int64_t rgbmask64 = 0xffffff00ffffff;
/**
 * Draws a 32-bit color texture from memory to the screen. This is the
 * low-level function used to draw text loaded from a PNG,JPG,TGA,GIF.
 *
 * @param tf pointer to top-left corner of SOURCE picture
 * @param tp pitch (bytes per line) of the SOURCE picture
 * @param tx dimensions of the SOURCE picture
 * @param ty dimensions of the SOURCE picture
 * @param tcx texel (<<16) at (sx,sy). Set this to (0,0) if you want (sx,sy)
 *            to be the top-left corner of the destination
 * @param tcy texel (<<16) at (sx,sy). Set this to (0,0) if you want (sx,sy)
 *            to be the top-left corner of the destination
 * @param sx screen coordinate (matches the texture at tcx,tcy)
 * @param sy screen coordinate (matches the texture at tcx,tcy)
 * @param xz x&y zoom, all (<<16). Use (65536,65536) for no zoom change
 * @param yz x&y zoom, all (<<16). Use (65536,65536) for no zoom change
 * @param black shade scale (ARGB format). For no effects, use (0,-1)
 *              NOTE: if alphas of black&white are same, then alpha channel ignored
 * @param white shade scale (ARGB format). For no effects, use (0,-1)
 *              NOTE: if alphas of black&white are same, then alpha channel ignored
 */
void drawtile (long tf, long tp, long tx, long ty, long tcx, long tcy,
					long sx, long sy, long xz, long yz, long black, long white)
{
	long sx0, sy0, sx1, sy1, x0, y0, x1, y1, x, y, u, v, ui, vi, uu, vv;
	long p, i, j, a;

	#if defined(__GNUC__) && !defined(NOASM) //only for gcc inline asm
	register lpoint2d reg0 asm("mm0");
	register lpoint2d reg1 asm("mm1");
	register lpoint2d reg2 asm("mm2");
	//register lpoint2d reg3 asm("mm3");
	register lpoint2d reg4 asm("mm4");
	register lpoint2d reg5 asm("mm5");
	register lpoint2d reg6 asm("mm6");
	register lpoint2d reg7 asm("mm7");
	#endif

	if (!tf) return;
	sx0 = sx - mulshr16(tcx,xz); sx1 = sx0 + xz*tx;
	sy0 = sy - mulshr16(tcy,yz); sy1 = sy0 + yz*ty;
	x0 = MAX((sx0+65535)>>16,0); x1 = MIN((sx1+65535)>>16,xres_voxlap);
	y0 = MAX((sy0+65535)>>16,0); y1 = MIN((sy1+65535)>>16,yres_voxlap);
	ui = shldiv16(65536,xz); u = mulshr16(-sx0,ui);
	vi = shldiv16(65536,yz); v = mulshr16(-sy0,vi);
	if (!((black^white)&0xff000000)) //Ignore alpha
	{
		for(y=y0,vv=y*vi+v;y<y1;y++,vv+=vi)
		{
			p = ylookup[y] + frameplace; j = (vv>>16)*tp + tf;
			for(x=x0,uu=x*ui+u;x<x1;x++,uu+=ui)
			*(long *)((x<<2)+p) = *(long *)(((uu>>16)<<2) + j);
		}
	}
	else //Use alpha for masking
	{
		//Init for black/white code
		#if defined(__GNUC__) && !defined(NOASM) //gcc inline asm
		__asm__ __volatile__
		(
			"pxor	%[y7], %[y7]\n"
			"movd	%[w], %[y5]\n"
			"movd	%[b], %[y4]\n"
			"punpcklbw	%[y7], %[y5]\n"   //mm5: [00Wa00Wr00Wg00Wb]
			"punpcklbw	%[y7], %[y4]\n"   //mm4: [00Ba00Br00Bg00Bb]
			"psubw	%[y4], %[y5]\n"       //mm5: each word range: -255 to 255
			"movq	%[y5], %[y0]\n"       //if (? == -255) ? = -256;
			"movq	%[y5], %[y1]\n"       //if (? ==  255) ? =  256;
			"pcmpeqw	%c[skp], %[y0]\n" //if (mm0.w[#] == 0x00ff) mm0.w[#] = 0xffff
			"pcmpeqw	%c[skn], %[y1]\n" //if (mm1.w[#] == 0xff01) mm1.w[#] = 0xffff
			"psubw	%[y5], %[y0]\n"
			"paddw	%[y5], %[y1]\n"
			"psllw	$4, %[y5]\n"          //mm5: [-WBa-WBr-WBg-WBb]
			"movq	%c[mask], %[y6]\n"
			: [y0] "=y" (reg0), [y1] "=y" (reg1),
			  [y4] "=y" (reg4), [y5] "=y" (reg5),
			  [y6] "=y" (reg6), [y7] "=y" (reg7)
			: [w] "g" (white), [skp] "p" (&mskp255),
			  [b] "g" (black), [skn] "p" (&mskn255),
			  [mask] "p" (&rgbmask64)
			:
		);
		#elif defined(_MSC_VER) && !defined(NOASM) //msvc inline asm
		_asm
		{
			pxor	mm7, mm7
			movd	mm5, white
			movd	mm4, black
			punpcklbw	mm5, mm7 //mm5: [00Wa00Wr00Wg00Wb]
			punpcklbw	mm4, mm7 //mm4: [00Ba00Br00Bg00Bb]
			psubw	mm5, mm4     //mm5: each word range: -255 to 255
			movq	mm0, mm5     //if (? == -255) ? = -256;
			movq	mm1, mm5     //if (? ==  255) ? =  256;
			pcmpeqw	mm0, mskp255 //if (mm0.w[#] == 0x00ff) mm0.w[#] = 0xffff
			pcmpeqw	mm1, mskn255 //if (mm1.w[#] == 0xff01) mm1.w[#] = 0xffff
			psubw	mm5, mm0
			paddw	mm5, mm1
			psllw	mm5, 4       //mm5: [-WBa-WBr-WBg-WBb]
			movq	mm6, rgbmask64
		}
		#endif //no C equivalent here
		for(y=y0,vv=y*vi+v;y<y1;y++,vv+=vi)
		{
			p = ylookup[y] + frameplace; j = (vv>>16)*tp + tf;
			for(x=x0,uu=x*ui+u;x<x1;x++,uu+=ui)
			{
				i = *(long *)(((uu>>16)<<2) + j);

				#ifdef NOASM
				((uint8_t *)&i)[0] = ((uint8_t *)&i)[0] * (((uint8_t *)&white)[0] - ((uint8_t *)&black)[0])/256 + ((uint8_t *)&black)[0];
				((uint8_t *)&i)[1] = ((uint8_t *)&i)[1] * (((uint8_t *)&white)[1] - ((uint8_t *)&black)[1])/256 + ((uint8_t *)&black)[1];
				((uint8_t *)&i)[2] = ((uint8_t *)&i)[2] * (((uint8_t *)&white)[2] - ((uint8_t *)&black)[2])/256 + ((uint8_t *)&black)[2];
				((uint8_t *)&i)[3] = ((uint8_t *)&i)[3] * (((uint8_t *)&white)[3] - ((uint8_t *)&black)[3])/256 + ((uint8_t *)&black)[3];
				#else
				#ifdef __GNUC__ //gcc inline asm
				__asm__ __volatile__
				(
					"punpcklbw	%[y7], %[y0]\n" //mm1: [00Aa00Rr00Gg00Bb]
					"psllw	$4, %[y0]\n"        //mm1: [0Aa00Rr00Gg00Bb0]
					"pmulhw	%[y5], %[y0]\n"     //mm1: [--Aa--Rr--Gg--Bb]
					"paddw	%[y4], %[y0]\n"     //mm1: [00Aa00Rr00Gg00Bb]
					"movq	%[y0], %[y1]\n"
					"packuswb	%[y0], %[y0]\n"  //mm1: [AaRrGgBbAaRrGgBb]
					: [y0] "+y" (i),    [y1] "+y" (reg1), [y7] "+y" (reg7),
					  [y4] "+y" (reg4), [y5] "+y" (reg5)
					:
					:
				);
				#endif
				#ifdef _MSC_VER //msvc inline asm
				_asm
				{
					movd	mm0, i       //mm1: [00000000AaRrGgBb]
					punpcklbw	mm0, mm7 //mm1: [00Aa00Rr00Gg00Bb]
					psllw	mm0, 4       //mm1: [0Aa00Rr00Gg00Bb0]
					pmulhw	mm0, mm5     //mm1: [--Aa--Rr--Gg--Bb]
					paddw	mm0, mm4     //mm1: [00Aa00Rr00Gg00Bb]
					movq	mm1, mm0
					packuswb	mm0, mm0 //mm1: [AaRrGgBbAaRrGgBb]
					movd	i, mm0
				}
				#endif
				#endif

					//a = (((unsigned long)i)>>24);
					//if (!a) continue;
					//if (a == 255) { *(long *)((x<<2)+p) = i; continue; }
				if ((unsigned long)(i+0x1000000) < 0x2000000)
				{
					if (i < 0) *(long *)((x<<2)+p) = i;
					continue;
				}
				#ifdef __GNUC__ //gcc inline asm
				__asm__ __volatile__
				(
					//mm0 = (mm1-mm0)*a + mm0
					"lea	(%[d],%[a],4), %[a]\n"
					"movd	(%[a]), %[y0]\n"       //mm0: [00000000AaRrGgBb]
					//"movd	mm1, i\n"                //mm1: [00000000AaRrGgBb]
					"pand	%[y6], %[y0]\n"        //zero alpha from screen pixel
					"punpcklbw	%[y7], %[y0]\n"    //mm0: [00Aa00Rr00Gg00Bb]
					//"punpcklbw	mm1, mm7\n"      //mm1: [00Aa00Rr00Gg00Bb]
					"psubw	%[y0], %[y1]\n"        //mm1: [--Aa--Rr--Gg--Bb] range:+-255
					"psllw	$4, %[y1]\n"           //mm1: [-Aa0-Rr0-Gg0-Bb0]
					"pshufw	$0xff, %[y1], %[y2]\n" //mm2: [-Aa0-Aa0-Aa0-Aa0]
					"pmulhw	%[y2], %[y1]\n"
					//"mov	edx, a\n"                //alphalookup[i] = i*0x001000100010;
					//"pmulhw	mm1, alphalookup[edx*8]\n"
					"paddw	%[y1], %[y0]\n"
					"packuswb	%[y0], %[y0]\n"
					"movd	%[y0], (%[a])\n"
					: [y0] "+y" (reg0), [y1] "+y" (reg1),
					  [y2] "+y" (reg2),
					  [y6] "+y" (reg6), [y7] "+y" (reg7)
					: [a] "r" (x), [d] "r" (p)
					:
				);
				#endif
				#ifdef _MSC_VER //msvc inline asm
				_asm
				{
					mov eax, x            //mm0 = (mm1-mm0)*a + mm0
					mov edx, p
					lea eax, [eax*4+edx]
					movd mm0, [eax]       //mm0: [00000000AaRrGgBb]
					//movd mm1, i           //mm1: [00000000AaRrGgBb]
					pand mm0, mm6         //zero alpha from screen pixel
					punpcklbw mm0, mm7    //mm0: [00Aa00Rr00Gg00Bb]
					//punpcklbw mm1, mm7    //mm1: [00Aa00Rr00Gg00Bb]
					psubw mm1, mm0        //mm1: [--Aa--Rr--Gg--Bb] range:+-255
					psllw mm1, 4          //mm1: [-Aa0-Rr0-Gg0-Bb0]
					pshufw mm2, mm1, 0xff //mm2: [-Aa0-Aa0-Aa0-Aa0]
					pmulhw mm1, mm2
					//mov edx, a            //alphalookup[i] = i*0x001000100010;
					//pmulhw mm1, alphalookup[edx*8]
					paddw mm0, mm1
					packuswb mm0, mm0
					movd [eax], mm0
				}
				#endif
			}
		}
		clearMMX();
	}
}
/**
 * Draw a 2d line on the screen
 */
void drawline2d (float x1, float y1, float x2, float y2, long col)
{
	float dx, dy, fxresm1, fyresm1;
	long i, j, incr, ie;

	dx = x2-x1; dy = y2-y1; if ((dx == 0) && (dy == 0)) return;
	fxresm1 = (float)xres_voxlap-.5; fyresm1 = (float)yres_voxlap-.5;
	if (x1 >= fxresm1) { if (x2 >= fxresm1) return; y1 += (fxresm1-x1)*dy/dx; x1 = fxresm1; }
	else if (x1 < 0) { if (x2 < 0) return; y1 += (0-x1)*dy/dx; x1 = 0; }
	if (x2 >= fxresm1) { y2 += (fxresm1-x2)*dy/dx; x2 = fxresm1; }
	else if (x2 < 0) { y2 += (0-x2)*dy/dx; x2 = 0; }
	if (y1 >= fyresm1) { if (y2 >= fyresm1) return; x1 += (fyresm1-y1)*dx/dy; y1 = fyresm1; }
	else if (y1 < 0) { if (y2 < 0) return; x1 += (0-y1)*dx/dy; y1 = 0; }
	if (y2 >= fyresm1) { x2 += (fyresm1-y2)*dx/dy; y2 = fyresm1; }
	else if (y2 < 0) { x2 += (0-y2)*dx/dy; y2 = 0; }

	if (fabs(dx) >= fabs(dy))
	{
		if (x2 > x1) { ftol(x1,&i); ftol(x2,&ie); } else { ftol(x2,&i); ftol(x1,&ie); }
		if (i < 0) i = 0; if (ie >= xres_voxlap) ie = xres_voxlap-1;
		ftol(1048576.0*dy/dx,&incr); ftol(y1*1048576.0+((float)i+.5f-x1)*incr,&j);
		for(;i<=ie;i++,j+=incr)
			if ((unsigned long)(j>>20) < (unsigned long)yres_voxlap)
				*(long *)(ylookup[j>>20]+(i<<2)+frameplace) = col;
	}
	else
	{
		if (y2 > y1) { ftol(y1,&i); ftol(y2,&ie); } else { ftol(y2,&i); ftol(y1,&ie); }
		if (i < 0) i = 0; if (ie >= yres_voxlap) ie = yres_voxlap-1;
		ftol(1048576.0*dx/dy,&incr); ftol(x1*1048576.0+((float)i+.5f-y1)*incr,&j);
		for(;i<=ie;i++,j+=incr)
			if ((unsigned long)(j>>20) < (unsigned long)xres_voxlap)
				*(long *)(ylookup[i]+((j>>18)&~3)+frameplace) = col;
	}
}

#if (USEZBUFFER == 1)
void drawline2dclip (float x1, float y1, float x2, float y2, float rx0, float ry0, float rz0, float rx1, float ry1, float rz1, long col)
{
	float dx, dy, fxresm1, fyresm1, Za, Zb, Zc, z;
	long i, j, incr, ie, p;

	dx = x2-x1; dy = y2-y1; if ((dx == 0) && (dy == 0)) return;
	fxresm1 = (float)xres_voxlap-.5; fyresm1 = (float)yres_voxlap-.5;
	if (x1 >= fxresm1) { if (x2 >= fxresm1) return; y1 += (fxresm1-x1)*dy/dx; x1 = fxresm1; }
	else if (x1 < 0) { if (x2 < 0) return; y1 += (0-x1)*dy/dx; x1 = 0; }
	if (x2 >= fxresm1) { y2 += (fxresm1-x2)*dy/dx; x2 = fxresm1; }
	else if (x2 < 0) { y2 += (0-x2)*dy/dx; x2 = 0; }
	if (y1 >= fyresm1) { if (y2 >= fyresm1) return; x1 += (fyresm1-y1)*dx/dy; y1 = fyresm1; }
	else if (y1 < 0) { if (y2 < 0) return; x1 += (0-y1)*dx/dy; y1 = 0; }
	if (y2 >= fyresm1) { x2 += (fyresm1-y2)*dx/dy; y2 = fyresm1; }
	else if (y2 < 0) { x2 += (0-y2)*dx/dy; y2 = 0; }

	if (fabs(dx) >= fabs(dy))
	{
			//Original equation: (rz1*t+rz0) / (rx1*t+rx0) = gihz/(sx-gihx)
		Za = gihz*(rx0*rz1 - rx1*rz0); Zb = rz1; Zc = -gihx*rz1 - gihz*rx1;

		if (x2 > x1) { ftol(x1,&i); ftol(x2,&ie); } else { ftol(x2,&i); ftol(x1,&ie); }
		if (i < 0) i = 0; if (ie >= xres_voxlap) ie = xres_voxlap-1;
		ftol(1048576.0*dy/dx,&incr); ftol(y1*1048576.0+((float)i+.5f-x1)*incr,&j);
		for(;i<=ie;i++,j+=incr)
			if ((unsigned long)(j>>20) < (unsigned long)yres_voxlap)
			{
				p = ylookup[j>>20]+(i<<2)+frameplace;
				z = Za / ((float)i*Zb + Zc);
				if (*(long *)&z >= *(long *)(p+zbufoff)) continue;
				*(long *)(p+zbufoff) = *(long *)&z;
				*(long *)p = col;
			}
	}
	else
	{
		Za = gihz*(ry0*rz1 - ry1*rz0); Zb = rz1; Zc = -gihy*rz1 - gihz*ry1;

		if (y2 > y1) { ftol(y1,&i); ftol(y2,&ie); } else { ftol(y2,&i); ftol(y1,&ie); }
		if (i < 0) i = 0; if (ie >= yres_voxlap) ie = yres_voxlap-1;
		ftol(1048576.0*dx/dy,&incr); ftol(x1*1048576.0+((float)i+.5f-y1)*incr,&j);
		for(;i<=ie;i++,j+=incr)
			if ((unsigned long)(j>>20) < (unsigned long)xres_voxlap)
			{
				p = ylookup[i]+((j>>18)&~3)+frameplace;
				z = Za / ((float)i*Zb + Zc);
				if (*(long *)&z >= *(long *)(p+zbufoff)) continue;
				*(long *)(p+zbufoff) = *(long *)&z;
				*(long *)p = col;
			}
	}
}
#endif
/**
 * Draw a 3d line on the screen (specified by VXL location).
 * Line is automatically Z-buffered into the map.
 * Set alpha of col to non-zero to disable Z-buffering
 */
void drawline3d (float x0, float y0, float z0, float x1, float y1, float z1, long col)
{
	float ox, oy, oz, r;

	ox = x0-gipos.x; oy = y0-gipos.y; oz = z0-gipos.z;
	x0 = ox*gistr.x + oy*gistr.y + oz*gistr.z;
	y0 = ox*gihei.x + oy*gihei.y + oz*gihei.z;
	z0 = ox*gifor.x + oy*gifor.y + oz*gifor.z;

	ox = x1-gipos.x; oy = y1-gipos.y; oz = z1-gipos.z;
	x1 = ox*gistr.x + oy*gistr.y + oz*gistr.z;
	y1 = ox*gihei.x + oy*gihei.y + oz*gihei.z;
	z1 = ox*gifor.x + oy*gifor.y + oz*gifor.z;

	if (z0 < SCISDIST)
	{
		if (z1 < SCISDIST) return;
		r = (SCISDIST-z0)/(z1-z0); z0 = SCISDIST;
		x0 += (x1-x0)*r; y0 += (y1-y0)*r;
	}
	else if (z1 < SCISDIST)
	{
		r = (SCISDIST-z1)/(z1-z0); z1 = SCISDIST;
		x1 += (x1-x0)*r; y1 += (y1-y0)*r;
	}

	ox = gihz/z0;
	oy = gihz/z1;

#if (USEZBUFFER == 1)
	if (!(col&0xff000000))
		drawline2dclip(x0*ox+gihx,y0*ox+gihy,x1*oy+gihx,y1*oy+gihy,x0,y0,z0,x1-x0,y1-y0,z1-z0,col);
	else
		drawline2d(x0*ox+gihx,y0*ox+gihy,x1*oy+gihx,y1*oy+gihy,col&0xffffff);
#else
	drawline2d(x0*ox+gihx,y0*ox+gihy,x1*oy+gihx,y1*oy+gihy,col);
#endif
}
/**
 * Draw a solid-color filled sphere on screen (useful for particle effects)
 * @param ox center of sphere
 * @param oy center of sphere
 * @param oz center of sphere
 * @param bakrad radius of sphere. NOTE: if bakrad is negative, it uses
 *               Z-buffering with abs(radius), otherwise no Z-buffering
 * @param col 32-bit color of sphere
 */
void drawspherefill (float ox, float oy, float oz, float bakrad, long col)
{
	float a, b, c, d, e, f, g, h, t, cxcx, cycy, Za, Zb, Zc, ysq;
	float r2a, rr2a, nb, nbi, isq, isqi, isqii, cx, cy, cz, rad;
	long sx1, sy1, sx2, sy2, p, sx;

	rad = fabs(bakrad);
#if (USEZBUFFER == 0)
	bakrad = rad;
#endif

	ox -= gipos.x; oy -= gipos.y; oz -= gipos.z;
	cz = ox*gifor.x + oy*gifor.y + oz*gifor.z; if (cz < SCISDIST) return;
	cx = ox*gistr.x + oy*gistr.y + oz*gistr.z;
	cy = ox*gihei.x + oy*gihei.y + oz*gihei.z;

		//3D Sphere projection (see spherast.txt for derivation) (13 multiplies)
	cxcx = cx*cx; cycy = cy*cy; g = rad*rad - cxcx - cycy - cz*cz;
	a = g + cxcx; if (!a) return;
	b = cx*cy; b += b;
	c = g + cycy;
	f = gihx*cx + gihy*cy - gihz*cz;
	d = -cx*f - gihx*g; d += d;
	e = -cy*f - gihy*g; e += e;
	f = f*f + g*(gihx*gihx+gihy*gihy+gihz*gihz);

		//isq = (b*b-4*a*c)yý + (2*b*d-4*a*e)y + (d*d-4*a*f) = 0
	Za = b*b - a*c*4; if (!Za) return;
	Zb = b*d*2 - a*e*4;
	Zc = d*d - a*f*4;
	ysq = Zb*Zb - Za*Zc*4; if (ysq <= 0) return;
	t = sqrt(ysq); //fsqrtasm(&ysq,&t);
	h = .5f / Za;
	ftol((-Zb+t)*h,&sy1); if (sy1 < 0) sy1 = 0;
	ftol((-Zb-t)*h,&sy2); if (sy2 > yres_voxlap) sy2 = yres_voxlap;
	if (sy1 >= sy2) return;
	r2a = .5f / a; rr2a = r2a*r2a;
	nbi = -b*r2a; nb = nbi*(float)sy1-d*r2a;
	h = Za*(float)sy1; isq = ((float)sy1*(h+Zb)+Zc)*rr2a;
	isqi = (h+h+Za+Zb)*rr2a; isqii = Za*rr2a*2;

	p = ylookup[sy1]+frameplace;
	sy2 = ylookup[sy2]+frameplace;
#if (USEZBUFFER == 1)
	if ((*(long *)&bakrad) >= 0)
	{
#endif
		while (1)  //(a)xý + (b*y+d)x + (c*y*y+e*y+f) = 0
		{
			t = sqrt(isq); //fsqrtasm(&isq,&t);
			ftol(nb-t,&sx1); if (sx1 < 0) sx1 = 0;
			ftol(nb+t,&sx2);
			sx2 = MIN(sx2,xres_voxlap)-sx1;
			if (sx2 > 0) clearbuf((void *)((sx1<<2)+p),sx2,col);
			p += bytesperline; if (p >= sy2) return;
			isq += isqi; isqi += isqii; nb += nbi;
		}
#if (USEZBUFFER == 1)
	}
	else
	{     //Use Z-buffering

		if (ofogdist >= 0) //If fog enabled...
		{
			ftol(sqrt(ox*ox + oy*oy),&sx); //Use cylindrical x-y distance for fog
			if (sx > 2047) sx = 2047;
			sx = (long)(*(short *)&foglut[sx]);
			col = ((((( vx5.fogcol     &255)-( col     &255))*sx)>>15)    ) +
					((((((vx5.fogcol>> 8)&255)-((col>> 8)&255))*sx)>>15)<< 8) +
					((((((vx5.fogcol>>16)&255)-((col>>16)&255))*sx)>>15)<<16) + col;
		}

		while (1)  //(a)xý + (b*y+d)x + (c*y*y+e*y+f) = 0
		{
			t = sqrt(isq); //fsqrtasm(&isq,&t);
			ftol(nb-t,&sx1); if (sx1 < 0) sx1 = 0;
			ftol(nb+t,&sx2);
			if (sx2 > xres_voxlap) sx2 = xres_voxlap;
			for(sx=sx1;sx<sx2;sx++)
				if (*(long *)&cz < *(long *)(p+(sx<<2)+zbufoff))
				{
					*(long *)(p+(sx<<2)+zbufoff) = *(long *)&cz;
					*(long *)(p+(sx<<2)) = col;
				}
			sy1++;
			p += bytesperline; if (p >= sy2) return;
			isq += isqi; isqi += isqii; nb += nbi;
		}
	}
#endif
}

/**
 * Draw a texture-mapped quadrilateral to the screen. Drawpicinquad projects
 * the source texture with perspective into the 4 coordinates specified.
 *
 * @param rpic source pointer to top-left corner
 * @param rbpl source pitch (bytes per line)
 * @param rxsiz source dimensions of texture/frame
 * @param rysiz source dimensions of texture/frame
 *
 * @param wpic destination pointer to top-left corner
 * @param wbpl destination pitch (bytes per line)
 * @param wxsiz destination dimensions of texture/frame
 * @param wysiz destination dimensions of texture/frame
 *
 * @param x0 the 4 points of the quadrilateral in the destination texture/frame.
 *           The points must be in loop order.
 * @param x1 the 4 points of the quadrilateral in the destination texture/frame.
 *           The points must be in loop order.
 * @param x2 the 4 points of the quadrilateral in the destination texture/frame.
 *           The points must be in loop order.
 * @param x3 the 4 points of the quadrilateral in the destination texture/frame.
 *           The points must be in loop order.
 *
 * @param y0 the 4 points of the quadrilateral in the destination texture/frame.
 *           The points must be in loop order.
 * @param y1 the 4 points of the quadrilateral in the destination texture/frame.
 *           The points must be in loop order.
 * @param y2 the 4 points of the quadrilateral in the destination texture/frame.
 *           The points must be in loop order.
 * @param y3 the 4 points of the quadrilateral in the destination texture/frame.
 *           The points must be in loop order.
 */
void drawpicinquad (long rpic, long rbpl, long rxsiz, long rysiz,
						  long wpic, long wbpl, long wxsiz, long wysiz,
						  float x0, float y0, float x1, float y1,
						  float x2, float y2, float x3, float y3)
{
	float px[4], py[4], k0, k1, k2, k3, k4, k5, k6, k7, k8;
	float t, u, v, dx, dy, l0, l1, m0, m1, m2, n0, n1, n2, r;
	long i, j, k, l, imin, imax, sx, sxe, sy, sy1, dd, uu, vv, ddi, uui, vvi;
	long x, xi, *p, *pe, uvmax, iu, iv;

	#if defined(__GNUC__) && !defined(NOASM) //only for gcc inline asm
	register lpoint2d reg0 asm("mm0");
	register lpoint2d reg1 asm("mm1");
	register lpoint2d reg2 asm("mm2");
	//register lpoint2d reg3 asm("mm3");
	register lpoint2d reg4 asm("mm4");
	register lpoint2d reg5 asm("mm5");
	register lpoint2d reg6 asm("mm6");
	register lpoint2d reg7 asm("mm7");
	#endif

	px[0] = x0; px[1] = x1; px[2] = x2; px[3] = x3;
	py[0] = y0; py[1] = y1; py[2] = y2; py[3] = y3;

		//This code projects 4 point2D's into a t,u,v screen-projection matrix
		//
		//Derivation: (given 4 known (sx,sy,kt,ku,kv) pairs, solve for k0-k8)
		//   kt = k0*sx + k1*sy + k2
		//   ku = k3*sx + k4*sy + k5
		//   kv = k6*sx + k7*sy + k8
		//0 = (k3*x0 + k4*y0 + k5) / (k0*x0 + k1*y0 + k2) / rxsiz
		//0 = (k6*x0 + k7*y0 + k8) / (k0*x0 + k1*y0 + k2) / rysiz
		//1 = (k3*x1 + k4*y1 + k5) / (k0*x1 + k1*y1 + k2) / rxsiz
		//0 = (k6*x1 + k7*y1 + k8) / (k0*x1 + k1*y1 + k2) / rysiz
		//1 = (k3*x2 + k4*y2 + k5) / (k0*x2 + k1*y2 + k2) / rxsiz
		//1 = (k6*x2 + k7*y2 + k8) / (k0*x2 + k1*y2 + k2) / rysiz
		//0 = (k3*x3 + k4*y3 + k5) / (k0*x3 + k1*y3 + k2) / rxsiz
		//1 = (k6*x3 + k7*y3 + k8) / (k0*x3 + k1*y3 + k2) / rysiz
		//   40*, 28+, 1~, 30W
	k3 = y3 - y0; k4 = x0 - x3; k5 = x3*y0 - x0*y3;
	k6 = y0 - y1; k7 = x1 - x0; k8 = x0*y1 - x1*y0;
	n0 = x2*y3 - x3*y2; n1 = x3*y1 - x1*y3; n2 = x1*y2 - x2*y1;
	l0 = k6*x2 + k7*y2 + k8;
	l1 = k3*x2 + k4*y2 + k5;
	t = n0 + n1 + n2; dx = (float)rxsiz*t*l0; dy = (float)rysiz*t*l1;
	t = l0*l1;
	l0 *= (k3*x1 + k4*y1 + k5);
	l1 *= (k6*x3 + k7*y3 + k8);
	m0 = l1 - t; m1 = l0 - l1; m2 = t - l0;
	k0 = m0*y1 + m1*y2 + m2*y3;
	k1 = -(m0*x1 + m1*x2 + m2*x3);
	k2 = n0*l0 + n1*t + n2*l1;
	k3 *= dx; k4 *= dx; k5 *= dx;
	k6 *= dy; k7 *= dy; k8 *= dy;

		//Make sure k's are in good range for conversion to integers...
	t = fabs(k0);
	if (fabs(k1) > t) t = fabs(k1);
	if (fabs(k2) > t) t = fabs(k2);
	if (fabs(k3) > t) t = fabs(k3);
	if (fabs(k4) > t) t = fabs(k4);
	if (fabs(k5) > t) t = fabs(k5);
	if (fabs(k6) > t) t = fabs(k6);
	if (fabs(k7) > t) t = fabs(k7);
	if (fabs(k8) > t) t = fabs(k8);
	t = -268435456.0 / t;
	k0 *= t; k1 *= t; k2 *= t;
	k3 *= t; k4 *= t; k5 *= t;
	k6 *= t; k7 *= t; k8 *= t;
	ftol(k0,&ddi);

	imin = 0; imax = 0;
	for(i=1;i<4;i++)
	{
		if (py[i] < py[imin]) imin = i;
		if (py[i] > py[imax]) imax = i;
	}

	uvmax = (rysiz-1)*rbpl + (rxsiz<<2);

	i = imax;
	do
	{
		j = ((i+1)&3);
			//offset would normally be -.5, but need to bias by +1.0
		ftol(py[j]+.5,&sy); if (sy < 0) sy = 0;
		ftol(py[i]+.5,&sy1); if (sy1 > wysiz) sy1 = wysiz;
		if (sy1 > sy)
		{
			ftol((px[i]-px[j])*4096.0/(py[i]-py[j]),&xi);
			ftol(((float)sy-py[j])*(float)xi + px[j]*4096.0 + 4096.0,&x);
			for(;sy<sy1;sy++,x+=xi) lastx[sy] = (x>>12);
		}
		i = j;
	} while (i != imin);
	do
	{
		j = ((i+1)&3);
			//offset would normally be -.5, but need to bias by +1.0
		ftol(py[i]+.5,&sy); if (sy < 0) sy = 0;
		ftol(py[j]+.5,&sy1); if (sy1 > wysiz) sy1 = wysiz;
		if (sy1 > sy)
		{
			ftol((px[j]-px[i])*4096.0/(py[j]-py[i]),&xi);
			ftol(((float)sy-py[i])*(float)xi + px[i]*4096.0 + 4096.0,&x);
			for(;sy<sy1;sy++,x+=xi)
			{
				sx = lastx[sy]; if (sx < 0) sx = 0;
				sxe = (x>>12); if (sxe > wxsiz) sxe = wxsiz;
				if (sx >= sxe) continue;
				t = k0*(float)sx + k1*(float)sy + k2; r = 1.0 / t;
				u = k3*(float)sx + k4*(float)sy + k5; ftol(u*r-.5,&iu);
				v = k6*(float)sx + k7*(float)sy + k8; ftol(v*r-.5,&iv);
				ftol(t,&dd);
				ftol((float)iu*k0 - k3,&uui); ftol((float)iu*t - u,&uu);
				ftol((float)iv*k0 - k6,&vvi); ftol((float)iv*t - v,&vv);
				if (k3*t < u*k0) k =    -4; else { uui = -(uui+ddi); uu = -(uu+dd); k =    4; }
				if (k6*t < v*k0) l = -rbpl; else { vvi = -(vvi+ddi); vv = -(vv+dd); l = rbpl; }
				iu = iv*rbpl + (iu<<2);
				p  = (long *)(sy*wbpl+(sx<<2)+wpic);
				pe = (long *)(sy*wbpl+(sxe<<2)+wpic);
				do
				{
					if ((unsigned long)iu < uvmax) p[0] = *(long *)(rpic+iu);
					dd += ddi;
					uu += uui; while (uu < 0) { iu += k; uui -= ddi; uu -= dd; }
					vv += vvi; while (vv < 0) { iu += l; vvi -= ddi; vv -= dd; }
					p++;
				} while (p < pe);
			}
		}
		i = j;
	} while (i != imax);
}

__ALIGN(16) static float dpqdistlut[MAXXDIM];
__ALIGN(16) static float dpqmulval[4] = {0,1,2,3}, dpqfour[4] = {4,4,4,4};
__ALIGN(8)  static float dpq3dn[4];
/** Draw a textured wuad using voxels, given 3 points (4th is calculated)
 *  and their UV coordinates, plus an optional pointer to a bitmap.
*/
void drawpolyquad (long rpic, long rbpl, long rxsiz, long rysiz,
				   float x0, float y0, float z0, float u0, float v0,
				   float x1, float y1, float z1, float u1, float v1,
				   float x2, float y2, float z2, float u2, float v2,
				   float x3, float y3, float z3)
{
	point3d fp, fp2;
	float px[6], py[6], pz[6], pu[6], pv[6], px2[4], py2[4], pz2[4], pu2[4], pv2[4];
	float f, t, u, v, r, nx, ny, nz, ox, oy, oz, scaler;
	float dx, dy, db, ux, uy, ub, vx, vy, vb;
	long i, j, k, l, imin, imax, sx, sxe, sy, sy1;
	long x, xi, *p, *pe, uvmax, iu, iv, n;
	long dd, uu, vv, ddi, uui, vvi, distlutoffs;

	#if defined(__GNUC__) && !defined(NOASM) //only for gcc inline asm
	//register lpoint2d reg0 asm("xmm0");
	//register lpoint2d reg1 asm("xmm1");
	//register lpoint2d reg2 asm("xmm2");
	//register lpoint2d reg3 asm("xmm3");
	//register lpoint2d reg4 asm("xmm4");
	//register lpoint2d reg5 asm("xmm5");
	register lpoint2d reg6 asm("xmm6");
	register lpoint2d reg7 asm("xmm7");
	#endif

	px2[0] = x0; py2[0] = y0; pz2[0] = z0; pu2[0] = u0; pv2[0] = v0;
	px2[1] = x1; py2[1] = y1; pz2[1] = z1; pu2[1] = u1; pv2[1] = v1;
	px2[2] = x2; py2[2] = y2; pz2[2] = z2; pu2[2] = u2; pv2[2] = v2;
	px2[3] = x3; py2[3] = y3; pz2[3] = z3;

		//Calculate U-V coordinate of 4th point on quad (based on 1st 3)
	nx = (y1-y0)*(z2-z0) - (z1-z0)*(y2-y0);
	ny = (z1-z0)*(x2-x0) - (x1-x0)*(z2-z0);
	nz = (x1-x0)*(y2-y0) - (y1-y0)*(x2-x0);
	if ((fabs(nx) > fabs(ny)) && (fabs(nx) > fabs(nz)))
	{     //(y1-y0)*u + (y2-y0)*v = (y3-y0)
			//(z1-z0)*u + (z2-z0)*v = (z3-z0)
		f = 1/nx;
		u = ((y3-y0)*(z2-z0) - (z3-z0)*(y2-y0))*f;
		v = ((y1-y0)*(z3-z0) - (z1-z0)*(y3-y0))*f;
	}
	else if (fabs(ny) > fabs(nz))
	{     //(x1-x0)*u + (x2-x0)*v = (x3-x0)
			//(z1-z0)*u + (z2-z0)*v = (z3-z0)
		f = -1/ny;
		u = ((x3-x0)*(z2-z0) - (z3-z0)*(x2-x0))*f;
		v = ((x1-x0)*(z3-z0) - (z1-z0)*(x3-x0))*f;
	}
	else
	{     //(x1-x0)*u + (x2-x0)*v = (x3-x0)
			//(y1-y0)*u + (y2-y0)*v = (y3-y0)
		f = 1/nz;
		u = ((x3-x0)*(y2-y0) - (y3-y0)*(x2-x0))*f;
		v = ((x1-x0)*(y3-y0) - (y1-y0)*(x3-x0))*f;
	}
	pu2[3] = (u1-u0)*u + (u2-u0)*v + u0;
	pv2[3] = (v1-v0)*u + (v2-v0)*v + v0;


	for(i=4-1;i>=0;i--) //rotation
	{
		fp.x = px2[i]-gipos.x; fp.y = py2[i]-gipos.y; fp.z = pz2[i]-gipos.z;
		px2[i] = fp.x*gistr.x + fp.y*gistr.y + fp.z*gistr.z;
		py2[i] = fp.x*gihei.x + fp.y*gihei.y + fp.z*gihei.z;
		pz2[i] = fp.x*gifor.x + fp.y*gifor.y + fp.z*gifor.z;
	}

		//Clip to SCISDIST plane
	n = 0;
	for(i=0;i<4;i++)
	{
		j = ((i+1)&3);
		if (pz2[i] >= SCISDIST) { px[n] = px2[i]; py[n] = py2[i]; pz[n] = pz2[i]; pu[n] = pu2[i]; pv[n] = pv2[i]; n++; }
		if ((pz2[i] >= SCISDIST) != (pz2[j] >= SCISDIST))
		{
			f = (SCISDIST-pz2[i])/(pz2[j]-pz2[i]);
			px[n] = (px2[j]-px2[i])*f + px2[i];
			py[n] = (py2[j]-py2[i])*f + py2[i];
			pz[n] = SCISDIST;
			pu[n] = (pu2[j]-pu2[i])*f + pu2[i];
			pv[n] = (pv2[j]-pv2[i])*f + pv2[i]; n++;
		}
	}
	if (n < 3) return;

	for(i=n-1;i>=0;i--) //projection
	{
		pz[i] = 1/pz[i]; f = pz[i]*gihz;
		px[i] = px[i]*f + gihx;
		py[i] = py[i]*f + gihy;
	}

		//General equations:
		//pz[i] = (px[i]*gdx + py[i]*gdy + gdo)
		//pu[i] = (px[i]*gux + py[i]*guy + guo)/pz[i]
		//pv[i] = (px[i]*gvx + py[i]*gvy + gvo)/pz[i]
		//
		//px[0]*gdx + py[0]*gdy + 1*gdo = pz[0]
		//px[1]*gdx + py[1]*gdy + 1*gdo = pz[1]
		//px[2]*gdx + py[2]*gdy + 1*gdo = pz[2]
		//
		//px[0]*gux + py[0]*guy + 1*guo = pu[0]*pz[0] (pu[i] premultiplied by pz[i] above)
		//px[1]*gux + py[1]*guy + 1*guo = pu[1]*pz[1]
		//px[2]*gux + py[2]*guy + 1*guo = pu[2]*pz[2]
		//
		//px[0]*gvx + py[0]*gvy + 1*gvo = pv[0]*pz[0] (pv[i] premultiplied by pz[i] above)
		//px[1]*gvx + py[1]*gvy + 1*gvo = pv[1]*pz[1]
		//px[2]*gvx + py[2]*gvy + 1*gvo = pv[2]*pz[2]
	pu[0] *= pz[0]; pu[1] *= pz[1]; pu[2] *= pz[2];
	pv[0] *= pz[0]; pv[1] *= pz[1]; pv[2] *= pz[2];
	ox = py[1]-py[2]; oy = py[2]-py[0]; oz = py[0]-py[1];
	r = 1.0 / (ox*px[0] + oy*px[1] + oz*px[2]);
	dx = (ox*pz[0] + oy*pz[1] + oz*pz[2])*r;
	ux = (ox*pu[0] + oy*pu[1] + oz*pu[2])*r;
	vx = (ox*pv[0] + oy*pv[1] + oz*pv[2])*r;
	ox = px[2]-px[1]; oy = px[0]-px[2]; oz = px[1]-px[0];
	dy = (ox*pz[0] + oy*pz[1] + oz*pz[2])*r;
	uy = (ox*pu[0] + oy*pu[1] + oz*pu[2])*r;
	vy = (ox*pv[0] + oy*pv[1] + oz*pv[2])*r;
	db = pz[0] - px[0]*dx - py[0]*dy;
	ub = pu[0] - px[0]*ux - py[0]*uy;
	vb = pv[0] - px[0]*vx - py[0]*vy;

#if 1
		//Make sure k's are in good range for conversion to integers...
	t = fabs(ux);
	if (fabs(uy) > t) t = fabs(uy);
	if (fabs(ub) > t) t = fabs(ub);
	if (fabs(vx) > t) t = fabs(vx);
	if (fabs(vy) > t) t = fabs(vy);
	if (fabs(vb) > t) t = fabs(vb);
	if (fabs(dx) > t) t = fabs(dx);
	if (fabs(dy) > t) t = fabs(dy);
	if (fabs(db) > t) t = fabs(db);
	scaler = -268435456.0 / t;
	ux *= scaler; uy *= scaler; ub *= scaler;
	vx *= scaler; vy *= scaler; vb *= scaler;
	dx *= scaler; dy *= scaler; db *= scaler;
	ftol(dx,&ddi);
	uvmax = (rysiz-1)*rbpl + (rxsiz<<2);

	scaler = 1.f/scaler; t = dx*scaler;
	if (cputype&(1<<25))
	{
		#if defined( __GNUC__ )
		__asm__ __volatile__ //SSE
		(
			                            //xmm6: -,-,-,dx*scaler
			"shufps	$0, %[x6], %[x6]\n" //xmm6: dx*scaler,dx*scaler,dx*scaler,dx*scaler
			"movaps	%[x6], %[x7]\n"     //xmm7: dx*scaler,dx*scaler,dx*scaler,dx*scaler
			"mulps	%c[mulv], %[x6]\n"  //xmm6: dx*scaler*3,dx*scaler*2,dx*scaler*1,0
			"mulps	%c[four], %[x6]\n"  //xmm7: dx*scaler*4,dx*scaler*4,dx*scaler*4,dx*scaler*4
			:     "=x" (reg6), [x7] "=x" (reg7)
			: [x6] "0" (t),
			  [mulv] "p" (&dpqmulval), [four] "p" (&dpqfour)
			:
		);
		#elif defined(_MSC_VER)
		_asm //SSE
		{
			movss	xmm6, t         //xmm6: -,-,-,dx*scaler
			shufps	xmm6, xmm6, 0   //xmm6: dx*scaler,dx*scaler,dx*scaler,dx*scaler
			movaps	xmm7, xmm6      //xmm7: dx*scaler,dx*scaler,dx*scaler,dx*scaler
			mulps	xmm6, dpqmulval //xmm6: dx*scaler*3,dx*scaler*2,dx*scaler*1,0
			mulps	xmm7, dpqfour   //xmm7: dx*scaler*4,dx*scaler*4,dx*scaler*4,dx*scaler*4
		}
		#endif
	}
	else { dpq3dn[0] = 0; dpq3dn[1] = t; dpq3dn[2] = dpq3dn[3] = t+t; } //3DNow!
#endif

	imin = (py[1]<py[0]); imax = 1-imin;
	for(i=n-1;i>1;i--)
	{
		if (py[i] < py[imin]) imin = i;
		if (py[i] > py[imax]) imax = i;
	}

	i = imax;
	do
	{
		j = i+1; if (j >= n) j = 0;
			//offset would normally be -.5, but need to bias by +1.0
		ftol(py[j]+.5,&sy); if (sy < 0) sy = 0;
		ftol(py[i]+.5,&sy1); if (sy1 > yres_voxlap) sy1 = yres_voxlap;
		if (sy1 > sy)
		{
			ftol((px[i]-px[j])*4096.0/(py[i]-py[j]),&xi);
			ftol(((float)sy-py[j])*(float)xi + px[j]*4096.0 + 4096.0,&x);
			for(;sy<sy1;sy++,x+=xi) lastx[sy] = (x>>12);
		}
		i = j;
	} while (i != imin);
	do
	{
		j = i+1; if (j >= n) j = 0;
			//offset would normally be -.5, but need to bias by +1.0
		ftol(py[i]+.5,&sy); if (sy < 0) sy = 0;
		ftol(py[j]+.5,&sy1); if (sy1 > yres_voxlap) sy1 = yres_voxlap;
		if (sy1 > sy)
		{
			ftol((px[j]-px[i])*4096.0/(py[j]-py[i]),&xi);
			ftol(((float)sy-py[i])*(float)xi + px[i]*4096.0 + 4096.0,&x);
			for(;sy<sy1;sy++,x+=xi)
			{
				sx = lastx[sy]; if (sx < 0) sx = 0;
				sxe = (x>>12); if (sxe > xres_voxlap) sxe = xres_voxlap;
				if (sx >= sxe) continue;
				p  = (long *)(sy*bytesperline+(sx<<2)+frameplace);
				pe = (long *)(sy*bytesperline+(sxe<<2)+frameplace);
			#if 0
					//Brute force
				do
				{
					f = 1.f/(dx*(float)sx + dy*(float)sy + db);
					if (f < *(float *)(((long)p)+zbufoff))
					{
						*(float *)(((long)p)+zbufoff) = f;
						ftol((ux*(float)sx + uy*(float)sy + ub)*f-.5,&iu);
						ftol((vx*(float)sx + vy*(float)sy + vb)*f-.5,&iv);
						if ((unsigned long)iu >= rxsiz) iu = 0;
						if ((unsigned long)iv >= rysiz) iv = 0;
						p[0] = *(long *)(iv*rbpl+(iu<<2)+rpic);
					}
					p++; sx++;
				} while (p < pe);
			#else
					//Optimized (in C) hyperbolic texture-mapping (Added Z-buffer using SSE/3DNow! for recip's)
				t = dx*(float)sx + dy*(float)sy + db; r = 1.0 / t;
				u = ux*(float)sx + uy*(float)sy + ub; ftol(u*r-.5,&iu);
				v = vx*(float)sx + vy*(float)sy + vb; ftol(v*r-.5,&iv);
				ftol(t,&dd);
				ftol((float)iu*dx - ux,&uui); ftol((float)iu*t - u,&uu);
				ftol((float)iv*dx - vx,&vvi); ftol((float)iv*t - v,&vv);
				if (ux*t < u*dx) k =    -4; else { uui = -(uui+ddi); uu = -(uu+dd); k =    4; }
				if (vx*t < v*dx) l = -rbpl; else { vvi = -(vvi+ddi); vv = -(vv+dd); l = rbpl; }
				iu = iv*rbpl + (iu<<2);

				t *= scaler;

				if (cputype&(1<<25)) {
					#ifdef __GNUC__ //gcc inline asm
					__asm__ __volatile__
					(
						"sub	%[c], %[a]\n"
						"add	%[distlut], %[c]\n" //distlut not deref

						//dd+ddi*3 dd+ddi*2 dd+ddi*1 dd+ddi*0
						"shufps	$0, %[x0], %[x0]\n"
						"addps	%[x6], %[x0]\n"
					".Ldpqbegsse:\n"
						"rcpps	%[x0], %[x1]\n"
						"addps  %[x7], %[x0]\n"
						"movaps	%[x1], (%[a],%[c])\n"
						"add	$16, %[a]\n"
						"jl	.Ldpqbegsse\n"
						"femms\n"
						:
						: [a] "r" (0), [c] "r" (4*(sxe - sx)),
						  [distlut] "p" (&dpqdistlut),
						  [x0] "x" (t), [x1] "x" (0),
						  [x6] "x" (reg6), [x7] "x" (reg7)
						:
					);
					#endif
					#ifdef _MSC_VER //msvc inline asm
					_asm
					{
						mov	ecx, sxe
						sub	ecx, sx
						xor	eax, eax
						lea	ecx, [ecx*4]
						sub	eax, ecx
						add	ecx, offset dpqdistlut

						movss	xmm0, t //dd+ddi*3 dd+ddi*2 dd+ddi*1 dd+ddi*0
						shufps	xmm0, xmm0, 0
						addps	xmm0, xmm6
					dpqbegsse:
						rcpps	xmm1, xmm0
						addps	xmm0, xmm7
						movaps	[eax+ecx], xmm1
						add	eax, 16
						jl	short dpqbegsse
						femms
					}
					#endif
				}
				else
				{
					#ifdef __GNUC__ //gcc inline asm
					__asm__ __volatile__
					(
						"sub	%[c], %[a]\n"
						"add	%[distlut], %[c]\n" //distlut not deref

						"punpckldq	%[y0], %[y0]\n"
						"pfadd	%c[dpq],%[y0]\n"
					".Ldpqbeg3dn:\n"
						"pswapd	%[y0], %[y2]\n"
						"pfrcp	%[y0], %[y1]\n"     //mm1: 1/mm0l 1/mm0l
						"pfrcp	%[y2], %[y2]\n"     //mm2: 1/mm0h 1/mm0h
						"punpckldq	%[y2], %[y1]\n" //mm1: 1/mm0h 1/mm0l
						"pfadd	%[y7], %[y0]\n"
						"movq	%[y0], (%[a],%[c])\n"
						"add	$8, %[a]\n"
						"jl	.Ldpqbeg3dn\n"
						"femms\n"
						:
						: [a] "r" (0), [c] "r" (4*(sxe - sx)),
						  [distlut] "p" (&dpqdistlut), [dpq] "p" (&dpq3dn), //(lpoint2d*)
						  [y1] "y" (0), [y2] "y" (0),
						  [y0] "y" (t), [y7] "y" (dpq3dn[8])
						:
					);
					#endif
					#ifdef _MSC_VER //msvc inline asm
					_asm
					{
						mov	ecx, sxe
						sub	ecx, sx
						xor	eax, eax
						lea	ecx, [ecx*4]
						sub	eax, ecx
						add	ecx, offset dpqdistlut

						movd	mm0, t           //dd+ddi*1 dd+ddi*0
						punpckldq	mm0, mm0
						pfadd	mm0, dpq3dn[0]
						movq	mm7, dpq3dn[8]
					dpqbeg3dn:
						pswapd	mm2, mm0
						pfrcp	mm1, mm0         //mm1: 1/mm0l 1/mm0l
						pfrcp	mm2, mm2         //mm2: 1/mm0h 1/mm0h
						punpckldq	mm1, mm2     //mm1: 1/mm0h 1/mm0l
						pfadd	mm0, mm7
						movq	[eax+ecx], mm1
						add	eax, 8
						jl	short dpqbeg3dn
						femms
					}
					#endif
				}

				distlutoffs = ((long)dpqdistlut)-((long)p);
				do
				{
				#if (USEZBUFFER != 0)
					if (*(long *)(((long)p)+zbufoff) > *(long *)(((long)p)+distlutoffs))
					{
						*(long *)(((long)p)+zbufoff) = *(long *)(((long)p)+distlutoffs);
				#endif
						if ((unsigned long)iu < uvmax) p[0] = *(long *)(rpic+iu);
				#if (USEZBUFFER != 0)
					}
				#endif
					dd += ddi;
					uu += uui; while (uu < 0) { iu += k; uui -= ddi; uu -= dd; }
					vv += vvi; while (vv < 0) { iu += l; vvi -= ddi; vv -= dd; }
					p++;
				} while (p < pe);
			#endif
			}
		}
		i = j;
	} while (i != imax);
}

//------------------------- SXL parsing code begins --------------------------

static char *sxlbuf = 0;
static long sxlparspos, sxlparslen;

/**
 * Loads and begins parsing of an .SXL file. Always call this first before
 * using parspr().
 * @param sxlnam .SXL filename
 * @param vxlnam pointer to .VXL filename (written by loadsxl)
 * @param vxlnam pointer to .SKY filename (written by loadsxl)
 * @param globst pointer to global user string. You parse this yourself!
 *               You can edit this in Voxed by pressing F6.
 * @return 0: loadsxl failed (file not found or malloc failed)
 *         1: loadsxl successful; call parspr()!
 */
long loadsxl (const char *sxlnam, char **vxlnam, char **skynam, char **globst)
{
	long j, k, m, n;

		//NOTE: MUST buffer file because insertsprite uses kz file code :/
	if (!kzopen(sxlnam)) return(0);
	sxlparslen = kzfilelength();
	if (sxlbuf) { free(sxlbuf); sxlbuf = 0; }
	if (!(sxlbuf = (char *)malloc(sxlparslen))) return(0);
	kzread(sxlbuf,sxlparslen);
	kzclose();

	j = n = 0;

		//parse vxlnam
	(*vxlnam) = &sxlbuf[j];
	while ((sxlbuf[j]!=13)&&(sxlbuf[j]!=10) && (j < sxlparslen)) j++; sxlbuf[j++] = 0;
	while (((sxlbuf[j]==13)||(sxlbuf[j]==10)) && (j < sxlparslen)) j++;

		//parse skynam
	(*skynam) = &sxlbuf[j];
	while ((sxlbuf[j]!=13)&&(sxlbuf[j]!=10) && (j < sxlparslen)) j++; sxlbuf[j++] = 0;
	while (((sxlbuf[j]==13)||(sxlbuf[j]==10)) && (j < sxlparslen)) j++;

		//parse globst
	m = n = j; (*globst) = &sxlbuf[n];
	while (((sxlbuf[j] == ' ') || (sxlbuf[j] == 9)) && (j < sxlparslen))
	{
		j++;
		while ((sxlbuf[j]!=13) && (sxlbuf[j]!=10) && (j < sxlparslen)) sxlbuf[n++] = sxlbuf[j++];
		sxlbuf[n++] = 13; j++;
		while (((sxlbuf[j]==13) || (sxlbuf[j]==10)) && (j < sxlparslen)) j++;
	}
	if (n > m) sxlbuf[n-1] = 0; else (*globst) = &nullst;

		//Count number of sprites in .SXL file (helpful for GAME)
	sxlparspos = j;
	return(1);
}

/**
 * If loadsxl returns a 1, then you should call parspr with a while loop
 * that terminates when the return value is 0.
 * @param spr pointer to sprite structure (written by parspr) You allocate
 *            the vx5sprite, & parspr fills in the position&orientation.
 * @param userst pointer to user string associated with the given sprite.
 *               You can edit this in Voxed by right-clicking the sprite.
 * @return pointer to .KV6 filename OR NULL if no more sprites left.
 *         You must load the .KV6 to memory yourself by doing:
 *         char *kv6filename = parspr(...)
 *         if (kv6filename) spr->voxnum = getkv6(kv6filename);
 */
char *parspr (vx5sprite *spr, char **userst)
{
	float f;
	long j, k, m, n;
	char *namptr;

	j = sxlparspos; //unnecessary temp variable (to shorten code)

		//Automatically free temp sxlbuf when done reading sprites
	if (((j+2 < sxlparslen) && (sxlbuf[j] == 'e') && (sxlbuf[j+1] == 'n') && (sxlbuf[j+2] == 'd') &&
		((j+3 == sxlparslen) || (sxlbuf[j+3] == 13) || (sxlbuf[j+3] == 10))) || (j > sxlparslen))
		return(0);

		//parse kv6name
	for(k=j;(sxlbuf[k]!=',') && (k < sxlparslen);k++); sxlbuf[k] = 0;
	namptr = &sxlbuf[j]; j = k+1;

		//parse 12 floats
	for(m=0;m<12;m++)
	{
		if (m < 11) { for(k=j;(sxlbuf[k]!=',') && (k < sxlparslen);k++); }
		else { for(k=j;(sxlbuf[k]!=13) && (sxlbuf[k]!=10) && (k < sxlparslen);k++); }

		sxlbuf[k] = 0; f = atof(&sxlbuf[j]); j = k+1;
		switch(m)
		{
			case  0: spr->p.x = f; break;
			case  1: spr->p.y = f; break;
			case  2: spr->p.z = f; break;
			case  3: spr->s.x = f; break;
			case  4: spr->s.y = f; break;
			case  5: spr->s.z = f; break;
			case  6: spr->h.x = f; break;
			case  7: spr->h.y = f; break;
			case  8: spr->h.z = f; break;
			case  9: spr->f.x = f; break;
			case 10: spr->f.y = f; break;
			case 11: spr->f.z = f; break;
			default: _gtfo(); //tells MSVC default can't be reached
		}
	}
	while (((sxlbuf[j]==13) || (sxlbuf[j]==10)) && (j < sxlparslen)) j++;

	spr->flags = 0;

		//parse userst
	m = n = j; (*userst) = &sxlbuf[n];
	while (((sxlbuf[j] == ' ') || (sxlbuf[j] == 9)) && (j < sxlparslen))
	{
		j++;
		while ((sxlbuf[j]!=13) && (sxlbuf[j]!=10) && (j < sxlparslen)) sxlbuf[n++] = sxlbuf[j++];
		sxlbuf[n++] = 13; j++;
		while (((sxlbuf[j]==13) || (sxlbuf[j]==10)) && (j < sxlparslen)) j++;
	}
	if (n > m) sxlbuf[n-1] = 0; else (*userst) = &nullst;

	sxlparspos = j; //unnecessary temp variable (for short code)
	return(namptr);
}

//--------------------------  Name hash code begins --------------------------

	//khashbuf format: (used by getkv6/getkfa to avoid duplicate loads)
	//[long index to next hash or -1][pointer to struct][char type]string[\0]
	//[long index to next hash or -1][pointer to struct][chat type]string[\0]
	//...
	//type:0 = kv6data
	//type:1 = kfatype
#define KHASHINITSIZE 8192
static char *khashbuf = 0;
static long khashead[256], khashpos = 0, khashsiz = 0;

/** Given a name associated with the kv6data/kfatype return a pointer
 * Notice that each structure has a "namoff" member. Since I
 * use remalloc(), I have to make these offsets, not pointers. Use this
 * function to convert the offsets into pointers.
 *
 * @param namoff offset to the name
 * @return a pointer to the filename associated with the kv6data/kfatype
 */
char *getkfilname (long namoff) { return(&khashbuf[namoff]); }

/** Uses a 256-entry hash to compare names very quickly.
 *  @return 2 values packed into a long
 *  0,retptr=-1: Error! (bad filename or out of memory)
 *	0,retptr>=0: Not in hash; new name allocated, valid index
 *	1,retptr>=0: Already in hash, valid index
*/
static long inkhash (const char *filnam, long *retind)
{
	long i, j, hashind;

	(*retind) = -1;

	if (!filnam) return(0);
	j = strlen(filnam); if (!j) return(0);
	j += 10;
	if (khashpos+j > khashsiz) //Make sure string fits in khashbuf
	{
		i = khashsiz; do { i <<= 1; } while (khashpos+j > i);
		if (!(khashbuf = (char *)realloc(khashbuf,i))) return(0);
		khashsiz = i;
	}

		//Copy filename to avoid destroying original string
		//Also, calculate hash index (which hopefully is uniformly random :)
	strcpy(&khashbuf[khashpos+9],filnam);
	for(i=khashpos+9,hashind=0;khashbuf[i];i++)
	{
		if ((khashbuf[i] >= 'a') && (khashbuf[i] <= 'z')) khashbuf[i] -= 32;
		if (khashbuf[i] == '/') khashbuf[i] = '\\';
		hashind = (khashbuf[i] - hashind*3);
	}
	hashind %= (sizeof(khashead)/sizeof(khashead[0]));

		//Find if string is already in hash...
	for(i=khashead[hashind];i>=0;i=(*(long *)&khashbuf[i]))
		if (!strcmp(&khashbuf[i+9],&khashbuf[khashpos+9]))
			{ (*retind) = i; return(1); } //Early out: already in hash

	(*retind) = khashpos;
	*(long *)&khashbuf[khashpos] = khashead[hashind];
	*(long *)&khashbuf[khashpos+4] = 0; //Set to 0 just in case load fails
	khashead[hashind] = khashpos; khashpos += j;
	return(0);
}

//-------------------------- KV6 sprite code begins --------------------------

//EQUIVEC code begins -----------------------------------------------------
point3d univec[256];
__ALIGN(8) short iunivec[256][4];

typedef struct
{
	float fibx[45], fiby[45];
	float azval[20], zmulk, zaddk;
	long fib[47], aztop, npoints;
} equivectyp;
static equivectyp equivec;

void equiind2vec (long i, float *x, float *y, float *z)
{
	float r;
	(*z) = (float)i*equivec.zmulk + equivec.zaddk; r = sqrt(1.f - (*z)*(*z));
	fcossin((float)i*(GOLDRAT*PI*2),x,y); (*x) *= r; (*y) *= r;
}

	//Very fast; good quality
long equivec2indmem (float x, float y, float z)
{
	long b, i, j, k, bestc;
	float xy, zz, md, d;

	xy = atan2(y,x); //atan2 is 150 clock cycles! True, but there are less acurate approximations or tables if needed
	j = ((*(long *)&z)&0x7fffffff);
	bestc = equivec.aztop;
	do
	{
		if (j < *(long *)&equivec.azval[bestc]) break;
		bestc--;
	} while (bestc);

	zz = z + 1.f;
	ftol(equivec.fibx[bestc]*xy + equivec.fiby[bestc]*zz - .5,&i);
	bestc++;
	ftol(equivec.fibx[bestc]*xy + equivec.fiby[bestc]*zz - .5,&j);

	k = dmulshr0(equivec.fib[bestc+2],i,equivec.fib[bestc+1],j);
	if ((unsigned long)k < equivec.npoints)
	{
		md = univec[k].x*x + univec[k].y*y + univec[k].z*z;
		j = k;
	} else md = -2.f;
	b = bestc+3;
	do
	{
		i = equivec.fib[b] + k;
		if ((unsigned long)i < equivec.npoints)
		{
			d = univec[i].x*x + univec[i].y*y + univec[i].z*z;
			if (*(long *)&d > *(long *)&md) { md = d; j = i; }
		}
		b--;
	} while (b != bestc);
	return(j);
}

void equivecinit (long n)
{
	float t0, t1;
	long z;

		//Init constants for ind2vec
	equivec.npoints = n;
	equivec.zmulk = 2 / (float)n; equivec.zaddk = equivec.zmulk*.5 - 1.0;

		//equimemset
	for(z=n-1;z>=0;z--)
		equiind2vec(z,&univec[z].x,&univec[z].y,&univec[z].z);
	if (n&1) //Hack for when n=255 and want a <0,0,0> vector
		{ univec[n].x = univec[n].y = univec[n].z = 0; }

		//Init Fibonacci table
	equivec.fib[0] = 0; equivec.fib[1] = 1;
	for(z=2;z<47;z++) equivec.fib[z] = equivec.fib[z-2]+equivec.fib[z-1];

		//Init fibx/y LUT
	t0 = .5 / PI; t1 = (float)n * -.5;
	for(z=0;z<45;z++)
	{
		t0 = -t0; equivec.fibx[z] = (float)equivec.fib[z+2]*t0;
		t1 = -t1; equivec.fiby[z] = ((float)equivec.fib[z+2]*GOLDRAT - (float)equivec.fib[z])*t1;
	}

	t0 = 1 / ((float)n * PI);
	for(equivec.aztop=0;equivec.aztop<20;equivec.aztop++)
	{
		t1 = 1 - (float)equivec.fib[(equivec.aztop<<1)+6]*t0; if (t1 < 0) break;
		equivec.azval[equivec.aztop+1] = sqrt(t1);
	}
}

//EQUIVEC code ends -------------------------------------------------------
/** Bitmask for multiplying npix at 9 mip levels */
static long umulmip[9] =
{
	(long)0             // 0b00000000000000000000000000000000
	,(long)4294967295   // 0b11111111111111111111111111111111
	,(long)2147483648   // 0b10000000000000000000000000000000
	,(long)1431655765   // 0b01010101010101010101010101010101
	,(long)1073741824   // 0b01000000000000000000000000000000
	,(long)858993459    // 0b00110011001100110011001100110011
	,(long)715827882    // 0b00101010101010101010101010101010
	,(long)613566756    // 0b00100100100100100100100100100100
	,(long)536870912    // 0b00100000000000000000000000000000
};

/** Generate 1 more mip-level for a .KV6 sprite.
 *  This function generates a
 *  lower MIP level only if kv6->lowermip is NULL, and kv6->xsiz,
 *  kv6->ysiz, and kv6->zsiz are all >= 3. When these conditions are
 *  true, it will generate a new .KV6 sprite with half the resolution in
 *  all 3 dimensions. It will set kv6->lowermip so it points to the newly
 *  generated .KV6 object. You can use freekv6() to de-allocate all levels
 *  of the .KV6 object.
 *
 *  To generate all mip levels use this pseudo-code:
 *  for(kv6data *tempkv6=mykv6;tempkv6=genmipkv6(tempkv6););
 *
 *  @param kv6 pointer to current MIP-level
 *  @return pointer to newly generated half-size MIP-level
 */
kv6data *genmipkv6 (kv6data *kv6)
{
	kv6data *nkv6;
	kv6voxtype *v0[2], *vs[4], *ve[4], *voxptr;
	unsigned short *xyptr, *xyi2, *sxyi2;
	long i, j, x, y, z, xs, ys, zs, xysiz, n, oxn, oxyn, *xptr;
	long xx, yy, zz, r, g, b, vis, npix, sxyi2i, darand = 0;
	char vecbuf[8];

	if ((!kv6) || (kv6->lowermip)) return(0);

	xs = ((kv6->xsiz+1)>>1); ys = ((kv6->ysiz+1)>>1); zs = ((kv6->zsiz+1)>>1);
	if ((xs < 2) || (ys < 2) || (zs < 2)) return(0);
	xysiz = ((((xs*ys)<<1)+3)&~3);
	i = sizeof(kv6data) + (xs<<2) + xysiz + kv6->numvoxs*sizeof(kv6voxtype);
	nkv6 = (kv6data *)malloc(i);
	if (!nkv6) return(0);

	kv6->lowermip = nkv6;
	nkv6->xsiz = xs;
	nkv6->ysiz = ys;
	nkv6->zsiz = zs;
	nkv6->xpiv = kv6->xpiv*.5;
	nkv6->ypiv = kv6->ypiv*.5;
	nkv6->zpiv = kv6->zpiv*.5;
	nkv6->namoff = 0;
	nkv6->lowermip = 0;

	xptr = (long *)(((long)nkv6) + sizeof(kv6data));
	xyptr = (unsigned short *)(((long)xptr) + (xs<<2));
	voxptr = (kv6voxtype *)(((long)xyptr) + xysiz);
	n = 0;

	v0[0] = kv6->vox; sxyi2 = kv6->ylen; sxyi2i = (kv6->ysiz<<1);
	for(x=0;x<xs;x++)
	{
		v0[1] = v0[0]+kv6->xlen[x<<1];

			//vs: start pointer of each of the 4 columns
			//ve: end pointer of each of the 4 columns
		vs[0] = v0[0]; vs[2] = v0[1];

		xyi2 = sxyi2; sxyi2 += sxyi2i;

		oxn = n;
		for(y=0;y<ys;y++)
		{
			oxyn = n;

			ve[0] = vs[1] = vs[0]+xyi2[0];
			if ((x<<1)+1 < kv6->xsiz) { ve[2] = vs[3] = vs[2]+xyi2[kv6->ysiz]; }
			if ((y<<1)+1 < kv6->ysiz)
			{
				ve[1] = vs[1]+xyi2[1];
				if ((x<<1)+1 < kv6->xsiz) ve[3] = vs[3]+xyi2[kv6->ysiz+1];
			}
			xyi2 += 2;

			while (1)
			{
				z = 0x7fffffff;
				for(i=3;i>=0;i--)
					if ((vs[i] < ve[i]) && (vs[i]->z < z)) z = vs[i]->z;
				if (z == 0x7fffffff) break;

				z |= 1;

				r = g = b = vis = npix = 0;
				for(i=3;i>=0;i--)
					for(zz=z-1;zz<=z;zz++)
					{
						if ((vs[i] >= ve[i]) || (vs[i]->z > zz)) continue;
						r += (vs[i]->col&0xff00ff); //MMX-style trick!
						g += (vs[i]->col&  0xff00);
						//b += (vs[i]->col&    0xff);
						vis |= vs[i]->vis;
						vecbuf[npix] = vs[i]->dir;
						npix++; vs[i]++;
					}

				if (npix)
				{
					if (n >= kv6->numvoxs) return(0); //Don't let it crash!

					i = umulmip[npix]; j = (npix>>1);
					voxptr[n].col = (umulshr32(r+(j<<16),i)&0xff0000) +
										 (umulshr32(g+(j<< 8),i)&  0xff00) +
										 (umulshr32((r&0xfff)+ j     ,i));
					voxptr[n].z = (z>>1);
					voxptr[n].vis = vis;
					voxptr[n].dir = vecbuf[umulshr32(darand,npix)]; darand += i;
					n++;
				}
			}
			xyptr[0] = n-oxyn; xyptr++;
			vs[0] = ve[1]; vs[2] = ve[3];
		}
		xptr[x] = n-oxn;
		if ((x<<1)+1 >= kv6->xsiz) break; //Avoid read page fault
		v0[0] = v0[1]+kv6->xlen[(x<<1)+1];
	}

	nkv6->leng = sizeof(kv6data) + (xs<<2) + xysiz + n*sizeof(kv6voxtype);
	nkv6 = (kv6data *)realloc(nkv6,nkv6->leng); if (!nkv6) return(0);
	nkv6->xlen = (unsigned long *)(((long)nkv6) + sizeof(kv6data));
	nkv6->ylen = (unsigned short *)(((long)nkv6->xlen) + (xs<<2));
	nkv6->vox = (kv6voxtype *)(((long)nkv6->ylen) + xysiz);
	nkv6->numvoxs = n;
	kv6->lowermip = nkv6;
	return(nkv6);
}
/**
 * This could be a handy function for debugging I suppose. Use it to save
 * .KV6 sprites to disk.
 *
 * @param filnam filename of .KV6 to save to disk. It's your responsibility to
 *               make sure it doesn't overwrite a file of the same name.
 * @param kv pointer to .KV6 object to save to disk.
 */
void savekv6 (const char *filnam, kv6data *kv)
{
	FILE *fil;
	long i;

	if ((fil = fopen(filnam,"wb")))
	{
		i = 0x6c78764b; fwrite(&i,4,1,fil); //Kvxl
		fwrite(&kv->xsiz,4,1,fil); fwrite(&kv->ysiz,4,1,fil); fwrite(&kv->zsiz,4,1,fil);
		fwrite(&kv->xpiv,4,1,fil); fwrite(&kv->ypiv,4,1,fil); fwrite(&kv->zpiv,4,1,fil);
		fwrite(&kv->numvoxs,4,1,fil);
		fwrite(kv->vox,kv->numvoxs*sizeof(kv6voxtype),1,fil);
		fwrite(kv->xlen,kv->xsiz*sizeof(long),1,fil);
		fwrite(kv->ylen,kv->xsiz*kv->ysiz*sizeof(short),1,fil);
		fclose(fil);
	}
}

/** @note should make this inline to getkv6!
    \Warning  This can evilquit when misaligned malloc happens.
*/
static kv6data *loadkv6 (const char *filnam)
{
	FILE *fil;
	kv6data tk, *newkv6;
	long i;

	if (!kzopen(filnam))
	{
			//File not found, but allocate a structure anyway
			//   so it can keep track of the filename
		if (!(newkv6 = (kv6data *)malloc(sizeof(kv6data)))) return(0);
		newkv6->leng = sizeof(kv6data);
		newkv6->xsiz = newkv6->ysiz = newkv6->zsiz = 0;
		newkv6->xpiv = newkv6->ypiv = newkv6->zpiv = 0;
		newkv6->numvoxs = 0;
		newkv6->namoff = 0;
		newkv6->lowermip = 0;
		newkv6->vox = (kv6voxtype *)(((long)newkv6)+sizeof(kv6data));
		newkv6->xlen = (unsigned long *)newkv6->vox;
		newkv6->ylen = (unsigned short *)newkv6->xlen;
		return(newkv6);
	}

	kzread((void *)&tk,32);

	i = tk.numvoxs*sizeof(kv6voxtype) + tk.xsiz*4 + tk.xsiz*tk.ysiz*2;
	newkv6 = (kv6data *)malloc(i+sizeof(kv6data));
	if (!newkv6) { kzclose(); return(0); }
	if (((long)newkv6)&3) evilquit("getkv6 malloc not 32-bit aligned!");

	newkv6->leng = i+sizeof(kv6data);
	memcpy(&newkv6->xsiz,&tk.xsiz,28);
	newkv6->namoff = 0;
	newkv6->lowermip = 0;
	newkv6->vox = (kv6voxtype *)(((long)newkv6)+sizeof(kv6data));
	newkv6->xlen = (unsigned long *)(((long)newkv6->vox)+tk.numvoxs*sizeof(kv6voxtype));
	newkv6->ylen = (unsigned short *)(((long)newkv6->xlen) + tk.xsiz*4);

	kzread((void *)newkv6->vox,i);
	kzclose();
	return(newkv6);
}

/**
 * Loads a .KV6 voxel sprite into memory. It malloc's the array for you and
 * returns the pointer to the loaded vx5sprite. If the same filename was
 * passed before to this function, it will return the pointer to the
 * previous instance of the .KV6 buffer in memory (It will NOT load the
 * same file twice). Uninitvoxlap() de-allocates all .KV6 sprites for
 * you.
 *
 * Other advanced info: Uses a 256-entry hash table to compare filenames, so
 * it should be fast. If you want to modify a .KV6 without affecting all
 * instances, you must allocate&de-allocate your own kv6data structure,
 * and use memcpy. The buffer is kv6data.leng bytes long (inclusive).
 *
 * Cover-up function for LOADKV6: returns a pointer to the loaded kv6data
 * structure. Loads file only if not already loaded before with getkv6.
 *
 * @param kv6nam .KV6 filename
 * @return pointer to malloc'ed kv6data structure. Do NOT free this buffer
 *         yourself! Returns 0 if there's an error - such as bad filename.
 */
kv6data *getkv6 (const char *filnam)
{
	kv6data *kv6ptr;
	long i;

	if (inkhash(filnam,&i)) return(*(kv6data **)&khashbuf[i+4]);
	if (i == -1) return(0);

	if ((kv6ptr = loadkv6((char *)&khashbuf[i+9])))
		kv6ptr->namoff = i+9; //Must use offset for ptr->name conversion

	*(kv6data **)&khashbuf[i+4] = kv6ptr;
	*(char *)&khashbuf[i+8] = 0; //0 for KV6
	return(kv6ptr);
}


void *caddasm;
#define cadd4 ((point4d *)&caddasm)
void *ztabasm;
#define ztab4 ((point4d *)&ztabasm)

short qsum0[4], qsum1[4], qbplbpp[4];
long kv6frameplace, kv6bytesperline;
float scisdist;
int64_t kv6colmul[256], kv6coladd[256];

#if defined(USEV5ASM) && !defined(NOASM)
EXTERN_C void drawboundcubesseinit ();
EXTERN_C void drawboundcubesse (kv6voxtype *, long);
EXTERN_C void drawboundcube3dninit ();
EXTERN_C void drawboundcube3dn (kv6voxtype *, long);
#endif

#ifdef __cplusplus
extern "C" {
#endif

char ptfaces16[43][8] =
{
    {0, 0, 0,  0,  0, 0, 0,0} , {4, 0,32,96, 64, 0,32,0} , {4,16,80,112,48, 16,80,0} , {0,0,0,0,0,0,0,0} ,
    {4,64,96,112, 80,64,96,0} , {6, 0,32,96,112,80,64,0} , {6,16,80, 64,96,112,48,0} , {0,0,0,0,0,0,0,0} ,
    {4, 0,16, 48, 32, 0,16,0} , {6, 0,16,48, 32,96,64,0} , {6, 0,16, 80,112,48,32,0} , {0,0,0,0,0,0,0,0} ,
    {0, 0, 0,  0,  0, 0, 0,0} , {0, 0, 0, 0,  0, 0, 0,0} , {0, 0, 0,  0,  0, 0, 0,0} , {0,0,0,0,0,0,0,0} ,
    {4, 0,64, 80, 16, 0,64,0} , {6, 0,32,96, 64,80,16,0} , {6, 0,64, 80,112,48,16,0} , {0,0,0,0,0,0,0,0} ,
    {6, 0,64, 96,112,80,16,0} , {6, 0,32,96,112,80,16,0} , {6, 0,64, 96,112,48,16,0} , {0,0,0,0,0,0,0,0} ,
    {6, 0,64, 80, 16,48,32,0} , {6,16,48,32, 96,64,80,0} , {6, 0,64, 80,112,48,32,0} , {0,0,0,0,0,0,0,0} ,
    {0, 0, 0,  0,  0, 0, 0,0} , {0, 0, 0, 0,  0, 0, 0,0} , {0, 0, 0,  0,  0, 0, 0,0} , {0,0,0,0,0,0,0,0} ,
    {4,32,48,112, 96,32,48,0} , {6, 0,32,48,112,96,64,0} , {6,16,80,112, 96,32,48,0} , {0,0,0,0,0,0,0,0} ,
    {6,32,48,112, 80,64,96,0} , {6, 0,32,48,112,80,64,0} , {6,16,80, 64, 96,32,48,0} , {0,0,0,0,0,0,0,0} ,
    {6, 0,16, 48,112,96,32,0} , {6, 0,16,48,112,96,64,0} , {6, 0,16, 80,112,96,32,0}
};


#ifdef __cplusplus
}
#endif

//static void initboundcubescr (long dafram, long dabpl, long x, long y, long dabpp)
//{
//   qsum1[3] = qsum1[1] = 0x7fff-y; qsum1[2] = qsum1[0] = 0x7fff-x;
//   qbplbpp[1] = dabpl; qbplbpp[0] = ((dabpp+7)>>3);
//   kv6frameplace = dafram; kv6bytesperline = dabpl;
//}

static __ALIGN(8) short lightlist[MAXLIGHTS+1][4];

/** Related to fog functions */
static int64_t all32767 FORCE_NAME("all32767") = 0x7fff7fff7fff7fff;

static void updatereflects (vx5sprite *spr)
{
	int64_t fogmul;
	point3d tp;
	float f, g, h, fx, fy, fz;
	long i, j;

	#if defined(__GNUC__) && !defined(NOASM) //only for gcc inline asm
	register lpoint2d reg0 asm("mm0");
	register lpoint2d reg1 asm("mm1");
	register lpoint2d reg2 asm("mm2");
	register lpoint2d reg3 asm("mm3");
	//register lpoint2d reg4 asm("mm4");
	register lpoint2d reg5 asm("mm5");
	register lpoint2d reg6 asm("mm6");
	//register lpoint2d reg7 asm("mm7");
	#endif

#if 0
	//KV6 lighting calculations for: fog, white, black, intens(normal dot product), black currently not supported!

	long vx5.kv6black = 0x000000, vx5.kv6white = 0x808080;
	long nw.r = vx5.kv6white.r-vx5.kv6black.r, nb.r = vx5.kv6black.r*2;
	long nw.g = vx5.kv6white.g-vx5.kv6black.g, nb.g = vx5.kv6black.g*2;
	long nw.b = vx5.kv6white.b-vx5.kv6black.b, nb.b = vx5.kv6black.b*2;
	col.r = mulshr7(col.r,nw.r)+nb.r; col.r = mulshr7(col.r,intens); col.r += mulshr15(fogcol.r-col.r,fogmul);
	col.g = mulshr7(col.g,nw.g)+nb.g; col.g = mulshr7(col.g,intens); col.g += mulshr15(fogcol.g-col.g,fogmul);
	col.b = mulshr7(col.b,nw.b)+nb.b; col.b = mulshr7(col.b,intens); col.b += mulshr15(fogcol.b-col.b,fogmul);

	col.r = ((col.r*intens*nw.r*(32767-fogmul))>>29) +
			  ((      intens*nb.r*(32767-fogmul))>>22) + ((fogcol.r*fogmul)>>15);
	col.g = ((col.g*intens*nw.g*(32767-fogmul))>>29) +
			  ((      intens*nb.g*(32767-fogmul))>>22) + ((fogcol.g*fogmul)>>15);
	col.b = ((col.b*intens*nw.b*(32767-fogmul))>>29) +
			  ((      intens*nb.b*(32767-fogmul))>>22) + ((fogcol.b*fogmul)>>15);
#endif

		//Use cylindrical x-y distance for fog
	if (ofogdist >= 0)
	{
		ftol(sqrt((spr->p.x-gipos.x)*(spr->p.x-gipos.x) + (spr->p.y-gipos.y)*(spr->p.y-gipos.y)),&i);
		if (i > 2047) i = 2047;
		fogmul = foglut[i];

        #if defined(NOASM)
		i = (long)(*(short *)&fogmul);
		((short *)kv6coladd)[0] = (short)((((long)(((short *)&fogcol)[0]))*i)>>1);
		((short *)kv6coladd)[1] = (short)((((long)(((short *)&fogcol)[1]))*i)>>1);
		((short *)kv6coladd)[2] = (short)((((long)(((short *)&fogcol)[2]))*i)>>1);
        #elif defined(__GNUC__) && !defined(NOASM)
		__asm__ __volatile__
		(
			"paddd	%[m64], %[m64]\n"
			"pmulhuw	%[mul], %[m64]\n"
			: [m64] "=y" (*(lpoint2d *)kv6coladd)
			: "0" (fogcol), [mul] "m" (fogmul)
			:
		);
        #elif defined(_MSC_VER) && !defined(NOASM)
		_asm
		{
			movq	mm0, fogcol
			paddd	mm0, mm0
			pmulhuw	mm0, fogmul
			movq	kv6coladd[0], mm0
			emms
		}
		#endif
	}
	else
	{
		fogmul = 0LL;
		kv6coladd[0] = 0LL;
	}

	if (spr->flags&1)
	{
		//((short *)kv6colmul)[0] = 0x010001000100;                                        //Do (nothing)
		  ((short *)kv6colmul)[0] = (short)((vx5.kv6col&255)<<1);                          //Do     white
		//((short *)kv6colmul)[0] = (short)((32767-(short)fogmul)>>7);                     //Do fog
		//((short *)kv6colmul)[0] = (short)(((32767-(short)fogmul)*(vx5.kv6col&255))>>14); //Do fog&white

		((short *)kv6colmul)[1] = ((short *)kv6colmul)[0];
		((short *)kv6colmul)[2] = ((short *)kv6colmul)[0];
		((short *)kv6colmul)[3] = 0;
		for(i=1;i<256;i++) kv6colmul[i] = kv6colmul[0];
		return;
	}

	if (vx5.lightmode < 2)
	{
		fx = 1.0; fy = 1.0; fz = 1.0;
		tp.x = spr->s.x*fx + spr->s.y*fy + spr->s.z*fz;
		tp.y = spr->h.x*fx + spr->h.y*fy + spr->h.z*fz;
		tp.z = spr->f.x*fx + spr->f.y*fy + spr->f.z*fz;

		f = 64.0 / sqrt(tp.x*tp.x + tp.y*tp.y + tp.z*tp.z);
        #if defined(NOASM)
		for(i=255;i>=0;i--)
		{
		   ftol(univec[i].x*tp.x + univec[i].y*tp.y + univec[i].z*tp.z,&j);
		   j = (lbound0(j+128,255)<<8);
		   ((unsigned short *)(&kv6colmul[i]))[0] = j;
		   ((unsigned short *)(&kv6colmul[i]))[1] = j;
		   ((unsigned short *)(&kv6colmul[i]))[2] = j;
		}
        #else
		g = ((float)((((long)fogmul)&32767)^32767))*(16.f*8.f/65536.f);
		if (!(((vx5.kv6col&0xffff)<<8)^(vx5.kv6col&0xffff00))) //Cool way to check if R==G==B :)
		{
			g *= ((float)(vx5.kv6col&255))/256.f;
				//This case saves 1 MMX multiply per iteration
			f *= g;
			lightlist[0][0] = (short)(tp.x*f);
			lightlist[0][1] = (short)(tp.y*f);
			lightlist[0][2] = (short)(tp.z*f);
			lightlist[0][3] = (short)(g*128.f);
			#ifdef __GNUC__ //gcc inline asm
			__asm__ __volatile__
			(
			".Lnolighta:\n"
				"movq	%c[uv](%[c]), %[y0]\n"
				"movq	%c[uv]-8(%[c]), %[y1]\n"
				"pmaddwd	%[y6], %[y0]\n"      //mm0: [tp.a*iunivec.a + tp.z*iunivec.z][tp.y*iunivec.y + tp.x*iunivec.x]
				"pmaddwd	%[y6], %[y1]\n"
				"pshufw	$0x4e, %[y0], %[y2]\n"   //Before: mm0: [ 0 ][ a ][   ][   ][ 0 ][ b ][   ][   ]
				"pshufw	$0x4e, %[y1], %[y3]\n"
				"paddd	%[y2], %[y0]\n"
				"paddd	%[y3], %[y1]\n"
				"pshufw $0x55, %[y0], %[y0]\n"
				"pshufw	$0x55, %[y1], %[y1]\n"   //After:  mm0: [   ][   ][   ][a+b][   ][a+b][   ][a+b]
				"movq	%[y0], %c[kvcm](%[c])\n"
				"movq	%[y1], %c[kvcm]-8(%[c])\n"
				"sub	$2*8, %[c]\n"
				"jnc    .Lnolighta\n"
				: [y0] "=y" (reg0), [y1] "=y" (reg1),
				  [y2] "=y" (reg2), [y3] "=y" (reg3),
				  [y6] "=y" (reg6)
				: [c]  "r" (255*8), "4" (*(int64_t *)lightlist),
				  [uv] "p" (iunivec), [kvcm] "p" (kv6colmul)
				:
			);
			#endif
			#ifdef _MSC_VER //msvc inline asm
			_asm
			{
				movq	mm6, lightlist[0]
				mov	ecx, 255*8
			nolighta:
				movq	mm0, iunivec[ecx]
				movq	mm1, iunivec[ecx-8]
				pmaddwd	mm0, mm6 //mm0: [tp.a*iunivec.a + tp.z*iunivec.z][tp.y*iunivec.y + tp.x*iunivec.x]
				pmaddwd	mm1, mm6
				pshufw	mm2, mm0, 0x4e  //Before: mm0: [ 0 ][ a ][   ][   ][ 0 ][ b ][   ][   ]
				pshufw	mm3, mm1, 0x4e
				paddd	mm0, mm2
				paddd	mm1, mm3
				pshufw	mm0, mm0, 0x55
				pshufw	mm1, mm1, 0x55  //After:  mm0: [   ][   ][   ][a+b][   ][a+b][   ][a+b]
				movq	kv6colmul[ecx], mm0
				movq	kv6colmul[ecx-8], mm1
				sub	ecx, 2*8
				jnc	short nolighta
			}
			#endif
		}
		else
		{
			f *= g;
			lightlist[0][0] = (short)(tp.x*f);
			lightlist[0][1] = (short)(tp.y*f);
			lightlist[0][2] = (short)(tp.z*f);
			lightlist[0][3] = (short)(g*128.f);
			#if defined(__GNUC__) && !defined(NOASM)
			__asm__ __volatile__
			(
				"punpcklbw	%[vxpart], %[y5]\n"
			".Lnolightb:\n"
				"movq	%c[uv](%[c]), %[y0]\n"
				"movq	%c[uv]-8(%[c]), %[y1]\n"
				"pmaddwd	%[y6], %[y0]\n"      //mm0: [tp.a*iunivec.a + tp.z*iunivec.z][tp.y*iunivec.y + tp.x*iunivec.x]
				"pmaddwd	%[y6], %[y1]\n"
				"pshufw	$0x4e, %[y0], %[y2]\n"   //Before: mm0: [ 0 ][ a ][   ][   ][ 0 ][ b ][   ][   ]
				"pshufw	$0x4e, %[y1], %[y3]\n"
				"paddd	%[y2], %[y0]\n"
				"paddd	%[y3], %[y1]\n"
				"pshufw $0x55, %[y0], %[y0]\n"
				"pshufw	$0x55, %[y1], %[y1]\n"   //After:  mm0: [   ][   ][   ][a+b][   ][a+b][   ][a+b]
				"pmulhuw	%[y5], %[y0]\n"
				"pmulhuw	%[y5], %[y1]\n"
				"movq	%[y0], %c[kvcm](%[c])\n"
				"movq	%[y1], %c[kvcm]-8(%[c])\n"
				"sub	$2*8, %[c]\n"
				"jnc .Lnolightb\n"
				: [y0] "+y" (reg0), [y1] "+y" (reg1),
				  [y2] "+y" (reg2), [y3] "+y" (reg3),
				  [y5] "=y" (reg5), [y6] "=y" (reg6)
				: "5" (*(int64_t *)lightlist),
				  [c]  "r" (255*8), [vxpart] "m" (vx5.kv6col),
				  [uv] "p" (iunivec), [kvcm] "p" (kv6colmul)
			);
            #elif defined(_MSC_VER) && !defined(NOASM)
			_asm
			{
				punpcklbw	mm5, vx5.kv6col
				movq	mm6, lightlist[0]
				mov	ecx, 255*8
			nolightb:
				movq	mm0, iunivec[ecx]
				movq	mm1, iunivec[ecx-8]
				pmaddwd	mm0, mm6 //mm0: [tp.a*iunivec.a + tp.z*iunivec.z][tp.y*iunivec.y + tp.x*iunivec.x]
				pmaddwd	mm1, mm6
				pshufw	mm2, mm0, 0x4e //Before: mm0: [ 0 ][ a ][   ][   ][ 0 ][ b ][   ][   ]
				pshufw	mm3, mm1, 0x4e
				paddd	mm0, mm2
				paddd	mm1, mm3
				pshufw	mm0, mm0, 0x55
				pshufw	mm1, mm1, 0x55 //After:  mm0: [   ][   ][   ][a+b][   ][a+b][   ][a+b]
				pmulhuw	mm0, mm5
				pmulhuw	mm1, mm5
				movq	kv6colmul[ecx], mm0
				movq	kv6colmul[ecx-8], mm1
				sub	ecx, 2*8
				jnc short nolightb
			}
			#endif
		}
		//NOTE: emms not necessary!
		#endif
	}
	else
	{
		point3d sprs, sprh, sprf;
		float ff, gg, hh;
		long k, lightcnt;

			//WARNING: this only works properly for orthonormal matrices!
		f = 1.0 / sqrt(spr->s.x*spr->s.x + spr->s.y*spr->s.y + spr->s.z*spr->s.z);
		sprs.x = spr->s.x*f; sprs.y = spr->s.y*f; sprs.z = spr->s.z*f;
		f = 1.0 / sqrt(spr->h.x*spr->h.x + spr->h.y*spr->h.y + spr->h.z*spr->h.z);
		sprh.x = spr->h.x*f; sprh.y = spr->h.y*f; sprh.z = spr->h.z*f;
		f = 1.0 / sqrt(spr->f.x*spr->f.x + spr->f.y*spr->f.y + spr->f.z*spr->f.z);
		sprf.x = spr->f.x*f; sprf.y = spr->f.y*f; sprf.z = spr->f.z*f;

		hh = ((float)((((long)fogmul)&32767)^32767))/65536.f * 2.f;


			//Find which lights are close enough to affect sprite.
		lightcnt = 0;
		for(i=vx5.numlights-1;i>=0;i--)
		{
			fx = vx5.lightsrc[i].p.x-(spr->p.x);
			fy = vx5.lightsrc[i].p.y-(spr->p.y);
			fz = vx5.lightsrc[i].p.z-(spr->p.z);
			gg = fx*fx + fy*fy + fz*fz; ff = vx5.lightsrc[i].r2;
			if (*(long *)&gg < *(long *)&ff)
			{
				f = sqrt(ff); g = sqrt(gg);
				//h = (16.0/(sqrt(gg)*gg) - 16.0/(sqrt(ff)*ff))*vx5.lightsrc[i].sc;
				h = (f*ff - g*gg)/(f*ff*g*gg) * vx5.lightsrc[i].sc*16.0;
				if (g*h > 4096.0) h = 4096.0/g; //Max saturation clipping
				h *= hh;
				lightlist[lightcnt][0] = (short)((fx*sprs.x + fy*sprs.y + fz*sprs.z)*h);
				lightlist[lightcnt][1] = (short)((fx*sprh.x + fy*sprh.y + fz*sprh.z)*h);
				lightlist[lightcnt][2] = (short)((fx*sprf.x + fy*sprf.y + fz*sprf.z)*h);
				lightlist[lightcnt][3] = 0;
				lightcnt++;
			}
		}

		fx = 0.0; fy = 0.5; fz = 1.0;
        #if defined(NOASM)
		tp.x = (sprs.x*fx + sprs.y*fy + sprs.z*fz)*16.0;
		tp.y = (sprh.x*fx + sprh.y*fy + sprh.z*fz)*16.0;
		tp.z = (sprf.x*fx + sprf.y*fy + sprf.z*fz)*16.0;
		for(i=255;i>=0;i--)
		{
			f = tp.x*univec[i].x + tp.y*univec[i].y + tp.z*univec[i].z + 48;
			for(k=lightcnt-1;k>=0;k--)
			{
				h = lightlist[k][0]*univec[i].x + lightlist[k][1]*univec[i].y + lightlist[k][2]*univec[i].z;
				if (*(long *)&h < 0) f -= h;
			}
			if (f > 255) f = 255;
			ftol(f,&j); j <<= 8;
			((unsigned short *)(&kv6colmul[i]))[0] = j;
			((unsigned short *)(&kv6colmul[i]))[1] = j;
			((unsigned short *)(&kv6colmul[i]))[2] = j;
		}
		#else
		hh *= 16*16.f*8.f/2.f;
		lightlist[lightcnt][0] = (short)((sprs.x*fx + sprs.y*fy + sprs.z*fz)*hh);
		lightlist[lightcnt][1] = (short)((sprh.x*fx + sprh.y*fy + sprh.z*fz)*hh);
		lightlist[lightcnt][2] = (short)((sprf.x*fx + sprf.y*fy + sprf.z*fz)*hh);
		lightlist[lightcnt][3] = (short)(hh*(48/16.0));
        #if defined(__GNUC__) && !defined(NOASM)
		__asm__ __volatile__
		(
			"punpcklbw	%[vxpart], %[y5]\n"
			"pxor	%[y6], %[y6]\n"
			"shl	$3, %[d]\n"
		".Lbeglig:\n"
			"movq	%c[uv](%[c]), %[y3]\n" //mm3: 256 u[i].z*256 u[i].y*256 u[i].x*256
			"mov	%[d], %[a]\n"
			"movq	%c[ll](%[d]), %[y0]\n" //mm0: 48*256,0 tp.z*256 tp.y*256 tp.x*256
			"pmaddwd	%[y3], %[y0]\n"
			"pshufw	$0x4e, %[y0], %[y2]\n"
			"paddd	%[y2], %[y0]\n"
			"sub	$8, %[a]\n"
			"js	.Lendlig\n"
		".Lbeglig2:\n"
			"movq	%c[ll](%[a]), %[y1]\n" //mm1: 0 tp.z*256 tp.y*256 tp.x*256
			"pmaddwd	%[y3], %[y1]\n"
			"pshufw	$0x4e, %[y1], %[y2]\n"
			"paddd	%[y2], %[y1]\n"
			"pminsw	%[y6], %[y1]\n"        //16-bits is ugly, but ok here
			"psubd	%[y1], %[y0]\n"
			"sub	$8, %[a]\n"
			"jns	.Lbeglig2\n"           //mm0: 00 II ii ii 00 II ii ii
		".Lendlig:\n"
			"pshufw	$0x55, %[y0], %[y0]\n"  //mm0: 00 II 00 II 00 II 00 II
			"pmulhuw	%[y5], %[y0]\n"
			"movq	%[y0], %c[kvcm](%[c])\n"
			"sub	$8, %[c]\n"
			"jnc	.Lbeglig\n"
			:
			: [y0] "y" (reg0), [y1] "y" (reg1),
			  [y2] "y" (reg2), [y3] "y" (reg3),
			  [y5] "y" (reg5), [y6] "y" (reg6),
			  [c]  "r" (255*8), [d] "r" (lightcnt), [a] "r" (0),
			  [vxpart] "m" (vx5.kv6col),
			  [uv] "p" (iunivec), [kvcm] "p" (kv6colmul),
			  [ll] "p" (lightlist)
			:
		);
        #elif defined(_MSC_VER) && !defined(NOASM)
		_asm
		{
			punpcklbw	mm5, vx5.kv6col
			pxor	mm6, mm6
			mov	edx, lightcnt
			shl	edx, 3
			mov	ecx, 255*8
		beglig:
			movq	mm3, iunivec[ecx]   //mm3: 256 u[i].z*256 u[i].y*256 u[i].x*256
			mov	eax, edx
			movq	mm0, lightlist[edx] //mm0: 48*256,0 tp.z*256 tp.y*256 tp.x*256
			pmaddwd	mm0, mm3
			pshufw	mm2, mm0, 0x4e
			paddd	mm0, mm2
			sub	eax, 8
			js	short endlig
		beglig2:
			movq	mm1, lightlist[eax] //mm1: 0 tp.z*256 tp.y*256 tp.x*256
			pmaddwd	mm1, mm3
			pshufw	mm2, mm1, 0x4e
			paddd	mm1, mm2
			pminsw	mm1, mm6         //16-bits is ugly, but ok here
			psubd	mm0, mm1
			sub	eax, 8
			jns	short beglig2        //mm0: 00 II ii ii 00 II ii ii
		endlig:
			pshufw	mm0, mm0, 0x55   //mm0: 00 II 00 II 00 II 00 II
			pmulhuw	mm0, mm5
			movq	kv6colmul[ecx], mm0
			sub	ecx, 8
			jnc	short beglig
		}
		#endif
		//NOTE: emms not necessary!
		#endif
	}
}

static inline void movps (point4d *dest, point4d *src)
{
	#if defined(NOASM)
	*dest = *src;
    #elif defined(__GNUC__) && !defined(NOASM)
	__asm__ __volatile__
	(
		""
		: "=x" (dest->vec)
		:  "0" (src->vec)
		:
	);
    #elif defined(_MSC_VER) && !defined(NOASM)
	_asm
	{
		mov	eax, src
		movaps	xmm7, [eax]
		mov	eax, dest
		movaps	[eax], xmm7
	}
	#endif
}

static inline void intss (point4d *dest, long src)
{
	#if defined(NOASM)
	dest->x = dest->y = dest->z = dest->z2 = (float)src;
    #elif defined(__GNUC__) && !defined(NOASM)
	__asm__ __volatile__
	(
		"cvtsi2ss	%[src], %[dest]\n"
		"shufps	$0, %[dest], %[dest]\n"
		: [dest] "=x" (dest->vec)
		: [src]   "g" (src)
		:
	);
    #elif defined(_MSC_VER) && !defined(NOASM)
	_asm
	{
		mov	eax, dest
		cvtsi2ss	xmm7, src
		shufps	xmm7, xmm7, 0
		movaps	[eax], xmm7
	}
	#endif
}

static inline void addps (point4d *sum, point4d *a, point4d *b)
{
	#if defined(NOASM)
	sum->x  =  a->x  +  b->x;
	sum->y  =  a->y  +  b->y;
	sum->z  =  a->z  +  b->z;
	sum->z2 =  a->z2 +  b->z2;
    #elif defined(__GNUC__) && !defined(NOASM)
	__asm__ __volatile__
	(
		"addps	%[b], %[a]\n"
		: [a] "=x" (sum->vec)
		:      "0" (a->vec), [b] "x" (b->vec)
		:
	);
    #elif defined(_MSC_VER) && !defined(NOASM)
	_asm
	{
		mov	eax, a
		movaps	xmm7, [eax]
		mov	eax, b
		addps	xmm7, [eax]
		mov	eax, sum
		movaps	[eax], xmm7
	}
	#endif
}

static inline void mulps (point4d *sum, point4d *a, point4d *b)
{
	#if defined(NOASM)
	sum->x  =  a->x  *  b->x;
	sum->y  =  a->y  *  b->y;
	sum->z  =  a->z  *  b->z;
	sum->z2 =  a->z2 *  b->z2;
    #elif defined(__GNUC__) && !defined(NOASM)
	__asm__ __volatile__
	(
		"mulps	%[b], %[a]\n"
		: [a] "=x" (sum->vec)
		:      "0" (a->vec), [b] "x" (b->vec)
		:
	);
    #elif defined(_MSC_VER) && !defined(NOASM)
	_asm
	{
		mov	eax, a
		movaps	xmm7, [eax]
		mov	eax, b
		mulps	xmm7, [eax]
		mov	eax, sum
		movaps	[eax], xmm7
	}
	#endif
}

static inline void subps (point4d *sum, point4d *a, point4d *b)
{
    #if defined(NOASM)
	sum->x  =  a->x  -  b->x;
	sum->y  =  a->y  -  b->y;
	sum->z  =  a->z  -  b->z;
	sum->z2 =  a->z2 -  b->z2;
    #elif defined(__GNUC__) && !defined(NOASM)
	__asm__ __volatile__
	(
		"subps	%[b], %[a]\n"
		: [a] "=x" (sum->vec)
		:      "0" (a->vec), [b] "x" (b->vec)
		:
	);
    #elif defined(_MSC_VER) && !defined(NOASM)
	_asm
	{
		mov	eax, a
		movaps	xmm7, [eax]
		mov	eax, b
		subps	xmm7, [eax]
		mov	eax, sum
		movaps	[eax], xmm7
	}
	#endif

}

static inline void minps (point4d *sum, point4d *a, point4d *b)
{
    #if defined(NOASM)
	sum->x  =  MIN(a->x,  b->x);
	sum->y  =  MIN(a->y,  b->y);
	sum->z  =  MIN(a->z,  b->z);
	sum->z2 =  MIN(a->z2, b->z2);
    #elif defined(__GNUC__) && !defined(NOASM)
	__asm__ __volatile__
	(
		"minps	%[b], %[a]\n"
		: [a] "=x" (sum->vec)
		:      "0" (a->vec), [b] "x" (b->vec)
		:
	);
    #elif defined(_MSC_VER) && !defined(NOASM)
	_asm
	{
		mov	eax, a
		movaps	xmm7, [eax]
		mov	eax, b
		minps	xmm7, [eax]
		mov	eax, sum
		movaps	[eax], xmm7
	}
	#endif
}

static inline void maxps (point4d *sum, point4d *a, point4d *b)
{
    #if defined(NOASM)
	sum->x  =  MAX(a->x,  b->x);
	sum->y  =  MAX(a->y,  b->y);
	sum->z  =  MAX(a->z,  b->z);
	sum->z2 =  MAX(a->z2, b->z2);
    #elif defined(__GNUC__) && !defined(NOASM)
		__asm__ __volatile__
	(
		"maxps	%[b], %[a]\n"
		: [a] "=x" (sum->vec)
		:      "0" (a->vec), [b] "x" (b->vec)
		:
	);
    #elif defined(_MSC_VER) && !defined(NOASM)
	_asm
	{
		mov	eax, a
		movaps	xmm7, [eax]
		mov	eax, b
		maxps	xmm7, [eax]
		mov	eax, sum
		movaps	[eax], xmm7
	}
	#endif
}

static inline void movps_3dn (point4d *dest, point4d *src)
{
    #if defined(NOASM)
	*dest = *src;
    #elif defined(__GNUC__) && !defined(NOASM)
	__asm__ __volatile__
	(
		""
		: "=y" (dest->svec[0]), "=y" (dest->svec[1])
		:  "0" (src ->svec[0]),  "1" (src ->svec[1])
		:
	);
    #elif defined(_MSC_VER) && !defined(NOASM)
	_asm
	{
		mov	eax, src
		movq	mm0, [eax]
		movq	mm1, [eax+8]
		mov	eax, dest
		movq	[eax], mm0
		movq	[eax+8], mm1
	}
	#endif
}

static inline void intss_3dn (point4d *dest, long src)
{
    #if defined(NOASM)
	dest->x = dest->y = dest->z = dest->z2 = (float)src;
    #elif defined(__GNUC__) && !defined(NOASM)
	__asm__ __volatile__
	(
		"pi2fd	%[y1], %[y1]\n"
		"punpckldq	%[y1], %[y1]\n"
		"movq	%[y1], (%[adrs])\n"
		: [y1] "=y" (dest->svec[0])
		:       "0" (src), [adrs] "r" (&(dest->svec[1]))
		:
	);
    #elif defined(_MSC_VER) && !defined(NOASM)
	_asm
	{
		mov	eax, dest
		movd	mm0, src
		pi2fd	mm0, mm0
		punpckldq	mm0, mm0
		movq	[eax], mm0
		movq	[eax+8], mm0
	}
	#endif
}

static inline void addps_3dn (point4d *sum, point4d *a, point4d *b)
{
    #if defined(NOASM)
	sum->x  =  a->x  +  b->x;
	sum->y  =  a->y  +  b->y;
	sum->z  =  a->z  +  b->z;
	sum->z2 =  a->z2 +  b->z2;
    #elif defined(__GNUC__) && !defined(NOASM)
	__asm__ __volatile__
	(
		"pfadd	(%[b]),  %[a1]\n"
		"pfadd	8(%[b]), %[a2]\n"
		: [a1] "=y" (sum->svec[0]), [a2] "=y" (sum->svec[1])
		:       "0" (a  ->svec[0]),       "1" (a  ->svec[1]), [b] "r" (&b)
		:
	);
    #elif defined(_MSC_VER) && !defined(NOASM)
	_asm
	{
		mov	eax, a
		movq	mm0, [eax]
		movq	mm1, [eax+8]
		mov	eax, b
		pfadd	mm0, [eax]
		pfadd	mm1, [eax+8]
		mov	eax, sum
		movq	[eax], mm0
		movq	[eax+8], mm1
	}
	#endif
}

static inline void mulps_3dn (point4d *sum, point4d *a, point4d *b)
{
    #if defined(NOASM)
	sum->x  =  a->x  *  b->x;
	sum->y  =  a->y  *  b->y;
	sum->z  =  a->z  *  b->z;
	sum->z2 =  a->z2 *  b->z2;
    #elif defined(__GNUC__) && !defined(NOASM)
	__asm__ __volatile__
	(
		"pfmul	(%[b]),  %[a1]\n"
		"pfmul	8(%[b]), %[a2]\n"
		: [a1] "=y" (sum->svec[0]), [a2] "=y" (sum->svec[1])
		:       "0" (a  ->svec[0]),       "1" (a  ->svec[1]), [b] "r" (&b)
		:
	);
    #elif defined(_MSC_VER) && !defined(NOASM)
	_asm
	{
		mov	eax, a
		movq	mm0, [eax]
		movq	mm1, [eax+8]
		mov	eax, b
		pfmul	mm0, [eax]
		pfmul	mm1, [eax+8]
		mov	eax, sum
		movq	[eax], mm0
		movq	[eax+8], mm1
	}
	#endif
}

static inline void subps_3dn (point4d *sum, point4d *a, point4d *b)
{
    #if defined(NOASM)
	sum->x  =  a->x  -  b->x;
	sum->y  =  a->y  -  b->y;
	sum->z  =  a->z  -  b->z;
	sum->z2 =  a->z2 -  b->z2;
    #elif defined(__GNUC__) && !defined(NOASM)
	__asm__ __volatile__
	(
		"pfsub	(%[b]),  %[a1]\n"
		"pfsub	8(%[b]), %[a2]\n"
		: [a1] "=y" (sum->svec[0]), [a2] "=y" (sum->svec[1])
		:       "0" (a  ->svec[0]),       "1" (a  ->svec[1]), [b] "r" (&b)
		:
	);
    #elif defined(_MSC_VER) && !defined(NOASM)
	_asm
	{
		mov	eax, a
		movq	mm0, [eax]
		movq	mm1, [eax+8]
		mov	eax, b
		pfsub	mm0, [eax]
		pfsub	mm1, [eax+8]
		mov	eax, sum
		movq	[eax], mm0
		movq	[eax+8], mm1
	}
	#endif
}

static inline void minps_3dn (point4d *sum, point4d *a, point4d *b)
{
    #if defined(NOASM)
	sum->x  =  MIN(a->x,  b->x);
	sum->y  =  MIN(a->y,  b->y);
	sum->z  =  MIN(a->z,  b->z);
	sum->z2 =  MIN(a->z2, b->z2);
    #elif defined(__GNUC__) && !defined(NOASM)
	__asm__ __volatile__
	(
		"pfmin	(%[b]),  %[a1]\n"
		"pfmin	8(%[b]), %[a2]\n"
		: [a1] "=y" (sum->svec[0]), [a2] "=y" (sum->svec[1])
		:       "0" (a  ->svec[0]),       "1" (a  ->svec[1]), [b] "r" (&b)
		:
	);
    #elif defined(_MSC_VER) && !defined(NOASM)
	_asm
	{
		mov	eax, a
		movq	mm0, [eax]
		movq	mm1, [eax+8]
		mov	eax, b
		pfmin	mm0, [eax]
		pfmin	mm1, [eax+8]
		mov	eax, sum
		movq	[eax], mm0
		movq	[eax+8], mm1
	}
	#endif
}

static inline void maxps_3dn (point4d *sum, point4d *a, point4d *b)
{
    #if defined(NOASM)
	sum->x  =  MAX(a->x,  b->x);
	sum->y  =  MAX(a->y,  b->y);
	sum->z  =  MAX(a->z,  b->z);
	sum->z2 =  MAX(a->z2, b->z2);
    #elif defined(__GNUC__) && !defined(NOASM)
	__asm__ __volatile__
	(
		"pfmax	(%[b]),  %[a1]\n"
		"pfmax	8(%[b]), %[a2]\n"
		: [a1] "=y" (sum->svec[0]), [a2] "=y" (sum->svec[1])
		:       "0" (a  ->svec[0]),       "1" (a  ->svec[1]), [b] "r" (&b)
		:
	);
    #elif defined(_MSC_VER) && !defined(NOASM)
	_asm
	{
		mov	eax, a
		movq	mm0, [eax]
		movq	mm1, [eax+8]
		mov	eax, b
		pfmax	mm0, [eax]
		pfmax	mm1, [eax+8]
		mov	eax, sum
		movq	[eax], mm0
		movq	[eax+8], mm1
	}
	#endif
}

#define DRAWBOUNDCUBELINE(const) \
	for(;v0<=v1 && v0->z<inz;v0++) drawboundcubesse(v0,const+0x20);\
	for(;v0<=v1 && v1->z>inz;v1--) drawboundcubesse(v1,const+0x10);\
						  if (v0 == v1) drawboundcubesse(v1,const+0x00);

#define DRAWBOUNDCUBELINE_3DN(const) \
	for(;v0<=v1 && v0->z<inz;v0++) drawboundcube3dn(v0,const+0x20);\
	for(;v0<=v1 && v1->z>inz;v1--) drawboundcube3dn(v1,const+0x10);\
						  if (v0 == v1) drawboundcube3dn(v1,const+0x00);

	//Code taken from renderboundcube of SLAB6D (Pentium III version :)
#define MAXZSIZ 1024
static void kv6draw (vx5sprite *spr)
{
	point4d *r0, *r1, *r2;
	kv6voxtype *xv, *yv, *v0, *v1;
	kv6data *kv;
	point3d ts, th, tf;
	point3d npos, nstr, nhei, nfor, tp, tp2;
	float f;
	long x, y, z, inx, iny, inz, nxplanemin, nxplanemax;
	unsigned short *ylenptr;

	kv = spr->voxnum; if (!kv) return;

	z = 0; //Quick & dirty estimation of distance
	ftol((spr->p.x-gipos.x)*gifor.x + (spr->p.y-gipos.y)*gifor.y + (spr->p.z-gipos.z)*gifor.z,&y);
	while ((kv->lowermip) && (y >= vx5.kv6mipfactor)) { kv = kv->lowermip; z++; y >>= 1; }
	if (!z)
	{
		nxplanemin = vx5.xplanemin; nxplanemax = vx5.xplanemax;
		ts = spr->s; th = spr->h; tf = spr->f;
	}
	else
	{
		nxplanemin = (vx5.xplanemin>>z);
		nxplanemax = (vx5.xplanemax>>z); f = (float)(1<<z);
		ts.x = spr->s.x*f; ts.y = spr->s.y*f; ts.z = spr->s.z*f;
		th.x = spr->h.x*f; th.y = spr->h.y*f; th.z = spr->h.z*f;
		tf.x = spr->f.x*f; tf.y = spr->f.y*f; tf.z = spr->f.z*f;
	}

		//View frustrum culling (72*,63+,12fabs,4cmp)
	tp2.x = ((float)kv->xsiz)*.5; tp.x = tp2.x - kv->xpiv;
	tp2.y = ((float)kv->ysiz)*.5; tp.y = tp2.y - kv->ypiv;
	tp2.z = ((float)kv->zsiz)*.5; tp.z = tp2.z - kv->zpiv;
	npos.x = tp.x*ts.x + tp.y*th.x + tp.z*tf.x + (spr->p.x-gipos.x);
	npos.y = tp.x*ts.y + tp.y*th.y + tp.z*tf.y + (spr->p.y-gipos.y);
	npos.z = tp.x*ts.z + tp.y*th.z + tp.z*tf.z + (spr->p.z-gipos.z);
	nstr.x = ts.x*tp2.x; nstr.y = ts.y*tp2.x; nstr.z = ts.z*tp2.x;
	nhei.x = th.x*tp2.y; nhei.y = th.y*tp2.y; nhei.z = th.z*tp2.y;
	nfor.x = tf.x*tp2.z; nfor.y = tf.y*tp2.z; nfor.z = tf.z*tp2.z;
	for(z=3;z>=0;z--) //72*,63+
	{
			//movaps xmm0, nx4      mulaps xmm0, ginor[0].x(dup 4)
			//movaps xmm1, ny4      mulaps xmm1, ginor[0].y(dup 4)
			//movaps xmm2, nz4      mulaps xmm2, ginor[0].z(dup 4)
			//addps xmm0, xmm1      addps xmm0, xmm2
			//andps xmm0, [0x7fffffff7fffffff7fffffffffffffff]
			//movhlps xmm1, xmm0    addps xmm0, xmm1
			//shufps xmm1, xmm0, 1  addss xmm0, xmm1
			//ucomiss xmm0, [0x0]   jnz retfunc
		if (fabs(nstr.x*ginor[z].x + nstr.y*ginor[z].y + nstr.z*ginor[z].z) +
			 fabs(nhei.x*ginor[z].x + nhei.y*ginor[z].y + nhei.z*ginor[z].z) +
			 fabs(nfor.x*ginor[z].x + nfor.y*ginor[z].y + nfor.z*ginor[z].z) +
					npos.x*ginor[z].x + npos.y*ginor[z].y + npos.z*ginor[z].z < 0) return;
	}
#if 0   //There are bugs when some vertices are behind ifor plane
	x = xres_voxlap; y = xres_voxlap; inx = 0; iny = 0;
	for(z=7;z>=0;z--) //This is useful for debugging.
	{
		tp.x = -kv->xpiv; if (z&1) tp.x += (float)kv->xsiz;
		tp.y = -kv->ypiv; if (z&2) tp.y += (float)kv->ysiz;
		tp.z = -kv->zpiv; if (z&4) tp.z += (float)kv->zsiz;
		drawspherefill(tp.x*ts.x+tp.y*th.x+tp.z*tf.x+spr->p.x,
							tp.x*ts.y+tp.y*th.y+tp.z*tf.y+spr->p.y,
							tp.x*ts.z+tp.y*th.z+tp.z*tf.z+spr->p.z,.5,0xf08040);
		tp2.x = tp.x*ts.x+tp.y*th.x+tp.z*tf.x+spr->p.x-gipos.x;
		tp2.y = tp.x*ts.y+tp.y*th.y+tp.z*tf.y+spr->p.y-gipos.y;
		tp2.z = tp.x*ts.z+tp.y*th.z+tp.z*tf.z+spr->p.z-gipos.z;
		tp.z = tp2.x*gifor.x + tp2.y*gifor.y + tp2.z*gifor.z; if (tp.z < 2) continue;
		tp.x = tp2.x*gistr.x + tp2.y*gistr.y + tp2.z*gistr.z;
		tp.y = tp2.x*gihei.x + tp2.y*gihei.y + tp2.z*gihei.z;
		ftol(tp.x*gihz/tp.z + gihx,&inz);
		if (inz < x) x = inz;
		if (inz > inx) inx = inz;
		ftol(tp.y*gihz/tp.z+gihy,&inz);
		if (inz < y) y = inz;
		if (inz > iny) iny = inz;
	}
	if (x < 0) x = 0;
	if (inx > xres_voxlap) inx = xres_voxlap;
	if (y < 0) y = 0;
	if (iny > yres_voxlap) iny = yres_voxlap;
	if (x < inx)
		for(inz=y;inz<iny;inz++)
			clearbuf((void *)(ylookup[inz]+(x<<2)+frameplace),inx-x,0L);
#endif

#if (USEZBUFFER == 0)
	lpoint3d lp;
	if (!cansee(&gipos,&spr->p,&lp)) return; //Very crappy Z-buffer!
#endif

	r0 = &ztab4[MAXZSIZ]; r1 = &ztab4[MAXZSIZ+1]; r2 = &ztab4[MAXZSIZ+2];

		//Rotate sprite from world to screen coordinates:
	mat2(&gixs,&giys,&gizs,&giadd, &ts,&th,&tf,&spr->p, &nstr,&nhei,&nfor,&npos);
	npos.x -= (kv->xpiv*nstr.x + kv->ypiv*nhei.x + kv->zpiv*nfor.x);
	npos.y -= (kv->xpiv*nstr.y + kv->ypiv*nhei.y + kv->zpiv*nfor.y);
	npos.z -= (kv->xpiv*nstr.z + kv->ypiv*nhei.z + kv->zpiv*nfor.z);

		//Find split point by using Cramer's rule
		//Must use Cramer's rule for non-orthonormal input matrices
	tp.x = nhei.y*nfor.z - nfor.y*nhei.z;
	tp.y = nfor.y*nstr.z - nstr.y*nfor.z;
	tp.z = nstr.y*nhei.z - nhei.y*nstr.z;
	f = nstr.x*tp.x + nhei.x*tp.y + nfor.x*tp.z;
	if (f != 0)
	{
		f = -1.0f / f;
		tp2.x = npos.y*nfor.z - nfor.y*npos.z;
		tp2.y = nhei.y*npos.z - npos.y*nhei.z;
		tp2.z = npos.y*nstr.z - nstr.y*npos.z;
		inx = (npos.x*tp.x - nhei.x*tp2.x - nfor.x*tp2.y)*f;
		iny = (npos.x*tp.y + nstr.x*tp2.x - nfor.x*tp2.z)*f;
		inz = (npos.x*tp.z + nstr.x*tp2.y + nhei.x*tp2.z)*f;
	}
	else { inx = iny = inz = -1; }
	inx = lbound(inx,-1,kv->xsiz);
	iny = lbound(iny,-1,kv->ysiz);
	inz = lbound(inz,-1,kv->zsiz);

	f = nhei.x; nhei.x = nfor.x; nfor.x = -f;
	f = nhei.y; nhei.y = nfor.y; nfor.y = -f;
	f = nhei.z; nhei.z = nfor.z; nfor.z = -f;

	if (kv->zsiz >= MAXZSIZ) return; //HACK TO PREVENT CRASHES FOR NOW... FIX!
	qsum0[2] = qsum0[0] = 0x7fff-(xres_voxlap-(long)gihx);
	qsum0[3] = qsum0[1] = 0x7fff-(yres_voxlap-(long)gihy);

		//r1->x = nstr.z; r1->y = nhei.z; r1->z = nfor.z;
		//minps(r1,r1,&ztab4[0]); //&ztab4[0] always 0
		//scisdist = -(r1->x + r1->y + r1->z);
	scisdist = 0;
	if (*(long *)&nstr.z < 0) scisdist -= nstr.z;
	if (*(long *)&nhei.z < 0) scisdist -= nhei.z;
	if (*(long *)&nfor.z < 0) scisdist -= nfor.z;

	cadd4[1].x = nstr.x*gihz; cadd4[1].y = nstr.y*gihz; cadd4[1].z = cadd4[1].z2 = nstr.z;
	cadd4[2].x = nhei.x*gihz; cadd4[2].y = nhei.y*gihz; cadd4[2].z = cadd4[2].z2 = nhei.z;
	cadd4[4].x = nfor.x*gihz; cadd4[4].y = nfor.y*gihz; cadd4[4].z = cadd4[4].z2 = nfor.z;
		  r1->x = npos.x*gihz;      r1->y = npos.y*gihz;      r1->z =      r1->z2 = npos.z;

	updatereflects(spr);
	//No more 8087 code after here!!! ----------------------------------------

	if (cputype&(1<<25))
	{
		addps(&cadd4[3],&cadd4[1],&cadd4[2]);
		addps(&cadd4[5],&cadd4[1],&cadd4[4]);
		addps(&cadd4[6],&cadd4[2],&cadd4[4]);
		addps(&cadd4[7],&cadd4[3],&cadd4[4]);

		for(z=1;z<kv->zsiz;z++) addps(&ztab4[z],&ztab4[z-1],&cadd4[2]);
		intss(r2,-kv->ysiz); mulps(r2,r2,&cadd4[4]);

		subps(r1,r1,&cadd4[4]); //ANNOYING HACK!!!

		#ifdef __GNUC__ //gcc inline asm
		__asm__ __volatile__
		(
			"movq	%c[q0], %%mm6\n"
			"movq	%c[q1], %%mm7\n"
			:
			: [q0] "p" (&qsum0), [q1] "p" (&qsum1)
			:
		);
		#endif
		#ifdef _MSC_VER //msvc inline asm
		_asm
		{
			movq	mm6, qsum0
			movq	mm7, qsum1
		}
		#endif

		xv = kv->vox; ylenptr = kv->ylen;
		for(x=0;x<inx;x++,ylenptr+=kv->ysiz)
		{
			if ((x < nxplanemin) || (x >= nxplanemax))
				{ xv += kv->xlen[x]; addps(r1,r1,&cadd4[1]); continue; }
			yv = xv+kv->xlen[x]; movps(r0,r1);
			for(y=0;y<iny;y++)
			{
				v0 = xv; xv += ylenptr[y]; v1 = xv-1;
				DRAWBOUNDCUBELINE(0xa)
				subps(r0,r0,&cadd4[4]);
			}
			xv = yv;
			addps(r0,r1,r2);
			addps(r1,r1,&cadd4[1]);
			for(y=kv->ysiz-1;y>iny;y--)
			{
				addps(r0,r0,&cadd4[4]);
				v1 = yv-1; yv -= ylenptr[y]; v0 = yv;
				DRAWBOUNDCUBELINE(0x6)
			}
			if ((unsigned long)iny < (unsigned long)kv->ysiz)
			{
				addps(r0,r0,&cadd4[4]);
				v1 = yv-1; yv -= ylenptr[y]; v0 = yv;
				DRAWBOUNDCUBELINE(0x2)
			}
		}
		xv = &kv->vox[kv->numvoxs]; ylenptr = &kv->ylen[(kv->xsiz-1)*kv->ysiz];
		intss(r0,kv->xsiz-x); mulps(r0,r0,&cadd4[1]); addps(r1,r1,r0);
		for(x=kv->xsiz-1;x>inx;x--,ylenptr-=kv->ysiz)
		{
			if ((x < nxplanemin) || (x >= nxplanemax))
				{ xv -= kv->xlen[x]; subps(r1,r1,&cadd4[1]); continue; }
			yv = xv-kv->xlen[x];
			subps(r1,r1,&cadd4[1]);
			addps(r0,r1,r2);
			for(y=kv->ysiz-1;y>iny;y--)
			{
				addps(r0,r0,&cadd4[4]);
				v1 = xv-1; xv -= ylenptr[y]; v0 = xv;
				DRAWBOUNDCUBELINE(0x5)
			}
			xv = yv; movps(r0,r1);
			for(y=0;y<iny;y++)
			{
				v0 = yv; yv += ylenptr[y]; v1 = yv-1;
				DRAWBOUNDCUBELINE(0x9)
				subps(r0,r0,&cadd4[4]);
			}
			if ((unsigned long)iny < (unsigned long)kv->ysiz)
			{
				v0 = yv; yv += ylenptr[y]; v1 = yv-1;
				DRAWBOUNDCUBELINE(0x1)
			}
		}
		if ((unsigned long)inx < (unsigned long)kv->xsiz)
		{
			if ((x < nxplanemin) || (x >= nxplanemax)) { { clearMMX(); } return;}
			yv = xv-kv->xlen[x];
			subps(r1,r1,&cadd4[1]);
			addps(r0,r1,r2);
			for(y=kv->ysiz-1;y>iny;y--)
			{
				addps(r0,r0,&cadd4[4]);
				v1 = xv-1; xv -= ylenptr[y]; v0 = xv;
				DRAWBOUNDCUBELINE(0x4)
			}
			xv = yv; movps(r0,r1);
			for(y=0;y<iny;y++)
			{
				v0 = yv; yv += ylenptr[y]; v1 = yv-1;
				DRAWBOUNDCUBELINE(0x8)
				subps(r0,r0,&cadd4[4]);
			}
			if ((unsigned long)iny < (unsigned long)kv->ysiz)
			{
				v0 = yv; yv += ylenptr[y]; v1 = yv-1;
				DRAWBOUNDCUBELINE(0x0)
			}
		}
	}
	else
	{
		addps_3dn(&cadd4[3],&cadd4[1],&cadd4[2]);
		addps_3dn(&cadd4[5],&cadd4[1],&cadd4[4]);
		addps_3dn(&cadd4[6],&cadd4[2],&cadd4[4]);
		addps_3dn(&cadd4[7],&cadd4[3],&cadd4[4]);

		for(z=1;z<kv->zsiz;z++) addps_3dn(&ztab4[z],&ztab4[z-1],&cadd4[2]);
		intss_3dn(r2,-kv->ysiz); mulps_3dn(r2,r2,&cadd4[4]);

		subps_3dn(r1,r1,&cadd4[4]); //ANNOYING HACK!!!

		#ifdef __GNUC__ //gcc inline asm
		__asm__ __volatile__
		(
			"movq	%c[q0], %%mm6\n"
			"movq	%c[q1], %%mm7\n"
			:
			: [q0] "p" (&qsum0), [q1] "p" (&qsum1)
			:
		);
		#endif
		#ifdef _MSC_VER //msvc inline asm
		_asm
		{
			movq	mm6, qsum0
			movq	mm7, qsum1
		}
		#endif

		xv = kv->vox; ylenptr = kv->ylen;
		for(x=0;x<inx;x++,ylenptr+=kv->ysiz)
		{
			if ((x < nxplanemin) || (x >= nxplanemax))
				{ xv += kv->xlen[x]; addps_3dn(r1,r1,&cadd4[1]); continue; }
			yv = xv+kv->xlen[x]; movps_3dn(r0,r1);
			for(y=0;y<iny;y++)
			{
				v0 = xv; xv += ylenptr[y]; v1 = xv-1;
				DRAWBOUNDCUBELINE_3DN(0xa)
				subps_3dn(r0,r0,&cadd4[4]);
			}
			xv = yv;
			addps_3dn(r0,r1,r2);
			addps_3dn(r1,r1,&cadd4[1]);
			for(y=kv->ysiz-1;y>iny;y--)
			{
				addps_3dn(r0,r0,&cadd4[4]);
				v1 = yv-1; yv -= ylenptr[y]; v0 = yv;
				DRAWBOUNDCUBELINE_3DN(0x6)
			}
			if ((unsigned long)iny < (unsigned long)kv->ysiz)
			{
				addps_3dn(r0,r0,&cadd4[4]);
				v1 = yv-1; yv -= ylenptr[y]; v0 = yv;
				DRAWBOUNDCUBELINE_3DN(0x2)
			}
		}
		xv = &kv->vox[kv->numvoxs]; ylenptr = &kv->ylen[(kv->xsiz-1)*kv->ysiz];
		intss_3dn(r0,kv->xsiz-x); mulps_3dn(r0,r0,&cadd4[1]); addps_3dn(r1,r1,r0);
		for(x=kv->xsiz-1;x>inx;x--,ylenptr-=kv->ysiz)
		{
			if ((x < nxplanemin) || (x >= nxplanemax))
				{ xv -= kv->xlen[x]; subps_3dn(r1,r1,&cadd4[1]); continue; }
			yv = xv-kv->xlen[x];
			subps_3dn(r1,r1,&cadd4[1]);
			addps_3dn(r0,r1,r2);
			for(y=kv->ysiz-1;y>iny;y--)
			{
				addps_3dn(r0,r0,&cadd4[4]);
				v1 = xv-1; xv -= ylenptr[y]; v0 = xv;
				DRAWBOUNDCUBELINE_3DN(0x5)
			}
			xv = yv; movps_3dn(r0,r1);
			for(y=0;y<iny;y++)
			{
				v0 = yv; yv += ylenptr[y]; v1 = yv-1;
				DRAWBOUNDCUBELINE_3DN(0x9)
				subps_3dn(r0,r0,&cadd4[4]);
			}
			if ((unsigned long)iny < (unsigned long)kv->ysiz)
			{
				v0 = yv; yv += ylenptr[y]; v1 = yv-1;
				DRAWBOUNDCUBELINE_3DN(0x1)
			}
		}
		if ((unsigned long)inx < (unsigned long)kv->xsiz)
		{
			if ((x < nxplanemin) || (x >= nxplanemax)) { { clearMMX(); } return;}
			yv = xv-kv->xlen[x];
			subps_3dn(r1,r1,&cadd4[1]);
			addps_3dn(r0,r1,r2);
			for(y=kv->ysiz-1;y>iny;y--)
			{
				addps_3dn(r0,r0,&cadd4[4]);
				v1 = xv-1; xv -= ylenptr[y]; v0 = xv;
				DRAWBOUNDCUBELINE_3DN(0x4)
			}
			xv = yv; movps_3dn(r0,r1);
			for(y=0;y<iny;y++)
			{
				v0 = yv; yv += ylenptr[y]; v1 = yv-1;
				DRAWBOUNDCUBELINE_3DN(0x8)
				subps_3dn(r0,r0,&cadd4[4]);
			}
			if ((unsigned long)iny < (unsigned long)kv->ysiz)
			{
				v0 = yv; yv += ylenptr[y]; v1 = yv-1;
				DRAWBOUNDCUBELINE_3DN(0x0)
			}
		}
	}
	clearMMX();
}

//-------------------------- KFA sprite code begins --------------------------

static kv6voxtype *getvptr (kv6data *kv, long x, long y)
{
	kv6voxtype *v;
	long i, j;

	v = kv->vox;
	if ((x<<1) < kv->xsiz) { for(i=0         ;i< x;i++) v += kv->xlen[i]; }
	else { v += kv->numvoxs; for(i=kv->xsiz-1;i>=x;i--) v -= kv->xlen[i]; }
	j = x*kv->ysiz;
	if ((y<<1) < kv->ysiz) { for(i=0         ;i< y;i++) v += kv->ylen[j+i]; }
	else { v += kv->xlen[x]; for(i=kv->ysiz-1;i>=y;i--) v -= kv->ylen[j+i]; }
	return(v);
}

#define VFIFSIZ 16384 //SHOULDN'T BE STATIC ALLOCATION!!!
static long vfifo[VFIFSIZ];
static void floodsucksprite (vx5sprite *spr, kv6data *kv, long ox, long oy,
									  kv6voxtype *v0, kv6voxtype *v1)
{
	kv6voxtype *v, *ve, *ov, *v2, *v3;
	kv6data *kv6;
	long i, j, x, y, z, x0, y0, z0, x1, y1, z1, n, vfif0, vfif1;

	x0 = x1 = ox; y0 = y1 = oy; z0 = v0->z; z1 = v1->z;

	n = (((long)v1)-((long)v0))/sizeof(kv6voxtype)+1;
	v1->vis &= ~64;

	vfifo[0] = ox; vfifo[1] = oy;
	vfifo[2] = (long)v0; vfifo[3] = (long)v1;
	vfif0 = 0; vfif1 = 4;

	while (vfif0 < vfif1)
	{
		i = (vfif0&(VFIFSIZ-1)); vfif0 += 4;
		ox = vfifo[i]; oy = vfifo[i+1];
		v0 = (kv6voxtype *)vfifo[i+2]; v1 = (kv6voxtype *)vfifo[i+3];

		if (ox < x0) x0 = ox;
		if (ox > x1) x1 = ox;
		if (oy < y0) y0 = oy;
		if (oy > y1) y1 = oy;
		if (v0->z < z0) z0 = v0->z;
		if (v1->z > z1) z1 = v1->z;
		for(v=v0;v<=v1;v++) v->vis |= 128; //Mark as part of current piece

		for(j=0;j<4;j++)
		{
			switch(j)
			{
				case 0: x = ox-1; y = oy; break;
				case 1: x = ox+1; y = oy; break;
				case 2: x = ox; y = oy-1; break;
				case 3: x = ox; y = oy+1; break;
				default: _gtfo(); //tells MSVC default can't be reached
			}
			if ((unsigned long)x >= kv->xsiz) continue;
			if ((unsigned long)y >= kv->ysiz) continue;

			v = getvptr(kv,x,y);
			for(ve=&v[kv->ylen[x*kv->ysiz+y]];v<ve;v++)
			{
				if (v->vis&16) ov = v;
				if (((v->vis&(64+32)) == 64+32) && (v0->z <= v->z) && (v1->z >= ov->z))
				{
					i = (vfif1&(VFIFSIZ-1)); vfif1 += 4;
					if (vfif1-vfif0 >= VFIFSIZ) //FIFO Overflow... make entire object 1 piece :/
					{
						for(i=kv->numvoxs-1;i>=0;i--)
						{
							if ((kv->vox[i].vis&(64+32)) == 64+32) { v1 = &kv->vox[i]; v1->vis &= ~64; }
							if (kv->vox[i].vis&16) for(v=&kv->vox[i];v<=v1;v++) kv->vox[i].vis |= 128;
						}
						x0 = y0 = z0 = 0; x1 = kv->xsiz; y1 = kv->ysiz; z1 = kv->zsiz; n = kv->numvoxs;
						goto floodsuckend;
					}
					vfifo[i] = x; vfifo[i+1] = y;
					vfifo[i+2] = (long)ov; vfifo[i+3] = (long)v;
					n += (((long)v)-((long)ov))/sizeof(kv6voxtype)+1;
					v->vis &= ~64;
				}
			}
		}
	}
	x1++; y1++; z1++;
floodsuckend:;

	i = sizeof(kv6data) + n*sizeof(kv6voxtype) + (x1-x0)*4 + (x1-x0)*(y1-y0)*2;
	if (!(kv6 = (kv6data *)malloc(i))) return;
	kv6->leng = i;
	kv6->xsiz = x1-x0;
	kv6->ysiz = y1-y0;
	kv6->zsiz = z1-z0;
	kv6->xpiv = 0; //Set limb pivots to 0 - don't need it twice!
	kv6->ypiv = 0;
	kv6->zpiv = 0;
	kv6->numvoxs = n;
	kv6->namoff = 0;
	kv6->lowermip = 0;
	kv6->vox = (kv6voxtype *)(((long)kv6)+sizeof(kv6data));
	kv6->xlen = (unsigned long *)(((long)kv6->vox)+n*sizeof(kv6voxtype));
	kv6->ylen = (unsigned short *)(((long)kv6->xlen)+(x1-x0)*4);

		//Extract sub-KV6 to newly allocated kv6data
	v3 = kv6->vox; n = 0;
	for(x=0,v=kv->vox;x<x0;x++) v += kv->xlen[x];
	for(;x<x1;x++)
	{
		v2 = v; ox = n;
		for(y=0;y<y0;y++) v += kv->ylen[x*kv->ysiz+y];
		for(;y<y1;y++)
		{
			oy = n;
			for(ve=&v[kv->ylen[x*kv->ysiz+y]];v<ve;v++)
				if (v->vis&128)
				{
					v->vis &= ~128;
					(*v3) = (*v);
					v3->z -= z0;
					v3++; n++;
				}
			kv6->ylen[(x-x0)*(y1-y0)+(y-y0)] = n-oy;
		}
		kv6->xlen[x-x0] = n-ox;
		v = v2+kv->xlen[x];
	}

	spr->p.x = x0-kv->xpiv;
	spr->p.y = y0-kv->ypiv;
	spr->p.z = z0-kv->zpiv;
	spr->s.x = 1; spr->s.y = 0; spr->s.z = 0;
	spr->h.x = 0; spr->h.y = 1; spr->h.z = 0;
	spr->f.x = 0; spr->f.y = 0; spr->f.z = 1;
	spr->voxnum = kv6;
	spr->flags = 0;
}
/** Skip past single forwad slash or double back slash in filename.
 * @param filnam Pointer to string containing filname
 * @param filnam Pointer to string containing filname after stripping leading / or double backslash
*/
static char *stripdir (char *filnam)
{
	long i, j;
	for(i=0,j=-1;filnam[i];i++)
		if ((filnam[i] == '/') || (filnam[i] == '\\')) j = i;
	return(&filnam[j+1]);
}

static void kfasorthinge (hingetype *h, long nh, long *hsort)
{
	long i, j, n;

		//First pass: stick hinges with parent=-1 at end
	n = nh; j = 0;
	for(i=n-1;i>=0;i--)
	{
		if (h[i].parent < 0) hsort[--n] = i;
							 else hsort[j++] = i;
	}
		//Finish accumulation (n*log(n) if tree is perfectly balanced)
	while (n > 0)
	{
		i--; if (i < 0) i = n-1;
		j = hsort[i];
		if (h[h[j].parent].parent < 0)
		{
			h[j].parent = -2-h[j].parent; n--;
			hsort[i] = hsort[n]; hsort[n] = j;
		}
	}
		//Restore parents to original values
	for(i=nh-1;i>=0;i--) h[i].parent = -2-h[i].parent;
}

/**
 * Loads a .KFA file and its associated .KV6 voxel sprite into memory. Works
 * just like getkv6() for for .KFA files: (Returns a pointer to the loaded
 * kfatype structure. Loads data only if not already loaded before with
 * getkfa.)
 *
 * @param kfanam .KFA filename
 * @return pointer to malloc'ed kfatype structure. Do NOT free this buffer
 *         yourself! Returns 0 if there's an error - such as bad filename.
 */
kfatype *getkfa (const char *kfanam)
{
	kfatype *kfa;
	kv6voxtype *v, *ov, *ve;
	kv6data *kv;
	long i, j, x, y;
	char *cptr, snotbuf[MAX_PATH];

	if (inkhash(kfanam,&i)) return(*(kfatype **)&khashbuf[i+4]);
	if (i == -1) return(0);

	if (!(kfa = (kfatype *)malloc(sizeof(kfatype)))) return(0);
	memset(kfa,0,sizeof(kfatype));

	kfa->namoff = i+9; //Must use offset for ptr->name conversion
	*(kfatype **)&khashbuf[i+4] = kfa;
	*(char *)&khashbuf[i+8] = 1; //1 for KFA

	if (!kzopen(kfanam)) return(0);
	kzread(&i,4); if (i != 0x6b6c774b) { kzclose(); return(0); } //Kwlk
	kzread(&i,4); strcpy(snotbuf,kfanam); cptr = stripdir(snotbuf);
	kzread(cptr,i); cptr[i] = 0;
	kzread(&kfa->numhin,4);

		//Actual allocation for ->spr is numspr, which is <= numhin!
	if (!(kfa->spr = (vx5sprite *)malloc(kfa->numhin*sizeof(vx5sprite)))) { kzclose(); return(0); }

	if (!(kfa->hinge = (hingetype *)malloc(kfa->numhin*sizeof(hingetype)))) { kzclose(); return(0); }
	kzread(kfa->hinge,kfa->numhin*sizeof(hingetype));

	kzread(&kfa->numfrm,4);
	if (!(kfa->frmval = (short *)malloc(kfa->numfrm*kfa->numhin*2))) { kzclose(); return(0); }
	kzread(kfa->frmval,kfa->numfrm*kfa->numhin*2);

	kzread(&kfa->seqnum,4);
	if (!(kfa->seq = (seqtyp *)malloc(kfa->seqnum*sizeof(seqtyp)))) { kzclose(); return(0); }
	kzread(kfa->seq,kfa->seqnum*sizeof(seqtyp));

	kzclose();

		//MUST load the associated KV6 AFTER the kzclose :/
	kfa->numspr = 0;
	kv = getkv6(snotbuf); if (!kv) return(0);
	kfa->basekv6 = kv;
	if (!kv->numvoxs) return(0);
	v = kv->vox;
	for(x=kv->numvoxs-1;x>=0;x--) v[x].vis |= ((v[x].vis&32)<<1);
	for(x=0;x<kv->xsiz;x++)
		for(y=0;y<kv->ysiz;y++)
			for(ve=&v[kv->ylen[x*kv->ysiz+y]];v<ve;v++)
			{
				if (v->vis&16) ov = v;
				if ((v->vis&(64+32)) == 64+32)
				{
					floodsucksprite(&kfa->spr[kfa->numspr],kv,x,y,ov,v);
					kfa->numspr++;
				}
			}

	kfa->hingesort = (long *)malloc(kfa->numhin*4);
	kfasorthinge(kfa->hinge,kfa->numhin,kfa->hingesort);

		//Remember position offsets of limbs with no parent in hinge[?].p[0]
	for(i=(kfa->numhin)-1;i>=0;i--)
	{
		j = kfa->hingesort[i]; if (j >= kfa->numspr) continue;
		if (kfa->hinge[j].parent < 0)
		{
			kfa->hinge[j].p[0].x = -kfa->spr[j].p.x;
			kfa->hinge[j].p[0].y = -kfa->spr[j].p.y;
			kfa->hinge[j].p[0].z = -kfa->spr[j].p.z;
		}
	}

	return(kfa);
}

/**
 * Cover-up function to handle both .KV6 and .KFA files. It looks at the
 * filename extension and uses the appropriate function (either getkv6
 * or getkfa) and sets the sprite flags depending on the type of file.
 * The file must have either .KV6 or .KFA as the filename extension. If
 * you want to use weird filenames, then use getkv6/getkfa instead.
 *
 * @param spr Pointer to sprite structure that you provide. getspr() writes:
 *            only to the kv6data/voxtype, kfatim, and flags members.
 * @param filnam filename of either a .KV6 or .KFA file.
 */
void getspr (vx5sprite *s, const char *filnam)
{
	long i;

	if (!filnam) return;
	i = strlen(filnam); if (!i) return;

	if ((filnam[i-1] == 'A') || (filnam[i-1] == 'a'))
		{ s->kfaptr = getkfa(filnam); s->flags = 2; s->kfatim = 0; }
	else if (filnam[i-1] == '6')
		{ s->voxnum = getkv6(filnam); s->flags = 0; }
}

/** Given vector a, returns b&c that makes (a,b,c) orthonormal */
void genperp (point3d *a, point3d *b, point3d *c)
{
	float t;

	if ((a->x == 0) && (a->y == 0) && (a->z == 0))
		{ b->x = 0; b->y = 0; b->z = 0; return; }
	if ((fabs(a->x) < fabs(a->y)) && (fabs(a->x) < fabs(a->z)))
		{ t = 1.0 / sqrt(a->y*a->y + a->z*a->z); b->x = 0; b->y = a->z*t; b->z = -a->y*t; }
	else if (fabs(a->y) < fabs(a->z))
		{ t = 1.0 / sqrt(a->x*a->x + a->z*a->z); b->x = -a->z*t; b->y = 0; b->z = a->x*t; }
	else
		{ t = 1.0 / sqrt(a->x*a->x + a->y*a->y); b->x = a->y*t; b->y = -a->x*t; b->z = 0; }
	c->x = a->y*b->z - a->z*b->y;
	c->y = a->z*b->x - a->x*b->z;
	c->z = a->x*b->y - a->y*b->x;
}

	//A * B = C, find A   36*, 27ñ
	//[asx ahx agx aox][bsx bhx bgx box]   [csx chx cgx cox]
	//[asy ahy agy aoy][bsy bhy bgy boy] = [csy chy cgy coy]
	//[asz ahz agz aoz][bsz bhz bgz boz]   [csz chz cgz coz]
	//[  0   0   0   1][  0   0   0   1]   [  0   0   0   1]
void mat0 (point3d *as, point3d *ah, point3d *ag, point3d *ao,
		   point3d *bs, point3d *bh, point3d *bg, point3d *bo,
		   point3d *cs, point3d *ch, point3d *cg, point3d *co)
{
	point3d ts, th, tg, to;
	ts.x = bs->x*cs->x + bh->x*ch->x + bg->x*cg->x;
	ts.y = bs->x*cs->y + bh->x*ch->y + bg->x*cg->y;
	ts.z = bs->x*cs->z + bh->x*ch->z + bg->x*cg->z;
	th.x = bs->y*cs->x + bh->y*ch->x + bg->y*cg->x;
	th.y = bs->y*cs->y + bh->y*ch->y + bg->y*cg->y;
	th.z = bs->y*cs->z + bh->y*ch->z + bg->y*cg->z;
	tg.x = bs->z*cs->x + bh->z*ch->x + bg->z*cg->x;
	tg.y = bs->z*cs->y + bh->z*ch->y + bg->z*cg->y;
	tg.z = bs->z*cs->z + bh->z*ch->z + bg->z*cg->z;
	to.x = co->x - bo->x*ts.x - bo->y*th.x - bo->z*tg.x;
	to.y = co->y - bo->x*ts.y - bo->y*th.y - bo->z*tg.y;
	to.z = co->z - bo->x*ts.z - bo->y*th.z - bo->z*tg.z;
	(*as) = ts; (*ah) = th; (*ag) = tg; (*ao) = to;
}

	//A * B = C, find B   36*, 27ñ
	//[asx ahx agx aox][bsx bhx bgx box]   [csx chx cgx cox]
	//[asy ahy agy aoy][bsy bhy bgy boy] = [csy chy cgy coy]
	//[asz ahz agz aoz][bsz bhz bgz boz]   [csz chz cgz coz]
	//[  0   0   0   1][  0   0   0   1]   [  0   0   0   1]
void mat1 (point3d *as, point3d *ah, point3d *ag, point3d *ao,
		   point3d *bs, point3d *bh, point3d *bg, point3d *bo,
		   point3d *cs, point3d *ch, point3d *cg, point3d *co)
{
	point3d ts, th, tg, to;
	float x = co->x-ao->x, y = co->y-ao->y, z = co->z-ao->z;
	ts.x = cs->x*as->x + cs->y*as->y + cs->z*as->z;
	ts.y = cs->x*ah->x + cs->y*ah->y + cs->z*ah->z;
	ts.z = cs->x*ag->x + cs->y*ag->y + cs->z*ag->z;
	th.x = ch->x*as->x + ch->y*as->y + ch->z*as->z;
	th.y = ch->x*ah->x + ch->y*ah->y + ch->z*ah->z;
	th.z = ch->x*ag->x + ch->y*ag->y + ch->z*ag->z;
	tg.x = cg->x*as->x + cg->y*as->y + cg->z*as->z;
	tg.y = cg->x*ah->x + cg->y*ah->y + cg->z*ah->z;
	tg.z = cg->x*ag->x + cg->y*ag->y + cg->z*ag->z;
	to.x = as->x*x + as->y*y + as->z*z;
	to.y = ah->x*x + ah->y*y + ah->z*z;
	to.z = ag->x*x + ag->y*y + ag->z*z;
	(*bs) = ts; (*bh) = th; (*bg) = tg; (*bo) = to;
}

	//A * B = C, find C   36*, 27ñ
	//[asx ahx afx aox][bsx bhx bfx box]   [csx chx cfx cox]
	//[asy ahy afy aoy][bsy bhy bfy boy] = [csy chy cfy coy]
	//[asz ahz afz aoz][bsz bhz bfz boz]   [csz chz cfz coz]
	//[  0   0   0   1][  0   0   0   1]   [  0   0   0   1]
void mat2 (point3d *a_s, point3d *a_h, point3d *a_f, point3d *a_o,
		   point3d *b_s, point3d *b_h, point3d *b_f, point3d *b_o,
		   point3d *c_s, point3d *c_h, point3d *c_f, point3d *c_o)
{
	point3d ts, th, tf, to;
	ts.x = a_s->x*b_s->x + a_h->x*b_s->y + a_f->x*b_s->z;
	ts.y = a_s->y*b_s->x + a_h->y*b_s->y + a_f->y*b_s->z;
	ts.z = a_s->z*b_s->x + a_h->z*b_s->y + a_f->z*b_s->z;
	th.x = a_s->x*b_h->x + a_h->x*b_h->y + a_f->x*b_h->z;
	th.y = a_s->y*b_h->x + a_h->y*b_h->y + a_f->y*b_h->z;
	th.z = a_s->z*b_h->x + a_h->z*b_h->y + a_f->z*b_h->z;
	tf.x = a_s->x*b_f->x + a_h->x*b_f->y + a_f->x*b_f->z;
	tf.y = a_s->y*b_f->x + a_h->y*b_f->y + a_f->y*b_f->z;
	tf.z = a_s->z*b_f->x + a_h->z*b_f->y + a_f->z*b_f->z;
	to.x = a_s->x*b_o->x + a_h->x*b_o->y + a_f->x*b_o->z + a_o->x;
	to.y = a_s->y*b_o->x + a_h->y*b_o->y + a_f->y*b_o->z + a_o->y;
	to.z = a_s->z*b_o->x + a_h->z*b_o->y + a_f->z*b_o->z + a_o->z;
	(*c_s) = ts; (*c_h) = th; (*c_f) = tf; (*c_o) = to;
}

static void setlimb (kfatype *kfa, long i, long p, long trans_type, short val)
{
	point3d ps, ph, pf, pp;
	point3d qs, qh, qf, qp;
	float r[2];

		//Generate orthonormal matrix in world space for child limb
	qp = kfa->hinge[i].p[0]; qs = kfa->hinge[i].v[0]; genperp(&qs,&qh,&qf);

	switch (trans_type)
	{
		case 0: //Hinge rotate!
			//fcossin(((float)val)*(PI/32768.0),&c,&s);
			ucossin(((long)val)<<16,r);
			ph = qh; pf = qf;
			qh.x = ph.x*r[0] - pf.x*r[1]; qf.x = ph.x*r[1] + pf.x*r[0];
			qh.y = ph.y*r[0] - pf.y*r[1]; qf.y = ph.y*r[1] + pf.y*r[0];
			qh.z = ph.z*r[0] - pf.z*r[1]; qf.z = ph.z*r[1] + pf.z*r[0];
			break;
		default: _gtfo(); //tells MSVC default can't be reached
	}

		//Generate orthonormal matrix in world space for parent limb
	pp = kfa->hinge[i].p[1]; ps = kfa->hinge[i].v[1]; genperp(&ps,&ph,&pf);

		//mat0(rotrans, loc_velcro, par_velcro)
	mat0(&qs,&qh,&qf,&qp, &qs,&qh,&qf,&qp, &ps,&ph,&pf,&pp);
		//mat2(par, rotrans, parent * par_velcro * (loc_velcro x rotrans)^-1)
	mat2(&kfa->spr[p].s,&kfa->spr[p].h,&kfa->spr[p].f,&kfa->spr[p].p,
		  &qs,&qh,&qf,&qp,
		  &kfa->spr[i].s,&kfa->spr[i].h,&kfa->spr[i].f,&kfa->spr[i].p);
}

	//Uses binary search to find sequence index at time "tim"
static long kfatime2seq (kfatype *kfa, long tim)
{
	long i, a, b;

	for(a=0,b=(kfa->seqnum)-1;b-a>=2;)
		{ i = ((a+b)>>1); if (tim >= kfa->seq[i].tim) a = i; else b = i; }
	return(a);
}

/**
 * You could animate .KFA sprites by simply modifying the .kfatim member of
 * vx5sprite structure. A better way is to use this function because it
 * will handle repeat/stop markers for you.
 *
 * @param spr .KFA sprite to animate
 * @param timeadd number of milliseconds to add to the current animation time
 */
void animsprite (vx5sprite *s, long ti)
{
	kfatype *kfa;
	long i, j, k, x, y, z, zz, trat;
	long trat2, z0, zz0, frm0;

	if (!(s->flags&2)) return;
	kfa = s->kfaptr; if (!kfa) return;

	z = kfatime2seq(kfa,s->kfatim);
	while (ti > 0)
	{
		z++; if (z >= kfa->seqnum) break;
		i = kfa->seq[z].tim-s->kfatim; if (i <= 0) break;
		if (i > ti) { s->kfatim += ti; break; }
		ti -= i;
		zz = ~kfa->seq[z].frm; if (zz >= 0) { if (z == zz) break; z = zz; }
		s->kfatim = kfa->seq[z].tim;
	}

// --------------------------------------------------------------------------

	z = kfatime2seq(kfa,s->kfatim); zz = z+1;
	if ((zz < kfa->seqnum) && (kfa->seq[zz].frm != ~zz))
	{
		trat = kfa->seq[zz].tim-kfa->seq[z].tim;
		if (trat) trat = shldiv16(s->kfatim-kfa->seq[z].tim,trat);
		i = kfa->seq[zz].frm; if (i < 0) zz = kfa->seq[~i].frm; else zz = i;
	} else trat = 0;
	z = kfa->seq[z].frm;
	if (z < 0)
	{
		z0 = kfatime2seq(kfa,s->okfatim); zz0 = z0+1;
		if ((zz0 < kfa->seqnum) && (kfa->seq[zz0].frm != ~zz0))
		{
			trat2 = kfa->seq[zz0].tim-kfa->seq[z0].tim;
			if (trat2) trat2 = shldiv16(s->okfatim-kfa->seq[z0].tim,trat2);
			i = kfa->seq[zz0].frm; if (i < 0) zz0 = kfa->seq[~i].frm; else zz0 = i;
		} else trat2 = 0;
		z0 = kfa->seq[z0].frm; if (z0 < 0) { z0 = zz0; trat2 = 0; }
	} else trat2 = -1;

	for(i=(kfa->numhin)-1;i>=0;i--)
	{
		if (kfa->hinge[i].parent < 0) continue;

		if (trat2 < 0) frm0 = (long)kfa->frmval[z*(kfa->numhin)+i];
		else
		{
			frm0 = (long)kfa->frmval[z0*(kfa->numhin)+i];
			if (trat2 > 0)
			{
				x = (((long)(kfa->frmval[zz0*(kfa->numhin)+i]-frm0))&65535);
				if (kfa->hinge[i].vmin == kfa->hinge[i].vmax) x = ((x<<16)>>16);
				else if ((((long)(kfa->frmval[zz0*(kfa->numhin)+i]-kfa->hinge[i].vmin))&65535) <
							(((long)(frm0-kfa->hinge[i].vmin))&65535))
					x -= 65536;
				frm0 += mulshr16(x,trat2);
			}
		}
		if (trat > 0)
		{
			x = (((long)(kfa->frmval[zz*(kfa->numhin)+i]-frm0))&65535);
			if (kfa->hinge[i].vmin == kfa->hinge[i].vmax) x = ((x<<16)>>16);
			else if ((((long)(kfa->frmval[zz*(kfa->numhin)+i]-kfa->hinge[i].vmin))&65535) <
						(((long)(frm0-kfa->hinge[i].vmin))&65535))
				x -= 65536;
			frm0 += mulshr16(x,trat);
		}
		vx5.kfaval[i] = frm0;
	}
}

static void kfadraw (vx5sprite *s)
{
	point3d tp;
	kfatype *kfa;
	long i, j, k;

	kfa = s->kfaptr; if (!kfa) return;

	for(i=(kfa->numhin)-1;i>=0;i--)
	{
		j = kfa->hingesort[i]; k = kfa->hinge[j].parent;
		if (k >= 0) setlimb(kfa,j,k,kfa->hinge[j].htype,vx5.kfaval[j]);
		else
		{
			kfa->spr[j].s = s->s;
			kfa->spr[j].h = s->h;
			kfa->spr[j].f = s->f;
			//kfa->spr[j].p = s->p;
			tp.x = kfa->hinge[j].p[0].x;
			tp.y = kfa->hinge[j].p[0].y;
			tp.z = kfa->hinge[j].p[0].z;
			kfa->spr[j].p.x = s->p.x - tp.x*s->s.x - tp.y*s->h.x - tp.z*s->f.x;
			kfa->spr[j].p.y = s->p.y - tp.x*s->s.y - tp.y*s->h.y - tp.z*s->f.y;
			kfa->spr[j].p.z = s->p.z - tp.x*s->s.z - tp.y*s->h.z - tp.z*s->f.z;
		}
		if (j < kfa->numspr) kv6draw(&kfa->spr[j]);
	}
}



/** Draw a .KV6/.KFA voxel sprite to the screen.
 *  Position & orientation are
 *  specified in the vx5sprite structure. See VOXLAP5.H for details on the
 *  structure.
 *
 *  @param spr pointer to vx5sprite
 */
void drawsprite (vx5sprite *spr)
{
	if (spr->flags&4) return;
	if (!(spr->flags&2)) kv6draw(spr); else kfadraw(spr);
}

#if 0

void setkv6 (vx5sprite *spr)
{
	point3d r0, r1;
	long x, y, vx, vy, vz;
	kv6data *kv;
	kv6voxtype *v, *ve;

	if (spr->flags&2) return;
	kv = spr->voxnum; if (!kv) return;

	vx5.minx = ?; vx5.maxx = ?+1;
	vx5.miny = ?; vx5.maxy = ?+1;
	vx5.minz = ?; vx5.maxz = ?+1;

	v = kv->vox; //.01 is to fool rounding so they aren't all even numbers
	r0.x = spr->p.x - kv->xpiv*spr->s.x - kv->ypiv*spr->h.x - kv->zpiv*spr->f.x - .01;
	r0.y = spr->p.y - kv->xpiv*spr->s.y - kv->ypiv*spr->h.y - kv->zpiv*spr->f.y - .01;
	r0.z = spr->p.z - kv->xpiv*spr->s.z - kv->ypiv*spr->h.z - kv->zpiv*spr->f.z - .01;
	vx5.colfunc = curcolfunc;
	for(x=0;x<kv->xsiz;x++)
	{
		r1 = r0;
		for(y=0;y<kv->ysiz;y++)
		{
			for(ve=&v[kv->ylen[x*kv->ysiz+y]];v<ve;v++)
			{
				ftol(spr->f.x*v->z + r1.x,&vx);
				ftol(spr->f.y*v->z + r1.y,&vy);
				ftol(spr->f.z*v->z + r1.z,&vz);
				vx5.curcol = ((v->col&0xffffff)|0x80000000);
				setcube(vx,vy,vz,-2);
			}
			r1.x += spr->h.x; r1.y += spr->h.y; r1.z += spr->h.z;
		}
		r0.x += spr->s.x; r0.y += spr->s.y; r0.z += spr->s.z;
	}
}

#else


static kv6data *gfrezkv;
static lpoint3d gfrezx, gfrezy, gfrezz, gfrezp;
static signed char gkv6colx[27] = {0,  0, 0, 0, 0, 1,-1, -1,-1,-1,-1, 0, 0, 0, 0, 1, 1, 1, 1,  1, 1, 1, 1,-1,-1,-1,-1};
static signed char gkv6coly[27] = {0,  0, 0, 1,-1, 0, 0,  0, 0,-1, 1, 1, 1,-1,-1, 0, 0,-1, 1,  1, 1,-1,-1, 1, 1,-1,-1};
static signed char gkv6colz[27] = {0,  1,-1, 0, 0, 0, 0, -1, 1, 0, 0, 1,-1, 1,-1, 1,-1, 0, 0,  1,-1, 1,-1, 1,-1, 1,-1};
long kv6colfunc (lpoint3d *p)
{
	kv6voxtype *v0, *v1, *v, *ve;
	long i, j, k, x, y, z, ox, oy, nx, ny, nz, mind, d;

	x = ((p->x*gfrezx.x + p->y*gfrezy.x + p->z*gfrezz.x + gfrezp.x)>>16);
	y = ((p->x*gfrezx.y + p->y*gfrezy.y + p->z*gfrezz.y + gfrezp.y)>>16);
	z = ((p->x*gfrezx.z + p->y*gfrezy.z + p->z*gfrezz.z + gfrezp.z)>>16);
	x = lbound0(x,gfrezkv->xsiz-1);
	y = lbound0(y,gfrezkv->ysiz-1);
	z = lbound0(z,gfrezkv->zsiz-1);

		//Process x
	v0 = gfrezkv->vox;
	if ((x<<1) < gfrezkv->xsiz) { ox = oy = j = 0; }
	else { v0 += gfrezkv->numvoxs; ox = gfrezkv->xsiz; oy = gfrezkv->ysiz; j = ox*oy; }
	v1 = v0;

	for(k=0;k<27;k++)
	{
		nx = ((long)gkv6colx[k])+x; if ((unsigned long)nx >= gfrezkv->xsiz) continue;
		ny = ((long)gkv6coly[k])+y; if ((unsigned long)ny >= gfrezkv->ysiz) continue;
		nz = ((long)gkv6colz[k])+z; if ((unsigned long)nz >= gfrezkv->zsiz) continue;

		if (nx != ox)
		{
			while (nx > ox) { v0 += gfrezkv->xlen[ox]; ox++; j += gfrezkv->ysiz; }
			while (nx < ox) { ox--; v0 -= gfrezkv->xlen[ox]; j -= gfrezkv->ysiz; }
			if ((ny<<1) < gfrezkv->ysiz) { oy = 0; v1 = v0; }
			else { oy = gfrezkv->ysiz; v1 = v0+gfrezkv->xlen[nx]; }
		}
		if (ny != oy)
		{
			while (ny > oy) { v1 += gfrezkv->ylen[j+oy]; oy++; }
			while (ny < oy) { oy--; v1 -= gfrezkv->ylen[j+oy]; }
		}

			//Process z
		for(v=v1,ve=&v1[gfrezkv->ylen[j+ny]];v<ve;v++)
			if (v->z == nz) return(v->col);
	}

		//Use brute force when all else fails.. :/
	v = gfrezkv->vox; mind = 0x7fffffff;
	for(nx=0;nx<gfrezkv->xsiz;nx++)
		for(ny=0;ny<gfrezkv->ysiz;ny++)
			for(ve=&v[gfrezkv->ylen[nx*gfrezkv->ysiz+ny]];v<ve;v++)
			{
				d = labs(x-nx)+labs(y-ny)+labs(z-v->z);
				if (d < mind) { mind = d; k = v->col; }
			}
	return(k);
}

static void kv6colfuncinit (vx5sprite *spr, float det)
{
	point3d tp, tp2;
	float f;

	gfrezkv = spr->voxnum; if (!gfrezkv) { vx5.colfunc = curcolfunc; return; }

	tp2.x = gfrezkv->xpiv + .5;
	tp2.y = gfrezkv->ypiv + .5;
	tp2.z = gfrezkv->zpiv + .5;
	tp.x = spr->p.x - spr->s.x*tp2.x - spr->h.x*tp2.y - spr->f.x*tp2.z;
	tp.y = spr->p.y - spr->s.y*tp2.x - spr->h.y*tp2.y - spr->f.y*tp2.z;
	tp.z = spr->p.z - spr->s.z*tp2.x - spr->h.z*tp2.y - spr->f.z*tp2.z;

		//spr->s.x*x + spr->h.x*y + spr->f.x*z = np.x; //Solve for x,y,z
		//spr->s.y*x + spr->h.y*y + spr->f.y*z = np.y;
		//spr->s.z*x + spr->h.z*y + spr->f.z*z = np.z;
	f = 65536.0 / det;

	tp2.x = (spr->h.y*spr->f.z - spr->h.z*spr->f.y)*f; ftol(tp2.x,&gfrezx.x);
	tp2.y = (spr->h.z*spr->f.x - spr->h.x*spr->f.z)*f; ftol(tp2.y,&gfrezy.x);
	tp2.z = (spr->h.x*spr->f.y - spr->h.y*spr->f.x)*f; ftol(tp2.z,&gfrezz.x);
	ftol(-tp.x*tp2.x - tp.y*tp2.y - tp.z*tp2.z,&gfrezp.x); gfrezp.x += 32767;

	tp2.x = (spr->f.y*spr->s.z - spr->f.z*spr->s.y)*f; ftol(tp2.x,&gfrezx.y);
	tp2.y = (spr->f.z*spr->s.x - spr->f.x*spr->s.z)*f; ftol(tp2.y,&gfrezy.y);
	tp2.z = (spr->f.x*spr->s.y - spr->f.y*spr->s.x)*f; ftol(tp2.z,&gfrezz.y);
	ftol(-tp.x*tp2.x - tp.y*tp2.y - tp.z*tp2.z,&gfrezp.y); gfrezp.y += 32767;

	tp2.x = (spr->s.y*spr->h.z - spr->s.z*spr->h.y)*f; ftol(tp2.x,&gfrezx.z);
	tp2.y = (spr->s.z*spr->h.x - spr->s.x*spr->h.z)*f; ftol(tp2.y,&gfrezy.z);
	tp2.z = (spr->s.x*spr->h.y - spr->s.y*spr->h.x)*f; ftol(tp2.z,&gfrezz.z);
	ftol(-tp.x*tp2.x - tp.y*tp2.y - tp.z*tp2.z,&gfrezp.z); gfrezp.z += 32768;
}

#define LSC3 8 //2 for testing, 8 is normal
typedef struct
{
	long xo, yo, zo, xu, yu, zu, xv, yv, zv, d, msk, pzi;
	long xmino, ymino, xmaxo, ymaxo, xusc, yusc, xvsc, yvsc;
} gfrezt;
static gfrezt gfrez[6];
typedef struct { char z[2]; long n; } slstype;
void setkv6 (vx5sprite *spr, long dacol)
{
	point3d tp, tp2; float f, det;
	long i, j, k, x, y, z, c, d, x0, y0, z0, x1, y1, z1, xi, yi, zi;
	long xo, yo, zo, xu, yu, zu, xv, yv, zv, stu, stv, tu, tv;
	long xx, yy, xmin, xmax, ymin, ymax, isrhs, ihxi, ihyi, ihzi, syshpit;
	long isx, isy, isz, ihx, ihy, ihz, ifx, ify, ifz, iox, ioy, ioz;
	long sx, sy, sx0, sy0, sz0, rx, ry, rz, pz, dcnt, dcnt2, vismask, xysiz;
	long bx0, by0, bz0, bx1, by1, bz1, *lptr, *shead, shpit, scnt, sstop;
	gfrezt *gf;
	slstype *slst;
	kv6data *kv;
	kv6voxtype *v0, *v1, *v2, *v3;
	void (*modslab)(long *, long, long);

	if (spr->flags&2) return;
	kv = spr->voxnum; if (!kv) return;

		//Calculate top-left-up corner in VXL world coordinates
	tp.x = kv->xpiv + .5;
	tp.y = kv->ypiv + .5;
	tp.z = kv->zpiv + .5;
	tp2.x = spr->p.x - spr->s.x*tp.x - spr->h.x*tp.y - spr->f.x*tp.z;
	tp2.y = spr->p.y - spr->s.y*tp.x - spr->h.y*tp.y - spr->f.y*tp.z;
	tp2.z = spr->p.z - spr->s.z*tp.x - spr->h.z*tp.y - spr->f.z*tp.z;

		//Get bounding x-y box of entire freeze area:
	bx0 = VSID; by0 = VSID; bz0 = MAXZDIM; bx1 = 0; by1 = 0; bz1 = 0;
	for(z=kv->zsiz;z>=0;z-=kv->zsiz)
		for(y=kv->ysiz;y>=0;y-=kv->ysiz)
			for(x=kv->xsiz;x>=0;x-=kv->xsiz)
			{
				ftol(spr->s.x*(float)x + spr->h.x*(float)y + spr->f.x*(float)z + tp2.x,&i);
				if (i < bx0) bx0 = i;
				if (i > bx1) bx1 = i;
				ftol(spr->s.y*(float)x + spr->h.y*(float)y + spr->f.y*(float)z + tp2.y,&i);
				if (i < by0) by0 = i;
				if (i > by1) by1 = i;
				ftol(spr->s.z*(float)x + spr->h.z*(float)y + spr->f.z*(float)z + tp2.z,&i);
				if (i < bz0) bz0 = i;
				if (i > bz1) bz1 = i;
			}
	bx0 -= 2; if (bx0 < 0) bx0 = 0;
	by0 -= 2; if (by0 < 0) by0 = 0;
	bz0 -= 2; if (bz0 < 0) bz0 = 0;
	bx1 += 2; if (bx1 > VSID) bx1 = VSID;
	by1 += 2; if (by1 > VSID) by1 = VSID;
	bz1 += 2; if (bz1 > MAXZDIM) bz1 = MAXZDIM;
	vx5.minx = bx0; vx5.maxx = bx1;
	vx5.miny = by0; vx5.maxy = by1;
	vx5.minz = bz0; vx5.maxz = bz1;

	shpit = bx1-bx0; i = (by1-by0)*shpit*sizeof(shead[0]);
		//Make sure to use array that's big enough: umost is 1MB
	shead = (long *)(((long)umost) - (by0*shpit+bx0)*sizeof(shead[0]));
	slst = (slstype *)(((long)umost)+i);
	scnt = 1; sstop = (sizeof(umost)-i)/sizeof(slstype);
	memset(umost,0,i);

	f = (float)(1<<LSC3);
	ftol(spr->s.x*f,&isx); ftol(spr->s.y*f,&isy); ftol(spr->s.z*f,&isz);
	ftol(spr->h.x*f,&ihx); ftol(spr->h.y*f,&ihy); ftol(spr->h.z*f,&ihz);
	ftol(spr->f.x*f,&ifx); ftol(spr->f.y*f,&ify); ftol(spr->f.z*f,&ifz);
	ftol(tp2.x*f,&iox);
	ftol(tp2.y*f,&ioy);
	ftol(tp2.z*f,&ioz);

		//Determine whether sprite is RHS(1) or LHS(0)
	det = (spr->h.y*spr->f.z - spr->h.z*spr->f.y)*spr->s.x +
			(spr->h.z*spr->f.x - spr->h.x*spr->f.z)*spr->s.y +
			(spr->h.x*spr->f.y - spr->h.y*spr->f.x)*spr->s.z;
	if ((*(long *)&det) > 0) isrhs = 1;
	else if ((*(long *)&det) < 0) isrhs = 0;
	else return;

	xi = (((ifx*ihy-ihx*ify)>>31)|1);
	yi = (((isx*ify-ifx*isy)>>31)|1);
	zi = (((ihx*isy-isx*ihy)>>31)|1);
	if (xi > 0) { x0 = 0; x1 = kv->xsiz; } else { x0 = kv->xsiz-1; x1 = -1; }
	if (yi > 0) { y0 = 0; y1 = kv->ysiz; } else { y0 = kv->ysiz-1; y1 = -1; }
	if (zi > 0) { z0 = 0; z1 = kv->zsiz; } else { z0 = kv->zsiz-1; z1 = -1; }

	vismask = (zi<<3)+24 + (yi<<1)+6 + (xi>>1)+2;

	dcnt = 0;
	for(j=2;j;j--)
	{
		dcnt2 = dcnt;
		vismask = ~vismask;
		for(i=1;i<64;i+=i)
		{
			if (!(vismask&i)) continue;

			if (i&0x15) { xo = yo = zo = 0; }
			else if (i == 2) { xo = isx; yo = isy; zo = isz; }
			else if (i == 8) { xo = ihx; yo = ihy; zo = ihz; }
			else             { xo = ifx; yo = ify; zo = ifz; }

				  if (i&3)  { xu = ihx; yu = ihy; zu = ihz; xv = ifx; yv = ify; zv = ifz; }
			else if (i&12) { xu = isx; yu = isy; zu = isz; xv = ifx; yv = ify; zv = ifz; }
			else           { xu = isx; yu = isy; zu = isz; xv = ihx; yv = ihy; zv = ihz; }

			if ((yu < 0) || ((!yu) && (xu < 0)))
				{ xo += xu; yo += yu; zo += zu; xu = -xu; yu = -yu; zu = -zu; }
			if ((yv < 0) || ((!yv) && (xv < 0)))
				{ xo += xv; yo += yv; zo += zv; xv = -xv; yv = -yv; zv = -zv; }
			d = xv*yu - xu*yv; if (!d) continue;
			if (d < 0)
			{
				k = xu; xu = xv; xv = k;
				k = yu; yu = yv; yv = k;
				k = zu; zu = zv; zv = k; d = -d;
			}
			xmin = ymin = xmax = ymax = 0;
			if (xu < 0) xmin += xu; else xmax += xu;
			if (yu < 0) ymin += yu; else ymax += yu;
			if (xv < 0) xmin += xv; else xmax += xv;
			if (yv < 0) ymin += yv; else ymax += yv;

			gf = &gfrez[dcnt];
			gf->xo = xo; gf->yo = yo; gf->zo = zo;
			gf->xu = xu; gf->yu = yu;
			gf->xv = xv; gf->yv = yv;
			gf->xmino = xmin; gf->ymino = ymin;
			gf->xmaxo = xmax; gf->ymaxo = ymax;
			gf->xusc = (xu<<LSC3); gf->yusc = (yu<<LSC3);
			gf->xvsc = (xv<<LSC3); gf->yvsc = (yv<<LSC3);
			gf->d = d; gf->msk = i;

			f = 1.0 / (float)d;
			ftol(((float)gf->yusc * (float)zv - (float)gf->yvsc * (float)zu) * f,&gf->pzi);
			f *= 4194304.0;
			ftol((float)zu*f,&gf->zu);
			ftol((float)zv*f,&gf->zv);

			dcnt++;
		}
	}

	ihxi = ihx*yi;
	ihyi = ihy*yi;
	ihzi = ihz*yi;

	if (xi < 0) v0 = kv->vox+kv->numvoxs; else v0 = kv->vox;
	for(x=x0;x!=x1;x+=xi)
	{
		i = (long)kv->xlen[x];
		if (xi < 0) v0 -= i;
		if (yi < 0) v1 = v0+i; else v1 = v0;
		if (xi >= 0) v0 += i;
		xysiz = x*kv->ysiz;
		sx0 = isx*x + ihx*y0 + iox;
		sy0 = isy*x + ihy*y0 + ioy;
		sz0 = isz*x + ihz*y0 + ioz;
		for(y=y0;y!=y1;y+=yi)
		{
			i = (long)kv->ylen[xysiz+y];
			if (yi < 0) v1 -= i;
			if (zi < 0) { v2 = v1+i-1; v3 = v1-1; }
					 else { v2 = v1; v3 = v1+i; }
			if (yi >= 0) v1 += i;
			while (v2 != v3)
			{
				z = v2->z; //c = v2->col;
				rx = ifx*z + sx0;
				ry = ify*z + sy0;
				rz = ifz*z + sz0;
				for(i=0;i<dcnt;i++)
				{
					gf = &gfrez[i]; if (!(v2->vis&gf->msk)) continue;
					xo = gf->xo + rx;
					yo = gf->yo + ry;
					zo = gf->zo + rz;
					xmin = ((gf->xmino + xo)>>LSC3); if (xmin < 0) xmin = 0;
					ymin = ((gf->ymino + yo)>>LSC3); if (ymin < 0) ymin = 0;
					xmax = ((gf->xmaxo + xo)>>LSC3)+1; if (xmax > VSID) xmax = VSID;
					ymax = ((gf->ymaxo + yo)>>LSC3)+1; if (ymax > VSID) ymax = VSID;
					xx = (xmin<<LSC3) - xo;
					yy = (ymin<<LSC3) - yo;
					stu = yy*gf->xu - xx*gf->yu;
					stv = yy*gf->xv - xx*gf->yv - gf->d;
					syshpit = ymin*shpit;
					for(sy=ymin;sy<ymax;sy++,syshpit+=shpit)
					{
						tu = stu; stu += gf->xusc;
						tv = stv; stv += gf->xvsc;
						sx = xmin;
						while ((tu&tv) >= 0)
						{
							sx++; if (sx >= xmax) goto freezesprcont;
							tu -= gf->yusc; tv -= gf->yvsc;
						}
						tu = ~tu; tv += gf->d;
						pz = dmulshr22(tu,gf->zv,tv,gf->zu) + zo; j = syshpit+sx;
						tu -= gf->d; tv = ~tv;
						while ((tu&tv) < 0)
						{
							if (i < dcnt2)
							{
								if (scnt >= sstop) return; //OUT OF BUFFER SPACE!
								slst[scnt].z[0] = (char)lbound0(pz>>LSC3,255);
								slst[scnt].n = shead[j]; shead[j] = scnt; scnt++;
							}
							else slst[shead[j]].z[1] = (char)lbound0(pz>>LSC3,255);
							tu += gf->yusc; tv += gf->yvsc; pz += gf->pzi; j++;
						}
freezesprcont:;
					}
				}
				v2 += zi;
			}
			sx0 += ihxi; sy0 += ihyi; sz0 += ihzi;
		}
	}

	if (dacol == -1) modslab = delslab; else modslab = insslab;

	if (vx5.colfunc == kv6colfunc) kv6colfuncinit(spr,det);

	j = by0*shpit+bx0;
	for(sy=by0;sy<by1;sy++)
		for(sx=bx0;sx<bx1;sx++,j++)
		{
			i = shead[j]; if (!i) continue;
			lptr = scum2(sx,sy);
			do
			{
				modslab(lptr,(long)slst[i].z[isrhs],(long)slst[i].z[isrhs^1]);
				i = slst[i].n;
			} while (i);
		}
	scum2finish();
	updatebbox(vx5.minx,vx5.miny,vx5.minz,vx5.maxx,vx5.maxy,vx5.maxz,dacol);
}

#endif

/** Free KV6 data
 * If you generate any sprites using one of the melt* functions, and then
 * generate mip-maps for it, you can use this function to de-allocate
 * all mip-maps of the .KV6 safely. You don't need to use this for
 * kv6data objects that were loaded by getkv6,getkfa, or getspr since
 * these functions automatically de-allocate them using this function.
 *
 * @param kv6 pointer to kv6 voxel sprite
 */
void freekv6 (kv6data *kv6)
{
	if (kv6->lowermip) freekv6(kv6->lowermip); //NOTE: dangerous - recursive!
	free((void *)kv6);
}


	//Sprite structure is already allocated
	//kv6, vox, xlen, ylen are all malloced in here!
/**
 * This converts a spherical cut-out of the VXL map into a .KV6 sprite in
 * memory. This function can be used to make walls fall over (with full
 * rotation). It allocates a new vx5sprite sprite structure and you are
 * responsible for freeing the memory using "free" in your own code.
 *
 * @param spr new vx5sprite structure. Position & orientation are initialized
 *        so when you call drawsprite, it exactly matches the VXL map.
 * @param hit center of sphere
 * @param hitrad radius of sphere
 * @param returns 0:bad, >0:mass of captured object (# of voxels)
 */
long meltsphere (vx5sprite *spr, lpoint3d *hit, long hitrad)
{
	long i, j, x, y, z, xs, ys, zs, xe, ye, ze, sq, z0, z1;
	long oxvoxs, oyvoxs, numvoxs, cx, cy, cz, cw;
	float f, ff;
	kv6data *kv;
	kv6voxtype *voxptr;
	unsigned long *xlenptr;
	unsigned short *ylenptr;

	xs = MAX(hit->x-hitrad,0); xe = MIN(hit->x+hitrad,VSID-1);
	ys = MAX(hit->y-hitrad,0); ye = MIN(hit->y+hitrad,VSID-1);
	zs = MAX(hit->z-hitrad,0); ze = MIN(hit->z+hitrad,MAXZDIM-1);
	if ((xs > xe) || (ys > ye) || (zs > ze)) return(0);

	if (hitrad >= SETSPHMAXRAD-1) hitrad = SETSPHMAXRAD-2;

	tempfloatbuf[0] = 0.0f;
#if 0
		//Totally unoptimized
	for(i=1;i<=hitrad;i++) tempfloatbuf[i] = pow((float)i,vx5.curpow);
#else
	tempfloatbuf[1] = 1.0f;
	for(i=2;i<=hitrad;i++)
	{
		if (!factr[i][0]) tempfloatbuf[i] = exp(logint[i]*vx5.curpow);
		else tempfloatbuf[i] = tempfloatbuf[factr[i][0]]*tempfloatbuf[factr[i][1]];
	}
#endif
	*(long *)&tempfloatbuf[hitrad+1] = 0x7f7fffff; //3.4028235e38f; //Highest float

// ---------- Need to know how many voxels to allocate... SLOW!!! :( ----------
	cx = cy = cz = 0; //Centroid
	cw = 0;       //Weight (1 unit / voxel)
	numvoxs = 0;
	sq = 0; //pow(fabs(x-hit->x),vx5.curpow) + "y + "z < pow(vx5.currad,vx5.curpow)
	for(x=xs;x<=xe;x++)
	{
		ff = tempfloatbuf[hitrad]-tempfloatbuf[labs(x-hit->x)];
		for(y=ys;y<=ye;y++)
		{
			f = ff-tempfloatbuf[labs(y-hit->y)];
			if (*(long *)&f > 0) //WARNING: make sure to always write ylenptr!
			{
				while (*(long *)&tempfloatbuf[sq] <  *(long *)&f) sq++;
				while (*(long *)&tempfloatbuf[sq] >= *(long *)&f) sq--;
				z0 = MAX(hit->z-sq,zs); z1 = MIN(hit->z+sq+1,ze);
				for(z=z0;z<z1;z++)
				{
					i = getcube(x,y,z); //0:air, 1:unexposed solid, 2:vbuf col ptr
					if (i)
					{
						cx += (x-hit->x); cy += (y-hit->y); cz += (z-hit->z); cw++;
					}
					if ((i == 0) || ((i == 1) && (1))) continue; //not_on_border))) continue; //FIX THIS!!!
					numvoxs++;
				}
			}
		}
	}
	if (numvoxs <= 0) return(0); //No voxels found!
// ---------------------------------------------------------------------------

	f = 1.0 / (float)cw; //Make center of sprite the centroid
	spr->p.x = (float)hit->x + (float)cx*f;
	spr->p.y = (float)hit->y + (float)cy*f;
	spr->p.z = (float)hit->z + (float)cz*f;
	spr->s.x = 1.f; spr->h.x = 0.f; spr->f.x = 0.f;
	spr->s.y = 0.f; spr->h.y = 1.f; spr->f.y = 0.f;
	spr->s.z = 0.f; spr->h.z = 0.f; spr->f.z = 1.f;

	x = xe-xs+1; y = ye-ys+1; z = ze-zs+1;

	j = sizeof(kv6data) + numvoxs*sizeof(kv6voxtype) + x*4 + x*y*2;
	i = (long)malloc(j); if (!i) return(0); if (i&3) { free((void *)i); return(0); }
	spr->voxnum = kv = (kv6data *)i; spr->flags = 0;
	kv->leng = j;
	kv->xsiz = x;
	kv->ysiz = y;
	kv->zsiz = z;
	kv->xpiv = spr->p.x - xs;
	kv->ypiv = spr->p.y - ys;
	kv->zpiv = spr->p.z - zs;
	kv->numvoxs = numvoxs;
	kv->namoff = 0;
	kv->lowermip = 0;
	kv->vox = (kv6voxtype *)((long)spr->voxnum+sizeof(kv6data));
	kv->xlen = (unsigned long *)(((long)kv->vox)+numvoxs*sizeof(kv6voxtype));
	kv->ylen = (unsigned short *)(((long)kv->xlen) + kv->xsiz*4);

	voxptr = kv->vox; numvoxs = 0;
	xlenptr = kv->xlen; oxvoxs = 0;
	ylenptr = kv->ylen; oyvoxs = 0;

	sq = 0; //pow(fabs(x-hit->x),vx5.curpow) + "y + "z < pow(vx5.currad,vx5.curpow)
	for(x=xs;x<=xe;x++)
	{
		ff = tempfloatbuf[hitrad]-tempfloatbuf[labs(x-hit->x)];
		for(y=ys;y<=ye;y++)
		{
			f = ff-tempfloatbuf[labs(y-hit->y)];
			if (*(long *)&f > 0) //WARNING: make sure to always write ylenptr!
			{
				while (*(long *)&tempfloatbuf[sq] <  *(long *)&f) sq++;
				while (*(long *)&tempfloatbuf[sq] >= *(long *)&f) sq--;
				z0 = MAX(hit->z-sq,zs); z1 = MIN(hit->z+sq+1,ze);
				for(z=z0;z<z1;z++)
				{
					i = getcube(x,y,z); //0:air, 1:unexposed solid, 2:vbuf col ptr
					if ((i == 0) || ((i == 1) && (1))) continue; //not_on_border))) continue; //FIX THIS!!!
					voxptr[numvoxs].col = lightvox(*(long *)i);
					voxptr[numvoxs].z = z-zs;
					voxptr[numvoxs].vis = 63; //FIX THIS!!!
					voxptr[numvoxs].dir = 0; //FIX THIS!!!
					numvoxs++;
				}
			}
			*ylenptr++ = numvoxs-oyvoxs; oyvoxs = numvoxs;
		}
		*xlenptr++ = numvoxs-oxvoxs; oxvoxs = numvoxs;
	}
	return(cw);
}

	//Sprite structure is already allocated
	//kv6, vox, xlen, ylen are all malloced in here!
/**
 * This function is similar to meltsphere, except you can use any user-
 * defined shape (with some size limits). The user-defined shape is
 * described by a list of vertical columns in the "vspans" format:
 *
 * typedef struct { char z1, z0, x, y; } vspans;
 *
 * The list MUST be ordered first in increasing Y, then in increasing X
 * or else the function will crash! Fortunately, the structure is
 * arranged in a way that the data can be sorted quite easily using a
 * simple trick: if you use a typecast from vspans to "unsigned long",
 * you can use a generic sort code on 32-bit integers to achieve a
 * correct sort. The vspans members are all treated as unsigned chars,
 * so it's usually a good idea to bias your columns by 128, and then
 * reverse-bias them in the "offs" offset.
 *
 * @param spr new vx5sprite structure. Position & orientation are initialized
 *            so when you call drawsprite, it exactly matches the VXL map.
 * @param lst list in "vspans" format
 * @param lstnum number of columns on list
 * @param offs offset of top-left corner in VXL coordinates
 * @return mass (in voxel units), returns 0 if error (or no voxels)
 */
long meltspans (vx5sprite *spr, vspans *lst, long lstnum, lpoint3d *offs)
{
	float f;
	long i, j, x, y, z, xs, ys, zs, xe, ye, ze, z0, z1;
	long ox, oy, oxvoxs, oyvoxs, numvoxs, cx, cy, cz, cw;
	kv6data *kv;
	kv6voxtype *voxptr;
	unsigned long *xlenptr;
	unsigned short *ylenptr;

	if (lstnum <= 0) return(0);
// ---------- Need to know how many voxels to allocate... SLOW!!! :( ----------
	cx = cy = cz = 0; //Centroid
	cw = 0;       //Weight (1 unit / voxel)
	numvoxs = 0;
	xs = xe = ((long)lst[0].x)+offs->x;
	ys = ((long)lst[       0].y)+offs->y;
	ye = ((long)lst[lstnum-1].y)+offs->y;
	zs = ze = ((long)lst[0].z0)+offs->z;
	for(j=0;j<lstnum;j++)
	{
		x = ((long)lst[j].x)+offs->x;
		y = ((long)lst[j].y)+offs->y; if ((x|y)&(~(VSID-1))) continue;
			  if (x < xs) xs = x;
		else if (x > xe) xe = x;
		z0 = ((long)lst[j].z0)+offs->z;   if (z0 < 0) z0 = 0;
		z1 = ((long)lst[j].z1)+offs->z+1; if (z1 > MAXZDIM) z1 = MAXZDIM;
		if (z0 < zs) zs = z0;
		if (z1 > ze) ze = z1;
		for(z=z0;z<z1;z++) //getcube too SLOW... FIX THIS!!!
		{
			i = getcube(x,y,z); //0:air, 1:unexposed solid, 2:vbuf col ptr
			if (i) { cx += x-offs->x; cy += y-offs->y; cz += z-offs->z; cw++; }
			if (i&~1) numvoxs++;
		}
	}
	if (numvoxs <= 0) return(0); //No voxels found!
// ---------------------------------------------------------------------------

	f = 1.0 / (float)cw; //Make center of sprite the centroid
	spr->p.x = (float)offs->x + (float)cx*f;
	spr->p.y = (float)offs->y + (float)cy*f;
	spr->p.z = (float)offs->z + (float)cz*f;
	spr->x.x = 0.f; spr->y.x = 1.f; spr->z.x = 0.f;
	spr->x.y = 1.f; spr->y.y = 0.f; spr->z.y = 0.f;
	spr->x.z = 0.f; spr->y.z = 0.f; spr->z.z = 1.f;

	x = xe-xs+1; y = ye-ys+1; z = ze-zs;

	j = sizeof(kv6data) + numvoxs*sizeof(kv6voxtype) + y*4 + x*y*2;
	i = (long)malloc(j); if (!i) return(0); if (i&3) { free((void *)i); return(0); }
	spr->voxnum = kv = (kv6data *)i; spr->flags = 0;
	kv->leng = j;
	kv->xsiz = y;
	kv->ysiz = x;
	kv->zsiz = z;
	kv->xpiv = spr->p.y - ys;
	kv->ypiv = spr->p.x - xs;
	kv->zpiv = spr->p.z - zs;
	kv->numvoxs = numvoxs;
	kv->namoff = 0;
	kv->lowermip = 0;
	kv->vox = (kv6voxtype *)((long)spr->voxnum+sizeof(kv6data));
	kv->xlen = (unsigned long *)(((long)kv->vox)+numvoxs*sizeof(kv6voxtype));
	kv->ylen = (unsigned short *)(((long)kv->xlen) + kv->xsiz*4);

	voxptr = kv->vox; numvoxs = 0;
	xlenptr = kv->xlen; oxvoxs = 0;
	ylenptr = kv->ylen; oyvoxs = 0;
	ox = xs; oy = ys;
	for(j=0;j<lstnum;j++)
	{
		x = ((long)lst[j].x)+offs->x;
		y = ((long)lst[j].y)+offs->y; if ((x|y)&(~(VSID-1))) continue;
		while ((ox != x) || (oy != y))
		{
			*ylenptr++ = numvoxs-oyvoxs; oyvoxs = numvoxs; ox++;
			if (ox > xe)
			{
				*xlenptr++ = numvoxs-oxvoxs; oxvoxs = numvoxs;
				ox = xs; oy++;
			}
		}
		z0 = ((long)lst[j].z0)+offs->z;   if (z0 < 0) z0 = 0;
		z1 = ((long)lst[j].z1)+offs->z+1; if (z1 > MAXZDIM) z1 = MAXZDIM;
		for(z=z0;z<z1;z++) //getcube TOO SLOW... FIX THIS!!!
		{
			i = getcube(x,y,z); //0:air, 1:unexposed solid, 2:vbuf col ptr
			if (!(i&~1)) continue;
			voxptr[numvoxs].col = lightvox(*(long *)i);
			voxptr[numvoxs].z = z-zs;

			voxptr[numvoxs].vis = 63; //FIX THIS!!!
			//if (!isvoxelsolid(x-1,y,z)) voxptr[numvoxs].vis |= 1;
			//if (!isvoxelsolid(x+1,y,z)) voxptr[numvoxs].vis |= 2;
			//if (!isvoxelsolid(x,y-1,z)) voxptr[numvoxs].vis |= 4;
			//if (!isvoxelsolid(x,y+1,z)) voxptr[numvoxs].vis |= 8;
			//if (!isvoxelsolid(x,y,z-1)) voxptr[numvoxs].vis |= 16;
			//if (!isvoxelsolid(x,y,z+1)) voxptr[numvoxs].vis |= 32;

			voxptr[numvoxs].dir = 0; //FIX THIS!!!
			numvoxs++;
		}
	}
	while (1)
	{
		*ylenptr++ = numvoxs-oyvoxs; oyvoxs = numvoxs; ox++;
		if (ox > xe)
		{
			*xlenptr++ = numvoxs-oxvoxs; oxvoxs = numvoxs;
			ox = xs; oy++; if (oy > ye) break;
		}
	}
	return(cw);
}

//--------------------------- KFA sprite code ends ---------------------------

static void setlighting (long x0, long y0, long z0, long x1, long y1, long z1, long lval)
{
	long i, x, y;
	char *v;

	x0 = MAX(x0,0); x1 = MIN(x1,VSID);
	y0 = MAX(y0,0); y1 = MIN(y1,VSID);
	z0 = MAX(z0,0); z1 = MIN(z1,MAXZDIM);

	lval <<= 24;

		//Set 4th byte of colors to full intensity
	for(y=y0;y<y1;y++)
		for(x=x0;x<x1;x++)
		{
			for(v=sptr[y*VSID+x];v[0];v+=v[0]*4)
				for(i=1;i<v[0];i++)
					(*(long *)&v[i<<2]) = (((*(long *)&v[i<<2])&0xffffff)|lval);
			for(i=1;i<=v[2]-v[1]+1;i++)
				(*(long *)&v[i<<2]) = (((*(long *)&v[i<<2])&0xffffff)|lval);
		}
}

/** Updates Lighting, Mip-mapping, and Floating objects list */
typedef struct { long x0, y0, z0, x1, y1, z1, csgdel; } bboxtyp;
#define BBOXSIZ 256
static bboxtyp bbox[BBOXSIZ];
static long bboxnum = 0;
void updatevxl ()
{
	long i;

	for(i=bboxnum-1;i>=0;i--)
	{
		if (vx5.lightmode)
			updatelighting(bbox[i].x0,bbox[i].y0,bbox[i].z0,bbox[i].x1,bbox[i].y1,bbox[i].z1);
		if (vx5.vxlmipuse > 1)
			genmipvxl(bbox[i].x0,bbox[i].y0,bbox[i].x1,bbox[i].y1);
		if ((vx5.fallcheck) && (bbox[i].csgdel))
			checkfloatinbox(bbox[i].x0,bbox[i].y0,bbox[i].z0,bbox[i].x1,bbox[i].y1,bbox[i].z1);
	}
	bboxnum = 0;
}

void updatebbox (long x0, long y0, long z0, long x1, long y1, long z1, long csgdel)
{
	long i;

	if ((x0 >= x1) || (y0 >= y1) || (z0 >= z1)) return;
	for(i=bboxnum-1;i>=0;i--)
	{
		if ((x0 >= bbox[i].x1) || (bbox[i].x0 >= x1)) continue;
		if ((y0 >= bbox[i].y1) || (bbox[i].y0 >= y1)) continue;
		if ((z0 >= bbox[i].z1) || (bbox[i].z0 >= z1)) continue;
		if (bbox[i].x0 < x0) x0 = bbox[i].x0;
		if (bbox[i].y0 < y0) y0 = bbox[i].y0;
		if (bbox[i].z0 < z0) z0 = bbox[i].z0;
		if (bbox[i].x1 > x1) x1 = bbox[i].x1;
		if (bbox[i].y1 > y1) y1 = bbox[i].y1;
		if (bbox[i].z1 > z1) z1 = bbox[i].z1;
		csgdel |= bbox[i].csgdel;
		bboxnum--; bbox[i] = bbox[bboxnum];
	}
	bbox[bboxnum].x0 = x0; bbox[bboxnum].x1 = x1;
	bbox[bboxnum].y0 = y0; bbox[bboxnum].y1 = y1;
	bbox[bboxnum].z0 = z0; bbox[bboxnum].z1 = z1;
	bbox[bboxnum].csgdel = csgdel; bboxnum++;
	if (bboxnum >= BBOXSIZ) updatevxl();
}

/** An array of the lights available */
static long lightlst[MAXLIGHTS];
static float lightsub[MAXLIGHTS];

/** Re-calculates lighting byte #4 of all voxels inside bounding box
 *  Takes 6 longs ( which should be 2 vectors ) representing min and max extents
 *  of an AABB.
 *  @param x0
 *  @param y0
 *  @param z0
 *  @param x1
 *  @param y1
 *  @param z1
*/
void updatelighting (long x0, long y0, long z0, long x1, long y1, long z1)
{
	point3d tp;
	float f, g, h, fx, fy, fz;
	long i, j, x, y, z, sz0, sz1, offs, cstat, lightcnt;
	long x2, y2, x3, y3;
	char *v;

	if (!vx5.lightmode) return;
	xbsox = -17;

	x0 = MAX(x0-ESTNORMRAD,0); x1 = MIN(x1+ESTNORMRAD,VSID);
	y0 = MAX(y0-ESTNORMRAD,0); y1 = MIN(y1+ESTNORMRAD,VSID);
	z0 = MAX(z0-ESTNORMRAD,0); z1 = MIN(z1+ESTNORMRAD,MAXZDIM);

	x2 = x0; y2 = y0;
	x3 = x1; y3 = y1;
	for(y0=y2;y0<y3;y0=y1)
	{
		y1 = MIN(y0+64,y3);  //"justfly -" (256 lights): +1024:41sec 512:29 256:24 128:22 64:21 32:21 16:21
		for(x0=x2;x0<x3;x0=x1)
		{
			x1 = MIN(x0+64,x3);


			if (vx5.lightmode == 2)
			{
				lightcnt = 0; //Find which lights are close enough to affect rectangle
				for(i=vx5.numlights-1;i>=0;i--)
				{
					ftol(vx5.lightsrc[i].p.x,&x);
					ftol(vx5.lightsrc[i].p.y,&y);
					ftol(vx5.lightsrc[i].p.z,&z);
					if (x < x0) x -= x0; else if (x > x1) x -= x1; else x = 0;
					if (y < y0) y -= y0; else if (y > y1) y -= y1; else y = 0;
					if (z < z0) z -= z0; else if (z > z1) z -= z1; else z = 0;
					f = vx5.lightsrc[i].r2;
					if ((float)(x*x+y*y+z*z) < f)
					{
						lightlst[lightcnt] = i;
						lightsub[lightcnt] = 1/(sqrt(f)*f);
						lightcnt++;
					}
				}
			}

			for(y=y0;y<y1;y++)
				for(x=x0;x<x1;x++)
				{
					v = sptr[y*VSID+x]; cstat = 0;
					while (1)
					{
						if (!cstat)
						{
							sz0 = ((long)v[1]); sz1 = ((long)v[2])+1; offs = 7-(sz0<<2);
							cstat = 1;
						}
						else
						{
							sz0 = ((long)v[2])-((long)v[1])-((long)v[0])+2;
							if (!v[0]) break; v += v[0]*4;
							sz1 = ((long)v[3]); sz0 += sz1; offs = 3-(sz1<<2);
							cstat = 0;
						}
						if (z0 > sz0) sz0 = z0;
						if (z1 < sz1) sz1 = z1;
						if (vx5.lightmode < 2)
						{
							for(z=sz0;z<sz1;z++)
							{
								estnorm(x,y,z,&tp);
								ftol((tp.y*.5+tp.z)*64.f+103.5f,&i);
								v[(z<<2)+offs] = *(char *)&i;
							}
						}
						else
						{
							for(z=sz0;z<sz1;z++)
							{
								estnorm(x,y,z,&tp);
								f = (tp.y*.5+tp.z)*16+47.5;
								for(i=lightcnt-1;i>=0;i--)
								{
									j = lightlst[i];
									fx = vx5.lightsrc[j].p.x-(float)x;
									fy = vx5.lightsrc[j].p.y-(float)y;
									fz = vx5.lightsrc[j].p.z-(float)z;
									h = tp.x*fx+tp.y*fy+tp.z*fz; if (*(long *)&h >= 0) continue;
									g = fx*fx+fy*fy+fz*fz; if (g >= vx5.lightsrc[j].r2) continue;

									#ifdef NOASM
									g = 1.0/(g*sqrt(g))-lightsub[i]; //1.0/g;
									#else
									if (cputype&(1<<25))
									{
										#ifdef __GNUC__ //gcc inline asm
										__asm__ __volatile__
										(
											"rcpss	%[x0], %[x1]\n"     //xmm1=1/g
											"rsqrtss	%[x0], %[x0]\n" //xmm0=1/sqrt(g)
											"mulss	%[x0], %[x1]\n"     //xmm1=1/(g*sqrt(g))
											"subss	%c[lsub](,%[i],4), %[x1]\n"
											: [x1] "=x" (g)
											: [x0] "x" (g), [i] "r" (i), [lsub] "p" (lightsub)
											:
										);
										#endif
										#ifdef _MSC_VER //msvc inline asm
										_asm
										{
											movss	xmm0, g        //xmm0=g
											rcpss	xmm1, xmm0     //xmm1=1/g
											rsqrtss	xmm0, xmm0     //xmm0=1/sqrt(g)
											mulss	xmm1, xmm0     //xmm1=1/(g*sqrt(g))
											mov	eax, i
											subss	xmm1, lightsub[eax*4]
											movss	g, xmm1
										}
										#endif
									}
									else
									{
										#ifdef __GNUC__ //gcc inline asm
										__asm__ __volatile__
										(
											"pfrcp	%[y0], %%mm1\n"
											"pfrsqrt	%[y0], %[y0]\n"
											"pfmul	%%mm1, %[y0]\n"
											"pfsub	%c[lsub](,%[i],4), %[y0]\n"
											"femms\n"
											: [y0] "+y" (g)
											: [i]   "r" (i), [lsub] "p" (lightsub)
											: "mm1"
										);
										#endif
										#ifdef _MSC_VER //msvc inline asm
										_asm
										{
											movd mm0, g
											pfrcp	mm1, mm0
											pfrsqrt	mm0, mm0
											pfmul	mm0, mm1
											mov	eax, i
											pfsub	mm0, lightsub[eax*4]
											movd	g, xmm0
											femms
										}
										#endif
									}
									#endif
									f -= g*h*vx5.lightsrc[j].sc;
								}
								if (*(long *)&f > 0x437f0000) f = 255; //0x437f0000 is 255.0
								ftol(f,&i);
								v[(z<<2)+offs] = *(char *)&i;
							}
						}
					}
				}
		}
	}
}

//float detection & falling code begins --------------------------------------
//How to use this section of code:
//Step 1: Call checkfloatinbox after every "deleting" set* call
//Step 2: Call dofalls(); at a constant rate in movement code

/** Adds all slabs inside box (inclusive) to "float check" list
 *  Takes 6 longs ( which should be 2 vectors ) representing min and max extents
 *  of an AABB.
 *  @param x0
 *  @param y0
 *  @param z0
 *  @param x1
 *  @param y1
 *  @param z1
*/
void checkfloatinbox (long x0, long y0, long z0, long x1, long y1, long z1)
{
	long x, y;
	char *ov, *v;

	if (flchkcnt >= FLCHKSIZ) return;

		//Make all off-by-1 hacks in other code unnecessary
	x0 = MAX(x0-1,0); x1 = MIN(x1+1,VSID);
	y0 = MAX(y0-1,0); y1 = MIN(y1+1,VSID);
	z0 = MAX(z0-1,0); z1 = MIN(z1+1,MAXZDIM);

		//Add local box's slabs to flchk list - checked in next dofalls()
	for(y=y0;y<y1;y++)
		for(x=x0;x<x1;x++)
		{
			v = sptr[y*VSID+x];
			while (1)
			{
				ov = v; if ((z1 <= v[1]) || (!v[0])) break;
				v += v[0]*4; if (z0 >= v[3]) continue;
				flchk[flchkcnt].x = x;
				flchk[flchkcnt].y = y;
				flchk[flchkcnt].z = ov[1];
				flchkcnt++; if (flchkcnt >= FLCHKSIZ) return;
			}
		}
}

void isnewfloatingadd (long f)
{
	long v = (((f>>(LOGHASHEAD+3))-(f>>3)) & ((1<<LOGHASHEAD)-1));
	vlst[vlstcnt].b = hhead[v]; hhead[v] = vlstcnt;
	vlst[vlstcnt].v = f; vlstcnt++;
}

long isnewfloatingot (long f)
{
	long v = hhead[((f>>(LOGHASHEAD+3))-(f>>3)) & ((1<<LOGHASHEAD)-1)];
	while (1)
	{
		if (v < 0) return(-1);
		if (vlst[v].v == f) return(v);
		v = vlst[v].b;
	}
}

/** removes a & adds b while preserving index; used only by meltfall(...)
 *  \Warning Must do nothing if 'a' not in hash
 *  @param a ( x coordinate ? )
 *  @param b ( y coordinate ? )
*/
void isnewfloatingchg (long a, long b)
{
	long ov, v, i, j;

	i = (((a>>(LOGHASHEAD+3))-(a>>3)) & ((1<<LOGHASHEAD)-1));
	j = (((b>>(LOGHASHEAD+3))-(b>>3)) & ((1<<LOGHASHEAD)-1));

	v = hhead[i]; ov = -1;
	while (v >= 0)
	{
		if (vlst[v].v == a)
		{
			vlst[v].v = b; if (i == j) return;
			if (ov < 0) hhead[i] = vlst[v].b; else vlst[ov].b = vlst[v].b;
			vlst[v].b = hhead[j]; hhead[j] = v;
			return;
		}
		ov = v; v = vlst[v].b;
	}
}

long isnewfloating (flstboxtype *flb)
{
	float f;
	lpoint3d p, cen;
	long i, j, nx, ny, z0, z1, fend, ovlstcnt, mass;
	char *v, *ov;

	p.x = flb->chk.x; p.y = flb->chk.y; p.z = flb->chk.z;
	v = sptr[p.y*VSID+p.x];
	while (1)
	{
		ov = v;
		if ((p.z < v[1]) || (!v[0])) return(0);
		v += v[0]*4;
		if (p.z < v[3]) break;
	}

	if (isnewfloatingot((long)ov) >= 0) return(0);
	ovlstcnt = vlstcnt;
	isnewfloatingadd((long)ov);
	if (vlstcnt >= VLSTSIZ) return(0); //EVIL HACK TO PREVENT CRASH!

		//Init: centroid, mass, bounding box
	cen = p; mass = 1;
	flb->x0 = p.x-1; flb->y0 = p.y-1; flb->z0 = p.z-1;
	flb->x1 = p.x+1; flb->y1 = p.y+1; flb->z1 = p.z+1;

	fend = 0;
	while (1)
	{
		z0 = ov[1];         if (z0 < flb->z0) flb->z0 = z0;
		z1 = ov[ov[0]*4+3]; if (z1 > flb->z1) flb->z1 = z1;

		i = z1-z0;
		cen.x += p.x*i;
		cen.y += p.y*i;
		cen.z += (((z0+z1)*i)>>1); //sum(z0 to z1-1)
		mass += i;

		for(i=0;i<8;i++) //26-connectivity
		{
			switch(i)
			{
				case 0: nx = p.x-1; ny = p.y  ; if (nx < flb->x0) flb->x0 = nx; break;
				case 1: nx = p.x  ; ny = p.y-1; if (ny < flb->y0) flb->y0 = ny; break;
				case 2: nx = p.x+1; ny = p.y  ; if (nx > flb->x1) flb->x1 = nx; break;
				case 3: nx = p.x  ; ny = p.y+1; if (ny > flb->y1) flb->y1 = ny; break;
				case 4: nx = p.x-1; ny = p.y-1; break;
				case 5: nx = p.x+1; ny = p.y-1; break;
				case 6: nx = p.x-1; ny = p.y+1; break;
				case 7: nx = p.x+1; ny = p.y+1; break;
				default: _gtfo(); //tells MSVC default can't be reached
			}
			if ((unsigned long)(nx|ny) >= VSID) continue;

			v = sptr[ny*VSID+nx];
			while (1)
			{
				if (!v[0])
				{
					if (v[1] <= z1) return(0);  //This MUST be <=, (not <) !!!
					break;
				}
				ov = v; v += v[0]*4; //NOTE: this is a 'different' ov
				if ((ov[1] > z1) || (z0 > v[3])) continue; //26-connectivity
				j = isnewfloatingot((long)ov);
				if (j < 0)
				{
					isnewfloatingadd((long)ov);
					if (vlstcnt >= VLSTSIZ) return(0); //EVIL HACK TO PREVENT CRASH!
					fstk[fend].x = nx; fstk[fend].y = ny; fstk[fend].z = (long)ov;
					fend++; if (fend >= FSTKSIZ) return(0); //EVIL HACK TO PREVENT CRASH!
					continue;
				}
				if ((unsigned long)j < ovlstcnt) return(0);
			}
		}

		if (!fend)
		{
			flb->i0 = ovlstcnt;
			flb->i1 = vlstcnt;
			flb->mass = mass; f = 1.0 / (float)mass;
			flb->centroid.x = (float)cen.x*f;
			flb->centroid.y = (float)cen.y*f;
			flb->centroid.z = (float)cen.z*f;
			return(1);
		}
		fend--;
		p.x = fstk[fend].x; p.y = fstk[fend].y; ov = (char *)fstk[fend].z;
	}
}

void startfalls ()
{
	long i, z;

		//This allows clear to be MUCH faster when there isn't much falling
	if (vlstcnt < ((1<<LOGHASHEAD)>>1))
	{
		for(i=vlstcnt-1;i>=0;i--)
		{
			z = vlst[i].v;
			hhead[((z>>(LOGHASHEAD+3))-(z>>3)) & ((1<<LOGHASHEAD)-1)] = -1;
		}
	}
	else { for(z=0;z<(1<<LOGHASHEAD);z++) hhead[z] = -1; }

		//Float detection...
		//flstcnt[].i0/i1 tell which parts of vlst are floating
	vlstcnt = 0;

		//Remove any current pieces that are no longer floating
	for(i=vx5.flstnum-1;i>=0;i--)
		if (!isnewfloating(&vx5.flstcnt[i])) //Modifies flstcnt,vlst[],vlstcnt
			vx5.flstcnt[i] = vx5.flstcnt[--vx5.flstnum]; //onground, so delete flstcnt[i]

		//Add new floating pieces (while space is left on flstcnt)
	if (vx5.flstnum < FLPIECES)
		for(i=flchkcnt-1;i>=0;i--)
		{
			vx5.flstcnt[vx5.flstnum].chk = flchk[i];
			if (isnewfloating(&vx5.flstcnt[vx5.flstnum])) //Modifies flstcnt,vlst[],vlstcnt
			{
				vx5.flstcnt[vx5.flstnum].userval = -1; //New piece: let game programmer know
				vx5.flstnum++; if (vx5.flstnum >= FLPIECES) break;
			}
		}
	flchkcnt = 0;
}

	//Call 0 or 1 times (per flstcnt) between startfalls&finishfalls
void dofall (long i)
{
	long j, z;
	char *v;

		//Falling code... call this function once per piece
	vx5.flstcnt[i].chk.z++;
	for(z=vx5.flstcnt[i].i1-1;z>=vx5.flstcnt[i].i0;z--)
	{
		v = (char *)vlst[z].v; v[1]++; v[2]++;
		v = &v[v[0]*4];
		v[3]++;
		if ((v[3] == v[1]) && (vx5.flstcnt[i].i1 >= 0))
		{
			j = isnewfloatingot((long)v);
				//Make sure it's not part of the same floating object
			if ((j < vx5.flstcnt[i].i0) || (j >= vx5.flstcnt[i].i1))
				vx5.flstcnt[i].i1 = -1; //Mark flstcnt[i] for scum2 fixup
		}
	}

	if (vx5.vxlmipuse > 1)
	{
		long x0, y0, x1, y1;
		x0 = MAX(vx5.flstcnt[i].x0,0); x1 = MIN(vx5.flstcnt[i].x1+1,VSID);
		y0 = MAX(vx5.flstcnt[i].y0,0); y1 = MIN(vx5.flstcnt[i].y1+1,VSID);
		//FIX ME!!!
		//if ((x1 > x0) && (y1 > y0)) genmipvxl(x0,y0,x1,y1); //Don't replace with bbox!
	}
}

	//Sprite structure is already allocated
	//kv6, vox, xlen, ylen are all malloced in here!
long meltfall (vx5sprite *spr, long fi, long delvxl)
{
	long i, j, k, x, y, z, xs, ys, zs, xe, ye, ze;
	long oxvoxs, oyvoxs, numvoxs;
	char *v, *ov, *nv;
	kv6data *kv;
	kv6voxtype *voxptr;
	unsigned long *xlenptr;
	unsigned short *ylenptr;

	if (vx5.flstcnt[fi].i1 < 0) return(0);

	xs = MAX(vx5.flstcnt[fi].x0,0); xe = MIN(vx5.flstcnt[fi].x1,VSID-1);
	ys = MAX(vx5.flstcnt[fi].y0,0); ye = MIN(vx5.flstcnt[fi].y1,VSID-1);
	zs = MAX(vx5.flstcnt[fi].z0,0); ze = MIN(vx5.flstcnt[fi].z1,MAXZDIM-1);
	if ((xs > xe) || (ys > ye) || (zs > ze)) return(0);

		//Need to know how many voxels to allocate... SLOW :(
	numvoxs = vx5.flstcnt[fi].i0-vx5.flstcnt[fi].i1;
	for(i=vx5.flstcnt[fi].i0;i<vx5.flstcnt[fi].i1;i++)
		numvoxs += ((char *)vlst[i].v)[0];
	if (numvoxs <= 0) return(0); //No voxels found!

	spr->p = vx5.flstcnt[fi].centroid;
	spr->s.x = 1.f; spr->h.x = 0.f; spr->f.x = 0.f;
	spr->s.y = 0.f; spr->h.y = 1.f; spr->f.y = 0.f;
	spr->s.z = 0.f; spr->h.z = 0.f; spr->f.z = 1.f;

	x = xe-xs+1; y = ye-ys+1; z = ze-zs+1;

	j = sizeof(kv6data) + numvoxs*sizeof(kv6voxtype) + x*4 + x*y*2;
	i = (long)malloc(j); if (!i) return(0); if (i&3) { free((void *)i); return(0); }
	spr->voxnum = kv = (kv6data *)i; spr->flags = 0;
	kv->leng = j;
	kv->xsiz = x;
	kv->ysiz = y;
	kv->zsiz = z;
	kv->xpiv = spr->p.x - xs;
	kv->ypiv = spr->p.y - ys;
	kv->zpiv = spr->p.z - zs;
	kv->numvoxs = numvoxs;
	kv->namoff = 0;
	kv->lowermip = 0;
	kv->vox = (kv6voxtype *)((long)spr->voxnum+sizeof(kv6data));
	kv->xlen = (unsigned long *)(((long)kv->vox)+numvoxs*sizeof(kv6voxtype));
	kv->ylen = (unsigned short *)(((long)kv->xlen) + kv->xsiz*4);

	voxptr = kv->vox; numvoxs = 0;
	xlenptr = kv->xlen; oxvoxs = 0;
	ylenptr = kv->ylen; oyvoxs = 0;

	for(x=xs;x<=xe;x++)
	{
		for(y=ys;y<=ye;y++)
		{
			for(v=sptr[y*VSID+x];v[0];v=nv)
			{
				nv = v+v[0]*4;

				i = isnewfloatingot((long)v);
				if (((unsigned long)i >= vx5.flstcnt[fi].i1) || (i < vx5.flstcnt[fi].i0))
					continue;

				for(z=v[1];z<=v[2];z++)
				{
					voxptr[numvoxs].col = lightvox(*(long *)&v[((z-v[1])<<2)+4]);
					voxptr[numvoxs].z = z-zs;

					voxptr[numvoxs].vis = 0; //OPTIMIZE THIS!!!
					if (!isvoxelsolid(x-1,y,z)) voxptr[numvoxs].vis |= 1;
					if (!isvoxelsolid(x+1,y,z)) voxptr[numvoxs].vis |= 2;
					if (!isvoxelsolid(x,y-1,z)) voxptr[numvoxs].vis |= 4;
					if (!isvoxelsolid(x,y+1,z)) voxptr[numvoxs].vis |= 8;
					//if (z == v[1]) voxptr[numvoxs].vis |= 16;
					//if (z == nv[3]-1) voxptr[numvoxs].vis |= 32;
					if (!isvoxelsolid(x,y,z-1)) voxptr[numvoxs].vis |= 16;
					if (!isvoxelsolid(x,y,z+1)) voxptr[numvoxs].vis |= 32;

					voxptr[numvoxs].dir = 0; //FIX THIS!!!
					numvoxs++;
				}
				for(z=nv[3]+v[2]-v[1]-v[0]+2;z<nv[3];z++)
				{
					voxptr[numvoxs].col = lightvox(*(long *)&nv[(z-nv[3])<<2]);
					voxptr[numvoxs].z = z-zs;

					voxptr[numvoxs].vis = 0; //OPTIMIZE THIS!!!
					if (!isvoxelsolid(x-1,y,z)) voxptr[numvoxs].vis |= 1;
					if (!isvoxelsolid(x+1,y,z)) voxptr[numvoxs].vis |= 2;
					if (!isvoxelsolid(x,y-1,z)) voxptr[numvoxs].vis |= 4;
					if (!isvoxelsolid(x,y+1,z)) voxptr[numvoxs].vis |= 8;
					//if (z == v[1]) voxptr[numvoxs].vis |= 16;
					//if (z == nv[3]-1) voxptr[numvoxs].vis |= 32;
					if (!isvoxelsolid(x,y,z-1)) voxptr[numvoxs].vis |= 16;
					if (!isvoxelsolid(x,y,z+1)) voxptr[numvoxs].vis |= 32;

					voxptr[numvoxs].dir = 0; //FIX THIS!!!
					numvoxs++;
				}

#if 0
				if (delvxl) //Quick&dirty dealloc from VXL (bad for holes!)
				{
						//invalidate current vptr safely
					isnewfloatingchg((long)v,0);

					k = nv-v; //perform slng(nv) and adjust vlst at same time
					for(ov=nv;ov[0];ov+=ov[0]*4)
						isnewfloatingchg((long)ov,((long)ov)-k);

					j = (long)ov-(long)nv+(ov[2]-ov[1]+1)*4+4;

						//shift end of RLE column up
					v[0] = nv[0]; v[1] = nv[1]; v[2] = nv[2];
					for(i=4;i<j;i+=4) *(long *)&v[i] = *(long *)&nv[i];

						//remove end of RLE column from vbit
					i = ((((long)(&v[i]))-(long)vbuf)>>2); j = (k>>2)+i;
#if 0
					while (i < j) { vbit[i>>5] &= ~(1<<i); i++; }
#else
					if (!((j^i)&~31))
						vbit[i>>5] &= ~(p2m[j&31]^p2m[i&31]);
					else
					{
						vbit[i>>5] &=   p2m[i&31];  i >>= 5;
						vbit[j>>5] &= (~p2m[j&31]); j >>= 5;
						for(j--;j>i;j--) vbit[j] = 0;
					}
#endif
					nv = v;
				}
#endif
			}
			*ylenptr++ = numvoxs-oyvoxs; oyvoxs = numvoxs;
		}
		*xlenptr++ = numvoxs-oxvoxs; oxvoxs = numvoxs;
	}

	if (delvxl)
		for(x=xs;x<=xe;x++)
			for(y=ys;y<=ye;y++)
				for(v=sptr[y*VSID+x];v[0];v=nv)
				{
					nv = v+v[0]*4;

					i = isnewfloatingot((long)v);
					if (((unsigned long)i >= vx5.flstcnt[fi].i1) || (i < vx5.flstcnt[fi].i0))
						continue;

						//Quick&dirty dealloc from VXL (bad for holes!)

						//invalidate current vptr safely
					isnewfloatingchg((long)v,0);

					k = nv-v; //perform slng(nv) and adjust vlst at same time
					for(ov=nv;ov[0];ov+=ov[0]*4)
						isnewfloatingchg((long)ov,((long)ov)-k);

					j = (long)ov-(long)nv+(ov[2]-ov[1]+1)*4+4;

						//shift end of RLE column up
					v[0] = nv[0]; v[1] = nv[1]; v[2] = nv[2];
					for(i=4;i<j;i+=4) *(long *)&v[i] = *(long *)&nv[i];

						//remove end of RLE column from vbit
					i = ((((long)(&v[i]))-(long)vbuf)>>2); j = (k>>2)+i;
#if 0
					while (i < j) { vbit[i>>5] &= ~(1<<i); i++; }
#else
					if (!((j^i)&~31))
						vbit[i>>5] &= ~(p2m[j&31]^p2m[i&31]);
					else
					{
						vbit[i>>5] &=   p2m[i&31];  i >>= 5;
						vbit[j>>5] &= (~p2m[j&31]); j >>= 5;
						for(j--;j>i;j--) vbit[j] = 0;
					}
#endif
					nv = v;
				}

	vx5.flstcnt[fi].i1 = -2; //Mark flstcnt[i] invalid; no scum2 fixup

	if (vx5.vxlmipuse > 1) genmipvxl(xs,ys,xe+1,ye+1);

	return(vx5.flstcnt[fi].mass);
}

void finishfalls ()
{
	long i, x, y;

		//Scum2 box fixup: refreshes rle voxel data inside a bounding rectangle
	for(i=vx5.flstnum-1;i>=0;i--)
		if (vx5.flstcnt[i].i1 < 0)
		{
			if (vx5.flstcnt[i].i1 == -1)
			{
				for(y=vx5.flstcnt[i].y0;y<=vx5.flstcnt[i].y1;y++)
					for(x=vx5.flstcnt[i].x0;x<=vx5.flstcnt[i].x1;x++)
						scum2(x,y);
				scum2finish();
				updatebbox(vx5.flstcnt[i].x0,vx5.flstcnt[i].y0,vx5.flstcnt[i].z0,vx5.flstcnt[i].x1,vx5.flstcnt[i].y1,vx5.flstcnt[i].z1,0);
			}
			vx5.flstcnt[i] = vx5.flstcnt[--vx5.flstnum]; //onground, so delete flstcnt[i]
		}
}

//float detection & falling code ends ----------------------------------------

//----------------------------------------------------------------------------

/**
 * Since voxlap is currently a software renderer and I don't have any system
 * dependent code in it, you must provide it with the frame buffer. You
 * MUST call this once per frame, AFTER startdirectdraw(), but BEFORE any
 * functions that access the frame buffer.
 * @param p pointer to the top-left corner of the frame
 * @param b pitch (bytes per line)
 * @param x frame width
 * @param y frame height
 */
void voxsetframebuffer (long p, long b, long x, long y)
{
	long i;

	frameplace = p;
	if (x > MAXXDIM) x = MAXXDIM; //This sucks, but it crashes without it
	if (y > MAXYDIM) y = MAXYDIM;

		//Set global variables used by kv6draw's PIII asm (drawboundcube)
	qsum1[3] = qsum1[1] = 0x7fff-y; qsum1[2] = qsum1[0] = 0x7fff-x;
	kv6bytesperline = qbplbpp[1] = b; qbplbpp[0] = 4;
	kv6frameplace = p - (qsum1[0]*qbplbpp[0] + qsum1[1]*qbplbpp[1]);

	if ((b != ylookup[1]) || (x != xres_voxlap) || (y != yres_voxlap))
	{
		bytesperline = b; xres_voxlap = x; yres_voxlap = y; xres4_voxlap = (xres_voxlap<<2);
		ylookup[0] = 0; for(i=0;i<yres_voxlap;i++) ylookup[i+1] = ylookup[i]+bytesperline;
		//gihx = gihz = (float)xres_voxlap*.5f; gihy = (float)yres_voxlap*.5f; //BAD!!!
#if (USEZBUFFER == 1)
		if ((ylookup[yres_voxlap]+256 > zbuffersiz) || (!zbuffermem))  //Increase Z buffer size if too small
		{
			if (zbuffermem) { free(zbuffermem); zbuffermem = 0; }
			zbuffersiz = ylookup[yres_voxlap]+256;
			if (!(zbuffermem = (long *)malloc(zbuffersiz))) evilquit("voxsetframebuffer: allocation too big");
		}
#endif
	}
#if (USEZBUFFER == 1)
		//zbuffer aligns its memory to the same pixel boundaries as the screen!
		//WARNING: Pentium 4's L2 cache has severe slowdowns when 65536-64 <= (zbufoff&65535) < 64
	zbufoff = (((((long)zbuffermem)-frameplace-128)+255)&~255)+128;
#endif
	uurend = &uurendmem[((frameplace&4)^(((long)uurendmem)&4))>>2];

	if (vx5.fogcol >= 0)
	{
		fogcol = (((int64_t)(vx5.fogcol&0xff0000))<<16) +
					(((int64_t)(vx5.fogcol&0x00ff00))<< 8) +
					(((int64_t)(vx5.fogcol&0x0000ff))    );

		if (vx5.maxscandist > 2047) vx5.maxscandist = 2047;
		if ((vx5.maxscandist != ofogdist) && (vx5.maxscandist > 0))
		{
			ofogdist = vx5.maxscandist;

			//foglut[?>>20] = MIN(?*32767/vx5.maxscandist,32767)
#if 0
			long j, k, l;
			j = 0; l = 0x7fffffff/vx5.maxscandist;
			for(i=0;i<2048;i++)
			{
				k = (j>>16); j += l;
				if (k < 0) break;
				foglut[i] = (((int64_t)k)<<32)+(((int64_t)k)<<16)+((int64_t)k);
			}
			while (i < 2048) foglut[i++] = all32767;
#else
			i = 0x7fffffff/vx5.maxscandist;
			#ifdef __GNUC__ //gcc inline asm
			__asm__ __volatile__
			(
				".intel_syntax noprefix\n"
				"xor	eax, eax\n"
				"mov	ecx, -2048*8\n"
			".Lfogbeg:\n"
				"movd	mm0, eax\n"
				"add	eax, edx\n"
				"jo	short .Lfogend\n"
				"pshufw	mm0, mm0, 0x55\n"
				"movq	foglut[ecx+2048*8], mm0\n"
				"add	ecx, 8\n"
				"js	short .Lfogbeg\n"
				"jmp	short .Lfogend2\n"
			".Lfogend:\n"
				"movq	mm0, all32767\n"
			".Lfogbeg2:\n"
				"movq	foglut[ecx+2048*8], mm0\n"
				"add	ecx, 8\n"
				"js	short .Lfogbeg2\n"
			".Lfogend2:\n"
				"emms\n"
				".att_syntax prefix\n"
				:
				: [i] "d" (i)
				: "eax", "ecx", "mm0"
			);
			#endif
			#ifdef _MSC_VER //msvc inline asm
			_asm
			{
				xor	eax, eax
				mov	ecx, -2048*8
				mov	edx, i
			fogbeg:
				movd	mm0, eax
				add	eax, edx
				jo	short fogend
				pshufw	mm0, mm0, 0x55
				movq	foglut[ecx+2048*8], mm0
				add	ecx, 8
				js	short fogbeg
				jmp	short fogend2
			fogend:
				movq	mm0, all32767
			fogbeg2:
				movq	foglut[ecx+2048*8], mm0
				add	ecx, 8
				js	short fogbeg2
			fogend2:
				emms
			}
			#endif
#endif
		}
	} else ofogdist = -1;

	if (cputype&(1<<25)) drawboundcubesseinit(); else drawboundcube3dninit();
}

//------------------------ Simple PNG OUT code begins ------------------------
FILE *pngofil;
long pngoxplc, pngoyplc, pngoxsiz, pngoysiz;
unsigned long pngocrc, pngoadcrc;

long crctab32[256];  //SEE CRC32.C
#define updatecrc32(c,crc) crc=(crctab32[(crc^c)&255]^(((unsigned)crc)>>8))
#define updateadl32(c,crc) \
{  c += (crc&0xffff); if (c   >= 65521) c   -= 65521; \
	crc = (crc>>16)+c; if (crc >= 65521) crc -= 65521; \
	crc = (crc<<16)+c; \
} \

void fputbytes (unsigned long v, long n)
	{ for(;n;v>>=8,n--) { fputc(v,pngofil); updatecrc32(v,pngocrc); } }

void pngoutopenfile (const char *fnam, long xsiz, long ysiz)
{
	long i, j, k;
	char a[40];

	pngoxsiz = xsiz; pngoysiz = ysiz; pngoxplc = pngoyplc = 0;
	for(i=255;i>=0;i--)
	{
		k = i; for(j=8;j;j--) k = ((unsigned long)k>>1)^((-(k&1))&0xedb88320);
		crctab32[i] = k;
	}
	pngofil = fopen(fnam,"wb");
	*(long *)&a[0] = 0x474e5089; *(long *)&a[4] = 0x0a1a0a0d;
	*(long *)&a[8] = 0x0d000000; *(long *)&a[12] = 0x52444849;
	*(long *)&a[16] = bswap(xsiz); *(long *)&a[20] = bswap(ysiz);
	*(long *)&a[24] = 0x00000208; *(long *)&a[28] = 0;
	for(i=12,j=-1;i<29;i++) updatecrc32(a[i],j);
	*(long *)&a[29] = bswap(j^-1);
	fwrite(a,37,1,pngofil);
	pngocrc = 0xffffffff; pngoadcrc = 1;
	fputbytes(0x54414449,4); fputbytes(0x0178,2);
}

void pngoutputpixel (long rgbcol)
{
	long a[4];

	if (!pngoxplc)
	{
		fputbytes(pngoyplc==pngoysiz-1,1);
		fputbytes(((pngoxsiz*3+1)*0x10001)^0xffff0000,4);
		fputbytes(0,1); a[0] = 0; updateadl32(a[0],pngoadcrc);
	}
	fputbytes(bswap(rgbcol<<8),3);
	a[0] = (rgbcol>>16)&255; updateadl32(a[0],pngoadcrc);
	a[0] = (rgbcol>> 8)&255; updateadl32(a[0],pngoadcrc);
	a[0] = (rgbcol    )&255; updateadl32(a[0],pngoadcrc);
	pngoxplc++; if (pngoxplc < pngoxsiz) return;
	pngoxplc = 0; pngoyplc++; if (pngoyplc < pngoysiz) return;
	fputbytes(bswap(pngoadcrc),4);
	a[0] = bswap(pngocrc^-1); a[1] = 0; a[2] = 0x444e4549; a[3] = 0x826042ae;
	fwrite(a,1,16,pngofil);
	a[0] = bswap(ftell(pngofil)-(33+8)-16);
	fseek(pngofil,33,SEEK_SET); fwrite(a,1,4,pngofil);
	fclose(pngofil);
}
//------------------------- Simple PNG OUT code ends -------------------------

/** Captures a screenshot of the current frame to disk.
 *  The current frame is defined by the last call to the voxsetframebuffer function.
 *  @note You <b>MUST</b> call this function while video memory is accessible. In
 *  DirectX, that means it MUST only be between a call to startdirectdraw and
 *  stopdirectdraw.
 *  @param fname filename to write to (writes uncompressed .PNG format)
 *  @return 0:always
 */
long screencapture32bit (const char *fname)
{
	long p, x, y;

	pngoutopenfile(fname,xres_voxlap,yres_voxlap);
	p = frameplace;
	for(y=0;y<yres_voxlap;y++,p+=bytesperline)
		for(x=0;x<xres_voxlap;x++)
			pngoutputpixel(*(long *)(p+(x<<2)));

	return(0);
}

/** Generates a cubic panorama (skybox) from the given position.
 *  This is an old function that is very slow, but it is pretty cool
 *  being able to view a full panorama screenshot. Unfortunately, it
 *  doesn't draw sprites or the sky.
 *
 *  @param pos VXL map position of camera
 *  @param fname filename to write to (writes uncompressed .PNG format)
 *  @param boxsiz length of side of square. I recommend using 256 or 512 for this.
 *  @return 0:always
 */
long surroundcapture32bit (dpoint3d *pos, const char *fname, long boxsiz)
{
	lpoint3d hit;
	dpoint3d d;
	long x, y, hboxsiz, *hind, hdir;
	float f;

	//Picture layout:
	//   ÛÛÛÛÛÛúúúú
	//   úúúúÛÛÛÛÛÛ

	f = 2.0 / (float)boxsiz; hboxsiz = (boxsiz>>1);
	pngoutopenfile(fname,boxsiz*5,boxsiz*2);
	for(y=-hboxsiz;y<hboxsiz;y++)
	{
		for(x=-hboxsiz;x<hboxsiz;x++) //(1,1,-1) - (-1,1,1)
		{
			d.x = -(x+.5)*f; d.y = 1; d.z = (y+.5)*f;
			hitscan(pos,&d,&hit,&hind,&hdir);
			if (hind) pngoutputpixel(lightvox(*hind)); else pngoutputpixel(0);
		}
		for(x=-hboxsiz;x<hboxsiz;x++) //(-1,1,-1) - (-1,-1,1)
		{
			d.x = -1; d.y = -(x+.5)*f; d.z = (y+.5)*f;
			hitscan(pos,&d,&hit,&hind,&hdir);
			if (hind) pngoutputpixel(lightvox(*hind)); else pngoutputpixel(0);
		}
		for(x=-hboxsiz;x<hboxsiz;x++) //(-1,-1,-1) - (1,-1,1)
		{
			d.x = (x+.5)*f; d.y = -1; d.z = (y+.5)*f;
			hitscan(pos,&d,&hit,&hind,&hdir);
			if (hind) pngoutputpixel(lightvox(*hind)); else pngoutputpixel(0);
		}
		for(x=(boxsiz<<1);x>0;x--) pngoutputpixel(0);
	}
	for(y=-hboxsiz;y<hboxsiz;y++)
	{
		for(x=(boxsiz<<1);x>0;x--) pngoutputpixel(0);
		for(x=-hboxsiz;x<hboxsiz;x++) //(-1,-1,1) - (1,1,1)
		{
			d.x = (x+.5)*f; d.y = (y+.5)*f; d.z = 1;
			hitscan(pos,&d,&hit,&hind,&hdir);
			if (hind) pngoutputpixel(lightvox(*hind)); else pngoutputpixel(0);
		}
		for(x=-hboxsiz;x<hboxsiz;x++) //(1,-1,1) - (1,1,-1)
		{
			d.x = 1; d.y = (y+.5)*f; d.z = -(x+.5)*f;
			hitscan(pos,&d,&hit,&hind,&hdir);
			if (hind) pngoutputpixel(lightvox(*hind)); else pngoutputpixel(0);
		}
		for(x=-hboxsiz;x<hboxsiz;x++) //(1,-1,-1) - (-1,1,-1)
		{
			d.x = -(x+.5)*f; d.y = (y+.5)*f; d.z = -1;
			hitscan(pos,&d,&hit,&hind,&hdir);
			if (hind) pngoutputpixel(lightvox(*hind)); else pngoutputpixel(0);
		}
	}
	return(0);
}


#if 0
/** @note This doesn't speed it up and it only makes it crash on some computers :/ */
static inline void fixsse ()
{
	static long asm32;
	#if (defined(__GNUC__) && (USEV5ASM != 0))
	__asm__ __volatile__
	(
		".intel_syntax noprefix\n"
		"stmxcsr	[asm32]\n" //Default is:0x1f80
		"or	asm32, 0x8040\n"   //enable ftz&daz to prevent slow denormals!
		"ldmxcsr	[asm32]\n"
		".att_syntax prefix\n"
	);
	#elif (defined(_MSC_VER) && (USEV5ASM != 0))
	_asm
	{
		stmxcsr	[asm32]   //Default is:0x1f80
		or	asm32, 0x8040 //enable ftz&daz to prevent slow denormals!
		ldmxcsr	[asm32]
	}
	#endif
}
#endif

void uninitvoxlap ()
{
	if (sxlbuf) { free(sxlbuf); sxlbuf = 0; }

	if (vbuf) { free(vbuf); vbuf = 0; }
	if (vbit) { free(vbit); vbit = 0; }

	if (khashbuf)
	{     //Free all KV6&KFA on hash list
		long i, j;
		kfatype *kfp;
		for(i=0;i<khashpos;i+=strlen(&khashbuf[i+9])+10)
		{
			switch (khashbuf[i+8])
			{
				case 0: //KV6
					freekv6(*(kv6data **)&khashbuf[i+4]);
					break;
				case 1: //KFA
					kfp = *(kfatype **)&khashbuf[i+4];
					if (!kfp) continue;
					if (kfp->seq) free((void *)kfp->seq);
					if (kfp->frmval) free((void *)kfp->frmval);
					if (kfp->hingesort) free((void *)kfp->hingesort);
					if (kfp->hinge) free((void *)kfp->hinge);
					if (kfp->spr)
					{
						for(j=kfp->numspr-1;j>=0;j--)
							if (kfp->spr[j].voxnum)
								freekv6((kv6data *)kfp->spr[j].voxnum);
						free((void *)kfp->spr);
					}
					free((void *)kfp);
					break;
				default: _gtfo(); //tells MSVC default can't be reached
			}
		}
		free(khashbuf); khashbuf = 0; khashpos = khashsiz = 0;
	}

	if (skylng) { free((void *)skylng); skylng = 0; }
	if (skylat) { free((void *)skylat); skylat = 0; }
	if (skypic) { free((void *)skypic); skypic = skyoff = 0; }

	if (vx5.pic) { free(vx5.pic); vx5.pic = 0; }
#if (USEZBUFFER == 1)
	if (zbuffermem) { free(zbuffermem); zbuffermem = 0; }
#endif
	if (radarmem) { free(radarmem); radarmem = 0; radar = 0; }
}

long initvoxlap ()
{
	int64_t q;
	long i, j, k, z, zz;
	float f, ff;

		//unlocking code memory for self-modifying code
	#if (defined(USEV5ASM) && (USEV5ASM != 0)) //if true
	code_rwx_unlock((void *)dep_protect_start, (void *)dep_protect_end);
	#endif

		//CPU Must have: FPU,RDTSC,CMOV,MMX,MMX+
	if ((cputype&((1<<0)|(1<<4)|(1<<15)|(1<<22)|(1<<23))) !=
					 ((1<<0)|(1<<4)|(1<<15)|(1<<22)|(1<<23))) return(-1);
		//CPU UNSUPPORTED!
	if ((!(cputype&(1<<25))) && //SSE
		(!((cputype&((1<<30)|(1<<31))) == ((1<<30)|(1<<31))))) //3DNow!+
		return(-1);
	//if (cputype&(1<<25)) fixsse(); //SSE

	  //WARNING: xres_voxlap & yres_voxlap are local to VOXLAP5.C so don't rely on them here!
	if (!(radarmem = (long *)malloc(MAX((((MAXXDIM*MAXYDIM*27)>>1)+7)&~7,(VSID+4)*3*SCPITCH*4+8))))
		return(-1);
	radar = (long *)((((long)radarmem)+7)&~7);

	for(i=0;i<32;i++) { xbsflor[i] = (-1<<i); xbsceil[i] = ~xbsflor[i]; }

		//Setsphere precalculations (factr[] tables) (Derivation in POWCALC.BAS)
		//   if (!factr[z][0]) z's prime else factr[z][0]*factr[z][1] == z
	factr[2][0] = 0; i = 1; j = 9; k = 0;
	for(z=3;z<SETSPHMAXRAD;z+=2)
	{
		if (z == j) { j += (i<<2)+12; i += 2; }
		factr[z][0] = 0; factr[k][1] = z;
		for(zz=3;zz<=i;zz=factr[zz][1])
			if (!(z%zz)) { factr[z][0] = zz; factr[z][1] = z/zz; break; }
		if (!factr[z][0]) k = z;
		factr[z+1][0] = ((z+1)>>1); factr[z+1][1] = 2;
	}
	for(z=1;z<SETSPHMAXRAD;z++) logint[z] = log((double)z);

#if (ESTNORMRAD == 2)
		//LUT for ESTNORM
	fsqrecip[0] = 0.f; fsqrecip[1] = 1.f;
	fsqrecip[2] = (float)(1.f/sqrt(2.f)); fsqrecip[3] = (float)1.f/sqrt(3.f);
	for(z=4,i=3;z<sizeof(fsqrecip)/sizeof(fsqrecip[0]);z+=6) //fsqrecip[z] = 1/sqrt(z);
	{
		fsqrecip[z+0] = fsqrecip[(z+0)>>1]*fsqrecip[2];
		fsqrecip[z+2] = fsqrecip[(z+2)>>1]*fsqrecip[2];
		fsqrecip[z+4] = fsqrecip[(z+4)>>1]*fsqrecip[2];
		fsqrecip[z+5] = fsqrecip[i]*fsqrecip[3]; i += 2;

		f = (fsqrecip[z+0]+fsqrecip[z+2])*.5f;
		if (z <= 22) f = (1.5f-(.5f*((float)(z+1))) * f*f)*f;
		fsqrecip[z+1] = (1.5f-(.5f*((float)(z+1))) * f*f)*f;

		f = (fsqrecip[z+2]+fsqrecip[z+4])*.5f;
		if (z <= 22) f = (1.5f-(.5f*((float)(z+3))) * f*f)*f;
		fsqrecip[z+3] = (1.5f-(.5f*((float)(z+3))) * f*f)*f;
	}
#endif

		//Lookup table to save 1 divide for gline()
	for(i=1;i<CMPRECIPSIZ;i++) cmprecip[i] = CMPPREC/(float)i;

		//Flashscan equal-angle compare table
	for(i=0;i<(1<<LOGFLASHVANG)*8;i++)
	{
		if (!(i&((1<<LOGFLASHVANG)-1)))
			j = (gfclookup[i>>LOGFLASHVANG]<<4)+8 - (1<<LOGFLASHVANG)*64;
		gfc[i].y = j; j += 64*2;
		ftol(sqrt((1<<(LOGFLASHVANG<<1))*64.f*64.f-gfc[i].y*gfc[i].y),&gfc[i].x);
	}

		//Init norm flash variables:
	ff = (float)GSIZ*.5f; // /(1);
	for(z=1;z<(GSIZ>>1);z++)
	{
		ffxptr = &ffx[(z+1)*z-1];
		f = ff; ff = (float)GSIZ*.5f/((float)z+1);
		for(zz=-z;zz<=z;zz++)
		{
			if (zz <= 0) i = (long)(((float)zz-.5f)*f); else i = (long)(((float)zz-.5f)*ff);
			if (zz >= 0) j = (long)(((float)zz+.5f)*f); else j = (long)(((float)zz+.5f)*ff);
			ffxptr[zz].x = (unsigned short)MAX(i+(GSIZ>>1),0);
			ffxptr[zz].y = (unsigned short)MIN(j+(GSIZ>>1),GSIZ);
		}
	}
	for(i=0;i<=25*5;i+=5) xbsbuf[i] = 0x00000000ffffffff;
	for(z=0;z<32;z++) { p2c[z] = (1<<z); p2m[z] = p2c[z]-1; }

		//Drawtile lookup table:
	//q = 0;
	//for(i=0;i<256;i++) { alphalookup[i] = q; q += 0x1000100010; }

		//Initialize univec normals (for KV6 lighting)
	equivecinit(255);
	//for(i=0;i<255;i++)
	//{
	//   univec[i].z = ((float)((i<<1)-254))/255.0;
	//   f = sqrt(1.0 - univec[i].z*univec[i].z);
	//   fcossin((float)i*(GOLDRAT*PI*2),&univec[i].x,&univec[i].y);
	//   univec[i].x *= f; univec[i].y *= f;
	//}
	//univec[255].x = univec[255].y = univec[255].z = 0;
	for(i=0;i<256;i++)
	{
		iunivec[i][0] = (short)(univec[i].x*4096.0);
		iunivec[i][1] = (short)(univec[i].y*4096.0);
		iunivec[i][2] = (short)(univec[i].z*4096.0);
		iunivec[i][3] = 4096;
	}
	ucossininit();

	memset(mixn,0,sizeof(mixn));

		//Initialize hash table for getkv6()
	memset(khashead,-1,sizeof(khashead));
	if (!(khashbuf = (char *)malloc(KHASHINITSIZE))) return(-1);
	khashsiz = KHASHINITSIZE;

	vx5.anginc = 1; //Higher=faster (1:full,2:half)
	vx5.sideshademode = 0; setsideshades(0,0,0,0,0,0);
	vx5.mipscandist = 128;
	vx5.maxscandist = 256; //must be <= 2047
	vx5.colfunc = curcolfunc; //This prevents omission bugs from crashing voxlap5
	vx5.curcol = 0x80804c33;
	vx5.currad = 8;
	vx5.curhei = 0;
	vx5.curpow = 2.0;
	vx5.amount = 0x70707;
	vx5.pic = 0;
	vx5.cliphitnum = 0;
	vx5.xplanemin = 0;
	vx5.xplanemax = 0x7fffffff;
	vx5.flstnum = 0;
	vx5.lightmode = 0;
	vx5.numlights = 0;
	vx5.kv6mipfactor = 96;
	vx5.kv6col = 0x808080;
	vx5.vxlmipuse = 1;
	vx5.fogcol = -1;
	vx5.fallcheck = 0;

	gmipnum = 0;

	return(0);
}

#if 0 //ndef _WIN32
	long i, j, k, l;
	char *v;

	j = k = l = 0;
	for(i=0;i<VSID*VSID;i++)
	{
		for(v=sptr[i];v[0];v+=v[0]*4) { j++; k += v[2]-v[1]+1; l += v[0]-1; }
		k += v[2]-v[1]+1; l += v[2]-v[1]+1;
	}

	printf("VOXLAP5 programmed by Ken Silverman (www.advsys.net/ken)\n");
	printf("Please DO NOT DISTRIBUTE! If this leaks, I will not be happy.\n\n");
	//printf("This copy licensed to:  \n\n");
	printf("Memory statistics upon exit: (all numbers in bytes)");
	printf("\n");
	if (screen) printf("   screen: %8ld\n",imageSize);
	printf("    radar: %8ld\n",MAX((((MAXXDIM*MAXYDIM*27)>>1)+7)&~7,(VSID+4)*3*SCPITCH*4+8));
	printf("  bacsptr: %8ld\n",sizeof(bacsptr));
	printf("     sptr: %8ld\n",(VSID*VSID)<<2);
	printf("     vbuf: %8ld(%8ld)\n",(j+VSID*VSID+l)<<2,VOXSIZ);
	printf("     vbit: %8ld(%8ld)\n",VOXSIZ>>5);
	printf("\n");
	printf("vbuf head: %8ld\n",(j+VSID*VSID)<<2);
	printf("vbuf cols: %8ld\n",l<<2);
	printf("     fcol: %8ld\n",k<<2);
	printf("     ccol: %8ld\n",(l-k)<<2);
	printf("\n");
	printf("%.2f bytes/column\n",(float)((j+VSID*VSID+l)<<2)/(float)(VSID*VSID));
#endif
