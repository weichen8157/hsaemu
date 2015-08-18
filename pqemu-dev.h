#ifndef __HSA_DEV_H__
#define __HSA_DEV_H__
#include <sys/queue.h>
#include "qapi/qmp/qdict.h"
#include "qemu-common.h"
#include <stdint.h>

void hsa_parse(QemuOpts *opts);
void hsa_set_debug(Monitor *mon, const QDict *qdict);
void hsa_set_debug_mask(Monitor *mon, const QDict *qdict);
void hsa_get_debug_info(Monitor *mon, const QDict *qdict);
void hsa_print_profile(Monitor *mon, const QDict *qdict);
void hsa_reset_profile(Monitor *mon, const QDict *qdict);
void hsa_get_profile_info(Monitor *mon, const QDict *qdict);
void hsa_flush_cu_tlb(void);

#ifndef NOT_INC_COPY_FUNC

#define HSA_BASE_ADDR 0xf0000000
#define HSA_MMIO_SIZE 4096

typedef struct _hsa_state
{
	SysBusDevice busdev;
	qemu_irq     irq;
	MemoryRegion iomem;
	int          count;
	uint8_t      data[HSA_MMIO_SIZE];
}hsa_state_t;

void hsa_dev_init(void);


#ifndef READ_ACCESS_TYPE
#define READ_ACCESS_TYPE 0
#endif

#define HSA_GET_PHYADDR2(__paddr, __ptr, __vaddr, __prof)                                         \
    do{                                                                                           \
        CPUArchState *__env = (CPUArchState*)__ptr;                                               \
        int index;                                                                                \
        target_ulong tlb_addr;                                                                    \
        index = (__vaddr >> TARGET_PAGE_BITS) & (CPU_TLB_SIZE - 1);                               \
        pthread_rwlock_rdlock(&cutlb_lock);                                                       \
    redo:                                                                                         \
        tlb_addr = __env->tlb_table[MMU_USER_IDX][index].addr_read;                               \
        if ((__vaddr & TARGET_PAGE_MASK) == (tlb_addr & (TARGET_PAGE_MASK | TLB_INVALID_MASK))) { \
            if (tlb_addr & ~TARGET_PAGE_MASK) {                                                   \
                HSA_DEBUG_LOG("should not access IO currently\n");                                \
                while(1);                                                                         \
            } else {                                                                              \
                uintptr_t addend;                                                                 \
                addend = __env->tlb_table[MMU_USER_IDX][index].addend;                            \
                __paddr = (uintptr_t)(__vaddr + addend);                                          \
            }                                                                                     \
        } else {                                                                                  \
            hsa_profile_t *profptr = __prof;                                                      \
            if (profptr) {                                                                        \
                profptr->tlb++;                                                                   \
            }                                                                                     \
            tlb_fill(__env, __vaddr, READ_ACCESS_TYPE, MMU_USER_IDX, GETPC());                    \
            goto redo;                                                                            \
        }                                                                                         \
        pthread_rwlock_unlock(&cutlb_lock);                                                       \
    } while (0)

#define HSA_GET_PHYADDR(__paddr, __ptr, __vaddr) \
	HSA_GET_PHYADDR2(__paddr, __ptr, __vaddr, NULL)
#endif


#endif

