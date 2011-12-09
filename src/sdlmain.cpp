#ifndef __GNUC__
#error GCC only.
#endif

#ifndef __i386__
#error i386 targets only.
#endif

#include "SDL/SDL.h"

#include <stdio.h>
#include <stdlib.h>

#ifndef NOSOUND
#ifdef USEKZ
extern long kzopen (const char *);
extern long kzread (void *, long);
extern void kzseek (long, long);
extern void kzclose ();
extern long kzeof ();
#endif

typedef struct __attribute__ ((packed)) tWAVEFORMATEX {
	short wFormatTag;
	short nChannels;
	long nSamplesPerSec;
	long nAvgBytesPerSec;
	short nBlockAlign;
	short wBitsPerSample;
	short cbSize;
} WAVEFORMATEX;
#define WAVE_FORMAT_PCM 1

#if !defined(max)
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif
#if !defined(min)
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#endif

#undef _WIN32
#include "Sysmain.h"

#define evalmacro(x) evalmacrox(x)
#define evalmacrox(x) #x

char *prognam = "WinMain App (by KS/TD)";
long progresiz = 1;

extern long initapp (long argc, char **argv);
extern void uninitapp ();
extern void doframe ();

static long quitprogram = 0, quitparam;
void breath();

long ActiveApp = 1, alwaysactive = 0;
long xres = 640, yres = 480, colbits = 8, fullscreen = 1, maxpages = 8;

//======================== CPU detection code begins ========================

static long cputype asm("cputype") = 0;

static inline long testflag (long c)
{
	long a;
	__asm__ __volatile__ (
		"pushf\n\tpopl %%eax\n\tmovl %%eax, %%ebx\n\txorl %%ecx, %%eax\n\tpushl %%eax\n\t"
		"popf\n\tpushf\n\tpopl %%eax\n\txorl %%ebx, %%eax\n\tmovl $1, %%eax\n\tjne 0f\n\t"
		"xorl %%eax, %%eax\n\t0:"
		: "=a" (a) : "c" (c) : "ebx","cc" );
	return a;
}

static inline void cpuid (long a, long *s)
{
	__asm__ __volatile__ (
		"cpuid\n\tmovl %%eax, (%%esi)\n\tmovl %%ebx, 4(%%esi)\n\t"
		"movl %%ecx, 8(%%esi)\n\tmovl %%edx, 12(%%esi)"
		: "+a" (a) : "S" (s) : "ebx","ecx","edx","memory","cc");
}

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

static Uint64 rdtsc64(void)
{
	Uint64 q;
	__asm__ __volatile__ ("rdtsc" : "=A" (q) : : "cc");
	return q;
}

static Uint32 pertimbase;
static Uint64 rdtimbase, nextimstep;
static double perfrq, klockmul, klockadd;

void initklock ()
{
	perfrq = 1000.0;
	rdtimbase = rdtsc64();
	pertimbase = SDL_GetTicks();
	nextimstep = 4194304; klockmul = 0.000000001; klockadd = 0.0;
}

void readklock (double *tim)
{
	Uint64 q = rdtsc64()-rdtimbase;
	if (q > nextimstep)
	{
		Uint32 p;
		double d;
		p = SDL_GetTicks();
		d = klockmul; klockmul = ((double)(p-pertimbase))/(((double)q)*perfrq);
		klockadd += (d-klockmul)*((double)q);
		do { nextimstep <<= 1; } while (q > nextimstep);
	}
	(*tim) = ((double)q)*klockmul + klockadd;
}

//=================== Fast & accurate TIMER FUNCTIONS ends ===================

//SDL Video VARIABLES & CODE--------------------------------------------------
#ifndef NODRAW

#define BASICSURFBITS (SDL_HWSURFACE|SDL_DOUBLEBUF)

PALETTEENTRY pal[256];

static SDL_Surface *sdlsurf = NULL;
static int surflocked = 0;

#define MAXVALIDMODES 256
//typedef struct { long x, y; char c, r0, g0, b0, a0, rn, gn, bn, an; } validmodetype;
static validmodetype validmodelist[MAXVALIDMODES];
static long validmodecnt = 0;
validmodetype curvidmodeinfo;


static void gvmladd(long x, long y, char c)
{
	if (validmodecnt == MAXVALIDMODES) return;
	memset(&validmodelist[validmodecnt], 0, sizeof(validmodetype));
	validmodelist[validmodecnt].x = x;
	validmodelist[validmodecnt].y = y;
	validmodelist[validmodecnt].c = c;
	validmodecnt++;
}

