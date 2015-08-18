#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include "hw/sysbus.h"
#include "hw/hw.h"
#include "sysemu/sysemu.h"
#include "hsa_vmdep.h"
#include "pqemu-dev.h"
#include "topology.h"

static char hsa_dev_name[] = "NTHU HSA device";

void hsa_dev_init(void)
{
	sysbus_create_simple(hsa_dev_name,
		HSA_BASE_ADDR,
		NULL);
	hsa_component_init();
}

static uint64_t hsa_dev_read(void *opaque,
	hwaddr addr,
	unsigned size)
{
	hsa_state_t *status = (hsa_state_t *)opaque;

	hsa_topology_header_t *head =
		(hsa_topology_header_t*)status->data;
	uintptr_t raddr = ((uintptr_t)addr +
			(uintptr_t)size - 1);
	if (raddr > head->total_size ||
		raddr > HSA_MMIO_SIZE) {
		fprintf(stderr, "should not read this addr %p\n",
			(void*)(uintptr_t)(HSA_BASE_ADDR + addr));
		return 0;
	}

	uint64_t ret = 0;
	raddr = (uintptr_t)status->data;
	raddr += addr;

	switch (size)
	{
	case 1:
		ret = *((uint8_t*)raddr);
		break;
	case 2:
		ret = *((uint16_t*)raddr);
		break;
	case 4:
		ret = *((uint32_t*)raddr);
		break;
	case 8:
		ret = *((uint64_t*)raddr);
		break;
	default:
		fprintf(stderr, "should not be here %s:%d\n",
			__func__, __LINE__);
		return 0;
	}

	//fprintf(stderr, "%s: addr = %lu, value = %"PRIu64"\n",
	//	__func__, (unsigned long)addr, ret);
	return ret;
}

static void hsa_dev_write(void *opaque,
	hwaddr addr,
	uint64_t value,
	unsigned size)
{
	fprintf(stderr, "should not write this addr "
		"(base = %p, offset = %"PRIu64")\n",
		(void*)HSA_BASE_ADDR, addr);
}

static const MemoryRegionOps hsa_dev_ops = {
	.read = hsa_dev_read,
	.write = hsa_dev_write,
	.endianness = DEVICE_NATIVE_ENDIAN,
};

static void *gen_hsa_topology(void)
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

static int hsa_dev_init_sysbus(SysBusDevice *dev)
{
	hsa_state_t *status = OBJECT_CHECK(hsa_state_t,
		dev,
		hsa_dev_name);

	hsa_topology_header_t *head = gen_hsa_topology();
	assert(head->total_size < HSA_MMIO_SIZE);
	memcpy(status->data, head, head->total_size);
	free(head);

	memory_region_init_io(&status->iomem,
		OBJECT(status),
		&hsa_dev_ops,
		status,
		hsa_dev_name,
		HSA_MMIO_SIZE);

	sysbus_init_mmio(dev,
		&status->iomem);

	return 0;
}

static const VMStateDescription hsa_vmstate = {
	.name = hsa_dev_name,
	.version_id = 1,
	.minimum_version_id = 1,
	.minimum_version_id_old = 1,
	.fields = (VMStateField[]) {
		VMSTATE_UINT8_ARRAY(data,
			hsa_state_t,
			HSA_MMIO_SIZE),
		VMSTATE_END_OF_LIST()
	}
};

static void hsa_dev_reset(DeviceState *dev)
{
}

static void hsa_dev_class_init(ObjectClass *class, void *data)
{
	DeviceClass *dc = DEVICE_CLASS(class);
	SysBusDeviceClass *sub = SYS_BUS_DEVICE_CLASS(class);

	sub->init = hsa_dev_init_sysbus;
	dc->vmsd = &hsa_vmstate;
	dc->reset = hsa_dev_reset;
}

static const TypeInfo hsa_dev_info = {
	.name          = hsa_dev_name,
	.parent        = TYPE_SYS_BUS_DEVICE,
	.instance_size = sizeof(hsa_state_t),
	.class_init    = hsa_dev_class_init,
};

static void hsa_dev_register_types(void)
{
	type_register_static(&hsa_dev_info);
}

type_init(hsa_dev_register_types);

#include "qemu-common.h"
#include "qemu/option.h"
#include "qemu/timer.h"
#include "hsa_comm_def.h"
#include "hsa_cache_def.h"
#include "hsa_vmdep.h"
#include "hsa_agent.h"
#include "hsa_helper.h"
#include "hsa_cmd.h"
#include "hsa_m2s.h"

