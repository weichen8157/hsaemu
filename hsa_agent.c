#include <stdbool.h>
#include <time.h>
#include <string.h>
#include <sys/queue.h>
#include "hsa_comm_def.h"
#include "hsa_agent.h"
#include "hsa_signal.h"
#include "hsa_cu.h"
#include "hsa_linkloader.h"
#include "hsa_vmdep.h"

__thread void *private_cpu_state = NULL;
__thread hsa_thread_type_t per_thread_type = NONE_HSA;
pthread_mutex_t ccmsg_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_rwlock_t cutlb_lock = PTHREAD_RWLOCK_INITIALIZER;
extern __thread hsa_wg_cntxt_t cu_context;
extern hsa_cpu_cache_t hsa_cpu_prof;

pthread_t counter_clock_t;

hsa_debug_mask_t global_debug = {
	.enable = false,
	.gid.idx = {0,0,0},
	.tid.idx = {0,0,0}
};

hsa_queue_t global_q = {
	.q_size = 0
};

hsa_service_queue_t global_service_q = {
	.q_size = 0
};

hsa_obj_t global_cc = {
	.pid = 0,
	.gv_addr = 0,
	.size = 0,
	.finish_id = 0,
	.flat_group_size = 0,
	.flat_grid_size = 0,
	.kentry = NULL,
	.prof = HSA_PROFILE_INITIALIZER
};

hsa_ctl_t global_ctl = {
	.wait_mntr = PTHREAD_COND_INITIALIZER,
	.mntr_s = NONE,
	.mntr_mutx = PTHREAD_MUTEX_INITIALIZER,
	.mntr_wait_cu = PTHREAD_COND_INITIALIZER,
	.cu_thread = NULL,
	.cu_mutx = PTHREAD_MUTEX_INITIALIZER,
	.cu_idle = PTHREAD_COND_INITIALIZER,
	.cu_busy = 0,
	.num_cus = 8,
	.show = true,
	.prof = HSA_PROFILE_INITIALIZER
};

hsa_agent_ctl global_agent_ctl = {
	.wait_agent = PTHREAD_COND_INITIALIZER,
	.agent_mutex = PTHREAD_MUTEX_INITIALIZER
};

static void hsa_main_resume_mntor(void)
{
	while(pthread_mutex_trylock(&global_ctl.mntr_mutx) == 0) {
		pthread_cond_signal(&global_ctl.wait_mntr);
		pthread_mutex_unlock(&global_ctl.mntr_mutx);
	}

	return;
}

int hsa_enqueue(
	void *cpu_state,
	size_t cpu_size,
	hsa_cmd_t *cmd)
{
	// only vCPU thread can call this function
	if (!cpu_state || !cmd) {
		HSA_DEBUG_LOG("useQ is NULL");
		return -1;
	}

	hsa_task_t *ptr = calloc(1, sizeof(hsa_task_t));

	// setting env
	if (cpu_size) {
		ptr->env = malloc(cpu_size);
		ptr->env_size = cpu_size;
		memcpy(ptr->env, cpu_state, cpu_size);
	} else {
		ptr->env = cpu_state;
	}

	// get AQL packet
	guest_vaddr_t aql_addr = cmd->enqueue.aql_address;
	hsa_copy_from_guest(&ptr->aql, aql_addr,
		sizeof(hsa_dispatch_packet_aql_t));

	// get kernel function name
	char buf[MAX_FUNC_NAME_SIZE];
	memset(buf, 0, MAX_FUNC_NAME_SIZE);
	hsa_copy_from_guest(buf,
		cmd->enqueue.func_name, MAX_FUNC_NAME_SIZE);
	strcpy(ptr->name, "Kernel_Entry_");
	strcat(ptr->name, buf);

	// get BRIG size
	ptr->brig_size = cmd->enqueue.brig_size;

	// get kernel arguments size
	ptr->karg_size = cmd->enqueue.karg_size;

	// set the return guest vaddr
	ptr->guest_vaddr = aql_addr;

	// enqueue
	pthread_spin_lock(&global_q.q_lock);
	global_q.q_size++;
	TAILQ_INSERT_TAIL(&global_q.q_head, ptr, next);
	pthread_spin_unlock(&global_q.q_lock);

	// wakeup monitor
	if (global_ctl.mntr_s == IDLE) {
		hsa_main_resume_mntor();
	}

	return 0;
}

