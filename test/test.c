#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <thread_pool.h>
#include <comm.h>
#include <hsa.h>
#define SIM_TASK_AMOUNT 10 /*must be a constant*/

#define SIM_WORKGROUP_SIZE_X 32
#define SIM_WORKGROUP_SIZE_Y 1
#define SIM_WORKGROUP_SIZE_Z 1
#define SIM_WORKGROUP_SIZE (SIM_WORKGROUP_SIZE_Z * SIM_WORKGROUP_SIZE_Y * SIM_WORKGROUP_SIZE_X)

#define SIM_NDRANGE_SIZE_X 50
#define SIM_NDRANGE_SIZE_Y 1
#define SIM_NDRANGE_SIZE_Z 1
#define SIM_NDRANGE_SIZE (SIM_NDRANGE_SIZE_Z * SIM_NDRANGE_SIZE_Y * SIM_NDRANGE_SIZE_X)

#define AQL_EMTRY 8

volatile int global_count = 0;
const int verify_count = SIM_NDRANGE_SIZE * SIM_WORKGROUP_SIZE * SIM_BARRIER_LOOP * SIM_TASK_AMOUNT;
pthread_mutex_t global_mutx;
extern uintptr_t hsa_cond_ptr;

int main()
{
	srand(time(NULL));
	pthread_mutex_init(&global_mutx, NULL);

	init_hsa_component();
		
	// create user level queue
	hsa_user_queue_t *userQ = calloc(1, sizeof(hsa_user_queue_t));
	userQ->basePointer = (uint64_t)calloc(AQL_EMTRY, sizeof(AQL));
	userQ->readOffset = 0;
	userQ->writeOffset = 0;
	userQ->size = AQL_EMTRY * sizeof(AQL);
	int i = 0;
	for (i = 0; i < AQL_EMTRY; i++)
	{
		AQL *aqlptr = (AQL*)(userQ->basePointer + 
			i * sizeof(AQL));
		completionObjec *ptr = (completionObjec*)calloc(1, 
			sizeof(completionObjec));
		ptr->status = 1;
		aqlptr->completionObjectAddress = (uint64_t)ptr;
	}


	int task_count = 0;
	uint64_t limit = userQ->basePointer + AQL_EMTRY * sizeof(AQL);
	while(1)
	{
		AQL *aqlptr = (AQL*)(userQ->basePointer + 
			(uint64_t)userQ->writeOffset);
		completionObjec *cobjptr = (completionObjec*)aqlptr->completionObjectAddress;

		// queue is full, wait the last aql complete
		while (cobjptr->status == 0) {
			sleep(1);
		}

		if (task_count < SIM_TASK_AMOUNT) 
		{
			// setup AQL
			aqlptr->gridSize_x = SIM_NDRANGE_SIZE_X;
			aqlptr->gridSize_y = SIM_NDRANGE_SIZE_Y;
			aqlptr->gridSize_z = SIM_NDRANGE_SIZE_Z;
			aqlptr->workgroupSize_x = SIM_WORKGROUP_SIZE_X;
			aqlptr->workgroupSize_y = SIM_WORKGROUP_SIZE_Y;
			aqlptr->workgroupSize_z = SIM_WORKGROUP_SIZE_Z;
			aqlptr->flag = 0;
			cobjptr->status = 0;

			// update offset
			userQ->writeOffset += sizeof(AQL);
			if ((userQ->basePointer + (uint64_t)userQ->writeOffset) >= limit)
				userQ->writeOffset = 0;

			// insert AQL, signal HSA component
			enqueue_task(userQ);
			pthread_cond_signal((pthread_cond_t*)hsa_cond_ptr);
			
			// temp, update
			task_count++;
		}

		//sleep(1);
		if (global_count == verify_count) exit(1);
	}

	return 0;
}
