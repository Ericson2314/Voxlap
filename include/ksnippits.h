/**************************************************************************************************
 * ksnippits.h: Bit's of inline assembly Ken commonly used because the C compiler sucked          *
 **************************************************************************************************/

	//Ericson2314's dirty porting tricks
#include "../include/porthacks.h"

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
	#ifdef __NOASM__
	*c = cosf(a);
	*s = cosf(a);
	#else
	#if __GNUC__ //AT&T SYNTAX ASSEMBLY
	__asm__ __volatile__
	(
		".intel_syntax noprefix\n"
		"fld	DWORD PTR a\n\t"
		"fsincos\n\t"
		"mov	eax, c\n\t"
		"fstp	DWORD PTR [eax]\n\t"
		"mov	eax, s\n\t"
		"fstp	DWORD PTR [eax]\n\t"
		".att_syntax prefix\n"
	);
	#endif
	#if _MSC_VER //MASM SYNTAX ASSEMBLY
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
	#ifdef __NOASM__
	*c = cos(a);
	*s = sin(a);
	#else
	#if defined(__GNUC__) && !defined(__NOASM__) //AT&T SYNTAX ASSEMBLY
	__asm__ __volatile__
	(
		".intel_syntax noprefix\n"
		"fld	qword ptr a\n"
		"fsincos\n"
		"mov	eax, c\n"
		"fstp	qword ptr [eax]\n"
		"mov	eax, s\n"
		"fstp	qword ptr [eax]\n"
		".att_syntax prefix\n"
	);
	#endif
	#if defined(_MSC_VER) && !defined(__NOASM__) //MASM SYNTAX ASSEMBLY
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
	#ifdef __NOASM__
	*a = (long) f;
	#else
	#if defined(__GNUC__) && !defined(__NOASM__) //AT&T SYNTAX ASSEMBLY
	__asm__ __volatile__
	(
		".intel_syntax prefix\n"
		"fistp	dword ptr [%[a]]\n"
		".att_syntax prefix\n"
		: 
		: [f]  "t" (f), [a] "r" (a)
		:
	);
	#endif
	#if defined(_MSC_VER) && !defined(__NOASM__) //MASM SYNTAX ASSEMBLY
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
	#ifdef __NOASM__
	*a = (long) d;
	#else
	#if defined(__GNUC__) && !defined(__NOASM__) //AT&T SYNTAX ASSEMBLY
	__asm__ __volatile__
	(
		".intel_syntax noprefix\n"
		"mov	eax, a\n"
		"fld	qword ptr d\n"
		"fistp	dword ptr [eax]\n"
		".att_syntax prefix\n"
	);
	#endif
	#if defined(_MSC_VER) && !defined(__NOASM__) //MASM SYNTAX ASSEMBLY
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
	#ifdef __NOASM__
	return BOUND(d, dmin, dmax);
	#else
	//WARNING: This ASM code requires >= PPRO
	#if defined(__GNUC__) && !defined(__NOASM__) //AT&T SYNTAX ASSEMBLY
	__asm__ __volatile__
	(
		".intel_syntax noprefix\n"
		"fld	qword ptr dmin\n"
		"fld	qword ptr d\n"
		"fucomi	st, st(1)\n"      //if (d < dmin)
		"fcmovb	st, st(1)\n"      //    d = dmin;
		"fld	qword ptr dmax\n"
		"fxch	st(1)\n"
		"fucomi	st, st(1)\n"      //if (d > dmax)
		"fcmovnb	st, st(1)\n"  //    d = dmax;
		"fstp	qword ptr d\n"
		"fucompp\n"
		".att_syntax prefix\n"
	);
	#endif
	#if defined(_MSC_VER) && !defined(__NOASM__) //MASM SYNTAX ASSEMBLY
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
	#endif
	return(d);
}

