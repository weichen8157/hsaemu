#if MEM_BIT == 64
#define DATA_TYPE  uint64_t
#elif MEM_BIT == 32
#define DATA_TYPE  uint32_t
#elif MEM_BIT == 16
#define DATA_TYPE  uint16_t
#elif MEM_BIT == 8
#define DATA_TYPE  uint8_t
#else
#error
#endif

void gum(hsa_store_, MEM_BIT)(guest_vaddr_t addr, DATA_TYPE val, int segment)
{
	uintptr_t phy_addr = 0;

	switch(segment){
		case HSA_GLOBAL:
			cu_context.prof.gst++;
			break;
		case HSA_GROUP:
			cu_context.prof.lst++;
			break;
		case HSA_PRIVATE:
			//FIX prof
			break;
		case HSA_KERNARG:
			//FIX prof
			break;
		case HSA_READ_ONLY:
			//FIX prof
			break;
		default:
			fprintf(stderr,"Store SegmentType is wrong!\n");
			break;
	}

	phy_addr = hsa_get_phy_addr_segment(addr, segment);

	(*(DATA_TYPE *)phy_addr) = val;
}

DATA_TYPE gum(hsa_load_, MEM_BIT)(guest_vaddr_t addr, int segment)
{
	uintptr_t phy_addr = 0;

	
	switch(segment){
		case HSA_GLOBAL:
			cu_context.prof.gld++;
			break;
		case HSA_GROUP:
			cu_context.prof.lld++;
			break;
		case HSA_PRIVATE:
			//FIX prof
			break;
		case HSA_KERNARG:
			//FIX prof
			break;
		case HSA_READ_ONLY:
			//FIX prof
			break;
		default:
			fprintf(stderr,"Load SegmentType is wrong!\n");
			break;
	}
	
	phy_addr = hsa_get_phy_addr_segment(addr, segment);

	return (*(DATA_TYPE *)phy_addr);
}

#undef DATA_TYPE
