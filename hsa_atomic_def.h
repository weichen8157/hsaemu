#if MEM_BIT == 32
#define DATA_TYPE uint32_t
#elif MEM_BIT == 64
#define DATA_TYPE uint64_t
#else
#error
#endif

static DATA_TYPE gum(atomic_op_, MEM_BIT)(
	uintptr_t phy_addr,
	DATA_TYPE src0,
	DATA_TYPE src1,
	hsa_atomic_op_t op)
{
	DATA_TYPE origin = 0;
	DATA_TYPE modify = 0;

	switch(op){
		case HSA_ATOMIC_LD:
			// FIXME
			break;
		case HSA_ATOMIC_ST:
			// FIXME
			break;
		case HSA_ATOMIC_AND:
			origin = __sync_fetch_and_and((DATA_TYPE*)phy_addr, src0);
			break;
		case HSA_ATOMIC_OR:
			origin = __sync_fetch_and_or((DATA_TYPE*)phy_addr, src0);
			break;
		case HSA_ATOMIC_XOR:
			origin = __sync_fetch_and_xor((DATA_TYPE*)phy_addr, src0);
			break;
		case HSA_ATOMIC_EXCH:
			// FIXME
			break;
		case HSA_ATOMIC_ADD:
			origin = __sync_fetch_and_add((DATA_TYPE*)phy_addr, src0);
			break;
		case HSA_ATOMIC_SUB:
			origin = __sync_fetch_and_sub((DATA_TYPE*)phy_addr, src0);
			break;
		case HSA_ATOMIC_INC:
			do{
				origin = *((DATA_TYPE*)phy_addr);
				modify = (origin >= src0) ? 0 : origin + 1;

			}while(!__sync_bool_compare_and_swap((DATA_TYPE*)phy_addr, origin, modify));
			break;
		case HSA_ATOMIC_DEC:
			do{
				origin = *((DATA_TYPE*)phy_addr);
				modify = (origin >= src0) ? 0 : origin - 1;

			}while(!__sync_bool_compare_and_swap((DATA_TYPE*)phy_addr, origin, modify));
			break;
		case HSA_ATOMIC_MAX:
			do{
				origin = *((DATA_TYPE*)phy_addr);
				modify = (origin > src0) ? origin : src0;

			}while(!__sync_bool_compare_and_swap((DATA_TYPE*)phy_addr, origin, modify));
			break;
		case HSA_ATOMIC_MIN:
			do{
				origin = *((DATA_TYPE*)phy_addr);
				modify = (origin < src0) ? origin : src0;

			}while(!__sync_bool_compare_and_swap((DATA_TYPE*)phy_addr, origin, modify));
			break;
		case HSA_ATOMIC_CAS:
			origin = __sync_val_compare_and_swap((DATA_TYPE*)phy_addr, src0, src1);
			break;
		default:
			fprintf(stderr, "compute unit fault: %s\n", __func__);
			hsa_component_fault(phy_addr);
			break;
	}

	return origin;
}

void gum(hsa_atomic_noret_, MEM_BIT)(
	guest_vaddr_t addr,
	DATA_TYPE src0,
	DATA_TYPE src1,
	int segment,
	int op)
{
	cu_context.prof.atom++;

	uintptr_t phy_addr = hsa_get_phy_addr_segment(addr, segment);

	gum(atomic_op_,MEM_BIT)(phy_addr, src0, src1, op);
}

DATA_TYPE gum(hsa_atomic_, MEM_BIT)(
	guest_vaddr_t addr,
	DATA_TYPE src0,
	DATA_TYPE src1,
	int segment,
	int op)
{	
	cu_context.prof.atom++;

	uintptr_t phy_addr = hsa_get_phy_addr_segment(addr, segment);

	return gum(atomic_op_,MEM_BIT)(phy_addr, src0, src1, op);
}

#undef DATA_TYPE
