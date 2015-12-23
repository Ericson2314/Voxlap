/**************************************************************************************************
 * ksnippits.h: Bit's of inline assembly Ken commonly used because the C compiler sucked          *
 **************************************************************************************************/

#pragma once

	//Ericson2314's dirty porting tricks
#include "porthacks.h"

#ifdef __WATCOMC__

void ftol (float, long *);
#pragma aux ftol =\
	"fistp dword ptr [eax]"\
	parm [8087][eax]\

void dcossin (double, double *, double *);
#pragma aux dcossin =\
	"fsincos"\
	"fstp qword ptr [eax]"\
	"fstp qword ptr [ebx]"\
	parm [8087][eax][ebx]\

void clearbuf (void *, long, long);
#pragma aux clearbuf =\
	"rep stosd"\
	parm [edi][ecx][eax]\
	modify exact [edi ecx]\
	value

long mulshr24 (long, long);
#pragma aux mulshr24 =\
	"imul edx",\
	"shrd eax, edx, 24",\
	parm nomemory [eax][edx]\
	modify exact [eax edx]\
	value [eax]

long umulshr32 (long, long);
#pragma aux umulshr32 =\
	"mul edx"\
	parm nomemory [eax][edx]\
	modify exact [eax edx]\
	value [edx]

void clearMMX ();
#pragma aux emms =\
	".686"\
	"emms"\
	parm nomemory []\
	modify exact []\
	value

#else

static inline void fcossin (float a, float *c, float *s)
{
	#ifdef NOASM
	*c = cosf(a);
	*s = cosf(a);
	#else
	#ifdef __GNUC__ //gcc inline asm
	__asm__ __volatile__
	(
		"fsincos\n\t"
		: "=t" (*c), "=u" (*s)
		:  "0" (a)
		:
	);
	#endif
	#ifdef _MSC_VER //msvc inline asm
	_asm
	{
		fld a
		fsincos
		mov	eax, c
		fstp	dword ptr [eax]
		mov	eax, s
		fstp	dword ptr [eax]
	}
	#endif
	#endif
}

static inline void dcossin (double a, double *c, double *s)
{
	#ifdef NOASM
	*c = cos(a);
	*s = sin(a);
	#else
	#ifdef __GNUC__ //gcc inline asm
	__asm__ __volatile__
	(
		"fsincos\n\t"
		: "=t" (*c), "=u" (*s)
		:  "0" (a)
		:
	);
	#endif
	#ifdef _MSC_VER //msvc inline asm
	_asm
	{
		fld	a
		fsincos
		mov	eax, c
		fstp	qword ptr [eax]
		mov	eax, s
		fstp	qword ptr [eax]
	}
	#endif
	#endif
}

static inline void ftol (float f, long *a)
{
	#ifdef NOASM
	*a = (long) f;
	#else
	#ifdef __GNUC__ //gcc inline asm
	__asm__ __volatile__
	(
		"fistpl	(%[a])"
		: 
		: "t" (f), [a] "r" (a)
		:
	);
	#endif
	#ifdef _MSC_VER //msvc inline asm
	_asm
	{
		mov	eax, a
		fld	f
		fistp	dword ptr [eax]
	}
	#endif
	#endif
}

static inline void dtol (double d, long *a)
{
	#ifdef NOASM
	*a = (long) d;
	#else
	#ifdef __GNUC__ //gcc inline asm
	__asm__ __volatile__
	(
		"fistpl	(%[a])"
		: 
		: "t" (d), [a] "r" (a)
		:
	);
	#endif
	#ifdef _MSC_VER //msvc inline asm
	_asm
	{
		mov	eax, a
		fld	qword ptr d
		fistp	dword ptr [eax]
	}
	#endif
	#endif
}