static void hsa_main_resume_agent(void)
{
	while(pthread_mutex_trylock(&global_agent_ctl.agent_mutex) == 0){
		pthread_cond_signal(&global_agent_ctl.wait_agent);
		pthread_mutex_unlock(&global_agent_ctl.agent_mutex);
	}

	return;
}

int hsa_service_enqueue(
	void *cpu_state,
	size_t cpu_size,
	hsa_cmd_t *cmd)
{

	if (!cpu_state || !cmd) {
		HSA_DEBUG_LOG("serviceQ is NULL");
		return -1;
	}

	agent_service_task_t *ptr = calloc(1,sizeof(agent_service_task_t));

	if(cpu_size){
		ptr->env = malloc(cpu_size);
		ptr->env_size = cpu_size;
		memcpy(ptr->env, cpu_state, cpu_size);
	} else{
		ptr->env = cpu_state;
	}

	guest_vaddr_t agent_aql_addr = cmd->service_enqueue.service_aql_address;
	hsa_copy_from_guest(&ptr->agent_aql, agent_aql_addr,
			sizeof(hsa_agent_dispatch_packet_aql_t));

	ptr->guest_vaddr = agent_aql_addr;

	pthread_spin_lock(&global_service_q.q_lock);
	global_service_q.q_size++;
	TAILQ_INSERT_TAIL(&global_service_q.q_head, ptr, next);
	pthread_spin_unlock(&global_service_q.q_lock);

	hsa_main_resume_agent();

	return 0;
}

void hsa_component_fault(guest_vaddr_t addr)
{
	pthread_rwlock_unlock(&cutlb_lock);

	if (per_thread_type == HSA_CU_THREAD) {
		hsa_wi_cntxt_t *ptr = cu_context.curt_wi_ptr;

		fprintf(stderr, "Compute Unit page fault: %p, "
			"work-group ID = (%d %d %d), work-item ID = (%d %d %d)\n",
			(void*)(uintptr_t)addr,
			cu_context.id.x, cu_context.id.y, cu_context.id.z,
			ptr->id.x, ptr->id.y, ptr->id.z);
		longjmp(cu_context.env, 1);

	} else if (per_thread_type == HSA_AGENT_THREAD){

		fprintf(stderr, "HSA Agent page fault: %p\n",
			(void*)(uintptr_t)addr);
		longjmp(global_ctl.env, 1);
	}
}

static void hsa_profile_print(int64_t exe, int64_t total)
{
	if (global_ctl.show == false) {
		return;
	}

	double fexe = ((double)exe) / 1000000000LL;
	double ftotal = ((double)total) / 1000000000LL;

	pthread_mutex_lock(&ccmsg_lock);

	fprintf(stderr,
		"== NTHU HSAemu profile info ==\n"
		"\tnumber of compute unit = %d\n"
		"\texecution time = %lf\n"
		"\ttotal time = %lf\n",
		global_ctl.num_cus, fexe, ftotal);

	global_cc.prof.cache += hsa_cpu_prof.cache;
	global_cc.prof.cache_miss += hsa_cpu_prof.cache_miss;
	hsa_show_prof(&global_cc.prof);
	hsa_update_prof(&global_ctl.prof, &global_cc.prof);
	hsa_clear_prof(&global_cc.prof);
	memset(&hsa_cpu_prof, 0, sizeof(hsa_cpu_cache_t));

	pthread_mutex_unlock(&ccmsg_lock);
}

void hsa_block_signal(void)
{
	sigset_t set;

	sigemptyset(&set);
	sigaddset(&set, SIGUSR1);
	sigaddset(&set, SIGIO);
	sigaddset(&set, SIGALRM);
	sigaddset(&set, SIGBUS);
	pthread_sigmask(SIG_BLOCK, &set, NULL);
}

