#include <comm.h>
#include <processing_element.h>

extern __thread thread_context *pre_thread_ptr;

void suspend_cu(int signo)
{
	CUContext *cu_env = GET_THREAD_DATA(pre_thread_ptr);
	pthread_mutex_unlock(&cu_env->mutx);
	sleep_thread(pre_thread_ptr);
	pthread_mutex_lock(&cu_env->mutx);
}

static void exec_wavefront(CUContext *cu_env,
                           int workitem_begin,
                           thread_context *self)
{
	thread_block *block = TAILQ_FIRST(&cu_env->pe_list->head);
	thread_context *cntxt = NULL;
	thread_context **count = NULL;
	cu_env->wavefront_count = 0;
	//fprintf(stderr, "\ncu %p active wavefront %4d - %d\n\n",
	//	(void*)cu_env->id,
	//	workitem_begin,
	//	workitem_begin + WAVEFRONT_SIZE - 1);
	int workitem_end = workitem_begin + WAVEFRONT_SIZE;
	int limit_size = cu_env->exec_task->workgroup_Fsize;
	if (workitem_end > limit_size) {
		workitem_end = limit_size;
	}
	int active_size = workitem_end - workitem_begin;
	THREAD_BLOCK_FOREACH(cntxt, count, block)
	{
		PEContext *pe_env_ptr = (PEContext*)cntxt->data;
		pthread_mutex_lock(&pe_env_ptr->mutx);
		if (workitem_begin < workitem_end) {
			pe_env_ptr->workitem_Fid = workitem_begin;
			pe_env_ptr->kernel = cu_env->exec_task->func;
			pe_env_ptr->max_wavefront_count = active_size;
		} else {
			pe_env_ptr->kernel = NULL;
		}		
		//fprintf(stderr, "cu %p signal, pe %p, workitem id %4d\n", 
		//	(void*)cu_env->id,
		//	(void*)pe_env_ptr->id,
		//	pe_env_ptr->workitem_Fid);
		wakeup_thread(cntxt, THREAD_PE, pe_func, pe_env_ptr, self);
		pthread_mutex_unlock(&pe_env_ptr->mutx);
		workitem_begin++;
	}
}

void move_wavefront(CUContext *cu_env, int amount)
{
	int i = 0;
	thread_block *ptr = NULL;

	if (amount > 0) {
		// require more
		for (i = 0; i < amount; i++)
		{
			ptr = TAILQ_FIRST(&cu_env->backup_list->head);
			TAILQ_REMOVE(&cu_env->backup_list->head, ptr, next);
			TAILQ_INSERT_TAIL(&cu_env->pe_list->head, ptr, next);
		}
	} else {
		// too mush
		for (i = 0; i > amount; i--)
		{
			ptr = TAILQ_LAST(&cu_env->pe_list->head, block_list);
			TAILQ_REMOVE(&cu_env->pe_list->head, ptr, next);
			TAILQ_INSERT_TAIL(&cu_env->backup_list->head, ptr, next);
		}	
	}

	cu_env->backup_list->amount -= amount;
	cu_env->pe_list->amount += amount;

	fprintf(stderr, "pe %d, backup %d, %s\n", cu_env->pe_list->amount,
		cu_env->backup_list->amount, __func__);
}

