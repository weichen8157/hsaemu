#ifndef __HSA_SIGNAL_H__
#define __HSA_SIGNAL_H__

#include <pthread.h>

typedef struct _hsa_opaque_signal{
	uint64_t		signal_id;
	int32_t			signal_value;

	uint32_t		consumers;
	pthread_spinlock_t 	sig_mutx;
}hsa_opaque_signal;

#endif
