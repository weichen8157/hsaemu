#ifndef __TOPOLOGY_H__
#define __TOPOLOGY_H__

#include <stdint.h>
#include <sys/queue.h>

#define HSA_SEGMENT_GLOBAL  1
#define HSA_SEGMENT_GROUP   4
#define HSA_SEGMENT_KERNARG 8
#define HSA_DEVICE_TYPE_CPU 0
#define HSA_DEVICE_TYPE_GPU 1
#define HSA_COMPONENT_FEATURE_DISPATCH 1

typedef struct _hsa_entry hsa_entry_t;
struct _hsa_entry {
	void *ptr;
	TAILQ_ENTRY(_hsa_entry) next;
};
typedef TAILQ_HEAD(_hsa_entry_list, _hsa_entry) hsa_entry_list_t;

typedef struct _hsa_link
{
	uint16_t agent;
	uint16_t region;
}hsa_link_t;

typedef struct _hsa_topology_header
{
	uint32_t total_size;
	uint32_t header_size;

	uint32_t agent_amount;
	uint32_t agent_size;

	uint32_t region_amount;
	uint32_t region_size;

	uint32_t link_amount;
	uint32_t link_size;
}hsa_topology_header_t;

typedef struct _hsa_agent_entry
{
	char agent_name[64];
	uint64_t node;
	uint64_t feature;
	uint64_t dev_type;
	char vendor_name[64];
	uint64_t wavefront_size;
	uint64_t cache_size[4];
	uint64_t grid_dim;
	uint64_t group_dim;
	uint64_t max_packet;
	uint64_t clock_info;
	uint64_t clock_frq;
	uint64_t max_wait;
}hsa_agent_entry_t;

typedef uint32_t guest_ptr_t;

typedef struct _hsa_region_entry
{
	char name[64];
	uint64_t base_addr;
	uint64_t size;
	uint64_t node;
	uint64_t max_alloc_size;
	uint64_t segment;
	uint64_t bandwidth;
	uint16_t cache[4]; // should be same as bool
}hsa_region_entry_t;

hsa_agent_entry_t* registe_agent(const char *agent_name,
	uint64_t node,
	uint32_t feature,
	uint8_t dev_type,
	const char *vendor_name,
	uint32_t wavefront_size,
	uint32_t *cache_size,
	uint8_t grid_dim,
	uint8_t group_dim,
	uint32_t max_packet,
	uint64_t clock_info,
	uint16_t clock_frq,
	uint64_t max_wait);

hsa_region_entry_t* registe_region(const char *name,
	guest_ptr_t base_addr,
	guest_ptr_t size,
	uint64_t node,
	guest_ptr_t max_alloc_size,
	uint8_t segment,
	uint32_t bandwidth,
	uint8_t *cache);

void agent_link_memory(hsa_agent_entry_t *agent,
	hsa_region_entry_t *region);

void* gen_topology(void);

#endif
