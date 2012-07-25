/**************************************************************************************************
 * porthacks.h: Macros-out differences between GCC & VS, and GNUC & MSVC                          *
 **************************************************************************************************/

/**
 * Compiler Directive Hacks
 **/

#ifdef __GNUC__
	// Maps __assume() to __builtin_unreachable()
	#define __assume(cond) do { if (!(cond)) __builtin_unreachable(); } while (0)
#endif

#ifdef _MSC_VER
	// Aligns symbol
	#define __ALIGN(num) __declspec(align(num))
	#ifndef __cplusplus
		#define inline __inline
	#endif
#endif

/**
 * Inline Assembly Syntax Hacks
 **/

static inline void clearMMX () // inserts opcode emms, used to avoid many compiler checks
{
	#ifdef _MSC_VER
	_asm { emms }
	#endif
	#ifdef __GNUC__
	__asm__ __volatile__ ("emms" : : : "cc");
	#endif
}

/**
 * Standard Library Hacks
 **/

#if defined(__GNUC__) && !(defined(__MINGW32__) || defined(__MINGW64__))
#include <ctype.h>
#include <limits.h>

/* Originally by Jim Meyering.  */
/* GPL 2 or later  */
static int memcasecmp (const void *voidptr1, const void *voidptr2, size_t n)
{
	size_t i;
	const char *ch1 = (const char*)(voidptr1);
	const char *ch2 = (const char*)(voidptr2);
	for (i = 0; i < n; i++)
		{
			unsigned char u1 = ch1[i];
			unsigned char u2 = ch2[i];
			int U1 = toupper (u1);
			int U2 = toupper (u2);
			int diff = (UCHAR_MAX <= INT_MAX ? U1 - U2
				: U1 < U2 ? -1 : U2 < U1);
			if (diff)
			return diff;
		}
	return 0;
}

#endif

#if (defined(_WIN32) || defined(_WINDOWS_)) || (defined(__MINGW32__) || defined(__MINGW64__))

#define strcasecmp _stricmp
#define memcasecmp _memicmp

#endif

/**
 * Visual Studio Type  Hacks
 **/


#if defined(_MSC_VER) && _MSC_VER<1600 //if Visual studio before 2010
typedef          __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef          __int64 int64_t;
typedef unsigned __int64 uint64_t;

#else
#include <stdint.h>
#endif


/**
 * Usefully macros
 **/
#if __cplusplus
	#define EXTERN_C extern "C"
#else
	#define EXTERN_C extern
#endif

#define COSSIN(degree, cos_, sin_) \
    do \
    { \
        sin_ = sin(degree); \
        cos_ = cos(degree); \
    } while(0)

#define MIN(a, b)  (((a) < (b)) ? (a) : (b))

#define MAX(a, b)  (((a) > (b)) ? (a) : (b))

#define BOUND(value, min, max)  ((value)>(max)?(max):((value)<(min)?(min):(value)))

#define ANTIBOUND(value, min, max) ((value)>((max)+(min)/2)?((value)>(max)?(value):(max)):((value)<(min)?(value):(min)))