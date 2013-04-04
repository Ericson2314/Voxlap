/**
 *  Moved this CPU / FPU code here 'cause it's generic, and doesn't just belong to just SDLMain or Winmain
 */

#include "sysmain.h"

//======================== CPU detection code begins ========================

//static long cputype asm("cputype") = 0; // What the hell? Doesn't match header or use; doesn't make much sense on it's own.
long cputype = 0; //I think this will work just fine.
/**
 *  Test specific processor flags
*/
static inline long testflag (long c)
{
	#ifdef __GNUC__ //gcc inline asm
	long a;
	__asm__ __volatile__ (
		"pushf\npopl %%eax\nmovl %%eax, %%ebx\nxorl %%ecx, %%eax\npushl %%eax\n"
		"popf\npushf\npopl %%eax\nxorl %%ebx, %%eax\nmovl $1, %%eax\njne 0f\n"
		"xorl %%eax, %%eax\n0:"
		: "=a" (a) : "c" (c) : "ebx","cc" );
	return a;
	#endif
	#ifdef _MSC_VER //msvc inline asm
	_asm
	{
		mov ecx, c
		pushfd
		pop eax
		mov edx, eax
		xor eax, ecx
		push eax
		popfd
		pushfd
		pop eax
		xor eax, edx
		mov eax, 1
		jne menostinx
		xor eax, eax
		menostinx:
	}
	#endif
}

static inline void cpuid (long a, long *s)
{
	#ifdef __GNUC__ //gcc inline asm
	__asm__ __volatile__ (
		"cpuid\nmovl %%eax, (%%esi)\nmovl %%ebx, 4(%%esi)\n"
		"movl %%ecx, 8(%%esi)\nmovl %%edx, 12(%%esi)"
		: "+a" (a) : "S" (s) : "ebx","ecx","edx","memory","cc");
	#endif
	#ifdef _MSC_VER //msvc inline asm
	_asm
	{
		push ebx
		push esi
		mov eax, a
		cpuid
		mov esi, s
		mov dword ptr [esi+0],  eax
		mov dword ptr [esi+4],  ebx
		mov dword ptr [esi+8],  ecx
		mov dword ptr [esi+12], edx
		pop esi
		pop ebx
	}
	#endif
}

/**
 *  Bit numbers of return value:
 *	0:FPU, 4:RDTSC, 15:CMOV, 22:MMX+, 23:MMX, 25:SSE, 26:SSE2, 30:3DNow!+, 31:3DNow!
*/
static long getcputype ()
{
	long i, cpb[4], cpid[4];
	if (!testflag(0x200000)) return(0);
	cpuid(0,cpid); if (!cpid[0]) return(0);
	cpuid(1,cpb); i = (cpb[3]&~((1<<22)|(1<<30)|(1<<31)));
	cpuid(0x80000000,cpb);
	if (((unsigned long)cpb[0]) > 0x80000000)
	{
		cpuid(0x80000001,cpb);
		i |= (cpb[3]&(1<<31));
		if (!((cpid[1]^0x68747541)|(cpid[3]^0x69746e65)|(cpid[2]^0x444d4163))) //AuthenticAMD
			i |= (cpb[3]&((1<<22)|(1<<30)));
	}
	if (i&(1<<25)) i |= (1<<22); //SSE implies MMX+ support
	return(i);
}

//========================= CPU detection code ends =========================

/**
 *  Precision: bits 8-9:, Rounding: bits 10-11:
 *	00 = 24-bit    (0)   00 = nearest/even (0)
 *	01 = reserved  (1)   01 = -inf         (4)
 *	10 = 53-bit    (2)   10 = inf          (8)
 *	11 = 64-bit    (3)   11 = 0            (c)
*/
static long fpuasm[2];
static inline void fpuinit (long a)
{
	#ifdef __GNUC__ //gcc inline asm
	__asm__ __volatile__
	(
		"fninit\n"
		"fstcww %c[fp]\n"
		"andb   $240, %c[fp]+1(,1)\n"
		"orb    %%al, %c[fp]+1(,1)\n"
		"fldcww %c[fp]\n"
		:
		: "a" (a), [fp] "p" (fpuasm)
		: "cc"
	);
	#endif
	#ifdef _MSC_VER //msvc inline asm
	_asm
	{
		mov	eax, a
		fninit
		fstcw	fpuasm
		and	byte ptr fpuasm[1], 0f0h
		or	byte ptr fpuasm[1], al
		fldcw	fpuasm
	}
	#endif
}

void code_rwx_unlock ( void * dep_protect_start, void * dep_protect_end)
{
#ifdef _WIN32
#ifndef PAGE_EXECUTE_READWRITE
#define PAGE_EXECUTE_READWRITE 0x40
#endif
	unsigned long oldprotectcode;
	VirtualProtect((void *)dep_protect_start, ((size_t)dep_protect_end - (size_t)dep_protect_start), PAGE_EXECUTE_READWRITE, &oldprotectcode);
#else
	size_t floorptr = (size_t)dep_protect_start & -sysconf(_SC_PAGE_SIZE);
	mprotect((void *)floorptr, ((size_t)dep_protect_end - (size_t)floorptr), PROT_READ|PROT_WRITE);
#endif
}
