#ifndef __HSA_MONITOR_H__
#define __HSA_MONITOR_H__
#include "hsa_cmd.h"

void hsa_mntor_resume_cus(void);
void hsa_create_mntor(void);
void hsa_block_signal(void);
int hsa_enqueue(void *cpu_state, size_t cpu_size, hsa_cmd_t *cmd);
int hsa_service_enqueue(void *cpu_state, size_t cpu_size, hsa_cmd_t *cmd);
void hsa_kill_convert(void);

#endif