void *cu_func(void *arg)
{
	thread_context *ptr = (thread_context*)arg;
	CUContext *cu_env = GET_THREAD_DATA(arg);
	HSAContext *master = cu_env->master_hsa_env;
	pthread_mutex_lock(&cu_env->mutx);
	pthread_mutex_lock(&cu_env->wakeup_pe_mutx);
	// simulate barrier
	cu_env->sim_barrier = SIM_BARRIER;
	
	HSATask *task = cu_env->exec_task;
	assert(task != NULL);

	// setting wavefront
	const int num_wavefront_req = (task->workgroup_Fsize + 
		WAVEFRONT_SIZE - 1) / WAVEFRONT_SIZE;
	// manage pe resource
	if (abs(num_wavefront_req - cu_env->backup_list->amount) <
		abs(num_wavefront_req - cu_env->pe_list->amount)) {
		list_thread_block *tmp = cu_env->pe_list;
		cu_env->pe_list = cu_env->backup_list;
		cu_env->backup_list = tmp;
	}

	int finish_workgroup_count = 0;
	while(true)
	{
		// get the workgroup from task
		pthread_spin_lock(&task->lock);
		if (task->dispach_group < task->ndrange_Fsize && 
			cu_env->change_task == false) {
			cu_env->workgroup_Fid = task->dispach_group;
			task->dispach_group++;
		} else {
			if (cu_env->change_task) {
				cu_env->change_task = false;
				fprintf(stderr, "cu %p exit, workgroup %d, task %s\n",
					(void*)cu_env->id,
					cu_env->workgroup_Fid,
					task->name);
			}
			pthread_spin_unlock(&task->lock);
			break;
		}
		pthread_spin_unlock(&task->lock);

		// setting workgroup id
		cvtFidToDIM3(&cu_env->workgroup_id, 
			cu_env->workgroup_Fid,
			&task->ndrange_size);

		// reset wavefront index
		int workitem_Fidx_count = 0;
		cu_env->workgroup_count = 0;
		cu_env->verfiy_count = 0;
		
		exec_wavefront(cu_env, workitem_Fidx_count, ptr);
		workitem_Fidx_count += WAVEFRONT_SIZE;
		
		// let cu idle, waitting pe
		pthread_cond_wait(&cu_env->wakeup_pe_cond, 
			&cu_env->wakeup_pe_mutx);

		// condition wait must before this line,
		// because we don't know pe state
		while (cu_env->workgroup_count != 
			task->workgroup_Fsize) {

			if (cu_env->barrier_happen == false) {
				exec_wavefront(cu_env, workitem_Fidx_count, ptr);
				workitem_Fidx_count += WAVEFRONT_SIZE;
			} else {
				// adjust wavefront list
				int move_count = num_wavefront_req - cu_env->pe_list->amount;
				if (move_count) {
					move_wavefront(cu_env, move_count);
				}
				// round-robin execute wavefront
				thread_block *tmp = TAILQ_FIRST(&cu_env->pe_list->head);
				TAILQ_REMOVE(&cu_env->pe_list->head, tmp, next);
				TAILQ_INSERT_TAIL(&cu_env->pe_list->head, tmp, next);
				// if one barrier done
				if (cu_env->barrier_count == task->workgroup_Fsize) {
					cu_env->barrier_count = 0;
					workitem_Fidx_count = 0;
				}
				exec_wavefront(cu_env, workitem_Fidx_count, ptr);
				workitem_Fidx_count += WAVEFRONT_SIZE;
			}
			
			// let cu idle, waitting pe
			pthread_cond_wait(&cu_env->wakeup_pe_cond, 
				&cu_env->wakeup_pe_mutx);
		}

		finish_workgroup_count++;
		assert(cu_env->verfiy_count == task->workgroup_Fsize);
	}

	// update task
	pthread_spin_lock(&task->lock);
	task->finish_group += finish_workgroup_count;
	if (finish_workgroup_count && 
		task->finish_group == task->ndrange_Fsize) {

		TaskQueue *q = master->runQ;
		pthread_spin_lock(&q->lock);
		q->n--;
		TAILQ_REMOVE(&q->head, task, next);
		pthread_spin_unlock(&q->lock);

		q = master->waitQ;
		pthread_spin_lock(&q->lock);
		q->n++;
		TAILQ_INSERT_TAIL(&q->head, task, next);
		pthread_spin_unlock(&q->lock);		
	}
	pthread_spin_unlock(&task->lock);
/*
	fprintf(stderr, "cu %p finish, workgroup Fid %4d (%d %d %d), task %s\n",
		(void*)cu_env->id,
		cu_env->workgroup_Fid,
		cu_env->workgroup_id.dim[0],
		cu_env->workgroup_id.dim[1],
		cu_env->workgroup_id.dim[2],
		task->name);
*/
		
//CU_SIG_MONITOR:
	// signal monitor
	pthread_mutex_unlock(&cu_env->wakeup_pe_mutx);
	pthread_mutex_lock(&master->mutx);
	cu_env->running = false;
	pthread_mutex_unlock(&cu_env->mutx);
	pthread_cond_signal(&master->cond);

	fprintf(stderr, "cu %p signal\n", (void*)cu_env->id);

	pthread_mutex_unlock(&master->mutx);

	return NULL;
}