static inline double dbound (double d, double dmin, double dmax)
{
	#ifdef NOASM
	return BOUND(d, dmin, dmax);
	#else
	//WARNING: This ASM code requires >= PPRO
	#ifdef __GNUC__ //gcc inline asm
	__asm__ __volatile__
	(
		"fucomi	%[dmin], %[d]\n"      //if (d < dmin)
		"fcmovb	%[dmin], %[d]\n"      //    d = dmin;
		"fucomi	%[dmax], %[d]\n"      //if (d > dmax)
		"fcmovnb	%[dmin], %[d]\n"  //    d = dmax;
		"fucom	%[dmax]\n"
		: [d] "=t" (d)
		:     "0"  (d), [dmin] "f" (dmin), [dmax] "f" (dmax)
		:
	);
	#endif
	#ifdef _MSC_VER //msvc inline asm
	_asm
	{
		fld	dmin
		fld	d
		fucomi	st, st(1)   //if (d < dmin)
		fcmovb	st, st(1)   //    d = dmin;
		fld	dmax
		fxch	st(1)
		fucomi	st, st(1)   //if (d > dmax)
		fcmovnb	st, st(1)   //    d = dmax;
		fstp	d
		fucompp
	}
	#endif
	return(d);
	#endif
}

static inline long mulshr16 (long a, long d)
{
	#ifdef NOASM
	return (long)(((int64_t)a * (int64_t)d) >> 16);
	#else
	#ifdef __GNUC__ //gcc inline asm
	__asm__ __volatile__
	(
		"imul	%[d]\n"
		"shrd	$16, %%edx, %[a]\n"
		: [a] "+a" (a)
		: [d]  "r" (d)
		: "edx"
	);
	return a;
	#endif
	#ifdef _MSC_VER //msvc inline asm
	_asm
	{
		mov	eax, a
		mov	edx, d
		imul	edx
		shrd	eax, edx, 16
	}
	#endif
	#endif
}

static inline long mulshr24 (long a, long d)
{
	#ifdef NOASM
	return (long)(((int64_t)a * (int64_t)d) >> 24);
	#else
	#ifdef __GNUC__ //gcc inline asm
	__asm__ __volatile__
	(
		"imul	%[d]\n"
		"shrd	$24, %%edx, %[a]\n"
		: [a] "+a" (a)
		: [d]  "r" (d)
		: "edx"
	);
	return a;
	#endif
	#ifdef _MSC_VER //msvc inline asm
	_asm
	{
		mov eax, a
		mov edx, d
		imul edx
		shrd eax, edx, 24
	}
	#endif
	#endif
}

static inline long mulshr32 (long a, long d)
{
	#ifdef NOASM
	return (long)(((int64_t)a * (int64_t)d) >> 32);
	#else
	#ifdef __GNUC__ //gcc inline asm
	__asm__ __volatile__
	(
		"imul %%edx"
		: "+d" (d)
		:  "a" (a)	
	);
	return d;
	#endif
	#ifdef _MSC_VER //msvc inline asm
	_asm
	{
		mov eax, a
		imul d
		mov eax, edx
	}
	#endif
	#endif
}

static inline int64_t mul64 (long a, long d)
{
	#ifdef NOASM
	return (int64_t)a * (int64_t)d;
	#else
	#ifdef __GNUC__ //gcc inline asm
	int64_t out64;
	__asm__ __volatile__
	(
		"imul	%[d] \n"
		: "=A" (out64)
		:  "a" (a),    [d] "r" (d)
		:
	);
	return out64;
	#endif
	#ifdef _MSC_VER //msvc inline asm
	_asm
	{
		mov	eax, a
		imul	d
	}
	#endif
	#endif
}

static inline long shldiv16 (long a, long b)
{
	#ifdef NOASM
	return (long)(((int64_t)a << 16) / (int64_t)b);
	#else
	#ifdef __GNUC__ //gcc inline asm
	__asm__ __volatile__
	(
		"mov	%[a], %%edx\n"
		"shl	$16, %[a]\n"
		"sar	$16, %%edx\n"
		"idiv	%[b]\n"
		: [a] "+a" (a)
		: [b] "r" (b)
		: "edx"
	);
	return a;
	#endif
	#ifdef _MSC_VER //msvc inline asm
	_asm
	{
		mov	eax, a
		mov	edx, eax
		shl	eax, 16
		sar	edx, 16
		idiv	b
	}
	#endif
	#endif
}

