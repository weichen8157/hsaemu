#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <thread_pool.h>

list_thread_pool* alloc_thread_pool_head(void)
{
	list_thread_pool *ptr = (list_thread_pool*)calloc(1, 
		sizeof(list_thread_pool));
	pthread_mutex_init(&ptr->mutx, NULL);
	TAILQ_INIT(&ptr->head);
	return ptr;
}

static void *thread_loop(void *arg)
{
	thread_context *ptr = (thread_context*)arg;
	ptr->id = (uintptr_t)pthread_self();
	pthread_mutex_lock(&ptr->mutx);
	// signal master, thread already create
	pthread_cond_signal(&ptr->cond);
	
	while(1)
	{	
		ptr->state = THREAD_WAIT;
		
		THREAD_HALT(&ptr->cond, &ptr->mutx);
		
		ptr->state = THREAD_RUN;

		if (ptr->report) {
			thread_context *master = ptr->report;
			THREAD_LOCK(&master->mutx);

			pthread_cond_signal(&master->cond);

			THREAD_FREE(&master->mutx);
		}

		if (ptr->func) {
			ptr->func(arg);
		}
	}
	return NULL;
}

static thread_pool* alloc_thread_pool_mode(list_thread_pool *list, 
                                           int amount,
                                           int lock_mode)
{
	if (!list || amount <= 0) return NULL;
	if (lock_mode) pthread_mutex_lock(&list->mutx);

	thread_pool *ptr = (thread_pool*)calloc(1, 
		sizeof(thread_pool));
	thread_context *ths_ptr = (thread_context*)calloc(amount, 
		sizeof(thread_context));

	int i = 0;
	for (i = 0; i < amount; i++)
	{
		thread_context *tmp = ths_ptr + i;
		pthread_cond_init(&tmp->cond, NULL);
		pthread_mutex_init(&tmp->mutx, NULL);
		pthread_mutex_lock(&tmp->mutx);
		pthread_create(&tmp->th, NULL, thread_loop, tmp);
		pthread_cond_wait(&tmp->cond, &tmp->mutx);
		tmp->state = THREAD_UNREG;
		pthread_mutex_unlock(&tmp->mutx);
	}
	// update
	ptr->amount = amount;
	ptr->remain = amount;
	ptr->ths_ptr = ths_ptr;
	list->amount++;
	TAILQ_INSERT_HEAD(&list->head, ptr, next);

	if (lock_mode) pthread_mutex_unlock(&list->mutx);
	return ptr;
}

thread_pool* alloc_thread_pool(list_thread_pool *list, 
                               int amount)
{
	return alloc_thread_pool_mode(list, amount, 1);
}

list_thread_block* alloc_thread_block_head(void)
{
	list_thread_block* ptr = (list_thread_block*)calloc(1,
		sizeof(list_thread_block));
	pthread_mutex_init(&ptr->mutx, NULL);
	TAILQ_INIT(&ptr->head);
	return ptr;
}

static void fill_thread_block(thread_pool *pool, 
                              thread_context **block,
                              int amount,
                              uint8_t *buf,
                              int offset)
{
	thread_context *ths_ptr = pool->ths_ptr;
	int i = 0;
	int count = 0;
	for(i=0; i<pool->amount; i++)
	{
		if (ths_ptr->state == THREAD_UNREG) {
			ths_ptr->state = THREAD_WAIT;
			
			//fprintf(stderr, "%p have %p, %s\n", 
			//	(void*)ths_ptr->id, &ths_ptr->mutx, __func__);

			if (buf) {
				ths_ptr->data = (void*)buf;
				buf += offset;
			}
			block[count] = ths_ptr;
			count++;
			if (count == amount) break;
		}
		ths_ptr++;
	}
	pool->remain -= amount;
}

