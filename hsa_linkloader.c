#include "hsa_comm_def.h"
#include "hsa_vmdep.h"
#include "hsa_helper.h"
#include "hsa_help_info.h"
#include "hsa_help_math.h"
#include "hsa_cu.h"
#include "hsa_linkloader.h"
#include "hsa_fexcept.h"

#define DEF(NAME, ADDR) \
{NAME, sizeof(NAME) - 1, (void *)(&(ADDR))},

typedef struct func_entry_t {
	char const *name;
	size_t name_len;
	void *addr;
}func_entry_t;

static int isfailed;

func_entry_t const helper_tab[] = {
	DEF("hsa_printf_32", hsa_printf_32)
	DEF("hsa_printf_64", hsa_printf_64)
	DEF("_ins_count", hsa_ins_count)

	
	DEF("hsa_load_8", hsa_load_8)
	DEF("hsa_load_16", hsa_load_16)
	DEF("hsa_load_32", hsa_load_32)
	DEF("hsa_load_64", hsa_load_64)
	DEF("hsa_store_8", hsa_store_8)
	DEF("hsa_store_16", hsa_store_16)
	DEF("hsa_store_32", hsa_store_32)
	DEF("hsa_store_64", hsa_store_64)

	DEF("Barrier", hsa_helper_barrier)

	DEF("helper_Fsin_32", helper_Fsin_f32)
	DEF("helper_Fsin_64", helper_Fsin_f64)
	DEF("helper_Fcos_32", helper_Fcos_f32)
	DEF("helper_Fcos_64", helper_Fcos_f64)
	DEF("helper_Flog2_32", helper_Flog2_f32)
	DEF("helper_Flog2_64", helper_Flog2_f64)
	DEF("helper_Fexp2_32", helper_Fexp2_f32)
	DEF("helper_Fexp2_64", helper_Fexp2_f64)
	DEF("helper_Sqrt_32", helper_FSqrt_f32)
	DEF("helper_Sqrt_64", helper_FSqrt_f64)

	DEF("helper_WorkItemAId", helper_WorkItemAId)
	DEF("helper_WorkItemId", helper_WorkItemId)
	DEF("helper_WorkGroupId", helper_WorkGroupId)
	DEF("helper_WorkGroupSize", helper_WorkGroupSize)
	DEF("helper_WorkNDRangesize", helper_WorkNDRangesize)
	DEF("helper_WorkNDRangegroups", helper_WorkNDRangegroups)

	DEF("hsa_atomic_32",hsa_atomic_32)
	DEF("hsa_atomic_64",hsa_atomic_64)
	DEF("hsa_atomic_noret_32",hsa_atomic_noret_32)
	DEF("hsa_atomic_noret_64",hsa_atomic_noret_64)

	DEF("helper_ClearDetectExcept",helper_ClearDetectExcept)
	DEF("helper_SetDetectExcept",helper_SetDetectExcept)
	DEF("helper_GetDetectExcept",helper_GetDetectExcept)
};

#undef DEF

static void set_sec_entry(struct elf_sec *sec_entry,
		struct elf_shdr *shdr,
		unsigned char *image_buf,
		int index)
{
	unsigned char *buf;

	sec_entry->shdr = shdr;
	sec_entry->sh_type = shdr->sh_type;
	sec_entry->buf_size = shdr->sh_size;


	switch (sec_entry->sh_type) {
		case SHT_STRTAB:
		case SHT_PROGBITS:
		case SHT_NOBITS:
			buf = image_buf + shdr->sh_offset;
			break;
		case SHT_REL:
		case SHT_RELA:
		case SHT_SYMTAB:
			buf = image_buf + shdr->sh_offset;
			sec_entry->extra = (void *)(unsigned long)(shdr->sh_size / shdr->sh_entsize);
			break;
		default:
			buf = NULL;
	}
	sec_entry->buf = buf;
}

static struct elf_sec *load_sectiontable(unsigned char *image_buf, 
		struct elf_shdr *shdr_table)
{
	struct elf_hdr *ehdr = (struct elf_hdr *)image_buf;
	struct elf_sec *sec_table;
	int i;

	sec_table = (struct elf_sec *)malloc(sizeof(struct elf_sec) * ehdr->e_shnum);
	if (!sec_table) {
		HSA_DEBUG_LOG("elf_sec space malloc failed.\n");
		return NULL;
	}