extern pthread_rwlock_t cutlb_lock;
static CPUArchState *global_cpu_state = NULL;
extern __thread void *private_cpu_state;
extern hsa_ctl_t global_ctl;
extern __thread hsa_wg_cntxt_t cu_context;
extern share_cache_t hsa_cache;

QemuOptsList hsa_core_opts = {
    .name = "hsa-opts",
    .implied_opt_name = "cus",
    .merge_lists = true,
    .head = QTAILQ_HEAD_INITIALIZER(hsa_core_opts.head),
    .desc = {
        {
            .name = "cus",
            .type = QEMU_OPT_NUMBER,
        },
        {
            .name = "waveform",
            .type = QEMU_OPT_NUMBER,
        },
        {
            .name = "pes",
            .type = QEMU_OPT_NUMBER,
        },
		{
			.name = "hq",
			.type = QEMU_OPT_NUMBER,
		},

        { /*End of list */ }
    },
};

void hsa_parse(QemuOpts *opts)
{
	int cus	     = qemu_opt_get_number(opts, "cus", 0);
	int waveform = qemu_opt_get_number(opts, "waveform", 0);
	int pes      = qemu_opt_get_number(opts, "pes", 0);
	int hq       = qemu_opt_get_number(opts, "hq", 0);

	cus = cus > 0 ? cus : 1;
	global_ctl.num_cus = cus;

	// waveform size should be 32 or 64
	waveform = 32;
	// process elements
	pes = 32;
	// hardware queue
	hq = 1;

	fprintf(stderr, "HSA component create:\n"
		"\tnumber of Compute Units = %d\n"
		"\twaveform size = %d\n"
		"\tnumber of Process Elements = %d\n"
		"\tnumber of hQ = %d\n",
		cus, waveform, pes, hq);
}

uintptr_t hsa_get_phy_addr_global(
	void *cpu_state,
	guest_vaddr_t addr)
{
	uintptr_t paddr = 0;
	HSA_GET_PHYADDR(paddr, cpu_state, addr);
	return paddr;
}

uintptr_t hsa_get_phy_addr_segment(
	guest_vaddr_t addr,
	int segment)
{
	uintptr_t phy_addr = 0;

	switch (segment){
		case HSA_GLOBAL:
			HSA_GET_PHYADDR2(phy_addr, private_cpu_state,
				addr, &cu_context.prof);
			break;
		case HSA_GROUP:
			GROUP_MEM_CHECK(addr, MAX_GROUP_SIZE);
			phy_addr = (uintptr_t)(cu_context.buf +  addr);
			break;
		case HSA_PRIVATE:
		case HSA_READ_ONLY:
		{
			hsa_wi_cntxt_t *ptr = cu_context.curt_wi_ptr;
			ITEM_MEM_CHECK(addr, MAX_ITEM_SIZE);
			phy_addr = (uintptr_t)(ptr->buf + addr);
			break;
		}
		case HSA_KERNARG:
			break;
		default:
			HSA_DEBUG_LOG("Get Physical Address In Wrong Segment!\n");
			break;
	}

	uint64_t tmp_tag = (uint64_t)phy_addr >> 6;
	uint64_t cache_index = tmp_tag & 0x1FF;
	
	tmp_tag = tmp_tag >> 9;
	
	pthread_spin_lock(&hsa_cache.cache_lock);
	if(hsa_cache.cache_tag[cache_index] != tmp_tag){
		hsa_cache.cache_tag[cache_index] = tmp_tag;
		cu_context.prof.cache_miss++;
	}
	cu_context.prof.cache++;
	pthread_spin_unlock(&hsa_cache.cache_lock);

	return phy_addr;
}

size_t hsa_copy_to_guest(
	guest_vaddr_t dst,
	void *src,
	const size_t size)
{
	size_t left_size = size;

	while (left_size > 0) {
		uintptr_t phy_addr = hsa_get_phy_addr_global(private_cpu_state, dst);

		if (phy_addr) {
			size_t valid_len = TARGET_PAGE_SIZE - (phy_addr & ~TARGET_PAGE_MASK);
			if (valid_len > left_size) valid_len = left_size;
			memcpy((void *)phy_addr, src, valid_len);
			dst += valid_len;
			src = ((uint8_t*)src) + valid_len;
			left_size -= valid_len;
		}
	}

	if(left_size){
		HSA_DEBUG_LOG("copy from guest fault\n");
	}

	return (size - left_size);
}

