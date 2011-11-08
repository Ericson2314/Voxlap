/***************************************************************************************************
WINMAIN.CPP & SYSMAIN.H

Windows layer code written by Ken Silverman (http://advsys.net/ken) (1997-2009)
Additional modifications by Tom Dobrowolski (http://ged.ax.pl/~tomkh)
You may use this code for non-commercial purposes as long as credit is maintained.
***************************************************************************************************/

//#define USED3D4FULL
//#define USE3DVISION

	//To compile, link with ddraw.lib, dinput.lib AND dxguid.lib.  Be sure ddraw.h and dinput.h
	//are in include path
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifndef NODRAW
#define DIRECTDRAW_VERSION 0x0300
#include <ddraw.h>
#endif
#ifndef NOINPUT
#define DIRECTINPUT_VERSION 0x0300
#include <dinput.h>
#endif
#ifndef NOSOUND
#define DIRECTSOUND_VERSION 0x0300
#include <mmsystem.h> //dsound.h requires this when using LEAN_AND_MEAN
#include <dsound.h>
#ifndef USEKZ
#include <stdio.h> //for fopen
#else
extern int kzopen (const char *);
extern int kzread (void *, int);
extern int kzseek (int, int);
extern void kzclose ();
extern int kzeof ();
#endif
#define DSOUNDINITCOM 1 //0=Link DSOUND.DLL, 1=Use COM interface to init DSOUND
#ifndef USEKENSOUND
#define USEKENSOUND 1   //0=playsound undefined (umixer only), 1=Ken's fancy sound renderer, 2=Standard Directsound secondary buffers
#endif
#define USETHREADS 20   //0=Disable, nonzero=enable multithreading and this is sleep value (20 is good)
								//Use threads to fix audio gaps (and possibly other bugs too!)
#else
#define USETHREADS 0
#endif

#if ((USEKENSOUND == 1) && (USETHREADS != 0))
	//WinXP:Sleep(0)=infinite,Sleep(1-16)=64hz,Sleep(17-32)=32hz,Sleep(33-48)=21.3hz, etc...)
	//Remember to add /MD, /MT or /link libcmt.lib kernel32.lib /nodefaultlib to compile options!
#include <process.h>
static HANDLE hmutx;
#define ENTERMUTX WaitForSingleObject(hmutx,USETHREADS) //Do NOT use ,INFINITE) - too dangerous!
#define LEAVEMUTX ReleaseMutex(hmutx)
#else
#define ENTERMUTX
#define LEAVEMUTX
#endif

#pragma warning(disable:4730)
#pragma warning(disable:4731)

char *prognam = "WinMain App (by KS/TD)";
long progresiz = 1;
long progwndflags, progwndx = 0x80000000, progwndy, progwndadd[2];
#ifndef NOINPUT
long dinputkeyboardflags = DISCL_NONEXCLUSIVE|DISCL_FOREGROUND;
long dinputmouseflags = DISCL_EXCLUSIVE|DISCL_FOREGROUND;
#endif

#define WM_MOUSEWHEEL 0x020A

extern long initapp (long argc, char **argv);
extern void uninitapp ();
extern void doframe ();

static long quitprogram = 0, quitparam;
void breath();

	//Global window variables
static WNDCLASS wc;
HWND ghwnd;
HINSTANCE ghinst, ghpinst;
LPSTR gcmdline;
int gncmdshow;

long ActiveApp = 1, alwaysactive = 0;
long xres = 640, yres = 480, colbits = 32, fullscreen = 0, maxpages = 8;

	// Note! Without clipper blit is faster but only with software bliter
//#define NO_CLIPPER

//======================== CPU detection code begins ========================

long cputype = 0;
static OSVERSIONINFO osvi;

#ifdef __WATCOMC__

long testflag (long);
#pragma aux testflag =\
	"pushfd"\
	"pop eax"\
	"mov ebx, eax"\
	"xor eax, ecx"\
	"push eax"\
	"popfd"\
	"pushfd"\
	"pop eax"\
	"xor eax, ebx"\
	"mov eax, 1"\
	"jne menostinx"\
	"xor eax, eax"\
	"menostinx:"\
	parm nomemory [ecx]\
	modify exact [eax ebx]\
	value [eax]

void cpuid (long, long *);
#pragma aux cpuid =\
	".586"\
	"cpuid"\
	"mov dword ptr [esi], eax"\
	"mov dword ptr [esi+4], ebx"\
	"mov dword ptr [esi+8], ecx"\
	"mov dword ptr [esi+12], edx"\
	parm [eax][esi]\
	modify exact [eax ebx ecx edx]\
	value

#endif
#ifdef _MSC_VER

#pragma warning(disable:4799) //I know how to use EMMS

static _inline long testflag (long c)
{
	_asm
	{
		mov ecx, c
		pushfd
		pop eax
		mov edx, eax
		xor eax, ecx
		push eax
		popfd
		pushfd
		pop eax
		xor eax, edx
		mov eax, 1
		jne menostinx
		xor eax, eax
		menostinx:
	}
}

static _inline void cpuid (long a, long *s)
{
	_asm
	{
		push ebx
		push esi
		mov eax, a
		cpuid
		mov esi, s
		mov dword ptr [esi+0], eax
		mov dword ptr [esi+4], ebx
		mov dword ptr [esi+8], ecx
		mov dword ptr [esi+12], edx
		pop esi
		pop ebx
	}
}

#endif

	//Bit numbers of return value:
	//0:FPU, 4:RDTSC, 15:CMOV, 22:MMX+, 23:MMX, 25:SSE, 26:SSE2, 30:3DNow!+, 31:3DNow!
static long getcputype ()
{
	long i, cpb[4], cpid[4];
	if (!testflag(0x200000)) return(0);
	cpuid(0,cpid); if (!cpid[0]) return(0);
	cpuid(1,cpb); i = (cpb[3]&~((1<<22)|(1<<30)|(1<<31)));
	cpuid(0x80000000,cpb);
	if (((unsigned long)cpb[0]) > 0x80000000)
	{
		cpuid(0x80000001,cpb);
		i |= (cpb[3]&(1<<31));
		if (!((cpid[1]^0x68747541)|(cpid[3]^0x69746e65)|(cpid[2]^0x444d4163))) //AuthenticAMD
			i |= (cpb[3]&((1<<22)|(1<<30)));
	}
	if (i&(1<<25)) i |= (1<<22); //SSE implies MMX+ support
	return(i);
}

//========================= CPU detection code ends =========================

//================== Fast & accurate TIMER FUNCTIONS begins ==================

#if 0
#ifdef __WATCOMC__

__int64 rdtsc64 ();
#pragma aux rdtsc64 = "rdtsc" value [edx eax] modify nomemory parm nomemory;

#endif
#ifdef _MSC_VER

static __forceinline __int64 rdtsc64 () { _asm rdtsc }

#endif

static __int64 pertimbase, rdtimbase, nextimstep;
static double perfrq, klockmul, klockadd;

void initklock ()
{
	__int64 q;
	QueryPerformanceFrequency((LARGE_INTEGER *)&q);
	perfrq = (double)q;
	rdtimbase = rdtsc64();
	QueryPerformanceCounter((LARGE_INTEGER *)&pertimbase);
	nextimstep = 4194304; klockmul = 0.000000001; klockadd = 0.0;
}

void readklock (double *tim)
{
	__int64 q = rdtsc64()-rdtimbase;
	if (q > nextimstep)
	{
		__int64 p;
		double d;
		QueryPerformanceCounter((LARGE_INTEGER *)&p);
		d = klockmul; klockmul = ((double)(p-pertimbase))/(((double)q)*perfrq);
		klockadd += (d-klockmul)*((double)q);
		do { nextimstep <<= 1; } while (q > nextimstep);
	}
	(*tim) = ((double)q)*klockmul + klockadd;
}
#else

static __int64 klocksub;
static double klockmul;

void initklock ()
{
	__int64 q;
	QueryPerformanceFrequency((LARGE_INTEGER *)&q);
	klockmul = 1.0/((double)q);
	QueryPerformanceCounter((LARGE_INTEGER *)&klocksub);
}

void readklock (double *tim)
{
	__int64 q;
	QueryPerformanceCounter((LARGE_INTEGER *)&q);
	(*tim) = ((double)(q-klocksub))*klockmul;
}

#endif

//=================== Fast & accurate TIMER FUNCTIONS ends ===================

//DirectDraw VARIABLES & CODE---------------------------------------------------------------
#ifndef NODRAW

//============================= D3D code begins  =============================
long g3dnumframes = 0;
#ifdef USED3D4FULL
#pragma comment(lib,"d3d8.lib")
#include <d3d8.h>
static LPDIRECT3D8 d3d = 0;
static LPDIRECT3DDEVICE8 d3ddev = 0;
#ifndef USE3DVISION
static LPDIRECT3DSURFACE8 d3dsur = 0;
#else
#define PHYSSIZ 0.1157385 //Don't change this value!
#define EYESEP (PHYSSIZ*0.80)
#define BACKDIST (PHYSSIZ*0.80)
static LPDIRECT3DVERTEXBUFFER8 gpvb = 0;
static LPDIRECT3DTEXTURE8 gpt[2] = {0};
#define D3DFVF (D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1)
typedef struct { float x, y, z; unsigned long c; float u, v; } vert3d;
#endif
static long d3df = 0, d3dp = 0;

static void d3duninit ()
{
#ifndef USE3DVISION
	if (d3dsur) { d3dsur->Release(); d3dsur = 0; }
#else
	long i;
	if (gpvb) gpvb->Release();
	for(i=2-1;i>=0;i--) if (gpt[i]) gpt[i]->Release();
#endif
	if (d3ddev) { d3ddev->Release(); d3ddev = 0; }
	if (d3d) { d3d->Release(); d3d = 0; }
}

static long d3dinit (HWND hwnd)
{
	D3DPRESENT_PARAMETERS pp;
#ifdef USE3DVISION
	vert3d *p;
	long i;
#endif

	d3d = Direct3DCreate8(120 /*D3D_SDK_VERSION*/); if (!d3d) return(-1); //120 gives DX8.0 compatibility

	memset(&pp,0,sizeof(pp));
	//pp.Windowed = 0; //(default)
	pp.SwapEffect = D3DSWAPEFFECT_FLIP;
		  if (colbits == 32) pp.BackBufferFormat = D3DFMT_X8R8G8B8;
	else if (colbits == 24) pp.BackBufferFormat = D3DFMT_R8G8B8;
	else if (colbits == 16) pp.BackBufferFormat = D3DFMT_R5G6B5;
	else if (colbits == 15) pp.BackBufferFormat = D3DFMT_X1R5G5B5;
	else if (colbits ==  8) pp.BackBufferFormat = D3DFMT_P8;

	pp.BackBufferWidth = xres;
	pp.BackBufferHeight = yres;
	if (maxpages > 2) pp.BackBufferCount = 2;
					 else pp.BackBufferCount = 1;
	pp.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
	//pp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
	pp.FullScreen_RefreshRateInHz = 60;

	if (d3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,hwnd,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,&d3ddev) < 0) return(-1);

	d3ddev->SetRenderState(D3DRS_LIGHTING,0);
#ifndef USE3DVISION
	d3ddev->SetRenderState(D3DRS_ZENABLE,0);

	if (d3ddev->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&d3dsur) < 0) return(-1);
#else
	//pp.EnableAutoDepthStencil = 1;
	//pp.AutoDepthStencilFormat = D3DFMT_D24S8; //D16,D24S8(cdim=32),D16_LOCKABLE(cdim=16);
	//d3ddev->SetRenderState(D3DRS_CULLMODE,D3DCULL_CCW);
	d3ddev->SetRenderState(D3DRS_ZENABLE,1);
		//Set/reset projection to fit screen properly
	D3DMATRIX tmat;
	memset((void *)&tmat,0,sizeof(tmat));
	tmat._11 = tmat._34 = 1.f;
	tmat._33 = 1.f; //maxclip/(maxclip-minclip); //assume maxclip is VERY high!
	tmat._43 =-.001f; //-tmat._33*minclip;
	tmat._22 = ((float)xres)/((float)yres);
	d3ddev->SetTransform(D3DTS_PROJECTION,&tmat);

	for(i=0;i<2;i++) d3ddev->CreateTexture(xres,yres,0,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&gpt[i]);
	if (d3ddev->CreateVertexBuffer(sizeof(vert3d)*4,0,D3DFVF,D3DPOOL_DEFAULT,&gpvb) < 0) return(-1);
	if (gpvb->Lock(0,0,(unsigned char **)&p,0) < 0) return(-1);
	p[0].x = -.8*PHYSSIZ; p[0].y = 0; p[0].z = -.5*PHYSSIZ; p[0].c = 0xffffff; p[0].u = 0; p[0].v = 0;
	p[1].x = +.8*PHYSSIZ; p[1].y = 0; p[1].z = -.5*PHYSSIZ; p[1].c = 0xffffff; p[1].u = 1; p[1].v = 0;
	p[2].x = +.8*PHYSSIZ; p[2].y = 0; p[2].z = +.5*PHYSSIZ; p[2].c = 0xffffff; p[2].u = 1; p[2].v = 1;
	p[3].x = -.8*PHYSSIZ; p[3].y = 0; p[3].z = +.5*PHYSSIZ; p[3].c = 0xffffff; p[3].u = 0; p[3].v = 1;
	gpvb->Unlock();

	d3ddev->Clear(0,0,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xc04040,1.0,0);
	d3ddev->BeginScene();
#endif
	return(0);
}
#endif

PALETTEENTRY pal[256];
long ddrawuseemulation = 0;
long ddrawdebugmode = -1;    // -1 = off, old ddrawuseemulation = on

static __int64 mask8a = 0x00ff00ff00ff00ff;
static __int64 mask8b; //0x10000d0010000d00 (fullscreen), 0x10000c0010000c00 (windowed)
static __int64 mask8c = 0x0000f8000000f800;
static __int64 mask8d = 0xff000000ff000000;
static __int64 mask8e = 0x00ff000000ff0000;
static __int64 mask8f; //0x00d000d000d000d0 (fullscreen), 0x0aca0aca0aca0aca (windowed)
static __int64 mask8g; //0xd000d000d000d000 (fullscreen), 0xca0aca0aca0aca0a (windowed)
static long mask8cnt = 0;
static __int64 mask8h = 0x0007050500070505;
static __int64 mask8lut[8] =
{
	0x0000000000000000,0x0000001300000013,
	0x0000080000000800,0x0000081300000813,
	0x0010000000100000,0x0010001300100013,
	0x0010080000100800,0x0010081300100813
};
static __int64 maskrb15 = 0x00f800f800f800f8;
static __int64 maskgg15 = 0x0000f8000000f800;
static __int64 maskml15 = 0x2000000820000008;
static __int64 maskrb16 = 0x00f800f800f800f8;
static __int64 maskgg16 = 0x0000fc000000fc00;
static __int64 maskml16 = 0x2000000420000004;
static __int64 mask16a  = 0x0000800000008000;
static __int64 mask16b  = 0x8000800080008000;
static __int64 mask24a  = 0x00ffffff00000000;
static __int64 mask24b  = 0x00000000ffffff00;
static __int64 mask24c  = 0x0000000000ffffff;
static __int64 mask24d  = 0xffffff0000000000;

static long lpal[256];

	//Beware of alignment issues!!!