	for (i = 0; i < ehdr->e_shnum; i++) {
		set_sec_entry(sec_table + i, shdr_table + i, image_buf, i);
	}

	return sec_table;
}

/* zdguo: when get index from name, please init idx=-1 before calling this function */
static char *get_idxorname(struct elf_obj *elfobj, 
		const char *name, 
		int *idx)
{
	struct elf_hdr *ehdrp = elfobj->ehdrp;
	struct elf_shdr *shdr_tablep = elfobj->shdr_tablep;
	struct elf_sec *sec_tablep = elfobj->sec_tablep;
	struct elf_sec *sec_shstrtab = &((sec_tablep)[ehdrp->e_shstrndx]);
	unsigned long stroff;
	int i;

	if (*idx < 0 && name != NULL) {
		/* find idx by name */
		for (i = 0; i < ehdrp->e_shnum; i++) {
			stroff = shdr_tablep[i].sh_name;
			if (strcmp((char *)(sec_shstrtab->buf + stroff), name) == 0) {
				*idx = i;
				return NULL;
			}
		}
	}
	else if (*idx > 0 && name == NULL) {
		/* find name by index */
		stroff = shdr_tablep[*idx].sh_name;
		return (char *)(sec_shstrtab->buf + stroff);
	}

	return NULL;
}

static char *get_symname(struct elf_obj *elfobj, 
		struct elf_sym *sym_entry)
{
	int idx;
	unsigned char *strtab;

	idx = -1;
	get_idxorname(elfobj, ".strtab", &idx);
	strtab = elfobj->sec_tablep[idx].buf;

	return (char *)(strtab + sym_entry->st_name);
}

static void *allocateSHNCommonData(struct elf_obj *elfobj, 
		size_t size, 
		size_t align)
{
	void *ret_addr;
	size_t rem;

	if (size <= 0 || align == 0)
		return NULL;

	rem = (unsigned long)(elfobj->SHNCommonDataPtr) % align;
	if (rem != 0) {
		elfobj->SHNCommonDataPtr += align - rem;
		elfobj->SHNCommonDataFreeSize -= align - rem;
	}

	if (elfobj->SHNCommonDataFreeSize < size)
		return NULL;

	ret_addr = elfobj->SHNCommonDataPtr;
	elfobj->SHNCommonDataPtr += size;
	elfobj->SHNCommonDataFreeSize -= size;

	return ret_addr;
}