long getvalidmodelist (validmodetype **davalidmodelist)
{
	int cd[5] = { 8,15,16,24,32 }, i, j;
	SDL_Rect **modes;
	SDL_PixelFormat pf = { NULL, 8, 1, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0 };
	
	if (!validmodecnt) {
		for (i=0;i<5;i++) {
			pf.BitsPerPixel = cd[i];
			pf.BytesPerPixel = cd[i] >> 3;
			modes = SDL_ListModes(&pf, BASICSURFBITS|SDL_FULLSCREEN);
			if (modes == (SDL_Rect**)0) {
				// none available
			} if (modes == (SDL_Rect**)-1) {
				// add some defaults
				gvmladd(640, 480, cd[i]);
				gvmladd(800, 600, cd[i]);
				gvmladd(1024, 768, cd[i]);
				gvmladd(1152, 864, cd[i]);
				gvmladd(1280, 960, cd[i]);
				gvmladd(1280, 1024, cd[i]);
				gvmladd(1600, 1200, cd[i]);
			} else {
				for (j=0;modes[j];j++)
					gvmladd(modes[j]->w, modes[j]->h, cd[i]);
			}
		}
	}
	(*davalidmodelist) = validmodelist;
	return(validmodecnt);
}

void updatepalette (long start, long danum)
{
	SDL_Color pe[256];
	int i,j;
	
	if (!sdlsurf) return;

	for (i=0,j=danum; j>0; i++,j--) {
		pe[i].r = pal[start+i].peRed;
		pe[i].g = pal[start+i].peGreen;
		pe[i].b = pal[start+i].peBlue;
		pe[i].unused = 0;
	}
	SDL_SetColors(sdlsurf, pe, start, danum);
}

void stopdirectdraw()
{
	if (!surflocked) return;
	if (sdlsurf && SDL_MUSTLOCK(sdlsurf)) SDL_UnlockSurface(sdlsurf);
	surflocked = 0;
}

long startdirectdraw(long *vidplc, long *dabpl, long *daxres, long *dayres)
{
	if (surflocked) stopdirectdraw();

	if (SDL_LockSurface(sdlsurf) < 0) return 0;
	
	*vidplc = (long)sdlsurf->pixels; *dabpl = sdlsurf->pitch;
	*daxres = sdlsurf->w; *dayres = sdlsurf->h; surflocked = 1;
	return 1;
}

void nextpage()
{
	if (!sdlsurf) return;
	if (surflocked) stopdirectdraw();
	SDL_Flip(sdlsurf);
}

long clearscreen(long fillcolor)
{
	return SDL_FillRect(sdlsurf, NULL, fillcolor) == 0;
}

long initdirectdraw(long daxres, long dayres, long dacolbits)
{
	Uint32 surfbits;

#ifndef NOINPUT
	extern long mouse_acquire;
	if (mouse_acquire) {
		SDL_WM_GrabInput(SDL_GRAB_OFF);
		SDL_ShowCursor(SDL_ENABLE);
	}
#endif
	
	xres = daxres; yres = dayres; colbits = dacolbits;

	surfbits = BASICSURFBITS;
	surfbits |= SDL_HWPALETTE;
	if (fullscreen) surfbits |= SDL_FULLSCREEN;
	else if (progresiz) surfbits |= SDL_RESIZABLE;

	sdlsurf = SDL_SetVideoMode(daxres, dayres, dacolbits, surfbits);
	if (!sdlsurf) {
		fputs("video init failed!",stderr);
		return 0;
	}

	// FIXME: should I?
	xres = sdlsurf->w;
	yres = sdlsurf->h;
	colbits = sdlsurf->format->BitsPerPixel;
	fullscreen = (sdlsurf->flags & SDL_FULLSCREEN) == SDL_FULLSCREEN;

	memset(&curvidmodeinfo, 0, sizeof(validmodetype));
	curvidmodeinfo.x = sdlsurf->w;
	curvidmodeinfo.y = sdlsurf->h;
	curvidmodeinfo.c = sdlsurf->format->BitsPerPixel;
	curvidmodeinfo.r0 = sdlsurf->format->Rshift;
	curvidmodeinfo.rn = 8 - sdlsurf->format->Rloss;
	curvidmodeinfo.g0 = sdlsurf->format->Gshift;
	curvidmodeinfo.gn = 8 - sdlsurf->format->Gloss;
	curvidmodeinfo.b0 = sdlsurf->format->Bshift;
	curvidmodeinfo.bn = 8 - sdlsurf->format->Bloss;
	curvidmodeinfo.a0 = sdlsurf->format->Ashift;
	curvidmodeinfo.an = 8 - sdlsurf->format->Aloss;
	
	if (colbits == 8) updatepalette(0,256);

#ifndef NOINPUT
	if (mouse_acquire) {
		SDL_WM_GrabInput(SDL_GRAB_ON);
		SDL_ShowCursor(SDL_DISABLE);
	}
#endif

	return 1;
}

