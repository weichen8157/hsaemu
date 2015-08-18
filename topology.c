#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "topology.h"

static hsa_entry_list_t _hsa_agent_head =
	TAILQ_HEAD_INITIALIZER(_hsa_agent_head);
static int _hsa_number_agent_reg = 0;

static hsa_entry_list_t _hsa_region_head =
	TAILQ_HEAD_INITIALIZER(_hsa_region_head);
static int _hsa_number_region_reg = 0;

static hsa_entry_list_t _hsa_link_head = 
	TAILQ_HEAD_INITIALIZER(_hsa_link_head);
static int _hsa_number_link = 0;

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
	uint64_t max_wait)
{

	hsa_agent_entry_t *ptr = 
		calloc(1, sizeof(hsa_agent_entry_t));

	if (agent_name) {
		fprintf(stderr, "%s: %s\n", __func__, agent_name);
		strncpy(ptr->agent_name,
			agent_name, 64);
	}

	ptr->node = node;
	ptr->feature = feature;
	ptr->dev_type = dev_type;

	if (vendor_name) {
		strncpy(ptr->vendor_name,
			vendor_name, 64);
	}

	ptr->wavefront_size =
		wavefront_size;

	if (cache_size) {
		memcpy(ptr->cache_size,
			cache_size,
			sizeof(uint32_t)*4);
	}

	ptr->grid_dim = grid_dim;
	ptr->group_dim = group_dim;
	ptr->max_packet = max_packet;
	ptr->clock_info = clock_info;
	ptr->clock_frq = clock_frq;
	ptr->max_wait = max_wait;

	hsa_entry_t *n = malloc(sizeof(hsa_entry_t));
	n->ptr = ptr;
	TAILQ_INSERT_TAIL(&_hsa_agent_head, n, next);
	_hsa_number_agent_reg++;

	return ptr;
}

hsa_region_entry_t* registe_region(const char *name,
	guest_ptr_t base_addr,
	guest_ptr_t size,
	uint64_t node,
	guest_ptr_t max_alloc_size,
	uint8_t segment,
	uint32_t bandwidth,
	uint8_t *cache)
{
	hsa_region_entry_t *ptr =
		calloc(1, sizeof(hsa_region_entry_t));

	if (name) {
		strncpy(ptr->name, name, 64);
	}

	ptr->base_addr = base_addr;
	ptr->size = size;
	ptr->node = node;
	ptr->max_alloc_size = max_alloc_size;
	ptr->segment = segment;
	ptr->bandwidth = bandwidth;

	if (cache) {
		memcpy(ptr->cache,
			cache,
			sizeof(uint8_t) * 4);
	}

	hsa_entry_t *n = malloc(sizeof(hsa_entry_t));
	n->ptr = ptr;
	TAILQ_INSERT_TAIL(&_hsa_region_head, n, next);
	_hsa_number_region_reg++;

	return ptr;
}

void agent_link_memory(hsa_agent_entry_t *agent,
	hsa_region_entry_t *region)
{
	assert(agent);
	assert(region);

	uint16_t agent_id = 0;
	hsa_entry_t *n = NULL;
	TAILQ_FOREACH(n, &_hsa_agent_head, next)
	{
		if (n->ptr == agent) {
			break;
		}
		agent_id++;
	}
	assert(agent_id < _hsa_number_agent_reg);

	uint16_t region_id = 0;
	TAILQ_FOREACH(n, &_hsa_region_head, next)
	{
		if (n->ptr == region) {
			break;
		}
		region_id++;
	}
	assert(region_id < _hsa_number_region_reg);

	hsa_link_t *l = malloc(sizeof(hsa_link_t));
	l->agent = agent_id;
	l->region = region_id;

	n = malloc(sizeof(hsa_entry_t));
	n->ptr = l;
	TAILQ_INSERT_TAIL(&_hsa_link_head, n, next);
	_hsa_number_link++;
}

void* gen_topology(void)
{
	size_t total_size = sizeof(hsa_topology_header_t) +
		sizeof(hsa_agent_entry_t) * _hsa_number_agent_reg +
		sizeof(hsa_region_entry_t) * _hsa_number_region_reg +
		sizeof(hsa_link_t) * _hsa_number_link;
	fprintf(stderr, "topology table size = %zu\n", total_size);

	char *buf = calloc(1, total_size);
	long offset = 0;


	// setting header
	hsa_topology_header_t *head = (hsa_topology_header_t*)buf;

	head->header_size = sizeof(hsa_topology_header_t);
	head->total_size = total_size;

	head->agent_amount = _hsa_number_agent_reg;
	head->agent_size = sizeof(hsa_agent_entry_t);

	head->region_amount = _hsa_number_region_reg;
	head->region_size = sizeof(hsa_region_entry_t);

	head->link_amount = _hsa_number_link;
	head->link_size = sizeof(hsa_link_t);

	offset += head->header_size;

	// setting agent entry
	hsa_agent_entry_t *agent = (hsa_agent_entry_t*)(buf + offset);
	hsa_entry_t *n = NULL;
	TAILQ_FOREACH(n, &_hsa_agent_head, next)
	{
		memcpy(agent, n->ptr,
			sizeof(hsa_agent_entry_t));
		agent++;
	}

	offset += (head->agent_amount * head->agent_size);

	// setting region entry
	hsa_region_entry_t *region = (hsa_region_entry_t*)(buf + offset);
	n = NULL;
	TAILQ_FOREACH(n, &_hsa_region_head, next)
	{
		memcpy(region, n->ptr,
			sizeof(hsa_region_entry_t));
		region++;
	}

	offset += (head->region_amount * head->region_size);

	// setting link entry
	hsa_link_t *link = (hsa_link_t*)(buf + offset);

	n = NULL;
	TAILQ_FOREACH(n, &_hsa_link_head, next)
	{
		memcpy(link, n->ptr,
			sizeof(hsa_link_t));
		link++;
	}

	return buf;
}

#if 0
void test(void)
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

	hsa_topology_header_t *table = NULL;
	table = gen_topology();

	int number_agent = table->agent_size / sizeof(hsa_agent_entry_t);
	int number_region = table->region_size / sizeof(hsa_region_entry_t);
	int number_link = table->link_size / sizeof(hsa_link_t);
	assert(number_agent == 3);
	assert(number_region == 1);
	assert(number_link == 3);

	uint8_t *tmp = (uint8_t*)table;
	tmp += table->agent_offset;
	cpu = (hsa_agent_entry_t*)tmp;

	int i = 0;
	for (i = 0; i < number_agent; ++i)
	{
		printf("%s\n", cpu[i].agent_name);
	}

	tmp = (uint8_t*)table;
	tmp += table->region_offset;
	sys_mem = (hsa_region_entry_t*)tmp;
	for (i = 0; i < number_region; ++i)
	{
		printf("%s\n", sys_mem[i].name);
	}

	tmp = (uint8_t*)table;
	tmp += table->link_offset;
	hsa_link_t *link = (hsa_link_t*)tmp;
	for (i = 0; i < number_link; ++i)
	{
		printf("( %d, %d )\n",
			link[i].agent, link[i].region);
	}
}
#endif
