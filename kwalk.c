#if 0 //To compile, type: "nmake kwalk.c"
kwalk.exe: kwalk.obj voxlap5.obj v5.obj kplib.obj winmain.obj; link kwalk voxlap5 v5 kplib winmain ddraw.lib dinput.lib ole32.lib dxguid.lib user32.lib gdi32.lib comdlg32.lib /opt:nowin98
	del winmain.obj
kwalk.obj:   kwalk.c voxlap5.h sysmain.h; cl /c /J /TP kwalk.c     /Ox /Ob2 /G6Fy /Gs /MD
voxlap5.obj: voxlap5.c voxlap5.h;         cl /c /J /TP voxlap5.c   /Ox /Ob2 /G6Fy /Gs /MD
v5.obj:      v5.asm;                      ml /c /coff v5.asm
kplib.obj:   kplib.c;                     cl /c /J /TP kplib.c     /Ox /Ob2 /G6Fy /Gs /MD
winmain.obj: winmain.cpp;                 cl /c /J /TP winmain.cpp /Ox /Ob2 /G6Fy /Gs /MD /DNOSOUND
!if 0
#endif

//VOXLAP engine by Ken Silverman (http://advsys.net/ken)

// write documentation! be sure to mention the "koosh" ball :)
// add shift, scale, shear transformations
// improve interpolation (cubic splines?)
// allow velcro re-adjustment without having to start from scratch
// add grid-type snapping to RMB
// improve ball joint controls (using 3 hardcoded directions sucks!)

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "sysmain.h"
#include "voxlap5.h"

long kfatim = -1, okfatim = -1;

dpoint3d ipos, istr, ihei, ifor;
float vx5hx, vx5hy, vx5hz;
long bstatus = 0, obstatus = 0;
float fmousymul = -1.0;

long curfrm = 0;
long grabspr = -1, grabseq = -1, grabseq2 = -1, setlimode = -1;

				// 0:100 ms/pix 5000ms/tick (25pix/tick)
				// 1: 50 ms/pix 2000ms/tick (20pix/tick)
				// 2: 20 ms/pix 1000ms/tick (20pix/tick)
				// 3: 10 ms/pix  500ms/tick (25pix/tick)
				// 4:  5 ms/pix  200ms/tick (20pix/tick)
				// 5:  2 ms/pix  100ms/tick (20pix/tick)
				// 6:  1 ms/pix   50ms/tick (25pix/tick)
long seqmode = 0, seqoffs = 0, seqzoom = 2, seqcur = 0, gridlockmode = 1;
long seqzoomsppix[] = { 100,  50,  20, 10,  5,  2, 1};
long seqzoomsptic[] = {5000,2000,1000,500,200,100,50};
long seqzoomspti2[] = {1000,1000, 500,100,100, 50,10};
long seqzoomspti3[] = {1000,1000, 100,100,100, 10,10};

	//Stitching variables
long velcron = -1, velcroi;

	//Timer global variables
double odtotclk, dtotclk;
float fsynctics;

	//FPS variables (integrated with timer handler)
#define AVERAGEFRAMES 32
long frameval[AVERAGEFRAMES], numframes = 0, averagefps = 0;

char kfanam[MAX_PATH] = "", kv6nam[MAX_PATH] = "";
char tempbuf[max(MAX_PATH+256,4096)];

	//LIMB information
#define MAXSPRITES 1024
kv6data *kv6[MAXSPRITES]; //4K
vx5sprite spr[MAXSPRITES]; //64K
long numspr = 0;

long kv6toocomplex;
//----------------------------- KFA file format: -----------------------------
#define MAXFRM 1024
#define MAXSEQS 4096

	//TREE/VELCRO information
long numhin = 0, numfrm = 1, seqnum = 0;
hingetype hinge[MAXSPRITES]; //64K (shares indeces with spr)
long hingesort[MAXSPRITES]; //4K

	//KEY-FRAME ANGLE information
short frmval[MAXFRM+1][MAXSPRITES]; //2050K

	//ACTION/MIDI STYLE information (Animation state fully desribed by time!)
	//All sequences are stored on same time-ordered list. For example,
	//   jump might be at time 0, while salute starts at time 10000 (ms).
	//   if (frm >= 0): frm is an index to a frame
	//   if (frm <  0): i = ~seq[i].frm, if (seq[i].frm == ~i) stop;
seqtyp seq[MAXSEQS]; //32K

//   //User-defined names associated with times
//#define MAXACTS 256
//long curact = 0, numact = 0, actstsiz = 0, actstmal = 0;
//typedef struct { long tim; char *aptr; } acttyp;
//acttyp actptr[MAXACTS];
//char *actst;

//---------------------------------------------------------------------------

extern "C" long zbufoff;

static _inline void ftol (float f, long *a)
{
	_asm
	{
		mov eax, a
		fld f
		fistp dword ptr [eax]
	}
}

static _inline void fcossin (float a, float *c, float *s)
{
	_asm
	{
		fld a
		fsincos
		mov eax, c
		fstp dword ptr [eax]
		mov eax, s
		fstp dword ptr [eax]
	}
}

static _inline long scale (long a, long d, long c)
{
	_asm
	{
		mov eax, a
		mov edx, d
		mov ecx, c
		imul edx
		idiv ecx
	}
}

static _inline long mulshr16 (long a, long d)
{
	_asm
	{
		mov eax, a
		mov edx, d
		imul edx
		shrd eax, edx, 16
	}
}

static _inline long shldiv16 (long a, long b)
{
	_asm
	{
		mov eax, a
		mov ebx, b
		mov edx, eax
		shl eax, 16
		sar edx, 16
		idiv ebx
	}
}

//----------------------  WIN file select code begins ------------------------