long changeres(long daxres, long dayres, long dacolbits, long dafullscreen)
{
	fullscreen = dafullscreen;
	return initdirectdraw(daxres, dayres, dacolbits);
}

#endif

//SDL Input VARIABLES & CODE--------------------------------------------------
char keystatus[256];
static long shkeystatus = 0;
#define KEYBUFSIZ 256
static long keybuf[KEYBUFSIZ], keybufr = 0, keybufw = 0, keybufw2 = 0;

#ifndef NOINPUT

char ext_keystatus[256]; // +TD
char ext_mbstatus[8] = {0}; // +TD extended mouse button status
long mouse_acquire = 1, kbd_acquire = 1;
static void (*setmousein)(long, long) = NULL;
static long mousex = 0, mousey = 0, gbstatus = 0;
long mousmoth=0;
float mousper=0.0;

void readkeyboard ()
{
	// all gets done in sdlmsgloop now
}

void readmouse (float *fmousx, float *fmousy, long *bstatus)
{
	if (!mouse_acquire) { *fmousx = *fmousy = 0; *bstatus = 0; return; }
	
	*fmousx = (float)mousex;
	*fmousy = (float)mousey;
	*bstatus = gbstatus;

	mousex = mousey = 0;
}

#endif

// Kensound code begins -------------------------------------------------------

#ifndef NOSOUND
typedef struct {
	void *buf;
	long buflen;
	long writecur;   // play cursor is behind this
	long samprate, numchans, bytespersamp;
} KSoundBuffer;

typedef struct {
	long dwBufferBytes;
	WAVEFORMATEX *lpwfxFormat;
} KSBufferDesc;


#define KS_OK 0
#define KS_NOTOK -1

int KSound_CreateSoundBuffer(KSBufferDesc *dsbdesc, KSoundBuffer **ksb)
{
	KSoundBuffer *ksbf;

	*ksb = NULL;

	ksbf = (KSoundBuffer *)calloc(1, sizeof(KSoundBuffer));
	if (!ksbf) return KS_NOTOK;

	ksbf->buf = (void*)calloc(1, dsbdesc->dwBufferBytes);
	if (!ksbf->buf) {
		free(ksbf);
		return KS_NOTOK;
	}

	ksbf->buflen = dsbdesc->dwBufferBytes;
	ksbf->writecur = 0;
	ksbf->samprate = dsbdesc->lpwfxFormat->nSamplesPerSec;
	ksbf->numchans = dsbdesc->lpwfxFormat->nChannels;
	ksbf->bytespersamp = dsbdesc->lpwfxFormat->wBitsPerSample >> 3;

	*ksb = ksbf;

	return KS_OK;
}

int KSoundBuffer_Release(KSoundBuffer *ksb)
{
	if (!ksb) return KS_NOTOK;

	if (ksb->buf) free(ksb->buf);
	free(ksb);

	return KS_OK;
}

#define KSBLOCK_FROMWRITECURSOR 1
#define KSBLOCK_ENTIREBUFFER 2
int KSoundBuffer_Lock(KSoundBuffer *ksb, long wroffs, long wrlen,
		void **ptr1, long *len1, void **ptr2, long *len2, long flags)
{
	if (!ksb || !ptr1 || !len1 || !ptr2 || !len2) return KS_NOTOK;
	
	if (flags & KSBLOCK_FROMWRITECURSOR) {
		wroffs = ksb->writecur;
	} else {
		wroffs %= ksb->buflen;
	}

	if (flags & KSBLOCK_ENTIREBUFFER) {
		wrlen = ksb->buflen;
	} else {
		wrlen = min(ksb->buflen, wrlen);
	}

	*ptr1 = (void*)((long)ksb->buf + wroffs);
	*len1 = wrlen;
	*ptr2 = NULL;
	*len2 = 0;
	if (wroffs + wrlen > ksb->buflen) {
		wrlen = wroffs + wrlen - ksb->buflen;
		*len1 -= wrlen;

		*ptr2 = (void*)(ksb->buf);
		*len2 = wrlen;
	}

	return KS_OK;
}

int KSoundBuffer_GetCurrentPosition(KSoundBuffer *ksb, long *play, long *write)
{
	if (!ksb) return KS_NOTOK;
	
	if (play) {
		// FIXME: Can't determine this since SDL calls us via a callback!
		*play = ksb->writecur - 0;
	}

	if (write) {
		*write = ksb->writecur;
	}
	
	return KS_OK;
}

