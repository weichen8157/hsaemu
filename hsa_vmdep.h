#ifndef __HSAEMU_VM_H__
#define __HSAEMU_VM_H__
#include <stdlib.h>
#include <stdint.h>

#ifndef GUEST_IS_64
typedef uint32_t guest_vaddr_t;
typedef uint32_t guest_paddr_t;
#else
typedef uint64_t guest_vaddr_t;
typedef uint64_t guest_paddr_t;
#endif

void hsa_agent_update(void *cpu_state);

// init HSA device
void hsa_component_init(void);
void hsa_init_mntor(void);
void hsa_mntor_update(void *cpu_state);
void hsa_init_cu(void *cpu_state);
void hsa_cu_update(void *cpu_state);

// transfer guest virtual address to physical addr
uintptr_t hsa_get_phy_addr_global(void *cpu_state, guest_vaddr_t addr);
uintptr_t hsa_get_phy_addr_segment(guest_vaddr_t addr, int segment);

// copy data
size_t hsa_copy_to_guest(guest_vaddr_t dst, void *src, const size_t size);
size_t hsa_copy_from_guest(void *dst, guest_vaddr_t src, const size_t size);

// insert command
int hsa_enqueue_cmd(void *cpu_state, guest_vaddr_t addr);

// get time
int64_t hsa_get_clock(void);

// reset HSA component
void hsa_component_fault(guest_vaddr_t addr);
#endif
