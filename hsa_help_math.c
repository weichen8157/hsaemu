#if MEM_BIT == 64
#define DATA_TYPE double
#define PRECISION 0x1.fffffffffffffp-1
#define FLOOR floor
#elif MEM_BIT == 32
#define DATA_TYPE float
#define PRECISION 0X1.fffffep-1f
#define FLOOR floorf
#else
#error
#endif

#define MIN(__x,__y) ((__x) > (__y) ? (__y) : (__x))

DATA_TYPE gum(helper_FSqrt_f, MEM_BIT)(DATA_TYPE arg)
{
	cu_context.prof.sfu++;
	return sqrt(arg);
}

DATA_TYPE gum(helper_Fract_f, MEM_BIT)(DATA_TYPE arg)
{
	cu_context.prof.sfu++;
	return MIN(arg - FLOOR(arg), PRECISION);
}
/*
double helpee_Fract_f64(double arg)
{
	cu_context.prof.sfu++;
	return MIN(arg - floor(arg), 0x1.fffffffffffffp-1);
}
*/
DATA_TYPE gum(helper_Fcos_f, MEM_BIT)(DATA_TYPE arg)
{
	cu_context.prof.sfu++;
	return cosf(arg);
}

DATA_TYPE gum(helper_Fsin_f, MEM_BIT)(DATA_TYPE arg)
{
	cu_context.prof.sfu++;
	return sinf(arg);
}

DATA_TYPE gum(helper_Flog2_f, MEM_BIT)(DATA_TYPE arg)
{
	cu_context.prof.sfu++;
	return log(arg)/log(2);
}

DATA_TYPE gum(helper_Fexp2_f, MEM_BIT)(DATA_TYPE arg)
{
	cu_context.prof.sfu++;
	return exp(arg * log(2));
}

#if MEM_BIT == 64

DATA_TYPE helper_Frsqrt(DATA_TYPE arg)
{
	cu_context.prof.sfu++;
	return 1.0 / sqrt(arg);
}

DATA_TYPE helper_Frcp(DATA_TYPE arg)
{
	cu_context.prof.sfu++;
	return 1.0 / arg;
}

#endif
#undef DATA_TYPE
#undef PRECISION
#undef FLOOR
