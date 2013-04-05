#ifndef KTIMER_H
#define KTIMER_H
/** Generic, high precision timer functions */
//================== Fast & accurate TIMER FUNCTIONS begins ==================
#include "porthacks.h"

static __int64 pertimbase, rdtimbase, nextimstep;
static double perfrq, klockmul, klockadd;

static MUST_INLINE uint64_t rdtsc64(void)
{
    #if defined(__GNUC__)
	uint64_t q;
	__asm__ __volatile__ ("rdtsc" : "=A" (q) : : "cc");
	return q;
	#elif defined(_MSC_VER)
    _asm rdtsc
    #endif
}

#if 0

#if defined(__WATCOMC__)
__int64 rdtsc64 ();
#pragma aux rdtsc64 = "rdtsc" value [edx eax] modify nomemory parm nomemory;

#elif defined(_MSC_VER)

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
#elif defined(__GNUC__)


#else
#error Fatal : No rdtsc64 function defined.
#endif

#endif //0
//=================== Fast & accurate TIMER FUNCTIONS ends ===================
#endif //KTIMER_H