#define ENTERMUTX SDL_LockAudio()
#define LEAVEMUTX SDL_UnlockAudio()

//--------------------------------------------------------------------------------------------------
#define MAXUMIXERS 256
#define UMIXERBUFSIZ 65536
typedef struct
{
	KSoundBuffer *streambuf;
	long samplerate, numspeakers, bytespersample, oplaycurs;
	void (*mixfunc)(void *dasnd, long danumbytes);
} umixertyp;
static umixertyp umixer[MAXUMIXERS];
static long umixernum = 0;

long umixerstart (void damixfunc (void *, long), long dasamprate, long danumspeak, long dabytespersamp)
{
	KSBufferDesc dsbdesc;
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

	memset(&dsbdesc,0,sizeof(KSBufferDesc));
	dsbdesc.dwBufferBytes = UMIXERBUFSIZ;
	dsbdesc.lpwfxFormat = &wfx;
	if (KSound_CreateSoundBuffer(&dsbdesc,&umixer[umixernum].streambuf) != KS_OK) return(-1);

		//Zero out streaming buffer before beginning play
	KSoundBuffer_Lock(umixer[umixernum].streambuf, 0,UMIXERBUFSIZ,&w0,&l0,&w1,&l1,0);
	if (w0) memset(w0,(dabytespersamp-2)&128,l0);
	if (w1) memset(w1,(dabytespersamp-2)&128,l1);
	//umixer[umixernum].streambuf->Unlock(w0,l0,w1,l1);

	//umixer[umixernum].streambuf->SetCurrentPosition(0);
	//umixer[umixernum].streambuf->Play(0,0,DSBPLAY_LOOPING);

	umixer[umixernum].oplaycurs = 0;
	umixer[umixernum].mixfunc = damixfunc;
	umixernum++;

	return(umixernum-1);
}

void umixerkill (long i)
{
	if ((unsigned long)i >= umixernum) return;
	if (umixer[i].streambuf) {
		//umixer[i].streambuf->Stop();
		KSoundBuffer_Release(umixer[i].streambuf);
		umixer[i].streambuf = 0;
	}
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
		if (KSoundBuffer_GetCurrentPosition(umixer[i].streambuf, &playcurs,&writcurs) != KS_OK) continue;
		playcurs += ((umixer[i].samplerate/8)<<(umixer[i].bytespersample+umixer[i].numspeakers-2));
		l = (((playcurs-umixer[i].oplaycurs)&(UMIXERBUFSIZ-1))>>(umixer[i].bytespersample+umixer[i].numspeakers-2));
		if (l <= 256) continue;
		if (KSoundBuffer_Lock(umixer[i].streambuf, umixer[i].oplaycurs&(UMIXERBUFSIZ-1),l<<(umixer[i].bytespersample+umixer[i].numspeakers-2),&w0,&l0,&w1,&l1,0) != KS_OK) continue;
		if (w0) umixer[i].mixfunc(w0,l0);
		if (w1) umixer[i].mixfunc(w1,l1);
		umixer[i].oplaycurs += l0+l1;
		//umixer[i].streambuf->Unlock(w0,l0,w1,l1);
	}
}
//--------------------------------------------------------------------------------------------------
typedef struct { float x, y, z; } point3d;
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

KSoundBuffer *streambuf = 0;

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
static short coef[NUMCOEF<<LOGCOEFPREC] __attribute__ (( aligned(16) )); //Sound re-scale filter coefficients
static short coeflopass[NUMCOEF<<LOGCOEFPREC] __attribute__ (( aligned(16) )); //Sound re-scale filter coefficients

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
		//Sinc*RaisedCos filter (dad says this is a window-designed filter):
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

#define mulshr16(a,d) \
	({ long __a=(a), __d=(d); \
		__asm__ __volatile__ ("imull %%edx; shrdl $16, %%edx, %%eax" \
		: "+a" (__a), "+d" (__d) : : "cc"); \
	 __a; })

#if 1
#define mulshr32(a,d) \
	({ long __a=(a), __d=(d); \
		__asm__ __volatile__ ("imull %%edx" \
		: "+a" (__a), "+d" (__d) : : "cc"); \
	 __d; })
#endif

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
	{
		short *ssnd;  //Fast sound rendering; ok quality
		do
		{
			ssnd = (short *)(((ispos>>LSAMPREC)<<1)+dasnd);
			lptr[0] += mulshr32((long)(((((long)ssnd[1])-((long)ssnd[0]))*(ispos&((1<<LSAMPREC)-1)))>>LSAMPREC)+((long)ssnd[0]),ivolsc);
			ivolsc += ivolsci; ispos += isinc; lptr += 2; nsamp--;
		} while (nsamp);
	}
