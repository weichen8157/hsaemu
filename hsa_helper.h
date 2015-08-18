#ifndef HSA_HELPER_H
#define HSA_HELPER_H

uint8_t  hsa_load_8(guest_vaddr_t addr, int segment);
uint16_t hsa_load_16(guest_vaddr_t addr, int segment);
uint32_t hsa_load_32(guest_vaddr_t addr, int segment);
uint64_t hsa_load_64(guest_vaddr_t addr, int segment);
void hsa_store_8(guest_vaddr_t addr, uint8_t val, int segment);
void hsa_store_16(guest_vaddr_t addr, uint16_t val, int segment);
void hsa_store_32(guest_vaddr_t addr, uint32_t val, int segment);
void hsa_store_64(guest_vaddr_t addr, uint64_t val, int segment);
void hsa_atomic_noret_32(guest_vaddr_t addr, uint32_t src0, uint32_t src1, int segment, int op);
void hsa_atomic_noret_64(guest_vaddr_t addr, uint64_t src0, uint64_t src1, int segment, int op);
uint32_t hsa_atomic_32(guest_vaddr_t addr, uint32_t src0, uint32_t src1, int segment, int op);
uint64_t hsa_atomic_64(guest_vaddr_t addr, uint64_t src0, uint64_t src1, int segment, int op);

#endif
