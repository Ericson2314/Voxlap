// VOXLAP engine by Ken Silverman (http://advsys.net/ken)
// This file has been modified from Ken Silverman's original release

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
//#define SYSMAIN_C //if sysmain is compiled as C
#include "sysmain.h"
//#define VOXLAP_C  //if voxlap5 is compiled as C
#include "voxlap5.h"
//#define KPLIB_C  //if kplib is compiled as C
#include "kplib.h"

	//Ericson2314's dirty porting tricks
#include "porthacks.h"

	//Ken's short, general-purpose to-be-inlined functions mainly consisting of inline assembly are now here
#include "ksnippits.h"

	//mmxcolor* now defined here
#include "voxflash.h"

#define SCISSORDIST 1.0
#define STEREOMODE 0  //0:no stereo (normal mode), 1:CrystalEyes, 2:Nuvision
#define USETDHELP 0 //0:Ken's notepad style help, 1:Tom's keyboard graphic help

#if defined(_WIN16) || defined(_DOS)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <dos.h>

void limitrate ();
#pragma aux limitrate =\
	"mov dx, 0x3da",\
	"wait1: in al, dx",\
	"test al, 8",\
	"jnz wait1",\
	"wait2: in al, dx",\
	"test al, 8",\
	"jz wait2",\
	modify exact [eax edx]\
	value

#endif

#define MAXBUL 1
long bulactive[MAXBUL] = {-1};
dpoint3d bul[MAXBUL], bulvel[MAXBUL];

	//KV6 sprite variables:
#define MAXSPRITES 1024
vx5sprite spr[MAXSPRITES];
long sortorder[MAXSPRITES], numsprites = 0, curspri = -1;
float sortdist[MAXSPRITES];

	//Sprite user messages:
char *sxlbuf = 0;
long sxlmallocsiz = 0, sxlind[MAXSPRITES+1];
	//sxlind: sxlind[0] is index to global userstring
	//sxlind: 1 more for using offsets instead of lengths

	//Position variables:
#define CLIPRAD 5
dpoint3d ipos, ivel, istr, ihei, ifor;
float vx5hx, vx5hy, vx5hz;

	//Timer global variables
double odtotclk, dtotclk;
float fsynctics;
long totclk;

	//FPS variables (integrated with timer handler)
#define AVERAGEFRAMES 32
long frameval[AVERAGEFRAMES], numframes = 0, averagefps = 0;

	//Misc global variables for doframe
static long *ohind, hindclk, obstatus = 0, bstatus = 0, oxres = -1, oyres = -1;

	//(&0x30000) == 0 means don't back up
	//(&0x10000) != 0 means back up all geometry
	//(&0x20000) != 0 means back up intensity only (faster)
enum { DONTBACKUP=0,
		 SETSPR=1,SETFIL,
		 SETREC=0x10000,SETTRI,SETSPH,SETELL,SETCYL,SETHUL,SETWAL,SETCEI,
							 SETSEC,SETLAT,SETBLO,SETKVX,SETDUP,SETDUQ,SETCOL,
		 SETFFL=0x20000,SETFLA,SETDYN };

static long frameplace, bytesperline;

EXTERN_C char *sptr[VSID*VSID];
EXTERN_VOXLAP long *radar;
EXTERN_VOXLAP long templongbuf[MAXZDIM];
EXTERN_VOXLAP long backtag, backedup, bacx0, bacy0, bacx1, bacy1;
extern long gxsizcache, gysizcache;
extern long cputype;
EXTERN_C long gmipnum;

	//Sprite data for hanging lights:
static kv6data *klight;

//Low-level editing functions
EXTERN_VOXLAP void scum (long, long, long, long, long *);
EXTERN_VOXLAP void scumfinish ();
EXTERN_VOXLAP long *scum2 (long, long);
EXTERN_VOXLAP void scum2finish ();

EXTERN_VOXLAP long findpath (long *, long, lpoint3d *, lpoint3d *);
#define PATHMAXSIZ 4096
long pathpos[PATHMAXSIZ], pathcnt = -1;
lpoint3d pathlast;

long capturecount = 0;
char sxlfilnam[MAX_PATH+1] = "";
char vxlfilnam[MAX_PATH+1] = "";
char skyfilnam[MAX_PATH+1] = "";

	//Displayed text message:
char message[256] = {0};
long messagetimeout = 0;

	//Help text status
static long helpmode = 0, helpleng, helpypos = 0;
static char *helpbuf = 0;

static float fmouseymul = -.008;

long anglemode = 0, angincmode = 0, gridmode = 0;
long notepadmode = -1, notepadinsertmode = 1, notepadcurstime = 0;
float anglng = 0, anglat = 0;

	//Floating cursors
#define MAXCURS 100
point3d curs[MAXCURS], tempoint[MAXCURS], cursnorm, cursnoff;
long point2[MAXCURS];
long numcurs = 0, cursaxmov = 4, editcurs = -1, lastcurs = -1;

lpoint3d hit;
long *hind, hdir;

lpoint3d thit[2];
long tsolid = 0, trot = 8, tedit = 0;
char tfilenam[MAX_PATH+1];

long cubedistweight[4] = {192,7,5,4};

long colfnum = 2;
void *colfunclst[] =
{
	curcolfunc,floorcolfunc,jitcolfunc,manycolfunc,
	sphcolfunc,woodcolfunc,pngcolfunc
};
#define numcolfunc (sizeof(colfunclst)>>2)

	//RGB color selection variables
#define CGRAD 64
float fcmousx, fcmousy;
long gbri, cpik, spik, clcnt, cmousx, cmousy, colselectmode = 0;
short clx[CGRAD*6+1];

#ifdef __WATCOMC__

	//mm4: [0Gii0Bii]
	//mm5: [00000Rii]
void rgbcolselinitinc (long, long, long);
#pragma aux rgbcolselinitinc =\
	"movd mm4, ecx"\
	"movd mm0, ebx"\
	"punpckldq mm4, mm0"\
	"movd mm5, eax"\
	parm nomemory [eax][ebx][ecx]\
	modify exact []\
	value

	//mm2: [0Ggg0Bbb]
	//mm3: [00000Rrr]
void rgbcolselrend (long, long, long, long, long);
#pragma aux rgbcolselrend =\
	"movd mm2, ecx"\
	"movd mm0, ebx"\
	"punpckldq mm2, mm0"\
	"movd mm3, eax"\
	"sub edi, esi"\
	"beg: movq mm0, mm2"\
	"packuswb mm0, mm3"\
	"paddd mm2, mm4"\
	"psrlw mm0, 8"\
	"paddd mm3, mm5"\
	"packuswb mm0, mm0"\
	"movd [edi+esi], mm0"\
	"add edi, 4"\
	"js short beg"\
	parm [eax][ebx][ecx][edi][esi]\
	modify exact [edi]\
	value

#endif

static inline void rgbcolselinitinc (long a, long b, long c)
{
	#ifdef __GNUC__ //gcc inline asm
	__asm__
	(
		"mov a, %eax\n"
		"mov b, %edx\n"
		"mov c, %ecx\n"
		"movd %ecx, %mm4\n"
		"movd %edx, %mm0\n"
		"punpckldq %mm0, %mm4\n"
		"movd %eax, %mm5\n"
	);
	#endif
	#ifdef _MSC_VER //msvc inline asm
	_asm
	{
		mov eax, a
		mov edx, b
		mov ecx, c
		movd mm4, ecx
		movd mm0, edx
		punpckldq mm4, mm0
		movd mm5, eax
	}
	#endif
}

static inline void rgbcolselrend (long a, long b, long c, long t, long s)
{
	#ifdef __GNUC__ //gcc inline asm
	__asm__
	(
		"push %ebx\n"
		"push %esi\n"
		"push %edi\n"
		"mov a, %eax\n"
		"mov b, %ebx\n"
		"mov c, %ecx\n"
		"mov t, %edi\n"
		"mov s, %esi\n"
		"movd %ecx, %mm2\n"
		"movd %ebx, %mm0\n"
		"punpckldq %mm0, %mm2\n"
		"movd %eax, %mm3\n"
		"sub %esi, %edi\n"
	"beg:\n"
		"movq %mm2, %mm0\n"
		"packuswb %mm3, %mm0\n"
		"paddd %mm4, %mm2\n"
		"psrlw $8, %mm0\n"
		"paddd %mm5, %mm3\n"
		"packuswb %mm0, %mm0\n"
		"movd %mm0, (%edi,%esi)\n"
		"add $4, %edi\n"
		"js short beg\n"

		"pop %edi\n"
		"pop %esi\n"
		"pop %ebx\n"
	);
	#endif
	#ifdef _MSC_VER //msvc inline asm
	_asm
	{
		push ebx
		push esi
		push edi
		mov eax, a
		mov ebx, b
		mov ecx, c
		mov edi, t
		mov esi, s
		movd mm2, ecx
		movd mm0, ebx
		punpckldq mm2, mm0
		movd mm3, eax
		sub edi, esi
	beg:
		movq mm0, mm2
		packuswb mm0, mm3
		paddd mm2, mm4
		psrlw mm0, 8
		paddd mm3, mm5
		packuswb mm0, mm0
		movd [edi+esi], mm0
		add edi, 4
		js short beg

		pop edi
		pop esi
		pop ebx
	}
	#endif
}

void initrgbcolselect ()
{
	long x, y, oy, c, oc, fx, fy;

	cpik = -16777216/CGRAD; ftol((float)cpik/sqrt(3.f),&spik);
	cmousx = xres-CGRAD; cmousy = yres-CGRAD-4; gbri = (128<<16);
	fcmousx = (float)cmousx; fcmousy = (float)cmousy;

	oc = -1; oy = 0x8000ffff; clcnt = 0;
	for(y=-CGRAD;y<=CGRAD;y++)
		for(x=-CGRAD;x<=CGRAD;x++)
		{
			fx = x*spik; fy = y*cpik;
			if (fy > labs(fx))
			{
				if (((16777216+fx-fy)|(16777216-fx-fy)) < 0) continue;
				c = 0;
			}
			else if (fx < 0)
			{
				if (((16777216+fx+fx)|(16777216+fx+fy)) < 0) continue;
				c = 1;
			}
			else
			{
				if (((16777216-fx+fy)|(16777216-fx-fx)) < 0) continue;
				c = 2;
			}

			if ((oc == c) && (clx[clcnt-1] == x))
				{ clx[clcnt-1]++; }
			else
			{
				if (oy != y) { clx[clcnt++] = x; oy = y; }
				clx[clcnt++] = x+1; oc = c;
			}
		}
}

long cub2rgb (long x, long y, long dabri)
{
	long r, g, b;

	x *= spik; y *= cpik;
	if (y > labs(x)) { r = 16777216+x-y; g = 16777216; b = 16777216-x-y; }
	else if (x < 0)  { r = 16777216+x+x; g = 16777216+x+y; b = 16777216; }
	else             { r = 16777216; g = 16777216-x+y; b = 16777216-x-x; }
	r = umulshr32(r>>8,dabri); if ((unsigned long)r >= 256) return(-1);
	g = umulshr32(g>>8,dabri); if ((unsigned long)g >= 256) return(-1);
	b = umulshr32(b>>8,dabri); if ((unsigned long)b >= 256) return(-1);
	return((r<<16)+(g<<8)+b+0x80000000);
}

void rgb2cub (long dacol, long *mx, long *my, long *dabri)
{
	long i, r, g, b, ox, oy;

	r = ((dacol>>16)&255);
	g = ((dacol>> 8)&255);
	b = ((dacol    )&255);
	if ((g >= r) && (g >= b))
	{
		if (g <= 0) { gbri = 0; ox = 0; oy = 0; }
		else
		{
			(*dabri) = (g<<16); i = 8388608 / g;
			oy = 16777216 - (r+b)*i; ox = (r-b)*i;
		}
	}
	else if (b >= r)
	{
		(*dabri) = (b<<16); i = 8388608 / b;
		ox = r*i - 8388608; oy = (g+g-r)*i - 8388608;
	}
	else
	{
		(*dabri) = (r<<16); i = 8388608 / r;
		ox = 8388608 - b*i; oy = (g+g-b)*i - 8388608;
	}
	(*mx) = ox/spik + xres-CGRAD;
	(*my) = oy/cpik + yres-CGRAD-4;
}

void drawcolorbar ()
{
	long i, j, x, y, xx, yy, z, r, g, b, l;

	l = mulshr24(spik,gbri); x = clx[0]; z = 1; yy = -CGRAD*cpik;
	y = (yres-CGRAD*2-4)*bytesperline+((xres-CGRAD)<<2)+frameplace;
	do
	{
		if (clx[z] < x) { x = clx[z++]; y += bytesperline; yy += cpik; }

		xx = x*spik;
		if (yy > labs(xx))
		{
			r = mulshr24(xx-yy,gbri) + gbri;
			b = mulshr24(-xx-yy,gbri) + gbri;
			g = gbri;
			rgbcolselinitinc(l,0,-l); //i = l; j = 0; k = -l;
		}
		else if (xx < 0)
		{
			r = mulshr24(xx+xx,gbri) + gbri;
			g = mulshr24(xx+yy,gbri) + gbri;
			b = gbri;
			rgbcolselinitinc(l<<1,l,0); //i = (l<<1); j = l; k = 0;
		}
		else
		{
			g = mulshr24(-xx+yy,gbri) + gbri;
			b = mulshr24(-xx-xx,gbri) + gbri;
			r = gbri;
			rgbcolselinitinc(0,-l,-(l<<1)); //i = 0; j = -l; k = -(l<<1);
		}
			//x = (x<<2)+y; m = (clx[z]<<2)+y;
			//do
			//{
			//   *(long *)x = (r&0xff0000)+((g>>8)&0xff00)+(b>>16);
			//   x += 4; r += i; g += j; b += k;
			//} while (x < m);
		rgbcolselrend(r,g,b,(x<<2)+y,(clx[z]<<2)+y);

		x = clx[z++];
	} while (z < clcnt);
	clearMMX();
}

static char relpathbase[MAX_PATH];
static void relpathinit (char *st)
{
	long i;

	for(i=0;st[i];i++) relpathbase[i] = st[i];
	if ((i) && (relpathbase[i-1] != '/') && (relpathbase[i-1] != '\\'))
		relpathbase[i++] = '\\';
	relpathbase[i] = 0;
}

	//Makes path relative to voxed directory (for sxl).
	//(relpathbase is "C:\kwin\voxlap\" on my machine.)
	//Examples:
	//   "C:\KWIN\VOXLAP\KV6\ANVIL.KV6" -> "KV6\ANVIL.KV6"
	//   "KV6\ANVIL.KV6" -> "KV6\ANVIL.KV6"
static char *relpath (char *st)
{
	long i;
	char ch0, ch1;

	for(i=0;relpathbase[i];i++)
	{
		ch0 = st[i];
		if ((ch0 >= 'a') && (ch0 <= 'z')) ch0 -= 32;
		if (ch0 == '/') ch0 = '\\';

		ch1 = relpathbase[i];
		if ((ch1 >= 'a') && (ch1 <= 'z')) ch1 -= 32;
		if (ch1 == '/') ch1 = '\\';

		if (ch0 != ch1) return(st);
	}
	return(&st[i]);
}

//----------------------  WIN file select code begins ------------------------

#ifdef _WIN32
#include <commdlg.h>

static char fileselectnam[MAX_PATH+1];
static long fileselect1stcall = -1; //Stupid directory hack
static char *loadfileselect (char *mess, char *spec, char *defext)
{
	long i;
	for(i=0;fileselectnam[i];i++) if (fileselectnam[i] == '/') fileselectnam[i] = '\\';
	do {
		OPENFILENAME ofn =
		{
			sizeof(OPENFILENAME),ghwnd,0,spec,0,0,1,fileselectnam,MAX_PATH,0,0,(char *)(((long)relpathbase)&fileselect1stcall),mess,
			/*OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST|*/ OFN_HIDEREADONLY,0,0,defext,0,0,0
		};
		fileselect1stcall = 0; //Let windows remember directory after 1st call
		if (!GetOpenFileName(&ofn)) return(0); else return(fileselectnam);
	} while (0);
}
static char *savefileselect (char *mess, char *spec, char *defext)
{
	long i;
	for(i=0;fileselectnam[i];i++) if (fileselectnam[i] == '/') fileselectnam[i] = '\\';
	do {
		OPENFILENAME ofn =
		{
			sizeof(OPENFILENAME),ghwnd,0,spec,0,0,1,fileselectnam,MAX_PATH,0,0,(char *)(((long)relpathbase)&fileselect1stcall),mess,
			OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST|OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT,0,0,defext,0,0,0
		};
		fileselect1stcall = 0; //Let windows remember directory after 1st call
		if (!GetSaveFileName(&ofn)) return(0); else return(fileselectnam);
	} while (0);
}
#endif

//------------------------- file select code begins --------------------------

#define MAXMENUFILES 1024
#define MAXMENUFILEBYTES 16384
static char *menuptr[MAXMENUFILES], *curmenuptr;
static char menubuf[MAXMENUFILEBYTES], menufiletype[MAXMENUFILES>>3];
static char curpath[MAX_PATH+1] = "", menupath[MAX_PATH+1];
static long menunamecnt = 0, menuhighlight = 0;

#ifndef _WIN32

static void getfilenames (char *kind)
{
	long i, type;
	struct find_t fileinfo;

	if ((kind[0] == '.') && (!kind[1]))
		  { if (_dos_findfirst("*.*",_A_SUBDIR,&fileinfo)) return; type = 1; }
	else { if (_dos_findfirst(kind,_A_NORMAL,&fileinfo)) return; type = 0; }
	do
	{
		if ((type) && (!(fileinfo.attrib&16))) continue;
		if ((fileinfo.name[0] == '.') && (!fileinfo.name[1])) continue;
		if (menunamecnt >= MAXMENUFILES) continue;
		i = strlen(fileinfo.name);
		if (&curmenuptr[i+1] >= &menubuf[MAXMENUFILEBYTES]) continue;
		strcpy(curmenuptr,fileinfo.name);
		menuptr[menunamecnt] = curmenuptr;
		if (type) menufiletype[menunamecnt>>3] |= (1<<(menunamecnt&7));
			  else menufiletype[menunamecnt>>3] &= ~(1<<(menunamecnt&7));
		curmenuptr = &curmenuptr[i+1]; menunamecnt++;
	} while (!_dos_findnext(&fileinfo));
}

#else

static void getfilenames (char *kind)
{
	long i, type;
	HANDLE hfind;
	WIN32_FIND_DATA findata;

	if ((kind[0] == '.') && (!kind[1]))
		  { hfind = FindFirstFile("*.*",&findata); type = 1; }
	else { hfind = FindFirstFile(kind,&findata); type = 0; }
	if (hfind == INVALID_HANDLE_VALUE) return;
	do
	{
		if (findata.dwFileAttributes&FILE_ATTRIBUTE_HIDDEN) continue;
		if ((type) && (!(findata.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))) continue;
		if ((findata.cFileName[0] == '.') && (!findata.cFileName[1])) continue;
		if (menunamecnt >= MAXMENUFILES) continue;
		i = strlen(findata.cFileName);
		if (&curmenuptr[i+1] >= &menubuf[MAXMENUFILEBYTES]) continue;
		strcpy(curmenuptr,findata.cFileName);
		menuptr[menunamecnt] = curmenuptr;
		if (type) menufiletype[menunamecnt>>3] |= (1<<(menunamecnt&7));
			  else menufiletype[menunamecnt>>3] &= ~(1<<(menunamecnt&7));
		curmenuptr = &curmenuptr[i+1]; menunamecnt++;
	} while (FindNextFile(hfind,&findata));
	FindClose(hfind);
}

#endif

void sortfilenames ()
{
	long i, j, k;
	char *ptr, ch0, ch1;

	for(i=1;i<menunamecnt;i++)
		for(j=0;j<i;j++)
		{
			for(k=0;1;k++)
			{
				ch0 = menuptr[i][k]; if ((ch0 >= 'a') && (ch0 <= 'z')) ch0 -= 32;
				ch1 = menuptr[j][k]; if ((ch1 >= 'a') && (ch1 <= 'z')) ch1 -= 32;
				if ((ch0 != ch1) || (!ch0) || (!ch1)) break;
			}
			if (ch0 < ch1)
			{
				ptr = menuptr[i]; menuptr[i] = menuptr[j]; menuptr[j] = ptr;
					//This ugly mess swaps menufiletype bits i&j
				if (((menufiletype[i>>3]&(1<<(i&7))) != 0) !=
					 ((menufiletype[j>>3]&(1<<(j&7))) != 0))
				{
					menufiletype[i>>3] ^= (1<<(i&7));
					menufiletype[j>>3] ^= (1<<(j&7));
				}
			}
		}
}

static char *fileselect (char *filespec)
{
	long newhighlight, i, j, topplc, menuheight = (yres-5)/6, b, x, y;
	char ch, buffer[MAX_PATH+5+1];

	if (!curpath[0])
	{
#ifndef _WIN32
		getcwd(curpath,sizeof(curpath));
#else
		GetCurrentDirectory(sizeof(curpath),curpath);
#endif
	}

#ifndef _WIN32
	chdir(menupath);
#else
	SetCurrentDirectory(menupath);
	while (keyread()); //Flush buffer
#endif
	menunamecnt = 0; curmenuptr = menubuf;
	getfilenames(".");

	i = 0;
	do
	{
		for(j=i;(filespec[j]) && (filespec[j] != ';');j++);
		ch = filespec[j]; filespec[j] = 0;
		getfilenames(&filespec[i]);
		filespec[j] = ch; i = j+1;
	} while (ch);

	sortfilenames();
	if (!menunamecnt) return(0);

	newhighlight = menuhighlight = MIN(MAX(menuhighlight,0),menunamecnt-1);
	while (1)
	{
		startdirectdraw(&frameplace,&b,&x,&y);
		voxsetframebuffer(frameplace,b,x,y);
		for(y=0,i=frameplace;y<yres;y++,i+=bytesperline) clearbuf((void *)i,xres,0);

		topplc = MAX(MIN(newhighlight-(menuheight>>1),menunamecnt-menuheight-1),0);
		for(i=0;i<menunamecnt;i++)
		{
			if (i == newhighlight)
				{ buffer[0] = buffer[1] = '-'; buffer[2] = '>'; buffer[3] = 32; }
			else
				{ buffer[0] = buffer[1] = buffer[2] = buffer[3] = 32; }

			if ((menufiletype[i>>3]&(1<<(i&7))) && ((menuptr[i][0] != '.') || (menuptr[i][1] != '.')))
				{ buffer[4] = 92; strcpy(&buffer[5],menuptr[i]); }
			else
				strcpy(&buffer[4],menuptr[i]);

			if ((unsigned)(i-topplc) <= (unsigned)menuheight)
				print4x6(0,(i-topplc)*6,0xffffff,-1,"%s",buffer);
		}

			//Nextpage
		stopdirectdraw();
		nextpage();

#ifndef _WIN32
		ch = 0;  //Interesting fakery of ch = getch()
		do
		{
			breath();
			readkeyboard();

			if (keystatus[0xcb]) { keystatus[0xcb] = 0; ch = 75; }
			if (keystatus[0xcd]) { keystatus[0xcd] = 0; ch = 77; }
			if (keystatus[0xc8]) { keystatus[0xc8] = 0; ch = 72; }
			if (keystatus[0xd0]) { keystatus[0xd0] = 0; ch = 80; }
			if (keystatus[0xc9]) { keystatus[0xc9] = 0; ch = 73; }  //PGUP
			if (keystatus[0xd1]) { keystatus[0xd1] = 0; ch = 81; }  //PGDN
			if (keystatus[0xc7]) { keystatus[0xc7] = 0; ch = 71; }  //Home
			if (keystatus[0xcf]) { keystatus[0xcf] = 0; ch = 79; }  //End
			if (keystatus[0x1c]) { keystatus[0x1c] = 0; ch = 13; }
			if (keystatus[1]) { keystatus[1] = 0; return(0); }
		} while (!ch);
#else
		while (!(i = keyread()))
		{
			extern void breath();
			breath(); readkeyboard();
		}
		switch((i>>8)&255)
		{
			case 0xcb: ch = 75; break;
			case 0xcd: ch = 77; break;
			case 0xc8: ch = 72; break;
			case 0xd0: ch = 80; break;
			case 0xc9: ch = 73; break; //PGUP
			case 0xd1: ch = 81; break; //PGDN
			case 0xc7: ch = 71; break; //Home
			case 0xcf: ch = 79; break; //End
			case 0x1c: ch = 13; break;
			case 0x1: keystatus[1] = 0; return(0);
		}
#endif

		if ((ch == 75) || (ch == 72)) newhighlight = MAX(newhighlight-1,0);
		if ((ch == 77) || (ch == 80)) newhighlight = MIN(newhighlight+1,menunamecnt-1);
		if (ch == 73) newhighlight = MAX(newhighlight-menuheight,0);
		if (ch == 81) newhighlight = MIN(newhighlight+menuheight,menunamecnt-1);
		if (ch == 71) newhighlight = 0;
		if (ch == 79) newhighlight = menunamecnt-1;
		if (ch == 13)
		{
			if (!(menufiletype[newhighlight>>3]&(1<<(newhighlight&7))))
			{
				menuhighlight = newhighlight;
				return(menuptr[newhighlight]);
			}
			if ((menuptr[newhighlight][0] == '.') && (menuptr[newhighlight][1] == '.'))
			{
				i = 0;
				while ((i < MAX_PATH-1) && (menupath[i])) i++;
				while ((i > 0) && (menupath[i] != 92)) i--;
				menupath[i] = 0;
			}
			else
			{
				strcat(menupath,"\\");
				strcat(menupath,menuptr[newhighlight]);
			}
#ifndef _WIN32
			chdir(menuptr[newhighlight]);
#else
			SetCurrentDirectory(menuptr[newhighlight]);
#endif
			menunamecnt = 0; curmenuptr = menubuf;
			getfilenames(".");

			i = 0;
			do
			{
				for(j=i;(filespec[j]) && (filespec[j] != ';');j++);
				ch = filespec[j]; filespec[j] = 0;
				getfilenames(&filespec[i]);
				filespec[j] = ch; i = j+1;
			} while (ch);

			sortfilenames();
			newhighlight = 0;
		}
	}
	return(0);
}

