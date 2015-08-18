#include <comm.h>
#include <compute_unit.h>

list_thread_pool *global_pool_head = NULL;
HSAContext *global_hsa_env = NULL;

static char *rand_string(char *str, size_t size)
{
    const char charset[] = "abcdefghijklmnopqrstuvwxyz";
    if (size) {
        --size;
        size_t n = 0;
        for (n = 0; n < size; n++) 
        {
            int key = rand() % (int) (sizeof charset - 1);
            str[n] = charset[key];
        }
        str[size] = '\0';
    }
    return str;
}

void enqueue_task(void *user_queue_ptr)
{
	HSAContext *hsa_env = global_hsa_env;
	assert(user_queue_ptr != NULL);

	hsa_user_queue_t userQ;
	memcpy(&userQ, user_queue_ptr, sizeof(hsa_user_queue_t));

	if(userQ.writeOffset == userQ.readOffset) return;

	AQL aql;
	uintptr_t addr = (uintptr_t)(userQ.basePointer + 
		(uint64_t)userQ.readOffset);
	memcpy(&aql, (void*)addr, sizeof(AQL));

	HSATask *ptr = (HSATask*)calloc(1, sizeof(HSATask));
	// setup ndrange
	ptr->ndrange_size.dim[0] = aql.gridSize_x;
	ptr->ndrange_size.dim[1] = aql.gridSize_y;
	ptr->ndrange_size.dim[2] = aql.gridSize_z;
	ptr->ndrange_Fsize = aql.gridSize_x *
		aql.gridSize_y * aql.gridSize_z;
	// setup workgroup
	ptr->workgroup_size.dim[0] = aql.workgroupSize_x;
	ptr->workgroup_size.dim[1] = aql.workgroupSize_y;
	ptr->workgroup_size.dim[2] = aql.workgroupSize_z;
	ptr->workgroup_Fsize = aql.workgroupSize_x * 
		aql.workgroupSize_y * aql.workgroupSize_z;
	
	ptr->priority = aql.flag;
	pthread_spin_init(&ptr->lock, PTHREAD_PROCESS_PRIVATE);
	ptr->func = (void*)0xF;
	rand_string((char*)ptr->name, 8);

	fprintf(stderr, "monitor get task, ndrange size %d (%d %d %d), workgroup size %d (%d %d %d), task %s, priority %d\n",
		ptr->ndrange_Fsize,
		ptr->ndrange_size.dim[0],
		ptr->ndrange_size.dim[1],
		ptr->ndrange_size.dim[2],
		ptr->workgroup_Fsize,
		ptr->workgroup_size.dim[0],
		ptr->workgroup_size.dim[1],
		ptr->workgroup_size.dim[2],
		ptr->name,
		ptr->priority);
	
	// quick schedule here
	TaskQueue *q = hsa_env->runQ;
	pthread_spin_lock(&q->lock);
	HSATask *old = TAILQ_FIRST(&q->head);
	if (old != NULL && 
		ptr->priority > old->priority) {
		q->preemption = true;
		TAILQ_INSERT_HEAD(&q->head, ptr, next);
		old->priority++;
		fprintf(stderr, "preemption task %s\n", ptr->name);
	} else {
		TAILQ_INSERT_TAIL(&q->head, ptr, next);
	}
	q->n++;
	pthread_spin_unlock(&q->lock);
}
void schedule(TaskQueue *q)
{
	pthread_spin_lock(&q->lock);
	// move task order here
	pthread_spin_unlock(&q->lock);
}
static void monitor_idle(HSAContext *hsa_env)
{
	fprintf(stderr, "monitor idle, waitting cu\n");
	pthread_cond_wait(&hsa_env->cond, &hsa_env->mutx);
	// check task finish yet
	TaskQueue *q = hsa_env->waitQ;
	pthread_spin_lock(&q->lock);
	while(!TAILQ_EMPTY(&q->head))
	{
		HSATask *task = TAILQ_FIRST(&q->head);
		assert(task->dispach_group == task->finish_group);
		q->n--;
		TAILQ_REMOVE(&q->head, task, next);
		free(task);
		fprintf(stderr, "monitor finish task %s\n", task->name);
	}
	pthread_spin_unlock(&q->lock);

	schedule(hsa_env->runQ);
}

static void dispatch_task_to_cu(thread_block *block_ptr,
                                TaskQueue *queue,
                                thread_context *self)
{
	thread_context *cntxt = NULL;
	thread_context **count = NULL;

	pthread_spin_lock(&queue->lock);
	HSATask *task = NULL;
	TAILQ_FOREACH(task, &queue->head, next)
	{
		pthread_spin_lock(&task->lock);
		if (task->dispach_group < task->ndrange_Fsize) {
			pthread_spin_unlock(&task->lock);
			break;
		}
		pthread_spin_unlock(&task->lock);
	}
	
	if (task == NULL) return;

	THREAD_BLOCK_FOREACH(cntxt, count, block_ptr)
	{
		CUContext *cu_env = (CUContext*)cntxt->data;
		if (cu_env->running == false) {
			pthread_mutex_lock(&cu_env->mutx);
			// update cu env
			cu_env->running = true;
			cu_env->exec_task = task;
			wakeup_thread(cntxt, THREAD_CU, cu_func, cu_env, self);

			fprintf(stderr, "monitor dispatch, cu %p, task %s\n",
				(void*)cu_env->id,
				task->name);
			
			pthread_mutex_unlock(&cu_env->mutx);			
		} else if(queue->preemption) {
			//HSATask *old = cu_env->exec_task;
			//if (task->priority > old->priority)
				cu_env->change_task = true;
		}
	}
	queue->preemption = false;
	pthread_spin_unlock(&queue->lock);
}

