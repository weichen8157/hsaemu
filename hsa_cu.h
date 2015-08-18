#ifndef __HSA_CU_H__
#define __HSA_CU_H__
void *hsa_cu_thread_fn(void *arg);
void term_dump(int sig);
void floating_dump(int signo, siginfo_t *info, void *none);
void hsa_helper_barrier(void);

#endif
