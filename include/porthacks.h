/**************************************************************************************************
 * porthacks.h: Macros-out differences between GCC & VS, and GNUC & MSVC                          *
 **************************************************************************************************/

/********************************
 * Inline Assembly Syntax Hacks *
 ********************************/

static inline void clearMMX () // inserts opcode emms, used to avoid many compiler checks
{
	#ifdef _MSC_VER
	_asm { emms }
	#endif
	#ifdef __GNUC__
	__asm__ __volatile__ ("emms" : : : "cc");
	#endif
}

/*
#ifdef __GNUC__ //AT&T SYNTAX ASSEMBLY
	#define STARTASMB() {__asm__ \
	( \
	.intel_syntax \
	}

	#define STARTASML "\t
	#define ENDASML \n"

	#define ENDASMB() {.at&t_syntax
	);
	}
#endif

#ifdef _MSC_VER //MASM SYNTAX ASSEMBLY
	#define STARTASMB() {_asm \
	\{ \
	}
	
	#define STARTASML
	#define ENDASML
	
	#define ENDASMB() {\}}
#endif
*/

/****************************
 * Compiler Directive Hacks *
 ****************************/

#ifdef __GNUC__
	// Maps __assume() to __builtin_unreachable()
	#define __assume(cond) do { if (!(cond)) __builtin_unreachable(); } while (0)
#endif

#ifdef _MSC_VER
#endif

/**************************
 * Standard Library Hacks *
 **************************/

#if defined(__GNUC__) && !( defined(_WIN32) || defined(_WINDOWS_) )
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

#define min(a, b)  (((a) < (b)) ? (a) : (b))
#define max(a, b)  (((a) > (b)) ? (a) : (b))

//-----------------------------------
// windows.h min() & max()

// END windows.h min() & max()
//-----------------------------------
	//Same as: stricmp(st0,st1) except: '/' == '\'
static int filnamcmp (const char *st0, const char *st1)
{
	int i;
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

#endif

#ifdef _MSC_VER
//#if defined(_WIN32) & !defined(__GNUC__) //maybe this is better

#define strcasecmp _stricmp
#define memcasecmp _memicmp

#endif


//Pastebin for assembly rewritting

/*

	#if defined(__NOASM__)
	#endif
	#if defined(__GNUC__) && !defined(__NOASM__) //AT&T SYNTAX ASSEMBLY
	__asm__ __volatile__
	(

	);
	#endif
	#if defined(_MSC_VER) && !defined(__NOASM__) //MASM SYNTAX ASSEMBLY
	_asm
	{

	}
	#endif

*/


