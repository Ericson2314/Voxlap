/**************************************************************************************************
 * porthacks.h: Macros-out differences between GCC & VS, and GNUC & MSVC                          *
 **************************************************************************************************/

#pragma once

/**
 * Compiler Directive Hacks
 **/

#if defined(_M_IX86) && !defined(__i386) //MSVC's way of declaring x86
	#define __i386__
#endif


#ifdef __GNUC__
	#define GCC_VERSION (__GNUC__ * 10000 \
		     + __GNUC_MINOR__ * 100 \
		     + __GNUC_PATCHLEVEL__)

	// Aligns symbol
	#define __ALIGN(num) __attribute__((aligned(num)))
	#define MUST_INLINE __attribute__((always_inline))
	#define FORCE_NAME(symbol) asm(symbol)

	//used to add a breakpoint
	#define DEBUG_BREAK() __asm__ __volatile__ ("int $3\n");

	//_gtfo marks dead code
	#if GCC_VERSION >= 40500
		#define _gtfo() __builtin_unreachable()
	#else
		#define _gtfo()
	#endif
#endif

#ifdef _MSC_VER
	// Aligns symbol
	#define __ALIGN(num) __declspec(align(num))
	#ifndef __cplusplus
		#define inline __inline
	#endif
	#define MUST_INLINE __forceinline
	#define FORCE_NAME(symbol)

	//used to add a breakpoint
	#define DEBUG_BREAK() _asm { int 3 }

	//_gtfo marks dead code
	#if _MSC_VER >= 1310
		#define _gtfo() __assume(0)
	#else
		#define _gtfo()
	#endif
#endif

/**
 * Standard Library Hacks
 **/

#if defined(__GNUC__) && !(defined(__MINGW32__) || defined(__MINGW64__))
#include <ctype.h>
#include <limits.h>
#include <stddef.h>


static int memcasecmp (const void * const ptr0, const void * const ptr1, size_t n)
{
	size_t i;
	int up0, up1;
	int diff = 0;
	for (i = 0; i < n; i++)
	{
		up0 = toupper(((const unsigned char* const)ptr0)[i]);
		up1 = toupper(((const unsigned char* const)ptr1)[i]);
		diff = UCHAR_MAX <= INT_MAX ? up0 - up1 : up0 < up1 ? -1 : up1 < up0;
		if (diff) break;
	}
	return diff;
}

#endif

#if (defined(_WIN32) || defined(_WINDOWS_)) || (defined(__MINGW32__) || defined(__MINGW64__))

#define strcasecmp _stricmp
#define memcasecmp _memicmp

#endif

/**
 * Visual Studio Type Hacks
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

#define COSSIN(degree, cos_, sin_)					\
	do								\
	{								\
		sin_ = sin(degree);					\
		cos_ = cos(degree);					\
	} while(0)

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#define BOUND(value, min, max) ((value)>(max)?(max):((value)<(min)?(min):(value)))

#define ANTIBOUND(value, min, max) ((value)>((max)+(min)/2)?((value)>(max)?(value):(max)):((value)<(min)?(value):(min)))