static void *get_symety_addr(struct elf_obj *elfobj, struct elf_sym *sym_entry, int autoAlloc)
{
	struct elf_shdr *shdr_tablep;
	struct elf_sec *sec;
	size_t idx = (size_t)sym_entry->st_shndx, align;
	unsigned int section_type;
	void *ret_addr = NULL;

	switch (ELF_ST_TYPE(sym_entry->st_info)) {
		case STT_OBJECT:
			switch (idx)
		case SHN_COMMON: {
					 if (!autoAlloc) {
						 return NULL;
					 }
					 align = sym_entry->st_value;
					 ret_addr = allocateSHNCommonData(elfobj, sym_entry->st_size, align);
					 if (!ret_addr) {
						 HSA_DEBUG_LOG("Unable to allocate memory for SHN_COMMON.\n");
						 isfailed = 1;
					 }
					 break;
					 case SHN_ABS:
					 case SHN_UNDEF:
					 case SHN_XINDEX:
					 HSA_DEBUG_LOG("STT_OBJECT with special st_shndx.\n");
					 isfailed = 1;
					 break;
					 default:
					 shdr_tablep = elfobj->shdr_tablep;
					 section_type = shdr_tablep[idx].sh_type;

					 if (section_type == SHT_PROGBITS) {
						 sec = &elfobj->sec_tablep[idx];
						 ret_addr = (void *)(sec->buf + sym_entry->st_value);
					 }
					 else if (section_type == SHT_NOBITS) {
						 align = 16;
						 ret_addr = allocateSHNCommonData(elfobj, sym_entry->st_size, align);
						 if (!ret_addr) {
							 HSA_DEBUG_LOG("Unable to allocate memory for SHN_COMMON.\n");
							 isfailed = 1;
						 }
					 }
					 else {
						 HSA_DEBUG_LOG("STT_OBJECT with not BITS setion.\n");
						 isfailed = 1;
					 }
					 break;
				 }
			break;
		case STT_FUNC:
			switch (idx) {
				case SHN_ABS:
				case SHN_COMMON:
				case SHN_UNDEF:
				case SHN_XINDEX:
					HSA_DEBUG_LOG("STT_FUNC with special st_shndx.\n");
					isfailed = 1;
					break;
				default: {
						 shdr_tablep = elfobj->shdr_tablep;
						 if (shdr_tablep[idx].sh_type != SHT_PROGBITS) {
							 HSA_DEBUG_LOG("STT_FUNC with not BITS section.\n");
							 isfailed = 1;
						 }

						 sec = &elfobj->sec_tablep[idx];
						 ret_addr = (void *)(sec->buf + sym_entry->st_value);
						 break;
					 }
			}
			break;
		case STT_SECTION:
			switch (idx) {
				case SHN_ABS:
				case SHN_COMMON:
				case SHN_UNDEF:
				case SHN_XINDEX:
					HSA_DEBUG_LOG("STT_SECTION with special st_shndx.\n");
					isfailed = 1;
					break;
				default: {
						 shdr_tablep = elfobj->shdr_tablep;
						 if (shdr_tablep[idx].sh_type != SHT_PROGBITS &&
								 shdr_tablep[idx].sh_type != SHT_NOBITS) {
							 HSA_DEBUG_LOG("STT_SECTION with not BITS section.\n");
							 isfailed = 1;
						 }

						 sec = &elfobj->sec_tablep[idx];
						 ret_addr = (void *)(sec->buf + sym_entry->st_value);
						 break;
					 }
			}
			break;
		case STT_NOTYPE:
			switch (idx) {
				case SHN_ABS:
				case SHN_COMMON:
				case SHN_XINDEX:
					HSA_DEBUG_LOG("STT_NOTYPE with special st_shndx.\n");
					isfailed = 1;
					break;
				case SHN_UNDEF:
					return 0;
				default: {
						 shdr_tablep = elfobj->shdr_tablep;
						 if (shdr_tablep[idx].sh_type != SHT_PROGBITS &&
								 shdr_tablep[idx].sh_type != SHT_NOBITS) {
							 HSA_DEBUG_LOG("STT_NOTYPE with not BITS section.\n");
							 isfailed = 1;
						 }

						 sec = &elfobj->sec_tablep[idx];
						 ret_addr = (void *)(sec->buf + sym_entry->st_value);
						 break;
					 }
			}
			break;
		case STT_COMMON:
		case STT_FILE:
		case STT_TLS:
		case STT_LOOS:
		case STT_HIOS:
		case STT_LOPROC:
		case STT_HIPROC:
		default:
			HSA_DEBUG_LOG("Not implement.\n");
			isfailed = 1;
			break;
	}

	return ret_addr;
}

static void relocateX86_64(struct elf_obj *elfobj,
		void *(*find_sym)(void *context, char const *name),
		void *context,
		struct elf_sec *reltab,
		unsigned char *text)
{
	int idx, i, reltab_size;
	struct elf_sym *symtab, *sym_entry;
	ELF_RELOC *rel_tablep, *rel_entry;
	int32_t *inst, P, A, S;

	idx = -1;
	get_idxorname(elfobj, ".symtab", &idx);
	if (idx < 0) {
		HSA_DEBUG_LOG(".symtab can't find.\n");
		isfailed = 1;
		return;
	}
	symtab = (struct elf_sym *)(elfobj->sec_tablep[idx].buf);

	reltab_size = (int)(unsigned long)reltab->extra;
	rel_tablep = (ELF_RELOC *)reltab->buf;
	for (i = 0; i < reltab_size; i++) {
		rel_entry = &rel_tablep[i];
		sym_entry = &symtab[ELF_R_SYM(rel_entry->r_info)];

		inst = (int32_t *)&(text[rel_entry->r_offset]);
		P = (int32_t)(int64_t)inst;
		A = (int32_t)(int64_t)rel_entry->r_addend;
		S = (int32_t)(int64_t)get_symety_addr(elfobj, sym_entry, 1);

		if (0 == S) {
			S = (int64_t)find_sym(context, get_symname(elfobj, sym_entry));
			/* zdguo: set the S to the symbol entry struct for optimization */
		}

		switch (ELF_R_TYPE(rel_entry->r_info)) {
			case R_X86_64_64:
				*inst = (S+A);
				break;
			case R_X86_64_PC32:
				*inst = (S+A-P);
				break;
			case R_X86_64_32:
			case R_X86_64_32S:
				*inst = (S+A);
				break;
			default:
				HSA_DEBUG_LOG("Not implemented relocation type.\n");
				isfailed = 1;
				break;
		}
	}

	return;
}

