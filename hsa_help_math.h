#ifndef __HSA_HELPER_MATH_H__
#define __HSA_HELPER_MATH_H__

float helper_FSqrt_f32(float arg);
float helper_Fract_f32(float arg);
float helper_Fcos_f32(float arg);
float helper_Fsin_f32(float arg);
float helper_Flog2_f32(float arg);
float helper_Fexp2_f32(float arg);
double helper_Frsqrt(double arg);
double helper_Frcp(double arg);

double helper_FSqrt_f64(double arg);
double helper_Fract_f64(double arg);
double helper_Fcos_f64(double arg);
double helper_Fsin_f64(double arg);
double helper_Flog2_f64(double arg);
double helper_Fexp2_f64(double arg);
#endif
