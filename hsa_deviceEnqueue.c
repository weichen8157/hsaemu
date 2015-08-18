#include "hsa_deviceEnqueue.h"
#include "hsa_comm_def.h"
#include "hsa_agent.h"

extern hsa_queue_t global_q;

uint64_t helper_GetDefaultQueue(void)
{
	void *ptr = (void *)&global_q;

	return (uint64_t)(uintptr_t)ptr;
}