static void relocate(struct elf_obj *elfobj,
		void *(*find_sym)(void *context, char const *name),
		void *context)
{
	struct elf_hdr *ehdrp = elfobj->ehdrp;
	struct elf_shdr *shdr_tablep = elfobj->shdr_tablep;
	struct elf_sec *sec_tablep = elfobj->sec_tablep;
	struct elf_sym *symtab, *symety;
	ELF_RELOC *reltab;
	int i, idx, symtab_size;
	unsigned long SHNCommonDataSize = 0, align;
	char *reltab_name, *need_rel_name;

	idx = -1;
	get_idxorname(elfobj, ".symtab", &idx);
	if (idx < 0) {
		HSA_DEBUG_LOG("Can't find .symtab section");
		isfailed = 1;
		return;
	}

	symtab = (struct elf_sym *)(sec_tablep[idx].buf);
	symtab_size = (int)(unsigned long)sec_tablep[idx].extra;
	for (i = 0; i < symtab_size; i++) {
		symety = &symtab[i];

		if (ELF_ST_TYPE(symety->st_info) != STT_OBJECT) {
			continue;
		}

		idx = (int)symety->st_shndx;
		switch (idx) {
			case SHN_COMMON:
				{
					align = (unsigned long)symety->st_value;
					SHNCommonDataSize += (unsigned long)symety->st_size + align;
				}
				break;
			case SHN_ABS:
			case SHN_UNDEF:
			case SHN_XINDEX:
				break;
			default:
				if (shdr_tablep[idx].sh_type == SHT_NOBITS) {
					// FIXME(logan): This is a workaround for .lcomm directives
					// bug of LLVM ARM MC code generator.  Remove this when the
					// LLVM bug is fixed.

					align = 16;
					SHNCommonDataSize += (unsigned long)symety->st_size + align;
				}
				break;
		}
	}

	if (SHNCommonDataSize > 0) {
		elfobj->SHNCommonData = (unsigned char *)valloc(SHNCommonDataSize);
		elfobj->SHNCommonDataFreeSize = SHNCommonDataSize;
		elfobj->SHNCommonDataPtr = elfobj->SHNCommonData;
	}
	else {
		elfobj->SHNCommonData = NULL;
		elfobj->SHNCommonDataFreeSize = SHNCommonDataSize;
		elfobj->SHNCommonDataPtr = NULL;
	}

	for (i = 0; i < ehdrp->e_shnum; i++) {
		if (shdr_tablep[i].sh_type != SHT_REL &&
				shdr_tablep[i].sh_type != SHT_RELA) {
			continue;
		}
		reltab = (ELF_RELOC *)sec_tablep[i].buf;
		if (!reltab) {
			HSA_DEBUG_LOG("Relocation section can't be NULL.\n");
			isfailed = 1;
			return;
		}

		reltab_name = get_idxorname(elfobj, NULL, &i);
		if (shdr_tablep[i].sh_type == SHT_REL) {
			need_rel_name = reltab_name + 4;
		}
		else {
			need_rel_name = reltab_name + 5;
		}

		idx = -1;
		get_idxorname(elfobj, need_rel_name, &idx);
		if (idx < 0) {
			HSA_DEBUG_LOG("Can't find need rel section");
			isfailed = 1;
			return;
		}

		switch (ehdrp->e_machine){
			case EM_X86_64:
				relocateX86_64(elfobj, find_sym, context, 
						&sec_tablep[i], sec_tablep[idx].buf);
				break;
			default:
				HSA_DEBUG_LOG("Only support X86_64 relocation\n");
				isfailed = 1;
				return;
				break;
		}
	}
	/* protect code cache */

	return;
}

