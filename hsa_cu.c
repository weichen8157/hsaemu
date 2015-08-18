#include <signal.h>
#include <assert.h>
#include <signal.h>
#include "hsa_comm_def.h"
#include "hsa_vmdep.h"
#include "hsa_agent.h"
#include "hsa_cu.h"
#include "hsa_fexcept.h"

extern __thread hsa_thread_type_t per_thread_type;
extern hsa_obj_t global_cc;
extern hsa_ctl_t global_ctl;

__thread hsa_wg_cntxt_t cu_context ={
	.flat_id = 0,
	.finish_id = 0,
	.curt_wi_ptr = NULL,
	.ccmsg_q.q_size = 0,
	.prof = HSA_PROFILE_INITIALIZER
};

void hsa_helper_barrier(void)
{
	cu_context.prof.barr++;
	assert(cu_context.curt_wi_ptr != NULL);

	if (swapcontext(&(cu_context.curt_wi_ptr->cntxt), 
		&cu_context.ret_cntxt) == -1) {
		HSA_DEBUG_LOG("barrier work-item");
	}
}

static void hsa_cu_resume_mntor(void)
{
	pthread_mutex_lock(&global_ctl.mntr_mutx);

	hsa_update_prof(&global_cc.prof, &cu_context.prof);
	
	global_ctl.cu_busy--;
	if (global_ctl.cu_busy < 0) {
		HSA_DEBUG_LOG("invail CU counter");
	}
	if (global_ctl.cu_busy == 0) {
		pthread_cond_signal(&global_ctl.mntr_wait_cu);
	}
	// idle
	pthread_cond_wait(&global_ctl.cu_idle,
		&global_ctl.mntr_mutx);
	pthread_mutex_unlock(&global_ctl.mntr_mutx);

	hsa_clear_prof(&cu_context.prof);
}

static void cvt_flat_to_dim3(hsa_dim3_t *id,
	const int flat_id,
	const hsa_dim3_t *size)
{
	int xy_size = size->idx[0] * size->idx[1];
	id->idx[2] = flat_id / xy_size;
	int tmp = flat_id % xy_size;
	id->idx[1] = tmp / (size->idx[0]);
	id->idx[0] = tmp % (size->idx[0]);
}

#define TAILQ_FOREACH_SAFE(__var, __head, __field, __tmp) \
	for ((__var) = ((__head)->tqh_first), (__tmp) = (__var) ? ((__var)->__field.tqe_next) : NULL; \
		(__var);	\
		(__var) = (__tmp), (__tmp) = (__var) ? ((__var)->__field.tqe_next) : NULL)

extern pthread_mutex_t ccmsg_lock;
static void dump_ccmsg(void)
{
	hsa_ccmsg_t *ptr = NULL;
	hsa_ccmsg_t *ptr2 = NULL;
	
	pthread_mutex_lock(&ccmsg_lock);

	if (! TAILQ_EMPTY(&cu_context.ccmsg_q.q_head)) {
		fprintf(stderr, "\n\n= HSAIL register |"
			"    work-group ID   |"
			"    work-item ID =\n");
	}

	TAILQ_FOREACH_SAFE(ptr, &cu_context.ccmsg_q.q_head, next, ptr2)
	{
		fprintf(stderr, "%16p | "
			"(%4d, %4d, %4d) | "
			"(%4d, %4d, %4d)\n",
			(void*)ptr->v, 
			ptr->gid.x, ptr->gid.y, ptr->gid.z,
			ptr->tid.x, ptr->tid.y, ptr->tid.z);
		TAILQ_REMOVE(&cu_context.ccmsg_q.q_head, ptr, next);
		free(ptr);
	}

	pthread_mutex_unlock(&ccmsg_lock);

	cu_context.ccmsg_q.q_size = 0;
}

void term_dump(int sig)
{
	fprintf(stderr, "\n *** recv SIGSEGV ***\n");
	dump_ccmsg();
	fprintf(stderr, "\n *** SIGSEGV ! shutdown. ***\n");
	exit(EXIT_FAILURE);
}