#elif (NUMCOEF == 4)
#if 1
	{
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
	}
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
		//Same as above, but does 8-byte aligned writes instead of 4
		__asm__ __volatile__ (
			"testl $4, %%edx\n\t"
			"leal (%%edx,%%ecx,4), %%edx\n\t"
			"leal (%%eax,%%ecx,8), %%eax\n\t"
			"jz 0f\n\t"   // skipc
			"negl %%ecx\n\t"
			"movq (%%eax,%%ecx,8), %%mm0\n\t"
			"packssdw %%mm0, %%mm0\n\t"
			"movd %%mm0, (%%edx,%%ecx,4)\n\t"
			"addl $2, %%ecx\n\t"
			"jg 3f\n\t"   // endc
			"jz 2f\n\t"   // skipd
			"jmp 1f\n\t"   // begc1
			"0:\n\tnegl %%ecx\n\t"   // skipc:
			"addl $1, %%ecx\n\t"
			"1:\n\tmovq -8(%%eax,%%ecx,8), %%mm0\n\t"   // begc1:
			"packssdw (%%eax,%%ecx,8), %%mm0\n\t"
			"movq %%mm0, -4(%%edx,%%ecx,4)\n\t"
			"addl $2, %%ecx\n\t"
			"jl 1b\n\t"   // begc1
			"jg 3f\n\t"   // endc
			"2:\n\tmovq -8(%%eax), %%mm0\n\t"   // skipd:
			"packssdw %%mm0, %%mm0\n\t"
			"movd %%mm0, -4(%%edx)\n\t"
			"3:\n\t"   // endc:
			: "+a" (lptr), "+d" (dptr), "+c" (nsamp) : : "memory","cc"
		);
#endif
	}
}

void setears3d (float iposx, float iposy, float iposz,
					 float iforx, float ifory, float iforz,
					 float iheix, float iheiy, float iheiz)
{
	float f;
	ENTERMUTX;
	f = 1.f/sqrt(iheix*iheix+iheiy*iheiy+iheiz*iheiz); //Make audiostr same magnitude as audiofor
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
	//streambuf->GetStatus(&u); if (!(u&DSBSTATUS_LOOPING)) return;
	if (KSoundBuffer_GetCurrentPosition(streambuf,&playcurs,&writcurs) != KS_OK) return;
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
		KSoundBuffer_Lock(streambuf,i,leng,&w[0],&l[0],&w[1],&l[1],0);
		if (w[0]) memset(w[0],(bytespersample-2)&128,l[0]);
		if (w[1]) memset(w[1],(bytespersample-2)&128,l[1]);
		//streambuf->Unlock(w[0],l[0],w[1],l[1]);
	}
	else
	{
		KSoundBuffer_Lock(streambuf,i,leng,&w[0],&l[0],&w[1],&l[1],0);

		lptr[0] = &lsnd[i>>1]; lptr[1] = lsnd;
		for(j=numrendersnd-1;j>=0;j--)
		{
			if (rendersnd[j].flags&KSND_MOVE) rendersnd[j].p = *rendersnd[j].ptr; //Get new 3D position
			if (rendersnd[j].flags&KSND_3D)
			{
				float f, g, h;
				n = (signed long)(leng>>gshiftval);

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
			if (cputype&(1<<23)) __asm__ __volatile__ ("emms" : : : "cc"); //MMX
		}
		for(m=0;m<2;m++) if (w[m]) audclipcopy(lptr[m],(short *)w[m],l[m]>>gshiftval);
		if (cputype&(1<<23)) __asm__ __volatile__ ("emms" : : : "cc"); //MMX
		//streambuf->Unlock(w[0],l[0],w[1],l[1]);
	}

	oplaycurs = playcurs;
}

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

	if (!streambuf) { return; }
	ENTERMUTX;
	//streambuf->GetStatus(&u); if (!(u&DSBSTATUS_LOOPING)) { LEAVEMUTX; return; }

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
	if (KSoundBuffer_GetCurrentPosition(streambuf,&playcurs,&writcurs) != KS_OK) { LEAVEMUTX; return; }
		//If you use playcurs instead of writcurs, beginning of sound gets cut off :/
		//on WinXP: ((writcurs-playcurs)&(SNDSTREAMSIZ-1)) ranges from 6880 to 8820 (step 4)
	i = (writcurs&(SNDSTREAMSIZ-1));
	if (KSoundBuffer_Lock(streambuf,i,(oplaycurs-i)&(SNDSTREAMSIZ-1),&w[0],&l[0],&w[1],&l[1],0) != KS_OK) { LEAVEMUTX; return; }
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
	if (cputype&(1<<23)) __asm__ __volatile__ ("emms" : : : "cc"); //MMX
	//streambuf->Unlock(w[0],l[0],w[1],l[1]);

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