//-------------------------- file select code ends ---------------------------

void voxfindsuck (long cx, long cy, long cz, long *ox, long *oy, long *oz)
{
	long i, j, k, d, x, y, z, xx, yy, zz;

	j = 0x7fffffff; (*ox) = cx; (*oy) = cy; (*oz) = cz;
	for(x=-4;x<=4;x++)
		for(y=-4;y<=4;y++)
			for(z=-4;z<=4;z++)
			{
				k = x*x+y*y+z*z; if (k > 4*4) continue;
				if (!isvoxelsolid(cx+x,cy+y,cz+z)) continue;

					//Find number of solid cubes in local sphere
				for(xx=-4;xx<=4;xx++)
					for(yy=-4;yy<=4;yy++)
						for(zz=-4;zz<=4;zz++)
						{
							if (xx*xx+yy*yy+zz*zz > 4*4) continue;
							if (isvoxelsolid(cx+x+xx,cy+y+yy,cz+z+zz)) k += 256;
						}
				if (k < j) { j = k; (*ox) = cx+x; (*oy) = cy+y; (*oz) = cz+z; }
			}
}

void voxfindspray (long cx, long cy, long cz, long *ox, long *oy, long *oz)
{
	long i, j, k, d, x, y, z, xx, yy, zz;

	j = 0x7fffffff; (*ox) = cx; (*oy) = cy; (*oz) = cz;
	for(x=-4;x<=4;x++)
		for(y=-4;y<=4;y++)
			for(z=-4;z<=4;z++)
			{
				k = x*x+y*y+z*z; if (k > 4*4) continue;
				if (isvoxelsolid(cx+x,cy+y,cz+z)) continue;

					//Find number of solid cubes in local sphere
				for(xx=-4;xx<=4;xx++)
					for(yy=-4;yy<=4;yy++)
						for(zz=-4;zz<=4;zz++)
						{
							if (xx*xx+yy*yy+zz*zz > 4*4) continue;
							if (!isvoxelsolid(cx+x+xx,cy+y+yy,cz+z+zz)) k += 256;
						}
				if (k < j) { j = k; (*ox) = cx+x; (*oy) = cy+y; (*oz) = cz+z; }
			}
}

void blursphere (lpoint3d *hit, long rad, long bakcol)
{
	long i, j, k, l, m, x, y, z, xx, yy, zz, r, g, b;

	for(xx=hit->x-rad+1;xx<hit->x+rad;xx++)
	{
		if ((unsigned long)xx >= VSID) continue;
		for(yy=hit->y-rad+1;yy<hit->y+rad;yy++)
		{
			m = (xx-hit->x)*(xx-hit->x)+(yy-hit->y)*(yy-hit->y)-rad*rad;
			if (m >= 0) continue;
			if ((unsigned long)yy >= VSID) continue;
			for(zz=1;zz<rad;zz++) if (zz*zz+m >= 0) break;
			m = zz-1;
			for(zz=hit->z-m;zz<=hit->z+m;zz++)
			{
				j = getcube(xx,yy,zz); if ((unsigned long)j < 2) continue;
				r = g = b = i = 0;
				for(x=-1;x<=1;x++)
				{
					if ((unsigned long)(xx+x) >= VSID) continue;
					for(y=-1;y<=1;y++)
					{
						if ((unsigned long)(yy+y) >= VSID) continue;
						for(z=-1;z<=1;z++)
						{
							k = getcube(xx+x,yy+y,zz+z);
							if ((unsigned long)k >= 2)
							{
								l = cubedistweight[labs(x)+labs(y)+labs(z)];
								if (bakcol)
								{
									r += (*(char *)(k+2))*l;
									g += (*(char *)(k+1))*l;
									b += (*(char *)(k  ))*l;
								}
								else r += (*(char *)(k+3))*l;
								i += l;
							}
						}
					}
				}
				if (bakcol)
				{
					(*(char *)(j+2)) = (r+(i>>1))/i;
					(*(char *)(j+1)) = (g+(i>>1))/i;
					(*(char *)(j  )) = (b+(i>>1))/i;
				}
				else (*(char *)(j+3)) = (r+(i>>1))/i;
			}
		}
	}
}

void suckthinvoxsphere (lpoint3d *hit, long rad)
{
	long j, m, xx, yy, zz, b;

	for(xx=hit->x-rad+1;xx<hit->x+rad;xx++)
	{
		if ((unsigned long)xx >= VSID) continue;
		for(yy=hit->y-rad+1;yy<hit->y+rad;yy++)
		{
			m = (xx-hit->x)*(xx-hit->x)+(yy-hit->y)*(yy-hit->y)-rad*rad;
			if (m >= 0) continue;
			if ((unsigned long)yy >= VSID) continue;
			for(zz=1;zz<rad;zz++) if (zz*zz+m >= 0) break;
			m = zz-1;
			for(zz=hit->z-m;zz<=hit->z+m;zz++)
			{
				j = getcube(xx,yy,zz); if ((unsigned long)j < 2) continue;

				b = 0;
				if ((unsigned long)(xx-1) < VSID-2)
					if ((!isvoxelsolid(xx-1,yy,zz)) && (!isvoxelsolid(xx+1,yy,zz))) b = 1;
				if ((unsigned long)(yy-1) < VSID-2)
					if ((!isvoxelsolid(xx,yy-1,zz)) && (!isvoxelsolid(xx,yy+1,zz))) b = 1;
				if ((unsigned long)(zz-1) < MAXZDIM-2)
					if ((!isvoxelsolid(xx,yy,zz-1)) && (!isvoxelsolid(xx,yy,zz+1))) b = 1;
				if (b) setcube(xx,yy,zz,-1);
			}
		}
	}
}

void photonflash (dpoint3d *ipos, long brightness, long numrays)
{
	dpoint3d dp;
	lpoint3d hit;
	float f;
	long i, j, k, n, x, y, z;

	flashbrival = brightness;
	for(j=numrays;j>0;j-=256)
	{
		n = MIN(j,256);
		for(i=0;i<n;i++)
		{
				//UNIFORM spherical randomization (see spherand.c)
			dp.z = ((double)(rand()&32767))/16383.5-1.0;
			dcossin((((double)(rand()&32767))/16383.5-1.0)*PI,&dp.x,&dp.y);
			f = sqrt(1.0 - dp.z*dp.z); dp.x *= f; dp.y *= f;

			hitscan(ipos,&dp,&hit,&hind,&hdir);
			radar[i*4  ] = (long)hind;
			radar[i*4+1] = hit.x;
			radar[i*4+2] = hit.y;
			radar[i*4+3] = hit.z;
		}
		for(i=0;i<n;i++)
		{
			if (!radar[i*4]) continue; //mmxcoloradd(radar[i*4]);
			hit.x = radar[i*4+1]; hit.y = radar[i*4+2]; hit.z = radar[i*4+3];
			for(x=-1;x<=1;x++)
			{
				if ((unsigned long)(hit.x+x) >= VSID) continue;
				for(y=-1;y<=1;y++)
				{
					if ((unsigned long)(hit.y+y) >= VSID) continue;
					for(z=-1;z<=1;z++)
					{
						k = getcube(hit.x+x,hit.y+y,hit.z+z);
						if ((unsigned long)k >= 2)
							{ if (!keystatus[0x38]) mmxcoloradd((long *)k); else mmxcolorsub((long *)k); }
					}
				}
			}
		}
		clearMMX();
	}
}

void aligncyltexture (float ox, float oy, float oz, float dx, float dy, float dz)
{
	float f, t;

	dx -= ox; dy -= oy; dz -= oz;

	vx5.picmode = 1;
	t = dx*dx+dy*dy+dz*dz;
	if (t > 0)
	{
		if (hind)
		{
			f = (((float)hit.x-ox)*dx+((float)hit.y-oy)*dy+((float)hit.z-oz)*dz) / t;
			ox += dx*f; oy += dy*f; oz += dz*f;
		}
		vx5.fpico.x = ox;
		vx5.fpico.y = oy;
		vx5.fpico.z = oz;

		t = 1.0/sqrt(t); dx *= t; dy *= t; dz *= t;
		vx5.fpicw.x = dx;
		vx5.fpicw.y = dy;
		vx5.fpicw.z = dz;
	} else vx5.picmode = 0;

	if (hind)
	{
		ox = (float)hit.x-vx5.fpico.x;
		oy = (float)hit.y-vx5.fpico.y;
		oz = (float)hit.z-vx5.fpico.z;
	}
	else
	{
		ox = ipos.x-vx5.fpico.x;
		oy = ipos.y-vx5.fpico.y;
		oz = ipos.z-vx5.fpico.z;
	}
	vx5.fpicv.x = oy*dz - oz*dy;
	vx5.fpicv.y = oz*dx - ox*dz;
	vx5.fpicv.z = ox*dy - oy*dx;
	f = vx5.fpicv.x*vx5.fpicv.x+vx5.fpicv.y*vx5.fpicv.y+vx5.fpicv.z*vx5.fpicv.z;
	if (f > 0) { f = 1.0/sqrt((float)f); vx5.fpicv.x *= f; vx5.fpicv.y *= f; vx5.fpicv.z *= f; } else vx5.picmode = 0;

	vx5.fpicu.x = vx5.fpicw.y*vx5.fpicv.z - vx5.fpicw.z*vx5.fpicv.y;
	vx5.fpicu.y = vx5.fpicw.z*vx5.fpicv.x - vx5.fpicw.x*vx5.fpicv.z;
	vx5.fpicu.z = vx5.fpicw.x*vx5.fpicv.y - vx5.fpicw.y*vx5.fpicv.x;

	if (vx5.xsiz > 0)
	{
		ftol((float)vx5.currad*PI*2,&vx5.xoru);
		vx5.xoru = MAX(vx5.xoru/vx5.xsiz,1)*vx5.xsiz;
	}
}

void voxredraw ()
{
	lpoint3d p, p2;
	long i, j, k, l, m, x, y, z, xx, yy, zz;
	float f;
	char *v;

	vx5.colfunc = (long (*)(lpoint3d *))colfunclst[colfnum];
	switch (backtag)
	{
		case SETSPR: //This doesn't actually modify the board map, but...
			if ((unsigned long)(curspri-1) < numsprites-1)
			{
				spr[curspri].p.x = thit[0].x;
				spr[curspri].p.y = thit[0].y;
				spr[curspri].p.z = thit[0].z;
				backedup = 0;
			}
			break;
		case SETFIL:
			if (numcurs == 2)
			{
				ftol(curs[0].x-.5,&x); ftol(curs[1].x-.5,&xx);
				ftol(curs[0].y-.5,&y); ftol(curs[1].y-.5,&yy);
				ftol(curs[0].z-.5,&z); ftol(curs[1].z-.5,&zz);
				if (x > xx) { i = x; x = xx; xx = i; }
				if (y > yy) { i = y; y = yy; yy = i; }
				if (z > zz) { i = z; z = zz; zz = i; }
				voxbackup(x,y,xx,yy,SETFIL);

				i = thit[0].x; if (i < x) i = x;
				j = thit[0].y; if (j < y) j = y;
				k = thit[0].z; if (k < z) k = z;
				if (i > xx) i = xx;
				if (j > yy) j = yy;
				if (k > zz) k = zz;
				setfloodfill3d(i,j,k,x,y,z,xx,yy,zz);
			}
			else
			{
				voxdontrestore();
				setfloodfill3d(thit[0].x,thit[0].y,thit[0].z,0,0,0,VSID-1,VSID-1,MAXZDIM-1);
			}
			break;

		case SETREC:         //Ins/Del (Draw Solid, numcurs == 2)
			voxbackup(MIN(thit[0].x,thit[1].x),MIN(thit[0].y,thit[1].y),MAX(thit[0].x,thit[1].x)+1,MAX(thit[0].y,thit[1].y)+1,SETREC);
			setrect(&thit[0],&thit[1],vx5.curcol|tsolid);
			break;
		case SETTRI:
			if (numcurs >= 3) settri(&curs[0],&curs[1],&curs[2],SETTRI);
			break;
		case SETSPH:         //Home/End (Insert/Delete sphere)
			voxbackup(thit[0].x-vx5.currad+1,thit[0].y-vx5.currad+1,thit[0].x+vx5.currad,thit[0].y+vx5.currad,SETSPH);
			if ((vx5.colfunc == pngcolfunc) && (vx5.picmode == 2))
			{
				vx5.fpico.x = (float)thit[0].x;
				vx5.fpico.y = (float)thit[0].y;
				vx5.fpico.z = (float)thit[0].z;
			}
			setsphere(&thit[0],vx5.currad,vx5.curcol|tsolid);
			break;
		case SETELL:         //E (Insert/Delete ellipsoid)
			aligncyltexture(thit[0].x,thit[0].y,thit[0].z,thit[1].x,thit[1].y,thit[1].z);
			setellipsoid(&thit[0],&thit[1],vx5.currad,vx5.curcol|tsolid,SETELL);
			break;
		case SETCYL:         //L (Insert/Delete cylinder)
			aligncyltexture(thit[0].x,thit[0].y,thit[0].z,thit[1].x,thit[1].y,thit[1].z);
			setcylinder(&thit[0],&thit[1],vx5.currad,vx5.curcol|tsolid,SETCYL);
			break;
		case SETHUL:         //H (Insert/Delete 3D convex hull)
			j = MIN(32,MAXCURS>>1);
			for(i=j-1;i>=0;i--)
			{
				do  //Uniform distribution on solid sphere
				{
					tempoint[i].x = (((float)rand())-16383.5)/16383.5;
					tempoint[i].y = (((float)rand())-16383.5)/16383.5;
					tempoint[i].z = (((float)rand())-16383.5)/16383.5;
				} while (tempoint[i].x*tempoint[i].x + tempoint[i].y*tempoint[i].y + tempoint[i].z*tempoint[i].z > 1.0);
				tempoint[i].x = tempoint[i].x*vx5.currad + thit[0].x;
				tempoint[i].y = tempoint[i].y*vx5.currad + thit[0].y;
				tempoint[i].z = tempoint[i].z*vx5.currad + thit[0].z;
			}
			sethull3d(tempoint,j,vx5.curcol|tsolid,SETHUL);
			break;
		case SETWAL:         //W (Draw Wall)
			if (numcurs < 2) break;

			thit[0].x = thit[0].y = VSID; thit[0].z = MAXZDIM; thit[1].x = thit[1].y = thit[1].z = 0;
			for(i=numcurs-1;i>=0;i--)
			{
				ftol(curs[i].x-.5,&x); thit[0].x = MIN(thit[0].x,x); thit[1].x = MAX(thit[1].x,x);
				ftol(curs[i].y-.5,&y); thit[0].y = MIN(thit[0].y,y); thit[1].y = MAX(thit[1].y,y);
				ftol(curs[i].z-.5,&z); thit[0].z = MIN(thit[0].z,z); thit[1].z = MAX(thit[1].z,z);
				ftol(curs[i].x+cursnoff.x-.5,&x); thit[0].x = MIN(thit[0].x,x); thit[1].x = MAX(thit[1].x,x);
				ftol(curs[i].y+cursnoff.y-.5,&y); thit[0].y = MIN(thit[0].y,y); thit[1].y = MAX(thit[1].y,y);
				ftol(curs[i].z+cursnoff.z-.5,&z); thit[0].z = MIN(thit[0].z,z); thit[1].z = MAX(thit[1].z,z);
			}
			voxbackup(MIN(thit[0].x,thit[1].x)-vx5.currad,MIN(thit[0].y,thit[1].y)-vx5.currad,MAX(thit[0].x,thit[1].x)+vx5.currad,MAX(thit[0].y,thit[1].y)+vx5.currad,SETWAL);

			if (numcurs == 2)
			{
				tempoint[0].z = tempoint[3].z = curs[0].z;
				tempoint[1].z = tempoint[2].z = curs[1].z;
				for(i=0;i<4;i++)
				{
					switch(i)
					{
						case 0:
							tempoint[0].x = curs[0].x; tempoint[0].y = curs[0].y;
							tempoint[2].x = curs[1].x; tempoint[2].y = curs[0].y;
							break;
						case 1:
							tempoint[0].x = curs[1].x; tempoint[0].y = curs[0].y;
							tempoint[2].x = curs[1].x; tempoint[2].y = curs[1].y;
							break;
						case 2:
							tempoint[0].x = curs[1].x; tempoint[0].y = curs[1].y;
							tempoint[2].x = curs[0].x; tempoint[2].y = curs[1].y;
							break;
						case 3:
							tempoint[0].x = curs[0].x; tempoint[0].y = curs[1].y;
							tempoint[2].x = curs[0].x; tempoint[2].y = curs[0].y;
							break;
					}

					tempoint[1].x = tempoint[0].x; tempoint[1].y = tempoint[0].y;
					tempoint[3].x = tempoint[2].x; tempoint[3].y = tempoint[2].y;
					point2[0] = 1; point2[1] = 2; point2[2] = 3; point2[3] = 0;
					setsector(tempoint,point2,4,2.0,vx5.curcol|tsolid,0);
				}
			}
			else
			{
				for(i=numcurs-1;i>=0;i--)
				{
					ftol(curs[i].x-.5,&p.x); ftol(curs[i].x+cursnoff.x-.5,&p2.x);
					ftol(curs[i].y-.5,&p.y); ftol(curs[i].y+cursnoff.y-.5,&p2.y);
					ftol(curs[i].z-.5,&p.z); ftol(curs[i].z+cursnoff.z-.5,&p2.z);
					setcylinder(&p,&p2,2,vx5.curcol|tsolid,0);
				}

				for(i=numcurs-1,j=0;i>=0;j=i,i--)
				{
					tempoint[0] = curs[i];
					tempoint[1] = curs[j];
					tempoint[2].x = curs[j].x+cursnoff.x;
					tempoint[2].y = curs[j].y+cursnoff.y;
					tempoint[2].z = curs[j].z+cursnoff.z;
					tempoint[3].x = curs[i].x+cursnoff.x;
					tempoint[3].y = curs[i].y+cursnoff.y;
					tempoint[3].z = curs[i].z+cursnoff.z;
					point2[0] = 1; point2[1] = 2; point2[2] = 3; point2[3] = 0;
					setsector(tempoint,point2,4,2.0,vx5.curcol|tsolid,0);
				}
			}
			vx5.minx = MIN(thit[0].x,thit[1].x)-vx5.currad;
			vx5.miny = MIN(thit[0].y,thit[1].y)-vx5.currad;
			vx5.minz = MIN(thit[0].z,thit[1].z)-vx5.currad;
			vx5.maxx = MAX(thit[0].x,thit[1].x)+vx5.currad;
			vx5.maxy = MAX(thit[0].y,thit[1].y)+vx5.currad;
			vx5.maxz = MAX(thit[0].z,thit[1].z)+vx5.currad;
			break;

		case SETCEI:        //C (Draw Ceiling)
			if (numcurs < 3) break;

			//thit[0].x = thit[0].y = VSID; thit[0].z = MAXZDIM; thit[1].x = thit[1].y = thit[1].z = 0;
			//for(i=numcurs-1;i>=0;i--)
			//{
			//   ftol(curs[i].x+cursnoff.x-.5,&x); thit[0].x = MIN(thit[0].x,x); thit[1].x = MAX(thit[1].x,x);
			//   ftol(curs[i].y+cursnoff.y-.5,&y); thit[0].y = MIN(thit[0].y,y); thit[1].y = MAX(thit[1].y,y);
			//   ftol(curs[i].z+cursnoff.z-.5,&z); thit[0].z = MIN(thit[0].z,z); thit[1].z = MAX(thit[1].z,z);
			//}
			//voxbackup(MIN(thit[0].x,thit[1].x)-vx5.currad,MIN(thit[0].y,thit[1].y)-vx5.currad,MAX(thit[0].x,thit[1].x)+vx5.currad,MAX(thit[0].y,thit[1].y)+vx5.currad,SETCEI);

			for(i=numcurs-1;i>=0;i--)
			{
				tempoint[i].x = curs[i].x+cursnoff.x;
				tempoint[i].y = curs[i].y+cursnoff.y;
				tempoint[i].z = curs[i].z+cursnoff.z;
				point2[i] = i+1;
			}
			point2[numcurs-1] = 0;
			setsector(tempoint,point2,numcurs,2.0,vx5.curcol|tsolid,SETCEI);
			break;

		case SETSEC:        //Ins/Del (Draw Solid, numcurs > 2)
			if (numcurs < 3) break;  //How should I handle this?

			//thit[0].x = thit[0].y = VSID; thit[0].z = MAXZDIM; thit[1].x = thit[1].y = thit[1].z = 0;
			//for(i=numcurs-1;i>=0;i--)
			//{
			//   ftol(curs[i].x-.5,&x); thit[0].x = MIN(thit[0].x,x); thit[1].x = MAX(thit[1].x,x);
			//   ftol(curs[i].y-.5,&y); thit[0].y = MIN(thit[0].y,y); thit[1].y = MAX(thit[1].y,y);
			//   ftol(curs[i].z-.5,&z); thit[0].z = MIN(thit[0].z,z); thit[1].z = MAX(thit[1].z,z);
			//   ftol(curs[i].x+cursnoff.x-.5,&x); thit[0].x = MIN(thit[0].x,x); thit[1].x = MAX(thit[1].x,x);
			//   ftol(curs[i].y+cursnoff.y-.5,&y); thit[0].y = MIN(thit[0].y,y); thit[1].y = MAX(thit[1].y,y);
			//   ftol(curs[i].z+cursnoff.z-.5,&z); thit[0].z = MIN(thit[0].z,z); thit[1].z = MAX(thit[1].z,z);
			//}
			//voxbackup(MIN(thit[0].x,thit[1].x),MIN(thit[0].y,thit[1].y),MAX(thit[0].x,thit[1].x),MAX(thit[0].y,thit[1].y),SETSEC);

			for(i=numcurs-1;i>=0;i--)
			{
				tempoint[i].x = curs[i].x+cursnoff.x*.5;
				tempoint[i].y = curs[i].y+cursnoff.y*.5;
				tempoint[i].z = curs[i].z+cursnoff.z*.5;
				point2[i] = i+1;
			}
			point2[numcurs-1] = 0;
			if (!tsolid)
				setsector(tempoint,point2,numcurs,fabs((float)vx5.curhei)*.5,vx5.curcol,SETSEC);
			else
				setsector(tempoint,point2,numcurs,fabs((float)vx5.curhei)*.5+.5,-1,SETSEC);
			break;

		case SETLAT:         //T (Turn - lathe tool)
			aligncyltexture(curs[0].x,curs[0].y,curs[0].z,curs[1].x,curs[1].y,curs[1].z);
			setlathe(curs,numcurs,vx5.curcol|tsolid,SETLAT);
			break;

		case SETBLO:         //B (Blobs)
			setblobs(curs,numcurs,vx5.curcol|tsolid,SETBLO);
			break;

		case SETKVX:         //U (Insert .KVX)
			setkvx(tfilenam,thit[0].x,thit[0].y,thit[0].z,trot,SETKVX);
			break;

		case SETDUP:         //D (Duplicate box)
		case SETDUQ:         //Shift+D (Duplicate box, don't copy air)
			if (numcurs < 2) break;

			ftol(curs[0].x-.5,&x); ftol(curs[1].x-.5,&xx);
			ftol(curs[0].y-.5,&y); ftol(curs[1].y-.5,&yy);
			ftol(curs[0].z-.5,&z); ftol(curs[1].z-.5,&zz);
			thit[1].x = thit[0].x-x; if (x > xx) { i = x; x = xx; xx = i; }
			thit[1].y = thit[0].y-y; if (y > yy) { i = y; y = yy; yy = i; }
			thit[1].z = thit[0].z-z; if (z > zz) { i = z; z = zz; zz = i; }

			vx5.minx = x+thit[1].x; vx5.maxx = xx+thit[1].x+1;
			vx5.miny = y+thit[1].y; vx5.maxy = yy+thit[1].y+1;
			vx5.minz = z+thit[1].z; vx5.maxz = zz+thit[1].z+1;
			voxbackup(x+thit[1].x,y+thit[1].y,xx+thit[1].x+1,yy+thit[1].y+1,backtag);

			clearbuf((void *)templongbuf,MAXZDIM,-3);

			if (thit[1].x < 0) { i = xx+1; xx = x; } else { i = x-1; }
			if (thit[1].y < 0) { j = yy+1; yy = y; } else { j = y-1; }
			if (thit[1].z < 0) { k = zz+1; zz = z; } else { k = z-1; }

			if (backtag == SETDUP) m = -1; else m = -3;

			for(x=xx;x!=i;x+=(((i-xx)>>31)|1))
				for(y=yy;y!=j;y+=(((j-yy)>>31)|1))
				{
					for(z=zz;z!=k;z+=(((k-zz)>>31)|1))
						if (!((z+thit[1].z)&0xffffff00))
						{
							l = getcube(x,y,z);
							if (l == 0) templongbuf[z+thit[1].z] = m;
							else if (l == 1) templongbuf[z+thit[1].z] = vx5.curcol;
							else templongbuf[z+thit[1].z] = *(long *)l;
						}
					scum(x+thit[1].x,y+thit[1].y,0,MAXZDIM,
					/* MAX(MIN(thit[0].z,thit[1].z),0),
						MIN(MAX(thit[0].z,thit[1].z)+1,MAXZDIM), */
						templongbuf); scumfinish();
				}
			break;
		case SETCOL:        //= (re-draw colfunc)
			if (numcurs != 2) break;
			ftol(curs[0].x-.5,&x); ftol(curs[1].x-.5,&xx);
			ftol(curs[0].y-.5,&y); ftol(curs[1].y-.5,&yy);
			ftol(curs[0].z-.5,&z); ftol(curs[1].z-.5,&zz);
			if (x > xx) { i = x; x = xx; xx = i; }
			if (y > yy) { i = y; y = yy; yy = i; }
			if (z > zz) { i = z; z = zz; zz = i; }
			vx5.minx = x; vx5.maxx = xx+1;
			vx5.miny = y; vx5.maxy = yy+1;
			vx5.minz = z; vx5.maxz = zz+1;
			voxbackup(x,y,xx+1,yy+1,SETCOL);

			for(p.y=y;p.y<=yy;p.y++)
				for(p.x=x;p.x<=xx;p.x++)
				{
					if ((p.x|p.y)&~(VSID-1)) continue;
					v = sptr[p.y*VSID+p.x]; //Fast method!
					while (1)
					{
						p.z = MAX(v[1],z); l = MIN(v[2],zz);
						for(;p.z<=l;p.z++) *(long *)&v[(p.z-v[1])*4+4] = vx5.colfunc(&p);

						l = v[2]-v[1]-v[0]+2;
						if (!v[0]) break;
						v += v[0]*4;

						p.z = MAX(v[3]+l,z); l = MIN(v[3],zz+1);
						for(;p.z<l;p.z++) *(long *)&v[(p.z-v[3])*4] = vx5.colfunc(&p);
					}
					//for(p.z=z;p.z<=zz;p.z++) //Slow and simple method
					//{
					//   l = getcube(p.x,p.y,p.z);
					//   if ((unsigned long)l >= 2) *(long *)l = vx5.colfunc(&p);
					//}
				}
			break;
		case SETFFL:        //F (new normal estimation flash)
			i = 128;
			voxbackup(thit[0].x-i+1,thit[0].y-i+1,thit[0].x+i,thit[0].y+i,SETFFL);
			setnormflash(thit[0].x,thit[0].y,thit[0].z,i,(long)(vx5.curpow*2047));
			break;
		case SETFLA:        //ALT-F (Old raycasting flash)
			voxbackup(thit[0].x-253,thit[0].y-253,thit[0].x+254,thit[0].y+254,SETFLA);
			setflash(thit[0].x,thit[0].y,thit[0].z,253,1024,1);
			break;
		case SETDYN: break; //~ (Shoot dynamic light bullet)
	}
}

