/**************************************************************************************************
 * porthacks.h: Macros-out differences between GCC & VS, and GNUC & MSVC                          *
 **************************************************************************************************/

/**
 * Compiler Directive Hacks
 **/

#ifdef __GNUC__
	// Maps __assume() to __builtin_unreachable()
	#define __assume(cond) do { if (!(cond)) __builtin_unreachable(); } while (0)
	// Aligns symbol
	#define __ALIGN(num) __attribute__ ((align(num)))
	#define FORCE_NAME(symbol) asm(symbol)
#endif

#ifdef _MSC_VER
	// Aligns symbol
	#define __ALIGN(num) __declspec(align(num))
	#ifndef __cplusplus
		#define inline __inline
	#endif
	#define FORCE_NAME(symbol)
#endif

/**
 * Inline Assembly Syntax Hacks
 **/

static inline void clearMMX () // inserts opcode emms, used to avoid many compiler checks
{
	#ifdef __GNUC__
	__asm__ __volatile__ ("emms" : : : "cc");
	#endif
	#ifdef _MSC_VER
	_asm { emms }
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
static int memcasecmp (const void* ptr0, const void* ptr1, size_t n)
{
	size_t i;
	const char* ch0 = (const char*)(ptr0);
	const char* ch1 = (const char*)(ptr1);
	for (i = 0; i < n; i++)
	{
		//int Up0 = toupper((unsigned char)(((const char*)(ptr0))[i]));
		//int Up1 = toupper((unsigned char)(((const char*)(ptr1))[i]));
		int Up0 = toupper((unsigned char)(ch0[i]));
		int Up1 = toupper((unsigned char)(ch1[i]));
		int diff = (UCHAR_MAX <= INT_MAX ? Up0 - Up1
			: Up0 < Up1 ? -1 : Up1 < Up0);
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
