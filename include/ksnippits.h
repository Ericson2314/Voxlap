/**************************************************************************************************
 * ksnippits.h: Bit's of inline assembly Ken commonly used because the C compiler sucked          *
 **************************************************************************************************/

#pragma once

	//Ericson2314's dirty porting tricks
#include "porthacks.h"

#ifdef __WATCOMC__

void clearMMX ();
#pragma aux emms =\
	".686"\
	"emms"\
	parm nomemory []\
	modify exact []\
	value

#else

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

static inline void fcossin (float a, float *c, float *s)
{
	*c = cosf(a);
	*s = cosf(a);
}

static inline void dcossin (double a, double *c, double *s)
{
	*c = cos(a);
	*s = sin(a);
}

static inline void ftol (float f, long *a)
{
	*a = (long) f;
}

static inline void dtol (double d, long *a)
{
	*a = (long) d;
}


static inline double dbound (double d, double dmin, double dmax)
{
	return BOUND(d, dmin, dmax);
}

static inline long mulshr16 (long a, long d)
{
	return (long)(((int64_t)a * (int64_t)d) >> 16);
}

static inline long mulshr24 (long a, long d)
{
	return (long)(((int64_t)a * (int64_t)d) >> 24);
}

static inline long mulshr32 (long a, long d)
{
	return (long)(((int64_t)a * (int64_t)d) >> 32);
}

static inline int64_t mul64 (long a, long d)
{
	return (int64_t)a * (int64_t)d;
}

static inline long shldiv16 (long a, long b)
{
	return (long)(((int64_t)a << 16) / (int64_t)b);
}

static inline long isshldiv16safe (long a, long b)
{
	return ((uint32_t)((-abs(b) - ((-abs(a)) >> 14)))) >> 31;
}

static inline long umulshr32 (long a, long d)
{
	return (long)(((uint64_t)a * (uint64_t)d) >> 32);
}

static inline long scale (long a, long d, long c)
{
	return (long)((int64_t)a * (int64_t)d / (int64_t)c);
}

static inline long dmulshr0 (long a, long d, long s, long t)
{
	return (long)((int64_t)a*(int64_t)d + (int64_t)s*(int64_t)t);
}

static inline long dmulshr22 (long a, long b, long c, long d)
{
	return (long)(((((int64_t)a)*((int64_t)b)) + (((int64_t)c)*((int64_t)d))) >> 22);
}

static inline long dmulrethigh (long b, long c, long a, long d)
{
	return (long)(((int64_t)b*(int64_t)c - (int64_t)a*(int64_t)d) >> 32);
}

static inline void copybuf (void *s, void *d, long c)
{
	int i;
	for (i = 0;	i < c; i++)
		((long *)d)[i] = ((long *)s)[i];
}

static inline void clearbuf (void *d, long c, long a)
{
	int i;
	for (i = 0;	i < c; i++)
		((long *)d)[i] = a;
}

static inline unsigned long bswap (unsigned long a)
{
	#ifdef __GNUC__
	return __builtin_bswap32(a);
	#endif
	#ifdef _MSC_VER
	return _byteswap_ulong(a);
	#endif
}