void kblit32 (long rplc, long rbpl, long rxsiz, long rysiz,
				  long wplc, long cdim, long wbpl)
{
#ifdef _MSC_VER
	if ((rxsiz <= 0) || (rysiz <= 0)) return;
	if (colbits == 8)
	{
		long x, y;
		switch(cdim)
		{
			case 15: case 16:
					//for(y=0;y<rysiz;y++)
					//   for(x=0;x<rxsiz;x++)
					//      *(short *)(wplc+y*wbpl+x*2) =
					//      lpal[*(unsigned char *)(rplc+y*rbpl+x)];
				if (cputype&(1<<22)) //MMX+
				{
					_asm
					{
						push ebx
						push esi
						push edi
						mov esi, rplc
						mov edi, wplc
begall8_16:       mov edx, edi
						mov eax, esi
						add esi, rbpl
						add edi, wbpl
						mov ecx, rxsiz
						test edx, 6
						jz short start_8_16
pre_8_16:         movzx ebx, byte ptr [eax]
						mov bx, word ptr lpal[ebx*4]
						mov [edx], bx
						sub ecx, 1
						jz short endall8_16
						add eax, 1
						add edx, 2
						test edx, 6
						jnz short pre_8_16
start_8_16:       sub ecx, 3
						add eax, ecx
						lea edx, [edx+ecx*2]
						neg ecx
						jge short skip8_16
						xor ebx, ebx
beg8_16:          mov bl, byte ptr [eax+ecx]
						movd mm0, lpal[ebx*4]
						mov bl, byte ptr [eax+ecx+1]
						movd mm1, lpal[ebx*4]
						punpcklwd mm0, mm1
						mov bl, byte ptr [eax+ecx+2]
						movd mm1, lpal[ebx*4]
						mov bl, byte ptr [eax+ecx+3]
						movd mm2, lpal[ebx*4]
						punpcklwd mm1, mm2
						punpckldq mm0, mm1
						movntq [edx+ecx*2], mm0
						add ecx, 4
						jl short beg8_16
skip8_16:         sub ecx, 3
						jz short endall8_16
end8_16:          movzx ebx, byte ptr [eax+ecx+3]
						mov bx, word ptr lpal[ebx*4]
						mov [edx+ecx*2+6], bx
						add ecx, 1
						jnz short end8_16
endall8_16:       sub rysiz, 1 ;/
						jnz short begall8_16
						pop edi
						pop esi
						pop ebx
						emms
					}
				}
				else
				{
					_asm
					{
						push ebx
						push esi
						push edi
						mov esi, rplc
						mov edi, wplc
begall8_16b:      mov edx, edi
						mov eax, esi
						add esi, rbpl
						add edi, wbpl
						mov ecx, rxsiz
						test edx, 6
						jz short start_8_16b
pre_8_16b:        movzx ebx, byte ptr [eax]
						mov bx, word ptr lpal[ebx*4]
						mov [edx], bx
						sub ecx, 1
						jz short endall8_16b
						add eax, 1
						add edx, 2
						test edx, 6
						jnz short pre_8_16b
start_8_16b:      sub ecx, 3
						add eax, ecx
						lea edx, [edx+ecx*2]
						neg ecx
						jge short skip8_16b
						xor ebx, ebx
beg8_16b:         mov bl, byte ptr [eax+ecx]
						movd mm0, lpal[ebx*4]
						mov bl, byte ptr [eax+ecx+1]
						movd mm1, lpal[ebx*4]
						punpcklwd mm0, mm1
						mov bl, byte ptr [eax+ecx+2]
						movd mm1, lpal[ebx*4]
						mov bl, byte ptr [eax+ecx+3]
						movd mm2, lpal[ebx*4]
						punpcklwd mm1, mm2
						punpckldq mm0, mm1
						movq [edx+ecx*2], mm0
						add ecx, 4
						jl short beg8_16b
skip8_16b:        sub ecx, 3
						jz short endall8_16b
end8_16b:         movzx ebx, byte ptr [eax+ecx+3]
						mov bx, word ptr lpal[ebx*4]
						mov [edx+ecx*2+6], bx
						add ecx, 1
						jnz short end8_16b
endall8_16b:      sub rysiz, 1 ;/
						jnz short begall8_16b
						pop edi
						pop esi
						pop ebx
						emms
					}
				}
				break;
			case 24:
					//Should work, but slow&ugly! :/
				for(y=0;y<rysiz;y++)
					for(x=0;x<rxsiz;x++)
						*(long *)(wplc+y*wbpl+x*3) = lpal[*(unsigned char *)(rplc+y*rbpl+x)];
				break;
			case 32:
					//for(y=0;y<rysiz;y++)
					//   for(x=0;x<rxsiz;x++)
					//      *(long *)(wplc+y*wbpl+(x<<2)) = lpal[*(unsigned char *)(rplc+y*rbpl+x)];
				if (cputype&(1<<22)) //MMX+
				{
					_asm
					{
						push ebx
						push esi
						push edi
						mov esi, rplc
						mov edi, wplc
begall8_32:       mov edx, edi
						mov eax, esi
						add esi, rbpl
						add edi, wbpl
						mov ecx, rxsiz
						xor ebx, ebx
						test edx, 4
						jz short start_8_32
						mov bl, byte ptr [eax]
						movd mm0, lpal[ebx*4]
						movd [edx], mm0
						sub ecx, 1
						jz short endall8_32
						add eax, 1
						add edx, 4
start_8_32:       sub ecx, 1
						add eax, ecx
						lea edx, [edx+ecx*4]
						neg ecx
						jge short skip8_32
beg8_32:          mov bl, byte ptr [eax+ecx]
						movd mm0, lpal[ebx*4]
						mov bl, byte ptr [eax+ecx+1]
						punpckldq mm0, lpal[ebx*4]
						movntq [edx+ecx*4], mm0
						add ecx, 2
						jl short beg8_32
skip8_32:         cmp ecx, 1
						jz short endall8_32
						movzx ebx, byte ptr [eax]
						movd mm0, lpal[ebx*4]
						movd [edx], mm0
endall8_32:       sub rysiz, 1 ;/
						jnz short begall8_32
						pop edi
						pop esi
						pop ebx
						emms
					}
				}
				else
				{
					_asm
					{
						push ebx
						push esi
						push edi
						mov esi, rplc
						mov edi, wplc
begall8_32b:      mov edx, edi
						mov eax, esi
						add esi, rbpl
						add edi, wbpl
						mov ecx, rxsiz
						xor ebx, ebx
						test edx, 4
						jz short start_8_32b
						mov bl, byte ptr [eax]
						movd mm0, lpal[ebx*4]
						movd [edx], mm0
						sub ecx, 1
						jz short endall8_32b
						add eax, 1
						add edx, 4
start_8_32b:      sub ecx, 1
						add eax, ecx
						lea edx, [edx+ecx*4]
						neg ecx
						jge short skip8_32b
beg8_32b:         mov bl, byte ptr [eax+ecx]
						movd mm0, lpal[ebx*4]
						mov bl, byte ptr [eax+ecx+1]
						punpckldq mm0, lpal[ebx*4]
						movq [edx+ecx*4], mm0
						add ecx, 2
						jl short beg8_32b
skip8_32b:        cmp ecx, 1
						jz short endall8_32b
						movzx ebx, byte ptr [eax]
						movd mm0, lpal[ebx*4]
						movd [edx], mm0
endall8_32b:      sub rysiz, 1 ;/
						jnz short begall8_32b
						pop edi
						pop esi
						pop ebx
						emms
					}
				}
				break;
		}
	}
	else if (colbits == 32)
	{
		switch (cdim)
		{
			case 8:
				rxsiz &= ~7; if (rxsiz <= 0) return;
				if (cputype&(1<<22)) //MMX+
				{
					_asm
					{
						push ebx
						push esi

						movq mm7, mask8a
						movq mm6, mask8b
						movq mm4, mask8d

						mov eax, rplc
						mov edx, wplc
						mov ecx, rxsiz
						mov ebx, rysiz
						shr ecx, 1
						lea eax, [eax+ecx*8]
						lea edx, [edx+ecx*2-8]
						neg ecx
						mov rxsiz, ecx

						mov ecx, mask8cnt
						xor ecx, 1
						mov mask8cnt, ecx
						add ecx, ebx
						test ecx, 1
						je into8b

			 begall8:psubd mm4, mask8h
						pmovmskb esi, mm4
						and esi, 7
						paddd mm4, mask8lut[esi*8]

						mov ecx, rxsiz
						movq mm5, mask8c
				beg8a:
							;src: AaaaaaaaRrrrrrrrGgggggggBbbbbbbbAaaaaaaaRrrrrrrrGgggggggBbbbbbbb
							;dst: RrrGggBbRrrGggBb
						movq mm3, [eax+ecx*8]   ;mm0: A1 R1 G1 B1 A0 R0 G0 B0
						movq mm1, [eax+ecx*8+8] ;mm1: A3 R3 G3 B3 A2 R2 G2 B2
						psubusb mm3, mm4
						psubusb mm1, mm4
						movq mm2, mm3      ;mm2: 00 R1 G1 B1 00 R0 G0 B0
						punpckldq mm3, mm1 ;mm3: 00 R2 G2 B2 A0 R0 G0 B0
						pand mm3, mm7      ;mm3: 00 R2 00 B2 00 R0 00 B0
						pmulhw mm3, mm6    ;mm3: 00 R2 00 B2 00 R0 00 B0 ;16<<8 13<<8 16<<8 13<<8
						punpckhdq mm2, mm1 ;mm2: 00 R3 G3 B3 00 R1 G1 B1
						pand mm2, mm5      ;mm2: 00 00 G3 00 00 00 G1 00
						pslld mm2, 5       ;mm2: 00 G3 00 00 00 G1 00 00
						movq mm1, mm3      ;mm1: 00 R2 00 B2 00 R0 00 B0
						pslld mm1, 20      ;mm1: 00 B2 00 00 00 B0 00 00
						paddd mm3, mm1     ;mm3: 00 C2 00 B2 00 C0 00 B0
						psrld mm3, 16      ;mm3: 00 00 00 C2 00 00 00 C0 ;(B2<<4)+R2  (B0<<4)+R0
						paddd mm3, mm2     ;mm3: 00 G3 00 C2 00 G1 00 C0

						movq mm0, [eax+ecx*8+16] ;mm0: A5 R5 G5 B5 A4 R4 G4 B4
						movq mm1, [eax+ecx*8+24] ;mm1: A7 R7 G7 B7 A6 R6 G6 B6
						psubusb mm0, mm4
						psubusb mm1, mm4
						movq mm2, mm0      ;mm2: 00 R5 G5 B5 00 R4 G4 B4
						punpckldq mm0, mm1 ;mm0: 00 R6 G6 B6 A4 R4 G4 B4
						pand mm0, mm7      ;mm0: 00 R6 00 B6 00 R4 00 B4
						pmulhw mm0, mm6    ;mm0: 00 R6 00 B6 00 R4 00 B4 ;16<<8 13<<8 16<<8 13<<8
						prefetchnta [eax+ecx*8+120]
						punpckhdq mm2, mm1 ;mm2: 00 R7 G7 B7 00 R5 G5 B5
						add ecx, 4
						pand mm2, mm5      ;mm2: 00 00 G7 00 00 00 G5 00
						pslld mm2, 5       ;mm2: 00 G7 00 00 00 G5 00 00
						movq mm1, mm0      ;mm1: 00 R6 00 B6 00 R4 00 B4
						pslld mm1, 20      ;mm1: 00 B6 00 00 00 B4 00 00
						paddd mm0, mm1     ;mm0: 00 C6 00 B6 00 C4 00 B4
						psrld mm0, 16      ;mm0: 00 00 00 C6 00 00 00 C4 ;(B6<<4)+R6  (B4<<4)+R4
						paddd mm0, mm2     ;mm0: 00 G7 00 C6 00 G5 00 C4

						packuswb mm3, mm0  ;mm3: G7 C6 G5 C4 G3 C2 G1 C0
						paddd mm3, mask8g  ;mm3: C7 C6 C5 C4 C3 C2 C1 C0
						movntq [edx+ecx*2], mm3
						js short beg8a

						add eax, rbpl
						add edx, wbpl
						sub ebx, 1
						jz endit8

			  into8b:psubd mm4, mask8h
						pmovmskb esi, mm4
						and esi, 7
						paddd mm4, mask8lut[esi*8]

						mov ecx, rxsiz
						movq mm5, mask8e
				beg8b:
							;src: AaaaaaaaRrrrrrrrGgggggggBbbbbbbbAaaaaaaaRrrrrrrrGgggggggBbbbbbbb
							;dst: RrrGggBbRrrGggBb
						movq mm3, [eax+ecx*8]   ;mm3: A1 R1 G1 B1 A0 R0 G0 B0
						movq mm1, [eax+ecx*8+8] ;mm1: A3 R3 G3 B3 A2 R2 G2 B2
						psubusb mm3, mm4
						psubusb mm1, mm4
						movq mm2, mm3      ;mm2: 00 R1 G1 B1 00 R0 G0 B0
						punpckhdq mm3, mm1 ;mm3: 00 R3 G3 B3 00 R1 G1 B1
						pand mm3, mm7      ;mm3: 00 R3 00 B3 00 R1 00 B1
						pmulhw mm3, mm6    ;mm3: 00 R3 00 B3 00 R1 00 B1 ;16<<8 13<<8 16<<8 13<<8
						punpckldq mm2, mm1 ;mm2: 00 R2 G2 B2 00 R0 G0 B0
						psrlw mm2, 11      ;mm2: 00 00 00 G2 00 00 00 G0
						movq mm1, mm3      ;mm1: 00 R3 00 B3 00 R1 00 B1
						pslld mm1, 20      ;mm1: 00 B3 00 00 00 B1 00 00
						paddd mm3, mm1     ;mm3: 00 C3 00 B3 00 C1 00 B1
						pand mm3, mm5      ;mm3: 00 C3 00 00 00 C1 00 00 ;(B3<<4)+R3  (B1<<4)+R1
						paddd mm3, mm2     ;mm3: 00 C3 00 G2 00 C1 00 G0

						movq mm0, [eax+ecx*8+16] ;mm0: A5 R5 G5 B5 A4 R4 G4 B4
						movq mm1, [eax+ecx*8+24] ;mm1: A7 R7 G7 B7 A6 R6 G6 B6
						psubusb mm0, mm4
						psubusb mm1, mm4
						movq mm2, mm0      ;mm2: 00 R5 G5 B5 00 R4 G4 B4
						punpckhdq mm0, mm1 ;mm0: 00 R7 G7 B7 00 R5 G5 B5
						pand mm0, mm7      ;mm0: 00 R7 00 B7 00 R5 00 B5
						pmulhw mm0, mm6    ;mm0: 00 R7 00 B7 00 R5 00 B5 ;16<<8 13<<8 16<<8 13<<8
						prefetchnta [eax+ecx*8+120]
						punpckldq mm2, mm1 ;mm2: 00 R6 G6 B6 00 R4 G4 B4
						add ecx, 4
						psrlw mm2, 11      ;mm2: 00 00 00 G6 00 00 00 G4
						movq mm1, mm0      ;mm1: 00 R7 00 B7 00 R5 00 B5
						pslld mm1, 20      ;mm1: 00 B7 00 00 00 B5 00 00
						paddd mm0, mm1     ;mm0: 00 C7 00 B7 00 C5 00 B5
						pand mm0, mm5      ;mm0: 00 C7 00 00 00 C5 00 00 ;(B7<<4)+R7  (B5<<4)+R5
						paddd mm0, mm2     ;mm0: 00 C7 00 G6 00 C5 00 G4

						packuswb mm3, mm0  ;mm3: C7 G6 C5 G4 C3 G2 C1 G0
						paddd mm3, mask8f  ;mm3: C7 C6 C5 C4 C3 C2 C1 C0
						movntq [edx+ecx*2], mm3
						js short beg8b

						add eax, rbpl
						add edx, wbpl
						sub ebx, 1
						jnz begall8

			  endit8:psubd mm4, mask8h
						pmovmskb esi, mm4
						and esi, 7
						paddd mm4, mask8lut[esi*8]
						movq mask8d, mm4

						pop esi
						pop ebx
						emms
					}
				}
				else
				{
					_asm
					{
						push ebx
						push esi

						movq mm7, mask8a
						movq mm6, mask8b
						movq mm4, mask8d

						mov eax, rplc
						mov edx, wplc
						mov ecx, rxsiz
						mov ebx, rysiz
						shr ecx, 1
						lea eax, [eax+ecx*8]
						lea edx, [edx+ecx*2-8]
						neg ecx
						mov rxsiz, ecx

						mov ecx, mask8cnt
						xor ecx, 1
						mov mask8cnt, ecx
						add ecx, ebx
						test ecx, 1
						je into8bb

			begall8b:psubd mm4, mask8h
						pmovmskb esi, mm4
						and esi, 7
						paddd mm4, mask8lut[esi*8]

						mov ecx, rxsiz
						movq mm5, mask8c
			  beg8ab:
							;src: AaaaaaaaRrrrrrrrGgggggggBbbbbbbbAaaaaaaaRrrrrrrrGgggggggBbbbbbbb
							;dst: RrrGggBbRrrGggBb
						movq mm3, [eax+ecx*8]   ;mm0: A1 R1 G1 B1 A0 R0 G0 B0
						movq mm1, [eax+ecx*8+8] ;mm1: A3 R3 G3 B3 A2 R2 G2 B2
						psubusb mm3, mm4
						psubusb mm1, mm4
						movq mm2, mm3      ;mm2: 00 R1 G1 B1 00 R0 G0 B0
						punpckldq mm3, mm1 ;mm3: 00 R2 G2 B2 A0 R0 G0 B0
						pand mm3, mm7      ;mm3: 00 R2 00 B2 00 R0 00 B0
						pmulhw mm3, mm6    ;mm3: 00 R2 00 B2 00 R0 00 B0 ;16<<8 13<<8 16<<8 13<<8
						punpckhdq mm2, mm1 ;mm2: 00 R3 G3 B3 00 R1 G1 B1
						pand mm2, mm5      ;mm2: 00 00 G3 00 00 00 G1 00
						pslld mm2, 5       ;mm2: 00 G3 00 00 00 G1 00 00
						movq mm1, mm3      ;mm1: 00 R2 00 B2 00 R0 00 B0
						pslld mm1, 20      ;mm1: 00 B2 00 00 00 B0 00 00
						paddd mm3, mm1     ;mm3: 00 C2 00 B2 00 C0 00 B0
						psrld mm3, 16      ;mm3: 00 00 00 C2 00 00 00 C0 ;(B2<<4)+R2  (B0<<4)+R0
						paddd mm3, mm2     ;mm3: 00 G3 00 C2 00 G1 00 C0

						movq mm0, [eax+ecx*8+16] ;mm0: A5 R5 G5 B5 A4 R4 G4 B4
						movq mm1, [eax+ecx*8+24] ;mm1: A7 R7 G7 B7 A6 R6 G6 B6
						psubusb mm0, mm4
						psubusb mm1, mm4
						movq mm2, mm0      ;mm2: 00 R5 G5 B5 00 R4 G4 B4
						punpckldq mm0, mm1 ;mm0: 00 R6 G6 B6 A4 R4 G4 B4
						pand mm0, mm7      ;mm0: 00 R6 00 B6 00 R4 00 B4
						pmulhw mm0, mm6    ;mm0: 00 R6 00 B6 00 R4 00 B4 ;16<<8 13<<8 16<<8 13<<8
						punpckhdq mm2, mm1 ;mm2: 00 R7 G7 B7 00 R5 G5 B5
						add ecx, 4
						pand mm2, mm5      ;mm2: 00 00 G7 00 00 00 G5 00
						pslld mm2, 5       ;mm2: 00 G7 00 00 00 G5 00 00
						movq mm1, mm0      ;mm1: 00 R6 00 B6 00 R4 00 B4
						pslld mm1, 20      ;mm1: 00 B6 00 00 00 B4 00 00
						paddd mm0, mm1     ;mm0: 00 C6 00 B6 00 C4 00 B4
						psrld mm0, 16      ;mm0: 00 00 00 C6 00 00 00 C4 ;(B6<<4)+R6  (B4<<4)+R4
						paddd mm0, mm2     ;mm0: 00 G7 00 C6 00 G5 00 C4

						packuswb mm3, mm0  ;mm3: G7 C6 G5 C4 G3 C2 G1 C0
						paddd mm3, mask8g  ;mm3: C7 C6 C5 C4 C3 C2 C1 C0
						movq [edx+ecx*2], mm3
						js short beg8ab

						add eax, rbpl
						add edx, wbpl
						sub ebx, 1
						jz endit8b

			 into8bb:psubd mm4, mask8h
						pmovmskb esi, mm4
						and esi, 7
						paddd mm4, mask8lut[esi*8]

						mov ecx, rxsiz
						movq mm5, mask8e
			  beg8bb:
							;src: AaaaaaaaRrrrrrrrGgggggggBbbbbbbbAaaaaaaaRrrrrrrrGgggggggBbbbbbbb
							;dst: RrrGggBbRrrGggBb
						movq mm3, [eax+ecx*8]   ;mm3: A1 R1 G1 B1 A0 R0 G0 B0
						movq mm1, [eax+ecx*8+8] ;mm1: A3 R3 G3 B3 A2 R2 G2 B2
						psubusb mm3, mm4
						psubusb mm1, mm4
						movq mm2, mm3      ;mm2: 00 R1 G1 B1 00 R0 G0 B0
						punpckhdq mm3, mm1 ;mm3: 00 R3 G3 B3 00 R1 G1 B1
						pand mm3, mm7      ;mm3: 00 R3 00 B3 00 R1 00 B1
						pmulhw mm3, mm6    ;mm3: 00 R3 00 B3 00 R1 00 B1 ;16<<8 13<<8 16<<8 13<<8
						punpckldq mm2, mm1 ;mm2: 00 R2 G2 B2 00 R0 G0 B0
						psrlw mm2, 11      ;mm2: 00 00 00 G2 00 00 00 G0
						movq mm1, mm3      ;mm1: 00 R3 00 B3 00 R1 00 B1
						pslld mm1, 20      ;mm1: 00 B3 00 00 00 B1 00 00
						paddd mm3, mm1     ;mm3: 00 C3 00 B3 00 C1 00 B1
						pand mm3, mm5      ;mm3: 00 C3 00 00 00 C1 00 00 ;(B3<<4)+R3  (B1<<4)+R1
						paddd mm3, mm2     ;mm3: 00 C3 00 G2 00 C1 00 G0

						movq mm0, [eax+ecx*8+16] ;mm0: A5 R5 G5 B5 A4 R4 G4 B4
						movq mm1, [eax+ecx*8+24] ;mm1: A7 R7 G7 B7 A6 R6 G6 B6
						psubusb mm0, mm4
						psubusb mm1, mm4
						movq mm2, mm0      ;mm2: 00 R5 G5 B5 00 R4 G4 B4
						punpckhdq mm0, mm1 ;mm0: 00 R7 G7 B7 00 R5 G5 B5
						pand mm0, mm7      ;mm0: 00 R7 00 B7 00 R5 00 B5
						pmulhw mm0, mm6    ;mm0: 00 R7 00 B7 00 R5 00 B5 ;16<<8 13<<8 16<<8 13<<8
						punpckldq mm2, mm1 ;mm2: 00 R6 G6 B6 00 R4 G4 B4
						add ecx, 4
						psrlw mm2, 11      ;mm2: 00 00 00 G6 00 00 00 G4
						movq mm1, mm0      ;mm1: 00 R7 00 B7 00 R5 00 B5
						pslld mm1, 20      ;mm1: 00 B7 00 00 00 B5 00 00
						paddd mm0, mm1     ;mm0: 00 C7 00 B7 00 C5 00 B5
						pand mm0, mm5      ;mm0: 00 C7 00 00 00 C5 00 00 ;(B7<<4)+R7  (B5<<4)+R5
						paddd mm0, mm2     ;mm0: 00 C7 00 G6 00 C5 00 G4

						packuswb mm3, mm0  ;mm3: C7 G6 C5 G4 C3 G2 C1 G0
						paddd mm3, mask8f  ;mm3: C7 C6 C5 C4 C3 C2 C1 C0
						movq [edx+ecx*2], mm3
						js short beg8bb

						add eax, rbpl
						add edx, wbpl
						sub ebx, 1
						jnz begall8b

			 endit8b:psubd mm4, mask8h
						pmovmskb esi, mm4
						and esi, 7
						paddd mm4, mask8lut[esi*8]
						movq mask8d, mm4

						pop esi
						pop ebx
						emms
					}

				}
				//mask8cnt ^= 1;

				break;
			case 15:
				if (cputype&(1<<22)) //MMX+
				{
					_asm
					{
						push ebx
						push esi

						movq mm7, maskrb15
						movq mm6, maskgg15
						movq mm5, maskml15

						mov eax, rplc
						mov edx, wplc
						mov ecx, rxsiz
						mov ebx, rysiz
						add ecx, 2
						shr ecx, 1
						lea eax, [eax+ecx*8]
						lea edx, [edx+ecx*4-8]
						neg ecx
						mov rysiz, ecx ;Use rysiz as scratch register
						jmp short into15
				beg15:   ;src: AaaaaaaaRrrrrrrrGgggggggBbbbbbbb """
							;     --------Rrrrr---Ggggg---Bbbbb--- """
							;dst: -RrrrrGggggBbbbb-RrrrrGggggBbbbb "
						movntq [edx+ecx*4], mm0
			  into15:movq mm0, [eax+ecx*8]   ;AaaaaaaaRrrrrrrrGgggggggBbbbbbbb "
						movq mm1, [eax+ecx*8+8] ;AaaaaaaaRrrrrrrrGgggggggBbbbbbbb "
						movq mm2, mm0
						movq mm3, mm1
						pand mm0, mm7      ;--------Rrrrr-----------Bbbbb--- "
						pand mm1, mm7      ;--------Rrrrr-----------Bbbbb--- "
						pmaddwd mm0, mm5   ;-----------Rrrrr-----Bbbbb------ " ;8192 8 8192 8
						pmaddwd mm1, mm5   ;-----------Rrrrr-----Bbbbb------ " ;8192 8 8192 8
						prefetchnta [eax+ecx*8+72]
						add ecx, 2
						pand mm2, mm6      ;----------------Ggggg----------- "
						pand mm3, mm6      ;----------------Ggggg----------- "
						por mm0, mm2       ;-----------RrrrrGggggBbbbb------ "
						por mm1, mm3       ;-----------RrrrrGggggBbbbb------ "
						psrld mm0, 6       ;-----------------RrrrrGggggBbbbb "
						psrld mm1, 6       ;-----------------RrrrrGggggBbbbb "
						packssdw mm0, mm1  ;RrrrrGggggBbbbb """
						js short beg15

						test rxsiz, 2
						jz short skip15a
						movd [edx+ecx*4], mm0
						psrlq mm0, 32
						add ecx, 1
			 skip15a:test rxsiz, 1
						jz short skip15b
						movd esi, mm0
						mov [edx+ecx*4], si
			 skip15b:

						add eax, rbpl
						add edx, wbpl
						sub ebx, 1
						mov ecx, rysiz
						jnz short into15
						emms

						pop esi
						pop ebx
					}
				}
				else
				{
					_asm
					{
						push ebx
						push esi

						movq mm7, maskrb15
						movq mm6, maskgg15
						movq mm5, maskml15

						mov eax, rplc
						mov edx, wplc
						mov ecx, rxsiz
						mov ebx, rysiz
						add ecx, 2
						shr ecx, 1
						lea eax, [eax+ecx*8]
						lea edx, [edx+ecx*4-8]
						neg ecx
						mov rysiz, ecx ;Use rysiz as scratch register
						jmp short into15b
			  beg15b:   ;src: AaaaaaaaRrrrrrrrGgggggggBbbbbbbb """
							;     --------Rrrrr---Ggggg---Bbbbb--- """
							;dst: -RrrrrGggggBbbbb-RrrrrGggggBbbbb "
						movq [edx+ecx*4], mm0
			 into15b:movq mm0, [eax+ecx*8]   ;AaaaaaaaRrrrrrrrGgggggggBbbbbbbb "
						movq mm1, [eax+ecx*8+8] ;AaaaaaaaRrrrrrrrGgggggggBbbbbbbb "
						movq mm2, mm0
						movq mm3, mm1
						pand mm0, mm7      ;--------Rrrrr-----------Bbbbb--- "
						pand mm1, mm7      ;--------Rrrrr-----------Bbbbb--- "
						pmaddwd mm0, mm5   ;-----------Rrrrr-----Bbbbb------ " ;8192 8 8192 8
						pmaddwd mm1, mm5   ;-----------Rrrrr-----Bbbbb------ " ;8192 8 8192 8
						add ecx, 2
						pand mm2, mm6      ;----------------Ggggg----------- "
						pand mm3, mm6      ;----------------Ggggg----------- "
						por mm0, mm2       ;-----------RrrrrGggggBbbbb------ "
						por mm1, mm3       ;-----------RrrrrGggggBbbbb------ "
						psrld mm0, 6       ;-----------------RrrrrGggggBbbbb "
						psrld mm1, 6       ;-----------------RrrrrGggggBbbbb "
						packssdw mm0, mm1  ;RrrrrGggggBbbbb """
						js short beg15b

						test rxsiz, 2
						jz short skip15ab
						movd [edx+ecx*4], mm0
						psrlq mm0, 32
						add ecx, 1
			skip15ab:test rxsiz, 1
						jz short skip15bb
						movd esi, mm0
						mov [edx+ecx*4], si
			skip15bb:

						add eax, rbpl
						add edx, wbpl
						sub ebx, 1
						mov ecx, rysiz
						jnz short into15b
						emms

						pop esi
						pop ebx
					}
				}
				break;
			case 16:
				if (cputype&(1<<22)) //MMX+
				{
					_asm
					{
						push ebx
						push esi

						movq mm7, maskrb16
						movq mm6, maskgg16
						movq mm5, maskml16
						mov eax, rplc
						mov edx, wplc
						mov ecx, rxsiz
						mov ebx, rysiz
						add ecx, 2
						shr ecx, 1
						lea eax, [eax+ecx*8]
						lea edx, [edx+ecx*4-8]
						neg ecx
						mov rysiz, ecx ;Use rysiz as scratch register
						jmp short into16
				beg16:   ;src: AaaaaaaaRrrrrrrrGgggggggBbbbbbbb """
							;     --------Rrrrr---Gggggg--Bbbbb--- """
							;dst: RrrrrGgggggBbbbbRrrrrGgggggBbbbb "
						movntq [edx+ecx*4], mm0
			  into16:movq mm0, [eax+ecx*8]   ;AaaaaaaaRrrrrrrrGgggggggBbbbbbbb "
						movq mm1, [eax+ecx*8+8] ;AaaaaaaaRrrrrrrrGgggggggBbbbbbbb "
						movq mm2, mm0
						movq mm3, mm1
						pand mm0, mm7      ;--------Rrrrr-----------Bbbbb--- "
						pand mm1, mm7      ;--------Rrrrr-----------Bbbbb--- "
						pmaddwd mm0, mm5   ;-----------Rrrrr------Bbbbb----- " ;8192 4 8192 4
						prefetchnta [eax+ecx*8+72]
						add ecx, 2
						pmaddwd mm1, mm5   ;-----------Rrrrr------Bbbbb----- " ;8192 4 8192 4
						pand mm2, mm6      ;----------------Gggggg---------- "
						pand mm3, mm6      ;----------------Gggggg---------- "
						por mm0, mm2       ;-----------RrrrrGgggggBbbbb----- "
						por mm1, mm3       ;-----------RrrrrGgggggBbbbb----- "
						psrld mm0, 5       ;----------------RrrrrGgggggBbbbb "
						psrld mm1, 5       ;----------------RrrrrGgggggBbbbb "
						pshufw mm0, mm0, 8 ;--------------------------------RrrrrGgggggBbbbbRrrrrGgggggBbbbb
						pshufw mm1, mm1, 8 ;--------------------------------RrrrrGgggggBbbbbRrrrrGgggggBbbbb
						punpckldq mm0, mm1 ;RrrrrGgggggBbbbbRrrrrGgggggBbbbb "
						js short beg16

						test rxsiz, 2
						jz short skip16a
						movd [edx+ecx*4], mm0
						psrlq mm0, 32
						add ecx, 1
			 skip16a:test rxsiz, 1
						jz short skip16b
						movd esi, mm0
						mov [edx+ecx*4], si
			 skip16b:

						add eax, rbpl
						add edx, wbpl
						sub ebx, 1
						mov ecx, rysiz
						jnz short into16
						emms

						pop esi
						pop ebx
					}
				}
				else
				{
					_asm
					{
						push ebx
						push esi

						movq mm7, maskrb16
						movq mm6, maskgg16
						movq mm5, maskml16
						movq mm4, mask16a
						mov eax, rplc
						mov edx, wplc
						mov ecx, rxsiz
						mov ebx, rysiz
						add ecx, 2
						shr ecx, 1
						lea eax, [eax+ecx*8]
						lea edx, [edx+ecx*4-8]
						neg ecx
						mov rysiz, ecx ;Use rysiz as scratch register
						jmp short into16b
			  beg16b:   ;src: AaaaaaaaRrrrrrrrGgggggggBbbbbbbb """
							;     --------Rrrrr---Gggggg--Bbbbb--- """
							;dst: RrrrrGgggggBbbbbRrrrrGgggggBbbbb "
						movq [edx+ecx*4], mm0
			 into16b:movq mm0, [eax+ecx*8]   ;AaaaaaaaRrrrrrrrGgggggggBbbbbbbb "
						movq mm1, [eax+ecx*8+8] ;AaaaaaaaRrrrrrrrGgggggggBbbbbbbb "
						movq mm2, mm0
						movq mm3, mm1
						pand mm0, mm7      ;--------Rrrrr-----------Bbbbb--- "
						pand mm1, mm7      ;--------Rrrrr-----------Bbbbb--- "
						pmaddwd mm0, mm5   ;-----------Rrrrr------Bbbbb----- " ;8192 4 8192 4
						add ecx, 2
						pmaddwd mm1, mm5   ;-----------Rrrrr------Bbbbb----- " ;8192 4 8192 4
						pand mm2, mm6      ;----------------Gggggg---------- "
						pand mm3, mm6      ;----------------Gggggg---------- "
						por mm0, mm2       ;-----------RrrrrGgggggBbbbb----- "
						por mm1, mm3       ;-----------RrrrrGgggggBbbbb----- "
						psrld mm0, 5       ;----------------RrrrrGgggggBbbbb "
						psrld mm1, 5       ;----------------RrrrrGgggggBbbbb "
						psubd mm0, mm4     ;subtract 32768 from all 4 dwords so packssdw doesn't saturate
						psubd mm1, mm4
						packssdw mm0, mm1
						paddw mm0, mask16b ;convert back to unsigned short
						js short beg16b

						test rxsiz, 2
						jz short skip16ab
						movd [edx+ecx*4], mm0
						psrlq mm0, 32
						add ecx, 1
			skip16ab:test rxsiz, 1
						jz short skip16bb
						movd esi, mm0
						mov [edx+ecx*4], si
			skip16bb:

						add eax, rbpl
						add edx, wbpl
						sub ebx, 1
						mov ecx, rysiz
						jnz short into16b
						emms

						pop esi
						pop ebx
					}
				}
				break;
			case 24:
				rxsiz &= ~3; if (rxsiz <= 0) return;
				if (cputype&(1<<22)) //MMX+
				{
					_asm
					{
						push ebx
						push edi

						movq mm7, mask24a
						movq mm6, mask24b
						movq mm5, mask24c
						movq mm4, mask24d
						mov eax, rplc
						mov edi, wplc
						mov ebx, rysiz
						mov ecx, rxsiz
						shr ecx, 1
						lea eax, [eax+ecx*8]
						sub edi, 12
						neg ecx
						mov rxsiz, ecx

			prebeg24:mov edx, edi
						mov ecx, rxsiz
				beg24:   ;src: aRGB aRGB aRGB aRGB
							;     RGBR GB-- --RG BRGB (intermediate step)
							;dst: RGBR GBRG BRGB
						movq mm0, [eax+ecx*8]   ;mm0: [aRGB aRGB]
						movq mm1, [eax+ecx*8+8] ;mm1: [aRGB aRGB]
						pslld mm1, 8       ;mm1: [RGB- RGB-]
						movq mm2, mm0      ;mm2: [aRGB aRGB]
						movq mm3, mm1      ;mm3: [RGB- RGB-]

						pand mm0, mm7      ;mm0: [-RGB ----]
						pand mm1, mm6      ;mm1: [---- RGB-]
						pand mm2, mm5      ;mm2: [---- -RGB]
						pand mm3, mm4      ;mm3: [RGB- ----]
						psrlq mm0, 8       ;mm0: [--RG B---]
						add edx, 12
						add ecx, 2
						psllq mm1, 8       ;mm1: [---R GB--]
						por mm0, mm2       ;mm0: [--RG BRGB]
						por mm1, mm3       ;mm1: [RGBR GB--]

						movd [edx], mm0    ;mm0: [--RG BRGB]
						psrlq mm0, 32      ;mm0: [---- --RG]
						por mm0, mm1       ;mm0: [RGBR GBRG]
						movd [edx+4], mm0
						psrlq mm0, 32      ;mm0: [---- RGBR]
						movd [edx+8], mm0

						prefetchnta [eax+ecx*8+128]
						js short beg24

						add eax, rbpl
						add edi, wbpl
						sub ebx, 1
						jnz short prebeg24
						emms

						pop edi
						pop ebx
					}
				}
				else
				{
					_asm
					{
						push ebx
						push edi

						movq mm7, mask24a
						movq mm6, mask24b
						movq mm5, mask24c
						movq mm4, mask24d
						mov eax, rplc
						mov edi, wplc
						mov ebx, rysiz
						mov ecx, rxsiz
						shr ecx, 1
						lea eax, [eax+ecx*8]
						sub edi, 12
						neg ecx
						mov rxsiz, ecx

		  prebeg24b:mov edx, edi
						mov ecx, rxsiz
			  beg24b:   ;src: aRGB aRGB aRGB aRGB
							;     RGBR GB-- --RG BRGB (intermediate step)
							;dst: RGBR GBRG BRGB
						movq mm0, [eax+ecx*8]   ;mm0: [aRGB aRGB]
						movq mm1, [eax+ecx*8+8] ;mm1: [aRGB aRGB]
						pslld mm1, 8       ;mm1: [RGB- RGB-]
						movq mm2, mm0      ;mm2: [aRGB aRGB]
						movq mm3, mm1      ;mm3: [RGB- RGB-]

						pand mm0, mm7      ;mm0: [-RGB ----]
						pand mm1, mm6      ;mm1: [---- RGB-]
						pand mm2, mm5      ;mm2: [---- -RGB]
						pand mm3, mm4      ;mm3: [RGB- ----]
						psrlq mm0, 8       ;mm0: [--RG B---]
						add edx, 12
						add ecx, 2
						psllq mm1, 8       ;mm1: [---R GB--]
						por mm0, mm2       ;mm0: [--RG BRGB]
						por mm1, mm3       ;mm1: [RGBR GB--]

						movd [edx], mm0    ;mm0: [--RG BRGB]
						psrlq mm0, 32      ;mm0: [---- --RG]
						por mm0, mm1       ;mm0: [RGBR GBRG]
						movd [edx+4], mm0
						psrlq mm0, 32      ;mm0: [---- RGBR]
						movd [edx+8], mm0

						js short beg24b

						add eax, rbpl
						add edi, wbpl
						sub ebx, 1
						jnz short prebeg24b
						emms

						pop edi
						pop ebx
					}
				}
				break;
			case 32:
				if (cputype&(1<<22)) //MMX+
				{
					_asm
					{
						push ebx
						push esi
						push edi

						mov eax, rplc
						mov edx, wplc
						mov ecx, rxsiz
						mov ebx, rysiz
						add ecx, 2
						shr ecx, 1
						lea eax, [eax+ecx*8]
						lea edx, [edx+ecx*8]
						neg ecx
						mov rysiz, ecx
						mov esi, rbpl
						mov edi, wbpl
						jmp short into32

				beg32:movntq qword ptr [edx+ecx*8-8], mm0
			  into32:movq mm0, qword ptr [eax+ecx*8]
						prefetchnta [eax+ecx*8+16]
						add ecx, 1
						jnz short beg32

						test rxsiz, 1
						jz short skip32
						movd dword ptr [edx+ecx*8-8], mm0
			  skip32:

						add eax, esi
						add edx, edi
						sub ebx, 1
						mov ecx, rysiz
						jnz short into32
						emms

						pop edi
						pop esi
						pop ebx
					}
				}
				else
				{
					_asm
					{
						push ebx
						push esi
						push edi

						mov eax, rplc
						mov edx, wplc
						mov ecx, rxsiz
						mov ebx, rysiz
						add ecx, 2
						shr ecx, 1
						lea eax, [eax+ecx*8]
						lea edx, [edx+ecx*8]
						neg ecx
						mov rysiz, ecx
						mov esi, rbpl
						mov edi, wbpl
						jmp short into32b

			  beg32b:movq qword ptr [edx+ecx*8-8], mm0
			 into32b:movq mm0, qword ptr [eax+ecx*8]
						add ecx, 1
						jnz short beg32b

						test rxsiz, 1
						jz short skip32b
						movd dword ptr [edx+ecx*8-8], mm0
			 skip32b:

						add eax, esi
						add edx, edi
						sub ebx, 1
						mov ecx, rysiz
						jnz short into32b
						emms

						pop edi
						pop esi
						pop ebx
					}
				}
				break;
		}
	}
#endif
}