//--------------------------------------------------------------------------------------------------
void sdlmixcallback(void *userdata, Uint8 *stream, int len)
{
	long amt;
	
	if (!streambuf) return;
	
	// copy over the normal kensound mixing buffer
	amt = min(streambuf->buflen - streambuf->writecur, len);
	memcpy(stream, &((Uint8*)streambuf->buf)[ streambuf->writecur ], amt);
	streambuf->writecur += amt;
	if (streambuf->writecur == streambuf->buflen) streambuf->writecur = 0;
	stream += amt;
	len -= amt;
	if (len > 0) {
		memcpy(stream, &((Uint8*)streambuf->buf)[ streambuf->writecur ], len);
		streambuf->writecur += len;
	}
	
	// mix in the umixer buffers
}

static void kensoundclose ()
{
	numrendersnd = 0;

	if (streambuf) SDL_CloseAudio();
	
	if (audhashbuf)
	{
		long i;
		for(i=audlastloadedwav;i>=0;i=(*(long *)&audhashbuf[i+4]))
			free((void *)(*(long *)&audhashbuf[i+8]));
		free(audhashbuf); audhashbuf = 0;
	}
	audhashpos = audhashsiz = 0;

	if (streambuf) { KSoundBuffer_Release(streambuf); streambuf = 0; }
}

static int kensoundinit (int samprate, int numchannels, int bytespersamp)
{
	SDL_AudioSpec audwant,audgot;
	WAVEFORMATEX wft;
	KSBufferDesc bufdesc;
	void *w0, *w1;
	unsigned long l0, l1;

	if (streambuf) kensoundclose();

	audwant.freq = samprate;
	audwant.format = bytespersamp == 1 ? AUDIO_U8 : AUDIO_S16;
	audwant.channels = numchannels;
	audwant.samples = MINBREATHREND;
	audwant.callback = sdlmixcallback;
	audwant.userdata = NULL;

	if (SDL_OpenAudio(&audwant, &audgot) < 0) return -1;
	samprate = audgot.freq;
	switch (audgot.format) {
		case AUDIO_U8:
			bytespersamp = 1;
			break;
		case AUDIO_S16:
			bytespersamp = 2;
			break;
		default:
			SDL_CloseAudio();
			return -1;
	}
	numchannels = audgot.channels;

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

	bufdesc.dwBufferBytes = SNDSTREAMSIZ;
	bufdesc.lpwfxFormat = &wft;
	if (KSound_CreateSoundBuffer(&bufdesc,&streambuf) != KS_OK) {
		fprintf(stderr, "kensoundinit(): failed creating sound buffer\n");
		SDL_CloseAudio();
		return(-1);
	}

	//streambuf->SetVolume(curmusicvolume);
	//streambuf->SetFrequency(curmusicfrequency);
	//streambuf->SetPan(curmusicpan);

		//Zero out streaming buffer before beginning play
	KSoundBuffer_Lock(streambuf, 0,SNDSTREAMSIZ,&w0,&l0,&w1,&l1,0);
	if (w0) memset(w0,(bytespersample-2)&128,l0);
	if (w1) memset(w1,(bytespersample-2)&128,l1);
	//streambuf->Unlock(w0,l0,w1,l1);

	initfilters();

	//streambuf->SetCurrentPosition(0);
	//streambuf->Play(0,0,DSBPLAY_LOOPING);

	SDL_PauseAudio(0);

	return(0);
}

#endif

// Kensound code ends ---------------------------------------------------------


//Quitting routines ----------------------------------------------------------

void quitloop ()
{
	SDL_Event ev;
	ev.type = SDL_QUIT;
	SDL_PushEvent(&ev);
}

void evilquit (const char *str) //Evil because this function makes awful assumptions!!!
{
	kensoundclose();
	SDL_Quit();
	if (str) fprintf(stderr,"fatal error! %s\n",str);
	uninitapp();
	exit(0);
}

//GENERAL CODE-------------------------------------------------------------------

#ifndef NOINPUT
void setacquire (long mouse, long kbd)
{
	SDL_GrabMode g;
	
	// SDL doesn't let the mouse and keyboard be grabbed independantly
	if ((mouse || kbd) != mouse_acquire) {
		g = SDL_WM_GrabInput( (mouse || kbd) ? SDL_GRAB_ON : SDL_GRAB_OFF );
		mouse_acquire = kbd_acquire = (g == SDL_GRAB_ON);

		SDL_ShowCursor(mouse_acquire ? SDL_DISABLE : SDL_ENABLE);
	}
}

