#ifndef __HSA_FEXCEPT_H__
#define __HSA_FEXCEPT_H__
#include <stdio.h>
#include <stdlib.h>
#include <fenv.h>
#include <stdint.h>

void helper_ClearDetectExcept(uint32_t exceptionsNumber);
void helper_SetDetectExcept(uint32_t exceptionsNumber);
uint32_t helper_GetDetectExcept(void);

#endif