#if 0
static long xorgridcnt = 0, gridclock = 0;
void xorgrid ()
{
	long x, y, x0, y0, x1, y1, i;
	char *v;

	x0 = MAX(((long)ipos.x-192)&~(gridmode-1),0); x1 = MIN(((long)ipos.x+192)&~(gridmode-1),VSID);
	y0 = MAX(((long)ipos.y-192)&~(gridmode-1),0); y1 = MIN(((long)ipos.y+192)&~(gridmode-1),VSID);

	if (!(xorgridcnt&1)) gridclock = (totclk>>5);
	xorgridcnt++;
	for(x=x0;x<x1;x+=gridmode)
	{
		if (x&gridmode) y = (gridclock&7); else y = ((-gridclock)&7);
		for(y+=y0;y<y1;y+=8)
		{
			for(v=sptr[y*VSID+x];v[0];v+=v[0]*4)
				for(i=1;i<v[0];i++) (*(long *)&v[i<<2]) ^= 0x000080;
			for(i=1;i<=v[2]-v[1]+1;i++) (*(long *)&v[i<<2]) ^= 0x000080;
		}
	}

	for(y=y0;y<y1;y+=gridmode)
	{
		if (y&gridmode) x = (gridclock&7); else x = ((-gridclock)&7);
		for(x+=x0;x<x1;x+=8)
		{
			for(v=sptr[y*VSID+x];v[0];v+=v[0]*4)
				for(i=1;i<v[0];i++) (*(long *)&v[i<<2]) ^= 0x000080;
			for(i=1;i<=v[2]-v[1]+1;i++) (*(long *)&v[i<<2]) ^= 0x000080;
		}
	}
}
#endif

	//Warning: Depends on ipos and ifor (globals)
long gridalignplane (point3d *p, point3d *o, point3d *n, long gmode)
{
	float t, ox, oy, oz;
	long i;

		//Keep curs[editcurs] on normal plane defined by curs[0-2]
		//Eq#1: (curs[editcurs]-curs[i]) dot cursnorm = 0
		//Eq#2: ipos + ifor * t = curs[editcurs]
		//   ((ipos.x + ifor.x * t)-curs[i].x) * cursnorm.x +
		//   ((ipos.y + ifor.y * t)-curs[i].y) * cursnorm.y +
		//   ((ipos.z + ifor.z * t)-curs[i].z) * cursnorm.z = 0
	t = ifor.x*n->x + ifor.y*n->y + ifor.z*n->z; if (t == 0) return(0);
	t = ((o->x-ipos.x)*n->x + (o->y-ipos.y)*n->y + (o->z-ipos.z)*n->z)/t;
	ox = ifor.x*t + ipos.x; if ((ox < 0) || (ox >= VSID)) return(0);
	oy = ifor.y*t + ipos.y; if ((oy < 0) || (oy >= VSID)) return(0);
	oz = ifor.z*t + ipos.z; if ((oz < 0) || (oz >= MAXZDIM)) return(0);
	if (gmode)
	{
		if ((fabs(n->x) > fabs(n->y)) && (fabs(n->x) > fabs(n->z))) { i = 6; }
		else if (fabs(n->y) > fabs(n->z)) { i = 5; }
		else { i = 3; }

		if (i&1) ox = (float)(((long)ox+(gmode>>1))&~(gmode-1));
		if (i&2) oy = (float)(((long)oy+(gmode>>1))&~(gmode-1));
		if (i&4) oz = (float)(((long)oz+(gmode>>1))&~(gmode-1));

		if (i == 6)      { ox = ((o->y-oy)*n->y + (o->z-oz)*n->z)/n->x+o->x; }
		else if (i == 5) { oy = ((o->x-ox)*n->x + (o->z-oz)*n->z)/n->y+o->y; }
		else             { oz = ((o->x-ox)*n->x + (o->y-oy)*n->y)/n->z+o->z; }
	}
	p->x = ox; p->y = oy; p->z = oz; return(1);
}

	//Warning: Depends on ipos and ifor (globals)
void gridalignline (point3d *p, long daxmov, long gmode)
{
	float t, ox, oy, oz;

		//   Derivation of math:
		//   Problem: Finding closest point between 2 lines
		//Treat the line as a cylinder and minimize r
		//Cylinder equation:
		//   (x-cx)ý+(y-cy)ý+(z-cz)ý =
		//   ((x-cx)vx+(y-cx)vy+(z-cz)vz)ý+rý
		//
		//Given: (x,y)      -> curs[editcurs].x, curs[editcurs].y
		//       (cx,cy,cz) -> ipos.x,ipos.y,ipos.z
		//       (vx,vy,vz) -> ifor.x,ifor.y,ifor.z
		//
		//Solve: (z)        -> curs[editcurs].z
		//Minimize: (r)
		//
		//Solution:
		//   Step 1: Re-order terms to get r = sqrt(Azý + Bz + C)
		//   Step 2: Take derivative of r (can ignore sqrt)
	if (daxmov&1)
	{
		t = (1-ifor.x*ifor.x); if (t == 0) return;
		t = (((p->y-ipos.y)*ifor.y + (p->z-ipos.z)*ifor.z)*ifor.x) / t + ipos.x;
		p->x = MIN(MAX(t,0),VSID-1);
		if (gmode) p->x = (float)(((long)p->x+(gmode>>1))&~(gmode-1));
	}
	if (daxmov&2)
	{
		t = (1-ifor.y*ifor.y); if (t == 0) return;
		t = (((p->x-ipos.x)*ifor.x + (p->z-ipos.z)*ifor.z)*ifor.y) / t + ipos.y;
		p->y = MIN(MAX(t,0),VSID-1);
		if (gmode) p->y = (float)(((long)p->y+(gmode>>1))&~(gmode-1));
	}
	if (daxmov&4)
	{
		t = (1-ifor.z*ifor.z); if (t == 0) return;
		t = (((p->x-ipos.x)*ifor.x + (p->y-ipos.y)*ifor.y)*ifor.z) / t + ipos.z;
		p->z = MIN(MAX(t,0),MAXZDIM-1);
		if (gmode) p->z = (float)(((long)p->z+(gmode>>1))&~(gmode-1));
	}
}

long iscursgood (double tolerance_sq)
{
	dpoint3d dp, dp2, dp3;

	if (numcurs == 2)
	{     //Push curs[1] up if it's too close to curs[0]
		dp.x = curs[1].x-curs[0].x;
		dp.y = curs[1].y-curs[0].y;
		dp.z = curs[1].z-curs[0].z;
		if (dp.x*dp.x + dp.y*dp.y + dp.z*dp.z < tolerance_sq) return(0);
	}
	else if (numcurs >= 3)
	{     //If first 3 cursors are close to being non-singular, don't allow move
		dp.x = curs[1].x-curs[0].x; dp2.x = curs[2].x-curs[1].x;
		dp.y = curs[1].y-curs[0].y; dp2.y = curs[2].y-curs[1].y;
		dp.z = curs[1].z-curs[0].z; dp2.z = curs[2].z-curs[1].z;
		dp3.x = dp.y*dp2.z - dp.z*dp2.y;
		dp3.y = dp.z*dp2.x - dp.x*dp2.z;
		dp3.z = dp.x*dp2.y - dp.y*dp2.x;
		if (dp3.x*dp3.x + dp3.y*dp3.y + dp3.z*dp3.z < tolerance_sq) return(0);
	}
	return(1);
}

	//Helper brute-force function for variable size sxl script strings
	//insert numchars chars at: &sxlbuf[sxlind[sxli]+sxlo];
void sxlinsert (long sxli, long sxlo, long numchars)
{
	long i;

	if (sxlind[numsprites]+numchars > sxlmallocsiz)
	{
		i = sxlmallocsiz;
		while (i < sxlind[numsprites]+numchars) i <<= 1;
		if (!(sxlbuf = (char *)realloc(sxlbuf,i)))
			return; //evilquit("sxlinsert realloc failed");
		sxlmallocsiz = i;
	}

		//move sxlbuf bytes
	for(i=sxlind[numsprites]-1;i>=sxlind[sxli]+sxlo;i--)
		sxlbuf[i+numchars] = sxlbuf[i];

		//move sxlind indexes
	for(i=numsprites;i>sxli;i--) sxlind[i] += numchars;
}

	//Helper brute-force function for variable size sxl script strings
	//delete numchars chars at: &sxlbuf[sxlind[sxli]+sxlo];
void sxldelete (long sxli, long sxlo, long numchars)
{
	long i;

		//move sxlbuf bytes
	for(i=sxlind[sxli]+sxlo+numchars;i<sxlind[numsprites];i++)
		sxlbuf[i-numchars] = sxlbuf[i];

		//move sxlind indexes
	for(i=sxli+1;i<=numsprites;i++) sxlind[i] -= numchars;
}

	//NOTE: skipuserst should be 0 except for the call from voxedloadsxl
long insertsprite (char *filnam, char *userst, long skipuserst)
{
	long i;

	if (numsprites >= MAXSPRITES) return(-1);

	memset((void *)&spr[numsprites],0,sizeof(vx5sprite));
	i = strlen(filnam);

	if ((i >= 4) && (filnam[i-4] == '.') &&
		((filnam[i-3] == 'K') || (filnam[i-3] == 'k')) &&
		((filnam[i-2] == 'F') || (filnam[i-2] == 'f')) &&
		((filnam[i-1] == 'A') || (filnam[i-1] == 'a')))
	{
		spr[numsprites].kfaptr = getkfa(filnam);
		spr[numsprites].kfatim = 0;
		spr[numsprites].flags = 2;
	}
	else
	{     //If .KV6 not found, getkv6 will generate a dummy kv6data structure
		spr[numsprites].voxnum = getkv6(filnam);
		spr[numsprites].flags = 0;
	}

	sortorder[numsprites] = numsprites;
	sortdist[numsprites] = 0;
	numsprites++;

	if (!skipuserst)
	{
			//Insert & copy string into sxlbuf
		i = strlen(userst);
		sxlind[numsprites] = sxlind[numsprites-1];
		sxlinsert(numsprites-1,0,i+1);
		memcpy(&sxlbuf[sxlind[numsprites-1]],userst,i+1);
	}
	return(numsprites-1);
}

void deletesprite (long daspri)
{
	long i, j, k;
	char ch;

	numsprites--;
	if (daspri < numsprites)
	{
		spr[daspri] = spr[numsprites];

			//char *sxlbuf;
			//long sxlind[MAXSPRITES+1];
			//3          4       5        6
			//asldkfjasd_rlkjwas_asdlkjhas_  (daspri=3, numsprites=5 after --)

		j = (sxlind[numsprites+1]-sxlind[numsprites])-(sxlind[daspri+1]-sxlind[daspri]);
		if (j <= 0) //Fast case: O(n) :)
		{
				//Shift buffer after daspri to the left (by daspri's bytes)
			k = sxlind[daspri]-sxlind[numsprites];
			for(i=sxlind[numsprites];i<sxlind[numsprites+1];i++)
				sxlbuf[i+k] = sxlbuf[i];

				//Shift rest of buffer to the left
			for(i+=k;i-j<sxlind[numsprites+1];i++) sxlbuf[i] = sxlbuf[i-j];

		}
		else //Slow case: O(3n) :(
		{
				//Reverse last sxl userstring (numsprites after --)
			for(i=((sxlind[numsprites+1]-sxlind[numsprites])>>1)-1;i>=0;i--)
			{
				ch = sxlbuf[sxlind[numsprites]+i];
				sxlbuf[sxlind[numsprites]+i] = sxlbuf[sxlind[numsprites+1]-i-1];
				sxlbuf[sxlind[numsprites+1]-i-1] = ch;
			}

				//Reverse rest (daspri+1) to (numsprites-1 after --)
			for(i=((sxlind[numsprites]-sxlind[daspri+1])>>1)-1;i>=0;i--)
			{
				ch = sxlbuf[sxlind[daspri+1]+i];
				sxlbuf[sxlind[daspri+1]+i] = sxlbuf[sxlind[numsprites]-i-1];
				sxlbuf[sxlind[numsprites]-i-1] = ch;
			}

				//Reverse both (daspri+1) to (numsprites after --)
			for(i=((sxlind[numsprites+1]-sxlind[daspri+1])>>1)-1;i>=0;i--)
			{
				ch = sxlbuf[sxlind[daspri+1]+i];
				sxlbuf[sxlind[daspri+1]+i] = sxlbuf[sxlind[numsprites+1]-i-1];
				sxlbuf[sxlind[numsprites+1]-i-1] = ch;
			}

				//Shift buffer after daspri to the left (by daspri's bytes)
			j = sxlind[daspri+1]-sxlind[daspri];
			for(i=sxlind[daspri+1];i<sxlind[numsprites+1];i++)
				sxlbuf[i-j] = sxlbuf[i];

			j = (sxlind[numsprites+1]-sxlind[numsprites])-(sxlind[daspri+1]-sxlind[daspri]);
		}
			//Fix sxlind
		for(i=daspri+1;i<=numsprites;i++) sxlind[i] += j;
	}

		//Make sure sortorder maintains correct indices
	for(i=1;i<numsprites;i++) sortorder[i] = i;
}

static point3d unitaxis[6] = {{1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}};
static char unitaxlu[24][3] =
{
	0,2,4, 0,3,5, 1,2,5, 1,3,4, 0,5,2, 0,4,3, 1,4,2, 1,5,3,
	2,0,5, 3,0,4, 2,1,4, 3,1,5, 2,4,0, 3,5,0, 2,5,1, 3,4,1,
	4,0,2, 5,0,3, 5,1,2, 4,1,3, 5,2,0, 4,3,0, 4,2,1, 5,3,1,
// Here are the 24 mirrored rotations: (remember to change to unitaxlu[48][3])
// 0,2,5, 0,3,4, 1,2,4, 1,3,5, 0,4,2, 0,5,3, 1,5,2, 1,4,3,
// 2,0,4, 3,0,5, 2,1,5, 3,1,4, 2,5,0, 3,4,0, 2,4,1, 3,5,1,
// 5,0,2, 4,0,3, 4,1,2, 5,1,3, 4,2,0, 5,3,0, 5,2,1, 4,3,1,
};

	//This uses code from slerp(...) - given 2 orthonormal bases, it returns
	//   the cosine of the angle along the shortest (optimal) axis of rotation
	//  (range: -1.0 to 1.0)
static float orthodist (point3d *a0, point3d *a1, point3d *a2, point3d *b0, point3d *b1, point3d *b2)
{
	point3d ax;
	float t, ox, oy, c, s;

	ax.x = a0->y*b0->z - a0->z*b0->y + a1->y*b1->z - a1->z*b1->y + a2->y*b2->z - a2->z*b2->y;
	ax.y = a0->z*b0->x - a0->x*b0->z + a1->z*b1->x - a1->x*b1->z + a2->z*b2->x - a2->x*b2->z;
	ax.z = a0->x*b0->y - a0->y*b0->x + a1->x*b1->y - a1->y*b1->x + a2->x*b2->y - a2->y*b2->x;
	t = ax.x*ax.x + ax.y*ax.y + ax.z*ax.z;

	ox = a0->x*ax.x + a0->y*ax.y + a0->z*ax.z;
	oy = a1->x*ax.x + a1->y*ax.y + a1->z*ax.z;
	if (fabs(ox) < fabs(oy))
		{ c = a0->x*b0->x + a0->y*b0->y + a0->z*b0->z; s = ox*ox; }
	else
		{ c = a1->x*b1->x + a1->y*b1->y + a1->z*b1->z; s = oy*oy; }
	if (t == s) //Exactly aligned on axes: must use dot product test instead
	{
		if (a0->x*b0->x + a0->y*b0->y + a0->z*b0->z < .5) return(-1.0);
		if (a1->x*b1->x + a1->y*b1->y + a1->z*b1->z < .5) return(-1.0);
		if (a2->x*b2->x + a2->y*b2->y + a2->z*b2->z < .5) return(-1.0);
		return(1.0);
	}
	return((c*t - s) / (t-s));
}

void fixanglemode ()
{
	if (anglemode) return;
	anglng = atan2(-ifor.y,-ifor.x);
	anglat = atan2(ifor.z,sqrt(ifor.x*ifor.x + ifor.y*ifor.y));
}

	//Call this at init if voxedloadsxl fails or isn't called
void initsxl ()
{
	long i;

	i = 256;
	if (i > sxlmallocsiz)
	{
		sxlmallocsiz = i;
		if (!(sxlbuf = (char *)realloc(sxlbuf,i)))
			return; //evilquit("sxlinsert realloc failed");
		sxlmallocsiz = i;
	}

	sxlind[0] = 0; sxlind[1] = 1; sxlbuf[0] = 0; numsprites = 1;
}

long voxedloadsxl (char *sxlnam)
{
	float f;
	long i, j, k, l, m, n, o, tabcnt;

		//NOTE: MUST buffer file because insertsprite uses kz file code :/
	if (!kzopen(sxlnam)) return(0);
	l = kzfilelength();
	for(i=256;i<l;i<<=1); //i = (lowest power of 2)>=l
	if (i > sxlmallocsiz)
	{
		if (!(sxlbuf = (char *)realloc(sxlbuf,i))) return(0);
		sxlmallocsiz = i;
	}
	kzread(sxlbuf,l);
	kzclose();

	j = 0; n = 0;

	k = 0; while ((sxlbuf[j]!=13) && (sxlbuf[j]!=10) && (j < l)) vxlfilnam[k++] = sxlbuf[j++];
	while (((sxlbuf[j]==13) || (sxlbuf[j]==10)) && (j < l)) j++; vxlfilnam[k] = 0;

	k = 0; while ((sxlbuf[j]!=13) && (sxlbuf[j]!=10) && (j < l)) skyfilnam[k++] = sxlbuf[j++];
	while (((sxlbuf[j]==13) || (sxlbuf[j]==10)) && (j < l)) j++; skyfilnam[k] = 0;

	i = 0; tabcnt = 0; numsprites = 1; goto voxedloadsxlskip;
	while (j < l)
	{
		for(k=j;(sxlbuf[k]!=',') && (k < l);k++); sxlbuf[k] = 0;

		i = insertsprite(&sxlbuf[j],"",1); j = k+1;

		for(m=0;m<12;m++)
		{
			if (m < 11) { for(k=j;(sxlbuf[k]!=',') && (k < l);k++); }
			else { for(k=j;(sxlbuf[k]!=13) && (sxlbuf[k]!=10) && (k < l);k++); }
			sxlbuf[k] = 0;

			if ((k-j > 2) && (sxlbuf[j] == '0') && (sxlbuf[j+1] == 'x'))
			{
				for(j+=2,o=0;j<k;j++) //Hex decoder
					{ o <<= 4; if (j < 65) o += (j-48); else o += (j&31)+9; }
				f = *(float *)&o;
			}
			else
				f = atof(&sxlbuf[j]);

			j = k+1;
			switch(m)
			{
				case  0: spr[i].p.x = f; break;
				case  1: spr[i].p.y = f; break;
				case  2: spr[i].p.z = f; break;
				case  3: spr[i].s.x = f; break;
				case  4: spr[i].s.y = f; break;
				case  5: spr[i].s.z = f; break;
				case  6: spr[i].h.x = f; break;
				case  7: spr[i].h.y = f; break;
				case  8: spr[i].h.z = f; break;
				case  9: spr[i].f.x = f; break;
				case 10: spr[i].f.y = f; break;
				case 11: spr[i].f.z = f; break;
			}
		}
		while (((sxlbuf[j]==13) || (sxlbuf[j]==10)) && (j < l)) j++;

voxedloadsxlskip:;
		sxlind[i] = n;
		while (((sxlbuf[j] == ' ') || (sxlbuf[j] == 9)) && (j < l))
		{
			j++;
			while ((sxlbuf[j]!=13) && (sxlbuf[j]!=10) && (j < l))
			{
				if (sxlbuf[j] == 9) tabcnt++;
				sxlbuf[n++] = sxlbuf[j]; j++;
			}
			while (((sxlbuf[j]==13) || (sxlbuf[j]==10)) && (j < l)) j++; sxlbuf[n++] = 0;
		}

		if ((j+2 < l) && (sxlbuf[j] == 'e') && (sxlbuf[j+1] == 'n') && (sxlbuf[j+2] == 'd') &&
			((j+3 == l) || (sxlbuf[j+3] == 13) || (sxlbuf[j+3] == 10))) break;
	}
	numsprites = i+1;
	sxlind[numsprites] = n;

		//Convert tabs to spaces
	tabcnt *= (3-1);
	if (n+tabcnt > sxlmallocsiz)
	{
		i = sxlmallocsiz;
		while (i < n+tabcnt) i <<= 1;
		if (!(sxlbuf = (char *)realloc(sxlbuf,i)))
			return(0); //evilquit("sxlinsert realloc failed");
		sxlmallocsiz = i;
	}
	sxlind[numsprites] = n+tabcnt;
	for(i=n+tabcnt-1,j=n-1,k=numsprites;j>=0;j--)
	{
		if (sxlbuf[j] != 9) { sxlbuf[i--] = sxlbuf[j]; }
		else { sxlbuf[i] = sxlbuf[i-1] = sxlbuf[i-2] = ' '; i -= 3; }
		if ((k) && (sxlind[k-1] == j-1)) sxlind[--k] = i;
	}

	sprintf(message,"Loaded %s",sxlnam); messagetimeout = totclk+4000;
	return(1);
}

void floatprint (float f, FILE *fil)
{
	char buf[32];

	sprintf(buf,"%g",f);
	if ((buf[0] == '0') && (buf[1] == '.'))
		fprintf(fil,"%s",&buf[1]); //"0." -> "."
	else if ((buf[0] == '-') && (buf[1] == '0') && (buf[2] == '.'))
		{ buf[1] = '-'; fprintf(fil,"%s",&buf[1]); } //"-0." -> "-."
	else fprintf(fil,"%s",buf);
}