size_t hsa_copy_from_guest(
	void *dst,
	guest_vaddr_t src,
	const size_t size)
{
	size_t left_size = size;

	while (left_size > 0) {
		uintptr_t phy_addr = hsa_get_phy_addr_global(private_cpu_state, src);

		if (phy_addr) {
			size_t valid_len = TARGET_PAGE_SIZE - (phy_addr & ~TARGET_PAGE_MASK);
			if (valid_len > left_size) valid_len = left_size;
			memcpy(dst, (void *)phy_addr, valid_len);
			dst = ((uint8_t*)dst) + valid_len;
			src += valid_len;
			left_size -= valid_len;
		}
	}

	if(left_size){
		HSA_DEBUG_LOG("copy from guest fault\n");
	}

	return (size - left_size);
}

enum m2s_abi_call_t
{
	opencl_abi_invalid,
#define OPENCL_ABI_CALL(__name, __code) __name = __code,
#include "opencl.dat"
#undef OPENCL_ABI_CALL
	opencl_abi_call_count
};

static int hsa_remote_m2s(void *env, hsa_cmd_t *cmd)
{
	// only vCPU thread can call this function
	void *data = NULL;
	guest_vaddr_t vaddr = 0;
	size_t size = 0;

	switch(cmd->op) {
		case si_program_set_binary:
			vaddr = cmd->pcreate_bin.bin;
			size = cmd->pcreate_bin.size;
			break;
		case si_kernel_create:
			vaddr = cmd->kcreate.func_name;
			size = cmd->kcreate.size;
			break;
		case si_kernel_set_arg_value:
			vaddr = cmd->karg.value;
			size = cmd->karg.size;
			break;
	}

	if (vaddr && size) {
		data = malloc(size);
		hsa_copy_from_guest(data, vaddr, size);
	}

	if (cmd->op == si_ndrange_initialize) {
		data = malloc(sizeof(int) * 9);
		int *ndr_arg = data;
		size_t cp_size = sizeof(int) * 3;
		hsa_copy_from_guest(ndr_arg,
			cmd->ndr_init.offset, cp_size);
		ndr_arg += 3;
		hsa_copy_from_guest(ndr_arg,
			cmd->ndr_init.global, cp_size);
		ndr_arg += 3;
		hsa_copy_from_guest(ndr_arg,
			cmd->ndr_init.local, cp_size);
	}

	int ret = hsa_m2s_call(cmd, data);

	if(data) {
		free(data);
	}

	return ret;
}

int hsa_enqueue_cmd(
	void *cpu_state,
	guest_vaddr_t addr)
{
	hsa_cmd_t cmd;
	private_cpu_state = cpu_state;
	hsa_copy_from_guest(&cmd, addr,
		sizeof(hsa_cmd_t));

	int ret = -1;

	if (cmd.dev == nthu_hsa_component) {
		if (cmd.op != hsa_abi_enqueue_aql) {
			return -1;
		}
		ret = hsa_enqueue(cpu_state, sizeof(CPUArchState), &cmd);
	} else if (cmd.dev == m2s_southern_islands) {
		ret = hsa_remote_m2s(cpu_state, &cmd);
	}
	else if(cmd.dev == nthu_hsa_agent){
		ret = hsa_service_enqueue(cpu_state, sizeof(CPUArchState), &cmd);
	}

	return ret;
}

// use to init multi2sim
struct _arg_entry {
	char *str;
	size_t size;
	TAILQ_ENTRY(_arg_entry) next;
};
typedef struct _arg_entry vm_arg_t;
static TAILQ_HEAD(_arg_list, _arg_entry) _arg_list_head =
	TAILQ_HEAD_INITIALIZER(_arg_list_head);
static int _arg_list_amount = 0;

static void add_opt(const char *str)
{
	assert(str != NULL);

	size_t size = strlen(str);

	assert(size > 0);

	vm_arg_t* n = malloc(sizeof(vm_arg_t));
	n->str = malloc(size+1);
	n->size = size + 1;
	strcpy(n->str, str);
	n->str[size] = '\0';

	_arg_list_amount++;
	TAILQ_INSERT_TAIL(&_arg_list_head, n, next);
}

void hsa_component_init(void)
{
	// only main thread can call this function
	hsa_create_mntor();

	// init multi2sim
	add_opt("/tmp");
	add_opt("--si-sim");
	add_opt("detailed");
	//add_opt("--si-report");
	//add_opt("HSAemu-M2S-SI.report");
	//add_opt("--mem-report");
	//add_opt("HSAemu-M2S-mem.report");

	char **argv = malloc(_arg_list_amount * sizeof(char**));

	vm_arg_t *n = NULL;
	int i = 0;
	TAILQ_FOREACH(n, &_arg_list_head, next)
	{
		argv[i] = n->str;
		i++;
	}

	hsa_m2s_init(_arg_list_amount, argv);
}