static inline long mulshr16 (long a, long d)
{
	#ifdef __NOASM__
	return (long)((((int64_t)a) * ((int64_t)d)) >> 16);
	#else
	#if defined(__GNUC__) && !defined(__NOASM__) //AT&T SYNTAX ASSEMBLY
	__asm__ __volatile__
	(
		".intel_syntax prefix\n"
		"imul	%[d]\n"
		"shrd	%[a], %[d], 16\n"
		".att_syntax prefix\n"
		: [a] "=a" (a)
		:      "0" (a), [d] "r" (d)
		:
	);
	return a;
	#endif
	#if defined(_MSC_VER) && !defined(__NOASM__) //MASM SYNTAX ASSEMBLY
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

static inline int64_t mul64 (long a, long d)
{
	#ifdef __NOASM__
	return ((int64_t)a) * ((int64_t)d);
	#else
	#if defined(__GNUC__) && !defined(__NOASM__) //AT&T SYNTAX ASSEMBLY
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
	#if defined(_MSC_VER) && !defined(__NOASM__) //MASM SYNTAX ASSEMBLY
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
	#ifdef __NOASM__
	return (long)((((int64_t)a) << 16) / ((int64_t)d));
	#else
	#if defined(__GNUC__) && !defined(__NOASM__) //AT&T SYNTAX ASSEMBLY
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
	#if defined(_MSC_VER) && !defined(__NOASM__) //MASM SYNTAX ASSEMBLY
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
	#ifdef __NOASM__
	
	#else
	#if defined(__GNUC__) && !defined(__NOASM__) //AT&T SYNTAX ASSEMBLY
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
		: [a] "=r" (b)
		:      "r" (a), [b] "0" (b)
		: "cc"
	);
	return b;
	#endif
	#if defined(_MSC_VER) && !defined(__NOASM__) //MASM SYNTAX ASSEMBLY
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

static inline long mulshr24 (long a, long d)
{
	#ifdef __GNUC__ //AT&T SYNTAX ASSEMBLY
	#endif
	#ifdef _MSC_VER //MASM SYNTAX ASSEMBLY
	_asm
	{
		mov eax, a
		mov edx, d
		imul edx
		shrd eax, edx, 24
	}
	#endif
}

static inline long umulshr32 (long a, long d)
{
	#ifdef __NOASM__
	
	#else
	#if defined(__GNUC__) && !defined(__NOASM__) //AT&T SYNTAX ASSEMBLY
	__asm__ __volatile__
	(
		".intel_syntax prefix\n"
		"mul	%[d]\n" //dword ptr
		"mov	%%eax, %%edx\n"
		".att_syntax prefix\n"
		: [a] "=a" (a)
		:      "0" (a), [d] "r" (d)
		: "edx"
	);
	return a;
	#endif
	#if defined(_MSC_VER) && !defined(__NOASM__) //MASM SYNTAX ASSEMBLY
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
	#ifdef __NOASM__
	return a * d / c;
	#else
	#if defined(__GNUC__) && !defined(__NOASM__) //AT&T SYNTAX ASSEMBLY
	__asm__ __volatile__
	(
		".intel_syntax noprefix\n"
		"mov	eax, a\n"
		"imul	dword ptr d\n"
		"idiv	dword ptr c\n"
		".att_syntax prefix\n"
	);
	#endif
	#if defined(_MSC_VER) && !defined(__NOASM__) //MASM SYNTAX ASSEMBLY
	_asm
	{
		mov	eax, a
		imul	d
		idiv	c
	}
	#endif
	#endif
}

static inline long dmulrethigh (long b, long c, long a, long d)
{
	#ifdef __NOASM__
	
	#else
	#if defined(__GNUC__) && !defined(__NOASM__) //AT&T SYNTAX ASSEMBLY
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
	#if defined(_MSC_VER) && !defined(__NOASM__) //MASM SYNTAX ASSEMBLY
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
	#ifdef __NOASM__
	int i;
	for (i = 0;	i < c; i++)
		((long *)d)[i] = ((long *)s)[i];
	#else
	#if defined(__GNUC__) && !defined(__NOASM__) //AT&T SYNTAX ASSEMBLY
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
	#if defined(_MSC_VER) && !defined(__NOASM__) //MASM SYNTAX ASSEMBLY
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
	#ifdef __NOASM__
	int i;
	for (i = 0;	i < c; i++)
		((long *)d)[i] = a;
	#else
	#if defined(__GNUC__) && !defined(__NOASM__) //AT&T SYNTAX ASSEMBLY
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
	#if defined(_MSC_VER) && !defined(__NOASM__) //MASM SYNTAX ASSEMBLY
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

#endif