static inline long isshldiv16safe (long a, long b)
{
	#ifdef NOASM
	return ((uint32_t)((-abs(b) - ((-abs(a)) >> 14)))) >> 31;
	#else
	#ifdef __GNUC__ //gcc inline asm
	__asm__ __volatile__
	(
		".intel_syntax prefix\n"
		"test	%[a], %[a]\n"
		"js	short .Lskipneg0\n"
		"neg	%[a]\n"
	".Lskipneg0:\n"
		"sar	%[a], 14\n"
		
		"test	%[b], %[b]\n"
		"js	short .Lskipneg1\n"
		"neg	%[b]\n"
	".Lskipneg1:\n"
			//abs((a<<16)/b) < (1<<30) //1 extra for good luck!
			//-abs(a)>>14 > -abs(b)    //use -abs because safe for 0x80000000
			//eax-edx < 0
		"sub	%[b], %[a]\n"
		"shr	%[b], 31\n"
		".att_syntax prefix\n"
		:               [b] "=r" (b)
		: [a]  "r" (a),      "0" (b)
		: "cc"
	);
	return b;
	#endif
	#ifdef _MSC_VER //msvc inline asm
	_asm
	{
		mov	edx, a
		test	edx, edx
		js	short skipneg0
		neg	edx
	skipneg0:
		sar	edx, 14

		mov	eax, b
		test	eax, eax
		js	short skipneg1
		neg	eax
	skipneg1:
			//abs((a<<16)/b) < (1<<30) //1 extra for good luck!
			//-abs(a)>>14 > -abs(b)    //use -abs because safe for 0x80000000
			//eax-edx	< 0
		sub	eax, edx
		shr	eax, 31
	}
	#endif
	#endif
}

static inline long umulshr32 (long a, long d)
{
	#ifdef NOASM
	return (long)(((uint64_t)a * (uint64_t)d) >> 32);
	#else
	#ifdef __GNUC__ //gcc inline asm
	__asm__ __volatile__
	(
		"mul	%%edx\n" //dword ptr
		: "+d" (d)
		:  "a" (a)
		:
	);
	return d;
	#endif
	#ifdef _MSC_VER //msvc inline asm
	_asm
	{
		mov	eax, a
		mul	d
		mov	eax, edx
	}
	#endif
	#endif
}

static inline long scale (long a, long d, long c)
{
	#ifdef NOASM
	return (long)((int64_t)a * (int64_t)d / (int64_t)c);
	#else
	#ifdef __GNUC__ //gcc inline asm
	__asm__ __volatile__
	(
		"imul	%[d]\n"
		"idiv	%[c]\n"
		: "+a" (a)
		: [c] "r" (c), [d] "r" (d)
		:
	);
	return a;
	#endif
	#ifdef _MSC_VER //msvc inline asm
	_asm
	{
		mov	eax, a
		imul	d
		idiv	c
	}
	#endif
	#endif
}

static inline long dmulshr0 (long a, long d, long s, long t)
{
	#ifdef NOASM
	return (long)((int64_t)a*(int64_t)d + (int64_t)s*(int64_t)t);
	#else
	#ifdef __GNUC__ //gcc inline asm
	__asm__ __volatile__
	(
		"imul	%[d]\n"
		:    "+a" (a)
		: [d] "r" (d)
		:
	);
	__asm__ __volatile__
	(
		"imul	%[d]\n"
		:    "+a" (s)
		: [d] "r" (t)
		:
	);
	return a + s;
	#endif
	#ifdef _MSC_VER //msvc inline asm
	_asm
	{
		mov	eax, a
		imul	d
		mov	ecx, eax
		mov	eax, s
		imul	t
		add	eax, ecx
	}
	#endif
	#endif
}

