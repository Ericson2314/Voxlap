	//Color arithemtic functions (used by voxlap itself and voxed)

#include "voxlap5.h"

EXTERN_VOXLAP int64_t flashbrival;

#ifdef __WATCOMC__

void mmxcoloradd (long *);
#pragma aux mmxcoloradd =\
	".686"\
	"movd mm0, [eax]"\
	"paddusb mm0, flashbrival"\
	"movd [eax], mm0"\
	parm nomemory [eax]\
	modify exact \
	value

void mmxcolorsub (long *);
#pragma aux mmxcolorsub =\
	".686"\
	"movd mm0, [eax]"\
	"psubusb mm0, flashbrival"\
	"movd [eax], mm0"\
	parm nomemory [eax]\
	modify exact \
	value

#else

static inline void mmxcoloradd (long *a)
{
	((uint8_t *)a)[0] += ((uint8_t *)flashbrival)[0];
	((uint8_t *)a)[1] += ((uint8_t *)flashbrival)[1];
	((uint8_t *)a)[2] += ((uint8_t *)flashbrival)[2];
	((uint8_t *)a)[3] += ((uint8_t *)flashbrival)[3];
}

static inline void mmxcolorsub (long *a)
{
	((uint8_t *)a)[0] -= ((uint8_t *)flashbrival)[0];
	((uint8_t *)a)[1] -= ((uint8_t *)flashbrival)[1];
	((uint8_t *)a)[2] -= ((uint8_t *)flashbrival)[2];
	((uint8_t *)a)[3] -= ((uint8_t *)flashbrival)[3];
}

#endif