void savesxl (char *sxlnam)
{
	FILE *fil;
	long i, j, k;

	if (!(fil = fopen(sxlnam,"wb"))) return;
	fprintf(fil,"%s\r\n",relpath(vxlfilnam));
	fprintf(fil,"%s\r\n",relpath(skyfilnam));

	i = 0; goto savesxlskip;
	do
	{
		if (spr[i].voxnum)
		{
			if (spr[i].flags&2) j = spr[i].kfaptr->namoff;
								else j = spr[i].voxnum->namoff;
			fprintf(fil,"%s,",relpath(getkfilname(j)));
		} else fprintf(fil,"DOT,");
		floatprint(spr[i].p.x,fil); fputc(',',fil);
		floatprint(spr[i].p.y,fil); fputc(',',fil);
		floatprint(spr[i].p.z,fil); fputc(',',fil);
		floatprint(spr[i].s.x,fil); fputc(',',fil);
		floatprint(spr[i].s.y,fil); fputc(',',fil);
		floatprint(spr[i].s.z,fil); fputc(',',fil);
		floatprint(spr[i].h.x,fil); fputc(',',fil);
		floatprint(spr[i].h.y,fil); fputc(',',fil);
		floatprint(spr[i].h.z,fil); fputc(',',fil);
		floatprint(spr[i].f.x,fil); fputc(',',fil);
		floatprint(spr[i].f.y,fil); fputc(',',fil);
		floatprint(spr[i].f.z,fil); fputc(13,fil); fputc(10,fil);
savesxlskip:;
			//User string
		for(j=sxlind[i],k=0;j<sxlind[i+1];j++)
		{
			if (!sxlbuf[j]) { fprintf(fil,"\r\n"); k = 0; continue; }
			if (!k) { fputc(32,fil); k = 1; }
			fputc(sxlbuf[j],fil);
		}

		i++;
	} while (i < numsprites);
	fprintf(fil,"end\r\n");
	fclose(fil);

	sprintf(message,"Saved %s",sxlnam); messagetimeout = totclk+4000;
}

void drawarrows (long xx, long yy, long zz, double iforx, double ifory, long col)
{
	long x, y, j, k;

		//   V0V1V2V3
		//H0   ÚÄ¿        (-x,-y)
		//H1 ÚÄÎÄÅÄ¿ (-y,x) ÄÅÄ (y,-x)
		//H2 ÀÄÁÄÁÄÙ       (x,y)

	  //KP2
	if (fabs(iforx) < fabs(ifory)) { x = 0; y = (((ifory < 0)-1)|1); }
									  else { y = 0; x = (((iforx < 0)-1)|1); }
	if (x < 0) { xx++; } else if (x > 0) { yy++; }
	if (y < 0) { xx++; yy++; }

	for(j=1;j>=0;j--)
	{
		drawline3d(xx-x,  yy-y,  zz+j,xx-x+y,  yy-y-x,  zz+j,col); //H0
		drawline3d(xx-y,  yy+x,  zz+j,xx+y*2,  yy-x*2,  zz+j,col); //H1
		drawline3d(xx-y+x,yy+x+y,zz+j,xx+y*2+x,yy-x*2+y,zz+j,col); //H2
		drawline3d(xx-y,  yy+x,  zz+j,xx+x-y,  yy+y+x,  zz+j,col); //V0
		drawline3d(xx-x,  yy-y,  zz+j,xx+x,    yy+y,    zz+j,col); //V1
		drawline3d(xx-x+y,yy-y-x,zz+j,xx+x+y,  yy+y-x,  zz+j,col); //V2
		drawline3d(xx+y*2,yy-x*2,zz+j,xx+x+y*2,yy+y-x*2,zz+j,col); //V3
	}
	drawline3d(xx-x    ,yy-y    ,zz,xx-x    ,yy-y    ,zz+1,col);
	drawline3d(xx-x+y  ,yy-y-x  ,zz,xx-x+y  ,yy-y-x  ,zz+1,col);
	drawline3d(xx-y    ,yy+x    ,zz,xx-y    ,yy+x    ,zz+1,col);
	drawline3d(xx      ,yy      ,zz,xx      ,yy      ,zz+1,col);
	drawline3d(xx+y    ,yy-x    ,zz,xx+y    ,yy-x    ,zz+1,col);
	drawline3d(xx+y*2  ,yy-x*2  ,zz,xx+y*2  ,yy-x*2  ,zz+1,col);
	drawline3d(xx+x-y  ,yy+y+x  ,zz,xx+x-y  ,yy+y+x  ,zz+1,col);
	drawline3d(xx+x    ,yy+y    ,zz,xx+x    ,yy+y    ,zz+1,col);
	drawline3d(xx+x+y  ,yy+y-x  ,zz,xx+x+y  ,yy+y-x  ,zz+1,col);
	drawline3d(xx+x+y*2,yy+y-x*2,zz,xx+x+y*2,yy+y-x*2,zz+1,col);
}

void getkparrows (lpoint3d *retv)
{
	long i, x, y, z;

	x = y = z = 0;
	ftol(fsynctics*1000,&i);
	if (keystatus[0x4f]) //KP1
	{
		if ((keystatus[0x4f] == 1) || (keystatus[0x4f] == 255))
		{
			if (fabs(ifor.x) < fabs(ifor.y))
				  { if (ifor.y < 0) x--; else x++; }
			else { if (ifor.x < 0) y++; else y--; }
		}
		keystatus[0x4f] = MIN(keystatus[0x4f]+i,255);
	}
	if (keystatus[0x51]) //KP3
	{
		if ((keystatus[0x51] == 1) || (keystatus[0x51] == 255))
		{
			if (fabs(ifor.x) < fabs(ifor.y))
				  { if (ifor.y < 0) x++; else x--; }
			else { if (ifor.x < 0) y--; else y++; }
		}
		keystatus[0x51] = MIN(keystatus[0x51]+i,255);
	}
	if (keystatus[0x4c]) //KP5
	{
		if ((keystatus[0x4c] == 1) || (keystatus[0x4c] == 255))
		{
			if (fabs(ifor.x) < fabs(ifor.y))
				  { if (ifor.y < 0) y--; else y++; }
			else { if (ifor.x < 0) x--; else x++; }
		}
		keystatus[0x4c] = MIN(keystatus[0x4c]+i,255);
	}
	if (keystatus[0x50]) //KP2
	{
		if ((keystatus[0x50] == 1) || (keystatus[0x50] == 255))
		{
			if (fabs(ifor.x) < fabs(ifor.y))
				  { if (ifor.y < 0) y++; else y--; }
			else { if (ifor.x < 0) x++; else x--; }
		}
		keystatus[0x50] = MIN(keystatus[0x50]+i,255);
	}
	if (keystatus[0x4b]) //KP4
	{
		if ((keystatus[0x4b] == 1) || (keystatus[0x4b] == 255)) z--;
		keystatus[0x4b] = MIN(keystatus[0x4b]+i,255);
	}
	if (keystatus[0x9c]) //KPEnter
	{
		if ((keystatus[0x9c] == 1) || (keystatus[0x9c] == 255)) z++;
		keystatus[0x9c] = MIN(keystatus[0x9c]+i,255);
	}
	retv->x = x; retv->y = y; retv->z = z;
}

void notepaddraw ()
{
	long i, j, k, l, x, y, xx, yy;
	char *v;

		//Find dimensions of text box
	v = &sxlbuf[sxlind[curspri]]; l = sxlind[curspri+1]-sxlind[curspri];
	i = x = y = 0;
	do
	{
		for(j=i;v[j];j++);
		if (j-i > x) x = j-i;
		y++; i = j+1;
	} while (i < l);
	xx = (xres>>1)-(((x+2)*6)>>1);
	yy = (yres>>1)-(((y+2)*8)>>1);

		//Draw text box
	i = k = 0;
	print6x8(xx,k*8+yy,0x202020,0xc0c0c0,"\xda%*c",x+1,'\xbf');
	memset((void *)((k*8+yy+3)*bytesperline+((xx+6)<<2)+frameplace),0x20,(x*6)<<2);
	k++;
	do
	{
		for(j=i;v[j];j++);
		print6x8(xx,k*8+yy,0x202020,0xc0c0c0,"\xb3%s%*c",&v[i],x-(j-i)+1,'\xb3'); k++;
		i = j+1;
	} while (i < l);
	print6x8(xx,k*8+yy,0x202020,0xc0c0c0,"\xc0%*c",x+1,'\xd9');
	memset((void *)((k*8+yy+3)*bytesperline+((xx+6)<<2)+frameplace),0x20,(x*6)<<2);

	if (!((totclk-notepadcurstime)&128))
	{
		i = 0; k = 1; //i = index to first char of cursor's line
		do
		{
			for(j=i;v[j];j++);
			if (j >= notepadmode) break;
			k++; i = j+1;
		} while (i < l);

		if (notepadinsertmode)
			print6x8((notepadmode+1-i)*6+xx-3,k*8+yy,0x800000,-1,"\xb3"); //179
		else
			print6x8((notepadmode+1-i)*6+xx,k*8+yy,0x800000,-1,"\xdb"); //219
	}

		//Draw text mouse cursor:
	j = cmousy*bytesperline+(cmousx<<2)+frameplace;
	if (notepadinsertmode)
	{
		k = bytesperline*4;
		for(i=bytesperline-k;i<k;i+=bytesperline) *(long *)(i+j) = 0x800000;
		for(i=-(2<<2);i;i+=(1<<2))
		{
			*(long *)(j-k+i) = *(long *)(j-k-i) = 0x800000;
			*(long *)(j+k+i) = *(long *)(j+k-i) = 0x800000;
		}
	}
	else
	{
		//     Insert:    Overwrite:
		//   ÛÛÛÛ  ÛÛÛÛ
		//       ÛÛ         ÛÛÛÛÛ
		//       ÛÛ       ÛÛ     ÛÛ
		//       ÛÛ       ÛÛ     ÛÛ
		//       []       []     ÛÛ
		//       ÛÛ       ÛÛ     ÛÛ
		//       ÛÛ       ÛÛ     ÛÛ
		//       ÛÛ         ÛÛÛÛÛ
		//   ÛÛÛÛ  ÛÛÛÛ

		for(i=-2;i<=2;i++)
		{
			*(long *)(j+bytesperline*i) = 0x800000;
			*(long *)(j+bytesperline*i+(3<<2)) = 0x800000;
		}
		for(i=1;i<=2;i++)
		{
			*(long *)(j-bytesperline*3+(i<<2)) = 0x800000;
			*(long *)(j+bytesperline*3+(i<<2)) = 0x800000;
		}
	}
}

long notepadinput ()
{
	float fmousx, fmousy;
	long i, j, k, l, x, y, xx, yy;
	char ch, *v;

	while (i = keyread())
	{
		notepadcurstime = totclk;
		if ((i&255) && (!(i&0x3c0000)))
		{
			switch(i&255)
			{
				case 27: //ESC
					keystatus[0x1] = 0; notepadmode = -1; return(0);
				case 8: //Backspace
					if (notepadmode > 0)
					{
						notepadmode--;
						sxldelete(curspri,notepadmode,1);
					}
					break;
				case 9: //Tab
					sxlinsert(curspri,notepadmode,3);
					sxlbuf[sxlind[curspri]+notepadmode  ] = 32;
					sxlbuf[sxlind[curspri]+notepadmode+1] = 32;
					sxlbuf[sxlind[curspri]+notepadmode+2] = 32;
					notepadmode += 3;
					break;
				case 13: //CR (stored as 0 in Voxed's memory, stored as 13 in savesxl)
					sxlinsert(curspri,notepadmode,1);
					sxlbuf[sxlind[curspri]+notepadmode] = 0;
					notepadmode++;
					break;
				default:
						//Normal ASCII char...
					if ((notepadinsertmode) || (!sxlbuf[sxlind[curspri]+notepadmode]))
						sxlinsert(curspri,notepadmode,1);
					sxlbuf[sxlind[curspri]+notepadmode] = (char)(i&255);
					notepadmode++;
					break;
			}
		}
		else
		{
			switch((i>>8)&0xff)
			{
#if 0
				case 0x15: //Y
					if (i&0xc0000) //CTRL
					{
						for(i=notepadmode;(i) && (sxlbuf[sxlind[curspri]+i-1] != 13);i--);
						l = sxlind[curspri+1]-sxlind[curspri]-1;
						for(k=notepadmode;(k < l) && (sxlbuf[sxlind[curspri]+k] != 13);k++);

						if (k < l) sxldelete(curspri,i,k-i+1);
						else
						{
							if (i) notepadmode = --i;
							sxldelete(curspri,i,k-i);
						}
					}
					break;
#endif
				case 0xc7: //Home
					if (i&0xc0000) { notepadmode = 0; break; } //CTRL
					while ((notepadmode) && (sxlbuf[sxlind[curspri]+notepadmode-1])) notepadmode--;
					break;
				case 0xcf: //End
					if (i&0xc0000) { notepadmode = sxlind[curspri+1]-sxlind[curspri]-1; break; } //CTRL
					while (sxlbuf[sxlind[curspri]+notepadmode]) notepadmode++;
					break;
				case 0xcb: //Left
					if (i&0xc0000) //CTRL
					{
						for(i=0;i<=2;i++)
						{
							do
							{
								if (notepadmode < 0) break;
								ch = sxlbuf[sxlind[curspri]+notepadmode];
								if ((notepadmode >= 0) &&
									((((ch >= '0') && (ch <= '1')) ||
									  ((ch >= 'A') && (ch <= 'Z')) ||
									  ((ch >= 'a') && (ch <= 'z'))) != (i&1)))
									notepadmode--;
								else break;
							} while (1);
						}
						notepadmode++; break;
					}
					if ((notepadmode) && (sxlbuf[sxlind[curspri]+notepadmode-1])) notepadmode--;
					break;
				case 0xcd: //Right
					l = sxlind[curspri+1]-sxlind[curspri]-1;
					if (i&0xc0000) //CTRL
					{
						for(i=0;i<=1;i++)
						{
							do
							{
								if (notepadmode >= l) break;
								ch = sxlbuf[sxlind[curspri]+notepadmode];
								if ((notepadmode < l) &&
									((((ch >= '0') && (ch <= '1')) ||
									  ((ch >= 'A') && (ch <= 'Z')) ||
									  ((ch >= 'a') && (ch <= 'z'))) != (i&1)))
									notepadmode++;
								else break;
							} while (1);
						}
						break;
					}
					if ((notepadmode < l) && (sxlbuf[sxlind[curspri]+notepadmode])) notepadmode++;
					break;
				case 0xc8: //Up
					for(i=notepadmode;(i) && (sxlbuf[sxlind[curspri]+i-1]);i--);
					if (i)
					{
						for(k=i-1;(k) && (sxlbuf[sxlind[curspri]+k-1]);k--);
						notepadmode = MIN(k+(notepadmode-i),i-1);
					}
					break;
				case 0xd0: //Down
					j = sxlind[curspri+1]-sxlind[curspri]-1;
					for(k=notepadmode;(k < j) && (sxlbuf[sxlind[curspri]+k]);k++); k++;
					if (k < j)
					{
						for(i=notepadmode;(i) && (sxlbuf[sxlind[curspri]+i-1]);i--);
						for(l=k;(l < j) && (sxlbuf[sxlind[curspri]+l]);l++);
						notepadmode = MIN(notepadmode-i,l-k)+k;
					}
					break;
				case 0xd3: if (notepadmode < sxlind[curspri+1]-sxlind[curspri]-1) sxldelete(curspri,notepadmode,1); break; //Delete
				case 0xd2: notepadinsertmode ^= 1; break; //Insert
			}
		}
	}
	obstatus = bstatus; readmouse(&fmousx,&fmousy,&bstatus);
	fcmousx = MIN(MAX(fcmousx+fmousx,3),xres-3); ftol(fcmousx-.5,&cmousx);
	fcmousy = MIN(MAX(fcmousy+fmousy,4),yres-4); ftol(fcmousy-.5,&cmousy);
	if ((~obstatus)&bstatus&1) //LMB (moves text cursor)
	{
			//Find dimensions of text box
		v = &sxlbuf[sxlind[curspri]]; l = sxlind[curspri+1]-sxlind[curspri];
		i = x = y = 0;
		do
		{
			for(j=i;v[j];j++);
			if (j-i > x) x = j-i;
			y++; i = j+1;
		} while (i < l);
		xx = (xres>>1)-(((x+2)*6)>>1);
		yy = (yres>>1)-(((y+2)*8)>>1);

		if (cmousy < yy+8) notepadmode = 0;
		else if (cmousy >= yy+y*8+8) notepadmode = l-1;
		else
		{
			i = 0; k = yy+16;
			do
			{
				for(j=i;v[j];j++);
				if (cmousy < k)
				{
					k = (cmousx-(xx+4))/6;
					notepadmode = MIN(MAX(k,0),j-i)+i; break;
				}
				k += 8; i = j+1;
			} while (i < l);
		}
	}
	if ((~obstatus)&bstatus&2) { notepadmode = -1; return(0); }
	return(1); //Return immediately - don't want to process any other keys...
}

#if (USETDHELP != 0)
#include "kbdhelp.h"

static struct {
	long tf, tp, tx, ty; // tile
	float x, y;
	float cx, cy;
	float tim;
} kbdh;

long kbdhelp_load()
{
	//kbdh.tf = kbd.f; kbdh.tp = kbd.p; kbdh.tx = kbd.x; kbdh.ty = kbd.y;
	kpzload("png\\keyboard.png", &kbdh.tf, &kbdh.tp, &kbdh.tx, &kbdh.ty);
	kbdh.x = (xres - kbdh.tx)/2;
	kbdh.y = yres - kbdh.ty; //yres*5/8;
	kbdh.cx = xres/2;
	kbdh.cy = yres*6/8;
	kbdh.tim = 0;
	return kbdh.tf;
}

long kbdhelp_update(float fmx, float fmy, long mb, long xdim, long ydim)
{
	long x1 = xdim*1/8;
	long x2 = xdim*7/8;
	long y1 = 4; //ydim*17/32;  // ydim*5/8;
	long y2 = ydim-4;      // ydim*7/8;
	fmx *= xdim*.0025f;
	fmy *= ydim*.0025f;
	if (mb&2) {
		kbdh.x += fmx;
		kbdh.cx += fmx;
		//kbdh.y += fmy;
	} else {
		const float spd = 1.0f;
		kbdh.cx += fmx;
		kbdh.cy += fmy;
		if (xdim >= kbdh.tx || kbdh.x == 0) { if (kbdh.cx < 4) kbdh.cx = 4; }
		else if (kbdh.cx < x1) { kbdh.cx = x1; kbdh.x -= fmx*spd; }
		if (xdim >= kbdh.tx || kbdh.x == xdim - kbdh.tx) { if (kbdh.cx > xdim-4) kbdh.cx = xdim-4; }
		else if (kbdh.cx > x2) { kbdh.cx = x2; kbdh.x -= fmx*spd; }
		if (kbdh.cy < y1) kbdh.cy = y1; else if (kbdh.cy > y2) kbdh.cy = y2;
	}
	if (xdim < kbdh.tx) {
		if (kbdh.x > 0) kbdh.x = 0; else if (kbdh.x + kbdh.tx < xdim) kbdh.x = xdim - kbdh.tx;
	} else {
		kbdh.x = (xdim - kbdh.tx)/2;
	}
	return 1;
}

void kbdhelp_draw(long frameptr, long pitch, long xdim, long ydim, float fsynctics)
{
	long colors[] = {0, 0xC06060, 0x60C060, 0x3060D0, 0xC0C060, 0xA030C0, 0x60C0C0, 0xA0A0A0};
	long i, j, n, cx, cy;
	long clipy = 0; //ydim*17/32;
	long x0 = (long)kbdh.x;
	long y0 = (long)kbdh.y;
	long x1 = x0;
	long y1 = y0;
	long x2 = x0 + kbdh.tx;
	long y2 = y0 + kbdh.ty;
		// clip
	if (x1 < 0) x1 = 0; else if (x2 > xdim) x2 = xdim;
	if (y1 < clipy) y1 = clipy; else if (y2 > ydim) y2 = ydim;
	if (x1 < x2 && y1 < y2) {
		long tptr = kbdh.tf + (y1 - y0)*kbdh.tp + (x1 - x0)*4;
		long ptr = frameptr + y1*pitch + x1*4;
		long w4 = (x2 - x1)*4;
		for( long h = y2 - y1; h > 0; h--, ptr += pitch - w4, tptr += kbdh.tp - w4)
			for( long w = x2 - x1; w > 0; w--, ptr += 4, tptr += 4)
				*(long *)ptr = *(long *)tptr;
	}
	cx = (long)(kbdh.cx - kbdh.x);
	cy = (long)(kbdh.cy - kbdh.y);
	i = -1;
	for(j=0; kbdhelp[j].keycode; j++) {
		if (cx >= kbdhelp[j].x && cx <= kbdhelp[j].x + kbdhelp[j].w &&
			 cy >= kbdhelp[j].y && cy <= kbdhelp[j].y + kbdhelp[j].h) {
			i = j;
		}
		if (kbdhelp[j].color && (i != j || kbdh.tim < 0.5f)) {
			x0 = kbdhelp[j].x;
			y0 = kbdhelp[j].y;
			x1 = x0 + (long)kbdh.x;
			y1 = y0 + (long)kbdh.y;
			x2 = x1 + kbdhelp[j].w;
			y2 = y1 + kbdhelp[j].h;
			if (x1 < 0) { x0 += 0 - x1; x1 = 0; } else if (x2 > xdim) x2 = xdim;
			if (y1 < clipy) { y0 += clipy - y1; y1 = ydim*17/32; } else if (y2 > ydim) y2 = ydim;
			if (x1 < x2 && y1 < y2) {
				long tptr = kbdh.tf + y0*kbdh.tp + x0*4;
				long ptr = frameptr + y1*pitch + x1*4;
				long w4 = (x2 - x1)*4;
				long col = (colors[kbdhelp[j].color]&0xFEFEFE)>>1;
				for( long h = y2 - y1; h > 0; h--, ptr += pitch - w4, tptr += kbdh.tp - w4)
					for( long w = x2 - x1; w > 0; w--, ptr += 4, tptr += 4)
						*(long *)ptr = (((*(long *)tptr)&0xFEFEFE)>>1)+col;
			}
		}
	}
	// cursor pos:
	x0 = (long)kbdh.cx;
	y0 = (long)kbdh.cy;
	// draw help strings:
	if (i != -1) {
		const long fw = 4;
		const long fh = 6;
		long max = 0, len;
		for(n=0; kbdhelp[i].help[n][0]; n++) {
			len = strlen(kbdhelp[i].help[n]);
			if (len > xdim/fw) { len = xdim/fw; kbdhelp[i].help[n][len] = 0; }
			if (len > max) max = len;
		}
		max *= fw;
		len = n*fh;
		//x1 = (long)kbdh.x + kbdhelp[i].x + kbdhelp[i].w/2 - max/2;
		//y1 = (long)kbdh.y + kbdhelp[i].y + kbdhelp[i].h + 4;
		x1 = 0;
		y1 = ydim; //(long)kbdh.y - len;
		if (x1 < 0) x1 = 0; else if (x1 + max > xdim) x1 = xdim - max;
		if (y1 + len > ydim) y1 = ydim - len; else if (y1 < 0) y1 = 0;
		for(j=0; j < n; j++) print4x6(x1, y1+j*fh, -1, 0, "%s", kbdhelp[i].help[j]);
	}
	// draw cursor:
	if (x0 > 1 && y0 > 1 && x0 < xdim-1 && y0 < ydim-1) {
		*(long *)(frameptr + (x0-1)*4 + (y0  )*pitch) = -1;
		*(long *)(frameptr + (x0+1)*4 + (y0  )*pitch) = -1;
		*(long *)(frameptr + (x0  )*4 + (y0-1)*pitch) = -1;
		*(long *)(frameptr + (x0  )*4 + (y0+1)*pitch) = -1;
	}
	kbdh.tim += fsynctics*4; while (kbdh.tim > 1.0f) kbdh.tim -= 1.0f;
}
#endif

void helpdraw ()
{
	long i, j, k, l, x, y, yy;
	char ch;

	i = 0;

	for(k=(helpypos>>10)/6;k;k--)
	{
		j = i;
		while ((j < helpleng) && ((helpbuf[j] != 13) && (helpbuf[j] != 10)))
			j++;
		if (j >= helpleng) break;

		i = j+1; if (i >= helpleng) break;
		if ((helpbuf[i] == 13) || (helpbuf[i] == 10)) i++;
	}
	x = MAX((xres-80*4)>>1,0);

	for(y=16;y<22-((helpypos>>10)%6);y++)
		memset((void *)(y*bytesperline+frameplace+(x<<2)),0xc0,80<<4);
	for(y=22-((helpypos>>10)%6);y<yres-12;y+=6)
	{
		j = i;
		while ((j < helpleng) && ((helpbuf[j] != 13) && (helpbuf[j] != 10)))
			j++;
		if (j >= helpleng) break;

			//l = actual length of line (tabs multiplied by 3)
		l = j-i; for(k=i;k<j;k++) if (helpbuf[k] == 9) l += 2;
		l++; //Hack for the 1 extra space before

		ch = helpbuf[j]; helpbuf[j] = 0;
		for(yy=y;yy<y+6;yy++)
			memset((void *)(yy*bytesperline+frameplace+(x<<2)),0xc0,1<<4);
		print4x6(x+4,y,0x202020,0xc0c0c0,"%s",&helpbuf[i]);
		for(yy=y;yy<y+6;yy++)
			memset((void *)(yy*bytesperline+frameplace+(x<<2)+(l<<4)),0xc0,(80-l)<<4);
		helpbuf[j] = ch;

		i = j+1; if (i >= helpleng) break;
		if ((helpbuf[i] == 13) || (helpbuf[i] == 10)) i++;
	}
	for(;y<yres-6;y++)
		memset((void *)(y*bytesperline+frameplace+(x<<2)),0xc0,(80*4)<<2);
}