void setmouseout (void (*in)(long,long), long x, long y)
{
	if (fullscreen) return;
	setmousein = in;
	setacquire(0, kbd_acquire);

	SDL_WarpMouse(x,y);
}
#endif

static unsigned char keytranslation[SDLK_LAST] = {
	0, 0, 0, 0, 0, 0, 0, 0, 14, 15, 0, 0, 0, 28, 0, 0, 0, 0, 0, 89, 0, 0, 0, 
	0, 0, 0, 0, 1, 0, 0, 0, 0, 57, 2, 40, 4, 5, 6, 8, 40, 10, 11, 9, 13, 51,
	12, 52, 53, 11, 2, 3, 4, 5, 6, 7, 8, 9, 10, 39, 39, 51, 13, 52, 53, 3, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	26, 43, 27, 7, 12, 41, 30, 48, 46, 32, 18, 33, 34, 35, 23, 36, 37, 38, 50,
	49, 24, 25, 16, 19, 31, 20, 22, 47, 17, 45, 21, 44, 0, 0, 0, 0, 211, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 82, 79, 80, 81, 75, 76, 77, 71, 72, 73, 83, 181, 55, 74, 78, 156, 0,
	200, 208, 205, 203, 210, 199, 207, 201, 209, 59, 60, 61, 62, 63, 64, 65,
	66, 67, 68, 87, 88, 0, 0, 0, 0, 0, 0, 69, 58, 70, 54, 42, 157, 29, 184, 56,
	0, 0, 219, 220, 0, 0, 0, /*-2*/0, 84, 183, 221, 0, 0, 0
};


static void sdlmsgloop(void)
{
	SDL_Event ev;
	unsigned char sc;

	while (SDL_PollEvent(&ev)) {
		switch (ev.type) {
			case SDL_ACTIVEEVENT:
				if (ev.active.state & SDL_APPINPUTFOCUS) {
					shkeystatus = 0;
					gbstatus = 0;
					ActiveApp = ev.active.gain;
					if (mouse_acquire) {
						if (ActiveApp) {
							SDL_WM_GrabInput(SDL_GRAB_ON);
							SDL_ShowCursor(SDL_DISABLE);
						} else {
							SDL_WM_GrabInput(SDL_GRAB_OFF);
							SDL_ShowCursor(SDL_ENABLE);
						}
					}
				}
				break;
#ifndef NODRAW
			case SDL_VIDEORESIZE:
				ActiveApp = 1;
				initdirectdraw(ev.resize.w, ev.resize.h, colbits);
				break;
#endif
			case SDL_KEYDOWN:
				sc = keytranslation[ev.key.keysym.sym];
				keystatus[ sc ] = 1;   // FIXME: verify this is kosher
				ext_keystatus[ sc ] = 1|2;
				switch (ev.key.keysym.sym) {
					case SDLK_LSHIFT: shkeystatus |= (1<<16); break;
					case SDLK_RSHIFT: shkeystatus |= (1<<17); break;
					case SDLK_LCTRL:  shkeystatus |= (1<<18); break;
					case SDLK_RCTRL:  shkeystatus |= (1<<19); break;
					case SDLK_LALT:   shkeystatus |= (1<<20); break;
					case SDLK_RALT:   shkeystatus |= (1<<21); break;
					default:
						{
							long i = ((keybufw2+1)&(KEYBUFSIZ-1));
							keybuf[keybufw2&(KEYBUFSIZ-1)] = ((long)sc << 8)+shkeystatus;   // FIXME: verify
							if (i != keybufr) keybufw2 = i; //prevent fifo overlap
						}
						break;
				}
				// The WM_CHAR should be in here somewhere
				if (ev.key.keysym.unicode != 0 && (ev.key.keysym.unicode & 0xff80) == 0) {
					if (keybufw2 != keybufr) //stick ASCII code in last FIFO value
						keybuf[(keybufw2-1)&(KEYBUFSIZ-1)] |= (ev.key.keysym.unicode & 0x7f);
				}
				break;
			case SDL_KEYUP:
				sc = keytranslation[ev.key.keysym.sym];
				keystatus[ sc ] = 0;   // FIXME: verify this is kosher
				ext_keystatus[ sc ] &= ~1; // preserve bit 2 only
				switch (ev.key.keysym.sym) {
					case SDLK_LSHIFT: shkeystatus &= ~(1<<16); break;
					case SDLK_RSHIFT: shkeystatus &= ~(1<<17); break;
					case SDLK_LCTRL:  shkeystatus &= ~(1<<18); break;
					case SDLK_RCTRL:  shkeystatus &= ~(1<<19); break;
					case SDLK_LALT:   shkeystatus &= ~(1<<20); break;
					case SDLK_RALT:   shkeystatus &= ~(1<<21); break;
					default:
						break;
				}
				break;
#ifndef NOINPUT
			// FIXME: mousewheel?
				//if (zdelta > 0) ext_mbstatus[6] = 2;
				//else if (zdelta < 0) ext_mbstatus[7] = 2;
			case SDL_MOUSEMOTION:
	//      if (gpMouse && ActiveApp && setmousein) {
	//         mouse_acquire = 1;
	//         gpMouse->Acquire(); gbstatus = 0;
	//         setmousein(mx, my);
	//         setmousein = NULL;
	//      }
				if (ActiveApp) {
					mousex += ev.motion.xrel;
					mousey += ev.motion.yrel;
				}
				break;

			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				{
					int i=-1;
					switch (ev.button.button) {
						case SDL_BUTTON_LEFT:   i = 0; break;
						case SDL_BUTTON_RIGHT:  i = 1; break;
						case SDL_BUTTON_MIDDLE: i = 2; break;
					}
					if (i<0) break;
					
					if (ev.type == SDL_MOUSEBUTTONDOWN) {
						ext_mbstatus[i] = 1|2;
						gbstatus |= 1<<i;
					} else {
						ext_mbstatus[i] &= 2;
						gbstatus &= ~(1<<i);
					}
				}
				break;
#endif
			case SDL_QUIT:
				kensoundclose();
				SDL_Quit();
				uninitapp();
				quitprogram = 1;
				break;
		}
	}
}