void floating_dump(int signo, siginfo_t *info, void *none)
{
	int fe_number;
	fprintf(stderr, "\n *** recv SIGFPE ***\n");
	dump_ccmsg();

	fe_number = info->si_code;
	if(fe_number & FPE_INTDIV)	fprintf(stderr,"integer divide by zero\n");
	if(fe_number & FPE_FLTDIV)	fprintf(stderr,"floating divide by zero\n");
	if(fe_number & FPE_FLTOVF)	fprintf(stderr,"floating overflow\n");
	if(fe_number & FPE_FLTUND)	fprintf(stderr,"floating underflow\n");
	if(fe_number & FPE_FLTRES)	fprintf(stderr,"floating inexact result\n");
	if(fe_number & FPE_FLTINV)	fprintf(stderr,"invalid floating operation\n");
	fprintf(stderr, "\n ***SIGFPE recv !***\n");
	longjmp(cu_context.env, 1);
}

static int hsa_get_work(void)
{
	pthread_spin_lock(&global_cc.ref_mutx);
	if (global_cc.finish_id == global_cc.flat_grid_size) {
		pthread_spin_unlock(&global_cc.ref_mutx);
		return 0;
	}
	
	// set work-group info
	cvt_flat_to_dim3(&cu_context.id,
		global_cc.finish_id, &global_cc.grid_size);
	// reset work-item
	cu_context.finish_id = 0;
	
	// update ID
	global_cc.finish_id++;
	pthread_spin_unlock(&global_cc.ref_mutx);
	return 1;
}

static void cu_start_kernel(hsa_wi_cntxt_t *ptr)
{
	((void (*)(void *))global_cc.kentry)(global_cc.karg);
	cu_context.finish_id++;
}

extern pthread_rwlock_t cutlb_lock;
void *hsa_cu_thread_fn(void *arg)
{
	hsa_cu_t *cuptr = (hsa_cu_t*)arg;
	per_thread_type = HSA_CU_THREAD;

	// create queue
	TAILQ_INIT(&cu_context.ccmsg_q.q_head);
	cu_context.ccmsg_q.q_size = 0;

	//
	struct sigaction action;
	action.sa_handler = term_dump;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	if(sigaction(SIGSEGV, &action, NULL) == -1){
		HSA_DEBUG_LOG("redirect SIGSEGV");
	}
	
	struct sigaction faction;
	sigemptyset(&faction.sa_mask);
	faction.sa_sigaction = floating_dump;
	faction.sa_flags = SA_SIGINFO;
	if(sigaction(SIGFPE, &faction, NULL) == -1){
		HSA_DEBUG_LOG("redirect SIGFPE");
	}

	hsa_init_cu(cuptr->env);
	hsa_block_signal();
	hsa_cu_resume_mntor();

	while (1) {

		if (setjmp(cu_context.env)) {
			// GPU Kernel fault
			goto GPU_KERNEL_FAULT;
		}
		
		hsa_cu_update(cuptr->env);
		int ret = hsa_get_work();
		while (ret) {
			
			// init & reset ucontxt
			int i = 0;
			for (i = 0; i < MAX_WORKGROUP_SIZE; ++i)
			{
				hsa_wi_cntxt_t *wi_ptr = &cu_context.wi_cntxt[i];
			
				if (getcontext(&wi_ptr->cntxt) == -1) {
					HSA_DEBUG_LOG("init work-item context");
				}
				
				wi_ptr->cntxt.uc_stack.ss_sp = wi_ptr->stack;
				wi_ptr->cntxt.uc_stack.ss_size = MAX_STACK_SIZE;
				wi_ptr->cntxt.uc_link = &cu_context.ret_cntxt;
			
				makecontext(&wi_ptr->cntxt, 
					(void (*)(void))cu_start_kernel, 1, wi_ptr);
			}
			
			// execute all work-item in a work-group
			i = 0;
			while(cu_context.finish_id < global_cc.flat_group_size)
			{
				hsa_wi_cntxt_t *wi_ptr = &cu_context.wi_cntxt[i];

				cu_context.curt_wi_ptr = wi_ptr;
				
				wi_ptr->flat_id = i;
				cvt_flat_to_dim3(&wi_ptr->id, wi_ptr->flat_id,
					&global_cc.group_size);
				
				if (swapcontext(&cu_context.ret_cntxt, 
					&wi_ptr->cntxt) == -1) {
					HSA_DEBUG_LOG("swap work-item context");
				}
			
				i = (i < (global_cc.flat_group_size - 1)) ? (i + 1) : 0;
			}
			
			// update work-group id
			dump_ccmsg();
			ret = hsa_get_work();
		}

		// no work-group remain, wakeup HSA monitor
		// and sleep
GPU_KERNEL_FAULT:
		hsa_cu_resume_mntor();
	}

	return NULL;
}