static void *loaderGetSymAddr(struct elf_obj *elfobj, const char *name)
{
	int idx, i, sym_size;
	struct elf_sym *symtab, *symety;
	unsigned char *strtable;

	idx = -1;
	get_idxorname(elfobj, ".symtab", &idx);
	symtab = (struct elf_sym *)elfobj->sec_tablep[idx].buf;
	sym_size = (int)(unsigned long)elfobj->sec_tablep[idx].extra;

	idx = -1;
	get_idxorname(elfobj, ".strtab", &idx);
	strtable = elfobj->sec_tablep[idx].buf;

	for (i = 0; i < sym_size; i++) {
		if (strcmp(name, (char *)(strtable + symtab[i].st_name)) == 0) {
			break;
		}
	}

	if (i >= sym_size) {
		fprintf(stderr, "can't find symbol: %s, at %s:%d\n",
			name, __func__, __LINE__);
		return NULL;
	}
	symety = &symtab[i];

	return get_symety_addr(elfobj, symety, 0);
}

/* Verify the portions of EHDR within E_IDENT for the target.
   This can be performed before bswapping the entire header.  */
static int elf_check_ident(struct elf_hdr *ehdr)
{
	return (ehdr->e_ident[EI_MAG0] == ELFMAG0
			&& ehdr->e_ident[EI_MAG1] == ELFMAG1
			&& ehdr->e_ident[EI_MAG2] == ELFMAG2
			&& ehdr->e_ident[EI_MAG3] == ELFMAG3
			&& ehdr->e_ident[EI_CLASS] == ELF_CLASS
			&& ehdr->e_ident[EI_DATA] == ELF_DATA
			&& ehdr->e_ident[EI_VERSION] == EV_CURRENT);
}

static int elf_check_ehdr(struct elf_hdr *ehdr)
{
	return (elf_check_arch(ehdr->e_machine)
			&& (ehdr->e_phnum == 0 || ehdr->e_ehsize == sizeof(struct elf_hdr))
			&& (ehdr->e_shnum == 0 || ehdr->e_shentsize == sizeof(struct elf_shdr))
			&& ehdr->e_shentsize == sizeof(struct elf_shdr));
}

static void *find_sym(void *context, char const *name) {

	static size_t const tab_size = 
		sizeof(helper_tab) / sizeof(struct func_entry_t);

	// Note: Since our table is small, we are using trivial O(n) searching
	// function.  For bigger table, it will be better to use binary
	// search or hash function.
	size_t i;
	size_t name_len = strlen(name);
	for (i = 0; i < tab_size; ++i) {
		if (name_len == helper_tab[i].name_len && 
				strcmp(name, helper_tab[i].name) == 0) {
			return helper_tab[i].addr;
		}
	}

	HSA_DEBUG_LOG("Can't find symbol");
	isfailed = 1;

	return NULL;
}

void *cc_producer(void *bin, const char *name)
{
	struct elf_hdr *ehdr;
	struct elf_shdr *shdr_table;
	struct elf_sec *sec_table = NULL;
	struct elf_obj elfobj;
	unsigned char *image = NULL;
	void *kernel_entry;

	isfailed = 0;
	image = bin;
	ehdr = (struct elf_hdr *)image;

	/* First of all, some simple consistency checks */
	if (!elf_check_ident(ehdr)) {
		HSA_DEBUG_LOG("Check ident failed.\n");
		goto failed;
	}

	if (!elf_check_ehdr(ehdr)) {
		HSA_DEBUG_LOG("Check ehdr failed.\n");
		goto failed;
	}
	memset(&elfobj, 0, sizeof(struct elf_obj));

	shdr_table = (struct elf_shdr *)(image + ehdr->e_shoff);

	sec_table = load_sectiontable(image, shdr_table);

	if (!sec_table) {
		HSA_DEBUG_LOG("fetch sec_table failed.\n");
		goto failed;
	}
	elfobj.ehdrp = ehdr;
	elfobj.shdr_tablep = shdr_table;
	elfobj.sec_tablep = sec_table;

	relocate(&elfobj, find_sym, NULL);

	if (isfailed)
		goto failed;

	kernel_entry = loaderGetSymAddr(&elfobj, name);

	if (sec_table)
		free(sec_table);
	if (elfobj.SHNCommonData)
		free(elfobj.SHNCommonData);

	return kernel_entry;

failed:
	if (sec_table)
		free(sec_table);
	if (elfobj.SHNCommonData)
		free(elfobj.SHNCommonData);
	return NULL;
}