char *stripdir (char *filnam)
{
	long i, j;
	for(i=0,j=-1;filnam[i];i++)
		if ((filnam[i] == '/') || (filnam[i] == '\\')) j = i;
	return(&filnam[j+1]);
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

static char *relpath (char *st)
{
	long i;
	char ch0, ch1;
	for(i=0;st[i];i++)
	{
		ch0 = st[i];
		if ((ch0 >= 'a') && (ch0 <= 'z')) ch0 -= 32;
		if (ch0 == '/') ch0 = '\\';

		ch1 = relpathbase[i];
		if ((ch1 >= 'a') && (ch1 <= 'z')) ch1 -= 32;
		if (ch1 == '/') ch1 = '\\';

		if (ch0 != ch1) return(&st[i]);
	}
	return(&st[0]);
}

#ifdef _WIN32
#include <commdlg.h>
static char fileselectnam[MAX_PATH+1];
static long fileselect1stcall = -1; //Stupid directory hack
static char *loadfileselect (char *mess, char *spec, char *defext)
{
	long i;
	for(i=0;fileselectnam[i];i++) if (fileselectnam[i] == '/') fileselectnam[i] = '\\';
	OPENFILENAME ofn =
	{
		sizeof(OPENFILENAME),ghwnd,0,spec,0,0,1,fileselectnam,MAX_PATH,0,0,(char *)(((long)relpathbase)&fileselect1stcall),mess,
		OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST|OFN_HIDEREADONLY,0,0,defext,0,0,0
	};
	fileselect1stcall = 0; //Let windows remember directory after 1st call
	if (!GetOpenFileName(&ofn)) return(0); else return(fileselectnam);
}
static char *savefileselect (char *mess, char *spec, char *defext)
{
	long i;
	for(i=0;fileselectnam[i];i++) if (fileselectnam[i] == '/') fileselectnam[i] = '\\';
	OPENFILENAME ofn =
	{
		sizeof(OPENFILENAME),ghwnd,0,spec,0,0,1,fileselectnam,MAX_PATH,0,0,(char *)(((long)relpathbase)&fileselect1stcall),mess,
		OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST|OFN_HIDEREADONLY|OFN_OVERWRITEPROMPT,0,0,defext,0,0,0
	};
	fileselect1stcall = 0; //Let windows remember directory after 1st call
	if (!GetSaveFileName(&ofn)) return(0); else return(fileselectnam);
}
#endif

//-----------------------  WIN file select code ends -------------------------

	//Note: ³p1-p0³ must equal ³p2-p0³
void drawarc3d (point3d *p0, point3d *p1, point3d *p2, long col)
{
	point3d tp, tp2;
	float f, t, d1, d2;


	d1 = sqrt((p1->x-p0->x)*(p1->x-p0->x) + (p1->y-p0->y)*(p1->y-p0->y) + (p1->z-p0->z)*(p1->z-p0->z));
	d2 = sqrt((p2->x-p0->x)*(p2->x-p0->x) + (p2->y-p0->y)*(p2->y-p0->y) + (p2->z-p0->z)*(p2->z-p0->z));

	tp = (*p1);
	for(t=.1;t<=1.05;t+=.1)
	{
		tp2.x = (p2->x-p1->x)*t + (p1->x-p0->x);
		tp2.y = (p2->y-p1->y)*t + (p1->y-p0->y);
		tp2.z = (p2->z-p1->z)*t + (p1->z-p0->z);

		f = ((d2-d1)*t + d1) / sqrt(tp2.x*tp2.x + tp2.y*tp2.y + tp2.z*tp2.z);
		tp2.x = tp2.x*f+p0->x;
		tp2.y = tp2.y*f+p0->y;
		tp2.z = tp2.z*f+p0->z;

		drawline3d(tp.x,tp.y,tp.z,tp2.x,tp2.y,tp2.z,col);
		tp = tp2;
	}
}

	//Converts from sprite coordinates to world coordinates
void spr2world (vx5sprite *sp, point3d *i, point3d *o)
{
	point3d tp = (*i);
	o->x = tp.x*sp->s.x + tp.y*sp->h.x + tp.z*sp->f.x + sp->p.x;
	o->y = tp.x*sp->s.y + tp.y*sp->h.y + tp.z*sp->f.y + sp->p.y;
	o->z = tp.x*sp->s.z + tp.y*sp->h.z + tp.z*sp->f.z + sp->p.z;
}

void fliphinge (long gspr)
{
	long i;
	short s;

	hinge[gspr].v[0].x = -hinge[gspr].v[0].x;
	hinge[gspr].v[0].y = -hinge[gspr].v[0].y;
	hinge[gspr].v[0].z = -hinge[gspr].v[0].z;
	hinge[gspr].v[1].x = -hinge[gspr].v[1].x;
	hinge[gspr].v[1].y = -hinge[gspr].v[1].y;
	hinge[gspr].v[1].z = -hinge[gspr].v[1].z;
	s = hinge[gspr].vmin;
	hinge[gspr].vmin = -hinge[gspr].vmax;
	hinge[gspr].vmax = -s;
	for(i=numfrm-1;i>=0;i--) frmval[i][gspr] = -frmval[i][gspr];
}

void seqsort ()
{
	long i, j;
	seqtyp tseq;

	for(i=1;i<seqnum;i++)
		for(j=0;j<i;j++)
			if (seq[i].tim < seq[j].tim)
				{ tseq = seq[i]; seq[i] = seq[j]; seq[j] = tseq; }
}

void savekfa (char *filnam)
{
	FILE *fil;
	long i;

	if (!(fil = fopen(filnam,"wb"))) return;
	i = 0x6b6c774b; fwrite(&i,4,1,fil); //Kwlk

	i = strlen(kv6nam); fwrite(&i,4,1,fil);
	fwrite(kv6nam,i,1,fil);

	fwrite(&numhin,4,1,fil);
	fwrite(hinge,numhin*sizeof(hingetype),1,fil);

	fwrite(&numfrm,4,1,fil);
	for(i=0;i<numfrm;i++)
		fwrite(&frmval[i][0],numhin*sizeof(frmval[0][0]),1,fil);

	fwrite(&seqnum,4,1,fil);
	fwrite(seq,seqnum*sizeof(seqtyp),1,fil);

	fclose(fil);
}

//----------------------------------------------------------------------

kv6voxtype *getvptr (kv6data *kv, long x, long y)
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
long vfifo[VFIFSIZ];
void floodsucksprite (kv6data *kv, long ox, long oy,
							 kv6voxtype *v0, kv6voxtype *v1)
{
	kv6voxtype *v, *ve, *ov, *v2, *v3;
	long i, j, x, y, z, x0, y0, z0, x1, y1, z1, n, vfif0, vfif1;

	if (numspr >= MAXSPRITES-1)
	{
		for(i=numspr-1;i>=0;i--) if (kv6[i]) { free(kv6[i]); kv6[i] = 0; }
		numspr = 0; kv6toocomplex = 1;
		for(i=kv->numvoxs-1;i>=0;i--) kv->vox[i].vis = (kv->vox[i].vis&~64)|128;
		x0 = y0 = z0 = 0; x1 = kv->xsiz; y1 = kv->ysiz; z1 = kv->zsiz; n = kv->numvoxs;
		goto floodsuckend;
	}

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
				default: __assume(0); //tells MSVC default can't be reached
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
						for(i=numspr-1;i>=0;i--) if (kv6[i]) { free(kv6[i]); kv6[i] = 0; }
						numspr = 0; kv6toocomplex = 1;
						for(i=kv->numvoxs-1;i>=0;i--) kv->vox[i].vis = (kv->vox[i].vis&~64)|128;
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
	if (!(kv6[numspr] = (kv6data *)malloc(i))) return;
	kv6[numspr]->leng = i;
	kv6[numspr]->xsiz = x1-x0;
	kv6[numspr]->ysiz = y1-y0;
	kv6[numspr]->zsiz = z1-z0;
	kv6[numspr]->xpiv = 0; //Set limb pivots to 0 - don't need it twice!
	kv6[numspr]->ypiv = 0;
	kv6[numspr]->zpiv = 0;
	kv6[numspr]->numvoxs = n;
	kv6[numspr]->namoff = 0;
	kv6[numspr]->lowermip = 0;
	kv6[numspr]->vox = (kv6voxtype *)(((long)(kv6[numspr]))+sizeof(kv6data));
	kv6[numspr]->xlen = (unsigned long *)(((long)(kv6[numspr]->vox))+n*sizeof(kv6voxtype));
	kv6[numspr]->ylen = (unsigned short *)(((long)(kv6[numspr]->xlen))+(x1-x0)*4);

		//Extract sub-KV6 to newly allocated kv6data
	v3 = kv6[numspr]->vox; n = 0;
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
			kv6[numspr]->ylen[(x-x0)*(y1-y0)+(y-y0)] = n-oy;
		}
		kv6[numspr]->xlen[x-x0] = n-ox;
		v = v2+kv->xlen[x];
	}

	spr[numspr].p.x = x0-kv->xpiv;
	spr[numspr].p.y = y0-kv->ypiv;
	spr[numspr].p.z = z0-kv->zpiv;
	spr[numspr].s.x = 1; spr[numspr].s.y = 0; spr[numspr].s.z = 0;
	spr[numspr].h.x = 0; spr[numspr].h.y = 1; spr[numspr].h.z = 0;
	spr[numspr].f.x = 0; spr[numspr].f.y = 0; spr[numspr].f.z = 1;
	spr[numspr].flags = 0;
	spr[numspr].voxnum = kv6[numspr];
	hinge[numspr].parent = -1;

	//{
	//   char snotbuf[256];
	//   memset((void *)snotbuf,0,256);
	//   for(i=kv6[numspr]->numvoxs-1;i>=0;i--) snotbuf[kv6[numspr]->vox[i].dir]++;
	//   for(i=0;i<256;i++) printf("%d ",snotbuf[i]); printf("\n");
	//}

	numspr++;
}

