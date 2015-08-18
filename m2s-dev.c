#include <mem-system/memory.h>
#include "hsa_comm_def.h"
#include "hsa_vmdep.h"
#include "hsa_agent.h"
#include "topology.h"
#include "m2s-dev.h"

static struct mem_t *hsa_global_mem = NULL;
extern __thread hsa_wg_cntxt_t cu_context;
extern __thread void *private_cpu_state;

// init HSA device
void hsa_component_init(void)
{
	hsa_create_mntor();
}
void hsa_init_mntor(void)
{
}
void hsa_mntor_update(void *cpu_state)
{
	// should only be called by HSA monitor thread
	hsa_task_t *ptr = (hsa_task_t*)cpu_state;
	hsa_global_mem = (void*)ptr->env;
}
void hsa_init_cu(void *cpu_state)
{
	private_cpu_state = hsa_global_mem;
}
void hsa_cu_update(void *cpu_state)
{
	private_cpu_state = hsa_global_mem;
}

// transfer guest virtual address to physical addr
uintptr_t hsa_get_phy_addr_global(void *cpu_state, guest_vaddr_t addr)
{
	struct mem_page_t *page = NULL;
	unsigned int offset = 0;

	page = mem_page_get(private_cpu_state, addr);
	offset = addr & (MEM_PAGE_SIZE - 1);

	return (uintptr_t)(page->data + offset);
}

uintptr_t hsa_get_phy_addr_segment(
	guest_vaddr_t addr,
	int segment)
{
	if (segment == HSA_GLOBAL) {
		return hsa_get_phy_addr_global(NULL, addr);
	} else {
		GROUP_MEM_CHECK(addr, MAX_GROUP_SIZE);
		return (uintptr_t)(cu_context.buf + addr);		
	}

	// should not be here.
	abort();
	return 0;
}

// copy data
size_t hsa_copy_to_guest(guest_vaddr_t dst, void *src, const size_t size)
{
	mem_write(hsa_global_mem, dst, size, src);
	return 0;
}
size_t hsa_copy_from_guest(void *dst, guest_vaddr_t src, const size_t size)
{
	mem_read(hsa_global_mem, src, size, dst);
	return 0;
}

// insert command
int hsa_enqueue_cmd(void *cpu_state, guest_vaddr_t addr)
{
	hsa_global_mem = cpu_state;
	return 0;
}

// get time
int64_t hsa_get_clock(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

void *gen_hsa_topology(void)
{
	hsa_agent_entry_t *cpu = NULL,
		  *hsa_component = NULL,
		  *m2s_gpu = NULL;

	hsa_region_entry_t *sys_mem = NULL;

	cpu = registe_agent("PQEMU-CPU",
		0,
		HSA_COMPONENT_FEATURE_DISPATCH,
		HSA_DEVICE_TYPE_CPU,
		"NTHU SSLAB",
		0, NULL, 0, 0, 0, 0, 0, 0);

	hsa_component = registe_agent("HSA Component",
		1,
		HSA_COMPONENT_FEATURE_DISPATCH,
		HSA_DEVICE_TYPE_GPU,
		"NTHU SSLAB",
		1, NULL, 3, 3, 64, 0, 0, 0);

	m2s_gpu = registe_agent("Southern Islands",
		2,
		HSA_COMPONENT_FEATURE_DISPATCH,
		HSA_DEVICE_TYPE_GPU,
		"Multi2Sim",
		32, NULL, 3, 3, 64, 0, 0, 0);
	sys_mem = registe_region("system memory",
		0, 1073741824 /* 1 GB */,
		0, 1073741824,
		HSA_SEGMENT_GLOBAL | HSA_SEGMENT_GROUP | HSA_SEGMENT_KERNARG,
		0, NULL);

	agent_link_memory(cpu, sys_mem);
	agent_link_memory(hsa_component, sys_mem);
	agent_link_memory(m2s_gpu, sys_mem);

	return gen_topology();
}

