#ifndef KSOUND_H
#define KSOUND_H

//Sound:
#define KSND_3D 1 //Use LOGICAL OR (|) to combine flags
#define KSND_MOVE 2
#define KSND_LOOP 4
#define KSND_LOPASS 8
#define KSND_MEM 16

/** @note Ken Silverman knows how to use EMMS */
#if defined(_MSC_VER) && !defined(NOASM)
	#pragma warning(disable:4799)
#endif
#ifndef ENTERMUTX
    #define ENTERMUTX LockAudio()
    /* a do nothing stub, which will not do nothing if SDL is used */
    void LockAudio(){}
#endif
#ifndef LEAVEMUTX
    #define LEAVEMUTX UnlockAudio()
    /* a do nothing stub, which will not do nothing if SDL is used */
    void UnlockAudio(){}
#endif

#if defined(_MSC_VER) //data definition is built into windows
typedef struct tWAVEFORMATEX
#else
typedef struct __attribute__ ((packed)) tWAVEFORMATEX
#endif
{
	short wFormatTag;
	short nChannels;
	long nSamplesPerSec;
	long nAvgBytesPerSec;
	short nBlockAlign;
	short wBitsPerSample;
	short cbSize;
} WAVEFORMATEX;

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

typedef struct { float x, y, z; } point3d;

/** For sounds longer than SNDSTREAMSIZ */
typedef struct
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

	//Internal streaming buffer variables:
#define SNDSTREAMSIZ 16384 //(16384) Keep this a power of 2!
#define MINBREATHREND 256  //(256) Keep this high to avoid clicks, but much smaller than SNDSTREAMSIZ!
#define LSAMPREC 10        //(11) No WAV file can have more than 2^(31-LSAMPREC) samples
#define MAXPLAYINGSNDS 256 //(256)
#define VOLSEPARATION 1.f  //(1.f) Range: 0 to 1, .5 good for headphones, 1 good for speakers
#define EARSEPARATION 8.f  //(8.f) Used for L/R sample shifting
#define SNDMINDIST 32.f    //(32.f) Shortest distance where volume is max / volume scale factor
#define SNDSPEED 4096.f    //(4096.f) Speed of sound in units per second
#define NUMCOEF 4          //(4) # filter coefficients (4 and 8 are good choices) Affects audio quality
#define LOGCOEFPREC 5      //(5) number of fractional bits of precision: 5 is ok
#define COEFRANG 32757     //(32757) DON'T make this > 32757 (not a typo) or else it'll overflow
#define KSND_3D 1
#define KSND_MOVE 2
#define KSND_LOOP 4
#define KSND_LOPASS 8
#define KSND_MEM 16
#define KSND_LOOPFADE 32            //for internal use only
#define WAVE_FORMAT_PCM 1
#define PI 3.14159265358979323
#define KS_OK 0
#define KS_NOTOK -1
#define AUDHASHINITSIZE 8192
#define AUDHASHEADSIZE 256          //must be power of 2, used by audcalchash
#define KSBLOCK_FROMWRITECURSOR 1   // use by ksoundbufferlock
#define KSBLOCK_ENTIREBUFFER 2      // use by ksoundbufferlock
#define MAXUMIXERS 256
#define UMIXERBUFSIZ 65536

static rendersndtyp rendersnd[MAXPLAYINGSNDS];
static long numrendersnd = 0;
KSoundBuffer *streambuf = 0;
//format: (used by audplay* to cache filenames&files themselves)
//[index to next hashindex or -1][index to last filnam][ptr to snd_buf][char snd_filnam[?]\0]
static char *audhashbuf = 0;
static long audhashead[AUDHASHEADSIZE], audhashpos, audlastloadedwav, audhashsiz;
static point3d audiopos, audiostr, audiohei, audiofor;
static float rsamplerate;
static long lsnd[SNDSTREAMSIZ>>1], samplerate, numspeakers, bytespersample, oplaycurs = 0;
static char gshiftval = 0;
static short __ALIGN(16) coef[NUMCOEF<<LOGCOEFPREC]; //Sound re-scale filter coefficients
static short __ALIGN(16) coeflopass[NUMCOEF<<LOGCOEFPREC]; //Sound re-scale filter coefficients
static long umixernum = 0;

typedef struct
{
	KSoundBuffer *streambuf;
	long samplerate, numspeakers, bytespersample, oplaycurs;
	void (*mixfunc)(void *dasnd, long danumbytes);
} umixertyp;
static umixertyp umixer[MAXUMIXERS];


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


int KSoundBuffer_Lock(KSoundBuffer *ksb, long wroffs, long wrlen,
		void **ptr1, unsigned long *len1, void **ptr2, unsigned long *len2, long flags)
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
		wrlen = MIN(ksb->buflen, wrlen);
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

int KSoundBuffer_GetCurrentPosition(KSoundBuffer *ksb, unsigned long *play, unsigned long *write)
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

