#ifndef __HSA_HELP_INFO_H_
#define __HSA_HELP_INFO_H_

#include <stdint.h>

void hsa_printf_32(int vaddr);
void hsa_printf_64(uintptr_t vaddr);
void hsa_ins_count(uint32_t count);

/*************************************************************************/

// for item IDs
uint32_t helper_WorkItemId(uint32_t dimension);
uint32_t helper_WorkItemAId(uint32_t dimension);
uint32_t helper_WorkGroupId(uint32_t dimension);
uint32_t helper_WorkGroupSize(uint32_t dimension);
/*uint32_t helper_WorkGridSize(uint32_t dimension);*/
uint32_t helper_WorkNDRangesize(uint32_t dimension);
uint32_t helper_WorkNDRangegroups(uint32_t dimension);

#endif