static void hsa_mntor_init_all_cu(void)
{
	global_ctl.cu_thread = calloc(global_ctl.num_cus,
		sizeof(hsa_cu_t));

	int i;
	for (i = 0; i < global_ctl.num_cus; i++) {
		hsa_cu_t *ptr = global_ctl.cu_thread + i;
		ptr->id = i;
		ptr->cu_s = IDLE;
		pthread_create(&ptr->cu_t, NULL, hsa_cu_thread_fn, ptr);
	}

	hsa_mntor_resume_cus();
}

void hsa_mntor_resume_cus(void)
{
	global_ctl.cu_busy = global_ctl.num_cus;
	pthread_cond_broadcast(&global_ctl.cu_idle);
	while (global_ctl.cu_busy)
	{
		pthread_cond_wait(&global_ctl.mntr_wait_cu,
			&global_ctl.mntr_mutx);
	}
}

static int fd_p2c[2] = {0, 0};
static int fd_c2p[2] = {0, 0};
static brig_format_t brigFile = {
	.size = 0,
	.path = "/tmp/file_"
};
#define get16bits(d) (*((const uint16_t *) (d)))

static uint32_t fast_hash (const unsigned char * data, size_t len)
{
    uint32_t hash = len, tmp;
    int rem;

    if (len <= 0 || data == NULL) return 0;

    rem = len & 3;
    len >>= 2;

    /* Main loop */
    for (;len > 0; len--) {
        hash  += get16bits (data);
        tmp    = (get16bits (data+2) << 11) ^ hash;
        hash   = (hash << 16) ^ tmp;
        data  += 2*sizeof (uint16_t);
        hash  += hash >> 11;
    }

    /* Handle end cases */
    switch (rem) {
        case 3: hash += get16bits (data);
                hash ^= hash << 16;
                hash ^= ((signed char)data[sizeof (uint16_t)]) << 18;
                hash += hash >> 11;
                break;
        case 2: hash += get16bits (data);
                hash ^= hash << 11;
                hash += hash >> 17;
                break;
        case 1: hash += (signed char)*data;
                hash ^= hash << 10;
                hash += hash >> 1;
    }

    /* Force "avalanching" of final 127 bits */
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 4;
    hash += hash >> 17;
    hash ^= hash << 25;
    hash += hash >> 6;

    return hash;
}

