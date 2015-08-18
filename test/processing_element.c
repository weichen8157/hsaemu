#include <unistd.h>
#include <comm.h>

__thread thread_context *pre_thread_ptr = NULL;
__thread PEContext *pre_pe_env = NULL;

static void barrier_happen(void)
{
	CUContext *master = pre_pe_env->master_cu_env;

	pthread_mutex_lock(&master->wakeup_pe_mutx);
	master->wavefront_count++;
	master->barrier_count++;
	if (master->wavefront_count == 
		pre_pe_env->max_wavefront_count) {
		// reset counter, and signal cu
		master->wavefront_count = 0;
		master->barrier_happen = true;
		pthread_cond_signal(&master->wakeup_pe_cond);

		//fprintf(stderr, "pe %p barrier, workitem id %4d\n", 
		//	(void*)pre_pe_env->id,
		//	pre_pe_env->workitem_Fid);
	}
	pthread_mutex_unlock(&master->wakeup_pe_mutx);

	pthread_mutex_unlock(&pre_pe_env->mutx);
	sleep_thread(pre_thread_ptr);
	pthread_mutex_lock(&pre_pe_env->mutx);
}

extern volatile int global_count;
extern pthread_mutex_t global_mutx;
static void gotoCodeCache(bool barrier)
{	
	int i = SIM_BARRIER_LOOP;

	do{
		// just do something
		pthread_mutex_lock(&global_mutx);
		/*
		fprintf(stderr, "pe %p exec, workitem Fid %4d (%d %d %d), count: %4d\n", 
			(void*)pre_pe_env->id,
			pre_pe_env->workitem_Fid,
			pre_pe_env->workitem_id.dim[0],
			pre_pe_env->workitem_id.dim[1],
			pre_pe_env->workitem_id.dim[2],
			global_count);
		*/
		pre_pe_env->master_cu_env->verfiy_count++;
		global_count++;
		pthread_mutex_unlock(&global_mutx);

		if (barrier) barrier_happen();
		i--;
	}while(i);
	/*
	CUContext *master = pre_pe_env->master_cu_env;
	HSATask *task = master->exec_task;
	usleep(task->sim_cost * 1024 * 10);*/
}

void *pe_func(void *arg)
{
	thread_context *ptr = (thread_context*)arg;
	PEContext *pe_env = GET_THREAD_DATA(arg);
	CUContext *master = pe_env->master_cu_env;
	pthread_mutex_lock(&pe_env->mutx);
	// setting global var
	cvtFidToDIM3(&pe_env->workitem_id,
		pe_env->workitem_Fid,
		&master->exec_task->workgroup_size);

	pre_thread_ptr = ptr;
	pre_pe_env = pe_env;

	if (pe_env->kernel) {
		gotoCodeCache(master->sim_barrier);
	} else {
		pthread_mutex_unlock(&pe_env->mutx);
		return NULL;
	}
	
	//pthread_barrier_wait(pe_env->barrier_ptr);
	//fprintf(stderr, "pe %p finish, workitem id %4d\n", 
	//	(void*)pe_env->id,
	//	pe_env->workitem_Fid);
	
	pthread_mutex_lock(&master->wakeup_pe_mutx);
	master->wavefront_count++;
	master->workgroup_count++;
	if (master->wavefront_count == 
		pe_env->max_wavefront_count) {

		// reset counter, and signal cu
		master->wavefront_count = 0;
		pthread_cond_signal(&master->wakeup_pe_cond);

		//fprintf(stderr, "pe %p signal, workitem id %4d\n", 
		//	(void*)pe_env->id,
		//	pe_env->workitem_Fid);
	}
	pthread_mutex_unlock(&master->wakeup_pe_mutx);

	pthread_mutex_unlock(&pe_env->mutx);
	return NULL;
}