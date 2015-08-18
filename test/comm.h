#ifndef __HSA_COMM_H__
#define __HSA_COMM_H__

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <assert.h> 
#include <thread_pool.h>

//*****************************************************************************

#define AMOUNT_CU 2
#define FINISHQ_ENTRY (AMOUNT_CU + 1)
#define WAVEFRONT_SIZE 8
#define MAX_WORKGROUP_SIZE 64
#define MAX_DIM 3
#define THREAD_POOL_SIZE (8 + AMOUNT_CU * MAX_WORKGROUP_SIZE)
#define SIG_CU SIGUSR1
#define SIG_PE SIGUSR2

#ifndef bool
#define bool int
#define true 1
#define false 0
#endif

#define SIM_BARRIER (rand()%2)
#define SIM_BARRIER_LOOP 1 /*must be a constant*/

//*****************************************************************************

enum _thread_type
{
	THREAD_MONITOR = 0,
	THREAD_CU,
	THREAD_PE
};

enum _task_priority
{
	MASK_ABLE = 0,
	UN_MASK_ABLE,
	AMOUNT_TASK_PRIORITY
};

typedef struct _HSAContext HSAContext;
typedef struct _CUContext CUContext;
typedef struct _PEContext PEContext;
typedef struct _HSATask HSATask;
typedef struct _TaskQueue TaskQueue;
typedef struct _DIM3 DIM3;
typedef struct _AQL AQL;
typedef struct _user_queue user_queue;
typedef struct _completionObjec completionObjec;
typedef struct _HSAQueue HSAQueue;
typedef struct _HSAEntry HSAEntry;

struct _user_queue{
	uint64_t basePointer;
	uint64_t doorbellPointer;
	uint64_t dispatchID;
	uint32_t writeOffset;
	uint32_t readOffset;
	uint32_t size;
	uint32_t PASID;
	uint32_t queueID;
	uint32_t priority;
};

struct _AQL{
	uint32_t flag;
	uint32_t reserved;
	uint64_t kernelObjectAddress;
	uint64_t completionObjectAddress;
	uint64_t kernargAddress;
	uint64_t dispatchId;
	uint32_t gridSize_x;
	uint32_t gridSize_y;
	uint32_t gridSize_z;
	uint32_t workgroupSize_x;
	uint32_t workgroupSize_y;
	uint32_t workgroupSize_z;
	uint32_t workgroupGroupSegmentSizeBytes;
	uint32_t workitemPrivateSegmentSizeBytes;
	uint32_t workitemSpillSegmentSizeBytes;
	uint32_t workitemArgSegmentSizeBytes;
	uint32_t syncDW0;
	uint32_t syncDW1;
	uint32_t syncDW2;
	uint32_t syncDW3;
	uint32_t syncDW4;
	uint32_t syncDW5;
	uint32_t syncDW6;
	uint32_t syncDW7;
	uint32_t syncDW8;
	uint32_t syncDW9;
};

struct _completionObjec{
	int32_t  status;
	uint32_t reserved;
	uint32_t completionSignalAddress;
	uint64_t parseTimeStamp;
	uint64_t dispatchTimeStamp;
	uint64_t completionTimeStamp;
};

struct _DIM3
{
	int dim[3];
};

struct _PEContext
{
	uintptr_t id;
	int workitem_Fid;
	DIM3 workitem_id;
	pthread_cond_t cond;
	pthread_mutex_t mutx;
	CUContext *master_cu_env;
	int max_wavefront_count;
	void *kernel;
};

struct _CUContext
{
	uintptr_t id;
	int workgroup_Fid;
	DIM3 workgroup_id;
	volatile bool barrier_happen;
	volatile bool running;
	volatile bool change_task;
	pthread_cond_t cond;
	pthread_mutex_t mutx;
	pthread_cond_t wakeup_pe_cond;
	pthread_mutex_t wakeup_pe_mutx;
	HSATask *exec_task;
	list_thread_block *pe_list;
	list_thread_block *backup_list;
	HSAContext *master_hsa_env;
	volatile int wavefront_count;
	volatile int workgroup_count;
	volatile int barrier_count;

	volatile int verfiy_count;
	volatile bool sim_barrier;
};

struct _HSAContext
{
	pthread_cond_t cond;
	pthread_mutex_t mutx;
	CUContext *cu_env;
	list_thread_block *cu_list;
	TaskQueue *runQ;
	TaskQueue *waitQ;
	HSAQueue *hQ;
};

struct _HSATask
{
	int priority;
	int ndrange_Fsize;
	int workgroup_Fsize;
	DIM3 ndrange_size;
	DIM3 workgroup_size;
	volatile int dispach_group;
	volatile int finish_group;
	pthread_spinlock_t lock;
	void *func;
	void *arg;
	char name[64];
	TAILQ_ENTRY(_HSATask) next;

	volatile int sim_cost;
};

struct _TaskQueue
{
	int max;
	volatile int n;
	bool preemption;
	pthread_spinlock_t lock;
	TAILQ_HEAD(task_list, _HSATask) head;
};

struct _HSAEntry
{
	//bool avalible;
	//CPUArchState env;
	hsa_user_queue_t userQ;
};

struct _HSAQueue
{
	pthread_mutex_t lock;
};

//*****************************************************************************

void cvtFidToDIM3(DIM3 *id, 
                const int Fid,
                const DIM3 *size);

#endif