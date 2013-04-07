#pragma once
	/** Screen related variables: */

#define PREC (256*4096)
#define CMPPREC (256*4096)
#define FPREC (256*4096)
#define USEZBUFFER 1                //Should a Z-Buffer be Used?
/** Opticast global variables
 *  radar: 320x200 requires  419560*2 bytes (area * 6.56*2)
 *  radar: 400x300 requires  751836*2 bytes (area * 6.27*2)
 *  radar: 640x480 requires 1917568*2 bytes (area * 6.24*2)
 */
#define SCPITCH 256
#define SCISDIST 1.0
#define GOLDRAT 0.3819660112501052  /** Golden Ratio: 1 - 1/((sqrt(5)+1)/2) */
#define ESTNORMRAD 2                /** \warning Specially optimized for 2: DON'T CHANGE unless testing! */
//#define NOASM                     //Instructs compiler to use C(++) alternatives

static long xres_voxlap, yres_voxlap, bytesperline, frameplace, xres4_voxlap;
long ylookup[MAXYDIM+1];
short qsum0[4], qsum1[4];

static long lastx[MAX(MAXYDIM,VSID)], uurendmem[MAXXDIM*2+8], *uurend;
	/** Rendering */
static unsigned short xyoffs[256][256+1]; /** @note used only by setkvx */
static float optistrx, optistry, optiheix, optiheiy, optiaddx, optiaddy;
	/** Related to fog functions */
static int64_t foglut[2048] FORCE_NAME("foglut"), fogcol;
static long ofogdist = -1;
static int64_t all32767 FORCE_NAME("all32767") = 0x7fff7fff7fff7fff;

	/** Parallaxing sky variables */
static long skypic = 0, nskypic = 0, skybpl, skyysiz, skycurlng, skycurdir;
static float skylngmul;
static point2d *skylng = 0;

#ifdef __cplusplus
extern "C" {
#endif
	long gylookup[512+36], gmipnum = 0; //256+4+128+4+64+4+...
	long gpz[2], gdz[2], gxmip, gxmax, gixy[2], gpixy;
	long zbufoff;

	long skyoff = 0, skyxsiz, *skylat = 0;
	char *sptr[(VSID*VSID*4)/3];
	
	#ifdef VOXLAP5
	void dep_protect_start();
	void dep_protect_end();
	void grouscanasm(long);
	void drawboundcubesseinit ();
	void drawboundcubesse (kv6voxtype *, long);
	void drawboundcube3dninit ();
	void drawboundcube3dn (kv6voxtype *, long);
	#endif


	int64_t gi, gcsub[8] =
	{
		0xff00ff00ff00ff,0xff00ff00ff00ff,0xff00ff00ff00ff,0xff00ff00ff00ff,
		0xff00ff00ff00ff,0xff00ff00ff00ff,0xff00ff00ff00ff,0xff00ff00ff00ff
	};
	static lpoint3d glipos;             // gline and opticast
	/** @note global eyepos opticast drawpoint 3d drawline3d setcamera project2d drawspherefill kv6draw */
	static point3d gipos;
	static point3d gistr, gihei, gifor;
	static point3d gixs, giys, gizs, giadd; // setcamera kv6draw
	static float gihx, gihy, gihz, gposxfrac[2], gposyfrac[2], grd;
	static long gposz, giforzsgn, gstartz0, gstartz1, gixyi[2];
	static char *gstartv;
	static point3d gcorn[4];
			 point3d ginor[4]; /** @note Should be static, but... necessary for stupid pingball hack :/ */
	#define gi0 (((long *)&gi)[0])
	#define gi1 (((long *)&gi)[1])
	static long gmaxscandist;

#ifdef __cplusplus
}
#endif


	/** Render related variables: */
#if (USEZBUFFER == 0)
	typedef struct { long col; } castdat;
#else
	static long *zbuffermem = 0, zbuffersiz = 0;
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

static castdat *angstart[MAXXDIM*4], *gscanptr;
#define CMPRECIPSIZ MAXXDIM+32
static float cmprecip[CMPRECIPSIZ], wx0, wy0, wx1, wy1; // Lookup table
static long iwx0, iwy0, iwx1, iwy1;

#define VFIFSIZ 16384 //SHOULDN'T BE STATIC ALLOCATION!!! floodsucksprite
static long vfifo[VFIFSIZ]; // used in floodsucksprite

void mat0(point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *);
void mat1(point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *);
void mat2(point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *, point3d *);

static int64_t mskp255 = 0x00ff00ff00ff00ff;
static int64_t mskn255 = 0xff01ff01ff01ff01;
static int64_t rgbmask64 = 0xffffff00ffffff;

long *radar = 0, *radarmem = 0;
