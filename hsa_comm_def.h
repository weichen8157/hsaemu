#ifndef __HSA_COMM_DEF__
#define __HSA_COMM_DEF__
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <setjmp.h>
#include <errno.h>
#include <unistd.h>
#include <ucontext.h>
#include <pthread.h>
#include <sys/queue.h>
#include "hsa_arch.h"
#include "hsa_prof.h"

#define RAND_STR_SIZE 16
#define MAX_STR_SIZE 1024
#define KB 1024
#define MB (KB*KB)
#define MAX_WORKITEM_DIM 3
#define MAX_WORKGROUP_DIM 3
#define MAX_WORKGROUP_SIZE 512
#define MAX_STACK_SIZE (4*KB)
#define MAX_BIN_SIZE (2*MB)
#define MAX_ARG_SIZE (4*KB)
#define MAX_GROUP_SIZE (64*KB)
#define MAX_ITEM_SIZE (4*KB)
#define MAX_ENV_SIZE (64*KB)
#define MAX_FUNC_NAME_SIZE 64

#define ALIGNMENT_DEV(x,y) (((x)+(y)-1)/(y))
#define SIZE_DIM2FLAT(src) (src.x * src.y * src.z)

#define HSA_DEBUG_LOG(__msg)                             \
	do {                                                 \
		fprintf(stderr, "in %s, line %u, %s: %s\n",      \
			__FILE__, __LINE__, __msg, "Error!"); \
		exit(EXIT_FAILURE);                              \
	} while (0)

#define GROUP_MEM_CHECK(__addr, __len) \
		do{ \
			if ((__addr) > (__len)) { \
				hsa_component_fault(__addr); \
			} \
		} while(0)

#define ITEM_MEM_CHECK(__addr, __len) \
		do{ \
			if ((__addr) > (__len)) { \
				fprintf(stderr,"Address is out of item memory range!\n"); \
			} \
		} while(0)

typedef enum {
	NONE_HSA = 0,
	HSA_AGENT_THREAD,
	HSA_CU_THREAD
}hsa_thread_type_t;

typedef enum {
	HSA_NONE = 0,
	HSA_FLAT,
	HSA_GLOBAL,
	HSA_READ_ONLY,
	HSA_KERNARG,
	HSA_GROUP,
	HSA_PRIVATE,
	HSA_SPILL,
	HSA_ARG,
}hsa_segment_t;

typedef enum {
	HSA_ATOMIC_ADD = 0,
	HSA_ATOMIC_AND,
	HSA_ATOMIC_CAS,
	HSA_ATOMIC_EXCH,
	HSA_ATOMIC_LD,
	HSA_ATOMIC_MAX,
	HSA_ATOMIC_MIN,
	HSA_ATOMIC_OR,
	HSA_ATOMIC_ST,
	HSA_ATOMIC_SUB,
	HSA_ATOMIC_DEC,
	HSA_ATOMIC_INC,
	HSA_ATOMIC_XOR,
	HSA_ATOMIC_WAIT_EQ,
	HSA_ATOMIC_WAIT_NE,
	HSA_ATOMIC_WAIT_LT,
	HSA_ATOMIC_WAIT_GTE,
	HSA_ATOMIC_WAITTIMEOUT_EQ,
	HSA_ATOMIC_WAITTIMEOUT_NE,
	HSA_ATOMIC_WAITTIMEOUT_LT,
	HSA_ATOMIC_WAITTIMEOUT_GTE,
}hsa_atomic_op_t;

typedef struct _brig_format
{
	size_t size;
	char path[MAX_STR_SIZE];
	unsigned char bin[MAX_BIN_SIZE];
}brig_format_t;

typedef union _hsa_dim3
{
	struct {int x, y, z;};
	int idx[3];
}hsa_dim3_t;


typedef struct _hsa_ccmsg hsa_ccmsg_t;
struct _hsa_ccmsg{
	uintptr_t v;
	hsa_dim3_t gid;
	hsa_dim3_t tid;
	TAILQ_ENTRY(_hsa_ccmsg) next;
};

typedef struct _hsa_ccmsg_queue hsa_ccmsg_queue_t;
struct _hsa_ccmsg_queue{
	int q_size;
	TAILQ_HEAD(_ccmsg_list, _hsa_ccmsg) q_head;
};