/** Same as: stricmp(st0,st1) except: '/' == '\' @note case insensitive
 *  @param st0 String to compare
 *  @param st1 String to compare
 *  @return -1 or 0
*/
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

/** Make a hash of an audio filename so we can load from memory in future calls
 *  @param *st Pointer to string that is being hashed
 *  @return Pointer to position of item in hash
*/
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

/** Make sure string fits in audhashbuf */
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

/** Generate polynomial filter - fewer divides algo. (See PIANO.C for derivation) */
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
		f3 = COEFRANG/f3; /** @note Enabling this code makes it LESS continuous! */
		for(j=0;j<NUMCOEF;j++) coeflopass[i*NUMCOEF+j] = (short)(fcoef[j]*f3);
	}
}

/** Render samples
 *  @param ssnd source sound
 *  @param ispos sub-sample counter
 *  @param isinc sub-sample increment per destination sample
 *  @param ivolsc 31-bit volume scale
 *  @param ivolsci 31-bit ivolsc increment per destination sample
 *  @param lptr 32-bit sound pointer (step by 8 because rendersamps only renders 1 channel!)
 *  @param nsamp number of destination samples to render
 */
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

/** Copy an audio clip from lptr to dptr of length nsamp
 *  @param lptr 32-bit sound pointer
 *  @param dptr 16-bit destination pointer
 *  @param nsamp number of destination samples to render
 */
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
		#if defined(__GNUC__)
		__asm__ __volatile__ (
			"testl $4, %%edx\n"
			"leal (%%edx,%%ecx,4), %%edx\n"
			"leal (%%eax,%%ecx,8), %%eax\n"
			"jz 0f\n"   // skipc
			"negl %%ecx\n"
			"movq (%%eax,%%ecx,8), %%mm0\n"
			"packssdw %%mm0, %%mm0\n"
			"movd %%mm0, (%%edx,%%ecx,4)\n"
			"addl $2, %%ecx\n"
			"jg 3f\n"   // endc
			"jz 2f\n"   // skipd
			"jmp 1f\n"   // begc1
			"0:\nnegl %%ecx\n"   // skipc:
			"addl $1, %%ecx\n"
			"1:\nmovq -8(%%eax,%%ecx,8), %%mm0\n"   // begc1:
			"packssdw (%%eax,%%ecx,8), %%mm0\n"
			"movq %%mm0, -4(%%edx,%%ecx,4)\n"
			"addl $2, %%ecx\n"
			"jl 1b\n"   // begc1
			"jg 3f\n"   // endc
			"2:\nmovq -8(%%eax), %%mm0\n"   // skipd:
			"packssdw %%mm0, %%mm0\n"
			"movd %%mm0, -4(%%edx)\n"
			"3:\n"   // endc:
			: "+a" (lptr), "+d" (dptr), "+c" (nsamp) : : "memory","cc"
		);
		#elif defined(_MSC_VER)
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
		skipc:
			neg ecx
			add ecx, 1
		begc1:
			movq mm0, [eax+ecx*8-8]
			packssdw mm0, [eax+ecx*8]
			movq [edx+ecx*4-4], mm0
			add ecx, 2
			jl short begc1
			jg short endc
		skipd:
			movq mm0, [eax-8]
			packssdw mm0, mm0
			movd [edx-4], mm0
		endc:
		}
		#endif
#endif
	}
}

/** Set listener location (for 3D sounds)
 *  Position unit vector
 *  @param iposx
 *  @param iposy
 *  @param iposz
 *
 *  FORWARD unit vector of listener
 *  @param iposx
 *  @param iposy
 *  @param iposz
 *
 *  DOWN unit vector of listener
 *  @param iheix
 *  @param iheiy
 *  @param iheiz
 */
void setears3d (float iposx, float iposy, float iposz,
					 float iforx, float ifory, float iforz,
					 float iheix, float iheiy, float iheiz)
{
	float f;
	ENTERMUTX; /** @note do nothing unless SDL is the build type */
	f = 1.f/sqrt(iheix*iheix+iheiy*iheiy+iheiz*iheiz); //Make audiostr same magnitude as audiofor
	audiopos.x = iposx; audiopos.y = iposy; audiopos.z = iposz;
	audiofor.x = iforx; audiofor.y = ifory; audiofor.z = iforz;
	audiohei.x = iheix; audiohei.y = iheiy; audiohei.z = iheiz;
	audiostr.x = (iheiy*iforz - iheiz*ifory)*f;
	audiostr.y = (iheiz*iforx - iheix*iforz)*f;
	audiostr.z = (iheix*ifory - iheiy*iforx)*f;
	LEAVEMUTX; /** @note do nothing unless SDL is the build type */
}

