#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "hsa_comm_def.h"
#include "hsa_vmdep.h"
#include "hsa_help_info.h"

extern hsa_obj_t global_cc;
extern hsa_debug_mask_t global_debug;
extern __thread hsa_wg_cntxt_t cu_context;

static void insert_ccmsg(guest_vaddr_t vaddr)
{
	hsa_wi_cntxt_t *ptr = cu_context.curt_wi_ptr;
	hsa_ccmsg_t *buf = malloc(sizeof(hsa_ccmsg_t));
	if (buf == NULL) {
		HSA_DEBUG_LOG("allocate ccmsg buffer");
	}

	buf->v = (uintptr_t)vaddr;
	buf->gid = cu_context.id;
	buf->tid = ptr->id;
	fprintf(stderr,"%4d %4d %4d %4d %4d %4d context:%16p\n", buf->gid.x, buf->gid.y, buf->gid.z,
			buf->tid.x, buf->tid.y, buf->tid.z, (void *)buf->v);

	TAILQ_INSERT_TAIL(&cu_context.ccmsg_q.q_head,
		buf, next);
	cu_context.ccmsg_q.q_size++;
}

void hsa_printf_32(int vaddr)
{
	if (global_debug.enable) {
		int eq = 0;
		hsa_wi_cntxt_t *ptr = cu_context.curt_wi_ptr;
		eq = memcmp(&global_debug.gid,
			&cu_context.id, sizeof(hsa_dim3_t));
		eq += memcmp(&global_debug.tid,
			&ptr->id, sizeof(hsa_dim3_t));
		if (eq) return;
	}
	insert_ccmsg(vaddr);
}

void hsa_printf_64(uintptr_t vaddr)
{
	hsa_wi_cntxt_t *ptr = cu_context.curt_wi_ptr;
	hsa_ccmsg_t *buf = malloc(sizeof(hsa_ccmsg_t));
	if (buf == NULL) {
		HSA_DEBUG_LOG("allocate ccmsg buffer");
	}

	buf->v = vaddr;
	buf->gid = cu_context.id;
	buf->tid = ptr->id;

	TAILQ_INSERT_TAIL(&cu_context.ccmsg_q.q_head,
		buf, next);
	cu_context.ccmsg_q.q_size++;
}

void hsa_ins_count(uint32_t count)
{
	cu_context.prof.ins += count;
}

/*************************************************************************/

#define GROUP_ID_CHECK(__idx) \
	do { \
		if ( __idx > MAX_WORKGROUP_DIM) { \
			fprintf(stderr, "invalid work-group dimension: %s\n", \
				__func__); \
			return 0; \
		} \
	}while(0)

#define WORK_ID_CHECK(__idx) \
	do { \
		if ( __idx > MAX_WORKGROUP_DIM) { \
			fprintf(stderr, "invalid work-item dimension: %s\n", \
				__func__); \
			return 0; \
		} \
	}while(0)

uint32_t helper_WorkItemId(uint32_t idx)
{
	WORK_ID_CHECK(idx);
	hsa_wi_cntxt_t *ptr = cu_context.curt_wi_ptr;
	return ptr->id.idx[idx];
}

uint32_t helper_WorkItemAId(uint32_t idx)
{
	WORK_ID_CHECK(idx);
	return (helper_WorkGroupId(idx) * helper_WorkGroupSize(idx) +
		helper_WorkItemId(idx));
}

uint32_t helper_WorkGroupId(uint32_t idx)
{
	WORK_ID_CHECK(idx);
	return cu_context.id.idx[idx];
}

uint32_t helper_WorkGroupSize(uint32_t idx)
{
	WORK_ID_CHECK(idx);
	return global_cc.group_size.idx[idx];
}

uint32_t helper_WorkNDRangegroups(uint32_t idx)
{
	GROUP_ID_CHECK(idx);
	return global_cc.grid_size.idx[idx];
}

uint32_t helper_WorkNDRangesize(uint32_t idx)
{
	WORK_ID_CHECK(idx);
	return (helper_WorkNDRangegroups(idx) *
		helper_WorkGroupSize(idx));
}