void kfasorthinge (hingetype *h, long nh, long *hsort)
{
	long i, j, n;

		// ANASPLIT.KFA:  h[0]=1  h[5]=4  h[10]=9
		// -------3-----  h[1]=2  h[6]=5  h[11]=2
		// ---2---  4  8  h[2]=3  h[7]=13 h[12]=11
		// 1 11 13  5  9  h[3]=-1 h[8]=3  h[13]=2
		// 0 12  7  6 10  h[4]=3  h[9]=8  order:3,2,4,5,6,8,9,10,11,12,13,1,7,0

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

long loadsplitkv6 (char *filnam)
{
	kv6voxtype *v, *ov, *ve;
	kv6data *kv;
	long i, x, y;

	kv6toocomplex = 0;
	strcpy(kv6nam,stripdir(filnam));

	for(i=numspr-1;i>=0;i--) if (kv6[i]) { free(kv6[i]); kv6[i] = 0; }
	numspr = 0;

	kv = getkv6(filnam); if (!kv) return(0);
	if (!kv->numvoxs) return(0);

	v = kv->vox;
	for(x=kv->numvoxs-1;x>=0;x--) v[x].vis |= ((v[x].vis&32)<<1);
	for(x=0;x<kv->xsiz;x++)
		for(y=0;y<kv->ysiz;y++)
			for(ve=&v[kv->ylen[x*kv->ysiz+y]];v<ve;v++)
			{
				if (v->vis&16) ov = v;
				if ((v->vis&(64+32)) == 64+32) floodsucksprite(kv,x,y,ov,v);
			}
	numhin = numspr;

	if (kv6toocomplex)
	{
		ddflip2gdi();
		MessageBox(ghwnd,"WARNING: KV6 too complex for animation",prognam,MB_OK);
	}

	numfrm = 1; seqnum = 0;
	kfasorthinge(hinge,numhin,hingesort);
	seqcur = curfrm = seqoffs = 0;
	grabspr = grabseq = grabseq2 = setlimode = velcron = kfatim = -1;

	return(1);
}

extern void genperp (point3d *, point3d *, point3d *);
extern void mat0 (point3d *, point3d *, point3d *, point3d *,
						point3d *, point3d *, point3d *, point3d *,
						point3d *, point3d *, point3d *, point3d *);
extern void mat1 (point3d *, point3d *, point3d *, point3d *,
						point3d *, point3d *, point3d *, point3d *,
						point3d *, point3d *, point3d *, point3d *);
extern void mat2 (point3d *, point3d *, point3d *, point3d *,
						point3d *, point3d *, point3d *, point3d *,
						point3d *, point3d *, point3d *, point3d *);

static void setlimb (long i, long p, long trans_type, short val)
{
	point3d ps, ph, pf, pp;
	point3d qs, qh, qf, qp;
	float c, s;

		//Generate orthonormal matrix in world space for child limb
	qp = hinge[i].p[0]; qs = hinge[i].v[0]; genperp(&qs,&qh,&qf);

	switch (trans_type)
	{
		case 0: //Hinge rotate!
			fcossin(((float)val)*(PI/32768.0),&c,&s);
			ph = qh; pf = qf;
			qh.x = ph.x*c - pf.x*s; qf.x = ph.x*s + pf.x*c;
			qh.y = ph.y*c - pf.y*s; qf.y = ph.y*s + pf.y*c;
			qh.z = ph.z*c - pf.z*s; qf.z = ph.z*s + pf.z*c;
			break;
	}

		//Generate orthonormal matrix in world space for parent limb
	pp = hinge[i].p[1]; ps = hinge[i].v[1]; genperp(&ps,&ph,&pf);

		//mat0(rotrans, loc_velcro, par_velcro)
	mat0(&qs,&qh,&qf,&qp, &qs,&qh,&qf,&qp, &ps,&ph,&pf,&pp);
		//mat2(par, rotrans, parent * par_velcro * (loc_velcro x rotrans)^-1)
	mat2(&spr[p].s,&spr[p].h,&spr[p].f,&spr[p].p, &qs,&qh,&qf,&qp,
		  &spr[i].s,&spr[i].h,&spr[i].f,&spr[i].p);
}

	//Uses binary search to find sequence index at time "tim"
static long kfatime2seq (long tim)
{
	long i, a, b;

	for(a=0,b=seqnum-1;b-a>=2;)
		{ i = ((a+b)>>1); if (tim >= seq[i].tim) a = i; else b = i; }
	return(a);
}

static long animsprite (long t, long ti)
{
	long i, z, zz;

	z = kfatime2seq(t);
	while (ti > 0)
	{
		z++; if (z >= seqnum) break;
		i = seq[z].tim-t; if (i <= 0) break;
		if (i > ti) { t += ti; break; }
		ti -= i;
		zz = ~seq[z].frm; if (zz >= 0) { if (z == zz) break; z = zz; }
		t = seq[z].tim;
	}
	return(t);
}

static void kfadraw ()
{
	long i, j, k, x, y, z, zz, trat;
	long trat2, z0, zz0, frm0;

	z = kfatime2seq(kfatim); zz = z+1;
	if ((zz < seqnum) && (seq[zz].frm != ~zz))
	{
		trat = seq[zz].tim-seq[z].tim;
		if (trat) trat = shldiv16(kfatim-seq[z].tim,trat);
		i = seq[zz].frm; if (i < 0) zz = seq[~i].frm; else zz = i;
	} else trat = 0;
	z = seq[z].frm;
	if (z < 0)
	{
		z0 = kfatime2seq(okfatim); zz0 = z0+1;
		if ((zz0 < seqnum) && (seq[zz0].frm != ~zz0))
		{
			trat2 = seq[zz0].tim-seq[z0].tim;
			if (trat2) trat2 = shldiv16(okfatim-seq[z0].tim,trat2);
			i = seq[zz0].frm; if (i < 0) zz0 = seq[~i].frm; else zz0 = i;
		} else trat2 = 0;
		z0 = seq[z0].frm; if (z0 < 0) { z0 = zz0; trat2 = 0; }
	} else trat2 = -1;

	for(i=numhin-1;i>=0;i--)
	{
		j = hingesort[i]; k = hinge[j].parent;
		if (k >= 0)
		{
			if (trat2 < 0) frm0 = (long)frmval[z][j];
			else
			{
				frm0 = (long)frmval[z0][j];
				if (trat2 > 0)
				{
					x = (((long)(frmval[zz0][j]-frm0))&65535);
					if (hinge[j].vmin == hinge[j].vmax) x = ((x<<16)>>16);
					else if ((((long)(frmval[zz0][j]-hinge[j].vmin))&65535) <
								(((long)(frm0          -hinge[j].vmin))&65535))
						x -= 65536;
					frm0 += mulshr16(x,trat2);
				}
			}

			if (trat > 0)
			{
				x = (((long)(frmval[zz][j]-frm0))&65535);
				if (hinge[j].vmin == hinge[j].vmax) x = ((x<<16)>>16);
				else if ((((long)(frmval[zz][j]-hinge[j].vmin))&65535) <
							(((long)(frm0         -hinge[j].vmin))&65535))
					x -= 65536;
				frm0 += mulshr16(x,trat);
			}
			setlimb(j,k,hinge[j].htype,frm0);
		}
		drawsprite(&spr[j]);
	}
}

long loadkfa (char *filnam)
{
	FILE *fil;
	long i;
	char *v;

	strcpy(kfanam,stripdir(filnam));

	if (!(fil = fopen(filnam,"rb"))) return(0);

	fread(&i,4,1,fil);
	if (i != 0x6b6c774b) { fclose(fil); return(0); } //Kwlk

	fread(&i,4,1,fil);
	strcpy(tempbuf,filnam); v = stripdir(tempbuf);
	fread(v,i,1,fil); v[i] = 0;
	if (!loadsplitkv6(tempbuf))
	{
		strcat(tempbuf," not found... please locate it!");
		if (v = (char *)loadfileselect(tempbuf,"3D voxel model (.KV6)\0;*.kv6\0All files (*.*)\0*.*\0\0","KV6"))
			loadsplitkv6(v);
		else
			{ fclose(fil); quitloop(); return(0); }
	}

	fread(&numhin,4,1,fil);
	fread(hinge,numhin*sizeof(hingetype),1,fil);

	fread(&numfrm,4,1,fil);
	for(i=0;i<numfrm;i++)
		fread(&frmval[i][0],numhin*sizeof(frmval[0][0]),1,fil);

	fread(&seqnum,4,1,fil);
	fread(seq,seqnum*sizeof(seqtyp),1,fil);

	fclose(fil);

	kfasorthinge(hinge,numhin,hingesort);

	seqcur = curfrm = 0;
	seqoffs = 0; gridlockmode = 1;
	grabspr = grabseq = grabseq2 = setlimode = velcron = kfatim = -1;
	return(1);
}

void uninitapp ()
{
	long i;
	for(i=numspr-1;i>=0;i--) if (kv6[i]) { free(kv6[i]); kv6[i] = 0; }
	uninitvoxlap();
}

long initapp (long argc, char **argv)
{
	long i, j, k, z, argfilindex = -1;
	char *v, snotbuf[MAX_PATH];

	GetCurrentDirectory(sizeof(snotbuf),snotbuf);
	relpathinit(snotbuf);

	xres = 1024; yres = 768; colbits = 32; fullscreen = 0; prognam = "KWALK";
	for(i=argc-1;i>0;i--)
	{
		if (argv[i][0] != '/') { argfilindex = i; continue; }
		if (!stricmp(&argv[i][1],"win")) { fullscreen = 0; continue; }
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
	kzaddstack("voxdata.zip");

	istr.x = 1; istr.y = 0; istr.z = 0;
	ihei.x = 0; ihei.y = 0; ihei.z = 1;
	ifor.x = 0; ifor.y = -1; ifor.z = 0;
	ipos.x = -ifor.x*192; ipos.y = -ifor.y*192; ipos.z = -ifor.z*192;

	i = 0;
	if (argfilindex >= 0)
	{
		i = strlen(argv[argfilindex]);
		if ((i > 0) && (argv[argfilindex][i-1] == '6'))
			i = loadsplitkv6(argv[argfilindex]);
		else
			i = loadkfa(argv[argfilindex]);
	}
	if (!i)
	{
		if (v = (char *)loadfileselect("Load animation or model...","Animation (.KFA), 3D voxel model (.KV6)\0*.kfa;*.kv6\0All files (*.*)\0*.*\0\0","KFA"))
		{
			strcpy(tempbuf,(char *)v);
			i = strlen(tempbuf);
			if ((i > 0) && (tempbuf[i-1] == '6'))
				i = loadsplitkv6(tempbuf);
			else
				i = loadkfa(tempbuf);
		}
		if (!i)
		{
			MessageBox(0,"This program requires you to load a .KV6 or .KFA file\n","Error",MB_OK);
			return(-1);
		}
	}

	readklock(&odtotclk);
	return(0);
}

void doframe ()
{
	vx5sprite *sp;
	kv6voxtype *kp;
	point3d tp, tp1, tp2, tp3, tp4, *tptr;
	lpoint3d lp, grablp;
	long i, j, k, x, y, z, zz, trat, frameplace, bpl, xdim, ydim, key;
	float f, fmousx, fmousy;
	char *v;

	clearscreen(0x502040);
	if (!startdirectdraw(&frameplace,&bpl,&xdim,&ydim)) goto skipalldraw;
	if (seqmode) ydim -= 32;
	voxsetframebuffer(frameplace,bpl,xdim,ydim);
	vx5hx = xdim*.5; vx5hy = ydim*.5; vx5hz = vx5hx;
	setcamera(&ipos,&istr,&ihei,&ifor,vx5hx,vx5hy,vx5hz);

		//Clear Z-buffer
	for(y=0,i=frameplace+zbufoff;y<ydim;y++,i+=bpl)
		memset((void *)i,0x7f,xdim<<2);

	if ((seqnum > 0) && ((unsigned long)kfatim < seq[seqnum-1].tim))
		kfadraw();
	else
	{
		for(i=numhin-1;i>=0;i--)
		{
			j = hingesort[i]; k = hinge[j].parent;
			if ((k >= 0) && ((velcron < 0) || (j != velcroi)))
			{
				if ((j == setlimode) && (j < numspr))
				{
					if (k < numspr)
					{
						if (hinge[j].vmin != hinge[j].vmax)
						{
							if (numframes&1) spr[j].flags |= 1;
							setlimb(j,k,hinge[j].htype,hinge[j].vmin); drawsprite(&spr[j]);
							setlimb(j,k,hinge[j].htype,hinge[j].vmax); drawsprite(&spr[j]);
							spr[j].flags &= ~1;
						}
					}
					else
					{
						if ((hinge[j].vmin != hinge[j].vmax) &&
							 (hinge[k].vmin != hinge[k].vmax))
						{
							if (numframes&1) spr[j].flags |= 1;

							setlimb(k,hinge[k].parent,hinge[k].htype,hinge[k].vmin);
							setlimb(j,k,              hinge[j].htype,hinge[j].vmin);
							drawsprite(&spr[j]);

							setlimb(k,hinge[k].parent,hinge[k].htype,hinge[k].vmin);
							setlimb(j,k,              hinge[j].htype,hinge[j].vmax);
							drawsprite(&spr[j]);

							setlimb(k,hinge[k].parent,hinge[k].htype,hinge[k].vmax);
							setlimb(j,k,              hinge[j].htype,hinge[j].vmin);
							drawsprite(&spr[j]);

							setlimb(k,hinge[k].parent,hinge[k].htype,hinge[k].vmax);
							setlimb(j,k,              hinge[j].htype,hinge[j].vmax);
							drawsprite(&spr[j]);

							setlimb(k,hinge[k].parent,hinge[k].htype,frmval[curfrm][k]);
							spr[j].flags &= ~1;
						}
					}
				}
				setlimb(j,k,hinge[j].htype,frmval[curfrm][j]);
			}
	
			if (j < numspr)
			{
				if (!(numframes&3)) spr[grabspr].flags |= 1;
				drawsprite(&spr[j]);
				spr[grabspr].flags &= ~1;
			}
		}
	}

	switch(velcron) //NOTE: no 'break's on purpose
	{
		case 3:
			spr2world(&spr[hinge[velcroi].parent],&hinge[velcroi].p[1],&tp);
			tp2.x = hinge[velcroi].p[1].x+hinge[velcroi].v[1].x*16.0;
			tp2.y = hinge[velcroi].p[1].y+hinge[velcroi].v[1].y*16.0;
			tp2.z = hinge[velcroi].p[1].z+hinge[velcroi].v[1].z*16.0;
			spr2world(&spr[hinge[velcroi].parent],&tp2,&tp2);

			tp3 = tp; tp4 = tp2;
			if (fabs(hinge[velcroi].v[1].x) >= fabs(hinge[velcroi].v[1].y))
				  { if (hinge[velcroi].v[1].x >= 0) tp3.x += 16; else tp3.x -= 16; }
			else { if (hinge[velcroi].v[1].y >= 0) tp3.y += 16; else tp3.y -= 16; }
			tp4.z = tp.z;
			f = (tp4.x-tp.x)*(tp4.x-tp.x)+(tp4.y-tp.y)*(tp4.y-tp.y)+(tp4.z-tp.z)*(tp4.z-tp.z);
			if (f > .0001)
			{
				f = 16.0 / sqrt(f);
				tp4.x = (tp4.x-tp.x)*f+tp.x;
				tp4.y = (tp4.y-tp.y)*f+tp.y;
				tp4.z = (tp4.z-tp.z)*f+tp.z;
				drawarc3d(&tp,&tp2,&tp4,0x3f3f7f);
				drawline3d(tp.x,tp.y,tp.z,tp4.x,tp4.y,tp4.z,0x7f3f3f);
				drawline3d(tp.x,tp.y,tp.z,tp3.x,tp3.y,tp3.z,0x7f3f3f);
				drawarc3d(&tp,&tp3,&tp4,0x7f3f3f);
			}
			drawline3d(tp.x,tp.y,tp.z,tp2.x,tp2.y,tp2.z,0x80ffffff);

			if (velcron == 3)
			{
				tptr = &hinge[velcroi].v[1];

				ftol(fabs(atan2(tptr->z,sqrt(tptr->x*tptr->x+tptr->y*tptr->y)))*180.0/PI,&i);
				if (i < 90)
				{
					if ((i) && (project2d((tp2.x+tp4.x)*.5,(tp2.y+tp4.y)*.5,(tp2.z+tp4.z)*.5,&tp2.x,&tp2.y,&tp2.z)))
					{
						ftol(tp2.x,&x); ftol(tp2.y,&y);
						if (((unsigned long)x < xdim) && ((unsigned long)y < ydim))
							print6x8(x,y,-1,0x3f3f7f,"%d%c",i,248);
					}

					ftol(fabs(atan2(tptr->y,tptr->x))*180.0/PI,&i);
					i = ((i+360)%90); if (i > 45) i = 90-i;
					if ((i) && (project2d((tp3.x+tp4.x)*.5,(tp3.y+tp4.y)*.5,(tp3.z+tp4.z)*.5,&tp3.x,&tp3.y,&tp3.z)))
					{
						ftol(tp3.x,&x); ftol(tp3.y,&y);
						if (((unsigned long)x < xdim) && ((unsigned long)y < ydim))
							print6x8(x,y,-1,0x7f3f3f,"%d%c",i,248);
					}
				}
			}

		case 2:
			if (velcron == 2)
				spr2world(&spr[hinge[velcroi].parent],&hinge[velcroi].p[1],&tp);
			for(i=-16;i;i++)
			{
				j = rand(); tp2.z = ((float)(j-16384))/16384.0;
				fcossin((float)j*2.399963229728654,&tp2.x,&tp2.y);
				f = sqrt(1.0 - tp2.z*tp2.z)*.5; tp2.x *= f; tp2.y *= f; tp2.z *= .5;
				drawline3d(tp.x,tp.y,tp.z,tp.x+tp2.x,tp.y+tp2.y,tp.z+tp2.z,0x80ffffff);
			}

		case 1:
			spr2world(&spr[velcroi],&hinge[velcroi].p[0],&tp);
			tp2.x = hinge[velcroi].p[0].x+hinge[velcroi].v[0].x*16.0;
			tp2.y = hinge[velcroi].p[0].y+hinge[velcroi].v[0].y*16.0;
			tp2.z = hinge[velcroi].p[0].z+hinge[velcroi].v[0].z*16.0;
			spr2world(&spr[velcroi],&tp2,&tp2);

			tp3 = tp; tp4 = tp2;
			if (fabs(hinge[velcroi].v[0].x) >= fabs(hinge[velcroi].v[0].y))
				  { if (hinge[velcroi].v[0].x >= 0) tp3.x += 16; else tp3.x -= 16; }
			else { if (hinge[velcroi].v[0].y >= 0) tp3.y += 16; else tp3.y -= 16; }
			tp4.z = tp.z;
			f = (tp4.x-tp.x)*(tp4.x-tp.x)+(tp4.y-tp.y)*(tp4.y-tp.y)+(tp4.z-tp.z)*(tp4.z-tp.z);
			if (f > .0001)
			{
				f = 16.0 / sqrt(f);
				tp4.x = (tp4.x-tp.x)*f+tp.x;
				tp4.y = (tp4.y-tp.y)*f+tp.y;
				tp4.z = (tp4.z-tp.z)*f+tp.z;
				drawarc3d(&tp,&tp2,&tp4,0x3f3f7f);
				drawline3d(tp.x,tp.y,tp.z,tp4.x,tp4.y,tp4.z,0x7f3f3f);
				drawline3d(tp.x,tp.y,tp.z,tp3.x,tp3.y,tp3.z,0x7f3f3f);
				drawarc3d(&tp,&tp3,&tp4,0x7f3f3f);
			}
			drawline3d(tp.x,tp.y,tp.z,tp2.x,tp2.y,tp2.z,0x80ffffff);

			if (velcron == 1)
			{
				tptr = &hinge[velcroi].v[0];

				ftol(fabs(atan2(tptr->z,sqrt(tptr->x*tptr->x+tptr->y*tptr->y)))*180.0/PI,&i);
				if (i < 90)
				{
					if ((i) && (project2d((tp2.x+tp4.x)*.5,(tp2.y+tp4.y)*.5,(tp2.z+tp4.z)*.5,&tp2.x,&tp2.y,&tp2.z)))
					{
						ftol(tp2.x,&x); ftol(tp2.y,&y);
						if (((unsigned long)x < xdim) && ((unsigned long)y < ydim))
							print6x8(x,y,-1,0x3f3f7f,"%d%c",i,248);
					}

					ftol(fabs(atan2(tptr->y,tptr->x))*180.0/PI,&i);
					i = ((i+360)%90); if (i > 45) i = 90-i;
					if ((i) && (project2d((tp3.x+tp4.x)*.5,(tp3.y+tp4.y)*.5,(tp3.z+tp4.z)*.5,&tp3.x,&tp3.y,&tp3.z)))
					{
						ftol(tp3.x,&x); ftol(tp3.y,&y);
						if (((unsigned long)x < xdim) && ((unsigned long)y < ydim))
							print6x8(x,y,-1,0x7f3f3f,"%d%c",i,248);
					}
				}
			}
		case 0:
			if (velcron == 0)
				spr2world(&spr[velcroi],&hinge[velcroi].p[0],&tp);
			for(i=-16;i;i++)
			{
				j = rand(); tp2.z = ((float)(j-16384))/16384.0;
				fcossin((float)j*2.399963229728654,&tp2.x,&tp2.y);
				f = sqrt(1.0 - tp2.z*tp2.z)*.5; tp2.x *= f; tp2.y *= f; tp2.z *= .5;
				drawline3d(tp.x,tp.y,tp.z,tp.x+tp2.x,tp.y+tp2.y,tp.z+tp2.z,0x80ffffff);
			}
		default: break;
	}

	print6x8(0,8,-1,-1,"Frame %d(%d)",curfrm,numfrm);

	if (seqmode)
	{
		ydim += 32;

			//Draw white-grayish box at bottom
		for(y=ydim-32;y<ydim;y++)
		{
			i = bpl*y+frameplace;
			for(x=0;x<xdim;x++) *(long *)((x<<2)+i) = 0xafafbf;
		}

			//Draw minor vertical ticks (no numbers)
		j = seqzoomspti3[seqzoom] / seqzoomsppix[seqzoom];
		x = -seqoffs / seqzoomsppix[seqzoom];
		if (x < 0) k = (j-1-x) / j; else k = 0;
		x += j*k; k *= seqzoomspti3[seqzoom];
		for(;x<xdim;x+=j)
			for(y=ydim-32;y<ydim-30;y++)
				*(long *)(bpl*y+(x<<2)+frameplace) = 0x8f8f9f;

		if (seqzoomspti2[seqzoom] > seqzoomspti3[seqzoom])
		{
				//Draw medium vertical ticks (no numbers)
			j = seqzoomspti2[seqzoom] / seqzoomsppix[seqzoom];
			x = -seqoffs / seqzoomsppix[seqzoom];
			if (x < 0) k = (j-1-x) / j; else k = 0;
			x += j*k; k *= seqzoomspti2[seqzoom];
			for(;x<xdim;x+=j)
				for(y=ydim-32;y<ydim-29;y++)
					*(long *)(bpl*y+(x<<2)+frameplace) = 0x6f6f7f;
		}

			//Draw major vertical ticks (with numbers)
		j = seqzoomsptic[seqzoom] / seqzoomsppix[seqzoom];
		x = -seqoffs / seqzoomsppix[seqzoom];
		if (x < 0) k = (j-1-x) / j; else k = 0;
		x += j*k; k *= seqzoomsptic[seqzoom];
		for(;x<xdim;x+=j)
		{
			for(y=ydim-32;y<ydim-29;y++)
				*(long *)(bpl*y+(x<<2)+frameplace) = 0x4f4f5f;

			if (seqzoom < 4) sprintf(tempbuf,"%d.%d",k/1000,(k/100)%10);
							else sprintf(tempbuf,"%d.%02d",k/1000,(k/10)%100);

			k += seqzoomsptic[seqzoom];
			y = strlen(tempbuf); i = x-y*2;
			if ((unsigned long)i < xdim-y*4)
				print4x6(i,ydim-28,0x7f3f3f,0xafafbf,"%s",tempbuf);
		}

			//Draw sequence data
		for(i=0;i<seqnum;i++)
		{
			x = (seq[i].tim-seqoffs) / seqzoomsppix[seqzoom];
			if ((unsigned long)x < xdim)
				for(y=ydim-22;y<ydim-15;y++) *(long *)(bpl*y+(x<<2)+frameplace) = 0;
			if (seq[i].frm >= 0)
			{
				sprintf(tempbuf,"%d",seq[i].frm);
				y = strlen(tempbuf); x -= y*3;
				if ((unsigned long)x < xdim-y*6)
					print6x8(x,ydim-14,0,-1,"%s",tempbuf);
			}
			else if (seq[i].frm == ~i)
			{     //Draws red octagon (stop sign)
				for(k=-4;k<=4;k++)
					for(j=-4;j<=4;j++)
						if ((labs(j)+labs(k) <= 6) && ((unsigned long)(x+j) < xdim))
							*(long *)(bpl*(ydim-10+k)+((x+j)<<2)+frameplace) = 0xbf0000;
			}
			else
			{
				z = (seq[~seq[i].frm].tim-seqoffs) / seqzoomsppix[seqzoom];

				if ((unsigned long)x < xdim)
					for(y=ydim-22;y<=ydim-3;y++)
						*(long *)(bpl*y+(x<<2)+frameplace) = 0x005f00;

				for(j=1;j<4;j++)
					if ((unsigned long)(z+j) < xdim)
					{
						*(long *)(bpl*(ydim-3+j)+((z+j)<<2)+frameplace) = 0x005f00;
						*(long *)(bpl*(ydim-3-j)+((z+j)<<2)+frameplace) = 0x005f00;
					}

				if (z < 0) z = 0;
				if (x > xdim) x = xdim;
				for(y=z;y<x;y++)
					*(long *)(bpl*(ydim-3)+(y<<2)+frameplace) = 0x005f00;
			}
		}

		x = (seqcur-seqoffs) / seqzoomsppix[seqzoom];
		if ((unsigned long)x < xdim)
			for(y=ydim-21;y<ydim-15;y++) *(long *)(bpl*y+(x<<2)+frameplace) = (-(((y^numframes)&3) == 0));
		sprintf(tempbuf,"%d",curfrm);
		y = strlen(tempbuf); x -= y*3;
		if ((unsigned long)x < xdim-y*6)
			print6x8(x,ydim-14,-1,-1,"%s",tempbuf);

		if ((seqnum > 0) && ((unsigned long)kfatim < seq[seqnum-1].tim))
		{
			x = (kfatim-seqoffs) / seqzoomsppix[seqzoom];
			if ((unsigned long)x < xdim)
				for(y=ydim-21;y<ydim;y++) *(long *)(bpl*y+(x<<2)+frameplace) = 0x00ff00;
		}
	}
	else
	{
		for(i=2;i<=4;i++)
		{
			drawpoint2d((xdim>>1)-i,ydim>>1,0xffffff);
			drawpoint2d((xdim>>1)+i,ydim>>1,0xffffff);
			drawpoint2d(xdim>>1,(ydim>>1)-i,0xffffff);
			drawpoint2d(xdim>>1,(ydim>>1)+i,0xffffff);
		}
	}

	odtotclk = dtotclk; readklock(&dtotclk);
	fsynctics = (float)(dtotclk-odtotclk);

		//Print MAX FRAME RATE
	ftol(1000.0/fsynctics,&frameval[numframes&(AVERAGEFRAMES-1)]);
	for(j=AVERAGEFRAMES-1,i=frameval[0];j>0;j--) i = max(i,frameval[j]);
	averagefps = ((averagefps*3+i)>>2);
	print4x6(0,0,0xc0c0c0,-1,"%.1f (%0.2fms)",(float)averagefps*.001,1000000.0/max((float)averagefps,1));

	stopdirectdraw();
	nextpage();
	numframes++;
skipalldraw:;

	obstatus = bstatus; readmouse(&fmousx,&fmousy,&bstatus);
	readkeyboard(); if (keystatus[1]) quitloop();

	if ((seqnum > 0) && ((unsigned long)kfatim < seq[seqnum-1].tim))
	{     //Animation mode
		ftol(fsynctics*1000.0,&i);
		kfatim = animsprite(kfatim,i);
	}

	if (keystatus[0x33]|keystatus[0x34]) //,.
	{
		f = fsynctics * 2.0;
		if (keystatus[0x2a]) f *= 0.25;
		if (keystatus[0x36]) f *= 4.0;

		tp1.z = ((float)(keystatus[0x33]-keystatus[0x34]))*f;
		fcossin(tp1.z,&tp1.x,&tp1.y);
		tp.x = ipos.x; tp.y = ipos.y; ipos.x = tp.x*tp1.x-tp.y*tp1.y; ipos.y = tp.y*tp1.x+tp.x*tp1.y;
		tp.x = istr.x; tp.y = istr.y; istr.x = tp.x*tp1.x-tp.y*tp1.y; istr.y = tp.y*tp1.x+tp.x*tp1.y;
		tp.x = ihei.x; tp.y = ihei.y; ihei.x = tp.x*tp1.x-tp.y*tp1.y; ihei.y = tp.y*tp1.x+tp.x*tp1.y;
		tp.x = ifor.x; tp.y = ifor.y; ifor.x = tp.x*tp1.x-tp.y*tp1.y; ifor.y = tp.y*tp1.x+tp.x*tp1.y;
	}

	if (!seqmode)
	{
		if (!bstatus)
		{
			f = min(vx5hx/vx5hz,1.0);
			dorthorotate(istr.z*.1,fmousy*f*.008*fmousymul,fmousx*f*.008,&istr,&ihei,&ifor);
		}

		f = fsynctics * 128.0;
		if (keystatus[0x2a]) f *= 0.25;
		if (keystatus[0x36]) f *= 4.0;

		if (keystatus[0xc8]) { ipos.x += ifor.x*f; ipos.y += ifor.y*f; ipos.z += ifor.z*f; }
		if (keystatus[0xd0]) { ipos.x -= ifor.x*f; ipos.y -= ifor.y*f; ipos.z -= ifor.z*f; }
		if (keystatus[0xcb]) { ipos.x -= istr.x*f; ipos.y -= istr.y*f; ipos.z -= istr.z*f; }
		if (keystatus[0xcd]) { ipos.x += istr.x*f; ipos.y += istr.y*f; ipos.z += istr.z*f; }
		if (keystatus[0x52]) { ipos.x += ihei.x*f; ipos.y += ihei.y*f; ipos.z += ihei.z*f; }
		if (keystatus[0x9d]) { ipos.x -= ihei.x*f; ipos.y -= ihei.y*f; ipos.z -= ihei.z*f; }
	}
	else
	{
		ftol(fmousx*seqzoomsppix[seqzoom],&x);
		seqcur += x;
		if (bstatus&1)
		{
			if ((~obstatus)&1)
			{
				j = 0x7fffffff;
				for(i=0;i<seqnum;i++)
					if (labs(seq[i].tim-seqcur) < j)
						{ j = labs(seq[i].tim-seqcur); k = i; }
				if (j < (seqzoomsptic[seqzoom]>>1)) grabseq = k;
														 else grabseq = -1;
			}
			if (grabseq >= 0)
			{
				seq[grabseq].tim += x;
				if (seq[grabseq].tim < 0) seq[grabseq].tim = 0;
			}
		}
		else if (grabseq >= 0)
		{
			if (gridlockmode)
			{
				seq[grabseq].tim += (seqzoomspti3[seqzoom]>>1);
				seq[grabseq].tim -= (seq[grabseq].tim%seqzoomspti3[seqzoom]);
			}
			for(i=seqnum-1;i>=0;i--)
				if ((i != grabseq) && (seq[i].tim == seq[grabseq].tim))
					{ seqnum--; seq[i] = seq[seqnum]; }
			seqsort();
			grabseq = -1;
		}

		if (bstatus&2)
		{
			if ((~obstatus)&2)
			{
				j = 0x7fffffff;
				for(i=0;i<seqnum;i++)
					if (labs(seq[i].tim-seqcur) < j)
						{ j = labs(seq[i].tim-seqcur); k = i; }
				if ((j < (seqzoomsptic[seqzoom]>>1)) && (seq[k].frm < 0))
					grabseq2 = k; else grabseq2 = -1;
			}
			if (grabseq2 >= 0)
			{
				j = 0x7fffffff;
				for(i=0;i<seqnum;i++)
					if (labs(seq[i].tim-seqcur) < j)
						{ j = labs(seq[i].tim-seqcur); k = i; }
				if (j < (seqzoomsptic[seqzoom]>>1))
				{
					seq[grabseq2].frm = ~k;
					//if (k == grabseq2) seq[grabseq2].frm = ~k;
					//              else seq[grabseq2].frm = ~grabseq2;
				}
			}
			else kfatim = seqcur;
		} else grabseq2 = 0;

		if (!gridlockmode)
		{
			f = fsynctics * 128.0;
			if (keystatus[0x2a]) f *= 0.25;
			if (keystatus[0x36]) f *= 4.0;

			ftol(seqzoomsppix[seqzoom]*f,&i); if (i <= 0) i = 1;
			if (keystatus[0xcb]) seqcur -= i;
			if (keystatus[0xcd]) seqcur += i;
		}
		else
		{
			if (keystatus[0xcb])
			{
				keystatus[0xcb] = 0;
				seqcur--;
				seqcur -= (seqcur%seqzoomsptic[seqzoom]);
			}
			if (keystatus[0xcd])
			{
				keystatus[0xcd] = 0;
				seqcur += seqzoomsptic[seqzoom];
				seqcur -= (seqcur%seqzoomsptic[seqzoom]);
			}
		}
		if (seqcur < 0) seqcur = 0;

		i = seqzoomsppix[seqzoom]*xdim;
			//Decide whether kfatim or seqcur stays in bounds
		if ((seqnum > 0) && ((unsigned long)kfatim < seq[seqnum-1].tim))
			j = kfatim; else j = seqcur;
		if (j < seqoffs+   (i>>3) ) seqoffs = j-   (i>>3);
		if (j > seqoffs+(i-(i>>3))) seqoffs = j-(i-(i>>3));
	}

	tptr = 0;
	if (velcron >= 0)
	{
		switch(velcron)
		{
			case 0: tptr = &hinge[velcroi].p[0]; break;
			case 1: tptr = &hinge[velcroi].v[0]; break;
			case 2: tptr = &hinge[velcroi].p[1]; break;
			case 3: tptr = &hinge[velcroi].v[1]; break;
		}
		if (velcron&1)
		{
			f = 1.0 / sqrt(tptr->x*tptr->x + tptr->y*tptr->y + tptr->z*tptr->z);
			tptr->x *= f; tptr->y *= f; tptr->z *= f;
		}
	}
	else if (grabspr >= 0) tptr = &spr[grabspr].p;

	while (key = keyread())
		switch ((key>>8)&255)
		{
			case 0x15: fmousymul = -fmousymul; break; //Y
			case 0x49: //KP9
			case 0x0a: //9
				if (grabspr >= 0)
				{
					tp.z = 0;
					if (fabs(istr.x) > fabs(istr.y))
						  { tp.y = 0; if (istr.x < 0) tp.x = 1; else tp.x = -1; }
					else { tp.x = 0; if (istr.y < 0) tp.y = 1; else tp.y = -1; }
					f = PI/36.0;
					axisrotate(&spr[grabspr].s,&tp,f);
					axisrotate(&spr[grabspr].h,&tp,f);
					axisrotate(&spr[grabspr].f,&tp,f);
				}
				break;
			case 0x4d: //KP6
			case 0x18: //O
				if (grabspr >= 0)
				{
					tp.z = 0;
					if (fabs(istr.x) > fabs(istr.y))
						  { tp.y = 0; if (istr.x < 0) tp.x = -1; else tp.x = 1; }
					else { tp.x = 0; if (istr.y < 0) tp.y = -1; else tp.y = 1; }
					f = PI/36.0;
					axisrotate(&spr[grabspr].s,&tp,f);
					axisrotate(&spr[grabspr].h,&tp,f);
					axisrotate(&spr[grabspr].f,&tp,f);
				}
				break;
			case 0x47: //KP7
			case 0x08: //7
				if (grabspr >= 0)
				{
					tp.x = 0; tp.y = 0; tp.z = -1;
					f = PI/36.0;
					axisrotate(&spr[grabspr].s,&tp,f);
					axisrotate(&spr[grabspr].h,&tp,f);
					axisrotate(&spr[grabspr].f,&tp,f);
				}
				break;
			case 0x48: //KP8
			case 0x09: //8
				if (grabspr >= 0)
				{
					tp.x = 0; tp.y = 0; tp.z = 1;
					f = PI/36.0;
					axisrotate(&spr[grabspr].s,&tp,f);
					axisrotate(&spr[grabspr].h,&tp,f);
					axisrotate(&spr[grabspr].f,&tp,f);
				}
				break;
			case 0x4f: //KP1
			case 0x24: //J
				if ((velcron == 1) || (velcron == 3))
					{ tp.x = 0; tp.y = 0; tp.z = 1; axisrotate(tptr,&tp,PI/36.0); }
				else if (tptr)
				{
					if (fabs(ifor.x) < fabs(ifor.y))
						  { if (ifor.y < 0) tptr->x -= .5; else tptr->x += .5; }
					else { if (ifor.x < 0) tptr->y += .5; else tptr->y -= .5; }
				}
				break;
			case 0x26: //L
				if (key&0x300000) //ALT-L
				{
					ddflip2gdi();
					if (v = (char *)loadfileselect("Load animation or model...","Animation (.KFA), 3D voxel model (.KV6)\0*.kfa;*.kv6\0All files (*.*)\0*.*\0\0","KFA"))
					{
						strcpy(tempbuf,(char *)v);
						i = strlen(tempbuf);
						if ((i > 0) && (tempbuf[i-1] == '6'))
							loadsplitkv6(tempbuf);
						else
							loadkfa(tempbuf);
					}
					break;
				}
				//No break intentional!
			case 0x51: //KP3
				if ((velcron == 1) || (velcron == 3))
					{ tp.x = 0; tp.y = 0; tp.z = 1; axisrotate(tptr,&tp,PI/-36.0); }
				else if (tptr)
				{
					if (fabs(ifor.x) < fabs(ifor.y))
						  { if (ifor.y < 0) tptr->x += .5; else tptr->x -= .5; }
					else { if (ifor.x < 0) tptr->y -= .5; else tptr->y += .5; }
				}
				break;
			case 0x4c: //KP5
			case 0x17: //I
				if ((velcron == 1) || (velcron == 3))
				{
					if (tptr->z > -.9999)
					{
						if (tptr->z > .9999)
						{
							tptr->x = 0; tptr->y = 0; tptr->z = 1;
							tp.x = 0; tp.y = -1; tp.z = 0;
							axisrotate(tptr,&tp,PI/-36.0);
						}
						else
						{
							tp.x = tptr->y; tp.y = -tptr->x; tp.z = 0;
							axisrotate(tptr,&tp,PI/-36.0);
						}
					}
				}
				else if (tptr)
				{
					if (fabs(ifor.x) < fabs(ifor.y))
						  { if (ifor.y < 0) tptr->y -= .5; else tptr->y += .5; }
					else { if (ifor.x < 0) tptr->x -= .5; else tptr->x += .5; }
				}
				break;
			case 0x50: //KP2
			case 0x25: //K
				if ((velcron == 1) || (velcron == 3))
				{
					if (tptr->z < .9999)
					{
						if (tptr->z < -.9999)
						{
							tptr->x = 0; tptr->y = 0; tptr->z = -1;
							tp.x = 0; tp.y = -1; tp.z = 0;
							axisrotate(tptr,&tp,PI/36.0);
						}
						else
						{
							tp.x = tptr->y; tp.y = -tptr->x; tp.z = 0;
							axisrotate(tptr,&tp,PI/36.0);
						}
					}
				}
				else if (tptr)
				{
					if (fabs(ifor.x) < fabs(ifor.y))
						  { if (ifor.y < 0) tptr->y += .5; else tptr->y -= .5; }
					else { if (ifor.x < 0) tptr->x += .5; else tptr->x -= .5; }
				}
				break;
			case 0x4b: //KP4
			case 0x16: //U
				if ((velcron == 1) || (velcron == 3))
				{
					if (tptr->z > -.9999)
					{
						if (tptr->z > .9999)
						{
							tptr->x = 0; tptr->y = 0; tptr->z = 1;
							tp.x = 0; tp.y = -1; tp.z = 0;
							axisrotate(tptr,&tp,PI/-36.0);
						}
						else
						{
							tp.x = tptr->y; tp.y = -tptr->x; tp.z = 0;
							axisrotate(tptr,&tp,PI/-36.0);
						}
					}
				}
				else if (tptr) tptr->z -= .5;
				break;
			case 0x9c: //KPEnter
			case 0x27: //;
				if ((velcron == 1) || (velcron == 3))
				{
					if (tptr->z < .9999)
					{
						if (tptr->z < -.9999)
						{
							tptr->x = 0; tptr->y = 0; tptr->z = -1;
							tp.x = 0; tp.y = -1; tp.z = 0;
							axisrotate(tptr,&tp,PI/36.0);
						}
						else
						{
							tp.x = tptr->y; tp.y = -tptr->x; tp.z = 0;
							axisrotate(tptr,&tp,PI/36.0);
						}
					}
				}
				else if (tptr) tptr->z += .5;
				break;
			case 0x35: // /
				if ((velcron == 1) || (velcron == 3))
				{
					if ((fabs(tptr->z) >= fabs(tptr->x)) && (fabs(tptr->z) >= fabs(tptr->y)))
						{ tptr->x = 0; tptr->y = 0; if (tptr->z >= 0) tptr->z = 1; else tptr->z = -1; }
					else if (fabs(tptr->y) > fabs(tptr->x))
						{ tptr->x = 0; tptr->z = 0; if (tptr->y >= 0) tptr->y = 1; else tptr->y = -1; }
					else
						{ tptr->y = 0; tptr->z = 0; if (tptr->x >= 0) tptr->x = 1; else tptr->x = -1; }
				}
				else if (velcron < 0)
				{
					if (setlimode >= 0)
					{
						hinge[setlimode].vmin = hinge[setlimode].vmax = 0;
					}
					else if (grabspr >= 0)
					{
						if (hinge[grabspr].parent < 0)
						{
							spr[grabspr].s.x = 1; spr[grabspr].s.y = 0; spr[grabspr].s.z = 0;
							spr[grabspr].h.x = 0; spr[grabspr].h.y = 1; spr[grabspr].h.z = 0;
							spr[grabspr].f.x = 0; spr[grabspr].f.y = 0; spr[grabspr].f.z = 1;
						}
						else
						{
								//Set it as close as possible to 0
							if ((((long)hinge[grabspr].vmin)&65535) >= (((long)hinge[grabspr].vmax)&65535))
								frmval[curfrm][grabspr] = 0;
							else if (labs(hinge[grabspr].vmax^32768) >= labs(hinge[grabspr].vmin^32768))
								frmval[curfrm][grabspr] = hinge[grabspr].vmin;
							else
								frmval[curfrm][grabspr] = hinge[grabspr].vmax;

							k = hinge[grabspr].parent;
							if (k >= numspr)
							{
									//Set it as close as possible to 0
								if ((((long)hinge[k].vmin)&65535) >= (((long)hinge[k].vmax)&65535))
									frmval[curfrm][k] = 0;
								else if (labs(hinge[k].vmax^32768) >= labs(hinge[k].vmin^32768))
									frmval[curfrm][k] = hinge[k].vmin;
								else
									frmval[curfrm][k] = hinge[k].vmax;
							}
						}
					}
					else
					{
						for(i=numhin-1;i>=0;i--)
						{
								//Set it as close as possible to 0
							if ((((long)hinge[i].vmin)&65535) >= (((long)hinge[i].vmax)&65535))
								frmval[curfrm][i] = 0;
							else if (labs(hinge[i].vmax^32768) >= labs(hinge[i].vmin^32768))
								frmval[curfrm][i] = hinge[i].vmin;
							else
								frmval[curfrm][i] = hinge[i].vmax;
						}
					}
				}
				break;
			case 0x0f: //Tab
				if (grabspr >= 0)
				{
					if (key&0xc0000) //CTRL-Tab to swap limbs (grabspr with j)
					{
						j = -1; f = 1e32;
						for(i=numspr-1;i>=0;i--)
						{
							sprhitscan(&ipos,&ifor,&spr[i],&lp,&kp,&f);
							if (kp) j = i;
						}
						if ((j >= 0) && (j != grabspr))
						{
							for(i=0;i<numhin;i++)
							{
								if (hinge[i].parent == grabspr)
									hinge[i].parent = j;
								else if (hinge[i].parent == j)
									hinge[i].parent = grabspr;
							}
							hinge[numhin] = hinge[j];
							hinge[j] = hinge[grabspr];
							hinge[grabspr] = hinge[numhin];
							kfasorthinge(hinge,numhin,hingesort);
						}
					}
					else
					{
						if (((unsigned long)hinge[grabspr].parent) < numspr)
							fliphinge(grabspr);
						else
						{
								  if (hinge[grabspr].v[0].x > 0) i = 1;
							else if (hinge[grabspr].v[0].x < 0) i = 0;
							else if (hinge[grabspr].v[0].y > 0) i = 1;
							else if (hinge[grabspr].v[0].y < 0) i = 0;
							else if (hinge[grabspr].v[0].z > 0) i = 1;
							else                                i = 0;
								  if (hinge[hinge[grabspr].parent].v[0].x > 0) i |= 2;
							else if (hinge[hinge[grabspr].parent].v[0].x < 0) i |= 0;
							else if (hinge[hinge[grabspr].parent].v[0].y > 0) i |= 2;
							else if (hinge[hinge[grabspr].parent].v[0].y < 0) i |= 0;
							else if (hinge[hinge[grabspr].parent].v[0].z > 0) i |= 2;
							if ((i-1)&2) fliphinge(grabspr);
									  else fliphinge(hinge[grabspr].parent);
						}
					}
				}
				break;
			case 0x2b: //Backslash
				if (grabspr >= 0) { hinge[grabspr].parent = -1; kfasorthinge(hinge,numhin,hingesort); }
				break;
			case 0x22: //G
				gridlockmode ^= 1;
				break;
			case 0x39: //Space
				if (seqmode)
				{
					if (!gridlockmode) j = seqzoomsppix[seqzoom];
									  else j = seqzoomspti3[seqzoom];
					for(i=0;i<seqnum;i++)
						if (seq[i].tim >= seqcur) seq[i].tim += j;
				}
				else
				{
					j = -1; f = 1e32;
					for(i=numspr-1;i>=0;i--)
					{
						sprhitscan(&ipos,&ifor,&spr[i],&lp,&kp,&f);
						if (kp) { j = i; grablp = lp; }
					}
					switch(velcron)
					{
						case -1:
							if (j >= 0)
							{
								velcroi = j;
								hinge[velcroi].p[0].x = (float)grablp.x;
								hinge[velcroi].p[0].y = (float)grablp.y;
								hinge[velcroi].p[0].z = (float)grablp.z;
								velcron = 0;
							}
							break;
						case 0:
							hinge[velcroi].v[0].x = (float)grablp.x-hinge[velcroi].p[0].x;
							hinge[velcroi].v[0].y = (float)grablp.y-hinge[velcroi].p[0].y;
							hinge[velcroi].v[0].z = (float)grablp.z-hinge[velcroi].p[0].z;

							tptr = &hinge[velcroi].v[0];
							if ((fabs(tptr->z) >= fabs(tptr->x)) && (fabs(tptr->z) >= fabs(tptr->y)))
								{ tptr->x = 0; tptr->y = 0; if (tptr->z >= 0) tptr->z = 1; else tptr->z = -1; }
							else if (fabs(tptr->y) > fabs(tptr->x))
								{ tptr->x = 0; tptr->z = 0; if (tptr->y >= 0) tptr->y = 1; else tptr->y = -1; }
							else
								{ tptr->y = 0; tptr->z = 0; if (tptr->x >= 0) tptr->x = 1; else tptr->x = -1; }

							velcron = 1;
							break;
						case 1:
							if ((j >= 0) && (j != velcroi))
							{
								hinge[velcroi].parent = j;
								hinge[velcroi].p[1].x = (float)grablp.x;
								hinge[velcroi].p[1].y = (float)grablp.y;
								hinge[velcroi].p[1].z = (float)grablp.z;
								velcron = 2;
							}
							break;
						case 2:
							hinge[velcroi].v[1] = hinge[velcroi].v[0];
							velcron = 3;
							break;
						case 3:
							hinge[velcroi].htype = 0;
							hinge[velcroi].vmin = 0;
							hinge[velcroi].vmax = 0;
							velcron = -1;
							kfasorthinge(hinge,numhin,hingesort);
							break;
					}
					grabspr = setlimode = -1;
				}
				break;
			case 0xe: //Backspace
				if (seqmode)
				{
					if (!gridlockmode) j = seqzoomsppix[seqzoom];
									  else j = seqzoomspti3[seqzoom];

					if (j > seqcur) j = seqcur;
					for(i=seqnum-1;i>=0;i--)
						if ((seq[i].tim < seqcur) && (seq[i].tim >= seqcur-j))
							j = seqcur-seq[i].tim-1;

					for(i=0;i<seqnum;i++)
						if (seq[i].tim >= seqcur) seq[i].tim -= j;
					seqcur -= j;
				}
				else if (velcron >= 0)
				{
					if (velcron == 2) hinge[velcroi].parent = -1;
					velcron--;
				}
				break;
			case 0xc9: //PGUP
				if (curfrm > 0)
				{
					if (curfrm == numfrm-1)
					{     //If last frame is same as one before it, remove it
						for(i=numhin-1;i>=0;i--)
							if (frmval[numfrm-1][i] != frmval[numfrm-2][i]) break;
						if (i < 0) numfrm--;
					}
					curfrm--;
				}
				break;
			case 0xd1: //PGDN
				if (curfrm < MAXFRM-1)
				{
					curfrm++;
					if (curfrm >= numfrm) //New frame: copy last frame
					{
						for(i=numhin-1;i>=0;i--) frmval[curfrm][i] = frmval[curfrm-1][i];
						if (numfrm >= 2) //Make sure last 2 frames are different
						{
							for(i=numhin-1;i>=0;i--)
								if (frmval[numfrm-1][i] != frmval[numfrm-2][i]) break;
							if (i < 0) curfrm--; else numfrm++;
						} else numfrm++;
					}
				}
				break;
			case 0x4a: //KP-
			case 0x0c: //- (dec sequence frame under cursor)
				if (seqmode)
				{
					j = 0x7fffffff;
					for(i=0;i<seqnum;i++)
						if (labs(seq[i].tim-seqcur) < j)
							{ j = labs(seq[i].tim-seqcur); k = i; }
					if (j < (seqzoomsptic[seqzoom]>>1))
						if (seq[k].frm > 0) seq[k].frm--;
				}
				break;
			case 0x4e: //KP+
			case 0x0d: //= (inc sequence frame under cursor)
				if (seqmode)
				{
					j = 0x7fffffff;
					for(i=0;i<seqnum;i++)
						if (labs(seq[i].tim-seqcur) < j)
							{ j = labs(seq[i].tim-seqcur); k = i; }
					if (j < (seqzoomsptic[seqzoom]>>1))
						if (seq[k].frm < numfrm-1) seq[k].frm++;
				}
				break;
			case 0xd2: //Insert
				if (seqmode) //Insert sequence marker
				{     //insert sequence
					j = seqcur;
					if (gridlockmode)
					{
						j += (seqzoomspti3[seqzoom]>>1);
						j -= (j%seqzoomspti3[seqzoom]);
					}
					for(i=seqnum-1;i>=0;i--)
						if (seq[i].tim == j)
							{ seq[i].frm = curfrm; break; }
					if (i < 0)
					{
						for(i=seqnum;(i > 0) && (seq[i-1].tim >= j);i--)
							seq[i] = seq[i-1];
						seq[i].tim = j;
						seq[i].frm = curfrm;
						seqnum++;

						for(k=seqnum;k>=0;k--)
							if ((seq[k].frm < 0) && ((~seq[k].frm) >= i)) seq[k].frm--;
					}
				}
				else if (grabspr >= 0) //Insert fake hinge (for ball joints)
				{
					if ((unsigned long)hinge[grabspr].parent < numspr)
					{
						hinge[numhin].parent = hinge[grabspr].parent;
						hinge[grabspr].parent = numhin;
						hinge[numhin].p[0].x = hinge[numhin].p[0].y = hinge[numhin].p[0].z = 0;
						hinge[numhin].p[1] = hinge[grabspr].p[1];

						hinge[numhin].v[0].x = 1;
						hinge[numhin].v[0].y = 0;
						hinge[numhin].v[0].z = 0;

						hinge[numhin].v[1] = hinge[numhin].v[0];
						hinge[grabspr].p[1].x = hinge[grabspr].p[1].y = hinge[grabspr].p[1].z = 0;
						hinge[numhin].vmin = hinge[numhin].vmax = 0;
						hinge[numhin].htype = 0;
						spr[numhin].voxnum = 0;
						numhin++;
						kfasorthinge(hinge,numhin,hingesort);
					}
					else
					{
						i = hinge[grabspr].parent;
						if (hinge[i].v[0].x != 0)
							{ hinge[i].v[0].x = 0; hinge[i].v[0].y = 1; hinge[i].v[0].z = 0; }
						else if (hinge[i].v[0].y != 0)
							{ hinge[i].v[0].x = 0; hinge[i].v[0].y = 0; hinge[i].v[0].z = 1; }
						else
							{ hinge[i].v[0].x = 1; hinge[i].v[0].y = 0; hinge[i].v[0].z = 0; }
						hinge[i].v[1] = hinge[i].v[0];
					}
				}
				else //Insert frame
				{
					numfrm++;
					for(j=numfrm-1;j>curfrm;j--)
						for(i=numhin-1;i>=0;i--)
							frmval[j][i] = frmval[j-1][i];
					for(i=seqnum-1;i>=0;i--)
						if (seq[i].frm >= curfrm) seq[i].frm++;
					curfrm++;
				}
				break;
			case 0xd3: //Delete
				if (seqmode) //Delete sequence mark
				{
					j = 0x7fffffff;
					for(i=0;i<seqnum;i++)
						if (labs(seq[i].tim-seqcur) < j)
							{ j = labs(seq[i].tim-seqcur); k = i; }
					if (j < (seqzoomsptic[seqzoom]>>1))
					{
						seqnum--;
						for(i=k;i<seqnum;i++) seq[i] = seq[i+1];

						for(i=seqnum-1;i>=0;i--)
							if ((seq[i].frm < 0) && ((~seq[i].frm) >= k)) seq[i].frm++;
					}
				}
				else if (numfrm > 1) //Delete frame
				{
					numfrm--;
					for(j=curfrm;j<numfrm;j++)
						for(i=numhin-1;i>=0;i--)
							frmval[j][i] = frmval[j+1][i];
					for(i=seqnum-1;i>=0;i--)
						if (seq[i].frm > curfrm) seq[i].frm--;
					if (curfrm >= numfrm) curfrm--;
				}
				break;
			case 0x42: //F8: change video mode
				changeres(xres,yres,colbits,fullscreen^1);
				break;
			case 0x1c: //LEnter
				if (key&0xc0000)
				{     //Use CTRL-ENTER to toggle mouse acquire in windowed mode
					static long macq = 1;
					setacquire(macq^=1,1);
					break;
				}
				if ((seqnum > 0) && ((unsigned long)kfatim < seq[seqnum-1].tim)) kfatim = -1; else kfatim = seqcur;
				break;
			case 0xc7: //Home (play from cursor)
				if (seqmode)
				{
					for(i=seqnum-1;i>=0;i--) if (seq[i].tim < seqcur) break;
					okfatim = kfatim; kfatim = seq[i].tim;
				}
				break;
			case 0xcf: //End (insert stop marker)
				if (seqmode)
				{     //insert sequence
					j = seqcur;
					if (gridlockmode)
					{
						j += (seqzoomspti3[seqzoom]>>1);
						j -= (j%seqzoomspti3[seqzoom]);
					}
					for(i=seqnum-1;i>=0;i--)
						if (seq[i].tim == j)
							{ seq[i].frm = ~i; break; }
					if (i < 0)
					{
						for(i=seqnum;(i > 0) && (seq[i-1].tim >= j);i--)
							seq[i] = seq[i-1];
						seq[i].tim = j;
						seq[i].frm = ~i;
						seqnum++;

						for(k=seqnum;k>=0;k--)
							if ((seq[k].frm < 0) && ((~seq[k].frm) >= i)) seq[k].frm--;
					}
				}
				break;
			case 0x29: //`
				seqmode ^= 1;
				grabspr = grabseq = setlimode = -1;
				break;
			case 0xb5: //KP/
				if (seqzoom > 0)
				{
					seqoffs = seqcur - scale(seqcur-seqoffs,seqzoomsppix[seqzoom-1],seqzoomsppix[seqzoom]);
					seqzoom--;
				}
				break;
			case 0x37: //KP*
				if (seqzoom < (sizeof(seqzoomsppix)/sizeof(seqzoomsppix[0]))-1)
				{
					seqoffs = seqcur - scale(seqcur-seqoffs,seqzoomsppix[seqzoom+1],seqzoomsppix[seqzoom]);
					seqzoom++;
				}
				break;
			case 0x1f: //S
				if (key&0x300000) //ALT-S
				{
					ddflip2gdi();
					if (kv6toocomplex)
						MessageBox(ghwnd,"WARNING: KV6 too complex for animation",prognam,MB_OK);
					else
					{
						if (v = (char *)savefileselect("SAVE animation...","*.KFA\0*.kfa\0All files (*.*)\0*.*\0\0","KFA"))
							savekfa(v);
					}
				}
				break;
			default:;
		}

	if (!seqmode)
	{
		if (bstatus&1)
		{
			if ((~obstatus)&1)
			{
				grabspr = -1; f = 1e32;
				for(i=numspr-1;i>=0;i--)
				{
					sprhitscan(&ipos,&ifor,&spr[i],&lp,&kp,&f);
					if (kp) { grabspr = i; grablp = lp; }
				}
			}
			if (grabspr >= 0)
			{
				if (fabs(hinge[grabspr].v[0].z) > 0.707)
					  { i = fmousx*256.0; j = fmousy*256.0; }
				else { i = fmousy*256.0; j = fmousx*256.0; }

				for(y=0;y<2;y++)
				{
					if (!y) { k = grabspr; x = i; }
						else { k = hinge[grabspr].parent; if (k < numspr) break; x = j; }

					if (hinge[k].vmin == hinge[k].vmax) //No restriction
						frmval[curfrm][k] += x;
					else
					{
						if (x < 0)
						{
							x = -x; if (x >= 65536) x = 65535;
							if ((((long)(frmval[curfrm][k]-x-hinge[k].vmin))&65535) < (((long)(frmval[curfrm][k]-hinge[k].vmin))&65535))
								frmval[curfrm][k] -= x;
							else
								frmval[curfrm][k] = hinge[k].vmin;
						}
						else if (x > 0)
						{
							if (x >= 65536) x = 65535;
							if ((((long)(frmval[curfrm][k]+x-hinge[k].vmax))&65535) > (((long)(frmval[curfrm][k]-hinge[k].vmax))&65535))
								frmval[curfrm][k] += x;
							else
								frmval[curfrm][k] = hinge[k].vmax-1;
						}
					}
				}
			}
		}

		if (bstatus&2) //set hinge limits
		{
			if ((~obstatus)&2)
			{
				setlimode = -1; f = 1e32;
				for(i=numspr-1;i>=0;i--)
				{
					sprhitscan(&ipos,&ifor,&spr[i],&lp,&kp,&f);
					if (kp) { setlimode = i; grablp = lp; }
				}
				if (setlimode >= 0)
				{
					hinge[setlimode].vmin = frmval[curfrm][setlimode];
					hinge[setlimode].vmax = frmval[curfrm][setlimode]+1;
					k = hinge[setlimode].parent;
					if (k >= numspr)
					{
						hinge[k].vmin = frmval[curfrm][k];
						hinge[k].vmax = frmval[curfrm][k]+1;
					}
				}
			}
			if (setlimode >= 0)
			{
				if (fabs(hinge[grabspr].v[0].z) > 0.707)
					  { i = fmousx*256.0; j = fmousy*256.0; }
				else { i = fmousy*256.0; j = fmousx*256.0; }

				for(y=0;y<2;y++)
				{
					if (!y) { k = setlimode; x = i; }
						else { k = hinge[setlimode].parent; if (k < 0) break; x = j; }

					frmval[curfrm][k] += x;

						//if current angle is out of the vmin/vmax bounds...
					z = (((long)(hinge[k].vmax-hinge[k].vmin))&65535);
					if (z != 0)
					{
						if ((((long)(frmval[curfrm][k]-hinge[k].vmin))&65535) >= z)
						{     //adjust bound that's closer to current angle
							if ((((long)(hinge[k].vmin-frmval[curfrm][k]))&65535) <
								 (((long)(frmval[curfrm][k]-hinge[k].vmax))&65535))
								hinge[k].vmin = frmval[curfrm][k];
							else
								hinge[k].vmax = frmval[curfrm][k]+1;
							if ((((long)(hinge[k].vmax-hinge[k].vmin))&65535) < z)
								{ hinge[k].vmin = hinge[k].vmax = 0; }
						}
					}
				}
			}
		} else if (setlimode >= 0) setlimode = -1;
	}
}

#if 0
!endif
#endif