static bool hsa_prepare_code_cache(hsa_task_t *task)
{
	hsa_dispatch_packet_aql_t *aqlp = &task->aql;
	guest_vaddr_t kernobj_addr = (guest_vaddr_t)aqlp->kernelObjectAddress;
	guest_vaddr_t kernarg_addr = (guest_vaddr_t)aqlp->kernargAddress;
	size_t kernarg_len = task->karg_size;

	if (kernarg_len > MAX_ARG_SIZE) {
		fprintf(stderr, "invaild kernel arguments size\n");
		return false;
	}

	// check
	if (!aqlp->gridSize_x || !aqlp->workgroupSize_x ||
		!aqlp->gridSize_y || !aqlp->workgroupSize_y ||
		!aqlp->gridSize_z || !aqlp->workgroupSize_z ) {
		fprintf(stderr, "invaild work-dim\n");
		return false;
	}
	if (aqlp->groupSegmentSizeBytes > MAX_GROUP_SIZE) {
		fprintf(stderr, "too large group memory\n");
		return false;
	}

	// setting global variables
	global_cc.group_size.x = aqlp->workgroupSize_x;
	global_cc.group_size.y = aqlp->workgroupSize_y;
	global_cc.group_size.z = aqlp->workgroupSize_z;
	global_cc.flat_group_size =
		SIZE_DIM2FLAT(global_cc.group_size);

	global_cc.grid_size.x = ALIGNMENT_DEV(aqlp->gridSize_x,
		aqlp->workgroupSize_x);
	global_cc.grid_size.y = ALIGNMENT_DEV(aqlp->gridSize_y,
		aqlp->workgroupSize_y);
	global_cc.grid_size.z = ALIGNMENT_DEV(aqlp->gridSize_z,
		aqlp->workgroupSize_z);
	global_cc.flat_grid_size =
		SIZE_DIM2FLAT(global_cc.grid_size);

	global_cc.finish_id = 0;

	// load brig
	brigFile.size = task->brig_size;
	if (brigFile.size > MAX_BIN_SIZE || brigFile.size == 0) {
		fprintf(stderr, "invaild kernel object file, "
			"brig size = %zu\n", brigFile.size);
		return false;
	}

	hsa_copy_from_guest(brigFile.bin, kernobj_addr, brigFile.size);

	// object cache
	uint32_t tmp_hash = fast_hash(brigFile.bin, brigFile.size);

	static uint32_t prev_hash = 0;
	if (tmp_hash == prev_hash) {
		goto LOAD_KERARG;
	} else {
		prev_hash = tmp_hash;
	}

	// gen rand file name
	const char randStr[] = "abcdefghijklmnopqrstuvwxyz";
	const char pathStr[] = "/tmp/file_";
	const size_t randStrLen = sizeof(randStr) - 1;

	char *randStrPtr = brigFile.path + (sizeof(pathStr) - 1);
	int i = 0;
	for (i = 0; i < RAND_STR_SIZE; i++)
	{
		randStrPtr[i] = randStr[ rand()%randStrLen ];
	}
	randStrPtr[RAND_STR_SIZE - 1] = '\0';

	// convert brig to object file
	ssize_t err = write(fd_p2c[1], &brigFile, sizeof(brig_format_t));
	if (err != sizeof(brig_format_t)) {
		HSA_DEBUG_LOG("output BRIG");
	}
	err = read(fd_c2p[0], brigFile.path, MAX_STR_SIZE);
	if (err != MAX_STR_SIZE) {
		HSA_DEBUG_LOG("read object path");
	}

	// read obj
	FILE *fd = fopen(brigFile.path, "rb");
	if (fd == NULL) {
		HSA_DEBUG_LOG("open object");
		return false;
	}
	int ferr = fseek(fd, 0, SEEK_END);
	if (ferr != 0) {
		HSA_DEBUG_LOG("fseek object");
		return false;
	}
	long int obj_size = ftell(fd);
	if (obj_size != -1) {
		global_cc.size = obj_size;
	} else {
		HSA_DEBUG_LOG("ftell object");
		return false;
	}
	rewind(fd);
	if (fread(global_cc.code, global_cc.size, 1, fd) != 1) {
		HSA_DEBUG_LOG("read object");
		return false;
	}
	ferr = fclose(fd);
	if (ferr != 0) {
		HSA_DEBUG_LOG("close object");
		return false;
	}
	ferr = remove(brigFile.path);
	if (ferr != 0) {
		HSA_DEBUG_LOG("delete object");
		return false;
	}

LOAD_KERARG:
	// link-loader
	global_cc.kentry = cc_producer(global_cc.code,
		task->name);
	if (global_cc.kentry == NULL) {
		HSA_DEBUG_LOG("link fault");
		return false;
	}

	// load kernel arg
	hsa_copy_from_guest(global_cc.karg, kernarg_addr, kernarg_len);
	return true;
}
static hsa_task_t* hsa_get_task(void)
{
RETRY:
	if (TAILQ_EMPTY(&global_q.q_head)== false) {
		pthread_spin_lock(&global_q.q_lock);
		hsa_task_t *ptr = TAILQ_FIRST(&global_q.q_head);
		TAILQ_REMOVE(&global_q.q_head, ptr, next);
		global_q.q_size--;
		pthread_spin_unlock(&global_q.q_lock);
		hsa_mntor_update(ptr);
		return ptr;
	} else {
		global_ctl.mntr_s = IDLE;
		pthread_cond_wait(&global_ctl.wait_mntr,
			&global_ctl.mntr_mutx);
		goto RETRY;
	}

	HSA_DEBUG_LOG("should not be here\n");
	return NULL;
}

