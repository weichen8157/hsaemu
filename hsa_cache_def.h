#ifndef __HSA_CACHE_DEF_H__
#define __HSA_CACHE_DEF_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

typedef struct _share_cahe
{
	uint64_t cache_tag[512];
	pthread_spinlock_t cache_lock;
}share_cache_t;

#endif