/** @note Because of 3D position calculations, it is better to render sound in sync with the movement
 *  and not at random times. In other words, call kensoundbreath with lower "minleng" values
 *  when calling from breath() than from other places, such as kensoundthread()
 */
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
				volsci0 = (long)((1.f-MAX(g*VOLSEPARATION,0))*h);
				volsci1 = (long)((1.f+MIN(g*VOLSEPARATION,0))*h);
				volsci0 = (volsci0-rendersnd[j].ivolsc0)/n;
				volsci1 = (volsci1-rendersnd[j].ivolsc1)/n;

					//ispos? = ispos + (f voxels)*(.00025sec/voxel) * (isinc*samplerate subsamples/sec);
				m = rendersnd[j].isinc*samplerate;
				k = rendersnd[j].isinc*n + rendersnd[j].ispos;
				h = MAX((f-SNDMINDIST)+g*(EARSEPARATION*.5f),0); nsinc0 = k - mulshr16((long)(h*(65536.f/SNDSPEED)),m);
				h = MAX((f-SNDMINDIST)-g*(EARSEPARATION*.5f),0); nsinc1 = k - mulshr16((long)(h*(65536.f/SNDSPEED)),m);
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
			if (cputype&(1<<23)) clearMMX(); //MMX
		}
		for(m=0;m<2;m++) if (w[m]) audclipcopy(lptr[m],(short *)w[m],l[m]>>gshiftval);
		if (cputype&(1<<23)) clearMMX(); //MMX
		//streambuf->Unlock(w[0],l[0],w[1],l[1]);
	}

	oplaycurs = playcurs;
}

/** Returns pointer to sound data; loads file if not already loaded. */
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

		if (!(newsnd = (long)malloc(16+i+8)))
#ifndef USEKZ
		{ fclose(fil); return(0); }
		fread((void *)(newsnd+16),leng,1,fil);
		fclose(fil);
#else
		{ kzclose(); return(0); }
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

/** Play a sound
 *  filnam: ASCIIZ string of filename (can be inside .ZIP file if USEKZ is enabled)
 *  Filenames are compared with a hash, and samples are cached, so don't worry about speed!
 *  volperc: volume scale (0 is silence, 100 is max volume)
 *  frqmul: frequency multiplier (1.0 is no scale)
 *      pos: if 0, then doesn't use 3D, if nonzero, this is pointer to 3D position
 *  flags: bit 0: 1=3D sound, 0=simple sound
 *      bit 1: 1=dynamic 3D position update (from given pointer), 0=disable (ignored if !pos)
 *      bit 2: 1=loop sound (use playsoundupdate to disable, for non 3D sounds use dummy point3d!)
 *
 *       Valid values for flags:
 *           flags=0: 0           flags=4: KSND_LOOP
 *           flags=1: KSND_3D     flags=5: KSND_LOOP|KSND_3D
 *           flags=3: KSND_MOVE   flags=7: KSND_LOOP|KSND_MOVE
 *
 *        NOTE: When using flags=4 (KSND_LOOP without KSND_3D or KSND_MOVE), you can pass a unique
 *               non-zero dummy pointer pos to playsound. This way, you can use playsoundupdate() to stop
 *              the individual sound. If you pass a NULL pointer, then only way to stop the sound is
 *              by stopping all sounds.
 */
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
		ivolsc0 = (long)((1.f-MAX(g*VOLSEPARATION,0))*h);
		ivolsc1 = (long)((1.f+MIN(g*VOLSEPARATION,0))*h);
			//ispos? = ispos + (f voxels)*(.00025sec/voxel) * (isinc*samplerate subsamples/sec);
		m = isinc*samplerate;
		h = MAX((f-SNDMINDIST)+g*(EARSEPARATION*.5f),0); ispos0 = -mulshr16((long)(h*(65536.f/SNDSPEED)),m);
		h = MAX((f-SNDMINDIST)-g*(EARSEPARATION*.5f),0); ispos1 = -mulshr16((long)(h*(65536.f/SNDSPEED)),m);
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
	if (cputype&(1<<23)) clearMMX(); //MMX
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

/** Modify a sound already in use
 *	Use this function to update a pointer location (if you need to move things around in memory)
 *	special cases: if (optr== 0) changes all pointers
 *	               if (nptr== 0) changes KSND_MOVE to KSND_3D
 *	               if (nptr==-1) changes KSND_MOVE to KSND_3D and stops sound (using KSND_LOOPFADE)
 *	examples: playsoundupdate(&spr[delete_me].p,&spr[--numspr].p); //update pointer location
 *            playsoundupdate(&spr[just_before_respawn],0);        //stop position updates from pointer
 *	          playsoundupdate(&my_looping_sound,(point3d *)-1);    //turn off a looping sound
 *	          playsoundupdate(0,0);                                //stop all position updates
 *	          playsoundupdate(0,(point3d *)-1);                    //turn off all sounds
 */
void playsoundupdate (void *optr, void *nptr)
{
	long i;
	ENTERMUTX;  /** @note Do nothing unless using SDL */
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
	LEAVEMUTX; /** @note Do nothing unless using SDL */
}

#endif //KSOUND_H
