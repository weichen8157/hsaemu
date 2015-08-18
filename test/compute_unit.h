#ifndef __HSA_CU_H__
#define __HSA_CU_H__

void *cu_func(void *arg);
void move_wavefront(CUContext *cu_env, int amount);
void suspend_cu(int signo);

#endif