#ifndef REFRESHACK
		 LPDIRECTDRAW lpdd = 0;
#else
		 LPDIRECTDRAW lpdd0 = 0;
		 LPDIRECTDRAW2 lpdd = 0;
		 long refreshz = 0;
#endif
		 LPDIRECTDRAWSURFACE ddsurf[3] = {0,0,0};
static LPDIRECTDRAWPALETTE ddpal = 0;
#ifndef NO_CLIPPER
static LPDIRECTDRAWCLIPPER ddclip = 0;
static RGNDATA *ddcliprd = 0;
static unsigned long ddcliprdbytes = 0;
#endif
static DDSURFACEDESC ddsd;
static long ddlocked = 0, ddrawemulbpl = 0;
static void *ddrawemulbuf = 0;
static long cantlockprimary = 0; //FUKFUKFUKFUK
//#define OFFSCREENHACK 1 //FUKFUKFUKFUK

  //The official DirectDraw method for retrieving video modes!
#define MAXVALIDMODES 256
typedef struct { long x, y; char c, r0, g0, b0, a0, rn, gn, bn, an; } validmodetype;
static validmodetype validmodelist[MAXVALIDMODES];
static long validmodecnt = 0;
validmodetype curvidmodeinfo;

#ifdef __WATCOMC__

#pragma aux bsf = "bsf eax, eax" parm [eax] modify nomemory exact [eax] value [eax]
#pragma aux bsr = "bsr eax, eax" parm [eax] modify nomemory exact [eax] value [eax]

#endif
#ifdef _MSC_VER

static _inline long bsf (long a) { _asm bsf eax, a } //is it safe to assume eax is return value?
static _inline long bsr (long a) { _asm bsr eax, a } //is it safe to assume eax is return value?

#endif

static void grabmodeinfo (long x, long y, DDPIXELFORMAT *ddpf, validmodetype *valptr)
{
	memset(valptr,0,sizeof(validmodetype));

	valptr->x = x; valptr->y = y;
	if (ddpf->dwFlags&DDPF_PALETTEINDEXED8) { valptr->c = 8; return; }
	if (ddpf->dwFlags&DDPF_RGB)
	{
		valptr->c = (char)ddpf->dwRGBBitCount;
		if (ddpf->dwRBitMask)
		{
			valptr->r0 = (char)bsf(ddpf->dwRBitMask);
			valptr->rn = (char)(bsr(ddpf->dwRBitMask)-(valptr->r0)+1);
		}
		if (ddpf->dwGBitMask)
		{
			valptr->g0 = (char)bsf(ddpf->dwGBitMask);
			valptr->gn = (char)(bsr(ddpf->dwGBitMask)-(valptr->g0)+1);
		}
		if (ddpf->dwBBitMask)
		{
			valptr->b0 = (char)bsf(ddpf->dwBBitMask);
			valptr->bn = (char)(bsr(ddpf->dwBBitMask)-(valptr->b0)+1);
		}
		if (ddpf->dwRGBAlphaBitMask)
		{
			valptr->a0 = (char)bsf(ddpf->dwRGBAlphaBitMask);
			valptr->an = (char)(bsr(ddpf->dwRGBAlphaBitMask)-(valptr->a0)+1);
		}
	}
}

HRESULT WINAPI lpEnumModesCallback (LPDDSURFACEDESC dsd, LPVOID lpc)
{
	grabmodeinfo(dsd->dwWidth,dsd->dwHeight,&dsd->ddpfPixelFormat,&validmodelist[validmodecnt]); validmodecnt++;
	if (validmodecnt >= MAXVALIDMODES) return(DDENUMRET_CANCEL); else return(DDENUMRET_OK);
}

long getvalidmodelist (validmodetype **davalidmodelist)
{
	if (!lpdd) return(0);
	if (!validmodecnt) lpdd->EnumDisplayModes(0,0,0,lpEnumModesCallback);
	(*davalidmodelist) = validmodelist;
	return(validmodecnt);
}

void uninitdirectdraw ()
{
#ifdef USED3D4FULL
	if (fullscreen) { d3duninit(); return; }
#endif

	if (ddpal) { ddpal->Release(); ddpal = 0; }
	if (lpdd)
	{
#ifndef NO_CLIPPER
		if (ddcliprd) { free(ddcliprd); ddcliprd = 0; ddcliprdbytes = 0; }
		if (ddclip) { ddclip->Release(); ddclip = 0; }
#endif
		if (ddsurf[0]) { ddsurf[0]->Release(); ddsurf[0] = 0; }
		if (ddrawemulbuf) { free(ddrawemulbuf); ddrawemulbuf = 0; }
		lpdd->Release(); lpdd = 0;
	}
}

void updatepalette (long start, long danum)
{
	long i;
	if (colbits == 8)
	{
		switch(ddrawuseemulation)
		{
			case 15:
				for(i=start+danum-1;i>=start;i--)
					lpal[i] = (((pal[i].peRed>>3)&31)<<10) + (((pal[i].peGreen>>3)&31)<<5) + ((pal[i].peBlue>>3)&31);
				return;
			case 16:
				for(i=start+danum-1;i>=start;i--)
					lpal[i] = (((pal[i].peRed>>3)&31)<<11) + (((pal[i].peGreen>>2)&63)<<5) + ((pal[i].peBlue>>3)&31);
				return;
			case 24: case 32:
				for(i=start+danum-1;i>=start;i--)
					lpal[i] = ((pal[i].peRed)<<16) + ((pal[i].peGreen)<<8) + pal[i].peBlue;
				return;
			default: break;
		}
	}
	if (!ddpal) return;
	if (ddpal->SetEntries(0,start,danum,&pal[start]) == DDERR_SURFACELOST)
		{ ddsurf[0]->Restore(); ddpal->SetEntries(0,start,danum,&pal[start]); }
}

	 //((z/(16-1))^.8)*255, ((z/(32-1))^.8)*255, ((z/((13 fullscreen/12 windowed)-1))^.8)*255
static char palr[16] = {0,29,51,70,89,106,123,139,154,169,184,199,213,227,241,255};
static char palg[32] = {0,16,28,39,50,59,69,78,86,95,103,111,119,127,135,143,
				 150,158,165,172,180,187,194,201,208,215,222,228,235,242,248,255};
static char palb[16] = {0,35,61,84,106,127,146,166,184,203,220,238,255,0,0,0};

void emul8setpal ()
{
	long i, r, g, b;

	if (fullscreen)
	{
			//13 shades of blue in fullscreen mode
		*(long *)&palb[0] =   0+( 35<<8)+( 61<<16)+( 84<<24);
		*(long *)&palb[4] = 106+(127<<8)+(146<<16)+(166<<24);
		*(long *)&palb[8] = 184+(203<<8)+(220<<16)+(238<<24);
		mask8b = 0x10000d0010000d00;
		mask8f = 0x00d000d000d000d0;
		mask8g = 0xd000d000d000d000;

		i = 0;
		for(b=0;b<13;b++)
			for(r=0;r<16;r++)
			{
				pal[i].peRed = palr[r];
				pal[i].peGreen = 0;
				pal[i].peBlue = palb[b];
				pal[i].peFlags = PC_NOCOLLAPSE;
				i++;
			} //224 cols
		for(g=0;g<32;g++)
		{
			pal[i].peRed = 0;
			pal[i].peGreen = palg[g];
			pal[i].peBlue = 0;
			pal[i].peFlags = PC_NOCOLLAPSE;
			i++;
		}
	}
	else
	{
			//12 shades of blue in fullscreen mode
		*(long *)&palb[0] =   0+( 37<<8)+( 65<<16)+( 90<<24);
		*(long *)&palb[4] = 114+(136<<8)+(157<<16)+(178<<24);
		*(long *)&palb[8] = 198+(217<<8)+(236<<16)+(255<<24);
		mask8b = 0x10000c0010000c00;
		mask8f = 0x0aca0aca0aca0aca;
		mask8g = 0xca0aca0aca0aca0a;

		for(i=0;i<10;i++) { pal[i].peFlags = PC_EXPLICIT; pal[i].peRed = (char)i; pal[i].peGreen = pal[i].peBlue = 0; }
		for(b=0;b<12;b++)
			for(r=0;r<16;r++)
			{
				pal[i].peRed = palr[r];
				pal[i].peGreen = 0;
				pal[i].peBlue = palb[b];
				pal[i].peFlags = PC_NOCOLLAPSE;
				i++;
			} //192 cols
		for(g=0;g<32;g++)
		{
			pal[i].peRed = 0;
			pal[i].peGreen = palg[g];
			pal[i].peBlue = 0;
			pal[i].peFlags = PC_NOCOLLAPSE;
			i++;
		}
		for(;i<256-10;i++) { pal[i].peFlags = PC_NOCOLLAPSE; pal[i].peRed = pal[i].peGreen = pal[i].peBlue = 0; }
		for(;i<256;i++) { pal[i].peFlags = PC_EXPLICIT; pal[i].peRed = (char)i; pal[i].peGreen = pal[i].peBlue = 0; }
	}
}

	// Note!
	// debugmode is automatically turned off after changeres or initdirectdraw!
	// do NOT call debugdirectdraw inside startdirectdraw()..stopdirectdraw() section
void debugdirectdraw ()
{
#ifndef NOINPUT
	extern long mouse_acquire, kbd_acquire;
	extern void setacquire (long mouse, long kbd);
	static long old_acq = 0;
#endif
	if (ddrawdebugmode == -1) {
		if (!ddrawuseemulation) {
			ddrawemulbuf = malloc(((xres*yres+7)&~7)*((colbits+7)>>3)+16); if (!ddrawemulbuf) return; // error!
			ddrawemulbpl = ((colbits+7)>>3)*xres;
			ddrawdebugmode = 0;
			ddrawuseemulation = 32;
		} else
			ddrawdebugmode = ddrawuseemulation;
#ifndef NOINPUT
		old_acq = (mouse_acquire&1) + ((kbd_acquire&1)<<1);
		setacquire(0,0);
#endif
	} else {
		ddrawuseemulation = ddrawdebugmode;
		if (!ddrawuseemulation) {
			if (ddrawemulbuf) { free(ddrawemulbuf); ddrawemulbuf = 0; }
		}
		ddrawdebugmode = -1;
#ifndef NOINPUT
		setacquire(old_acq&1, old_acq>>1);
#endif
	}
}

