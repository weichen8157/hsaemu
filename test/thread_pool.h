#ifndef __THREAD_POOL_H__
#define __THREAD_POOL_H__
#include <stdint.h>
#include <sys/queue.h>
#include <pthread.h>

// STRUCTURE ******************************************************************

typedef struct _thread_pool thread_pool;
typedef struct _thread_pool_head list_thread_pool;
typedef struct _thread_context thread_context;
typedef struct _thread_block thread_block;
typedef struct _thread_block_head list_thread_block;

enum _thread_state
{
	THREAD_UNREG = 0,
	THREAD_WAIT,
	THREAD_READY,
	THREAD_RUN
};

struct _thread_pool
{
	int remain;
	int amount;
	thread_context *ths_ptr;
	TAILQ_ENTRY(_thread_pool) next;
};

struct _thread_pool_head
{
	int amount;
	pthread_mutex_t mutx;
	TAILQ_HEAD(pool_list, _thread_pool) head;	
};

struct _thread_context
{
	int th_type;
	volatile int state;
	pthread_t th;
	pthread_cond_t cond;
	pthread_mutex_t mutx;
	void *(*func)(void*);
	void *data;
	thread_context *report;
	uintptr_t id;
};

struct _thread_block
{	
	int amount;
	list_thread_pool *list_from;
	thread_pool *pool_from;
	thread_context **ths_bk;
	TAILQ_ENTRY(_thread_block) next;
};

struct _thread_block_head
{
	int amount;
	pthread_mutex_t mutx;
	TAILQ_HEAD(block_list, _thread_block) head;
};

// MARCO **********************************************************************

#ifndef LIST_FOREACH_SAFE
#define LIST_FOREACH_SAFE(var, head, field, temp) \
	for ((var) = ((head)->lh_first), (temp) = (var) ? ((var)->field.le_next) : NULL; \
		(var);	\
		(var) = (temp), (temp) = (var) ? ((var)->field.le_next) : NULL)
#endif

#define THREAD_BLOCK_FOREACH(thread, count, block)			\
	for((count) = ((block)->ths_bk), (thread) = (*(count));	\
		(count) < ((block)->ths_bk + (block)->amount);		\
		(count)++, (thread) = (*(count)))

#define GET_THREAD_DATA(ptr) ((thread_context*)(ptr))->data;

#ifdef DEBUG

#define THREAD_LOCK(mutx_ptr)                          \
	pthread_mutex_lock(mutx_ptr);                      \
	fprintf(stderr, "%p lock %p, %s\n",                \
		(void*)pthread_self(), (mutx_ptr), __func__);

#define THREAD_FREE(mutx_ptr)                          \
	pthread_mutex_unlock(mutx_ptr);                    \
	fprintf(stderr, "%p free %p, %s\n",                \
		(void*)pthread_self(), (mutx_ptr), __func__);

#define THREAD_HALT(cond_ptr, mutx_ptr)                \
	fprintf(stderr, "%p free %p, %s\n",                \
		(void*)pthread_self(), (mutx_ptr), __func__);  \
	pthread_cond_wait((cond_ptr), (mutx_ptr));         \
	fprintf(stderr, "%p lock %p, %s\n",                \
		(void*)pthread_self(), (mutx_ptr), __func__);

#else

#define THREAD_LOCK(mutx_ptr)                    \
	pthread_mutex_lock(mutx_ptr);

#define THREAD_FREE(mutx_ptr)                    \
	pthread_mutex_unlock(mutx_ptr);

#define THREAD_HALT(cond_ptr, mutx_ptr)          \
	pthread_cond_wait((cond_ptr), (mutx_ptr));

#endif

// API ************************************************************************

list_thread_pool* alloc_thread_pool_head(void);
thread_pool* alloc_thread_pool(list_thread_pool *list, 
                               int amount);
thread_block* regs_thread_block(list_thread_block *block_list,
                                list_thread_pool *pool_list,
                                int amount,
                                int size);
list_thread_block* alloc_thread_block_head(void);
void unregs_thread_block(list_thread_block *list,
                         thread_block *arg);
void unregs_num_thread_block(list_thread_block *list,
                             int amount);
void wakeup_thread(thread_context *arg,
                   int type,
                   void *(*func)(void*),
                   void *data,
                   thread_context *report);
void sleep_thread(thread_context *arg);

#endif