long keyread ()
{
	long i;

	if (keybufr == keybufw) return(0);
	i = keybuf[keybufr]; keybufr = ((keybufr+1)&(KEYBUFSIZ-1));
	return(i);
}

void breath ()
{
	sdlmsgloop();
#ifndef NOSOUND
	ENTERMUTX;
	kensoundbreath(MINBREATHREND);
	LEAVEMUTX;
#endif
}

	//Call like this: arg2filename(argv[1],".ksm",curfilename);
	//Make sure curfilename length is lon(wParam&255)g enough!
static void strlwr(char *s)
{
		for ( ; *s; s++)
				if ((*s) >= 'A' || (*s) <= 'Z')
						*s &= ~0x20;
}
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

	//Precision: bits 8-9:, Rounding: bits 10-11:
	//00 = 24-bit    (0)   00 = nearest/even (0)
	//01 = reserved  (1)   01 = -inf         (4)
	//10 = 53-bit    (2)   10 = inf          (8)
	//11 = 64-bit    (3)   11 = 0            (c)
static long fpuasm[2] asm("fpuasm");
void fpuinit (long a)
{
	__asm__ __volatile__ (
		"fninit; fstcw fpuasm; andb $240, fpuasm(,1); orb %%al, fpuasm(,1); fldcw fpuasm;"
		: : "a" (a) : "memory","cc");
}


int main(int argc, char **argv)
{
	Uint32 sdlinitflags;
	int i;
	
	cputype = getcputype();
	if (cputype&((1<<0)+(1<<4)) != ((1<<0)+(1<<4)))
		{ fputs("Sorry, this program requires FPU&RDTSC support (>=Pentium)", stderr); return(-1); }
	
	sdlinitflags = SDL_INIT_TIMER;
#ifndef NOINPUT
	for(i=0;i<256;i++) keystatus[i] = 0;
	for(i=0;i<256;i++) ext_keystatus[i] = 0;
#endif
#ifndef NODRAW
	sdlinitflags |= SDL_INIT_VIDEO;
#endif
#ifndef NOSOUND
	sdlinitflags |= SDL_INIT_AUDIO;
#endif

	if (SDL_Init(sdlinitflags) < 0) { fputs("Failure initialising SDL.",stderr); return -1; }
	atexit(SDL_Quit);   // In case we exit() somewhere
	
	initklock();

	if (initapp(argc, argv) < 0) return -1;

#ifndef NODRAW
	if (!initdirectdraw(xres,yres,colbits))
	{
		SDL_Quit();
		return 0;
	}
#endif

#ifndef NOSOUND
	kensoundinit(44100,2,2);
#endif

	breath();
	while (!quitprogram)
	{
		if (alwaysactive || ActiveApp) { fpuinit(0x2); doframe(); }
		else SDL_WaitEvent(NULL);
		breath();
	}

	return quitparam;
}

/*
 * vim:ts=4:
 */