long initdirectdraw (long daxres, long dayres, long dacolbits)
{
	HRESULT hr;
	DDSCAPS ddscaps;
	char buf[256];
	long ncolbits;

	xres = daxres; yres = dayres; colbits = dacolbits;

#ifdef USED3D4FULL
	if (fullscreen)
	{
		ddrawdebugmode = -1; ddrawuseemulation = 0;
		if (d3dinit(ghwnd) < 0) return(-1);
		return(1);
	}
#endif

#ifndef REFRESHACK
	if ((hr = DirectDrawCreate(0,&lpdd,0)) >= 0)
	{
#else
	if ((hr = DirectDrawCreate(0,&lpdd0,0)) >= 0)
	{
		lpdd0->QueryInterface(IID_IDirectDraw2,(void **)&lpdd);
#endif
		if (fullscreen)
		{
			if ((hr = lpdd->SetCooperativeLevel(ghwnd,DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN)) >= 0)
			{
				ddrawdebugmode = -1;
				ncolbits = dacolbits; ddrawuseemulation = 0;
				//ncolbits = 8; //HACK FOR TESTING!
				//ncolbits = 16; //HACK FOR TESTING!
				do
				{
#ifndef REFRESHACK
					if ((hr = lpdd->SetDisplayMode(daxres,dayres,ncolbits)) >= 0)
#else
					if ((hr = lpdd->SetDisplayMode(daxres,dayres,ncolbits,refreshz,0)) >= 0)
#endif
					{
						if (ncolbits != dacolbits)
						{
							ddrawemulbuf = malloc(((daxres*dayres+7)&~7)*((dacolbits+7)>>3)+16);
							if (!ddrawemulbuf) { ncolbits = 0; break; }
							ddrawemulbpl = ((dacolbits+7)>>3)*daxres;
							ddrawuseemulation = ncolbits;
						}
						ddsd.dwSize = sizeof(ddsd);
						ddsd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
						ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE|DDSCAPS_COMPLEX|DDSCAPS_FLIP;
						if (maxpages > 2) ddsd.dwBackBufferCount = 2;
										 else ddsd.dwBackBufferCount = 1;
						if ((hr = lpdd->CreateSurface(&ddsd,&ddsurf[0],0)) >= 0)
						{
							DDPIXELFORMAT ddpf;
							ddpf.dwSize = sizeof(DDPIXELFORMAT);
							if (!ddsurf[0]->GetPixelFormat(&ddpf))
								grabmodeinfo(daxres,dayres,&ddpf,&curvidmodeinfo);

							ddscaps.dwCaps = DDSCAPS_BACKBUFFER;
							if ((hr = ddsurf[0]->GetAttachedSurface(&ddscaps,&ddsurf[1])) >= 0)
							{
#if (OFFSCREENHACK)
								DDSURFACEDESC nddsd;
								ddsurf[2] = ddsurf[1];
								ZeroMemory(&nddsd,sizeof(nddsd));
								nddsd.dwSize = sizeof(nddsd);
								nddsd.dwFlags = DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT;
								nddsd.dwWidth = (daxres|1); //This hack (|1) ensures pitch isn't near multiple of 4096 (slow on P4)!
								nddsd.dwHeight = dayres;
								nddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN|DDSCAPS_SYSTEMMEMORY;
								if ((hr = lpdd->CreateSurface(&nddsd,&ddsurf[1],0)) < 0) return(-1);
#endif
								if (ncolbits != 8) return(1);
								if (lpdd->CreatePalette(DDPCAPS_8BIT,pal,&ddpal,0) >= 0)
								{
									ddsurf[0]->SetPalette(ddpal);
									if (ddrawuseemulation)
										{ emul8setpal(); updatepalette(0,256); }
									return(1);
								}
							}
						}
					}
					switch(dacolbits)
					{
						case 8:
							switch (ncolbits)
							{
								case 8: ncolbits = 32; break;
								case 32: ncolbits = 24; break;
								case 24: ncolbits = 16; break;
								case 16: ncolbits = 15; break;
								default: ncolbits = 0; break;
							}
							break;
						case 32:
							switch (ncolbits)
							{
								case 32: ncolbits = 24; break;
								case 24: ncolbits = 16; break;
								case 16: ncolbits = 15; break;
								case 15: ncolbits = 8; break;
								default: ncolbits = 0; break;
							}
							break;
						default: ncolbits = 0; break;
					}
				} while (ncolbits);
				if (!ncolbits)
				{
					validmodetype *validmodelist;
					char vidlistbuf[4096];
					long i, j;

					validmodecnt = getvalidmodelist(&validmodelist);
					wsprintf(vidlistbuf,"Valid fullscreen %d-bit DirectDraw modes:\n",colbits);
					j = 0;
					for(i=0;i<validmodecnt;i++)
						if (validmodelist[i].c == colbits)
						{
							wsprintf(buf,"/%dx%d\n",validmodelist[i].x,validmodelist[i].y);
							strcat(vidlistbuf,buf);
							j++;
						}
					if (!j) strcat(vidlistbuf,"None! Try a different bit depth.");

					wsprintf(buf,"initdirectdraw failed: 0x%x",hr);
					MessageBox(ghwnd,vidlistbuf,buf,MB_OK);

					uninitdirectdraw();
					return(0);
				}
			}
		}
		else
		{
			if ((hr = lpdd->SetCooperativeLevel(ghwnd,DDSCL_NORMAL)) >= 0)
			{
				ddsd.dwSize = sizeof(ddsd);
				ddsd.dwFlags = DDSD_CAPS;
				ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
				if ((hr = lpdd->CreateSurface(&ddsd,&ddsurf[0],0)) >= 0)
				{
#ifndef NO_CLIPPER
						//Create clipper object for windowed mode
					if ((hr = lpdd->CreateClipper(0,&ddclip,0)) == DD_OK)
					{
						ddclip->SetHWnd(0,ghwnd); //Associate clipper with window
						hr = ddsurf[0]->SetClipper(ddclip);
						if (hr == DD_OK)
						{
#endif
							ddsd.dwSize = sizeof(ddsd);
							ddsd.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_CAPS;
#if (OFFSCREENHACK == 0)
							ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
#else
							ddsd.ddsCaps.dwCaps = DDSCAPS_SYSTEMMEMORY;
#endif
							ddsd.dwWidth = xres;
							ddsd.dwHeight = yres;
							if ((hr = lpdd->CreateSurface(&ddsd,&ddsurf[1],0)) >= 0)
							{
								DDCAPS ddcaps;
								HDC hDC = GetDC(0);
								ncolbits = GetDeviceCaps(hDC,BITSPIXEL);
								ReleaseDC(0,hDC);

								ddcaps.dwSize = sizeof(ddcaps);
								if (lpdd->GetCaps(&ddcaps,0) == DD_OK) //Better to catch DDERR_CANTLOCKSURFACE here than in startdirectdraw()
									if (ddcaps.dwCaps&DDCAPS_NOHARDWARE) cantlockprimary = 1; //For LCD screens mainly
								if (osvi.dwMajorVersion >= 6) cantlockprimary = 1; //Longhorn/Vista

									//03/08/2004: Now that clipper works for ddrawemulbuf, ddrawemulbuf is not only
									//faster, but it supports WM_SIZE dragging better than ddsurf[0]->Blt! :)
									//
									//kblit32 currently supports these modes:
									//   colbits ->    ncolbits
									//       8   ->  . 15 16 24 32
									//      15   ->  .  .  .  .  .
									//      16   ->  .  .  .  .  .
									//      24   ->  .  .  .  .  .
									//      32   ->  8 15 16 24 32
								if ((ncolbits != colbits) || ((colbits == 32) && (!cantlockprimary)))
								{
#if (OFFSCREENHACK)
									if (colbits != colbits)
#endif
									{
										ddrawemulbuf = malloc(((xres*yres+7)&~7)*((colbits+7)>>3)+16);
										if (ddrawemulbuf)
										{
											ddrawemulbpl = ((colbits+7)>>3)*xres;
											ddrawuseemulation = ncolbits;
											if ((ncolbits == 8) && (colbits == 32)) emul8setpal();
										}
									}
								}

								if ((ncolbits == 8) || (colbits == 8)) updatepalette(0,256);

								DDPIXELFORMAT ddpf;
								ddpf.dwSize = sizeof(DDPIXELFORMAT);
								if (!ddsurf[0]->GetPixelFormat(&ddpf)) //colbits = ddpf.dwRGBBitCount;
								{
									grabmodeinfo(daxres,dayres,&ddpf,&curvidmodeinfo);

										//If mode is 555 color (and not 565), use 15-bit emulation code...
									if ((colbits != 16) && (ncolbits == 16)
																&& (curvidmodeinfo.r0 == 10) && (curvidmodeinfo.rn == 5)
																&& (curvidmodeinfo.g0 ==  5) && (curvidmodeinfo.gn == 5)
																&& (curvidmodeinfo.b0 ==  0) && (curvidmodeinfo.bn == 5))
										ddrawuseemulation = 15;
								}

								if (ncolbits == 8)
									if (lpdd->CreatePalette(DDPCAPS_8BIT,pal,&ddpal,0) >= 0)
										ddsurf[0]->SetPalette(ddpal);

								return(1);
							}
#ifndef NO_CLIPPER
						}
					}
#endif
				}
			}
		}
	}
	uninitdirectdraw();
	wsprintf(buf,"initdirectdraw failed: 0x%08lx",hr);
	MessageBox(ghwnd,buf,"ERROR",MB_OK);
	return(0);
}

void stopdirectdraw ()
{
	if (!ddlocked) return;
	ddlocked = 0;
	if (!ddrawuseemulation)
	{
#ifdef USED3D4FULL
		if (fullscreen)
		{
#ifndef USE3DVISION
			d3ddev->EndScene(); d3dsur->UnlockRect();
#else
			gpt[g3dnumframes&1]->UnlockRect(0);
#endif
			return;
		}
#endif
		ddsurf[1]->Unlock(ddsd.lpSurface);
	}
}

long startdirectdraw (long *vidplc, long *dabpl, long *daxres, long *dayres)
{
	HRESULT hr;

	if (ddlocked) { stopdirectdraw(); ddlocked = 0; }  //{ return(0); }

	if (ddrawuseemulation)
	{
		*vidplc = (long)ddrawemulbuf; *dabpl = xres*((colbits+7)>>3);
		*daxres = xres; *dayres = yres; ddlocked = 1;
		return(1);
	}

#ifdef USED3D4FULL
	if (fullscreen)
	{
		D3DLOCKED_RECT r;
#ifndef USE3DVISION
		if (d3dsur->LockRect(&r,0,0) < 0) return(0);
#else
		gpt[g3dnumframes&1]->LockRect(0,&r,0,0);
#endif
		d3df = (long)r.pBits; d3dp = (long)r.Pitch;
		(*vidplc) = (long)r.pBits; (*dabpl) = r.Pitch;
		(*daxres) = xres; (*dayres) = yres; ddlocked = 1;
#ifndef USE3DVISION
		d3ddev->BeginScene();
#endif
		return(1);
	}
#endif

	while (1)
	{
		if ((hr = ddsurf[1]->Lock(0,&ddsd,DDLOCK_SURFACEMEMORYPTR|DDLOCK_WAIT,0)) == DD_OK) break;
		if (hr == DDERR_SURFACELOST)
		{
			if (ddsurf[0]->Restore() != DD_OK) return(0);
			if (ddsurf[1]->Restore() != DD_OK) return(0);
		}
		if (hr == DDERR_CANTLOCKSURFACE) return(-1); //if true, set cantlockprimary = 1;
		if (hr != DDERR_WASSTILLDRAWING) return(0);
	}

		//DDLOCK_WAIT MANDATORY! (to prevent sudden exit back to windows)!
	//if (hr = ddsurf[1]->Lock(0,&ddsd,DDLOCK_SURFACEMEMORYPTR|DDLOCK_WAIT,0) != DD_OK)
	//   return(0);

	*vidplc = (long)ddsd.lpSurface; *dabpl = ddsd.lPitch;
	*daxres = xres; *dayres = yres; ddlocked = 1;
	return(1);
}

static HDC ghdc;
HDC startdc ()
{
#ifdef USED3D4FULL
	if (fullscreen) return(0); //d3ddev->GetDC(&ghdc); else
#endif
	ddsurf[1]->GetDC(&ghdc);
	return(ghdc);
}

void stopdc ()
{
#ifdef USED3D4FULL
	if (fullscreen) return; //d3ddev->ReleaseDC(ghdc); else
#endif
	ddsurf[1]->ReleaseDC(ghdc);
}

void nextpage ()
{
	HRESULT hr;

	if (ddrawdebugmode != -1) return;

		//Note: windowed mode could be faster by having kblit go directly
		// to the ddsurf[0] (windows video memory)
	if (ddrawuseemulation)
	{
		long i, fplace, bpl, xsiz, ysiz;
		if (!fullscreen)
		{
			POINT topLeft, p2;
			long j;
#ifndef NO_CLIPPER
			RECT *r;
			unsigned long siz;
#endif

				//Note: this hack for windowed mode saves an unnecessary blit by
				//   going directly to primary buffer (ddsurf[0])
				//  Unfortunately, this means it ignores the clipper & you can't
				//   use the ddraw stretch function :/
			topLeft.x = 0; topLeft.y = 0;
			if (!cantlockprimary)
			{
				ddsurf[2] = ddsurf[0]; ddsurf[0] = ddsurf[1]; ddsurf[1] = ddsurf[2];
				ClientToScreen(ghwnd,&topLeft);
			}
			else
			{
				p2.x = 0; p2.y = 0;
				ClientToScreen(ghwnd,&p2);
			}

				//This is sort of a hack... but it works
			i = ddrawuseemulation; ddrawuseemulation = 0;
			j = startdirectdraw(&fplace,&bpl,&xsiz,&ysiz);
			if (j == -1)
			{
				if (!cantlockprimary)
				{
					ddsurf[2] = ddsurf[0]; ddsurf[0] = ddsurf[1]; ddsurf[1] = ddsurf[2]; //Undo earlier surface swap
					cantlockprimary = 1; ddrawuseemulation = i; return;
				}
			}
			else if (j)
			{
#ifndef NO_CLIPPER
				ddclip->GetClipList(0,0,&siz);
				if (siz > ddcliprdbytes)
				{
					ddcliprdbytes = siz;
					ddcliprd = (RGNDATA *)realloc(ddcliprd,siz);
				}
				if (!ddcliprd)
				{
#endif
					kblit32(max(-topLeft.y,0)*ddrawemulbpl +
							  max(-topLeft.x,0)*((colbits+7)>>3) + ((long)ddrawemulbuf),
							  ddrawemulbpl,
							  min(topLeft.x+xsiz,GetSystemMetrics(SM_CXSCREEN))-max(topLeft.x,0),
							  min(topLeft.y+ysiz,GetSystemMetrics(SM_CYSCREEN))-max(topLeft.y,0),
							  max(topLeft.y,0)*bpl+max(topLeft.x,0)*((i+7)>>3)+fplace,
							  i,bpl);
#ifndef NO_CLIPPER
				}
				else
				{
					ddclip->GetClipList(0,ddcliprd,&siz);

					r = (RECT *)ddcliprd->Buffer;
					for(j=0;j<(long)ddcliprd->rdh.nCount;j++)
					{
						if (cantlockprimary) { r[j].left  -= p2.x; r[j].top -= p2.y; r[j].right -= p2.x; r[j].bottom -= p2.y; }
						kblit32(max(r[j].top -topLeft.y,0)*ddrawemulbpl +
								  max(r[j].left-topLeft.x,0)*((colbits+7)>>3) + ((long)ddrawemulbuf),
								  ddrawemulbpl,
								  min(topLeft.x+xsiz,r[j].right )-max(topLeft.x,r[j].left),
								  min(topLeft.y+ysiz,r[j].bottom)-max(topLeft.y,r[j].top ),
								  max(topLeft.x,r[j].left)*((i+7)>>3) +
								  max(topLeft.y,r[j].top )*bpl + fplace,
								  i,bpl);
					}
				}
#endif
				stopdirectdraw();
			}
			ddrawuseemulation = i;

				//Finish windowed mode hack to primary buffer
			if (!cantlockprimary) { ddsurf[2] = ddsurf[0]; ddsurf[0] = ddsurf[1]; ddsurf[1] = ddsurf[2]; return; }
		}
		else
		{
				//This is sort of a hack... but it works
			i = ddrawuseemulation; ddrawuseemulation = 0;
			if (startdirectdraw(&fplace,&bpl,&xsiz,&ysiz))
			{
			kblit32((long)ddrawemulbuf,ddrawemulbpl,min(xsiz,xres),min(ysiz,yres),fplace,i,bpl);
			stopdirectdraw();
			}
			ddrawuseemulation = i;
		}
	}

	if (fullscreen)
	{
#ifdef USED3D4FULL
#ifndef USE3DVISION
		d3ddev->Present(0,0,0,0);
#else
		if (g3dnumframes&1)
		{
			D3DMATRIX tmat;
			tmat._11 = 1.f; tmat._21 = 0.f; tmat._31 = 0.f;   tmat._41 = -1e-4-EYESEP;
			tmat._12 = 0.f; tmat._22 = 0.f; tmat._32 =-1.f;   tmat._42 = 1e-4;
			tmat._13 = 0.f; tmat._23 =-1.f; tmat._33 =-1e-16; tmat._43 = BACKDIST - tmat._33*1e-4;
			tmat._14 = 0.f; tmat._24 = 0.f; tmat._34 = 0.f;   tmat._44 = 1.f;
			d3ddev->SetTransform(D3DTS_WORLD,&tmat);
			d3ddev->SetTexture(0,gpt[0]);
			d3ddev->SetStreamSource(0,gpvb,sizeof(vert3d));
			d3ddev->SetVertexShader(D3DFVF);
			d3ddev->DrawPrimitive(D3DPT_TRIANGLEFAN,0,2);

			tmat._11 = 1.f; tmat._21 = 0.f; tmat._31 = 0.f;   tmat._41 = -1e-4+EYESEP;
			tmat._12 = 0.f; tmat._22 = 0.f; tmat._32 =-1.f;   tmat._42 = +1e-4;
			tmat._13 = 0.f; tmat._23 =-1.f; tmat._33 =-1e-16; tmat._43 = BACKDIST - tmat._33*1e-4;
			tmat._14 = 0.f; tmat._24 = 0.f; tmat._34 = 0.f;   tmat._44 = 1.f;
			d3ddev->SetTransform(D3DTS_WORLD,&tmat);
			d3ddev->SetTexture(0,gpt[1]);
			d3ddev->SetStreamSource(0,gpvb,sizeof(vert3d));
			d3ddev->SetVertexShader(D3DFVF);
			d3ddev->DrawPrimitive(D3DPT_TRIANGLEFAN,0,2);

			d3ddev->EndScene();
			d3ddev->Present(0,0,0,0);

			d3ddev->Clear(0,0,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xc04040,1.0,0);
			d3ddev->BeginScene();
		}
		g3dnumframes++;
#endif
		return;
#endif
#if (OFFSCREENHACK)
		RECT r; r.left = 0; r.top = 0; r.right = xres; r.bottom = yres; //with width hack, src rect not always: xres,yres
		if (ddsurf[2]->GetBltStatus(DDGBS_CANBLT) == DDERR_WASSTILLDRAWING) return;
		if (ddsurf[2]->BltFast(0,0,ddsurf[1],&r,DDBLTFAST_NOCOLORKEY) == DDERR_SURFACELOST)
			{ ddsurf[0]->Restore(); if (ddsurf[2]->BltFast(0,0,ddsurf[1],&r,DDBLTFAST_NOCOLORKEY) != DD_OK) return; }
		if (ddsurf[0]->GetFlipStatus(DDGFS_CANFLIP) == DDERR_WASSTILLDRAWING) return; //wait for blit to complete
		if (ddsurf[0]->Flip(0,0) == DDERR_SURFACELOST) { ddsurf[0]->Restore(); ddsurf[0]->Flip(0,0); }
#elif 1
			//NOTE! If not using DDFLIP_WAIT, >70fps only works
			//      when ddsd.dwBackBufferCount > 1 (triple+ buffering)
		while (1) //Recommended flipper from 1997 DirectDraw 3.0 MSDN CD:
		{
			if ((hr = ddsurf[0]->Flip(0,DDFLIP_WAIT)) == DD_OK) break;
			if (hr == DDERR_SURFACELOST)
			{
				if (ddsurf[0]->Restore() != DD_OK) break;
				if (ddsurf[1]->Restore() != DD_OK) break;
			}
			if (hr != DDERR_WASSTILLDRAWING) break;
		}
#else
			//This loop allows windows to breath while waiting for the flip
			//   so, it gives smoother execution
			// WARNING: bad things can happen if breath() receives WM_CLOSE!!!
		while (1)
		{
			if ((hr = ddsurf[0]->Flip(0,0)) == DD_OK) break;
			if (hr == DDERR_WASSTILLDRAWING) { breath(); continue; }
			if (hr == DDERR_SURFACELOST)
			{
				if (ddsurf[0]->Restore() != DD_OK) break;
				if (ddsurf[1]->Restore() != DD_OK) break;
			}
			break;
		}
#endif
	}
	else
	{
		RECT rcSrc, rcDst;
		POINT topLeft;

		rcSrc.left = 0; rcSrc.top = 0;
		rcSrc.right = xres; rcSrc.bottom = yres;
#ifndef ZOOM_TEST
		rcDst = rcSrc;
#else
		GetClientRect(ghwnd, &rcDst);
#endif
		topLeft.x = 0; topLeft.y = 0;
		ClientToScreen(ghwnd,&topLeft);
		rcDst.left += topLeft.x; rcDst.right += topLeft.x;
		rcDst.top += topLeft.y; rcDst.bottom += topLeft.y;
#ifdef NO_CLIPPER
		if (ddsurf[0]->BltFast(rcDst.left,rcDst.top,ddsurf[1],&rcSrc,DDBLTFAST_WAIT|DDBLTFAST_NOCOLORKEY) < 0)
#else
		if (ddsurf[0]->Blt(&rcDst,ddsurf[1],&rcSrc,DDBLT_WAIT|DDBLT_ASYNC,0) < 0)
#endif
		{
			if (ddsurf[0]->IsLost() == DDERR_SURFACELOST) ddsurf[0]->Restore();
			if (ddsurf[1]->IsLost() == DDERR_SURFACELOST) ddsurf[1]->Restore();
#ifdef NO_CLIPPER
			ddsurf[0]->BltFast(rcDst.left,rcDst.top,ddsurf[1],&rcSrc,DDBLTFAST_WAIT|DDBLTFAST_NOCOLORKEY);
#else
			ddsurf[0]->Blt(&rcDst,ddsurf[1],&rcSrc,DDBLT_WAIT|DDBLT_ASYNC,0);
#endif
		}
	}
}

void ddflip2gdi ()
{
#ifdef USED3D4FULL
	if (fullscreen) return;
#endif
	if (lpdd) lpdd->FlipToGDISurface();
}

long clearscreen (long fillcolor)
{
	DDBLTFX blt;
	HRESULT hr;

	if (ddrawuseemulation)
	{
		long c = ((xres*yres+7)&~7)*((colbits+7)>>3); c >>= 3;
		if (cputype&(1<<25)) //SSE
		{
			_asm
			{
				mov edx, ddrawemulbuf
				mov ecx, c
				movd mm0, fillcolor
				punpckldq mm0, mm0
  clear32a: movntq qword ptr [edx], mm0
				add edx, 8
				sub ecx, 1
				jnz short clear32a
				emms
			}
		}
		else if (cputype&(1<<23)) //MMX
		{
			_asm
			{
				mov edx, ddrawemulbuf
				mov ecx, c
				movd mm0, fillcolor
				punpckldq mm0, mm0
  clear32b: movq qword ptr [edx], mm0
				add edx, 8
				sub ecx, 1
				jnz short clear32b
				emms
			}
		}
		else
		{
			long q[2]; q[0] = q[1] = fillcolor;
			_asm
			{
				mov edx, ddrawemulbuf
				mov ecx, c
  clear32c: fild qword ptr q
				fistp qword ptr [edx] ;NOTE: fist doesn't have a 64-bit form!
				add edx, 8
				sub ecx, 1
				jnz short clear32c
			}
		}
		return(1);
	}

		//Can't clear when locked: would SUPERCRASH!
	if (ddlocked)
	{
		long i, j, x, p, pe, ddf, ddp;

#ifdef USED3D4FULL
		if (fullscreen) { ddf = d3df; ddp = d3dp; } else
#endif
		{ ddf = (long)ddsd.lpSurface; ddp = ddsd.lPitch; }

		p = ddf; pe = ddp*yres + p;
		if (colbits != 24)
		{
			switch(colbits)
			{
				case 8:           fillcolor = (fillcolor&255)*0x1010101; i = xres  ; break;
				case 15: case 16: fillcolor = (fillcolor&65535)*0x10001; i = xres*2; break;
				case 32:                                                 i = xres*4; break;
			}
			for(;p<pe;p+=ddp)
			{
#if 0
				for(x=p,j=p+i-4;x<=j;x+=4) *(long *)x = fillcolor;
#else
				j = (i>>3);
				if (cputype&(1<<25)) //SSE
				{
					_asm
					{
						mov edx, p
						mov ecx, j
						movd mm0, fillcolor
						punpckldq mm0, mm0
		  clear32d: movntq qword ptr [edx], mm0
						add edx, 8
						sub ecx, 1
						jnz short clear32d
						emms
					}
				}
				else if (cputype&(1<<23)) //MMX
				{
					_asm
					{
						mov edx, p
						mov ecx, j
						movd mm0, fillcolor
						punpckldq mm0, mm0
		  clear32e: movq qword ptr [edx], mm0
						add edx, 8
						sub ecx, 1
						jnz short clear32e
						emms
					}
				}
				else
				{
					long q[2]; q[0] = q[1] = fillcolor;
					_asm
					{
						mov edx, p
						mov ecx, j
		  clear32f: fild qword ptr q
						fistp qword ptr [edx] ;NOTE: fist doesn't have a 64-bit form!
						add edx, 8
						sub ecx, 1
						jnz short clear32f
					}
				}
				x = (i&~7)+p;
				if (i&4) { *(long  *)x =        fillcolor; x += 4; }
#endif
				if (i&2) { *(short *)x = (short)fillcolor; x += 2; }
				if (i&1) { *(char  *)x =  (char)fillcolor;         }
			}
			if (cputype&(1<<25)) _asm emms //SSE
		}
		else
		{
			long dacol[3];
			fillcolor &= 0xffffff;
			dacol[0] = (fillcolor<<24)+fillcolor;      //BRGB
			dacol[1] = (fillcolor>>8)+(fillcolor<<16); //GBRG
			dacol[2] = (fillcolor>>16)+(fillcolor<<8); //RGBR
			i = xres*3;
			for(;p<pe;p+=ddp)
			{
				j = p+i-12;
				for(x=p;x<=j;x+=12)
				{                             //  x-j ³0123³4567³89AB
					*(long *)(x  ) = dacol[0]; // ÚÄÄÄÄÅÄÄÄÄÅÄÄÄÄÅÄÄÄÄ¿
					*(long *)(x+4) = dacol[1]; // ³ 12 ³    ³    ³    ³
					*(long *)(x+8) = dacol[2]; // ³  9 ³BGR ³    ³    ³
				}                             // ³  6 ³BGRB³GR  ³    ³
				switch(x-j)                   // ³  3 ³BGRB³GRBG³R   ³
				{                             // ÀÄÄÄÄÁÄÄÄÄÁÄÄÄÄÁÄÄÄÄÙ
					case 9: *(short *)x = (short)dacol[0]; *(char *)(x+2) = (char)dacol[2]; break;
					case 6: *(long *)x = dacol[0]; *(short *)(x+4) = (short)dacol[1]; break;
					case 3: *(long *)x = dacol[0]; *(long *)(x+4) = dacol[1]; *(char *)(x+8) = (char)dacol[2]; break;
				}
			}
		}
		return(1);
	}

#ifdef USED3D4FULL
	if (fullscreen) { d3ddev->Clear(0,0,D3DCLEAR_TARGET,fillcolor,1.0,0); return; }
#endif

	blt.dwSize = sizeof(blt);
	blt.dwFillColor = fillcolor;
	while (1)
	{
			//Try |DDBLT_ASYNC
		hr = ddsurf[1]->Blt(0,0,0,DDBLT_COLORFILL,&blt); //Try |DDBLT_WAIT
		if (hr == DD_OK) return(1);
		if (hr == DDERR_SURFACELOST)
		{
			if (ddsurf[0]->Restore() != DD_OK) return(0);
			if (ddsurf[1]->Restore() != DD_OK) return(0);
		}
	}
}

long changeres (long daxres, long dayres, long dacolbits, long dafullscreen)
{
	uninitdirectdraw();

	ShowWindow(ghwnd,FALSE);
	if (dafullscreen)
	{
		SetWindowLong(ghwnd,GWL_EXSTYLE,WS_EX_TOPMOST);
		SetWindowLong(ghwnd,GWL_STYLE,WS_POPUP);
		MoveWindow(ghwnd,0,0,GetSystemMetrics(SM_CXSCREEN),
									GetSystemMetrics(SM_CYSCREEN),0);
	}
	else
	{
		RECT rw;
		SystemParametersInfo(SPI_GETWORKAREA,0,&rw,0);

		SetWindowLong(ghwnd,GWL_EXSTYLE, 0);
		SetWindowLong(ghwnd,GWL_STYLE,progwndflags);
		MoveWindow(ghwnd,((rw.right -rw.left-(daxres+progwndadd[0]))>>1) + rw.left,
							  ((rw.bottom-rw.top -(dayres+progwndadd[1]))>>1) + rw.top,
							  daxres+progwndadd[0], dayres+progwndadd[1], 0);
	}
	ShowWindow(ghwnd,TRUE);

	fullscreen = dafullscreen;
	return(initdirectdraw(daxres,dayres,dacolbits));
}

//   //How to use dc & draw text:
//HDC hdc;
//if (ddsurf[1]->GetDC(&hdc) == DD_OK)
//{
//   SetBkColor(hdc,RGB(0,0,255)); //SetBkMode(hdc,TRANSPARENT);
//   SetTextColor(hdc,RGB(255,255,0));
//   //HFONT fontInUse = CreateFont(size,0,0,0,FW_NORMAL,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,
//   //   CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,VARIABLE_PITCH,"Arial");
//   //oldFont = SelectObject(hdc,fontInUse);
//   TextOut(hdc,x,y,message,strlen(message));
//   //SelectObject(hdc,oldFont);
//   //DeleteObject(fontInUse);
//   ddsurf[1]->ReleaseDC(hdc);
//}

//   //Do this to ensure GDI stuff is on the screen:
//lpdd->FlipToGDISurface();

//   //Use real timers, (forget SetTimer/WM_TIMER crap):
//#include <mmsystem.h>
//void CALLBACK OneShotTimer(UINT wTimerID, UINT msg, DWORD dwUser, DWORD dw1, DWORD dw2)
//   { printf("Alarm!"); }
//timeSetEvent(msInterval,msInterval,OneShotTimer,(DWORD)npSeq,TIME_PERIODIC);

//   //09/26/2005:
//#define DIRECTDRAW_VERSION 0x0300
//#include <ddraw.h>
//extern LPDIRECTDRAW lpdd;
//   unsigned long u;
//   lpdd->GetMonitorFrequency(&u); //u=85...
//   lpdd->WaitForVerticalBlank(DDWAITVB_BLOCKBEGIN,0); //DDWAITVB_BLOCKBEGIN,DDWAITVB_BLOCKEND
//   lpdd->GetScanLine(&u); //0 to yres+blanking-1 DD_OK,DDERR_VERTICALBLANKINPROGRESS
//   lpdd->GetVerticalBlankStatus((int *)&i); //0 or 1

#endif

//DirectInput VARIABLES & CODE-------------------------------------------------------
char keystatus[256];
static long shkeystatus = 0;
#define KEYBUFSIZ 256
static long keybuf[KEYBUFSIZ], keybufr = 0, keybufw = 0, keybufw2 = 0;

char ext_keystatus[256]; // +TD
char ext_mbstatus[8] = {0}; // +TD extended mouse button status
long ext_mwheel = 0;
#ifdef NOINPUT
long mouse_acquire = 0;
#else
long mouse_acquire = 1, kbd_acquire = 1;
static void (*setmousein)(long, long) = NULL;
static long mouse_out_x, mouse_out_y;
static HANDLE dinputevent[2] = {0,0};

static LPDIRECTINPUT gpdi = 0;
long initdirectinput (HWND hwnd)
{
	HRESULT hr;
	char buf[256];

	if ((hr = DirectInputCreate(ghinst,DIRECTINPUT_VERSION,&gpdi,0)) >= 0) return(1);
	wsprintf(buf,"initdirectinput failed: %08lx\n",hr);
	MessageBox(ghwnd,buf,"ERROR",MB_OK);
	return(0);
}

void uninitdirectinput ()
{
	if (gpdi) { gpdi->Release(); gpdi = 0; }
}

//DirectInput (KEYBOARD) VARIABLES & CODE-------------------------------------------------------
static LPDIRECTINPUTDEVICE gpKeyboard = 0;
#define KBDBUFFERSIZE 64
DIDEVICEOBJECTDATA KbdBuffer[KBDBUFFERSIZE];

//#define KEYBUFSIZ 256
//static long keybuf[KEYBUFSIZ], keybufr = 0, keybufw = 0, keybufw2 = 0;

void uninitkeyboard ()
{
	if (gpKeyboard)
	{
		if (dinputevent[1])
		{
			gpKeyboard->SetEventNotification(dinputevent[1]);
			CloseHandle(dinputevent[1]); dinputevent[1] = 0;
		}
		gpKeyboard->Unacquire(); gpKeyboard->Release(); gpKeyboard = 0;
	}
}

long initkeyboard (HWND hwnd)
{
	HRESULT hr;
	DIPROPDWORD dipdw;
	char buf[256];

	if ((hr = gpdi->CreateDevice(GUID_SysKeyboard,&gpKeyboard,0)) < 0) goto initkeyboard_bad;
	if ((hr = gpKeyboard->SetDataFormat(&c_dfDIKeyboard)) < 0) goto initkeyboard_bad;
	if ((hr = gpKeyboard->SetCooperativeLevel(hwnd,dinputkeyboardflags)) < 0) goto initkeyboard_bad;

	dinputevent[1] = CreateEvent(0,0,0,0); if (!dinputevent[1]) goto initkeyboard_bad;
	if ((hr = gpKeyboard->SetEventNotification(dinputevent[1])) < 0) goto initkeyboard_bad;

	dipdw.diph.dwSize = sizeof(DIPROPDWORD);
	dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	dipdw.diph.dwObj = 0;
	dipdw.diph.dwHow = DIPH_DEVICE;
	dipdw.dwData = KBDBUFFERSIZE;
	if ((hr = gpKeyboard->SetProperty(DIPROP_BUFFERSIZE,&dipdw.diph)) < 0) goto initkeyboard_bad;
	if (kbd_acquire) { gpKeyboard->Acquire(); shkeystatus = 0; }
	return(1);

initkeyboard_bad:;
	uninitkeyboard();
	wsprintf(buf,"initdirectinput(keyboard) failed: %08lx\n",hr);
	MessageBox(ghwnd,buf,"ERROR",MB_OK);
	return(0);
}

long readkeyboard ()
{
	HRESULT hr;
	long i;
	unsigned long dwItems;
	DIDEVICEOBJECTDATA *lpdidod;

	dwItems = KBDBUFFERSIZE;
	hr = gpKeyboard->GetDeviceData(sizeof(DIDEVICEOBJECTDATA),KbdBuffer,&dwItems,0);
	//if (hr == DI_BUFFEROVERFLOW) ?;
	if ((hr == DIERR_INPUTLOST) || (hr == DIERR_NOTACQUIRED))
	{
		gpKeyboard->Acquire(); shkeystatus = 0;
		hr = gpKeyboard->GetDeviceData(sizeof(DIDEVICEOBJECTDATA),KbdBuffer,&dwItems,0);
	}
	if (hr < 0) return(0);
	for(i=0;i<(long)dwItems;i++)
	{
		lpdidod = &KbdBuffer[i];

		if (lpdidod->dwData&128) keystatus[lpdidod->dwOfs] = 1;
								  else keystatus[lpdidod->dwOfs] = 0;
		//event occured "GetTickCount()-lpdidod->dwTimeStamp" milliseconds ago

		// +TD:
		if (lpdidod->dwData&128) ext_keystatus[lpdidod->dwOfs] = 1|2;
								  else ext_keystatus[lpdidod->dwOfs] &= ~1; // preserve bit 2 only
	}
	return(dwItems);
}


//DirectInput (MOUSE) VARIABLES & CODE-------------------------------------------------------

static LPDIRECTINPUTDEVICE gpMouse = 0;
#define MOUSBUFFERSIZE 64
DIDEVICEOBJECTDATA MousBuffer[MOUSBUFFERSIZE];
static long gbstatus = 0, gkillbstatus = 0;

	//Mouse smoothing variables:
long mousmoth = 1;
static double dmoutsc;
float mousper;
static float mousince, mougoalx, mougoaly, mougoalz, moutscale;
static long moult[4], moultavg, moultavgcnt;

void uninitmouse ()
{
	if (gpMouse)
	{
		if (dinputevent[0])
		{
			gpMouse->SetEventNotification(dinputevent[0]);
			CloseHandle(dinputevent[0]); dinputevent[0] = 0;
		}
		gpMouse->Unacquire(); gpMouse->Release(); gpMouse = 0;
	}
}

long initmouse (HWND hwnd)
{
	HRESULT hr;
	DIPROPDWORD dipdw;
	char buf[256];

	if ((hr = gpdi->CreateDevice(GUID_SysMouse,&gpMouse,0)) < 0) goto initmouse_bad;
	if ((hr = gpMouse->SetDataFormat(&c_dfDIMouse)) < 0) goto initmouse_bad;
	if ((hr = gpMouse->SetCooperativeLevel(hwnd,dinputmouseflags)) < 0) goto initmouse_bad;

	dinputevent[0] = CreateEvent(0,0,0,0); if (!dinputevent[0]) goto initmouse_bad;
	if ((hr = gpMouse->SetEventNotification(dinputevent[0])) < 0) goto initmouse_bad;

	dipdw.diph.dwSize = sizeof(DIPROPDWORD);
	dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	dipdw.diph.dwObj = 0;
	dipdw.diph.dwHow = DIPH_DEVICE;
	dipdw.dwData = MOUSBUFFERSIZE;
	if ((hr = gpMouse->SetProperty(DIPROP_BUFFERSIZE, &dipdw.diph)) < 0) goto initmouse_bad;

	if (mouse_acquire) { gpMouse->Acquire(); gbstatus = 0; }
	mousper = 1.0; mousince = mougoalx = mougoaly = mougoalz = 0.0;
	moult[0] = -1; moultavg = moultavgcnt = 0;
	readklock(&dmoutsc);
	return(1);

initmouse_bad:;
	uninitmouse();
	wsprintf(buf,"initdirectinput(mouse) failed: %08lx\n",hr);
	MessageBox(ghwnd,buf,"ERROR",MB_OK);
	return(0);
}

void readmouse (float *fmousx, float *fmousy, float *fmousz, long *bstatus)
{
	double odmoutsc;
	float f, fmousynctics;
	long i, got, ltmin, ltmax, nlt0, nlt1, nlt2;
	long mousx, mousy, mousz;
	HRESULT hr;
	unsigned long dwItems;
	DIDEVICEOBJECTDATA *lpdidod;

	if ((!mouse_acquire) || (!gpMouse)) { *fmousx = 0; *fmousy = 0; *fmousz = 0; *bstatus = 0; return; }

	dwItems = MOUSBUFFERSIZE;
	hr = gpMouse->GetDeviceData(sizeof(DIDEVICEOBJECTDATA),MousBuffer,&dwItems,0);
	if (hr == DI_BUFFEROVERFLOW) moult[0] = -1;
	if ((hr == DIERR_INPUTLOST) || (hr == DIERR_NOTACQUIRED))
	{
		gpMouse->Acquire(); gbstatus = 0;
		hr = gpMouse->GetDeviceData(sizeof(DIDEVICEOBJECTDATA),MousBuffer,&dwItems,0);
		if ((hr == DIERR_INPUTLOST) || (hr == DIERR_NOTACQUIRED)) { *fmousx = 0; *fmousy = 0; *fmousz = 0; *bstatus = 0; return; }
	}

		//Estimate mouse period (mousper) in units of CPU cycles:
	mousx = mousy = mousz = 0; got = 0;
	i = 0; lpdidod = &MousBuffer[i];
	while (i < (long)dwItems)
	{
		moult[3] = moult[2]; moult[2] = moult[1]; moult[1] = moult[0];
		moult[0] = lpdidod->dwTimeStamp;
		do
		{
			switch(lpdidod->dwOfs)
			{
				case DIMOFS_X: mousx += lpdidod->dwData; break;
				case DIMOFS_Y: mousy += lpdidod->dwData; break;
				case DIMOFS_Z: mousz += lpdidod->dwData; break;
				case DIMOFS_BUTTON0: if (lpdidod->dwData&128) ext_mbstatus[0] = 1|2; else ext_mbstatus[0] &= 2;
											gbstatus = ((gbstatus&~1)|((lpdidod->dwData>>7)&1)); moult[0] = -1; break;
				case DIMOFS_BUTTON1: if (lpdidod->dwData&128) ext_mbstatus[1] = 1|2; else ext_mbstatus[1] &= 2;
											gbstatus = ((gbstatus&~2)|((lpdidod->dwData>>6)&2)); moult[0] = -1; break;
				case DIMOFS_BUTTON2: if (lpdidod->dwData&128) ext_mbstatus[2] = 1|2; else ext_mbstatus[2] &= 2;
											gbstatus = ((gbstatus&~4)|((lpdidod->dwData>>5)&4)); moult[0] = -1; break;
				case DIMOFS_BUTTON3: if (lpdidod->dwData&128) ext_mbstatus[3] = 1|2; else ext_mbstatus[3] &= 2;
											gbstatus = ((gbstatus&~8)|((lpdidod->dwData>>4)&8)); moult[0] = -1; break;
			}
			i++; lpdidod = &MousBuffer[i];
		} while ((i < (long)dwItems) && ((long)lpdidod->dwTimeStamp == moult[0]));

		if (moult[0] != -1)
		{
			got++;
			if ((moult[1] != -1) && (moult[2] != -1) && (moult[3] != -1))
			{
				nlt0 = moult[0]-moult[1];
				nlt1 = moult[1]-moult[2];
				nlt2 = moult[2]-moult[3];
				ltmin = nlt0; ltmax = nlt0;
				if (nlt1 < ltmin) ltmin = nlt1;
				if (nlt2 < ltmin) ltmin = nlt2;
				if (nlt1 > ltmax) ltmax = nlt1;
				if (nlt2 > ltmax) ltmax = nlt2;
				if (ltmin*2 >= ltmax) //WARNING: NT's timer has 10ms resolution!
				{
					moultavg += moult[0]-moult[3]; moultavgcnt += 3;
					mousper = (float)moultavg/(float)moultavgcnt;
				}
			}
		}
	}
	if (gkillbstatus) { gkillbstatus = 0; gbstatus = 0; } //Flush packets after task switch
	(*bstatus) = gbstatus;

		//Calculate and return smoothed mouse data in: (fmousx, fmousy)
	odmoutsc = dmoutsc; readklock(&dmoutsc);
	fmousynctics = (float)((dmoutsc-odmoutsc)*1000.0);

		//At one time, readklock() wasn't always returning increasing values.
		//This made fmousynctics <= 0 possible, causing /0. Fixed now :)
	if ((!moultavgcnt) || (!mousmoth)) //|| ((*(long *)&fmousynctics) <= 0))
		{ (*fmousx) = (float)mousx; (*fmousy) = (float)mousy; (*fmousz) = (float)mousz; return; }

	mousince = min(mousince+mousper*(float)got,mousper+fmousynctics);
	if (fmousynctics >= mousince) { f = 1; mousince = 0; }
	else { f = fmousynctics / mousince; mousince -= fmousynctics; }
	mougoalx += (float)mousx; (*fmousx) = mougoalx*f; mougoalx -= (*fmousx);
	mougoaly += (float)mousy; (*fmousy) = mougoaly*f; mougoaly -= (*fmousy);
	mougoalz += (float)mousz; (*fmousz) = mougoalz*f; mougoalz -= (*fmousz);
}
void readmouse (float *fmousx, float *fmousy, long *bstatus)
{
	float fmousz;
	readmouse(fmousx,fmousy,&fmousz,bstatus);
}

void smartsleep (long timeoutms)
{
	MsgWaitForMultipleObjects(2,dinputevent,0,timeoutms,QS_KEY|QS_PAINT);
}

#endif


// Kensound code begins -------------------------------------------------------

#ifndef NOSOUND
//--------------------------------------------------------------------------------------------------
#define MAXUMIXERS 256
#define UMIXERBUFSIZ 65536
typedef struct
{
	LPDIRECTSOUNDBUFFER streambuf;
	long samplerate, numspeakers, bytespersample, oplaycurs;
	void (*mixfunc)(void *dasnd, long danumbytes);
} umixertyp;
static umixertyp umixer[MAXUMIXERS];
static long umixernum = 0;

extern LPDIRECTSOUND dsound; //Defined later

long umixerstart (void damixfunc (void *, long), long dasamprate, long danumspeak, long dabytespersamp)
{
	DSBUFFERDESC dsbdesc;
	DSBCAPS dsbcaps;
	WAVEFORMATEX wfx;
	void *w0 = 0, *w1 = 0;
	unsigned long l0 = 0, l1 = 0;

	if ((dasamprate <= 0) || (danumspeak <= 0) || (dabytespersamp <= 0) || (umixernum >= MAXUMIXERS)) return(-1);

	umixer[umixernum].samplerate = dasamprate;
	umixer[umixernum].numspeakers = danumspeak;
	umixer[umixernum].bytespersample = dabytespersamp;

	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nSamplesPerSec = dasamprate;
	wfx.wBitsPerSample = (dabytespersamp<<3);
	wfx.nChannels = danumspeak;
	wfx.nBlockAlign = (wfx.wBitsPerSample>>3) * wfx.nChannels;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
	wfx.cbSize = 0;

	memset(&dsbdesc,0,sizeof(DSBUFFERDESC));
	dsbdesc.dwSize = sizeof(DSBUFFERDESC);
	dsbdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2|DSBCAPS_LOCSOFTWARE|DSBCAPS_GLOBALFOCUS;
	dsbdesc.dwBufferBytes = UMIXERBUFSIZ;
	dsbdesc.lpwfxFormat = &wfx;
	if (dsound->CreateSoundBuffer(&dsbdesc,&umixer[umixernum].streambuf,0) != DS_OK) return(-1);

		//Zero out streaming buffer before beginning play
	umixer[umixernum].streambuf->Lock(0,UMIXERBUFSIZ,&w0,&l0,&w1,&l1,0);
	if (w0) memset(w0,(dabytespersamp-2)&128,l0);
	if (w1) memset(w1,(dabytespersamp-2)&128,l1);
	umixer[umixernum].streambuf->Unlock(w0,l0,w1,l1);

	umixer[umixernum].streambuf->SetCurrentPosition(0);
	umixer[umixernum].streambuf->Play(0,0,DSBPLAY_LOOPING);

	umixer[umixernum].oplaycurs = 0;
	umixer[umixernum].mixfunc = damixfunc;
	umixernum++;

	return(umixernum-1);
}

void umixerkill (long i)
{
	if ((unsigned long)i >= umixernum) return;
	if (umixer[i].streambuf)
		{ umixer[i].streambuf->Stop(); umixer[i].streambuf->Release(); umixer[i].streambuf = 0; }
	umixernum--; if (i != umixernum) umixer[i] = umixer[umixernum];
}

void umixerbreathe ()
{
	void *w0 = 0, *w1 = 0;
	unsigned long l, l0 = 0, l1 = 0, playcurs, writcurs;
	long i;

	for(i=0;i<umixernum;i++)
	{
		if (!umixer[i].streambuf) continue;
		if (umixer[i].streambuf->GetCurrentPosition(&playcurs,&writcurs) != DS_OK) continue;
		playcurs += ((umixer[i].samplerate/8)<<(umixer[i].bytespersample+umixer[i].numspeakers-2));
		l = (((playcurs-umixer[i].oplaycurs)&(UMIXERBUFSIZ-1))>>(umixer[i].bytespersample+umixer[i].numspeakers-2));
		if (l <= 256) continue;
		if (umixer[i].streambuf->Lock(umixer[i].oplaycurs&(UMIXERBUFSIZ-1),l<<(umixer[i].bytespersample+umixer[i].numspeakers-2),&w0,&l0,&w1,&l1,0) != DS_OK) continue;
		if (w0) umixer[i].mixfunc(w0,l0);
		if (w1) umixer[i].mixfunc(w1,l1);
		umixer[i].oplaycurs += l0+l1;
		umixer[i].streambuf->Unlock(w0,l0,w1,l1);
	}
}
//--------------------------------------------------------------------------------------------------
typedef struct { float x, y, z; } point3d;
#if (USEKENSOUND == 1)
#include <math.h>

	//Internal streaming buffer variables:
#define SNDSTREAMSIZ 16384 //(16384) Keep this a power of 2!
#define MINBREATHREND 256  //(256) Keep this high to avoid clicks, but much smaller than SNDSTREAMSIZ!
#define LSAMPREC 10        //(11) No WAV file can have more than 2^(31-LSAMPREC) samples
#define MAXPLAYINGSNDS 256 //(256)
#define VOLSEPARATION 1.f  //(1.f) Range: 0 to 1, .5 good for headphones, 1 good for speakers
#define EARSEPARATION 8.f  //(8.f) Used for L/R sample shifting
#define SNDMINDIST 32.f    //(32.f) Shortest distance where volume is max / volume scale factor
#define SNDSPEED 4096.f    //(4096.f) Speed of sound in units per second
#define NUMCOEF 4          //(4) # filter coefficients (4 and 8 are good choices)
#define LOGCOEFPREC 5      //(5) number of fractional bits of precision: 5 is ok
#define COEFRANG 32757     //(32757) DON'T make this > 32757 (not a typo) or else it'll overflow
#define KSND_3D 1
#define KSND_MOVE 2
#define KSND_LOOP 4
#define KSND_LOPASS 8
#define KSND_MEM 16
#define KSND_LOOPFADE 32    //for internal use only

typedef struct //For sounds longer than SNDSTREAMSIZ
{
	point3d *ptr; //followstat (-1 means not 3D sound, 0 means 3D sound but don't follow, else follow 3D!)
	point3d p;    //current position
	long flags;   //Bit0:1=3D, Bit1:1=follow 3D, Bit2:1=loop
	long ssnd;    //source sound
	long ispos;   //sub-sample counter (before doppler delay)
	long ispos0;  //L sub-sample counter
	long ispos1;  //R sub-sample counter
	long isinc;   //sub-sample increment
	long ivolsc;  //volume scale
	long ivolsc0; //L volume scale
	long ivolsc1; //R volume scale
	short *coefilt; //pointer to coef (for low pass filter, etc...)
} rendersndtyp;
static rendersndtyp rendersnd[MAXPLAYINGSNDS];
static long numrendersnd = 0;

LPDIRECTSOUNDBUFFER streambuf = 0;
#endif
LPDIRECTSOUND dsound = 0;
#if (USEKENSOUND == 1)

	//format: (used by audplay* to cache filenames&files themselves)
	//[index to next hashindex or -1][index to last filnam][ptr to snd_buf][char snd_filnam[?]\0]
#define AUDHASHINITSIZE 8192
static char *audhashbuf = 0;
#define AUDHASHEADSIZE 256 //must be power of 2
static long audhashead[AUDHASHEADSIZE], audhashpos, audlastloadedwav, audhashsiz;

static point3d audiopos, audiostr, audiohei, audiofor;
static float rsamplerate;
static long lsnd[SNDSTREAMSIZ>>1], samplerate, numspeakers, bytespersample, oplaycurs = 0;
static char gshiftval = 0;
__declspec(align(16)) static short coef[NUMCOEF<<LOGCOEFPREC]; //Sound re-scale filter coefficients
__declspec(align(16)) static short coeflopass[NUMCOEF<<LOGCOEFPREC]; //Sound re-scale filter coefficients


	//Same as: stricmp(st0,st1) except: '/' == '\'
static long filnamcmp (const char *st0, const char *st1)
{
	long i;
	char ch0, ch1;

	for(i=0;st0[i];i++)
	{
		ch0 = st0[i]; if ((ch0 >= 'a') && (ch0 <= 'z')) ch0 -= 32;
		ch1 = st1[i]; if ((ch1 >= 'a') && (ch1 <= 'z')) ch1 -= 32;
		if (ch0 == '/') ch0 = '\\';
		if (ch1 == '/') ch1 = '\\';
		if (ch0 != ch1) return(-1);
	}
	if (!st1[i]) return(0);
	return(-1);
}

static long audcalchash (const char *st)
{
	long i, hashind;
	char ch;

	for(i=0,hashind=0;st[i];i++)
	{
		ch = st[i];
		if ((ch >= 'a') && (ch <= 'z')) ch -= 32;
		if (ch == '/') ch = '\\';
		hashind = (ch - hashind*3);
	}
	return(hashind&(AUDHASHEADSIZE-1));
}

static long audcheckhashsiz (long siz)
{
	long i;

	if (!audhashbuf) //Initialize hash table on first call
	{
		memset(audhashead,-1,sizeof(audhashead));
		if (!(audhashbuf = (char *)malloc(AUDHASHINITSIZE))) return(0);
		audhashpos = 0; audlastloadedwav = -1; audhashsiz = AUDHASHINITSIZE;
	}
	if (audhashpos+siz > audhashsiz) //Make sure string fits in audhashbuf
	{
		i = audhashsiz; do { i <<= 1; } while (audhashpos+siz > i);
		if (!(audhashbuf = (char *)realloc(audhashbuf,i))) return(0);
		audhashsiz = i;
	}
	return(1);
}

static void kensoundclose ()
{
	numrendersnd = 0;

	ENTERMUTX;
	if (streambuf) streambuf->Stop();

	if (audhashbuf)
	{
		long i;
		for(i=audlastloadedwav;i>=0;i=(*(long *)&audhashbuf[i+4]))
			free((void *)(*(long *)&audhashbuf[i+8]));
		free(audhashbuf); audhashbuf = 0;
	}
	audhashpos = audhashsiz = 0;

	if (streambuf) { streambuf->Release(); streambuf = 0; }
	LEAVEMUTX;
}

#define PI 3.14159265358979323
static void initfilters ()
{
	float f, f2, f3, f4, fcoef[NUMCOEF];
	long i, j, k, cenbias;

		//Generate polynomial filter - fewer divides algo. (See PIANO.C for derivation)
	f4 = 1.0 / (float)(1<<LOGCOEFPREC);
	for(j=0;j<NUMCOEF;j++)
	{
		for(k=0,f3=1.0;k<j;k++) f3 *= (float)(j-k);
		for(k=j+1;k<NUMCOEF;k++) f3 *= (float)(j-k);
		f3 = COEFRANG / f3; f = ((NUMCOEF-1)>>1);
		for(i=0;i<(1<<LOGCOEFPREC);i++,f+=f4)
		{
			for(k=0,f2=f3;k<j;k++) f2 *= (f-(float)k);
			for(k=j+1;k<NUMCOEF;k++) f2 *= (f-(float)k);
			coef[i*NUMCOEF+j] = (short)f2;
		}
	}
		//Sinc*RaisedCos filter (window-designed filter):
	cenbias = (long)((float)(NUMCOEF-1)*.5f);
	for(i=0;i<(1<<LOGCOEFPREC);i++)
	{
		f3 = 0;
		for(j=0;j<NUMCOEF;j++)
		{
			f = ((float)((cenbias-j)*(1<<LOGCOEFPREC)+i))*PI/(float)(1<<LOGCOEFPREC);
			if (f == 0) f2 = 1.f; else f2 = (float)(sin(f*.25) / (f*.25));
			f2 *= (cos((f+f)/(float)NUMCOEF)*.5+.5); //Hanning
			fcoef[j] = f2; f3 += f2;
		}
		f3 = COEFRANG/f3; //NOTE: Enabling this code makes it LESS continuous!
		for(j=0;j<NUMCOEF;j++) coeflopass[i*NUMCOEF+j] = (short)(fcoef[j]*f3);
	}
}

long kensoundflags = DSBCAPS_GETCURRENTPOSITION2;
static int kensoundinit (LPDIRECTSOUND dsound, int samprate, int numchannels, int bytespersamp)
{
	WAVEFORMATEX wft;
	DSBUFFERDESC bufdesc;
	void *w0, *w1;
	unsigned long l0, l1;

	if (streambuf) kensoundclose();

	samplerate = samprate; rsamplerate = 1.0/(float)samplerate;
	numspeakers = numchannels;
	bytespersample = bytespersamp;
	gshiftval = (numspeakers+bytespersample-2);

	wft.wFormatTag = WAVE_FORMAT_PCM;
	wft.nSamplesPerSec = samprate;
	wft.wBitsPerSample = (bytespersamp<<3);
	wft.nChannels = numchannels;
	wft.nBlockAlign = bytespersamp*wft.nChannels;
	wft.nAvgBytesPerSec = wft.nSamplesPerSec * wft.nBlockAlign;
	wft.cbSize = 0;

	bufdesc.dwSize = sizeof(DSBUFFERDESC);
	bufdesc.dwFlags = kensoundflags;
	  //|DSBCAPS_LOCSOFTWARE; //lowers latency, but causes delays between DirectInput & threads !??
	bufdesc.dwBufferBytes = SNDSTREAMSIZ;
	bufdesc.dwReserved = 0;
	bufdesc.lpwfxFormat = &wft;
	if (dsound->CreateSoundBuffer(&bufdesc,&streambuf,0) != DS_OK) return(-1);

	//streambuf->SetVolume(curmusicvolume);
	//streambuf->SetFrequency(curmusicfrequency);
	//streambuf->SetPan(curmusicpan);

		//Zero out streaming buffer before beginning play
	streambuf->Lock(0,SNDSTREAMSIZ,&w0,&l0,&w1,&l1,0);
	if (w0) memset(w0,(bytespersample-2)&128,l0);
	if (w1) memset(w1,(bytespersample-2)&128,l1);
	streambuf->Unlock(w0,l0,w1,l1);

	initfilters();

	streambuf->SetCurrentPosition(0);
	streambuf->Play(0,0,DSBPLAY_LOOPING);

	return(0);
}

static __forceinline long mulshr16 (long a, long d)
{
	_asm
	{
		mov eax, a
		imul d
		shrd eax, edx, 16
	}
}

static __forceinline long mulshr32 (long a, long d)
{
	_asm
	{
		mov eax, a
		imul d
		mov eax, edx
	}
}

	//   ssnd: source sound
	//  ispos: sub-sample counter
	//  isinc: sub-sample increment per destination sample
	// ivolsc: 31-bit volume scale
	//ivolsci: 31-bit ivolsc increment per destination sample
	//   lptr: 32-bit sound pointer (step by 8 because rendersamps only renders 1 channel!)
	//  nsamp: number of destination samples to render
static void rendersamps (long dasnd, long ispos, long isinc, long ivolsc, long ivolsci, long *lptr, long nsamp, short *coefilt)
{
	long i, j, k;

	i = isinc*nsamp + ispos; if (i < isinc)              return; //ispos <  0 for all samples!
	j = *(long *)(dasnd-8);  if ((ispos>>LSAMPREC) >= j) return; //ispos >= j for all samples!
		//Clip off low ispos
	if (ispos < 0) { k = (isinc-1-ispos)/isinc; ivolsc += ivolsci*k; ispos += isinc*k; lptr += k*2; nsamp -= k; }
	if (((i-isinc)>>LSAMPREC) >= j) nsamp = ((j<<LSAMPREC)-ispos+isinc-1)/isinc; //Clip off high ispos

#if (NUMCOEF == 1)
	do //Very fast sound rendering; terrible quality!
	{
		lptr[0] += mulshr32(((short *)dasnd)[ispos>>LSAMPREC],ivolsc);
		ivolsc += ivolsci; ispos += isinc; lptr += 2; nsamp--;
	} while (nsamp);
#elif (NUMCOEF == 2)
	short *ssnd;  //Fast sound rendering; ok quality
	do
	{
		ssnd = (short *)(((ispos>>LSAMPREC)<<1)+dasnd);
		lptr[0] += mulshr32((long)(((((long)ssnd[1])-((long)ssnd[0]))*(ispos&((1<<LSAMPREC)-1)))>>LSAMPREC)+((long)ssnd[0]),ivolsc);
		ivolsc += ivolsci; ispos += isinc; lptr += 2; nsamp--;
	} while (nsamp);
#elif (NUMCOEF == 4)
#if 0
	short *soef, *ssnd; //Slow sound rendering; great quality
	do
	{
		ssnd = (short *)(((ispos>>LSAMPREC)<<1)+dasnd);
		soef = &coef[((ispos&((1<<LSAMPREC)-1))>>(LSAMPREC-LOGCOEFPREC))<<2];
		lptr[0] += mulshr32((long)ssnd[0]*(long)soef[0]+
								  (long)ssnd[1]*(long)soef[1]+
								  (long)ssnd[2]*(long)soef[2]+
								  (long)ssnd[3]*(long)soef[3],ivolsc>>15);
		ivolsc += ivolsci; ispos += isinc; lptr += 2; nsamp--;
	} while (nsamp);
#else

		//c = &coef[l];
		//h = &ssnd[u>>LSAMPREC]
		// c[3]  c[2]  c[1]  c[0]
		//*h[3] *h[2] *h[1] *h[0]
		//+p[3] +p[2] +p[1] +p[0] = s1
	if (cputype&(1<<23)) //MMX
	{
		_asm
		{
			push ebx
			push esi
			push edi
			push ebp
			mov esi, ispos
			mov edi, lptr
			mov ecx, nsamp
			lea edi, [edi+ecx*8]
			neg ecx
			mov edx, dasnd
			mov ebx, isinc
			movd mm2, ivolsc
			movd mm3, ivolsci

			test cputype, 1 shl 22
			mov ebp, coefilt
			jnz short srndmmp

			mov eax, 0xffff0000
			movd mm7, eax
 srndmmx:mov eax, esi       ;MMX loop begins here
			shr esi, LSAMPREC
			movq mm0, [edx+esi*2]
			lea esi, [eax+ebx]
			and eax, (1<<LSAMPREC)-1
			shr eax, LSAMPREC-LOGCOEFPREC
			pmaddwd mm0, [ebp+eax*8]
			movd mm5, [edi+ecx*8]
			movq mm1, mm0
			punpckhdq mm0, mm0
			paddd mm0, mm1
			movq mm4, mm2
			pand mm4, mm7  ;mm4: 0 0 (ivolsc&0xff00) 0
			pmaddwd mm0, mm4
			paddd mm2, mm3
			psrad mm0, 15
			paddd mm5, mm0
			movd [edi+ecx*8], mm5
			add ecx, 1
			jnz short srndmmx
			jmp short srend

 srndmmp:mov eax, esi       ;MMX+ loop begins here
			shr esi, LSAMPREC
			movq mm0, [edx+esi*2]
			lea esi, [eax+ebx]
			and eax, (1<<LSAMPREC)-1
			shr eax, LSAMPREC-LOGCOEFPREC
			pmaddwd mm0, [ebp+eax*8]
			movd mm5, [edi+ecx*8]
			pshufw mm1, mm0, 0xe
			paddd mm0, mm1
			pshufw mm4, mm2, 0xe6  ;mm4: 0 0 (ivolsc&0xff00) 0
			pmaddwd mm0, mm4
			paddd mm2, mm3
			psrad mm0, 15
			paddd mm5, mm0
			movd [edi+ecx*8], mm5
			add ecx, 1
			jnz short srndmmp
srend:
			pop ebp
			pop edi
			pop esi
			pop ebx
		}
	}
#endif
#endif
}

static void rendersampsloop (long dasnd, long ispos, long isinc, long ivolsc, long ivolsci, long *lptr, long nsamp, short *coefilt)
{
	long i, k, numsamps, repstart;

	i = isinc*nsamp + ispos; if (i < isinc) return; //ispos <  0 for all samples!
		//Clip off low ispos
	if (ispos < 0) { k = (isinc-1-ispos)/isinc; ivolsc += ivolsci*k; ispos += isinc*k; lptr += k*2; nsamp -= k; }
	numsamps = *(long *)(dasnd-8);
	repstart = *(long *)(dasnd-12);

		//Make sure loop transition is smooth when using (NUMCOEF > 1)
	*(long *)(dasnd+(numsamps<<1)  ) = *(long *)(dasnd+(repstart<<1)  );
	*(long *)(dasnd+(numsamps<<1)+4) = *(long *)(dasnd+(repstart<<1)+4);

	while (i-isinc >= (numsamps<<LSAMPREC))
	{
		k = ((numsamps<<LSAMPREC)-ispos+isinc-1)/isinc;
		rendersamps(dasnd,ispos,isinc,ivolsc,ivolsci,lptr,k,coefilt);
		ivolsc += ivolsci*k; ispos += isinc*k; lptr += k*2; nsamp -= k;
		ispos -= ((numsamps-repstart)<<LSAMPREC); //adjust ispos so last sample equivalent to repstart
		i = isinc*nsamp + ispos;
	}
	rendersamps(dasnd,ispos,isinc,ivolsc,ivolsci,lptr,nsamp,coefilt);

		//Restore loop transition to silence (in case sound is also played without looping)
	*(long *)(dasnd+(numsamps<<1)  ) = 0;
	*(long *)(dasnd+(numsamps<<1)+4) = 0;
}

	//  lptr: 32-bit sound pointer
	//  dptr: 16-bit destination pointer
	// nsamp: number of destination samples to render
static void audclipcopy (long *lptr, short *dptr, long nsamp)
{
	if (cputype&(1<<23)) //MMX
	{
#if 0
		_asm
		{
			mov eax, lptr
			mov edx, dptr
			mov ecx, nsamp
			lea edx, [edx+ecx*4]
			lea eax, [eax+ecx*8]
			neg ecx
  begc0: movq mm0, [eax+ecx*8]
			packssdw mm0, mm0
			movd [edx+ecx*4], mm0
			add ecx, 1
			jnz short begc0
		}
#else
		_asm //Same as above, but does 8-byte aligned writes instead of 4
		{
			mov eax, lptr
			mov edx, dptr
			mov ecx, nsamp
			test edx, 4
			lea edx, [edx+ecx*4]
			lea eax, [eax+ecx*8]
			jz short skipc
			neg ecx
			movq mm0, [eax+ecx*8]
			packssdw mm0, mm0
			movd [edx+ecx*4], mm0
			add ecx, 2
			jg short endc
			jz short skipd
			jmp short begc1
	skipc:neg ecx
			add ecx, 1
	begc1:movq mm0, [eax+ecx*8-8]
			packssdw mm0, [eax+ecx*8]
			movq [edx+ecx*4-4], mm0
			add ecx, 2
			jl short begc1
			jg short endc
	skipd:movq mm0, [eax-8]
			packssdw mm0, mm0
			movd [edx-4], mm0
	endc:
		}
#endif
	}
}

void setears3d (float iposx, float iposy, float iposz,
					 float iforx, float ifory, float iforz,
					 float iheix, float iheiy, float iheiz)
{
	ENTERMUTX;
	float f = 1.f/sqrt(iheix*iheix+iheiy*iheiy+iheiz*iheiz); //Make audiostr same magnitude as audiofor
	audiopos.x = iposx; audiopos.y = iposy; audiopos.z = iposz;
	audiofor.x = iforx; audiofor.y = ifory; audiofor.z = iforz;
	audiohei.x = iheix; audiohei.y = iheiy; audiohei.z = iheiz;
	audiostr.x = (iheiy*iforz - iheiz*ifory)*f;
	audiostr.y = (iheiz*iforx - iheix*iforz)*f;
	audiostr.z = (iheix*ifory - iheiy*iforx)*f;
	LEAVEMUTX;
}

	//Because of 3D position calculations, it is better to render sound in sync with the movement
	//   and not at random times. In other words, call kensoundbreath with lower "minleng" values
	//   when calling from breath() than from other places, such as kensoundthread()
static void kensoundbreath (long minleng)
{
	void *w[2];
	unsigned long l[2], playcurs, writcurs, leng, u;
	long i, j, k, m, n, *lptr[2], nsinc0, nsinc1, volsci0, volsci1;

	if (!streambuf) return;
	streambuf->GetStatus(&u); if (!(u&DSBSTATUS_LOOPING)) return;
	if (streambuf->GetCurrentPosition(&playcurs,&writcurs) != DS_OK) return;
	leng = ((playcurs-oplaycurs)&(SNDSTREAMSIZ-1)); if (leng < minleng) return;
	i = (oplaycurs&(SNDSTREAMSIZ-1));
	if (i < ((playcurs-1)&(SNDSTREAMSIZ-1)))
		memset(&lsnd[i>>1],0,leng<<1);
	else
	{
		memset(&lsnd[i>>1],0,((-oplaycurs)&(SNDSTREAMSIZ-1))<<1);
		memset(lsnd,0,(playcurs&(SNDSTREAMSIZ-1))<<1);
	}

	if (!numrendersnd)
	{
		streambuf->Lock(i,leng,&w[0],&l[0],&w[1],&l[1],0);
		if (w[0]) memset(w[0],(bytespersample-2)&128,l[0]);
		if (w[1]) memset(w[1],(bytespersample-2)&128,l[1]);
		streambuf->Unlock(w[0],l[0],w[1],l[1]);
	}
	else
	{
		streambuf->Lock(i,leng,&w[0],&l[0],&w[1],&l[1],0);

		lptr[0] = &lsnd[i>>1]; lptr[1] = lsnd;
		for(j=numrendersnd-1;j>=0;j--)
		{
			if (rendersnd[j].flags&KSND_MOVE) rendersnd[j].p = *rendersnd[j].ptr; //Get new 3D position
			if (rendersnd[j].flags&KSND_3D)
			{
				n = (signed long)(leng>>gshiftval);

				float f, g, h;
				f = (rendersnd[j].p.x-audiopos.x)*(rendersnd[j].p.x-audiopos.x)+(rendersnd[j].p.y-audiopos.y)*(rendersnd[j].p.y-audiopos.y)+(rendersnd[j].p.z-audiopos.z)*(rendersnd[j].p.z-audiopos.z);
				g = (rendersnd[j].p.x-audiopos.x)*audiostr.x+(rendersnd[j].p.y-audiopos.y)*audiostr.y+(rendersnd[j].p.z-audiopos.z)*audiostr.z;
				if (f <= SNDMINDIST*SNDMINDIST) { f = SNDMINDIST; h = (float)rendersnd[j].ivolsc; } else { f = sqrt(f); h = ((float)rendersnd[j].ivolsc)*SNDMINDIST/f; }
				g /= f;  //g=-1:pure left, g=0:center, g=1:pure right
					//Should use exponential scaling to keep volume constant!
				volsci0 = (long)((1.f-max(g*VOLSEPARATION,0))*h);
				volsci1 = (long)((1.f+min(g*VOLSEPARATION,0))*h);
				volsci0 = (volsci0-rendersnd[j].ivolsc0)/n;
				volsci1 = (volsci1-rendersnd[j].ivolsc1)/n;

					//ispos? = ispos + (f voxels)*(.00025sec/voxel) * (isinc*samplerate subsamples/sec);
				m = rendersnd[j].isinc*samplerate;
				k = rendersnd[j].isinc*n + rendersnd[j].ispos;
				h = max((f-SNDMINDIST)+g*(EARSEPARATION*.5f),0); nsinc0 = k - mulshr16((long)(h*(65536.f/SNDSPEED)),m);
				h = max((f-SNDMINDIST)-g*(EARSEPARATION*.5f),0); nsinc1 = k - mulshr16((long)(h*(65536.f/SNDSPEED)),m);
				nsinc0 = (nsinc0-rendersnd[j].ispos0)/n;
				nsinc1 = (nsinc1-rendersnd[j].ispos1)/n;
			}
			else
			{
				nsinc0 = rendersnd[j].isinc;
				nsinc1 = rendersnd[j].isinc;
				volsci0 = 0;
				volsci1 = 0;
			}
			if (rendersnd[j].flags&KSND_LOOPFADE)
			{
				n = -(signed long)(leng>>gshiftval);
				volsci0 = rendersnd[j].ivolsc0/n;
				volsci1 = rendersnd[j].ivolsc1/n;
			}
			for(m=0;m<2;m++)
				if (w[m])
				{
					k = (l[m]>>gshiftval);
					if (!(rendersnd[j].flags&KSND_LOOP))
					{
						rendersamps(rendersnd[j].ssnd,rendersnd[j].ispos0,nsinc0,rendersnd[j].ivolsc0,volsci0,lptr[m]  ,k,rendersnd[j].coefilt);
						rendersamps(rendersnd[j].ssnd,rendersnd[j].ispos1,nsinc1,rendersnd[j].ivolsc1,volsci1,lptr[m]+1,k,rendersnd[j].coefilt);
						rendersnd[j].ispos += rendersnd[j].isinc*k;
						rendersnd[j].ispos0 += nsinc0*k; rendersnd[j].ivolsc0 += volsci0*k;
						rendersnd[j].ispos1 += nsinc1*k; rendersnd[j].ivolsc1 += volsci1*k;

							//Delete sound only when both L&R channels have played through all their samples
						if (((rendersnd[j].ispos0>>LSAMPREC) >= (*(long *)(rendersnd[j].ssnd-8))) &&
							 ((rendersnd[j].ispos1>>LSAMPREC) >= (*(long *)(rendersnd[j].ssnd-8))))
						{
							(*(long *)(rendersnd[j].ssnd-16))--; numrendersnd--;
							if (j != numrendersnd) rendersnd[j] = rendersnd[numrendersnd];
							break;
						}
					}
					else
					{
						long numsamps, repleng;
						numsamps = ((*(long *)(rendersnd[j].ssnd-8))<<LSAMPREC);
						repleng = numsamps-((*(long *)(rendersnd[j].ssnd-12))<<LSAMPREC);

						for(n=rendersnd[j].ispos0;n>=numsamps;n-=repleng);
						rendersampsloop(rendersnd[j].ssnd,n,nsinc0,rendersnd[j].ivolsc0,volsci0,lptr[m]  ,k,rendersnd[j].coefilt);
						for(n=rendersnd[j].ispos1;n>=numsamps;n-=repleng);
						rendersampsloop(rendersnd[j].ssnd,n,nsinc1,rendersnd[j].ivolsc1,volsci1,lptr[m]+1,k,rendersnd[j].coefilt);
						rendersnd[j].ispos += rendersnd[j].isinc*k;
						rendersnd[j].ispos0 += nsinc0*k;
						rendersnd[j].ispos1 += nsinc1*k;
						rendersnd[j].ivolsc0 += volsci0*k;
						rendersnd[j].ivolsc1 += volsci1*k;

							//Hack to keep sample pointers away from the limit...
						while ((rendersnd[j].ispos0 >= numsamps) && (rendersnd[j].ispos1 >= numsamps))
						{
							rendersnd[j].ispos -= repleng;
							rendersnd[j].ispos0 -= repleng; rendersnd[j].ispos1 -= repleng;
						}
					}
				}
			if ((m >= 2) && (rendersnd[j].flags&KSND_LOOPFADE)) //Remove looping sound after fade-out
			{
				(*(long *)(rendersnd[j].ssnd-16))--; numrendersnd--;
				if (j != numrendersnd) rendersnd[j] = rendersnd[numrendersnd];
			}
			if (cputype&(1<<23)) _asm emms //MMX
		}
		for(m=0;m<2;m++) if (w[m]) audclipcopy(lptr[m],(short *)w[m],l[m]>>gshiftval);
		if (cputype&(1<<23)) _asm emms //MMX
		streambuf->Unlock(w[0],l[0],w[1],l[1]);
	}

	oplaycurs = playcurs;
}

#if (USETHREADS != 0)
static void kensoundthread (void *_)
{
	while (!quitprogram)
	{
		ENTERMUTX;
		kensoundbreath(SNDSTREAMSIZ>>1); //WARNING: Don't call breath() here - WaitForSingleObject will fail :/
		LEAVEMUTX;
		Sleep(USETHREADS);
	}
	quitprogram = 2;
}
#endif

	//Returns pointer to sound data; loads file if not already loaded.
long audgetfilebufptr (const char *filnam)
{
	WAVEFORMATEX wft;
	long i, j, leng, hashind, newsnd, *lptr, repstart;
	char tempbuf[12], *fptr;
#ifndef USEKZ
	FILE *fil;
#endif

	if (audhashbuf)
	{
		for(i=audhashead[audcalchash(filnam)];i>=0;i=(*(long *)&audhashbuf[i]))
			if (!filnamcmp(filnam,&audhashbuf[i+12])) return(*(long *)&audhashbuf[i+8]);
	}

		//Load WAV file here!
	repstart = 0x80000000;
	if (filnam[0] == '<') //Sound is in memory! (KSND_MEM flag)
	{
		unsigned long u; //Filename is in weird hex format
		for(i=1,u=0;i<9;i++) u = (u<<4)+(filnam[i]&15);
		fptr = (char *)u;

		memcpy(tempbuf,fptr,12); fptr += 12;
		if (*(long *)&tempbuf[0] != 0x46464952) return(0); //RIFF
		if (*(long *)&tempbuf[8] != 0x45564157) return(0); //WAVE
		for(j=16;j;j--)
		{
			memcpy(tempbuf,fptr,8); fptr += 8; i = *(long *)&tempbuf[0]; leng = *(long *)&tempbuf[4];
			if (i == 0x61746164) break; //data
			if (i == 0x20746d66) //fmt
			{
				memcpy(&wft,fptr,16); fptr += 16;
				if ((wft.wFormatTag != WAVE_FORMAT_PCM) || (wft.nChannels != 1)) return(0);
				if (leng == 20) { memcpy(&repstart,fptr,4); fptr += 4; }
				else if (leng > 16) fptr += ((leng-16+1)&~1);
				continue;
			}
			fptr += ((leng+1)&~1);
			//if (fptr > ?eof?) return(0); //corrupt WAV files in memory aren't detected :/
		}
		if ((!j) || (!leng)) return(0);
	}
	else
	{
#ifndef USEKZ
		if (!(fil = fopen(filnam,"rb"))) return(0);
		fread(tempbuf,12,1,fil);
		if (*(long *)&tempbuf[0] != 0x46464952) { fclose(fil); return(0); } //RIFF
		if (*(long *)&tempbuf[8] != 0x45564157) { fclose(fil); return(0); } //WAVE
		for(j=16;j;j--)
		{
			fread(&tempbuf,8,1,fil); i = *(long *)&tempbuf[0]; leng = *(long *)&tempbuf[4];
			if (i == 0x61746164) break; //data
			if (i == 0x20746d66) //fmt
			{
				fread(&wft,16,1,fil);
				if ((wft.wFormatTag != WAVE_FORMAT_PCM) || (wft.nChannels != 1)) { fclose(fil); return(0); }
				if (leng == 20) fread(&repstart,4,1,fil);
				else if (leng > 16) fseek(fil,(leng-16+1)&~1,SEEK_CUR);
				continue;
			}
			fseek(fil,(leng+1)&~1,SEEK_CUR);
			if (feof(fil)) { fclose(fil); return(0); }
		}
		if ((!j) || (!leng)) { fclose(fil); return(0); }
#else
		if (!kzopen(filnam)) return(0);
		kzread(tempbuf,12);
		if (*(long *)&tempbuf[0] != 0x46464952) { kzclose(); return(0); } //RIFF
		if (*(long *)&tempbuf[8] != 0x45564157) { kzclose(); return(0); } //WAVE
		for(j=16;j;j--)
		{
			kzread(tempbuf,8); i = *(long *)&tempbuf[0]; leng = *(long *)&tempbuf[4];
			if (i == 0x61746164) break; //data
			if (i == 0x20746d66) //fmt
			{
				kzread(&wft,16);
				if ((wft.wFormatTag != WAVE_FORMAT_PCM) || (wft.nChannels != 1)) { kzclose(); return(0); }
				if (leng == 20) kzread(&repstart,4);
				else if (leng > 16) kzseek((leng-16+1)&~1,SEEK_CUR);
				continue;
			}
			kzseek((leng+1)&~1,SEEK_CUR);
			if (kzeof()) { kzclose(); return(0); }
		}
		if ((!j) || (!leng)) { kzclose(); return(0); }
#endif
	}

	if (wft.wBitsPerSample == 16) i = leng; else i = (leng<<1); //Convert 8-bit to 16-bit

	if (filnam[0] == '<') //Sound is in memory! (KSND_MEM flag)
	{
		if (!(newsnd = (long)malloc(16+i+8))) return(0);
		memcpy((void *)(newsnd+16),fptr,leng);
	}
	else
	{
#ifndef USEKZ
		if (!(newsnd = (long)malloc(16+i+8))) { fclose(fil); return(0); }
		fread((void *)(newsnd+16),leng,1,fil);
		fclose(fil);
#else
		if (!(newsnd = (long)malloc(16+i+8))) { kzclose(); return(0); }
		kzread((void *)(newsnd+16),leng);
		kzclose();
#endif
	}

	if (wft.wBitsPerSample == 8) //Convert 8-bit to 16-bit
	{
		j = newsnd+16;
		for(i=leng-1;i>=0;i--) (*(short *)((i<<1)+j)) = (((short)((*(signed char *)(i+j))-128))<<8);
		wft.wBitsPerSample = 16; leng <<= 1;
	}
	*(long *)(newsnd) = 0; //Allocation count
	*(long *)(newsnd+4) = repstart; //Loop repeat start sample
	*(long *)(newsnd+8) = (leng>>1); // /((wft.wBitsPerSample>>3)*wft.nChannels); //numsamples
	*(float *)(newsnd+12) = (float)(wft.nSamplesPerSec<<LSAMPREC);
	*(long *)(newsnd+leng+16) = *(long *)(newsnd+leng+20) = 0;

		//Write new file info to hash
	j = 12+strlen(filnam)+1; if (!audcheckhashsiz(j)) return(0);
	hashind = audcalchash(filnam);
	*(long *)&audhashbuf[audhashpos] = audhashead[hashind];
	*(long *)&audhashbuf[audhashpos+4] = audlastloadedwav;
	*(long *)&audhashbuf[audhashpos+8] = newsnd;
	strcpy(&audhashbuf[audhashpos+12],filnam);
	audhashead[hashind] = audhashpos; audlastloadedwav = audhashpos; audhashpos += j;

	return(newsnd);
}

	// filnam: ASCIIZ string of filename (can be inside .ZIP file if USEKZ is enabled)
	//         Filenames are compared with a hash, and samples are cached, so don't worry about speed!
	//volperc: volume scale (0 is silence, 100 is max volume)
	// frqmul: frequency multiplier (1.0 is no scale)
	//    pos: if 0, then doesn't use 3D, if nonzero, this is pointer to 3D position
	//  flags: bit 0: 1=3D sound, 0=simple sound
	//         bit 1: 1=dynamic 3D position update (from given pointer), 0=disable (ignored if !pos)
	//         bit 2: 1=loop sound (use playsoundupdate to disable, for non 3D sounds use dummy point3d!)
	//            Valid values for flags:
	//            flags=0: 0           flags=4: KSND_LOOP
	//            flags=1: KSND_3D     flags=5: KSND_LOOP|KSND_3D
	//            flags=3: KSND_MOVE   flags=7: KSND_LOOP|KSND_MOVE
	//
	//         NOTE: When using flags=4 (KSND_LOOP without KSND_3D or KSND_MOVE), you can pass a unique
	//               non-zero dummy pointer pos to playsound. This way, you can use playsoundupdate() to stop
	//               the individual sound. If you pass a NULL pointer, then only way to stop the sound is
	//               by stopping all sounds.
void playsound (const char *filnam, long volperc, float frqmul, void *pos, long flags)
{
	void *w[2];
	unsigned long l[2], playcurs, writcurs, u;
	long i, j, k, m, ispos, ispos0, ispos1, isinc, ivolsc, ivolsc0, ivolsc1, newsnd, *lptr[2], numsamps;
	short *coefilt;
	float f, g, h;
	
	if (!dsound) return;
	ENTERMUTX;
	if (!streambuf) { LEAVEMUTX; return; }
	streambuf->GetStatus(&u); if (!(u&DSBSTATUS_LOOPING)) { LEAVEMUTX; return; }

	if (flags&KSND_MEM)
	{
		char tempbuf[10]; //Convert to ASCII string so filnamcmp()&audcalchash() works right
		tempbuf[9] = 0; u = (unsigned long)filnam;
		for(i=8;i>0;i--) { tempbuf[i] = (u&15)+64; u >>= 4; }
		tempbuf[0] = '<'; //This invalid filename char tells audgetfilebufptr it's KSND_MEM
		if (!(newsnd = audgetfilebufptr(tempbuf))) { LEAVEMUTX; return; }
	}
	else
	{
		if (!(newsnd = audgetfilebufptr(filnam))) { LEAVEMUTX; return; }
	}
	newsnd += 16;

	ispos = 0; isinc = (long)((*(float *)(newsnd-4))*frqmul*rsamplerate);
	ivolsc = (volperc<<15)/100; if (ivolsc >= 32768) ivolsc = 32767; ivolsc <<= 16;

	if (!pos) flags &= ~(KSND_3D|KSND_MOVE); //null pointers not allowed for 3D sound
	if (flags&KSND_MOVE) flags |= KSND_3D; //moving sound must be 3D sound
	if ((flags&KSND_LOOP) && ((*(long *)(newsnd-12)) == 0x80000000))
		flags &= ~KSND_LOOP; //WAV must support looping to allow looping

	if (!(flags&KSND_LOPASS)) coefilt = coef; else coefilt = coeflopass;

	if (flags&KSND_3D)
	{
		f = (((point3d *)pos)->x-audiopos.x)*(((point3d *)pos)->x-audiopos.x)+(((point3d *)pos)->y-audiopos.y)*(((point3d *)pos)->y-audiopos.y)+(((point3d *)pos)->z-audiopos.z)*(((point3d *)pos)->z-audiopos.z);
		g = (((point3d *)pos)->x-audiopos.x)*audiostr.x+(((point3d *)pos)->y-audiopos.y)*audiostr.y+(((point3d *)pos)->z-audiopos.z)*audiostr.z;
		if (f <= SNDMINDIST*SNDMINDIST) { f = SNDMINDIST; h = (float)ivolsc; } else { f = sqrt(f); h = ((float)ivolsc)*SNDMINDIST/f; }
		g /= f;  //g=-1:pure left, g=0:center, g=1:pure right
			//Should use exponential scaling to keep volume constant!
		ivolsc0 = (long)((1.f-max(g*VOLSEPARATION,0))*h);
		ivolsc1 = (long)((1.f+min(g*VOLSEPARATION,0))*h);
			//ispos? = ispos + (f voxels)*(.00025sec/voxel) * (isinc*samplerate subsamples/sec);
		m = isinc*samplerate;
		h = max((f-SNDMINDIST)+g*(EARSEPARATION*.5f),0); ispos0 = -mulshr16((long)(h*(65536.f/SNDSPEED)),m);
		h = max((f-SNDMINDIST)-g*(EARSEPARATION*.5f),0); ispos1 = -mulshr16((long)(h*(65536.f/SNDSPEED)),m);
	}
	else { ispos0 = ispos1 = 0; ivolsc0 = ivolsc1 = ivolsc; }

	kensoundbreath(SNDSTREAMSIZ>>1); //Not necessary, but for good luck
	if (streambuf->GetCurrentPosition(&playcurs,&writcurs) != DS_OK) { LEAVEMUTX; return; }
		//If you use playcurs instead of writcurs, beginning of sound gets cut off :/
		//on WinXP: ((writcurs-playcurs)&(SNDSTREAMSIZ-1)) ranges from 6880 to 8820 (step 4)
	i = (writcurs&(SNDSTREAMSIZ-1));
	if (streambuf->Lock(i,(oplaycurs-i)&(SNDSTREAMSIZ-1),&w[0],&l[0],&w[1],&l[1],0) != DS_OK) { LEAVEMUTX; return; }
	lptr[0] = &lsnd[i>>1]; lptr[1] = lsnd;
	for(m=0;m<2;m++)
		if (w[m])
		{
			j = (l[m]>>gshiftval);
			if (!(flags&KSND_LOOP))
			{
				rendersamps(newsnd,ispos0,isinc,ivolsc0,0,lptr[m]  ,j,coefilt);
				rendersamps(newsnd,ispos1,isinc,ivolsc1,0,lptr[m]+1,j,coefilt);
				j *= isinc; ispos += j; ispos0 += j; ispos1 += j;
			}
			else
			{
				long numsamps, repleng, n;
				numsamps = ((*(long *)(newsnd-8))<<LSAMPREC);
				repleng = numsamps-((*(long *)(newsnd-12))<<LSAMPREC);

				for(n=ispos0;n>=numsamps;n-=repleng);
				rendersampsloop(newsnd,n,isinc,ivolsc0,0,lptr[m]  ,j,coefilt);
				for(n=ispos1;n>=numsamps;n-=repleng);
				rendersampsloop(newsnd,n,isinc,ivolsc1,0,lptr[m]+1,j,coefilt);
				j *= isinc; ispos += j; ispos0 += j; ispos1 += j;

					//Hack to keep sample pointers away from the limit...
				while ((ispos0 >= numsamps) && (ispos1 >= numsamps))
					{ ispos -= repleng; ispos0 -= repleng; ispos1 -= repleng; }
			}
		}
	for(m=0;m<2;m++) if (w[m]) audclipcopy(lptr[m],(short *)w[m],l[m]>>gshiftval);
	if (cputype&(1<<23)) _asm emms //MMX
	streambuf->Unlock(w[0],l[0],w[1],l[1]);

		//Save params to continue playing later (when both L&R channels haven't played through)
	numsamps = *(long *)(newsnd-8);
	if ((((ispos0>>LSAMPREC) < numsamps) || ((ispos1>>LSAMPREC) < numsamps) || (flags&KSND_LOOP)) && (numrendersnd < MAXPLAYINGSNDS))
	{
		rendersnd[numrendersnd].ptr = (point3d *)pos;
		if (flags&KSND_3D) rendersnd[numrendersnd].p = *((point3d *)pos);
		else { rendersnd[numrendersnd].p.x = rendersnd[numrendersnd].p.y = rendersnd[numrendersnd].p.z = 0; }
		rendersnd[numrendersnd].flags = flags;
		rendersnd[numrendersnd].ssnd = newsnd;
		rendersnd[numrendersnd].ispos = ispos;
		rendersnd[numrendersnd].ispos0 = ispos0;
		rendersnd[numrendersnd].ispos1 = ispos1;
		rendersnd[numrendersnd].isinc = isinc;
		rendersnd[numrendersnd].ivolsc = ivolsc;
		rendersnd[numrendersnd].ivolsc0 = ivolsc0;
		rendersnd[numrendersnd].ivolsc1 = ivolsc1;
		rendersnd[numrendersnd].coefilt = coefilt;
		numrendersnd++;
		(*(long *)(newsnd-16))++;
	}

	LEAVEMUTX;
}

	//Use this function to update a pointer location (if you need to move things around in memory)
	//special cases: if (optr== 0) changes all pointers
	//               if (nptr== 0) changes KSND_MOVE to KSND_3D
	//               if (nptr==-1) changes KSND_MOVE to KSND_3D and stops sound (using KSND_LOOPFADE)
	//examples: playsoundupdate(&spr[delete_me].p,&spr[--numspr].p); //update pointer location
	//          playsoundupdate(&spr[just_before_respawn],0);        //stop position updates from pointer
	//          playsoundupdate(&my_looping_sound,(point3d *)-1);    //turn off a looping sound
	//          playsoundupdate(0,0);                                //stop all position updates
	//          playsoundupdate(0,(point3d *)-1);                    //turn off all sounds
void playsoundupdate (void *optr, void *nptr)
{
	long i;

	if (!dsound) return;
	ENTERMUTX;
	for(i=numrendersnd-1;i>=0;i--)
	{
		if ((rendersnd[i].ptr != (point3d *)optr) && (optr)) continue;
		if (((long)nptr) == -2) { rendersnd[i].flags |= KSND_LOPASS; rendersnd[i].coefilt = coeflopass; continue; }
		if (((long)nptr) == -3) { rendersnd[i].flags &= ~KSND_LOPASS; rendersnd[i].coefilt = coef; continue; }
		rendersnd[i].ptr = (point3d *)nptr;
		if (!((((long)nptr)+1)&~1))
		{
			rendersnd[i].flags &= ~KSND_MOVE;              //nptr == {0,-1}
			if (nptr) rendersnd[i].flags |= KSND_LOOPFADE; //nptr == {-1}
		}
	}
	LEAVEMUTX;
}

#endif
// Kensound code ends ---------------------------------------------------------

// DirectSound variables & code ----------------------------------------------

#ifdef DSOUNDINITCOM
static HRESULT coinit;
#endif

#define MAXSOUNDS 256
LPDIRECTSOUNDBUFFER dsprim = 0;
#if (USEKENSOUND == 2)
LPDIRECTSOUNDBUFFER dsbuf[MAXSOUNDS];
LPDIRECTSOUND3DBUFFER ds3dbuf[MAXSOUNDS];
LPDIRECTSOUND3DLISTENER ds3dlis = 0;
static long sndindex = -1;
#endif
static long globvolume = 100;

	//0=-10000,1-100=CINT(log10(z/100)*1000)
signed short volperc2db100[104] =
{
  -10000,-2000,-1699,-1523,-1398,-1301,-1222,-1155,-1097,-1046, // 0 to  9
	-1000, -959, -921, -886, -854, -824, -796, -770, -745, -721, //10 to 19
	 -699, -678, -658, -638, -620, -602, -585, -569, -553, -538, //20 to 29
	 -523, -509, -495, -481, -469, -456, -444, -432, -420, -409, //30 to 39
	 -398, -387, -377, -367, -357, -347, -337, -328, -319, -310, //40 to 49
	 -301, -292, -284, -276, -268, -260, -252, -244, -237, -229, //50 to 59
	 -222, -215, -208, -201, -194, -187, -180, -174, -167, -161, //60 to 69
	 -155, -149, -143, -137, -131, -125, -119, -114, -108, -102, //70 to 79
	  -97,  -92,  -86,  -81,  -76,  -71,  -66,  -60,  -56,  -51, //80 to 89
	  -46,  -41,  -36,  -32,  -27,  -22,  -18,  -13,   -9,   -4, //90 to 99
		 0,                                                       //100
};

void setvolume (long percentmax)
{
	if (!dsprim) return;
	globvolume = min(max(percentmax,0),100);
	dsprim->SetVolume(volperc2db100[globvolume]);
}

void uninitdirectsound ()
{
	long i;

	if (!dsound) return;
#if (USEKENSOUND == 1)
	kensoundclose();
#endif

#if (USEKENSOUND == 2)
	for(i=MAXSOUNDS-1;i>=0;i--)
		if (ds3dbuf[i]) { ds3dbuf[i]->Release(); ds3dbuf[i] = 0; }
	if (ds3dlis) { ds3dlis->Release(); ds3dlis = 0; }
	for(i=MAXSOUNDS-1;i>=0;i--)
		if (dsbuf[i]) { dsbuf[i]->Release(); dsbuf[i] = 0; }
#endif
	dsound->Release(); dsound = 0;
}

void initdirectsound ()
{
	DSBUFFERDESC dsbdesc;
	long i;

	if (dsound) uninitdirectsound();

#if (USEKENSOUND == 2)
	for(i=0;i<MAXSOUNDS;i++) { dsbuf[i] = 0; ds3dbuf[i] = 0; }
#endif

#ifdef DSOUNDINITCOM
	if (coinit == S_OK)
	{
		HRESULT dsrval = CoCreateInstance(CLSID_DirectSound,NULL,CLSCTX_INPROC_SERVER,
													 IID_IDirectSound,(void **)&dsound);
		if (dsrval == S_OK) {
			dsrval = (dsound)->Initialize(NULL);
			//dsrval = IDirectSound_Initialize(dsound,NULL);
			//(dsound)->Release(lpds);
			if (dsrval != S_OK) { dsound = NULL; return; }
		}
	}   
#else
	if (DirectSoundCreate(0,&dsound,0) != DS_OK) { MessageBox(ghwnd,"DirectSoundCreate","ERROR",MB_OK); exit(0); }
#endif
	if (dsound->SetCooperativeLevel(ghwnd,DSSCL_PRIORITY) != DS_OK) { MessageBox(ghwnd,"SetCooperativeLevel","ERROR",MB_OK); exit(0); }

		//Create primary buffer
	dsbdesc.dwSize = sizeof(DSBUFFERDESC);
	dsbdesc.dwFlags = DSBCAPS_PRIMARYBUFFER;
#if (USEKENSOUND == 2)
	dsbdesc.dwFlags |= DSBCAPS_CTRL3D|DSBCAPS_CTRLVOLUME;
#endif
	dsbdesc.dwBufferBytes = 0;  //0 for primary
	dsbdesc.dwReserved = 0;
	dsbdesc.lpwfxFormat = 0;    //0 for primary
	if (dsound->CreateSoundBuffer(&dsbdesc,&dsprim,0) < 0)
		{ dsound->Release(); MessageBox(ghwnd,"CreateSoundBuffer (primary) failed","ERROR",MB_OK); exit(0); }

	dsprim->Play(0,0,DSBPLAY_LOOPING);  //Force mixer to always be on

#if (USEKENSOUND == 2)
		//Default listener orientation:
		//   <1,0,0>: right
		//   <0,1,0>: up
		//   <0,0,1>: front
		//Initialize 3D sound
	if (dsprim->QueryInterface(IID_IDirectSound3DListener,(void **)&ds3dlis) != S_OK)
		{ dsound->Release(); MessageBox(ghwnd,"Query...Listener failed","ERROR",MB_OK); exit(0); }
	ds3dlis->SetDistanceFactor(.03,DS3D_DEFERRED);      //1 (meters/unit)
	ds3dlis->SetRolloffFactor(.03,DS3D_DEFERRED);       //1
	ds3dlis->SetDopplerFactor(1,DS3D_DEFERRED);         //1
	ds3dlis->SetPosition(0,0,0,DS3D_DEFERRED);          //0,0,0
	ds3dlis->SetVelocity(0,0,0,DS3D_DEFERRED);          //0,0,0
	ds3dlis->SetOrientation(0,0,1,0,1,0,DS3D_DEFERRED); //0,0,1,0,1,0
	ds3dlis->CommitDeferredSettings();
#endif

	if (globvolume != 100) dsprim->SetVolume(globvolume);

#if (USEKENSOUND == 1)
	kensoundinit(dsound,44100,2,2); //last line
#endif
}

#if (USEKENSOUND == 2)

#define MINSNDLENG 4096 //is 1764 min size? too bad it fails sometimes :/ 1764*25 = 44100
long loadwav (LPDIRECTSOUNDBUFFER *dabuf, const char *fnam, float freqmul, unsigned long daflags)
{
	WAVEFORMATEX wft;
	DSBUFFERDESC bufdesc;
	void *w0, *w1;
	unsigned long l0, l1, leng;
	char tempbuf[12];

#ifndef USEKZ
	FILE *fil;
	if (!(fil = fopen(fnam,"rb"))) return(0);
	fread(tempbuf,12,1,fil);
	if (*(long *)&tempbuf[0] != 0x46464952) { fclose(fil); return(0); } //RIFF
	if (*(long *)&tempbuf[8] != 0x45564157) { fclose(fil); return(0); } //WAVE
	for(l1=16;l1;l1--)
	{
		fread(&tempbuf,8,1,fil); l0 = *(long *)&tempbuf[0]; leng = *(long *)&tempbuf[4];
		if (l0 == 0x61746164) break; //data
		if (l0 == 0x20746d66) //fmt
		{
			fread(&wft,16,1,fil);
			//if ((wft.wFormatTag != WAVE_FORMAT_PCM) || (wft.nChannels != 1)) { fclose(fil); return(0); }
			if (leng > 16) fseek(fil,(leng-16+1)&~1,SEEK_CUR);
			continue;
		}
		fseek(fil,(leng+1)&~1,SEEK_CUR);
		if (feof(fil)) { fclose(fil); return(0); }
	}
	if (!l1) { fclose(fil); return(0); }
#else
	if (!kzopen(fnam)) return(0);
	kzread(tempbuf,12);
	if (*(long *)&tempbuf[0] != 0x46464952) { kzclose(); return(0); } //RIFF
	if (*(long *)&tempbuf[8] != 0x45564157) { kzclose(); return(0); } //WAVE
	for(l1=16;l1;l1--)
	{
		kzread(tempbuf,8); l0 = *(long *)&tempbuf[0]; leng = *(long *)&tempbuf[4];
		if (l0 == 0x61746164) break; //data
		if (l0 == 0x20746d66) //fmt
		{
			kzread(&wft,16);
			//if ((wft.wFormatTag != WAVE_FORMAT_PCM) || (wft.nChannels != 1)) { kzclose(); return(0); }
			if (leng > 16) kzseek((leng-16+1)&~1,SEEK_CUR);
			continue;
		}
		kzseek((leng+1)&~1,SEEK_CUR);
		if (kzeof()) { kzclose(); return(0); }
	}
	if (!l1) { kzclose(); return(0); }
#endif

	if (*(long *)&freqmul != 0x3f800000)
	{
		wft.nSamplesPerSec = (long)((float)wft.nSamplesPerSec * freqmul);
		wft.nAvgBytesPerSec = wft.nSamplesPerSec * wft.nBlockAlign;
	}
	wft.cbSize = 0;

	bufdesc.dwSize = sizeof(DSBUFFERDESC);
	bufdesc.dwFlags = daflags;
	bufdesc.dwBufferBytes = max(leng,MINSNDLENG);
	bufdesc.dwReserved = 0;
	bufdesc.lpwfxFormat = &wft;
	//bufdesc.guid3DAlgorithm = GUID_NULL;
#ifndef USEKZ
	if (dsound->CreateSoundBuffer(&bufdesc,dabuf,0) != DS_OK)
		{ fclose(fil); return(0); }
		//write wave data to directsound buffer you just created
	if (((*dabuf)->Lock(0,leng,&w0,&l0,&w1,&l1,0)) == DSERR_BUFFERLOST) { fclose(fil); return(0); }
	if (w0) fread(w0,l0,1,fil);
	if (w1) fread(w1,l1,1,fil);
	(*dabuf)->Unlock(w0,l0,w1,l1);
	fclose(fil);
#else
	if (dsound->CreateSoundBuffer(&bufdesc,dabuf,0) != DS_OK)
		{ kzclose(); return(0); }
		//write wave data to directsound buffer you just created
	(*dabuf)->Lock(0,leng,&w0,&l0,&w1,&l1,0);
	if (w0) kzread(w0,l0);
	if (w1) kzread(w1,l1);
	(*dabuf)->Unlock(w0,l0,w1,l1);
	kzclose();
#endif

		//This hack is because DSOUND seems to have problems with very short secondary buffers!
	if (leng < MINSNDLENG)
	{
		if (((*dabuf)->Lock(leng,MINSNDLENG-leng,&w0,&l0,&w1,&l1,0)) == DSERR_BUFFERLOST) return(0);
		if (w0) memset(w0,(wft.wBitsPerSample==8)<<7,l0);
		if (w1) memset(w1,(wft.wBitsPerSample==8)<<7,l1);
		(*dabuf)->Unlock(w0,l0,w1,l1);
	}

	return(1);
}

void setears3d (float iposx, float iposy, float iposz,
					 float iforx, float ifory, float iforz,
					 float iheix, float iheiy, float iheiz)
{
	if (!ds3dlis) return;
	ds3dlis->SetPosition(iposx,iposy,iposz,DS3D_DEFERRED);
	ds3dlis->SetOrientation(iforx,ifory,iforz,iheix,iheiy,iheiz,DS3D_DEFERRED); //0,0,1,0,1,0
	ds3dlis->CommitDeferredSettings();
}

long reallyuglyandslowhack4sound ()
{
	long i;
	unsigned long u;

	sndindex++;

	i = (sndindex&(MAXSOUNDS-1));
	if (ds3dbuf[i]) { ds3dbuf[i]->Release(); ds3dbuf[i] = 0; }
	if (dsbuf[i]) { dsbuf[i]->Release(); dsbuf[i] = 0; }

	for(i=0;i<MAXSOUNDS;i++)
	{
		if (!dsbuf[i]) continue;
		dsbuf[i]->GetStatus(&u);
		if (!(u&DSBSTATUS_PLAYING))
		{
			if (ds3dbuf[i]) { ds3dbuf[i]->Release(); ds3dbuf[i] = 0; }
			if (dsbuf[i]) { dsbuf[i]->Release(); dsbuf[i] = 0; }
		}
	}
	return(sndindex&(MAXSOUNDS-1));
}

void playsound (const char *filnam, long volperc, float freqmul)
{
	unsigned long u, cnt;
	long i;

	if (!dsound) return;
	i = reallyuglyandslowhack4sound();

	if ((!dsound) || ((unsigned long)i >= MAXSOUNDS)) return;
	for(cnt=2;cnt;cnt--)
	{
		if (!dsbuf[i]) if (!loadwav(&dsbuf[i],filnam,freqmul,(((volperc-100)>>31)&DSBCAPS_CTRLVOLUME)|DSBCAPS_STATIC)) return;
		dsbuf[i]->GetStatus(&u); if (u&DSBSTATUS_PLAYING) return;
		if (volperc < 100) dsbuf[i]->SetVolume(volperc2db100[max(volperc,0)]);
		dsbuf[i]->SetCurrentPosition(0);
		if (dsbuf[i]->Play(0,0,0) != DSERR_BUFFERLOST) return;
		dsbuf[i]->Release(); dsbuf[i] = 0;
	}
}

void playsound (const char *filnam, long volperc, float freqmul, void *pos, long flags)
{
	unsigned long u, cnt;
	long i;

	if (!dsound) return;
	if (!(flags&1)) { playsound(filnam,volperc,freqmul); return; }

	i = reallyuglyandslowhack4sound();

	if ((!dsound) || ((unsigned long)i >= MAXSOUNDS)) return;
	for(cnt=2;cnt;cnt--)
	{
		if (!dsbuf[i])
		{
			if (!loadwav(&dsbuf[i],filnam,freqmul,(((volperc-100)>>31)&DSBCAPS_CTRLVOLUME)|
				DSBCAPS_CTRL3D|DSBCAPS_STATIC|DSBCAPS_MUTE3DATMAXDISTANCE)) return;
			if (ds3dbuf[i]) { ds3dbuf[i]->Release(); ds3dbuf[i] = 0; }
			dsbuf[i]->QueryInterface(IID_IDirectSound3DBuffer,(void **)&ds3dbuf[i]);
			ds3dbuf[i]->SetMode(DS3DMODE_NORMAL,DS3D_DEFERRED);    //DS3DMODE_NORMAL
			//ds3dbuf[i]->SetMaxDistance(1000000000,DS3D_DEFERRED);  //1000000000
			//ds3dbuf[i]->SetMinDistance(1,DS3D_DEFERRED);           //1
			ds3dbuf[i]->SetPosition(((point3d *)pos)->x,((point3d *)pos)->y,((point3d *)pos)->z,DS3D_DEFERRED); //0,0,0
			//ds3dbuf[i]->SetVelocity(0,0,0,DS3D_DEFERRED);          //0,0,0
			//ds3dbuf[i]->SetConeAngles(0,360,DS3D_DEFERRED);        //0,360
			//ds3dbuf[i]->SetConeOrientation(0,0,0,DS3D_DEFERRED);   //0,0,0
			//ds3dbuf[i]->SetConeOutsideVolume(DSBVOLUME_MAX,DS3D_DEFERRED); //DSBVOLUME_MAX
			ds3dlis->CommitDeferredSettings();
		}
		else
		{
			ds3dbuf[i]->SetPosition(((point3d *)pos)->x,((point3d *)pos)->y,((point3d *)pos)->z,DS3D_IMMEDIATE);
		}
		dsbuf[i]->GetStatus(&u); if (u&DSBSTATUS_PLAYING) return;
		if (volperc < 100) dsbuf[i]->SetVolume(volperc2db100[max(volperc,0)]);
		dsbuf[i]->SetCurrentPosition(0);
		if (dsbuf[i]->Play(0,0,0) != DSERR_BUFFERLOST) return;
		if (ds3dbuf[i]) { ds3dbuf[i]->Release(); ds3dbuf[i] = 0; }
		if (dsbuf[i]) { dsbuf[i]->Release(); dsbuf[i] = 0; }
	}
}

void playsoundupdate (void *oposptr, void *nposptr)
{
	//if (!nptr) follow = 0
}

#endif
#endif

//Quitting routines ----------------------------------------------------------

void quitloop () { PostMessage(ghwnd,WM_CLOSE,0,0); }

void evilquit (const char *str) //Evil because this function makes awful assumptions!!!
{
#ifndef NODRAW
	stopdirectdraw();
	ddflip2gdi();
#endif
	if (str) MessageBox(ghwnd,str,"fatal error!",MB_OK);
#ifndef NOINPUT
	uninitmouse();
	uninitkeyboard();
	uninitdirectinput();
#endif
#ifndef NOSOUND
	uninitdirectsound();
#endif
#ifndef NODRAW
	uninitdirectdraw();
#endif
	uninitapp();
	ExitProcess(0);
}

//GENERAL WINDOWS CODE-------------------------------------------------------------------

void setalwaysactive (long active) { alwaysactive = active; }

#ifndef NODRAW
long canrender () { return(ActiveApp); }
#endif

#ifndef NOINPUT
void setacquire (long mouse, long kbd)
{
	if (mouse_acquire != mouse)
	{
		if (ActiveApp && gpMouse) { if (mouse) { gpMouse->Acquire(); gbstatus = 0; } else gpMouse->Unacquire(); }
		mouse_acquire = mouse;
	}
	if (kbd_acquire != kbd)
	{
		if (ActiveApp && gpKeyboard) { if (kbd) { gpKeyboard->Acquire(); shkeystatus = 0; } else gpKeyboard->Unacquire(); }
		kbd_acquire = kbd;
	}
}
void setmouseout (void (*in)(long,long), long x, long y)
{
	if (fullscreen) return;
	setmousein = in;
	setacquire(0, kbd_acquire);

	POINT topLeft;
	topLeft.x = 0; topLeft.y = 0;
	ClientToScreen(ghwnd, &topLeft);
	SetCursorPos(topLeft.x + x, topLeft.y + y);
}

	//Use fancy clipper to determine if mouse cursor (x,y) is outside the window
	//Note: x,y is relative to top-left corner of ghwnd
long ismouseout (long x, long y)
{
	if (fullscreen) return(0);
#ifndef NO_CLIPPER
	//unsigned long siz;

	//ddclip->GetClipList(0,0,&siz);
	//if (siz > ddcliprdbytes)
	//{
	//   ddcliprdbytes = siz;
	//   ddcliprd = (RGNDATA *)realloc(ddcliprd,siz);
	//}
#ifndef NODRAW
	if (ddcliprd)
	{
		POINT abspos;
		RECT *r;
		long j;
		//ddclip->GetClipList(0,ddcliprd,&siz);

		abspos.x = x; abspos.y = y;
		ClientToScreen(ghwnd,&abspos);

		r = (RECT *)ddcliprd->Buffer;
		for(j=0;j<(long)ddcliprd->rdh.nCount;j++)
		{
			if ((abspos.x >= r[j].left) && (abspos.x < r[j].right) &&
				 (abspos.y >= r[j].top) && (abspos.y < r[j].bottom))
				return(0);
		}
		return(1);
	}
#endif
#endif
	return(((unsigned long)x >= (unsigned long)xres) || ((unsigned long)y >= (unsigned long)yres));
}

#endif

long (CALLBACK *peekwindowproc)(HWND,UINT,WPARAM,LPARAM) = 0;
long CALLBACK WindowProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// mouse pos:
	static long omx, omy;
	long mx, my;

	if (peekwindowproc) { mx = peekwindowproc(hwnd,msg,wParam,lParam); if (mx >= 0) return(mx); }
	switch (msg)
	{
		case WM_SYSCOMMAND:
			if ((wParam == SC_KEYMENU) || (wParam == SC_HOTKEY)) return(0);
			break;
		case WM_ACTIVATEAPP:
			ActiveApp = (BOOL)wParam; //!((BOOL)HIWORD(wParam));
			shkeystatus = 0;
#ifndef NOINPUT
			gkillbstatus = 1;
#endif
			break;

		case WM_ACTIVATE:
			//ActiveApp = LOWORD(wParam); //((wParam&65535) != WA_INACTIVE);
#ifndef NOINPUT
			if (gpMouse) { if (ActiveApp && mouse_acquire) { gpMouse->Acquire(); gbstatus = 0; } else gpMouse->Unacquire(); }
			if (gpKeyboard) { if (ActiveApp && kbd_acquire) { gpKeyboard->Acquire(); shkeystatus = 0; } else gpKeyboard->Unacquire(); }
#endif
#ifndef NODRAW
			if ((!fullscreen) && (ActiveApp) && (ddpal) && (ddsurf[0]))
			{
				if (ddsurf[0]->IsLost() == DDERR_SURFACELOST) ddsurf[0]->Restore();
				ddsurf[0]->SetPalette(ddpal);
				updatepalette(0,256);
			}
			InvalidateRect(hwnd,0,1);
#endif
			break;
		case WM_PAINT:
#ifndef NODRAW
			if (!fullscreen) nextpage();
#endif
			break;

		case WM_SIZE:
			if (wParam == SIZE_MAXHIDE)
				ActiveApp = 0;
			else if (wParam == SIZE_MINIMIZED)
				ActiveApp = 0;
			else
			{
				ActiveApp = 1;
#ifndef NODRAW
#ifndef ZOOM_TEST
				if ((!fullscreen) && (ddsurf[1]))
				{
					long oxres = xres, oyres = yres;
					xres = LOWORD(lParam); yres = HIWORD(lParam);

					if (ddsurf[1]) { ddsurf[1]->Release(); ddsurf[1] = 0; }

					ddsd.dwSize = sizeof(ddsd);
					ddsd.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_CAPS;
					ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
					ddsd.dwWidth = xres;
					ddsd.dwHeight = yres;
					lpdd->CreateSurface(&ddsd,&ddsurf[1],0);

					if (ddrawemulbuf)
					{
						long i, x, y, ye, pw, pr;

						i = ((colbits+7)>>3)*xres;
						if (i < ddrawemulbpl)
						{
							pw = pr = ((long)ddrawemulbuf); ye = min(yres,oyres);
							for(y=1;y<ye;y++)
							{
								pw += i; pr += ddrawemulbpl;
								memcpy((void *)pw,(void *)pr,i);
							}
						}

						ddrawemulbuf = realloc(ddrawemulbuf,((xres*yres+7)&~7)*((colbits+7)>>3)+16);
						if (!ddrawemulbuf)
						{
							xres = oxres; yres = oyres; //oops :/
							ddrawemulbuf = realloc(ddrawemulbuf,((xres*yres+7)&~7)*((colbits+7)>>3)+16);
							i = ddrawemulbpl;
						}

						if (i > ddrawemulbpl)
						{
							pw = yres*i            + ((long)ddrawemulbuf);
							pr = yres*ddrawemulbpl + ((long)ddrawemulbuf);
							for(y=yres-1;y>=0;y--)
							{
								pw -= i; pr -= ddrawemulbpl;
								for(x=i-1;x>=ddrawemulbpl;x--) *(char *)(pw+x) = 0;
								for(;x>=0;x--) *(char *)(pw+x) = *(char *)(pr+x);
							}
						}
						if (yres > oyres) memset((void *)(oyres*i+((long)ddrawemulbuf)),0,(yres-oyres)*i);

						ddrawemulbpl = i;
					}
				}
#endif
#endif
			}
#ifndef NOINPUT
			if (gpMouse) { if (ActiveApp && mouse_acquire) { gpMouse->Acquire(); gbstatus = 0; } else gpMouse->Unacquire(); }
			if (gpKeyboard) { if (ActiveApp && kbd_acquire) { gpKeyboard->Acquire(); shkeystatus = 0; } else gpKeyboard->Unacquire(); }
#endif
			break;
		case WM_KEYDOWN:
#ifdef NOINPUT
			keystatus[((lParam>>16)&127)+((lParam>>17)&128)] = 1;
#endif
		case WM_SYSKEYDOWN:
			if ((wParam&0xff) == 0xff) break; //Fixes SHIFT+[ext key] on XP
			switch (lParam&0x17f0000)
			{
				case 0x02a0000: shkeystatus |= (1<<16); break; //0x2a
				case 0x0360000: shkeystatus |= (1<<17); break; //0x36
				case 0x01d0000: shkeystatus |= (1<<18); break; //0x1d
				case 0x11d0000: shkeystatus |= (1<<19); break; //0x9d
				case 0x0380000: shkeystatus |= (1<<20); break; //0x38
				case 0x1380000: shkeystatus |= (1<<21); break; //0xb8
				default:
					{
					long i = ((keybufw2+1)&(KEYBUFSIZ-1));
					keybuf[keybufw2&(KEYBUFSIZ-1)] = (((lParam>>8)&0x7f00)+((lParam>>9)&0x8000))+shkeystatus;
					if (i != keybufr) keybufw2 = i; //prevent fifo overlap
					}
			}
			return(0);
		case WM_KEYUP:
#ifdef NOINPUT
			keystatus[((lParam>>16)&127)+((lParam>>17)&128)] = 0;
#endif
		case WM_SYSKEYUP:
			if ((wParam&0xff) == 0xff) break; //Fixes SHIFT+[ext key] on XP
			switch (lParam&0x17f0000)
			{
				case 0x02a0000: shkeystatus &= ~(3<<16); break; //0x2a
				case 0x0360000: shkeystatus &= ~(3<<16); break; //0x36
				case 0x01d0000: shkeystatus &= ~(1<<18); break; //0x1d
				case 0x11d0000: shkeystatus &= ~(1<<19); break; //0x9d
				case 0x0380000: shkeystatus &= ~(1<<20); break; //0x38
				case 0x1380000: shkeystatus &= ~(1<<21); break; //0xb8
			}
			return(0);
		case WM_CHAR:
			if (keybufw2 != keybufr) //stick ASCII code in last FIFO value
				keybuf[(keybufw2-1)&(KEYBUFSIZ-1)] |= (wParam&255);
			return(0);
		case WM_MOUSEWHEEL:
			mx = (signed short)HIWORD(wParam);
				  if (mx > 0) ext_mbstatus[6] = 2;
			else if (mx < 0) ext_mbstatus[7] = 2;
			ext_mwheel = min(max(ext_mwheel+mx,(1<<16)-(1<<31)),(1<<31)-(1<<16));
			break;
#ifndef NOINPUT
		case WM_MOUSEMOVE:
			mx = LOWORD(lParam);
			my = HIWORD(lParam);
			if (gpMouse && ActiveApp && setmousein) {
				mouse_acquire = 1;
				gpMouse->Acquire(); gbstatus = 0;
				setmousein(mx, my);
				setmousein = NULL;
			}
			break;
#endif
		case WM_LBUTTONDOWN: if (!mouse_acquire) ext_mbstatus[0] = 1|2; break;
		case WM_LBUTTONUP:   if (!mouse_acquire) ext_mbstatus[0] &= ~2; break;
		case WM_RBUTTONDOWN: if (!mouse_acquire) ext_mbstatus[1] = 1|2; break;
		case WM_RBUTTONUP:   if (!mouse_acquire) ext_mbstatus[1] &= ~2; break;
#ifndef NODRAW
		case WM_ERASEBKGND: return(1); //flicker bug?
		case WM_NCPAINT: if (fullscreen) return(0); break; //don't redraw window frame - flicker bug?
#endif
		case WM_CLOSE:
			 //FYI: Application terminates in this order:
			 //if (keystatus[1]) Post(WM_CLOSE) //Located inside doframe
			 //WM_CLOSE: uninitdirect* //WARNING: must be before DestroyWindow!
			 //          DestroyWindow!
			 //          Post(WM_DESTROY)
			 //WM_DESTROY: Post(WM_QUIT)
			 //WM_QUIT: quitprogram = 1;
			 //if (quitprogram) exit();
#ifndef NOINPUT
			uninitmouse();
			uninitkeyboard();
			uninitdirectinput();
#endif
#ifndef NOSOUND
			uninitdirectsound();
#endif
#ifndef NODRAW
			uninitdirectdraw();
#endif
			uninitapp();
			break;
		case WM_DESTROY:
				//Does this remove taskbar icon?
			UnregisterClass(wc.lpszClassName,ghinst);
			PostQuitMessage(0);
			return(0);
		default: break;
	}
	return(DefWindowProc(hwnd,msg,wParam,lParam));
}