void uninitapp ()
{
	long i;

	if (sxlbuf) { free(sxlbuf); sxlbuf = 0; }
	if (helpbuf) { free(helpbuf); helpbuf = 0; }

	if (curpath[0])
	{
#ifndef _WIN32
		chdir(curpath);
#else
		SetCurrentDirectory(curpath);
#endif
	}

	kzuninit();
	uninitvoxlap();
}

void doframe ()
{
	vx5sprite tempspr;
	kv6voxtype *kp;
	dpoint3d dp, dp2, dp3;
	point3d tp;
	lpoint3d lp, lipos; //lipos for speed purposes only
	float f, t, ox, oy, oz, dx, dy, dz, rr[9], fmousx, fmousy;
	long i, j, k, l, m, x, y, z, xx, yy, zz, r, g, b, bakcol;
	char snotbuf[MAX(MAX_PATH+1,256)], ch, *v;

	startdirectdraw(&frameplace,&bytesperline,&x,&y);
	voxsetframebuffer(frameplace,bytesperline,x,y);
	if ((x != oxres) || (y != oyres))
		{ oxres = x; oyres = y; vx5hx = xres*.5; vx5hy = yres*.5; vx5hz = xres*.5; }

	ftol(ipos.x-.5,&lipos.x);
	ftol(ipos.y-.5,&lipos.y);
	ftol(ipos.z-.5,&lipos.z);

	while (isvoxelsolid(lipos.x,lipos.y,lipos.z))
		{ ipos.z--; ftol(ipos.z-.5,&lipos.z); }

	//--------------------------------
	hitscan(&ipos,&ifor,&hit,&hind,&hdir);
	if ((hind) && (!colselectmode))
	{
		if (gridmode)
		{
			hit.x = ((hit.x+(gridmode>>1))&~(gridmode-1));
			hit.y = ((hit.y+(gridmode>>1))&~(gridmode-1));
			hit.z = MIN(MAX(hit.z,0),MAXZDIM-1);
			hind = 0;
			if (((unsigned long)hit.x < VSID) && ((unsigned long)hit.y < VSID))
				for(i=1;i<512;i++)
				{
					if (!(i&1)) z = hit.z-(i>>1); else z = hit.z+(i>>1);
					j = getcube(hit.x,hit.y,z);
					if ((unsigned long)j >= 2)
						{ hit.z = z; hind = (long *)j; break; }
				}
		}
		if (hind)
		{
			bakcol = *hind;
			if ((hind != ohind) || ((((totclk-hindclk)>>3)&31) < 12))
			{
				*hind ^= 0x101010; ohind = hind;
				if (hind != ohind) hindclk = totclk;
			}
		}
	}

	for(i=MAXBUL-1;i>=0;i--)
	{
		if (bulactive[i] < 0) continue;
		bul[i].x += bulvel[i].x * fsynctics*32;
		bul[i].y += bulvel[i].y * fsynctics*32;
		bul[i].z += bulvel[i].z * fsynctics*32;
		if (((unsigned long)bul[i].x >= VSID) || ((unsigned long)bul[i].y >= VSID))
			bulactive[i] = -1;
		else if (isvoxelsolid((long)bul[i].x,(long)bul[i].y,(long)bul[i].z))
			bulactive[i] = -1;
		else
		{
			thit[0].x = bul[i].x; thit[0].y = bul[i].y; thit[0].z = bul[i].z;
			voxbackup(thit[0].x-64,thit[0].y-64,thit[0].x+65,thit[0].y+65,SETDYN);
			setflash(thit[0].x,thit[0].y,thit[0].z,64,512,8);
		}
	}

	//if (gridmode) xorgrid();

#if (STEREOMODE != 0)
		//(0.9  ,.11  ,.60) : Lots of depth
		//(0.5  ,.06  ,.60) : A valid compromise?
		//(0.225,.0275,.60) : This looks like cube-aspect ratio, but is it?
	if (!(numframes&1)) { f = +0.5; vx5hx = xres*(.5+.06); }
						else { f = -0.5; vx5hx = xres*(.5-.06); }
	vx5hy = yres*.5; vx5hz = xres*.6;
	dp.x = istr.x*f+ipos.x; dp.y = istr.y*f+ipos.y; dp.z = istr.z*f+ipos.z;
	setcamera(&dp,&istr,&ihei,&ifor,vx5hx,vx5hy,vx5hz);
	opticast();

#if (STEREOMODE == 2)
#ifndef _WIN32
	limitrate();
	outp(0x378,((numframes&1)<<2)^0xff);
#endif
#endif
#else
	setcamera(&ipos,&istr,&ihei,&ifor,vx5hx,vx5hy,vx5hz);
	opticast();
#endif
	//if (gridmode) xorgrid();

	for(i=MAXBUL-1;i>=0;i--)
		if (bulactive[i] >= 0) voxrestore();

		//Sort KV6 sprites
	for(i=numsprites-1;i>0;i--)
	{
		ox = spr[i].p.x-ipos.x;
		oy = spr[i].p.y-ipos.y;
		oz = spr[i].p.z-ipos.z;
		sortdist[i] = ox*ox+oy*oy+oz*oz;
	}
	for(j=2;j<numsprites;j++) //NOTE: sprite #0 is dummy - ignore it!
		for(i=1;i<j;i++)
			if (sortdist[sortorder[i]] < sortdist[sortorder[j]])
				{ k = sortorder[i]; sortorder[i] = sortorder[j]; sortorder[j] = k; }

		//Draw KV6 sprites
	for(l=1;l<numsprites;l++) //NOTE: l forwards means back to front
	{
		i = sortorder[l];

		if ((spr[i].voxnum) && (spr[i].voxnum->numvoxs))
		{
			if (spr[i].flags&2) animsprite(&spr[i],fsynctics*1000.0);
			drawsprite(&spr[i]);
		}
		else //Draw spheres when KV6 object doesn't exist
		{
			drawspherefill(spr[i].p.x,spr[i].p.y,spr[i].p.z,-2,0xff00ff);
			if ((spr[i].voxnum) && (spr[i].voxnum->namoff))
				if (!strcmp(getkfilname(spr[i].voxnum->namoff),"BOX"))
				{
					ox = spr[i].s.x; oy = spr[i].s.y; oz = spr[i].s.z;
					dx = spr[i].h.x; dy = spr[i].h.y; dz = spr[i].h.z;
					if (i == curspri)
					{
						k = (labs(((totclk-(i<<5))&127)-64)>>1)+32;
						k = k*0x10101+0x80006000;
					} else k = 0x208020;
					drawline3d(ox,oy,oz,dx,oy,oz,k);
					drawline3d(ox,dy,oz,dx,dy,oz,k);
					drawline3d(ox,oy,dz,dx,oy,dz,k);
					drawline3d(ox,dy,dz,dx,dy,dz,k);
					drawline3d(ox,oy,oz,ox,dy,oz,k);
					drawline3d(dx,oy,oz,dx,dy,oz,k);
					drawline3d(ox,oy,dz,ox,dy,dz,k);
					drawline3d(dx,oy,dz,dx,dy,dz,k);
					drawline3d(ox,oy,oz,ox,oy,dz,k);
					drawline3d(dx,oy,oz,dx,oy,dz,k);
					drawline3d(ox,dy,oz,ox,dy,dz,k);
					drawline3d(dx,dy,oz,dx,dy,dz,k);
				}
		}

		if (sortdist[i] < 64*64)
			if (project2d(spr[i].p.x,spr[i].p.y,spr[i].p.z,&ox,&oy,&oz))
			{
				v = &sxlbuf[sxlind[i]]; j = strlen(v);
				x = ((long)ox)-(j<<1); y = ((long)oy)-3;
				if (((unsigned long)x <= xres-(j<<2)) && ((unsigned long)y <= yres-6))
					print4x6(x,y,0xffffff,-1,"%s",v);
			}
	}

#if 0 //sprhitscan test code (ENABLE ONLY FOR TESTING!!!)
	for(y=rand()%5;y<yres;y+=5)
		for(x=rand()%5;x<xres;x+=5)
		{
			ox = (double)x-vx5hx; oy = (double)y-vx5hy;
			dp.x = ox*istr.x + oy*ihei.x + vx5hz*ifor.x;
			dp.y = ox*istr.y + oy*ihei.y + vx5hz*ifor.y;
			dp.z = ox*istr.z + oy*ihei.z + vx5hz*ifor.z;

				//NOTE: hitscan is MUCH slower than ALL sprhitscan's!
			hitscan(&ipos,&dp,&hit,&hind,&hdir);
			if (hind)
			{
				f = ((hit.x-ipos.x)*dp.x + (hit.y-ipos.y)*dp.y + (hit.z-ipos.z)*dp.z) /
					  (dp.x*dp.x + dp.y*dp.y + dp.z*dp.z);
			}
			else f = 1e32;

			j = -1;
			for(i=numsprites-1;i>0;i--)
			{
				sprhitscan(&ipos,&dp,&spr[i],&lp,&kp,&f);
				if (kp) j = i;
			}
			if (j >= 0) *(long *)(y*bytesperline+(x<<2)+frameplace) = j*0x638374+0x574623;
		}
	hitscan(&ipos,&ifor,&hit,&hind,&hdir); //Restore original hitscan values!
#endif

		//Draw hanging lights
	if (vx5.numlights > 0)
	{
		if (!klight)
		{
			for(i=vx5.numlights-1;i>=0;i--)
				drawspherefill(vx5.lightsrc[i].p.x,vx5.lightsrc[i].p.y,vx5.lightsrc[i].p.z,-2,0xffff00);
		}
		else
		{
			tempspr.voxnum = klight;
			tempspr.flags = 1;
			tempspr.s.x = .25; tempspr.s.y = 0; tempspr.s.z = 0;
			tempspr.h.x = 0; tempspr.h.y = .25; tempspr.h.z = 0;
			tempspr.f.x = 0; tempspr.f.y = 0; tempspr.f.z = .25;
			for(i=vx5.numlights-1;i>=0;i--)
				{ tempspr.p = vx5.lightsrc[i].p; drawsprite(&tempspr); }
		}
	}

	if ((gridmode) && ((unsigned long)editcurs < numcurs))
	{
		i = gridmode; j = MAX(gridmode*3,64);
		k = (labs(((totclk-(i<<5))&255)-128)>>1)+64;
		ox = curs[editcurs].x; oy = curs[editcurs].y; oz = curs[editcurs].z;
		ftol(ox,&xx); ftol(oy,&yy); ftol(oz,&zz);

		if (cursaxmov == 3)
		{
			for(x=(xx-j)&(~(i-1));x<=xx+j;x+=i)
			{
				f = (float)(j*j - (x-ox)*(x-ox)); if (f <= 0) continue;
				f = sqrt(f); drawline3d(x,oy-f,oz,x,oy+f,oz,k);
			}
			for(y=(yy-j)&(~(i-1));y<=yy+j;y+=i)
			{
				f = (float)(j*j - (y-oy)*(y-oy)); if (f <= 0) continue;
				f = sqrt(f); drawline3d(ox-f,y,oz,ox+f,y,oz,k);
			}
		}
		else
		{
			dx = ox-ipos.x; dy = oy-ipos.y;
			f = dx*dx + dy*dy;
			if (f > 0)
			{
				f = 1.0/sqrt(f); dx *= f; dy *= f;
				for(z=(zz-j)&(~(i-1));z<=zz+j;z+=i)
				{
					f = (float)(j*j - (z-oz)*(z-oz)); if (f <= 0) continue;
					f = sqrt(f);
					drawline3d(ox-dy*f,oy+dx*f,z,ox+dy*f,oy-dx*f,z,k);
				}
			}
		}
	}

	if (numcurs >= 3)
	{
		dx = dy = dz = 0; j = 0; k = 1;
		for(i=2;i>=0;i--)
		{
			dx += (curs[i].y-curs[j].y)*(curs[k].z-curs[j].z) - (curs[i].z-curs[j].z)*(curs[k].y-curs[j].y);
			dy += (curs[i].z-curs[j].z)*(curs[k].x-curs[j].x) - (curs[i].x-curs[j].x)*(curs[k].z-curs[j].z);
			dz += (curs[i].x-curs[j].x)*(curs[k].y-curs[j].y) - (curs[i].y-curs[j].y)*(curs[k].x-curs[j].x);
			k = j; j = i;
		}
		t = 1.0 / sqrt(dx*dx + dy*dy + dz*dz);
		cursnorm.x = dx*t; cursnorm.y = dy*t; cursnorm.z = dz*t;
		cursnoff.x = cursnorm.x*vx5.curhei;
		cursnoff.y = cursnorm.y*vx5.curhei;
		cursnoff.z = cursnorm.z*vx5.curhei;
	}
	for(i=0;i<numcurs;i++)
	{
		k = (labs(((totclk-(i<<5))&255)-128)>>1)+64;

		j = i+1; if (j >= numcurs) j = 0;
		ox = curs[i].x; oy = curs[i].y; oz = curs[i].z;

//-------------------------------------------------------
			//Draw vertical ruler to ground
		ftol(ox-.5,&x); ftol(oy-.5,&y); ftol(oz-.5,&z);
		if ((unsigned long)(x|y) < VSID)
		{
			v = sptr[y*VSID+x];
			if ((v[0]) && (z+z > v[2]+v[v[0]*4+3]))
			{
				do
				{
					v += v[0]*4;
				} while ((v[0]) && (z+z > v[2]+v[v[0]*4+3]));
				if (z > v[1]) z = v[1]; //Below ground: don't draw anything
							else z = v[3];
			}
			zz = v[1];
			if (zz < z) { l = z; z = zz; zz = l; }

				//Draw tics on vertical ruler to ground
			if (gridmode) l = gridmode; else l = 8;
			m = z; z = ((z+l-1)&~(l-1));
			for(;z<zz;z+=l)
			{
				if (z-1 > m) drawline3d(ox,oy,m,ox,oy,z-1,k<<(((-(z&l))>>31)&8));
				m = z+1;
			}
			if (zz > m) drawline3d(ox,oy,m,ox,oy,zz,k<<(((-(z&l))>>31)&8));
		}
//-------------------------------------------------------

		dx = curs[j].x; dy = curs[j].y; dz = curs[j].z;
		if (numcurs > 2)
		{
			drawline3d(ox,oy,oz,dx,dy,dz,k);
			if (vx5.curhei)
			{
				drawline3d(ox,oy,oz,ox+cursnoff.x,oy+cursnoff.y,oz+cursnoff.z,k);
				drawline3d(ox+cursnoff.x,oy+cursnoff.y,oz+cursnoff.z,dx+cursnoff.x,dy+cursnoff.y,dz+cursnoff.z,k);
			}
		}
		else if (numcurs == 2)
		{
			if (ox > dx) { t = ox; ox = dx; dx = t; }
			if (oy > dy) { t = oy; oy = dy; dy = t; }
			if (oz > dz) { t = oz; oz = dz; dz = t; }
			ftol(ox-.5,&x); ox = (float)x; ftol(dx+.5,&x); dx = (float)x;
			ftol(oy-.5,&y); oy = (float)y; ftol(dy+.5,&y); dy = (float)y;
			ftol(oz-.5,&z); oz = (float)z; ftol(dz+.5,&z); dz = (float)z;
			drawline3d(ox,oy,oz,dx,oy,oz,k);
			drawline3d(ox,dy,oz,dx,dy,oz,k);
			drawline3d(ox,oy,dz,dx,oy,dz,k);
			drawline3d(ox,dy,dz,dx,dy,dz,k);
			drawline3d(ox,oy,oz,ox,dy,oz,k);
			drawline3d(dx,oy,oz,dx,dy,oz,k);
			drawline3d(ox,oy,dz,ox,dy,dz,k);
			drawline3d(dx,oy,dz,dx,dy,dz,k);
			drawline3d(ox,oy,oz,ox,oy,dz,k);
			drawline3d(dx,oy,oz,dx,oy,dz,k);
			drawline3d(ox,dy,oz,ox,dy,dz,k);
			drawline3d(dx,dy,oz,dx,dy,dz,k);
		}

		dx = curs[i].x-ipos.x;
		dy = curs[i].y-ipos.y;
		dz = curs[i].z-ipos.z;
		oz = dx*ifor.x + dy*ifor.y + dz*ifor.z; if (oz <= SCISSORDIST) continue;
		ox = dx*istr.x + dy*istr.y + dz*istr.z;
		oy = dx*ihei.x + dy*ihei.y + dz*ihei.z;

		f = vx5hz / oz;
		ftol(oy*f + vx5hy,&y); if ((unsigned long)y >= (unsigned long)yres) continue;
		ftol(ox*f + vx5hx,&x); if ((unsigned long)x >= (unsigned long)xres) continue;

		if (i != lastcurs) k = 64;
		drawspherefill(curs[i].x,curs[i].y,curs[i].z,2,k*0x010101+0x204060);

		ftol(curs[i].x-.5,&xx);
		ftol(curs[i].y-.5,&yy);
		ftol(curs[i].z-.5,&zz);
		sprintf(snotbuf,"%d",i); j = strlen(snotbuf);
		if ((x-(j<<1) >= 0) && (x+(j<<1) < xres) && (y-3 >= 0) && (y+4 < yres))
			print4x6(x-(j<<1),y-2,k*0x010101+0x6080a0,k*0x010101+0x204060,"%s",snotbuf);
	}
	if (((unsigned long)editcurs < numcurs) && (editcurs < 3))
	{
		k = ((labs((totclk&255)-128)+64)<<16);

		ox = curs[editcurs].x; oy = curs[editcurs].y; oz = curs[editcurs].z;
		if (cursaxmov&1)
		{
			drawline3d(ox-3,oy,oz,ox+3,oy,oz,k);
			drawline3d(ox-3,oy,oz,ox-2,oy+1,oz,k);
			drawline3d(ox-3,oy,oz,ox-2,oy-1,oz,k);
			drawline3d(ox+3,oy,oz,ox+2,oy+1,oz,k);
			drawline3d(ox+3,oy,oz,ox+2,oy-1,oz,k);
		}
		if (cursaxmov&2)
		{
			drawline3d(ox,oy-3,oz,ox,oy+3,oz,k);
			drawline3d(ox,oy-3,oz,ox+1,oy-2,oz,k);
			drawline3d(ox,oy-3,oz,ox-1,oy-2,oz,k);
			drawline3d(ox,oy+3,oz,ox+1,oy+2,oz,k);
			drawline3d(ox,oy+3,oz,ox-1,oy+2,oz,k);
		}
		if (cursaxmov&4)
		{     //<dx,dy,dz> = <0,0,1> cross <ox-ipos.x,oy-ipos.y,oz-ipos.z>
			dx = oy-ipos.y; dy = ox-ipos.x; f = dx*dx+dy*dy;
#if ((PENTIUM3 == 1) && (__WATCOMC__))
			rsqrtss(&f);
#else
			f = 1.0/sqrt(f);
#endif
			dx *= f; dy *= f;
			drawline3d(ox,oy,oz-3,ox,oy,oz+3,k);
			drawline3d(ox,oy,oz-3,ox-dx,oy+dy,oz-2,k);
			drawline3d(ox,oy,oz-3,ox+dx,oy-dy,oz-2,k);
			drawline3d(ox,oy,oz+3,ox-dx,oy+dy,oz+2,k);
			drawline3d(ox,oy,oz+3,ox+dx,oy-dy,oz+2,k);
		}
	}
	if ((backedup >= 0) && ((totclk>>3)&8))
	{
		k = ((labs(((totclk-(i<<5))&255)-128)>>1)+64)*0x010101+0x503070;
		if (tedit)
		{
			if (backtag == SETREC)
			{
				for(z=1;z>=0;z--)
					for(y=1;y>=0;y--)
						for(x=1;x>=0;x--)
						{
							//drawspherefill((float)thit[x].x+.5,(float)thit[y].y+.5,(float)thit[z].z+.5,1,k);
							drawarrows(thit[x].x,thit[y].y,thit[z].z,ifor.x,ifor.y,k|0x1000000);
						}
			}
			else if ((backtag == SETELL) || (backtag == SETCYL))
			{
				for(z=1;z>=0;z--)
				{
					//drawspherefill((float)thit[z].x+.5,(float)thit[z].y+.5,(float)thit[z].z+.5,1,k);
					drawarrows(thit[z].x,thit[z].y,thit[z].z,ifor.x,ifor.y,k|0x1000000);
				}
			}
			else
			{
				//drawspherefill((float)thit[0].x+.5,(float)thit[0].y+.5,(float)thit[0].z+.5,1,k);
				drawarrows(thit[0].x,thit[0].y,thit[0].z,ifor.x,ifor.y,k|0x1000000);
			}
		}
		else
		{
			//drawspherefill((float)thit[0].x+.5,(float)thit[0].y+.5,(float)thit[0].z+.5,1,k);
			drawarrows(thit[0].x,thit[0].y,thit[0].z,ifor.x,ifor.y,k|0x1000000);
		}
	}

	//for(i=vx5.cliphitnum-1;i>=0;i--)
	//   drawspherefill(vx5.cliphit[i].x,vx5.cliphit[i].y,vx5.cliphit[i].z,.125,0x3fff7f-i*0x104020);

	if (colselectmode)
	{
		drawcolorbar();
		drawpoint2d(cmousx,cmousy,(rand()<<8)^rand());
	}
	else
	{
		if (hind) *hind = bakcol;
		drawpoint2d(xres>>1,yres>>1,rand());
	}

	//--------------------------------

	odtotclk = dtotclk; readklock(&dtotclk);
	ftol(dtotclk*1000.0,&totclk); fsynctics = (float)(dtotclk-odtotclk);
	ftol(1000.0/fsynctics,&i);

	frameval[numframes&(AVERAGEFRAMES-1)] = i;
		//Print MAX FRAME RATE
	i = frameval[0];
	for(j=AVERAGEFRAMES-1;j>0;j--) i = MAX(i,frameval[j]);
	averagefps = ((averagefps*3+i)>>2);
	print4x6(0,0,0xc0c0c0,-1,"%.1f (%0.2fms)",(float)averagefps*.001,1000000.0/MAX((float)averagefps,1));

	if ((unsigned long)lastcurs < numcurs)
		print4x6(0L,8,0xc0c0c0,-1,"(%.2f,%.2f,%.2f)",curs[lastcurs].x,curs[lastcurs].y,curs[lastcurs].z);
	else if (hind)
		print4x6(0L,8,0xc0c0c0,-1,"(%ld,%ld,%ld)",hit.x,hit.y,hit.z);

	//print4x6(0,24,0xffffff,-1,"Pos:(%.8f, %.8f, %.8f)",ipos.x,ipos.y,ipos.z);
	//print4x6(0,32,0xffffff,-1,"Vec:(%.8f, %.8f, %.8f)",ivel.x,ivel.y,ivel.z);
	//print4x6(0,40,0xffffff,-1,"MaxCliprad:%.8f",findmaxcr(ipos.x,ipos.y,ipos.z,CLIPRAD));

		//Show last message
	if (totclk < messagetimeout)
	{
		j = strlen(message);
		print6x8((((xres-15*4)-j*6)>>1)+15*(4>>1),0,0xc0c0c0,0x404040,"%s",message);
	}

	if (notepadmode >= 0) notepaddraw();

	if (helpmode&1)
#if (USETDHELP != 0)
		kbdhelp_draw(frameplace, bytesperline, xres, yres, fsynctics);
#else
		helpdraw();
#endif
	else print6x8((xres-(47*6))>>1,yres-8,0xc0c0c0,-1,"VOXlap EDitor by Ken Silverman (advsys.net/ken)");

	readkeyboard();
	if (keystatus[0x58]) //F12
	{
		FILE *fil = 0;
		do
		{
			if (fil) fclose(fil);
			sprintf(snotbuf,"VXL5%04d.PNG",capturecount);
			fil = fopen(snotbuf,"rb");
			if (fil) capturecount++;
		} while (fil);
		keystatus[0x58] = 0;
		if (keystatus[0x38]|keystatus[0xb8])
			{ if (!surroundcapture32bit(&ipos,snotbuf,512)) capturecount++; }
		else
			{ if (!screencapture32bit(snotbuf)) capturecount++; }
	}

#if (STEREOMODE == 1)
	i = (yres-1)*bytesperline+frameplace;
	if (numframes&1)
	{
		clearbuf((void *)i,xres>>2,0xffffff);
		clearbuf((void *)(i+xres),(xres*3)>>2,0);
	}
	else
	{
		clearbuf((void *)i,(xres*3)>>2,0xffffff);
		clearbuf((void *)(i+xres*3),xres>>2,0);
	}
#endif

	//print6x8(16,64,-1,-1,"%d",vx5.globalmass);

	stopdirectdraw();
	nextpage();
#if (STEREOMODE == 2)
#ifdef _WIN32
	i = ((numframes&1^1)<<2)^0xff;
	#ifdef __GNUC__ //gcc inline asm
	__asm__
	{
		"mov $0x378, %dx\n"
		"movb i, %al\n"
		"out %al, %dx\n"
	}
	#endif
	#ifdef _MSC_VER //msvc inline asm
	_asm
	{
		mov	dx, 0x378
		mov	al, byte ptr i
		out	dx, al
	}
	#endif
#endif
#endif
	numframes++;
	//----------------------------------------------------------

	if (notepadmode >= 0) { if (notepadinput()) return; }

	ivel.x = ivel.y = ivel.z = 0;
	if (!(helpmode&1))
	{
			//Keyboard controls
		if (keystatus[0xcb]) { ivel.x -= istr.x; ivel.y -= istr.y; ivel.z -= istr.z; }
		if (keystatus[0xcd]) { ivel.x += istr.x; ivel.y += istr.y; ivel.z += istr.z; }
		if (keystatus[0xc8]) { ivel.x += ifor.x; ivel.y += ifor.y; ivel.z += ifor.z; }
		if (keystatus[0xd0]) { ivel.x -= ifor.x; ivel.y -= ifor.y; ivel.z -= ifor.z; }
		if (keystatus[0x9d]) { ivel.x -= ihei.x; ivel.y -= ihei.y; ivel.z -= ihei.z; } //Rt.Ctrl
		if (keystatus[0x52]) { ivel.x += ihei.x; ivel.y += ihei.y; ivel.z += ihei.z; } //KP0
	}
	else
	{
		if (keystatus[0xc8]) helpypos = MAX(helpypos-(long)(fsynctics*(float)(keystatus[0x36]*16+4-keystatus[0x2a]*3)*32768),0);
		if (keystatus[0xd0]) helpypos += (long)(fsynctics*(float)(keystatus[0x36]*16+4-keystatus[0x2a]*3)*32768);
	}

	if (keystatus[0x37]) //KP*
	{
		f = vx5hz; vx5hz = MIN((1.0+fsynctics)*vx5hz,vx5hx*8);
		if ((f < vx5hx) && (vx5hz >= vx5hx))
			{ vx5hz = vx5hx; keystatus[0x37] = 0; }
	}
	if (keystatus[0xb5]) //KP/
	{
		f = vx5hz; vx5hz = MAX((1.0-fsynctics)*vx5hz,vx5hx*.125);
		if ((f > vx5hx) && (vx5hz <= vx5hx))
			{ vx5hz = vx5hx; keystatus[0xb5] = 0; }
	}
	if (keystatus[0x33]) { keystatus[0x33] = 0; vx5.anginc = MAX(vx5.anginc-1,1); angincmode = 0; } //,
	if (keystatus[0x34]) { keystatus[0x34] = 0; vx5.anginc = MIN(vx5.anginc+1,16); angincmode = 0; } //.
	if (keystatus[0x35]) // / (reset angles, etc...)
	{
		keystatus[0x35] = 0;
		if ((backtag == SETSPR) && ((unsigned long)(curspri-1) < (numsprites-1)) && (backedup >= 0))
		{
			f = spr[curspri].s.x*spr[curspri].s.x + spr[curspri].s.y*spr[curspri].s.y + spr[curspri].s.z*spr[curspri].s.z;
				//This cool trick does exponential quantization (& sqrt for free! :)
			(*(long *)&f) = (((((*(long *)&f)-0x3f000000)>>1)+0x3f800000)&0xff800000);

			t = 1.0/sqrt(spr[curspri].s.x*spr[curspri].s.x + spr[curspri].s.y*spr[curspri].s.y + spr[curspri].s.z*spr[curspri].s.z);
			spr[curspri].s.x *= t; spr[curspri].s.y *= t; spr[curspri].s.z *= t;
			t = 1.0/sqrt(spr[curspri].h.x*spr[curspri].h.x + spr[curspri].h.y*spr[curspri].h.y + spr[curspri].h.z*spr[curspri].h.z);
			spr[curspri].h.x *= t; spr[curspri].h.y *= t; spr[curspri].h.z *= t;
			t = 1.0/sqrt(spr[curspri].f.x*spr[curspri].f.x + spr[curspri].f.y*spr[curspri].f.y + spr[curspri].f.z*spr[curspri].f.z);
			spr[curspri].f.x *= t; spr[curspri].f.y *= t; spr[curspri].f.z *= t;

			t = -1e32; j = 0;
			for(i=0;i<24;i++)
			{
				ox = orthodist(&spr[curspri].s,&spr[curspri].h,&spr[curspri].f,
								  &unitaxis[unitaxlu[i][0]],&unitaxis[unitaxlu[i][1]],&unitaxis[unitaxlu[i][2]]);
				if (ox > t) { t = ox; j = i; }
			}
			spr[curspri].s = unitaxis[unitaxlu[j][0]];
			spr[curspri].h = unitaxis[unitaxlu[j][1]];
			spr[curspri].f = unitaxis[unitaxlu[j][2]];
			spr[curspri].s.x *= f; spr[curspri].s.y *= f; spr[curspri].s.z *= f;
			spr[curspri].h.x *= f; spr[curspri].h.y *= f; spr[curspri].h.z *= f;
			spr[curspri].f.x *= f; spr[curspri].f.y *= f; spr[curspri].f.z *= f;

			voxrestore(); voxredraw();
		}
		else if ((backtag == SETSPH) && (vx5.curpow != 2.0))
		{
			vx5.curpow = 2.0; if (backedup >= 0) { voxrestore(); voxredraw(); }
		}
		else
		{
			vx5hz = vx5hx; vx5.anginc = 1; angincmode = 0;
			numcurs = 0; editcurs = lastcurs = -1;
		}
		pathcnt = -1;
	}
	if (keystatus[0x45]) //Numlock
	{
		keystatus[0x45] = 0;
		if (keystatus[0x1d]|keystatus[0x9d])
		{
			if (keystatus[0x2a]|keystatus[0x36])
			{
				vx5.mipscandist -= (vx5.mipscandist>>2);
				if (vx5.mipscandist < 16) vx5.mipscandist = 16;
			}
			else
			{
				vx5.mipscandist += (vx5.mipscandist>>2);
				if (vx5.mipscandist > 1536) vx5.mipscandist = 1536;
			}
		}
		else
		{
			if (keystatus[0x2a]|keystatus[0x36])
			{
				vx5.maxscandist -= (vx5.maxscandist>>2);
				if (vx5.maxscandist < 64) vx5.maxscandist = 64;
			}
			else
			{
				vx5.maxscandist += (vx5.maxscandist>>2);
				if (vx5.maxscandist > 1536) vx5.maxscandist = 1536;
			}
		}
		sprintf(message,"mip=%d max=%d",vx5.mipscandist,vx5.maxscandist);
		messagetimeout = totclk+4000;
	}

	if (backedup >= 0)
	{
		i = 0; if (gridmode) j = gridmode; else j = (keystatus[0x2a]*3+1)*(keystatus[0x36]*3+1);

		if ((backtag == SETSPR) && ((unsigned long)(curspri-1) < (numsprites-1)))
		{
			if (keystatus[0x49]) //KP9
			{
				tp.z = 0;
				if (fabs(istr.x) > fabs(istr.y))
					  { tp.y = 0; if (istr.x < 0) tp.x = 1; else tp.x = -1; }
				else { tp.x = 0; if (istr.y < 0) tp.y = 1; else tp.y = -1; }
				if (!(keystatus[0x2a]|keystatus[0x36])) { f = PI*.5; keystatus[0x49] = 0; } else f = fsynctics*2.0;
				axisrotate(&spr[curspri].s,&tp,f);
				axisrotate(&spr[curspri].h,&tp,f);
				axisrotate(&spr[curspri].f,&tp,f);
			}
			if (keystatus[0x4d]) //KP6
			{
				tp.z = 0;
				if (fabs(istr.x) > fabs(istr.y))
					  { tp.y = 0; if (istr.x < 0) tp.x = -1; else tp.x = 1; }
				else { tp.x = 0; if (istr.y < 0) tp.y = -1; else tp.y = 1; }
				if (!(keystatus[0x2a]|keystatus[0x36])) { f = PI*.5; keystatus[0x4d] = 0; } else f = fsynctics*2.0;
				axisrotate(&spr[curspri].s,&tp,f);
				axisrotate(&spr[curspri].h,&tp,f);
				axisrotate(&spr[curspri].f,&tp,f);
			}
			if (keystatus[0x47]) //KP7
			{
				tp.x = 0; tp.y = 0; tp.z = -1;
				if (!(keystatus[0x2a]|keystatus[0x36])) { f = PI*.5; keystatus[0x47] = 0; } else f = fsynctics*2.0;
				axisrotate(&spr[curspri].s,&tp,f);
				axisrotate(&spr[curspri].h,&tp,f);
				axisrotate(&spr[curspri].f,&tp,f);
			}
			if (keystatus[0x48]) //KP8
			{
				tp.x = 0; tp.y = 0; tp.z = 1;
				if (!(keystatus[0x2a]|keystatus[0x36])) { f = PI*.5; keystatus[0x48] = 0; } else f = fsynctics*2.0;
				axisrotate(&spr[curspri].s,&tp,f);
				axisrotate(&spr[curspri].h,&tp,f);
				axisrotate(&spr[curspri].f,&tp,f);
			}
		}
		else if (backtag == SETKVX)
		{
			tedit = 1;
			if (keystatus[0x4d]) { keystatus[0x4d] = 0; trot = (trot+8)%48; i = 1; } //KP6
			if (keystatus[0x47]) { keystatus[0x47] = 0; trot ^= 1; i = 1; } //KP7
			if (keystatus[0x48]) { keystatus[0x48] = 0; trot ^= 2; i = 1; } //KP8
			if (keystatus[0x49]) { keystatus[0x49] = 0; trot ^= 4; i = 1; } //KP9
		}
		else if (backtag == SETSPH)
		{
			tedit = 1;
		}
		else
		{
			if (keystatus[0x49]) //KP9
			{
				keystatus[0x49] = 0;
				tedit ^= 1;
				if ((!tedit) && (hind))
				{
					if (backtag == SETREC)
					{
						if ((thit[0].x+thit[1].x < (hit.x<<1)) ^ (thit[1].x<thit[0].x))
							{ k = thit[0].x; thit[0].x = thit[1].x; thit[1].x = k; }
						if ((thit[0].y+thit[1].y < (hit.y<<1)) ^ (thit[1].y<thit[0].y))
							{ k = thit[0].y; thit[0].y = thit[1].y; thit[1].y = k; }
						if ((thit[0].z+thit[1].z < (hit.z<<1)) ^ (thit[1].z<thit[0].z))
							{ k = thit[0].z; thit[0].z = thit[1].z; thit[1].z = k; }
					}
					else if ((backtag == SETELL) || (backtag == SETCYL))
					{
						ox = (float)(thit[0].x-hit.x); dx = (float)(thit[1].x-hit.x);
						oy = (float)(thit[0].y-hit.y); dy = (float)(thit[1].y-hit.y);
						oz = (float)(thit[0].z-hit.z); dz = (float)(thit[1].z-hit.z);
						if (dx*dx+dy*dy+dz*dz < ox*ox+oy*oy+oz*oz)
						{
							k = thit[0].x; thit[0].x = thit[1].x; thit[1].x = k;
							k = thit[0].y; thit[0].y = thit[1].y; thit[1].y = k;
							k = thit[0].z; thit[0].z = thit[1].z; thit[1].z = k;
						}
					}
				}
			}
		}

		getkparrows(&lp);
		if (lp.x|lp.y|lp.z)
			for(i=0;i<=tedit;i++)
			{
				thit[i].x += lp.x*j;
				thit[i].y += lp.y*j;
				thit[i].z += lp.z*j;
			}

		if (keystatus[0x4a]) //KP-
		{
			if (backtag == SETSPR)
			{
				if ((unsigned long)(curspri-1) < (numsprites-1))
				{
					f = MAX(1-fsynctics,.75);
					if (!(keystatus[0x2a]|keystatus[0x36])) { keystatus[0x4a] = 0; f = 0.5; }
					spr[curspri].s.x *= f; spr[curspri].s.y *= f; spr[curspri].s.z *= f;
					spr[curspri].h.x *= f; spr[curspri].h.y *= f; spr[curspri].h.z *= f;
					spr[curspri].f.x *= f; spr[curspri].f.y *= f; spr[curspri].f.z *= f;
				}
			}
			else
			{
				keystatus[0x4a] = 0;
				colfnum--; if (colfnum < 0) colfnum = numcolfunc-1;
			}
			i = 1;
		}
		if (keystatus[0x4e]) //KP+
		{
			if (backtag == SETSPR)
			{
				if ((unsigned long)(curspri-1) < (numsprites-1))
				{
					f = MIN(1+fsynctics,1.25);
					if (!(keystatus[0x2a]|keystatus[0x36])) { keystatus[0x4e] = 0; f = 2.0; }
					spr[curspri].s.x *= f; spr[curspri].s.y *= f; spr[curspri].s.z *= f;
					spr[curspri].h.x *= f; spr[curspri].h.y *= f; spr[curspri].h.z *= f;
					spr[curspri].f.x *= f; spr[curspri].f.y *= f; spr[curspri].f.z *= f;
				}
			}
			else
			{
				keystatus[0x4e] = 0;
				colfnum++; if (colfnum >= numcolfunc) colfnum = 0;
			}
			i = 1;
		}
		if (i) { voxrestore(); voxredraw(); }
	}
	else if ((unsigned long)lastcurs < numcurs)
	{
		if (gridmode) j = gridmode; else j = (keystatus[0x2a]*3+1)*(keystatus[0x36]*3+1);

		if (keystatus[0x49]) //KP9
		{
			keystatus[0x49] = 0;
			lastcurs++; if (lastcurs >= numcurs) lastcurs = 0;
		}

			//Align to center of nearest voxel
		tp = curs[lastcurs];
		ftol(curs[lastcurs].x-.5,&i); curs[lastcurs].x = (float)i+.5;
		ftol(curs[lastcurs].y-.5,&i); curs[lastcurs].y = (float)i+.5;
		ftol(curs[lastcurs].z-.5,&i); curs[lastcurs].z = (float)i+.5;

		getkparrows(&lp);
		curs[lastcurs].x += lp.x*j;
		curs[lastcurs].y += lp.y*j;
		curs[lastcurs].z += lp.z*j;
		if (!iscursgood(1.0)) curs[lastcurs] = tp;
	}

	if ((ivel.x != 0) || (ivel.y != 0) || (ivel.z != 0))
	{
		f = (float)fsynctics * 64.0;
		if (keystatus[0x2a]) f *= 0.25;
		if (keystatus[0x36]) f *= 4.0;
		dp.x = ivel.x*f;
		dp.y = ivel.y*f;
		dp.z = ivel.z*f;
		clipmove(&ipos,&dp,CLIPRAD);
		ftol(ipos.x-.5,&lipos.x);
		ftol(ipos.y-.5,&lipos.y);
		ftol(ipos.z-.5,&lipos.z);
	}

	if (keystatus[0x13]) { keystatus[0x13] = 0; angincmode ^= 1; if (!angincmode) vx5.anginc = 1; }  //R
	if (keystatus[0x1e]) { keystatus[0x1e] = 0; anglemode ^= 1; fixanglemode(); } //A

	if (keystatus[0xe])    //Backspace (delete most recent cursor)
	{
		keystatus[0xe] = 0;
		if (colselectmode) colselectmode = 0; else numcurs = MAX(numcurs-1,0);
	}
	if (keystatus[0x2b])   //\ (undo/redo most recent editing changes)
	{
		keystatus[0x2b] = 0;
		if (colselectmode) colselectmode = 0;
		else if (backedup >= 0) voxrestore(); else voxredraw();
	}

	if (keystatus[0x2d])  //X (paint color permanently)
	{
		voxdontrestore();
		if (hind) *hind = vx5.curcol;
	}
	if ((keystatus[0x0f]) && (hind)) //Tab (lift color)
	{
		if (!colselectmode)
		{
			vx5.curcol = *hind; if (backedup >= 0) { voxrestore(); voxredraw(); }
		}
		else
		{
			i = cub2rgb(cmousx-(xres-CGRAD),cmousy-(yres-CGRAD-4),gbri);
			if (i != -1)
			{
				vx5.curcol = i;
				if (backedup >= 0) { voxrestore(); voxredraw(); }
			}
		}
	}

#if 1
	if ((keystatus[0xc9]) && ((unsigned long)(curspri-1) < (numsprites-1))) //PGUP
	{
		keystatus[0xc9] = 0;

		while ((!isvoxelsolid(thit[0].x,thit[0].y,thit[0].z)) && (thit[0].z > 0)) thit[0].z--;
		f = spr[curspri].voxnum->zpiv;
		thit[0].x += spr[curspri].f.x*f;
		thit[0].y += spr[curspri].f.y*f;
		thit[0].z += spr[curspri].f.z*f;
		spr[curspri].p.x = thit[0].x;
		spr[curspri].p.y = thit[0].y;
		spr[curspri].p.z = thit[0].z;
		backedup = 0;
	}
	if ((keystatus[0xd1]) && ((unsigned long)(curspri-1) < (numsprites-1))) //PGDN
	{
		keystatus[0xd1] = 0;

		while ((!isvoxelsolid(thit[0].x,thit[0].y,thit[0].z)) && (thit[0].z < 255)) thit[0].z++;
		f = (float)spr[curspri].voxnum->zsiz-spr[curspri].voxnum->zpiv;
		thit[0].x -= spr[curspri].f.x*f;
		thit[0].y -= spr[curspri].f.y*f;
		thit[0].z -= spr[curspri].f.z*f;
		spr[curspri].p.x = thit[0].x;
		spr[curspri].p.y = thit[0].y;
		spr[curspri].p.z = thit[0].z;
		backedup = 0;
	}
#else  //PGUP&PGDN now control "vx5.curhei"
	if ((keystatus[0xc9]) && (hind)) //PGUP
	{
		voxbackup(hit.x-vx5.currad,hit.y-vx5.currad,hit.x+vx5.currad,hit.y+vx5.currad,SET???);
		vx5.colfunc = (long (*)(lpoint3d *))colfunclst[colfnum];
		for(y=-vx5.currad;y<vx5.currad;y++)
			for(x=-vx5.currad;x<vx5.currad;x++)
			{
				if ((unsigned long)(hit.x+x-2) >= VSID-4) continue;
				if ((unsigned long)(hit.y+y-2) >= VSID-4) continue;

				i = x*x + y*y; if (i >= vx5.currad*vx5.currad) continue;
				ftol((cos(sqrt((double)i)*PI/32.0)+1.0)*4.0,&i);

				expandstack(hit.x+x,hit.y+y,templongbuf);
				for(z=0;z<MAXZDIM-1;z++)
					if (templongbuf[z] != -1) break;
				while (i > 0)
				{
					if (z <= 0) break;
					templongbuf[z-1] = templongbuf[z];
					z--; i--;
				}
				scum(hit.x+x,hit.y+y,0,MAXZDIM,templongbuf);
			}
		scumfinish();
	}
	if ((keystatus[0xd1]) && (hind)) //PGDN
	{
		voxbackup(hit.x-vx5.currad,hit.y-vx5.currad,hit.x+vx5.currad,hit.y+vx5.currad,SET???);
		vx5.colfunc = (long (*)(lpoint3d *))colfunclst[colfnum];
		for(y=-vx5.currad;y<vx5.currad;y++)
			for(x=-vx5.currad;x<vx5.currad;x++)
			{
				if ((unsigned long)(hit.x+x-2) >= VSID-4) continue;
				if ((unsigned long)(hit.y+y-2) >= VSID-4) continue;

				i = x*x + y*y; if (i >= vx5.currad*vx5.currad) continue;
				ftol((cos(sqrt((double)i)*PI/32.0)+1.0)*16.0,&i);

				expandstack(hit.x+x,hit.y+y,templongbuf);
				for(z=0;z<MAXZDIM-1;z++)
					if (templongbuf[z] != -1) break;
				j = templongbuf[z];
				while (i > 0)
				{
					if (z >= MAXZDIM-1) break;
					templongbuf[z] = -1;
					z++; i--;
				}
				templongbuf[z] = j;
				scum(hit.x+x,hit.y+y,0,MAXZDIM,templongbuf);
			}
		scumfinish();
	}
#endif

		//dec/inc default sphere radius respectively
	if ((keystatus[0x1a]) && (vx5.currad > 1)) //[
	{
		if ((keystatus[0x1a] == 1) || (keystatus[0x1a] == 255))
		{
			vx5.currad--; if (backedup >= 0) { voxrestore(); voxredraw(); }
		}
		if (keystatus[0x1a])
			keystatus[0x1a] = MIN(keystatus[0x1a]+(long)(fsynctics*1000),255);
	}
	if ((keystatus[0x1b]) && (vx5.currad < 64)) //]
	{
		if ((keystatus[0x1b] == 1) || (keystatus[0x1b] == 255))
		{
			vx5.currad++; if (backedup >= 0) { voxrestore(); voxredraw(); }
		}
		if (keystatus[0x1b])
			keystatus[0x1b] = MIN(keystatus[0x1b]+(long)(fsynctics*1000),255);
	}

	if ((keystatus[0x27]) && (vx5.curpow > .16)) //;
	{
		if ((keystatus[0x27] == 1) || (keystatus[0x27] == 255))
		{
			f = vx5.curpow; if (vx5.curpow > 2.0) vx5.curpow /= 1.05; else vx5.curpow /= 1.025;
			if ((f > 2.0) && (vx5.curpow <= 2.0)) { vx5.curpow = 2.0; keystatus[0x27] = 0; }
			if ((f > 1.0) && (vx5.curpow <= 1.0)) { vx5.curpow = 1.0; keystatus[0x27] = 0; }
			if (backedup >= 0) { voxrestore(); voxredraw(); }
		}
		if (keystatus[0x27])
			keystatus[0x27] = MIN(keystatus[0x27]+(long)(fsynctics*1000),255);
	}
	if ((keystatus[0x28]) && (vx5.curpow < 16.0)) //;
	{
		if ((keystatus[0x28] == 1) || (keystatus[0x28] == 255))
		{
			f = vx5.curpow; if (vx5.curpow > 2.0) vx5.curpow *= 1.05; else vx5.curpow *= 1.025;
			if ((f < 2.0) && (vx5.curpow >= 2.0)) { vx5.curpow = 2.0; keystatus[0x28] = 0; }
			if ((f < 1.0) && (vx5.curpow >= 1.0)) { vx5.curpow = 1.0; keystatus[0x28] = 0; }
			if (backedup >= 0) { voxrestore(); voxredraw(); }
		}
		if (keystatus[0x28])
			keystatus[0x28] = MIN(keystatus[0x28]+(long)(fsynctics*1000),255);
	}

	if ((keystatus[0xc9]) && ((unsigned long)(curspri-1) >= (numsprites-1)))  //PGUP
	{
		if ((keystatus[0x38]|keystatus[0xb8]) && (numcurs >= 3))
		{
			keystatus[0xc9] = 0; //swap ceiling&floor
			for(i=0;i<numcurs;i++)
			{
				curs[i].x += cursnoff.x;
				curs[i].y += cursnoff.y;
				curs[i].z += cursnoff.z;
			}
			vx5.curhei = -vx5.curhei;
			cursnoff.x = -cursnoff.x;
			cursnoff.y = -cursnoff.y;
			cursnoff.z = -cursnoff.z;
			if (backedup >= 0) { voxrestore(); voxredraw(); }
		}
		else
		{
			if ((keystatus[0xc9] == 1) || (keystatus[0xc9] == 255))
			{
				j = keystatus[0x2a]*3+1;
				if (numcurs == 2) curs[1].z = MAX(curs[1].z-j,0);
				else if (numcurs > 2)
				{
					vx5.curhei = MIN(vx5.curhei+j,MAXZDIM-1);
					cursnoff.x = cursnorm.x*vx5.curhei;
					cursnoff.y = cursnorm.y*vx5.curhei;
					cursnoff.z = cursnorm.z*vx5.curhei;
				}
				if (backedup >= 0) { voxrestore(); voxredraw(); }
			}
			keystatus[0xc9] = MIN(keystatus[0xc9]+(long)(fsynctics*1000),255);
		}
	}
	if ((keystatus[0xd1]) && ((unsigned long)(curspri-1) >= (numsprites-1)))  //PGDN
	{
		if ((keystatus[0x38]|keystatus[0xb8]) && (numcurs >= 3))
		{
			keystatus[0xd1] = 0;
			for(i=0;i<(numcurs>>1);i++) //reverse point order around 0
			{
				tempoint[0] = curs[i];
				curs[i] = curs[numcurs-1-i];
				curs[numcurs-1-i] = tempoint[0];
			}

			tempoint[1] = curs[numcurs-1];  //Rotate point list by 2
			tempoint[0] = curs[numcurs-2];
			for(i=numcurs-3;i>=0;i--) curs[i+2] = curs[i];
			curs[1] = tempoint[1];
			curs[0] = tempoint[0];

			for(i=0;i<numcurs;i++) //also swap ceil&flor
			{
				curs[i].x += cursnoff.x;
				curs[i].y += cursnoff.y;
				curs[i].z += cursnoff.z;
			}
			cursnorm.x = -cursnorm.x;
			cursnorm.y = -cursnorm.y;
			cursnorm.z = -cursnorm.z;
			cursnoff.x = -cursnoff.x;
			cursnoff.y = -cursnoff.y;
			cursnoff.z = -cursnoff.z;
			if (backedup >= 0) { voxrestore(); voxredraw(); }
		}
		else
		{
			if ((keystatus[0xd1] == 1) || (keystatus[0xd1] == 255))
			{
				j = keystatus[0x2a]*3+1;
				if (numcurs == 2) curs[1].z = MIN(curs[1].z+j,MAXZDIM-1);
				else if (numcurs > 2)
				{
					vx5.curhei = MAX(vx5.curhei-j,-(MAXZDIM-1));
					cursnoff.x = cursnorm.x*vx5.curhei;
					cursnoff.y = cursnorm.y*vx5.curhei;
					cursnoff.z = cursnorm.z*vx5.curhei;
				}
				if (backedup >= 0) { voxrestore(); voxredraw(); }
			}
			keystatus[0xd1] = MIN(keystatus[0xd1]+(long)(fsynctics*1000),255);
		}
	}

	if ((keystatus[0xc7]) && ((hind) || (numcurs >= 1))) //Home
	{
		if (keystatus[0x38]|keystatus[0xb8])
		{
			if (hind)
			{
				voxfindspray(hit.x,hit.y,hit.z,&x,&y,&z);
				setcube(x,y,z,-2);
			}
		}
		else
		{
			keystatus[0xc7] = 0; tsolid = 0;
			if (numcurs == 1)
			{
				ftol(curs[0].x-.5,&thit[0].x);
				ftol(curs[0].y-.5,&thit[0].y);
				ftol(curs[0].z-.5,&thit[0].z);
				numcurs = 0;
			}
			else { hind = 0; thit[0].x = hit.x; thit[0].y = hit.y; thit[0].z = hit.z; }
			backtag = SETSPH; voxredraw();
		}
	}
	if ((keystatus[0xcf]) && ((hind) || (numcurs >= 1))) //End
	{
		if (keystatus[0x38]|keystatus[0xb8])
		{
			if (hind)
			{
				voxfindsuck(hit.x,hit.y,hit.z,&x,&y,&z);
				setcube(x,y,z,-1);
			}
		}
		else
		{
			keystatus[0xcf] = 0; tsolid = -1;
			if (numcurs >= 1)
			{
				ftol(curs[0].x-.5,&thit[0].x);
				ftol(curs[0].y-.5,&thit[0].y);
				ftol(curs[0].z-.5,&thit[0].z);
				numcurs = 0;
			}
			else { hind = 0; thit[0].x = hit.x; thit[0].y = hit.y; thit[0].z = hit.z; }
			backtag = SETSPH; voxredraw();
		}
	}

	if ((keystatus[0x12]) && ((hind) || (numcurs >= 2))) //E (draw Ellipsoid)
	{
		keystatus[0x12] = 0; tsolid = -((long)keystatus[0x2a]);
		if (numcurs >= 2)
		{
			for(i=0;i<2;i++)
			{
				ftol(curs[i].x-.5,&thit[1-i].x);
				ftol(curs[i].y-.5,&thit[1-i].y);
				ftol(curs[i].z-.5,&thit[1-i].z);
			}
			numcurs = 0;
		}
		else
		{
			hind = 0; thit[0].x = hit.x; thit[0].y = hit.y; thit[0].z = hit.z;
			thit[1].x = hit.x; thit[1].y = hit.y; thit[1].z = hit.z;
		}
		tedit = 0; backtag = SETELL; voxredraw();
	}

	if ((keystatus[0x26]) && ((hind) || (numcurs >= 2))) //L (draw cyLinder/Line)
	{
		keystatus[0x26] = 0; tsolid = -((long)keystatus[0x2a]);
		if (numcurs >= 2)
		{
			for(i=0;i<2;i++)
			{
				ftol(curs[i].x-.5,&thit[1-i].x);
				ftol(curs[i].y-.5,&thit[1-i].y);
				ftol(curs[i].z-.5,&thit[1-i].z);
			}
			numcurs = 0;
		}
		else
		{
			hind = 0; thit[0].x = hit.x; thit[0].y = hit.y; thit[0].z = hit.z;
			thit[1].x = hit.x; thit[1].y = hit.y; thit[1].z = hit.z;
		}
		tedit = 0; backtag = SETCYL; voxredraw();
	}

	if ((keystatus[0x23]) && ((hind) || (numcurs >= 1))) //H (draw 3D convex hull)
	{
		keystatus[0x23] = 0; tsolid = -((long)keystatus[0x2a]);
		if (numcurs >= 1)
		{
			ftol(curs[0].x-.5,&thit[0].x);
			ftol(curs[0].y-.5,&thit[0].y);
			ftol(curs[0].z-.5,&thit[0].z);
			numcurs = 0;
		}
		else { hind = 0; thit[0].x = hit.x; thit[0].y = hit.y; thit[0].z = hit.z; }
		backtag = SETHUL; voxredraw();
	}

	if ((keystatus[0x11]) && (numcurs >= 2)) //W (draw wall)
	{
		keystatus[0x11] = 0; tsolid = -((long)keystatus[0x2a]);
		backtag = SETWAL; voxredraw();
	}

	if ((keystatus[0x2e]) && (numcurs >= 3)) //C (draw ceiling)
	{
		keystatus[0x2e] = 0; tsolid = -((long)keystatus[0x2a]);
		backtag = SETCEI; voxredraw();
	}

	if ((keystatus[0x14]) && (numcurs >= 3)) //T (turn - lathe tool)
	{
		keystatus[0x14] = 0; tsolid = -((long)keystatus[0x2a]);
		backtag = SETLAT; voxredraw();
	}

	if ((keystatus[0x30]) && (numcurs >= 1)) //B (blobs)
	{
		keystatus[0x30] = 0; tsolid = -((long)keystatus[0x2a]);
		backtag = SETBLO; voxredraw();
	}

	if (keystatus[0xd2]) //Insert
	{
		keystatus[0xd2] = 0; tsolid = 0;
		if ((unsigned long)(curspri-1) < (numsprites-1))
		{
			vx5.colfunc = kv6colfunc;
			if (keystatus[0x2a]|keystatus[0x36])
				setkv6(&spr[curspri],-1);
			else
				setkv6(&spr[curspri],0);

			deletesprite(curspri); curspri = -1;

			if ((backedup >= 0) && (backtag == SETSPR)) //Deselect sprite
				backedup = -1;
		}
		else if (numcurs == 2)
		{
			for(i=0;i<2;i++)
			{
				ftol(curs[i].x-.5,&thit[1-i].x);
				ftol(curs[i].y-.5,&thit[1-i].y);
				ftol(curs[i].z-.5,&thit[1-i].z);
			}
			numcurs = 0;
			backtag = SETREC; voxredraw();
		}
		else if (numcurs > 2)
		{
			backtag = SETSEC; voxredraw();
		}
		else
		{
			if (hind)
			{
				x = hit.x; y = hit.y; z = hit.z;
				switch (hdir)
				{
					case 0: x--; break;
					case 1: x++; break;
					case 2: y--; break;
					case 3: y++; break;
					case 4: z--; break;
					case 5: z++; break;
				}
					//This case lets you expand the cube into a rectangle :)
				thit[0].x = thit[1].x = x;
				thit[0].y = thit[1].y = y;
				thit[0].z = thit[1].z = z;
				tedit = 0; backtag = SETREC; voxredraw();
				hind = 0;
			}
		}
	}
	if (keystatus[0xd3]) //Delete
	{
		keystatus[0xd3] = 0; tsolid = -1;
		if ((unsigned long)(curspri-1) < (numsprites-1))
		{
			deletesprite(curspri); curspri = -1;

			if ((backedup >= 0) && (backtag == SETSPR)) //Deselect sprite
				backedup = -1;
		}
		else if (numcurs == 2)
		{
			for(i=0;i<2;i++)
			{
				ftol(curs[i].x-.5,&thit[1-i].x);
				ftol(curs[i].y-.5,&thit[1-i].y);
				ftol(curs[i].z-.5,&thit[1-i].z);
			}
			numcurs = 0;
			backtag = SETREC; voxredraw();
		}
		else if (numcurs > 2)
		{
			backtag = SETSEC; voxredraw();
		}
		else
		{
			if (hind)
			{
				thit[0].x = thit[1].x = hit.x;
				thit[0].y = thit[1].y = hit.y;
				thit[0].z = thit[1].z = hit.z;
				tedit = 0; backtag = SETREC; voxredraw();
				hind = 0;
			}
		}
	}

	if (keystatus[0x2])  //1 (shoot flash bullet: useless!)
	{
		keystatus[0x2] = 0;
		voxdontrestore();
		for(i=MAXBUL-1;i>=0;i--)
			if (bulactive[i] < 0)
				{ bul[i] = ipos; bulvel[i] = ifor; bulactive[i] = 0; break; }
	}
	if (keystatus[0x3]) //2 (for debugging... remove this!)
	{
		keystatus[0x3] = 0;
		if (numcurs >= 3)
			if (triscan(&curs[0],&curs[1],&curs[2],&curs[3],&lp))
			{
				point2[0] = 1; point2[1] = 2; point2[2] = 3; point2[3] = 0;
				numcurs = 4;
			}
	}
	if (keystatus[0x4]) //3
	{
		keystatus[0x4] = 0;
		if (numcurs >= 3) { backtag = SETTRI; voxredraw(); }
	}

	if (keystatus[0x5]) //4 (floodfill at hit)
	{
		keystatus[0x5] = 0;
		if (hind)
		{
			x = hit.x; y = hit.y; z = hit.z;
			switch (hdir)
			{
				case 0: x--; break;
				case 1: x++; break;
				case 2: y--; break;
				case 3: y++; break;
				case 4: z--; break;
				case 5: z++; break;
			}
			thit[0].x = x; thit[0].y = y; thit[0].z = z;
			backtag = SETFIL; voxredraw();
		}
	}
	if (keystatus[0x6]) //5 (start or draw path to hit)
	{
		keystatus[0x6] = 0;
		if (hind)
		{
			if (pathcnt < 0) { pathcnt = 0; }
			else
			{
				k = findpath(&pathpos[pathcnt],PATHMAXSIZ-pathcnt,&pathlast,&hit);
				for(i=pathcnt;i<pathcnt+k;i++)
				{
					j = pathpos[i];
					pathlast.x = j/(VSID*MAXZDIM);
					pathlast.y = ((j/MAXZDIM)&(VSID-1));
					pathlast.z = (j%MAXZDIM);
					j = getcube(pathlast.x,pathlast.y,pathlast.z);
					if (j&~1) (*(long *)j) = vx5.colfunc(&pathlast);
				}
				pathcnt += k;
			}
			pathlast = hit;
		}
	}
	if ((keystatus[0x7]) && (pathcnt > 0))  //6 (fill path)
	{
		keystatus[0x7] = 0;
#if 0
		for(i=pathcnt-1;i>=0;i--)
		{
			j = pathpos[i];
			x = (j/(VSID*MAXZDIM));
			y = ((j/MAXZDIM)&(VSID-1));
			z = (j&(MAXZDIM-1));

			k = getcube(x,y,z);
			if (k&~1) *(long *)k = 0x80ffffff; else setcube(x,y,z,0x80ffffff);
		}
#elif 0
		l = (rand()*27584621)+1;
		for(i=4096;i;i--)
		{
			j = pathpos[umulshr32(l<<16,pathcnt)];
			k = pathpos[umulshr32(l,pathcnt)]; l = (l*27584621)+1;
			x = (( (j/(VSID*MAXZDIM))     + (k/(VSID*MAXZDIM))     + 1)>>1);
			y = (( ((j/MAXZDIM)&(VSID-1)) + ((k/MAXZDIM)&(VSID-1)) + 1)>>1);
			z = (( (j&(MAXZDIM-1))        + (k&(MAXZDIM-1))        + 1)>>1);
			setcube(x,y,z,-2);
		}
#else
		for(i=pathcnt-1;i>=0;i--) //Just to be safe
			if (pathpos[i] == -1) break;
		if (i < 0)
		{
			i = pathcnt;
			for(l=pathcnt;l>2;l--) //fill it all in with triangles
			{
				do { i--; if (i < 0) i = pathcnt-1; } while (pathpos[i] == -1);
				tempoint[0].x = ((float)(pathpos[i]/(VSID*MAXZDIM)))+.5;
				tempoint[0].y = ((float)((pathpos[i]/MAXZDIM)&(VSID-1)))+.5;
				tempoint[0].z = ((float)(pathpos[i]&(MAXZDIM-1)))+.5;

				do { i--; if (i < 0) i = pathcnt-1; } while (pathpos[i] == -1);
				tempoint[1].x = ((float)(pathpos[i]/(VSID*MAXZDIM)))+.5;
				tempoint[1].y = ((float)((pathpos[i]/MAXZDIM)&(VSID-1)))+.5;
				tempoint[1].z = ((float)(pathpos[i]&(MAXZDIM-1)))+.5;

				pathpos[i] = -1;

				do { i--; if (i < 0) i = pathcnt-1; } while (pathpos[i] == -1);
				tempoint[2].x = ((float)(pathpos[i]/(VSID*MAXZDIM)))+.5;
				tempoint[2].y = ((float)((pathpos[i]/MAXZDIM)&(VSID-1)))+.5;
				tempoint[2].z = ((float)(pathpos[i]&(MAXZDIM-1)))+.5;

				settri(&tempoint[0],&tempoint[1],&tempoint[2],0);
			}
		}
		pathcnt = -1;
#endif
	}
	if (keystatus[0x8])  //7 (hollow fill)
	{
		keystatus[0x8] = 0;
		sethollowfill();
	}

	if (keystatus[0x29]) //` (use cool lighting mode)
	{
		keystatus[0x29] = 0;
		if (!vx5.lightmode) setsideshades(0,0,0,0,0,0);
		vx5.lightmode++; if (vx5.lightmode >= 3) vx5.lightmode = 0;
		if (!vx5.lightmode) setsideshades(0,28,8,24,12,12);
		if (vx5.lightmode)
		{
			voxrestore();
			updatebbox(0,0,0,VSID,VSID,MAXZDIM,0);
			updatevxl();
			voxredraw();
		}
	}

	if (keystatus[0x20]) //D (duplicate)
	{
		keystatus[0x20] = 0;
		if ((numcurs == 2) && (hind))
		{
			thit[0].x = hit.x; thit[0].y = hit.y; thit[0].z = hit.z;
			if (keystatus[0x2a]|keystatus[0x36]) backtag = SETDUQ;
													  else backtag = SETDUP;
			voxredraw(); hind = 0;
		}
	}

	if (keystatus[0xd]) //= (re-draw colfunc)
	{
		keystatus[0xd] = 0;
		if (numcurs == 2) { backtag = SETCOL; voxredraw(); }
	}

	if (keystatus[0x16]) //U (update (add) voxel)
	{
		keystatus[0x16] = 0;
		if (numcurs == 2)
		{
			curspri = insertsprite("BOX","",0);
			if (curspri >= 0)
			{
				spr[curspri].p.x = (float)hit.x;
				spr[curspri].p.y = (float)hit.y;
				spr[curspri].p.z = (float)hit.z;
				spr[curspri].s.x = curs[0].x;
				spr[curspri].s.y = curs[0].y;
				spr[curspri].s.z = curs[0].z;
				spr[curspri].h.x = curs[1].x;
				spr[curspri].h.y = curs[1].y;
				spr[curspri].h.z = curs[1].z;
				spr[curspri].f.x = 0.f;
				spr[curspri].f.y = 0.f;
				spr[curspri].f.z = 0.f;
				backedup = -1; numcurs = 0;
			}
		}
		else if (hind)
		{
			hind = 0;
			//if (v = (char *)fileselect("*.KFA;*.KV6;*.KVX;*.VOX"))
			ddflip2gdi(); if (v = (char *)loadfileselect("Insert voxel object...","KFA, KV6, KVX, VOX\0*.kfa;*.kv6;*.kvx;*.vox\0All files (*.*)\0*.*\0\0","KV6"))
			{
				strcpy(tfilenam,(char *)v);
				i = strlen(tfilenam);
				if ((i > 0) && ((tfilenam[i-1] == '6') || (tfilenam[i-1] == 'A') || (tfilenam[i-1] == 'a')))
				{
					thit[0] = hit;
					curspri = insertsprite(tfilenam,"",0);
					if (curspri >= 0)
					{
						while ((!isvoxelsolid(thit[0].x,thit[0].y,thit[0].z)) && (thit[0].z < 255)) thit[0].z++;
						if (spr[curspri].voxnum) f = (float)spr[curspri].voxnum->zsiz-spr[curspri].voxnum->zpiv;
						else f = 0.f;
						spr[curspri].s.x = spr[curspri].h.y = spr[curspri].f.z = 0.5;
						thit[0].x -= spr[curspri].f.x*f;
						thit[0].y -= spr[curspri].f.y*f;
						thit[0].z -= spr[curspri].f.z*f;
						spr[curspri].p.x = thit[0].x;
						spr[curspri].p.y = thit[0].y;
						spr[curspri].p.z = thit[0].z;
						backedup = 0; backtag = SETSPR;
					} else backtag = 0;
				}
				else
				{
					thit[0].x = hit.x; thit[0].y = hit.y; thit[0].z = hit.z;
					setkvx(tfilenam,thit[0].x,thit[0].y,thit[0].z,trot,SETKVX);
				}
			}
		}
	}

	if (keystatus[0x2f]) //V (select .PNG as procedural texture)
	{
		keystatus[0x2f] = 0;
		//if (v = (char *)fileselect("*.PNG;*.JPG;*.JPE;*.JPEG"))
		ddflip2gdi(); if (v = (char *)loadfileselect("Select texture...","PNG, JPEG\0*.png;*.jpe;*.jpg;*.jpeg\0All files (*.*)\0*.*\0\0","PNG"))
		{
			strcpy(tfilenam,(char *)v);
			if (vx5.pic) { free(vx5.pic); vx5.pic = 0; }
			kpzload(tfilenam,(int *)&i,(int *)&vx5.bpl,(int *)&vx5.xsiz,(int *)&vx5.ysiz); vx5.pic = (long *)i;
			vx5.picmode = 0;
			if (((backtag == SETCYL) || (backtag == SETELL) || (backtag == SETLAT)) && (backedup >= 0))
			{
				if (backtag == SETLAT)
					aligncyltexture(curs[0].x,curs[0].y,curs[0].z,curs[1].x,curs[1].y,curs[1].z);
				else
					aligncyltexture(thit[0].x,thit[0].y,thit[0].z,thit[1].x,thit[1].y,thit[1].z);
			}
			else if ((backtag == SETSPH) && (backedup >= 0))
			{
				vx5.picmode = 2;
				vx5.fpico.x = (float)thit[0].x;
				vx5.fpico.y = (float)thit[0].y;
				vx5.fpico.z = (float)thit[0].z;
				vx5.fpicu.x = istr.x; vx5.fpicu.y = istr.y; vx5.fpicu.z = istr.z;
				vx5.fpicv.x = ifor.x; vx5.fpicv.y = ifor.y; vx5.fpicv.z = ifor.z;
				vx5.fpicw.x = ihei.x; vx5.fpicw.y = ihei.y; vx5.fpicw.z = ihei.z;
				vx5.xoru = vx5.xsiz;
			}
			else if (numcurs >= 3)
			{
				vx5.picmode = 3;
				t = (curs[1].x-curs[0].x)*(curs[1].x-curs[0].x)+
					 (curs[1].y-curs[0].y)*(curs[1].y-curs[0].y)+
					 (curs[1].z-curs[0].z)*(curs[1].z-curs[0].z);
				if (t >= 0)
				{
					t = 1.0/sqrt(t);
					vx5.fpico.x = curs[0].x;
					vx5.fpico.y = curs[0].y;
					vx5.fpico.z = curs[0].z;
					vx5.fpicu.x = (curs[1].x-curs[0].x)*t;
					vx5.fpicu.y = (curs[1].y-curs[0].y)*t;
					vx5.fpicu.z = (curs[1].z-curs[0].z)*t;
					vx5.fpicv.x = vx5.fpicu.y*cursnorm.z - vx5.fpicu.z*cursnorm.y;
					vx5.fpicv.y = vx5.fpicu.z*cursnorm.x - vx5.fpicu.x*cursnorm.z;
					vx5.fpicv.z = vx5.fpicu.x*cursnorm.y - vx5.fpicu.y*cursnorm.x;
				}
				else
					vx5.picmode = 0;
				vx5.xoru = vx5.xsiz;
			}

			if (!vx5.picmode) //Activated when math errors occur in vx5.picmode==1
			{
				if (hind) { vx5.pico.x = hit.x; vx5.pico.y = hit.y; vx5.pico.z = hit.z; }
					  else { vx5.pico.x = lipos.x; vx5.pico.y = lipos.y; vx5.pico.z = lipos.z; }

				vx5.picu.x = vx5.picu.y = vx5.picu.z = vx5.xoru = 0;
				if ((fabs(istr.x) > fabs(istr.y)) && (fabs(istr.x) > fabs(istr.z)))
					  { vx5.picu.x = -1; if (istr.x < 0) { vx5.xoru = -1; vx5.pico.x++; } }
				else if (fabs(istr.y) > fabs(istr.z))
					  { vx5.picu.y = -1; if (istr.y < 0) { vx5.xoru = -1; vx5.pico.y++; } }
				else { vx5.picu.z = -1; if (istr.z < 0) { vx5.xoru = -1; vx5.pico.z++; } }

				vx5.picv.x = vx5.picv.y = vx5.picv.z = vx5.xorv = 0;
				if ((fabs(ihei.x) > fabs(ihei.y)) && (fabs(ihei.x) > fabs(ihei.z)))
					  { vx5.picv.x = -1; if (ihei.x < 0) { vx5.xorv = -1; vx5.pico.x++; } }
				else if (fabs(ihei.y) > fabs(ihei.z))
					  { vx5.picv.y = -1; if (ihei.y < 0) { vx5.xorv = -1; vx5.pico.y++; } }
				else { vx5.picv.z = -1; if (ihei.z < 0) { vx5.xorv = -1; vx5.pico.z++; } }
			}

			for(i=numcolfunc-1;i>=0;i--)
				if ((long (*)(lpoint3d *))colfunclst[i] == pngcolfunc) { colfnum = i; break; }

			if (backedup >= 0) { voxrestore(); voxredraw(); }
		}
	}
	if (keystatus[0x19]) //P (projection paint .PNG onto wall)
	{
		keystatus[0x19] = 0;
		//if (v = (char *)fileselect("*.PNG;*.JPG;*.JPE;*.JPEG"))
		ddflip2gdi(); if (v = (char *)loadfileselect("Select texture to project...","PNG, JPEG\0*.png;*.jpe;*.jpg;*.jpeg\0All files (*.*)\0*.*\0\0","PNG"))
		{
			strcpy(tfilenam,(char *)v);
			if (vx5.pic) { free(vx5.pic); vx5.pic = 0; }
			kpzload(tfilenam,(int *)&i,(int *)&vx5.bpl,(int *)&vx5.xsiz,(int *)&vx5.ysiz); vx5.pic = (long *)i;
			for(y=0;y<(vx5.ysiz<<2);y++)
			{
				dx = (float)(vx5.xsiz<<2); dy = (float)((y<<1)-(vx5.ysiz<<2));
				ox = (ifor.x-istr.x)*dx + ihei.x*dy;
				oy = (ifor.y-istr.y)*dx + ihei.y*dy;
				oz = (ifor.z-istr.z)*dx + ihei.z*dy;
				for(x=0;x<(vx5.xsiz<<2);x++)
				{
					dz = (float)(x<<1);
					dp.x = istr.x*dz + ox;
					dp.y = istr.y*dz + oy;
					dp.z = istr.z*dz + oz;
					hitscan(&ipos,&dp,&hit,&hind,&hdir);
					if (hind) (*hind) = (((*hind)&0xff000000)|(vx5.pic[(y>>2)*(vx5.bpl>>2)+(x>>2)]&0xffffff));
				}
			}
			if (vx5.pic) { free(vx5.pic); vx5.pic = 0; }
			hind = 0;
		}
	}

	if (keystatus[0x3b]) //F1
	{
		keystatus[0x3b] = 0;
		if (helpmode) helpmode = (helpmode&1)+1; //0,1,2,1,2,...
	}

	if (keystatus[0x3c]) //Alt + F2 (Load!)
	{
		keystatus[0x3c] = 0;
		if (keystatus[0x38]|keystatus[0xb8])
		{
#ifndef _WIN32
			loadvxl(vxlfilnam,&ipos,&istr,&ihei,&ifor);
			ftol(ipos.x-.5,&lipos.x);
			ftol(ipos.y-.5,&lipos.y);
			ftol(ipos.z-.5,&lipos.z);
			fixanglemode();
#else
			keystatus[0x38] = keystatus[0xb8] = 0;
			ddflip2gdi(); strcpy(fileselectnam,vxlfilnam);

			wsprintf(snotbuf,"Save .VXL before loading new file?");
			if (MessageBox(ghwnd,snotbuf,"VOXED",MB_YESNO) == IDYES)
				if (v = (char *)savefileselect("SAVE VXL file AS...","*.VXL\0*.vxl\0All files (*.*)\0*.*\0\0","VXL"))
					savevxl(v,&ipos,&istr,&ihei,&ifor);

			wsprintf(snotbuf,"Save .SXL before loading new file?");
			if (MessageBox(ghwnd,snotbuf,"VOXED",MB_YESNO) == IDYES)
				if (v = (char *)savefileselect("SAVE SXL file AS...","*.SXL\0*.sxl\0All files (*.*)\0*.*\0\0","SXL"))
					savesxl(v);

			if (v = (char *)loadfileselect("LOAD SXL/VXL file...","SXL, VXL\0*.sxl;*.vxl\0All files (*.*)\0*.*\0\0","SXL"))
			{
				i = strlen(v);
				if ((i >= 3) && (!strcasecmp(&v[i-4],".sxl")))
				{
					strcpy(tfilenam,skyfilnam);
					if (voxedloadsxl(v))
					{
						strcpy(sxlfilnam,v);
							//Only load sky if different
						if (skyfilnam != tfilenam) loadsky(skyfilnam);
						strcpy(v,vxlfilnam);
					}
				}
				else i = -1;
				if (loadvxl(v,&ipos,&istr,&ihei,&ifor))
				{
					initsxl(); //IS THIS A GOOD IDEA?
					strcpy(vxlfilnam,v);
					ftol(ipos.x-.5,&lipos.x);
					ftol(ipos.y-.5,&lipos.y);
					ftol(ipos.z-.5,&lipos.z);
					fixanglemode();
				}
			}
#endif
		}
	}
	if (keystatus[0x3d]) //Alt + F3 (Save!)
	{
		keystatus[0x3d] = 0;
		if (keystatus[0x38]|keystatus[0xb8])
		{
#ifndef _WIN32
			savevxl(vxlfilnam,&ipos,&istr,&ihei,&ifor);
			//savesxl(sxlfilnam);
#else
			keystatus[0x38] = keystatus[0xb8] = 0;
			ddflip2gdi(); strcpy(fileselectnam,vxlfilnam);
			if (v = (char *)savefileselect("SAVE VXL file AS...","*.VXL\0*.vxl\0All files (*.*)\0*.*\0\0","VXL"))
			{
				strcpy(vxlfilnam,v);
				savevxl(vxlfilnam,&ipos,&istr,&ihei,&ifor);
				//savesxl(sxlfilnam);
			}
#endif
		}
		else
		{
			ddflip2gdi(); strcpy(fileselectnam,sxlfilnam);
			if (v = (char *)savefileselect("SAVE SXL file AS...","*.SXL\0*.sxl\0All files (*.*)\0*.*\0\0","SXL"))
			{
				strcpy(sxlfilnam,v);
				savesxl(sxlfilnam);
			}
		}
	}

	if (keystatus[0x3f]) //F5 (load new sky)
	{
		keystatus[0x3f] = 0;
		//if (v = (char *)fileselect("*.PNG;*.JPG;*.JPE;*.JPEG"))
		ddflip2gdi(); if (v = (char *)loadfileselect("Select sky...","PNG, JPEG\0*.png;*.jpe;*.jpg;*.jpeg\0All files (*.*)\0*.*\0\0",0))
			{ if (!loadsky(v)) strcpy(skyfilnam,v); }
	}

	if (keystatus[0x40]) //F6 (edit global userstring (sprite #0 = dummy))
	{
		keystatus[0x40] = 0;

		curspri = 0;
		while (keyread()); //HACK to flush key buffer!
		notepadmode = sxlind[curspri+1]-sxlind[curspri]-1;
		notepadcurstime = totclk;
		cmousx = (xres>>1); fcmousx = (float)cmousx;
		cmousy = (yres>>1); fcmousy = (float)cmousy;
		return;
	}

	if (keystatus[0x41]) //F7 (save floating object to .KV6)
	{
		keystatus[0x41] = 0;
		if (hind)
		{
			vx5sprite tspr;
			checkfloatinbox(hit.x,hit.y,hit.z,hit.x,hit.y,hit.z);

			startfalls();
			for(i=vx5.flstnum-1;i>=0;i--)
				if (vx5.flstcnt[i].userval == -1) //New piece
					if (meltfall(&tspr,i,0))
					{
						ddflip2gdi(); fileselectnam[0] = 0;
						if (v = (char *)savefileselect("SAVE KV6 file AS...","*.KV6\0*.kv6\0All files (*.*)\0*.*\0\0","KV6"))
							savekv6(v,tspr.voxnum);
						free(tspr.voxnum);
						break;
					}
			finishfalls();
		}
	}

	if (keystatus[0x44]) //F10: mip testing... TEMP HACK!!!
	{
		keystatus[0x44] = 0;
		if (vx5.vxlmipuse > 1)      { vx5.vxlmipuse = 1; vx5.maxscandist = 256; }
		else { vx5.mipscandist = 128; vx5.vxlmipuse = 9; vx5.maxscandist = 1536; }
		genmipvxl(0,0,VSID,VSID);
	}

	if (keystatus[0x1d])  //L.Ctrl
	{
		voxdontrestore();
		photonflash(&ipos,0x01000000,512);
		hind = 0;
	}

	if (keystatus[0x2c])  //Z (average voxel colors only)
	{
		voxdontrestore();
		setsideshades(0,0,0,0,0,0); //Automatically turn off sideshademode
		if (hind) blursphere(&hit,32,keystatus[0x2a]);
	}

	if (keystatus[0x1f])  //S (suck out thin voxels)
	{
		voxdontrestore();
		if (hind)
		{
			vx5.colfunc = (long (*)(lpoint3d *))colfunclst[colfnum];
			suckthinvoxsphere(&hit,32);
			hind = 0;
		}
	}

	if (keystatus[0x53]) //KP. (toggle 3D shading)
	{
		keystatus[0x53] = 0;
		if ((backedup >= 0) && (backtag == SETSPR) && ((unsigned long)(curspri-1) < (numsprites-1)))
		{
			spr[curspri].s.x = -spr[curspri].s.x; //Mirror sprite (RHS <-> LHS)
			spr[curspri].s.y = -spr[curspri].s.y;
			spr[curspri].s.z = -spr[curspri].s.z;
		}
		else
		{
			vx5.sideshademode++; if (vx5.sideshademode >= 2) vx5.sideshademode = 0;
			if (!vx5.sideshademode) setsideshades(0,0,0,0,0,0);
									 else setsideshades(0,28,8,24,12,12);
		}
	}

	if (keystatus[0x3a])  //Caps Lock
	{
		keystatus[0x3a] = 0;
		if (!vx5.lightmode)
		{
			voxdontrestore();
	
			if (keystatus[0x38]|keystatus[0xb8]) flashbrival = (0xff<<24);
													  else flashbrival = (0x10<<24);
	
				//Darken all colors
			x = VSID; y = VSID; i = 0;
			for(j=0;j<gmipnum;j++) { i += x*y; x >>= 1; y >>= 1; }

			for(i--;i>=0;i--)
			{
				for(v=sptr[i];v[0];v+=v[0]*4)
					for(j=1;j<v[0];j++) mmxcolorsub((long *)&v[j<<2]);
				for(j=1;j<=v[2]-v[1]+1;j++) mmxcolorsub((long *)&v[j<<2]);

				//(*(long *)&v[j<<2]) = colormul(*(long *)&v[j<<2],256-64);
			}
			clearMMX();
		}
	}

	if (keystatus[0x22])  //G (grid)
	{
		keystatus[0x22] = 0;
		switch(gridmode)
		{
			case 0:  gridmode = 16; break;
			case 16: gridmode = 32; break;
			case 32: gridmode = 0; break;
		}
	}

	if (keystatus[0x21])  //F
	{
		keystatus[0x21] = 0;

		//if ((backtag != SETFLA) || (lipos.x != thit[0].x) ||
		//                           (lipos.y != thit[0].y) ||
		//                           (lipos.z != thit[0].z))
		//{
		//   voxdontrestore();
		//   thit[0].x = lipos.x; thit[0].y = lipos.y; thit[0].z = lipos.z;
		//   voxbackup(thit[0].x-254,thit[0].y-254,thit[0].x+255,thit[0].y+255,SETFLA);
		//}
		//setflash(thit[0].x,thit[0].y,thit[0].z,254,1024,1);

		if (vx5.lightmode == 2)
		{
				//Lighting for hanging lights:
			if (vx5.numlights < MAXLIGHTS)
			{
				i = 128; //radius to use
				if (!klight) klight = getkv6("kv6/klight.kv6");
				vx5.lightsrc[vx5.numlights].p.x = ipos.x;
				vx5.lightsrc[vx5.numlights].p.y = ipos.y;
				vx5.lightsrc[vx5.numlights].p.z = ipos.z;
				vx5.lightsrc[vx5.numlights].r2 = (float)(i*i);
				vx5.lightsrc[vx5.numlights].sc = 262144.0; //4096.0;
				vx5.numlights++;

				voxrestore();
				updatebbox(lipos.x-i,lipos.y-i,lipos.z-i,lipos.x+i,lipos.y+i,lipos.z+i,0);
				updatevxl();
				voxredraw();
			}
		}
		else
		{
			thit[0].x = lipos.x; thit[0].y = lipos.y; thit[0].z = lipos.z;
			if (!(keystatus[0x38]|keystatus[0xb8])) backtag = SETFFL;
														  else backtag = SETFLA;
			voxredraw();
		}
	}

	//if (keystatus[?]) { } //` (Look at voxel cache graphically)

	obstatus = bstatus; readmouse(&fmousx,&fmousy,&bstatus);

#if (USETDHELP != 0)
	if (helpmode&1)
	{
		if (kbdhelp_update(fmousx, fmousy, bstatus, xres, yres))
		{
			fmousx = fmousy = 0; //Don't allow any other mouse movement
			bstatus = 0;
		}
	}
#endif

	if ((bstatus&1) && (!colselectmode)) //Find floating cursors to grab
	{
		if (!(obstatus&1))
		{
			lastcurs = -1;
			for(i=numcurs-1;i>=0;i--)
			{
				dx = (float)curs[i].x-ipos.x;
				dy = (float)curs[i].y-ipos.y;
				dz = (float)curs[i].z-ipos.z;
				t = dx*ifor.x + dy*ifor.y + dz*ifor.z;
					//Dot product equation squared (tý>³dx,dy,dy³ý*cosý(ang))
				if ((t < SCISSORDIST) || (t*t < (dx*dx+dy*dy+dz*dz)*0.995)) continue;
				lastcurs = i;
				editcurs = i; //cursaxmov = 4;
				curspri = -1; if (backtag == SETSPR) backedup = -1; //deselect sprite
				break;
			}
			if (i < 0) //Mouse cursor not near floating numbered cursor
			{
				curspri = -1; if (backtag == SETSPR) backedup = -1; //deselect sprite
				f = 1e32;
				for(i=numsprites-1;i>0;i--) //NOTE: sprite #0 is dummy
				{
					if ((spr[i].voxnum) && (spr[i].voxnum->numvoxs))
					{
							//lp: coordinate of voxel hit in sprite coordinates (if any)
							//kp: pointer to voxel hit (kv6voxtype) (0 if none hit)
						sprhitscan(&ipos,&ifor,&spr[i],&lp,&kp,&f);
						if (kp) curspri = i;
					}
					else //test spheres when KV6 object doesn't exist
					{
							//does ray(ipos,ifor) hit sphere(spr[i].p),radius=2?
						t = (spr[i].p.x-ipos.x)*ifor.x +
							 (spr[i].p.y-ipos.y)*ifor.y +
							 (spr[i].p.z-ipos.z)*ifor.z;
						if ((t < 0) || (t > f)) continue;
						dx = ipos.x+ifor.x*t;
						dy = ipos.y+ifor.y*t;
						dz = ipos.z+ifor.z*t;
						if ((spr[i].p.x-dx)*(spr[i].p.x-dx) +
							 (spr[i].p.y-dy)*(spr[i].p.y-dy) +
							 (spr[i].p.z-dz)*(spr[i].p.z-dz) > 2*2) continue;
						f = t; curspri = i;
					}
				}
				if (curspri >= 0)
				{
					lastcurs = -1;
					editcurs = -1; backedup = 0; backtag = SETSPR; //select sprite
					thit[0].x = spr[curspri].p.x;
					thit[0].y = spr[curspri].p.y;
					thit[0].z = spr[curspri].p.z;
				}
			}
		}
	}
	if (!(bstatus&1)) editcurs = -1;
	if (((bstatus&2) > (obstatus&2)) && (!colselectmode))
	{
		if (((unsigned long)editcurs < numcurs) && (bstatus&1))
			cursaxmov ^= 7;
		else
		{
			curspri = -1; if (backtag == SETSPR) backedup = -1; //deselect sprite
			f = 1e32;
			for(i=numsprites-1;i>0;i--) //NOTE: sprite #0 is dummy
			{
				if ((spr[i].voxnum) && (spr[i].voxnum->numvoxs))
				{
						//lp: coordinate of voxel hit in sprite coordinates (if any)
						//kp: pointer to voxel hit (kv6voxtype) (0 if none hit)
					sprhitscan(&ipos,&ifor,&spr[i],&lp,&kp,&f);
					if (kp) curspri = i;
				}
				else //test spheres when KV6 object doesn't exist
				{
						//does ray(ipos,ifor) hit sphere(spr[i].p),radius=2?
					t = (spr[i].p.x-ipos.x)*ifor.x +
						 (spr[i].p.y-ipos.y)*ifor.y +
						 (spr[i].p.z-ipos.z)*ifor.z;
					if ((t < 0) || (t > f)) continue;
					dx = ipos.x+ifor.x*t;
					dy = ipos.y+ifor.y*t;
					dz = ipos.z+ifor.z*t;
					if ((spr[i].p.x-dx)*(spr[i].p.x-dx) +
						 (spr[i].p.y-dy)*(spr[i].p.y-dy) +
						 (spr[i].p.z-dz)*(spr[i].p.z-dz) > 2*2) continue;
					f = t; curspri = i;
				}
			}
			if (curspri >= 0)
			{
				editcurs = -1; backedup = 0; backtag = SETSPR; //select sprite
				thit[0].x = spr[curspri].p.x;
				thit[0].y = spr[curspri].p.y;
				thit[0].z = spr[curspri].p.z;

				while (keyread()); //HACK to flush key buffer!
				notepadmode = sxlind[curspri+1]-sxlind[curspri]-1;
				notepadcurstime = totclk;
				cmousx = (xres>>1); fcmousx = (float)cmousx;
				cmousy = (yres>>1); fcmousy = (float)cmousy;
				return;
			}
		}
	}

	if ((keystatus[0x39]) && (numcurs < MAXCURS)) //Space: Insert floating cursor
	{
		keystatus[0x39] = 0;
		if (editcurs == -1) { editcurs = numcurs; lastcurs = numcurs; }
		if (numcurs+(editcurs==numcurs) <= 3)
		{
			if (hind)
			{
				curs[editcurs].x = (float)hit.x+.5;
				curs[editcurs].y = (float)hit.y+.5;
				curs[editcurs].z = (float)hit.z+.5;
				//cursaxmov = (1<<(hdir>>1)); //{0,1,2,3,4,5} -> {1,1,2,2,4,4}
				if (editcurs == numcurs)
				{
					numcurs++;
					if (!iscursgood(1.0))
					{
						if (numcurs == 2) dp.z -= 2e-6; //Push curs[1] up if it's too close to curs[0]
						else if (numcurs == 3) numcurs--; //cancel insert
					}
				}
				vx5.curhei = 0;
			} else editcurs = -1;
		}
		else
		{
			if (gridalignplane(&curs[editcurs],&curs[0],&cursnorm,gridmode))
				{ if (editcurs == numcurs) numcurs++; }
			else editcurs = -1;
		}
		if ((unsigned long)editcurs < numcurs) goto dragfloatcurs;
	}
	if ((bstatus&1) && (!colselectmode) && ((unsigned long)editcurs < numcurs)) //Drag floating cursors
	{
dragfloatcurs:;
		tp = curs[editcurs];
		if (numcurs <= 3) gridalignline(&curs[editcurs],cursaxmov,gridmode);
						 else gridalignplane(&curs[editcurs],&curs[0],&cursnorm,gridmode);
		if (!iscursgood(1.0)) curs[editcurs] = tp;
	}

	if (j = keystatus[0x1c] + ((bstatus&1)<<1))
	{
		keystatus[0x1c] = 0;
		if (!colselectmode) //L.Enter/L.Mouse
		{
			if ((backedup < 0) && (hind)) vx5.curcol = *hind;
			rgb2cub(vx5.curcol,&cmousx,&cmousy,&gbri);
			fcmousx = (float)cmousx; fcmousy = (float)cmousy;
		}
		else if (j&2)
		{
			i = cub2rgb(cmousx-(xres-CGRAD),cmousy-(yres-CGRAD-4),gbri);
			if (i != -1)
			{
				vx5.curcol = i;
				if (backedup >= 0) { voxrestore(); voxredraw(); }
			}
		}
		if (j&1) colselectmode ^= 1;
	}

	if (colselectmode)
	{
		if (!(bstatus&2))
		{
			fcmousx = MIN(MAX(fcmousx+fmousx,xres-CGRAD*2  ),xres-1  );
			fcmousy = MIN(MAX(fcmousy+fmousy,yres-CGRAD*2-4),yres-1-4);
			ftol(fcmousx,&cmousx);
			ftol(fcmousy,&cmousy);
		}
		else
		{
			i = cub2rgb(cmousx-(xres-CGRAD),cmousy-(yres-CGRAD-4),gbri);
			gbri = MIN(MAX(gbri-fmousy*65536,0),(256<<16)-1);
			if (i != -1)
			{
				if (fmousy > 0) i = -1; else i = 1;
				while (cub2rgb(cmousx-(xres-CGRAD),cmousy-(yres-CGRAD-4),gbri) == -1)
					gbri += (i<<16);
			}
		}

		fmousx = fmousy = 0; //Don't allow any other mouse movement
	}

	f = MIN(vx5hx/vx5hz,1.0);
	if (!anglemode) //Latitude/longitude mode
	{
		anglng += fmousx*f*.008;
		if (anglng < -PI) anglng += PI*2;
		if (anglng >  PI) anglng -= PI*2;
		anglat += fmousy*f*fmouseymul;
		if (anglat < -PI) anglat += PI*2;
		if (anglat >  PI) anglat -= PI*2;
		istr.x = 0.0; istr.y =-1.0; istr.z = 0.0;
		ihei.x = 0.0; ihei.y = 0.0; ihei.z = 1.0;
		ifor.x =-1.0; ifor.y = 0.0; ifor.z = 0.0;
		dorthorotate(0,anglat,anglng,&istr,&ihei,&ifor);
	}
	else //Free-angle horizon correction mode
	{
		dorthorotate(istr.z*.1,fmousy*f*fmouseymul,fmousx*f*.008,&istr,&ihei,&ifor);
	}

	if (keystatus[0x10]) //Q (09/07/2004: hack for causing crash. isshldiv16safe didn't solve it :/)
	{
		if (keystatus[0x2a])
			anglat += ((ihei.z*vx5hz/ifor.z + vx5hy)*65536 - (float)0x01c10384)*.00000001; //Read fault
		else
			anglat += ((ihei.z*vx5hz/ifor.z + vx5hy)*65536 - (float)0x01c10384)*.00000002; //shldiv16 fault

		istr.x = 0.0; istr.y =-1.0; istr.z = 0.0;
		ihei.x = 0.0; ihei.y = 0.0; ihei.z = 1.0;
		ifor.x =-1.0; ifor.y = 0.0; ifor.z = 0.0;
		dorthorotate(0,anglat,anglng,&istr,&ihei,&ifor);
	}

	if (angincmode)
	{
		vx5.anginc = 1; f = ox*ox + oy*oy + oz*oz;
		if (f >= .01*.01)
		{
			vx5.anginc = 2;
			if (f >= .08*.08)
			{
				vx5.anginc = 3;
				if (f >= .64*.64) vx5.anginc = 4;
			}
		}
	}

	if (keystatus[0x1]) //ESC
	{
		keystatus[0x1] = 0;
		if (helpmode&1) helpmode = 2;
		else { strcpy(message,"PRESS Y TO QUIT"); messagetimeout = totclk+4000; }
	}
	if (keystatus[0x15]) //Y
	{
		keystatus[0x15] = 0;
		fmouseymul = -fmouseymul;
		if ((totclk < messagetimeout) && (!strcmp(message,"PRESS Y TO QUIT")))
			quitloop();
	}
	if (keystatus[0x31]) //N
	{
		keystatus[0x31] = 0;
		if ((totclk < messagetimeout) && (!strcmp(message,"PRESS Y TO QUIT")))
			messagetimeout = 0;
	}

	updatevxl();
}