typedef struct _hsa_wi_cntxt
{
	// currently work-item id
	uint32_t flat_id;
	hsa_dim3_t id;
	// work-item context
	ucontext_t cntxt;
	uint8_t stack[MAX_STACK_SIZE];
	//work-item private memory
	uint8_t buf[MAX_ITEM_SIZE];
}hsa_wi_cntxt_t;

typedef struct _hsa_wg_cntxt
{
	// currently work-group id
	uint32_t flat_id;
	hsa_dim3_t id;
	// finish work-item id
	uint32_t finish_id;
	// currently work-item context pointer
	hsa_wi_cntxt_t *curt_wi_ptr;
	// total work-item context in a CU
	hsa_wi_cntxt_t wi_cntxt[MAX_WORKGROUP_SIZE];
	// a context used to sanp
	ucontext_t ret_cntxt;
	// profiling counter
	hsa_profile_t prof;
	// work-group memory
	uint8_t buf[MAX_GROUP_SIZE];
	// save debug message from code cache
	hsa_ccmsg_queue_t ccmsg_q;
	// excpt
	jmp_buf env;
}hsa_wg_cntxt_t;

typedef struct _hsa_obj{
	// two key values
	size_t pid;
	uintptr_t gv_addr;
	// number of work-items in a work-group
	hsa_dim3_t group_size;
	uint32_t flat_group_size;
	// number of work-groups in a grid
	hsa_dim3_t grid_size; 
	uint32_t flat_grid_size;
	// code cache
	uint8_t code[MAX_BIN_SIZE]__attribute__((aligned(4*KB)));
	void *kentry;
	size_t size;
	// kernel arguments
	uint8_t karg[MAX_ARG_SIZE];
	// finish work-group id
	uint32_t finish_id;
	pthread_spinlock_t ref_mutx;
	// profiling counter
	hsa_profile_t prof;	
}hsa_obj_t;

typedef enum {
	NONE = 0,
	CREATE,
	IDLE,
	BUSY
}hsa_thread_status_t;

typedef struct _hsa_cu{
	pthread_t cu_t;
	int id;
	uint8_t env[MAX_ENV_SIZE];
	hsa_thread_status_t cu_s;
}hsa_cu_t;

typedef struct _hsa_ctl{
	// used by other emulator
	pthread_cond_t wait_mntr;
	hsa_thread_status_t mntr_s;
	// used by monitor thread
	pthread_t mntr;
	pthread_mutex_t mntr_mutx;
	pthread_cond_t mntr_wait_cu;
	hsa_cu_t *cu_thread;
	// used by CU thread
	pthread_mutex_t cu_mutx; // no use currently
	pthread_cond_t cu_idle;
	// a busy counter
	volatile sig_atomic_t cu_busy;
	// record number of CU threads
	int num_cus;
	// profiling counter
	bool show;
	hsa_profile_t prof;
	// excpt
	jmp_buf env;
}hsa_ctl_t;

typedef struct _hsa_agent_ctl{
	pthread_cond_t wait_agent;
	pthread_t agent;
	pthread_mutex_t agent_mutex;
}hsa_agent_ctl;

typedef struct _hsa_task hsa_task_t;
struct _hsa_task{
	// used to save architecture context
	uint8_t *env;
	int env_size;
	hsa_dispatch_packet_aql_t aql;
	char name[MAX_FUNC_NAME_SIZE];
	int brig_size;
	int karg_size;
	uintptr_t guest_vaddr;
    TAILQ_ENTRY(_hsa_task) next;
};

typedef struct _agent_service_task agent_service_task_t;
struct _agent_service_task{
	uint8_t *env;
	int env_size;
	hsa_agent_dispatch_packet_aql_t agent_aql;
	uintptr_t guest_vaddr;
	TAILQ_ENTRY(_agent_service_task) next;
};

typedef struct _hsa_queue hsa_queue_t;
struct _hsa_queue{
	int q_size;
	pthread_spinlock_t q_lock;
	TAILQ_HEAD(_task_list, _hsa_task) q_head;
};

typedef struct _hsa_service_queue hsa_service_queue_t;
struct _hsa_service_queue{
	int q_size;
	pthread_spinlock_t q_lock;
	TAILQ_HEAD(_agent_task_list, _agent_service_task) q_head;
};

typedef struct _hsa_debug_mask
{
	bool enable;
	hsa_dim3_t gid;
	hsa_dim3_t tid;
}hsa_debug_mask_t;

#endif