long keyread ()
{
	long i;

	if (keybufr == keybufw) return(0);
	i = keybuf[keybufr]; keybufr = ((keybufr+1)&(KEYBUFSIZ-1));
#if defined(_DEBUG) && !defined(NODRAW)
	if (((i>>8)&255) == 0x2e && (i&(3<<18))) { // && (i&(3<<20))) {
		debugdirectdraw();
		if (ddrawdebugmode != -1) __asm { int 3 };
	}
#endif
	return(i);
}

void breath ()
{
	MSG msg;
	while (PeekMessage(&msg,0,0,0,PM_REMOVE)) //&msg,ghwnd, |PM_NOYIELD?
	{
		if (msg.message == WM_QUIT) { quitprogram = 1; quitparam = msg.wParam; return; }
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	keybufw = keybufw2; // to be safe with multithreads

#ifndef NOSOUND
#if (USEKENSOUND == 1)
	ENTERMUTX;
	kensoundbreath(MINBREATHREND);
	LEAVEMUTX;
	umixerbreathe();
#endif
#endif
}

	//10/20/2004: this code is useless now that quotes are automatically stripped off properly in WinMain()
	//Call like this: arg2filename(argv[1],".ksm",curfilename);
	//Make sure curfilename length is long enough!
void arg2filename (const char *oarg, const char *ext, char *narg)
{
	long i;

		//Chop off quotes at beginning and end of filename
	for(i=strlen(oarg);i>0;i--)
		if (oarg[i-1] == '\\') break;
	if ((!i) && (oarg[0] == '\"')) i = 1;
	strcpy(narg,&oarg[i]);
	if (narg[0] == 0) return;
	if (narg[strlen(narg)-1] == '\"') narg[strlen(narg)-1] = 0;
	strlwr(narg);
	if (!strstr(narg,ext)) strcat(narg,ext);
}

#ifdef __WATCOMC__

	//Precision: bits 8-9:, Rounding: bits 10-11:
	//00 = 24-bit    (0)   00 = nearest/even (0)
	//01 = reserved  (1)   01 = -inf         (4)
	//10 = 53-bit    (2)   10 = inf          (8)
	//11 = 64-bit    (3)   11 = 0            (c)
static long fpuasm[2];
void fpuinit (long);
#pragma aux fpuinit =\
	"fninit"\
	"fstcw fpuasm"\
	"and byte ptr fpuasm[1], 0f0h"\
	"or byte ptr fpuasm[1], al"\
	"fldcw fpuasm"\
	parm [eax]

#endif
#ifdef _MSC_VER

	//Precision: bits 8-9:, Rounding: bits 10-11:
	//00 = 24-bit    (0)   00 = nearest/even (0)
	//01 = reserved  (1)   01 = -inf         (4)
	//10 = 53-bit    (2)   10 = inf          (8)
	//11 = 64-bit    (3)   11 = 0            (c)
static long fpuasm[2];
static _inline void fpuinit (long a)
{
	_asm
	{
		mov eax, a
		fninit
		fstcw fpuasm
		and byte ptr fpuasm[1], 0f0h
		or byte ptr fpuasm[1], al
		fldcw fpuasm
	}
}

#endif

int WINAPI WinMain (HINSTANCE hinst, HINSTANCE hpinst, LPSTR cmdline, int ncmdshow)
{
	long i, j, k, inquote, argc;
	char *argv[MAX_PATH>>1];

	ghinst = hinst; ghpinst = hpinst; gcmdline = cmdline; gncmdshow = ncmdshow;

	cputype = getcputype();
	if (cputype&((1<<0)+(1<<4)) != ((1<<0)+(1<<4)))
		{ MessageBox(0,"Sorry, this program requires FPU&RDTSC support (>=Pentium)",prognam,MB_OK); return(-1); }
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&osvi);

	initklock();
#ifndef NOINPUT
	for(i=0;i<256;i++) keystatus[i] = 0;
	for(i=0;i<256;i++) ext_keystatus[i] = 0;
#endif

		//Convert Windows command line into ANSI 'C' command line...
	argv[0] = "exe"; argc = 1; j = inquote = 0;
	for(i=0;cmdline[i];i++)
	{
		k = (((cmdline[i] != ' ') && (cmdline[i] != '\t')) || (inquote));
		if (cmdline[i] == '\"') inquote ^= 1;
		if (j < k) { argv[argc++] = &cmdline[i+inquote]; j = inquote+1; continue; }
		if ((j) && (!k))
		{
			if ((j == 2) && (cmdline[i-1] == '\"')) cmdline[i-1] = 0;
			cmdline[i] = 0; j = 0;
		}
	}
	if ((j == 2) && (cmdline[i-1] == '\"')) cmdline[i-1] = 0;
	argv[argc] = 0;
	if (initapp(argc,argv) < 0) return(-1);

	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WindowProc;
	wc.cbClsExtra = wc.cbWndExtra = 0;
	wc.hInstance = ghinst;
	wc.hIcon = LoadIcon(ghinst,IDI_APPLICATION);
	wc.hCursor = 0; //LoadCursor(0,IDC_ARROW);
#ifndef NODRAW
	wc.hbrBackground = 0;
#else
#ifdef _MSC_VER
	//wc.hbrBackground = (HBRUSH__ *)GetStockObject(BLACK_BRUSH);
	wc.hbrBackground = (struct HBRUSH__ *)GetStockObject(BLACK_BRUSH);
#endif
#ifdef __WATCOMC__
	wc.hbrBackground = GetStockObject(BLACK_BRUSH);
#endif
#endif
	wc.lpszMenuName = 0;
	wc.lpszClassName = prognam;
	RegisterClass(&wc);

	progwndflags = WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;
	progwndadd[1] = GetSystemMetrics(SM_CYCAPTION);
	if (progresiz) {
		progwndflags |= WS_MAXIMIZEBOX|WS_THICKFRAME;
		progwndadd[0] = GetSystemMetrics(SM_CXSIZEFRAME)*2;
		progwndadd[1] += GetSystemMetrics(SM_CYSIZEFRAME)*2;
	} else {
		progwndadd[0] = GetSystemMetrics(SM_CXFIXEDFRAME)*2;
		progwndadd[1] += GetSystemMetrics(SM_CYFIXEDFRAME)*2;
	}

#ifndef NODRAW
	if (fullscreen)
	{
		if ((ghwnd = CreateWindowEx(WS_EX_TOPMOST,prognam,prognam,WS_POPUP,0,0,GetSystemMetrics(SM_CXSCREEN),
											 GetSystemMetrics(SM_CYSCREEN),0,0,ghinst,0)) == 0) return(0);
	}
	else
	{
#endif
		if (progwndx == 0x80000000)
		{
			RECT rw;
			SystemParametersInfo(SPI_GETWORKAREA,0,&rw,0);
			progwndx = ((rw.right -rw.left-(xres+progwndadd[0]))>>1) + rw.left;
			progwndy = ((rw.bottom-rw.top -(yres+progwndadd[1]))>>1) + rw.top;
		}
		if ((ghwnd = CreateWindowEx(0,prognam,prognam,progwndflags,
			progwndx,progwndy,xres+progwndadd[0],yres+progwndadd[1],0,0,ghinst,0)) == 0) return(0);
#ifndef NODRAW
	}
#endif

	ShowWindow(ghwnd,gncmdshow);
#ifndef NODRAW
	if ((!fullscreen) && (gncmdshow == SW_MAXIMIZE))
	{
		RECT rw;
		SystemParametersInfo(SPI_GETWORKAREA,0,&rw,0);
		xres = rw.right -rw.left;
		yres = rw.bottom-rw.top;
	}
#endif

		//Enable this for smooth start-up, but beware: it halts disk caches!
	//HANDLE hProc = GetCurrentProcess();
	//SetPriorityClass(hProc,REALTIME_PRIORITY_CLASS);

#ifndef NODRAW
	if (!initdirectdraw(xres,yres,colbits)) { DestroyWindow(ghwnd); return(0); }
#endif
#if defined(_DEBUG) && !defined(NODRAW)
	debugdirectdraw(); // enable debug mode by default
#endif

#ifndef NOSOUND
#ifdef DSOUNDINITCOM
	coinit = CoInitialize(NULL);
#endif
	initdirectsound();
#endif

	UpdateWindow(ghwnd);

#ifndef NOINPUT
	if (!initdirectinput(ghwnd))
	{
		DestroyWindow(ghwnd);
#ifndef NOSOUND
		uninitdirectsound();
#endif
#ifndef NODRAW
		uninitdirectdraw();
#endif
		return(0);
	}
	if (!initkeyboard(ghwnd))
	{
		uninitdirectinput(); DestroyWindow(ghwnd);
#ifndef NOSOUND
		uninitdirectsound();
#endif
#ifndef NODRAW
		uninitdirectdraw();
#endif
		return(0);
	}
	if (!initmouse(ghwnd))
	{
		uninitkeyboard(); uninitdirectinput(); DestroyWindow(ghwnd);
#ifndef NOSOUND
		uninitdirectsound();
#endif
#ifndef NODRAW
		uninitdirectdraw();
#endif
		return(0);
	}
#endif

#if ((USEKENSOUND == 1) && (USETHREADS != 0))
	if (dsound)
	{
		hmutx = CreateMutex(0,0,0);
		_beginthread(kensoundthread,0,0);
	}
#endif
	breath();
	while (!quitprogram)
	{
		if (alwaysactive || ActiveApp) { fpuinit(0x2); doframe(); }
		else WaitMessage();
		breath();
	}
#if ((USEKENSOUND == 1) && (USETHREADS != 0))
	if (dsound)
	{
		while (quitprogram != 2) Sleep(USETHREADS);
		ENTERMUTX;
		CloseHandle(hmutx);
	}
#endif
#ifndef NOSOUND
#ifdef DSOUNDINITCOM
	if (coinit == S_OK) CoUninitialize();
#endif
#endif
	return(quitparam);
}