thread_block* regs_thread_block(list_thread_block *block_list,
                                list_thread_pool *pool_list,
                                int amount,
                                int size)
{
	if (!block_list || !pool_list || amount<=0) return NULL;

	pthread_mutex_lock(&pool_list->mutx);
	pthread_mutex_lock(&block_list->mutx);

	thread_block *ptr = (thread_block*)calloc(1, 
		sizeof(thread_block));
	thread_context **ths_bk = (thread_context**)calloc(amount, 
		sizeof(thread_context*));
	uint8_t *buf = NULL;
	if (size) buf = calloc(amount, size);
	
	thread_pool *pool = NULL;
	TAILQ_FOREACH(pool, &pool_list->head, next)
	{
		if (pool->remain >= amount) {
			fill_thread_block(pool, ths_bk, amount, buf, size);
			goto SKIP_ALLOC_THREAD_POOL;
		}
	}
	
	// it means that need allocate new thread pool
	pool = alloc_thread_pool_mode(pool_list, 
		(amount > 1024) ? amount : 1024, 0);
	fill_thread_block(pool, ths_bk, amount, buf, size);

SKIP_ALLOC_THREAD_POOL:
	ptr->amount = amount;
	ptr->ths_bk = ths_bk;
	ptr->list_from = pool_list;
	ptr->pool_from = pool;
	block_list->amount++;
	TAILQ_INSERT_TAIL(&block_list->head, ptr, next);
	pthread_mutex_unlock(&block_list->mutx);
	pthread_mutex_unlock(&pool_list->mutx);	
	return ptr;
}

void unregs_thread_block(list_thread_block *list,
                         thread_block *arg)
{
	if (!list || !list->amount || !arg) return;

	pthread_mutex_lock(&list->mutx);

	thread_context *ptr = *(arg->ths_bk);
	if (ptr->data) free(ptr->data);

	int i = 0;
	for(i=0; i<arg->amount; i++)
	{
		ptr = *(arg->ths_bk + i);
		pthread_mutex_lock(&ptr->mutx);
		ptr->th_type = 0;
		ptr->state = THREAD_UNREG;
		ptr->data = NULL;
		ptr->func = NULL;
		ptr->report = NULL;
		pthread_mutex_unlock(&ptr->mutx);
	}
	
	// check arg in list
	thread_block *curt = NULL;
	TAILQ_FOREACH(curt, &list->head, next)
	{
		if (curt == arg) {
			// report to thread pool
			list_thread_pool *head = arg->list_from;
			thread_pool *pool = arg->pool_from;
			pthread_mutex_lock(&head->mutx);
			pool->remain += arg->amount;
			pthread_mutex_unlock(&head->mutx);
			// modify list thread block
			list->amount--;
			TAILQ_REMOVE(&list->head, arg, next);
			free(arg);
			pthread_mutex_unlock(&list->mutx);
			return;
		}
	}

	// should not be here
	abort();
}

void unregs_num_thread_block(list_thread_block *list,
                             int amount)
{
	if (!list || !amount) return;

	int i = 0;
	for(i=0; i<amount; i++) 
	{
		thread_block *ptr = TAILQ_FIRST(&list->head);
		if (ptr) unregs_thread_block(list, ptr);
		else return;
	}
}

void wakeup_thread(thread_context *arg,
                   int type,
                   void *(*func)(void*),
                   void *data,
                   thread_context *report)
{
	if (!arg) return;

	THREAD_LOCK(&arg->mutx);

	arg->state = THREAD_READY; 
	arg->th_type = type;
	arg->func = func;
	arg->data = data;
	arg->report = report;
	pthread_cond_signal(&arg->cond);

	THREAD_FREE(&arg->mutx);

	if (report) {
		THREAD_HALT(&report->cond,
				&report->mutx);
		while (arg->state != THREAD_RUN)
		{
			pthread_cond_signal(&arg->cond);
			THREAD_HALT(&report->cond,
				&report->mutx);
		}
	}
}

void sleep_thread(thread_context *arg)
{
	arg->state = THREAD_WAIT;
		
	THREAD_HALT(&arg->cond, &arg->mutx);

	arg->state = THREAD_RUN;

	if (arg->report) {
		thread_context *master = arg->report;
		THREAD_LOCK(&master->mutx);
		pthread_cond_signal(&master->cond);
		THREAD_FREE(&master->mutx);
	}
}