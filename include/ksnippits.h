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
		".intel_syntax prefix\n"
		"fsincos\n\t"
		"fstp	DWORD PTR [%[c]]\n\t"
		"fstp	DWORD PTR [%[s]]\n\t"
		".att_syntax prefix\n"
		:
		: "t" (a), [c] "r" (c), [s] "r" (s)
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
		".intel_syntax prefix\n"
		"fsincos\n\t"
		"fstp	QWORD PTR [%[c]]\n\t"
		"fstp	QWORD PTR [%[s]]\n\t"
		".att_syntax prefix\n"
		:
		: "t" (a), [c] "r" (c), [s] "r" (s)
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
		".intel_syntax prefix\n"
		"fstp	DWORD PTR [%[a]]\n\t"
		".att_syntax prefix\n"
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
		".intel_syntax prefix\n"
		"fstp	QWORD PTR [%[a]]\n\t"
		".att_syntax prefix\n"
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
		".intel_syntax noprefix\n"
		"fucomi	%[d], %[dmin]\n"      //if (d < dmin)
		"fcmovb	%[d], %[dmin]\n"      //    d = dmin;
		"fucomi	%[d], %[dmax]\n"      //if (d > dmax)
		"fcmovnb	%[d], %[dmax]\n"  //    d = dmax;
		"fucom	%[dmax]\n"
		".att_syntax prefix\n"
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
	return (long)((((int64_t)a) * ((int64_t)d)) >> 16);
	#else
	#ifdef __GNUC__ //gcc inline asm
	__asm__ __volatile__
	(
		".intel_syntax prefix\n"
		"imul	%[d]\n"
		"shrd	%[a], %%edx, 16\n"
		".att_syntax prefix\n"
		: [a] "=a" (a)
		:      "0" (a), [d] "r" (d)
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
	return (long)((((int64_t)a) * ((int64_t)d)) >> 24);
	#else
	#ifdef __GNUC__ //gcc inline asm
	__asm__ __volatile__
	(
		".intel_syntax prefix\n"
		"imul	%[d]\n"
		"shrd	%[a], %%edx, 24\n"
		".att_syntax prefix\n"
		: [a] "=a" (a)
		:      "0" (a), [d] "r" (d)
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
	return (long)((((int64_t)a) * ((int64_t)d)) >> 32);
	#else
	#ifdef __GNUC__ //gcc inline asm
	__asm__ __volatile__
	(
		"imull %%edx"
		: "+a" (a), "+d" (d)
		:
		: "cc"
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
	return ((int64_t)a) * ((int64_t)d);
	#else
	#ifdef __GNUC__ //gcc inline asm
	int64_t out64;
	__asm__ __volatile__
	(
		".intel_syntax prefix \n"
		"imul	%[d] \n"
		".att_syntax prefix \n"
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
	return (long)((((int64_t)a) << 16) / ((int64_t)b));
	#else
	#ifdef __GNUC__ //gcc inline asm
	__asm__ __volatile__
	(
		".intel_syntax prefix\n"
		"mov	%%edx, %[a]\n"
		"shl	%[a], 16\n"
		"sar	%%edx, 16\n"
		"idiv	%[b]\n" //dword ptr
		".att_syntax prefix\n"
		: [a] "=a" (a)
		:      "0" (a), [b] "r" (b)
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
	return ((uint32_t)(((-abs(b)) - ((-abs(a)) >> 14)))) >> 31;
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
	return (long)((((uint64_t)a) * ((uint64_t)d)) >> 32);
	#else
	#ifdef __GNUC__ //gcc inline asm
	long product;
	__asm__ __volatile__
	(
		".intel_syntax prefix\n"
		"mul	%[d]\n" //dword ptr
		".att_syntax prefix\n"
		: "=d" (product)
		:  "a" (a), [d] "r" (d)
		:
	);
	return product;
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
	return a * d / c;
	#else
	#ifdef __GNUC__ //gcc inline asm
	__asm__ __volatile__
	(
		".intel_syntax noprefix\n"
		"mov	eax, a\n"
		"imul	dword ptr d\n"
		"idiv	dword ptr c\n"
		".att_syntax prefix\n"
	);
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
	return a*d + s*t;
	#else
	#ifdef __GNUC__ //gcc inline asm
	__asm__ __volatile__
	(
		".intel_syntax prefix\n"
		"imul	%[d]\n"
		".att_syntax prefix\n"
		:    "=a" (a)
		: [a] "0" (a), [d] "r" (d)
		:
	);
	__asm__ __volatile__
	(
		".intel_syntax prefix\n"
		"imul	%[d]\n"
		".att_syntax prefix\n"
		:    "=a" (s)
		: [s] "0" (s), [d] "r" (t)
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
		".intel_syntax prefix\n"
		"imul	%[b]\n"
		".att_syntax prefix\n"
		:    "=a" (a),    "=d" (b)
		: [a] "0" (a), [b] "1" (b)
		:
	);
	__asm__ __volatile__
	(
		".intel_syntax prefix\n"
		"imul	%[c]\n"
		".att_syntax prefix\n"
		:    "=a" (c),    "=d" (d)
		: [c] "0" (c), [d] "1" (d)
		:
	);
	__asm__ __volatile__
	(
		".intel_syntax prefix\n"
		"add	%[c], %[a]\n"
		"adc	%[d], %[b]\n"
		"shrd	%[c], %[d], 22\n"
		".att_syntax prefix\n"
		:                              "=r" (c)
		: [a] "r" (a), [b] "r" (b), [c] "0" (c), [d] "r" (d)
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
	return (long)(((((int64_t)a)*((int64_t)b)) + (((int64_t)c)*((int64_t)d))) >> 32);
	#else
	#ifdef __GNUC__ //gcc inline asm
	__asm__ __volatile__
	(
		".intel_syntax prefix\n"
		"imul	%[d]\n"
		".att_syntax prefix\n"
		:    "=a" (a),    "=d" (d)
		: [a] "0" (a), [d] "1" (d)
		:
	);
	__asm__ __volatile__
	(
		".intel_syntax prefix\n"
		"imul	%[c]\n"
		".att_syntax prefix\n"
		:    "=a" (b),    "=d" (c)
		: [b] "0" (b), [c] "1" (c)
		:
	);
	__asm__ __volatile__
	(
		".intel_syntax prefix\n"
		"sub	%[b], %[a]\n"
		"sbb	%[c], %[d]\n"
		".att_syntax prefix\n"
		:                              "=r" (c)
		: [a] "r" (a), [b] "r" (b), [c] "0" (c), [d] "r" (d)
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
		".intel_syntax noprefix\n"
		"rep	movsd\n"
		".att_syntax prefix\n"
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
		".intel_syntax noprefix\n"
		"rep	stosd\n"
		".att_syntax prefix\n"
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
		".intel_syntax noprefix\n"
		"bswap	%[a]\n"
		".att_syntax prefix\n"
		:    "=r" (a)
		: [a] "0" (a)
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

#endif
