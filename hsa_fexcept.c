#include "hsa_fexcept.h"

void helper_ClearDetectExcept(uint32_t exceptionsNumber)
{
	switch (exceptionsNumber)
	{
		case 0:
			feclearexcept (FE_INVALID);
			break;
		case 1:
			feclearexcept (FE_DIVBYZERO);
			break;
		case 2:
			feclearexcept (FE_OVERFLOW);
			break;
		case 3:
			feclearexcept (FE_UNDERFLOW);
			break;
		case 4:
			feclearexcept (FE_INEXACT);
			break;
		default:
			fprintf(stderr,"Wrong exceptionsNumber!\n");
			break;
	}
}

void helper_SetDetectExcept(uint32_t exceptionsNumber)
{
	switch (exceptionsNumber)
	{
		case 0:
			feraiseexcept (FE_INVALID);
			break;
		case 1:
			feraiseexcept (FE_DIVBYZERO);
			break;
		case 2:
			feraiseexcept (FE_OVERFLOW);
			break;
		case 3:
			feraiseexcept (FE_UNDERFLOW);
			break;
		case 4:
			feraiseexcept (FE_INEXACT);
			break;
		default:
			fprintf(stderr,"Wrong exceptionsNumber!\n");
			break;
	}
}

uint32_t helper_GetDetectExcept(void)
{
	int fe;

	fe = fetestexcept (FE_ALL_EXCEPT);
	fprintf(stderr,"the exceptions are set:\t");
	if(fe & FE_INVALID)		fprintf(stderr,"FE_INVALID\t");
	if(fe & FE_DIVBYZERO)	fprintf(stderr,"FE_DIVBYZERO\t");
	if(fe & FE_OVERFLOW)	fprintf(stderr,"FE_OVERFLOW\t");
	if(fe & FE_UNDERFLOW)	fprintf(stderr,"FE_UNDERFLOW\t");
	if(fe & FE_INEXACT)		fprintf(stderr,"FE_INEXACT\t");
	fprintf(stderr,"\n");

	return (uint32_t)fe;
}
