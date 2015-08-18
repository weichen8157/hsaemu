#include <stdint.h>
#include <pthread.h>
#include <thread_pool.h>
#include <comm.h>

void cvtFidToDIM3(DIM3 *id, 
                const int Fid,
                const DIM3 *size)
{
	int xy_size = size->dim[0] * size->dim[1];
	id->dim[2] = Fid / xy_size;
	int tmp = Fid % xy_size;
	id->dim[1] = tmp / (size->dim[0]);
	id->dim[0] = tmp % (size->dim[0]);
}