long initapp (long argc, char **argv)
{
	long i, j, k, z, argfilindex = -1, dausesse = -1;
	char snotbuf[MAX_PATH];

#ifndef _WIN32
	getcwd(snotbuf,sizeof(snotbuf));
#else
	GetCurrentDirectory(sizeof(snotbuf),snotbuf);
#endif
	relpathinit(snotbuf);
	//relpathinit(argv[0]); //NOTE: Winmain doesn't pass correct program name!

	prognam = "VOXED by Ken Silverman";
	xres = 640; yres = 480; colbits = 32; fullscreen = 0;
	for(i=argc-1;i>0;i--)
	{
		if (argv[i][0] != '/') { argfilindex = i; continue; }
		if (!strcasecmp(&argv[i][1],"3dn")) { dausesse = 0; continue; }
		if (!strcasecmp(&argv[i][1],"sse")) { dausesse = 1; continue; }
		if (!strcasecmp(&argv[i][1],"win")) { fullscreen = 0; continue; }
		//if (argv[i][1] == '?') { showinfo(); return(-1); }
		if ((argv[i][1] >= '0') && (argv[i][1] <= '9'))
		{
			k = 0; z = 0;
			for(j=1;;j++)
			{
				if ((argv[i][j] >= '0') && (argv[i][j] <= '9'))
					{ k = (k*10+argv[i][j]-48); continue; }
				switch (z)
				{
					case 0: xres = k; break;
					case 1: yres = k; break;
					//case 2: colbits = k; break;
				}
				if (!argv[i][j]) break;
				z++; if (z > 2) break;
				k = 0;
			}
		}
	}
	if (xres > MAXXDIM) xres = MAXXDIM;
	if (yres > MAXYDIM) yres = MAXYDIM;

	if (initvoxlap()) return(-1);
	if (dausesse >= 0)
	{
		if (dausesse == 0) { cputype &= ~(1<<25); cputype |= ((1<<30)|(1<<31)); } //Override auto-detected CPU type
		if (dausesse == 1) cputype |= (1<<25); //Override auto-detected CPU type
	}
	initrgbcolselect();

	vx5.fallcheck = 0; //Voxed doesn't need this!

	kzaddstack("voxdata.zip");

	strcpy(vxlfilnam,"vxl/untitled.vxl");
	strcpy(sxlfilnam,"vxl/default.sxl");
	if (argfilindex >= 0)
	{
		if (!argv[argfilindex][1])
		{
			strcpy(snotbuf,"c1.dta");
			snotbuf[1] = argv[argfilindex][0];
			if (!loaddta(snotbuf,&ipos,&istr,&ihei,&ifor)) loadnul(&ipos,&istr,&ihei,&ifor);
			initsxl();
		}
		else
		{
			strcpy(vxlfilnam,argv[argfilindex]);

			i = strlen(vxlfilnam);
				//Hack to remove spaces at end (but why is this necessary?)
			while ((i) && (vxlfilnam[i-1] == ' ')) { vxlfilnam[i-1] = 0; i--; }

			if ((i >= 5) && (!strcasecmp(&vxlfilnam[i-4],".BSP")))
			{
				loadbsp(vxlfilnam,&ipos,&istr,&ihei,&ifor);
				initsxl();
			}
			else if ((i >= 5) && (!strcasecmp(&vxlfilnam[i-4],".PNG")))
			{
				loadpng(vxlfilnam,&ipos,&istr,&ihei,&ifor);
				initsxl();
			}
			else if ((i >= 5) && (!strcasecmp(&vxlfilnam[i-4],".SXL")))
			{
				strcpy(sxlfilnam,vxlfilnam);
				if (voxedloadsxl(sxlfilnam))
				{
					if (!loadvxl(vxlfilnam,&ipos,&istr,&ihei,&ifor)) loadnul(&ipos,&istr,&ihei,&ifor);
				}
				else
				{
					loadnul(&ipos,&istr,&ihei,&ifor);
					initsxl();
				}
			}
			else
			{
				if (!strchr(vxlfilnam,'.')) strcat(vxlfilnam,".vxl");
				if (!loadvxl(vxlfilnam,&ipos,&istr,&ihei,&ifor)) loadnul(&ipos,&istr,&ihei,&ifor);
				initsxl();
			}
		}
	}
	else
	{
		if (voxedloadsxl(sxlfilnam))
		{
			if (!loadvxl(vxlfilnam,&ipos,&istr,&ihei,&ifor)) loadnul(&ipos,&istr,&ihei,&ifor);
		}
		else
		{
			loadnul(&ipos,&istr,&ihei,&ifor);
			initsxl();
		}
	}
	fixanglemode();
	setsideshades(0,28,8,24,12,12);

	//if (!skyfilnam[0]) strcpy(skyfilnam,"png/voxsky.png");
	if (skyfilnam[0]) loadsky(skyfilnam);

	if (!helpmode)
#if (USETDHELP == 0)
		if (kzopen("voxedhlp.txt"))
		{
			helpleng = kzfilelength();
			if (helpbuf = (char *)malloc(helpleng))
				{ kzread(helpbuf,helpleng); helpmode = 2; }
			kzclose();
		}
#else
		if (kbdhelp_load()) helpmode = 2; // TD help
#endif

	readklock(&odtotclk);
	numframes = 0;
	return(0);
}

#if 0
!endif
#endif