pid_t hconverter_pid = 0;
static void hsa_fork_convert(void)
{
	srand(time(NULL));

	pipe(fd_p2c);
	pipe(fd_c2p);
	pid_t pid = fork();

	if (pid == -1) {
		HSA_DEBUG_LOG("fork convert");
	}

	if (pid) {
		hconverter_pid = pid;
		return;
	}

	int err = dup2(fd_p2c[0], 0);
	if (err == -1) {
		HSA_DEBUG_LOG("redirect IO p2c");
	}

	err = dup2(fd_c2p[1], 1);
	if (err == -1) {
		HSA_DEBUG_LOG("redirect IO c2p");
	}

	err = execlp("brig2obj_pipe", "brig2obj_pipe", NULL);
	if (err == -1) {
		HSA_DEBUG_LOG("launch brig2obj_pipe");
	}

	exit(EXIT_FAILURE);
}

void hsa_kill_convert(void)
{
	if (hconverter_pid == 0) {
		return;
	}

	if (kill(hconverter_pid, SIGKILL)) {
			perror("kill brig2obj_pipe");
	}
}
static void *hsa_mntor_thread_fn(void *arg)
{
	per_thread_type = HSA_AGENT_THREAD;

	// always get the lock first
	pthread_mutex_lock(&global_ctl.mntr_mutx);

	// initialize code cache
	pthread_spin_init(&global_cc.ref_mutx, PTHREAD_PROCESS_PRIVATE);
	int ret = mprotect(global_cc.code, MAX_BIN_SIZE,
		PROT_READ | PROT_WRITE | PROT_EXEC);
	if (ret != 0) {
		HSA_DEBUG_LOG("set memory executable");
	}

	// initialize variable from emulator
	hsa_init_mntor();

	// create queue
	pthread_spin_init(&global_q.q_lock,
		PTHREAD_PROCESS_PRIVATE);
	TAILQ_INIT(&global_q.q_head);
	global_q.q_size = 0;

	// fork Hconverter
	hsa_fork_convert();

	// block signal
	hsa_block_signal();

	// init all CU threads
	hsa_mntor_init_all_cu();

	// wake up PQEMU
	global_ctl.mntr_s = IDLE;
	pthread_cond_signal(&global_ctl.wait_mntr);

	// start loop
	while (1)
	{
		if (setjmp(global_ctl.env)) {
			//Prepare code cache fault ?
			goto HSA_ABORT_TASK;
		}

		hsa_task_t *task = hsa_get_task();
		uint64_t prepare = hsa_get_clock();
		hsa_dispatch_packet_aql_t *aqlptr = &task->aql;

		hsa_opaque_signal completionSignal;
		hsa_copy_from_guest(&completionSignal, aqlptr->completionSignal, sizeof(hsa_opaque_signal));
		if (completionSignal.signal_value != 0) {
			HSA_DEBUG_LOG("invalid AQL\n");
			break;
		}

		if(hsa_prepare_code_cache(task)){
			uint64_t start = hsa_get_clock();
			hsa_mntor_resume_cus();
			uint64_t end = hsa_get_clock();

			hsa_profile_print(end - start,
				end - prepare);
		}else{
			fprintf(stderr, "kernel entry is NULL\n");
			goto HSA_ABORT_TASK;
		}

      	guest_vaddr_t vaddr = aqlptr->completionSignal;
		completionSignal.signal_value = 1;
		hsa_copy_to_guest(vaddr,
			&completionSignal, sizeof(hsa_opaque_signal));

HSA_ABORT_TASK:
		// job finished, ready again
		if (task->env_size) {
			free(task->env);
		}
		free(task);
	}

	return NULL;
}

static void *counter_peripherals_thread_fn(void *arg)
{
	uint64_t currt, old=0;

	while(1){
		currt = hsa_get_clock();
		/*
		if(old != 0)
			fprintf(stderr,"time(ns): %ld\n",currt - old);
			*/
		old = old * 1;
		old = currt;
	}
	
	return NULL;
}

void hsa_create_mntor(void)
{
	pthread_mutex_lock(&global_ctl.mntr_mutx);
	pthread_create(&global_ctl.mntr, NULL,
		hsa_mntor_thread_fn, NULL);

	while (global_ctl.mntr_s != IDLE) {
		pthread_cond_wait(&global_ctl.wait_mntr,
			&global_ctl.mntr_mutx);
	}

	pthread_mutex_unlock(&global_ctl.mntr_mutx);

	pthread_create(&counter_clock_t, NULL,
			counter_peripherals_thread_fn, NULL);
}