static inline long dmulshr22 (long a, long b, long c, long d)
{
	#ifdef NOASM
	return (long)(((((int64_t)a)*((int64_t)b)) + (((int64_t)c)*((int64_t)d))) >> 22);
	#else
	#ifdef __GNUC__ //gcc inline asm
	__asm__ __volatile__
	(
		"imul	%[b]\n"
		:    "+a,a" (a), "=d,d" (b)
		: [b] "r,1" (b)
		:
	);
	__asm__ __volatile__
	(
		"imul	%[d]\n"
		:    "+a,a" (c), "=d,d" (d) 
		: [d] "r,1" (d)
		:
	);
	__asm__ __volatile__
	(
		"add	%[a], %[c]\n"
		"adc	%[b], %[d]\n"
		"shrd	$22,  %[d], %[c]\n"
		: [c] "+r" (c)
		: [a]  "r" (a), [b] "r" (b), [d] "r" (d)
		:
	);
	return c;
	#endif
	#ifdef _MSC_VER //msvc inline asm
	_asm
	{
		mov	eax, a
		imul	b
		mov	ecx, eax
		push	edx
		mov	eax, c
		imul	d
		add	eax, ecx
		pop	ecx
		adc	edx, ecx
		shrd	eax, edx, 22
	}
	#endif
	#endif
}

static inline long dmulrethigh (long b, long c, long a, long d)
{
	#ifdef NOASM
	return (long)(((int64_t)b*(int64_t)c - (int64_t)a*(int64_t)d) >> 32);
	#else
	#ifdef __GNUC__ //gcc inline asm
	__asm__ __volatile__
	(
		"imul	%[d]\n"
		:    "+a,a" (a), "=d,d" (d)
		: [d] "r,1" (d)
		:
	);
	__asm__ __volatile__
	(
		"imul	%[c]\n"
		:    "+a,a" (b), "=d,d" (c)
		: [c] "r,1" (c)
		:
	);
	__asm__ __volatile__
	(
		"sub	%[a], %[b]\n"
		"sbb	%[d], %[c]\n"
		: [c] "+r" (c)
		: [a]  "r" (a), [b] "r" (b), [d] "r" (d)
		:
	);
	return c;
	#endif
	#ifdef _MSC_VER //msvc inline asm
	_asm
	{
		mov	eax, a
		imul	d
		mov	ecx, eax
		push	edx
		mov	eax, b
		imul	c
		sub	eax, ecx
		pop	ecx
		sbb	edx, ecx
		mov	eax, edx
	}
	#endif
	#endif
}

static inline void copybuf (void *s, void *d, long c)
{
	#ifdef NOASM
	int i;
	for (i = 0;	i < c; i++)
		((long *)d)[i] = ((long *)s)[i];
	#else
	#ifdef __GNUC__ //gcc inline asm
	__asm__ __volatile__
	(
		"rep	movsl\n"
		:
		: "S" (s), "D" (d), "c" (c)
		:
	);
	#endif
	#ifdef _MSC_VER //msvc inline asm
	_asm
	{
		push	esi
		push	edi
		mov	esi, s
		mov	edi, d
		mov	ecx, c
		rep	movsd
		pop	edi
		pop	esi
	}
	#endif
	#endif
}

static inline void clearbuf (void *d, long c, long a)
{
	#ifdef NOASM
	int i;
	for (i = 0;	i < c; i++)
		((long *)d)[i] = a;
	#else
	#ifdef __GNUC__ //gcc inline asm
	__asm__ __volatile__
	(
		"rep	stosl\n"
		:
		: "D" (d), "c" (c), "a" (a)
		:
	);
	#endif
	#ifdef _MSC_VER //msvc inline asm
	_asm
	{
		push	edi
		mov	edi, d
		mov	ecx, c
		mov	eax, a
		rep	stosd
		pop	edi
	}
	#endif
	#endif
}

static inline unsigned long bswap (unsigned long a)
{
	#ifdef NOASM
	#ifdef __GNUC__
	return __builtin_bswap32(a);
	#endif
	#ifdef _MSC_VER
	return _byteswap_ulong(a);
	#endif
	#else
	#ifdef __GNUC__ //gcc inline asm
	__asm__ __volatile__
	(
		"bswap	%[a]\n"
		: [a] "+r" (a)
		:
		:
	);
	return a;
	#endif
	#ifdef _MSC_VER //msvc inline asm
	_asm
	{
		mov	eax, a
		bswap	eax
	}
	#endif
	#endif
}

static inline void clearMMX () // inserts opcode emms, used to avoid many compiler checks
{
	#ifdef __GNUC__
	__asm__ __volatile__ ("emms" : : : "cc");
	#endif
	#ifdef _MSC_VER
	_asm { emms }
	#endif
}

#endif