static void alloc_wavefront(CUContext *cu_env)
{
	thread_block *block = regs_thread_block(cu_env->pe_list, 
		global_pool_head, WAVEFRONT_SIZE, sizeof(PEContext));
	thread_context *cntxt = NULL;
	thread_context **count = NULL;

	THREAD_BLOCK_FOREACH(cntxt, count, block)
	{
		// setting pe context
		PEContext *pe_env_ptr = (PEContext*)cntxt->data;
		pe_env_ptr->master_cu_env = cu_env;
		pe_env_ptr->id = cntxt->id;
		pthread_cond_init(&pe_env_ptr->cond, NULL);
		pthread_mutex_init(&pe_env_ptr->mutx, NULL);
	}
}

void *monitor_func(void *arg)
{
	HSAContext *hsa_env = GET_THREAD_DATA(arg);
	pthread_mutex_lock(&hsa_env->mutx);

	// allocate cu resource
	hsa_env->cu_list = alloc_thread_block_head();
	thread_block *block_ptr = regs_thread_block(hsa_env->cu_list, 
		global_pool_head, AMOUNT_CU, sizeof(CUContext));
	thread_context *tmp = *(block_ptr->ths_bk);
	hsa_env->cu_env = (CUContext*)tmp->data;

	int max_wavefront = MAX_WORKGROUP_SIZE / WAVEFRONT_SIZE;
	
	thread_context *cntxt = NULL;
	thread_context **count = NULL;
	THREAD_BLOCK_FOREACH(cntxt, count, block_ptr)
	{
		// setting cu context
		CUContext *cu_env = (CUContext*)cntxt->data;
		cu_env->master_hsa_env = hsa_env;
		cu_env->id = cntxt->id;
		cu_env->backup_list = alloc_thread_block_head();
		cu_env->pe_list = alloc_thread_block_head();
		int i = 0;
		for (i = 0; i < max_wavefront; i++)
		{
			alloc_wavefront(cu_env);
		}
		move_wavefront(cu_env, -1);

		// use to know task finished yet
		pthread_mutex_init(&cu_env->mutx, NULL);
		pthread_cond_init(&cu_env->cond, NULL);
		// use to wakeup pe
		pthread_cond_init(&cu_env->wakeup_pe_cond, NULL);
		pthread_mutex_init(&cu_env->wakeup_pe_mutx, NULL);
	}

	while(1) {
		//get_task(hsa_env);
		TaskQueue *q = hsa_env->runQ;
		if (!TAILQ_EMPTY(&q->head)) {
			dispatch_task_to_cu(block_ptr, q, arg);
		}
		monitor_idle(hsa_env);
	}

	return NULL;
}

uintptr_t hsa_cond_ptr = 0;
void init_hsa_component(void)
{
	// registe ISR
	struct sigaction cu_action;
	cu_action.sa_flags = 0;
	cu_action.sa_handler = suspend_cu;
	int err = sigaction(SIG_CU, &cu_action, NULL);
	assert(err == 0);
	
	// allocate thread pool resource
	list_thread_pool *pool_list = alloc_thread_pool_head();
	alloc_thread_pool(pool_list, THREAD_POOL_SIZE);
	global_pool_head = pool_list;
	
	//register 1 monitor thread
	list_thread_block *thread_list = alloc_thread_block_head();
	thread_block *block_ptr = regs_thread_block(thread_list, 
		pool_list, 1, sizeof(HSAContext));
	thread_context *cntxt = *block_ptr->ths_bk;

	HSAContext *hsa_env = GET_THREAD_DATA(cntxt);
	global_hsa_env = hsa_env;
	pthread_mutex_init(&hsa_env->mutx, NULL);
	pthread_cond_init(&hsa_env->cond, NULL);

	// allocate queue
	TaskQueue *ptr = calloc(1, sizeof(TaskQueue));
	TAILQ_INIT(&ptr->head);
	pthread_spin_init(&ptr->lock, PTHREAD_PROCESS_PRIVATE);
	hsa_env->runQ = ptr;

	ptr = calloc(1, sizeof(TaskQueue));
	TAILQ_INIT(&ptr->head);
	pthread_spin_init(&ptr->lock, PTHREAD_PROCESS_PRIVATE);
	hsa_env->waitQ = ptr;
	
	// temp, use for test
	hsa_cond_ptr = (uintptr_t)(&hsa_env->cond);

	wakeup_thread(cntxt, THREAD_MONITOR, monitor_func, cntxt->data, NULL);
}