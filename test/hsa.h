#ifndef __HSA_H__
#define __HSA_H__

void *monitor_func(void *arg);
void init_hsa_component(void);
void enqueue_task(void *user_queue_ptr);

#endif