extern hsa_debug_mask_t global_debug;
void hsa_set_debug(Monitor *mon, const QDict *qdict)
{
	global_debug.enable = qdict_get_bool(qdict, "status");
}
void hsa_set_debug_mask(Monitor *mon, const QDict *qdict)
{
	global_debug.gid.x = qdict_get_int(qdict, "gidx");
	global_debug.gid.y = qdict_get_int(qdict, "gidy");
	global_debug.gid.z = qdict_get_int(qdict, "gidz");

	global_debug.tid.x = qdict_get_int(qdict, "tidx");
	global_debug.tid.y = qdict_get_int(qdict, "tidy");
	global_debug.tid.z = qdict_get_int(qdict, "tidz");
}

void hsa_get_debug_info(Monitor *mon, const QDict *qdict)
{
	const char on[] = "on";
	const char off[] = "off";

	fprintf(stderr, "debug option:\n\t%s\n"
		"debug mask:\n"
		"\twork-group ID: %4d %4d %4d\n"
		"\twork-item ID : %4d %4d %4d\n",
		(global_debug.enable)? on:off,
		global_debug.gid.x, global_debug.gid.y, global_debug.gid.z,
		global_debug.tid.x, global_debug.tid.y, global_debug.tid.z);
}

void hsa_print_profile(Monitor *mon, const QDict *qdict)
{
	global_ctl.show = qdict_get_bool(qdict, "status");
}

void hsa_reset_profile(Monitor *mon, const QDict *qdict)
{
	hsa_clear_prof(&global_ctl.prof);
}
void hsa_get_profile_info(Monitor *mon, const QDict *qdict)
{
	hsa_show_prof(&global_ctl.prof);
}

void hsa_init_mntor(void)
{
	// init variable from emulator
	// should only be called by HSA monitor thread
	// allocate CPU state, just for softmmu
	private_cpu_state = (CPUArchState*)malloc(sizeof(CPUArchState));
	if (private_cpu_state == NULL) {
		HSA_DEBUG_LOG("allocate architecture state");
	}
}

void hsa_mntor_update(void *cpu_state)
{
	// should only be called by HSA monitor thread
	hsa_task_t *ptr = (hsa_task_t*)cpu_state;
	global_cpu_state = (void*)ptr->env;
	memcpy(private_cpu_state, ptr->env, sizeof(CPUArchState));
}

void hsa_agent_update(void *cpu_state)
{
	agent_service_task_t *ptr = (agent_service_task_t*)cpu_state;
	global_cpu_state = (void*)ptr->env;
	memcpy(private_cpu_state, ptr->env, sizeof(CPUArchState));
}

void hsa_init_cu(void *cpu_state)
{
	// init variable from emulator
	// allocate CPU state, just for softmmu
	// should only be called by CU thread
	if (cpu_state == NULL) {
		HSA_DEBUG_LOG("allocate architecture state");
	}

	private_cpu_state = cpu_state;
}
void hsa_cu_update(void *cpu_state)
{
	// should only be called by CU thread
	if (cpu_state == NULL) {
		HSA_DEBUG_LOG("allocate architecture state");
	}

	memcpy(cpu_state, global_cpu_state, sizeof(CPUArchState));
}
int64_t hsa_get_clock(void)
{
	return qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
}

void hsa_flush_cu_tlb(void)
{
	//fprintf(stderr, "%s\n", __func__);
	hsa_cu_t *head = global_ctl.cu_thread;
	if (head == NULL) return;

	int i = 0;
	for (i = 0; i<global_ctl.num_cus; i++) {

		CPUArchState *envptr = (CPUArchState*)head->env;
		int j = 0;
		for (j = 0; j < CPU_TLB_SIZE; j++) {

			int mmu_idx;
			pthread_rwlock_wrlock(&cutlb_lock);
			for (mmu_idx = 0; mmu_idx < NB_MMU_MODES; mmu_idx++) {
				envptr->tlb_table[mmu_idx][j].addr_read = -1;
				envptr->tlb_table[mmu_idx][j].addr_write = -1;
				envptr->tlb_table[mmu_idx][j].addr_code = -1;
				envptr->tlb_table[mmu_idx][j].addend = -1;
			}
			pthread_rwlock_unlock(&cutlb_lock);
		}
		head += 1;
	}
}
